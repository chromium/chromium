// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WEBAUTHN_REMOTE_WEBAUTHN_CALLER_SECURITY_UTILS_H_
#define REMOTING_HOST_WEBAUTHN_REMOTE_WEBAUTHN_CALLER_SECURITY_UTILS_H_

namespace base {
class CommandLine;
}  // namespace base

namespace remoting {

// Returns true if the current process is launched by a trusted process.
bool IsLaunchedByTrustedProcess();

// Returns true if the calling extension origin, which Chrome passes as the
// first positional argument when launching a native messaging host, belongs to
// one of the allowed remote WebAuthn extensions. This must be used together
// with IsLaunchedByTrustedProcess(), which establishes that the arguments were
// supplied by Chrome.
//
// Note that the arguments are only as trustworthy as the launch chain. A
// same-user attacker who can register a native messaging host wrapper (e.g. a
// script that launches this binary) can still spoof them.
bool IsLaunchedByTrustedExtension(const base::CommandLine& command_line);

}  // namespace remoting

#endif  // REMOTING_HOST_WEBAUTHN_REMOTE_WEBAUTHN_CALLER_SECURITY_UTILS_H_
