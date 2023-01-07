// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WEBAUTHN_REMOTE_WEBAUTHN_CALLER_SECURITY_UTILS_H_
#define REMOTING_HOST_WEBAUTHN_REMOTE_WEBAUTHN_CALLER_SECURITY_UTILS_H_

namespace remoting {

// Returns true if the current process is launched by a trusted process.
bool IsLaunchedByTrustedProcess();

}  // namespace remoting

#endif  // REMOTING_HOST_WEBAUTHN_REMOTE_WEBAUTHN_CALLER_SECURITY_UTILS_H_
