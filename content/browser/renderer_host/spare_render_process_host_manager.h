// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_SPARE_RENDER_PROCESS_HOST_MANAGER_H_
#define CONTENT_BROWSER_RENDERER_HOST_SPARE_RENDER_PROCESS_HOST_MANAGER_H_

#include <optional>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/timer/timer.h"
#include "content/common/content_export.h"
#include "content/public/browser/render_process_host_observer.h"

namespace content {

class BrowserContext;
class SiteInstanceImpl;
class RenderProcessHost;

// This class manages spare RenderProcessHosts.
//
// There is a singleton instance of this class which manages a single spare
// renderer (SpareRenderProcessHostManager::GetInstance(), below). This class
// encapsulates the implementation of
// RenderProcessHost::WarmupSpareRenderProcessHost()
//
// RenderProcessHostImpl should call
// SpareRenderProcessHostManager::MaybeTakeSpareRenderProcessHost when creating
// a new RPH. In this implementation, the spare renderer is bound to a
// BrowserContext and its default StoragePartition. If
// MaybeTakeSpareRenderProcessHost is called with a BrowserContext that does not
// match, the spare renderer is discarded. Only the default StoragePartition
// will be able to use a spare renderer. The spare renderer will also not be
// used as a guest renderer (flags_ contains kForGuestsOnly).
//
// It is safe to call WarmupSpareRenderProcessHost multiple times, although if
// called in a context where the spare renderer is not likely to be used
// performance may suffer due to the unnecessary RPH creation.
class CONTENT_EXPORT SpareRenderProcessHostManager
    : public RenderProcessHostObserver {
 public:
  SpareRenderProcessHostManager();
  ~SpareRenderProcessHostManager() override;

  SpareRenderProcessHostManager(const SpareRenderProcessHostManager& other) =
      delete;
  SpareRenderProcessHostManager& operator=(
      const SpareRenderProcessHostManager& other) = delete;

  static SpareRenderProcessHostManager& GetInstance();

  // Start a spare renderer immediately if there isn't one.
  void WarmupSpareRenderProcessHost(BrowserContext* browser_context);

  RenderProcessHost* MaybeTakeSpareRenderProcessHost(
      BrowserContext* browser_context,
      SiteInstanceImpl* site_instance);

  // Prepares for future requests (with an assumption that a future navigation
  // might require a new process for |browser_context|).
  //
  // Note that depending on the caller PrepareForFutureRequests can be called
  // after the spare_render_process_host_ has either been 1) matched and taken
  // or 2) mismatched and ignored or 3) matched and ignored.
  //
  // The creation of new spare renderer will be delayed by `delay` if present.
  // This is used to mitigate resource contention.
  void PrepareForFutureRequests(
      BrowserContext* browser_context,
      std::optional<base::TimeDelta> delay = std::nullopt);

  // Gracefully remove and cleanup a spare RenderProcessHost if it exists.
  void CleanupSpareRenderProcessHost();

  RenderProcessHost* spare_render_process_host() {
    return spare_render_process_host_;
  }

  base::CallbackListSubscription RegisterSpareRenderProcessHostChangedCallback(
      const base::RepeatingCallback<void(RenderProcessHost*)>& cb);

 private:
  // Release ownership of the spare renderer. Called when the spare has either
  // been 1) claimed to be used in a navigation or 2) shutdown somewhere else.
  void ReleaseSpareRenderProcessHost();

  // RenderProcessHostObserver:
  void RenderProcessReady(RenderProcessHost* host) override;
  void RenderProcessExited(RenderProcessHost* host,
                           const ChildProcessTerminationInfo& info) override;
  void RenderProcessHostDestroyed(RenderProcessHost* host) override;

  // Start a spare renderer at a later time if there isn't one.
  // This is to avoid resource contention between existing renderers and a
  // new spare renderer.
  void DeferredWarmupSpareRenderProcessHost(BrowserContext* browser_context,
                                            base::TimeDelta delay);

  // The clients who want to know when the spare render process host has
  // changed.
  base::RepeatingCallbackList<void(RenderProcessHost*)>
      spare_render_process_host_changed_callback_list_;

  // This is a bare pointer, because RenderProcessHost manages the lifetime of
  // all its instances; see GetAllHosts().
  raw_ptr<RenderProcessHost> spare_render_process_host_ = nullptr;

  // The timer used to track the startup time of the spare renderer process.
  std::unique_ptr<base::ElapsedTimer> process_startup_timer_;
  // The timer used to track the delay of spare renderer creation.
  std::unique_ptr<base::ElapsedTimer> delay_timer_;

  base::OneShotTimer deferred_warmup_timer_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_SPARE_RENDER_PROCESS_HOST_MANAGER_H_
