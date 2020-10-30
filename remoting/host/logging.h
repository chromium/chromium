// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LOGGING_H_
#define REMOTING_HOST_LOGGING_H_

#include "base/logging.h"
#include "build/build_config.h"

#if defined(OS_WIN)
#include <guiddef.h>
#endif

namespace remoting {

#if defined(OS_WIN)
// {2db51ca1-4fd8-4b88-b5a2-fb8606b66b02}
constexpr GUID kRemotingHostLogProviderGuid = {
    0x2db51ca1,
    0x4fd8,
    0x4b88,
    {0xb5, 0xa2, 0xfb, 0x86, 0x06, 0xb6, 0x6b, 0x02}};
#endif

// Initializes host logging.
extern void InitHostLogging();

}  // namespace remoting

#endif  // REMOTING_HOST_LOGGING_H_
