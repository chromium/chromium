// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CHROMEOS_BROWSER_INTEROP_H_
#define REMOTING_HOST_CHROMEOS_BROWSER_INTEROP_H_

#include <memory>

namespace remoting {

class ChromotingHostContext;
class PolicyWatcher;

// This file provides a set of helper functions which rely on the browser
// process. They are broken out into this file in order to prevent cyclical
// dependencies in the other CRD Chrome OS build targets which the browser
// process has a dependency on (e.g. RemotingService).

// Must be called on the main/UI sequence.
extern std::unique_ptr<ChromotingHostContext> CreateChromotingHostContext();

// Can be called on any sequence.
extern std::unique_ptr<PolicyWatcher> CreatePolicyWatcher();

}  // namespace remoting

#endif  // REMOTING_HOST_CHROMEOS_BROWSER_INTEROP_H_
