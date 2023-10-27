// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_SERVICE_WORKER_MESSAGE_FILTER_H_
#define EXTENSIONS_BROWSER_EXTENSION_SERVICE_WORKER_MESSAGE_FILTER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner_helpers.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/mojom/frame.mojom-forward.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_LEGACY_IPC)

namespace content {
class BrowserContext;
class ServiceWorkerContext;
}

namespace extensions {

// IPC handler class for extension service worker.
//
// Created and destroyed on the UI thread.
class ExtensionServiceWorkerMessageFilter
    : public content::BrowserMessageFilter {
 public:
  ExtensionServiceWorkerMessageFilter(
      int render_process_id,
      content::BrowserContext* context,
      content::ServiceWorkerContext* service_worker_context);

  ExtensionServiceWorkerMessageFilter(
      const ExtensionServiceWorkerMessageFilter&) = delete;
  ExtensionServiceWorkerMessageFilter& operator=(
      const ExtensionServiceWorkerMessageFilter&) = delete;

  // content::BrowserMessageFilter:
  bool OnMessageReceived(const IPC::Message& message) override;
  void OverrideThreadForMessage(const IPC::Message& message,
                                content::BrowserThread::ID* thread) override;
  void OnDestruct() const override;

  static void EnsureShutdownNotifierFactoryBuilt();

 private:
  friend class base::DeleteHelper<ExtensionServiceWorkerMessageFilter>;
  friend class content::BrowserThread;
  ~ExtensionServiceWorkerMessageFilter() override;

  void ShutdownOnUIThread();

  // Message handlers.
  void OnEventAckWorker(const ExtensionId& extension_id,
                        int64_t service_worker_version_id,
                        int thread_id,
                        int event_id);

  void DidFailDecrementInflightEvent();

  // Only accessed from the UI thread.
  raw_ptr<content::BrowserContext, DanglingUntriaged> browser_context_;

  const int render_process_id_;

  base::CallbackListSubscription shutdown_notifier_subscription_;

  // Owned by the StoragePartition of our profile.
  raw_ptr<content::ServiceWorkerContext, AcrossTasksDanglingUntriaged>
      service_worker_context_;
};

}  // namespace extensions

#endif

#endif  // EXTENSIONS_BROWSER_EXTENSION_SERVICE_WORKER_MESSAGE_FILTER_H_
