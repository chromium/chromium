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
  base::ThreadPriorityForTest expected_priority =
      base::ThreadPriorityForTest::kNormal;
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
  // In browser main loop the kCompositing thread type is set.
  // Only Windows, Android and ChromeOS will set kDisplay priority for
  // kCompositing thread type. We omit Windows here as it has a special
  // treatment for the UI thread.
#if BUILDFLAG(IS_CHROMEOS)
  // TODO(1340997): ChromeOS results a kNormal priority unexpectedly.
  expected_priority = base::ThreadPriorityForTest::kNormal;
#else
  expected_priority = base::ThreadPriorityForTest::kDisplay;
#endif
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)

  EXPECT_EQ(base::PlatformThread::GetCurrentThreadPriorityForTest(),
            expected_priority);

  // The `expected_priority` for browser IO is the same as browser main's.
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
