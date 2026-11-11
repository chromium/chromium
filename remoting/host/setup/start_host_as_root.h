// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SETUP_START_HOST_AS_ROOT_H_
#define REMOTING_HOST_SETUP_START_HOST_AS_ROOT_H_

#include <string>

namespace base {
class CommandLine;
}  // namespace base

namespace remoting {

class HostType;

// Returns the local account that the host is set up for, taken from
// `--user-name`, or else derived from `--corp-user` or `--cloud-user`, in that
// order. Returns an empty string if none of these switches is present, if
// `--user-name` is empty, or if `--cloud-user` is not an email address.
// The returned name is not validated, see CheckLocalUserForHostTypeSetup().
std::string GetLocalUserNameFromCommandLine(
    const base::CommandLine& command_line);

// Sets up the single-process host of type `host_type` as root: registers the
// host as the local user, then runs the start hook of `host_type` (see
// HostType::RunStartHostHook()). The caller is responsible for validating the
// local user from the command line (e.g. with
// CheckLocalUserForHostTypeSetup()).
int StartHostAsRoot(const HostType& host_type);

}  // namespace remoting

#endif  // REMOTING_HOST_SETUP_START_HOST_AS_ROOT_H_
