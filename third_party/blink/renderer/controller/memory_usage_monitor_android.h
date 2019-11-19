// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CONTROLLER_MEMORY_USAGE_MONITOR_ANDROID_H_
#define THIRD_PARTY_BLINK_RENDERER_CONTROLLER_MEMORY_USAGE_MONITOR_ANDROID_H_

#include "base/files/file.h"
#include "base/files/scoped_file.h"
#include "third_party/blink/renderer/controller/controller_export.h"
#include "third_party/blink/renderer/controller/memory_usage_monitor.h"

namespace blink {

class CONTROLLER_EXPORT MemoryUsageMonitorAndroid : public MemoryUsageMonitor {
 public:
  MemoryUsageMonitorAndroid() = default;

  void ReplaceFileDescriptorsForTesting(base::File statm_file,
                                        base::File status_file);

 private:
  friend class CrashMemoryMetricsReporterImpl;
  void ResetFileDescriptors();
  void GetProcessMemoryUsage(MemoryUsage&) override;
  static bool CalculateProcessMemoryFootprint(int statm_fd,
                                              int status_fd,
                                              uint64_t* private_footprint,
                                              uint64_t* swap_footprint,
                                              uint64_t* vm_size,
                                              uint64_t* vm_hwm_size);

  bool file_descriptors_reset_ = false;
  // The file descriptor to current process proc files. The files are kept open
  // when detection is on to reduce measurement overhead.
  base::ScopedFD statm_fd_;
  base::ScopedFD status_fd_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CONTROLLER_MEMORY_USAGE_MONITOR_ANDROID_H_
