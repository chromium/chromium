// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_ACL_UTIL_H_
#define REMOTING_HOST_WIN_ACL_UTIL_H_

#include <windows.h>

#include "base/win/sid.h"

namespace remoting {

// Adds new access right to the current process for |type|. Returns a boolean
// indicating whether the operation was successful.
bool AddProcessAccessRightForWellKnownSid(
    base::win::WellKnownSid well_known_sid,
    DWORD new_right);

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_ACL_UTIL_H_
