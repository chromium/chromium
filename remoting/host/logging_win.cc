// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/logging.h"

#include <guiddef.h>

#include "base/logging.h"
#include "base/logging_win.h"

// {2db51ca1-4fd8-4b88-b5a2-fb8606b66b02}
const GUID kRemotingHostLogProvider =
    { 0x2db51ca1, 0x4fd8, 0x4b88,
        { 0xb5, 0xa2, 0xfb, 0x86, 0x06, 0xb6, 0x6b, 0x02 } };

namespace remoting {

void InitHostLogging() {
  // Write logs to the system debug log.
  logging::LoggingSettings settings;
  settings.logging_dest =
      logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
  logging::InitLogging(settings);

  // Enable trace control and transport through event tracing for Windows.
  logging::LogEventProvider::Initialize(kRemotingHostLogProvider);
}

}  // namespace remoting
