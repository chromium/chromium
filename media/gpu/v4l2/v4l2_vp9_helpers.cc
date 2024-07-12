// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/v4l2_vp9_helpers.h"

#include "base/containers/heap_array.h"
#include "base/logging.h"

namespace media {
namespace {
// Creates superframe index from |frame_sizes|. The frame sizes is stored in the
// same bytes. For example, if the max frame size is two bytes, even if the
// smaller frame sizes are 1 byte, they are stored as two bytes. See the detail
// for VP9 Spec Annex B.
std::vector<uint8_t> CreateSuperFrameIndex(
    const std::vector<uint32_t>& frame_sizes) {
  if (frame_sizes.size() < 2) {
    return {};
  }

  // Computes the bytes of the maximum frame size.
  const uint32_t max_frame_size =
      *std::max_element(frame_sizes.begin(), frame_sizes.end());
  uint8_t bytes_per_framesize = 1;
  for (uint32_t mask = 0xff; bytes_per_framesize <= 4; bytes_per_framesize++) {
    if (max_frame_size < mask) {
      break;
    }
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
  for (uint32_t frame_size : frame_sizes) {
    sum_frame_size += frame_size;
  }
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
  const size_t vp9_superframe_size = buffer->size() + superframe_index.size();
  auto vp9_superframe = base::HeapArray<uint8_t>::Uninit(vp9_superframe_size);
  memcpy(vp9_superframe.data(), buffer->data(), buffer->size());
  memcpy(vp9_superframe.data() + buffer->size(), superframe_index.data(),
         superframe_index.size());

  if (!OverwriteShowFrame(vp9_superframe, frame_sizes)) {
    return false;
  }

  DVLOG(3) << "DecoderBuffer is overwritten";
  buffer = DecoderBuffer::FromArray(std::move(vp9_superframe));

  return true;
}
}  // namespace media
