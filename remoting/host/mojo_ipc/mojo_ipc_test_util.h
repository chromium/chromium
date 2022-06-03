// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_MOJO_IPC_MOJO_IPC_TEST_UTIL_H_
#define REMOTING_HOST_MOJO_IPC_MOJO_IPC_TEST_UTIL_H_

#include "mojo/public/cpp/platform/named_platform_channel.h"

namespace remoting {
namespace test {

// Generates a random server name for unittests.
mojo::NamedPlatformChannel::ServerName GenerateRandomServerName();

}  // namespace test
}  // namespace remoting

#endif  // REMOTING_HOST_MOJO_IPC_MOJO_IPC_TEST_UTIL_H_
