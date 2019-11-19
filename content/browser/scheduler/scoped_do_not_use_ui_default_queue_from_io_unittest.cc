// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/scoped_do_not_use_ui_default_queue_from_io.h"

#include "base/bind_helpers.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/test/gtest_util.h"
#include "build/build_config.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

#if defined(THREAD_SANITIZER)
// The combination of EXPECT_DCHECK_DEATH and TSan doesn't work, although we get
// the expected DCHECK with TSan builds.
#define MAYBE_BadPostFromIO DISABLED_BadPostFromIO
#elif defined(OS_MACOSX)
// This test fails to DCHECK on mac release builds for reasons unknown.
#define MAYBE_BadPostFromIO DISABLED_BadPostFromIO
#else
#define MAYBE_BadPostFromIO BadPostFromIO
#endif

TEST(ScopedDoNotUseUIDefaultQueueFromIO, MAYBE_BadPostFromIO) {
  EXPECT_DCHECK_DEATH({
    BrowserTaskEnvironment task_environment;
    base::RunLoop run_loop;

    base::PostTask(
        FROM_HERE, {BrowserThread::IO}, base::BindLambdaForTesting([&]() {
          ScopedDoNotUseUIDefaultQueueFromIO do_not_post_to_ui_default(
              FROM_HERE);

          // Posting to the UI thread with no other traits is prohibited.
          base::PostTask(FROM_HERE, {BrowserThread::UI}, base::DoNothing());
        }));

    run_loop.Run();
  });
}

TEST(ScopedDoNotUseUIDefaultQueueFromIO, PostFromIO) {
  BrowserTaskEnvironment task_environment;

  base::RunLoop run_loop;

  base::PostTask(
      FROM_HERE, {BrowserThread::IO}, base::BindLambdaForTesting([&]() {
        {
          ScopedDoNotUseUIDefaultQueueFromIO do_not_post_to_ui_default(
              FROM_HERE);

          // Posting with non default BrowserTaskType is OK.
          base::PostTask(FROM_HERE,
                         {BrowserThread::IO, BrowserTaskType::kNavigation},
                         base::DoNothing());

          // Posting to the IO thread default queue is OK.
          base::PostTask(FROM_HERE, {BrowserThread::IO}, base::DoNothing());
        }

        // After |do_not_post_to_ui_default| has gone out of scope it's fine to
        // post to the UI thread's default queue again.
        base::PostTask(FROM_HERE, {BrowserThread::UI}, run_loop.QuitClosure());
      }));

  run_loop.Run();
}

TEST(ScopedDoNotUseUIDefaultQueueFromIO, PostFromUI) {
  BrowserTaskEnvironment task_environment(
      BrowserTaskEnvironment::REAL_IO_THREAD);
  base::RunLoop run_loop;

  base::PostTask(
      FROM_HERE, {BrowserThread::UI}, base::BindLambdaForTesting([&]() {
        ScopedDoNotUseUIDefaultQueueFromIO do_not_post_to_ui_default(FROM_HERE);

        // It's fine to post from the UI thread.
        base::PostTask(FROM_HERE, {BrowserThread::UI}, run_loop.QuitClosure());
      }));

  run_loop.Run();
}

}  // namespace content
