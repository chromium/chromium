// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/notimplemented.h"
#include "remoting/host/base/host_exit_codes.h"

namespace remoting {

int DaemonProcessMain() {
  // TODO(crbug.com/499082867): Implement DaemonProcessMac.
  NOTIMPLEMENTED();
  return kInitializationFailed;
}

}  // namespace remoting
