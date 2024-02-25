// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_MAC_SANDBOX_CRASH_MESSAGE_H_
#define SANDBOX_MAC_SANDBOX_CRASH_MESSAGE_H_

namespace sandbox::crash_message {

// A simple wrapper around macOS's crash reporter annotations

// We use macOS's crash reporter annotations rather than crash keys to avoid
// linking against //base. Sandbox should not link against libbase because
// libbase brings in numerous system libraries that increase the attack surface
// of the sandbox code.
//
// Both the system CrashReporter and Crashpad collect these annotations.

void SetCrashMessage(const char* message);

}  // namespace sandbox::crash_message

#endif  // SANDBOX_MAC_SANDBOX_CRASH_MESSAGE_H_
