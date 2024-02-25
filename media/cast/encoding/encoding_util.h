// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_ENCODING_ENCODING_UTIL_H_
#define MEDIA_CAST_ENCODING_ENCODING_UTIL_H_

#include <cstdint>

namespace media::cast {

// Returns the drop frame threshold for a video encoder.
// TODO(b/280363228): Clean up code and shares the threshold function with other
// use cases.
uint8_t GetEncoderDropFrameThreshold();

}  // namespace media::cast
#endif  // MEDIA_CAST_ENCODING_ENCODING_UTIL_H_
