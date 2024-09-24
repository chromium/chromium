// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SPARE_RENDER_PROCESS_HOST_MANAGER_H_
#define CONTENT_PUBLIC_BROWSER_SPARE_RENDER_PROCESS_HOST_MANAGER_H_

#include <optional>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/time/time.h"

namespace content {

class RenderProcessHost;
class BrowserContext;

// This class manages spare RenderProcessHosts.
//
// There is a singleton instance of this class which manages a single spare
// renderer (SpareRenderProcessHostManager::Get(), below).
class CONTENT_EXPORT SpareRenderProcessHostManager {
 public:
  static SpareRenderProcessHostManager& Get();

  // Possibly start an unbound, spare RenderProcessHost. A subsequent creation
  // of a RenderProcessHost with a matching browser_context may use this
  // preinitialized RenderProcessHost, improving performance.
  //
  // It is safe to call this multiple times or when it is not certain that the
  // spare renderer will be used, although calling this too eagerly may reduce
  // performance as unnecessary RenderProcessHosts are created. The spare
  // renderer will only be used if it using the default StoragePartition of a
  // matching BrowserContext.
  //
  // The spare RenderProcessHost is meant to be created in a situation where a
  // navigation is imminent and it is unlikely an existing RenderProcessHost
  // will be used, for example in a cross-site navigation when a Service Worker
  // will need to be started.  Note that if ContentBrowserClient opts into
  // strict site isolation (via ShouldEnableStrictSiteIsolation), then the
  // //content layer will maintain a warm spare process host at all times
  // (without a need for separate calls to WarmupSpare).
  virtual void WarmupSpare(BrowserContext* browser_context) = 0;

  // Return the spare RenderProcessHost, if it exists. There is at most one
  // globally-used spare RenderProcessHost at any time.
  // TODO(crbug.com/41492171): remove the non-test method once the performance
  // investigation is finished.
  virtual RenderProcessHost* GetSpare() = 0;
  virtual RenderProcessHost* GetSpareForTesting() = 0;

  // Registers a callback to be notified when the spare RenderProcessHost is
  // changed. If a new spare RenderProcessHost is created, the callback is made
  // when the host is ready (RenderProcessHostObserver::RenderProcessReady). If
  // the spare RenderProcessHost is promoted to be a "real" RenderProcessHost or
  // discarded for any reason, the callback is made with a null pointer.
  virtual base::CallbackListSubscription RegisterSpareChangedCallback(
      const base::RepeatingCallback<void(RenderProcessHost*)>& cb) = 0;

 protected:
  virtual ~SpareRenderProcessHostManager() = default;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SPARE_RENDER_PROCESS_HOST_MANAGER_H_
