// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/spare_render_process_host_manager.h"

#include <utility>

#include "base/callback_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

using SpareRenderProcessHostManagerTest = ContentBrowserTest;

// Verify that a deferred spare renderer is eventually not created if the
// browser context was destroyed.
IN_PROC_BROWSER_TEST_F(SpareRenderProcessHostManagerTest,
                       BrowserContextDestroyEarly) {
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner =
      new base::TestMockTimeTaskRunner();
  auto& manager = SpareRenderProcessHostManager::GetInstance();
  // TestBrowserContext needs to create a temporary folder synchronously.
  base::ScopedAllowBlockingForTesting allow_blocking;
  auto browser_context = std::make_unique<TestBrowserContext>();

  {
    bool renderer_created = false;
    base::RunLoop run_loop;
    base::CallbackListSubscription subscription =
        manager.RegisterSpareRenderProcessHostChangedCallback(
            base::BindRepeating(
                [](base::RunLoop* run_loop, bool* renderer_created,
                   RenderProcessHost*) {
                  *renderer_created = true;
                  run_loop->Quit();
                },
                &run_loop, &renderer_created));

    manager.PrepareForFutureRequests(browser_context.get(), base::Seconds(1));
    EXPECT_EQ(manager.spare_render_process_host(), nullptr);

    // Wait until the renderer process is successfully started.
    run_loop.Run();
    // The spare renderer should be created.
    EXPECT_TRUE(renderer_created);
  }

  manager.CleanupSpareRenderProcessHost();
  EXPECT_EQ(manager.spare_render_process_host(), nullptr);

  {
    manager.PrepareForFutureRequests(browser_context.get(), base::Seconds(1));
    // Destroy the browser context.
    browser_context.reset();
    task_runner->RunUntilIdle();
    // The spare renderer shouldn't be created.
    EXPECT_EQ(manager.spare_render_process_host(), nullptr);
  }
}

}  // namespace content
