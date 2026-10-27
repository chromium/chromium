// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/process_manager.h"

#include <memory>

#include "base/process/kill.h"
#include "build/android_buildflags.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/site_instance.h"
#include "content/public/common/content_client.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/browser/process_manager_delegate.h"
#include "extensions/browser/service_worker/worker_id.h"
#include "extensions/browser/test_extensions_browser_client.h"
#include "third_party/blink/public/common/tokens/tokens.h"

using content::BrowserContext;
using content::SiteInstance;
using content::TestBrowserContext;

namespace extensions {

namespace {

// A trivial ProcessManagerDelegate.
class TestProcessManagerDelegate : public ProcessManagerDelegate {
 public:
  TestProcessManagerDelegate()
      : is_background_page_allowed_(true),
        defer_creating_startup_background_hosts_(false) {}
  ~TestProcessManagerDelegate() override {}

  // ProcessManagerDelegate implementation.
  bool AreBackgroundPagesAllowedForContext(
      BrowserContext* context) const override {
    return is_background_page_allowed_;
  }
  bool IsExtensionBackgroundPageAllowed(
      BrowserContext* context,
      const Extension& extension) const override {
    return is_background_page_allowed_;
  }
  bool DeferCreatingStartupBackgroundHosts(
      BrowserContext* context) const override {
    return defer_creating_startup_background_hosts_;
  }

  bool is_background_page_allowed_;
  bool defer_creating_startup_background_hosts_;
};

}  // namespace

class ProcessManagerTest : public ExtensionsTest {
 public:
  ProcessManagerTest() {}

  ProcessManagerTest(const ProcessManagerTest&) = delete;
  ProcessManagerTest& operator=(const ProcessManagerTest&) = delete;

  ~ProcessManagerTest() override {}

  void SetUp() override {
    ExtensionsTest::SetUp();
    extension_registry_ =
        std::make_unique<ExtensionRegistry>(browser_context());
    extensions_browser_client()->set_process_manager_delegate(
        &process_manager_delegate_);
  }

  void TearDown() override {
    extensions_browser_client()->set_process_manager_delegate(nullptr);
    extension_registry_.reset();
    ExtensionsTest::TearDown();
  }

  // Use original_context() to make it clear it is a non-incognito context.
  BrowserContext* original_context() { return browser_context(); }
  ExtensionRegistry* extension_registry() { return extension_registry_.get(); }
  TestProcessManagerDelegate* process_manager_delegate() {
    return &process_manager_delegate_;
  }

 private:
  std::unique_ptr<ExtensionRegistry>
      extension_registry_;  // Shared between BrowserContexts.
  TestProcessManagerDelegate process_manager_delegate_;
};

// Test that startup background hosts are created when the extension system
// becomes ready.
//
// NOTE: This test and those that follow do not try to create ExtensionsHosts
// because ExtensionHost is tightly coupled to WebContents and can't be
// constructed in unit tests.
TEST_F(ProcessManagerTest, CreateBackgroundHostsOnExtensionsReady) {
  ProcessManager manager(original_context(), extension_registry());
  ASSERT_FALSE(manager.startup_background_hosts_created_for_test());

  // Simulate the extension system becoming ready.
  extension_system()->SetReady();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(manager.startup_background_hosts_created_for_test());
}

// Test that the embedder can defer background host creation. Chrome does this
// when the profile is created asynchronously, which may take a while.
TEST_F(ProcessManagerTest, CreateBackgroundHostsDeferred) {
  ProcessManager manager(original_context(), extension_registry());
  ASSERT_FALSE(manager.startup_background_hosts_created_for_test());

  // Don't create background hosts if the delegate says to defer them.
  process_manager_delegate()->defer_creating_startup_background_hosts_ = true;
  manager.MaybeCreateStartupBackgroundHosts();
  EXPECT_FALSE(manager.startup_background_hosts_created_for_test());

  // The extension system becoming ready still doesn't create the hosts.
  extension_system()->SetReady();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(manager.startup_background_hosts_created_for_test());

  // Once the embedder is ready the background hosts can be created.
  process_manager_delegate()->defer_creating_startup_background_hosts_ = false;
  manager.MaybeCreateStartupBackgroundHosts();
  EXPECT_TRUE(manager.startup_background_hosts_created_for_test());
}

// Test that the embedder can disallow background host creation.
// Chrome OS does this in guest mode.
TEST_F(ProcessManagerTest, IsBackgroundHostAllowed) {
  ProcessManager manager(original_context(), extension_registry());
  ASSERT_FALSE(manager.startup_background_hosts_created_for_test());

  // Don't create background hosts if the delegate disallows them.
  process_manager_delegate()->is_background_page_allowed_ = false;
  manager.MaybeCreateStartupBackgroundHosts();
  EXPECT_FALSE(manager.startup_background_hosts_created_for_test());

  // The extension system becoming ready still doesn't create the hosts.
  extension_system()->SetReady();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(manager.startup_background_hosts_created_for_test());
}

// Test that destroying a RenderProcessHost without a subsequent
// RenderProcessExited notification (for example, when a worker registration
// arrived after RenderProcessHostImpl::FastShutdown() marked the host dead)
// still removes the host observation and tracked worker before ~ProcessManager.
TEST_F(ProcessManagerTest,
       CleansUpTrackedWorkerWhenRenderProcessHostDestroyed) {
  ProcessManager manager(original_context(), extension_registry());
  auto render_process_host =
      std::make_unique<content::MockRenderProcessHost>(original_context());
  render_process_host->Init();
  render_process_host->SimulateRenderProcessExit(
      base::TERMINATION_STATUS_NORMAL_TERMINATION, /*exit_code=*/0);
  ASSERT_FALSE(render_process_host->IsInitializedAndNotDead());

  const WorkerId worker_id{/*extension_id=*/"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                           render_process_host->GetID(),
                           /*version_id=*/1,
                           /*thread_id=*/1, blink::ServiceWorkerToken()};
  manager.StartTrackingServiceWorkerRunningInstance(worker_id);
  EXPECT_TRUE(manager.HasServiceWorker(worker_id));

  // Destroying a dead RenderProcessHost fires RenderProcessHostDestroyed()
  // without firing RenderProcessExited() again.
  render_process_host.reset();
  EXPECT_FALSE(manager.HasServiceWorker(worker_id));
}

}  // namespace extensions
