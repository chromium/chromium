// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Represents the browser side of the browser <--> renderer communication
// channel. There will be one RenderProcessHost per renderer process.

#include "content/browser/renderer_host/render_process_host_impl.h"

#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/containers/adapters.h"
#include "base/debug/alias.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/memory_pressure_monitor.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory.h"
#include "base/memory/shared_memory_handle.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "base/metrics/statistics_recorder.h"
#include "base/metrics/user_metrics.h"
#include "base/no_destructor.h"
#include "base/numerics/ranges.h"
#include "base/process/process_handle.h"
#include "base/rand_util.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/supports_user_data.h"
#include "base/synchronization/lock.h"
#include "base/sys_info.h"
#include "base/task/post_task.h"
#include "base/thread_annotations.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_dump_provider.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "components/metrics/single_sample_metrics.h"
#include "components/tracing/common/tracing_switches.h"
#include "components/viz/common/switches.h"
#include "components/viz/host/gpu_client.h"
#include "content/browser/appcache/appcache_dispatcher_host.h"
#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/browser/background_fetch/background_fetch_context.h"
#include "content/browser/background_fetch/background_fetch_service_impl.h"
#include "content/browser/background_sync/background_sync_service_impl.h"
#include "content/browser/bad_message.h"
#include "content/browser/blob_storage/blob_dispatcher_host.h"
#include "content/browser/blob_storage/blob_registry_wrapper.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/broadcast_channel/broadcast_channel_provider.h"
#include "content/browser/browser_child_process_host_impl.h"
#include "content/browser/browser_main.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/browser_plugin/browser_plugin_message_filter.h"
#include "content/browser/cache_storage/cache_storage_context_impl.h"
#include "content/browser/cache_storage/cache_storage_dispatcher_host.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/code_cache/generated_code_cache.h"
#include "content/browser/code_cache/generated_code_cache_context.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/dom_storage/dom_storage_context_wrapper.h"
#include "content/browser/dom_storage/dom_storage_message_filter.h"
#include "content/browser/field_trial_recorder.h"
#include "content/browser/fileapi/file_system_manager_impl.h"
#include "content/browser/font_unique_name_lookup/font_unique_name_lookup_service.h"
#include "content/browser/frame_host/render_frame_message_filter.h"
#include "content/browser/gpu/browser_gpu_client_delegate.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/gpu/shader_cache_factory.h"
#include "content/browser/histogram_controller.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_dispatcher_host.h"
#include "content/browser/loader/resource_message_filter.h"
#include "content/browser/loader/resource_scheduler_filter.h"
#include "content/browser/loader/url_loader_factory_impl.h"
#include "content/browser/media/capture/audio_mirroring_manager.h"
#include "content/browser/media/media_internals.h"
#include "content/browser/media/midi_host.h"
#include "content/browser/mime_registry_impl.h"
#include "content/browser/payments/payment_manager.h"
#include "content/browser/permissions/permission_service_context.h"
#include "content/browser/permissions/permission_service_impl.h"
#include "content/browser/push_messaging/push_messaging_manager.h"
#include "content/browser/renderer_host/clipboard_host_impl.h"
#include "content/browser/renderer_host/code_cache_host_impl.h"
#include "content/browser/renderer_host/embedded_frame_sink_provider_impl.h"
#include "content/browser/renderer_host/file_utilities_host_impl.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/media_stream_track_metrics_host.h"
#include "content/browser/renderer_host/media/peer_connection_tracker_host.h"
#include "content/browser/renderer_host/media/video_capture_host.h"
#include "content/browser/renderer_host/p2p/socket_dispatcher_host.h"
#include "content/browser/renderer_host/pepper/pepper_message_filter.h"
#include "content/browser/renderer_host/pepper/pepper_renderer_connection.h"
#include "content/browser/renderer_host/plugin_registry_impl.h"
#include "content/browser/renderer_host/render_message_filter.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_helper.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/text_input_client_message_filter.h"
#include "content/browser/renderer_host/web_database_host_impl.h"
#include "content/browser/resolve_proxy_msg_helper.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_dispatcher_host.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/streams/stream_context.h"
#include "content/browser/tracing/trace_message_filter.h"
#include "content/browser/webrtc/webrtc_internals.h"
#include "content/browser/websockets/websocket_manager.h"
#include "content/browser/webui/web_ui_controller_factory_registry.h"
#include "content/common/child_process_host_impl.h"
#include "content/common/content_switches_internal.h"
#include "content/common/frame_messages.h"
#include "content/common/in_process_child_thread_params.h"
#include "content/common/media/aec_dump_messages.h"
#include "content/common/media/peer_connection_tracker_messages.h"
#include "content/common/navigation_subresource_loader_params.h"
#include "content/common/resource_messages.h"
#include "content/common/service_manager/child_connection.h"
#include "content/common/service_manager/service_manager_connection_impl.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/common/view_messages.h"
#include "content/common/widget_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host_factory.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/webrtc_log.h"
#include "content/public/common/bind_interface_helpers.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/connection_filter.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/process_type.h"
#include "content/public/common/resource_type.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/common/url_constants.h"
#include "device/gamepad/gamepad_haptics_manager.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gpu_switches.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/ipc/host/gpu_memory_buffer_support.h"
#include "ipc/ipc.mojom.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_mojo.h"
#include "ipc/ipc_logging.h"
#include "media/audio/audio_manager.h"
#include "media/base/media_switches.h"
#include "media/media_buildflags.h"
#include "media/mojo/services/video_decode_perf_history.h"
#include "media/webrtc/webrtc_switches.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "net/url_request/url_request_context_getter.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/device/public/mojom/battery_monitor.mojom.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/network/cross_origin_read_blocking.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/resource_coordinator/public/cpp/process_resource_coordinator.h"
#include "services/service_manager/embedder/switches.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "services/service_manager/runner/common/client_util.h"
#include "services/service_manager/runner/common/switches.h"
#include "services/service_manager/sandbox/switches.h"
#include "services/service_manager/zygote/common/zygote_buildflags.h"
#include "storage/browser/fileapi/sandbox_file_system_backend.h"
#include "third_party/blink/public/common/page/launching_process_state.h"
#include "third_party/blink/public/public_buildflags.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"
#include "ui/base/ui_base_switches_util.h"
#include "ui/display/display_switches.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gpu_switching_manager.h"
#include "ui/native_theme/native_theme_features.h"

#if defined(OS_ANDROID)
#include "content/public/browser/android/java_interfaces.h"
#include "ipc/ipc_sync_channel.h"
#include "media/audio/android/audio_manager_android.h"
#else
#include "content/browser/compositor/image_transport_factory.h"
#endif

#if defined(OS_MACOSX)
#include "content/browser/mach_broker_mac.h"
#endif

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#include "base/win/windows_version.h"
#include "sandbox/win/src/sandbox_policy.h"
#include "services/service_manager/sandbox/win/sandbox_win.h"
#include "ui/display/win/dpi.h"
#endif

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
#include "content/browser/media/key_system_support_impl.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/browser/plugin_service_impl.h"
#include "ppapi/shared_impl/ppapi_switches.h"  // nogncheck
#endif

#if BUILDFLAG(ENABLE_REPORTING)
#include "content/browser/net/reporting_service_proxy.h"
#endif

#if BUILDFLAG(USE_MINIKIN_HYPHENATION)
#include "content/browser/hyphenation/hyphenation_impl.h"
#endif

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_switches.h"
#endif

#if BUILDFLAG(USE_ZYGOTE_HANDLE)
#include "services/service_manager/zygote/common/zygote_handle.h"  // nogncheck
#endif

#if defined(OS_WIN)
#define IntToStringType base::IntToString16
#else
#define IntToStringType base::IntToString
#endif

namespace content {

using CheckOriginLockResult =
    ChildProcessSecurityPolicyImpl::CheckOriginLockResult;

namespace {

// Stores the maximum number of renderer processes the content module can
// create. Only applies if it is set to a non-zero value.
size_t g_max_renderer_count_override = 0;

bool g_run_renderer_in_process = false;

RendererMainThreadFactoryFunction g_renderer_main_thread_factory = nullptr;
RenderProcessHostImpl::CreateStoragePartitionServiceFunction
    g_create_storage_partition = nullptr;

base::MessageLoop* g_in_process_thread;

const RenderProcessHostFactory* g_render_process_host_factory_ = nullptr;
const char kSiteProcessMapKeyName[] = "content_site_process_map";

RenderProcessHost::AnalyzeHungRendererFunction g_analyze_hung_renderer =
    nullptr;

const base::FilePath::CharType kAecDumpFileNameAddition[] =
    FILE_PATH_LITERAL("aec_dump");

void CacheShaderInfo(int32_t id, base::FilePath path) {
  if (GetShaderCacheFactorySingleton())
    GetShaderCacheFactorySingleton()->SetCacheInfo(id, path);
}

void RemoveShaderInfo(int32_t id) {
  if (GetShaderCacheFactorySingleton())
    GetShaderCacheFactorySingleton()->RemoveCacheInfo(id);
}

net::URLRequestContext* GetRequestContext(
    scoped_refptr<net::URLRequestContextGetter> request_context,
    scoped_refptr<net::URLRequestContextGetter> media_request_context,
    ResourceType resource_type) {
  // If the request has resource type of RESOURCE_TYPE_MEDIA, we use a request
  // context specific to media for handling it because these resources have
  // specific needs for caching.
  if (resource_type == RESOURCE_TYPE_MEDIA)
    return media_request_context->GetURLRequestContext();
  return request_context->GetURLRequestContext();
}

void GetContexts(
    ResourceContext* resource_context,
    scoped_refptr<net::URLRequestContextGetter> request_context,
    scoped_refptr<net::URLRequestContextGetter> media_request_context,
    ResourceType resource_type,
    ResourceContext** resource_context_out,
    net::URLRequestContext** request_context_out) {
  *resource_context_out = resource_context;
  *request_context_out =
      GetRequestContext(request_context, media_request_context, resource_type);
}

// Creates a file used for handing over to the renderer.
IPC::PlatformFileForTransit CreateFileForProcess(base::FilePath file_path) {
  base::File dump_file(file_path,
                       base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_APPEND);
  if (!dump_file.IsValid()) {
    VLOG(1) << "Could not open AEC dump file, error="
            << dump_file.error_details();
    return IPC::InvalidPlatformFileForTransit();
  }
  return IPC::TakePlatformFileForTransit(std::move(dump_file));
}

// Allow us to only run the trial in the first renderer.
bool has_done_stun_trials = false;

// Globally tracks all existing RenderProcessHostImpl instances.
//
// TODO(https://crbug.com/813045): Remove this.
class RenderProcessMemoryDumpProvider
    : public base::trace_event::MemoryDumpProvider {
 public:
  RenderProcessMemoryDumpProvider() {
    base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
        this, "RenderProcessHost", base::ThreadTaskRunnerHandle::Get());
  }

  ~RenderProcessMemoryDumpProvider() override {
    base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
        this);
  }

  void AddHost(RenderProcessHostImpl* host) {
    hosts_.emplace(host, base::Time::Now());
  }

  void RemoveHost(RenderProcessHostImpl* host) { hosts_.erase(host); }

 private:
  // base::trace_event::MemoryDumpProvider:
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override {
    for (auto& iter : hosts_) {
      auto* host = iter.first;
      base::trace_event::MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(
          base::StringPrintf("mojo/render_process_host/0x%" PRIxPTR,
                             reinterpret_cast<uintptr_t>(host)));
      dump->AddScalar("is_initialized",
                      base::trace_event::MemoryAllocatorDump::kUnitsObjects,
                      host->is_initialized() ? 1 : 0);
      dump->AddScalar("age",
                      base::trace_event::MemoryAllocatorDump::kUnitsObjects,
                      (base::Time::Now() - iter.second).InSeconds());
    }
    return true;
  }

  std::map<RenderProcessHostImpl*, base::Time> hosts_;

  DISALLOW_COPY_AND_ASSIGN(RenderProcessMemoryDumpProvider);
};

RenderProcessMemoryDumpProvider& GetMemoryDumpProvider() {
  static base::NoDestructor<RenderProcessMemoryDumpProvider> tracker;
  return *tracker;
}

// the global list of all renderer processes
base::LazyInstance<base::IDMap<RenderProcessHost*>>::Leaky g_all_hosts =
    LAZY_INSTANCE_INITIALIZER;

// Map of site to process, to ensure we only have one RenderProcessHost per
// site in process-per-site mode.  Each map is specific to a BrowserContext.
class SiteProcessMap : public base::SupportsUserData::Data {
 public:
  typedef base::hash_map<std::string, RenderProcessHost*> SiteToProcessMap;
  SiteProcessMap() {}

  void RegisterProcess(const std::string& site, RenderProcessHost* process) {
    // There could already exist a site to process mapping due to races between
    // two WebContents with blank SiteInstances. If that occurs, keeping the
    // exising entry and not overwriting it is a predictable behavior that is
    // safe.
    auto i = map_.find(site);
    if (i == map_.end())
      map_[site] = process;
  }

  RenderProcessHost* FindProcess(const std::string& site) {
    auto i = map_.find(site);
    if (i != map_.end())
      return i->second;
    return nullptr;
  }

  void RemoveProcess(RenderProcessHost* host) {
    // Find all instances of this process in the map, then separately remove
    // them.
    std::set<std::string> sites;
    for (SiteToProcessMap::const_iterator i = map_.begin(); i != map_.end();
         ++i) {
      if (i->second == host)
        sites.insert(i->first);
    }
    for (auto i = sites.begin(); i != sites.end(); ++i) {
      auto iter = map_.find(*i);
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

// NOTE: changes to this class need to be reviewed by the security team.
class RendererSandboxedProcessLauncherDelegate
    : public SandboxedProcessLauncherDelegate {
 public:
  RendererSandboxedProcessLauncherDelegate() {}

  ~RendererSandboxedProcessLauncherDelegate() override {}

#if defined(OS_WIN)
  bool PreSpawnTarget(sandbox::TargetPolicy* policy) override {
    service_manager::SandboxWin::AddBaseHandleClosePolicy(policy);

    const base::string16& sid =
        GetContentClient()->browser()->GetAppContainerSidForSandboxType(
            GetSandboxType());
    if (!sid.empty())
      service_manager::SandboxWin::AddAppContainerPolicy(policy, sid.c_str());

    return GetContentClient()->browser()->PreSpawnRenderer(policy);
  }
#endif  // OS_WIN

#if BUILDFLAG(USE_ZYGOTE_HANDLE)
  service_manager::ZygoteHandle GetZygote() override {
    const base::CommandLine& browser_command_line =
        *base::CommandLine::ForCurrentProcess();
    base::CommandLine::StringType renderer_prefix =
        browser_command_line.GetSwitchValueNative(switches::kRendererCmdPrefix);
    if (!renderer_prefix.empty())
      return nullptr;
    return service_manager::GetGenericZygote();
  }
#endif  // BUILDFLAG(USE_ZYGOTE_HANDLE)

  service_manager::SandboxType GetSandboxType() override {
    return service_manager::SANDBOX_TYPE_RENDERER;
  }
};

const char kSessionStorageHolderKey[] = "kSessionStorageHolderKey";

class SessionStorageHolder : public base::SupportsUserData::Data {
 public:
  SessionStorageHolder()
      : session_storage_namespaces_awaiting_close_(
            new std::map<int, SessionStorageNamespaceMap>) {
  }

  ~SessionStorageHolder() override {
    // Its important to delete the map on the IO thread to avoid deleting
    // the underlying namespaces prior to processing ipcs referring to them.
    BrowserThread::DeleteSoon(
        BrowserThread::IO, FROM_HERE,
        session_storage_namespaces_awaiting_close_.release());
  }

  void Hold(const SessionStorageNamespaceMap& sessions, int widget_route_id) {
    (*session_storage_namespaces_awaiting_close_)[widget_route_id] = sessions;
  }

  void Release(int old_route_id) {
    session_storage_namespaces_awaiting_close_->erase(old_route_id);
  }

 private:
  std::unique_ptr<std::map<int, SessionStorageNamespaceMap>>
      session_storage_namespaces_awaiting_close_;
  DISALLOW_COPY_AND_ASSIGN(SessionStorageHolder);
};

// This class manages spare RenderProcessHosts.
//
// There is a singleton instance of this class which manages a single spare
// renderer (g_spare_render_process_host_manager, below). This class
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
// used as a guest renderer (is_for_guests_ == true).
//
// It is safe to call WarmupSpareRenderProcessHost multiple times, although if
// called in a context where the spare renderer is not likely to be used
// performance may suffer due to the unnecessary RPH creation.
class SpareRenderProcessHostManager : public RenderProcessHostObserver {
 public:
  SpareRenderProcessHostManager() {}

  void WarmupSpareRenderProcessHost(BrowserContext* browser_context) {
    if (spare_render_process_host_ &&
        spare_render_process_host_->GetBrowserContext() == browser_context) {
      DCHECK_EQ(BrowserContext::GetDefaultStoragePartition(browser_context),
                spare_render_process_host_->GetStoragePartition());
      return;  // Nothing to warm up.
    }

    CleanupSpareRenderProcessHost();

    // Don't create a spare renderer if we're using --single-process or if we've
    // got too many processes. See also ShouldTryToUseExistingProcessHost in
    // this file.
    if (RenderProcessHost::run_renderer_in_process() ||
        g_all_hosts.Get().size() >=
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
        browser_context, nullptr /* storage_partition_impl */,
        nullptr /* site_instance */, false /* is_for_guests_only */);
    spare_render_process_host_->AddObserver(this);
    spare_render_process_host_->Init();
  }

  RenderProcessHost* MaybeTakeSpareRenderProcessHost(
      BrowserContext* browser_context,
      SiteInstanceImpl* site_instance,
      bool is_for_guests_only) {
    // Give embedder a chance to disable using a spare RenderProcessHost for
    // certain SiteInstances.  Some navigations, such as to NTP or extensions,
    // require passing command-line flags to the renderer process at process
    // launch time, but this cannot be done for spare RenderProcessHosts, which
    // are started before it is known which navigation might use them.  So, a
    // spare RenderProcessHost should not be used in such cases.
    bool embedder_allows_spare_usage =
        GetContentClient()->browser()->ShouldUseSpareRenderProcessHost(
            browser_context, site_instance->GetSiteURL());

    bool site_instance_allows_spare_usage =
        site_instance->CanAssociateWithSpareProcess();

    // Get the StoragePartition for |site_instance|.  Note that this might be
    // different than the default StoragePartition for |browser_context|.
    StoragePartition* site_storage =
        BrowserContext::GetStoragePartition(browser_context, site_instance);

    // Log UMA metrics.
    using SpareProcessMaybeTakeAction =
        RenderProcessHostImpl::SpareProcessMaybeTakeAction;
    SpareProcessMaybeTakeAction action =
        SpareProcessMaybeTakeAction::kNoSparePresent;
    if (!spare_render_process_host_)
      action = SpareProcessMaybeTakeAction::kNoSparePresent;
    else if (browser_context != spare_render_process_host_->GetBrowserContext())
      action = SpareProcessMaybeTakeAction::kMismatchedBrowserContext;
    else if (site_storage != spare_render_process_host_->GetStoragePartition())
      action = SpareProcessMaybeTakeAction::kMismatchedStoragePartition;
    else if (!embedder_allows_spare_usage)
      action = SpareProcessMaybeTakeAction::kRefusedByEmbedder;
    else if (!site_instance_allows_spare_usage)
      action = SpareProcessMaybeTakeAction::kRefusedBySiteInstance;
    else
      action = SpareProcessMaybeTakeAction::kSpareTaken;
    UMA_HISTOGRAM_ENUMERATION(
        "BrowserRenderProcessHost.SpareProcessMaybeTakeAction", action);

    // Decide whether to take or drop the spare process.
    RenderProcessHost* returned_process = nullptr;
    if (spare_render_process_host_ &&
        browser_context == spare_render_process_host_->GetBrowserContext() &&
        site_storage == spare_render_process_host_->GetStoragePartition() &&
        !is_for_guests_only && embedder_allows_spare_usage &&
        site_instance_allows_spare_usage) {
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
    } else if (g_all_hosts.Get().size() >=
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
      if (!spare_render_process_host_->IsKeepAliveRefCountDisabled())
        spare_render_process_host_->Cleanup();

      // Drop reference to the RenderProcessHost object.
      spare_render_process_host_ = nullptr;
    }
  }

  RenderProcessHost* spare_render_process_host() {
    return spare_render_process_host_;
  }

 private:
  // Release ownership of |host| as a possible spare renderer.  Called when
  // |host| has either been 1) claimed to be used in a navigation or 2) shutdown
  // somewhere else.
  void ReleaseSpareRenderProcessHost(RenderProcessHost* host) {
    if (spare_render_process_host_ && spare_render_process_host_ == host) {
      spare_render_process_host_->RemoveObserver(this);
      spare_render_process_host_ = nullptr;
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

  // This is a bare pointer, because RenderProcessHost manages the lifetime of
  // all its instances; see g_all_hosts, above.
  RenderProcessHost* spare_render_process_host_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(SpareRenderProcessHostManager);
};

base::LazyInstance<SpareRenderProcessHostManager>::Leaky
    g_spare_render_process_host_manager = LAZY_INSTANCE_INITIALIZER;

const void* const kDefaultSubframeProcessHostHolderKey =
    &kDefaultSubframeProcessHostHolderKey;

class DefaultSubframeProcessHostHolder : public base::SupportsUserData::Data,
                                         public RenderProcessHostObserver {
 public:
  explicit DefaultSubframeProcessHostHolder(BrowserContext* browser_context)
      : browser_context_(browser_context) {}
  ~DefaultSubframeProcessHostHolder() override {}

  // Gets the correct render process to use for this SiteInstance.
  RenderProcessHost* GetProcessHost(SiteInstance* site_instance,
                                    bool is_for_guests_only) {
    StoragePartitionImpl* default_partition =
        static_cast<StoragePartitionImpl*>(
            BrowserContext::GetDefaultStoragePartition(browser_context_));
    StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
        BrowserContext::GetStoragePartition(browser_context_, site_instance));

    // Is this the default storage partition? If it isn't, then just give it its
    // own non-shared process.
    if (partition != default_partition || is_for_guests_only) {
      RenderProcessHost* host = RenderProcessHostImpl::CreateRenderProcessHost(
          browser_context_, partition, site_instance, is_for_guests_only);
      host->SetIsNeverSuitableForReuse();
      return host;
    }

    // If we already have a shared host for the default storage partition, use
    // it.
    if (host_)
      return host_;

    host_ = RenderProcessHostImpl::CreateRenderProcessHost(
        browser_context_, partition, site_instance,
        false /* is for guests only */);
    host_->SetIsNeverSuitableForReuse();
    host_->AddObserver(this);

    return host_;
  }

  // Implementation of RenderProcessHostObserver.
  void RenderProcessHostDestroyed(RenderProcessHost* host) override {
    DCHECK_EQ(host_, host);
    host_->RemoveObserver(this);
    host_ = nullptr;
  }

 private:
  BrowserContext* browser_context_;

  // The default subframe render process used for the default storage partition
  // of this BrowserContext.
  RenderProcessHost* host_ = nullptr;
};

void CreateProcessResourceCoordinator(
    RenderProcessHostImpl* render_process_host,
    resource_coordinator::mojom::ProcessCoordinationUnitRequest request) {
  render_process_host->GetProcessResourceCoordinator()->AddBinding(
      std::move(request));
}

// Forwards service requests to Service Manager since the renderer cannot launch
// out-of-process services on is own.
template <typename Interface>
void ForwardRequest(const char* service_name,
                    mojo::InterfaceRequest<Interface> request) {
  // TODO(beng): This should really be using the per-profile connector.
  service_manager::Connector* connector =
      ServiceManagerConnection::GetForProcess()->GetConnector();
  connector->BindInterface(service_name, std::move(request));
}

class RenderProcessHostIsReadyObserver : public RenderProcessHostObserver {
 public:
  RenderProcessHostIsReadyObserver(RenderProcessHost* render_process_host,
                                   base::OnceClosure task)
      : render_process_host_(render_process_host),
        task_(std::move(task)),
        weak_factory_(this) {
    render_process_host_->AddObserver(this);
    if (render_process_host_->IsReady())
      PostTask();
  }

  ~RenderProcessHostIsReadyObserver() override {
    render_process_host_->RemoveObserver(this);
  }

  void RenderProcessReady(RenderProcessHost* host) override { PostTask(); }

  void RenderProcessHostDestroyed(RenderProcessHost* host) override {
    delete this;
  }

 private:
  void PostTask() {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&RenderProcessHostIsReadyObserver::CallTask,
                       weak_factory_.GetWeakPtr()));
  }

  void CallTask() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (render_process_host_->IsReady())
      std::move(task_).Run();

    delete this;
  }

  RenderProcessHost* render_process_host_;
  base::OnceClosure task_;
  base::WeakPtrFactory<RenderProcessHostIsReadyObserver> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RenderProcessHostIsReadyObserver);
};

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
//
// TODO(alexmos): Currently, the tracking in this class and in
// UnmatchedServiceWorkerProcessTracker is associated with a BrowserContext,
// but it needs to also consider StoragePartitions, so that process reuse is
// allowed only within the same StoragePartition.  For now, the tracking is
// done only for the default StoragePartition.  See https://crbug.com/752667.
const void* const kCommittedSiteProcessCountTrackerKey =
    "CommittedSiteProcessCountTrackerKey";
const void* const kPendingSiteProcessCountTrackerKey =
    "PendingSiteProcessCountTrackerKey";
class SiteProcessCountTracker : public base::SupportsUserData::Data,
                                public RenderProcessHostObserver {
 public:
  SiteProcessCountTracker() {}
  ~SiteProcessCountTracker() override { DCHECK(map_.empty()); }

  void IncrementSiteProcessCount(const GURL& site_url,
                                 int render_process_host_id) {
    std::map<ProcessID, Count>& counts_per_process = map_[site_url];
    ++counts_per_process[render_process_host_id];

#ifndef NDEBUG
    // In debug builds, observe the RenderProcessHost destruction, to check
    // that it is properly removed from the map.
    RenderProcessHost* host = RenderProcessHost::FromID(render_process_host_id);
    if (!HasProcess(host))
      host->AddObserver(this);
#endif
  }

  void DecrementSiteProcessCount(const GURL& site_url,
                                 int render_process_host_id) {
    auto result = map_.find(site_url);
    DCHECK(result != map_.end());
    std::map<ProcessID, Count>& counts_per_process = result->second;

    --counts_per_process[render_process_host_id];
    DCHECK_GE(counts_per_process[render_process_host_id], 0);

    if (counts_per_process[render_process_host_id] == 0)
      counts_per_process.erase(render_process_host_id);

    if (counts_per_process.empty())
      map_.erase(site_url);
  }

  void FindRenderProcessesForSiteInstance(
      SiteInstanceImpl* site_instance,
      std::set<RenderProcessHost*>* foreground_processes,
      std::set<RenderProcessHost*>* background_processes) {
    auto result = map_.find(site_instance->GetSiteURL());
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

      // It's possible that |host| has become unsuitable for hosting
      // |site_url|, for example if it was reused by a navigation to a
      // different site, and |site_url| requires a dedicated process.  Do not
      // allow such hosts to be reused.  See https://crbug.com/780661.
      if (!host->MayReuseHost() ||
          !RenderProcessHostImpl::IsSuitableHost(
              host, host->GetBrowserContext(), site_instance->GetSiteURL(),
              site_instance->lock_url())) {
        continue;
      }

      if (host->VisibleClientCount())
        foreground_processes->insert(host);
      else
        background_processes->insert(host);
    }
  }

 private:
  void RenderProcessHostDestroyed(RenderProcessHost* host) override {
#ifndef NDEBUG
    host->RemoveObserver(this);
    DCHECK(!HasProcess(host));
#endif
  }

#ifndef NDEBUG
  // Used in debug builds to ensure that RenderProcessHost don't persist in the
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

  using ProcessID = int;
  using Count = int;
  using CountPerProcessPerSiteMap = std::map<GURL, std::map<ProcessID, Count>>;
  CountPerProcessPerSiteMap map_;
};

bool ShouldUseSiteProcessTracking(BrowserContext* browser_context,
                                  StoragePartition* dest_partition,
                                  const GURL& site_url) {
  // TODO(alexmos): Sites should be tracked separately for each
  // StoragePartition.  For now, track them only in the default one.
  StoragePartition* default_partition =
      BrowserContext::GetDefaultStoragePartition(browser_context);
  if (dest_partition != default_partition)
    return false;

  return true;
}

bool ShouldTrackProcessForSite(BrowserContext* browser_context,
                               RenderProcessHost* render_process_host,
                               const GURL& site_url) {
  if (site_url.is_empty())
    return false;

  return ShouldUseSiteProcessTracking(
      browser_context, render_process_host->GetStoragePartition(), site_url);
}

bool ShouldFindReusableProcessHostForSite(BrowserContext* browser_context,
                                          const GURL& site_url) {
  if (site_url.is_empty())
    return false;

  return ShouldUseSiteProcessTracking(
      browser_context,
      BrowserContext::GetStoragePartitionForSite(browser_context, site_url),
      site_url);
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
//
// TODO(alexmos): Currently, the tracking in this class and in
// SiteProcessCountTracker is associated with a BrowserContext, but it needs to
// also consider StoragePartitions, so that process reuse is allowed only
// within the same StoragePartition.  For now, the tracking is done only for
// the default StoragePartition.  See https://crbug.com/752667.
class UnmatchedServiceWorkerProcessTracker
    : public base::SupportsUserData::Data,
      public RenderProcessHostObserver {
 public:
  // Registers |render_process_host| as having an unmatched service worker for
  // |site_instance|.
  static void Register(RenderProcessHost* render_process_host,
                       SiteInstanceImpl* site_instance) {
    BrowserContext* browser_context = site_instance->GetBrowserContext();
    DCHECK(!site_instance->GetSiteURL().is_empty());
    if (!ShouldTrackProcessForSite(browser_context, render_process_host,
                                   site_instance->GetSiteURL()))
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
    if (!ShouldFindReusableProcessHostForSite(browser_context,
                                              site_instance->GetSiteURL()))
      return nullptr;

    UnmatchedServiceWorkerProcessTracker* tracker =
        static_cast<UnmatchedServiceWorkerProcessTracker*>(
            browser_context->GetUserData(
                kUnmatchedServiceWorkerProcessTrackerKey));
    if (!tracker)
      return nullptr;
    return tracker->TakeFreshestProcessForSite(site_instance);
  }

  UnmatchedServiceWorkerProcessTracker() {}

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
  void RegisterProcessForSite(RenderProcessHost* host,
                              SiteInstanceImpl* site_instance) {
    if (!HasProcess(host))
      host->AddObserver(this);
    site_process_set_.insert(
        SiteProcessIDPair(site_instance->GetSiteURL(), host->GetID()));
  }

  RenderProcessHost* TakeFreshestProcessForSite(
      SiteInstanceImpl* site_instance) {
    RenderProcessHost* host = FindFreshestProcessForSite(site_instance);
    if (!host)
      return nullptr;

    // It's possible that |host| is currently unsuitable for hosting
    // |site_url|, for example if it was used for a ServiceWorker for a
    // nonexistent extension URL.  See https://crbug.com/782349 and
    // https://crbug.com/780661.
    GURL site_url(site_instance->GetSiteURL());
    if (!host->MayReuseHost() || !RenderProcessHostImpl::IsSuitableHost(
                                     host, host->GetBrowserContext(), site_url,
                                     site_instance->lock_url()))
      return nullptr;

    site_process_set_.erase(SiteProcessIDPair(site_url, host->GetID()));
    if (!HasProcess(host))
      host->RemoveObserver(this);
    return host;
  }

  RenderProcessHost* FindFreshestProcessForSite(
      SiteInstanceImpl* site_instance) const {
    GURL site_url(site_instance->GetSiteURL());
    for (const auto& site_process_pair : base::Reversed(site_process_set_)) {
      if (site_process_pair.first == site_url)
        return RenderProcessHost::FromID(site_process_pair.second);
    }
    return nullptr;
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

  using ProcessID = int;
  using SiteProcessIDPair = std::pair<GURL, ProcessID>;
  using SiteProcessIDPairSet = std::set<SiteProcessIDPair>;

  // Use std::set because duplicates don't need to be tracked separately (eg.,
  // service workers for the same site in the same process). It is sorted in the
  // order of insertion.
  SiteProcessIDPairSet site_process_set_;
};

bool ShouldBoostPriorityForPendingViews() {
#if defined(OS_ANDROID)
  // On Android, renderer processes with pending views get an extra boost.
  return true;
#else
  // On desktop platforms, new renderer processes have been considered
  // foreground regardless of visibility since r385608. This is because it was
  // previously discovered that backgrounding processes that weren't responsible
  // for visible content resulted in running foreground navigations at
  // background priority until the main frame was committed (and became a
  // visible widget)... Thus new processes responsible for hidden content are
  // now foreground until explicitly made visible and hidden again. Full details
  // @ https://crbug.com/560446.
  //
  // The experiment below will attempt to use the new "pending views" signal to
  // go back to pre r385608 behavior while still boosting priority of processes
  // with pending views (until widgets are created and visibility kicks in).
  // This will however not keep the nice side-effect of r385608 which was to
  // boost new page loads happening in background tabs (example use case of a
  // foreground action in a background tab : ctrl+click link of interest and
  // actively wait for spinner to stop before switching tabs). If we decide that
  // the latter use case is relevant, we should support it cross-platform.
  //
  // Thus, while this is a "boosting" experiment, it's really a "consider the
  // initial foreground state a boost so we can unboost and background sooner".
  //
  // Returning true here triggers the experiment because it will cause the
  // initial |ChildProcessLauncherPriority::boost_for_pending_views| value to be
  // |true| which will in turn result in a call to UpdateProcessPriority() when
  // RemovePendingView() is invoked (without the experiment the initial state
  // has no pending view and although UpdateProcessPriority() is invoked from
  // RemovePendingView(), it no-ops per "no change" compared to the initial
  // ChildProcessLauncherPriority).
  //
  // Furthermore, it has been discovered that there is a priority inversion in
  // foreground navigation without this experiment, the reason being that
  // ChildProcessLauncherPriority::operator==() notices a difference in
  // UpdateProcessPriority() when only |boost_for_pending_views| has changed. On
  // desktop it would start in the |false| state and AddPendingView() would
  // toggle it. This would result in UpdateProcessPriority() but desktop not
  // handling the presence of |boost_for_pending_views|, would look at |!visible
  // && !has_media_stream| and decide that it's time to background (bringing
  // back the OP of https://crbug.com/560446 but worse because actual background
  // tabs are running as foreground per r385608 -- so during a big session
  // restore it's possible to end up with all processes foreground except for
  // the foreground tab's process...). Full details @
  // https://crbug.com/560446#c74.
  //
  // Hence this experiment is definitely the desired behavior, running it as an
  // experiment on Canary merely to see what was impacted by existing bugs.
  // TODO(gab): End the experiment and turn ShouldBoostPriorityForPendingViews()
  // into a constant when results are in.
  static bool should_boost_for_pending_views =
      !base::StartsWith(base::FieldTrialList::FindFullName(
                            "BoostRendererPriorityForPendingViews"),
                        "Disabled", base::CompareCase::SENSITIVE);
  return should_boost_for_pending_views;
#endif
}

void CopyFeatureSwitch(const base::CommandLine& src,
                       base::CommandLine* dest,
                       const char* switch_name) {
  std::vector<std::string> features = FeaturesFromSwitch(src, switch_name);
  if (!features.empty())
    dest->AppendSwitchASCII(switch_name, base::JoinString(features, ","));
}

std::set<int>& GetCurrentCorbPluginExceptions() {
  static base::NoDestructor<std::set<int>> s_data;
  return *s_data;
}

void OnNetworkServiceCrashForCorb() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  network::mojom::NetworkService* network_service = GetNetworkService();
  for (int process_id : GetCurrentCorbPluginExceptions())
    network_service->AddCorbExceptionForPlugin(process_id);
}

void RemoveCorbExceptionForPluginOnIOThread(int process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Without NetworkService the exception list is stored directly in the browser
  // process.
  if (!base::FeatureList::IsEnabled(network::features::kNetworkService))
    network::CrossOriginReadBlocking::RemoveExceptionForPlugin(process_id);
}

// This is the entry point (i.e. this is called on the UI thread *before*
// we post a task for RemoveCorbExceptionForPluginOnIOThread).
void RemoveCorbExceptionForPluginOnUIThread(int process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    GetCurrentCorbPluginExceptions().erase(process_id);
    GetNetworkService()->RemoveCorbExceptionForPlugin(process_id);
  } else {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&RemoveCorbExceptionForPluginOnIOThread, process_id));
  }
}

void AddCorbExceptionForPluginOnUIThread(int process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RenderProcessHost* process = RenderProcessHostImpl::FromID(process_id);
  if (!process) {
    // In this case the exception won't be added via NetworkService (because of
    // the early return below), but we need to proactively do clean-up on IO
    // thread.
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&RemoveCorbExceptionForPluginOnIOThread, process_id));
    return;
  }
  process->CleanupCorbExceptionForPluginUponDestruction();

  if (base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    static NetworkServiceCrashHandlerId s_crash_handler_id;
    if (s_crash_handler_id.is_null()) {
      s_crash_handler_id = RegisterNetworkServiceCrashHandler(
          base::BindRepeating(&OnNetworkServiceCrashForCorb));
    }

    GetCurrentCorbPluginExceptions().insert(process_id);
    GetNetworkService()->AddCorbExceptionForPlugin(process_id);
  }
}

// This is the entry point (i.e. this is called on the IO thread *before*
// we post a task for AddCorbExceptionForPluginOnUIThread).
void AddCorbExceptionForPluginOnIOThread(int process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Without NetworkService the exception list is stored directly in the browser
  // process.
  if (!base::FeatureList::IsEnabled(network::features::kNetworkService))
    network::CrossOriginReadBlocking::AddExceptionForPlugin(process_id);

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&AddCorbExceptionForPluginOnUIThread, process_id));
}

}  // namespace

// Held by the RPH and used to control an (unowned) ConnectionFilterImpl from
// any thread.
class RenderProcessHostImpl::ConnectionFilterController
    : public base::RefCountedThreadSafe<ConnectionFilterController> {
 public:
  // |filter| is not owned by this object.
  explicit ConnectionFilterController(ConnectionFilterImpl* filter)
      : filter_(filter) {}

  void DisableFilter();

 private:
  friend class base::RefCountedThreadSafe<ConnectionFilterController>;
  friend class ConnectionFilterImpl;

  ~ConnectionFilterController() {}

  void Detach() {
    base::AutoLock lock(lock_);
    filter_ = nullptr;
  }

  base::Lock lock_;
  ConnectionFilterImpl* filter_ PT_GUARDED_BY(lock_);
};

// Held by the RPH's BrowserContext's ServiceManagerConnection, ownership
// transferred back to RPH upon RPH destruction.
class RenderProcessHostImpl::ConnectionFilterImpl : public ConnectionFilter {
 public:
  ConnectionFilterImpl(
      const service_manager::Identity& child_identity,
      std::unique_ptr<service_manager::BinderRegistry> registry)
      : child_identity_(child_identity),
        registry_(std::move(registry)),
        controller_(new ConnectionFilterController(this)),
        weak_factory_(this) {
    // Registration of this filter may race with browser shutdown, in which case
    // it's possible for this filter to be destroyed on the main thread. This
    // is fine as long as the filter hasn't been used on the IO thread yet. We
    // detach the ThreadChecker initially and the first use of the filter will
    // bind it.
    thread_checker_.DetachFromThread();
  }

  ~ConnectionFilterImpl() override {
    DCHECK(thread_checker_.CalledOnValidThread());
    controller_->Detach();
  }

  scoped_refptr<ConnectionFilterController> controller() { return controller_; }

  void Disable() {
    base::AutoLock lock(enabled_lock_);
    enabled_ = false;
  }

 private:
  // ConnectionFilter:
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle* interface_pipe,
                       service_manager::Connector* connector) override {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    // We only fulfill connections from the renderer we host.
    if (child_identity_.name() != source_info.identity.name() ||
        child_identity_.instance() != source_info.identity.instance()) {
      return;
    }

    base::AutoLock lock(enabled_lock_);
    if (!enabled_)
      return;

    registry_->TryBindInterface(interface_name, interface_pipe);
  }

  base::ThreadChecker thread_checker_;
  service_manager::Identity child_identity_;
  std::unique_ptr<service_manager::BinderRegistry> registry_;
  scoped_refptr<ConnectionFilterController> controller_;

  base::Lock enabled_lock_;
  bool enabled_ GUARDED_BY(enabled_lock_) = true;

  base::WeakPtrFactory<ConnectionFilterImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ConnectionFilterImpl);
};

void RenderProcessHostImpl::ConnectionFilterController::DisableFilter() {
  base::AutoLock lock(lock_);
  if (filter_)
    filter_->Disable();
}

base::MessageLoop*
RenderProcessHostImpl::GetInProcessRendererThreadForTesting() {
  return g_in_process_thread;
}

// static
size_t RenderProcessHost::GetMaxRendererProcessCount() {
  if (g_max_renderer_count_override)
    return g_max_renderer_count_override;

#if defined(OS_ANDROID)
  // On Android we don't maintain a limit of renderer process hosts - we are
  // happy with keeping a lot of these, as long as the number of live renderer
  // processes remains reasonable, and on Android the OS takes care of that.
  return std::numeric_limits<size_t>::max();
#endif
#if defined(OS_CHROMEOS)
  // On Chrome OS new renderer processes are very cheap and there's no OS
  // driven constraint on the number of processes, and the effectiveness
  // of the tab discarder is very poor when we have tabs sharing a
  // renderer process.  So, set a high limit, and based on UMA stats
  // for CrOS the 99.9th percentile of Tabs.MaxTabsInADay is around 100.
  return 100;
#endif

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
  //   128 MB -> 1
  //   512 MB -> 4
  //  1024 MB -> 8
  //  4096 MB -> 34
  // 16384 MB -> 136
  //
  // Then the calculated value will be clamped by |kMinRendererProcessCount| and
  // |kMaxRendererProcessCount|.

  static size_t max_count = 0;
  if (!max_count) {
    static constexpr size_t kEstimatedWebContentsMemoryUsage =
#if defined(ARCH_CPU_64_BITS)
        60;  // In MB
#else
        40;  // In MB
#endif
    max_count = base::SysInfo::AmountOfPhysicalMemoryMB() / 2;
    max_count /= kEstimatedWebContentsMemoryUsage;

    static constexpr size_t kMinRendererProcessCount = 3;
    max_count = base::ClampToRange(max_count, kMinRendererProcessCount,
                                   kMaxRendererProcessCount);
  }
  return max_count;
}

// static
void RenderProcessHost::SetMaxRendererProcessCount(size_t count) {
  g_max_renderer_count_override = count;
  if (g_all_hosts.Get().size() > count)
    g_spare_render_process_host_manager.Get().CleanupSpareRenderProcessHost();
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
    StoragePartitionImpl* storage_partition_impl,
    SiteInstance* site_instance,
    bool is_for_guests_only) {
  if (g_render_process_host_factory_) {
    return g_render_process_host_factory_->CreateRenderProcessHost(
        browser_context, site_instance);
  }

  if (!storage_partition_impl) {
    storage_partition_impl = static_cast<StoragePartitionImpl*>(
        BrowserContext::GetStoragePartition(browser_context, site_instance));
  }
  // If we've made a StoragePartition for guests (e.g., for the <webview> tag),
  // stash the Site URL on it. This way, when we start a service worker inside
  // this storage partition, we can create the appropriate SiteInstance for
  // finding a process (e.g., we will try to start a worker from
  // "https://example.com/sw.js" but need to use the site URL
  // "chrome-guest://blahblah" to get a process in the guest's
  // StoragePartition.)
  if (is_for_guests_only && site_instance &&
      storage_partition_impl->site_for_service_worker().is_empty()) {
    storage_partition_impl->set_site_for_service_worker(
        site_instance->GetSiteURL());
  }

  return new RenderProcessHostImpl(browser_context, storage_partition_impl,
                                   is_for_guests_only);
}

// static
const unsigned int RenderProcessHostImpl::kMaxFrameDepthForPriority =
    std::numeric_limits<unsigned int>::max();

RenderProcessHostImpl::RenderProcessHostImpl(
    BrowserContext* browser_context,
    StoragePartitionImpl* storage_partition_impl,
    bool is_for_guests_only)
    : fast_shutdown_started_(false),
      deleting_soon_(false),
#ifndef NDEBUG
      is_self_deleted_(false),
#endif
      pending_views_(0),
      keep_alive_ref_count_(0),
      is_keep_alive_ref_count_disabled_(false),
      route_provider_binding_(this),
      visible_clients_(0),
      priority_(!blink::kLaunchingProcessIsBackgrounded,
                false /* has_media_stream */,
                frame_depth_,
                false /* intersects_viewport */,
                ShouldBoostPriorityForPendingViews(),
#if defined(OS_ANDROID)
                // Only use |boost_for_pending_views| to infer is_background()
                // on non-Android platforms for now to avoid changing the
                // old behavior while the experiment is under way.
                // Without this, the following tests were failing on Android
                // (they assume that toggling WidgetHidden() is sufficient to
                // toggle backgrounding):
                //   NavigationControllerBrowserTest.
                //     NoDialogsFromSwappedOutFrames
                //   SitePerProcessBrowserTest.
                //     CommitTimeoutForHungRenderer
                //     HiddenOOPIFWillNotGenerateCompositorFrames
                // TODO(gab): Clean this up as soon as the experiment is over.
                false
#else
                ShouldBoostPriorityForPendingViews()
#endif
#if defined(OS_ANDROID)
                ,
                ChildProcessImportance::NORMAL
#endif
                ),
      id_(ChildProcessHostImpl::GenerateChildProcessUniqueId()),
      browser_context_(browser_context),
      storage_partition_impl_(storage_partition_impl),
      sudden_termination_allowed_(true),
      ignore_input_events_(false),
      is_for_guests_only_(is_for_guests_only),
      is_unused_(true),
      gpu_observer_registered_(false),
      delayed_cleanup_needed_(false),
      within_process_died_observer_(false),
      permission_service_context_(new PermissionServiceContext(this)),
      indexed_db_factory_(new IndexedDBDispatcherHost(
          id_,
          storage_partition_impl_->GetIndexedDBContext(),
          ChromeBlobStorageContext::GetFor(browser_context_))),
      service_worker_dispatcher_host_(new ServiceWorkerDispatcherHost(
          storage_partition_impl_->GetServiceWorkerContext(),
          id_)),
      channel_connected_(false),
      sent_render_process_ready_(false),
#if defined(OS_ANDROID)
      never_signaled_(base::WaitableEvent::ResetPolicy::MANUAL,
                      base::WaitableEvent::InitialState::NOT_SIGNALED),
#endif
      renderer_host_binding_(this),
      instance_weak_factory_(
          new base::WeakPtrFactory<RenderProcessHostImpl>(this)),
      frame_sink_provider_(id_),
      weak_factory_(this) {
  for (size_t i = 0; i < kNumKeepAliveClients; i++)
    keep_alive_client_count_[i] = 0;

  widget_helper_ = new RenderWidgetHelper();

  ChildProcessSecurityPolicyImpl::GetInstance()->Add(GetID());

  CHECK(!BrowserMainRunner::ExitedMainMessageLoop());
  RegisterHost(GetID(), this);
  g_all_hosts.Get().set_check_on_null_data(true);
  // Initialize |child_process_activity_time_| to a reasonable value.
  mark_child_process_activity_time();

  if (!GetBrowserContext()->IsOffTheRecord() &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableGpuShaderDiskCache)) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&CacheShaderInfo, GetID(),
                       storage_partition_impl_->GetPath()));
  }

  push_messaging_manager_.reset(new PushMessagingManager(
      GetID(), storage_partition_impl_->GetServiceWorkerContext()));

  AddObserver(indexed_db_factory_.get());
  AddObserver(service_worker_dispatcher_host_.get());
#if defined(OS_MACOSX)
  AddObserver(MachBroker::GetInstance());
#endif

  InitializeChannelProxy();

  if (!features::IsMultiProcessMash()) {
    const int id = GetID();
    const uint64_t tracing_id =
        ChildProcessHostImpl::ChildProcessUniqueIdToTracingProcessId(id);
    gpu_client_.reset(new viz::GpuClient(
        std::make_unique<BrowserGpuClientDelegate>(), id, tracing_id,
        base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO})));
  }

  GetMemoryDumpProvider().AddHost(this);
}

// static
void RenderProcessHostImpl::ShutDownInProcessRenderer() {
  DCHECK(g_run_renderer_in_process);

  switch (g_all_hosts.Pointer()->size()) {
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

void RenderProcessHostImpl::SetCreateStoragePartitionServiceFunction(
    CreateStoragePartitionServiceFunction function) {
  g_create_storage_partition = function;
}

RenderProcessHostImpl::~RenderProcessHostImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
#ifndef NDEBUG
  DCHECK(is_self_deleted_)
      << "RenderProcessHostImpl is destroyed by something other than itself";
#endif

  // Make sure to clean up the in-process renderer before the channel, otherwise
  // it may still run and have its IPCs fail, causing asserts.
  in_process_renderer_.reset();

  ChildProcessSecurityPolicyImpl::GetInstance()->Remove(GetID());

  if (gpu_observer_registered_) {
    ui::GpuSwitchingManager::GetInstance()->RemoveObserver(this);
    gpu_observer_registered_ = false;
  }

  is_dead_ = true;

  UnregisterHost(GetID());

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableGpuShaderDiskCache)) {
    base::PostTaskWithTraits(FROM_HERE, {BrowserThread::IO},
                             base::BindOnce(&RemoveShaderInfo, GetID()));
  }

  GetMemoryDumpProvider().RemoveHost(this);

  if (cleanup_corb_exception_for_plugin_upon_destruction_)
    RemoveCorbExceptionForPluginOnUIThread(GetID());
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

#if defined(OS_LINUX)
  int flags = renderer_prefix.empty() ? ChildProcessHost::CHILD_ALLOW_SELF
                                      : ChildProcessHost::CHILD_NORMAL;
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

  if (gpu_client_)
    gpu_client_->PreEstablishGpuChannel();

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
  service_manager::mojom::ServiceRequest service_request;
  GetContentClient()->browser()->RenderProcessWillLaunch(this,
                                                         &service_request);
  if (service_request.is_pending()) {
    GetRendererInterface()->CreateEmbedderRendererService(
        std::move(service_request));
  }

#if !defined(OS_MACOSX)
  if (!BrowserMainLoop::GetInstance()->AudioServiceOutOfProcess()) {
    DCHECK(BrowserMainLoop::GetInstance()->audio_manager());
    // Intentionally delay the hang monitor creation after the first renderer
    // is created. On Mac audio thread is the UI thread, a hang monitor is not
    // necessary or recommended.
    media::AudioManager::StartHangMonitorIfNeeded(
        base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO}));
  }
#endif  // !defined(OS_MACOSX)

#if defined(OS_ANDROID)
  // Initialize the java audio manager so that media session tests will pass.
  // See internal b/29872494.
  static_cast<media::AudioManagerAndroid*>(media::AudioManager::Get())->
      InitializeIfNeeded();
#endif  // defined(OS_ANDROID)

  CreateMessageFilters();
  RegisterMojoInterfaces();

  if (run_renderer_in_process()) {
    DCHECK(g_renderer_main_thread_factory);
    // Crank up a thread and run the initialization there.  With the way that
    // messages flow between the browser and renderer, this thread is required
    // to prevent a deadlock in single-process mode.  Since the primordial
    // thread in the renderer process runs the WebKit code and can sometimes
    // make blocking calls to the UI thread (i.e. this thread), they need to run
    // on separate threads.
    in_process_renderer_.reset(
        g_renderer_main_thread_factory(InProcessChildThreadParams(
            base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO}),
            &mojo_invitation_, child_connection_->service_token())));

    base::Thread::Options options;
#if defined(OS_WIN) && !defined(OS_MACOSX)
    // In-process plugins require this to be a UI message loop.
    options.message_loop_type = base::MessageLoop::TYPE_UI;
#else
    // We can't have multiple UI loops on Linux and Android, so we don't support
    // in-process plugins.
    options.message_loop_type = base::MessageLoop::TYPE_DEFAULT;
#endif
    // As for execution sequence, this callback should have no any dependency
    // on starting in-process-render-thread.
    // So put it here to trigger ChannelMojo initialization earlier to enable
    // in-process-render-thread using ChannelMojo there.
    OnProcessLaunched();  // Fake a callback that the process is ready.

    in_process_renderer_->StartWithOptions(options);

    g_in_process_thread = in_process_renderer_->message_loop();

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

    // Spawn the child process asynchronously to avoid blocking the UI thread.
    // As long as there's no renderer prefix, we can use the zygote process
    // at this stage.
    child_process_launcher_ = std::make_unique<ChildProcessLauncher>(
        std::make_unique<RendererSandboxedProcessLauncherDelegate>(),
        std::move(cmd_line), GetID(), this, std::move(mojo_invitation_),
        base::BindRepeating(&RenderProcessHostImpl::OnMojoError, id_));
    channel_->Pause();

    fast_shutdown_started_ = false;
  }

  if (!gpu_observer_registered_) {
    gpu_observer_registered_ = true;
    ui::GpuSwitchingManager::GetInstance()->AddObserver(this);
  }

  init_time_ = base::TimeTicks::Now();
  return true;
}

void RenderProcessHostImpl::EnableSendQueue() {
  if (!channel_)
    InitializeChannelProxy();
}

void RenderProcessHostImpl::InitializeChannelProxy() {
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner =
      base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO});

  // Acquire a Connector which will route connections to a new instance of the
  // renderer service.
  service_manager::Connector* connector =
      BrowserContext::GetConnectorFor(browser_context_);
  if (!connector) {
    // Note that some embedders (e.g. Android WebView) may not initialize a
    // Connector per BrowserContext. In those cases we fall back to the
    // browser-wide Connector.
    if (!ServiceManagerConnection::GetForProcess()) {
      // Additionally, some test code may not initialize the process-wide
      // ServiceManagerConnection prior to this point. This class of test code
      // doesn't care about render processes, so we can initialize a dummy
      // connection.
      ServiceManagerConnection::SetForProcess(ServiceManagerConnection::Create(
          mojo::MakeRequest(&test_service_), io_task_runner));
    }
    connector = ServiceManagerConnection::GetForProcess()->GetConnector();
  }

  // Establish a ServiceManager connection for the new render service instance.
  mojo_invitation_ = {};
  service_manager::Identity child_identity(
      mojom::kRendererServiceName,
      BrowserContext::GetServiceUserIdFor(GetBrowserContext()),
      base::StringPrintf("%d_%d", id_, instance_id_++));
  child_connection_ = std::make_unique<ChildConnection>(
      child_identity, &mojo_invitation_, connector, io_task_runner);

  // Send an interface request to bootstrap the IPC::Channel. Note that this
  // request will happily sit on the pipe until the process is launched and
  // connected to the ServiceManager. We take the other end immediately and
  // plug it into a new ChannelProxy.
  mojo::MessagePipe pipe;
  BindInterface(IPC::mojom::ChannelBootstrap::Name_, std::move(pipe.handle1));
  std::unique_ptr<IPC::ChannelFactory> channel_factory =
      IPC::ChannelMojo::CreateServerFactory(
          std::move(pipe.handle0), io_task_runner,
          base::ThreadTaskRunnerHandle::Get());

  content::BindInterface(this, &child_control_interface_);

  ResetChannelProxy();

  // Do NOT expand ifdef or run time condition checks here! Synchronous
  // IPCs from browser process are banned. It is only narrowly allowed
  // for Android WebView to maintain backward compatibility.
  // See crbug.com/526842 for details.
#if defined(OS_ANDROID)
  if (GetContentClient()->UsingSynchronousCompositing()) {
    channel_ = IPC::SyncChannel::Create(this, io_task_runner.get(),
                                        base::ThreadTaskRunnerHandle::Get(),
                                        &never_signaled_);
  }
#endif  // OS_ANDROID
  if (!channel_) {
    channel_ = std::make_unique<IPC::ChannelProxy>(
        this, io_task_runner.get(), base::ThreadTaskRunnerHandle::Get());
  }
  channel_->Init(std::move(channel_factory), true /* create_pipe_now */);

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
  channel_->GetRemoteAssociatedInterface(&remote_route_provider_);
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
  // Add BrowserPluginMessageFilter to ensure it gets the first stab at messages
  // from guests.
  bp_message_filter_ = new BrowserPluginMessageFilter(GetID());
  AddFilter(bp_message_filter_.get());

  scoped_refptr<net::URLRequestContextGetter> request_context(
      storage_partition_impl_->GetURLRequestContext());
  scoped_refptr<RenderMessageFilter> render_message_filter =
      base::MakeRefCounted<RenderMessageFilter>(
          GetID(), GetBrowserContext(), request_context.get(),
          widget_helper_.get(), media_internals);
  AddFilter(render_message_filter.get());

  render_frame_message_filter_ = new RenderFrameMessageFilter(
      GetID(),
#if BUILDFLAG(ENABLE_PLUGINS)
      PluginServiceImpl::GetInstance(),
#else
      nullptr,
#endif
      GetBrowserContext(), storage_partition_impl_, widget_helper_.get());
  AddFilter(render_frame_message_filter_.get());

  BrowserContext* browser_context = GetBrowserContext();
  ResourceContext* resource_context = browser_context->GetResourceContext();

  scoped_refptr<net::URLRequestContextGetter> media_request_context(
      GetStoragePartition()->GetMediaURLRequestContext());

  ResourceMessageFilter::GetContextsCallback get_contexts_callback(base::Bind(
      &GetContexts, resource_context, request_context, media_request_context));

  // Several filters need the Blob storage context, so fetch it in advance.
  scoped_refptr<ChromeBlobStorageContext> blob_storage_context =
      ChromeBlobStorageContext::GetFor(browser_context);

  resource_message_filter_ = new ResourceMessageFilter(
      GetID(), storage_partition_impl_->GetAppCacheService(),
      blob_storage_context.get(),
      storage_partition_impl_->GetFileSystemContext(),
      storage_partition_impl_->GetServiceWorkerContext(),
      storage_partition_impl_->GetPrefetchURLLoaderService(),
      BrowserContext::GetSharedCorsOriginAccessList(browser_context),
      std::move(get_contexts_callback),
      base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO}));

  AddFilter(resource_message_filter_.get());

  AddFilter(
      new MidiHost(GetID(), BrowserMainLoop::GetInstance()->midi_service()));
  AddFilter(new DOMStorageMessageFilter(
      storage_partition_impl_->GetDOMStorageContext()));

  peer_connection_tracker_host_ = new PeerConnectionTrackerHost(GetID());
  AddFilter(peer_connection_tracker_host_.get());
#if BUILDFLAG(ENABLE_PLUGINS)
  AddFilter(new PepperRendererConnection(GetID()));
#endif
  AddFilter(new BlobDispatcherHost(GetID(), blob_storage_context));
#if defined(OS_MACOSX)
  AddFilter(new TextInputClientMessageFilter());
#endif

  p2p_socket_dispatcher_host_ =
      std::make_unique<P2PSocketDispatcherHost>(GetID());

  AddFilter(new TraceMessageFilter(GetID()));
  AddFilter(new ResolveProxyMsgHelper(GetID()));

  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context(
      static_cast<ServiceWorkerContextWrapper*>(
          storage_partition_impl_->GetServiceWorkerContext()));
}

void RenderProcessHostImpl::BindCacheStorage(
    blink::mojom::CacheStorageRequest request,
    const url::Origin& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!cache_storage_dispatcher_host_) {
    cache_storage_dispatcher_host_ =
        base::MakeRefCounted<CacheStorageDispatcherHost>();
    cache_storage_dispatcher_host_->Init(
        storage_partition_impl_->GetCacheStorageContext());
  }
  // Send the binding to IO thread, because Cache Storage handles Mojo IPC on IO
  // thread entirely.
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&CacheStorageDispatcherHost::AddBinding,
                     cache_storage_dispatcher_host_, std::move(request),
                     origin));
}

void RenderProcessHostImpl::BindFileSystemManager(
    blink::mojom::FileSystemManagerRequest request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&FileSystemManagerImpl::BindRequest,
                     base::Unretained(file_system_manager_impl_.get()),
                     std::move(request)));
}

void RenderProcessHostImpl::CancelProcessShutdownDelayForUnload() {
  if (IsKeepAliveRefCountDisabled())
    return;
  DecrementKeepAliveRefCount(RenderProcessHost::KeepAliveClientType::kUnload);
}

void RenderProcessHostImpl::DelayProcessShutdownForUnload(
    const base::TimeDelta& timeout) {
  // No need to delay shutdown if the process is already shutting down.
  if (IsKeepAliveRefCountDisabled() || deleting_soon_ || fast_shutdown_started_)
    return;

  IncrementKeepAliveRefCount(RenderProcessHost::KeepAliveClientType::kUnload);
  base::PostDelayedTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(
          &RenderProcessHostImpl::CancelProcessShutdownDelayForUnload,
          weak_factory_.GetWeakPtr()),
      timeout);
}

// static
void RenderProcessHostImpl::AddCorbExceptionForPlugin(int process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  AddCorbExceptionForPluginOnIOThread(process_id);
}

void RenderProcessHostImpl::CleanupCorbExceptionForPluginUponDestruction() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  cleanup_corb_exception_for_plugin_upon_destruction_ = true;
}

void RenderProcessHostImpl::RegisterMojoInterfaces() {
  auto registry = std::make_unique<service_manager::BinderRegistry>();

  channel_->AddAssociatedInterfaceForIOThread(base::BindRepeating(
      &ServiceWorkerDispatcherHost::AddBinding,
      base::Unretained(service_worker_dispatcher_host_.get())));

  AddUIThreadInterface(
      registry.get(), base::Bind(&ForwardRequest<device::mojom::BatteryMonitor>,
                                 device::mojom::kServiceName));

  AddUIThreadInterface(
      registry.get(),
      base::Bind(&RenderProcessHostImpl::CreateEmbeddedFrameSinkProvider,
                 base::Unretained(this)));

  AddUIThreadInterface(registry.get(),
                       base::Bind(&RenderProcessHostImpl::BindFrameSinkProvider,
                                  base::Unretained(this)));

  AddUIThreadInterface(
      registry.get(),
      base::Bind(&RenderProcessHostImpl::BindCompositingModeReporter,
                 base::Unretained(this)));

  AddUIThreadInterface(
      registry.get(),
      base::Bind(&BackgroundSyncContext::CreateService,
                 base::Unretained(
                     storage_partition_impl_->GetBackgroundSyncContext())));
  AddUIThreadInterface(
      registry.get(),
      base::Bind(&RenderProcessHostImpl::CreateStoragePartitionService,
                 base::Unretained(this)));
  AddUIThreadInterface(
      registry.get(),
      base::Bind(&BroadcastChannelProvider::Connect,
                 base::Unretained(
                     storage_partition_impl_->GetBroadcastChannelProvider())));

  AddUIThreadInterface(
      registry.get(),
      base::Bind(&CreateProcessResourceCoordinator, base::Unretained(this)));

  AddUIThreadInterface(registry.get(),
                       base::BindRepeating(&ClipboardHostImpl::Create));

  media::VideoDecodePerfHistory* video_perf_history =
      GetBrowserContext()->GetVideoDecodePerfHistory();
  AddUIThreadInterface(
      registry.get(),
      base::BindRepeating(&media::VideoDecodePerfHistory::BindRequest,
                          base::Unretained(video_perf_history)));

  registry->AddInterface(
      base::Bind(&MimeRegistryImpl::Create),
      base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
           base::TaskPriority::USER_BLOCKING}));
#if BUILDFLAG(USE_MINIKIN_HYPHENATION)
  registry->AddInterface(base::Bind(&hyphenation::HyphenationImpl::Create),
                         hyphenation::HyphenationImpl::GetTaskRunner());
#endif
#if defined(OS_ANDROID)
  if (base::FeatureList::IsEnabled(features::kFontSrcLocalMatching)) {
    registry->AddInterface(
        base::BindRepeating(&FontUniqueNameLookupService::Create),
        FontUniqueNameLookupService::GetTaskRunner());
  }
#endif

  registry->AddInterface(base::Bind(&device::GamepadHapticsManager::Create));

  registry->AddInterface(
      base::Bind(&PushMessagingManager::BindRequest,
                 base::Unretained(push_messaging_manager_.get())));

  file_system_manager_impl_.reset(new FileSystemManagerImpl(
      GetID(), MSG_ROUTING_NONE,
      storage_partition_impl_->GetFileSystemContext(),
      ChromeBlobStorageContext::GetFor(GetBrowserContext())));
  // This interface is still exposed by the RenderProcessHost's registry so
  // that it can be accessed by PepperFileSystemHost. Blink accesses this
  // interface through RenderFrameHost/RendererInterfaceBinders.
  // TODO(https://crbug.com/873661): Make PepperFileSystemHost access this with
  // the RenderFrameHost's registry, and remove this registration.
  registry->AddInterface(
      base::BindRepeating(&FileSystemManagerImpl::BindRequest,
                          base::Unretained(file_system_manager_impl_.get())));

  if (gpu_client_) {
    // |gpu_client_| outlives the registry, because its destruction is posted to
    // IO thread from the destructor of |this|.
    registry->AddInterface(base::BindRepeating(
        &viz::GpuClient::Add, base::Unretained(gpu_client_.get())));
  }

  registry->AddInterface(
      base::BindRepeating(&IndexedDBDispatcherHost::AddBinding,
                          base::Unretained(indexed_db_factory_.get())));

  registry->AddInterface(
      base::Bind(
          &WebDatabaseHostImpl::Create, GetID(),
          base::WrapRefCounted(storage_partition_impl_->GetDatabaseTracker())),
      storage_partition_impl_->GetDatabaseTracker()->task_runner());

  MediaStreamManager* media_stream_manager =
      BrowserMainLoop::GetInstance()->media_stream_manager();

  registry->AddInterface(
      base::Bind(&VideoCaptureHost::Create, GetID(), media_stream_manager));

  registry->AddInterface(
      base::Bind(&FileUtilitiesHostImpl::Create, GetID()),
      base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE}));

  registry->AddInterface(base::BindRepeating(
      &RenderProcessHostImpl::CreateMediaStreamTrackMetricsHost,
      base::Unretained(this)));

  registry->AddInterface(
      base::Bind(&metrics::CreateSingleSampleMetricsProvider));

  registry->AddInterface(base::BindRepeating(
      &CodeCacheHostImpl::Create, GetID(),
      base::RetainedRef(storage_partition_impl_->GetCacheStorageContext()),
      base::RetainedRef(
          storage_partition_impl_->GetGeneratedCodeCacheContext())));

#if BUILDFLAG(ENABLE_REPORTING)
  registry->AddInterface(
      base::Bind(&CreateReportingServiceProxy, storage_partition_impl_));
#endif  // BUILDFLAG(ENABLE_REPORTING)

  registry->AddInterface(base::BindRepeating(
      &AppCacheDispatcherHost::Create,
      base::Unretained(storage_partition_impl_->GetAppCacheService()),
      GetID()));

  AddUIThreadInterface(
      registry.get(),
      base::BindRepeating(&P2PSocketDispatcherHost::BindRequest,
                          base::Unretained(p2p_socket_dispatcher_host_.get())));

  AddUIThreadInterface(registry.get(), base::Bind(&FieldTrialRecorder::Create));

  associated_interfaces_ =
      std::make_unique<blink::AssociatedInterfaceRegistry>();
  blink::AssociatedInterfaceRegistry* associated_registry =
      associated_interfaces_.get();
  associated_registry->AddInterface(base::Bind(
      &RenderProcessHostImpl::BindRouteProvider, base::Unretained(this)));
  associated_registry->AddInterface(base::Bind(
      &RenderProcessHostImpl::CreateRendererHost, base::Unretained(this)));

  if (base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    // Using an opaque origin here should be safe - the URLLoaderFactory created
    // for such origin shouldn't have any special privileges.
    //
    // TODO(lukasza): https://crbug.com/871827: Use the actual origin that will
    // be used as |request_initiator|.  The origin should come from the browser
    // process.
    const url::Origin kSafeOrigin = url::Origin();

    AddUIThreadInterface(
        registry.get(),
        base::Bind(&RenderProcessHostImpl::CreateURLLoaderFactory,
                   base::Unretained(this), kSafeOrigin));
  }

  registry->AddInterface(
      base::BindRepeating(&BlobRegistryWrapper::Bind,
                          storage_partition_impl_->GetBlobRegistry(), GetID()));

#if BUILDFLAG(ENABLE_PLUGINS)
  // Initialization can happen more than once (in the case of a child process
  // crash), but we don't want to lose the plugin registry in this case.
  if (!plugin_registry_) {
    plugin_registry_.reset(
        new PluginRegistryImpl(GetBrowserContext()->GetResourceContext()));
  }
  registry->AddInterface(base::BindRepeating(
      &PluginRegistryImpl::Bind, base::Unretained(plugin_registry_.get())));
#endif

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
  registry->AddInterface(base::BindRepeating(&KeySystemSupportImpl::Create));
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

  AddUIThreadInterface(
      registry.get(),
      base::BindRepeating(&RenderProcessHostImpl::BindVideoDecoderService,
                          base::Unretained(this)));

  // ---- Please do not register interfaces below this line ------
  //
  // This call should be done after registering all interfaces above, so that
  // embedder can override any interfaces. The fact that registry calls
  // the last registration for the name allows us to easily override interfaces.
  GetContentClient()->browser()->ExposeInterfacesToRenderer(
      registry.get(), associated_interfaces_.get(), this);

  ServiceManagerConnection* service_manager_connection =
      BrowserContext::GetServiceManagerConnectionFor(browser_context_);
  if (connection_filter_id_ !=
      ServiceManagerConnection::kInvalidConnectionFilterId) {
    connection_filter_controller_->DisableFilter();
    service_manager_connection->RemoveConnectionFilter(connection_filter_id_);
  }
  std::unique_ptr<ConnectionFilterImpl> connection_filter =
      std::make_unique<ConnectionFilterImpl>(
          child_connection_->child_identity(), std::move(registry));
  connection_filter_controller_ = connection_filter->controller();
  connection_filter_id_ = service_manager_connection->AddConnectionFilter(
      std::move(connection_filter));
}

void RenderProcessHostImpl::BindRouteProvider(
    mojom::RouteProviderAssociatedRequest request) {
  if (route_provider_binding_.is_bound())
    return;
  route_provider_binding_.Bind(std::move(request));
}

void RenderProcessHostImpl::GetRoute(
    int32_t routing_id,
    blink::mojom::AssociatedInterfaceProviderAssociatedRequest request) {
  DCHECK(request.is_pending());
  associated_interface_provider_bindings_.AddBinding(
      this, std::move(request), routing_id);
}

void RenderProcessHostImpl::GetAssociatedInterface(
    const std::string& name,
    blink::mojom::AssociatedInterfaceAssociatedRequest request) {
  int32_t routing_id =
      associated_interface_provider_bindings_.dispatch_context();
  IPC::Listener* listener = listeners_.Lookup(routing_id);
  if (listener)
    listener->OnAssociatedInterfaceRequest(name, request.PassHandle());
}

void RenderProcessHostImpl::CreateEmbeddedFrameSinkProvider(
    blink::mojom::EmbeddedFrameSinkProviderRequest request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!embedded_frame_sink_provider_) {
    // The client id gets converted to a uint32_t in FrameSinkId.
    uint32_t renderer_client_id = base::checked_cast<uint32_t>(id_);
    embedded_frame_sink_provider_ =
        std::make_unique<EmbeddedFrameSinkProviderImpl>(
            GetHostFrameSinkManager(), renderer_client_id);
  }
  embedded_frame_sink_provider_->Add(std::move(request));
}

void RenderProcessHostImpl::BindFrameSinkProvider(
    mojom::FrameSinkProviderRequest request) {
  frame_sink_provider_.Bind(std::move(request));
}

void RenderProcessHostImpl::BindCompositingModeReporter(
    viz::mojom::CompositingModeReporterRequest request) {
  BrowserMainLoop::GetInstance()->GetCompositingModeReporter(
      std::move(request));
}

void RenderProcessHostImpl::CreateStoragePartitionService(
    blink::mojom::StoragePartitionServiceRequest request) {
  if (g_create_storage_partition) {
    g_create_storage_partition(this, std::move(request));
    return;
  }

  storage_partition_impl_->Bind(id_, std::move(request));
}

void RenderProcessHostImpl::BindVideoDecoderService(
    media::mojom::InterfaceFactoryRequest request) {
  if (!video_decoder_proxy_)
    video_decoder_proxy_.reset(new VideoDecoderProxy());
  video_decoder_proxy_->Add(std::move(request));
}

void RenderProcessHostImpl::CreateRendererHost(
    mojom::RendererHostAssociatedRequest request) {
  renderer_host_binding_.Bind(std::move(request));
}

int RenderProcessHostImpl::GetNextRoutingID() {
  return widget_helper_->GetNextRoutingID();
}

void RenderProcessHostImpl::BindInterface(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  child_connection_->BindInterface(interface_name, std::move(interface_pipe));
}

const service_manager::Identity& RenderProcessHostImpl::GetChildIdentity()
    const {
  // GetChildIdentity should only be called if the RPH is (or soon will be)
  // backed by an actual renderer process.  This helps prevent leaks similar to
  // the ones raised in https://crbug.com/813045.
  DCHECK(IsInitializedAndNotDead());

  return child_connection_->child_identity();
}

std::unique_ptr<base::SharedPersistentMemoryAllocator>
RenderProcessHostImpl::TakeMetricsAllocator() {
  return std::move(metrics_allocator_);
}

const base::TimeTicks& RenderProcessHostImpl::GetInitTimeForNavigationMetrics()
    const {
  return init_time_;
}

bool RenderProcessHostImpl::IsProcessBackgrounded() const {
  return priority_.is_background();
}

void RenderProcessHostImpl::IncrementKeepAliveRefCount(
    RenderProcessHost::KeepAliveClientType client) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!is_keep_alive_ref_count_disabled_);
  base::TimeTicks now = base::TimeTicks::Now();
  size_t client_type = static_cast<size_t>(client);
  keep_alive_client_count_[client_type]++;
  if (keep_alive_client_count_[client_type] == 1)
    keep_alive_client_start_time_[client_type] = now;

  ++keep_alive_ref_count_;
  if (keep_alive_ref_count_ == 1) {
    GetRendererInterface()->SetSchedulerKeepActive(true);
  }
}

void RenderProcessHostImpl::DecrementKeepAliveRefCount(
    RenderProcessHost::KeepAliveClientType client) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!is_keep_alive_ref_count_disabled_);
  DCHECK_GT(keep_alive_ref_count_, 0U);
  base::TimeTicks now = base::TimeTicks::Now();
  size_t client_type = static_cast<size_t>(client);
  keep_alive_client_count_[client_type]--;
  if (keep_alive_client_count_[client_type] == 0) {
    RecordKeepAliveDuration(client, keep_alive_client_start_time_[client_type],
                            now);
  }

  --keep_alive_ref_count_;
  if (keep_alive_ref_count_ == 0) {
    Cleanup();
    GetRendererInterface()->SetSchedulerKeepActive(false);
  }
}

void RenderProcessHostImpl::RecordKeepAliveDuration(
    RenderProcessHost::KeepAliveClientType client,
    base::TimeTicks start,
    base::TimeTicks end) {
  switch (client) {
    case RenderProcessHost::KeepAliveClientType::kServiceWorker:
      UMA_HISTOGRAM_LONG_TIMES(
          "BrowserRenderProcessHost.KeepAliveDuration.ServiceWorker",
          end - start);
      break;
    case RenderProcessHost::KeepAliveClientType::kSharedWorker:
      UMA_HISTOGRAM_LONG_TIMES(
          "BrowserRenderProcessHost.KeepAliveDuration.SharedWorker",
          end - start);
      break;
    case RenderProcessHost::KeepAliveClientType::kFetch:
      UMA_HISTOGRAM_LONG_TIMES(
          "BrowserRenderProcessHost.KeepAliveDuration.Fetch", end - start);
      break;
    case RenderProcessHost::KeepAliveClientType::kUnload:
      UMA_HISTOGRAM_LONG_TIMES(
          "BrowserRenderProcessHost.KeepAliveDuration.Unload", end - start);
      break;
  }
}

void RenderProcessHostImpl::DisableKeepAliveRefCount() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (is_keep_alive_ref_count_disabled_)
    return;
  is_keep_alive_ref_count_disabled_ = true;

  keep_alive_ref_count_ = 0;
  base::TimeTicks now = base::TimeTicks::Now();
  for (size_t i = 0; i < kNumKeepAliveClients; i++) {
    if (keep_alive_client_count_[i] > 0) {
      RecordKeepAliveDuration(
          static_cast<RenderProcessHost::KeepAliveClientType>(i),
          keep_alive_client_start_time_[i], now);
      keep_alive_client_count_[i] = 0;
    }
  }

  // Cleaning up will also remove this from the SpareRenderProcessHostManager.
  // (in this case |keep_alive_ref_count_| would be 0 even before).
  Cleanup();
}

bool RenderProcessHostImpl::IsKeepAliveRefCountDisabled() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return is_keep_alive_ref_count_disabled_;
}

void RenderProcessHostImpl::PurgeAndSuspend() {
  GetRendererInterface()->ProcessPurgeAndSuspend();
}

void RenderProcessHostImpl::Resume() {}

mojom::Renderer* RenderProcessHostImpl::GetRendererInterface() {
  return renderer_interface_.get();
}

resource_coordinator::ProcessResourceCoordinator*
RenderProcessHostImpl::GetProcessResourceCoordinator() {
  if (!process_resource_coordinator_) {
    auto* connection = ServiceManagerConnection::GetForProcess();
    process_resource_coordinator_ =
        std::make_unique<resource_coordinator::ProcessResourceCoordinator>(
            connection ? connection->GetConnector() : nullptr);
  }
  return process_resource_coordinator_.get();
}

void RenderProcessHostImpl::CreateURLLoaderFactory(
    const url::Origin& origin,
    network::mojom::URLLoaderFactoryRequest request) {
  if (!base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&ResourceMessageFilter::Clone, resource_message_filter_,
                       std::move(request)));
    return;
  }

  network::mojom::NetworkContext* network_context =
      storage_partition_impl_->GetNetworkContext();
  network::mojom::URLLoaderFactoryPtrInfo embedder_provided_factory =
      GetContentClient()->browser()->CreateURLLoaderFactoryForNetworkRequests(
          this, network_context, origin);
  if (embedder_provided_factory) {
    mojo::FuseInterface(std::move(request),
                        std::move(embedder_provided_factory));
  } else {
    network::mojom::URLLoaderFactoryParamsPtr params =
        network::mojom::URLLoaderFactoryParams::New();
    params->process_id = GetID();
    params->disable_web_security =
        base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kDisableWebSecurity);
    SiteIsolationPolicy::PopulateURLLoaderFactoryParamsPtrForCORB(params.get());
    network_context->CreateURLLoaderFactory(std::move(request),
                                            std::move(params));
  }
}

void RenderProcessHostImpl::SetIsNeverSuitableForReuse() {
  is_never_suitable_for_reuse_ = true;
}

bool RenderProcessHostImpl::MayReuseHost() {
  if (is_never_suitable_for_reuse_)
    return false;

  return GetContentClient()->browser()->MayReuseHost(this);
}

bool RenderProcessHostImpl::IsUnused() {
  return is_unused_;
}

void RenderProcessHostImpl::SetIsUsed() {
  is_unused_ = false;
}

mojom::RouteProvider* RenderProcessHostImpl::GetRemoteRouteProvider() {
  return remote_route_provider_.get();
}

void RenderProcessHostImpl::AddRoute(int32_t routing_id,
                                     IPC::Listener* listener) {
  CHECK(!listeners_.Lookup(routing_id)) << "Found Routing ID Conflict: "
                                        << routing_id;
  listeners_.AddWithID(listener, routing_id);
}

void RenderProcessHostImpl::RemoveRoute(int32_t routing_id) {
  DCHECK(listeners_.Lookup(routing_id) != nullptr);
  listeners_.Remove(routing_id);
  Cleanup();
}

void RenderProcessHostImpl::AddObserver(RenderProcessHostObserver* observer) {
  observers_.AddObserver(observer);
}

void RenderProcessHostImpl::RemoveObserver(
    RenderProcessHostObserver* observer) {
  observers_.RemoveObserver(observer);
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
  // browser to try to survive when it gets illegal messages from the renderer.
  Shutdown(RESULT_CODE_KILLED_BAD_MESSAGE);

  if (crash_report_mode == CrashReportMode::GENERATE_CRASH_DUMP) {
    // Set crash keys to understand renderer kills related to site isolation.
    auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
    std::string lock_url = policy->GetOriginLock(GetID()).spec();
    base::debug::SetCrashKeyString(bad_message::GetKilledProcessOriginLockKey(),
                                   lock_url.empty() ? "(none)" : lock_url);

    std::string site_isolation_mode;
    if (SiteIsolationPolicy::UseDedicatedProcessesForAllSites())
      site_isolation_mode += "spp ";
    if (SiteIsolationPolicy::AreIsolatedOriginsEnabled())
      site_isolation_mode += "io ";
    if (site_isolation_mode.empty())
      site_isolation_mode = "(none)";

    static auto* isolation_mode_key = base::debug::AllocateCrashKeyString(
        "site_isolation_mode", base::debug::CrashKeySize::Size32);
    base::debug::SetCrashKeyString(isolation_mode_key, site_isolation_mode);

    // Report a crash, since none will be generated by the killed renderer.
    base::debug::DumpWithoutCrashing();
  }

  // Log the renderer kill to the histogram tracking all kills.
  BrowserChildProcessHostImpl::HistogramBadMessageTerminated(
      PROCESS_TYPE_RENDERER);
}

void RenderProcessHostImpl::UpdateClientPriority(PriorityClient* client) {
  DCHECK(client);
  DCHECK_EQ(1u, priority_clients_.count(client));
  UpdateProcessPriorityInputs();
}

int RenderProcessHostImpl::VisibleClientCount() const {
  return visible_clients_;
}

unsigned int RenderProcessHostImpl::GetFrameDepth() const {
  return frame_depth_;
}

bool RenderProcessHostImpl::GetIntersectsViewport() const {
  return intersects_viewport_;
}

#if defined(OS_ANDROID)
ChildProcessImportance RenderProcessHostImpl::GetEffectiveImportance() {
  return effective_importance_;
}
#endif

RendererAudioOutputStreamFactoryContext*
RenderProcessHostImpl::GetRendererAudioOutputStreamFactoryContext() {
  if (!audio_output_stream_factory_context_) {
    media::AudioManager* audio_manager =
        BrowserMainLoop::GetInstance()->audio_manager();
    DCHECK(audio_manager) << "AudioManager is not instantiated: running the "
                             "audio service out of process?";
    MediaStreamManager* media_stream_manager =
        BrowserMainLoop::GetInstance()->media_stream_manager();
    media::AudioSystem* audio_system =
        BrowserMainLoop::GetInstance()->audio_system();
    audio_output_stream_factory_context_.reset(
        new RendererAudioOutputStreamFactoryContextImpl(
            GetID(), audio_system, audio_manager, media_stream_manager));
  }
  return audio_output_stream_factory_context_.get();
}

void RenderProcessHostImpl::OnMediaStreamAdded() {
  ++media_stream_count_;
  UpdateProcessPriority();
}

void RenderProcessHostImpl::OnMediaStreamRemoved() {
  DCHECK_GT(media_stream_count_, 0);
  --media_stream_count_;
  UpdateProcessPriority();
}

// static
void RenderProcessHostImpl::set_render_process_host_factory_for_testing(
    const RenderProcessHostFactory* rph_factory) {
  g_render_process_host_factory_ = rph_factory;
}

// static
const RenderProcessHostFactory*
RenderProcessHostImpl::get_render_process_host_factory_for_testing() {
  return g_render_process_host_factory_;
}

// static
void RenderProcessHostImpl::AddFrameWithSite(
    BrowserContext* browser_context,
    RenderProcessHost* render_process_host,
    const GURL& site_url) {
  if (!ShouldTrackProcessForSite(browser_context, render_process_host,
                                 site_url))
    return;

  SiteProcessCountTracker* tracker = static_cast<SiteProcessCountTracker*>(
      browser_context->GetUserData(kCommittedSiteProcessCountTrackerKey));
  if (!tracker) {
    tracker = new SiteProcessCountTracker();
    browser_context->SetUserData(kCommittedSiteProcessCountTrackerKey,
                                 base::WrapUnique(tracker));
  }
  tracker->IncrementSiteProcessCount(site_url, render_process_host->GetID());
}

// static
void RenderProcessHostImpl::RemoveFrameWithSite(
    BrowserContext* browser_context,
    RenderProcessHost* render_process_host,
    const GURL& site_url) {
  if (!ShouldTrackProcessForSite(browser_context, render_process_host,
                                 site_url))
    return;

  SiteProcessCountTracker* tracker = static_cast<SiteProcessCountTracker*>(
      browser_context->GetUserData(kCommittedSiteProcessCountTrackerKey));
  if (!tracker) {
    tracker = new SiteProcessCountTracker();
    browser_context->SetUserData(kCommittedSiteProcessCountTrackerKey,
                                 base::WrapUnique(tracker));
  }
  tracker->DecrementSiteProcessCount(site_url, render_process_host->GetID());
}

// static
void RenderProcessHostImpl::AddExpectedNavigationToSite(
    BrowserContext* browser_context,
    RenderProcessHost* render_process_host,
    const GURL& site_url) {
  if (!ShouldTrackProcessForSite(browser_context, render_process_host,
                                 site_url))
    return;

  SiteProcessCountTracker* tracker = static_cast<SiteProcessCountTracker*>(
      browser_context->GetUserData(kPendingSiteProcessCountTrackerKey));
  if (!tracker) {
    tracker = new SiteProcessCountTracker();
    browser_context->SetUserData(kPendingSiteProcessCountTrackerKey,
                                 base::WrapUnique(tracker));
  }
  tracker->IncrementSiteProcessCount(site_url, render_process_host->GetID());
}

// static
void RenderProcessHostImpl::RemoveExpectedNavigationToSite(
    BrowserContext* browser_context,
    RenderProcessHost* render_process_host,
    const GURL& site_url) {
  if (!ShouldTrackProcessForSite(browser_context, render_process_host,
                                 site_url))
    return;

  SiteProcessCountTracker* tracker = static_cast<SiteProcessCountTracker*>(
      browser_context->GetUserData(kPendingSiteProcessCountTrackerKey));
  if (!tracker) {
    tracker = new SiteProcessCountTracker();
    browser_context->SetUserData(kPendingSiteProcessCountTrackerKey,
                                 base::WrapUnique(tracker));
  }
  tracker->DecrementSiteProcessCount(site_url, render_process_host->GetID());
}

// static
void RenderProcessHostImpl::NotifySpareManagerAboutRecentlyUsedBrowserContext(
    BrowserContext* browser_context) {
  g_spare_render_process_host_manager.Get().PrepareForFutureRequests(
      browser_context);
}

// static
RenderProcessHost*
RenderProcessHostImpl::GetSpareRenderProcessHostForTesting() {
  return g_spare_render_process_host_manager.Get().spare_render_process_host();
}

// static
void RenderProcessHostImpl::DiscardSpareRenderProcessHostForTesting() {
  g_spare_render_process_host_manager.Get().CleanupSpareRenderProcessHost();
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
  // The comparison below is using 1077 rather than 1024 because 1) this helps
  // ensure that devices with exactly 1GB of RAM won't get included because of
  // inaccuracies or off-by-one errors and 2) this is the bucket boundary in
  // Memory.Stats.Win.TotalPhys2.
  if (base::SysInfo::AmountOfPhysicalMemoryMB() <= 1077)
    return false;

  return true;
}

bool RenderProcessHostImpl::HostHasNotBeenUsed() {
  return IsUnused() && listeners_.IsEmpty() && keep_alive_ref_count_ == 0 &&
         pending_views_ == 0;
}

void RenderProcessHostImpl::LockToOrigin(const GURL& lock_url) {
  ChildProcessSecurityPolicyImpl::GetInstance()->LockToOrigin(GetID(),
                                                              lock_url);

  // Note that LockToOrigin is only called once per RenderProcessHostImpl (when
  // committing a navigation into an empty renderer).  Therefore, the call to
  // NotifyRendererIfLockedToSite below is insufficient for setting up renderers
  // respawned after crashing - this is handled by another call to
  // NotifyRendererIfLockedToSite from OnProcessLaunched.
  NotifyRendererIfLockedToSite();
}

void RenderProcessHostImpl::NotifyRendererIfLockedToSite() {
  GURL lock_url =
      ChildProcessSecurityPolicyImpl::GetInstance()->GetOriginLock(GetID());
  if (!lock_url.is_valid())
    return;

  if (!SiteInstanceImpl::IsOriginLockASite(lock_url))
    return;

  GetRendererInterface()->SetIsLockedToSite();
}

bool RenderProcessHostImpl::IsForGuestsOnly() const {
  return is_for_guests_only_;
}

StoragePartition* RenderProcessHostImpl::GetStoragePartition() const {
  return storage_partition_impl_;
}

static void AppendCompositorCommandLineFlags(base::CommandLine* command_line) {
  command_line->AppendSwitchASCII(
      switches::kNumRasterThreads,
      base::IntToString(NumberOfRendererRasterThreads()));

  int msaa_sample_count = GpuRasterizationMSAASampleCount();
  if (msaa_sample_count >= 0) {
    command_line->AppendSwitchASCII(switches::kGpuRasterizationMSAASampleCount,
                                    base::IntToString(msaa_sample_count));
  }

  if (IsZeroCopyUploadEnabled())
    command_line->AppendSwitch(switches::kEnableZeroCopy);
  if (!IsPartialRasterEnabled())
    command_line->AppendSwitch(switches::kDisablePartialRaster);

  if (IsGpuMemoryBufferCompositorResourcesEnabled()) {
    command_line->AppendSwitch(
        switches::kEnableGpuMemoryBufferCompositorResources);
  }

  if (IsMainFrameBeforeActivationEnabled())
    command_line->AppendSwitch(cc::switches::kEnableMainFrameBeforeActivation);
}

void RenderProcessHostImpl::AppendRendererCommandLine(
    base::CommandLine* command_line) {
  // Pass the process type first, so it shows first in process listings.
  command_line->AppendSwitchASCII(switches::kProcessType,
                                  switches::kRendererProcess);

#if defined(OS_WIN)
  command_line->AppendArg(switches::kPrefetchArgumentRenderer);
#endif  // defined(OS_WIN)

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

  GetContentClient()->browser()->AppendExtraCommandLineSwitches(command_line,
                                                                GetID());

#if defined(OS_WIN)
  command_line->AppendSwitchASCII(
      switches::kDeviceScaleFactor,
      base::NumberToString(display::win::GetDPIScale()));
#endif

  AppendCompositorCommandLineFlags(command_line);

  command_line->AppendSwitchASCII(
      service_manager::switches::kServiceRequestChannelToken,
      child_connection_->service_token());
  command_line->AppendSwitchASCII(switches::kRendererClientId,
                                  std::to_string(GetID()));

  if (SiteIsolationPolicy::UseDedicatedProcessesForAllSites()) {
    // Disable V8 code mitigations if renderer processes are site-isolated.
    command_line->AppendSwitch(switches::kNoV8UntrustedCodeMitigations);
  }
}

void RenderProcessHostImpl::PropagateBrowserCommandLineToRenderer(
    const base::CommandLine& browser_cmd,
    base::CommandLine* renderer_cmd) {
  // Propagate the following switches to the renderer command line (along
  // with any associated values) if present in the browser command line.
  static const char* const kSwitchNames[] = {
    network::switches::kNoReferrers,
    service_manager::switches::kDisableInProcessStackTraces,
    service_manager::switches::kDisableSeccompFilterSandbox,
    service_manager::switches::kNoSandbox,
#if defined(OS_MACOSX)
    // Allow this to be set when invoking the browser and relayed along.
    service_manager::switches::kEnableSandboxLogging,
#endif
    switches::kAgcStartupMinVolume,
    switches::kAecRefinedAdaptiveFilter,
    switches::kAllowLoopbackInPeerConnection,
    switches::kAndroidFontsPath,
    switches::kAudioBufferSize,
    switches::kAutoplayPolicy,
    switches::kBlinkSettings,
    switches::kDefaultTileWidth,
    switches::kDefaultTileHeight,
    switches::kDisable2dCanvasImageChromium,
    switches::kDisableAcceleratedJpegDecoding,
    switches::kDisableAcceleratedVideoDecode,
    switches::kDisableBackgroundTasks,
    switches::kDisableBackgroundTimerThrottling,
    switches::kDisableBreakpad,
    switches::kDisableCompositorUkmForTests,
    switches::kDisablePreferCompositingToLCDText,
    switches::kDisableDatabases,
    switches::kDisableFileSystem,
    switches::kDisableFrameRateLimit,
    switches::kDisableGpuMemoryBufferVideoFrames,
    switches::kDisableImageAnimationResync,
    switches::kDisableLowResTiling,
    switches::kDisableHistogramCustomizer,
    switches::kDisableLCDText,
    switches::kDisableLogging,
    switches::kDisableMediaSuspend,
    switches::kDisableNotifications,
    switches::kDisableOopRasterization,
    switches::kDisableOriginTrialControlledBlinkFeatures,
    switches::kDisablePepper3DImageChromium,
    switches::kDisablePermissionsAPI,
    switches::kDisablePresentationAPI,
    switches::kDisableRGBA4444Textures,
    switches::kDisableRTCSmoothnessAlgorithm,
    switches::kDisableSharedWorkers,
    switches::kDisableSkiaRuntimeOpts,
    switches::kDisableSpeechAPI,
    switches::kDisableThreadedCompositing,
    switches::kDisableThreadedScrolling,
    switches::kDisableTouchAdjustment,
    switches::kDisableTouchDragDrop,
    switches::kDisableV8IdleTasks,
    switches::kDisableWebGLImageChromium,
    switches::kDomAutomationController,
    switches::kEnableAccessibilityObjectModel,
    switches::kEnableAutomation,
    switches::kEnableBlinkGenPropertyTrees,
    switches::kEnableExperimentalWebPlatformFeatures,
    switches::kEnableGPUClientLogging,
    switches::kEnableGpuClientTracing,
    switches::kEnableGpuMemoryBufferVideoFrames,
    switches::kEnableGPUServiceLogging,
    switches::kEnableLowResTiling,
    switches::kEnableMediaSuspend,
    switches::kEnableLCDText,
    switches::kEnableLogging,
    switches::kEnableNetworkInformationDownlinkMax,
    switches::kEnableOopRasterization,
    switches::kEnablePluginPlaceholderTesting,
    switches::kEnablePreciseMemoryInfo,
    switches::kEnablePrintBrowser,
    switches::kEnablePreferCompositingToLCDText,
    switches::kEnableRGBA4444Textures,
    switches::kEnableSkiaBenchmarking,
    switches::kEnableSlimmingPaintV2,
    switches::kEnableThreadedCompositing,
    switches::kEnableTouchDragDrop,
    switches::kEnableUnsafeWebGPU,
    switches::kEnableUseZoomForDSF,
    switches::kEnableViewport,
    switches::kEnableVtune,
    switches::kEnableWebGL2ComputeContext,
    switches::kEnableWebGLDraftExtensions,
    switches::kEnableWebGLImageChromium,
    switches::kEnableWebVR,
    switches::kExplicitlyAllowedPorts,
    switches::kFileUrlPathAlias,
    switches::kForceDisplayColorProfile,
    switches::kForceDeviceScaleFactor,
    switches::kForceGpuMemAvailableMb,
    switches::kForceGpuRasterization,
    switches::kForceOverlayFullscreenVideo,
    switches::kForceVideoOverlays,
    switches::kFullMemoryCrashReport,
    switches::kIPCConnectionTimeout,
    switches::kJavaScriptFlags,
    switches::kLoggingLevel,
    switches::kMaxUntiledLayerWidth,
    switches::kMaxUntiledLayerHeight,
    switches::kMSEAudioBufferSizeLimitMb,
    switches::kMSEVideoBufferSizeLimitMb,
    switches::kNetworkQuietTimeout,
    switches::kNoZygote,
    switches::kOverridePluginPowerSaverForTesting,
    switches::kPassiveListenersDefault,
    switches::kPpapiInProcess,
    switches::kReducedReferrerGranularity,
    switches::kRegisterPepperPlugins,
    switches::kRendererStartupDialog,
    switches::kReportVp9AsAnUnsupportedMimeType,
    switches::kSamplingHeapProfiler,
    switches::kShowPaintRects,
    switches::kStatsCollectionController,
    switches::kSkiaFontCacheLimitMb,
    switches::kSkiaResourceCacheLimitMb,
    switches::kTestType,
    switches::kTouchEventFeatureDetection,
    switches::kTouchTextSelectionStrategy,
    switches::kTraceToConsole,
    switches::kUseFakeUIForMediaStream,
    // This flag needs to be propagated to the renderer process for
    // --in-process-webgl.
    switches::kUseGL,
    switches::kUseGpuInTests,
    switches::kUseMobileUserAgent,
    switches::kV,
    switches::kVideoThreads,
    switches::kVideoUnderflowThresholdMs,
    switches::kVModule,
    // Please keep these in alphabetical order. Compositor switches here should
    // also be added to chrome/browser/chromeos/login/chrome_restart_request.cc.
    cc::switches::kAlwaysRequestPresentationTime,
    cc::switches::kCheckDamageEarly,
    cc::switches::kDisableCheckerImaging,
    cc::switches::kDisableCompositedAntialiasing,
    cc::switches::kDisableThreadedAnimation,
    cc::switches::kEnableGpuBenchmarking,
    cc::switches::kShowCompositedLayerBorders,
    cc::switches::kShowFPSCounter,
    cc::switches::kShowLayerAnimationBounds,
    cc::switches::kShowPropertyChangedRects,
    cc::switches::kShowScreenSpaceRects,
    cc::switches::kShowSurfaceDamageRects,
    cc::switches::kSlowDownRasterScaleFactor,
    cc::switches::kBrowserControlsHideThreshold,
    cc::switches::kBrowserControlsShowThreshold,
    switches::kEnableSurfaceSynchronization,
    switches::kRunAllCompositorStagesBeforeDraw,
    switches::kUseVizHitTestSurfaceLayer,

#if BUILDFLAG(ENABLE_PLUGINS)
    switches::kEnablePepperTesting,
#endif
#if BUILDFLAG(ENABLE_RUNTIME_MEDIA_RENDERER_SELECTION)
    switches::kDisableMojoRenderer,
#endif
    switches::kDisableWebRtcHWDecoding,
    switches::kDisableWebRtcHWEncoding,
    switches::kEnableWebRtcSrtpAesGcm,
    switches::kEnableWebRtcSrtpEncryptedHeaders,
    switches::kEnableWebRtcStunOrigin,
    switches::kEnforceWebRtcIPPermissionCheck,
    switches::kWebRtcMaxCaptureFramerate,
    switches::kEnableLowEndDeviceMode,
    switches::kDisableLowEndDeviceMode,
    switches::kDisallowNonExactResourceReuse,
#if defined(OS_ANDROID)
    switches::kDisableMediaSessionAPI,
    switches::kOrderfileMemoryOptimization,
    switches::kRendererWaitForJavaDebugger,
#endif
#if defined(OS_WIN)
    service_manager::switches::kDisableWin32kLockDown,
    switches::kEnableWin7WebRtcHWH264Decoding,
    switches::kTrySupportedChannelLayouts,
    switches::kTraceExportEventsToETW,
#endif
#if defined(USE_OZONE)
    switches::kOzonePlatform,
#endif
#if defined(ENABLE_IPC_FUZZER)
    switches::kIpcDumpDirectory,
    switches::kIpcFuzzerTestcase,
#endif
  };
  renderer_cmd->CopySwitchesFrom(browser_cmd, kSwitchNames,
                                 arraysize(kSwitchNames));

  BrowserChildProcessHostImpl::CopyFeatureAndFieldTrialFlags(renderer_cmd);
  BrowserChildProcessHostImpl::CopyTraceStartupFlags(renderer_cmd);

  // Only run the Stun trials in the first renderer.
  if (!has_done_stun_trials &&
      browser_cmd.HasSwitch(switches::kWebRtcStunProbeTrialParameter)) {
    has_done_stun_trials = true;
    renderer_cmd->AppendSwitchASCII(
        switches::kWebRtcStunProbeTrialParameter,
        browser_cmd.GetSwitchValueASCII(
            switches::kWebRtcStunProbeTrialParameter));
  }

  // Disable databases in incognito mode.
  if (GetBrowserContext()->IsOffTheRecord() &&
      !browser_cmd.HasSwitch(switches::kDisableDatabases)) {
    renderer_cmd->AppendSwitch(switches::kDisableDatabases);
  }

#if defined(OS_ANDROID)
  if (browser_cmd.HasSwitch(switches::kDisableGpuCompositing)) {
    renderer_cmd->AppendSwitch(switches::kDisableGpuCompositing);
  }
#elif !defined(OS_CHROMEOS)
#if !BUILDFLAG(ENABLE_MUS)
  // If gpu compositing is not being used, tell the renderer at startup. This
  // is inherently racey, as it may change while the renderer is being launched,
  // but the renderer will hear about the correct state eventually. This
  // optimizes the common case to avoid wasted work.
  // Note: There is no ImageTransportFactory with Mus, but there is also no
  // software compositing on platforms where Mus is used, e.g. ChromeOS, so
  // no need to check this state and forward it.
  if (ImageTransportFactory::GetInstance()->IsGpuCompositingDisabled())
    renderer_cmd->AppendSwitch(switches::kDisableGpuCompositing);
#else   // BUILDFLAG(ENABLE_MUS)
// TODO(tonikitoo): Check if renderer should use software compositing
// through some mechanism that isn't ImageTransportFactory with mus.
#endif  // !BUILDFLAG(ENABLE_MUS)
#endif  // defined(OS_ANDROID)

  // Add kWaitForDebugger to let renderer process wait for a debugger.
  if (browser_cmd.HasSwitch(switches::kWaitForDebuggerChildren)) {
    // Look to pass-on the kWaitForDebugger flag.
    std::string value =
        browser_cmd.GetSwitchValueASCII(switches::kWaitForDebuggerChildren);
    if (value.empty() || value == switches::kRendererProcess) {
      renderer_cmd->AppendSwitch(switches::kWaitForDebugger);
    }
  }

  DCHECK(child_connection_);
  renderer_cmd->AppendSwitchASCII(service_manager::switches::kServicePipeToken,
                                  child_connection_->service_token());

#if defined(OS_WIN) && !defined(OFFICIAL_BUILD)
  // Needed because we can't show the dialog from the sandbox. Don't pass
  // --no-sandbox in official builds because that would bypass the bad_flgs
  // prompt.
  if (renderer_cmd->HasSwitch(switches::kRendererStartupDialog) &&
      !renderer_cmd->HasSwitch(service_manager::switches::kNoSandbox)) {
    renderer_cmd->AppendSwitch(service_manager::switches::kNoSandbox);
  }
#endif

  CopyFeatureSwitch(browser_cmd, renderer_cmd, switches::kEnableBlinkFeatures);
  CopyFeatureSwitch(browser_cmd, renderer_cmd, switches::kDisableBlinkFeatures);
}

const base::Process& RenderProcessHostImpl::GetProcess() const {
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

bool RenderProcessHostImpl::IsReady() const {
  // The process launch result (that sets GetHandle()) and the channel
  // connection (that sets channel_connected_) can happen in either order.
  return GetProcess().Handle() && channel_connected_;
}

bool RenderProcessHostImpl::Shutdown(int exit_code) {
  if (run_renderer_in_process())
    return false;  // Single process mode never shuts down the renderer.

  if (!child_process_launcher_.get())
    return false;

  return child_process_launcher_->Terminate(exit_code);
}

bool RenderProcessHostImpl::FastShutdownIfPossible(size_t page_count,
                                                   bool skip_unload_handlers) {
  // Do not shut down the process if there are active or pending views other
  // than the ones we're shutting down.
  if (page_count && page_count != (GetActiveViewCount() + pending_views_))
    return false;

  if (run_renderer_in_process())
    return false;  // Single process mode never shuts down the renderer.

  if (!child_process_launcher_.get())
    return false;  // Render process hasn't started or is probably crashed.

  // Test if there's an unload listener.
  // NOTE: It's possible that an onunload listener may be installed
  // while we're shutting down, so there's a small race here.  Given that
  // the window is small, it's unlikely that the web page has much
  // state that will be lost by not calling its unload handlers properly.
  if (!skip_unload_handlers && !SuddenTerminationAllowed())
    return false;

  if (keep_alive_ref_count_ != 0) {
    if (keep_alive_start_time_.is_null())
      keep_alive_start_time_ = base::TimeTicks::Now();
    return false;
  }

  // Set this before ProcessDied() so observers can tell if the render process
  // died due to fast shutdown versus another cause.
  fast_shutdown_started_ = true;

  ProcessDied(false /* already_dead */, nullptr);
  return true;
}

bool RenderProcessHostImpl::Send(IPC::Message* msg) {
  TRACE_EVENT2("renderer_host", "RenderProcessHostImpl::Send", "class",
               IPC_MESSAGE_ID_CLASS(msg->type()), "line",
               IPC_MESSAGE_ID_LINE(msg->type()));

  std::unique_ptr<IPC::Message> message(msg);

  // |channel_| is only null after Cleanup(), at which point we don't care about
  // delivering any messages.
  if (!channel_)
    return false;

#if !defined(OS_ANDROID)
  DCHECK(!message->is_sync());
#else
  if (message->is_sync()) {
    // If Init() hasn't been called yet since construction or the last
    // ProcessDied() we avoid blocking on sync IPC.
    if (!IsInitializedAndNotDead())
      return false;

    // Likewise if we've done Init(), but process launch has not yet completed,
    // we avoid blocking on sync IPC.
    if (child_process_launcher_.get() && child_process_launcher_->IsStarting())
      return false;
  }
#endif

  return channel_->Send(message.release());
}

bool RenderProcessHostImpl::OnMessageReceived(const IPC::Message& msg) {
  // If we're about to be deleted, or have initiated the fast shutdown sequence,
  // we ignore incoming messages.

  if (deleting_soon_ || fast_shutdown_started_)
    return false;

  mark_child_process_activity_time();
  if (msg.routing_id() == MSG_ROUTING_CONTROL) {
    // Dispatch control messages.
    IPC_BEGIN_MESSAGE_MAP(RenderProcessHostImpl, msg)
      IPC_MESSAGE_HANDLER(ViewHostMsg_UserMetricsRecordAction,
                          OnUserMetricsRecordAction)
      IPC_MESSAGE_HANDLER(WidgetHostMsg_Close_ACK, OnCloseACK)
      IPC_MESSAGE_HANDLER(AecDumpMsg_RegisterAecDumpConsumer,
                          OnRegisterAecDumpConsumer)
      IPC_MESSAGE_HANDLER(AecDumpMsg_UnregisterAecDumpConsumer,
                          OnUnregisterAecDumpConsumer)
      IPC_MESSAGE_HANDLER(AudioProcessingMsg_Aec3Enabled, OnAec3Enabled)
    // Adding single handlers for your service here is fine, but once your
    // service needs more than one handler, please extract them into a new
    // message filter and add that filter to CreateMessageFilters().
    IPC_END_MESSAGE_MAP()

    return true;
  }

  // Dispatch incoming messages to the appropriate IPC::Listener.
  IPC::Listener* listener = listeners_.Lookup(msg.routing_id());
  if (!listener) {
    if (msg.is_sync()) {
      // The listener has gone away, so we must respond or else the caller will
      // hang waiting for a reply.
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
  if (IsReady()) {
    DCHECK(!sent_render_process_ready_);
    sent_render_process_ready_ = true;
    // Send RenderProcessReady only if we already received the process handle.
    for (auto& observer : observers_)
      observer.RenderProcessReady(this);
  }

#if BUILDFLAG(IPC_MESSAGE_LOG_ENABLED)
  child_control_interface_->SetIPCLoggingEnabled(
      IPC::Logging::GetInstance()->Enabled());
#endif
}

void RenderProcessHostImpl::OnChannelError() {
  UMA_HISTOGRAM_BOOLEAN("BrowserRenderProcessHost.OnChannelError", true);
  ProcessDied(true /* already_dead */, nullptr);
}

void RenderProcessHostImpl::OnBadMessageReceived(const IPC::Message& message) {
  // Message de-serialization failed. We consider this a capital crime. Kill the
  // renderer if we have one.
  auto type = message.type();
  LOG(ERROR) << "bad message " << type << " terminating renderer.";

  // The ReceivedBadMessage call below will trigger a DumpWithoutCrashing. Alias
  // enough information here so that we can determine what the bad message was.
  base::debug::Alias(&type);

  bad_message::ReceivedBadMessage(this,
                                  bad_message::RPH_DESERIALIZATION_FAILED);
}

BrowserContext* RenderProcessHostImpl::GetBrowserContext() const {
  return browser_context_;
}

bool RenderProcessHostImpl::InSameStoragePartition(
    StoragePartition* partition) const {
  return storage_partition_impl_ == partition;
}

int RenderProcessHostImpl::GetID() const {
  return id_;
}

bool RenderProcessHostImpl::IsInitializedAndNotDead() const {
  return is_initialized_ && !is_dead_;
}

void RenderProcessHostImpl::SetIgnoreInputEvents(bool ignore_input_events) {
  if (ignore_input_events == ignore_input_events_)
    return;

  ignore_input_events_ = ignore_input_events;
  for (auto* widget : widgets_) {
    widget->ProcessIgnoreInputEventsChanged(ignore_input_events);
  }
}

bool RenderProcessHostImpl::IgnoreInputEvents() const {
  return ignore_input_events_;
}

void RenderProcessHostImpl::Cleanup() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Keep the one renderer thread around forever in single process mode.
  if (run_renderer_in_process())
    return;

  // If within_process_died_observer_ is true, one of our observers performed an
  // action that caused us to die (e.g. http://crbug.com/339504). Therefore,
  // delay the destruction until all of the observer callbacks have been made,
  // and guarantee that the RenderProcessHostDestroyed observer callback is
  // always the last callback fired.
  if (within_process_died_observer_) {
    delayed_cleanup_needed_ = true;
    return;
  }
  delayed_cleanup_needed_ = false;

  // Records the time when the process starts kept alive by the ref count for
  // UMA.
  if (listeners_.IsEmpty() && keep_alive_ref_count_ > 0 &&
      keep_alive_start_time_.is_null()) {
    keep_alive_start_time_ = base::TimeTicks::Now();
  }

  // Until there are no other owners of this object, we can't delete ourselves.
  if (!listeners_.IsEmpty() || keep_alive_ref_count_ != 0)
    return;

  if (is_initialized_) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&WebRtcLog::ClearLogMessageCallback, GetID()));
  }

  if (!keep_alive_start_time_.is_null()) {
    UMA_HISTOGRAM_LONG_TIMES("BrowserRenderProcessHost.KeepAliveDuration",
                             base::TimeTicks::Now() - keep_alive_start_time_);
  }

  // We cannot clean up twice; if this fails, there is an issue with our
  // control flow.
  DCHECK(!deleting_soon_);

  DCHECK_EQ(0, pending_views_);

  // If the process associated with this RenderProcessHost is still alive,
  // notify all observers that the process has exited cleanly, even though it
  // will be destroyed a bit later. Observers shouldn't rely on this process
  // anymore.
  if (IsInitializedAndNotDead()) {
    // Populates Android-only fields and closes the underlying base::Process.
    ChildProcessTerminationInfo info =
        child_process_launcher_->GetChildTerminationInfo(
            false /* already_dead */);
    info.status = base::TERMINATION_STATUS_NORMAL_TERMINATION;
    info.exit_code = 0;
    for (auto& observer : observers_) {
      observer.RenderProcessExited(this, info);
    }
  }
  for (auto& observer : observers_)
    observer.RenderProcessHostDestroyed(this);
  NotificationService::current()->Notify(
      NOTIFICATION_RENDERER_PROCESS_TERMINATED,
      Source<RenderProcessHost>(this), NotificationService::NoDetails());

  if (connection_filter_id_ !=
        ServiceManagerConnection::kInvalidConnectionFilterId) {
    ServiceManagerConnection* service_manager_connection =
        BrowserContext::GetServiceManagerConnectionFor(browser_context_);
    connection_filter_controller_->DisableFilter();
    service_manager_connection->RemoveConnectionFilter(connection_filter_id_);
    connection_filter_id_ =
        ServiceManagerConnection::kInvalidConnectionFilterId;
  }

#ifndef NDEBUG
  is_self_deleted_ = true;
#endif
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
  deleting_soon_ = true;

  // Destroy all mojo bindings and IPC channels that can cause calls to this
  // object, to avoid method invocations that trigger usages of profile.
  ResetIPC();

  // Its important to remove the kSessionStorageHolder after the channel
  // has been reset to avoid deleting the underlying namespaces prior
  // to processing ipcs referring to them.
  DCHECK(!channel_);
  RemoveUserData(kSessionStorageHolderKey);

  // Remove ourself from the list of renderer processes so that we can't be
  // reused in between now and when the Delete task runs.
  UnregisterHost(GetID());

  instance_weak_factory_ =
      std::make_unique<base::WeakPtrFactory<RenderProcessHostImpl>>(this);
}

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

void RenderProcessHostImpl::AddWidget(RenderWidgetHost* widget) {
  RenderWidgetHostImpl* widget_impl =
      static_cast<RenderWidgetHostImpl*>(widget);
  widgets_.insert(widget_impl);

  DCHECK(!base::ContainsKey(priority_clients_, widget_impl));
  priority_clients_.insert(widget_impl);
  UpdateProcessPriorityInputs();
}

void RenderProcessHostImpl::RemoveWidget(RenderWidgetHost* widget) {
  RenderWidgetHostImpl* widget_impl =
      static_cast<RenderWidgetHostImpl*>(widget);
  widgets_.erase(widget_impl);

  DCHECK(base::ContainsKey(priority_clients_, widget_impl));
  priority_clients_.erase(widget_impl);
  UpdateProcessPriorityInputs();
}

void RenderProcessHostImpl::SetSuddenTerminationAllowed(bool enabled) {
  sudden_termination_allowed_ = enabled;
}

bool RenderProcessHostImpl::SuddenTerminationAllowed() const {
  return sudden_termination_allowed_;
}

base::TimeDelta RenderProcessHostImpl::GetChildProcessIdleTime() const {
  return base::TimeTicks::Now() - child_process_activity_time_;
}

void RenderProcessHostImpl::FilterURL(bool empty_allowed, GURL* url) {
  FilterURL(this, empty_allowed, url);
}

void RenderProcessHostImpl::EnableAudioDebugRecordings(
    const base::FilePath& file) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Enable AEC dump for each registered consumer.
  base::FilePath file_with_extensions = GetAecDumpFilePathWithExtensions(file);
  for (int id : aec_dump_consumers_) {
    EnableAecDumpForId(file_with_extensions, id);
  }
}

void RenderProcessHostImpl::DisableAudioDebugRecordings() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Posting on the sequence and then replying back on the UI thread is only
  // for avoiding races between enable and disable. Nothing is done on the
  // sequence.
  GetAecDumpFileTaskRunner().PostTaskAndReply(
      FROM_HERE, base::DoNothing(),
      base::BindOnce(&RenderProcessHostImpl::SendDisableAecDumpToRenderer,
                     weak_factory_.GetWeakPtr()));
}

void RenderProcessHostImpl::SetEchoCanceller3(
    bool enable,
    base::OnceCallback<void(bool, const std::string&)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());

  if (!aec3_set_callback_.is_null()) {
    MediaStreamManager::SendMessageToNativeLog("RPHI: Failed to set AEC3");
    base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                             base::BindOnce(std::move(callback), false,
                                            "Operation already in progress"));
    return;
  }

  aec3_set_callback_ = std::move(callback);
  Send(new AudioProcessingMsg_EnableAec3(enable));
}

RenderProcessHostImpl::WebRtcStopRtpDumpCallback
RenderProcessHostImpl::StartRtpDump(
    bool incoming,
    bool outgoing,
    const WebRtcRtpPacketCallback& packet_callback) {
  p2p_socket_dispatcher_host_->StartRtpDump(incoming, outgoing,
                                            packet_callback);

  if (stop_rtp_dump_callback_.is_null()) {
    stop_rtp_dump_callback_ =
        base::Bind(&P2PSocketDispatcherHost::StopRtpDump,
                   p2p_socket_dispatcher_host_->GetWeakPtr());
  }
  return stop_rtp_dump_callback_;
}

void RenderProcessHostImpl::SetWebRtcEventLogOutput(int lid, bool enabled) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (enabled) {
    Send(new PeerConnectionTracker_StartEventLogOutput(lid));
  } else {
    Send(new PeerConnectionTracker_StopEventLog(lid));
  }
}

IPC::ChannelProxy* RenderProcessHostImpl::GetChannel() {
  return channel_.get();
}

void RenderProcessHostImpl::AddFilter(BrowserMessageFilter* filter) {
  filter->RegisterAssociatedInterfaces(channel_.get());
  channel_->AddFilter(filter->GetFilter());
}

bool RenderProcessHostImpl::FastShutdownStarted() const {
  return fast_shutdown_started_;
}

// static
void RenderProcessHostImpl::RegisterHost(int host_id, RenderProcessHost* host) {
  g_all_hosts.Get().AddWithID(host, host_id);
}

// static
void RenderProcessHostImpl::UnregisterHost(int host_id) {
  RenderProcessHost* host = g_all_hosts.Get().Lookup(host_id);
  if (!host)
    return;

  g_all_hosts.Get().Remove(host_id);

  // Look up the map of site to process for the given browser_context,
  // in case we need to remove this process from it.  It will be registered
  // under any sites it rendered that use process-per-site mode.
  SiteProcessMap* map =
      GetSiteProcessMapForBrowserContext(host->GetBrowserContext());
  map->RemoveProcess(host);
}

// static
void RenderProcessHostImpl::FilterURL(RenderProcessHost* rph,
                                      bool empty_allowed,
                                      GURL* url) {
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();

  if (empty_allowed && url->is_empty())
    return;

  if (!url->is_valid()) {
    // Have to use about:blank for the denied case, instead of an empty GURL.
    // This is because the browser treats navigation to an empty GURL as a
    // navigation to the home page. This is often a privileged page
    // (chrome://newtab/) which is exactly what we don't want.
    *url = GURL(url::kAboutBlankURL);
    return;
  }

  if (!policy->CanRequestURL(rph->GetID(), *url)) {
    // If this renderer is not permitted to request this URL, we invalidate the
    // URL.  This prevents us from storing the blocked URL and becoming confused
    // later.
    VLOG(1) << "Blocked URL " << url->spec();
    *url = GURL(url::kAboutBlankURL);
  }
}

// static
bool RenderProcessHostImpl::IsSuitableHost(RenderProcessHost* host,
                                           BrowserContext* browser_context,
                                           const GURL& site_url,
                                           const GURL& lock_url) {
  if (run_renderer_in_process()) {
    DCHECK_EQ(host->GetBrowserContext(), browser_context)
        << " Single-process mode does not support multiple browser contexts.";
    return true;
  }

  if (host->GetBrowserContext() != browser_context)
    return false;

  // Do not allow sharing of guest hosts. This is to prevent bugs where guest
  // and non-guest storage gets mixed. In the future, we might consider enabling
  // the sharing of guests, in this case this check should be removed and
  // InSameStoragePartition should handle the possible sharing.
  if (host->IsForGuestsOnly())
    return false;

  // Check whether the given host and the intended site_url will be using the
  // same StoragePartition, since a RenderProcessHost can only support a single
  // StoragePartition.  This is relevant for packaged apps.
  StoragePartition* dest_partition =
      BrowserContext::GetStoragePartitionForSite(browser_context, site_url);
  if (!host->InSameStoragePartition(dest_partition))
    return false;

  // Check WebUI bindings and origin locks.  Note that |lock_url| may differ
  // from |site_url| if an effective URL is used.
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  bool host_has_web_ui_bindings = policy->HasWebUIBindings(host->GetID());
  auto lock_state = policy->CheckOriginLock(host->GetID(), lock_url);
  if (host->HostHasNotBeenUsed()) {
    // If the host hasn't been used, it won't have the expected WebUI bindings
    // or origin locks just *yet* - skip the checks in this case.  One example
    // where this case can happen is when the spare RenderProcessHost gets used.
    CHECK(!host_has_web_ui_bindings);
    CHECK_EQ(CheckOriginLockResult::NO_LOCK, lock_state);
  } else {
    // WebUI checks.
    bool url_requires_web_ui_bindings =
        WebUIControllerFactoryRegistry::GetInstance()->UseWebUIBindingsForURL(
            browser_context, site_url);
    if (host_has_web_ui_bindings != url_requires_web_ui_bindings)
      return false;

    // Sites requiring dedicated processes can only reuse a compatible process.
    switch (lock_state) {
      case CheckOriginLockResult::HAS_EQUAL_LOCK:
        break;
      case CheckOriginLockResult::HAS_WRONG_LOCK:
        return false;
      case CheckOriginLockResult::NO_LOCK:
        if (!host->IsUnused() &&
            SiteInstanceImpl::ShouldLockToOrigin(browser_context, site_url)) {
          // If this process has been used to host any other content, it cannot
          // be reused if the destination site requires a dedicated process and
          // should use a process locked to just that site.
          return false;
        }
        break;
    }
  }

  return GetContentClient()->browser()->IsSuitableHost(host, site_url);
}

// static
void RenderProcessHost::WarmupSpareRenderProcessHost(
    content::BrowserContext* browser_context) {
  g_spare_render_process_host_manager.Get().WarmupSpareRenderProcessHost(
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
      // Modify the current process' command line to include the browser locale,
      // as the renderer expects this flag to be set.
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
RenderProcessHost::iterator RenderProcessHost::AllHostsIterator() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return iterator(g_all_hosts.Pointer());
}

// static
RenderProcessHost* RenderProcessHost::FromID(int render_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return g_all_hosts.Get().Lookup(render_process_id);
}

// static
RenderProcessHost* RenderProcessHost::FromRendererIdentity(
    const service_manager::Identity& identity) {
  for (RenderProcessHost::iterator i(RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    RenderProcessHost* process = i.GetCurrentValue();
    if (process->GetChildIdentity() == identity)
      return process;
  }
  return nullptr;
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
  if (g_all_hosts.Get().size() >= GetMaxRendererProcessCount())
    return true;

  return GetContentClient()->browser()->ShouldTryToUseExistingProcessHost(
      browser_context, url);
}

// static
RenderProcessHost* RenderProcessHostImpl::GetExistingProcessHost(
    SiteInstanceImpl* site_instance) {
  // First figure out which existing renderers we can use.
  std::vector<RenderProcessHost*> suitable_renderers;
  suitable_renderers.reserve(g_all_hosts.Get().size());

  iterator iter(AllHostsIterator());
  while (!iter.IsAtEnd()) {
    if (iter.GetCurrentValue()->MayReuseHost() &&
        RenderProcessHostImpl::IsSuitableHost(
            iter.GetCurrentValue(), site_instance->GetBrowserContext(),
            site_instance->GetSiteURL(), site_instance->lock_url())) {
      // The spare is always considered before process reuse.
      DCHECK_NE(iter.GetCurrentValue(),
                g_spare_render_process_host_manager.Get()
                    .spare_render_process_host());

      suitable_renderers.push_back(iter.GetCurrentValue());
    }
    iter.Advance();
  }

  // Now pick a random suitable renderer, if we have any.
  if (!suitable_renderers.empty()) {
    int suitable_count = static_cast<int>(suitable_renderers.size());
    int random_index = base::RandInt(0, suitable_count - 1);
    return suitable_renderers[random_index];
  }

  return nullptr;
}

// static
bool RenderProcessHost::ShouldUseProcessPerSite(BrowserContext* browser_context,
                                                const GURL& url) {
  // Returns true if we should use the process-per-site model.  This will be
  // the case if the --process-per-site switch is specified, or in
  // process-per-site-instance for particular sites (e.g., WebUI).
  // Note that --single-process is handled in ShouldTryToUseExistingProcessHost.
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kProcessPerSite))
    return true;

  // Error pages should use process-per-site model, as it is useful to
  // consolidate them to minimize resource usage and there is no security
  // drawback to combining them all in the same process.
  if (url.SchemeIs(kChromeErrorScheme))
    return true;

  // We want to consolidate particular sites like WebUI even when we are using
  // the process-per-tab or process-per-site-instance models.
  // Note: DevTools pages have WebUI type but should not reuse the same host.
  if (WebUIControllerFactoryRegistry::GetInstance()->UseWebUIForURL(
          browser_context, url) &&
      !url.SchemeIs(kChromeDevToolsScheme)) {
    return true;
  }

  // Otherwise let the content client decide, defaulting to false.
  return GetContentClient()->browser()->ShouldUseProcessPerSite(browser_context,
                                                                url);
}

// static
RenderProcessHost* RenderProcessHostImpl::GetSoleProcessHostForURL(
    BrowserContext* browser_context,
    const GURL& url) {
  GURL site_url = SiteInstance::GetSiteForURL(browser_context, url);
  GURL lock_url =
      SiteInstanceImpl::DetermineProcessLockURL(browser_context, url);
  return GetSoleProcessHostForSite(browser_context, site_url, lock_url);
}

// static
RenderProcessHost* RenderProcessHostImpl::GetSoleProcessHostForSite(
    BrowserContext* browser_context,
    const GURL& site_url,
    const GURL& lock_url) {
  // Look up the map of site to process for the given browser_context.
  SiteProcessMap* map = GetSiteProcessMapForBrowserContext(browser_context);

  // See if we have an existing process with appropriate bindings for this site.
  // If not, the caller should create a new process and register it.  Note that
  // IsSuitableHost expects a site URL rather than the full |url|.
  RenderProcessHost* host = map->FindProcess(site_url.possibly_invalid_spec());
  if (host && (!host->MayReuseHost() ||
               !IsSuitableHost(host, browser_context, site_url, lock_url))) {
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
    BrowserContext* browser_context,
    RenderProcessHost* process,
    SiteInstanceImpl* site_instance) {
  // Look up the map of site to process for the given browser_context.
  SiteProcessMap* map = GetSiteProcessMapForBrowserContext(browser_context);

  // Only register valid, non-empty sites.  Empty or invalid sites will not
  // use process-per-site mode.  We cannot check whether the process has
  // appropriate bindings here, because the bindings have not yet been granted.
  std::string site = site_instance->GetSiteURL().possibly_invalid_spec();
  if (!site.empty())
    map->RegisterProcess(site, process);
}

// static
RenderProcessHost* RenderProcessHostImpl::GetProcessHostForSiteInstance(
    SiteInstanceImpl* site_instance) {
  const GURL site_url = site_instance->GetSiteURL();
  SiteInstanceImpl::ProcessReusePolicy process_reuse_policy =
      site_instance->process_reuse_policy();
  bool is_for_guests_only = site_url.SchemeIs(kGuestScheme);
  RenderProcessHost* render_process_host = nullptr;

  bool is_unmatched_service_worker = site_instance->is_for_service_worker();
  BrowserContext* browser_context = site_instance->GetBrowserContext();

  // First, attempt to reuse an existing RenderProcessHost if necessary.
  switch (process_reuse_policy) {
    case SiteInstanceImpl::ProcessReusePolicy::PROCESS_PER_SITE:
      render_process_host = GetSoleProcessHostForSite(
          browser_context, site_url, site_instance->lock_url());
      break;
    case SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE:
      render_process_host =
          FindReusableProcessHostForSiteInstance(site_instance);
      UMA_HISTOGRAM_BOOLEAN(
          "SiteIsolation.ReusePendingOrCommittedSite.CouldReuse",
          render_process_host != nullptr);
      if (render_process_host)
        is_unmatched_service_worker = false;
      break;
    default:
      break;
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

  // See if the spare RenderProcessHost can be used.
  SpareRenderProcessHostManager& spare_process_manager =
      g_spare_render_process_host_manager.Get();
  bool spare_was_taken = false;
  if (!render_process_host) {
    render_process_host = spare_process_manager.MaybeTakeSpareRenderProcessHost(
        browser_context, site_instance, is_for_guests_only);
    spare_was_taken = (render_process_host != nullptr);
  }

  // If not (or if none found), see if we should reuse an existing process.
  if (!render_process_host &&
      ShouldTryToUseExistingProcessHost(browser_context, site_url)) {
    render_process_host = GetExistingProcessHost(site_instance);
  }

  // If we found a process to reuse, sanity check that it is suitable for
  // hosting |site_url|. For example, if |site_url| requires a dedicated
  // process, we should never pick a process used by, or locked to, a different
  // site.
  if (render_process_host && !RenderProcessHostImpl::IsSuitableHost(
                                 render_process_host, browser_context, site_url,
                                 site_instance->lock_url())) {
    ChildProcessSecurityPolicyImpl* policy =
        ChildProcessSecurityPolicyImpl::GetInstance();
    base::debug::SetCrashKeyString(bad_message::GetRequestedSiteURLKey(),
                                   site_url.spec());
    base::debug::SetCrashKeyString(
        bad_message::GetKilledProcessOriginLockKey(),
        policy->GetOriginLock(render_process_host->GetID()).spec());
    CHECK(false) << "Unsuitable process reused for site " << site_url;
  }

  // Otherwise, create a new RenderProcessHost.
  if (!render_process_host) {
    // Pass a null StoragePartition. Tests with TestBrowserContext using a
    // RenderProcessHostFactory may not instantiate a StoragePartition, and
    // creating one here with GetStoragePartition() can run into cross-thread
    // issues as TestBrowserContext initialization is done on the main thread.
    render_process_host = CreateRenderProcessHost(
        browser_context, nullptr, site_instance, is_for_guests_only);
  }

  // It is important to call PrepareForFutureRequests *after* potentially
  // creating a process a few statements earlier - doing this avoids violating
  // the process limit.
  //
  // We should not warm-up another spare if the spare was not taken, because in
  // this case we might have created a new process - we want to avoid spawning
  // two processes at the same time.  In this case the call to
  // PrepareForFutureRequests will be postponed until later (e.g. until the
  // navigation commits or a cross-site redirect happens).
  if (spare_was_taken)
    spare_process_manager.PrepareForFutureRequests(browser_context);

  if (is_unmatched_service_worker) {
    UnmatchedServiceWorkerProcessTracker::Register(render_process_host,
                                                   site_instance);
  }

  // Make sure the chosen process is in the correct StoragePartition for the
  // SiteInstance.
  CHECK(render_process_host->InSameStoragePartition(
      BrowserContext::GetStoragePartition(browser_context, site_instance,
                                          false /* can_create */)));

  return render_process_host;
}

void RenderProcessHostImpl::CreateSharedRendererHistogramAllocator() {
  // Create a persistent memory segment for renderer histograms only if
  // they're active in the browser.
  if (!base::GlobalHistogramAllocator::Get()) {
    if (is_initialized_) {
      HistogramController::GetInstance()->SetHistogramMemory<RenderProcessHost>(
          this, mojo::ScopedSharedBufferHandle());
    }
    return;
  }

  // Get handle to the renderer process. Stop if there is none.
  base::ProcessHandle destination = GetProcess().Handle();
  if (destination == base::kNullProcessHandle)
    return;

  // If a renderer crashes before completing startup and gets restarted, this
  // method will get called a second time meaning that a metrics-allocator
  // already exists. Don't recreate it.
  if (!metrics_allocator_) {
    // Create persistent/shared memory and allow histograms to be stored in
    // it. Memory that is not actualy used won't be physically mapped by the
    // system. RendererMetrics usage, as reported in UMA, peaked around 0.7MiB
    // as of 2016-12-20.
    auto shm = std::make_unique<base::SharedMemory>();
    if (!shm->CreateAndMapAnonymous(2 << 20))  // 2 MiB
      return;
    metrics_allocator_ =
        std::make_unique<base::SharedPersistentMemoryAllocator>(
            std::move(shm), GetID(), "RendererMetrics", /*readonly=*/false);
  }

  HistogramController::GetInstance()->SetHistogramMemory<RenderProcessHost>(
      this, mojo::WrapSharedMemoryHandle(
                metrics_allocator_->shared_memory()->handle().Duplicate(),
                metrics_allocator_->shared_memory()->mapped_size(),
                mojo::UnwrappedSharedMemoryHandleProtection::kReadWrite));
}

void RenderProcessHostImpl::ProcessDied(
    bool already_dead,
    ChildProcessTerminationInfo* known_info) {
  // Our child process has died.  If we didn't expect it, it's a crash.
  // In any case, we need to let everyone know it's gone.
  // The OnChannelError notification can fire multiple times due to nested sync
  // calls to a renderer. If we don't have a valid channel here it means we
  // already handled the error.

  // It should not be possible for us to be called re-entrantly.
  DCHECK(!within_process_died_observer_);

  // It should not be possible for a process death notification to come in while
  // we are dying.
  DCHECK(!deleting_soon_);

  // child_process_launcher_ can be NULL in single process mode or if fast
  // termination happened.
  ChildProcessTerminationInfo info;
  info.exit_code = 0;
  if (known_info) {
    info = *known_info;
  } else if (child_process_launcher_.get()) {
    info = child_process_launcher_->GetChildTerminationInfo(already_dead);
    if (already_dead && info.status == base::TERMINATION_STATUS_STILL_RUNNING) {
      // May be in case of IPC error, if it takes long time for renderer
      // to exit. Child process will be killed in any case during
      // child_process_launcher_.reset(). Make sure we will not broadcast
      // FrameHostMsg_RenderProcessGone with status
      // TERMINATION_STATUS_STILL_RUNNING, since this will break WebContentsImpl
      // logic.
      info.status = base::TERMINATION_STATUS_PROCESS_CRASHED;

// TODO(siggi): Remove this once https://crbug.com/806661 is resolved.
#if defined(OS_WIN)
      if (info.exit_code == WAIT_TIMEOUT && g_analyze_hung_renderer)
        g_analyze_hung_renderer(child_process_launcher_->GetProcess());
#endif
    }
  }

  child_process_launcher_.reset();
  is_dead_ = true;
  // Make sure no IPCs or mojo calls from the old process get dispatched after
  // it has died.
  ResetIPC();
  process_resource_coordinator_.reset();

  UpdateProcessPriority();

  within_process_died_observer_ = true;
  NotificationService::current()->Notify(
      NOTIFICATION_RENDERER_PROCESS_CLOSED, Source<RenderProcessHost>(this),
      Details<ChildProcessTerminationInfo>(&info));
  for (auto& observer : observers_)
    observer.RenderProcessExited(this, info);
  within_process_died_observer_ = false;

  RemoveUserData(kSessionStorageHolderKey);

  base::IDMap<IPC::Listener*>::iterator iter(&listeners_);
  while (!iter.IsAtEnd()) {
    iter.GetCurrentValue()->OnMessageReceived(FrameHostMsg_RenderProcessGone(
        iter.GetCurrentKey(), static_cast<int>(info.status), info.exit_code));
    iter.Advance();
  }

  // Initialize a new ChannelProxy in case this host is re-used for a new
  // process. This ensures that new messages can be sent on the host ASAP (even
  // before Init()) and they'll eventually reach the new process.
  //
  // Note that this may have already been called by one of the above observers
  EnableSendQueue();

  // It's possible that one of the calls out to the observers might have caused
  // this object to be no longer needed.
  if (delayed_cleanup_needed_)
    Cleanup();

  compositing_mode_reporter_.reset();

  HistogramController::GetInstance()->NotifyChildDied<RenderProcessHost>(this);
  // This object is not deleted at this point and might be reused later.
  // TODO(darin): clean this up
}

void RenderProcessHostImpl::ResetIPC() {
  if (renderer_host_binding_.is_bound())
    renderer_host_binding_.Unbind();
  if (route_provider_binding_.is_bound())
    route_provider_binding_.Close();
  associated_interface_provider_bindings_.CloseAllBindings();
  associated_interfaces_.reset();

  // Destroy all embedded CompositorFrameSinks.
  embedded_frame_sink_provider_.reset();

  // If RenderProcessHostImpl is reused, the next renderer will send a new
  // request for FrameSinkProvider so make sure frame_sink_provider_ is ready
  // for that.
  frame_sink_provider_.Unbind();

  // It's important not to wait for the DeleteTask to delete the channel
  // proxy. Kill it off now. That way, in case the profile is going away, the
  // rest of the objects attached to this RenderProcessHost start going
  // away first, since deleting the channel proxy will post a
  // OnChannelClosed() to IPC::ChannelProxy::Context on the IO thread.
  ResetChannelProxy();
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

void RenderProcessHostImpl::ReleaseOnCloseACK(
    RenderProcessHost* host,
    const SessionStorageNamespaceMap& sessions,
    int widget_route_id) {
  DCHECK(host);
  if (sessions.empty())
    return;
  SessionStorageHolder* holder = static_cast<SessionStorageHolder*>(
      host->GetUserData(kSessionStorageHolderKey));
  if (!holder) {
    holder = new SessionStorageHolder();
    host->SetUserData(kSessionStorageHolderKey, base::WrapUnique(holder));
  }
  holder->Hold(sessions, widget_route_id);
}

void RenderProcessHostImpl::SuddenTerminationChanged(bool enabled) {
  SetSuddenTerminationAllowed(enabled);
}

void RenderProcessHostImpl::UpdateProcessPriorityInputs() {
  int32_t new_visible_widgets_count = 0;
  unsigned int new_frame_depth = kMaxFrameDepthForPriority;
  bool new_intersects_viewport = false;
#if defined(OS_ANDROID)
  ChildProcessImportance new_effective_importance =
      ChildProcessImportance::NORMAL;
#endif
  for (auto* client : priority_clients_) {
    Priority priority = client->GetPriority();

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

#if defined(OS_ANDROID)
    new_effective_importance =
        std::max(new_effective_importance, priority.importance);
#endif
  }

  bool inputs_changed = new_visible_widgets_count != visible_clients_;
  // Hide this update behind the ShouldBoostPriorityForPendingViews() experiment
  // at the moment to avoid causing an undesired early UpdateProcessPriority().
  // See the comment in OnProcessLaunched() and https://crbug.com/560446.
  if (ShouldBoostPriorityForPendingViews()) {
    inputs_changed = inputs_changed || frame_depth_ != new_frame_depth ||
                     intersects_viewport_ != new_intersects_viewport;
  }
  visible_clients_ = new_visible_widgets_count;
  frame_depth_ = new_frame_depth;
  intersects_viewport_ = new_intersects_viewport;
#if defined(OS_ANDROID)
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
    // This path can be hit early (no-op) or on ProcessDied(). Reset |priority_|
    // to defaults in case this RenderProcessHostImpl is re-used.
    priority_.visible = !blink::kLaunchingProcessIsBackgrounded;
    priority_.boost_for_pending_views = ShouldBoostPriorityForPendingViews();
    return;
  }

  if (!has_recorded_media_stream_frame_depth_metric_ && !visible_clients_ &&
      media_stream_count_) {
    UMA_HISTOGRAM_EXACT_LINEAR(
        "BrowserRenderProcessHost.InvisibleMediaStreamFrameDepth", frame_depth_,
        50);
    has_recorded_media_stream_frame_depth_metric_ = true;
  }

  const ChildProcessLauncherPriority priority(
      visible_clients_ > 0 || base::CommandLine::ForCurrentProcess()->HasSwitch(
                                  switches::kDisableRendererBackgrounding),
      media_stream_count_ > 0, frame_depth_, intersects_viewport_,
      !!pending_views_ /* boost_for_pending_views */,
#if defined(OS_ANDROID)
      // Same hack as in RenderProcessHostImpl::RenderProcessHostImpl.
      // TODO(gab): Clean this up after ShouldBoostPriorityForPendingViews()
      // experiment.
      false
#else
      ShouldBoostPriorityForPendingViews()
#endif
#if defined(OS_ANDROID)
      ,
      GetEffectiveImportance()
#endif
          );

  const bool should_background_changed =
      priority_.is_background() != priority.is_background();
  if (priority_ == priority)
    return;

  TRACE_EVENT2("renderer_host", "RenderProcessHostImpl::UpdateProcessPriority",
               "should_background", priority.is_background(),
               "has_pending_views", priority.boost_for_pending_views);
  priority_ = priority;

#if defined(OS_WIN)
  // The cbstext.dll loads as a global GetMessage hook in the browser process
  // and intercepts/unintercepts the kernel32 API SetPriorityClass in a
  // background thread. If the UI thread invokes this API just when it is
  // intercepted the stack is messed up on return from the interceptor
  // which causes random crashes in the browser process. Our hack for now
  // is to not invoke the SetPriorityClass API if the dll is loaded.
  if (GetModuleHandle(L"cbstext.dll"))
    return;
#endif  // OS_WIN

  // Control the background state from the browser process, otherwise the task
  // telling the renderer to "unbackground" itself may be preempted by other
  // tasks executing at lowered priority ahead of it or simply by not being
  // swiftly scheduled by the OS per the low process priority
  // (http://crbug.com/398103).
  if (!run_renderer_in_process()) {
    DCHECK(child_process_launcher_.get());
    DCHECK(!child_process_launcher_->IsStarting());
    child_process_launcher_->SetProcessPriority(priority_);
  }

  // Notify the child process of background state.
  if (should_background_changed) {
    GetRendererInterface()->SetProcessBackgrounded(priority_.is_background());
  }
}

void RenderProcessHostImpl::OnProcessLaunched() {
  // No point doing anything, since this object will be destructed soon.  We
  // especially don't want to send the RENDERER_PROCESS_CREATED notification,
  // since some clients might expect a RENDERER_PROCESS_TERMINATED afterwards to
  // properly cleanup.
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

    if (child_connection_) {
      child_connection_->SetProcessHandle(
          child_process_launcher_->GetProcess().Handle());
    }

// Not all platforms launch processes in the same backgrounded state. Make
// sure |priority_.visible| reflects this platform's initial process
// state.
#if defined(OS_MACOSX)
    priority_.visible =
        !child_process_launcher_->GetProcess().IsProcessBackgrounded(
            MachBroker::GetInstance());
#elif defined(OS_ANDROID)
    // Android child process priority works differently and cannot be queried
    // directly from base::Process.
    // TODO(https://crbug.com/875933): Fix initial priority on Android to
    // reflect |priority_.is_background()|.
    DCHECK_EQ(blink::kLaunchingProcessIsBackgrounded, !priority_.visible);
#else
    priority_.visible =
        !child_process_launcher_->GetProcess().IsProcessBackgrounded();
#endif  // defined(OS_MACOSX)

    // Only update the priority on startup if boosting is enabled (to avoid
    // reintroducing https://crbug.com/560446#c13 while pending views only
    // experimentally result in a boost).
    if (priority_.boost_for_pending_views)
      UpdateProcessPriority();

    // Share histograms between the renderer and this process.
    CreateSharedRendererHistogramAllocator();
  }

  // Pass bits of global renderer state to the renderer.
  GetRendererInterface()->SetUserAgent(GetContentClient()->GetUserAgent());
  NotifyRendererIfLockedToSite();
  if (SiteIsolationPolicy::UseDedicatedProcessesForAllSites() &&
      base::FeatureList::IsEnabled(features::kV8LowMemoryModeForSubframes)) {
    GetRendererInterface()->EnableV8LowMemoryMode();
  }

  // NOTE: This needs to be before flushing queued messages, because
  // ExtensionService uses this notification to initialize the renderer process
  // with state that must be there before any JavaScript executes.
  //
  // The queued messages contain such things as "navigate". If this notification
  // was after, we can end up executing JavaScript before the initialization
  // happens.
  NotificationService::current()->Notify(NOTIFICATION_RENDERER_PROCESS_CREATED,
                                         Source<RenderProcessHost>(this),
                                         NotificationService::NoDetails());

  if (child_process_launcher_)
    channel_->Flush();

  if (IsReady()) {
    DCHECK(!sent_render_process_ready_);
    sent_render_process_ready_ = true;
    // Send RenderProcessReady only if the channel is already connected.
    for (auto& observer : observers_)
      observer.RenderProcessReady(this);
  }

  GetProcessResourceCoordinator()->SetLaunchTime(base::Time::Now());
  GetProcessResourceCoordinator()->SetPID(GetProcess().Pid());

  WebRTCInternals* webrtc_internals = WebRTCInternals::GetInstance();
  if (webrtc_internals->IsAudioDebugRecordingsEnabled()) {
    EnableAudioDebugRecordings(
        webrtc_internals->GetAudioDebugRecordingsFilePath());
  }
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
  ProcessDied(true, &info);
}

void RenderProcessHostImpl::OnUserMetricsRecordAction(
    const std::string& action) {
  base::RecordComputedAction(action);
}

void RenderProcessHostImpl::OnCloseACK(int closed_widget_route_id) {
  SessionStorageHolder* holder =
      static_cast<SessionStorageHolder*>(GetUserData(kSessionStorageHolderKey));
  if (!holder)
    return;
  holder->Release(closed_widget_route_id);
}

void RenderProcessHostImpl::OnGpuSwitched() {
  RecomputeAndUpdateWebKitPreferences();
}

// static
RenderProcessHost*
RenderProcessHostImpl::FindReusableProcessHostForSiteInstance(
    SiteInstanceImpl* site_instance) {
  BrowserContext* browser_context = site_instance->GetBrowserContext();
  GURL site_url(site_instance->GetSiteURL());
  if (!ShouldFindReusableProcessHostForSite(browser_context, site_url))
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
        site_instance, &eligible_foreground_hosts, &eligible_background_hosts);
  }

  if (eligible_foreground_hosts.empty()) {
    // If needed, add the RenderProcessHosts hosting a frame for |site_url| to
    // the list of eligible RenderProcessHosts.
    SiteProcessCountTracker* committed_tracker =
        static_cast<SiteProcessCountTracker*>(
            browser_context->GetUserData(kCommittedSiteProcessCountTrackerKey));
    if (committed_tracker) {
      committed_tracker->FindRenderProcessesForSiteInstance(
          site_instance, &eligible_foreground_hosts,
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

void RenderProcessHostImpl::CreateMediaStreamTrackMetricsHost(
    mojom::MediaStreamTrackMetricsHostRequest request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!media_stream_track_metrics_host_)
    media_stream_track_metrics_host_.reset(new MediaStreamTrackMetricsHost());
  media_stream_track_metrics_host_->BindRequest(std::move(request));
}

void RenderProcessHostImpl::OnRegisterAecDumpConsumer(int id) {
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&RenderProcessHostImpl::RegisterAecDumpConsumerOnUIThread,
                     weak_factory_.GetWeakPtr(), id));
}

void RenderProcessHostImpl::OnUnregisterAecDumpConsumer(int id) {
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(
          &RenderProcessHostImpl::UnregisterAecDumpConsumerOnUIThread,
          weak_factory_.GetWeakPtr(), id));
}

void RenderProcessHostImpl::RegisterAecDumpConsumerOnUIThread(int id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  aec_dump_consumers_.push_back(id);

  WebRTCInternals* webrtc_internals = WebRTCInternals::GetInstance();
  if (webrtc_internals->IsAudioDebugRecordingsEnabled()) {
    base::FilePath file_with_extensions = GetAecDumpFilePathWithExtensions(
        webrtc_internals->GetAudioDebugRecordingsFilePath());
    EnableAecDumpForId(file_with_extensions, id);
  }
}

void RenderProcessHostImpl::UnregisterAecDumpConsumerOnUIThread(int id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto it =
      std::find(aec_dump_consumers_.begin(), aec_dump_consumers_.end(), id);
  if (it != aec_dump_consumers_.end())
    aec_dump_consumers_.erase(it);
}

void RenderProcessHostImpl::EnableAecDumpForId(const base::FilePath& file,
                                               int id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::PostTaskAndReplyWithResult(
      &GetAecDumpFileTaskRunner(), FROM_HERE,
      base::Bind(&CreateFileForProcess, file.AddExtension(IntToStringType(id))),
      base::Bind(&RenderProcessHostImpl::SendAecDumpFileToRenderer,
                 weak_factory_.GetWeakPtr(), id));
}

void RenderProcessHostImpl::SendAecDumpFileToRenderer(
    int id,
    IPC::PlatformFileForTransit file_for_transit) {
  if (file_for_transit == IPC::InvalidPlatformFileForTransit())
    return;
  Send(new AecDumpMsg_EnableAecDump(id, file_for_transit));
}

void RenderProcessHostImpl::SendDisableAecDumpToRenderer() {
  Send(new AecDumpMsg_DisableAecDump());
}

base::FilePath RenderProcessHostImpl::GetAecDumpFilePathWithExtensions(
    const base::FilePath& file) {
  return file.AddExtension(IntToStringType(GetProcess().Pid()))
      .AddExtension(kAecDumpFileNameAddition);
}

base::SequencedTaskRunner& RenderProcessHostImpl::GetAecDumpFileTaskRunner() {
  if (!audio_debug_recordings_file_task_runner_) {
    audio_debug_recordings_file_task_runner_ =
        base::CreateSequencedTaskRunnerWithTraits(
            {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
             base::TaskPriority::USER_BLOCKING});
  }
  return *audio_debug_recordings_file_task_runner_;
}

void RenderProcessHostImpl::OnAec3Enabled() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // The callback should be set unless the message from the renderer is
  // spurious.
  if (!aec3_set_callback_.is_null())
    std::move(aec3_set_callback_).Run(true, std::string());
}

void RenderProcessHostImpl::RecomputeAndUpdateWebKitPreferences() {
  // We are updating all widgets including swapped out ones.
  for (auto* widget : widgets_) {
    RenderViewHost* rvh = RenderViewHost::From(widget);
    if (!rvh)
      continue;

    rvh->OnWebkitPreferencesChanged();
  }
}

// static
void RenderProcessHostImpl::OnMojoError(int render_process_id,
                                        const std::string& error) {
  LOG(ERROR) << "Terminating render process for bad Mojo message: " << error;

  // The ReceivedBadMessage call below will trigger a DumpWithoutCrashing.
  // Capture the error message in a crash key value.
  base::debug::ScopedCrashKeyString error_key_value(
      bad_message::GetMojoErrorCrashKey(), error);
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

void RenderProcessHostImpl::SetBrowserPluginMessageFilterSubFilterForTesting(
    scoped_refptr<BrowserMessageFilter> message_filter) const {
  bp_message_filter_->SetSubFilterForTesting(std::move(message_filter));
}

}  // namespace content
