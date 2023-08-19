// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_MAC_PERMISSION_PROCESS_UTILS_H_
#define REMOTING_HOST_MAC_PERMISSION_PROCESS_UTILS_H_

// Utilities for doing permission-checks in a separate process from the
// ME2ME or IT2ME host binaries. These checks are carried out by running the
// relevant binary (the one that needs the permission) with a command-line
// option, and examining the returned exit-code.

namespace remoting::mac {

enum class HostMode { ME2ME, IT2ME };

// These methods must be called on a thread which allows blocking I/O.
bool CheckAccessibilityPermission(HostMode mode);
bool CheckScreenRecordingPermission(HostMode mode);

}  // namespace remoting::mac

#endif  // REMOTING_HOST_MAC_PERMISSION_PROCESS_UTILS_H_
