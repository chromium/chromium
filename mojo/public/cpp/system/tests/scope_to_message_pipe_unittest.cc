// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/system/scope_to_message_pipe.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace {

class RunCallbackOnDestruction {
 public:
  explicit RunCallbackOnDestruction(base::OnceClosure destruction_callback)
      : destruction_callback_(std::move(destruction_callback)) {}
  ~RunCallbackOnDestruction() { std::move(destruction_callback_).Run(); }

 private:
  base::OnceClosure destruction_callback_;

  DISALLOW_COPY_AND_ASSIGN(RunCallbackOnDestruction);
};

class ScopeToMessagePipeTest : public testing::Test {
 public:
  ScopeToMessagePipeTest() = default;
  ~ScopeToMessagePipeTest() override = default;

 private:
  base::test::TaskEnvironment task_environment_;
  DISALLOW_COPY_AND_ASSIGN(ScopeToMessagePipeTest);
};

TEST_F(ScopeToMessagePipeTest, ObjectDestroyedOnPeerClosure) {
  base::RunLoop wait_for_destruction;
  MessagePipe pipe;
  ScopeToMessagePipe(std::make_unique<RunCallbackOnDestruction>(
                         wait_for_destruction.QuitClosure()),
                     std::move(pipe.handle0));
  pipe.handle1.reset();
  wait_for_destruction.Run();
}

TEST_F(ScopeToMessagePipeTest, PipeClosedOnPeerClosure) {
  base::RunLoop wait_for_pipe_closure;
  MessagePipe pipe;
  SimpleWatcher watcher(FROM_HERE, SimpleWatcher::ArmingPolicy::AUTOMATIC);
  watcher.Watch(pipe.handle1.get(), MOJO_HANDLE_SIGNAL_READABLE,
                MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                base::BindLambdaForTesting(
                    [&](MojoResult result, const HandleSignalsState& state) {
                      EXPECT_EQ(result, MOJO_RESULT_CANCELLED);
                      wait_for_pipe_closure.Quit();
                    }));

  ScopeToMessagePipe(42, std::move(pipe.handle0));
  pipe.handle1.reset();
  wait_for_pipe_closure.Run();
}

}  // namespace
}  // namespace mojo
