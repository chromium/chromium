// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_SPARE_RENDER_PROCESS_HOST_MANAGER_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_SPARE_RENDER_PROCESS_HOST_MANAGER_IMPL_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/timer/timer.h"
#include "content/common/content_export.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/spare_render_process_host_manager.h"

namespace content {

class BrowserContext;
class SiteInstanceImpl;
class RenderProcessHost;

class CONTENT_EXPORT SpareRenderProcessHostManagerImpl
    : public SpareRenderProcessHostManager,
      public RenderProcessHostObserver {
 public:
  SpareRenderProcessHostManagerImpl();
  ~SpareRenderProcessHostManagerImpl() override;

  SpareRenderProcessHostManagerImpl(
      const SpareRenderProcessHostManagerImpl& other) = delete;
  SpareRenderProcessHostManagerImpl& operator=(
      const SpareRenderProcessHostManagerImpl& other) = delete;

  static SpareRenderProcessHostManagerImpl& Get();

  // SpareRenderProcessHostManager:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void WarmupSpare(BrowserContext* browser_context) override;
  const std::vector<RenderProcessHost*>& GetSpares() override;
  std::vector<int> GetSpareIds() override;
  void CleanupSparesForTesting() override;

  // Start a spare renderer immediately, only if there is none.
  // If the timeout is given, the spare render process will not be created
  // if there is a delayed creation which indicates no timeout.
  //
  // The created spare render process will be destroyed after the timeout
  // if it is given and accepted. Otherwise, the spare renderer will be kept
  // until used by some navigation or cleared for memory pressure.
  //
  // The general rule for the timeout update is to always keep the value with
  // a larger timeout. The timeout will be accepted when:
  // * There is no spare render process or the spare render process was created
  //   for a different browser context.
  // * The current timeout will be fired before the specified timeout.
  // If the function is called again without a timeout, the current timeout will
  // be cancelled. If the function is called again with a timeout firing after
  // the current timeout, the timeout will be updated.
  void WarmupSpare(BrowserContext* browser_context,
                   std::optional<base::TimeDelta> timeout);

  // RenderProcessHostImpl should call
  // SpareRenderProcessHostManager::MaybeTakeSpare when creating a new RPH. In
  // this implementation, the spare renderer is bound to a BrowserContext and
  // its default StoragePartition. If MaybeTakeSpare is called with a
  // BrowserContext that does not match, the spare renderer is discarded. Only
  // the default StoragePartition will be able to use a spare renderer. The
  // spare renderer will also not be used as a guest renderer (flags_ contains
  // kForGuestsOnly).
  RenderProcessHost* MaybeTakeSpare(BrowserContext* browser_context,
                                    SiteInstanceImpl* site_instance);

  // Prepares for future requests (with an assumption that a future navigation
  // might require a new process for |browser_context|).
  //
  // Note that depending on the caller PrepareForFutureRequests can be called
  // after a spare RPH has either been 1) matched and taken or 2) mismatched and
  // ignored or 3) matched and ignored.
  //
  // The creation of new spare renderer will be delayed by `delay` if present.
  // This is used to mitigate resource contention.
  void PrepareForFutureRequests(BrowserContext* browser_context,
                                std::optional<base::TimeDelta> delay);

  // Gracefully remove and cleanup all existing spare RenderProcessHost.
  void CleanupSpares();

  void SetDeferTimerTaskRunnerForTesting(
      scoped_refptr<base::SequencedTaskRunner> task_runner);

 private:
  // Release ownership of a spare renderer. Called when the spare has either
  // been 1) claimed to be used in a navigation or 2) shutdown somewhere else.
  void ReleaseSpare(RenderProcessHost* host);

  // RenderProcessHostObserver:
  void RenderProcessReady(RenderProcessHost* host) override;
  void RenderProcessExited(RenderProcessHost* host,
                           const ChildProcessTerminationInfo& info) override;
  void RenderProcessHostDestroyed(RenderProcessHost* host) override;

  // Start a spare renderer at a later time if there isn't one.
  // This is to avoid resource contention between existing renderers and a
  // new spare renderer.
  void DeferredWarmupSpare(BrowserContext* browser_context,
                           base::TimeDelta delay,
                           std::optional<base::TimeDelta> timeout);

  void StartDestroyTimer(std::optional<base::TimeDelta> timeout);

  bool DestroyTimerWillFireBefore(base::TimeDelta timeout);

  // The clients who want to know when the spare render process host has
  // changed.
  base::ObserverList<Observer> observer_list_;

  // All spare RPHs. RPH instances are self-owned, hence the raw pointers.
  std::vector<RenderProcessHost*> spare_rphs_;

  // The timer used to track the startup time of the spare renderer process.
  std::unique_ptr<base::ElapsedTimer> process_startup_timer_;
  // The timer used to track the delay of spare renderer creation.
  std::unique_ptr<base::ElapsedTimer> delay_timer_;

  base::OneShotTimer deferred_warmup_timer_;
  base::OneShotTimer deferred_destroy_timer_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_SPARE_RENDER_PROCESS_HOST_MANAGER_IMPL_H_
