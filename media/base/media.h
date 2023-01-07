// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains code that should be used for initializing, or querying the state
// of the media library as a whole.

#ifndef MEDIA_BASE_MEDIA_H_
#define MEDIA_BASE_MEDIA_H_

#include <stdint.h>

#include "build/build_config.h"
#include "media/base/media_export.h"

namespace media {

// Initializes media libraries (e.g. ffmpeg) as well as CPU specific media
// features.
MEDIA_EXPORT void InitializeMediaLibrary();

// Same as InitializeMediaLibrary() but specifies the CPU flags used by libyuv
// and ffmpeg (libavutil). Retrieving these flags may access the file system
// (/proc/cpuinfo) which won't work in sandboxed processes. For such processes,
// a non sandboxed process should retrieve these flags in advance (via
// libyuv::InitCpuFlags() and av_get_cpu_flags()) and pass them to the sandboxed
// process that should then call this method.
MEDIA_EXPORT void InitializeMediaLibraryInSandbox(int64_t libyuv_cpu_flags,
                                                  int64_t libavutil_cpu_flags);

}  // namespace media

#endif  // MEDIA_BASE_MEDIA_H_
