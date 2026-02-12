// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_PAM_UTILS_H_
#define REMOTING_HOST_PAM_UTILS_H_

#include "base/strings/cstring_view.h"

namespace remoting {

// Calls PAM to determine if `username` is allowed for local login.
bool IsLocalLoginAllowed(base::cstring_view username);

}  // namespace remoting

#endif  // REMOTING_HOST_PAM_UTILS_H_
