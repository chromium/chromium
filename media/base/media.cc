// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media.h"

#include <stdint.h>

#include <limits>

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/no_destructor.h"
#include "base/trace_event/trace_event.h"
#include "media/base/libaom_thread_wrapper.h"
#include "media/base/libvpx_thread_wrapper.h"
#include "media/base/media_switches.h"
#include "media/media_buildflags.h"
#include "partition_alloc/buildflags.h"
#include "third_party/libyuv/include/libyuv.h"

#if BUILDFLAG(ENABLE_FFMPEG)
#include "third_party/ffmpeg/ffmpeg_features.h"  // nogncheck
extern "C" {
#include <libavutil/cpu.h>
#include <libavutil/log.h>
#include <libavutil/mem.h>
}
#endif

namespace media {

// Media must only be initialized once; use a thread-safe static to do this.
class MediaInitializer {
 public:
  MediaInitializer() {
    // Initializing the CPU flags may query /proc for details on the current CPU
    // for NEON, VFP, etc optimizations. If in a sandboxed process, they should
    // have been forced (see InitializeMediaLibraryInSandbox).
    libyuv::InitCpuFlags();

#if BUILDFLAG(ENABLE_FFMPEG)
    av_get_cpu_flags();

    // Disable logging as it interferes with layout tests.
    av_log_set_level(AV_LOG_QUIET);

#if PA_BUILDFLAG(USE_ALLOCATOR_SHIM)
    // Remove allocation limit from ffmpeg, so calls go down to shim layer.
    av_max_alloc(std::numeric_limits<size_t>::max());
#endif  // PA_BUILDFLAG(USE_ALLOCATOR_SHIM)

#endif  // BUILDFLAG(ENABLE_FFMPEG)

#if BUILDFLAG(ENABLE_LIBVPX)
    if (base::FeatureList::IsEnabled(kLibvpxUseChromeThreads)) {
      InitLibVpxThreadWrapper();
    }
#endif  // BUILDFLAG(ENABLE_LIBVPX)
#if BUILDFLAG(ENABLE_LIBAOM)
    if (base::FeatureList::IsEnabled(kLibaomUseChromeThreads)) {
      InitLibAomThreadWrapper();
    }
#endif  // BUILDFLAG(ENABLE_LIBAOM)
  }

  MediaInitializer(const MediaInitializer&) = delete;
  MediaInitializer& operator=(const MediaInitializer&) = delete;

 private:
  ~MediaInitializer() = delete;
};

static const MediaInitializer& GetMediaInstance() {
  static const base::NoDestructor<MediaInitializer> instance;
  return *instance;
}

void InitializeMediaLibrary() {
  GetMediaInstance();
}

void InitializeMediaLibraryInSandbox(int64_t libyuv_cpu_flags,
                                     int64_t libavutil_cpu_flags) {
  // Force the CPU flags so when they don't require disk access when queried
  // from MediaInitializer().
  libyuv::SetCpuFlags(libyuv_cpu_flags);
#if BUILDFLAG(ENABLE_FFMPEG)
  av_force_cpu_flags(libavutil_cpu_flags);
#endif
  GetMediaInstance();
}

}  // namespace media
