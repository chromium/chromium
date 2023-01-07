// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/logging.h"

#include "base/command_line.h"

namespace remoting {

void InitHostLoggingCommon() {
#ifdef WEBRTC_VERBOSE_LOGGING
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII("vmodule",
                                                            "*/webrtc/*=1");
#endif
}

}  // namespace remoting
