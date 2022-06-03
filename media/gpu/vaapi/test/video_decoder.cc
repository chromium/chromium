// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/test/video_decoder.h"

#include "media/filters/ivf_parser.h"

namespace media {
namespace vaapi_test {

VideoDecoder::VideoDecoder(std::unique_ptr<IvfParser> ivf_parser,
                           const VaapiDevice& va_device,
                           SharedVASurface::FetchPolicy fetch_policy)
    : ivf_parser_(std::move(ivf_parser)),
      va_device_(va_device),
      fetch_policy_(fetch_policy) {}

VideoDecoder::~VideoDecoder() {
  // The implementation should have destroyed everything in the right order,
  // including |last_decoded_surface_|.
  // Just confirm here that that destruction was indeed done.
  DCHECK(!last_decoded_surface_);
}

}  // namespace vaapi_test
}  // namespace media
