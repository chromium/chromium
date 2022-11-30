// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_PROFILING_H_
#define CONTENT_PUBLIC_COMMON_PROFILING_H_

#include "build/build_config.h"

#include "base/debug/profiler.h"
#include "content/common/content_export.h"

namespace content {

// The Profiling class manages the interaction with a sampling based profiler.
// Its function is controlled by the kProfilingAtStart, kProfilingFile, and
// kProfilingFlush command line values.
// All of the API should only be called from the main thread of the process.
class CONTENT_EXPORT Profiling {
 public:
  Profiling(const Profiling&) = delete;
  Profiling& operator=(const Profiling&) = delete;

  // Called early in a process' life to allow profiling of startup time.
  // the presence of kProfilingAtStart is checked.
  static void ProcessStarted();

  // Start profiling.
  static void Start();

  // Stop profiling and write out profiling file.
  static void Stop();

  // Returns true if the process is being profiled.
  static bool BeingProfiled();

  // Toggle profiling on/off.
  static void Toggle();

 private:
  // Do not instantiate this class.
  Profiling();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_PROFILING_H_
