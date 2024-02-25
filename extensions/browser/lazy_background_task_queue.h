// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_LAZY_BACKGROUND_TASK_QUEUE_H_
#define EXTENSIONS_BROWSER_LAZY_BACKGROUND_TASK_QUEUE_H_

#include <stddef.h>

#include <algorithm>
#include <map>
#include <string>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/extension_host_registry.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/lazy_context_id.h"
#include "extensions/browser/lazy_context_task_queue.h"
#include "extensions/common/extension_id.h"

namespace content {
class BrowserContext;
}

namespace extensions {
class Extension;
class ExtensionHost;

// This class maintains a queue of tasks that should execute when an
// extension's lazy background page is loaded. It is also in charge of loading
// the page when the first task is queued.
//
// It is the consumer's responsibility to use this class when appropriate, i.e.
// only with extensions that have not-yet-loaded lazy background pages.
class LazyBackgroundTaskQueue : public KeyedService,
                                public LazyContextTaskQueue,
                                public ExtensionRegistryObserver,
                                public ExtensionHostRegistry::Observer {
 public:
  explicit LazyBackgroundTaskQueue(content::BrowserContext* browser_context);

  LazyBackgroundTaskQueue(const LazyBackgroundTaskQueue&) = delete;
  LazyBackgroundTaskQueue& operator=(const LazyBackgroundTaskQueue&) = delete;

  ~LazyBackgroundTaskQueue() override;

  // Convenience method to return the LazyBackgroundTaskQueue for a given
  // |context|.
  static LazyBackgroundTaskQueue* Get(content::BrowserContext* context);

  // Returns true if the task should be added to the queue (that is, if the
  // extension has a lazy background page that isn't ready yet). If the
  // extension has a lazy background page that is being suspended this method
  // cancels that suspension.
  bool ShouldEnqueueTask(content::BrowserContext* context,
                         const Extension* extension) const override;

  // Returns true if the lazy background is ready to run tasks. This currently
  // means this and `ShouldEnqueueTask()` will return true at the same time. But
  // because of experiments on service workers needs to be separated out into
  // its own function.
  bool IsReadyToRunTasks(content::BrowserContext* context,
                         const Extension* extension) const override;

  // Adds a task to the queue for a given extension. If this is the first
  // task added for the extension, its lazy background page will be loaded.
  // The task will be called either when the page is loaded, or when the
  // page fails to load for some reason (e.g. a crash or browser
  // shutdown). In the latter case, |task| will be called with an empty
  // std::unique_ptr<ContextItem> parameter.
  void AddPendingTask(const LazyContextId& context_id,
                      PendingTask task) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(LazyBackgroundTaskQueueTest, AddPendingTask);
  FRIEND_TEST_ALL_PREFIXES(LazyBackgroundTaskQueueTest, ProcessPendingTasks);
  FRIEND_TEST_ALL_PREFIXES(LazyBackgroundTaskQueueTest,
                           CreateLazyBackgroundPageOnExtensionLoaded);
  using PendingTasksList = std::vector<PendingTask>;
  // A map between a LazyContextId and the queue of tasks pending the load of
  // its background page.
  using PendingTasksMap = std::map<LazyContextId, PendingTasksList>;

  // ExtensionHostRegistry::Observer:
  void OnExtensionHostCompletedFirstLoad(
      content::BrowserContext* browser_context,
      ExtensionHost* host) override;
  void OnExtensionHostDestroyed(content::BrowserContext* browser_context,
                                ExtensionHost* host) override;

  // ExtensionRegistryObserver interface.
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

  // If there are pending tasks for |extension| in |browser_context|, try to
  // create the background host. If the background host cannot be created, the
  // pending tasks are invoked with nullptr.
  void CreateLazyBackgroundHostOnExtensionLoaded(
      content::BrowserContext* browser_context,
      const Extension* extension);

  // Called when a lazy background page has finished loading, or has failed to
  // load (host is nullptr in that case). All enqueued tasks are run in order.
  void ProcessPendingTasks(
      ExtensionHost* host,
      content::BrowserContext* context,
      const Extension* extension);

  // Notifies queued tasks that a lazy background page has failed to load.
  void NotifyTasksExtensionFailedToLoad(
      content::BrowserContext* browser_context,
      const Extension* extension);

  raw_ptr<content::BrowserContext, DanglingUntriaged> browser_context_;
  PendingTasksMap pending_tasks_;

  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observation_{this};
  base::ScopedObservation<ExtensionHostRegistry,
                          ExtensionHostRegistry::Observer>
      extension_host_registry_observation_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_LAZY_BACKGROUND_TASK_QUEUE_H_
