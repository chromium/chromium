// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_switches.h"
#include "base/command_line.h"
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
