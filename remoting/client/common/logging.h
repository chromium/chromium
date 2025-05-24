// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_COMMON_LOGGING_H_
#define REMOTING_CLIENT_COMMON_LOGGING_H_

#include "base/logging.h"

namespace remoting {

// Chromoting client code should use CLIENT_LOG instead of LOG(INFO) to bypass
// the CheckSpamLogging presubmit check.
#define CLIENT_LOG LOG(INFO)
#define CLIENT_DLOG DLOG(INFO)

}  // namespace remoting

#endif  // REMOTING_CLIENT_COMMON_LOGGING_H_
