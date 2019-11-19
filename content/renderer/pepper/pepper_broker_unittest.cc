// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_broker.h"

#if defined(OS_POSIX)
#include <fcntl.h>
#include <sys/socket.h>
#endif  // defined(OS_POSIX)

#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "content/test/mock_render_process.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class PepperBrokerTest : public ::testing::Test {
 protected:
  PepperBrokerTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}
  base::test::TaskEnvironment task_environment_;
  // We need a render process for ppapi::proxy::ProxyChannel to work.
  MockRenderProcess mock_process_;
};

// Try to initialize PepperBrokerDispatcherWrapper with invalid ChannelHandle.
// Initialization should fail.
TEST_F(PepperBrokerTest, InitFailure) {
  PepperBrokerDispatcherWrapper dispatcher_wrapper;
  IPC::ChannelHandle invalid_channel;  // Invalid by default.

  // An invalid handle should result in a failure (false) without a LOG(FATAL),
  // such as the one in CreatePipe().  Call it twice because without the invalid
  // handle check, the posix code would hit a one-time path due to a static
  // variable and go through the LOG(FATAL) path.
  EXPECT_FALSE(dispatcher_wrapper.Init(base::kNullProcessId, invalid_channel));
  EXPECT_FALSE(dispatcher_wrapper.Init(base::kNullProcessId, invalid_channel));
}

// On valid ChannelHandle, initialization should succeed.
TEST_F(PepperBrokerTest, InitSuccess) {
  PepperBrokerDispatcherWrapper dispatcher_wrapper;
  mojo::MessagePipe pipe;
  IPC::ChannelHandle valid_channel(pipe.handle0.release());

  EXPECT_TRUE(dispatcher_wrapper.Init(base::kNullProcessId, valid_channel));
}

}  // namespace content
