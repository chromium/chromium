// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Represents the browser side of the browser <--> renderer communication
// channel. There will be one RenderProcessHost per renderer process.

#include "content/browser/renderer_host/render_process_host_impl.h"

#include <algorithm>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/base_switches.h"
#include "base/clang_profiling_buildflags.h"
#include "base/command_line.h"
#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/debug/alias.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/memory_pressure_monitor.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/writable_shared_memory_region.h"
#include "base/message_loop/message_pump_type.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_shared_memory.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "base/metrics/statistics_recorder.h"
#include "base/metrics/user_metrics.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/observer_list.h"
#include "base/process/process_handle.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/supports_user_data.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/typed_macros.h"
#include "base/tracing/protos/chrome_track_event.pbzero.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "cc/base/switches.h"
#include "components/discardable_memory/public/mojom/discardable_shared_memory_manager.mojom.h"
#include "components/discardable_memory/service/discardable_shared_memory_manager.h"
#include "components/metrics/single_sample_metrics.h"
#include "components/services/storage/privileged/mojom/indexed_db_control.mojom.h"
#include "components/services/storage/public/cpp/buckets/bucket_id.h"
#include "components/services/storage/public/cpp/buckets/bucket_info.h"
#include "components/services/storage/public/cpp/buckets/constants.h"
#include "components/services/storage/public/cpp/quota_error_or.h"
#include "components/services/storage/public/mojom/cache_storage_control.mojom.h"
#include "components/tracing/common/tracing_switches.h"
#include "components/viz/common/switches.h"
#include "components/viz/host/gpu_client.h"
#include "content/browser/attribution_reporting/attribution_manager.h"
#include "content/browser/bad_message.h"
#include "content/browser/blob_storage/blob_registry_wrapper.h"
#include "content/browser/browser_child_process_host_impl.h"
#include "content/browser/browser_context_impl.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/buckets/bucket_manager.h"
#include "content/browser/child_process_host_impl.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/code_cache/generated_code_cache_context.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/field_trial_recorder.h"
#include "content/browser/field_trial_synchronizer.h"
#include "content/browser/file_system/file_system_manager_impl.h"
#include "content/browser/file_system_access/file_system_access_manager_impl.h"
#include "content/browser/gpu/browser_gpu_client_delegate.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/gpu/gpu_disk_cache_factory.h"
#include "content/browser/locks/lock_manager.h"
#include "content/browser/media/frameless_media_interface_proxy.h"
#include "content/browser/media/media_internals.h"
#include "content/browser/metrics/histogram_controller.h"
#include "content/browser/metrics/histogram_shared_memory_config.h"
#include "content/browser/mime_registry_impl.h"
#include "content/browser/network_service_instance_impl.h"
#include "content/browser/notifications/platform_notification_context_impl.h"
#include "content/browser/payments/payment_app_context_impl.h"
#include "content/browser/permissions/permission_service_context.h"
#include "content/browser/process_lock.h"
#include "content/browser/push_messaging/push_messaging_manager.h"
#include "content/browser/quota/quota_context.h"
#include "content/browser/renderer_host/embedded_frame_sink_provider_impl.h"
#include "content/browser/renderer_host/indexed_db_client_state_checker_factory.h"
#include "content/browser/renderer_host/media/media_stream_track_metrics_host.h"
#include "content/browser/renderer_host/p2p/socket_dispatcher_host.h"
#include "content/browser/renderer_host/recently_destroyed_hosts.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_message_filter.h"
#include "content/browser/renderer_host/render_widget_helper.h"
#include "content/browser/renderer_host/renderer_sandboxed_process_launcher_delegate.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/site_info.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/theme_helper.h"
#include "content/browser/tracing/background_tracing_manager_impl.h"
#include "content/browser/web_database/web_database_host_impl.h"
#include "content/browser/websockets/websocket_connector_impl.h"
#include "content/browser/webui/web_ui_controller_factory_registry.h"
#include "content/common/child_process.mojom.h"
#include "content/common/content_constants_internal.h"
#include "content/common/content_switches_internal.h"
#include "content/common/in_process_child_thread_params.h"
#include "content/common/pseudonymization_salt.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_or_resource_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/device_service.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host_creation_observer.h"
#include "content/public/browser/render_process_host_factory.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/render_process_host_priority_client.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/resource_coordinator_service.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/browser/webrtc_log.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/process_type.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/zygote/zygote_buildflags.h"
#include "google_apis/gaia/gaia_config.h"
#include "google_apis/gaia/gaia_switches.h"
#include "gpu/command_buffer/client/gpu_switches.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/config/gpu_switches.h"
#include "ipc/ipc_channel_mojo.h"
#include "ipc/trace_ipc_message.h"
#include "media/base/media_switches.h"
#include "media/capture/capture_switches.h"
#include "media/media_buildflags.h"
#include "media/mojo/services/video_decode_perf_history.h"
#include "media/mojo/services/webrtc_video_perf_history.h"
#include "media/webrtc/webrtc_features.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/scoped_message_error_crash_key.h"
#include "net/cookies/cookie_setting_override.h"
#include "sandbox/policy/switches.h"
#include "services/device/public/mojom/power_monitor.mojom.h"
#include "services/device/public/mojom/screen_orientation.mojom.h"
#include "services/device/public/mojom/time_zone_monitor.mojom.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"
#include "services/metrics/ukm_recorder_factory_impl.h"
#include "services/network/public/mojom/attribution.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/memory_instrumentation.mojom.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "storage/browser/database/database_tracker.h"
#include "storage/browser/quota/quota_manager.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/page/launching_process_state.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/public/mojom/disk_allocator.mojom.h"
#include "third_party/blink/public/mojom/origin_trials/origin_trials_settings.mojom.h"
#include "third_party/blink/public/mojom/plugins/plugin_registry.mojom.h"
#include "third_party/blink/public/public_buildflags.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"
#include "ui/display/display_switches.h"
#include "ui/gfx/switches.h"
#include "ui/gl/gl_switches.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/child_process_binding_types.h"
#include "content/browser/android/java_interfaces_impl.h"
#include "content/browser/font_unique_name_lookup/font_unique_name_lookup_service.h"
#include "content/public/browser/android/java_interfaces.h"
#include "media/audio/android/audio_manager_android.h"
#include "third_party/blink/public/mojom/android_font_lookup/android_font_lookup.mojom.h"
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include <sys/resource.h>

#include "components/services/font/public/mojom/font_service.mojom.h"  // nogncheck
#include "content/browser/font_service.h"  // nogncheck
#include "third_party/blink/public/mojom/memory_usage_monitor_linux.mojom.h"  // nogncheck

#include "content/browser/media/video_encode_accelerator_provider_launcher.h"
#include "media/mojo/mojom/video_encode_accelerator.mojom.h"
#endif

#if BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
#include "content/public/browser/stable_video_decoder_factory.h"
#endif

#if BUILDFLAG(IS_APPLE)
#include "content/browser/child_process_task_port_provider_mac.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "content/browser/sandbox_support_mac_impl.h"
#include "content/common/sandbox_support_mac.mojom.h"
#endif

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID)
#include "services/tracing/public/cpp/system_tracing_service.h"
#endif

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)
#include "content/browser/v8_snapshot_files.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/win/scoped_com_initializer.h"
#include "base/win/windows_version.h"
#include "content/browser/renderer_host/dwrite_font_proxy_impl_win.h"
#include "content/public/common/font_cache_dispatcher_win.h"
#include "content/public/common/font_cache_win.mojom.h"
#include "ui/display/win/dpi.h"
#endif

#if BUILDFLAG(ENABLE_LIBRARY_CDMS) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
#include "content/browser/media/key_system_support_impl.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/browser/renderer_host/plugin_registry_impl.h"
#endif

#if BUILDFLAG(ENABLE_PPAPI)
#include "content/browser/plugin_service_impl.h"
#include "content/browser/renderer_host/pepper/pepper_renderer_connection.h"
#include "ppapi/shared_impl/ppapi_switches.h"  // nogncheck
#endif

#if BUILDFLAG(IPC_MESSAGE_LOG_ENABLED)
#include "ipc/ipc_logging.h"
#endif

#if BUILDFLAG(USE_MINIKIN_HYPHENATION)
#include "content/browser/hyphenation/hyphenation_impl.h"
#endif

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_switches.h"
#endif

#if BUILDFLAG(CLANG_PROFILING_INSIDE_SANDBOX)
#include "content/public/common/profiling_utils.h"
#endif

// VLOG additional statements in Fuchsia release builds.
#if BUILDFLAG(IS_FUCHSIA)
#define MAYBEVLOG VLOG
#else
#define MAYBEVLOG DVLOG
#endif

namespace content {

namespace {

using perfetto::protos::pbzero::ChromeTrackEvent;

// Stores the maximum number of renderer processes the content module can
// create. Only applies if it is set to a non-zero value.
size_t g_max_renderer_count_override = 0;

bool g_run_renderer_in_process = false;

RendererMainThreadFactoryFunction g_renderer_main_thread_factory = nullptr;

base::Thread* g_in_process_thread = nullptr;

RenderProcessHostFactory* g_render_process_host_factory_ = nullptr;
const char kSiteProcessMapKeyName[] = "content_site_process_map";

RenderProcessHost::AnalyzeHungRendererFunction g_analyze_hung_renderer =
    nullptr;

// the global list of all renderer processes
base::IDMap<RenderProcessHost*>& GetAllHosts() {
  static base::NoDestructor<base::IDMap<RenderProcessHost*>> s_all_hosts;
  return *s_all_hosts;
}

// Returns the global list of RenderProcessHostCreationObserver objects. Uses
// std::list to ensure iterators remain valid if observers are created or
// removed during iteration.
std::list<RenderProcessHostCreationObserver*>& GetAllCreationObservers() {
  static base::NoDestructor<std::list<RenderProcessHostCreationObserver*>>
      s_all_creation_observers;
  return *s_all_creation_observers;
}

// Returns |host|'s PID if the process is valid and "no-process" otherwise.
std::string GetRendererPidAsString(RenderProcessHost* host) {
  if (host->GetProcess().IsValid()) {
    return base::NumberToString(host->GetProcess().Pid());
  }

  return "no-process";
}

std::ostream& operator<<(std::ostream& o,
                         const SiteInstanceProcessAssignment& assignment) {
  switch (assignment) {
    case SiteInstanceProcessAssignment::UNKNOWN:
      return o << "No renderer process has been assigned to the SiteInstance "
                  "yet.";
    case SiteInstanceProcessAssignment::REUSED_EXISTING_PROCESS:
      return o << "Reused some pre-existing process.";
    case SiteInstanceProcessAssignment::USED_SPARE_PROCESS:
      return o << "Used an existing spare process.";
    case SiteInstanceProcessAssignment::CREATED_NEW_PROCESS:
      return o << "No renderer could be reused, so a new one was created for "
                  "the SiteInstance.";
  }
}

// Map of site to process, to ensure we only have one RenderProcessHost per
// site in process-per-site mode.  Each map is specific to a BrowserContext.
class SiteProcessMap : public base::SupportsUserData::Data {
 public:
  typedef std::map<SiteInfo, RenderProcessHost*> SiteToProcessMap;
  SiteProcessMap() = default;

  void RegisterProcess(const SiteInfo& site_info, RenderProcessHost* process) {
    // There could already exist a site to process mapping due to races between
    // two WebContents with blank SiteInstances. If that occurs, keeping the
    // existing entry and not overwriting it is a predictable behavior that is
    // safe.
    auto i = map_.find(site_info);
    if (i == map_.end())
      map_[site_info] = process;
  }

  RenderProcessHost* FindProcess(const SiteInfo& site_info) {
    auto i = map_.find(site_info);
    if (i != map_.end())
      return i->second;
    return nullptr;
  }

  void RemoveProcess(RenderProcessHost* host) {
    // Find all instances of this process in the map, then separately remove
    // them.
    std::set<SiteInfo> site_info_set;
    for (SiteToProcessMap::const_iterator i = map_.begin(); i != map_.end();
         ++i) {
      if (i->second == host)
        site_info_set.insert(i->first);
    }
    for (const auto& i : site_info_set) {
      auto iter = map_.find(i);
      if (iter != map_.end()) {
        DCHECK_EQ(iter->second, host);
        map_.erase(iter);
      }
    }
  }

 private:
  SiteToProcessMap map_;
};

// Find the SiteProcessMap specific to the given context.
SiteProcessMap* GetSiteProcessMapForBrowserContext(BrowserContext* context) {
  DCHECK(context);
  SiteProcessMap* existing_map = static_cast<SiteProcessMap*>(
      context->GetUserData(kSiteProcessMapKeyName));
  if (existing_map)
    return existing_map;

  auto new_map = std::make_unique<SiteProcessMap>();
  auto* new_map_ptr = new_map.get();
  context->SetUserData(kSiteProcessMapKeyName, std::move(new_map));
  return new_map_ptr;
}

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
class SpareRenderProcessHostManager : public RenderProcessHostObserver {
 public:
  SpareRenderProcessHostManager() = default;

  SpareRenderProcessHostManager(const SpareRenderProcessHostManager& other) =
      delete;
  SpareRenderProcessHostManager& operator=(
      const SpareRenderProcessHostManager& other) = delete;

  static SpareRenderProcessHostManager& GetInstance() {
    static base::NoDestructor<SpareRenderProcessHostManager> s_instance;
    return *s_instance;
  }

  void WarmupSpareRenderProcessHost(BrowserContext* browser_context) {
    if (spare_render_process_host_ &&
        spare_render_process_host_->GetBrowserContext() == browser_context) {
      DCHECK_EQ(browser_context->GetDefaultStoragePartition(),
                spare_render_process_host_->GetStoragePartition());
      return;  // Nothing to warm up.
    }

    CleanupSpareRenderProcessHost();

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

    // Don't create a spare renderer if we're using --single-process or if we've
    // got too many processes. See also ShouldTryToUseExistingProcessHost in
    // this file.
    if (RenderProcessHost::run_renderer_in_process() ||
        RenderProcessHostImpl::GetProcessCountForLimit() >=
            RenderProcessHostImpl::GetMaxRendererProcessCount())
      return;

    // Don't create a spare renderer when the system is under load.  This is
    // currently approximated by only looking at the memory pressure.  See also
    // https://crbug.com/852905.
    auto* memory_monitor = base::MemoryPressureMonitor::Get();
    if (memory_monitor &&
        memory_monitor->GetCurrentPressureLevel() >=
            base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE)
      return;

    spare_render_process_host_ = RenderProcessHostImpl::CreateRenderProcessHost(
        browser_context, nullptr /* site_instance */);
    spare_render_process_host_->AddObserver(this);
    spare_render_process_host_->Init();

    // The spare render process isn't ready, so wait and do the "spare render
    // process changed" callback in RenderProcessReady().
  }

  RenderProcessHost* MaybeTakeSpareRenderProcessHost(
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
    bool embedder_allows_spare_usage =
        GetContentClient()->browser()->ShouldUseSpareRenderProcessHost(
            browser_context, site_instance->GetSiteInfo().site_url());

    // The spare RenderProcessHost always launches with JIT enabled, so if JIT
    // is disabled for the site then it's not possible to use this as the JIT
    // policy will differ.
    if (GetContentClient()->browser()->IsJitDisabledForSite(
            browser_context, site_instance->GetSiteInfo().process_lock_url()))
      embedder_allows_spare_usage = false;

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

    // Log UMA metrics.
    using SpareProcessMaybeTakeAction =
        RenderProcessHostImpl::SpareProcessMaybeTakeAction;
    SpareProcessMaybeTakeAction action =
        SpareProcessMaybeTakeAction::kNoSparePresent;
    if (!spare_render_process_host_)
      action = SpareProcessMaybeTakeAction::kNoSparePresent;
    else if (browser_context != spare_render_process_host_->GetBrowserContext())
      action = SpareProcessMaybeTakeAction::kMismatchedBrowserContext;
    else if (!spare_render_process_host_->InSameStoragePartition(site_storage))
      action = SpareProcessMaybeTakeAction::kMismatchedStoragePartition;
    else if (!embedder_allows_spare_usage)
      action = SpareProcessMaybeTakeAction::kRefusedByEmbedder;
    else if (!site_instance_allows_spare_usage)
      action = SpareProcessMaybeTakeAction::kRefusedBySiteInstance;
    else if (hosts_pdf_content)
      action = SpareProcessMaybeTakeAction::kRefusedForPdfContent;
    else
      action = SpareProcessMaybeTakeAction::kSpareTaken;
    UMA_HISTOGRAM_ENUMERATION(
        "BrowserRenderProcessHost.SpareProcessMaybeTakeAction", action);

    // Decide whether to take or drop the spare process.
    RenderProcessHost* returned_process = nullptr;
    if (spare_render_process_host_ &&
        browser_context == spare_render_process_host_->GetBrowserContext() &&
        spare_render_process_host_->InSameStoragePartition(site_storage) &&
        !site_instance->IsGuest() && embedder_allows_spare_usage &&
        site_instance_allows_spare_usage && !hosts_pdf_content) {
      CHECK(spare_render_process_host_->HostHasNotBeenUsed());

      // If the spare process ends up getting killed, the spare manager should
      // discard the spare RPH, so if one exists, it should always be live here.
      CHECK(spare_render_process_host_->IsInitializedAndNotDead());

      DCHECK_EQ(SpareProcessMaybeTakeAction::kSpareTaken, action);
      returned_process = spare_render_process_host_;
      ReleaseSpareRenderProcessHost(spare_render_process_host_);
    } else if (!RenderProcessHostImpl::IsSpareProcessKeptAtAllTimes()) {
      // If the spare shouldn't be kept around, then discard it as soon as we
      // find that the current spare was mismatched.
      CleanupSpareRenderProcessHost();
    } else if (RenderProcessHostImpl::GetProcessCountForLimit() >=
               RenderProcessHostImpl::GetMaxRendererProcessCount()) {
      // Drop the spare if we are at a process limit and the spare wasn't taken.
      // This helps avoid process reuse.
      CleanupSpareRenderProcessHost();
    }

    return returned_process;
  }

  // Prepares for future requests (with an assumption that a future navigation
  // might require a new process for |browser_context|).
  //
  // Note that depending on the caller PrepareForFutureRequests can be called
  // after the spare_render_process_host_ has either been 1) matched and taken
  // or 2) mismatched and ignored or 3) matched and ignored.
  void PrepareForFutureRequests(BrowserContext* browser_context) {
    if (RenderProcessHostImpl::IsSpareProcessKeptAtAllTimes()) {
      // Always keep around a spare process for the most recently requested
      // |browser_context|.
      WarmupSpareRenderProcessHost(browser_context);
    } else {
      // Discard the ignored (probably non-matching) spare so as not to waste
      // resources.
      CleanupSpareRenderProcessHost();
    }
  }

  // Gracefully remove and cleanup a spare RenderProcessHost if it exists.
  void CleanupSpareRenderProcessHost() {
    if (spare_render_process_host_) {
      // Stop observing the process, to avoid getting notifications as a
      // consequence of the Cleanup call below - such notification could call
      // back into CleanupSpareRenderProcessHost leading to stack overflow.
      spare_render_process_host_->RemoveObserver(this);

      // Make sure the RenderProcessHost object gets destroyed.
      if (!spare_render_process_host_->AreRefCountsDisabled())
        spare_render_process_host_->Cleanup();

      // Drop reference to the RenderProcessHost object.
      spare_render_process_host_ = nullptr;
      spare_render_process_host_changed_callback_list_.Notify(nullptr);
    }
  }

  RenderProcessHost* spare_render_process_host() {
    return spare_render_process_host_;
  }

  base::CallbackListSubscription RegisterSpareRenderProcessHostChangedCallback(
      const base::RepeatingCallback<void(RenderProcessHost*)>& cb) {
    // Do an initial notification, as the subscriber will need to know what the
    // current spare host is.
    cb.Run(spare_render_process_host_.get());
    return spare_render_process_host_changed_callback_list_.Add(cb);
  }

 private:
  // Release ownership of |host| as a possible spare renderer.  Called when
  // |host| has either been 1) claimed to be used in a navigation or 2) shutdown
  // somewhere else.
  void ReleaseSpareRenderProcessHost(RenderProcessHost* host) {
    if (spare_render_process_host_ && spare_render_process_host_ == host) {
      spare_render_process_host_->RemoveObserver(this);
      spare_render_process_host_ = nullptr;
      spare_render_process_host_changed_callback_list_.Notify(nullptr);
    }
  }

  void RenderProcessReady(RenderProcessHost* host) override {
    if (host == spare_render_process_host_) {
      spare_render_process_host_changed_callback_list_.Notify(
          spare_render_process_host_);
    }
  }

  void RenderProcessExited(RenderProcessHost* host,
                           const ChildProcessTerminationInfo& info) override {
    if (host == spare_render_process_host_)
      CleanupSpareRenderProcessHost();
  }

  void RenderProcessHostDestroyed(RenderProcessHost* host) override {
    ReleaseSpareRenderProcessHost(host);
  }

  // The clients who want to know when the spare render process host has
  // changed.
  base::RepeatingCallbackList<void(RenderProcessHost*)>
      spare_render_process_host_changed_callback_list_;

  // This is a bare pointer, because RenderProcessHost manages the lifetime of
  // all its instances; see GetAllHosts().
  raw_ptr<RenderProcessHost> spare_render_process_host_ = nullptr;
};

class RenderProcessHostIsReadyObserver : public RenderProcessHostObserver {
 public:
  RenderProcessHostIsReadyObserver(RenderProcessHost* render_process_host,
                                   base::OnceClosure task)
      : render_process_host_(render_process_host), task_(std::move(task)) {
    render_process_host_->AddObserver(this);
    if (render_process_host_->IsReady())
      PostTask();
  }

  ~RenderProcessHostIsReadyObserver() override {
    render_process_host_->RemoveObserver(this);
  }

  RenderProcessHostIsReadyObserver(
      const RenderProcessHostIsReadyObserver& other) = delete;
  RenderProcessHostIsReadyObserver& operator=(
      const RenderProcessHostIsReadyObserver& other) = delete;

  void RenderProcessReady(RenderProcessHost* host) override { PostTask(); }

  void RenderProcessHostDestroyed(RenderProcessHost* host) override {
    delete this;
  }

 private:
  void PostTask() {
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&RenderProcessHostIsReadyObserver::CallTask,
                                  weak_factory_.GetWeakPtr()));
  }

  void CallTask() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (render_process_host_->IsReady())
      std::move(task_).Run();

    delete this;
  }

  raw_ptr<RenderProcessHost> render_process_host_;
  base::OnceClosure task_;
  base::WeakPtrFactory<RenderProcessHostIsReadyObserver> weak_factory_{this};
};

bool MayReuseAndIsSuitableWithMainFrameThreshold(
    RenderProcessHost* host,
    SiteInstanceImpl* site_instance,
    absl::optional<size_t> main_frame_threshold) {
  // It's possible that |host| has become unsuitable for hosting
  // |site_instance|, for example if it was reused by a navigation to a
  // different site, and |site_instance| requires a dedicated process. Do not
  // allow such hosts to be reused.  See https://crbug.com/780661.
  if (!RenderProcessHostImpl::MayReuseAndIsSuitable(host, site_instance)) {
    return false;
  }

  if (!main_frame_threshold) {
    return true;
  }

  size_t main_frame_count = 0;
  bool devtools_attached = false;
  host->ForEachRenderFrameHost(base::BindRepeating(
      [](size_t& main_frame_count, bool& devtools_attached,
         RenderFrameHost* render_frame_host) {
        if (static_cast<RenderFrameHostImpl*>(render_frame_host)
                ->IsOutermostMainFrame()) {
          ++main_frame_count;
        }

        if (DevToolsAgentHost::GetForId(
                render_frame_host->GetDevToolsFrameToken().ToString())) {
          devtools_attached = true;
        }
      },
      std::ref(main_frame_count), std::ref(devtools_attached)));

  // If a threshold is specified, don't reuse `host` if it already hosts more
  // main frames (including BFCached and prerendered) than the threshold.
  if (main_frame_count >= *main_frame_threshold) {
    return false;
  }

  // Don't reuse `host` if DevTools is attached to any frame in that process
  // since DevTools doesn't work well when a renderer has multiple main frames.
  // TODO(https://crbug.com/1449114): This is just a heuristic and won't work if
  // DevTools is attached later, and hence this should be eventually removed and
  // fixed properly in the renderer process.
  if (devtools_attached) {
    return false;
  }

  return true;
}

// The following class is used to track the sites each RenderProcessHost is
// hosting frames for and expecting navigations to. There are two of them per
// BrowserContext: one for frames and one for navigations.
//
// For each site, the SiteProcessCountTracker keeps a map of counts per
// RenderProcessHost, which represents the number of frames/navigations
// for this site that are associated with the RenderProcessHost. This allows to
// quickly lookup a list of RenderProcessHost that can be used by a particular
// SiteInstance. On the other hand, it does not allow to quickly lookup which
// sites are hosted by a RenderProcessHost. This class is meant to help reusing
// RenderProcessHosts among SiteInstances, not to perform security checks for a
// RenderProcessHost.
const void* const kCommittedSiteProcessCountTrackerKey =
    "CommittedSiteProcessCountTrackerKey";
const void* const kPendingSiteProcessCountTrackerKey =
    "PendingSiteProcessCountTrackerKey";
const void* const kDelayedShutdownSiteProcessCountTrackerKey =
    "DelayedShutdownSiteProcessCountTrackerKey";
class SiteProcessCountTracker : public base::SupportsUserData::Data,
                                public RenderProcessHostObserver {
 public:
  SiteProcessCountTracker() = default;
  ~SiteProcessCountTracker() override { DCHECK(map_.empty()); }
  static SiteProcessCountTracker* GetInstance(BrowserContext* browser_context,
                                              const void* const tracker_key) {
    SiteProcessCountTracker* tracker = static_cast<SiteProcessCountTracker*>(
        browser_context->GetUserData(tracker_key));
    if (!tracker) {
      tracker = new SiteProcessCountTracker();
      browser_context->SetUserData(tracker_key, base::WrapUnique(tracker));
    }
    return tracker;
  }

  void IncrementSiteProcessCount(const SiteInfo& site_info,
                                 int render_process_host_id) {
    std::map<ProcessID, Count>& counts_per_process = map_[site_info];
    ++counts_per_process[render_process_host_id];

#ifndef NDEBUG
    // In debug builds, observe the RenderProcessHost destruction, to check
    // that it is properly removed from the map.
    RenderProcessHost* host = RenderProcessHost::FromID(render_process_host_id);
    if (!HasProcess(host))
      host->AddObserver(this);
#endif
  }

  void DecrementSiteProcessCount(const SiteInfo& site_info,
                                 int render_process_host_id) {
    auto result = map_.find(site_info);
    DCHECK(result != map_.end());
    std::map<ProcessID, Count>& counts_per_process = result->second;

    --counts_per_process[render_process_host_id];
    DCHECK_GE(counts_per_process[render_process_host_id], 0);

    if (counts_per_process[render_process_host_id] == 0)
      counts_per_process.erase(render_process_host_id);

    if (counts_per_process.empty())
      map_.erase(site_info);
  }

  void FindRenderProcessesForSiteInstance(
      SiteInstanceImpl* site_instance,
      absl::optional<size_t> main_frame_threshold,
      std::set<RenderProcessHost*>* foreground_processes,
      std::set<RenderProcessHost*>* background_processes) {
    auto result = map_.find(site_instance->GetSiteInfo());
    if (result == map_.end())
      return;

    std::map<ProcessID, Count>& counts_per_process = result->second;
    for (auto iter : counts_per_process) {
      RenderProcessHost* host = RenderProcessHost::FromID(iter.first);
      if (!host) {
        // TODO(clamy): This shouldn't happen but we are getting reports from
        // the field that this is happening. We need to figure out why some
        // RenderProcessHosts are not taken out of the map when they're
        // destroyed.
        NOTREACHED();
        continue;
      }

      if (!MayReuseAndIsSuitableWithMainFrameThreshold(host, site_instance,
                                                       main_frame_threshold)) {
        continue;
      }

      if (host->VisibleClientCount())
        foreground_processes->insert(host);
      else
        background_processes->insert(host);
    }
  }

  // Check whether |host| is associated with at least one URL for which
  // SiteInstance does not assign site URLs.  This is used to disqualify |host|
  // from being reused if it has pending navigations to such URLs.
  bool ContainsNonReusableSiteForHost(RenderProcessHost* host) {
    for (auto iter : map_) {
      // If SiteInstance doesn't assign a site URL for the current entry, check
      // whether |host| is on the list of processes the entry is associated
      // with.  Skip entries for about:blank, which is allowed anywhere.  Note
      // that about:blank could have an initiator origin, and a process with
      // such a pending navigation wouldn't be safe to reuse, but in that case
      // the site URL would reflect the initiator origin and wouldn't match
      // about:blank.
      //
      // TODO(alexmos): ShouldAssignSiteForURL() expects a full URL, whereas we
      // only have a site URL here.  For now, this mismatch is ok since
      // ShouldAssignSiteForURL() only cares about schemes in practice, but
      // this should be cleaned up.
      //
      // TODO(alexmos): Additionally, site URLs will never match the full
      // "about:blank" URL which has no host; a site URL could only be
      // "about:" in that case.  This looks like a bug that needs to be fixed!
      if (!SiteInstance::ShouldAssignSiteForURL(iter.first.site_url()) &&
          !iter.first.site_url().IsAboutBlank() &&
          base::Contains(iter.second, host->GetID())) {
        return true;
      }
    }
    return false;
  }

  // Removes |render_process_host_id| from all sites in |map_|.
  void ClearProcessForAllSites(int render_process_host_id) {
    for (auto iter = map_.begin(); iter != map_.end();) {
      std::map<ProcessID, Count>& counts_per_process = iter->second;
      counts_per_process.erase(render_process_host_id);
      // If the site is mapped to no more processes, remove it.
      iter = counts_per_process.empty() ? map_.erase(iter) : ++iter;
    }
  }

  // Returns a string containing formatted data from
  // GetHostIdToSiteMapForDebugging().
  std::string GetDebugString() const {
    HostIdToSiteMap rph_to_sites_map = GetHostIdToSiteMapForDebugging();
    std::string output;

    for (auto host_info : rph_to_sites_map) {
      RenderProcessHost* host = GetAllHosts().Lookup(host_info.first);
      DCHECK(host);

      bool is_locked_to_site = host->GetProcessLock().is_locked_to_site();
      output += base::StringPrintf("\tProcess Host ID %d (PID %s, %s):\n",
                                   host_info.first,
                                   GetRendererPidAsString(host).c_str(),
                                   is_locked_to_site ? "locked" : "not locked");

      for (auto site_string : host_info.second) {
        output += base::StringPrintf("\t\t%s\n", site_string.c_str());
      }
    }

    return output;
  }

  // Returns true if |site_info| is present in |map_| and has
  // |render_process_host_id| in its map of processes that it is hosted by.
  bool Contains(const SiteInfo& site_info, int render_process_host_id) {
    auto site_info_found = map_.find(site_info);
    if (site_info_found == map_.end())
      return false;
    auto counts_per_process = site_info_found->second;
    return counts_per_process.find(render_process_host_id) !=
           counts_per_process.end();
  }

  // Returns true if |render_process_host_id| is present for any site in |map_|.
  bool ContainsHost(int render_process_host_id) {
    for (auto iter : map_) {
      auto counts_per_process = iter.second;
      if (counts_per_process.find(render_process_host_id) !=
          counts_per_process.end()) {
        return true;
      }
    }
    return false;
  }

 private:
  using ProcessID = int;
  using Count = int;
  using HostIdToSiteMap = base::flat_map<ProcessID, std::vector<std::string>>;

  // Creates a new mapping of the ProcessID to sites and their count based on
  // the current map_.
  HostIdToSiteMap GetHostIdToSiteMapForDebugging() const {
    HostIdToSiteMap rph_to_sites_map;

    // There may be process hosts without sites. To ensure all process hosts are
    // represented, start by adding entries for all hosts.
    rph_to_sites_map.reserve(RenderProcessHostImpl::GetProcessCount());
    for (auto iter(RenderProcessHost::AllHostsIterator()); !iter.IsAtEnd();
         iter.Advance()) {
      rph_to_sites_map[iter.GetCurrentValue()->GetID()];
    }

    for (auto iter : map_) {
      std::string site = iter.first.GetDebugString();
      std::map<ProcessID, Count>& counts_per_process = iter.second;
      for (auto iter_process : counts_per_process) {
        ProcessID id = iter_process.first;
        Count count = iter_process.second;

        rph_to_sites_map[id].push_back(
            base::StringPrintf("%s -- count: %d", site.c_str(), count));
      }
    }

    return rph_to_sites_map;
  }

  void RenderProcessHostDestroyed(RenderProcessHost* host) override {
#ifndef NDEBUG
    host->RemoveObserver(this);
    DCHECK(!HasProcess(host));
#endif
  }

#ifndef NDEBUG
  // Used in debug builds to ensure that RenderProcessHosts don't persist in the
  // map after they've been destroyed.
  bool HasProcess(RenderProcessHost* process) {
    for (auto iter : map_) {
      std::map<ProcessID, Count>& counts_per_process = iter.second;
      for (auto iter_process : counts_per_process) {
        if (iter_process.first == process->GetID())
          return true;
      }
    }
    return false;
  }
#endif

  using CountPerProcessPerSiteMap =
      std::map<SiteInfo, std::map<ProcessID, Count>>;
  CountPerProcessPerSiteMap map_;
};

bool ShouldTrackProcessForSite(const SiteInfo& site_info) {
  return !site_info.site_url().is_empty();
}

bool ShouldFindReusableProcessHostForSite(const SiteInfo& site_info) {
  return !site_info.site_url().is_empty();
}

std::string GetCurrentHostMapDebugString(
    const SiteProcessCountTracker* tracker) {
  std::string output =
      base::StringPrintf("There are now %zu RenderProcessHosts.",
                         RenderProcessHostImpl::GetProcessCount());
  if (tracker) {
    output += base::StringPrintf("\nThe mappings are:\n%s",
                                 tracker->GetDebugString().c_str());
  }

  return output;
}

const void* const kUnmatchedServiceWorkerProcessTrackerKey =
    "UnmatchedServiceWorkerProcessTrackerKey";

// This class tracks 'unmatched' service worker processes. When a service worker
// is started after a navigation to the site, SiteProcessCountTracker that is
// implemented above is used to find the matching renderer process which is used
// for the navigation. But a service worker may be started before a navigation
// (ex: Push notification -> show the page of the notification).
// This class tracks processes with 'unmatched' service workers until the
// processes are reused for a navigation to a matching site. After a single
// matching navigation is put into the process, all service workers for that
// site in that process are considered 'matched.'
class UnmatchedServiceWorkerProcessTracker
    : public base::SupportsUserData::Data,
      public RenderProcessHostObserver {
 public:
  // Registers |render_process_host| as having an unmatched service worker for
  // |site_instance|.
  static void Register(RenderProcessHost* render_process_host,
                       SiteInstanceImpl* site_instance) {
    BrowserContext* browser_context = site_instance->GetBrowserContext();
    DCHECK(!site_instance->GetSiteInfo().site_url().is_empty());
    if (!ShouldTrackProcessForSite(site_instance->GetSiteInfo()))
      return;

    UnmatchedServiceWorkerProcessTracker* tracker =
        static_cast<UnmatchedServiceWorkerProcessTracker*>(
            browser_context->GetUserData(
                kUnmatchedServiceWorkerProcessTrackerKey));
    if (!tracker) {
      tracker = new UnmatchedServiceWorkerProcessTracker();
      browser_context->SetUserData(kUnmatchedServiceWorkerProcessTrackerKey,
                                   base::WrapUnique(tracker));
    }
    tracker->RegisterProcessForSite(render_process_host, site_instance);
  }

  // Find a process with an unmatched service worker for |site_instance| and
  // removes the process from the tracker if it exists.
  static RenderProcessHost* MatchWithSite(SiteInstanceImpl* site_instance) {
    BrowserContext* browser_context = site_instance->GetBrowserContext();
    if (!ShouldFindReusableProcessHostForSite(site_instance->GetSiteInfo()))
      return nullptr;

    UnmatchedServiceWorkerProcessTracker* tracker =
        static_cast<UnmatchedServiceWorkerProcessTracker*>(
            browser_context->GetUserData(
                kUnmatchedServiceWorkerProcessTrackerKey));
    if (!tracker)
      return nullptr;
    return tracker->TakeFreshestProcessForSite(site_instance);
  }

  UnmatchedServiceWorkerProcessTracker() = default;

  ~UnmatchedServiceWorkerProcessTracker() override {
    DCHECK(site_process_set_.empty());
  }

  // Implementation of RenderProcessHostObserver.
  void RenderProcessHostDestroyed(RenderProcessHost* host) override {
    DCHECK(HasProcess(host));
    int process_id = host->GetID();
    for (auto it = site_process_set_.begin(); it != site_process_set_.end();) {
      if (it->second == process_id) {
        it = site_process_set_.erase(it);
      } else {
        ++it;
      }
    }
    host->RemoveObserver(this);
  }

 private:
  using ProcessID = int;
  using SiteProcessIDPair = std::pair<SiteInfo, ProcessID>;
  using SiteProcessIDPairSet = std::set<SiteProcessIDPair>;

  void RegisterProcessForSite(RenderProcessHost* host,
                              SiteInstanceImpl* site_instance) {
    if (!HasProcess(host))
      host->AddObserver(this);
    site_process_set_.insert(
        SiteProcessIDPair(site_instance->GetSiteInfo(), host->GetID()));
  }

  RenderProcessHost* TakeFreshestProcessForSite(
      SiteInstanceImpl* site_instance) {
    absl::optional<SiteProcessIDPair> site_process_pair =
        FindFreshestProcessForSite(site_instance);

    if (!site_process_pair)
      return nullptr;

    RenderProcessHost* host =
        RenderProcessHost::FromID(site_process_pair->second);

    if (!host)
      return nullptr;

    // It's possible that |host| is currently unsuitable for hosting
    // |site_instance|, for example if it was used for a ServiceWorker for a
    // nonexistent extension URL.  See https://crbug.com/782349 and
    // https://crbug.com/780661.
    if (!RenderProcessHostImpl::MayReuseAndIsSuitable(host, site_instance))
      return nullptr;

    site_process_set_.erase(site_process_pair.value());
    if (!HasProcess(host))
      host->RemoveObserver(this);
    return host;
  }

  absl::optional<SiteProcessIDPair> FindFreshestProcessForSite(
      SiteInstanceImpl* site_instance) const {
    const auto reversed_site_process_set = base::Reversed(site_process_set_);
    if (site_instance->IsDefaultSiteInstance()) {
      // See if we can find an entry that maps to a site associated with the
      // default SiteInstance. This allows the default SiteInstance to reuse a
      // service worker process for any site that has been associated with it.
      for (const auto& site_process_pair : reversed_site_process_set) {
        if (site_instance->IsSiteInDefaultSiteInstance(
                site_process_pair.first.site_url()))
          return site_process_pair;
      }
    } else {
      for (const auto& site_process_pair : reversed_site_process_set) {
        if (site_process_pair.first == site_instance->GetSiteInfo())
          return site_process_pair;
      }
    }
    return absl::nullopt;
  }

  // Returns true if this tracker contains the process ID |host->GetID()|.
  bool HasProcess(RenderProcessHost* host) const {
    int process_id = host->GetID();
    for (const auto& site_process_id : site_process_set_) {
      if (site_process_id.second == process_id)
        return true;
    }
    return false;
  }

  // Use std::set because duplicates don't need to be tracked separately (eg.,
  // service workers for the same site in the same process). It is sorted in the
  // order of insertion.
  SiteProcessIDPairSet site_process_set_;
};

void CopyFeatureSwitch(const base::CommandLine& src,
                       base::CommandLine* dest,
                       const char* switch_name) {
  std::vector<std::string> features = FeaturesFromSwitch(src, switch_name);
  if (!features.empty())
    dest->AppendSwitchASCII(switch_name, base::JoinString(features, ","));
}

RenderProcessHostImpl::DomStorageBinder& GetDomStorageBinder() {
  static base::NoDestructor<RenderProcessHostImpl::DomStorageBinder> binder;
  return *binder;
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
static constexpr size_t kUnknownPlatformProcessLimit = 0;

// Returns the process limit from the system. Use |kUnknownPlatformProcessLimit|
// to indicate failure and std::numeric_limits<size_t>::max() to indicate
// unlimited.
size_t GetPlatformProcessLimit() {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  struct rlimit limit;
  if (getrlimit(RLIMIT_NPROC, &limit) != 0)
    return kUnknownPlatformProcessLimit;

  if (limit.rlim_cur == RLIM_INFINITY)
    return std::numeric_limits<size_t>::max();
  return base::saturated_cast<size_t>(limit.rlim_cur);
#else
  // TODO(https://crbug.com/104689): Implement on other platforms.
  return kUnknownPlatformProcessLimit;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)

RenderProcessHost::BindHostReceiverInterceptor&
GetBindHostReceiverInterceptor() {
  static base::NoDestructor<RenderProcessHost::BindHostReceiverInterceptor>
      interceptor;
  return *interceptor;
}

RenderProcessHostImpl::CreateNetworkFactoryCallback&
GetCreateNetworkFactoryCallback() {
  static base::NoDestructor<RenderProcessHostImpl::CreateNetworkFactoryCallback>
      s_callback;
  return *s_callback;
}

RenderProcessHostImpl::BadMojoMessageCallbackForTesting&
GetBadMojoMessageCallbackForTesting() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  static base::NoDestructor<
      RenderProcessHostImpl::BadMojoMessageCallbackForTesting>
      s_callback;
  return *s_callback;
}

void InvokeBadMojoMessageCallbackForTesting(int render_process_id,
                                            const std::string& error) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&InvokeBadMojoMessageCallbackForTesting,
                                  render_process_id, error));
    return;
  }

  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderProcessHostImpl::BadMojoMessageCallbackForTesting& callback =
      GetBadMojoMessageCallbackForTesting();
  if (!callback.is_null())
    callback.Run(render_process_id, error);
}

void LogDelayReasonForFastShutdown(
    const RenderProcessHostImpl::DelayShutdownReason& reason) {
  UMA_HISTOGRAM_ENUMERATION(
      "BrowserRenderProcessHost.FastShutdownIfPossible.DelayReason", reason);
}

void LogDelayReasonForCleanup(
    const RenderProcessHostImpl::DelayShutdownReason& reason) {
  UMA_HISTOGRAM_ENUMERATION("BrowserRenderProcessHost.Cleanup.DelayReason",
                            reason);
}

#if BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
RenderProcessHostImpl::StableVideoDecoderFactoryCreationCB&
GetStableVideoDecoderFactoryCreationCB() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  static base::NoDestructor<
      RenderProcessHostImpl::StableVideoDecoderFactoryCreationCB>
      s_callback;
  return *s_callback;
}

RenderProcessHostImpl::StableVideoDecoderEventCB&
GetStableVideoDecoderEventCB() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  static base::NoDestructor<RenderProcessHostImpl::StableVideoDecoderEventCB>
      s_callback;
  return *s_callback;
}

void InvokeStableVideoDecoderEventCB(
    RenderProcessHostImpl::StableVideoDecoderEvent event) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderProcessHostImpl::StableVideoDecoderEventCB& callback =
      GetStableVideoDecoderEventCB();
  if (!callback.is_null()) {
    callback.Run(event);
  }
}
#endif  // BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)

// Kill-switch for the new CHECKs from https://crrev.com/c/4134809.
BASE_FEATURE(kCheckNoNewRefCountsWhenRphDeletingSoon,
             "CheckNoNewRefCountsWhenRphDeletingSoon",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Please keep in sync with "RenderProcessHostBlockedURLReason" in
// tools/metrics/histograms/enums.xml. // These values are persisted to logs.
// Entries should not be renumbered and numeric values should never be reused.
enum class BlockedURLReason {
  kInvalidURL = 0,
  kFailedCanRequestURLCheck = 1,

  kMaxValue = kFailedCanRequestURLCheck
};

}  // namespace

// A RenderProcessHostImpl's IO thread implementation of the
// |mojom::ChildProcessHost| interface. This exists to allow the process host
// to bind incoming receivers on the IO-thread without a main-thread hop if
// necessary. Also owns the RPHI's |mojom::ChildProcess| remote.
class RenderProcessHostImpl::IOThreadHostImpl : public mojom::ChildProcessHost {
 public:
  IOThreadHostImpl(int render_process_id,
                   base::WeakPtr<RenderProcessHostImpl> weak_host,
                   std::unique_ptr<service_manager::BinderRegistry> binders,
                   mojo::PendingReceiver<mojom::ChildProcessHost> host_receiver)
      : render_process_id_(render_process_id),
        weak_host_(std::move(weak_host)),
        binders_(std::move(binders)),
        receiver_(this, std::move(host_receiver)) {}
  ~IOThreadHostImpl() override = default;

  IOThreadHostImpl(const IOThreadHostImpl& other) = delete;
  IOThreadHostImpl& operator=(const IOThreadHostImpl& other) = delete;

 private:
  // mojom::ChildProcessHost implementation:
  void Ping(PingCallback callback) override { std::move(callback).Run(); }

  void BindHostReceiver(mojo::GenericPendingReceiver receiver) override {
    const auto& interceptor = GetBindHostReceiverInterceptor();
    if (interceptor) {
      interceptor.Run(render_process_id_, &receiver);
      if (!receiver)
        return;
    }

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    if (auto font_receiver = receiver.As<font_service::mojom::FontService>()) {
      ConnectToFontService(std::move(font_receiver));
      return;
    }

    if (base::FeatureList::IsEnabled(media::kUseOutOfProcessVideoEncoding)) {
      if (auto r =
              receiver.As<media::mojom::VideoEncodeAcceleratorProvider>()) {
        if (!video_encode_accelerator_factory_remote_.is_bound()) {
          LaunchVideoEncodeAcceleratorProviderFactory(
              video_encode_accelerator_factory_remote_
                  .BindNewPipeAndPassReceiver());
          video_encode_accelerator_factory_remote_.reset_on_disconnect();
        }

        if (!video_encode_accelerator_factory_remote_.is_bound())
          return;

        video_encode_accelerator_factory_remote_
            ->CreateVideoEncodeAcceleratorProvider(std::move(r));
        return;
      }
    }
#endif  // (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS))

#if BUILDFLAG(IS_WIN)
    if (auto r = receiver.As<mojom::FontCacheWin>()) {
      FontCacheDispatcher::Create(std::move(r));
      return;
    }
#endif

#if BUILDFLAG(IS_MAC)
    if (auto r = receiver.As<mojom::SandboxSupportMac>()) {
      static base::NoDestructor<SandboxSupportMacImpl> sandbox_support;
      sandbox_support->BindReceiver(std::move(r));
      return;
    }
#endif

    if (auto r = receiver.As<
                 discardable_memory::mojom::DiscardableSharedMemoryManager>()) {
      discardable_memory::DiscardableSharedMemoryManager::Get()->Bind(
          std::move(r));
      return;
    }

    if (auto r = receiver.As<ukm::mojom::UkmRecorderFactory>()) {
      metrics::UkmRecorderFactoryImpl::Create(ukm::UkmRecorder::Get(),
                                              std::move(r));
      return;
    }

#if BUILDFLAG(IS_ANDROID)
    // Bind the font lookup on the IO thread as an optimization to avoid
    // running navigation critical path tasks on the UI thread.
    if (auto r = receiver.As<blink::mojom::AndroidFontLookup>()) {
      GetGlobalJavaInterfacesOnIOThread()->GetInterface(std::move(r));
      return;
    }
#endif

    std::string interface_name = *receiver.interface_name();
    mojo::ScopedMessagePipeHandle pipe = receiver.PassPipe();
    if (binders_->TryBindInterface(interface_name, &pipe))
      return;

    receiver = mojo::GenericPendingReceiver(interface_name, std::move(pipe));
    if (!receiver)
      return;

    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&IOThreadHostImpl::BindHostReceiverOnUIThread,
                                  weak_host_, std::move(receiver)));
  }

  static void BindHostReceiverOnUIThread(
      base::WeakPtr<RenderProcessHostImpl> weak_host,
      mojo::GenericPendingReceiver receiver) {
    if (weak_host)
      weak_host->OnBindHostReceiver(std::move(receiver));
  }

  const int render_process_id_;
  const base::WeakPtr<RenderProcessHostImpl> weak_host_;
  std::unique_ptr<service_manager::BinderRegistry> binders_;
  mojo::Receiver<mojom::ChildProcessHost> receiver_{this};

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  mojo::Remote<media::mojom::VideoEncodeAcceleratorProviderFactory>
      video_encode_accelerator_factory_remote_;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
};

// static
scoped_refptr<base::SingleThreadTaskRunner>
RenderProcessHostImpl::GetInProcessRendererThreadTaskRunnerForTesting() {
  return g_in_process_thread->task_runner();
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
// static
size_t RenderProcessHostImpl::GetPlatformMaxRendererProcessCount() {
  // Set the limit to half of the system limit to leave room for other programs.
  size_t limit = GetPlatformProcessLimit() / 2;

  // If the system limit is unavailable, use a fallback value instead.
  if (limit == kUnknownPlatformProcessLimit) {
    static constexpr size_t kMaxRendererProcessCount = 82;
    limit = kMaxRendererProcessCount;
  }
  return limit;
}
#endif

// static
size_t RenderProcessHost::GetMaxRendererProcessCount() {
  if (g_max_renderer_count_override)
    return g_max_renderer_count_override;

  size_t client_override =
      GetContentClient()->browser()->GetMaxRendererProcessCountOverride();
  if (client_override)
    return client_override;

#if BUILDFLAG(IS_ANDROID)
  // On Android we don't maintain a limit of renderer process hosts - we are
  // happy with keeping a lot of these, as long as the number of live renderer
  // processes remains reasonable, and on Android the OS takes care of that.
  return std::numeric_limits<size_t>::max();
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  // On Chrome OS new renderer processes are very cheap and there's no OS
  // driven constraint on the number of processes, and the effectiveness
  // of the tab discarder is very poor when we have tabs sharing a
  // renderer process.  So, set a high limit, and based on UMA stats
  // for CrOS the 99.9th percentile of Tabs.MaxTabsInADay is around 100.
  return 100;
#else
  // On other platforms, calculate the maximum number of renderer process hosts
  // according to the amount of installed memory as reported by the OS, along
  // with some hard-coded limits. The calculation assumes that the renderers
  // will use up to half of the installed RAM and assumes that each WebContents
  // uses |kEstimatedWebContentsMemoryUsage| MB. If this assumption changes, the
  // ThirtyFourTabs test needs to be adjusted to match the expected number of
  // processes.
  //
  // Using the above assumptions, with the given amounts of installed memory
  // below on a 64-bit CPU, the maximum renderer count based on available RAM
  // alone will be as follows:
  //
  //   128 MB -> 0
  //   512 MB -> 3
  //  1024 MB -> 6
  //  4096 MB -> 24
  // 16384 MB -> 96
  //
  // Then the calculated value will be clamped by |kMinRendererProcessCount| and
  // GetPlatformMaxRendererProcessCount().

  static size_t max_count = 0;
  if (!max_count) {
    static constexpr size_t kEstimatedWebContentsMemoryUsage =
#if defined(ARCH_CPU_64_BITS)
        85;  // In MB
#else
        60;  // In MB
#endif
    max_count = base::SysInfo::AmountOfPhysicalMemoryMB() / 2;
    max_count /= kEstimatedWebContentsMemoryUsage;

    static constexpr size_t kMinRendererProcessCount = 3;
    static const size_t kMaxRendererProcessCount =
        RenderProcessHostImpl::GetPlatformMaxRendererProcessCount();
    DCHECK_LE(kMinRendererProcessCount, kMaxRendererProcessCount);

    max_count = std::clamp(max_count, kMinRendererProcessCount,
                           kMaxRendererProcessCount);
    MAYBEVLOG(1) << __func__ << ": Calculated max " << max_count;
  }
  return max_count;
#endif
}

// static
void RenderProcessHost::SetMaxRendererProcessCount(size_t count) {
  MAYBEVLOG(1) << __func__ << ": Max override set to " << count;
  g_max_renderer_count_override = count;

  if (RenderProcessHostImpl::GetProcessCount() > count) {
    SpareRenderProcessHostManager::GetInstance()
        .CleanupSpareRenderProcessHost();
  }
}

// static
int RenderProcessHost::GetCurrentRenderProcessCountForTesting() {
  RenderProcessHost::iterator it = RenderProcessHost::AllHostsIterator();
  int count = 0;
  while (!it.IsAtEnd()) {
    RenderProcessHost* host = it.GetCurrentValue();
    if (host->IsInitializedAndNotDead() &&
        host != RenderProcessHostImpl::GetSpareRenderProcessHostForTesting()) {
      count++;
    }
    it.Advance();
  }
  return count;
}

// static
RenderProcessHost* RenderProcessHostImpl::CreateRenderProcessHost(
    BrowserContext* browser_context,
    SiteInstanceImpl* site_instance) {
  if (g_render_process_host_factory_) {
    return g_render_process_host_factory_->CreateRenderProcessHost(
        browser_context, site_instance);
  }

  StoragePartitionImpl* storage_partition_impl =
      static_cast<StoragePartitionImpl*>(
          browser_context->GetStoragePartition(site_instance));

  int flags = RenderProcessFlags::kNone;

  if (site_instance && site_instance->IsGuest()) {
    flags |= RenderProcessFlags::kForGuestsOnly;

    // If we've made a StoragePartition for guests (e.g., for the <webview>
    // tag), make the StoragePartition aware of that.  This will be consulted
    // when we start a service worker inside this StoragePartition, so that we
    // can create the appropriate SiteInstance (e.g., we will try to start a
    // worker from "https://example.com/sw.js" but need to use the guest's
    // StoragePartitionConfig to get a process in the guest's
    // StoragePartition.)
    storage_partition_impl->set_is_guest();
  }

  if (site_instance) {
    if (site_instance->IsJitDisabled()) {
      flags |= RenderProcessFlags::kJitDisabled;
    }
    if (site_instance->IsPdf()) {
      flags |= RenderProcessFlags::kPdf;
    }
  }

  return new RenderProcessHostImpl(browser_context, storage_partition_impl,
                                   flags);
}

// static
const unsigned int RenderProcessHostImpl::kMaxFrameDepthForPriority =
    std::numeric_limits<unsigned int>::max();

// static
const base::TimeDelta RenderProcessHostImpl::kKeepAliveHandleFactoryTimeout =
    base::Milliseconds(kKeepAliveHandleFactoryTimeoutInMSec);

RenderProcessHostImpl::RenderProcessHostImpl(
    BrowserContext* browser_context,
    StoragePartitionImpl* storage_partition_impl,
    int flags)
    : priority_(!blink::kLaunchingProcessIsBackgrounded,
                false /* has_media_stream */,
                false /* has_foreground_service_worker */,
                frame_depth_,
                false /* intersects_viewport */,
                true /* boost_for_pending_views */
#if BUILDFLAG(IS_ANDROID)
                ,
                ChildProcessImportance::NORMAL
#endif
                ),
      id_(ChildProcessHostImpl::GenerateChildProcessUniqueId()),
      browser_context_(browser_context),
      storage_partition_impl_(storage_partition_impl->GetWeakPtr()),
      sudden_termination_allowed_(true),
      is_blocked_(false),
      flags_(flags),
      is_unused_(true),
      delayed_cleanup_needed_(false),
      within_process_died_observer_(false),
      channel_connected_(false),
      sent_render_process_ready_(false),
      shutdown_exit_code_(-1) {
  CHECK(!browser_context->ShutdownStarted());
  TRACE_EVENT("shutdown", "RenderProcessHostImpl",
              ChromeTrackEvent::kRenderProcessHost, *this);
  TRACE_EVENT_BEGIN("shutdown", "Browser.RenderProcessHostImpl",
                    perfetto::Track::FromPointer(this),
                    ChromeTrackEvent::kRenderProcessHost, *this);

#if BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
  stable_video_decoder_trackers_.set_disconnect_handler(base::BindRepeating(
      &RenderProcessHostImpl::OnStableVideoDecoderDisconnected,
      instance_weak_factory_.GetWeakPtr()));
#endif  // BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)

  widget_helper_ = new RenderWidgetHelper();

  ChildProcessSecurityPolicyImpl::GetInstance()->Add(GetID(), browser_context);

  CHECK(!BrowserMainRunner::ExitedMainMessageLoop());
  RegisterHost(GetID(), this);
  GetAllHosts().set_check_on_null_data(true);
  // Initialize |child_process_activity_time_| to a reasonable value.
  mark_child_process_activity_time();

  // This instance of PushMessagingManager is only used from clients
  // bound to service workers (i.e. PushProvider), since frame-bound
  // clients will rely on BrowserInterfaceBroker instead. Therefore,
  // pass an invalid frame ID here.
  push_messaging_manager_ = std::make_unique<PushMessagingManager>(
      *this,
      /* render_frame_id= */ ChildProcessHost::kInvalidUniqueID,
      base::WrapRefCounted(storage_partition_impl_->GetServiceWorkerContext()));

  InitializeChannelProxy();

  const int id = GetID();
  const uint64_t tracing_id =
      ChildProcessHostImpl::ChildProcessUniqueIdToTracingProcessId(id);
  gpu_client_.reset(
      new viz::GpuClient(std::make_unique<BrowserGpuClientDelegate>(), id,
                         tracing_id, GetUIThreadTaskRunner({})));
}

// static
void RenderProcessHostImpl::ShutDownInProcessRenderer() {
  DCHECK(g_run_renderer_in_process);

  switch (RenderProcessHostImpl::GetProcessCount()) {
    case 0:
      return;
    case 1: {
      RenderProcessHostImpl* host = static_cast<RenderProcessHostImpl*>(
          AllHostsIterator().GetCurrentValue());
      for (auto& observer : host->observers_)
        observer.RenderProcessHostDestroyed(host);
#ifndef NDEBUG
      host->is_self_deleted_ = true;
#endif
      delete host;
      return;
    }
    default:
      NOTREACHED() << "There should be only one RenderProcessHost when running "
                   << "in-process.";
  }
}

void RenderProcessHostImpl::RegisterRendererMainThreadFactory(
    RendererMainThreadFactoryFunction create) {
  g_renderer_main_thread_factory = create;
}

void RenderProcessHostImpl::SetDomStorageBinderForTesting(
    DomStorageBinder binder) {
  GetDomStorageBinder() = std::move(binder);
}

bool RenderProcessHostImpl::HasDomStorageBinderForTesting() {
  return !GetDomStorageBinder().is_null();
}

// static
void RenderProcessHostImpl::SetBadMojoMessageCallbackForTesting(
    BadMojoMessageCallbackForTesting callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // No support for setting the global callback twice.
  DCHECK_NE(callback.is_null(),
            GetBadMojoMessageCallbackForTesting().is_null());

  GetBadMojoMessageCallbackForTesting() = callback;
}

void RenderProcessHostImpl::SetForGuestsOnlyForTesting() {
  flags_ |= RenderProcessFlags::kForGuestsOnly;
}

RenderProcessHostImpl::~RenderProcessHostImpl() {
  TRACE_EVENT("shutdown", "~RenderProcessHostImpl",
              ChromeTrackEvent::kRenderProcessHost, *this);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
#ifndef NDEBUG
  DCHECK(is_self_deleted_)
      << "RenderProcessHostImpl is destroyed by something other than itself";
#endif

  // Make sure to clean up the in-process renderer before the channel, otherwise
  // it may still run and have its IPCs fail, causing asserts.
  in_process_renderer_.reset();
  g_in_process_thread = nullptr;

  ChildProcessSecurityPolicyImpl::GetInstance()->Remove(GetID());

  is_dead_ = true;

  UnregisterHost(GetID());

  // Remove the cache handles for the client at teardown if relevant.
  if (GetGpuDiskCacheFactorySingleton()) {
    gpu_client_->RemoveDiskCacheHandles();
  }

  // "Cleanup in progress"
  TRACE_EVENT_END("shutdown", perfetto::Track::FromPointer(this),
                  ChromeTrackEvent::kRenderProcessHost, *this);
  // "Browser.RenderProcessHostImpl"
  TRACE_EVENT_END("shutdown", perfetto::Track::FromPointer(this),
                  ChromeTrackEvent::kRenderProcessHost, *this);
}

bool RenderProcessHostImpl::Init() {
  // calling Init() more than once does nothing, this makes it more convenient
  // for the view host which may not be sure in some cases
  if (IsInitializedAndNotDead())
    return true;

  base::CommandLine::StringType renderer_prefix;
  // A command prefix is something prepended to the command line of the spawned
  // process.
  const base::CommandLine& browser_command_line =
      *base::CommandLine::ForCurrentProcess();
  renderer_prefix =
      browser_command_line.GetSwitchValueNative(switches::kRendererCmdPrefix);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  int flags = renderer_prefix.empty() ? ChildProcessHost::CHILD_ALLOW_SELF
                                      : ChildProcessHost::CHILD_NORMAL;
#elif BUILDFLAG(IS_MAC)
  int flags = ChildProcessHost::CHILD_RENDERER;
#else
  int flags = ChildProcessHost::CHILD_NORMAL;
#endif

  // Find the renderer before creating the channel so if this fails early we
  // return without creating the channel.
  base::FilePath renderer_path = ChildProcessHost::GetChildPath(flags);
  if (renderer_path.empty())
    return false;

  is_initialized_ = true;
  is_dead_ = false;
  sent_render_process_ready_ = false;

  gpu_client_->PreEstablishGpuChannel();

  // Set cache information after establishing a channel since the handles are
  // stored on the channels. Note that we also check if the factory is
  // initialized because in tests the factory may never have been initialized.
  if (!GetBrowserContext()->IsOffTheRecord() &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableGpuShaderDiskCache)) {
    if (auto* cache_factory = GetGpuDiskCacheFactorySingleton()) {
      for (const gpu::GpuDiskCacheType type : gpu::kGpuDiskCacheTypes) {
        auto handle = cache_factory->GetCacheHandle(
            type, storage_partition_impl_->GetPath().Append(
                      gpu::GetGpuDiskCacheSubdir(type)));
        gpu_client_->SetDiskCacheHandle(handle);
      }
    }
  }

  // We may reach Init() during process death notification (e.g.
  // RenderProcessExited on some observer). In this case the Channel may be
  // null, so we re-initialize it here.
  if (!channel_)
    InitializeChannelProxy();

  // Unpause the Channel briefly. This will be paused again below if we launch a
  // real child process. Note that messages may be sent in the short window
  // between now and then (e.g. in response to RenderProcessWillLaunch) and we
  // depend on those messages being sent right away.
  //
  // |channel_| must always be non-null here: either it was initialized in
  // the constructor, or in the most recent call to ProcessDied().
  channel_->Unpause(false /* flush */);

  // Call the embedder first so that their IPC filters have priority.
  GetContentClient()->browser()->RenderProcessWillLaunch(this);

  FieldTrialSynchronizer::UpdateRendererVariationsHeader(this);

#if BUILDFLAG(IS_ANDROID)
  // Initialize the java audio manager so that media session tests will pass.
  // See internal b/29872494.
  static_cast<media::AudioManagerAndroid*>(media::AudioManager::Get())
      ->InitializeIfNeeded();
#endif  // BUILDFLAG(IS_ANDROID)

  CreateMessageFilters();
  RegisterMojoInterfaces();

  // Call this now and not in OnProcessLaunched in case any mojo calls get
  // dispatched before this.
  GetRendererInterface()->InitializeRenderer(
      GetContentClient()->browser()->GetUserAgentBasedOnPolicy(
          browser_context_),
      GetContentClient()->browser()->GetUserAgentMetadata(),
      storage_partition_impl_->cors_exempt_header_list(),
      AttributionManager::GetSupport(),
      GetContentClient()->browser()->GetOriginTrialsSettings());

  if (run_renderer_in_process()) {
    DCHECK(g_renderer_main_thread_factory);
    // Crank up a thread and run the initialization there.  With the way that
    // messages flow between the browser and renderer, this thread is required
    // to prevent a deadlock in single-process mode.  Since the primordial
    // thread in the renderer process runs the WebKit code and can sometimes
    // make blocking calls to the UI thread (i.e. this thread), they need to run
    // on separate threads.
    in_process_renderer_.reset(g_renderer_main_thread_factory(
        InProcessChildThreadParams(GetIOThreadTaskRunner({}),
                                   &mojo_invitation_),
        base::checked_cast<int32_t>(id_)));

    base::Thread::Options options;
#if BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_MAC)
    // In-process plugins require this to be a UI message loop.
    options.message_pump_type = base::MessagePumpType::UI;
#else
    // We can't have multiple UI loops on Linux and Android, so we don't support
    // in-process plugins.
    options.message_pump_type = base::MessagePumpType::DEFAULT;
#endif
    // As for execution sequence, this callback should have no any dependency
    // on starting in-process-render-thread.
    // So put it here to trigger ChannelMojo initialization earlier to enable
    // in-process-render-thread using ChannelMojo there.
    OnProcessLaunched();  // Fake a callback that the process is ready.

    in_process_renderer_->StartWithOptions(std::move(options));

    g_in_process_thread = in_process_renderer_.get();

    // Make sure any queued messages on the channel are flushed in the case
    // where we aren't launching a child process.
    channel_->Flush();
  } else {
    // Build command line for renderer.  We call AppendRendererCommandLine()
    // first so the process type argument will appear first.
    std::unique_ptr<base::CommandLine> cmd_line =
        std::make_unique<base::CommandLine>(renderer_path);
    if (!renderer_prefix.empty())
      cmd_line->PrependWrapper(renderer_prefix);
    AppendRendererCommandLine(cmd_line.get());

#if BUILDFLAG(IS_WIN)
    std::unique_ptr<SandboxedProcessLauncherDelegate> sandbox_delegate =
        std::make_unique<RendererSandboxedProcessLauncherDelegateWin>(
            *cmd_line, IsPdf(), IsJitDisabled());
#else
    std::unique_ptr<SandboxedProcessLauncherDelegate> sandbox_delegate =
        std::make_unique<RendererSandboxedProcessLauncherDelegate>();
#endif

    auto file_data = std::make_unique<ChildProcessLauncherFileData>();
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)
    file_data->files_to_preload = GetV8SnapshotFilesToPreload();
#endif

    // Spawn the child process asynchronously to avoid blocking the UI thread.
    // As long as there's no renderer prefix, we can use the zygote process
    // at this stage.
    child_process_launcher_ = std::make_unique<ChildProcessLauncher>(
        std::move(sandbox_delegate), std::move(cmd_line), GetID(), this,
        std::move(mojo_invitation_),
        base::BindRepeating(&RenderProcessHostImpl::OnMojoError, id_),
        std::move(file_data));
    channel_->Pause();

    // In single process mode, browser-side tracing and memory will cover the
    // whole process including renderers.
    BackgroundTracingManagerImpl::ActivateForProcess(GetID(),
                                                     child_process_.get());

    fast_shutdown_started_ = false;
    shutdown_requested_ = false;
  }

  last_init_time_ = base::TimeTicks::Now();
  return true;
}

void RenderProcessHostImpl::EnableSendQueue() {
  if (!channel_)
    InitializeChannelProxy();
}

void RenderProcessHostImpl::InitializeChannelProxy() {
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner =
      GetIOThreadTaskRunner({});

  // Establish a ChildProcess interface connection to the new renderer. This is
  // connected as the primordial message pipe via a Mojo invitation to the
  // process.
  mojo_invitation_ = {};
  child_process_.reset();
  mojo::PendingRemote<mojom::ChildProcess> child_pending_remote(
      mojo_invitation_.AttachMessagePipe(kChildProcessReceiverAttachmentName),
      /*version=*/0);
  child_process_.Bind(std::move(child_pending_remote));

  // We'll bind this receiver to |io_thread_host_impl_| when it is created.
  child_host_pending_receiver_ = mojo::PendingReceiver<mojom::ChildProcessHost>(
      mojo_invitation_.AttachMessagePipe(
          kChildProcessHostRemoteAttachmentName));

  // Bootstrap the IPC Channel.
  mojo::ScopedMessagePipeHandle bootstrap =
      mojo_invitation_.AttachMessagePipe(kLegacyIpcBootstrapAttachmentName);
  std::unique_ptr<IPC::ChannelFactory> channel_factory =
      IPC::ChannelMojo::CreateServerFactory(
          std::move(bootstrap), io_task_runner,
          base::SingleThreadTaskRunner::GetCurrentDefault());

  ResetChannelProxy();

  DCHECK(!channel_);
  channel_ = IPC::ChannelProxy::Create(
      std::move(channel_factory), this,
      /*ipc_task_runner=*/io_task_runner.get(),
      /*listener_task_runner=*/
      base::SingleThreadTaskRunner::GetCurrentDefault());

  // Note that Channel send is effectively paused and unpaused at various points
  // during startup, and existing code relies on a fragile relative message
  // ordering resulting from some early messages being queued until process
  // launch while others are sent immediately. See https://goo.gl/REW75h for
  // details.
  //
  // We acquire a few associated interface proxies here -- before the channel is
  // paused -- to ensure that subsequent initialization messages on those
  // interfaces behave properly. Specifically, this avoids the risk of an
  // interface being requested while the Channel is paused, which could
  // effectively and undesirably block the transmission of a subsequent message
  // on that interface while the Channel is unpaused.
  //
  // See OnProcessLaunched() for some additional details of this somewhat
  // surprising behavior.
  renderer_interface_.reset();
  channel_->GetRemoteAssociatedInterface(&renderer_interface_);

  // We start the Channel in a paused state. It will be briefly unpaused again
  // in Init() if applicable, before process launch is initiated.
  channel_->Pause();
}

void RenderProcessHostImpl::ResetChannelProxy() {
  if (!channel_)
    return;

  channel_.reset();
  channel_connected_ = false;
}

void RenderProcessHostImpl::CreateMessageFilters() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  MediaInternals* media_internals = MediaInternals::GetInstance();

  scoped_refptr<RenderMessageFilter> render_message_filter =
      base::MakeRefCounted<RenderMessageFilter>(
          GetID(), GetBrowserContext(), widget_helper_.get(), media_internals);
  AddFilter(render_message_filter.get());

#if BUILDFLAG(ENABLE_PPAPI)
  pepper_renderer_connection_ = base::MakeRefCounted<PepperRendererConnection>(
      GetID(), PluginServiceImpl::GetInstance(), GetBrowserContext(),
      GetStoragePartition());
  AddFilter(pepper_renderer_connection_.get());
#endif

  // TODO(https://crbug.com/1178670): Move this initialization out of
  // CreateMessageFilters().
  p2p_socket_dispatcher_host_ =
      std::make_unique<P2PSocketDispatcherHost>(GetID());
}

void RenderProcessHostImpl::BindCacheStorage(
    const network::CrossOriginEmbedderPolicy& cross_origin_embedder_policy,
    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
        coep_reporter_remote,
    const storage::BucketLocator& bucket_locator,
    mojo::PendingReceiver<blink::mojom::CacheStorage> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  storage_partition_impl_->GetCacheStorageControl()->AddReceiver(
      cross_origin_embedder_policy, std::move(coep_reporter_remote),
      bucket_locator, storage::mojom::CacheStorageOwner::kCacheAPI,
      std::move(receiver));
}

void RenderProcessHostImpl::BindIndexedDB(
    const blink::StorageKey& storage_key,
    const GlobalRenderFrameHostId& rfh_id,
    mojo::PendingReceiver<blink::mojom::IDBFactory> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (storage_key.origin().opaque()) {
    // Opaque origins aren't valid for IndexedDB access, so we won't bind
    // |receiver| to |indexed_db_factory_|.  Return early here which
    // will cause |receiver| to be freed.  When |receiver| is
    // freed, we expect the pipe on the client will be closed.
    return;
  }

  storage_partition_impl_->GetIndexedDBControl().BindIndexedDB(
      storage_key,
      IndexedDBClientStateCheckerFactory::InitializePendingAssociatedRemote(
          rfh_id),
      std::move(receiver));
}

void RenderProcessHostImpl::BindBucketManagerHost(
    base::WeakPtr<BucketContext> bucket_context,
    mojo::PendingReceiver<blink::mojom::BucketManagerHost> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  storage_partition_impl_->GetBucketManager()->BindReceiver(
      std::move(bucket_context), std::move(receiver),
      mojo::GetBadMessageCallback());
}

void RenderProcessHostImpl::ForceCrash() {
  child_process_->CrashHungProcess();
}

void RenderProcessHostImpl::BindFileSystemManager(
    const blink::StorageKey& storage_key,
    mojo::PendingReceiver<blink::mojom::FileSystemManager> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Note, the base::Unretained() is safe because the target object has an IO
  // thread deleter and the callback is also targeting the IO thread.
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&FileSystemManagerImpl::BindReceiver,
                     base::Unretained(file_system_manager_impl_.get()),
                     storage_key, std::move(receiver)));
}

void RenderProcessHostImpl::BindFileSystemAccessManager(
    const blink::StorageKey& storage_key,
    mojo::PendingReceiver<blink::mojom::FileSystemAccessManager> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // This code path is only for workers, hence always pass in
  // MSG_ROUTING_NONE as frame ID. Frames themselves go through
  // RenderFrameHostImpl instead.
  auto* manager = storage_partition_impl_->GetFileSystemAccessManager();
  manager->BindReceiver(
      FileSystemAccessManagerImpl::BindingContext(
          storage_key,
          // TODO(https://crbug.com/989323): Obtain and use a better
          // URL for workers instead of the origin as source url.
          // This URL will be used for SafeBrowsing checks and for
          // the Quarantine Service.
          storage_key.origin().GetURL(), GetID()),
      std::move(receiver));
}

void RenderProcessHostImpl::GetSandboxedFileSystemForBucket(
    const storage::BucketLocator& bucket,
    blink::mojom::FileSystemAccessManager::GetSandboxedFileSystemCallback
        callback) {
  auto* manager = storage_partition_impl_->GetFileSystemAccessManager();
  manager->GetSandboxedFileSystem(
      FileSystemAccessManagerImpl::BindingContext(
          bucket.storage_key,
          // TODO(https://crbug.com/989323): Obtain and use a better
          // URL for workers instead of the origin as source url.
          // This URL will be used for SafeBrowsing checks and for
          // the Quarantine Service.
          bucket.storage_key.origin().GetURL(), GetID()),
      bucket, std::move(callback));
}

void RenderProcessHostImpl::BindRestrictedCookieManagerForServiceWorker(
    const blink::StorageKey& storage_key,
    mojo::PendingReceiver<network::mojom::RestrictedCookieManager> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(crbug.com/1386190): Consider whether/how to get cookie setting
  // overrides for a service worker.
  storage_partition_impl_->CreateRestrictedCookieManager(
      network::mojom::RestrictedCookieManagerRole::SCRIPT, storage_key.origin(),
      storage_key.ToPartialNetIsolationInfo(),
      /*is_service_worker=*/true, GetID(), MSG_ROUTING_NONE,
      net::CookieSettingOverrides(), std::move(receiver),
      storage_partition_impl_->CreateCookieAccessObserverForServiceWorker());
}

void RenderProcessHostImpl::BindVideoDecodePerfHistory(
    mojo::PendingReceiver<media::mojom::VideoDecodePerfHistory> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetBrowserContext()->GetVideoDecodePerfHistory()->BindReceiver(
      std::move(receiver));
}

void RenderProcessHostImpl::BindWebrtcVideoPerfHistory(
    mojo::PendingReceiver<media::mojom::WebrtcVideoPerfHistory> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserContextImpl::From(GetBrowserContext())
      ->GetWebrtcVideoPerfHistory()
      ->BindReceiver(std::move(receiver));
}

void RenderProcessHostImpl::BindQuotaManagerHost(
    const blink::StorageKey& storage_key,
    mojo::PendingReceiver<blink::mojom::QuotaManagerHost> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  storage_partition_impl_->GetQuotaContext()->BindQuotaManagerHost(
      storage_key, std::move(receiver));
}

void RenderProcessHostImpl::CreateLockManager(
    const blink::StorageKey& storage_key,
    mojo::PendingReceiver<blink::mojom::LockManager> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  storage_partition_impl_->GetQuotaManager()->proxy()->UpdateOrCreateBucket(
      storage::BucketInitParams::ForDefaultBucket(storage_key),
      GetUIThreadTaskRunner({}),
      base::BindOnce(&RenderProcessHostImpl::CreateLockManagerWithBucketInfo,
                     instance_weak_factory_.GetWeakPtr(), std::move(receiver)));
}

void RenderProcessHostImpl::CreateLockManagerWithBucketInfo(
    mojo::PendingReceiver<blink::mojom::LockManager> receiver,
    storage::QuotaErrorOr<storage::BucketInfo> bucket) {
  storage_partition_impl_->GetLockManager()->BindReceiver(
      bucket.has_value() ? bucket->id : storage::BucketId(),
      std::move(receiver));
}

void RenderProcessHostImpl::CreatePermissionService(
    const url::Origin& origin,
    mojo::PendingReceiver<blink::mojom::PermissionService> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!permission_service_context_) {
    permission_service_context_ =
        std::make_unique<PermissionServiceContext>(this);
  }

  permission_service_context_->CreateServiceForWorker(origin,
                                                      std::move(receiver));
}

void RenderProcessHostImpl::CreatePaymentManagerForOrigin(
    const url::Origin& origin,
    mojo::PendingReceiver<payments::mojom::PaymentManager> receiver) {
  storage_partition_impl_->GetPaymentAppContext()
      ->CreatePaymentManagerForOrigin(origin, std::move(receiver));
}

void RenderProcessHostImpl::CreateNotificationService(
    GlobalRenderFrameHostId rfh_id,
    const RenderProcessHost::NotificationServiceCreatorType creator_type,
    const blink::StorageKey& storage_key,
    mojo::PendingReceiver<blink::mojom::NotificationService> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderFrameHost* rfh = RenderFrameHost::FromID(rfh_id);
  WeakDocumentPtr weak_document_ptr =
      rfh ? rfh->GetWeakDocumentPtr() : WeakDocumentPtr();
  switch (creator_type) {
    case RenderProcessHost::NotificationServiceCreatorType::kServiceWorker:
    case RenderProcessHost::NotificationServiceCreatorType::kSharedWorker:
    case RenderProcessHost::NotificationServiceCreatorType::kDedicatedWorker: {
      storage_partition_impl_->GetPlatformNotificationContext()->CreateService(
          this, storage_key, /*document_url=*/GURL(), weak_document_ptr,
          creator_type, std::move(receiver));
      break;
    }
    case RenderProcessHost::NotificationServiceCreatorType::kDocument: {
      CHECK(rfh);

      storage_partition_impl_->GetPlatformNotificationContext()->CreateService(
          this, storage_key, rfh->GetLastCommittedURL(), weak_document_ptr,
          creator_type, std::move(receiver));
      break;
    }
  }
}

void RenderProcessHostImpl::CreateWebSocketConnector(
    const blink::StorageKey& storage_key,
    mojo::PendingReceiver<blink::mojom::WebSocketConnector> receiver) {
  // TODO(jam): is it ok to not send extraHeaders for sockets created from
  // shared and service workers?
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<WebSocketConnectorImpl>(
          GetID(), MSG_ROUTING_NONE, storage_key.origin(),
          storage_key.ToPartialNetIsolationInfo()),
      std::move(receiver));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void RenderProcessHostImpl::ReinitializeLogging(
    uint32_t logging_dest,
    base::ScopedFD log_file_descriptor) {
  auto logging_settings = mojom::LoggingSettings::New();
  logging_settings->logging_dest = logging_dest;
  logging_settings->log_file_descriptor =
      mojo::PlatformHandle(std::move(log_file_descriptor));
  child_process_->ReinitializeLogging(std::move(logging_settings));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
void RenderProcessHostImpl::CreateStableVideoDecoder(
    mojo::PendingReceiver<media::stable::mojom::StableVideoDecoder> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!stable_video_decoder_factory_remote_.is_bound()) {
    auto creation_cb = GetStableVideoDecoderFactoryCreationCB();
    if (creation_cb.is_null()) {
      LaunchStableVideoDecoderFactory(
          stable_video_decoder_factory_remote_.BindNewPipeAndPassReceiver());
    } else {
      creation_cb.Run(
          stable_video_decoder_factory_remote_.BindNewPipeAndPassReceiver());
    }

    stable_video_decoder_factory_remote_.set_disconnect_handler(
        base::BindOnce(&RenderProcessHostImpl::ResetStableVideoDecoderFactory,
                       instance_weak_factory_.GetWeakPtr()));

    // Version 1 introduced the ability to pass a
    // mojo::PendingRemote<StableVideoDecoderTracker> to
    // CreateStableVideoDecoder().
    stable_video_decoder_factory_remote_.RequireVersion(1u);
  }

  CHECK(stable_video_decoder_factory_remote_.is_bound());

  mojo::PendingRemote<media::stable::mojom::StableVideoDecoderTracker>
      tracker_remote;
  stable_video_decoder_trackers_.Add(
      this, tracker_remote.InitWithNewPipeAndPassReceiver());
  stable_video_decoder_factory_remote_->CreateStableVideoDecoder(
      std::move(receiver), std::move(tracker_remote));
  if (stable_video_decoder_factory_reset_timer_.IsRunning()) {
    // |stable_video_decoder_factory_reset_timer_| has been started to
    // eventually reset() the |stable_video_decoder_factory_remote_|. Now that
    // we got a request to create a StableVideoDecoder before the timer
    // triggered, we can stop it so that the utility process associated with the
    // |stable_video_decoder_factory_remote_| doesn't die.
    stable_video_decoder_factory_reset_timer_.Stop();
    InvokeStableVideoDecoderEventCB(
        StableVideoDecoderEvent::kFactoryResetTimerStopped);
  }
}

void RenderProcessHostImpl::OnStableVideoDecoderDisconnected() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (stable_video_decoder_trackers_.empty()) {
    // All StableVideoDecoders have disconnected. Let's reset() the
    // |stable_video_decoder_factory_remote_| so that the corresponding utility
    // process gets terminated. Note that we don't reset() immediately. Instead,
    // we wait a little bit in case a request to create another
    // StableVideoDecoder comes in. That way, we don't unnecessarily tear down
    // the video decoder process just to create another one almost immediately.
    // We chose 3 seconds because it seemed "reasonable."
    constexpr base::TimeDelta kTimeToResetStableVideoDecoderFactory =
        base::Seconds(3);
    stable_video_decoder_factory_reset_timer_.Start(
        FROM_HERE, kTimeToResetStableVideoDecoderFactory,
        base::BindOnce(&RenderProcessHostImpl::ResetStableVideoDecoderFactory,
                       instance_weak_factory_.GetWeakPtr()));
    InvokeStableVideoDecoderEventCB(
        StableVideoDecoderEvent::kAllDecodersDisconnected);
  }
}

void RenderProcessHostImpl::ResetStableVideoDecoderFactory() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  stable_video_decoder_factory_remote_.reset();

  // Note that |stable_video_decoder_trackers_| should be empty if
  // ResetStableVideoDecoderFactory() was called because
  // |stable_video_decoder_factory_reset_timer_| fired. Otherwise, there's no
  // guarantee about its contents. For example, maybe
  // ResetStableVideoDecoderFactory() got called because the video decoder
  // process crashed and we got the disconnection notification for
  // |stable_video_decoder_factory_remote_| before the disconnection
  // notification for any of the elements in |stable_video_decoder_trackers_|.
  stable_video_decoder_trackers_.Clear();

  if (stable_video_decoder_factory_reset_timer_.IsRunning()) {
    stable_video_decoder_factory_reset_timer_.Stop();
    InvokeStableVideoDecoderEventCB(
        StableVideoDecoderEvent::kFactoryResetTimerStopped);
  }
}

void RenderProcessHostImpl::SetStableVideoDecoderFactoryCreationCBForTesting(
    StableVideoDecoderFactoryCreationCB callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetStableVideoDecoderFactoryCreationCB() = callback;
}

void RenderProcessHostImpl::SetStableVideoDecoderEventCBForTesting(
    StableVideoDecoderEventCB callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetStableVideoDecoderEventCB() = callback;
}
#endif  // BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)

void RenderProcessHostImpl::DelayProcessShutdown(
    const base::TimeDelta& subframe_shutdown_timeout,
    const base::TimeDelta& unload_handler_timeout,
    const SiteInfo& site_info) {
  // No need to delay shutdown if the process is already shutting down.
  if (AreRefCountsDisabled() || deleting_soon_ || fast_shutdown_started_) {
    return;
  }

  shutdown_delay_ref_count_++;

  // Add to the delayed-shutdown tracker with the site that triggered the delay.
  if (ShouldDelayProcessShutdown() && ShouldTrackProcessForSite(site_info)) {
    SiteProcessCountTracker* delayed_shutdown_tracker =
        SiteProcessCountTracker::GetInstance(
            GetBrowserContext(),
            content::kDelayedShutdownSiteProcessCountTrackerKey);
    delayed_shutdown_tracker->IncrementSiteProcessCount(site_info, GetID());
  }

  // Don't delay shutdown longer than the maximum delay for renderer process,
  // enforced for security reasons (https://crbug.com/1177674).
  GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&RenderProcessHostImpl::CancelProcessShutdownDelay,
                     instance_weak_factory_.GetWeakPtr(), site_info),
      std::min(subframe_shutdown_timeout + unload_handler_timeout,
               kKeepAliveHandleFactoryTimeout));

  time_spent_running_unload_handlers_ = unload_handler_timeout;
}

bool RenderProcessHostImpl::IsProcessShutdownDelayedForTesting() {
  SiteProcessCountTracker* delayed_shutdown_tracker =
      SiteProcessCountTracker::GetInstance(
          GetBrowserContext(),
          content::kDelayedShutdownSiteProcessCountTrackerKey);
  return delayed_shutdown_tracker->ContainsHost(GetID());
}

std::string
RenderProcessHostImpl::GetInfoForBrowserContextDestructionCrashReporting() {
  std::string ret = " pl='" + GetProcessLock().ToString() + "'";

  if (HostHasNotBeenUsed())
    ret += " hnbu";

  if (IsSpareProcessForCrashReporting(this))
    ret += " spr";

  if (delayed_cleanup_needed_)
    ret += " dcn";

  if (keep_alive_ref_count_ != 0) {
    CHECK(!base::FeatureList::IsEnabled(
        blink::features::kKeepAliveInBrowserMigration));
    ret += " karc=" + base::NumberToString(keep_alive_ref_count_);
  }

  if (shutdown_delay_ref_count_ != 0)
    ret += " sdrc=" + base::NumberToString(shutdown_delay_ref_count_);

  if (worker_ref_count_ != 0)
    ret += " wrc=" + base::NumberToString(worker_ref_count_);

  if (pending_reuse_ref_count_ != 0) {
    ret += " prrc=" + base::NumberToString(pending_reuse_ref_count_);
  }

  if (!listeners_.IsEmpty()) {
    ret += " lsn=" + base::NumberToString(listeners_.size());

    base::IDMap<IPC::Listener*>::Iterator<IPC::Listener> it(&listeners_);
    IPC::Listener* example_listener = it.GetCurrentValue();
    ret += "[" + example_listener->ToDebugString() + "]";
  }

  if (deleting_soon_)
    ret += " ds";

  return ret;
}

#if BUILDFLAG(CLANG_PROFILING_INSIDE_SANDBOX)
void RenderProcessHostImpl::DumpProfilingData(base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetRendererInterface()->WriteClangProfilingProfile(std::move(callback));
}
#endif

void RenderProcessHostImpl::WriteIntoTrace(
    perfetto::TracedProto<perfetto::protos::pbzero::RenderProcessHost> proto)
    const {
  proto->set_id(GetID());
  proto->set_process_lock(GetProcessLock().ToString());
  proto.Set(TraceProto::kBrowserContext, browser_context_);

  // Pid() can be called only on valid process, so we should check for this
  // before accessing it. In addition, Pid() should only be read once the
  // process has finished starting.
  // TODO(ssid): Consider moving this to ChildProcessLauncher proto field.
  if (child_process_launcher_ && !child_process_launcher_->IsStarting()) {
    const base::Process& process = child_process_launcher_->GetProcess();
    if (process.IsValid())
      proto->set_child_process_id(process.Pid());
  }

  perfetto::TracedDictionary dict = std::move(proto).AddDebugAnnotations();
  // Can be null in the unittests.
  if (ChildProcessSecurityPolicyImpl::GetInstance())
    dict.Add("process_lock", GetProcessLock().ToString());
}

void RenderProcessHostImpl::RegisterMojoInterfaces() {
  auto registry = std::make_unique<service_manager::BinderRegistry>();

  AddUIThreadInterface(
      registry.get(),
      base::BindRepeating(
          [](mojo::PendingReceiver<device::mojom::TimeZoneMonitor> receiver) {
            GetDeviceService().BindTimeZoneMonitor(std::move(receiver));
          }));

  AddUIThreadInterface(
      registry.get(),
      base::BindRepeating(
          [](mojo::PendingReceiver<device::mojom::PowerMonitor> receiver) {
            GetDeviceService().BindPowerMonitor(std::move(receiver));
          }));

  AddUIThreadInterface(
      registry.get(),
      base::BindRepeating(
          [](mojo::PendingReceiver<device::mojom::ScreenOrientationListener>
                 receiver) {
            GetDeviceService().BindScreenOrientationListener(
                std::move(receiver));
          }));

  AddUIThreadInterface(
      registry.get(),
      base::BindRepeating(
          &RenderProcessHostImpl::CreateEmbeddedFrameSinkProvider,
          instance_weak_factory_.GetWeakPtr()));

  AddUIThreadInterface(
      registry.get(),
      base::BindRepeating(&RenderProcessHostImpl::BindCompositingModeReporter,
                          instance_weak_factory_.GetWeakPtr()));

  AddUIThreadInterface(
      registry.get(),
      base::BindRepeating(&RenderProcessHostImpl::CreateDomStorageProvider,
                          instance_weak_factory_.GetWeakPtr()));

  AddUIThreadInterface(
      registry.get(),
      base::BindRepeating(&RenderProcessHostImpl::BindWebDatabaseHostImpl,
                          instance_weak_factory_.GetWeakPtr()));

  AddUIThreadInterface(
      registry.get(),
      base::BindRepeating(
          [](base::WeakPtr<RenderProcessHostImpl> host,
             mojo::PendingReceiver<
                 memory_instrumentation::mojom::CoordinatorConnector>
                 receiver) {
            if (!host)
              return;
            host->coordinator_connector_receiver_.reset();
            host->coordinator_connector_receiver_.Bind(std::move(receiver));
            if (!host->GetProcess().IsValid()) {
              // We only want to accept messages from this interface once we
              // have a known PID.
              host->coordinator_connector_receiver_.Pause();
            }
          },
          instance_weak_factory_.GetWeakPtr()));

  registry->AddInterface(
      base::BindRepeating(&MimeRegistryImpl::Create),
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
           base::TaskPriority::USER_BLOCKING}));
#if BUILDFLAG(USE_MINIKIN_HYPHENATION)
#if !BUILDFLAG(IS_ANDROID)
  hyphenation::HyphenationImpl::RegisterGetDictionary();
#endif
  registry->AddInterface(
      base::BindRepeating(&hyphenation::HyphenationImpl::Create),
      hyphenation::HyphenationImpl::GetTaskRunner());
#endif
#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(features::kFontSrcLocalMatching)) {
    registry->AddInterface(
        base::BindRepeating(&FontUniqueNameLookupService::Create),
        FontUniqueNameLookupService::GetTaskRunner());
  }
#endif

#if BUILDFLAG(IS_WIN)
  registry->AddInterface(
      base::BindRepeating(&DWriteFontProxyImpl::Create),
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::USER_BLOCKING, base::MayBlock()}));
#endif

  file_system_manager_impl_.reset(new FileSystemManagerImpl(
      GetID(), storage_partition_impl_->GetFileSystemContext(),
      ChromeBlobStorageContext::GetFor(GetBrowserContext())));

  AddUIThreadInterface(
      registry.get(), base::BindRepeating(&viz::GpuClient::Add,
                                          base::Unretained(gpu_client_.get())));

  registry->AddInterface(
      base::BindRepeating(&GpuDataManagerImpl::BindReceiver));

  // Note, the base::Unretained() is safe because the target object has an IO
  // thread deleter and the callback is also targeting the IO thread.  When
  // the RPHI is destroyed it also triggers the destruction of the registry
  // on the IO thread.
  media_stream_track_metrics_host_.reset(new MediaStreamTrackMetricsHost());
  registry->AddInterface(base::BindRepeating(
      &MediaStreamTrackMetricsHost::BindReceiver,
      base::Unretained(media_stream_track_metrics_host_.get())));

  registry->AddInterface(
      base::BindRepeating(&metrics::CreateSingleSampleMetricsProvider));

  AddUIThreadInterface(
      registry.get(),
      base::BindRepeating(&RenderProcessHostImpl::CreateMediaLogRecordHost,
                          instance_weak_factory_.GetWeakPtr()));

  AddUIThreadInterface(registry.get(),
                       base::BindRepeating(&FieldTrialRecorder::Create));

  associated_interfaces_ =
      std::make_unique<blink::AssociatedInterfaceRegistry>();
  blink::AssociatedInterfaceRegistry* associated_registry =
      associated_interfaces_.get();

  // This base::Unretained() usage is safe since the associated_registry is
  // owned by this RPHI.
  associated_registry->AddInterface<mojom::RendererHost>(base::BindRepeating(
      &RenderProcessHostImpl::CreateRendererHost, base::Unretained(this)));

  registry->AddInterface(
      base::BindRepeating(&BlobRegistryWrapper::Bind,
                          storage_partition_impl_->GetBlobRegistry(), GetID()));

#if BUILDFLAG(ENABLE_PLUGINS)
  // Initialization can happen more than once (in the case of a child process
  // crash), but we don't want to lose the plugin registry in this case.
  if (!plugin_registry_) {
    plugin_registry_ = std::make_unique<PluginRegistryImpl>(GetID());
  }
  AddUIThreadInterface(
      registry.get(),
      base::BindRepeating(&RenderProcessHostImpl::BindPluginRegistry,
                          instance_weak_factory_.GetWeakPtr()));
#else
  // On platforms where plugins are disabled, the PluginRegistry interface is
  // never bound. This still results in posting a task on the UI thread to
  // look for the interface which can be slow. Instead, drop the interface
  // immediately on the IO thread by binding en empty interface handler.
  registry->AddInterface(base::BindRepeating(
      [](mojo::PendingReceiver<blink::mojom::PluginRegistry> receiver) {}));
#endif

#if BUILDFLAG(ENABLE_LIBRARY_CDMS) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
  AddUIThreadInterface(
      registry.get(), base::BindRepeating(&KeySystemSupportImpl::BindReceiver));
#endif

  AddUIThreadInterface(
      registry.get(),
      base::BindRepeating(&RenderProcessHostImpl::BindMediaInterfaceProxy,
                          instance_weak_factory_.GetWeakPtr()));

  AddUIThreadInterface(
      registry.get(),
      base::BindRepeating(&RenderProcessHostImpl::BindAecDumpManager,
                          instance_weak_factory_.GetWeakPtr()));

#if BUILDFLAG(IS_FUCHSIA)
  AddUIThreadInterface(
      registry.get(),
      base::BindRepeating(&RenderProcessHostImpl::BindMediaCodecProvider,
                          instance_weak_factory_.GetWeakPtr()));
#endif

  // ---- Please do not register interfaces below this line ------
  //
  // This call should be done after registering all interfaces above, so that
  // embedder can override any interfaces. The fact that registry calls
  // the last registration for the name allows us to easily override interfaces.
  GetContentClient()->browser()->ExposeInterfacesToRenderer(
      registry.get(), associated_interfaces_.get(), this);

  DCHECK(child_host_pending_receiver_);
  io_thread_host_impl_.emplace(
      GetIOThreadTaskRunner({}), GetID(), instance_weak_factory_.GetWeakPtr(),
      std::move(registry), std::move(child_host_pending_receiver_));
}

void RenderProcessHostImpl::CreateEmbeddedFrameSinkProvider(
    mojo::PendingReceiver<blink::mojom::EmbeddedFrameSinkProvider> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!embedded_frame_sink_provider_) {
    // The client id gets converted to a uint32_t in FrameSinkId.
    uint32_t renderer_client_id = base::checked_cast<uint32_t>(id_);
    embedded_frame_sink_provider_ =
        std::make_unique<EmbeddedFrameSinkProviderImpl>(
            GetHostFrameSinkManager(), renderer_client_id);
  }
  embedded_frame_sink_provider_->Add(std::move(receiver));
}

void RenderProcessHostImpl::BindCompositingModeReporter(
    mojo::PendingReceiver<viz::mojom::CompositingModeReporter> receiver) {
  BrowserMainLoop::GetInstance()->GetCompositingModeReporter(
      std::move(receiver));
}

void RenderProcessHostImpl::CreateDomStorageProvider(
    mojo::PendingReceiver<blink::mojom::DomStorageProvider> receiver) {
  DCHECK(!dom_storage_provider_receiver_.is_bound());
  dom_storage_provider_receiver_.Bind(std::move(receiver));
}

void RenderProcessHostImpl::BindMediaInterfaceProxy(
    mojo::PendingReceiver<media::mojom::InterfaceFactory> receiver) {
  if (!media_interface_proxy_)
    media_interface_proxy_ =
        std::make_unique<FramelessMediaInterfaceProxy>(this);
  media_interface_proxy_->Add(std::move(receiver));
}

void RenderProcessHostImpl::BindWebDatabaseHostImpl(
    mojo::PendingReceiver<blink::mojom::WebDatabaseHost> receiver) {
  storage::DatabaseTracker* db_tracker =
      storage_partition_impl_->GetDatabaseTracker();
  db_tracker->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&WebDatabaseHostImpl::Create, GetID(),
                     base::WrapRefCounted(db_tracker), std::move(receiver)));
}
void RenderProcessHostImpl::BindAecDumpManager(
    mojo::PendingReceiver<blink::mojom::AecDumpManager> receiver) {
  aec_dump_manager_.AddReceiver(std::move(receiver));
}

void RenderProcessHostImpl::CreateOneShotSyncService(
    const url::Origin& origin,
    mojo::PendingReceiver<blink::mojom::OneShotBackgroundSyncService>
        receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  storage_partition_impl_->GetBackgroundSyncContext()->CreateOneShotSyncService(
      origin, this, std::move(receiver));
}

void RenderProcessHostImpl::CreatePeriodicSyncService(
    const url::Origin& origin,
    mojo::PendingReceiver<blink::mojom::PeriodicBackgroundSyncService>
        receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  storage_partition_impl_->GetBackgroundSyncContext()
      ->CreatePeriodicSyncService(origin, this, std::move(receiver));
}

void RenderProcessHostImpl::BindPushMessaging(
    mojo::PendingReceiver<blink::mojom::PushMessaging> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  push_messaging_manager_->AddPushMessagingReceiver(std::move(receiver));
}

void RenderProcessHostImpl::BindP2PSocketManager(
    net::NetworkAnonymizationKey anonymization_key,
    mojo::PendingReceiver<network::mojom::P2PSocketManager> receiver,
    GlobalRenderFrameHostId render_frame_host_id) {
  p2p_socket_dispatcher_host_->BindReceiver(
      *this, std::move(receiver), anonymization_key, render_frame_host_id);
}

void RenderProcessHostImpl::CreateMediaLogRecordHost(
    mojo::PendingReceiver<content::mojom::MediaInternalLogRecords> receiver) {
  content::MediaInternals::CreateMediaLogRecords(GetID(), std::move(receiver));
}

#if BUILDFLAG(ENABLE_PLUGINS)
void RenderProcessHostImpl::BindPluginRegistry(
    mojo::PendingReceiver<blink::mojom::PluginRegistry> receiver) {
  plugin_registry_->Bind(std::move(receiver));
}
#endif

#if BUILDFLAG(IS_FUCHSIA)
void RenderProcessHostImpl::BindMediaCodecProvider(
    mojo::PendingReceiver<media::mojom::FuchsiaMediaCodecProvider> receiver) {
  if (!media_codec_provider_)
    media_codec_provider_ = std::make_unique<FuchsiaMediaCodecProviderImpl>();
  media_codec_provider_->AddReceiver(std::move(receiver));
}
#endif

void RenderProcessHostImpl::BindDomStorage(
    mojo::PendingReceiver<blink::mojom::DomStorage> receiver,
    mojo::PendingRemote<blink::mojom::DomStorageClient> client) {
  const DomStorageBinder& binder = GetDomStorageBinder();
  if (binder) {
    binder.Run(this, std::move(receiver));
    return;
  }

  dom_storage_receiver_ids_.insert(storage_partition_impl_->BindDomStorage(
      id_, std::move(receiver), std::move(client)));

  // Renderers only use this interface to send a single BindDomStorage message,
  // so we can tear down the receiver now.
  dom_storage_provider_receiver_.reset();
}

void RenderProcessHostImpl::RegisterCoordinatorClient(
    mojo::PendingReceiver<memory_instrumentation::mojom::Coordinator> receiver,
    mojo::PendingRemote<memory_instrumentation::mojom::ClientProcess>
        client_process) {
  // Intentionally disallow non-browser processes from getting a Coordinator.
  receiver.reset();

  if (!GetProcess().IsValid()) {
    // If the process dies before we get this message. we have no valid PID
    // and there's nothing to register.
    return;
  }

  base::trace_event::MemoryDumpManager::GetInstance()
      ->GetDumpThreadTaskRunner()
      ->PostTask(
          FROM_HERE,
          base::BindOnce(
              [](mojo::PendingReceiver<
                     memory_instrumentation::mojom::Coordinator> receiver,
                 mojo::PendingRemote<
                     memory_instrumentation::mojom::ClientProcess>
                     client_process,
                 base::ProcessId pid) {
                GetMemoryInstrumentationRegistry()->RegisterClientProcess(
                    std::move(receiver), std::move(client_process),
                    memory_instrumentation::mojom::ProcessType::RENDERER, pid,
                    /*service_name=*/absl::nullopt);
              },
              std::move(receiver), std::move(client_process),
              GetProcess().Pid()));

  coordinator_connector_receiver_.reset();
}

void RenderProcessHostImpl::CreateRendererHost(
    mojo::PendingAssociatedReceiver<mojom::RendererHost> receiver) {
  renderer_host_receiver_.Bind(std::move(receiver));
}

int RenderProcessHostImpl::GetNextRoutingID() {
  return widget_helper_->GetNextRoutingID();
}

void RenderProcessHostImpl::BindReceiver(
    mojo::GenericPendingReceiver receiver) {
  child_process_->BindReceiver(std::move(receiver));
}

std::unique_ptr<base::PersistentMemoryAllocator>
RenderProcessHostImpl::TakeMetricsAllocator() {
  return std::move(metrics_allocator_);
}

const base::TimeTicks& RenderProcessHostImpl::GetLastInitTime() {
  return last_init_time_;
}

bool RenderProcessHostImpl::IsProcessBackgrounded() {
  return priority_.is_background();
}

void RenderProcessHostImpl::IncrementKeepAliveRefCount(uint64_t handle_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(!are_ref_counts_disabled_);
  if (base::FeatureList::IsEnabled(kCheckNoNewRefCountsWhenRphDeletingSoon)) {
    CHECK(!deleting_soon_);
  }
  CHECK(!base::FeatureList::IsEnabled(
      blink::features::kKeepAliveInBrowserMigration));
  ++keep_alive_ref_count_;
  DCHECK(!keep_alive_start_times_.contains(handle_id));
  keep_alive_start_times_[handle_id] = base::Time::Now();
}

bool RenderProcessHostImpl::AreAllRefCountsZero() {
  if (base::FeatureList::IsEnabled(
          blink::features::kKeepAliveInBrowserMigration)) {
    CHECK_EQ(keep_alive_ref_count_, 0);
  }
  return keep_alive_ref_count_ == 0 && worker_ref_count_ == 0 &&
         shutdown_delay_ref_count_ == 0 && pending_reuse_ref_count_ == 0;
}

void RenderProcessHostImpl::DecrementKeepAliveRefCount(uint64_t handle_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(!are_ref_counts_disabled_);
  CHECK(!base::FeatureList::IsEnabled(
      blink::features::kKeepAliveInBrowserMigration));
  CHECK_GT(keep_alive_ref_count_, 0);
  --keep_alive_ref_count_;
  DCHECK(keep_alive_start_times_.contains(handle_id));
  keep_alive_start_times_.erase(handle_id);
  if (AreAllRefCountsZero())
    Cleanup();
}

void RenderProcessHostImpl::IncrementPendingReuseRefCount() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(!are_ref_counts_disabled_);
  if (base::FeatureList::IsEnabled(kCheckNoNewRefCountsWhenRphDeletingSoon)) {
    CHECK(!deleting_soon_);
  }
  ++pending_reuse_ref_count_;
}

void RenderProcessHostImpl::DecrementPendingReuseRefCount() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(!are_ref_counts_disabled_);
  CHECK_GT(pending_reuse_ref_count_, 0);
  --pending_reuse_ref_count_;
  if (AreAllRefCountsZero()) {
    Cleanup();
  }
}

std::string RenderProcessHostImpl::GetKeepAliveDurations() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::stringstream result;
  base::Time now = base::Time::Now();
  result << keep_alive_start_times_.size() << " uid/time-deltas:";
  for (auto entry : keep_alive_start_times_)
    result << " " << entry.first << "/" << (now - entry.second);
  result << ".";
  return result.str();
}

size_t RenderProcessHostImpl::GetShutdownDelayRefCount() const {
  return shutdown_delay_ref_count_;
}

int RenderProcessHostImpl::GetRenderFrameHostCount() const {
  return render_frame_host_id_set_.size();
}

void RenderProcessHostImpl::ForEachRenderFrameHost(
    base::RepeatingCallback<void(RenderFrameHost*)> on_render_frame_host) {
  for (auto rfh_id : render_frame_host_id_set_) {
    RenderFrameHostImpl* rfh = RenderFrameHostImpl::FromID(rfh_id);
    // Note that some RenderFrameHosts in the set may not be found by FromID if
    // we get here during their destructor (e.g., while deleting their subframe
    // RenderFrameHosts).
    if (!rfh)
      continue;

    // Speculative RFHs are not exposed to //content embedders, so we have to
    // explicitly check them here to avoid leaks.
    if (rfh->lifecycle_state() ==
        RenderFrameHostImpl::LifecycleStateImpl::kSpeculative) {
      continue;
    }
    on_render_frame_host.Run(rfh);
  }
}

void RenderProcessHostImpl::RegisterRenderFrameHost(
    const GlobalRenderFrameHostId& render_frame_host_id) {
  DCHECK(!base::Contains(render_frame_host_id_set_, render_frame_host_id));
  render_frame_host_id_set_.insert(render_frame_host_id);
}

void RenderProcessHostImpl::UnregisterRenderFrameHost(
    const GlobalRenderFrameHostId& render_frame_host_id) {
  DCHECK(base::Contains(render_frame_host_id_set_, render_frame_host_id));
  render_frame_host_id_set_.erase(render_frame_host_id);
}

void RenderProcessHostImpl::IncrementWorkerRefCount() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(!are_ref_counts_disabled_);
  if (base::FeatureList::IsEnabled(kCheckNoNewRefCountsWhenRphDeletingSoon)) {
    CHECK(!deleting_soon_);
  }
  ++worker_ref_count_;
}

void RenderProcessHostImpl::DecrementWorkerRefCount() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(!are_ref_counts_disabled_);
  CHECK_GT(worker_ref_count_, 0);
  --worker_ref_count_;
  if (AreAllRefCountsZero())
    Cleanup();
}

void RenderProcessHostImpl::DisableRefCounts() {
  TRACE_EVENT("shutdown", "RenderProcessHostImpl::DisableRefCounts",
              ChromeTrackEvent::kRenderProcessHost, *this);

  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (are_ref_counts_disabled_)
    return;
  are_ref_counts_disabled_ = true;

  keep_alive_ref_count_ = 0;
  worker_ref_count_ = 0;
  shutdown_delay_ref_count_ = 0;
  pending_reuse_ref_count_ = 0;
  // Cleaning up will also remove this from the SpareRenderProcessHostManager.
  // (in this case |keep_alive_ref_count_| would be 0 even before).
  Cleanup();
}

bool RenderProcessHostImpl::AreRefCountsDisabled() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return are_ref_counts_disabled_;
}

mojom::Renderer* RenderProcessHostImpl::GetRendererInterface() {
  return renderer_interface_.get();
}

ProcessLock RenderProcessHostImpl::GetProcessLock() const {
  return ChildProcessSecurityPolicyImpl::GetInstance()->GetProcessLock(GetID());
}

// static
void RenderProcessHostImpl::SetNetworkFactoryForTesting(
    const CreateNetworkFactoryCallback& create_network_factory_callback) {
  DCHECK(!BrowserThread::IsThreadInitialized(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(create_network_factory_callback.is_null() ||
         GetCreateNetworkFactoryCallback().is_null())
      << "It is not expected that this is called with non-null callback when "
      << "another overriding callback is already set.";
  GetCreateNetworkFactoryCallback() = create_network_factory_callback;
}

void RenderProcessHostImpl::CreateURLLoaderFactory(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
    network::mojom::URLLoaderFactoryParamsPtr params) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(params);
  DCHECK_EQ(GetID(), static_cast<int>(params->process_id));

  if (GetCreateNetworkFactoryCallback().is_null()) {
    storage_partition_impl_->GetNetworkContext()->CreateURLLoaderFactory(
        std::move(receiver), std::move(params));
  } else {
    mojo::PendingRemote<network::mojom::URLLoaderFactory> original_factory;
    storage_partition_impl_->GetNetworkContext()->CreateURLLoaderFactory(
        original_factory.InitWithNewPipeAndPassReceiver(), std::move(params));
    GetCreateNetworkFactoryCallback().Run(std::move(receiver), GetID(),
                                          std::move(original_factory));
  }
}

bool RenderProcessHostImpl::MayReuseHost() {
  return GetContentClient()->browser()->MayReuseHost(this);
}

bool RenderProcessHostImpl::IsUnused() {
  return is_unused_;
}

void RenderProcessHostImpl::SetIsUsed() {
  is_unused_ = false;
}

void RenderProcessHostImpl::AddRoute(int32_t routing_id,
                                     IPC::Listener* listener) {
  TRACE_EVENT("shutdown", "RenderProcessHostImpl::AddRoute",
              ChromeTrackEvent::kRenderProcessHost, *this,
              [&](perfetto::EventContext ctx) {
                auto* proto = ctx.event<ChromeTrackEvent>()
                                  ->set_render_process_host_listener_changed();
                proto->set_routing_id(routing_id);
              });
  if (base::FeatureList::IsEnabled(kCheckNoNewRefCountsWhenRphDeletingSoon)) {
    CHECK(!deleting_soon_);
  }
  CHECK(!listeners_.Lookup(routing_id))
      << "Found Routing ID Conflict: " << routing_id;
  listeners_.AddWithID(listener, routing_id);
}

void RenderProcessHostImpl::RemoveRoute(int32_t routing_id) {
  TRACE_EVENT("shutdown", "RenderProcessHostImpl::RemoveRoute",
              ChromeTrackEvent::kRenderProcessHost, *this,
              [&](perfetto::EventContext ctx) {
                auto* proto = ctx.event<ChromeTrackEvent>()
                                  ->set_render_process_host_listener_changed();
                proto->set_routing_id(routing_id);
              });
  DCHECK(listeners_.Lookup(routing_id) != nullptr);
  listeners_.Remove(routing_id);
  Cleanup();
}

bool RenderProcessHostImpl::TakeFrameTokensForFrameRoutingID(
    int32_t new_routing_id,
    blink::LocalFrameToken& frame_token,
    base::UnguessableToken& devtools_frame_token,
    blink::DocumentToken& document_token) {
  return widget_helper_->TakeFrameTokensForFrameRoutingID(
      new_routing_id, frame_token, devtools_frame_token, document_token);
}

void RenderProcessHostImpl::AddObserver(RenderProcessHostObserver* observer) {
  observers_.AddObserver(observer);
}

void RenderProcessHostImpl::RemoveObserver(
    RenderProcessHostObserver* observer) {
  observers_.RemoveObserver(observer);
}

void RenderProcessHostImpl::AddInternalObserver(
    RenderProcessHostInternalObserver* observer) {
  internal_observers_.AddObserver(observer);
}

void RenderProcessHostImpl::RemoveInternalObserver(
    RenderProcessHostInternalObserver* observer) {
  internal_observers_.RemoveObserver(observer);
}

void RenderProcessHostImpl::ShutdownForBadMessage(
    CrashReportMode crash_report_mode) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDisableKillAfterBadIPC))
    return;

  if (run_renderer_in_process()) {
    // In single process mode it is better if we don't suicide but just
    // crash.
    CHECK(false);
  }

  // We kill the renderer but don't include a NOTREACHED, because we want the
  // browser to try to survive when it gets illegal messages from the
  // renderer.
  Shutdown(RESULT_CODE_KILLED_BAD_MESSAGE);

  if (crash_report_mode == CrashReportMode::GENERATE_CRASH_DUMP) {
    // Set crash keys to understand renderer kills related to site isolation.
    ChildProcessSecurityPolicyImpl::GetInstance()->LogKilledProcessOriginLock(
        GetID());

    std::string site_isolation_mode;
    if (SiteIsolationPolicy::UseDedicatedProcessesForAllSites())
      site_isolation_mode += "spp ";
    if (SiteIsolationPolicy::AreIsolatedOriginsEnabled())
      site_isolation_mode += "io ";
    if (SiteIsolationPolicy::IsStrictOriginIsolationEnabled())
      site_isolation_mode += "soi ";
    if (site_isolation_mode.empty())
      site_isolation_mode = "(none)";

    SCOPED_CRASH_KEY_STRING32("RPH.BadMessageKill", "isolation_mode",
                              site_isolation_mode);

    ChildProcessSecurityPolicyImpl::GetInstance()->LogKilledProcessOriginLock(
        GetID());

    // Report a crash, since none will be generated by the killed renderer.
    base::debug::DumpWithoutCrashing();
  }
}

void RenderProcessHostImpl::UpdateClientPriority(
    RenderProcessHostPriorityClient* client) {
  DCHECK(client);
  DCHECK_EQ(1u, priority_clients_.count(client));
  UpdateProcessPriorityInputs();
}

int RenderProcessHostImpl::VisibleClientCount() {
  return visible_clients_;
}

unsigned int RenderProcessHostImpl::GetFrameDepth() {
  return frame_depth_;
}

bool RenderProcessHostImpl::GetIntersectsViewport() {
  return intersects_viewport_;
}

#if BUILDFLAG(IS_ANDROID)
ChildProcessImportance RenderProcessHostImpl::GetEffectiveImportance() {
  return effective_importance_;
}

base::android::ChildBindingState
RenderProcessHostImpl::GetEffectiveChildBindingState() {
  if (child_process_launcher_) {
    return child_process_launcher_->GetEffectiveChildBindingState();
  }

  // If there is no ChildProcessLauncher this is the best default.
  return base::android::ChildBindingState::UNBOUND;
}

void RenderProcessHostImpl::DumpProcessStack() {
  if (child_process_launcher_)
    child_process_launcher_->DumpProcessStack();
}
#endif

void RenderProcessHostImpl::OnMediaStreamAdded() {
  ++media_stream_count_;
  UpdateProcessPriority();
}

void RenderProcessHostImpl::OnMediaStreamRemoved() {
  DCHECK_GT(media_stream_count_, 0);
  --media_stream_count_;
  UpdateProcessPriority();
}

void RenderProcessHostImpl::OnForegroundServiceWorkerAdded() {
  foreground_service_worker_count_ += 1;
  UpdateProcessPriority();
}

void RenderProcessHostImpl::OnForegroundServiceWorkerRemoved() {
  DCHECK_GT(foreground_service_worker_count_, 0);
  foreground_service_worker_count_ -= 1;
  UpdateProcessPriority();
}

// static
void RenderProcessHostImpl::set_render_process_host_factory_for_testing(
    RenderProcessHostFactory* rph_factory) {
  g_render_process_host_factory_ = rph_factory;
}

// static
RenderProcessHostFactory*
RenderProcessHostImpl::get_render_process_host_factory_for_testing() {
  return g_render_process_host_factory_;
}

// static
void RenderProcessHostImpl::AddFrameWithSite(
    BrowserContext* browser_context,
    RenderProcessHost* render_process_host,
    const SiteInfo& site_info) {
  if (!ShouldTrackProcessForSite(site_info))
    return;

  SiteProcessCountTracker* tracker = SiteProcessCountTracker::GetInstance(
      browser_context, kCommittedSiteProcessCountTrackerKey);
  tracker->IncrementSiteProcessCount(site_info, render_process_host->GetID());

  MAYBEVLOG(2) << __func__ << "(" << site_info
               << "): Site added to process host "
               << render_process_host->GetID() << "." << std::endl
               << GetCurrentHostMapDebugString(tracker);
}

// static
void RenderProcessHostImpl::RemoveFrameWithSite(
    BrowserContext* browser_context,
    RenderProcessHost* render_process_host,
    const SiteInfo& site_info) {
  if (!ShouldTrackProcessForSite(site_info))
    return;

  SiteProcessCountTracker* tracker = SiteProcessCountTracker::GetInstance(
      browser_context, kCommittedSiteProcessCountTrackerKey);
  tracker->DecrementSiteProcessCount(site_info, render_process_host->GetID());
}

// static
void RenderProcessHostImpl::AddExpectedNavigationToSite(
    BrowserContext* browser_context,
    RenderProcessHost* render_process_host,
    const SiteInfo& site_info) {
  if (!ShouldTrackProcessForSite(site_info))
    return;

  SiteProcessCountTracker* tracker = SiteProcessCountTracker::GetInstance(
      browser_context, kPendingSiteProcessCountTrackerKey);
  tracker->IncrementSiteProcessCount(site_info, render_process_host->GetID());
}

// static
void RenderProcessHostImpl::RemoveExpectedNavigationToSite(
    BrowserContext* browser_context,
    RenderProcessHost* render_process_host,
    const SiteInfo& site_info) {
  if (!ShouldTrackProcessForSite(site_info))
    return;

  SiteProcessCountTracker* tracker = SiteProcessCountTracker::GetInstance(
      browser_context, kPendingSiteProcessCountTrackerKey);
  tracker->DecrementSiteProcessCount(site_info, render_process_host->GetID());
}

// static
void RenderProcessHostImpl::NotifySpareManagerAboutRecentlyUsedBrowserContext(
    BrowserContext* browser_context) {
  SpareRenderProcessHostManager::GetInstance().PrepareForFutureRequests(
      browser_context);
}

// static
RenderProcessHost* RenderProcessHost::GetSpareRenderProcessHostForTesting() {
  return SpareRenderProcessHostManager::GetInstance()
      .spare_render_process_host();
}

// static
base::CallbackListSubscription
RenderProcessHost::RegisterSpareRenderProcessHostChangedCallback(
    const base::RepeatingCallback<void(RenderProcessHost*)>& cb) {
  return SpareRenderProcessHostManager::GetInstance()
      .RegisterSpareRenderProcessHostChangedCallback(cb);
}

// static
void RenderProcessHostImpl::DiscardSpareRenderProcessHostForTesting() {
  SpareRenderProcessHostManager::GetInstance().CleanupSpareRenderProcessHost();
}

// static
bool RenderProcessHostImpl::IsSpareProcessKeptAtAllTimes() {
  if (!SiteIsolationPolicy::UseDedicatedProcessesForAllSites())
    return false;

  if (!base::FeatureList::IsEnabled(features::kSpareRendererForSitePerProcess))
    return false;

  // Spare renderer actually hurts performance on low-memory devices.  See
  // https://crbug.com/843775 for more details.
  //
  // The comparison below is using 1077 rather than 1024 because this helps
  // ensure that devices with exactly 1GB of RAM won't get included because of
  // inaccuracies or off-by-one errors.
  if (base::SysInfo::AmountOfPhysicalMemoryMB() <= 1077)
    return false;

  return true;
}

// static
bool RenderProcessHostImpl::IsSpareProcessForCrashReporting(
    RenderProcessHost* render_process_host) {
  return render_process_host == SpareRenderProcessHostManager::GetInstance()
                                    .spare_render_process_host();
}

// static
void RenderProcessHostImpl::ClearAllResourceCaches() {
  for (iterator iter(AllHostsIterator()); !iter.IsAtEnd(); iter.Advance()) {
    mojom::Renderer* renderer_interface =
        iter.GetCurrentValue()->GetRendererInterface();
    renderer_interface->PurgeResourceCache(base::DoNothing());
  }
}

bool RenderProcessHostImpl::HostHasNotBeenUsed() {
  return IsUnused() && listeners_.IsEmpty() && AreAllRefCountsZero() &&
         pending_views_ == 0;
}

void RenderProcessHostImpl::SetProcessLock(
    const IsolationContext& isolation_context,
    const ProcessLock& process_lock) {
  ChildProcessSecurityPolicyImpl::GetInstance()->LockProcess(
      isolation_context, GetID(), !IsUnused(), process_lock);

  // Note that SetProcessLock is only called on ProcessLock state transitions.
  // (e.g. invalid -> allows_any_site and allows_any_site -> locked_to_site).
  // Therefore, the call to NotifyRendererOfLockedStateUpdate below is
  // insufficient for setting up renderers respawned after crashing - this is
  // handled by another call to NotifyRendererOfLockedStateUpdate from
  // OnProcessLaunched.
  NotifyRendererOfLockedStateUpdate();
}

bool RenderProcessHostImpl::IsProcessLockedToSiteForTesting() {
  return GetProcessLock().is_locked_to_site();
}

void RenderProcessHostImpl::NotifyRendererOfLockedStateUpdate() {
  ProcessLock process_lock = GetProcessLock();

  if (process_lock.is_invalid())
    return;

  GetRendererInterface()->SetIsCrossOriginIsolated(
      process_lock.GetWebExposedIsolationInfo().is_isolated());

  bool is_isolated_context_allowed_by_embedder =
      GetContentClient()->browser()->IsIsolatedContextAllowedForUrl(
          GetBrowserContext(), process_lock.lock_url());

  GetRendererInterface()->SetIsIsolatedContext(
      process_lock.GetWebExposedIsolationInfo().is_isolated_application() ||
      is_isolated_context_allowed_by_embedder);

  if (!process_lock.IsASiteOrOrigin())
    return;

  CHECK(process_lock.is_locked_to_site());
  GetRendererInterface()->SetIsLockedToSite();
}

bool RenderProcessHostImpl::IsForGuestsOnly() {
  return !!(flags_ & RenderProcessFlags::kForGuestsOnly);
}

bool RenderProcessHostImpl::IsJitDisabled() {
  return !!(flags_ & RenderProcessFlags::kJitDisabled);
}

bool RenderProcessHostImpl::IsPdf() {
  return !!(flags_ & RenderProcessFlags::kPdf);
}

StoragePartitionImpl* RenderProcessHostImpl::GetStoragePartition() {
  // TODO(https://crbug.com/1382971): Remove the `CHECK` after the ad-hoc
  // debugging is no longer needed to investigate the bug.
  CHECK(!!storage_partition_impl_);

  return storage_partition_impl_.get();
}

static void AppendCompositorCommandLineFlags(base::CommandLine* command_line) {
  command_line->AppendSwitchASCII(
      cc::switches::kNumRasterThreads,
      base::NumberToString(NumberOfRendererRasterThreads()));

  int msaa_sample_count = GpuRasterizationMSAASampleCount();
  if (msaa_sample_count >= 0) {
    command_line->AppendSwitchASCII(
        blink::switches::kGpuRasterizationMSAASampleCount,
        base::NumberToString(msaa_sample_count));
  }

  if (IsZeroCopyUploadEnabled())
    command_line->AppendSwitch(blink::switches::kEnableZeroCopy);
  if (!IsPartialRasterEnabled())
    command_line->AppendSwitch(blink::switches::kDisablePartialRaster);

  if (IsGpuMemoryBufferCompositorResourcesEnabled()) {
    command_line->AppendSwitch(
        blink::switches::kEnableGpuMemoryBufferCompositorResources);
  }

  if (IsMainFrameBeforeActivationEnabled())
    command_line->AppendSwitch(cc::switches::kEnableMainFrameBeforeActivation);
}

void RenderProcessHostImpl::AppendRendererCommandLine(
    base::CommandLine* command_line) {
  // Pass the process type first, so it shows first in process listings.
  command_line->AppendSwitchASCII(switches::kProcessType,
                                  switches::kRendererProcess);

  // Call this as early as possible so that --extension-process will show early
  // in process listings. See https://crbug.com/1211558 for details.
  GetContentClient()->browser()->AppendExtraCommandLineSwitches(command_line,
                                                                GetID());

  static bool first_renderer_process = true;
  if (first_renderer_process) {
    command_line->AppendSwitch(kFirstRendererProcess);
    first_renderer_process = false;
  }

  if (IsPdf())
    command_line->AppendSwitch(switches::kPdfRenderer);

#if BUILDFLAG(IS_WIN)
  command_line->AppendArg(switches::kPrefetchArgumentRenderer);
#endif  // BUILDFLAG(IS_WIN)

  // Now send any options from our own command line we want to propagate.
  const base::CommandLine& browser_command_line =
      *base::CommandLine::ForCurrentProcess();
  PropagateBrowserCommandLineToRenderer(browser_command_line, command_line);

  // Pass on the browser locale.
  const std::string locale =
      GetContentClient()->browser()->GetApplicationLocale();
  command_line->AppendSwitchASCII(switches::kLang, locale);

  // A non-empty RendererCmdPrefix implies that Zygote is disabled.
  if (!base::CommandLine::ForCurrentProcess()
           ->GetSwitchValueNative(switches::kRendererCmdPrefix)
           .empty()) {
    command_line->AppendSwitch(switches::kNoZygote);
  }

  if (IsJitDisabled())
    command_line->AppendSwitchASCII(blink::switches::kJavaScriptFlags,
                                    "--jitless");

#if BUILDFLAG(IS_CHROMEOS)
  if (features::IsTouchTextEditingRedesignEnabled()) {
    command_line->AppendSwitchASCII(
        blink::switches::kTouchTextSelectionStrategy,
        blink::switches::kTouchTextSelectionStrategy_Direction);
  }
#endif

#if BUILDFLAG(IS_WIN)
  command_line->AppendSwitchASCII(
      switches::kDeviceScaleFactor,
      base::NumberToString(display::win::GetDPIScale()));
#endif

  AppendCompositorCommandLineFlags(command_line);

  command_line->AppendSwitchASCII(switches::kRendererClientId,
                                  std::to_string(GetID()));

  // Synchronize unix/monotonic clocks across consistent processes.
  if (base::TimeTicks::IsConsistentAcrossProcesses()) {
    command_line->AppendSwitchASCII(
        switches::kTimeTicksAtUnixEpoch,
        base::NumberToString(
            base::TimeTicks::UnixEpoch().since_origin().InMicroseconds()));
  }

#if BUILDFLAG(IS_LINUX)
  // Append `kDisableVideoCaptureUseGpuMemoryBuffer` flag if there is no support
  // for NV12 GPU memory buffer.
  if (switches::IsVideoCaptureUseGpuMemoryBufferEnabled() &&
      !GpuDataManagerImpl::GetInstance()->IsGpuMemoryBufferNV12Supported()) {
    command_line->AppendSwitch(
        switches::kDisableVideoCaptureUseGpuMemoryBuffer);
  }
#endif  // BUILDFLAG(IS_LINUX)
}

void RenderProcessHostImpl::PropagateBrowserCommandLineToRenderer(
    const base::CommandLine& browser_cmd,
    base::CommandLine* renderer_cmd) {
  // Propagate the following switches to the renderer command line (along
  // with any associated values) if present in the browser command line.
  static const char* const kSwitchNames[] = {
    switches::kDisableInProcessStackTraces,
    sandbox::policy::switches::kDisableSeccompFilterSandbox,
    sandbox::policy::switches::kNoSandbox,
#if BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS_ASH) && \
    !BUILDFLAG(IS_CHROMEOS_LACROS)
    switches::kDisableDevShmUsage,
#endif
#if BUILDFLAG(IS_MAC)
    // Allow this to be set when invoking the browser and relayed along.
    sandbox::policy::switches::kEnableSandboxLogging,
#endif
    switches::kAllowCommandLinePlugins,
    switches::kAllowLoopbackInPeerConnection,
    switches::kAudioBufferSize,
    switches::kAutoplayPolicy,
    switches::kMojoCoreLibraryPath,
    switches::kDisable2dCanvasImageChromium,
    switches::kDisableYUVImageDecoding,
    switches::kDisableAXMenuList,
    switches::kDisableAcceleratedVideoDecode,
    switches::kDisableBackForwardCache,
    switches::kDisableBackForwardCacheForCacheControlNoStorePage,
    switches::kDisableBackgroundTimerThrottling,
    switches::kDisableBestEffortTasks,
    switches::kDisableBreakpad,
    switches::kDisableDatabases,
    switches::kDisableFileSystem,
    switches::kDisableFrameRateLimit,
    switches::kDisableGpuMemoryBufferVideoFrames,
    switches::kDisableHistogramCustomizer,
    switches::kDisableLCDText,
    switches::kDisableLogging,
    switches::kDisableBackgroundMediaSuspend,
    switches::kDisableNotifications,
    switches::kDisableOriginTrialControlledBlinkFeatures,
    switches::kDisablePermissionsAPI,
    switches::kDisablePresentationAPI,
    switches::kDisableRTCSmoothnessAlgorithm,
    switches::kDisableScrollToTextFragment,
    switches::kDisableSharedWorkers,
    switches::kDisableSkiaRuntimeOpts,
    switches::kDisableSpeechAPI,
    switches::kDisableThreadedCompositing,
    switches::kDisableTouchDragDrop,
    switches::kDisableV8IdleTasks,
    switches::kDisableVideoCaptureUseGpuMemoryBuffer,
    switches::kDisableWebGLImageChromium,
    switches::kDomAutomationController,
    switches::kEnableAutomation,
    switches::kEnableBackgroundThreadPool,
    switches::kEnableExperimentalAccessibilityLanguageDetection,
    switches::kEnableExperimentalAccessibilityLabelsDebugging,
    switches::kEnableExperimentalWebPlatformFeatures,
    switches::kEnableBlinkTestFeatures,
    switches::kEnableGPUClientLogging,
    switches::kEnableGpuClientTracing,
    switches::kEnableGpuMemoryBufferVideoFrames,
    switches::kEnableGPUServiceLogging,
    switches::kEnableLCDText,
    switches::kEnableLogging,
    switches::kEnableNetworkInformationDownlinkMax,
    switches::kEnablePluginPlaceholderTesting,
    switches::kEnablePreciseMemoryInfo,
    switches::kEnableSkiaBenchmarking,
    switches::kEnableTouchDragDrop,
    switches::kEnableUnsafeWebGPU,
    switches::kEnableViewport,
    switches::kEnableVtune,
    switches::kEnableWebGLDeveloperExtensions,
    switches::kEnableWebGLDraftExtensions,
    switches::kEnableWebGLImageChromium,
    switches::kEnableWebGPUDeveloperFeatures,
    switches::kFileUrlPathAlias,
    switches::kForceDeviceScaleFactor,
    switches::kForceDisplayColorProfile,
    switches::kForceGpuMemAvailableMb,
    switches::kForceHighContrast,
    switches::kForceRasterColorProfile,
    switches::kForceVideoOverlays,
    switches::kFullMemoryCrashReport,
    switches::kGaiaUrl,
    switches::kIPCConnectionTimeout,
    switches::kLogBestEffortTasks,
    switches::kLogFile,
    switches::kLoggingLevel,
    switches::kMaxActiveWebGLContexts,
    switches::kMaxDecodedImageSizeMb,
    switches::kMaxWebMediaPlayerCount,
    switches::kMSEAudioBufferSizeLimitMb,
    switches::kMSEVideoBufferSizeLimitMb,
    switches::kNoZygote,
    switches::kOverrideLanguageDetection,
    switches::kPerfettoDisableInterning,
    switches::kPpapiInProcess,
    switches::kProfilingAtStart,
    switches::kProfilingFile,
    switches::kProfilingFlush,
    switches::kRegisterPepperPlugins,
    switches::kRemoteDebuggingPipe,
    switches::kRemoteDebuggingPort,
    switches::kRendererStartupDialog,
    switches::kReportVp9AsAnUnsupportedMimeType,
    switches::kStatsCollectionController,
    switches::kSkiaFontCacheLimitMb,
    switches::kSkiaResourceCacheLimitMb,
    switches::kTestType,
    switches::kTimeZoneForTesting,
    switches::kTouchEventFeatureDetection,
    switches::kTraceToConsole,
    switches::kUseFakeCodecForPeerConnection,
    switches::kUseFakeUIForMediaStream,
    switches::kUseMobileUserAgent,
    switches::kV,
    switches::kVideoCaptureUseGpuMemoryBuffer,
    switches::kVideoThreads,
    switches::kVideoUnderflowThresholdMs,
    switches::kVModule,
    switches::kWaitForDebuggerOnNavigation,
    switches::kWebAuthRemoteDesktopSupport,
    switches::kWebViewDrawFunctorUsesVulkan,
    switches::kWebglAntialiasingMode,
    switches::kWebglMSAASampleCount,
    // Please keep these in alphabetical order.
    blink::switches::kAllowPreCommitInput,
    blink::switches::kBlinkSettings,
    blink::switches::kDarkModeSettings,
    blink::switches::kDefaultTileWidth,
    blink::switches::kDefaultTileHeight,
    blink::switches::kForcePermissionPolicyUnloadDefaultEnabled,
    blink::switches::kDisableImageAnimationResync,
    blink::switches::kDisableLowResTiling,
    blink::switches::kDisableNewBaseUrlInheritanceBehavior,
    blink::switches::kDisablePreferCompositingToLCDText,
    blink::switches::kDisableRGBA4444Textures,
    blink::switches::kDisableThreadedScrolling,
    blink::switches::kDisableThrottleNonVisibleCrossOriginIframes,
    blink::switches::kEnableLowResTiling,
    blink::switches::kEnablePreferCompositingToLCDText,
    blink::switches::kEnableRGBA4444Textures,
    blink::switches::kEnableRasterSideDarkModeForImages,
    blink::switches::kMinHeightForGpuRasterTile,
    blink::switches::kMaxUntiledLayerWidth,
    blink::switches::kMaxUntiledLayerHeight,
    blink::switches::kNetworkQuietTimeout,
    blink::switches::kShowLayoutShiftRegions,
    blink::switches::kShowPaintRects,
    blink::switches::kTouchTextSelectionStrategy,
    blink::switches::kJavaScriptFlags,
    // Please keep these in alphabetical order. Compositor switches here
    // should also be added to
    // chrome/browser/ash/login/chrome_restart_request.cc.
    cc::switches::kCCScrollAnimationDurationForTesting,
    cc::switches::kCheckDamageEarly,
    cc::switches::kDisableCheckerImaging,
    cc::switches::kDisableCompositedAntialiasing,
    cc::switches::kDisableThreadedAnimation,
    cc::switches::kEnableGpuBenchmarking,
    cc::switches::kEnableClippedImageScaling,
    cc::switches::kHighlightNonLCDTextLayers,
    cc::switches::kShowCompositedLayerBorders,
    cc::switches::kShowFPSCounter,
    cc::switches::kShowLayerAnimationBounds,
    cc::switches::kShowPropertyChangedRects,
    cc::switches::kShowScreenSpaceRects,
    cc::switches::kShowSurfaceDamageRects,
    cc::switches::kSlowDownRasterScaleFactor,
    cc::switches::kBrowserControlsHideThreshold,
    cc::switches::kBrowserControlsShowThreshold,
    switches::kRunAllCompositorStagesBeforeDraw,

#if BUILDFLAG(ENABLE_PPAPI)
    switches::kEnablePepperTesting,
#endif
    switches::kDisableWebRtcHWDecoding,
    switches::kDisableWebRtcHWEncoding,
    switches::kEnableWebRtcSrtpEncryptedHeaders,
    switches::kEnforceWebRtcIPPermissionCheck,
    switches::kWebRtcMaxCaptureFramerate,
    switches::kEnableLowEndDeviceMode,
    switches::kDisableLowEndDeviceMode,
    switches::kDisallowNonExactResourceReuse,
#if BUILDFLAG(IS_ANDROID)
    switches::kDisableMediaSessionAPI,
    switches::kEnableReachedCodeProfiler,
    switches::kReachedCodeSamplingIntervalUs,
    switches::kRendererWaitForJavaDebugger,
#endif
#if BUILDFLAG(IS_WIN)
    switches::kDisableHighResTimer,
    switches::kTrySupportedChannelLayouts,
    switches::kRaiseTimerFrequency,
#endif
#if BUILDFLAG(IS_OZONE)
    switches::kOzonePlatform,
#endif
#if defined(ENABLE_IPC_FUZZER)
    switches::kIpcDumpDirectory,
    switches::kIpcFuzzerTestcase,
#endif
#if BUILDFLAG(IS_CHROMEOS)
    switches::kSchedulerBoostUrgent,
#endif
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    switches::kLacrosEnablePlatformHevc,
    switches::kLacrosUseChromeosProtectedMedia,
    switches::kLacrosUseChromeosProtectedAv1,
#endif
  };
  renderer_cmd->CopySwitchesFrom(browser_cmd, kSwitchNames);

  // |switches::kGaiaConfig| can be set via browser command-line arguments,
  // usually by developers working on signin code. The switch, however, cannot
  // be passed as is, because the renderer cannot read the file anyway. In those
  // cases (and only in those cases) GaiaConfig::GetInstance() returns non-null,
  // and it can be used to serialize its content (usually <2 KB) into a
  // command-line switch.
  if (GaiaConfig::GetInstance()) {
    GaiaConfig::GetInstance()->SerializeContentsToCommandLineSwitch(
        renderer_cmd);
  }
  BrowserChildProcessHostImpl::CopyTraceStartupFlags(renderer_cmd);

  // Disable databases in incognito mode.
  if (GetBrowserContext()->IsOffTheRecord() &&
      !browser_cmd.HasSwitch(switches::kDisableDatabases)) {
    renderer_cmd->AppendSwitch(switches::kDisableDatabases);
  }

#if BUILDFLAG(IS_ANDROID)
  if (browser_cmd.HasSwitch(switches::kDisableGpuCompositing)) {
    renderer_cmd->AppendSwitch(switches::kDisableGpuCompositing);
  }
#elif !BUILDFLAG(IS_CHROMEOS_ASH)
  // If gpu compositing is not being used, tell the renderer at startup. This
  // is inherently racey, as it may change while the renderer is being
  // launched, but the renderer will hear about the correct state eventually.
  // This optimizes the common case to avoid wasted work.
  if (GpuDataManagerImpl::GetInstance()->IsGpuCompositingDisabled())
    renderer_cmd->AppendSwitch(switches::kDisableGpuCompositing);
#endif  // BUILDFLAG(IS_ANDROID)

  // Add kWaitForDebugger to let renderer process wait for a debugger.
  if (browser_cmd.HasSwitch(switches::kWaitForDebuggerChildren)) {
    // Look to pass-on the kWaitForDebugger flag.
    std::string value =
        browser_cmd.GetSwitchValueASCII(switches::kWaitForDebuggerChildren);
    if (value.empty() || value == switches::kRendererProcess) {
      renderer_cmd->AppendSwitch(switches::kWaitForDebugger);
    }
  }

#if BUILDFLAG(IS_WIN) && !defined(OFFICIAL_BUILD)
  // Needed because we can't show the dialog from the sandbox. Don't pass
  // --no-sandbox in official builds because that would bypass the bad_flgs
  // prompt.
  if (renderer_cmd->HasSwitch(switches::kRendererStartupDialog) &&
      !renderer_cmd->HasSwitch(sandbox::policy::switches::kNoSandbox)) {
    renderer_cmd->AppendSwitch(sandbox::policy::switches::kNoSandbox);
  }
#endif

  CopyFeatureSwitch(browser_cmd, renderer_cmd, switches::kEnableBlinkFeatures);
  CopyFeatureSwitch(browser_cmd, renderer_cmd, switches::kDisableBlinkFeatures);

#if BUILDFLAG(IS_WIN)
  if (media::IsMediaFoundationD3D11VideoCaptureEnabled()) {
    renderer_cmd->AppendSwitch(switches::kVideoCaptureUseGpuMemoryBuffer);
  }
#endif
}

const base::Process& RenderProcessHostImpl::GetProcess() {
  if (run_renderer_in_process()) {
    // This is a sentinel object used for this process in single process mode.
    static const base::NoDestructor<base::Process> self(
        base::Process::Current());
    return *self;
  }

  if (!child_process_launcher_.get() || child_process_launcher_->IsStarting()) {
    // This is a sentinel for "no process".
    static const base::NoDestructor<base::Process> null_process;
    return *null_process;
  }

  return child_process_launcher_->GetProcess();
}

bool RenderProcessHostImpl::IsReady() {
  // The process launch result (that sets GetHandle()) and the channel
  // connection (that sets channel_connected_) can happen in either order.
  return GetProcess().Handle() && channel_connected_;
}

bool RenderProcessHostImpl::Shutdown(int exit_code) {
  if (run_renderer_in_process())
    return false;  // Single process mode never shuts down the renderer.

  if (!child_process_launcher_.get())
    return false;

  shutdown_exit_code_ = exit_code;
  shutdown_requested_ = true;
  return child_process_launcher_->Terminate(exit_code);
}

bool RenderProcessHostImpl::ShutdownRequested() {
  return shutdown_requested_;
}

bool RenderProcessHostImpl::FastShutdownIfPossible(size_t page_count,
                                                   bool skip_unload_handlers) {
  base::UmaHistogramBoolean(
      "BrowserRenderProcessHost.FastShutdownIfPossible.Total", true);
  // Do not shut down the process if there are active or pending views other
  // than the ones we're shutting down.
  if (page_count && page_count != (GetActiveViewCount() + pending_views_)) {
    LogDelayReasonForFastShutdown(
        DelayShutdownReason::kOtherActiveOrPendingViews);
    return false;
  }

  if (run_renderer_in_process()) {
    LogDelayReasonForFastShutdown(DelayShutdownReason::kSingleProcess);
    return false;  // Single process mode never shuts down the renderer.
  }

  if (!child_process_launcher_.get()) {
    LogDelayReasonForFastShutdown(DelayShutdownReason::kNoProcess);
    return false;  // Render process hasn't started or is probably crashed.
  }

  // Test if there's an unload listener.
  // NOTE: It's possible that an onunload listener may be installed
  // while we're shutting down, so there's a small race here.  Given that
  // the window is small, it's unlikely that the web page has much
  // state that will be lost by not calling its unload handlers properly.
  if (!skip_unload_handlers && !SuddenTerminationAllowed()) {
    LogDelayReasonForFastShutdown(DelayShutdownReason::kUnload);
    return false;
  }

  // TODO(crbug.com/1356128): Remove this block once the migration is launched.
  if (keep_alive_ref_count_ != 0) {
    CHECK(!base::FeatureList::IsEnabled(
        blink::features::kKeepAliveInBrowserMigration));
    LogDelayReasonForFastShutdown(DelayShutdownReason::kFetchKeepAlive);
    return false;
  }

  if (worker_ref_count_ != 0) {
    LogDelayReasonForFastShutdown(DelayShutdownReason::kWorker);
    return false;
  }

  if (pending_reuse_ref_count_ != 0) {
    LogDelayReasonForFastShutdown(DelayShutdownReason::kPendingReuse);
    return false;
  }

  // TODO(wjmaclean): This is probably unnecessary, but let's remove it in a
  // separate CL to be safe.
  if (shutdown_delay_ref_count_ != 0) {
    LogDelayReasonForFastShutdown(DelayShutdownReason::kShutdownDelay);
    return false;
  }

  // Set this before ProcessDied() so observers can tell if the render process
  // died due to fast shutdown versus another cause.
  fast_shutdown_started_ = true;

  ChildProcessTerminationInfo info =
      GetChildTerminationInfo(false /* already_dead */);
  ProcessDied(info);
  LogDelayReasonForFastShutdown(DelayShutdownReason::kNoDelay);
  return true;
}

bool RenderProcessHostImpl::Send(IPC::Message* msg) {
  TRACE_IPC_MESSAGE_SEND("renderer_host", "RenderProcessHostImpl::Send", msg);

  std::unique_ptr<IPC::Message> message(msg);

  // |channel_| is only null after Cleanup(), at which point we don't care
  // about delivering any messages.
  if (!channel_)
    return false;

  DCHECK(!message->is_sync());

  // Allow tests to watch IPCs sent to the renderer.
  if (ipc_send_watcher_for_testing_)
    ipc_send_watcher_for_testing_.Run(*message);

  return channel_->Send(message.release());
}

bool RenderProcessHostImpl::OnMessageReceived(const IPC::Message& msg) {
  // If we're about to be deleted, or have initiated the fast shutdown
  // sequence, we ignore incoming messages.

  if (deleting_soon_ || fast_shutdown_started_)
    return false;

  mark_child_process_activity_time();

  // Dispatch incoming messages to the appropriate IPC::Listener.
  IPC::Listener* listener = listeners_.Lookup(msg.routing_id());
  if (!listener) {
    if (msg.is_sync()) {
      // The listener has gone away, so we must respond or else the caller
      // will hang waiting for a reply.
      IPC::Message* reply = IPC::SyncMessage::GenerateReply(&msg);
      reply->set_reply_error();
      Send(reply);
    }
    return true;
  }
  return listener->OnMessageReceived(msg);
}

void RenderProcessHostImpl::OnAssociatedInterfaceRequest(
    const std::string& interface_name,
    mojo::ScopedInterfaceEndpointHandle handle) {
  if (associated_interfaces_ &&
      !associated_interfaces_->TryBindInterface(interface_name, &handle)) {
    LOG(ERROR) << "Request for unknown Channel-associated interface: "
               << interface_name;
  }
}

void RenderProcessHostImpl::OnChannelConnected(int32_t peer_pid) {
  channel_connected_ = true;

#if BUILDFLAG(IS_MAC)
  ChildProcessTaskPortProvider::GetInstance()->OnChildProcessLaunched(
      peer_pid, child_process_.get());
#endif

  if (IsReady()) {
    DCHECK(!sent_render_process_ready_);
    sent_render_process_ready_ = true;
    // Send RenderProcessReady only if we already received the process handle.
    for (auto& observer : observers_)
      observer.RenderProcessReady(this);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    // Provide /proc/{renderer pid}/status and statm files for
    // MemoryUsageMonitor in blink.
    ProvideStatusFileForRenderer();
#endif

    ProvideSwapFileForRenderer();
  }

#if BUILDFLAG(IPC_MESSAGE_LOG_ENABLED)
  child_process_->SetIPCLoggingEnabled(IPC::Logging::GetInstance()->Enabled());
#endif

#if BUILDFLAG(CLANG_PROFILING_INSIDE_SANDBOX)
  child_process_->SetProfilingFile(OpenProfilingFile());
#endif

  // Propagate the pseudonymization salt to all the child processes.
  //
  // TODO(dullweber, lukasza): Figure out if it is possible to reset the salt
  // at a regular interval (on the order of hours?).  The browser would need to
  // be responsible for 1) deciding when the refresh happens and 2) pushing the
  // updated salt to all the child processes.
  child_process_->SetPseudonymizationSalt(GetPseudonymizationSalt());
}

void RenderProcessHostImpl::OnChannelError() {
  ChildProcessTerminationInfo info =
      GetChildTerminationInfo(true /* already_dead */);
  ProcessDied(info);
}

void RenderProcessHostImpl::OnBadMessageReceived(const IPC::Message& message) {
  // Message de-serialization failed. We consider this a capital crime. Kill
  // the renderer if we have one.
  auto type = message.type();
  LOG(ERROR) << "bad message " << type << " terminating renderer.";

  // The ReceivedBadMessage call below will trigger a DumpWithoutCrashing.
  // Alias enough information here so that we can determine what the bad
  // message was.
  base::debug::Alias(&type);

  bad_message::ReceivedBadMessage(this,
                                  bad_message::RPH_DESERIALIZATION_FAILED);
}

BrowserContext* RenderProcessHostImpl::GetBrowserContext() {
  return browser_context_;
}

bool RenderProcessHostImpl::InSameStoragePartition(
    StoragePartition* partition) {
  return GetStoragePartition() == partition;
}

int RenderProcessHostImpl::GetID() const {
  return id_;
}

base::SafeRef<RenderProcessHost> RenderProcessHostImpl::GetSafeRef() const {
  return safe_ref_factory_.GetSafeRef();
}

bool RenderProcessHostImpl::IsInitializedAndNotDead() {
  return is_initialized_ && !is_dead_;
}

void RenderProcessHostImpl::SetBlocked(bool blocked) {
  if (blocked == is_blocked_)
    return;

  is_blocked_ = blocked;
  blocked_state_changed_callback_list_.Notify(blocked);
}

bool RenderProcessHostImpl::IsBlocked() {
  return is_blocked_;
}

void RenderProcessHostImpl::PauseSocketManagerForRenderFrameHost(
    const GlobalRenderFrameHostId& render_frame_host_id) {
  p2p_socket_dispatcher_host_->PauseSocketManagerForRenderFrameHost(
      render_frame_host_id);
}

void RenderProcessHostImpl::ResumeSocketManagerForRenderFrameHost(
    const GlobalRenderFrameHostId& render_frame_host_id) {
  p2p_socket_dispatcher_host_->ResumeSocketManagerForRenderFrameHost(
      render_frame_host_id);
}

base::CallbackListSubscription
RenderProcessHostImpl::RegisterBlockStateChangedCallback(
    const BlockStateChangedCallback& cb) {
  return blocked_state_changed_callback_list_.Add(cb);
}

bool RenderProcessHostImpl::HasOnlyNonLiveRenderFrameHosts() {
  if (GetRenderFrameHostCount() == 0)
    return false;

  // Iterate over the RenderFrameHosts in this process. While listeners_ may
  // also contain RenderViewHosts and RenderFrameProxyHosts, those on their own
  // do not need to keep a process alive.
  int found_rfh_count = 0;
  for (auto rfh_id : render_frame_host_id_set_) {
    // Note that some RenderFrameHosts in the set may not be found by FromID if
    // we get here during their destructor (e.g., while deleting their subframe
    // RenderFrameHosts).
    if (RenderFrameHostImpl* rfh = RenderFrameHostImpl::FromID(rfh_id)) {
      found_rfh_count++;
      if (rfh->IsRenderFrameLive())
        return false;

      // If this process contains a frame from an inner WebContents, skip the
      // process leak cleanup for now. Inner WebContents attachment can break
      // if the process it starts with goes away before it attaches.
      // TODO(https://crbug.com/1295431): Remove in favor of tracking pending
      // guest initializations instead.
      if (rfh->delegate()->IsInnerWebContentsForGuest())
        return false;
    }
  }

  // If we didn't find all the known RenderFrameHosts via FromID (because some
  // are in their destructor), consider them live until they finish destructing.
  if (found_rfh_count < GetRenderFrameHostCount())
    return false;

  // We should never find more than render_frame_host_count_.
  DCHECK_EQ(GetRenderFrameHostCount(), found_rfh_count);

  // We accounted for all the RenderFrameHosts (at least one), and none were
  // live.
  return true;
}

void RenderProcessHostImpl::Cleanup() {
  TRACE_EVENT("shutdown", "RenderProcessHostImpl::Cleanup",
              ChromeTrackEvent::kRenderProcessHost, *this);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::UmaHistogramBoolean("BrowserRenderProcessHost.Cleanup.Total", true);
  // Keep the one renderer thread around forever in single process mode.
  if (run_renderer_in_process()) {
    LogDelayReasonForCleanup(DelayShutdownReason::kSingleProcess);
    return;
  }

  // If within_process_died_observer_ is true, one of our observers performed
  // an action that caused us to die (e.g. http://crbug.com/339504).
  // Therefore, delay the destruction until all of the observer callbacks have
  // been made, and guarantee that the RenderProcessHostDestroyed observer
  // callback is always the last callback fired.
  if (within_process_died_observer_) {
    TRACE_EVENT("shutdown",
                "RenderProcessHostImpl::Cleanup : within_process_died_observer",
                ChromeTrackEvent::kRenderProcessHost, *this);
    delayed_cleanup_needed_ = true;
    LogDelayReasonForCleanup(DelayShutdownReason::kObserver);
    return;
  }
  delayed_cleanup_needed_ = false;

  // Check whether there are only non-live RenderFrameHosts remaining (with at
  // least one). If so, it is safe for the process to exit even if we keep the
  // RenderProcessHost around for non-live listeners_. Allow embedders to skip
  // this check (e.g., Android WebView does not yet cleanly handle this exit for
  // its sole RenderProcessHost).
  // TODO(crbug.com/1268454): Support this on Android WebView as well and remove
  // the ContentBrowserClient method.
  bool has_only_non_live_rfhs =
      GetContentClient()->browser()->ShouldAllowNoLongerUsedProcessToExit() &&
      HasOnlyNonLiveRenderFrameHosts();

  // Until there are no other owners of this object, we can't delete
  // ourselves. Note that it may be safe for the renderer process to exit even
  // if some non-live listeners remain, though they still depend on this
  // RenderProcessHost object.
  if (!listeners_.IsEmpty() && !has_only_non_live_rfhs) {
    TRACE_EVENT(
        "shutdown", "RenderProcessHostImpl::Cleanup : Has listeners.",
        ChromeTrackEvent::kRenderProcessHost, *this,
        [&](perfetto::EventContext ctx) {
          auto* proto =
              ctx.event<ChromeTrackEvent>()->set_render_process_host_cleanup();
          proto->set_listener_count(listeners_.size());
        });
    LogDelayReasonForCleanup(DelayShutdownReason::kListener);
    return;
  } else if (keep_alive_ref_count_ != 0) {
    CHECK(!base::FeatureList::IsEnabled(
        blink::features::kKeepAliveInBrowserMigration));
    TRACE_EVENT(
        "shutdown", "RenderProcessHostImpl::Cleanup : Have keep_alive_ref.",
        ChromeTrackEvent::kRenderProcessHost, *this,
        [&](perfetto::EventContext ctx) {
          auto* proto =
              ctx.event<ChromeTrackEvent>()->set_render_process_host_cleanup();
          proto->set_keep_alive_ref_count(keep_alive_ref_count_);
        });
    LogDelayReasonForCleanup(DelayShutdownReason::kFetchKeepAlive);
    return;
  } else if (shutdown_delay_ref_count_ != 0) {
    TRACE_EVENT(
        "shutdown", "RenderProcessHostImpl::Cleanup : Have shutdown_delay_ref.",
        ChromeTrackEvent::kRenderProcessHost, *this,
        [&](perfetto::EventContext ctx) {
          auto* proto =
              ctx.event<ChromeTrackEvent>()->set_render_process_host_cleanup();
          proto->set_shutdown_delay_ref_count(shutdown_delay_ref_count_);
        });
    LogDelayReasonForCleanup(DelayShutdownReason::kShutdownDelay);
    return;
  } else if (worker_ref_count_ != 0) {
    TRACE_EVENT(
        "shutdown", "RenderProcessHostImpl::Cleanup : Have worker_ref.",
        ChromeTrackEvent::kRenderProcessHost, *this,
        [&](perfetto::EventContext ctx) {
          auto* proto =
              ctx.event<ChromeTrackEvent>()->set_render_process_host_cleanup();
          proto->set_worker_ref_count(worker_ref_count_);
        });
    LogDelayReasonForCleanup(DelayShutdownReason::kWorker);
    return;
  } else if (pending_reuse_ref_count_ != 0) {
    TRACE_EVENT(
        "shutdown", "RenderProcessHostImpl::Cleanup : Have pending_reuse_ref.",
        ChromeTrackEvent::kRenderProcessHost, *this,
        [&](perfetto::EventContext ctx) {
          auto* proto =
              ctx.event<ChromeTrackEvent>()->set_render_process_host_cleanup();
          proto->set_pending_reuse_ref_count(pending_reuse_ref_count_);
        });
    LogDelayReasonForCleanup(DelayShutdownReason::kPendingReuse);
    return;
  }

  LogDelayReasonForCleanup(DelayShutdownReason::kNoDelay);

  // If there are listeners but they do not include any live RenderFrameHosts
  // (and there aren't other reasons to keep the process around), then it is
  // safe for the process to cleanly exit but not for the RenderProcessHost to
  // be deleted.
  if (has_only_non_live_rfhs) {
    DCHECK(!listeners_.IsEmpty());

    // No need to terminate the renderer if it is already gone.
    if (!IsInitializedAndNotDead())
      return;

    TRACE_EVENT("shutdown",
                "RenderProcessHostImpl::Cleanup : Exit without full cleanup.",
                ChromeTrackEvent::kRenderProcessHost, *this);

    // Notify all observers that the process has exited cleanly, even though it
    // will be destroyed a bit later. Observers shouldn't rely on this process
    // anymore.
    // Populates Android-only fields and closes the underlying base::Process.
    ChildProcessTerminationInfo info =
        GetChildTerminationInfo(false /* already_dead */);
    info.status = base::TERMINATION_STATUS_NORMAL_TERMINATION;
    info.exit_code = 0;

    // Set this before ProcessDied() so observers treat this process exit as
    // similar to the fast shutdown case.
    fast_shutdown_started_ = true;

    // Notify observers using the clean exit info.
    ProcessDied(info);
    return;
  }

  TRACE_EVENT("shutdown", "RenderProcessHostImpl::Cleanup : Starting cleanup.",
              ChromeTrackEvent::kRenderProcessHost, *this);
  TRACE_EVENT_BEGIN("shutdown", "Cleanup in progress",
                    perfetto::Track::FromPointer(this),
                    ChromeTrackEvent::kRenderProcessHost, *this);

  // We cannot delete `this` twice; if this fails, there is an issue with our
  // control flow.
  //
  // TODO(crbug.com/1200340): Revert this to a DCHECK after some investigation.
  CHECK(!deleting_soon_);

  // There are no `return` statements anywhere below - at this point we have
  // made a decision to destroy `this` `RenderProcessHostImpl` object and we
  // *will* post a `DeleteSoon` task a bit further down.
  deleting_soon_ = true;

  if (is_initialized_) {
    GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&WebRtcLog::ClearLogMessageCallback, GetID()));
  }

  DCHECK_EQ(0, pending_views_);

  // If the process associated with this RenderProcessHost is still alive,
  // notify all observers that the process has exited cleanly, even though it
  // will be destroyed a bit later. Observers shouldn't rely on this process
  // anymore.
  if (IsInitializedAndNotDead()) {
    // Populates Android-only fields and closes the underlying base::Process.
    ChildProcessTerminationInfo info = GetChildTerminationInfo(false);
    info.status = base::TERMINATION_STATUS_NORMAL_TERMINATION;
    info.exit_code = 0;
    for (auto& observer : observers_) {
      observer.RenderProcessExited(this, info);
    }
  }
  for (auto& observer : observers_)
    observer.RenderProcessHostDestroyed(this);
  NotificationService::current()->Notify(
      NOTIFICATION_RENDERER_PROCESS_TERMINATED, Source<RenderProcessHost>(this),
      NotificationService::NoDetails());

  RecentlyDestroyedHosts::Add(this, time_spent_running_unload_handlers_,
                              browser_context_);
  // Remove this host from the delayed-shutdown tracker if present, as the
  // shutdown delay has now been cancelled.
  StopTrackingProcessForShutdownDelay();

  // Use `DeleteSoon` to delete `this` RenderProcessHost *after* the tasks
  // that are *already* queued on the UI thread have been given a chance to run
  // (this may include IPC handling tasks that depend on the existence of
  // RenderProcessHost and/or ChildProcessSecurityPolicyImpl::SecurityState).
#ifndef NDEBUG
  is_self_deleted_ = true;
#endif
  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                                this);
  // Destroy all mojo bindings and IPC channels that can cause calls to this
  // object, to avoid method invocations that trigger usages of profile.
  ResetIPC();

  // Remove ourself from the list of renderer processes so that we can't be
  // reused in between now and when the Delete task runs.
  UnregisterHost(GetID());
  browser_context_ = nullptr;
  storage_partition_impl_ = nullptr;
}

#if BUILDFLAG(IS_ANDROID)
void RenderProcessHostImpl::PopulateTerminationInfoRendererFields(
    ChildProcessTerminationInfo* info) {
  info->renderer_has_visible_clients = VisibleClientCount() > 0;
  info->renderer_was_subframe = GetFrameDepth() > 0;
}
#endif  // BUILDFLAG(IS_ANDROID)

void RenderProcessHostImpl::AddPendingView() {
  const bool had_pending_views = pending_views_++;
  if (!had_pending_views)
    UpdateProcessPriority();
}

void RenderProcessHostImpl::RemovePendingView() {
  DCHECK(pending_views_);
  --pending_views_;
  if (!pending_views_)
    UpdateProcessPriority();
}

void RenderProcessHostImpl::AddPriorityClient(
    RenderProcessHostPriorityClient* priority_client) {
  DCHECK(!base::Contains(priority_clients_, priority_client));
  priority_clients_.insert(priority_client);
  UpdateProcessPriorityInputs();
}

void RenderProcessHostImpl::RemovePriorityClient(
    RenderProcessHostPriorityClient* priority_client) {
  DCHECK(base::Contains(priority_clients_, priority_client));
  priority_clients_.erase(priority_client);
  UpdateProcessPriorityInputs();
}

void RenderProcessHostImpl::SetPriorityOverride(bool foregrounded) {
  priority_override_ = foregrounded;
  UpdateProcessPriority();
}

bool RenderProcessHostImpl::HasPriorityOverride() {
  return priority_override_.has_value();
}

void RenderProcessHostImpl::ClearPriorityOverride() {
  priority_override_.reset();
  UpdateProcessPriority();
}

void RenderProcessHostImpl::SetSuddenTerminationAllowed(bool enabled) {
  sudden_termination_allowed_ = enabled;
}

bool RenderProcessHostImpl::SuddenTerminationAllowed() {
  return sudden_termination_allowed_;
}

base::TimeDelta RenderProcessHostImpl::GetChildProcessIdleTime() {
  return base::TimeTicks::Now() - child_process_activity_time_;
}

RenderProcessHost::FilterURLResult RenderProcessHostImpl::FilterURL(
    bool empty_allowed,
    GURL* url) {
  return FilterURL(this, empty_allowed, url);
}

void RenderProcessHostImpl::EnableAudioDebugRecordings(
    const base::FilePath& file_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  aec_dump_manager_.Start(file_path);
}

void RenderProcessHostImpl::DisableAudioDebugRecordings() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  aec_dump_manager_.Stop();
}

RenderProcessHostImpl::WebRtcStopRtpDumpCallback
RenderProcessHostImpl::StartRtpDump(bool incoming,
                                    bool outgoing,
                                    WebRtcRtpPacketCallback packet_callback) {
  p2p_socket_dispatcher_host_->StartRtpDump(incoming, outgoing,
                                            std::move(packet_callback));

  return base::BindOnce(&P2PSocketDispatcherHost::StopRtpDump,
                        p2p_socket_dispatcher_host_->GetWeakPtr());
}

IPC::ChannelProxy* RenderProcessHostImpl::GetChannel() {
  return channel_.get();
}

void RenderProcessHostImpl::AddFilter(BrowserMessageFilter* filter) {
  filter->RegisterAssociatedInterfaces(channel_.get());
  channel_->AddFilter(filter->GetFilter());
}

bool RenderProcessHostImpl::FastShutdownStarted() {
  return fast_shutdown_started_;
}

// static
void RenderProcessHostImpl::RegisterHost(int host_id, RenderProcessHost* host) {
  TRACE_EVENT(
      "shutdown", "RenderProcessHostImpl::RegisterHost",
      [&](perfetto::EventContext ctx) {
        ctx.event<ChromeTrackEvent>()->set_render_process_host()->set_id(
            host_id);
      });
  GetAllHosts().AddWithID(host, host_id);
}

// static
void RenderProcessHostImpl::UnregisterHost(int host_id) {
  RenderProcessHost* host = GetAllHosts().Lookup(host_id);
  if (!host)
    return;
  TRACE_EVENT(
      "shutdown", "RenderProcessHostImpl::UnregisterHost",
      [&](perfetto::EventContext ctx) {
        ctx.event<ChromeTrackEvent>()->set_render_process_host()->set_id(
            host_id);
      });

  GetAllHosts().Remove(host_id);

  // Log after updating the GetAllHosts() list but before deleting the host.
  MAYBEVLOG(3) << __func__ << "(" << host_id << ")" << std::endl
               << GetCurrentHostMapDebugString(
                      static_cast<SiteProcessCountTracker*>(
                          host->GetBrowserContext()->GetUserData(
                              kCommittedSiteProcessCountTrackerKey)));

  // Look up the map of site to process for the given browser_context,
  // in case we need to remove this process from it.  It will be registered
  // under any sites it rendered that use process-per-site mode.
  SiteProcessMap* map =
      GetSiteProcessMapForBrowserContext(host->GetBrowserContext());
  map->RemoveProcess(host);
}

// static
void RenderProcessHostImpl::RegisterCreationObserver(
    RenderProcessHostCreationObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetAllCreationObservers().push_back(observer);
}

// static
void RenderProcessHostImpl::UnregisterCreationObserver(
    RenderProcessHostCreationObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto iter = base::ranges::find(GetAllCreationObservers(), observer);
  DCHECK(iter != GetAllCreationObservers().end());
  GetAllCreationObservers().erase(iter);
}

// static
RenderProcessHost::FilterURLResult RenderProcessHostImpl::FilterURL(
    RenderProcessHost* rph,
    bool empty_allowed,
    GURL* url) {
  if (empty_allowed && url->is_empty()) {
    return FilterURLResult::kAllowed;
  }

  if (!url->is_valid()) {
    // Have to use about:blank for the denied case, instead of an empty GURL.
    // This is because the browser treats navigation to an empty GURL as a
    // navigation to the home page. This is often a privileged page
    // (chrome://newtab/) which is exactly what we don't want.
    TRACE_EVENT1("navigation", "RenderProcessHost::FilterURL - invalid URL",
                 "process_id", rph->GetID());
    VLOG(1) << "Blocked invalid URL";
    base::UmaHistogramEnumeration("BrowserRenderProcessHost.BlockedByFilterURL",
                                  BlockedURLReason::kInvalidURL);

    *url = GURL(kBlockedURL);
    return FilterURLResult::kBlocked;
  }

  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  if (!policy->CanRequestURL(rph->GetID(), *url)) {
    // If this renderer is not permitted to request this URL, we invalidate
    // the URL.  This prevents us from storing the blocked URL and becoming
    // confused later.
    TRACE_EVENT2("navigation",
                 "RenderProcessHost::FilterURL - failed CanRequestURL",
                 "process_id", rph->GetID(), "url", url->spec());
    VLOG(1) << "Blocked URL " << url->spec();
    base::UmaHistogramEnumeration("BrowserRenderProcessHost.BlockedByFilterURL",
                                  BlockedURLReason::kFailedCanRequestURLCheck);

    *url = GURL(kBlockedURL);
    return FilterURLResult::kBlocked;
  }
  return FilterURLResult::kAllowed;
}

// static
bool RenderProcessHostImpl::IsSuitableHost(
    RenderProcessHost* host,
    const IsolationContext& isolation_context,
    const SiteInfo& site_info) {
  BrowserContext* browser_context =
      isolation_context.browser_or_resource_context().ToBrowserContext();
  DCHECK(browser_context);
  if (run_renderer_in_process()) {
    DCHECK_EQ(host->GetBrowserContext(), browser_context)
        << " Single-process mode does not support multiple browser contexts.";
    return true;
  }

  if (host->GetBrowserContext() != browser_context)
    return false;

  // Do not allow sharing of guest and non-guest hosts.  Note that we also
  // enforce that `host` and `site_info` must belong to the same
  // StoragePartition via the InSameStoragePartition() check below.
  if (host->IsForGuestsOnly() != site_info.is_guest())
    return false;

  // If this process has a different JIT policy to the site then it can't be
  // reused.
  if (host->IsJitDisabled() != site_info.is_jit_disabled())
    return false;

  // PDF and non-PDF content cannot share processes.
  if (host->IsPdf() != site_info.is_pdf())
    return false;

  // Check whether the given host and the intended site_info will be using the
  // same StoragePartition, since a RenderProcessHost can only support a
  // single StoragePartition.  This is relevant for packaged apps.
  StoragePartition* dest_partition = browser_context->GetStoragePartition(
      site_info.storage_partition_config());
  if (!host->InSameStoragePartition(dest_partition))
    return false;

  // Check WebUI bindings and origin locks.  Note that |lock_url| may differ
  // from |site_url| if an effective URL is used.
  bool host_has_web_ui_bindings =
      ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
          host->GetID());
  ProcessLock process_lock = host->GetProcessLock();
  if (host->HostHasNotBeenUsed()) {
    // If the host hasn't been used, it won't have the expected WebUI bindings
    // or origin locks just *yet* - skip the checks in this case.  One example
    // where this case can happen is when the spare RenderProcessHost gets
    // used.
    CHECK(!host_has_web_ui_bindings);
    CHECK(process_lock.is_invalid());
  } else {
    // WebUI checks.
    bool url_is_for_web_ui =
        WebUIControllerFactoryRegistry::GetInstance()->UseWebUIForURL(
            browser_context, site_info.site_url());
    if (host_has_web_ui_bindings && !url_is_for_web_ui)
      return false;
    // A host with no bindings is not necessarily unsuitable for a WebUI, but we
    // incorrectly return false here. For example, some WebUIs, like
    // chrome://process-internals, don't have bindings, so this method would
    // return false for a chrome://process-internals host and a
    // chrome://process-internals target URL.
    // TODO(crbug.com/1158277): Don't return false for suitable WebUI hosts
    // and WebUI target URLs.
    if (!host_has_web_ui_bindings && url_is_for_web_ui)
      return false;

    if (process_lock.is_locked_to_site()) {
      // If this process is locked to a site, it cannot be reused for a
      // destination that doesn't require a dedicated process, even for the
      // same site. This can happen with dynamic isolated origins (see
      // https://crbug.com/950453).
      if (!site_info.ShouldLockProcessToSite(isolation_context))
        return false;

      // If the destination requires a different process lock, this process
      // cannot be used.
      if (process_lock != ProcessLock::FromSiteInfo(site_info))
        return false;
    } else {
      // Even when this process is not locked to a site, it is still associated
      // with a particular isolation configuration.  Ensure that it cannot be
      // reused for destinations with incompatible isolation requirements.
      if (process_lock.allows_any_site() &&
          !process_lock.IsCompatibleWithWebExposedIsolation(site_info)) {
        return false;
      }

      if (!host->IsUnused() &&
          site_info.ShouldLockProcessToSite(isolation_context)) {
        // If this process has been used to host any other content, it cannot
        // be reused if the destination site requires a dedicated process and
        // should use a process locked to just that site.
        return false;
      }
    }
  }

  // If this site_info is going to require a dedicated process, then check
  // whether this process has a pending navigation to a URL for which
  // SiteInstance does not assign site URLs.  If this is the case, it is not
  // safe to reuse this process for a navigation which itself assigns site
  // URLs, since in that case the latter navigation could lock this process
  // before the commit for the siteless URL arrives, resulting in a renderer
  // kill. See https://crbug.com/970046.
  if (SiteInstance::ShouldAssignSiteForURL(site_info.site_url()) &&
      site_info.RequiresDedicatedProcess(isolation_context)) {
    SiteProcessCountTracker* pending_tracker =
        static_cast<SiteProcessCountTracker*>(
            browser_context->GetUserData(kPendingSiteProcessCountTrackerKey));
    if (pending_tracker &&
        pending_tracker->ContainsNonReusableSiteForHost(host))
      return false;
  }

  // Finally, let the embedder decide if there are any last reasons to consider
  // this process unsuitable. This check is last so that it cannot override any
  // of the earlier reasons.
  return GetContentClient()->browser()->IsSuitableHost(host,
                                                       site_info.site_url());
}

// static
bool RenderProcessHostImpl::MayReuseAndIsSuitable(
    RenderProcessHost* host,
    const IsolationContext& isolation_context,
    const SiteInfo& site_info) {
  return host->MayReuseHost() &&
         IsSuitableHost(host, isolation_context, site_info);
}

// static
bool RenderProcessHostImpl::MayReuseAndIsSuitable(
    RenderProcessHost* host,
    SiteInstanceImpl* site_instance) {
  return MayReuseAndIsSuitable(host, site_instance->GetIsolationContext(),
                               site_instance->GetSiteInfo());
}

// static
bool RenderProcessHostImpl::ShouldDelayProcessShutdown() {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  return true;
#else
  return false;
#endif
}

// static
void RenderProcessHost::WarmupSpareRenderProcessHost(
    content::BrowserContext* browser_context) {
  SpareRenderProcessHostManager::GetInstance().WarmupSpareRenderProcessHost(
      browser_context);
}

// static
bool RenderProcessHost::run_renderer_in_process() {
  return g_run_renderer_in_process;
}

// static
void RenderProcessHost::SetRunRendererInProcess(bool value) {
  g_run_renderer_in_process = value;

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (value) {
    if (!command_line->HasSwitch(switches::kLang)) {
      // Modify the current process' command line to include the browser
      // locale, as the renderer expects this flag to be set.
      const std::string locale =
          GetContentClient()->browser()->GetApplicationLocale();
      command_line->AppendSwitchASCII(switches::kLang, locale);
    }
    // TODO(piman): we should really send configuration through bools rather
    // than by parsing strings, i.e. sending an IPC rather than command line
    // args. crbug.com/314909
    AppendCompositorCommandLineFlags(command_line);
  }
}

// static
void RenderProcessHost::ShutDownInProcessRenderer() {
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope
      allow_base_sync_primitives;
  RenderProcessHostImpl::ShutDownInProcessRenderer();
}

// static
RenderProcessHost::iterator RenderProcessHost::AllHostsIterator() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return iterator(&GetAllHosts());
}

// static
RenderProcessHost* RenderProcessHost::FromID(int render_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return GetAllHosts().Lookup(render_process_id);
}

// static
size_t RenderProcessHostImpl::GetProcessCount() {
  return GetAllHosts().size();
}

// static
size_t RenderProcessHostImpl::GetProcessCountForLimit() {
  // Let the embedder specify a number of processes to ignore when checking
  // against the process limit, to avoid forcing normal pages to reuse processes
  // too soon.
  size_t process_count_to_ignore =
      GetContentClient()->browser()->GetProcessCountToIgnoreForLimit();
  CHECK_LE(process_count_to_ignore, RenderProcessHostImpl::GetProcessCount());
  return RenderProcessHostImpl::GetProcessCount() - process_count_to_ignore;
}

// static
bool RenderProcessHost::ShouldTryToUseExistingProcessHost(
    BrowserContext* browser_context,
    const GURL& url) {
  if (run_renderer_in_process())
    return true;

  // NOTE: Sometimes it's necessary to create more render processes than
  //       GetMaxRendererProcessCount(), for instance when we want to create
  //       a renderer process for a browser context that has no existing
  //       renderers. This is OK in moderation, since the
  //       GetMaxRendererProcessCount() is conservative.
  size_t process_count = RenderProcessHostImpl::GetProcessCountForLimit();
  if (process_count >= GetMaxRendererProcessCount()) {
    MAYBEVLOG(4) << __func__
                 << ": process_count >= GetMaxRendererProcessCount() ("
                 << process_count << " >= " << GetMaxRendererProcessCount()
                 << ") - will try to reuse an existing process";
    return true;
  }

  return GetContentClient()->browser()->ShouldTryToUseExistingProcessHost(
      browser_context, url);
}

// static
RenderProcessHost* RenderProcessHostImpl::GetExistingProcessHost(
    SiteInstanceImpl* site_instance) {
  // First figure out which existing renderers we can use.
  std::vector<RenderProcessHost*> suitable_renderers;
  suitable_renderers.reserve(RenderProcessHostImpl::GetProcessCount());

  for (iterator iter(AllHostsIterator()); !iter.IsAtEnd(); iter.Advance()) {
    // The spare RenderProcessHost will have been considered by this point.
    // Ensure it is not added to the collection of suitable renderers.
    if (iter.GetCurrentValue() == SpareRenderProcessHostManager::GetInstance()
                                      .spare_render_process_host()) {
      continue;
    }
    if (MayReuseAndIsSuitable(iter.GetCurrentValue(), site_instance))
      suitable_renderers.push_back(iter.GetCurrentValue());
  }

  MAYBEVLOG(4) << __func__ << ": Found " << suitable_renderers.size()
               << " suitable process hosts out of "
               << RenderProcessHostImpl::GetProcessCount() << ".";

  // Now pick a random suitable renderer, if we have any.
  if (!suitable_renderers.empty()) {
    int suitable_count = static_cast<int>(suitable_renderers.size());
    int random_index = base::RandInt(0, suitable_count - 1);
    return suitable_renderers[random_index];
  }

  return nullptr;
}

// static
RenderProcessHost* RenderProcessHostImpl::GetSoleProcessHostForSite(
    const IsolationContext& isolation_context,
    const SiteInfo& site_info) {
  // Look up the map of site to process for the given browser_context.
  SiteProcessMap* map = GetSiteProcessMapForBrowserContext(
      isolation_context.browser_or_resource_context().ToBrowserContext());

  // See if we have an existing process with appropriate bindings for this
  // site. If not, the caller should create a new process and register it.
  // Note that MayReuseAndIsSuitable expects a SiteInfo rather than the full
  // |url|.
  RenderProcessHost* host = map->FindProcess(site_info);
  if (host && !MayReuseAndIsSuitable(host, isolation_context, site_info)) {
    // The registered process does not have an appropriate set of bindings for
    // the url.  Remove it from the map so we can register a better one.
    RecordAction(
        base::UserMetricsAction("BindingsMismatch_GetProcessHostPerSite"));
    map->RemoveProcess(host);
    host = nullptr;
  }

  return host;
}

void RenderProcessHostImpl::RegisterSoleProcessHostForSite(
    RenderProcessHost* process,
    SiteInstanceImpl* site_instance) {
  DCHECK(process);
  DCHECK(site_instance);

  // Look up the map of site to process for site_instance's BrowserContext.
  SiteProcessMap* map =
      GetSiteProcessMapForBrowserContext(site_instance->GetBrowserContext());

  // Only register valid, non-empty sites.  Empty or invalid sites will not
  // use process-per-site mode.  We cannot check whether the process has
  // appropriate bindings here, because the bindings have not yet been
  // granted.
  if (!site_instance->GetSiteInfo().is_empty())
    map->RegisterProcess(site_instance->GetSiteInfo(), process);
}

// static
RenderProcessHost* RenderProcessHostImpl::GetProcessHostForSiteInstance(
    SiteInstanceImpl* site_instance) {
  const SiteInfo& site_info = site_instance->GetSiteInfo();
  SiteInstanceImpl::ProcessReusePolicy process_reuse_policy =
      site_instance->process_reuse_policy();
  RenderProcessHost* render_process_host = nullptr;

  bool is_unmatched_service_worker = site_instance->is_for_service_worker();
  BrowserContext* browser_context = site_instance->GetBrowserContext();

  // First, attempt to reuse an existing RenderProcessHost if necessary.
  switch (process_reuse_policy) {
    case SiteInstanceImpl::ProcessReusePolicy::PROCESS_PER_SITE: {
      render_process_host = GetSoleProcessHostForSite(
          site_instance->GetIsolationContext(), site_info);
      break;
    }
    case SiteInstanceImpl::ProcessReusePolicy::
        REUSE_PENDING_OR_COMMITTED_SITE: {
      render_process_host =
          FindReusableProcessHostForSiteInstance(site_instance);
      const base::TimeTicks reusable_host_lookup_time = base::TimeTicks::Now();
      UMA_HISTOGRAM_BOOLEAN(
          "SiteIsolation.ReusePendingOrCommittedSite.CouldReuse2",
          render_process_host != nullptr);
      if (render_process_host) {
        is_unmatched_service_worker = false;
        render_process_host->StopTrackingProcessForShutdownDelay();
      } else {
        RecentlyDestroyedHosts::RecordMetricIfReusableHostRecentlyDestroyed(
            reusable_host_lookup_time,
            ProcessLock::FromSiteInfo(site_instance->GetSiteInfo()),
            site_instance->GetBrowserContext());
      }
      break;
    }
    case SiteInstanceImpl::ProcessReusePolicy::
        REUSE_PENDING_OR_COMMITTED_SITE_WITH_MAIN_FRAME_THRESHOLD: {
      CHECK(base::FeatureList::IsEnabled(
          features::kProcessPerSiteUpToMainFrameThreshold));
      size_t main_frame_threshold = base::checked_cast<size_t>(
          features::kProcessPerSiteMainFrameThreshold.Get());
      render_process_host = FindReusableProcessHostForSiteInstance(
          site_instance, main_frame_threshold);
      if (render_process_host) {
        is_unmatched_service_worker = false;
        render_process_host->StopTrackingProcessForShutdownDelay();
      }
      break;
    }
    default: {
      break;
    }
  }

  // If not, attempt to reuse an existing process with an unmatched service
  // worker for this site. Exclude cases where the policy is DEFAULT and the
  // site instance is for a service worker. We use DEFAULT when we have failed
  // to start the service worker before and want to use a new process.
  if (!render_process_host &&
      !(process_reuse_policy == SiteInstanceImpl::ProcessReusePolicy::DEFAULT &&
        site_instance->is_for_service_worker())) {
    render_process_host =
        UnmatchedServiceWorkerProcessTracker::MatchWithSite(site_instance);
  }

  // If a process hasn't been selected yet, check whether there is a process
  // tracked by the SiteInstanceGroupManager that could be reused by this
  // SiteInstance.  This method is used to place all SiteInstances within a
  // group into a single process. It also allows the SiteInstanceGroupManager to
  // place SiteInstances with similar requirements in different groups, but
  // still allow them to share a process (e.g. default process mode).
  if (!render_process_host) {
    render_process_host =
        site_instance->GetSiteInstanceGroupProcessIfAvailable();
  }

  if (render_process_host) {
    site_instance->set_process_assignment(
        SiteInstanceProcessAssignment::REUSED_EXISTING_PROCESS);
  }

  // See if the spare RenderProcessHost can be used.
  auto& spare_process_manager = SpareRenderProcessHostManager::GetInstance();
  bool spare_was_taken = false;
  if (!render_process_host) {
    render_process_host = spare_process_manager.MaybeTakeSpareRenderProcessHost(
        browser_context, site_instance);
    if (render_process_host) {
      site_instance->set_process_assignment(
          SiteInstanceProcessAssignment::USED_SPARE_PROCESS);
      spare_was_taken = true;
    }
  }

  // If not (or if none found), see if we should reuse an existing process.
  if (!render_process_host && ShouldTryToUseExistingProcessHost(
                                  browser_context, site_info.site_url())) {
    render_process_host = GetExistingProcessHost(site_instance);
    if (render_process_host) {
      site_instance->set_process_assignment(
          SiteInstanceProcessAssignment::REUSED_EXISTING_PROCESS);
    }
  }

  // If we found a process to reuse, double-check that it is suitable for
  // |site_instance|. For example, if the SiteInfo for |site_instance| requires
  // a dedicated process, we should never pick a process used by, or locked to,
  // a different site.
  if (render_process_host && !RenderProcessHostImpl::MayReuseAndIsSuitable(
                                 render_process_host, site_instance)) {
    base::debug::SetCrashKeyString(bad_message::GetRequestedSiteInfoKey(),
                                   site_info.GetDebugString());
    ChildProcessSecurityPolicyImpl::GetInstance()->LogKilledProcessOriginLock(
        render_process_host->GetID());
    CHECK(false) << "Unsuitable process reused for site " << site_info;
  }

  // Otherwise, create a new RenderProcessHost.
  if (!render_process_host) {
    // Pass a null StoragePartition. Tests with TestBrowserContext using a
    // RenderProcessHostFactory may not instantiate a StoragePartition, and
    // creating one here with GetStoragePartition() can run into cross-thread
    // issues as TestBrowserContext initialization is done on the main thread.
    render_process_host =
        CreateRenderProcessHost(browser_context, site_instance);

    site_instance->set_process_assignment(
        SiteInstanceProcessAssignment::CREATED_NEW_PROCESS);
  }

  // It is important to call PrepareForFutureRequests *after* potentially
  // creating a process a few statements earlier - doing this avoids violating
  // the process limit.
  //
  // We should not warm-up another spare if the spare was not taken, because
  // in this case we might have created a new process - we want to avoid
  // spawning two processes at the same time.  In this case the call to
  // PrepareForFutureRequests will be postponed until later (e.g. until the
  // navigation commits or a cross-site redirect happens).
  if (spare_was_taken) {
    spare_process_manager.PrepareForFutureRequests(browser_context);
  }

  if (is_unmatched_service_worker) {
    UnmatchedServiceWorkerProcessTracker::Register(render_process_host,
                                                   site_instance);
  }

  // Make sure the chosen process is in the correct StoragePartition for the
  // SiteInstance.
  CHECK(render_process_host->InSameStoragePartition(
      browser_context->GetStoragePartition(site_instance,
                                           false /* can_create */)));

  MAYBEVLOG(2) << __func__ << "(" << site_info << ") selected process host "
               << render_process_host->GetID() << " using assignment \""
               << site_instance->GetLastProcessAssignmentOutcome() << "\""
               << std::endl
               << GetCurrentHostMapDebugString(
                      static_cast<SiteProcessCountTracker*>(
                          browser_context->GetUserData(
                              kCommittedSiteProcessCountTrackerKey)));

  return render_process_host;
}

void RenderProcessHostImpl::CreateSharedRendererHistogramAllocator() {
  // Create a persistent memory segment for renderer histograms only if
  // they're active in the browser.
  if (!base::GlobalHistogramAllocator::Get()) {
    if (is_initialized_) {
      HistogramController::GetInstance()->SetHistogramMemory<RenderProcessHost>(
          this, base::WritableSharedMemoryRegion());
    }
    return;
  }

  // Get handle to the renderer process. Stop if there is none.
  base::ProcessHandle destination = GetProcess().Handle();
  if (destination == base::kNullProcessHandle)
    return;

  // Get the renderer histogram shared memory configuration.
  auto shared_memory_config =
      GetHistogramSharedMemoryConfig(PROCESS_TYPE_RENDERER);
  CHECK(shared_memory_config.has_value());

  // Create the shared memory region and allocator.
  auto shared_memory = base::HistogramSharedMemory::Create(
      GetID(), shared_memory_config.value());
  if (!shared_memory.has_value()) {
    return;
  }

  // Move the region and allocator out of the |shared_memory| helper.
  metrics_allocator_ = shared_memory->TakeAllocator();

  HistogramController::GetInstance()->SetHistogramMemory<RenderProcessHost>(
      this, shared_memory->TakeRegion());
}

ChildProcessTerminationInfo RenderProcessHostImpl::GetChildTerminationInfo(
    bool already_dead) {
  DCHECK(child_process_launcher_);

  ChildProcessTerminationInfo info;

  info = child_process_launcher_->GetChildTerminationInfo(already_dead);
  if (already_dead && info.status == base::TERMINATION_STATUS_STILL_RUNNING) {
    // May be in case of IPC error, if it takes long time for renderer
    // to exit. Child process will be killed in any case during
    // child_process_launcher_.reset(). Make sure we will not broadcast
    // RenderProcessExited with status TERMINATION_STATUS_STILL_RUNNING,
    // since this will break WebContentsImpl logic.
    info.status = base::TERMINATION_STATUS_PROCESS_CRASHED;

    // TODO(siggi): Remove this once https://crbug.com/806661 is resolved.
#if BUILDFLAG(IS_WIN)
    if (info.exit_code == WAIT_TIMEOUT && g_analyze_hung_renderer)
      g_analyze_hung_renderer(child_process_launcher_->GetProcess());
#endif
  }

#if BUILDFLAG(IS_ANDROID)
  PopulateTerminationInfoRendererFields(&info);
#endif  // BUILDFLAG(IS_ANDROID)

  return info;
}

void RenderProcessHostImpl::ProcessDied(
    const ChildProcessTerminationInfo& termination_info) {
  TRACE_EVENT0("content", "RenderProcessHostImpl::ProcessDied");
  // Our child process has died.  If we didn't expect it, it's a crash.
  // In any case, we need to let everyone know it's gone.
  // The OnChannelError notification can fire multiple times due to nested
  // sync calls to a renderer. If we don't have a valid channel here it means
  // we already handled the error.

  // It should not be possible for us to be called re-entrantly.
  DCHECK(!within_process_died_observer_);

  // It should not be possible for a process death notification to come in
  // while we are dying.
  DCHECK(!deleting_soon_);

  child_process_launcher_.reset();
  is_dead_ = true;
  // Make sure no IPCs or mojo calls from the old process get dispatched after
  // it has died.
  ResetIPC();

  UpdateProcessPriority();

  // RenderProcessExited relies on the exit code set during shutdown.
  ChildProcessTerminationInfo info = termination_info;
  if (shutdown_exit_code_ != -1)
    info.exit_code = shutdown_exit_code_;

  within_process_died_observer_ = true;
  for (auto& observer : observers_)
    observer.RenderProcessExited(this, info);

  NotificationService::current()->Notify(
      NOTIFICATION_RENDERER_PROCESS_CLOSED, Source<RenderProcessHost>(this),
      Details<ChildProcessTerminationInfo>(&info));
  within_process_died_observer_ = false;

  // Initialize a new ChannelProxy in case this host is re-used for a new
  // process. This ensures that new messages can be sent on the host ASAP
  // (even before Init()) and they'll eventually reach the new process.
  //
  // Note that this may have already been called by one of the above observers
  EnableSendQueue();

  // It's possible that one of the calls out to the observers might have
  // caused this object to be no longer needed.
  if (delayed_cleanup_needed_)
    Cleanup();

  compositing_mode_reporter_.reset();

  HistogramController::GetInstance()->NotifyChildDied<RenderProcessHost>(this);
  // This object is not deleted at this point and might be reused later.
  // TODO(darin): clean this up
}

void RenderProcessHostImpl::ResetIPC() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  media_interface_proxy_.reset();
  renderer_host_receiver_.reset();
  io_thread_host_impl_.reset();
  associated_interfaces_.reset();
  coordinator_connector_receiver_.reset();
  tracing_registration_.reset();

#if BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
  ResetStableVideoDecoderFactory();
#endif  // BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)

  // Destroy all embedded CompositorFrameSinks.
  embedded_frame_sink_provider_.reset();

  dom_storage_provider_receiver_.reset();
  for (auto receiver_id : dom_storage_receiver_ids_)
    storage_partition_impl_->UnbindDomStorage(receiver_id);

  instance_weak_factory_.InvalidateWeakPtrs();

  // It's important not to wait for the DeleteTask to delete the channel
  // proxy. Kill it off now. That way, in case the profile is going away, the
  // rest of the objects attached to this RenderProcessHost start going
  // away first, since deleting the channel proxy will post a
  // OnChannelClosed() to IPC::ChannelProxy::Context on the IO thread.
  ResetChannelProxy();

  // The PermissionServiceContext holds PermissionSubscriptions originating
  // from service workers. These subscriptions observe the
  // PermissionControllerImpl that is owned by the Profile corresponding to
  // |this|. At this point, IPC are unbound so no new subscriptions can be
  // made. Existing subscriptions need to be released here, as the Profile,
  // and with it, the PermissionControllerImpl, can be destroyed anytime after
  // RenderProcessHostImpl::Cleanup() returns.
  permission_service_context_.reset();
}

size_t RenderProcessHost::GetActiveViewCount() {
  size_t num_active_views = 0;
  std::unique_ptr<RenderWidgetHostIterator> widgets(
      RenderWidgetHost::GetRenderWidgetHosts());
  while (RenderWidgetHost* widget = widgets->GetNextHost()) {
    // Count only RenderWidgetHosts in this process.
    if (widget->GetProcess()->GetID() == GetID())
      num_active_views++;
  }
  return num_active_views;
}

WebExposedIsolationLevel RenderProcessHost::GetWebExposedIsolationLevel() {
  WebExposedIsolationInfo info = GetProcessLock().GetWebExposedIsolationInfo();
  if (info.is_isolated_application()) {
    // TODO(crbug.com/1159832): Check the document policy once it's available to
    // find out if this process is actually isolated.
    return WebExposedIsolationLevel::kMaybeIsolatedApplication;
  } else if (info.is_isolated()) {
    // TODO(crbug.com/1159832): Check the document policy once it's available to
    // find out if this process is actually isolated.
    return WebExposedIsolationLevel::kMaybeIsolated;
  }
  return WebExposedIsolationLevel::kNotIsolated;
}

void RenderProcessHost::PostTaskWhenProcessIsReady(base::OnceClosure task) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!task.is_null());
  new RenderProcessHostIsReadyObserver(this, std::move(task));
}

// static
void RenderProcessHost::SetHungRendererAnalysisFunction(
    AnalyzeHungRendererFunction analyze_hung_renderer) {
  g_analyze_hung_renderer = analyze_hung_renderer;
}

void RenderProcessHostImpl::SuddenTerminationChanged(bool enabled) {
  SetSuddenTerminationAllowed(enabled);
}

void RenderProcessHostImpl::RecordUserMetricsAction(const std::string& action) {
  base::RecordComputedAction(action);
}

#if BUILDFLAG(IS_ANDROID)
void RenderProcessHostImpl::SetPrivateMemoryFootprint(
    uint64_t private_memory_footprint_bytes) {
  private_memory_footprint_bytes_ = private_memory_footprint_bytes;
}
#endif

void RenderProcessHostImpl::UpdateProcessPriorityInputs() {
  int32_t new_visible_widgets_count = 0;
  unsigned int new_frame_depth = kMaxFrameDepthForPriority;
  bool new_intersects_viewport = false;
#if BUILDFLAG(IS_ANDROID)
  ChildProcessImportance new_effective_importance =
      ChildProcessImportance::NORMAL;
#endif
  for (auto* client : priority_clients_) {
    RenderProcessHostPriorityClient::Priority priority = client->GetPriority();

    // Compute the lowest depth of widgets with highest visibility priority.
    // See comment on |frame_depth_| for more details.
    if (priority.is_hidden) {
      if (!new_visible_widgets_count) {
        new_frame_depth = std::min(new_frame_depth, priority.frame_depth);
        new_intersects_viewport =
            new_intersects_viewport || priority.intersects_viewport;
      }
    } else {
      if (new_visible_widgets_count) {
        new_frame_depth = std::min(new_frame_depth, priority.frame_depth);
        new_intersects_viewport =
            new_intersects_viewport || priority.intersects_viewport;
      } else {
        new_frame_depth = priority.frame_depth;
        new_intersects_viewport = priority.intersects_viewport;
      }
      new_visible_widgets_count++;
    }

#if BUILDFLAG(IS_ANDROID)
    new_effective_importance =
        std::max(new_effective_importance, priority.importance);
#endif
  }

  bool inputs_changed = new_visible_widgets_count != visible_clients_ ||
                        frame_depth_ != new_frame_depth ||
                        intersects_viewport_ != new_intersects_viewport;
  visible_clients_ = new_visible_widgets_count;
  frame_depth_ = new_frame_depth;
  intersects_viewport_ = new_intersects_viewport;
#if BUILDFLAG(IS_ANDROID)
  inputs_changed =
      inputs_changed || new_effective_importance != effective_importance_;
  effective_importance_ = new_effective_importance;
#endif
  if (inputs_changed)
    UpdateProcessPriority();
}

void RenderProcessHostImpl::UpdateProcessPriority() {
  if (!run_renderer_in_process() && (!child_process_launcher_.get() ||
                                     child_process_launcher_->IsStarting())) {
    // This path can be hit early (no-op) or on ProcessDied(). Reset
    // |priority_| to defaults in case this RenderProcessHostImpl is re-used.
    priority_.visible = !blink::kLaunchingProcessIsBackgrounded;
    priority_.boost_for_pending_views = true;
    return;
  }

  RenderProcessPriority priority(
      visible_clients_ > 0 || base::CommandLine::ForCurrentProcess()->HasSwitch(
                                  switches::kDisableRendererBackgrounding),
      media_stream_count_ > 0, foreground_service_worker_count_ > 0,
      frame_depth_, intersects_viewport_,
      !!pending_views_ /* boost_for_pending_views */
#if BUILDFLAG(IS_ANDROID)
      ,
      GetEffectiveImportance()
#endif
  );

  // If a priority override has been specified, use it instead.
  // TODO(chrisha): After experimentation, either integrate the experimental
  // logic into this class, or rip out the existing logic entirely.
  if (priority_override_.has_value()) {
    bool foregrounded = priority_override_.value();
    priority = RenderProcessPriority(
        foregrounded, /* is_visible */
        foregrounded, /* has_media_stream */
        foregrounded, /* has_foreground_service_worker */
        0,            /* frame_depth */
        foregrounded, /* intersects_viewport */
        false         /* boost_for_pending_views */
#if BUILDFLAG(IS_ANDROID)
        ,
        foregrounded ? ChildProcessImportance::NORMAL
                     : ChildProcessImportance::MODERATE /* importance */
#endif
    );
    DCHECK_EQ(!foregrounded, priority.is_background());
  }

  if (priority_ == priority)
    return;
  const bool background_state_changed =
      priority_.is_background() != priority.is_background();
  const bool visibility_state_changed = priority_.visible != priority.visible;

  TRACE_EVENT("renderer_host", "RenderProcessHostImpl::UpdateProcessPriority",
              ChromeTrackEvent::kRenderProcessHost, *this,
              ChromeTrackEvent::kChildProcessLauncherPriority, priority);
  priority_ = priority;

  // Control the background state from the browser process, otherwise the task
  // telling the renderer to "unbackground" itself may be preempted by other
  // tasks executing at lowered priority ahead of it or simply by not being
  // swiftly scheduled by the OS per the low process priority
  // (http://crbug.com/398103).
  if (!run_renderer_in_process()) {
    DCHECK(child_process_launcher_.get());
    DCHECK(!child_process_launcher_->IsStarting());
#if BUILDFLAG(IS_ANDROID)
    child_process_launcher_->SetRenderProcessPriority(priority_);
#elif BUILDFLAG(IS_MAC)
    if (base::FeatureList::IsEnabled(
            features::kMacAllowBackgroundingRenderProcesses)) {
      child_process_launcher_->SetProcessPriority(
          priority_.is_background() ? base::Process::Priority::kBestEffort
                                    : base::Process::Priority::kUserBlocking);
    }
#else
    child_process_launcher_->SetProcessPriority(
        priority_.is_background() ? base::Process::Priority::kBestEffort
                                  : base::Process::Priority::kUserBlocking);
#endif
  }

  // Notify the child process of the change in state.
  if ((background_state_changed) || visibility_state_changed) {
    SendProcessStateToRenderer();
  }
  for (auto& observer : internal_observers_)
    observer.RenderProcessBackgroundedChanged(this);

  // Update the priority of the process running the controller service worker
  // when client's background state changed. We can make the service worker
  // process backgrounded if all of its clients are backgrounded.
  if (background_state_changed) {
    UpdateControllerServiceWorkerProcessPriority();
  }
}

void RenderProcessHostImpl::UpdateControllerServiceWorkerProcessPriority() {
  ServiceWorkerContextWrapper* context =
      storage_partition_impl_->GetServiceWorkerContext();
  if (!context)
    return;

  for (const auto& kv : context->GetRunningServiceWorkerInfos()) {
    ServiceWorkerVersion* version = context->GetLiveVersion(kv.first);
    // TODO(https://crbug.com/1271393): It appears that some times `version` is
    // nullptr here, but we don't know why.  Once that is solved revert this
    // runtime check back to a DCHECK.
    if (version && version->IsControlleeProcessID(GetID())) {
      version->UpdateForegroundPriority();
      break;
    }
  }
}

void RenderProcessHostImpl::SendProcessStateToRenderer() {
  mojom::RenderProcessBackgroundState background_state =
      priority_.is_background()
          ? mojom::RenderProcessBackgroundState::kBackgrounded
          : mojom::RenderProcessBackgroundState::kForegrounded;
  mojom::RenderProcessVisibleState visible_state =
      priority_.visible ? mojom::RenderProcessVisibleState::kVisible
                        : mojom::RenderProcessVisibleState::kHidden;
  GetRendererInterface()->SetProcessState(background_state, visible_state);
}

void RenderProcessHostImpl::OnProcessLaunched() {
  // No point doing anything, since this object will be destructed soon.  We
  // especially don't want to send the RENDERER_PROCESS_CREATED notification,
  // since some clients might expect a RENDERER_PROCESS_TERMINATED afterwards
  // to properly cleanup.
  if (deleting_soon_)
    return;

  if (child_process_launcher_) {
    DCHECK(child_process_launcher_->GetProcess().IsValid());
    // TODO(https://crbug.com/875933): This should be based on
    // |priority_.is_background()|, see similar check below.
    DCHECK_EQ(blink::kLaunchingProcessIsBackgrounded, !priority_.visible);

    // Unpause the channel now that the process is launched. We don't flush it
    // yet to ensure that any initialization messages sent here (e.g., things
    // done in response to NOTIFICATION_RENDER_PROCESS_CREATED; see below)
    // preempt already queued messages.
    channel_->Unpause(false /* flush */);

    gpu_client_->SetClientPid(GetProcess().Pid());

    if (coordinator_connector_receiver_.is_bound())
      coordinator_connector_receiver_.Resume();

      // Not all platforms launch processes in the same backgrounded state. Make
      // sure |priority_.visible| reflects this platform's initial process
      // state.
#if BUILDFLAG(IS_APPLE)
    priority_.visible = child_process_launcher_->GetProcess().GetPriority(
                            ChildProcessTaskPortProvider::GetInstance()) ==
                        base::Process::Priority::kUserBlocking;
#elif BUILDFLAG(IS_ANDROID)
    // Android child process priority works differently and cannot be queried
    // directly from base::Process.
    // TODO(https://crbug.com/875933): Fix initial priority on Android to
    // reflect |priority_.is_background()|.
    DCHECK_EQ(blink::kLaunchingProcessIsBackgrounded, !priority_.visible);
#else
    priority_.visible = child_process_launcher_->GetProcess().GetPriority() ==
                        base::Process::Priority::kUserBlocking;
#endif  // BUILDFLAG(IS_MAC)

    // Only update the priority on startup if boosting is enabled (to avoid
    // reintroducing https://crbug.com/560446#c13 while pending views only
    // experimentally result in a boost).
    if (priority_.boost_for_pending_views)
      UpdateProcessPriority();

    // Share histograms between the renderer and this process.
    CreateSharedRendererHistogramAllocator();
  }

  // Pass bits of global renderer state to the renderer.
  NotifyRendererOfLockedStateUpdate();

  // Send the initial system color info to the renderer.
  ThemeHelper::GetInstance()->SendSystemColorInfo(GetRendererInterface());

  // NOTE: This needs to be before flushing queued messages, because
  // ExtensionService uses this notification to initialize the renderer
  // process with state that must be there before any JavaScript executes.
  //
  // The queued messages contain such things as "navigate". If this
  // notification was after, we can end up executing JavaScript before the
  // initialization happens.
  NotificationService::current()->Notify(NOTIFICATION_RENDERER_PROCESS_CREATED,
                                         Source<RenderProcessHost>(this),
                                         NotificationService::NoDetails());
  for (auto* observer : GetAllCreationObservers())
    observer->OnRenderProcessHostCreated(this);

  if (child_process_launcher_)
    channel_->Flush();

  if (IsReady()) {
    DCHECK(!sent_render_process_ready_);
    sent_render_process_ready_ = true;
    // Send RenderProcessReady only if the channel is already connected.
    for (auto& observer : observers_)
      observer.RenderProcessReady(this);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    // Provide /proc/{renderer pid}/status and statm files for
    // MemoryUsageMonitor in blink.
    ProvideStatusFileForRenderer();
#endif
  }

  aec_dump_manager_.set_pid(GetProcess().Pid());
  aec_dump_manager_.AutoStart();

  tracing_registration_ = TracingServiceController::Get().RegisterClient(
      GetProcess().Pid(),
      base::BindRepeating(&RenderProcessHostImpl::BindTracedProcess,
                          instance_weak_factory_.GetWeakPtr()));

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID)
  system_tracing_service_ = std::make_unique<tracing::SystemTracingService>();
  child_process_->EnableSystemTracingService(
      system_tracing_service_->BindAndPassPendingRemote());
#endif
}

void RenderProcessHostImpl::OnProcessLaunchFailed(int error_code) {
  // If this object will be destructed soon, then observers have already been
  // sent a RenderProcessHostDestroyed notification, and we must observe our
  // contract that says that will be the last call.
  if (deleting_soon_)
    return;

  ChildProcessTerminationInfo info;
  info.status = base::TERMINATION_STATUS_LAUNCH_FAILED;
  info.exit_code = error_code;
#if BUILDFLAG(IS_ANDROID)
  PopulateTerminationInfoRendererFields(&info);
#endif  // BUILDFLAG(IS_ANDROID)
  ProcessDied(info);
}

// static
RenderProcessHost*
RenderProcessHostImpl::FindReusableProcessHostForSiteInstance(
    SiteInstanceImpl* site_instance,
    absl::optional<size_t> main_frame_threshold) {
  BrowserContext* browser_context = site_instance->GetBrowserContext();
  if (!ShouldFindReusableProcessHostForSite(site_instance->GetSiteInfo()))
    return nullptr;

  std::set<RenderProcessHost*> eligible_foreground_hosts;
  std::set<RenderProcessHost*> eligible_background_hosts;

  // First, add the RenderProcessHosts expecting a navigation to |site_url| to
  // the list of eligible RenderProcessHosts.
  SiteProcessCountTracker* pending_tracker =
      static_cast<SiteProcessCountTracker*>(
          browser_context->GetUserData(kPendingSiteProcessCountTrackerKey));
  if (pending_tracker) {
    pending_tracker->FindRenderProcessesForSiteInstance(
        site_instance, main_frame_threshold, &eligible_foreground_hosts,
        &eligible_background_hosts);
  }

  if (eligible_foreground_hosts.empty()) {
    // If needed, add the RenderProcessHosts hosting a frame for |site_url| to
    // the list of eligible RenderProcessHosts.
    SiteProcessCountTracker* committed_tracker =
        static_cast<SiteProcessCountTracker*>(
            browser_context->GetUserData(kCommittedSiteProcessCountTrackerKey));
    if (committed_tracker) {
      committed_tracker->FindRenderProcessesForSiteInstance(
          site_instance, main_frame_threshold, &eligible_foreground_hosts,
          &eligible_background_hosts);
    }
  }

  // If there are no eligible existing RenderProcessHosts, add
  // RenderProcessHosts whose shutdown is pending that previously hosted a frame
  // for `site_url`.
  if (eligible_foreground_hosts.empty() && eligible_background_hosts.empty() &&
      ShouldDelayProcessShutdown()) {
    SiteProcessCountTracker* delayed_shutdown_tracker =
        static_cast<SiteProcessCountTracker*>(browser_context->GetUserData(
            kDelayedShutdownSiteProcessCountTrackerKey));
    if (delayed_shutdown_tracker) {
      delayed_shutdown_tracker->FindRenderProcessesForSiteInstance(
          site_instance, main_frame_threshold, &eligible_foreground_hosts,
          &eligible_background_hosts);
    }
  }

  if (!eligible_foreground_hosts.empty()) {
    int index = base::RandInt(0, eligible_foreground_hosts.size() - 1);
    auto iterator = eligible_foreground_hosts.begin();
    for (int i = 0; i < index; ++i)
      ++iterator;
    return *iterator;
  }

  if (!eligible_background_hosts.empty()) {
    int index = base::RandInt(0, eligible_background_hosts.size() - 1);
    auto iterator = eligible_background_hosts.begin();
    for (int i = 0; i < index; ++i)
      ++iterator;
    return *iterator;
  }

  return nullptr;
}

// static
void RenderProcessHostImpl::OnMojoError(int render_process_id,
                                        const std::string& error) {
  LOG(ERROR) << "Terminating render process for bad Mojo message: " << error;

  InvokeBadMojoMessageCallbackForTesting(render_process_id, error);

  // The ReceivedBadMessage call below will trigger a DumpWithoutCrashing.
  // Capture the error message in a crash key value.
  mojo::debug::ScopedMessageErrorCrashKey error_key_value(error);
  bad_message::ReceivedBadMessage(render_process_id,
                                  bad_message::RPH_MOJO_PROCESS_ERROR);
}

void RenderProcessHostImpl::GetBrowserHistogram(
    const std::string& name,
    BrowserHistogramCallback callback) {
  // Security: Only allow access to browser histograms when running in the
  // context of a test.
  bool using_stats_collection_controller =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kStatsCollectionController);
  if (!using_stats_collection_controller) {
    std::move(callback).Run(std::string());
    return;
  }
  base::HistogramBase* histogram =
      base::StatisticsRecorder::FindHistogram(name);
  std::string histogram_json;
  if (!histogram) {
    histogram_json = "{}";
  } else {
    histogram->WriteJSON(&histogram_json, base::JSON_VERBOSITY_LEVEL_FULL);
  }
  std::move(callback).Run(histogram_json);
}

void RenderProcessHostImpl::CancelProcessShutdownDelay(
    const SiteInfo& site_info) {
  if (AreRefCountsDisabled())
    return;

  // Remove from the delayed-shutdown tracker. This may have already been done
  // in StopTrackingProcessForShutdownDelay() if the process was reused before
  // this task executed.
  if (ShouldDelayProcessShutdown() && ShouldTrackProcessForSite(site_info)) {
    SiteProcessCountTracker* delayed_shutdown_tracker =
        SiteProcessCountTracker::GetInstance(
            GetBrowserContext(),
            content::kDelayedShutdownSiteProcessCountTrackerKey);
    if (delayed_shutdown_tracker->Contains(site_info, GetID()))
      delayed_shutdown_tracker->DecrementSiteProcessCount(site_info, GetID());
  }

  // Decrement shutdown delay ref count.
  CHECK(!are_ref_counts_disabled_);
  CHECK_GT(shutdown_delay_ref_count_, 0);
  shutdown_delay_ref_count_--;
  if (AreAllRefCountsZero())
    Cleanup();
}

void RenderProcessHostImpl::StopTrackingProcessForShutdownDelay() {
  if (!ShouldDelayProcessShutdown()) {
    return;
  }
  SiteProcessCountTracker* delayed_shutdown_tracker =
      SiteProcessCountTracker::GetInstance(
          GetBrowserContext(),
          content::kDelayedShutdownSiteProcessCountTrackerKey);
  delayed_shutdown_tracker->ClearProcessForAllSites(GetID());
}

void RenderProcessHostImpl::BindTracedProcess(
    mojo::PendingReceiver<tracing::mojom::TracedProcess> receiver) {
  BindReceiver(std::move(receiver));
}

void RenderProcessHostImpl::OnBindHostReceiver(
    mojo::GenericPendingReceiver receiver) {
#if BUILDFLAG(IS_ANDROID)
  // content::GetGlobalJavaInterfaces() works only on the UI Thread.
  if (auto r = receiver.As<blink::mojom::AndroidFontLookup>()) {
    content::GetGlobalJavaInterfaces()->GetInterface(std::move(r));
    return;
  }
#endif

  GetContentClient()->browser()->BindHostReceiverForRenderer(
      this, std::move(receiver));
}

// static
void RenderProcessHost::InterceptBindHostReceiverForTesting(
    BindHostReceiverInterceptor callback) {
  GetBindHostReceiverInterceptor() = std::move(callback);
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
void RenderProcessHostImpl::ProvideStatusFileForRenderer() {
  // We use ScopedAllowBlocking, because opening /proc/{pid}/status and
  // /proc/{pid}/statm is not blocking call.
  base::ScopedAllowBlocking allow_blocking;
  base::FilePath proc_pid_dir =
      base::FilePath("/proc").Append(base::NumberToString(GetProcess().Pid()));

  base::File status_file(
      proc_pid_dir.Append("status"),
      base::File::Flags::FLAG_OPEN | base::File::Flags::FLAG_READ);
  base::File statm_file(
      proc_pid_dir.Append("statm"),
      base::File::Flags::FLAG_OPEN | base::File::Flags::FLAG_READ);
  if (!status_file.IsValid() || !statm_file.IsValid())
    return;

  mojo::Remote<blink::mojom::MemoryUsageMonitorLinux> monitor;
  BindReceiver(monitor.BindNewPipeAndPassReceiver());
  monitor->SetProcFiles(statm_file.Duplicate(), status_file.Duplicate());
}
#endif

void RenderProcessHostImpl::ProvideSwapFileForRenderer() {
  if (!blink::features::IsParkableStringsToDiskEnabled() &&
      !blink::features::IsParkableImagesToDiskEnabled()) {
    return;
  }

  // In Incognito, nothing should be written to disk. Don't provide a file..
  if (GetBrowserContext()->IsOffTheRecord())
    return;

  mojo::Remote<blink::mojom::DiskAllocator> allocator;
  BindReceiver(allocator.BindNewPipeAndPassReceiver());

  // File creation done on a background thread. The renderer side will behave
  // correctly even if the file is provided later or never.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()}, base::BindOnce([]() {
        base::FilePath path;
        if (!base::CreateTemporaryFile(&path))
          return base::File();

        int flags = base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_READ |
                    base::File::FLAG_WRITE | base::File::FLAG_DELETE_ON_CLOSE;
        // This File is being passed to an untrusted renderer process.
        flags = base::File::AddFlagsForPassingToUntrustedProcess(flags);
        return base::File(base::FilePath(path), flags);
      }),
      base::BindOnce(
          [](mojo::Remote<blink::mojom::DiskAllocator> allocator,
             base::File file) {
            // File creation failed in the background. In this case, don't
            // provide a file, the renderer will not wait for one (see the
            // incognito case above, the renderer deals with no file being
            // provided).
            if (file.IsValid())
              allocator->ProvideTemporaryFile(std::move(file));
          },
          std::move(allocator)));
}

void RenderProcessHostImpl::SetAttributionReportingSupport(
    network::mojom::AttributionSupport attribution_support) {
  GetRendererInterface()->SetAttributionReportingSupport(attribution_support);
}

#if BUILDFLAG(IS_ANDROID)

void RenderProcessHostImpl::NotifyMemoryPressureToRenderer(
    base::MemoryPressureListener::MemoryPressureLevel level) {
  child_process_->OnMemoryPressure(level);
}

#endif

}  // namespace content
