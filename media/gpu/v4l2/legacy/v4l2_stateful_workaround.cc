// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/legacy/v4l2_stateful_workaround.h"

#include <string.h>
#include <memory>
#include <vector>

#include <linux/videodev2.h>

#include "base/containers/small_map.h"
#include "base/containers/span.h"
#include "base/memory/ptr_util.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_types.h"
#include "media/gpu/macros.h"
#include "media/parsers/vp8_parser.h"
#include "media/video/video_decode_accelerator.h"

namespace media {
namespace {
// Creates superframe index from |frame_sizes|. The frame sizes is stored in the
// same bytes. For example, if the max frame size is two bytes, even if the
// smaller frame sizes are 1 byte, they are stored as two bytes. See the detail
// for VP9 Spec Annex B.
std::vector<uint8_t> CreateSuperFrameIndex(
    const std::vector<uint32_t>& frame_sizes) {
  if (frame_sizes.size() < 2)
    return {};

  // Computes the bytes of the maximum frame size.
  const uint32_t max_frame_size =
      *std::max_element(frame_sizes.begin(), frame_sizes.end());
  uint8_t bytes_per_framesize = 1;
  for (uint32_t mask = 0xff; bytes_per_framesize <= 4; bytes_per_framesize++) {
    if (max_frame_size < mask)
      break;
    mask <<= 8;
    mask |= 0xff;
  }

  uint8_t superframe_header = 0xc0;
  superframe_header |= static_cast<uint8_t>(frame_sizes.size() - 1);
  superframe_header |= (bytes_per_framesize - 1) << 3;
  const size_t index_sz = 2 + bytes_per_framesize * frame_sizes.size();
  std::vector<uint8_t> superframe_index(index_sz);
  size_t pos = 0;
  superframe_index[pos++] = superframe_header;
  for (uint32_t size : frame_sizes) {
    for (int i = 0; i < bytes_per_framesize; i++) {
      superframe_index[pos++] = size & 0xff;
      size >>= 8;
    }
  }
  superframe_index[pos++] = superframe_header;

  return superframe_index;
}

// Overwrites show_frame of each frame. It is set to 1 for the top spatial layer
// or otherwise 0.
bool OverwriteShowFrame(base::span<uint8_t> frame_data,
                        const std::vector<uint32_t>& frame_sizes) {
  size_t sum_frame_size = 0;
  for (uint32_t frame_size : frame_sizes)
    sum_frame_size += frame_size;
  if (frame_data.size() != sum_frame_size) {
    LOG(ERROR) << "frame data size=" << frame_data.size()
               << " is different from the sum of frame sizes"
               << " index size=" << sum_frame_size;
    return false;
  }

  size_t offset = 0;
  for (size_t i = 0; i < frame_sizes.size(); ++i) {
    uint8_t* header = frame_data.data() + offset;

    // See VP9 Spec Annex B.
    const uint8_t frame_marker = (*header >> 6);
    if (frame_marker != 0b10) {
      LOG(ERROR) << "Invalid frame marker: " << static_cast<int>(frame_marker);
      return false;
    }
    const uint8_t profile = (*header >> 4) & 0b11;
    if (profile == 3) {
      LOG(ERROR) << "Unsupported profile";
      return false;
    }

    const bool show_existing_frame = (*header >> 3) & 1;
    const bool show_frame = i == frame_sizes.size() - 1;
    int bit = 0;
    if (show_existing_frame) {
      header++;
      bit = 6;
    } else {
      bit = 1;
    }
    if (show_frame) {
      *header |= (1u << bit);
    } else {
      *header &= ~(1u << bit);
    }

    offset += frame_sizes[i];
  }

  return true;
}
}  // namespace
// If the given resolution is not supported by the driver, some IOCTL must
// return some error code (e.g. EIO). However, there is a driver that doesn't
// follow this specification, for example go2001. This will be called before
// a bitstream to the driver in the driver. This parses the bitstream, gets
// its resolution and compares with the supported resolution.
// Returns true if the resolution is supported or this workaround is
// unnecessary. Otherwise return false.
// This class is currently created only on guado when codec is VP8.
// TODO(crbug.com/968945): Check this workaround is necessary for other codecs
// and other devices.
class SupportResolutionChecker : public V4L2StatefulWorkaround {
 public:
  static std::unique_ptr<V4L2StatefulWorkaround> CreateIfNeeded(
      V4L2Device::Type device_type,
      VideoCodecProfile profile);
  ~SupportResolutionChecker() override = default;

  Result Apply(const uint8_t* data, size_t size) override;

 private:
  using SupportedProfileMap = base::small_map<
      std::map<VideoCodecProfile, VideoDecodeAccelerator::SupportedProfile>>;

  SupportResolutionChecker(SupportedProfileMap supported_profile_map)
      : supported_profile_map_(std::move(supported_profile_map)),
        vp8_parser_(std::make_unique<Vp8Parser>()) {}

  SupportedProfileMap supported_profile_map_;
  const std::unique_ptr<Vp8Parser> vp8_parser_;
};

std::unique_ptr<V4L2StatefulWorkaround>
SupportResolutionChecker::CreateIfNeeded(V4L2Device::Type device_type,
                                         VideoCodecProfile profile) {
  if (device_type != V4L2Device::Type::kDecoder || profile < VP8PROFILE_MIN ||
      profile > VP8PROFILE_MAX) {
    return nullptr;
  }

  auto device = base::MakeRefCounted<V4L2Device>();
  if (!device->Open(V4L2Device::Type::kDecoder, V4L2_PIX_FMT_VP8)) {
    VPLOGF(1) << "Failed to open device for profile: " << profile
              << " fourcc: " << FourccToString(V4L2_PIX_FMT_VP8);
    return nullptr;
  }

  // Get the driver name.
  struct v4l2_capability caps;
  if (device->Ioctl(VIDIOC_QUERYCAP, &caps) != 0) {
    VPLOGF(1) << "ioctl() failed: VIDIOC_QUERYCAP"
              << ", caps check failed: 0x" << std::hex << caps.capabilities;
    return nullptr;
  }
  constexpr char go2001[] = "go2001";
  if (strcmp(reinterpret_cast<const char*>(caps.driver), go2001))
    return nullptr;

  const std::vector<uint32_t> supported_input_fourccs = {
      V4L2_PIX_FMT_VP8,
  };

  // Recreate the V4L2 device in order to close the opened decoder, since
  // we are about to query the supported decode profiles.
  device = base::MakeRefCounted<V4L2Device>();
  auto supported_profiles =
      device->GetSupportedDecodeProfiles(supported_input_fourccs);
  SupportedProfileMap supported_profile_map;
  for (const auto& entry : supported_profiles)
    supported_profile_map[entry.profile] = entry;

  VLOGF(2) << "Create SupportResolutionChecker workaround";
  return base::WrapUnique(
      new SupportResolutionChecker(std::move(supported_profile_map)));
}

V4L2StatefulWorkaround::Result SupportResolutionChecker::Apply(
    const uint8_t* data,
    size_t size) {
  Vp8FrameHeader fhdr;
  vp8_parser_->ParseFrame(data, size, &fhdr);
  if (fhdr.IsKeyframe()) {
    DCHECK(supported_profile_map_.find(VP8PROFILE_ANY) !=
           supported_profile_map_.end());
    const auto& supported_profile = supported_profile_map_[VP8PROFILE_ANY];
    const auto& min_resolution = supported_profile.min_resolution;
    const auto& max_resolution = supported_profile.max_resolution;
    const gfx::Rect current_resolution(fhdr.width, fhdr.height);
    if (!gfx::Rect(max_resolution).Contains(current_resolution) ||
        !(current_resolution).Contains(gfx::Rect(min_resolution))) {
      VLOGF(1) << "Resolution is unsupported: "
               << current_resolution.size().ToString()
               << ", min supported resolution: " << min_resolution.ToString()
               << ", max supported resolution: " << max_resolution.ToString();
      return Result::NotifyError;
    }
  }
  return Result::Success;
}

std::vector<std::unique_ptr<V4L2StatefulWorkaround>>
CreateV4L2StatefulWorkarounds(V4L2Device::Type device_type,
                              VideoCodecProfile profile) {
  using CreateWorkaroundFuncType = std::unique_ptr<V4L2StatefulWorkaround> (*)(
      V4L2Device::Type device_type, VideoCodecProfile profile);
  const CreateWorkaroundFuncType kWorkaroundFactoryFunction[] = {
      &SupportResolutionChecker::CreateIfNeeded,
  };

  std::vector<std::unique_ptr<V4L2StatefulWorkaround>> workarounds;
  for (const auto func : kWorkaroundFactoryFunction) {
    auto vw = func(device_type, profile);
    if (vw)
      workarounds.push_back(std::move(vw));
  }
  return workarounds;
}

bool AppendVP9SuperFrameIndex(scoped_refptr<DecoderBuffer>& buffer) {
  DCHECK(buffer->has_side_data());
  DCHECK(!buffer->side_data()->spatial_layers.empty());

  const size_t num_of_layers = buffer->side_data()->spatial_layers.size();
  if (num_of_layers > 3u) {
    LOG(ERROR) << "The maximum number of spatial layers in VP9 is three";
    return false;
  }

  const uint32_t* cue_data = buffer->side_data()->spatial_layers.data();
  std::vector<uint32_t> frame_sizes(cue_data, cue_data + num_of_layers);
  std::vector<uint8_t> superframe_index = CreateSuperFrameIndex(frame_sizes);
  const size_t vp9_superframe_size =
      buffer->data_size() + superframe_index.size();
  auto vp9_superframe = std::make_unique<uint8_t[]>(vp9_superframe_size);
  memcpy(vp9_superframe.get(), buffer->data(), buffer->data_size());
  memcpy(vp9_superframe.get() + buffer->data_size(), superframe_index.data(),
         superframe_index.size());

  if (!OverwriteShowFrame(
          base::make_span(vp9_superframe.get(), buffer->data_size()),
          frame_sizes)) {
    return false;
  }

  DVLOG(3) << "DecoderBuffer is overwritten";
  buffer =
      DecoderBuffer::FromArray(std::move(vp9_superframe), vp9_superframe_size);

  return true;
}
}  // namespace media
