// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/logging_internal.h"

#include <guiddef.h>

#include "base/logging.h"
#include "base/logging_win.h"
#include "remoting/base/logging.h"

namespace remoting {

void InitHostLogging() {
  InitHostLoggingCommon();

  // Write logs to the system debug log.
  logging::LoggingSettings settings;
  settings.logging_dest =
      logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
  logging::InitLogging(settings);

  // Enable trace control and transport through event tracing for Windows.
  logging::LogEventProvider::Initialize(kRemotingHostLogProviderGuid);
}

}  // namespace remoting
