// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/test/video_decoder.h"

namespace media {
namespace vaapi_test {

VideoDecoder::VideoDecoder(const VaapiDevice& va_device,
                           SharedVASurface::FetchPolicy fetch_policy)
    : va_device_(va_device), fetch_policy_(fetch_policy) {}

VideoDecoder::~VideoDecoder() {
  // The implementation should have destroyed everything in the right order,
  // including |last_decoded_surface_|.
  // Just confirm here that that destruction was indeed done.
  DCHECK(!last_decoded_surface_);
}

}  // namespace vaapi_test
}  // namespace media
