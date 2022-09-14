// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "mojo/core/core.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/test/mojo_test_base.h"
#include "mojo/public/c/system/types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace core {
namespace {

using NodeControllerTest = test::MojoTestBase;

TEST_F(NodeControllerTest, AcceptInvitationFailure) {
  if (IsMojoIpczEnabled()) {
    GTEST_SKIP() << "Not relevant when MojoIpcz is enabled.";
  }

  // Spawn a child process that will send an invalid AcceptInvitation
  // NodeChannel message. This is a regression test for
  // https://crbug.com/1162198.
  RunTestClient("SendInvalidAcceptInvitation",
                [&](MojoHandle h) { WriteMessage(h, "hi"); });
}

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(SendInvalidAcceptInvitation,
                                  NodeControllerTest,
                                  h) {
  // A little communication to synchronize against Mojo bringup. By the time
  // this read completes, we must have an internal NodeController with the
  // parent test process connected as its broker.
  EXPECT_EQ("hi", ReadMessage(h));

  // Send an unexpected AcceptInvitation message to the parent process. This
  // exercises the regression code path in the parent process.
  NodeController* controller = Core::Get()->GetNodeController();
  scoped_refptr<NodeChannel> channel = controller->GetBrokerChannel();
  channel->AcceptInvitation(ports::NodeName{0, 0}, ports::NodeName{0, 0});
  MojoClose(h);
}

}  // namespace
}  // namespace core
}  // namespace mojo
