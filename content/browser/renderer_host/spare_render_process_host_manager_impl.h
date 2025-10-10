// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_SPARE_RENDER_PROCESS_HOST_MANAGER_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_SPARE_RENDER_PROCESS_HOST_MANAGER_IMPL_H_

#include <optional>

#include "base/memory/memory_pressure_listener.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/timer/timer.h"
#include "components/performance_manager/scenario_api/performance_scenario_observer.h"
#include "content/common/content_export.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/process_allocation_context.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/spare_render_process_host_manager.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/application_status_listener.h"
#endif

namespace content {

class BrowserContext;
class SiteInstanceImpl;
class RenderProcessHost;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(SpareRendererDispatchResult)
enum class SpareRendererDispatchResult {
  kUsed = 0,
  kTimeout = 1,
  kOverridden = 2,
  kDestroyedNotEnabled = 3,
  kDestroyedProcessLimit = 4,
  kProcessExited = 5,
  kProcessHostDestroyed = 6,
  kMemoryPressure = 7,
  kKillAfterBackgrounded = 8,
  kMaxValue = kKillAfterBackgrounded
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/browser/enums.xml:SpareRendererDispatchResult)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(NoSpareRendererReason)
enum class NoSpareRendererReason {
  kNotYetCreated = 0,
  kTakenByPreviousNavigation = 1,
  kTimeout = 2,
  kNotEnabled = 3,
  kProcessLimit = 4,
  kMemoryPressure = 5,
  kProcessExited = 6,
  kProcessHostDestroyed = 7,
  kNotYetCreatedFirstLaunch = 8,
  kNotYetCreatedAfterWarmup = 9,
  kOnceBackgrounded = 10,
  kMaxValue = kOnceBackgrounded
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/browser/enums.xml:NoSpareRendererReason)

class CONTENT_EXPORT SpareRenderProcessHostManagerImpl
    : public SpareRenderProcessHostManager,
      public RenderProcessHostObserver,
      public performance_scenarios::PerformanceScenarioObserver,
      public base::MemoryPressureListener {
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
  RenderProcessHost* WarmupSpare(BrowserContext* browser_context) override;
  const std::vector<RenderProcessHost*>& GetSpares() override;
  std::vector<ChildProcessId> GetSpareIds() override;
  void CleanupSparesForTesting() override;
  const std::optional<LastSpareRendererCreationInfo>&
  GetLastSpareRendererCreationInfo() const override;

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
  //
  // Returns a RenderProcessHost if a new one is created.
  RenderProcessHost* WarmupSpare(BrowserContext* browser_context,
                                 std::optional<base::TimeDelta> timeout);

  // RenderProcessHostImpl should call
  // SpareRenderProcessHostManager::MaybeTakeSpare when creating a new RPH. In
  // this implementation, the spare renderer is bound to a BrowserContext and
  // its default StoragePartition. If MaybeTakeSpare is called with a
  // BrowserContext that does not match, the spare renderer is discarded. Only
  // the default StoragePartition will be able to use a spare renderer. The
  // spare renderer will also not be used as a guest renderer (flags_ contains
  // kForGuestsOnly).
  RenderProcessHost* MaybeTakeSpare(
      BrowserContext* browser_context,
      SiteInstanceImpl* site_instance,
      const ProcessAllocationContext& allocation_context);

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
  //
  // Passing std::nullopt as dispatch_result is for test only.
  void CleanupSpares(
      std::optional<SpareRendererDispatchResult> dispatch_result);

  // Gracefully removes and cleanups any extra spare RenderProcessHost beyond
  // the first one. This is always a nop if the kMultipleSpareRPHs feature is
  // disabled.
  void CleanupExtraSpares(
      std::optional<SpareRendererDispatchResult> dispatch_result);

  void SetDeferTimerTaskRunnerForTesting(
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  void SetIsBrowserIdleForTesting(bool is_browser_idle);

  bool HasSpareRenderer() { return !spare_rphs_.empty(); }

 private:
  // Release ownership of a spare renderer. Called when the spare has either
  // been 1) claimed to be used in a navigation or 2) shutdown somewhere else.
  void ReleaseSpare(RenderProcessHost* host,
                    SpareRendererDispatchResult dispatch_result);

  // RenderProcessHostObserver:
  void RenderProcessReady(RenderProcessHost* host) override;
  void RenderProcessExited(RenderProcessHost* host,
                           const ChildProcessTerminationInfo& info) override;
  void RenderProcessHostDestroyed(RenderProcessHost* host) override;

  // performance_scenarios::PerformanceScenarioObserver:
  void OnLoadingScenarioChanged(
      performance_scenarios::ScenarioScope scope,
      performance_scenarios::LoadingScenario old_scenario,
      performance_scenarios::LoadingScenario new_scenario) override;

  void SetIsBrowserIdle(bool is_browser_idle);

  // Start a spare renderer at a later time if there isn't one.
  // This is to avoid resource contention between existing renderers and a
  // new spare renderer.
  void DeferredWarmupSpare(BrowserContext* browser_context,
                           base::TimeDelta delay,
                           std::optional<base::TimeDelta> timeout);

  void StartDestroyTimer(std::optional<base::TimeDelta> timeout);

  bool DestroyTimerWillFireBefore(base::TimeDelta timeout);

  void OnMemoryPressure(
      base::MemoryPressureLevel memory_pressure_level) override;

  // When the system is under memory pressure, this function is called every 5
  // minutes to determine when it ends.
  void CheckIfMemoryPressureEnded();

  // Returns true if an extra spare should be created.
  bool ShouldCreateExtraSpare() const;

  // Creates an extra spare, if ShouldCreateExtraSpare() returns true. This
  // function is responsible for the creation of all the extra spares.
  // `WarmupSpare()` is still responsible for creating the first one.
  void MaybeCreateExtraSpare();

  // Records heartbeat metrics for the spare RPHs. Called every 2 minutes.
  void OnMetricsHeartbeatTimerFired();

#if BUILDFLAG(IS_ANDROID)
  FRIEND_TEST_ALL_PREFIXES(
      SpareRenderProcessHostManagerMemoryThresholdBrowserTest,
      CorrectThresholdLogic);
  void OnApplicationStateChange(base::android::ApplicationState state);

  bool ShouldCreateSpareRendererWithAvailableMemory(
      int available_memory_mb) const;
#endif

  // Checks various conditions that could prevent an embedder from using the
  // spare.
  std::optional<ContentBrowserClient::SpareProcessRefusedByEmbedderReason>
  DoesEmbedderAllowSpareUsage(BrowserContext* browser_context,
                              SiteInstanceImpl* site_instance);

  base::MemoryPressureListenerRegistration
      memory_pressure_listener_registration_;

  // If this timer is running, then the system is under memory pressure.
  // TODO(380805024): Remove the polling timer when possible.
  base::RepeatingTimer check_memory_pressure_timer_;

  // The clients who want to know when the spare render process host has
  // changed.
  base::ObserverList<Observer> observer_list_;

  // All spare RPHs. RPH instances are self-owned, hence the raw pointers.
  std::vector<RenderProcessHost*> spare_rphs_;

  // The timer used to track the startup time of the spare renderer process.
  // The elapsed time will be tracked even if the spare renderer is destroyed
  // for memory pressure or timeout.
  std::unique_ptr<base::ElapsedTimer> process_startup_timer_;
  // The timer used to track the delay of spare renderer creation.
  std::unique_ptr<base::ElapsedTimer> delay_timer_;
  // The timer used to track the time from the last spare renderer creation to
  // the next call to MaybeTakeSpare(). Note that the created spare renderer
  // might have already been deleted at the time MaybeTakeSpare() runs, but we
  // still want to record it so that we can potentially adjust the timeout of
  // the spare renderer.
  std::unique_ptr<base::ElapsedTimer> spare_renderer_maybe_take_timer_;

  base::OneShotTimer deferred_warmup_timer_;
  base::OneShotTimer deferred_destroy_timer_;

  // The reason for there being no spare render process present.
  NoSpareRendererReason no_spare_renderer_reason_ =
      NoSpareRendererReason::kNotYetCreatedFirstLaunch;
  // The process allocation context for the previous successful
  // MaybeTakeSpare() function call.
  std::optional<ProcessAllocationContext> previous_taken_context_;

  // Indicates if the browser is not currently loading content.
  bool is_browser_idle_ = true;

  base::RepeatingTimer metrics_heartbeat_timer_;

  std::optional<LastSpareRendererCreationInfo>
      last_spare_renderer_creation_info_;

#if BUILDFLAG(IS_ANDROID)
  std::unique_ptr<base::android::ApplicationStatusListener>
      app_status_listener_;
  bool is_app_backgroud_;
#endif
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_SPARE_RENDER_PROCESS_HOST_MANAGER_IMPL_H_
