// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/lazy_context_id.h"

#include "extensions/browser/lazy_background_task_queue.h"
#include "extensions/browser/service_worker_task_queue.h"
#include "extensions/browser/task_queue_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/background_info.h"

namespace extensions {

LazyContextId::LazyContextId(content::BrowserContext* context,
                             const ExtensionId& extension_id)
    : type_(Type::kEventPage), context_(context), extension_id_(extension_id) {}

LazyContextId::LazyContextId(content::BrowserContext* context,
                             const ExtensionId& extension_id,
                             const GURL& service_worker_scope)
    : type_(Type::kServiceWorker),
      context_(context),
      extension_id_(extension_id),
      service_worker_scope_(service_worker_scope) {}

LazyContextId::LazyContextId(content::BrowserContext* context,
                             const Extension* extension)
    : context_(context), extension_id_(extension->id()) {
  if (BackgroundInfo::HasLazyBackgroundPage(extension)) {
    type_ = Type::kEventPage;
  } else {
    // TODO(crbug.com/773103): This currently assumes all workers are
    // registered in the '/' scope.
    DCHECK(BackgroundInfo::IsServiceWorkerBased(extension));
    type_ = Type::kServiceWorker;
    service_worker_scope_ =
        Extension::GetBaseURLFromExtensionId(extension->id());
  }
}

LazyContextTaskQueue* LazyContextId::GetTaskQueue() const {
  return GetTaskQueueForLazyContextId(*this);
}

}  // namespace extensions
