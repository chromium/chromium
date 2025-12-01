// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/spare_render_process_host_manager_impl.h"

#include <optional>

#include "base/check.h"
#include "base/debug/dump_without_crashing.h"
#include "base/memory/memory_pressure_monitor.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/system/sys_info.h"
#include "components/performance_manager/scenario_api/performance_scenarios.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/common/features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_main_runner.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"

namespace content {

using performance_scenarios::LoadingScenario;
using performance_scenarios::PerformanceScenarioObserverList;
using performance_scenarios::ScenarioScope;
using SpareProcessMaybeTakeAction =
    content::RenderProcessHostImpl::SpareProcessMaybeTakeAction;

namespace {

// Enables killing spare renders when memory pressure signal is received.
BASE_FEATURE(kKillSpareRenderOnMemoryPressure,
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, MEMORY_PRESSURE_LEVEL_CRITICAL is used as the threshold that
// determines when a spare RPH can be created or killed. By default,
// MEMORY_PRESSURE_LEVEL_MODERATE is used.
BASE_FEATURE(kSpareRPHUseCriticalMemoryPressure,
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, only the extra RPHs (controlled by the MultipleSpareRPHs
// experiment) are killed on memory pressure. Does nothing if
// kKillSpareRenderOnMemoryPressure is disabled.
BASE_FEATURE(kSpareRPHKeepOneAliveOnMemoryPressure,
             "kSpareRPHKeepOneAliveOnMemoryPressure",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
// Enables the available memory threshold for creating a spare renderer.
BASE_FEATURE_PARAM(bool,
                   kSpareRendererAvailableMemoryThresholdEnabled,
                   &features::kAndroidWarmUpSpareRendererWithTimeout,
                   "spare_renderer_available_memory_threshold_enabled",
                   false);

// Memory threshold for considering a device as "large memory".
BASE_FEATURE_PARAM(int,
                   kLargeMemoryDeviceThresholdMb,
                   &features::kAndroidWarmUpSpareRendererWithTimeout,
                   "large_memory_device_threshold_mb",
                   4200);

// Available memory threshold for "limited memory devices".
BASE_FEATURE_PARAM(int,
                   kLimitedMemoryDeviceAvailableMemoryThresholdMb,
                   &features::kAndroidWarmUpSpareRendererWithTimeout,
                   "limited_memory_device_available_memory_threshold_mb",
                   100);

// Available memory threshold for "large memory devices".
BASE_FEATURE_PARAM(int,
                   kLargeMemoryDeviceAvailableMemoryThresholdMb,
                   &features::kAndroidWarmUpSpareRendererWithTimeout,
                   "large_memory_device_available_memory_threshold_mb",
                   150);
#endif  // BUILDFLAG(IS_ANDROID)

constexpr char kSpareProcessMaybeTakeActionUmaName[] =
    "BrowserRenderProcessHost.SpareProcessMaybeTakeAction";
constexpr char kSpareRendererTakenTimeSinceCreation[] =
    "BrowserRenderProcessHost.SpareRendererTaken.TimeSinceCreation";
constexpr char kSpareRendererTakenIsReady[] =
    "BrowserRenderProcessHost.SpareRendererTaken.IsReady";
constexpr char kSpareRendererDispatchResultUmaName[] =
    "BrowserRenderProcessHost.SpareRendererDispatchResult";
constexpr char kPreviouslyTakenSourceUmaName[] =
    "BrowserRenderProcessHost.SpareRendererPreviouslyTaken.Source";
constexpr char kPreviouslyTakenStageUmaName[] =
    "BrowserRenderProcessHost.SpareRendererPreviouslyTaken.Stage";
constexpr char kPreviouslyTakenForCOOPUMAName[] =
    "BrowserRenderProcessHost.SpareRendererPreviouslyTaken.ForCOOP";
constexpr char kSameNavigationStageCombinationUMAName[] =
    "BrowserRenderProcessHost.SpareRendererTakenInSameNavigation."
    "StageCombination";
constexpr char kSameNavigationForCOOPUMAName[] =
    "BrowserRenderProcessHost.SpareRendererTakenInSameNavigation.ForCOOP";

int ToStageCombinationValue(ProcessAllocationNavigationStage previous_stage,
                            ProcessAllocationNavigationStage current_stage) {
  static_assert(static_cast<int>(ProcessAllocationNavigationStage::kMaxValue) <
                10);
  return static_cast<int>(previous_stage) * 100 +
         static_cast<int>(current_stage);
}

content::NoSpareRendererReason MapToNoSpareRendererReason(
    content::SpareRendererDispatchResult dispatch_result) {
  switch (dispatch_result) {
    case content::SpareRendererDispatchResult::kUsed:
      return content::NoSpareRendererReason::kTakenByPreviousNavigation;
    case content::SpareRendererDispatchResult::kTimeout:
      return content::NoSpareRendererReason::kTimeout;
    case content::SpareRendererDispatchResult::kOverridden:
      return content::NoSpareRendererReason::kNotYetCreatedAfterWarmup;
    case content::SpareRendererDispatchResult::kDestroyedNotEnabled:
      return content::NoSpareRendererReason::kNotEnabled;
    case content::SpareRendererDispatchResult::kDestroyedProcessLimit:
      return content::NoSpareRendererReason::kProcessLimit;
    case content::SpareRendererDispatchResult::kProcessExited:
      return content::NoSpareRendererReason::kProcessExited;
    case content::SpareRendererDispatchResult::kProcessHostDestroyed:
      return content::NoSpareRendererReason::kProcessHostDestroyed;
    case content::SpareRendererDispatchResult::kMemoryPressure:
      return content::NoSpareRendererReason::kMemoryPressure;
    case content::SpareRendererDispatchResult::kKillAfterBackgrounded:
      return content::NoSpareRendererReason::kOnceBackgrounded;
  }
}

std::string GetCategorizedSpareProcessMaybeTakeTimeUMAName(
    SpareProcessMaybeTakeAction action) {
  std::string action_name;
  switch (action) {
    case SpareProcessMaybeTakeAction::kNoSparePresent:
      action_name = "NoSparePresent";
      break;
    case SpareProcessMaybeTakeAction::kMismatchedBrowserContext:
      action_name = "MismatchedBrowserContext";
      break;
    case SpareProcessMaybeTakeAction::kMismatchedStoragePartition:
      action_name = "MismatchedStoragePartition";
      break;
    case SpareProcessMaybeTakeAction::kRefusedByEmbedder:
      action_name = "RefusedByEmbedder";
      break;
    case SpareProcessMaybeTakeAction::kSpareTaken:
      action_name = "SpareTaken";
      break;
    case SpareProcessMaybeTakeAction::kRefusedBySiteInstance:
      action_name = "RefusedBySiteInstance";
      break;
    case SpareProcessMaybeTakeAction::kRefusedForPdfContent:
      action_name = "RefusedForPdfContent";
      break;
    case SpareProcessMaybeTakeAction::kRefusedForJitMismatch:
      action_name = "RefusedForJitMismatch";
      break;
    case SpareProcessMaybeTakeAction::kRefusedForV8OptimizationMismatch:
      action_name = "RefusedForV8OptimizationMismatch";
      break;
    case SpareProcessMaybeTakeAction::kRefusedNonNavigation:
      action_name = "RefusedNonNavigation";
      break;
    case SpareProcessMaybeTakeAction::kCannotAddThrottle:
      action_name = "CannotAddThrottle";
      break;
  }
  return base::StrCat(
      {"BrowserRenderProcessHost.SpareProcessMaybeTakeTime.", action_name});
}

std::string_view GetNoSpareRendererReasonName(NoSpareRendererReason reason) {
  switch (reason) {
    case NoSpareRendererReason::kNotYetCreated:
      return "NotYetCreated";
    case NoSpareRendererReason::kTakenByPreviousNavigation:
      return "TakenByPreviousNavigation";
    case NoSpareRendererReason::kTimeout:
      return "Timeout";
    case NoSpareRendererReason::kNotEnabled:
      return "NotEnabled";
    case NoSpareRendererReason::kProcessLimit:
      return "ProcessLimit";
    case NoSpareRendererReason::kMemoryPressure:
      return "MemoryPressure";
    case NoSpareRendererReason::kProcessExited:
      return "ProcessExited";
    case NoSpareRendererReason::kProcessHostDestroyed:
      return "ProcessHostDestroyed";
    case NoSpareRendererReason::kNotYetCreatedFirstLaunch:
      return "NotYetCreatedFirstLaunch";
    case NoSpareRendererReason::kNotYetCreatedAfterWarmup:
      return "NotYetCreatedAfterWarmup";
    case NoSpareRendererReason::kOnceBackgrounded:
      return "OnceBackgrounded";
  }
}

std::string GetNoSpareRendererAllocationSourceUMAName(
    NoSpareRendererReason reason) {
  return base::StrCat(
      {"BrowserRenderProcessHost.NoSpareRenderer.AllocationSource.",
       GetNoSpareRendererReasonName(reason)});
}

std::string GetNoSpareRendererAllocationNavigationStageUMAName(
    NoSpareRendererReason reason) {
  return base::StrCat(
      {"BrowserRenderProcessHost.NoSpareRenderer.NavigationStage.",
       GetNoSpareRendererReasonName(reason)});
}

std::string GetNoSpareRendererAllocationForCOOPUMAName(
    NoSpareRendererReason reason) {
  return base::StrCat({"BrowserRenderProcessHost.NoSpareRenderer.ForCOOP.",
                       GetNoSpareRendererReasonName(reason)});
}

bool IsCurrentlyUnderMemoryPressure() {
  base::MemoryPressureMonitor* memory_pressure_monitor =
      base::MemoryPressureMonitor::Get();
  if (!memory_pressure_monitor) {
    return false;
  }

  return memory_pressure_monitor->GetCurrentPressureLevel(
             base::MemoryPressureMonitorTag::kSpareRendererHostManager) !=
         base::MEMORY_PRESSURE_LEVEL_NONE;
}

// Returns the number of spare hosts that should be created. Ensures the field
// trial is not activated on excluded machines.
size_t GetSpareRPHCount() {
  // Exclude machines with less than 4gigs of ram.
  if (base::SysInfo::AmountOfPhysicalMemory() < base::GiB(4)) {
    return 1u;
  }
  return features::kMultipleSpareRPHsCount.Get();
}

void LogAllocationContext(const std::string& source_uma_name,
                          const std::string& stage_uma_name,
                          const std::string& for_coop_uma_name,
                          const ProcessAllocationContext& context) {
  base::UmaHistogramEnumeration(source_uma_name, context.source);
  if (context.source == ProcessAllocationSource::kNavigationRequest) {
    CHECK(context.navigation_context.has_value());
    base::UmaHistogramEnumeration(stage_uma_name,
                                  context.navigation_context->stage);
    base::UmaHistogramBoolean(
        for_coop_uma_name,
        context.navigation_context->requires_new_process_for_coop);
  }
}

void LogNoSparePresentUmas(
    NoSpareRendererReason no_spare_renderer_reason,
    const ProcessAllocationContext& allocation_context,
    const std::optional<ProcessAllocationContext>& previous_taken_context) {
  base::UmaHistogramEnumeration(
      "BrowserRenderProcessHost.NoSparePresentReason2",
      no_spare_renderer_reason);
  // Log the allocation context, categorized by NoSpareRendererReason.
  LogAllocationContext(
      GetNoSpareRendererAllocationSourceUMAName(no_spare_renderer_reason),
      GetNoSpareRendererAllocationNavigationStageUMAName(
          no_spare_renderer_reason),
      GetNoSpareRendererAllocationForCOOPUMAName(no_spare_renderer_reason),
      allocation_context);
  if (no_spare_renderer_reason ==
          NoSpareRendererReason::kTakenByPreviousNavigation &&
      previous_taken_context.has_value()) {
    LogAllocationContext(
        kPreviouslyTakenSourceUmaName, kPreviouslyTakenStageUmaName,
        kPreviouslyTakenForCOOPUMAName, previous_taken_context.value());
    const auto& previous_navigation_context =
        previous_taken_context->navigation_context;
    const auto& current_navigation_context =
        allocation_context.navigation_context;
    // Especially, if both the previous and the current allocation happens
    // during the navigation. We will log the combination of the navigation
    // stage and whether the allocation is caused by a COOP swap.
    if (previous_navigation_context.has_value() &&
        current_navigation_context.has_value() &&
        previous_navigation_context->navigation_id ==
            current_navigation_context->navigation_id) {
      base::UmaHistogramSparse(
          kSameNavigationStageCombinationUMAName,
          ToStageCombinationValue(previous_navigation_context->stage,
                                  current_navigation_context->stage));
      // Local traces found that the spare renderer allocation tends to fail
      // if we receive a COOP header in the response. The UMA is added to verify
      // the frequency.
      base::UmaHistogramBoolean(
          kSameNavigationForCOOPUMAName,
          current_navigation_context->requires_new_process_for_coop);
    }
  }
}

void LogSpareProcessTakeActionUMAs(
    RenderProcessHost* host,
    SpareProcessMaybeTakeAction action,
    const ProcessAllocationContext& allocation_context) {
  base::UmaHistogramEnumeration(kSpareProcessMaybeTakeActionUmaName, action);
  if (allocation_context.source ==
      ProcessAllocationSource::kNavigationRequest) {
    base::UmaHistogramEnumeration(
        base::StrCat(
            {kSpareProcessMaybeTakeActionUmaName, ".NavigationRequest"}),
        action);
  }
  if (action == SpareProcessMaybeTakeAction::kSpareTaken) {
    CHECK(host);
    base::UmaHistogramBoolean(kSpareRendererTakenIsReady, host->IsReady());
    base::UmaHistogramLongTimes(
        kSpareRendererTakenTimeSinceCreation,
        base::TimeTicks::Now() - host->GetLastInitTime());
  }
}

// Returns the MemoryPressureLevel threshold that determines when a spare RPH
// can be created or killed.
base::MemoryPressureLevel GetMemoryPressureLevelThreshold() {
  if (base::FeatureList::IsEnabled(kSpareRPHUseCriticalMemoryPressure)) {
    return base::MEMORY_PRESSURE_LEVEL_CRITICAL;
  }
  return base::MEMORY_PRESSURE_LEVEL_MODERATE;
}

}  // namespace

SpareRenderProcessHostManagerImpl::SpareRenderProcessHostManagerImpl()
    : memory_pressure_listener_registration_(
          FROM_HERE,
          base::MemoryPressureListenerTag::kSpareRenderProcessHostManagerImpl,
          this),
      check_memory_pressure_timer_(
          FROM_HERE,
          base::Minutes(5),
          base::BindRepeating(
              &SpareRenderProcessHostManagerImpl::CheckIfMemoryPressureEnded,
              base::Unretained(this))),
      metrics_heartbeat_timer_(
          FROM_HERE,
          base::Minutes(2),
          base::BindRepeating(
              &SpareRenderProcessHostManagerImpl::OnMetricsHeartbeatTimerFired,
              base::Unretained(this)))
#if BUILDFLAG(IS_ANDROID)
      ,
      app_status_listener_(
          base::android::ApplicationStatusListener::New(base::BindRepeating(
              &SpareRenderProcessHostManagerImpl::OnApplicationStateChange,
              base::Unretained(this))))
#endif
{
  metrics_heartbeat_timer_.Reset();

  // Immediately start the timer if the system is already under memory pressure.
  if (IsCurrentlyUnderMemoryPressure()) {
    check_memory_pressure_timer_.Reset();
  }

  // Need to register first before checking the state to make sure we don't miss
  // a notification.
  if (auto performance_scenario_observer_list =
          PerformanceScenarioObserverList::GetForScope(
              ScenarioScope::kGlobal)) {
    // Note: SpareRenderProcessHostManagerImpl is a global singleton using
    // base::NoDestructor, so no need to call RemoveObserver() later.
    performance_scenario_observer_list->AddObserver(this);

    is_browser_idle_ =
        performance_scenarios::GetLoadingScenario(ScenarioScope::kGlobal)
            ->load(std::memory_order_relaxed) ==
        LoadingScenario::kNoPageLoading;
  }
#if BUILDFLAG(IS_ANDROID)
  OnApplicationStateChange(
      base::android::ApplicationStatusListener::GetState());
#endif
}

SpareRenderProcessHostManagerImpl::~SpareRenderProcessHostManagerImpl() =
    default;

// static
SpareRenderProcessHostManager& SpareRenderProcessHostManager::Get() {
  return SpareRenderProcessHostManagerImpl::Get();
}

// static
SpareRenderProcessHostManagerImpl& SpareRenderProcessHostManagerImpl::Get() {
  static base::NoDestructor<SpareRenderProcessHostManagerImpl> s_instance;
  return *s_instance;
}

void SpareRenderProcessHostManagerImpl::StartDestroyTimer(
    std::optional<base::TimeDelta> timeout) {
  if (!timeout) {
    return;
  }
  deferred_destroy_timer_.Start(
      FROM_HERE, timeout.value(),
      base::BindOnce(&SpareRenderProcessHostManagerImpl::CleanupSpares,
                     base::Unretained(this),
                     SpareRendererDispatchResult::kTimeout));
}

bool SpareRenderProcessHostManagerImpl::DestroyTimerWillFireBefore(
    base::TimeDelta timeout) {
  return deferred_destroy_timer_.IsRunning() &&
         deferred_destroy_timer_.GetCurrentDelay() < timeout;
}

void SpareRenderProcessHostManagerImpl::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void SpareRenderProcessHostManagerImpl::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

RenderProcessHost* SpareRenderProcessHostManagerImpl::WarmupSpare(
    BrowserContext* browser_context) {
  return WarmupSpare(browser_context, std::nullopt);
}

const std::vector<RenderProcessHost*>&
SpareRenderProcessHostManagerImpl::GetSpares() {
  return spare_rphs_;
}

std::vector<ChildProcessId> SpareRenderProcessHostManagerImpl::GetSpareIds() {
  std::vector<ChildProcessId> spare_ids;
  spare_ids.reserve(spare_rphs_.size());
  for (RenderProcessHost* spare_rph : spare_rphs_) {
    spare_ids.push_back(spare_rph->GetID());
  }
  return spare_ids;
}

void SpareRenderProcessHostManagerImpl::CleanupSparesForTesting() {
  CleanupSpares(std::nullopt);
}

const std::optional<LastSpareRendererCreationInfo>&
SpareRenderProcessHostManagerImpl::GetLastSpareRendererCreationInfo() const {
  return last_spare_renderer_creation_info_;
}

RenderProcessHost* SpareRenderProcessHostManagerImpl::WarmupSpare(
    BrowserContext* browser_context,
    std::optional<base::TimeDelta> timeout) {
  if (delay_timer_) {
    // If the timeout does not have a value, the delayed creation is no longer
    // required since we will create the spare renderer here.
    // Otherwise we will create the spare renderer and have the delayed creation
    // override the timeout later on.
    if (!timeout.has_value()) {
      UMA_HISTOGRAM_TIMES("BrowserRenderProcessHost.SpareProcessDelayTime",
                          delay_timer_->Elapsed());
      delay_timer_.reset();
    }
  }

  // Check if there's already an existing, matching, spare.
  RenderProcessHost* spare_rph =
      !spare_rphs_.empty() ? spare_rphs_.at(0) : nullptr;
  if (spare_rph && spare_rph->GetBrowserContext() == browser_context) {
    DCHECK_EQ(browser_context->GetDefaultStoragePartition(),
              spare_rph->GetStoragePartition());

    // Use the new timeout if the specified timeout will be triggered after the
    // current timeout (or not triggered at all).
    if (!timeout.has_value() || DestroyTimerWillFireBefore(timeout.value())) {
      deferred_destroy_timer_.Stop();
      StartDestroyTimer(timeout);
    }
    return nullptr;
  }

  bool had_spare_renderer = !!spare_rph;
  CleanupSpares(SpareRendererDispatchResult::kOverridden);
  CHECK(no_spare_renderer_reason_ ==
        NoSpareRendererReason::kNotYetCreatedAfterWarmup);
  UMA_HISTOGRAM_BOOLEAN(
      "BrowserRenderProcessHost.SpareProcessEvictedOtherSpare",
      had_spare_renderer);

  // Don't create a spare renderer for a BrowserContext that is in the
  // process of shutting down.
  if (browser_context->ShutdownStarted()) {
    // Create a crash dump to help us assess what scenarios trigger this
    // path to be taken.
    // TODO(acolwell): Remove this call once are confident we've eliminated
    // any problematic callers.
    base::debug::DumpWithoutCrashing();

    return nullptr;
  }

  if (BrowserMainRunner::ExitedMainMessageLoop()) {
    // Don't create a new process when the browser is shutting down. No
    // DumpWithoutCrashing here since there are known cases in the wild. See
    // https://crbug.com/40274462 for details.
    return nullptr;
  }

  // Don't create a spare renderer if we're using --single-process or if we've
  // got too many processes.
  if (RenderProcessHost::IsProcessLimitReached()) {
    no_spare_renderer_reason_ = NoSpareRendererReason::kProcessLimit;
    return nullptr;
  }

  // Don't create a spare renderer when the system is under load.  This is
  // currently approximated by only looking at the memory pressure.  See also
  // https://crbug.com/852905.
  auto* memory_monitor = base::MemoryPressureMonitor::Get();
  if (memory_monitor &&
      memory_monitor->GetCurrentPressureLevel(
          base::MemoryPressureMonitorTag::kSpareRendererHostManager) >=
          GetMemoryPressureLevelThreshold()) {
    no_spare_renderer_reason_ = NoSpareRendererReason::kMemoryPressure;
    return nullptr;
  }

#if BUILDFLAG(IS_ANDROID)
  if (features::kAndroidSpareRendererKillWhenBackgrounded.Get() &&
      is_app_backgroud_) {
    no_spare_renderer_reason_ = NoSpareRendererReason::kOnceBackgrounded;
    return nullptr;
  }

  base::SystemMemoryInfo meminfo;
  base::GetSystemMemoryInfo(&meminfo);
  if (!ShouldCreateSpareRendererWithAvailableMemory(
          static_cast<int>(meminfo.available.InMiB()))) {
    no_spare_renderer_reason_ = NoSpareRendererReason::kMemoryPressure;
    return nullptr;
  }

  base::UmaHistogramMemoryLargeMB(
      "BrowserRenderProcessHost.AvailableMemoryBeforeCreation.SpareRenderer",
      meminfo.available);
  last_spare_renderer_creation_info_ = LastSpareRendererCreationInfo{
      .creation_time = base::TimeTicks::Now(),
      .available_memory_mb = static_cast<int>(meminfo.available.InMiB())};
#endif

  process_startup_timer_ = std::make_unique<base::ElapsedTimer>();

  // Start the timer to track how long it takes for a spare renderer to be used.
  // Note: This timer only makes sense when there is a single spare.
  if (GetSpareRPHCount() == 1u) {
    spare_renderer_maybe_take_timer_ = std::make_unique<base::ElapsedTimer>();
  }
  RenderProcessHost* new_spare_rph =
      RenderProcessHostImpl::CreateSpareRenderProcessHost(
          browser_context, nullptr /* site_instance */);
  new_spare_rph->AddObserver(this);
  new_spare_rph->Init();
  spare_rphs_.push_back(new_spare_rph);

  // Use the new timeout if there is no previous renderer or
  // the specified timeout will be triggered after the current timeout
  // (or not triggered at all).
  if (!had_spare_renderer || !timeout.has_value() ||
      DestroyTimerWillFireBefore(timeout.value())) {
    deferred_destroy_timer_.Stop();
    StartDestroyTimer(timeout);
  }

  // The spare render process isn't ready, so wait and do the "spare render
  // process changed" callback in RenderProcessReady().
  return new_spare_rph;
}

void SpareRenderProcessHostManagerImpl::DeferredWarmupSpare(
    BrowserContext* browser_context,
    base::TimeDelta delay,
    std::optional<base::TimeDelta> timeout) {
  delay_timer_ = std::make_unique<base::ElapsedTimer>();
  deferred_warmup_timer_.Start(
      FROM_HERE, delay,
      base::BindOnce(
          [](SpareRenderProcessHostManagerImpl* self,
             base::WeakPtr<BrowserContext> browser_context,
             std::optional<base::TimeDelta> timeout) {
            // Don't create spare process if the browser context is destroyed
            // or the shutdown has started.
            if (browser_context && !browser_context->ShutdownStarted()) {
              self->WarmupSpare(browser_context.get(), timeout);
            }
          },
          base::Unretained(this), browser_context->GetWeakPtr(), timeout));
}

RenderProcessHost* SpareRenderProcessHostManagerImpl::MaybeTakeSpare(
    BrowserContext* browser_context,
    SiteInstanceImpl* site_instance,
    const ProcessAllocationContext& allocation_context) {
  // Get the StoragePartition for |site_instance|.  Note that this might be
  // different than the default StoragePartition for |browser_context|.
  StoragePartition* site_storage =
      browser_context->GetStoragePartition(site_instance);

  // Bail early if there is no spare renderer available.
  SpareProcessMaybeTakeAction action =
      SpareProcessMaybeTakeAction::kNoSparePresent;
  RenderProcessHost* next_spare_rph =
      !spare_rphs_.empty() ? spare_rphs_.at(0) : nullptr;
  if (!next_spare_rph) {
    LogNoSparePresentUmas(no_spare_renderer_reason_, allocation_context,
                          previous_taken_context_);
  } else if (browser_context != next_spare_rph->GetBrowserContext()) {
    action = SpareProcessMaybeTakeAction::kMismatchedBrowserContext;
  } else if (!next_spare_rph->InSameStoragePartition(site_storage)) {
    action = SpareProcessMaybeTakeAction::kMismatchedStoragePartition;
  } else if (auto refuse_reason =
                 DoesEmbedderAllowSpareUsage(browser_context, site_instance);
             refuse_reason.has_value()) {
    base::UmaHistogramEnumeration(
        "BrowserRenderProcessHost.SpareProcessRefusedByEmbedderReason",
        refuse_reason.value());
    action = SpareProcessMaybeTakeAction::kRefusedByEmbedder;
  } else if (
      // We shouldn't use the spare if:
      // 1. The SiteInstance has already got an associated process.  This is
      //    important to avoid taking and then immediately discarding the spare
      //    for process-per-site scenarios (which the HasProcess call below
      //    accounts for).  Note that HasProcess will return false and allow
      //    using the spare if the given process-per-site process hasn't been
      //    launched.
      // 2. The SiteInstance has opted out of using the spare process.
      // 3. The SiteInstance is a guest SiteInstance.
      // 4. The SiteInstance is a initial WebUI SiteInstance.
      site_instance->HasProcess() ||
      !site_instance->CanAssociateWithSpareProcess() || site_instance->IsGuest()
#if !BUILDFLAG(IS_ANDROID)
      || GetContentClient()->browser()->IsInitialWebUIURL(
             site_instance->GetSiteURL())
#endif
  ) {
    action = SpareProcessMaybeTakeAction::kRefusedBySiteInstance;
  } else if (site_instance->GetSiteInfo().is_pdf()) {
    action = SpareProcessMaybeTakeAction::kRefusedForPdfContent;
  } else if (next_spare_rph->IsJitDisabled() !=
             site_instance->GetSiteInfo().is_jit_disabled()) {
    action = SpareProcessMaybeTakeAction::kRefusedForJitMismatch;
  } else if (next_spare_rph->AreV8OptimizationsDisabled() !=
             site_instance->GetSiteInfo().are_v8_optimizations_disabled()) {
    action = SpareProcessMaybeTakeAction::kRefusedForV8OptimizationMismatch;
  }
#if BUILDFLAG(IS_ANDROID)
  // Always allow test to allocate a spare renderer so as
  // not to break existing tests.
  else if (allocation_context.source == ProcessAllocationSource::kTest) {
    action = SpareProcessMaybeTakeAction::kSpareTaken;
  } else if (features::kAndroidSpareRendererOnlyForNavigation.Get() &&
             !allocation_context.IsForNavigation()) {
    action = SpareProcessMaybeTakeAction::kRefusedNonNavigation;
  } else if (base::FeatureList::IsEnabled(
                 features::kAndroidWarmUpSpareRendererWithTimeout) &&
             features::kAndroidSpareRendererAddNavigationThrottle.Get() &&
             (!allocation_context.navigation_context.has_value() ||
              allocation_context.navigation_context->stage !=
                  ProcessAllocationNavigationStage::kBeforeNetworkRequest)) {
    // All the renderers returned by MaybeTakeSpare is of lowest priority on
    // Android. To ensure the liveness of the renderer process, we need to
    // add a throttle to ensure the priority update before deciding the final
    // renderer process for the navigation. Thus we can only use the spare
    // renderer for navigation before sending the network request.
    action = SpareProcessMaybeTakeAction::kCannotAddThrottle;
  }
#endif
  else {
    action = SpareProcessMaybeTakeAction::kSpareTaken;
  }
  LogSpareProcessTakeActionUMAs(next_spare_rph, action, allocation_context);

  if (spare_renderer_maybe_take_timer_) {
    auto maybe_take_time = spare_renderer_maybe_take_timer_->Elapsed();
    base::UmaHistogramLongTimes(
        "BrowserRenderProcessHost.SpareProcessMaybeTakeTime", maybe_take_time);
    base::UmaHistogramLongTimes(
        GetCategorizedSpareProcessMaybeTakeTimeUMAName(action),
        maybe_take_time);
  }

  // Decide whether to take or drop the spare process.
  RenderProcessHost* returned_process = nullptr;
  if (action == SpareProcessMaybeTakeAction::kSpareTaken) {
    CHECK(next_spare_rph->HostHasNotBeenUsed());

    // If the spare process ends up getting killed, the spare manager should
    // discard the spare RPH, so if one exists, it should always be live here.
    CHECK(next_spare_rph->IsInitializedAndNotDead());

    returned_process = next_spare_rph;
    previous_taken_context_ = allocation_context;
    ReleaseSpare(next_spare_rph, SpareRendererDispatchResult::kUsed);
  } else if (!RenderProcessHostImpl::IsSpareProcessKeptAtAllTimes()) {
    // If the spare shouldn't be kept around, then discard it as soon as we
    // find that the current spare was mismatched.
    CleanupSpares(SpareRendererDispatchResult::kDestroyedNotEnabled);
    CHECK(no_spare_renderer_reason_ == NoSpareRendererReason::kNotEnabled);
  } else if (RenderProcessHost::IsProcessLimitReached()) {
    // Drop all spares if we are at a process limit and the spare wasn't taken.
    // This helps avoid process reuse.
    // TODO(pmonette): Only cleanup n spares, where n is the count of processes
    // that is over the limit.
    CleanupSpares(SpareRendererDispatchResult::kDestroyedProcessLimit);
    CHECK(no_spare_renderer_reason_ == NoSpareRendererReason::kProcessLimit);
  }

#if BUILDFLAG(IS_ANDROID)
  // SetHasSpareRendererPriority(false) will cause the priority to drop until
  // further updates are made. For navigation requests we will keep the priority
  // until the RenderFrameHostImpl constructor sets the priority.
  if (returned_process && !allocation_context.IsForNavigation()) {
    returned_process->GraduateSpareToNormalRendererPriority();
  }
#endif

  return returned_process;
}

std::optional<ContentBrowserClient::SpareProcessRefusedByEmbedderReason>
SpareRenderProcessHostManagerImpl::DoesEmbedderAllowSpareUsage(
    BrowserContext* browser_context,
    SiteInstanceImpl* site_instance) {
  // Give embedder a chance to disable using a spare RenderProcessHost for
  // certain SiteInstances.  Some navigations, such as to NTP or extensions,
  // require passing command-line flags to the renderer process at process
  // launch time, but this cannot be done for spare RenderProcessHosts, which
  // are started before it is known which navigation might use them.  So, a
  // spare RenderProcessHost should not be used in such cases.
  //
  // Note that exempting NTP and extensions from using the spare process might
  // also happen via HasProcess check below (which returns true for
  // process-per-site SiteInstances if the given process-per-site process
  // already exists).  Despite this potential overlap, it is important to do
  // both kinds of checks (to account for other non-ntp/extension
  // process-per-site scenarios + to work correctly even if
  // ShouldUseSpareRenderProcessHost starts covering non-process-per-site
  // scenarios).
  std::optional<ContentBrowserClient::SpareProcessRefusedByEmbedderReason>
      refuse_reason;
  if (!GetContentClient()->browser()->ShouldUseSpareRenderProcessHost(
          browser_context, site_instance->GetSiteInfo().site_url(),
          refuse_reason)) {
    CHECK(refuse_reason.has_value());
    return refuse_reason;
  }

  // The spare RenderProcessHost always launches with JIT enabled, so if JIT
  // is disabled for the site then it's not possible to use this as the JIT
  // policy will differ.
  if (GetContentClient()->browser()->IsJitDisabledForSite(
          browser_context, site_instance->GetSiteInfo().GetProcessLockURL())) {
    return ContentBrowserClient::SpareProcessRefusedByEmbedderReason::
        JitDisabled;
  }

  // V8 optimizations are globally enabled or disabled for a whole process,
  // and spare renderers always have V8 optimizations enabled, so we can never
  // use them if they're supposed to be disabled for this site.
  if (!GetContentClient()->browser()->AreV8OptimizationsEnabledForSite(
          browser_context, std::nullopt,
          site_instance->GetSiteInfo().GetProcessLockURL())) {
    return ContentBrowserClient::SpareProcessRefusedByEmbedderReason::
        V8OptimizationsDisabled;
  }

  // V8 feature flags are globally initialized during renderer process
  // startup, and spare renderers allow V8 feature flag overrides by default.
  // As such spare renderers should not be used when v8 flag overrides are
  // disabled.
  if (GetContentClient()->browser()->DisallowV8FeatureFlagOverridesForSite(
          site_instance->GetSiteInfo().GetProcessLockURL())) {
    return ContentBrowserClient::SpareProcessRefusedByEmbedderReason::
        DisallowV8FeatureFlagOverrides;
  }

  return std::nullopt;
}

void SpareRenderProcessHostManagerImpl::PrepareForFutureRequests(
    BrowserContext* browser_context,
    std::optional<base::TimeDelta> delay) {
  if (RenderProcessHostImpl::IsSpareProcessKeptAtAllTimes()) {
    std::optional<base::TimeDelta> timeout = std::nullopt;
    if (base::FeatureList::IsEnabled(
            features::kAndroidWarmUpSpareRendererWithTimeout)) {
      if (features::kAndroidSpareRendererCreationTiming.Get() !=
          features::kAndroidSpareRendererCreationDelayedDuringLoading) {
        // The creation of the spare renderer will be managed in
        // WebContentsImpl::DidStopLoading or
        // WebContentsImpl::OnFirstVisuallyNonEmptyPaint.
        return;
      }
      if (features::kAndroidSpareRendererTimeoutSeconds.Get() > 0) {
        timeout =
            base::Seconds(features::kAndroidSpareRendererTimeoutSeconds.Get());
      }
    }
    // Always keep around a spare process for the most recently requested
    // |browser_context|.
    if (delay.has_value()) {
      DeferredWarmupSpare(browser_context, *delay, timeout);
    } else {
      WarmupSpare(browser_context, timeout);
    }
  } else {
    // Discard the ignored (probably non-matching) spares so as not to waste
    // resources.
    CleanupSpares(SpareRendererDispatchResult::kDestroyedNotEnabled);
    CHECK(no_spare_renderer_reason_ == NoSpareRendererReason::kNotEnabled);
  }
}

void SpareRenderProcessHostManagerImpl::CleanupSpares(
    std::optional<SpareRendererDispatchResult> dispatch_result) {
  std::vector<RenderProcessHost*> spare_rphs = std::move(spare_rphs_);

  // Stop the destroy timer since it is no longer required.
  deferred_destroy_timer_.Stop();

  for (RenderProcessHost* spare_rph : spare_rphs) {
    if (dispatch_result.has_value()) {
      base::UmaHistogramEnumeration(kSpareRendererDispatchResultUmaName,
                                    dispatch_result.value());
    }
    // Stop observing the process, to avoid getting notifications as a
    // consequence of the Cleanup call below - such notification could call
    // back into CleanupSpare leading to stack overflow.
    spare_rph->RemoveObserver(this);

    // Make sure the RenderProcessHost object gets destroyed.
    if (!spare_rph->AreRefCountsDisabled()) {
      spare_rph->Cleanup();
    }

    for (auto& observer : observer_list_) {
      observer.OnSpareRenderProcessHostRemoved(spare_rph);
    }
  }
  if (dispatch_result.has_value()) {
    no_spare_renderer_reason_ =
        MapToNoSpareRendererReason(dispatch_result.value());
    // The timer is not reset during the timeout to collect data about
    // when the spare renderers will be used without timeout. The data
    // will be used to set an appropriate timeout value.
    if (dispatch_result.value() != SpareRendererDispatchResult::kTimeout) {
      spare_renderer_maybe_take_timer_.reset();
    }
  }
}

void SpareRenderProcessHostManagerImpl::CleanupExtraSpares(
    std::optional<SpareRendererDispatchResult> dispatch_result) {
  if (spare_rphs_.size() <= 1u) {
    // There is either zero or one spare. Nothing to do.
    return;
  }

  // Pop the front element, as we want to preserve it.
  RenderProcessHost* first_spare = spare_rphs_.front();

  // Swap the front and back to efficient removal.
  std::swap(spare_rphs_.front(), spare_rphs_.back());
  spare_rphs_.pop_back();

  // Cleanup all remaining spares in the vector.
  CleanupSpares(dispatch_result);

  // Re-add the spare to the vector.
  spare_rphs_.push_back(first_spare);
}

void SpareRenderProcessHostManagerImpl::SetDeferTimerTaskRunnerForTesting(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  deferred_warmup_timer_.SetTaskRunner(task_runner);
  deferred_destroy_timer_.SetTaskRunner(task_runner);
}

void SpareRenderProcessHostManagerImpl::SetIsBrowserIdleForTesting(
    bool is_browser_idle) {
  DCHECK(!PerformanceScenarioObserverList::GetForScope(ScenarioScope::kGlobal));
  SetIsBrowserIdle(is_browser_idle);
}

void SpareRenderProcessHostManagerImpl::ReleaseSpare(
    RenderProcessHost* host,
    SpareRendererDispatchResult dispatch_result) {
  // Erase while intentionally preserving the order of the other elements.
  size_t removed = std::erase(spare_rphs_, host);
  base::UmaHistogramEnumeration(kSpareRendererDispatchResultUmaName,
                                dispatch_result);
  CHECK_EQ(removed, 1u);
  host->RemoveObserver(this);
  for (auto& observer : observer_list_) {
    observer.OnSpareRenderProcessHostRemoved(host);
  }

  if (spare_rphs_.empty()) {
    no_spare_renderer_reason_ = MapToNoSpareRendererReason(dispatch_result);
  }

  // Since that a spare was just released, check if we need to start another.
  MaybeCreateExtraSpare();
}

void SpareRenderProcessHostManagerImpl::RenderProcessReady(
    RenderProcessHost* host) {
  CHECK(base::Contains(spare_rphs_, host));

  CHECK(process_startup_timer_);
  UMA_HISTOGRAM_TIMES("BrowserRenderProcessHost.SpareProcessStartupTime",
                      process_startup_timer_->Elapsed());

  process_startup_timer_.reset();

  for (auto& observer : observer_list_) {
    observer.OnSpareRenderProcessHostReady(host);
  }

  // Now that a spare was just fully initialized, check if we need to start
  // another.
  MaybeCreateExtraSpare();
}

void SpareRenderProcessHostManagerImpl::RenderProcessExited(
    RenderProcessHost* host,
    const ChildProcessTerminationInfo& info) {
  ReleaseSpare(host, SpareRendererDispatchResult::kProcessExited);

  // Make sure the RenderProcessHost object gets destroyed.
  if (!host->AreRefCountsDisabled()) {
    host->Cleanup();
  }

  if (spare_rphs_.empty()) {
    deferred_destroy_timer_.Stop();
  }
}

void SpareRenderProcessHostManagerImpl::RenderProcessHostDestroyed(
    RenderProcessHost* host) {
  ReleaseSpare(host, SpareRendererDispatchResult::kProcessHostDestroyed);
}

void SpareRenderProcessHostManagerImpl::OnLoadingScenarioChanged(
    ScenarioScope scope,
    LoadingScenario old_scenario,
    LoadingScenario new_scenario) {
  SetIsBrowserIdle(new_scenario == LoadingScenario::kNoPageLoading);
}

void SpareRenderProcessHostManagerImpl::SetIsBrowserIdle(bool is_browser_idle) {
  if (is_browser_idle_ == is_browser_idle) {
    return;
  }

  is_browser_idle_ = is_browser_idle;

  // Now that the browser is idle, check if we need to start another spare.
  MaybeCreateExtraSpare();
}

void SpareRenderProcessHostManagerImpl::OnMemoryPressure(
    base::MemoryPressureLevel memory_pressure_level) {
  if (memory_pressure_level < GetMemoryPressureLevelThreshold()) {
    return;
  }

  CHECK_NE(memory_pressure_level, base::MEMORY_PRESSURE_LEVEL_NONE);
  if (check_memory_pressure_timer_.IsRunning() ||
      !base::FeatureList::IsEnabled(kKillSpareRenderOnMemoryPressure)) {
    return;
  }

  if (base::FeatureList::IsEnabled(kSpareRPHKeepOneAliveOnMemoryPressure)) {
    CleanupExtraSpares(SpareRendererDispatchResult::kMemoryPressure);
  } else {
    CleanupSpares(SpareRendererDispatchResult::kMemoryPressure);
    CHECK(no_spare_renderer_reason_ == NoSpareRendererReason::kMemoryPressure);
  }

  // `reset()` will start the timer.
  check_memory_pressure_timer_.Reset();
}

void SpareRenderProcessHostManagerImpl::CheckIfMemoryPressureEnded() {
  if (IsCurrentlyUnderMemoryPressure()) {
    return;
  }

  check_memory_pressure_timer_.Stop();

  // Now that the system is no longer under memory pressure, check if we need
  // to start another spare.
  MaybeCreateExtraSpare();
}

bool SpareRenderProcessHostManagerImpl::ShouldCreateExtraSpare() const {
  // Check target spare count. This function has the side-effect of
  // activating the field trial.
  if (spare_rphs_.size() >= GetSpareRPHCount()) {
    return false;
  }

  // Avoid doing work on shutdown.
  if (BrowserMainRunner::ExitedMainMessageLoop()) {
    return false;
  }

  // Only create extra spares when the browser is idle.
  if (!is_browser_idle_) {
    return false;
  }

  // The first spare is created using either WarmupSpare() or
  // PrepareForFutureRequests().
  if (spare_rphs_.empty()) {
    return false;
  }

  // Avoid doing work on shutdown again.
  if (spare_rphs_.back()->GetBrowserContext()->ShutdownStarted()) {
    return false;
  }

  // Don't create spares beyond the renderer count limit.
  if (RenderProcessHost::IsProcessLimitReached()) {
    return false;
  }

  // Don't create spares when under memory pressure.
  if (check_memory_pressure_timer_.IsRunning()) {
    return false;
  }

  // A spare is already being initialized right now.
  if (!spare_rphs_.back()->IsReady()) {
    return false;
  }

  return true;
}

void SpareRenderProcessHostManagerImpl::MaybeCreateExtraSpare() {
  if (!ShouldCreateExtraSpare()) {
    return;
  }

  // Use the same browser context of an existing spare.
  BrowserContext* browser_context = spare_rphs_.at(0)->GetBrowserContext();

  process_startup_timer_ = std::make_unique<base::ElapsedTimer>();
  RenderProcessHost* new_spare_rph =
      RenderProcessHostImpl::CreateSpareRenderProcessHost(
          browser_context, nullptr /* site_instance */);
  new_spare_rph->AddObserver(this);
  new_spare_rph->Init();
  spare_rphs_.push_back(new_spare_rph);
}

void SpareRenderProcessHostManagerImpl::OnMetricsHeartbeatTimerFired() {
  base::UmaHistogramCounts100("BrowserRenderProcessHost.SpareCount",
                              spare_rphs_.size());
}

#if BUILDFLAG(IS_ANDROID)
bool SpareRenderProcessHostManagerImpl::
    ShouldCreateSpareRendererWithAvailableMemory(
        int available_memory_mb) const {
  if (!kSpareRendererAvailableMemoryThresholdEnabled.Get()) {
    return true;
  }

  const int total_memory_mb = base::SysInfo::AmountOfPhysicalMemory().InMiB();
  const int available_memory_threshold_mb =
      total_memory_mb >= kLargeMemoryDeviceThresholdMb.Get()
          ? kLargeMemoryDeviceAvailableMemoryThresholdMb.Get()
          : kLimitedMemoryDeviceAvailableMemoryThresholdMb.Get();

  return available_memory_mb >= available_memory_threshold_mb;
}

void SpareRenderProcessHostManagerImpl::OnApplicationStateChange(
    base::android::ApplicationState state) {
  if (!features::kAndroidSpareRendererKillWhenBackgrounded.Get()) {
    return;
  }
  using ApplicationState = base::android::ApplicationState;
  switch (state) {
    case ApplicationState::APPLICATION_STATE_UNKNOWN:
      return;
    case ApplicationState::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES:
    case ApplicationState::APPLICATION_STATE_HAS_PAUSED_ACTIVITIES:
      is_app_backgroud_ = false;
      return;
    case ApplicationState::APPLICATION_STATE_HAS_STOPPED_ACTIVITIES:
    case ApplicationState::APPLICATION_STATE_HAS_DESTROYED_ACTIVITIES:
      if (!is_app_backgroud_) {
        CleanupSpares(SpareRendererDispatchResult::kKillAfterBackgrounded);
      }
      is_app_backgroud_ = true;
      return;
  }
}
#endif

}  // namespace content
