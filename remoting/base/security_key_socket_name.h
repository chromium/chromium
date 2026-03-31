// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_SECURITY_KEY_SOCKET_NAME_H_
#define REMOTING_BASE_SECURITY_KEY_SOCKET_NAME_H_

#include "base/files/file_path.h"
#include "build/build_config.h"
#include "remoting/base/remoting_export.h"

namespace remoting {

// Returns the default security key socket name.
REMOTING_EXPORT base::FilePath GetDefaultSecurityKeySocketName();

// Sets an override for the default security key socket name, for testing.
REMOTING_EXPORT void SetDefaultSecurityKeySocketNameForTest(
    const base::FilePath& path);

}  // namespace remoting

#endif  // REMOTING_BASE_SECURITY_KEY_SOCKET_NAME_H_
