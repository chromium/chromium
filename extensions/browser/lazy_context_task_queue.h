// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_LAZY_CONTEXT_TASK_QUEUE_H_
#define EXTENSIONS_BROWSER_LAZY_CONTEXT_TASK_QUEUE_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "extensions/common/extension_id.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
class RenderProcessHost;
class WebContents;
}  // namespace content

namespace extensions {
class Extension;
class ExtensionHost;
class LazyContextId;

// Interface for performing tasks after loading lazy contexts of an extension.
//
// Lazy contexts are non-persistent, so they can unload any time and this
// interface exposes an async mechanism to perform tasks after loading the
// context.
class LazyContextTaskQueue {
 public:
  // Represents information about an extension lazy context, which is passed to
  // consumers that add tasks to LazyContextTaskQueue.
  struct ContextInfo {
    const ExtensionId extension_id;
    // `render_process_host` is not a raw_ptr<...> for performance reasons
    // (based on analysis of sampling profiler data).
    RAW_PTR_EXCLUSION content::RenderProcessHost* const render_process_host;
    const int64_t service_worker_version_id;
    const int worker_thread_id;
    const GURL url;
    // TODO(dbertoni): This needs to be initialized for the Service Worker
    // version of the constructor.
    // `browser_context` is not a raw_ptr<...> for performance reasons (based on
    // analysis of sampling profiler data).
    RAW_PTR_EXCLUSION content::BrowserContext* const browser_context = nullptr;
    // This data member will have a nullptr value for Service Worker-related
    // tasks.
    // `web_contents` is not a raw_ptr<...> for performance reasons (based on
    // analysis of sampling profiler data).
    RAW_PTR_EXCLUSION content::WebContents* const web_contents = nullptr;

    explicit ContextInfo(ExtensionHost* host);

    ContextInfo(const ExtensionId& extension_id,
                content::RenderProcessHost* render_process_host,
                int64_t service_worker_version_id,
                int worker_thread_id,
                const GURL& url);
  };
  using PendingTask =
      base::OnceCallback<void(std::unique_ptr<ContextInfo> params)>;

  // Returns true if the task should be added to the queue (that is, if the
  // extension has a lazy background page or service worker that isn't ready
  // yet).
  virtual bool ShouldEnqueueTask(content::BrowserContext* context,
                                 const Extension* extension) const = 0;

  // Returns true if the lazy context is ready to run tasks (a.k.a active).
  virtual bool IsReadyToRunTasks(content::BrowserContext* context,
                                 const Extension* extension) const = 0;

  // Adds a task to the queue for a given extension. If this is the first
  // task added for the extension, its "lazy context" (i.e. lazy background
  // page for event pages, service worker for extension service workers) will
  // be loaded. The task will be called either when the page is loaded,
  // or when the page fails to load for some reason (e.g. a crash or browser
  // shutdown). In the latter case, the ContextInfo will be nullptr.
  virtual void AddPendingTask(const LazyContextId& context_id,
                              PendingTask task) = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_LAZY_CONTEXT_TASK_QUEUE_H_
