// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_AC3_AC3_UTIL_H_
#define MEDIA_FORMATS_AC3_AC3_UTIL_H_

#include <stddef.h>
#include <stdint.h>

#include "media/base/media_export.h"

namespace media {

class MEDIA_EXPORT Ac3Util {
 public:
  Ac3Util(const Ac3Util&) = delete;
  Ac3Util& operator=(const Ac3Util&) = delete;

  // Returns the total number of audio samples in the given buffer, which
  // contains several complete AC3 syncframes.
  static int ParseTotalAc3SampleCount(const uint8_t* data, size_t size);

  // Returns the total number of audio samples in the given buffer, which
  // contains several complete E-AC3 syncframes.
  static int ParseTotalEac3SampleCount(const uint8_t* data, size_t size);
};

}  // namespace media

#endif  // MEDIA_FORMATS_AC3_AC3_UTIL_H_
