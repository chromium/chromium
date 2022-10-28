// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_MACROS_H_
#define MEDIA_GPU_MACROS_H_

#include "base/logging.h"

#define DVLOGF(level) DVLOG(level) << __func__ << "(): "
#define VLOGF(level) VLOG(level) << __func__ << "(): "
#define VPLOGF(level) VPLOG(level) << __func__ << "(): "
#define LOGF(severity) LOG(severity) << __func__ << "(): "

namespace media {

// Copy the memory between arrays with checking the array size.
template <typename T, size_t N>
inline void SafeArrayMemcpy(T (&to)[N], const T (&from)[N]) {
  memcpy(to, from, sizeof(T[N]));
}

}  // namespace media

#endif  // MEDIA_GPU_MACROS_H_
