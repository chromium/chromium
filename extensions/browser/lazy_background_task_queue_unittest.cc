// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/lazy_background_task_queue.h"

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/test/test_browser_context.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/browser/lazy_context_id.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_factory.h"
#include "extensions/browser/test_extensions_browser_client.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserContext;

namespace extensions {
namespace {

// A ProcessManager that doesn't create background host pages.
class TestProcessManager : public ProcessManager {
 public:
  explicit TestProcessManager(BrowserContext* context)
      : ProcessManager(context, context, ExtensionRegistry::Get(context)),
        create_count_(0) {
    // ProcessManager constructor above assumes non-incognito.
    DCHECK(!context->IsOffTheRecord());
  }
  ~TestProcessManager() override {}

  int create_count() { return create_count_; }

  // ProcessManager overrides:
  bool CreateBackgroundHost(const Extension* extension,
                            const GURL& url) override {
    // Don't actually try to create a web contents.
    create_count_++;
    return false;
  }

 private:
  int create_count_;

  DISALLOW_COPY_AND_ASSIGN(TestProcessManager);
};

std::unique_ptr<KeyedService> CreateTestProcessManager(
    BrowserContext* context) {
  return std::make_unique<TestProcessManager>(context);
}

}  // namespace

// Derives from ExtensionsTest to provide content module and keyed service
// initialization.
class LazyBackgroundTaskQueueTest : public ExtensionsTest {
 public:
  LazyBackgroundTaskQueueTest() : task_run_count_(0) {}
  ~LazyBackgroundTaskQueueTest() override {}

  int task_run_count() { return task_run_count_; }
  TestProcessManager* process_manager() { return process_manager_; }

  // A simple callback for AddPendingTask.
  void RunPendingTask(std::unique_ptr<LazyContextTaskQueue::ContextInfo>) {
    task_run_count_++;
  }

  // Creates and registers an extension without a background page.
  scoped_refptr<const Extension> CreateSimpleExtension() {
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("No background")
            .SetID("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")
            .Build();
    ExtensionRegistry::Get(browser_context())->AddEnabled(extension);
    return extension;
  }

  // Creates and registers an extension with a lazy background page.
  scoped_refptr<const Extension> CreateLazyBackgroundExtension() {
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("Lazy background")
            .SetBackgroundContext(
                ExtensionBuilder::BackgroundContext::EVENT_PAGE)
            .SetID("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb")
            .Build();
    ExtensionRegistry::Get(browser_context())->AddEnabled(extension);
    return extension;
  }

 protected:
  void SetUp() override {
    ExtensionsTest::SetUp();
    user_prefs::UserPrefs::Set(browser_context(), &testing_pref_service_);

    process_manager_ = static_cast<TestProcessManager*>(
        ProcessManagerFactory::GetInstance()->SetTestingFactoryAndUse(
            browser_context(), base::BindRepeating(&CreateTestProcessManager)));
  }

  void TearDown() override {
    process_manager_ = nullptr;
    ExtensionsTest::TearDown();
  }

 private:
  sync_preferences::TestingPrefServiceSyncable testing_pref_service_;

  // The total number of pending tasks that have been executed.
  int task_run_count_;
  TestProcessManager* process_manager_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(LazyBackgroundTaskQueueTest);
};

// Tests that only extensions with background pages should have tasks queued.
TEST_F(LazyBackgroundTaskQueueTest, ShouldEnqueueTask) {
  LazyBackgroundTaskQueue queue(browser_context());

  // Build a simple extension with no background page.
  scoped_refptr<const Extension> no_background = CreateSimpleExtension();
  EXPECT_FALSE(queue.ShouldEnqueueTask(browser_context(), no_background.get()));

  // Build another extension with a background page.
  scoped_refptr<const Extension> with_background =
      CreateLazyBackgroundExtension();
  EXPECT_TRUE(
      queue.ShouldEnqueueTask(browser_context(), with_background.get()));
}

// Tests that adding tasks actually increases the pending task count, and that
// multiple extensions can have pending tasks.
TEST_F(LazyBackgroundTaskQueueTest, AddPendingTask) {
  LazyBackgroundTaskQueue queue(browser_context());

  // Build a simple extension with no background page.
  scoped_refptr<const Extension> no_background = CreateSimpleExtension();

  // Adding a pending task increases the number of extensions with tasks, but
  // doesn't run the task.
  const LazyContextId no_background_context_id(browser_context(),
                                               no_background->id());
  queue.AddPendingTask(
      no_background_context_id,
      base::BindOnce(&LazyBackgroundTaskQueueTest::RunPendingTask,
                     base::Unretained(this)));
  EXPECT_EQ(1u, queue.pending_tasks_.size());
  EXPECT_EQ(0, task_run_count());

  // Another task on the same extension doesn't increase the number of
  // extensions that have tasks and doesn't run any tasks.
  queue.AddPendingTask(
      no_background_context_id,
      base::BindOnce(&LazyBackgroundTaskQueueTest::RunPendingTask,
                     base::Unretained(this)));
  EXPECT_EQ(1u, queue.pending_tasks_.size());
  EXPECT_EQ(0, task_run_count());

  // Adding a task on an extension with a lazy background page tries to create
  // a background host, and if that fails, runs the task immediately.
  scoped_refptr<const Extension> lazy_background =
      CreateLazyBackgroundExtension();
  const LazyContextId lazy_background_context_id(browser_context(),
                                                 lazy_background->id());
  queue.AddPendingTask(
      lazy_background_context_id,
      base::BindOnce(&LazyBackgroundTaskQueueTest::RunPendingTask,
                     base::Unretained(this)));
  EXPECT_EQ(1u, queue.pending_tasks_.size());
  // The process manager tried to create a background host.
  EXPECT_EQ(1, process_manager()->create_count());
  // The task ran immediately because the creation failed.
  EXPECT_EQ(1, task_run_count());
}

// Tests that pending tasks are actually run.
TEST_F(LazyBackgroundTaskQueueTest, ProcessPendingTasks) {
  LazyBackgroundTaskQueue queue(browser_context());

  // ProcessPendingTasks is a no-op if there are no tasks.
  scoped_refptr<const Extension> extension = CreateSimpleExtension();
  queue.ProcessPendingTasks(NULL, browser_context(), extension.get());
  EXPECT_EQ(0, task_run_count());

  // Schedule a task to run.
  queue.AddPendingTask(
      LazyContextId(browser_context(), extension->id()),
      base::BindOnce(&LazyBackgroundTaskQueueTest::RunPendingTask,
                     base::Unretained(this)));
  EXPECT_EQ(0, task_run_count());
  EXPECT_EQ(1u, queue.pending_tasks_.size());

  // Trying to run tasks for an unrelated BrowserContext should do nothing.
  content::TestBrowserContext unrelated_context;
  queue.ProcessPendingTasks(NULL, &unrelated_context, extension.get());
  EXPECT_EQ(0, task_run_count());
  EXPECT_EQ(1u, queue.pending_tasks_.size());

  // Processing tasks when there is one pending runs the task and removes the
  // extension from the list of extensions with pending tasks.
  queue.ProcessPendingTasks(NULL, browser_context(), extension.get());
  EXPECT_EQ(1, task_run_count());
  EXPECT_EQ(0u, queue.pending_tasks_.size());
}

// Tests that if a pending task was added before the extension with a lazy
// background page is loaded, then we will create the lazy background page when
// the extension is loaded.
TEST_F(LazyBackgroundTaskQueueTest, CreateLazyBackgroundPageOnExtensionLoaded) {
  LazyBackgroundTaskQueue queue(browser_context());

  scoped_refptr<const Extension> lazy_background =
      ExtensionBuilder("Lazy background")
          .SetBackgroundContext(ExtensionBuilder::BackgroundContext::EVENT_PAGE)
          .SetID("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb")
          .Build();

  queue.OnExtensionLoaded(browser_context(), lazy_background.get());
  // Did not try to create a background host because there are no queued tasks.
  EXPECT_EQ(0, process_manager()->create_count());

  queue.AddPendingTask(
      LazyContextId(browser_context(), lazy_background->id()),
      base::BindOnce(&LazyBackgroundTaskQueueTest::RunPendingTask,
                     base::Unretained(this)));
  EXPECT_EQ(1u, queue.pending_tasks_.size());
  // Did not try to create a background host because extension is not yet
  // loaded.
  EXPECT_EQ(0, process_manager()->create_count());
  // The task is queued.
  EXPECT_EQ(0, task_run_count());

  ExtensionRegistry::Get(browser_context())->AddEnabled(lazy_background);
  queue.OnExtensionLoaded(browser_context(), lazy_background.get());

  // The process manager tried to create a background host because there is a
  // queued task.
  EXPECT_EQ(1, process_manager()->create_count());
  // The queued task ran because the creation failed.
  EXPECT_EQ(1, task_run_count());
  EXPECT_EQ(0u, queue.pending_tasks_.size());
}

}  // namespace extensions
