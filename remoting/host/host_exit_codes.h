// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HOST_EXIT_CODES_H_
#define REMOTING_HOST_HOST_EXIT_CODES_H_

namespace remoting {

// Known host exit codes. The exit codes indicating permanent errors must be in
// sync with:
//  - remoting/host/installer/mac/PrivilegedHelperTools/
//      org.chromium.chromoting.me2me.sh
//  - remoting/host/linux/linux_me2me_host.py
enum HostExitCodes {
  // Error codes that don't indicate a permanent error condition.
  kSuccessExitCode = 0,
  kReservedForX11ExitCode = 1,
  kInitializationFailed = 2,
  kInvalidCommandLineExitCode = 3,
  kNoPermissionExitCode = 4,

  // Error codes that do indicate a permanent error condition.
  kInvalidHostConfigurationExitCode = 100,
  kInvalidHostIdExitCode = 101,
  kInvalidOauthCredentialsExitCode = 102,
  kInvalidHostDomainExitCode = 103,
  kLoginScreenNotSupportedExitCode = 104,
  kUsernameMismatchExitCode = 105,
  kHostDeletedExitCode = 106,

  // The range of the exit codes that should be interpreted as a permanent error
  // condition.
  kMinPermanentErrorExitCode = kInvalidHostConfigurationExitCode,
  kMaxPermanentErrorExitCode = kHostDeletedExitCode
};

const char* ExitCodeToString(HostExitCodes exit_code);

// Returns nullptr if |exit_code| is not a HostExitCodes.
const char* ExitCodeToStringUnchecked(int exit_code);

}  // namespace remoting

#endif  // REMOTING_HOST_HOST_EXIT_CODES_H_
