// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/log/net_log_capture_mode.h"

namespace net {

bool NetLogCaptureIncludesSensitive(NetLogCaptureMode capture_mode) {
  return capture_mode >= NetLogCaptureMode::kIncludeSensitive;
}

bool NetLogCaptureIncludesSocketBytes(NetLogCaptureMode capture_mode) {
  return capture_mode == NetLogCaptureMode::kEverything;
}

}  // namespace net
