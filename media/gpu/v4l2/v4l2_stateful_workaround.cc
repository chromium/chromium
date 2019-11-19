// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/v4l2_stateful_workaround.h"

#include <string.h>

#include <linux/videodev2.h>

#include "base/containers/small_map.h"
#include "base/memory/ptr_util.h"
#include "media/base/video_types.h"
#include "media/gpu/macros.h"
#include "media/parsers/vp8_parser.h"
#include "media/video/video_decode_accelerator.h"

namespace media {

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

  Result Apply(const uint8_t* data, size_t size, size_t* endpos) override;

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

  scoped_refptr<V4L2Device> device = V4L2Device::Create();
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

  constexpr uint32_t supported_input_fourccs[] = {
      V4L2_PIX_FMT_VP8,
  };

  // Recreate the V4L2 device in order to close the opened decoder, since
  // we are about to query the supported decode profiles.
  device = V4L2Device::Create();
  auto supported_profiles = device->GetSupportedDecodeProfiles(
      base::size(supported_input_fourccs), supported_input_fourccs);
  SupportedProfileMap supported_profile_map;
  for (const auto& profile : supported_profiles)
    supported_profile_map[profile.profile] = profile;

  VLOGF(2) << "Create SupportResolutionChecker workaround";
  return base::WrapUnique(
      new SupportResolutionChecker(std::move(supported_profile_map)));
}

V4L2StatefulWorkaround::Result SupportResolutionChecker::Apply(
    const uint8_t* data,
    size_t size,
    size_t* endpos) {
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

}  // namespace media
