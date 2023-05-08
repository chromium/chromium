// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/breakpad_utils_linux.h"

namespace remoting {

const char kMinidumpPath[] = "/tmp/chromoting/minidumps";
const char kBreakpadProcessIdKey[] = "process_id";
const char kBreakpadProcessNameKey[] = "process_name";
const char kBreakpadProcessStartTimeKey[] = "process_start_time";
const char kBreakpadProcessUptimeKey[] = "process_uptime";
const char kBreakpadHostVersionKey[] = "host_version";

}  // namespace remoting
