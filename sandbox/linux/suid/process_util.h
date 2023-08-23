// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The following is duplicated from base/process_utils.h.
// We shouldn't link against C++ code in a setuid binary.

#ifndef SANDBOX_LINUX_SUID_PROCESS_UTIL_H_
#define SANDBOX_LINUX_SUID_PROCESS_UTIL_H_

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

// This adjusts /proc/process/oom_score_adj so the Linux OOM killer
// will prefer certain process types over others. The range for the
// adjustment is [-1000, 1000], with [0, 1000] being user accessible.
//
// If the Linux system isn't new enough to use oom_score_adj, then we
// try to set the older oom_adj value instead, scaling the score to
// the required range of [0, 15]. This may result in some aliasing of
// values, of course.
bool AdjustOOMScore(pid_t process, int score);

#endif  // SANDBOX_LINUX_SUID_PROCESS_UTIL_H_
