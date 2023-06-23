// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/test/hevc_decoder.h"

#include "base/notreached.h"

namespace media {
namespace v4l2_test {

HevcDecoder::HevcDecoder(std::unique_ptr<V4L2IoctlShim> v4l2_ioctl,
                         gfx::Size display_resolution,
                         const base::MemoryMappedFile& data_stream)
    : VideoDecoder::VideoDecoder(std::move(v4l2_ioctl), display_resolution) {}

HevcDecoder::~HevcDecoder() = default;

// static
std::unique_ptr<HevcDecoder> HevcDecoder::Create(
    const base::MemoryMappedFile& stream) {
  NOTIMPLEMENTED();
  return nullptr;
}

VideoDecoder::Result HevcDecoder::DecodeNextFrame(const int frame_number,
                                                  std::vector<uint8_t>& y_plane,
                                                  std::vector<uint8_t>& u_plane,
                                                  std::vector<uint8_t>& v_plane,
                                                  gfx::Size& size) {
  NOTIMPLEMENTED();
  return VideoDecoder::kOk;
}
}  // namespace v4l2_test
}  // namespace media
