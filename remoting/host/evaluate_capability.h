// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_EVALUATE_CAPABILITY_H_
#define REMOTING_HOST_EVALUATE_CAPABILITY_H_

#include <string>

namespace remoting {

// Evaluates the host capability in current process. This function should only
// be called in HostMain(), which consumes a --type=evaluate command line
// parameter. Note, this function may execute some experimental features and
// crash the process.
int EvaluateCapabilityLocally(const std::string& type);

// Evaluates the host capability in a different process and returns its exit
// code. If |output| is provided, it will be set to the stdout of the process.
// If the process failed to be started, though usually this should not happen,
// it returns TERMINATION_STATUS_LAUNCH_FAILED.
// Note, this is a blocking call. Depending on the platform and system load, it
// may take a noticeable amount of time to complete.
int EvaluateCapability(const std::string& type, std::string* output = nullptr);

}  // namespace remoting

#endif  // REMOTING_HOST_EVALUATE_CAPABILITY_H_
