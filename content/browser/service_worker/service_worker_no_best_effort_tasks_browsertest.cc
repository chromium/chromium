// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/task/task_traits.h"
#include "base/test/test_timeouts.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

namespace content {

class ServiceWorkerNoBestEffortTasksTest : public ContentBrowserTest {
 public:
  ServiceWorkerNoBestEffortTasksTest() = default;

  ServiceWorkerNoBestEffortTasksTest(
      const ServiceWorkerNoBestEffortTasksTest&) = delete;
  ServiceWorkerNoBestEffortTasksTest& operator=(
      const ServiceWorkerNoBestEffortTasksTest&) = delete;

  ~ServiceWorkerNoBestEffortTasksTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    ContentBrowserTest::SetUp();
  }

 private:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kDisableBestEffortTasks);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->StartAcceptingConnections();
  }
};

// Verify that BEST_EFFORT tasks don't run during these tests.
IN_PROC_BROWSER_TEST_F(ServiceWorkerNoBestEffortTasksTest,
                       ValidatePreconditions) {
  // `best_effort_tasks_allowed` must be heap-allocated because the validation
  // task could in theory run after returning from this scope.
  auto best_effort_tasks_allowed = std::make_unique<bool>(false);
#if BUILDFLAG(IS_ANDROID)
  bool* best_effort_tasks_allowed_ptr = best_effort_tasks_allowed.get();
#endif

  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT},
      base::BindOnce(
          [](std::unique_ptr<bool> best_effort_tasks_allowed) {
            EXPECT_TRUE(*best_effort_tasks_allowed);
          },
          std::move(best_effort_tasks_allowed)));

  // Give the validation task a chance to run before continuing, to avoid
  // false positives.
  base::RunLoop run_loop;
  base::ThreadPool::PostDelayedTask(FROM_HERE, run_loop.QuitClosure(),
                                    TestTimeouts::action_timeout());
  run_loop.Run();

#if BUILDFLAG(IS_ANDROID)
  // Android doesn't shut down the ThreadPool between tests so the validation
  // task could run during teardown. On other platforms it shouldn't run at all.
  *best_effort_tasks_allowed_ptr = true;
#endif
}

// Verify that the promise returned by navigator.serviceWorker.register()
// settles without running BEST_EFFORT tasks.
// This is a regression test for https://crbug.com/939250.
IN_PROC_BROWSER_TEST_F(ServiceWorkerNoBestEffortTasksTest,
                       RegisterServiceWorker) {
  EXPECT_TRUE(NavigateToURL(shell(),
                            embedded_test_server()->GetURL(
                                "/service_worker/create_service_worker.html")));
  EXPECT_EQ("DONE", EvalJs(shell(), "register('empty.js');"));
}

}  // namespace content
