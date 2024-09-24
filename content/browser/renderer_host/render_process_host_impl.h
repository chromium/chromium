// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_PROCESS_HOST_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_PROCESS_HOST_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/safe_ref.h"
#include "base/memory/structured_shared_memory.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/observer_list.h"
#include "base/scoped_observation_traits.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/metrics/histogram_child_process.h"
#include "components/services/storage/public/cpp/buckets/bucket_id.h"
#include "components/services/storage/public/cpp/buckets/bucket_info.h"
#include "components/services/storage/public/cpp/quota_error_or.h"
#include "content/browser/blob_storage/file_backed_blob_factory_worker_impl.h"
#include "content/browser/child_process_launcher.h"
#include "content/browser/renderer_host/media/aec_dump_manager_impl.h"
#include "content/browser/renderer_host/render_process_host_internal_observer.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/tracing/tracing_service_controller.h"
#include "content/common/buildflags.h"
#include "content/common/child_process.mojom.h"
#include "content/common/content_export.h"
#include "content/common/media/media_log_records.mojom-forward.h"
#include "content/common/renderer.mojom.h"
#include "content/common/renderer_host.mojom.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "media/mojo/mojom/interface_factory.mojom-forward.h"
#include "media/mojo/mojom/video_decode_perf_history.mojom-forward.h"
#include "media/mojo/mojom/video_encoder_metrics_provider.mojom-forward.h"
#include "media/mojo/mojom/webrtc_video_perf.mojom-forward.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "mojo/public/cpp/system/invitation.h"
#include "net/base/network_isolation_key.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/network/public/mojom/p2p.mojom-forward.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/memory_instrumentation.mojom.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/tracing/public/mojom/traced_process.mojom-forward.h"
#include "services/viz/public/mojom/compositing/compositing_mode_watcher.mojom.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/associated_interfaces/associated_interfaces.mojom-forward.h"
#include "third_party/blink/public/mojom/background_sync/background_sync.mojom-forward.h"
#include "third_party/blink/public/mojom/blob/file_backed_blob_factory.mojom.h"
#include "third_party/blink/public/mojom/buckets/bucket_manager_host.mojom-forward.h"
#include "third_party/blink/public/mojom/call_stack_generator/call_stack_generator.mojom.h"
#include "third_party/blink/public/mojom/dom_storage/dom_storage.mojom.h"
#include "third_party/blink/public/mojom/filesystem/file_system.mojom-forward.h"
#include "third_party/blink/public/mojom/frame_sinks/embedded_frame_sink.mojom-forward.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-shared.h"
#include "third_party/blink/public/mojom/loader/code_cache.mojom-forward.h"
#include "third_party/blink/public/mojom/plugins/plugin_registry.mojom-forward.h"
#include "third_party/blink/public/mojom/push_messaging/push_messaging.mojom-forward.h"
#include "third_party/blink/public/mojom/webdatabase/web_database.mojom-forward.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_proto.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"
#include "ui/gfx/gpu_memory_buffer.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/memory/memory_pressure_listener.h"
#include "content/public/browser/android/child_process_importance.h"
#endif

#if BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
#include "media/mojo/mojom/stable/stable_video_decoder.mojom.h"
#endif  // BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)

#if BUILDFLAG(IS_FUCHSIA)
#include "media/fuchsia_media_codec_provider_impl.h"
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "content/browser/child_thread_type_switcher_linux.h"
#include "media/mojo/mojom/video_encode_accelerator.mojom.h"
#endif

namespace base {
class CommandLine;
class PersistentMemoryAllocator;
#if BUILDFLAG(IS_ANDROID)
namespace android {
enum class ChildBindingState;
}
#endif
}  // namespace base

namespace blink {
class AssociatedInterfaceRegistry;
class StorageKey;
}  // namespace blink

namespace perfetto {
namespace protos {
namespace pbzero {
class RenderProcessHost;
}
}  // namespace protos
}  // namespace perfetto

namespace tracing {
class SystemTracingService;
}  // namespace tracing

namespace url {
class Origin;
}  // namespace url

namespace viz {
class GpuClient;
}  // namespace viz

namespace content {
class AgentSchedulingGroupHost;
class BucketContext;
class EmbeddedFrameSinkProviderImpl;
class FileSystemManagerImpl;
class FramelessMediaInterfaceProxy;
class InProcessChildThreadParams;
class IsolationContext;
class MediaStreamTrackMetricsHost;
class P2PSocketDispatcherHost;
class PepperRendererConnection;
class PermissionServiceContext;
class PluginRegistryImpl;
class ProcessLock;
class PushMessagingManager;
class RenderProcessHostCreationObserver;
class RenderProcessHostFactory;
class RenderProcessHostPriorityClients;
class RenderProcessHostTestBase;
class RenderWidgetHelper;
class SiteInfo;
class SiteInstance;
class SiteInstanceImpl;
enum class ProcessReusePolicy;
struct ChildProcessTerminationInfo;
struct GlobalRenderFrameHostId;

typedef base::Thread* (*RendererMainThreadFactoryFunction)(
    const InProcessChildThreadParams& params,
    int32_t renderer_client_id);

// Implements a concrete RenderProcessHost for the browser process for talking
// to actual renderer processes (as opposed to mocks).
//
// Represents the browser side of the browser <--> renderer communication
// channel. There will be one RenderProcessHost per renderer process.
//
// This object is refcounted so that it can release its resources when all
// hosts using it go away.
//
// This object communicates back and forth with the RenderProcess object
// running in the renderer process. Each RenderProcessHost and RenderProcess
// keeps a list of `blink::WebView` (renderer) and WebContentsImpl (browser)
// which are correlated with IDs. This way, the Views and the corresponding
// ViewHosts communicate through the two process objects.
//
// A RenderProcessHost is also associated with one and only one
// StoragePartition.  This allows us to implement strong storage isolation
// because all the IPCs from the `blink::WebView`s (renderer) will only ever be
// able to access the partition they are assigned to.
class CONTENT_EXPORT RenderProcessHostImpl
    : public RenderProcessHost,
      public ChildProcessLauncher::Client,
      public mojom::RendererHost,
      public blink::mojom::DomStorageProvider,
      public memory_instrumentation::mojom::CoordinatorConnector,
      public metrics::HistogramChildProcess
#if BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
    ,
      public media::stable::mojom::StableVideoDecoderTracker
#endif  // BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
{
 public:
  // Special depth used when there are no RenderProcessHostPriorityClients.
  static const unsigned int kMaxFrameDepthForPriority;

  // Exposed as a public constant to share with other entities that need to
  // accommodate frame/process shutdown delays.
  static constexpr int kKeepAliveHandleFactoryTimeoutInMSec = 30 * 1000;
  static const base::TimeDelta kKeepAliveHandleFactoryTimeout;

  // Create a new RenderProcessHost. The storage partition for the process
  // is retrieved from |browser_context| based on information in
  // |site_instance|. The default storage partition is selected if
  // |site_instance| is null.
  static RenderProcessHost* CreateRenderProcessHost(
      BrowserContext* browser_context,
      SiteInstanceImpl* site_instance);

  ~RenderProcessHostImpl() override;

  RenderProcessHostImpl(const RenderProcessHostImpl& other) = delete;
  RenderProcessHostImpl& operator=(const RenderProcessHostImpl& other) = delete;

  // RenderProcessHost implementation (public portion).
  bool Init() override;
  void EnableSendQueue() override;
  int GetNextRoutingID() override;
  void AddRoute(int32_t routing_id, IPC::Listener* listener) override;
  void RemoveRoute(int32_t routing_id) override;
  void AddObserver(RenderProcessHostObserver* observer) override;
  void RemoveObserver(RenderProcessHostObserver* observer) override;
  void ShutdownForBadMessage(CrashReportMode crash_report_mode) override;
  void UpdateClientPriority(RenderProcessHostPriorityClient* client) override;
  int VisibleClientCount() override;
  unsigned int GetFrameDepth() override;
  bool GetIntersectsViewport() override;
  bool IsForGuestsOnly() override;
  bool IsJitDisabled() override;
  bool AreV8OptimizationsDisabled() override;
  bool IsPdf() override;
  StoragePartitionImpl* GetStoragePartition() override;
  bool Shutdown(int exit_code) override;
  bool ShutdownRequested() override;
  bool FastShutdownIfPossible(size_t page_count = 0,
                              bool skip_unload_handlers = false) override;
  const base::Process& GetProcess() override;
  bool IsReady() override;
  BrowserContext* GetBrowserContext() override;
  bool InSameStoragePartition(StoragePartition* partition) override;
  int GetID() const override;
  base::SafeRef<RenderProcessHost> GetSafeRef() const override;
  bool IsInitializedAndNotDead() override;
  bool IsDeletingSoon() override;
  void SetBlocked(bool blocked) override;
  bool IsBlocked() override;
  base::CallbackListSubscription RegisterBlockStateChangedCallback(
      const BlockStateChangedCallback& cb) override;
  void Cleanup() override;
  void AddPendingView() override;
  void RemovePendingView() override;
  void AddPriorityClient(
      RenderProcessHostPriorityClient* priority_client) override;
  void RemovePriorityClient(
      RenderProcessHostPriorityClient* priority_client) override;
#if !BUILDFLAG(IS_ANDROID)
  void SetPriorityOverride(base::Process::Priority priority) override;
  bool HasPriorityOverride() override;
  void ClearPriorityOverride() override;
#endif
#if BUILDFLAG(IS_ANDROID)
  ChildProcessImportance GetEffectiveImportance() override;
  base::android::ChildBindingState GetEffectiveChildBindingState() override;
  void DumpProcessStack() override;
#endif
  void SetSuddenTerminationAllowed(bool enabled) override;
  bool SuddenTerminationAllowed() override;
  IPC::ChannelProxy* GetChannel() override;
#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
  void AddFilter(BrowserMessageFilter* filter) override;
#endif
  bool FastShutdownStarted() override;
  base::TimeDelta GetChildProcessIdleTime() override;
  viz::GpuClient* GetGpuClient();
  FilterURLResult FilterURL(bool empty_allowed, GURL* url) override;
  void EnableAudioDebugRecordings(const base::FilePath& file) override;
  void DisableAudioDebugRecordings() override;
  WebRtcStopRtpDumpCallback StartRtpDump(
      bool incoming,
      bool outgoing,
      WebRtcRtpPacketCallback packet_callback) override;
  void BindReceiver(mojo::GenericPendingReceiver receiver) override;
  std::unique_ptr<base::PersistentMemoryAllocator> TakeMetricsAllocator()
      override;
  const base::TimeTicks& GetLastInitTime() override;
  base::Process::Priority GetPriority() override;
  std::string GetKeepAliveDurations() const override;
  size_t GetShutdownDelayRefCount() const override;
  int GetRenderFrameHostCount() const override;
  void RegisterRenderFrameHost(
      const GlobalRenderFrameHostId& render_frame_host_id,
      bool is_outermost_main_frame) override;
  void UnregisterRenderFrameHost(
      const GlobalRenderFrameHostId& render_frame_host_id,
      bool is_outermost_main_frame) override;
  void ForEachRenderFrameHost(
      base::FunctionRef<void(RenderFrameHost*)> on_render_frame_host) override;
  void IncrementWorkerRefCount() override;
  void DecrementWorkerRefCount() override;
  void IncrementPendingReuseRefCount() override;
  void DecrementPendingReuseRefCount() override;
  void DisableRefCounts() override;
  bool AreRefCountsDisabled() override;
  mojom::Renderer* GetRendererInterface() override;

  blink::mojom::CallStackGenerator* GetJavaScriptCallStackGeneratorInterface();

  bool MayReuseHost() override;
  bool IsUnused() override;
  void SetIsUsed() override;

  bool HostHasNotBeenUsed() override;
  bool IsSpare() const override;
  void SetProcessLock(const IsolationContext& isolation_context,
                      const ProcessLock& process_lock) override;
  ProcessLock GetProcessLock() const override;
  bool IsProcessLockedToSiteForTesting() override;
  void BindCacheStorage(
      const network::CrossOriginEmbedderPolicy& cross_origin_embedder_policy,
      mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
          coep_reporter,
      const network::DocumentIsolationPolicy& document_isolation_policy,
      const storage::BucketLocator& bucket_locator,
      mojo::PendingReceiver<blink::mojom::CacheStorage> receiver) override;
  void BindIndexedDB(
      const blink::StorageKey& storage_key,
      BucketContext& bucket_context,
      mojo::PendingReceiver<blink::mojom::IDBFactory> receiver) override;
  void BindBucketManagerHost(
      base::WeakPtr<BucketContext> bucket_context,
      mojo::PendingReceiver<blink::mojom::BucketManagerHost> receiver) override;
  void ForceCrash() override;
  std::string GetInfoForBrowserContextDestructionCrashReporting() override;
  void WriteIntoTrace(perfetto::TracedProto<TraceProto> proto) const override;
#if BUILDFLAG(CLANG_PROFILING_INSIDE_SANDBOX)
  void DumpProfilingData(base::OnceClosure callback) override;
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
  void ReinitializeLogging(uint32_t logging_dest,
                           base::ScopedFD log_file_descriptor) override;
#endif
  void SetBatterySaverMode(bool battery_saver_mode_enabled) override;
  uint64_t GetPrivateMemoryFootprint() override;

  void PauseSocketManagerForRenderFrameHost(
      const GlobalRenderFrameHostId& render_frame_host_id) override;
  void ResumeSocketManagerForRenderFrameHost(
      const GlobalRenderFrameHostId& render_frame_host_id) override;

  // IPC::Sender via RenderProcessHost.
  bool Send(IPC::Message* msg) override;

  // IPC::Listener via RenderProcessHost.
  bool OnMessageReceived(const IPC::Message& msg) override;
  void OnAssociatedInterfaceRequest(
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle handle) override;
  void OnChannelConnected(int32_t peer_pid) override;
  void OnChannelError() override;
  void OnBadMessageReceived(const IPC::Message& message) override;

  // ChildProcessLauncher::Client implementation.
  void OnProcessLaunched() override;
  void OnProcessLaunchFailed(int error_code) override;

  const std::string& GetUnresponsiveDocumentJavascriptCallStack() const;
  const blink::LocalFrameToken& GetUnresponsiveDocumentToken() const;

  void SetUnresponsiveDocumentJSCallStackAndToken(
      const std::string& untrusted_javascript_call_stack,
      const std::optional<blink::LocalFrameToken>& frame_token);

  void InterruptJavaScriptIsolateAndCollectCallStack();

  // HistogramChildProcess implementation:
  void BindChildHistogramFetcherFactory(
      mojo::PendingReceiver<metrics::mojom::ChildHistogramFetcherFactory>
          factory) override;

  // Call this function when it is evident that the child process is actively
  // performing some operation, for example if we just received an IPC message.
  void mark_child_process_activity_time() {
    child_process_activity_time_ = base::TimeTicks::Now();
  }

  // Return the set of previously stored data for a `frame_token`.
  // The routing ID and frame tokens were stored on the IO thread via the
  // RenderMessageFilter::GenerateSingleFrameRoutingInfo mojo call. Returns
  // false if `frame_token` was not found in the token table.
  bool TakeStoredDataForFrameToken(const blink::LocalFrameToken& frame_token,
                                   int32_t& new_routing_id,
                                   base::UnguessableToken& devtools_frame_token,
                                   blink::DocumentToken& document_token);

  void AddInternalObserver(RenderProcessHostInternalObserver* observer);
  void RemoveInternalObserver(RenderProcessHostInternalObserver* observer);

  // Register/unregister the host identified by the host id in the global host
  // list.
  static void RegisterHost(int host_id, RenderProcessHost* host);
  static void UnregisterHost(int host_id);

  // "Keep alive ref count" represents the number of the customers of this
  // render process who wish the renderer process to be alive. While the ref
  // count is positive, |this| object will keep the renderer process alive,
  // unless DisableRefCounts() is called. |handle_id| is a unique identifier
  // associated with each keep-alive request.
  // TODO(wjmaclean): Remove |handle_id| once the causes behind
  // https://crbug.com/1148542 are known.
  //
  // Here is the list of users:
  //  - Keepalive request (if the KeepAliveRendererForKeepaliveRequests
  //    feature is enabled):
  //    When a fetch request with keepalive flag
  //    (https://fetch.spec.whatwg.org/#request-keepalive-flag) specified is
  //    pending, it wishes the renderer process to be kept alive.
  //  - Unload handlers:
  //    Keeps the process alive briefly to give subframe unload handlers a
  //    chance to execute after their parent frame navigates or is detached.
  //    See https://crbug.com/852204.
  //  - Process reuse timer (experimental):
  //    Keeps the process alive for a set period of time in case it can be
  //    reused for the same site. See https://crbug.com/894253.
  void IncrementKeepAliveRefCount(uint64_t handle_id_);
  void DecrementKeepAliveRefCount(uint64_t handle_id_);

  int keep_alive_ref_count() const { return keep_alive_ref_count_; }
  int worker_ref_count() const { return worker_ref_count_; }

  // See `navigation_state_keepalive_count_`.
  void IncrementNavigationStateKeepAliveCount();
  void DecrementNavigationStateKeepAliveCount();

  static void RegisterCreationObserver(
      RenderProcessHostCreationObserver* observer);
  static void UnregisterCreationObserver(
      RenderProcessHostCreationObserver* observer);

  // Implementation of FilterURL below that can be shared with the mock class.
  static FilterURLResult FilterURL(RenderProcessHost* rph,
                                   bool empty_allowed,
                                   GURL* url);

  // Returns the current count of renderer processes. For the count used when
  // comparing against the process limit, see `GetProcessCountForLimit`.
  static size_t GetProcessCount();

  // Returns the current process count for comparisons against
  // GetMaxRendererProcessCount, taking into account any processes the embedder
  // wants to ignore via ContentBrowserClient::GetProcessCountToIgnoreForLimit.
  static size_t GetProcessCountForLimit();

  // Returns true if |host| is suitable for rendering a page in the given
  // |isolation_context|, where the page would utilize |site_info.site_url()| as
  // its SiteInstance site URL, and its process would be locked to
  // |site_info.lock_url()|. Site and lock urls may differ in cases where
  // an effective URL is not the actual site that the process is locked to,
  // which happens for hosted apps.
  static bool IsSuitableHost(RenderProcessHost* host,
                             const IsolationContext& isolation_context,
                             const SiteInfo& site_info);

  // Helper function that returns true if |host| returns true for MayReuseHost()
  // and IsSuitableHost() returns true.
  static bool MayReuseAndIsSuitable(RenderProcessHost* host,
                                    const IsolationContext& isolation_context,
                                    const SiteInfo& site_info);
  // Same as the method above but uses the IsolationContext and SiteInfo
  // provided by |site_instance|.
  static bool MayReuseAndIsSuitable(RenderProcessHost* host,
                                    SiteInstanceImpl* site_instance);

  // Returns true if RenderProcessHost shutdown should be delayed by a few
  // seconds to allow the subframe's process to be potentially reused. This aims
  // to reduce process churn in navigations where the source and destination
  // share subframes. Only returns true on platforms where process startup is
  // expensive.
  static bool ShouldDelayProcessShutdown();

  // Returns an existing RenderProcessHost for |site_info| in
  // |isolation_context|, if one exists.  Otherwise a new RenderProcessHost
  // should be created and registered using RegisterProcessHostForSite(). This
  // should only be used for process-per-site mode, which can be enabled
  // globally with a command line flag or per-site, as determined by
  // SiteInstanceImpl::ShouldUseProcessPerSite.
  static RenderProcessHost* GetSoleProcessHostForSite(
      const IsolationContext& isolation_context,
      const SiteInfo& site_info);

  // Registers the given |process| to be used for all sites identified by
  // |site_instance| within its BrowserContext. This should only be used for
  // process-per-site mode, which can be enabled globally with a command line
  // flag or per-site, as determined by
  // SiteInstanceImpl::ShouldUseProcessPerSite.
  static void RegisterSoleProcessHostForSite(RenderProcessHost* process,
                                             SiteInstanceImpl* site_instance);

  // Returns a suitable RenderProcessHost to use for |site_instance|. Depending
  // on the SiteInstance's ProcessReusePolicy and its url, this may be an
  // existing RenderProcessHost or a new one.
  //
  // This is the main entrypoint into the process assignment logic, which
  // handles all cases.  These cases include:
  // - process-per-site: see
  //   RegisterSoleProcessHostForSite/GetSoleProcessHostForSite.
  // - REUSE_PENDING_OR_COMMITTED reuse policy (for ServiceWorkers and OOPIFs):
  //   see FindReusableProcessHostForSiteInstance.
  // - normal process reuse when over process limit:  see
  //   GetExistingProcessHost.
  // - using the spare RenderProcessHost when possible: see
  //   MaybeTakeSpareRenderProcessHost.
  // - process creation when an existing process couldn't be found: see
  //   CreateRenderProcessHost.
  static RenderProcessHost* GetProcessHostForSiteInstance(
      SiteInstanceImpl* site_instance);

  // Should be called when `site_instance` is used in a navigation.
  //
  // The SpareRenderProcessHostManager can decide how to respond (for example,
  // by shutting down the spare process to conserve resources, or alternatively
  // by making sure that the spare process belongs to the same BrowserContext as
  // the most recent navigation).
  static void NotifySpareManagerAboutRecentlyUsedSiteInstance(
      SiteInstance* site_instance);

  // This enum backs a histogram, so do not change the order of entries or
  // remove entries and update enums.xml if adding new entries.
  enum class SpareProcessMaybeTakeAction {
    kNoSparePresent = 0,
    kMismatchedBrowserContext = 1,
    kMismatchedStoragePartition = 2,
    kRefusedByEmbedder = 3,
    kSpareTaken = 4,
    kRefusedBySiteInstance = 5,
    kRefusedForPdfContent = 6,
    kMaxValue = kRefusedForPdfContent
  };

  // Please keep in sync with "RenderProcessHostDelayShutdownReason" in
  // tools/metrics/histograms/metadata/browser/enums.xml. These values should
  // not be renumbered.
  enum class DelayShutdownReason {
    kNoDelay = 0,
    // There are active or pending views other than the ones shutting down.
    kOtherActiveOrPendingViews = 1,
    // Single process mode never shuts down the renderer.
    kSingleProcess = 2,
    // Render process hasn't started or is probably crashed.
    kNoProcess = 3,
    // There is unload handler.
    kUnload = 4,
    // There is pending fetch keepalive request.
    kFetchKeepAlive = 5,
    // There is worker.
    kWorker = 6,
    // The process is pending to reuse.
    kPendingReuse = 7,
    // The process is requested to delay shutdown.
    kShutdownDelay = 8,
    // Has listeners.
    kListener = 9,
    // Delays until all observer callbacks completed.
    kObserver = 10,
    // There are NavigationStateKeepAlive objects in this process.
    kNavigationStateKeepAlive = 11,

    kMaxValue = kNavigationStateKeepAlive,
  };

  static scoped_refptr<base::SingleThreadTaskRunner>
  GetInProcessRendererThreadTaskRunnerForTesting();

#if !BUILDFLAG(IS_ANDROID)
  // Gets the platform-specific limit. Used by GetMaxRendererProcessCount().
  static size_t GetPlatformMaxRendererProcessCount();
#endif

  // This forces a renderer that is running "in process" to shut down.
  static void ShutDownInProcessRenderer();

  static void RegisterRendererMainThreadFactory(
      RendererMainThreadFactoryFunction create);

  // Allows external code to supply a callback which handles a DomStorage
  // binding request. Used for supplying test versions of DomStorage.
  using DomStorageBinder = base::RepeatingCallback<void(
      RenderProcessHostImpl* rph,
      mojo::PendingReceiver<blink::mojom::DomStorage> receiver)>;
  static void SetDomStorageBinderForTesting(DomStorageBinder binder);
  static bool HasDomStorageBinderForTesting();

  using BadMojoMessageCallbackForTesting =
      base::RepeatingCallback<void(int render_process_host_id,
                                   const std::string& error)>;
  static void SetBadMojoMessageCallbackForTesting(
      BadMojoMessageCallbackForTesting callback);

  // Sets this RenderProcessHost to be guest only. For Testing only.
  void SetForGuestsOnlyForTesting();

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_MAC)
  // Launch the zygote early in the browser startup.
  static void EarlyZygoteLaunch();
#endif  // BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_MAC)

  // Called when a video capture stream or an audio stream is added or removed
  // and used to determine if the process should be backgrounded or not.
  void OnMediaStreamAdded() override;
  void OnMediaStreamRemoved() override;
  int get_media_stream_count_for_testing() const { return media_stream_count_; }

  void OnForegroundServiceWorkerAdded() override;
  void OnForegroundServiceWorkerRemoved() override;

  void OnBoostForLoadingAdded() override;
  void OnBoostForLoadingRemoved() override;

  // Sets the global factory used to create new RenderProcessHosts in unit
  // tests.  It may be nullptr, in which case the default RenderProcessHost will
  // be created (this is the behavior if you don't call this function).  The
  // factory must be set back to nullptr before it's destroyed; ownership is not
  // transferred.
  static void set_render_process_host_factory_for_testing(
      RenderProcessHostFactory* rph_factory);
  // Gets the global factory used to create new RenderProcessHosts in unit
  // tests.
  static RenderProcessHostFactory*
  get_render_process_host_factory_for_testing();

  // Tracks which sites' frames are hosted in which RenderProcessHosts.
  // TODO(ericrobinson): These don't need to be static.
  static void AddFrameWithSite(BrowserContext* browser_context,
                               RenderProcessHost* render_process_host,
                               const SiteInfo& site_info);
  static void RemoveFrameWithSite(BrowserContext* browser_context,
                                  RenderProcessHost* render_process_host,
                                  const SiteInfo& site_info);

  // Tracks which sites navigations are expected to commit in which
  // RenderProcessHosts.
  static void AddExpectedNavigationToSite(
      BrowserContext* browser_context,
      RenderProcessHost* render_process_host,
      const SiteInfo& site_info);
  static void RemoveExpectedNavigationToSite(
      BrowserContext* browser_context,
      RenderProcessHost* render_process_host,
      const SiteInfo& site_info);

  // Returns true if a spare RenderProcessHost should be kept at all times.
  static bool IsSpareProcessKeptAtAllTimes();

  // Iterate over all renderers and clear their in-memory resource cache.
  static void ClearAllResourceCaches();

  PermissionServiceContext& permission_service_context() {
    return *permission_service_context_;
  }

  bool is_initialized() const { return is_initialized_; }

  // Ensures that this process is kept alive for the specified timeouts. This
  // delays by |unload_handler_timeout| to ensure that unload handlers have a
  // chance to execute before the process shuts down, and by
  // |subframe_shutdown_timeout| to experimentally delay subframe process
  // shutdown for potential reuse (see https://crbug.com/894253). The total
  // shutdown delay is the sum of the two timeouts. |site_info| should
  // correspond to the frame that triggered this shutdown delay.
  void DelayProcessShutdown(const base::TimeDelta& subframe_shutdown_timeout,
                            const base::TimeDelta& unload_handler_timeout,
                            const SiteInfo& site_info) override;
  bool IsProcessShutdownDelayedForTesting();
  // Remove the host from the delayed-shutdown tracker, if present. This does
  // not decrement |shutdown_delay_ref_count_|; if it was incremented by a
  // shutdown delay, it will be decremented when the delay expires. This ensures
  // that the host is not destroyed between cancelling its shutdown delay and
  // the new navigation adding listeners to keep it alive.
  void StopTrackingProcessForShutdownDelay() override;

  // Binds `receiver` to the FileSystemManager instance owned by the render
  // process host, and is used by workers via BrowserInterfaceBroker.
  void BindFileSystemManager(
      const blink::StorageKey& storage_key,
      mojo::PendingReceiver<blink::mojom::FileSystemManager> receiver) override;

  // Binds `receiver` to the FileSystemAccessManager instance owned by the
  // render process host, and is used by workers via BrowserInterfaceBroker.
  void BindFileSystemAccessManager(
      const blink::StorageKey& storage_key,
      mojo::PendingReceiver<blink::mojom::FileSystemAccessManager> receiver)
      override;

  void BindFileBackedBlobFactory(
      const url::Origin& origin,
      mojo::PendingReceiver<blink::mojom::FileBackedBlobFactory> receiver);

  // Returns an OPFS (origin private file system) associated with
  // `bucket_locator`.
  void GetSandboxedFileSystemForBucket(
      const storage::BucketLocator& bucket_locator,
      const std::vector<std::string>& directory_path_components,
      blink::mojom::FileSystemAccessManager::GetSandboxedFileSystemCallback
          callback) override;

  FileSystemManagerImpl* GetFileSystemManagerForTesting() {
    return file_system_manager_impl_.get();
  }

  // Binds |receiver| to the RestrictedCookieManager instance owned by
  // |storage_partition_impl_|, and is used by a service worker via
  // BrowserInterfaceBroker. |receiver| belongs to the service worker that use
  // |storage_key| hosted by this process,
  void BindRestrictedCookieManagerForServiceWorker(
      const blink::StorageKey& storage_key,
      mojo::PendingReceiver<network::mojom::RestrictedCookieManager> receiver)
      override;

  // Binds |receiver| to the VideoDecodePerfHistory instance owned by the render
  // process host, and is used by workers via BrowserInterfaceBroker.
  void BindVideoDecodePerfHistory(
      mojo::PendingReceiver<media::mojom::VideoDecodePerfHistory> receiver)
      override;

  // Binds |receiver| to the WebrtcVideoPerfHistory instance owned by the render
  // process host, and is used by workers via BrowserInterfaceBroker.
  void BindWebrtcVideoPerfHistory(
      mojo::PendingReceiver<media::mojom::WebrtcVideoPerfHistory> receiver);

  // Binds `receiever` to the `PushMessagingManager` instance owned by the
  // render process host, and is used by workers via `BrowserInterfaceBroker`.
  void BindPushMessaging(
      mojo::PendingReceiver<blink::mojom::PushMessaging> receiver);

#if BUILDFLAG(IS_FUCHSIA)
  // Binds |receiver| to the FuchsiaMediaCodecProvider instance owned by the
  // render process host, and is used by workers via BrowserInterfaceBroker.
  void BindMediaCodecProvider(
      mojo::PendingReceiver<media::mojom::FuchsiaMediaCodecProvider> receiver)
      override;
#endif

  // Binds |receiver| to a OneShotBackgroundSyncService instance owned by the
  // StoragePartition associated with the render process host, and is used by
  // frames and service workers via BrowserInterfaceBroker.
  void CreateOneShotSyncService(
      const url::Origin& origin,
      mojo::PendingReceiver<blink::mojom::OneShotBackgroundSyncService>
          receiver) override;

  // Binds |receiver| to a PeriodicBackgroundSyncService instance owned by the
  // StoragePartition associated with the render process host, and is used by
  // frames and service workers via BrowserInterfaceBroker.
  void CreatePeriodicSyncService(
      const url::Origin& origin,
      mojo::PendingReceiver<blink::mojom::PeriodicBackgroundSyncService>
          receiver) override;

  // Binds |receiver| to a QuotaManagerHost instance indirectly owned by the
  // StoragePartition associated with the render process host. Used by workers
  // via BrowserInterfaceBroker. Frames should call out to QuotaContext directly
  // to pass in the correct frame id as well.
  void BindQuotaManagerHost(
      const blink::StorageKey& storage_key,
      mojo::PendingReceiver<blink::mojom::QuotaManagerHost> receiver) override;

  // Binds |receiver| to the LockManager owned by |storage_partition_impl_|.
  // |receiver| belongs to a frame or worker with |storage_key| hosted by this
  // process.
  //
  // Used by frames and workers via BrowserInterfaceBroker.
  void CreateLockManager(
      const blink::StorageKey& storage_key,
      mojo::PendingReceiver<blink::mojom::LockManager> receiver) override;

  // Binds |receiver| to the PermissionService instance owned by
  // |permission_service_context_|, and is used by workers via
  // BrowserInterfaceBroker.
  void CreatePermissionService(
      const url::Origin& origin,
      mojo::PendingReceiver<blink::mojom::PermissionService> receiver) override;

  // Binds |receiver| to the PaymentManager instance owned by
  // |storage_partition_impl_|, and is used by workers via
  // BrowserInterfaceBroker.
  void CreatePaymentManagerForOrigin(
      const url::Origin& origin,
      mojo::PendingReceiver<payments::mojom::PaymentManager> receiver) override;

  // Binds `creator_type`, `origin`, `receiver` and the information obtained
  // from the (possibly empty) `rfh_id` to the NotificationService instance
  // owned by `storage_partition_impl_`, and is used by documents and workers
  // via BrowserInterfaceBroker.
  void CreateNotificationService(
      GlobalRenderFrameHostId rfh_id,
      RenderProcessHost::NotificationServiceCreatorType creator_type,
      const blink::StorageKey& storage_key,
      mojo::PendingReceiver<blink::mojom::NotificationService> receiver)
      override;

  // Used for shared workers and service workers to create a websocket.
  // In other cases, RenderFrameHostImpl for documents or DedicatedWorkerHost
  // for dedicated workers handles interface requests in order to associate
  // websockets with a frame. Shared workers and service workers don't have to
  // do it because they don't have a frame.
  void CreateWebSocketConnector(
      const blink::StorageKey& storage_key,
      mojo::PendingReceiver<blink::mojom::WebSocketConnector> receiver)
      override;

#if BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
  void CreateStableVideoDecoder(
      mojo::PendingReceiver<media::stable::mojom::StableVideoDecoder> receiver)
      override;
#endif  // BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)

  void BindP2PSocketManager(
      net::NetworkAnonymizationKey isolation_key,
      mojo::PendingReceiver<network::mojom::P2PSocketManager> receiver,
      GlobalRenderFrameHostId render_frame_host_id);

  using IpcSendWatcher = base::RepeatingCallback<void(const IPC::Message& msg)>;
  void SetIpcSendWatcherForTesting(IpcSendWatcher watcher) {
    ipc_send_watcher_for_testing_ = std::move(watcher);
  }

#if BUILDFLAG(ENABLE_PPAPI)
  PepperRendererConnection* pepper_renderer_connection() {
    return pepper_renderer_connection_.get();
  }
#endif

#if BUILDFLAG(IS_ANDROID)
  // Notifies the renderer process of memory pressure level.
  void NotifyMemoryPressureToRenderer(
      base::MemoryPressureListener::MemoryPressureLevel level);
#endif

#if BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
  using StableVideoDecoderFactoryCreationCB = base::RepeatingCallback<void(
      mojo::PendingReceiver<media::stable::mojom::StableVideoDecoderFactory>)>;
  static void SetStableVideoDecoderFactoryCreationCBForTesting(
      StableVideoDecoderFactoryCreationCB cb);

  enum class StableVideoDecoderEvent {
    kFactoryResetTimerStopped,
    kAllDecodersDisconnected,
  };
  using StableVideoDecoderEventCB =
      base::RepeatingCallback<void(StableVideoDecoderEvent)>;
  static void SetStableVideoDecoderEventCBForTesting(
      StableVideoDecoderEventCB cb);
#endif  // BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)

  void GetBoundInterfacesForTesting(std::vector<std::string>& out);

  void SetPrivateMemoryFootprintForTesting(
      uint64_t private_memory_footprint_bytes);

  mojo::AssociatedReceiver<mojom::RendererHost>&
  renderer_host_receiver_for_testing() {
    return renderer_host_receiver_;
  }

 protected:
  // A proxy for our IPC::Channel that lives on the IO thread.
  std::unique_ptr<IPC::ChannelProxy> channel_;

  // True if fast shutdown has been performed on this RenderProcessHost.
  bool fast_shutdown_started_ = false;

  // True if shutdown from started by the |Shutdown()| method.
  bool shutdown_requested_ = false;

  // True if we've posted a DeleteTask and will be deleted soon.
  bool deleting_soon_ = false;

#ifndef NDEBUG
  // True if this object has deleted itself.
  bool is_self_deleted_ = false;
#endif

  // The count of currently swapped out but pending `blink::WebView`s.  We have
  // started to swap these in, so the renderer process should not exit if
  // this count is non-zero.
  int32_t pending_views_ = 0;

 private:
  friend class ChildProcessLauncherBrowserTest_ChildSpawnFail_Test;
  friend class RenderFrameHostImplSubframeReuseBrowserTest_MultipleDelays_Test;
  friend class VisitRelayingRenderProcessHost;
  friend class StoragePartitonInterceptor;
  friend class RenderProcessHostTestBase;
  // TODO(crbug.com/40142495): This class is a friend so that it can call our
  // private mojo implementation methods, acting as a pass-through. This is only
  // necessary during the associated interface migration, after which,
  // AgentSchedulingGroupHost will not act as a pass-through to the private
  // methods here. At that point we'll remove this friend class.
  friend class AgentSchedulingGroupHost;

  // A set of flags for this RenderProcessHost.
  enum RenderProcessFlags : int {
    kNone = 0,

    // Indicates whether this RenderProcessHost is exclusively hosting guest
    // RenderFrames.
    kForGuestsOnly = 1 << 0,

    // Indicates whether JavaScript JIT will be disabled for the renderer
    // process hosted by this RenderProcessHost.
    kJitDisabled = 1 << 1,

    // Indicates whether this RenderProcessHost is exclusively hosting PDF
    // contents.
    kPdf = 1 << 2,

#if BUILDFLAG(IS_WIN)
    // Indicates whether this RenderProcessHost should use SkiaFontManager as
    // the default font manager.
    kSkiaFontManager = 1 << 3,
#endif

    // Indicates whether v8 optimizations are disabled in this renderer process.
    kV8OptimizationsDisabled = 1 << 4,
  };

  // A RenderProcessHostImpl's IO thread implementation of the
  // |mojom::ChildProcessHost| interface. This exists to allow the process host
  // to bind incoming receivers on the IO-thread without a main-thread hop if
  // necessary. Also owns the RPHI's |mojom::ChildProcess| remote.
  class IOThreadHostImpl : public mojom::ChildProcessHost {
   public:
    IOThreadHostImpl(
        int render_process_id,
        base::WeakPtr<RenderProcessHostImpl> weak_host,
        std::unique_ptr<service_manager::BinderRegistry> binders,
        mojo::PendingReceiver<mojom::ChildProcessHost> host_receiver);
    ~IOThreadHostImpl() override;

    IOThreadHostImpl(const IOThreadHostImpl& other) = delete;
    IOThreadHostImpl& operator=(const IOThreadHostImpl& other) = delete;

    void SetPid(base::ProcessId child_pid);

    void GetInterfacesForTesting(std::vector<std::string>& out);

   private:
    // mojom::ChildProcessHost implementation:
    void Ping(PingCallback callback) override;

    // To enforce security review for IPC, these 2 methods are defined in
    // render_process_host_impl_receiver_bindings.cc.
    void BindHostReceiver(mojo::GenericPendingReceiver receiver) override;
    static void BindHostReceiverOnUIThread(
        base::WeakPtr<RenderProcessHostImpl> weak_host,
        mojo::GenericPendingReceiver receiver);

    const int render_process_id_;
    const base::WeakPtr<RenderProcessHostImpl> weak_host_;
    std::unique_ptr<service_manager::BinderRegistry> binders_;
    mojo::Receiver<mojom::ChildProcessHost> receiver_{this};

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    mojo::Remote<media::mojom::VideoEncodeAcceleratorProviderFactory>
        video_encode_accelerator_factory_remote_;
    ChildThreadTypeSwitcher child_thread_type_switcher_;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  };

  // Use CreateRenderProcessHost() instead of calling this constructor
  // directly.
  RenderProcessHostImpl(BrowserContext* browser_context,
                        StoragePartitionImpl* storage_partition_impl,
                        int flags);

  // Initializes a new IPC::ChannelProxy in |channel_|, which will be
  // connected to the next child process launched for this host, if any.
  void InitializeChannelProxy();

  // Initializes shared memory regions between this host and its renderer.
  // Called at the end of each call to InitializeChannelProxy() so the shared
  // memory regions can be sent to the (new) renderer.
  void InitializeSharedMemoryRegionsOnceChannelIsUp();

  // Resets |channel_|, removing it from the attachment broker if necessary.
  // Always call this in lieu of directly resetting |channel_|.
  void ResetChannelProxy();

  // Creates and adds the IO thread message filters.
  void CreateMessageFilters();

  // Registers Mojo interfaces to be exposed to the renderer.
  // To enforce security review for IPC, this method is defined in
  // render_process_host_impl_receiver_bindings.cc.
  void RegisterMojoInterfaces();

  // mojom::RendererHost
  using BrowserHistogramCallback =
      mojom::RendererHost::GetBrowserHistogramCallback;
  void GetBrowserHistogram(const std::string& name,
                           BrowserHistogramCallback callback) override;
  void SuddenTerminationChanged(bool enabled) override;
  void RecordUserMetricsAction(const std::string& action) override;
#if BUILDFLAG(IS_ANDROID)
  void SetPrivateMemoryFootprint(
      uint64_t private_memory_footprint_bytes) override;
#endif
  void HasGpuProcess(HasGpuProcessCallback callback) override;

  void CreateEmbeddedFrameSinkProvider(
      mojo::PendingReceiver<blink::mojom::EmbeddedFrameSinkProvider> receiver);
  void BindCompositingModeReporter(
      mojo::PendingReceiver<viz::mojom::CompositingModeReporter> receiver);
  void CreateDomStorageProvider(
      mojo::PendingReceiver<blink::mojom::DomStorageProvider> receiver);
  void CreateRendererHost(
      mojo::PendingAssociatedReceiver<mojom::RendererHost> receiver);
  void BindMediaInterfaceProxy(
      mojo::PendingReceiver<media::mojom::InterfaceFactory> receiver);
  void BindVideoEncoderMetricsProvider(
      mojo::PendingReceiver<media::mojom::VideoEncoderMetricsProvider>
          receiver);
#if BUILDFLAG(IS_ANDROID)
  void BindWebDatabaseHostImpl(
      mojo::PendingReceiver<blink::mojom::WebDatabaseHost> receiver);
#endif  // BUILDFLAG(IS_ANDROID)
  void BindAecDumpManager(
      mojo::PendingReceiver<blink::mojom::AecDumpManager> receiver);
  void CreateMediaLogRecordHost(
      mojo::PendingReceiver<content::mojom::MediaInternalLogRecords> receiver);
#if BUILDFLAG(ENABLE_PLUGINS)
  void BindPluginRegistry(
      mojo::PendingReceiver<blink::mojom::PluginRegistry> receiver);
#endif

  // blink::mojom::DomStorageProvider:
  void BindDomStorage(
      mojo::PendingReceiver<blink::mojom::DomStorage> receiver,
      mojo::PendingRemote<blink::mojom::DomStorageClient> client) override;

  // memory_instrumentation::mojom::CoordinatorConnector implementation:
  void RegisterCoordinatorClient(
      mojo::PendingReceiver<memory_instrumentation::mojom::Coordinator>
          receiver,
      mojo::PendingRemote<memory_instrumentation::mojom::ClientProcess>
          client_process) override;

  // Generates a command line to be used to spawn a renderer and appends the
  // results to |*command_line|.
  void AppendRendererCommandLine(base::CommandLine* command_line);

  // Copies applicable command line switches from the given |browser_cmd| line
  // flags to the output |renderer_cmd| line flags. Not all switches will be
  // copied over.
  void PropagateBrowserCommandLineToRenderer(
      const base::CommandLine& browser_cmd,
      base::CommandLine* renderer_cmd);

  // Recompute |visible_clients_| and |effective_importance_| from
  // |priority_clients_|.
  void UpdateProcessPriorityInputs();

  // Inspects the current object state and sets/removes background priority if
  // appropriate. Should be called after any of the involved data members
  // change.
  void UpdateProcessPriority();

  // When the |kChangeServiceWorkerPriorityForClientForegroundStateChange| is
  // enabled, if this render process's foreground state has changed, notify its
  // controller service worker to update its process priority if needed.
  void UpdateControllerServiceWorkerProcessPriority();

  // Called if the backgrounded or visibility state of the process changes.
  void SendProcessStateToRenderer();

  // Creates an UnsafeSharedMemoryRegion and PersistentMemoryAllocator for
  // the renderer process to store histograms. The allocator is available for
  // extraction by a SubprocesMetricsProvider in order to report those
  // histograms to UMA. This must be called before launching the renderer
  // process.
  void CreateMetricsAllocator();

  // Shares the histogram UnsafeSharedMemoryRegion, post launch, with the child
  // renderer process via IPC. This also serves to and notify the child to send
  // any early histograms it may have recorded before the shared memory region
  // became available to it. This must be called just after launching the
  // renderer process.
  //
  // If passing the memory region on launch is enabled, a duplicate handle to
  // the memory region may have already been passed to the renderer process
  // during launch. If so, the passing of the shmem handle is a NOP. There may
  // still be early histograms recorded before the child reads its launch
  // parameters to learn of the shared memory region.
  //
  // TODO(crbug.com/40109064): It may be possible to completely remove this once
  // passing the memory region on launch is rolled-out, if the shmem parameter
  // is consumed before the child records any histograms.
  void ShareMetricsMemoryRegion();

  // Retrieves the details of the terminating child process.
  //
  // If the process is no longer running, this will also reset the process
  // handle and (where applicable) reap the zombie process.
  //
  // |already_dead| should be set to true if we already know the process is
  // dead. See `ChildProcessLauncher::GetChildTerminationInfo()` for more info
  // on this flag.
  ChildProcessTerminationInfo GetChildTerminationInfo(bool already_dead);

  // Handle termination of our process.
  void ProcessDied(const ChildProcessTerminationInfo& termination_info);

  // Shutdowns the child process as fast as possible. This is similar to the
  // public `FastShutdownIfPossible()` method, but doesn't perform any checks
  // before initiating fast shutdown.
  void FastShutdown();

  // Destroy all objects that can cause methods to be invoked on this object or
  // any other that hang off it.
  void ResetIPC();

  // Returns whether this RenderProcessHost contains at least one
  // RenderFrameHost, but all of its RenderFrameHosts are non-live. In this case
  // the RenderProcessHost is needed but the renderer process is not.
  bool HasOnlyNonLiveRenderFrameHosts();

  // Helper method for CreateLockManager() which facilitates use of |bucket|
  // instead of |origin| for binding |receiver|
  void CreateLockManagerWithBucketInfo(
      mojo::PendingReceiver<blink::mojom::LockManager> receiver,
      storage::QuotaErrorOr<storage::BucketInfo> bucket);

  // Get an existing RenderProcessHost associated with the given browser
  // context, if possible.  The renderer process is chosen randomly from
  // suitable renderers that share the same context and type (determined by the
  // site url of |site_instance|).
  // Returns nullptr if no suitable renderer process is available, in which case
  // the caller is free to create a new renderer.
  static RenderProcessHost* GetExistingProcessHost(
      SiteInstanceImpl* site_instance);
  FRIEND_TEST_ALL_PREFIXES(RenderProcessHostUnitTest,
                           GuestsAreNotSuitableHosts);

  // Returns a RenderProcessHost that is rendering a URL corresponding to
  // |site_instance| in one of its frames, or that is expecting a navigation to
  // that SiteInstance. `process_reuse_policy` indicates the context so that
  // appropriate thresholds can be applied.
  static RenderProcessHost* FindReusableProcessHostForSiteInstance(
      SiteInstanceImpl* site_instance,
      ProcessReusePolicy process_reuse_policy);

  void NotifyRendererOfLockedStateUpdate();

#if BUILDFLAG(IS_ANDROID)
  // Populates the ChildProcessTerminationInfo fields that are strictly related
  // to renderer (This struct is also used for other child processes).
  void PopulateTerminationInfoRendererFields(ChildProcessTerminationInfo* info);
#endif  // BUILDFLAG(IS_ANDROID)

  static void OnMojoError(int render_process_id, const std::string& error);

  template <typename InterfaceType>
  using AddReceiverCallback =
      base::RepeatingCallback<void(mojo::PendingReceiver<InterfaceType>)>;

  template <typename CallbackType>
  struct InterfaceGetter;

  template <typename InterfaceType>
  struct InterfaceGetter<AddReceiverCallback<InterfaceType>> {
    static void GetInterfaceOnUIThread(
        base::WeakPtr<RenderProcessHostImpl> weak_host,
        AddReceiverCallback<InterfaceType> callback,
        mojo::PendingReceiver<InterfaceType> receiver) {
      if (!weak_host)
        return;
      std::move(callback).Run(std::move(receiver));
    }
  };

  // Helper to bind an interface callback whose lifetime is limited to that of
  // the render process currently hosted by the RenderProcessHost. Callbacks
  // added by this method will never run beyond the next invocation of
  // Cleanup().
  template <typename CallbackType>
  void AddUIThreadInterface(service_manager::BinderRegistry* registry,
                            CallbackType callback) {
    registry->AddInterface(
        base::BindRepeating(
            &InterfaceGetter<CallbackType>::GetInterfaceOnUIThread,
            instance_weak_factory_.GetWeakPtr(), std::move(callback)),
        GetUIThreadTaskRunner({}));
  }

  // Callback to unblock process shutdown after waiting for the delay timeout to
  // complete.
  void CancelProcessShutdownDelay(const SiteInfo& site_info);

  // Binds a TracedProcess interface in the renderer process. This is used to
  // communicate with the Tracing service.
  void BindTracedProcess(
      mojo::PendingReceiver<tracing::mojom::TracedProcess> receiver);

  // Handles incoming requests to bind a process-scoped receiver from the
  // renderer process. This is posted to the main thread by IOThreadHostImpl
  // if the request isn't handled on the IO thread.
  void OnBindHostReceiver(mojo::GenericPendingReceiver receiver);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // Provides /proc/{renderer pid}/status and statm files for the renderer,
  // because the files are required to calculate the renderer's private
  // footprint on Chromium Linux. Regarding MacOS X and Windows, we have
  // the different way to calculate renderer's private memory footprint.
  // So this method is implemented only when OS_LINUX or OS_CHROMEOS is defined.
  void ProvideStatusFileForRenderer();
#endif

  // Gives a DELETE_ON_CLOSE file descriptor to the renderer, to use for
  // swapping. See blink::DiskDataAllocator for uses.
  void ProvideSwapFileForRenderer();

  // True when |keep_alive_ref_count_|, |worker_ref_count_|,
  // |shutdown_delay_ref_count_|, and |pending_reuse_ref_count_| are all zero.
  bool AreAllRefCountsZero();

#if BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
  void OnStableVideoDecoderDisconnected();

  void ResetStableVideoDecoderFactory();
#endif  // BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)

  mojo::OutgoingInvitation mojo_invitation_;

  // These cover mutually-exclusive cases. While keep-alive is time-based,
  // workers are not. Shutdown-delay is also time-based, but uses a different
  // delay time. |pending_reuse_ref_count_| is not time-based and is used when
  // the process needs to be kept alive because it will be reused soon.
  // Attached documents are tracked via |listeners_| below.
  int keep_alive_ref_count_ = 0;
  int worker_ref_count_ = 0;
  int shutdown_delay_ref_count_ = 0;
  int pending_reuse_ref_count_ = 0;
  // We track the start-time for each |handle_id|, for crashkey reporting.
  base::flat_map<uint64_t, base::Time> keep_alive_start_times_;

  // Count of NavigationStateKeepAlives that depend on state tied to this
  // RenderProcessHost. This is related to SiteInstanceGroup::keep_alive_count_,
  // but it aggregates the keep alive count across all SiteInstanceGroups in
  // this process. This allows individual SiteInstanceGroups to go away even
  // when there are NavigationStateKeepAlives in other SiteInstanceGroups in the
  // same process. This also lets RenderProcessHosts go away even if there are
  // NavigationStateKeepAlives in other processes in the same StoragePartition.
  int navigation_state_keepalive_count_ = 0;

  // Set in DisableRefCounts(). When true, |keep_alive_ref_count_| and
  // |worker_ref_count_|, |shutdown_delay_ref_count_|, and
  // |pending_reuse_ref_count_| must no longer be modified.
  bool are_ref_counts_disabled_ = false;

  // The registered IPC listener objects. When this list is empty, we should
  // delete ourselves.
  base::IDMap<IPC::Listener*> listeners_;

  // Mojo interfaces provided to the child process are registered here if they
  // need consistent delivery ordering with legacy IPC, and are process-wide in
  // nature (e.g. metrics, memory usage).
  std::unique_ptr<blink::AssociatedInterfaceRegistry> associated_interfaces_;

  // These fields are cached values that are updated in
  // UpdateProcessPriorityInputs, and are used to compute priority sent to
  // ChildProcessLauncher.
  // |visible_clients_| is the count of currently visible clients.
  int32_t visible_clients_ = 0;
  // |frame_depth_| can be used to rank processes of the same visibility, ie it
  // is the lowest depth of all visible clients, or if there are no visible
  // widgets the lowest depth of all hidden clients. Initialized to max depth
  // when there are no clients.
  unsigned int frame_depth_ = kMaxFrameDepthForPriority;
  // |intersects_viewport_| similar to |frame_depth_| can be used to rank
  // processes of same visibility. It indicates process has frames that
  // intersect with the viewport.
  bool intersects_viewport_ = false;
#if BUILDFLAG(IS_ANDROID)
  // Highest importance of all clients that contribute priority.
  ChildProcessImportance effective_importance_ = ChildProcessImportance::NORMAL;
#endif

  // Clients that contribute priority to this process.
  base::flat_set<raw_ptr<RenderProcessHostPriorityClient, CtnExperimental>>
      priority_clients_;

  RenderProcessPriority priority_;

#if !BUILDFLAG(IS_ANDROID)
  // If this is set then the built-in process priority calculation system is
  // ignored, and an externally computed process priority is used.
  // TODO(pmonette): After experimentation, either remove this or rip out the
  // existing logic entirely.
  std::optional<base::Process::Priority> priority_override_;
#endif

  // Used to allow a RenderWidgetHost to intercept various messages on the
  // IO thread.
  scoped_refptr<RenderWidgetHelper> widget_helper_;

  // Used in single-process mode.
  std::unique_ptr<base::Thread> in_process_renderer_;

  // True after Init() has been called.
  bool is_initialized_ = false;

  // True after ProcessDied(), until the next call to Init().
  bool is_dead_ = false;

  // Stores the time at which the last successful call to Init happened.
  base::TimeTicks last_init_time_;

  // Used to launch and terminate the process without blocking the UI thread.
  std::unique_ptr<ChildProcessLauncher> child_process_launcher_;

  // The globally-unique identifier for this RenderProcessHost.
  const int id_;

  // This field is not a raw_ptr<> because problems related to passing to a
  // templated && parameter, which is later forwarded to something that doesn't
  // vibe with raw_ptr<T>.
  RAW_PTR_EXCLUSION BrowserContext* browser_context_ = nullptr;

  // Owned by |browser_context_|.
  //
  // TODO(crbug.com/40061679): Change back to `raw_ptr` after the ad-hoc
  // debugging is no longer needed to investigate the bug.
  base::WeakPtr<StoragePartitionImpl> storage_partition_impl_;

  // Owns the singular DomStorageProvider binding established by this renderer.
  mojo::Receiver<blink::mojom::DomStorageProvider>
      dom_storage_provider_receiver_{this};

  // Keeps track of the ReceiverIds returned by
  // storage_partition_impl_->BindDomStorage() calls so we can Unbind() them on
  // cleanup.
  std::set<mojo::ReceiverId> dom_storage_receiver_ids_;

  std::set<GlobalRenderFrameHostId> render_frame_host_id_set_;

  // The observers watching our lifetime.
  base::ObserverList<RenderProcessHostObserver> observers_;

  // The observers watching content-internal events.
  base::ObserverList<RenderProcessHostInternalObserver> internal_observers_;

  // True if the process can be shut down suddenly.  If this is true, then we're
  // sure that all the `blink::WebView`s in the process can be shutdown
  // suddenly.  If it's false, then specific `blink::WebView`s might still be
  // allowed to be shutdown suddenly by checking their
  // SuddenTerminationAllowed() flag.  This can occur if one WebContents has an
  // unload event listener but another WebContents in the same process doesn't.
  bool sudden_termination_allowed_ = true;

  // Set to true if this process is blocked and shouldn't be sent input events.
  // The checking of this actually happens in the RenderWidgetHost.
  bool is_blocked_ = false;

  // The clients who want to know when the blocked state has changed.
  BlockStateChangedCallbackList blocked_state_changed_callback_list_;

  // Records the last time we regarded the child process active.
  base::TimeTicks child_process_activity_time_;

  std::string unresponsive_document_javascript_call_stack_;
  blink::LocalFrameToken unresponsive_document_token_;

  // A set of flags that influence RenderProcessHost behavior.
  int flags_;

  // Indicates whether this RenderProcessHost is unused, meaning that it has
  // not committed any web content, and it has not been given to a SiteInstance
  // that has a site assigned.
  bool is_unused_ = true;

  // Set if a call to Cleanup is required once the RenderProcessHostImpl is no
  // longer within the RenderProcessHostObserver::RenderProcessExited callbacks.
  bool delayed_cleanup_needed_ = false;

  // Indicates whether RenderProcessHostImpl::ProcessDied is currently iterating
  // and calling through RenderProcessHostObserver::RenderProcessExited.
  bool within_process_died_observer_ = false;

  std::unique_ptr<P2PSocketDispatcherHost> p2p_socket_dispatcher_host_;

  // Must be accessed on UI thread.
  AecDumpManagerImpl aec_dump_manager_;

  std::unique_ptr<MediaStreamTrackMetricsHost, BrowserThread::DeleteOnIOThread>
      media_stream_track_metrics_host_;

  std::unique_ptr<FramelessMediaInterfaceProxy> media_interface_proxy_;

  // Context shared for each mojom::PermissionService instance created for this
  // RenderProcessHost. This is destroyed early in ResetIPC() method.
  std::unique_ptr<PermissionServiceContext> permission_service_context_;

#if BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
  // Connection to the StableVideoDecoderFactory that lives in a utility
  // process. This is only used for out-of-process video decoding.
  mojo::Remote<media::stable::mojom::StableVideoDecoderFactory>
      stable_video_decoder_factory_remote_;

  // Using |stable_video_decoder_trackers_|, we track the StableVideoDecoders
  // that have been created using |stable_video_decoder_factory_remote_|. That
  // way, we know when the remote StableVideoDecoder dies.
  mojo::ReceiverSet<media::stable::mojom::StableVideoDecoderTracker>
      stable_video_decoder_trackers_;

  // |stable_video_decoder_factory_reset_timer_| allows us to delay the reset()
  // of |stable_video_decoder_factory_remote_|: after all StableVideoDecoders
  // have disconnected, we wait for the timer to trigger, and if no request
  // comes in to create a StableVideoDecoder before that, we reset the
  // |stable_video_decoder_factory_remote_| which should cause the destruction
  // of the remote video decoder utility process.
  base::OneShotTimer stable_video_decoder_factory_reset_timer_;
#endif  // BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)

#if BUILDFLAG(IS_FUCHSIA)
  std::unique_ptr<FuchsiaMediaCodecProviderImpl> media_codec_provider_;
#endif

  // The memory allocator, if any, in which the renderer will write its metrics.
  std::unique_ptr<base::PersistentMemoryAllocator> metrics_allocator_;
  base::UnsafeSharedMemoryRegion metrics_memory_region_;

  bool channel_connected_ = false;
  bool sent_render_process_ready_ = false;
  bool sent_process_created_ = false;

  std::unique_ptr<FileSystemManagerImpl, BrowserThread::DeleteOnIOThread>
      file_system_manager_impl_;
  std::unique_ptr<viz::GpuClient> gpu_client_;
  std::unique_ptr<PushMessagingManager> push_messaging_manager_;

  std::unique_ptr<EmbeddedFrameSinkProviderImpl> embedded_frame_sink_provider_;
#if BUILDFLAG(ENABLE_PLUGINS)
  std::unique_ptr<PluginRegistryImpl> plugin_registry_;
#endif

  mojo::Remote<mojom::ChildProcess> child_process_;
  // This will be bound to |io_thread_host_impl_|.
  mojo::PendingReceiver<mojom::ChildProcessHost> child_host_pending_receiver_;
  mojo::AssociatedRemote<mojom::Renderer> renderer_interface_;
  mojo::Remote<blink::mojom::CallStackGenerator>
      javascript_call_stack_generator_interface_;
  mojo::AssociatedReceiver<mojom::RendererHost> renderer_host_receiver_{this};
  mojo::Receiver<memory_instrumentation::mojom::CoordinatorConnector>
      coordinator_connector_receiver_{this};

  // A shared memory mapping of a std::atomic<TimeTicks> used to atomically
  // communicate the last time the hosted renderer was foregrounded. This is
  // preferable to IPC as it ensures the timing is visible immediately after
  // recovering from a jank (e.g. important for metrics).
  // TODO(pmonette): Update this to support all process priority levels.
  std::optional<base::AtomicSharedMemory<base::TimeTicks>>
      last_foreground_time_region_;

  // Tracks active audio and video streams within the render process; used to
  // determine if if a process should be backgrounded.
  int media_stream_count_ = 0;

  // Tracks service workers that may need to respond to events from other
  // processes in a timely manner.  Used to determine if a process should
  // not be backgrounded.
  int foreground_service_worker_count_ = 0;

  // Tracks the count of render frame host that requested prioritize the
  // processing commit navigation and initial loading (crbug/351953350).
  int boost_for_loading_count_ = 0;

  std::unique_ptr<mojo::Receiver<viz::mojom::CompositingModeReporter>>
      compositing_mode_reporter_;

  // Stores the amount of time that this RenderProcessHost's shutdown has been
  // delayed to run unload handlers, or zero if the process shutdown was not
  // delayed due to unload handlers.
  base::TimeDelta time_spent_running_unload_handlers_;

  // If the RenderProcessHost is being shutdown via Shutdown(), this records the
  // exit code.
  int shutdown_exit_code_ = -1;

  IpcSendWatcher ipc_send_watcher_for_testing_;

  // Keeps this process registered with the tracing subsystem.
  std::unique_ptr<TracingServiceController::ClientRegistration>
      tracing_registration_;

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID)
  // For the render process to connect to the system tracing service.
  std::unique_ptr<tracing::SystemTracingService> system_tracing_service_;
#endif

#if BUILDFLAG(ENABLE_PPAPI)
  scoped_refptr<PepperRendererConnection> pepper_renderer_connection_;
#endif

  // The memory size that the renderer has allocated. On Android
  // this value is pushed from the renderer periodically. On other platforms
  // this value is a cached value calculated from the last call to
  // `GetPrivateMemoryFootprint`. Because of this caching this value should
  // not be used directly but `GetPrivateMemoryFootprint` should be called
  // each time.
  uint64_t private_memory_footprint_bytes_ = 0u;
#if !BUILDFLAG(IS_ANDROID)
  base::TimeTicks private_memory_footprint_valid_until_;
#endif

  // IOThreadHostImpl owns some IO-thread state associated with this
  // RenderProcessHostImpl. This is mainly to allow various IPCs from the
  // renderer to be handled on the IO thread without a hop to the UI thread.
  //
  // Declare this at then end to ensure it triggers the destruction of the
  // IOThreadHostImpl prior to other members with an IO thread deleter that are
  // bound to a mojo receiver callback using a base::Unretained.  This is
  // necessary to ensure those objects stop receiving mojo messages before their
  // destruction.
  std::optional<base::SequenceBound<IOThreadHostImpl>> io_thread_host_impl_;

  std::unique_ptr<FileBackedBlobFactoryWorkerImpl> file_backed_blob_factory_;

  // Number of current outermost frames in this process.
  size_t outermost_main_frame_count_ = 0;
  // Maximum number of outermost main frames this process hosted concurrently.
  size_t max_outermost_main_frames_ = 0;

  // A WeakPtrFactory which is reset every time ResetIPC() or Cleanup() is run.
  // Used to vend WeakPtrs which are invalidated any time the RenderProcessHost
  // is used for a new renderer process or prepares for deletion.
  // Most cases should use this factory, so the resulting WeakPtrs are no longer
  // valid after DeleteSoon is called, when the RenderProcessHost is in a partly
  // torn-down state.
  base::WeakPtrFactory<RenderProcessHostImpl> instance_weak_factory_{this};

  // A WeakPtrFactory which should only be used for creating SafeRefs. All other
  // weak pointers should use |instance_weak_factory_|. This WeakPtrFactory
  // doesn't get reset until this RenderProcessHost object is actually deleted.
  base::WeakPtrFactory<RenderProcessHostImpl> safe_ref_factory_{this};
};

}  // namespace content

namespace base {

template <>
struct ScopedObservationTraits<content::RenderProcessHostImpl,
                               content::RenderProcessHostInternalObserver> {
  static void AddObserver(
      content::RenderProcessHostImpl* source,
      content::RenderProcessHostInternalObserver* observer) {
    source->AddInternalObserver(observer);
  }
  static void RemoveObserver(
      content::RenderProcessHostImpl* source,
      content::RenderProcessHostInternalObserver* observer) {
    source->RemoveInternalObserver(observer);
  }
};

}  // namespace base

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_PROCESS_HOST_IMPL_H_
