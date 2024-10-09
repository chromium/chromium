// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/spare_render_process_host_manager_impl.h"

#include "base/check.h"
#include "base/debug/dump_without_crashing.h"
#include "base/memory/memory_pressure_monitor.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/common/features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_main_runner.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"

namespace content {

SpareRenderProcessHostManagerImpl::SpareRenderProcessHostManagerImpl() =
    default;
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
                     base::Unretained(this)));
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

void SpareRenderProcessHostManagerImpl::WarmupSpare(
    BrowserContext* browser_context) {
  WarmupSpare(browser_context, std::nullopt);
}

const std::vector<RenderProcessHost*>&
SpareRenderProcessHostManagerImpl::GetSpares() {
  return spare_rphs_;
}

std::vector<int> SpareRenderProcessHostManagerImpl::GetSpareIds() {
  std::vector<int> spare_ids;
  spare_ids.reserve(spare_rphs_.size());
  for (RenderProcessHost* spare_rph : spare_rphs_) {
    spare_ids.push_back(spare_rph->GetID());
  }
  return spare_ids;
}

void SpareRenderProcessHostManagerImpl::CleanupSparesForTesting() {
  CleanupSpares();
}

void SpareRenderProcessHostManagerImpl::WarmupSpare(
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
      !spare_rphs_.empty() ? spare_rphs_[0] : nullptr;
  if (spare_rph && spare_rph->GetBrowserContext() == browser_context) {
    DCHECK_EQ(browser_context->GetDefaultStoragePartition(),
              spare_rph->GetStoragePartition());

    // Use the new timeout if the specified timeout will be triggered after the
    // current timeout (or not triggered at all).
    if (!timeout.has_value() || DestroyTimerWillFireBefore(timeout.value())) {
      deferred_destroy_timer_.Stop();
      StartDestroyTimer(timeout);
    }
    return;
  }

  bool had_spare_renderer = !!spare_rph;
  CleanupSpares();
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

    return;
  }

  if (BrowserMainRunner::ExitedMainMessageLoop()) {
    // Don't create a new process when the browser is shutting down. No
    // DumpWithoutCrashing here since there are known cases in the wild. See
    // https://crbug.com/40274462 for details.
    return;
  }

  // Don't create a spare renderer if we're using --single-process or if we've
  // got too many processes. See also ShouldTryToUseExistingProcessHost in
  // this file.
  if (RenderProcessHost::run_renderer_in_process() ||
      RenderProcessHostImpl::GetProcessCountForLimit() >=
          RenderProcessHostImpl::GetMaxRendererProcessCount()) {
    return;
  }

  // Don't create a spare renderer when the system is under load.  This is
  // currently approximated by only looking at the memory pressure.  See also
  // https://crbug.com/852905.
  auto* memory_monitor = base::MemoryPressureMonitor::Get();
  if (memory_monitor &&
      memory_monitor->GetCurrentPressureLevel() >=
          base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE) {
    return;
  }

  process_startup_timer_ = std::make_unique<base::ElapsedTimer>();

  RenderProcessHost* new_spare_rph =
      RenderProcessHostImpl::CreateRenderProcessHost(
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
      refuse_reason =
          GetContentClient()->browser()->ShouldUseSpareRenderProcessHost(
              browser_context, site_instance->GetSiteInfo().site_url());
  bool embedder_allows_spare_usage = !refuse_reason.has_value();

  // The spare RenderProcessHost always launches with JIT enabled, so if JIT
  // is disabled for the site then it's not possible to use this as the JIT
  // policy will differ.
  if (GetContentClient()->browser()->IsJitDisabledForSite(
          browser_context, site_instance->GetSiteInfo().process_lock_url())) {
    embedder_allows_spare_usage = false;
    refuse_reason =
        ContentBrowserClient::SpareProcessRefusedByEmbedderReason::JitDisabled;
  }

  // V8 optimizations are globally enabled or disabled for a whole process, and
  // spare renderers always have V8 optimizations enabled, so we can never use
  // them if they're supposed to be disabled for this site.
  if (GetContentClient()->browser()->AreV8OptimizationsDisabledForSite(
          browser_context, site_instance->GetSiteInfo().process_lock_url())) {
    embedder_allows_spare_usage = false;
    refuse_reason = ContentBrowserClient::SpareProcessRefusedByEmbedderReason::
        V8OptimizationsDisabled;
  }

  if (refuse_reason.has_value()) {
    base::UmaHistogramEnumeration(
        "BrowserRenderProcessHost.SpareProcessRefusedByEmbedderReason",
        refuse_reason.value());
  }

  // Do not use spare renderer if running an experiment to run SkiaFontManager.
  // SkiaFontManager needs to be initialized during renderer creation.
  // This is temporary and will be removed after the experiment has concluded;
  // see crbug.com/335680565.
  bool use_skia_font_manager = false;
#if BUILDFLAG(IS_WIN)
  use_skia_font_manager =
      GetContentClient()->browser()->ShouldUseSkiaFontManager(
          site_instance->GetSiteURL());
#endif

  // We shouldn't use the spare if:
  // 1. The SiteInstance has already got an associated process.  This is
  //    important to avoid taking and then immediately discarding the spare
  //    for process-per-site scenarios (which the HasProcess call below
  //    accounts for).  Note that HasProcess will return false and allow using
  //    the spare if the given process-per-site process hasn't been launched.
  // 2. The SiteInstance has opted out of using the spare process.
  bool site_instance_allows_spare_usage =
      !site_instance->HasProcess() &&
      site_instance->CanAssociateWithSpareProcess();

  bool hosts_pdf_content = site_instance->GetSiteInfo().is_pdf();

  // Get the StoragePartition for |site_instance|.  Note that this might be
  // different than the default StoragePartition for |browser_context|.
  StoragePartition* site_storage =
      browser_context->GetStoragePartition(site_instance);

  // GetSpare UMA metrics.
  using SpareProcessMaybeTakeAction =
      RenderProcessHostImpl::SpareProcessMaybeTakeAction;
  SpareProcessMaybeTakeAction action =
      SpareProcessMaybeTakeAction::kNoSparePresent;

  RenderProcessHost* next_spare_rph =
      !spare_rphs_.empty() ? spare_rphs_[0] : nullptr;

  if (!next_spare_rph) {
    action = SpareProcessMaybeTakeAction::kNoSparePresent;
  } else if (browser_context != next_spare_rph->GetBrowserContext()) {
    action = SpareProcessMaybeTakeAction::kMismatchedBrowserContext;
  } else if (!next_spare_rph->InSameStoragePartition(site_storage)) {
    action = SpareProcessMaybeTakeAction::kMismatchedStoragePartition;
  } else if (!embedder_allows_spare_usage) {
    action = SpareProcessMaybeTakeAction::kRefusedByEmbedder;
  } else if (!site_instance_allows_spare_usage) {
    action = SpareProcessMaybeTakeAction::kRefusedBySiteInstance;
  } else if (hosts_pdf_content) {
    action = SpareProcessMaybeTakeAction::kRefusedForPdfContent;
  } else {
    action = SpareProcessMaybeTakeAction::kSpareTaken;
  }
  UMA_HISTOGRAM_ENUMERATION(
      "BrowserRenderProcessHost.SpareProcessMaybeTakeAction", action);

  // Decide whether to take or drop the spare process.
  RenderProcessHost* returned_process = nullptr;
  if (next_spare_rph &&
      browser_context == next_spare_rph->GetBrowserContext() &&
      next_spare_rph->InSameStoragePartition(site_storage) &&
      !site_instance->IsGuest() && embedder_allows_spare_usage &&
      site_instance_allows_spare_usage && !hosts_pdf_content &&
      !use_skia_font_manager) {
    CHECK(next_spare_rph->HostHasNotBeenUsed());

    // If the spare process ends up getting killed, the spare manager should
    // discard the spare RPH, so if one exists, it should always be live here.
    CHECK(next_spare_rph->IsInitializedAndNotDead());

    DCHECK_EQ(SpareProcessMaybeTakeAction::kSpareTaken, action);
    returned_process = next_spare_rph;
    ReleaseSpare(next_spare_rph);
  } else if (!RenderProcessHostImpl::IsSpareProcessKeptAtAllTimes()) {
    // If the spare shouldn't be kept around, then discard it as soon as we
    // find that the current spare was mismatched.
    CleanupSpares();
  } else if (RenderProcessHostImpl::GetProcessCountForLimit() >=
             RenderProcessHostImpl::GetMaxRendererProcessCount()) {
    // Drop all spares if we are at a process limit and the spare wasn't taken.
    // This helps avoid process reuse.
    // TODO(pmonette): Only cleanup n spares, where n is the count of processes
    // that is over the limit.
    CleanupSpares();
  }

  return returned_process;
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
    CleanupSpares();
  }
}

void SpareRenderProcessHostManagerImpl::CleanupSpares() {
  std::vector<RenderProcessHost*> spare_rphs = std::move(spare_rphs_);

  // Stop the destroy timer since it is no longer required.
  deferred_destroy_timer_.Stop();

  for (RenderProcessHost* spare_rph : spare_rphs) {
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
}

void SpareRenderProcessHostManagerImpl::SetDeferTimerTaskRunnerForTesting(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  deferred_warmup_timer_.SetTaskRunner(task_runner);
  deferred_destroy_timer_.SetTaskRunner(task_runner);
}

void SpareRenderProcessHostManagerImpl::ReleaseSpare(RenderProcessHost* host) {
  // Erase while intentionally preserving the order of the other elements.
  size_t removed = std::erase(spare_rphs_, host);
  CHECK_EQ(removed, 1u);
  host->RemoveObserver(this);
  for (auto& observer : observer_list_) {
    observer.OnSpareRenderProcessHostRemoved(host);
  }
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
}

void SpareRenderProcessHostManagerImpl::RenderProcessExited(
    RenderProcessHost* host,
    const ChildProcessTerminationInfo& info) {
  ReleaseSpare(host);

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
  ReleaseSpare(host);
}

}  // namespace content
