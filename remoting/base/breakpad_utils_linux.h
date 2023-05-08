// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_BREAKPAD_UTILS_LINUX_H_
#define REMOTING_BASE_BREAKPAD_UTILS_LINUX_H_

#include "base/threading/thread_restrictions.h"

namespace remoting {

extern const char kMinidumpPath[];
extern const char kBreakpadProcessIdKey[];
extern const char kBreakpadProcessNameKey[];
extern const char kBreakpadProcessStartTimeKey[];
extern const char kBreakpadProcessUptimeKey[];
extern const char kBreakpadHostVersionKey[];

class ScopedAllowBlockingForCrashReporting : public base::ScopedAllowBlocking {
};

}  // namespace remoting

#endif  // REMOTING_BASE_BREAKPAD_UTILS_LINUX_H_
