// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_IPC_CONSTANTS_H_
#define REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_IPC_CONSTANTS_H_

#include "mojo/public/cpp/platform/named_platform_channel.h"

namespace remoting {

// Used to indicate an error during a security key forwarding session.
extern const char kSecurityKeyConnectionError[];

}  // namespace remoting

#endif  // REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_IPC_CONSTANTS_H_
