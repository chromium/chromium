// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/lazy_context_id.h"

#include "extensions/browser/lazy_background_task_queue.h"
#include "extensions/browser/service_worker/service_worker_task_queue.h"
#include "extensions/browser/task_queue_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/background_info.h"

namespace extensions {

LazyContextId::LazyContextId(Type type,
                             content::BrowserContext* context,
                             const ExtensionId& extension_id)
    : type_(type), context_(context), extension_id_(extension_id) {}

LazyContextId::LazyContextId(content::BrowserContext* context,
                             const Extension* extension)
    : context_(context), extension_id_(extension->id()) {
  if (BackgroundInfo::IsServiceWorkerBased(extension)) {
    type_ = Type::kServiceWorker;
  } else if (BackgroundInfo::HasBackgroundPage(extension)) {
    // Packaged apps and extensions with persistent background and event pages
    // all use the same task queue.
    type_ = Type::kBackgroundPage;
  } else {
    // There are tests where a LazyContextId is constructed for an extension
    // without a background page or service worker, so this is a fallback.
    type_ = Type::kNone;
  }
}

LazyContextTaskQueue* LazyContextId::GetTaskQueue() const {
  return GetTaskQueueForLazyContextId(*this);
}

}  // namespace extensions
