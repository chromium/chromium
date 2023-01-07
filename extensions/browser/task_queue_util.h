// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_TASK_QUEUE_UTIL_H_
#define EXTENSIONS_BROWSER_TASK_QUEUE_UTIL_H_

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {
class Extension;
class LazyContextId;
class LazyContextTaskQueue;

// Determines the correct task queue for |context_id|.
LazyContextTaskQueue* GetTaskQueueForLazyContextId(
    const LazyContextId& context_id);

// Activates the service worker task queue for |browser_context| and
// |extension|. This must be called only once when an extension is loaded
// and before queueing any tasks.
//
// This is called for all extensions, not just for service worker-based
// ones.
void ActivateTaskQueueForExtension(content::BrowserContext* browser_context,
                                   const Extension* extension);

// Deactivates the service worker task queue for |browser_context| and
// |extension|. This should be called when the extension is unloaded. Once
// it completes, it's safe to call ActivateTaskQueueForExtension if the
// extension is reloaded.
//
// This is called for all extensions, not just for service worker-based
// ones.
void DeactivateTaskQueueForExtension(content::BrowserContext* browser_context,
                                     const Extension* extension);

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_TASK_QUEUE_UTIL_H_
