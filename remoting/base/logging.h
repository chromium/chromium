// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_LOGGING_H_
#define REMOTING_BASE_LOGGING_H_

#include "base/logging.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <guiddef.h>
#endif

namespace remoting {

// Chromoting host code should use HOST_LOG instead of LOG(INFO) to bypass the
// CheckSpamLogging presubmit check. This won't spam chrome output because it
// runs in the chromoting host processes.
// In the future we may also consider writing to a log file instead of the
// console.
#define HOST_LOG LOG(INFO)
#define HOST_DLOG DLOG(INFO)

#if BUILDFLAG(IS_WIN)
// {2db51ca1-4fd8-4b88-b5a2-fb8606b66b02}
constexpr GUID kRemotingHostLogProviderGuid = {
    0x2db51ca1,
    0x4fd8,
    0x4b88,
    {0xb5, 0xa2, 0xfb, 0x86, 0x06, 0xb6, 0x6b, 0x02}};
#endif

// Initializes host logging.
void InitHostLogging();

}  // namespace remoting

#endif  // REMOTING_BASE_LOGGING_H_
