// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/lazy_context_task_queue.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/extension_host.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"

namespace extensions {

LazyContextTaskQueue::ContextInfo::ContextInfo(ExtensionHost* host)
    : extension_id(host->extension()->id()),
      render_process_host(host->render_process_host()),
      service_worker_version_id(blink::mojom::kInvalidServiceWorkerVersionId),
      worker_thread_id(kMainThreadId),
      url(host->initial_url()),
      browser_context(host->browser_context()),
      web_contents(host->host_contents()) {}

LazyContextTaskQueue::ContextInfo::ContextInfo(
    const ExtensionId& extension_id,
    content::RenderProcessHost* render_process_host,
    int64_t service_worker_version_id,
    int worker_thread_id,
    const GURL& url)
    : extension_id(extension_id),
      render_process_host(render_process_host),
      service_worker_version_id(service_worker_version_id),
      worker_thread_id(worker_thread_id),
      url(url),
      browser_context(render_process_host->GetBrowserContext()) {}

}  // namespace extensions
