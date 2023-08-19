// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_MACROS_H_
#define MEDIA_GPU_MACROS_H_

#include "base/logging.h"

// Try to adhere to [1] when adding and using logging.
// [1]
// https://chromium.googlesource.com/chromium/src/+/main/media/README.md#dvlog

#define DVLOGF(level) DVLOG(level) << __func__ << "(): "
#define DVLOGF_IF(level, condition) \
  DVLOG_IF(level, condition) << __func__ << "(): "
#define VLOGF(level) VLOG(level) << __func__ << "(): "
#define VPLOGF(level) VPLOG(level) << __func__ << "(): "
#define LOGF(severity) LOG(severity) << __func__ << "(): "
#define DLOGF(severity) DLOG(severity) << __func__ << "(): "
#define DLOGF_IF(severity, condition) \
  DLOG_IF(severity, condition) << __func__ << "(): "

namespace media {

// Copy the memory between arrays with checking the array size.
template <typename T, size_t N>
inline void SafeArrayMemcpy(T (&to)[N], const T (&from)[N]) {
  memcpy(to, from, sizeof(T[N]));
}

}  // namespace media

#endif  // MEDIA_GPU_MACROS_H_
