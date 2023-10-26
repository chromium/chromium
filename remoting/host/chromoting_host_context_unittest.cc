// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromoting_host_context.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

// A simple test that starts and stops the context. This tests the context
// operates properly and all threads and message loops are valid.
TEST(ChromotingHostContextTest, StartAndStop) {
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::MainThreadType::UI};
  base::RunLoop run_loop;

  scoped_refptr<network::TestSharedURLLoaderFactory> test_url_loader_factory;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  test_url_loader_factory = new network::TestSharedURLLoaderFactory();
#endif

  std::unique_ptr<ChromotingHostContext> context =
      ChromotingHostContext::CreateForTesting(
          new AutoThreadTaskRunner(task_environment.GetMainThreadTaskRunner(),
                                   run_loop.QuitClosure()),
          test_url_loader_factory);

  EXPECT_TRUE(context);
  if (!context) {
    return;
  }
  EXPECT_TRUE(context->audio_task_runner().get());
  EXPECT_TRUE(context->video_capture_task_runner().get());
  EXPECT_TRUE(context->video_encode_task_runner().get());
  EXPECT_TRUE(context->file_task_runner().get());
  EXPECT_TRUE(context->input_task_runner().get());
  EXPECT_TRUE(context->network_task_runner().get());
  EXPECT_TRUE(context->ui_task_runner().get());

  context.reset();
  run_loop.Run();
}

}  // namespace remoting
