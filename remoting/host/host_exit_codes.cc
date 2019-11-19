// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_exit_codes.h"

#include "remoting/base/name_value_map.h"

namespace remoting {

const NameMapElement<HostExitCodes> kHostExitCodeStrings[] = {
    {kSuccessExitCode, "SUCCESS_EXIT"},
    {kInitializationFailed, "INITIALIZATION_FAILED"},
    {kInvalidCommandLineExitCode, "INVALID_COMMAND_LINE"},
    {kNoPermissionExitCode, "NO_PERMISSION"},
    {kInvalidHostConfigurationExitCode, "INVALID_HOST_CONFIGURATION"},
    {kInvalidHostIdExitCode, "INVALID_HOST_ID"},
    {kInvalidOauthCredentialsExitCode, "INVALID_OAUTH_CREDENTIALS"},
    {kInvalidHostDomainExitCode, "INVALID_HOST_DOMAIN"},
    {kLoginScreenNotSupportedExitCode, "LOGIN_SCREEN_NOT_SUPPORTED"},
    {kUsernameMismatchExitCode, "USERNAME_MISMATCH"},
    {kHostDeletedExitCode, "HOST_DELETED"},
};

const char* ExitCodeToString(HostExitCodes exit_code) {
  return ValueToName(kHostExitCodeStrings, exit_code);
}

const char* ExitCodeToStringUnchecked(int exit_code) {
  return ValueToNameUnchecked(kHostExitCodeStrings,
                              static_cast<HostExitCodes>(exit_code));
}

}  // namespace remoting
