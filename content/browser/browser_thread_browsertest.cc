// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/gtest_util.h"
#include "build/build_config.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/test_utils.h"

namespace content {

class BrowserThreadPostTaskBeforeInitBrowserTest : public ContentBrowserTest {
 protected:
  void SetUp() override {
    // This should fail because the ThreadPool + TaskExecutor weren't created
    // yet.
    EXPECT_DCHECK_DEATH(
        GetIOThreadTaskRunner({})->PostTask(FROM_HERE, base::DoNothing()));

    // Obtaining a TaskRunner should also fail.
    EXPECT_DCHECK_DEATH(GetIOThreadTaskRunner({}));

    ContentBrowserTest::SetUp();
  }
};

IN_PROC_BROWSER_TEST_F(BrowserThreadPostTaskBeforeInitBrowserTest,
                       ExpectFailures) {}

IN_PROC_BROWSER_TEST_F(ContentBrowserTest, ExpectedThreadPriorities) {
  base::ThreadPriorityForTest expected_priority;
  // In browser main loop the kDisplayCritical thread type is set.
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  // TODO(40230522): ChromeOS and Linux result a kNormal priority unexpectedly.
  expected_priority = base::ThreadPriorityForTest::kNormal;
#else
  expected_priority = base::ThreadPriorityForTest::kDisplay;
#endif

  EXPECT_EQ(base::PlatformThread::GetCurrentThreadPriorityForTest(),
            expected_priority);

  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::ThreadPriorityForTest expected_priority) {
            EXPECT_EQ(base::PlatformThread::GetCurrentThreadPriorityForTest(),
                      expected_priority);
          },
          expected_priority));
  BrowserThread::RunAllPendingTasksOnThreadForTesting(BrowserThread::IO);
}

}  // namespace content
