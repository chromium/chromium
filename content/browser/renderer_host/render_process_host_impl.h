// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_PROCESS_HOST_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_PROCESS_HOST_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <queue>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/containers/flat_set.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/process/process.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/sequence_bound.h"
#include "build/build_config.h"
#include "content/browser/child_process_launcher.h"
#include "content/browser/dom_storage/session_storage_namespace_impl.h"
#include "content/browser/media/media_internals.h"
#include "content/browser/media/video_decoder_proxy.h"
#include "content/browser/renderer_host/embedded_frame_sink_provider_impl.h"
#include "content/browser/renderer_host/frame_sink_provider_impl.h"
#include "content/browser/renderer_host/media/aec_dump_manager_impl.h"
#include "content/browser/tracing/tracing_service_controller.h"
#include "content/common/associated_interfaces.mojom.h"
#include "content/common/child_process.mojom.h"
#include "content/common/content_export.h"
#include "content/common/renderer.mojom.h"
#include "content/common/renderer_host.mojom.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_platform_file.h"
#include "media/media_buildflags.h"
#include "media/mojo/mojom/video_decode_perf_history.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/invitation.h"
#include "net/base/network_isolation_key.h"
#include "net/net_buildflags.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/device/public/mojom/battery_monitor.mojom-forward.h"
#include "services/network/public/mojom/mdns_responder.mojom.h"
#include "services/network/public/mojom/p2p.mojom-forward.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/memory_instrumentation.mojom.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/tracing/public/mojom/traced_process.mojom.h"
#include "services/viz/public/mojom/compositing/compositing_mode_watcher.mojom.h"
#include "services/viz/public/mojom/gpu.mojom.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/mojom/agents/agent_metrics.mojom.h"
#include "third_party/blink/public/mojom/associated_interfaces/associated_interfaces.mojom.h"
#include "third_party/blink/public/mojom/background_sync/background_sync.mojom-forward.h"
#include "third_party/blink/public/mojom/broadcastchannel/broadcast_channel.mojom.h"
#include "third_party/blink/public/mojom/dom_storage/dom_storage.mojom.h"
#include "third_party/blink/public/mojom/filesystem/file_system.mojom.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-shared.h"
#include "third_party/blink/public/mojom/loader/code_cache.mojom.h"
#include "third_party/blink/public/mojom/media/renderer_audio_output_stream_factory.mojom.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "third_party/blink/public/mojom/native_io/native_io.mojom-forward.h"
#include "third_party/blink/public/mojom/peerconnection/peer_connection_tracker.mojom.h"
#include "third_party/blink/public/mojom/plugins/plugin_registry.mojom-forward.h"
#include "third_party/blink/public/mojom/push_messaging/push_messaging.mojom-forward.h"
#include "third_party/blink/public/mojom/webdatabase/web_database.mojom.h"
#include "ui/gfx/gpu_memory_buffer.h"

#if defined(OS_ANDROID)
#include "content/public/browser/android/child_process_importance.h"
#endif

namespace base {
class CommandLine;
class PersistentMemoryAllocator;
}  // namespace base

namespace url {
class Origin;
}

namespace viz {
class GpuClient;
}

namespace content {
class AgentMetricsCollectorHost;
class AgentSchedulingGroupHost;
class CodeCacheHostImpl;
class FileSystemManagerImpl;
class InProcessChildThreadParams;
class IsolationContext;
class MediaStreamTrackMetricsHost;
class P2PSocketDispatcherHost;
class PermissionServiceContext;
class PeerConnectionTrackerHost;
class PluginRegistryImpl;
class ProcessLock;
class PushMessagingManager;
class RenderFrameMessageFilter;
class RenderProcessHostCreationObserver;
class RenderProcessHostFactory;
class RenderProcessHostTest;
class RenderWidgetHelper;
class ResolveProxyHelper;
class SiteInfo;
class SiteInstance;
class SiteInstanceImpl;
class StoragePartition;
class StoragePartitionImpl;
struct ChildProcessTerminationInfo;

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
// keeps a list of RenderView (renderer) and WebContentsImpl (browser) which
// are correlated with IDs. This way, the Views and the corresponding ViewHosts
// communicate through the two process objects.
//
// A RenderProcessHost is also associated with one and only one
// StoragePartition.  This allows us to implement strong storage isolation
// because all the IPCs from the RenderViews (renderer) will only ever be able
// to access the partition they are assigned to.
class CONTENT_EXPORT RenderProcessHostImpl
    : public RenderProcessHost,
      public ChildProcessLauncher::Client,
      public mojom::RouteProvider,
      public blink::mojom::AssociatedInterfaceProvider,
      public mojom::RendererHost,
      public blink::mojom::DomStorageProvider,
      public memory_instrumentation::mojom::CoordinatorConnector {
 public:
  // Special depth used when there are no PriorityClients.
  static const unsigned int kMaxFrameDepthForPriority;

  // Create a new RenderProcessHost. The storage partition for the process
  // is retrieved from |browser_context| based on information in
  // |site_instance|. The default storage partition is selected if
  // |site_instance| is null.
  static RenderProcessHost* CreateRenderProcessHost(
      BrowserContext* browser_context,
      SiteInstanceImpl* site_instance);

  // Returns whether the process-per-site model is in use (globally or just for
  // the current site), in which case we should ensure there is only one
  // RenderProcessHost per site for the entire browser context.
  static bool ShouldUseProcessPerSite(BrowserContext* browser_context,
                                      const SiteInfo& site_info);

  ~RenderProcessHostImpl() override;

  // RenderProcessHost implementation (public portion).
  bool Init() override;
  void EnableSendQueue() override;
  int GetNextRoutingID() override;
  void AddRoute(int32_t routing_id, IPC::Listener* listener) override;
  void RemoveRoute(int32_t routing_id) override;
  void AddObserver(RenderProcessHostObserver* observer) override;
  void RemoveObserver(RenderProcessHostObserver* observer) override;
  void ShutdownForBadMessage(CrashReportMode crash_report_mode) override;
  void UpdateClientPriority(PriorityClient* client) override;
  int VisibleClientCount() override;
  unsigned int GetFrameDepth() override;
  bool GetIntersectsViewport() override;
  bool IsForGuestsOnly() override;
  StoragePartition* GetStoragePartition() override;
  bool Shutdown(int exit_code) override;
  bool ShutdownRequested() override;
  bool FastShutdownIfPossible(size_t page_count = 0,
                              bool skip_unload_handlers = false) override;
  const base::Process& GetProcess() override;
  bool IsReady() override;
  BrowserContext* GetBrowserContext() override;
  bool InSameStoragePartition(StoragePartition* partition) override;
  int GetID() override;
  bool IsInitializedAndNotDead() override;
  void SetBlocked(bool blocked) override;
  bool IsBlocked() override;
  std::unique_ptr<base::CallbackList<void(bool)>::Subscription>
  RegisterBlockStateChangedCallback(
      const BlockStateChangedCallback& cb) override;
  void Cleanup() override;
  void AddPendingView() override;
  void RemovePendingView() override;
  void AddPriorityClient(PriorityClient* priority_client) override;
  void RemovePriorityClient(PriorityClient* priority_client) override;
  void SetPriorityOverride(bool foreground) override;
  bool HasPriorityOverride() override;
  void ClearPriorityOverride() override;
#if defined(OS_ANDROID)
  ChildProcessImportance GetEffectiveImportance() override;
  void DumpProcessStack() override;
#endif
  void SetSuddenTerminationAllowed(bool enabled) override;
  bool SuddenTerminationAllowed() override;
  IPC::ChannelProxy* GetChannel() override;
  void AddFilter(BrowserMessageFilter* filter) override;
  bool FastShutdownStarted() override;
  base::TimeDelta GetChildProcessIdleTime() override;
  void FilterURL(bool empty_allowed, GURL* url) override;
  void EnableAudioDebugRecordings(const base::FilePath& file) override;
  void DisableAudioDebugRecordings() override;
  WebRtcStopRtpDumpCallback StartRtpDump(
      bool incoming,
      bool outgoing,
      WebRtcRtpPacketCallback packet_callback) override;
  void EnableWebRtcEventLogOutput(int lid, int output_period_ms) override;
  void DisableWebRtcEventLogOutput(int lid) override;
  void BindReceiver(mojo::GenericPendingReceiver receiver) override;
  std::unique_ptr<base::PersistentMemoryAllocator> TakeMetricsAllocator()
      override;
  const base::TimeTicks& GetInitTimeForNavigationMetrics() override;
  bool IsProcessBackgrounded() override;
  void IncrementKeepAliveRefCount() override;
  void DecrementKeepAliveRefCount() override;
  void DisableKeepAliveRefCount() override;
  bool IsKeepAliveRefCountDisabled() override;
  mojom::Renderer* GetRendererInterface() override;
  void CreateURLLoaderFactory(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
      network::mojom::URLLoaderFactoryParamsPtr params) override;

  bool MayReuseHost() override;
  bool IsUnused() override;
  void SetIsUsed() override;

  bool HostHasNotBeenUsed() override;
  void SetProcessLock(const IsolationContext& isolation_context,
                      const ProcessLock& process_lock) override;
  bool IsProcessLockedToSiteForTesting() override;
  void BindCacheStorage(
      const network::CrossOriginEmbedderPolicy& cross_origin_embedder_policy,
      mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
          coep_reporter,
      const url::Origin& origin,
      mojo::PendingReceiver<blink::mojom::CacheStorage> receiver) override;
  void BindIndexedDB(
      const url::Origin& origin,
      mojo::PendingReceiver<blink::mojom::IDBFactory> receiver) override;
  void ForceCrash() override;
  void CleanupNetworkServicePluginExceptionsUponDestruction() override;
  std::string GetInfoForBrowserContextDestructionCrashReporting() override;
#if BUILDFLAG(CLANG_PROFILING_INSIDE_SANDBOX)
  void DumpProfilingData(base::OnceClosure callback) override;
#endif

  mojom::RouteProvider* GetRemoteRouteProvider(
      util::PassKey<AgentSchedulingGroupHost>);

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

  // Update the total and low priority count as indicated by the previous and
  // new priorities of the underlying document.  The nullopt option is used when
  // there is no previous/subsequent navigation (when the frame is added/removed
  // from the RenderProcessHostImpl).
  void UpdateFrameWithPriority(
      base::Optional<FramePriority> previous_priority,
      base::Optional<FramePriority> new_priority) override;

  // Call this function when it is evident that the child process is actively
  // performing some operation, for example if we just received an IPC message.
  void mark_child_process_activity_time() {
    child_process_activity_time_ = base::TimeTicks::Now();
  }

  // Used to extend the lifetime of the sessions until the render view
  // in the renderer is fully closed. This is static because its also called
  // with mock hosts as input in test cases. The RenderWidget routing associated
  // with the view is used as the key since the WidgetMsg_Close and
  // WidgetHostMsg_Close_ACK logic is centered around RenderWidgets.
  static void ReleaseOnCloseACK(RenderProcessHost* host,
                                const SessionStorageNamespaceMap& sessions,
                                int widget_route_id);

  // Register/unregister the host identified by the host id in the global host
  // list.
  static void RegisterHost(int host_id, RenderProcessHost* host);
  static void UnregisterHost(int host_id);

  static void RegisterCreationObserver(
      RenderProcessHostCreationObserver* observer);
  static void UnregisterCreationObserver(
      RenderProcessHostCreationObserver* observer);

  // Implementation of FilterURL below that can be shared with the mock class.
  static void FilterURL(RenderProcessHost* rph, bool empty_allowed, GURL* url);

  // Returns true if |host| is suitable for rendering a page in the given
  // |isolation_context|, where the page would utilize |site_info.site_url()| as
  // its SiteInstance site URL, and its process would be locked to
  // |site_info.lock_url()|. Site and lock urls may differ in cases where
  // an effective URL is not the actual site that the process is locked to,
  // which happens for hosted apps. |is_guest| should be set to true if the call
  // is being made for a <webview> guest SiteInstance.
  // TODO(wjmaclean): move is_guest into SiteInfo at some point.
  static bool IsSuitableHost(RenderProcessHost* host,
                             const IsolationContext& isolation_context,
                             const SiteInfo& site_info,
                             bool is_guest);

  // Returns an existing RenderProcessHost for |site_info| in
  // |isolation_context|, if one exists.  Otherwise a new RenderProcessHost
  // should be created and registered using RegisterProcessHostForSite(). This
  // should only be used for process-per-site mode, which can be enabled
  // globally with a command line flag or per-site, as determined by
  // SiteInstanceImpl::ShouldUseProcessPerSite. |is_guest| should be set to
  // true if the call is being made for a <webview> guest SiteInstance.
  // TODO(wjmaclean): Move is_guest into SiteInfo at some point.
  static RenderProcessHost* GetSoleProcessHostForSite(
      const IsolationContext& isolation_context,
      const SiteInfo& site_info,
      const bool is_guest);

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

  // Should be called when |browser_context| is used in a navigation.
  //
  // The SpareRenderProcessHostManager can decide how to respond (for example,
  // by shutting down the spare process to conserve resources, or alternatively
  // by making sure that the spare process belongs to the same BrowserContext as
  // the most recent navigation).
  static void NotifySpareManagerAboutRecentlyUsedBrowserContext(
      BrowserContext* browser_context);

  // This enum backs a histogram, so do not change the order of entries or
  // remove entries and update enums.xml if adding new entries.
  enum class SpareProcessMaybeTakeAction {
    kNoSparePresent = 0,
    kMismatchedBrowserContext = 1,
    kMismatchedStoragePartition = 2,
    kRefusedByEmbedder = 3,
    kSpareTaken = 4,
    kRefusedBySiteInstance = 5,
    kMaxValue = kRefusedBySiteInstance
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused. Must agree with FramePrioritiesSeen
  // in enums.xml.
  enum class FramePrioritiesSeen {
    kNoFramesSeen = 0,
    kMixedPrioritiesSeen = 1,
    kOnlyLowPrioritiesSeen = 2,
    kOnlyNormalPrioritiesSeen = 3,
    kMaxValue = kOnlyNormalPrioritiesSeen
  };

  static scoped_refptr<base::SingleThreadTaskRunner>
  GetInProcessRendererThreadTaskRunnerForTesting();

#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
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

  using BadMojoMessageCallbackForTesting =
      base::RepeatingCallback<void(int render_process_host_id,
                                   const std::string& error)>;
  static void SetBadMojoMessageCallbackForTesting(
      BadMojoMessageCallbackForTesting callback);

  // Allows external code to supply a callback which handles a
  // mojo::PendingReceiver<blink::mojom::BroadcastChannelProvider>. Used for
  // supplying test versions of the service.
  using BroadcastChannelProviderReceiverHandler = base::RepeatingCallback<void(
      RenderProcessHostImpl* rph,
      mojo::PendingReceiver<blink::mojom::BroadcastChannelProvider> receiver)>;
  static void SetBroadcastChannelProviderReceiverHandlerForTesting(
      BroadcastChannelProviderReceiverHandler handler);

  // Allows external code to supply a callback that is invoked immediately
  // after the CodeCacheHostImpl is created and bound.  Used for swapping
  // the binding for a test version of the service.
  using CodeCacheHostReceiverHandler =
      base::RepeatingCallback<void(RenderProcessHost*, CodeCacheHostImpl*)>;
  static void SetCodeCacheHostReceiverHandlerForTesting(
      CodeCacheHostReceiverHandler handler);

  RenderFrameMessageFilter* render_frame_message_filter_for_testing() const {
    return render_frame_message_filter_.get();
  }

  void set_is_for_guests_only_for_testing(bool is_for_guests_only) {
    is_for_guests_only_ = is_for_guests_only;
  }

#if defined(OS_POSIX) && !defined(OS_ANDROID) && !defined(OS_MAC)
  // Launch the zygote early in the browser startup.
  static void EarlyZygoteLaunch();
#endif  // defined(OS_POSIX) && !defined(OS_ANDROID) && !defined(OS_MAC)

  // Called when a video capture stream or an audio stream is added or removed
  // and used to determine if the process should be backgrounded or not.
  void OnMediaStreamAdded() override;
  void OnMediaStreamRemoved() override;
  int get_media_stream_count_for_testing() const { return media_stream_count_; }

  void OnForegroundServiceWorkerAdded() override;
  void OnForegroundServiceWorkerRemoved() override;

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

  // Discards the spare RenderProcessHost.  After this call,
  // GetSpareRenderProcessHostForTesting will return nullptr.
  static void DiscardSpareRenderProcessHostForTesting();

  // Returns true if a spare RenderProcessHost should be kept at all times.
  static bool IsSpareProcessKeptAtAllTimes();

  // Helper method that allows crash reporting logic to determine if a
  // specific RenderProcessHost is the current spare process.
  // Returns true if |render_process_host| is the current spare
  // RenderProcessHost.
  static bool IsSpareProcessForCrashReporting(
      RenderProcessHost* render_process_host);

  PermissionServiceContext& permission_service_context() {
    return *permission_service_context_;
  }

  bool is_initialized() const { return is_initialized_; }

  // Ensures that this process is kept alive for the specified amount of time.
  // This is used to ensure that unload handlers have a chance to execute
  // before the process shuts down.
  void DelayProcessShutdownForUnload(const base::TimeDelta& timeout);

  // Binds |receiver| to the FileSystemManager instance owned by the render
  // process host, and is used by workers via BrowserInterfaceBroker.
  void BindFileSystemManager(
      const url::Origin& origin,
      mojo::PendingReceiver<blink::mojom::FileSystemManager> receiver) override;

  // Binds |receiver| to the NativeFileSystemManager instance owned by the
  // render process host, and is used by workers via BrowserInterfaceBroker.
  void BindNativeFileSystemManager(
      const url::Origin& origin,
      mojo::PendingReceiver<blink::mojom::NativeFileSystemManager> receiver)
      override;

  // Binds |receiver| to a NativeIOHost instance indirectly owned by the
  // StoragePartition. Used by frames and workers via BrowserInterfaceBroker.
  void BindNativeIOHost(
      const url::Origin& origin,
      mojo::PendingReceiver<blink::mojom::NativeIOHost> receiver);

  FileSystemManagerImpl* GetFileSystemManagerForTesting() {
    return file_system_manager_impl_.get();
  }

  // Sets the internal clock to the one specified, and also updates the
  // appropriate initialization times appropriately.
  void SetClockForTesting(base::TickClock* clock);

  // Binds |receiver| to the RestrictedCookieManager instance owned by
  // |storage_partition_impl_|, and is used by a service worker via
  // BrowserInterfaceBroker. |receiver| belongs to the service worker at
  // |origin| hosted by this process,
  void BindRestrictedCookieManagerForServiceWorker(
      const url::Origin& origin,
      mojo::PendingReceiver<network::mojom::RestrictedCookieManager> receiver)
      override;

  // Binds |receiver| to the VideoDecodePerfHistory instance owned by the render
  // process host, and is used by workers via BrowserInterfaceBroker.
  void BindVideoDecodePerfHistory(
      mojo::PendingReceiver<media::mojom::VideoDecodePerfHistory> receiver)
      override;

  // Binds |receiver| to a OneShotBackgroundSyncService instance owned by the
  // StoragePartition associated with the render process host, and is used by
  // frames and service workers via BrowserInterfaceBroker.
  void CreateOneShotSyncService(
      mojo::PendingReceiver<blink::mojom::OneShotBackgroundSyncService>
          receiver) override;

  // Binds |receiver| to a PeriodicBackgroundSyncService instance owned by the
  // StoragePartition associated with the render process host, and is used by
  // frames and service workers via BrowserInterfaceBroker.
  void CreatePeriodicSyncService(
      mojo::PendingReceiver<blink::mojom::PeriodicBackgroundSyncService>
          receiver) override;

  // Binds |receiver| to a QuotaManagerHost instance indirectly owned by the
  // StoragePartition associated with the render process host. Used by frames
  // and workers via BrowserInterfaceBroker.
  void BindQuotaManagerHost(
      int render_frame_id,
      const url::Origin& origin,
      mojo::PendingReceiver<blink::mojom::QuotaManagerHost> receiver) override;

  // Binds |receiver| to the LockManager owned by |storage_partition_impl_|.
  // |receiver| belongs to a frame or worker at |origin| hosted by this process.
  // If it belongs to a frame, |render_frame_id| identifies it, otherwise it is
  // MSG_ROUTING_NONE.
  //
  // Used by frames and workers via BrowserInterfaceBroker.
  void CreateLockManager(
      int render_frame_id,
      const url::Origin& origin,
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

  // Binds |receiver| to the NotificationService instance owned by
  // |storage_partition_impl_|, and is used by frames and workers via
  // BrowserInterfaceBroker.
  void CreateNotificationService(
      const url::Origin& origin,
      mojo::PendingReceiver<blink::mojom::NotificationService> receiver)
      override;

  // Used for shared workers and service workers to create a websocket.
  // In other cases, RenderFrameHostImpl for documents or DedicatedWorkerHost
  // for dedicated workers handles interface requests in order to associate
  // websockets with a frame. Shared workers and service workers don't have to
  // do it because they don't have a frame.
  void CreateWebSocketConnector(
      const url::Origin& origin,
      mojo::PendingReceiver<blink::mojom::WebSocketConnector> receiver)
      override;

  // Disables CORB (Cross-Origin Read Blocking) and
  // |request_initiator_origin_lock| enforcement for |process_id|.
  //
  // The exception will be removed when the corresponding RenderProcessHostImpl
  // is destroyed (see
  // |cleanup_network_service_plugin_exceptions_upon_destruction_|).
  static void AddCorbExceptionForPlugin(int process_id);

  // Allows |process_id| to use an additional |allowed_request_initiator|
  // (bypassing |request_initiator_origin_lock| enforcement).
  //
  // The exception will be removed when the corresponding RenderProcessHostImpl
  // is destroyed (see
  // |cleanup_network_service_plugin_exceptions_upon_destruction_|).
  static void AddAllowedRequestInitiatorForPlugin(
      int process_id,
      const url::Origin& allowed_request_initiator);

  using IpcSendWatcher = base::RepeatingCallback<void(const IPC::Message& msg)>;
  void SetIpcSendWatcherForTesting(IpcSendWatcher watcher) {
    ipc_send_watcher_for_testing_ = std::move(watcher);
  }

  size_t keep_alive_ref_count() const { return keep_alive_ref_count_; }

  PeerConnectionTrackerHost* GetPeerConnectionTrackerHost();

  // Allows tests to override how RenderProcessHosts bind incoming
  // BatteryMonitor receivers.
  using BatteryMonitorBinder = base::RepeatingCallback<void(
      mojo::PendingReceiver<device::mojom::BatteryMonitor>)>;
  static void OverrideBatteryMonitorBinderForTesting(
      BatteryMonitorBinder binder);

  // Allows overriding the URLLoaderFactory creation via CreateURLLoaderFactory.
  // Passing a null callback will restore the default behavior.
  // This method must be called either on the UI thread or before threads start.
  // This |url_loader_factory_callback| is run on the UI thread.
  using CreateNetworkFactoryCallback = base::RepeatingCallback<void(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
      int worker_process_id,
      mojo::PendingRemote<network::mojom::URLLoaderFactory> original_factory)>;
  static void SetNetworkFactoryForTesting(
      const CreateNetworkFactoryCallback& url_loader_factory_callback);

 protected:
  // A proxy for our IPC::Channel that lives on the IO thread.
  std::unique_ptr<IPC::ChannelProxy> channel_;

  // True if fast shutdown has been performed on this RPH.
  bool fast_shutdown_started_;

  // True if shutdown from started by the |Shutdown()| method.
  bool shutdown_requested_ = false;

  // True if we've posted a DeleteTask and will be deleted soon.
  bool deleting_soon_;

#ifndef NDEBUG
  // True if this object has deleted itself.
  bool is_self_deleted_;
#endif

  // The count of currently swapped out but pending RenderViews.  We have
  // started to swap these in, so the renderer process should not exit if
  // this count is non-zero.
  int32_t pending_views_;

 private:
  friend class ChildProcessLauncherBrowserTest_ChildSpawnFail_Test;
  friend class VisitRelayingRenderProcessHost;
  friend class StoragePartitonInterceptor;
  friend class RenderProcessHostTest;
  // TODO(crbug.com/1111231): This class is a friend so that it can call our
  // private mojo implementation methods, acting as a pass-through. This is only
  // necessary during the associated interface migration, after which,
  // AgentSchedulingGroupHost will not act as a pass-through to the private
  // methods here. At that point we'll remove this friend class.
  friend class AgentSchedulingGroupHost;

  // Use CreateRenderProcessHost() instead of calling this constructor
  // directly.
  RenderProcessHostImpl(BrowserContext* browser_context,
                        StoragePartitionImpl* storage_partition_impl,
                        bool is_for_guests_only);

  // True if this ChildProcessLauncher has a non-zero number of frames attached
  // to it and they're all low priority.  Note: This will always return false
  // unless features::kUseFramePriorityInProcessHost is enabled.
  bool HasOnlyLowPriorityFrames();

  // Initializes a new IPC::ChannelProxy in |channel_|, which will be
  // connected to the next child process launched for this host, if any.
  void InitializeChannelProxy();

  // Resets |channel_|, removing it from the attachment broker if necessary.
  // Always call this in lieu of directly resetting |channel_|.
  void ResetChannelProxy();

  // Creates and adds the IO thread message filters.
  void CreateMessageFilters();

  // Registers Mojo interfaces to be exposed to the renderer.
  void RegisterMojoInterfaces();

  // mojom::RouteProvider:
  void GetRoute(
      int32_t routing_id,
      mojo::PendingAssociatedReceiver<blink::mojom::AssociatedInterfaceProvider>
          receiver) override;

  // blink::mojom::AssociatedInterfaceProvider:
  void GetAssociatedInterface(
      const std::string& name,
      mojo::PendingAssociatedReceiver<blink::mojom::AssociatedInterface>
          receiver) override;

  // mojom::RendererHost
  using BrowserHistogramCallback =
      mojom::RendererHost::GetBrowserHistogramCallback;
  void GetBrowserHistogram(const std::string& name,
                           BrowserHistogramCallback callback) override;
  void SuddenTerminationChanged(bool enabled) override;
  void RecordUserMetricsAction(const std::string& action) override;
  void ResolveProxy(
      const GURL& url,
      mojom::RendererHost::ResolveProxyCallback callback) override;

  void BindRouteProvider(
      mojo::PendingAssociatedReceiver<mojom::RouteProvider> receiver);

  void CreateEmbeddedFrameSinkProvider(
      mojo::PendingReceiver<blink::mojom::EmbeddedFrameSinkProvider> receiver);
  void BindFrameSinkProvider(
      mojo::PendingReceiver<mojom::FrameSinkProvider> receiver);
  void BindCompositingModeReporter(
      mojo::PendingReceiver<viz::mojom::CompositingModeReporter> receiver);
  void CreateDomStorageProvider(
      mojo::PendingReceiver<blink::mojom::DomStorageProvider> receiver);
  void CreateBroadcastChannelProvider(
      mojo::PendingReceiver<blink::mojom::BroadcastChannelProvider> receiver);
  void CreateCodeCacheHost(
      mojo::PendingReceiver<blink::mojom::CodeCacheHost> receiver);
  void CreateRendererHost(
      mojo::PendingAssociatedReceiver<mojom::RendererHost> receiver);
  void BindVideoDecoderService(
      mojo::PendingReceiver<media::mojom::InterfaceFactory> receiver);
  void BindWebDatabaseHostImpl(
      mojo::PendingReceiver<blink::mojom::WebDatabaseHost> receiver);
  void BindAecDumpManager(
      mojo::PendingReceiver<blink::mojom::AecDumpManager> receiver);
  void BindPushMessagingManager(
      mojo::PendingReceiver<blink::mojom::PushMessaging> receiver);
  void BindP2PSocketManager(
      mojo::PendingReceiver<network::mojom::P2PSocketManager> receiver);
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

  // Control message handlers.
  void OnCloseACK(int closed_widget_route_id);

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

  // Called if the backgrounded or visibility state of the process changes.
  void SendProcessStateToRenderer();

  // Creates a PersistentMemoryAllocator and shares it with the renderer
  // process for it to store histograms from that process. The allocator is
  // available for extraction by a SubprocesMetricsProvider in order to
  // report those histograms to UMA.
  void CreateSharedRendererHistogramAllocator();

  // Handle termination of our process.
  void ProcessDied(bool already_dead,
                   ChildProcessTerminationInfo* known_details);

  // Destroy all objects that can cause methods to be invoked on this object or
  // any other that hang off it.
  void ResetIPC();

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

  // Returns an existing RenderProcessHost that has not yet been used and is
  // suitable for the given |site_instance|, or null if no such process host
  // exists.
  //
  // This function is used when finding a process for a service worker. The
  // idea is to choose the process that will be chosen by a navigation that will
  // use the service worker. While navigations typically try to choose the
  // process with the relevant service worker (using
  // UnmatchedServiceWorkerProcessTracker), navigations out of the Android New
  // Tab Page use a SiteInstance with an empty URL by design in order to choose
  // the NTP process, and do not go through the typical matching algorithm. The
  // goal of this function is to return the NTP process so the service worker
  // can also use it.
  static RenderProcessHost* GetUnusedProcessHostForServiceWorker(
      SiteInstanceImpl* site_instance);

  // Returns a RenderProcessHost that is rendering a URL corresponding to
  // |site_instance| in one of its frames, or that is expecting a navigation to
  // that SiteInstance.
  static RenderProcessHost* FindReusableProcessHostForSiteInstance(
      SiteInstanceImpl* site_instance);

  void CreateAgentMetricsCollectorHost(
      mojo::PendingReceiver<blink::mojom::AgentMetricsCollectorHost> receiver);

  void BindPeerConnectionTrackerHost(
      mojo::PendingReceiver<blink::mojom::PeerConnectionTrackerHost> receiver);

#if BUILDFLAG(ENABLE_MDNS)
  void CreateMdnsResponder(
      mojo::PendingReceiver<network::mojom::MdnsResponder> receiver);
#endif  // BUILDFLAG(ENABLE_MDNS)

  void NotifyRendererOfLockedStateUpdate();
  void PopulateTerminationInfoRendererFields(ChildProcessTerminationInfo* info);

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
  // the render process currently hosted by the RPHI. Callbacks added by this
  // method will never run beyond the next invocation of Cleanup().
  template <typename CallbackType>
  void AddUIThreadInterface(service_manager::BinderRegistry* registry,
                            CallbackType callback) {
    registry->AddInterface(
        base::BindRepeating(
            &InterfaceGetter<CallbackType>::GetInterfaceOnUIThread,
            instance_weak_factory_->GetWeakPtr(), std::move(callback)),
        GetUIThreadTaskRunner({}));
  }

  // Callback to unblock process shutdown after waiting for unload handlers to
  // execute.
  void CancelProcessShutdownDelayForUnload();

  // Creates a URLLoaderFactory that can be used by the renderer process,
  // without binding it to a specific frame or an origin.
  //
  // TODO(kinuko, lukasza): https://crbug.com/1114822: Remove, once all
  // URLLoaderFactories vended to a renderer process are associated with a
  // specific origin and an execution context (e.g. a frame, a service worker or
  // any other kind of worker).
  void CreateURLLoaderFactoryForRendererProcess(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver);

  // Binds a TracedProcess interface in the renderer process. This is used to
  // communicate with the Tracing service.
  void BindTracedProcess(
      mojo::PendingReceiver<tracing::mojom::TracedProcess> receiver);

  // Handles incoming requests to bind a process-scoped receiver from the
  // renderer process. This is posted to the main thread by IOThreadHostImpl
  // if the request isn't handled on the IO thread.
  void OnBindHostReceiver(mojo::GenericPendingReceiver receiver);

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
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

  mojo::OutgoingInvitation mojo_invitation_;

  size_t keep_alive_ref_count_;

  // Set in DisableKeepAliveRefCount(). When true, |keep_alive_ref_count_| must
  // no longer be modified.
  bool is_keep_alive_ref_count_disabled_;

  // The registered IPC listener objects. When this list is empty, we should
  // delete ourselves.
  base::IDMap<IPC::Listener*> listeners_;

  // Mojo interfaces provided to the child process are registered here if they
  // need consistent delivery ordering with legacy IPC, and are process-wide in
  // nature (e.g. metrics, memory usage).
  std::unique_ptr<blink::AssociatedInterfaceRegistry> associated_interfaces_;

  mojo::AssociatedReceiver<mojom::RouteProvider> route_provider_receiver_{this};
  mojo::AssociatedReceiverSet<blink::mojom::AssociatedInterfaceProvider,
                              int32_t>
      associated_interface_provider_receivers_;

  // These fields are cached values that are updated in
  // UpdateProcessPriorityInputs, and are used to compute priority sent to
  // ChildProcessLauncher.
  // |visible_clients_| is the count of currently visible clients.
  int32_t visible_clients_;
  // |frame_depth_| can be used to rank processes of the same visibility, ie it
  // is the lowest depth of all visible clients, or if there are no visible
  // widgets the lowest depth of all hidden clients. Initialized to max depth
  // when there are no clients.
  unsigned int frame_depth_ = kMaxFrameDepthForPriority;
  // Tracks the number of low priority frames currently hosted in this process.
  // Always 0 unless features::kUseFramePriorityInProcessHost is enabled.
  unsigned int low_priority_frames_ = 0;
  // Tracks the total number of frames currently hosted in this process.
  // Always 0 unless features::kUseFramePriorityInProcessHost is enabled.
  unsigned int total_frames_ = 0;
  // |intersects_viewport_| similar to |frame_depth_| can be used to rank
  // processes of same visibility. It indicates process has frames that
  // intersect with the viewport.
  bool intersects_viewport_ = false;
#if defined(OS_ANDROID)
  // Highest importance of all clients that contribute priority.
  ChildProcessImportance effective_importance_ = ChildProcessImportance::NORMAL;
#endif

  // Clients that contribute priority to this process.
  base::flat_set<PriorityClient*> priority_clients_;

  ChildProcessLauncherPriority priority_;

  // If this is set then the built-in process priority calculation system is
  // ignored, and an externally computed process priority is used. Set to true
  // and the process will stay foreground priority; set to false and it will
  // stay background priority.
  base::Optional<bool> priority_override_;

  // Track whether a low/normal priority frame (respectively) has been attached
  // to the host at any point during its lifetime.  Used for UMA metrics
  // recorded in destructor.
  bool low_priority_frames_seen_ = false;
  bool normal_priority_frames_seen_ = false;

  // Used for tracking how much time out of the total time the process spent in
  // the background.  For this, we track if the process |is_backgrounded_|
  // currently as well as the |background_duration_| for the process so far and
  // the last |background_status_update_time_| for the process as observed in
  // UpdateProcessPriority.  Total time is obtained by looking at the current
  // time upon deletion relative to |init_time_|.
  bool is_backgrounded_ = false;
  base::TimeTicks background_status_update_time_;
  base::TimeDelta background_duration_;

  // The tick clock used to get the current time.  Used for determining
  // |init_time_| and |background_status_update_time_| for metrics.  Can be
  // replaced by tests.
  const base::TickClock* clock_;

  // Used to allow a RenderWidgetHost to intercept various messages on the
  // IO thread.
  scoped_refptr<RenderWidgetHelper> widget_helper_;

  scoped_refptr<ResolveProxyHelper> resolve_proxy_helper_;

  scoped_refptr<RenderFrameMessageFilter> render_frame_message_filter_;

  // Used in single-process mode.
  std::unique_ptr<base::Thread> in_process_renderer_;

  // True after Init() has been called.
  bool is_initialized_ = false;

  // True after ProcessDied(), until the next call to Init().
  bool is_dead_ = false;

  // Stores the time at which the first call to Init happened.
  base::TimeTicks init_time_;

  // Used to launch and terminate the process without blocking the UI thread.
  std::unique_ptr<ChildProcessLauncher> child_process_launcher_;

  // The globally-unique identifier for this RPH.
  const int id_;

  BrowserContext* const browser_context_;

  // Owned by |browser_context_|.
  StoragePartitionImpl* const storage_partition_impl_;

  // Owns the singular DomStorageProvider binding established by this renderer.
  mojo::Receiver<blink::mojom::DomStorageProvider>
      dom_storage_provider_receiver_{this};

  // Keeps track of the ReceiverIds returned by
  // storage_partition_impl_->BindDomStorage() calls so we can Unbind() them on
  // cleanup.
  std::set<mojo::ReceiverId> dom_storage_receiver_ids_;

  // The observers watching our lifetime.
  base::ObserverList<RenderProcessHostObserver>::Unchecked observers_;

  // True if the process can be shut down suddenly.  If this is true, then we're
  // sure that all the RenderViews in the process can be shutdown suddenly.  If
  // it's false, then specific RenderViews might still be allowed to be shutdown
  // suddenly by checking their SuddenTerminationAllowed() flag.  This can occur
  // if one WebContents has an unload event listener but another WebContents in
  // the same process doesn't.
  bool sudden_termination_allowed_;

  // Set to true if this process is blocked and shouldn't be sent input events.
  // The checking of this actually happens in the RenderWidgetHost.
  bool is_blocked_;

  // The clients who want to know when the blocked state has changed.
  BlockStateChangedCallbackList blocked_state_changed_callback_list_;

  // Records the last time we regarded the child process active.
  base::TimeTicks child_process_activity_time_;

  // Indicates whether this RenderProcessHost is exclusively hosting guest
  // RenderFrames.
  bool is_for_guests_only_;

  // Indicates whether this RenderProcessHost is unused, meaning that it has
  // not committed any web content, and it has not been given to a SiteInstance
  // that has a site assigned.
  bool is_unused_;

  // Set if a call to Cleanup is required once the RenderProcessHostImpl is no
  // longer within the RenderProcessHostObserver::RenderProcessExited callbacks.
  bool delayed_cleanup_needed_;

  // Indicates whether RenderProcessHostImpl::ProcessDied is currently iterating
  // and calling through RenderProcessHostObserver::RenderProcessExited.
  bool within_process_died_observer_;

  std::unique_ptr<P2PSocketDispatcherHost> p2p_socket_dispatcher_host_;

  // Must be accessed on UI thread.
  AecDumpManagerImpl aec_dump_manager_;

  std::unique_ptr<MediaStreamTrackMetricsHost, BrowserThread::DeleteOnIOThread>
      media_stream_track_metrics_host_;

  std::unique_ptr<AgentMetricsCollectorHost> agent_metrics_collector_host_;

  std::unique_ptr<VideoDecoderProxy> video_decoder_proxy_;

  // Forwards messages between WebRTCInternals in the browser process
  // and PeerConnectionTracker in the renderer process.
  std::unique_ptr<PeerConnectionTrackerHost> peer_connection_tracker_host_;

  // Records the time when the process starts surviving for workers for UMA.
  base::TimeTicks keep_alive_start_time_;

  // Context shared for each mojom::PermissionService instance created for this
  // RPH. This is destroyed early in ResetIPC() method.
  std::unique_ptr<PermissionServiceContext> permission_service_context_;

  // The memory allocator, if any, in which the renderer will write its metrics.
  std::unique_ptr<base::PersistentMemoryAllocator> metrics_allocator_;

  std::unique_ptr<CodeCacheHostImpl> code_cache_host_impl_;

  bool channel_connected_;
  bool sent_render_process_ready_;

  std::unique_ptr<FileSystemManagerImpl, BrowserThread::DeleteOnIOThread>
      file_system_manager_impl_;
  std::unique_ptr<viz::GpuClient, BrowserThread::DeleteOnIOThread> gpu_client_;
  std::unique_ptr<PushMessagingManager, base::OnTaskRunnerDeleter>
      push_messaging_manager_;

  std::unique_ptr<EmbeddedFrameSinkProviderImpl> embedded_frame_sink_provider_;
  std::unique_ptr<PluginRegistryImpl> plugin_registry_;

  mojo::Remote<mojom::ChildProcess> child_process_;
  // This will be bound to |io_thread_host_impl_|.
  mojo::PendingReceiver<mojom::ChildProcessHost> child_host_pending_receiver_;
  mojo::AssociatedRemote<mojom::RouteProvider> remote_route_provider_;
  mojo::AssociatedRemote<mojom::Renderer> renderer_interface_;
  mojo::AssociatedReceiver<mojom::RendererHost> renderer_host_receiver_{this};
  mojo::Receiver<memory_instrumentation::mojom::CoordinatorConnector>
      coordinator_connector_receiver_{this};

  // Tracks active audio and video streams within the render process; used to
  // determine if if a process should be backgrounded.
  int media_stream_count_ = 0;

  // Tracks service workers that may need to respond to events from other
  // processes in a timely manner.  Used to determine if a process should
  // not be backgrounded.
  int foreground_service_worker_count_ = 0;

  // A WeakPtrFactory which is reset every time Cleanup() runs. Used to vend
  // WeakPtrs which are invalidated any time the RPHI is recycled.
  base::Optional<base::WeakPtrFactory<RenderProcessHostImpl>>
      instance_weak_factory_;

  FrameSinkProviderImpl frame_sink_provider_;
  std::unique_ptr<mojo::Receiver<viz::mojom::CompositingModeReporter>>
      compositing_mode_reporter_;

  bool cleanup_network_service_plugin_exceptions_upon_destruction_ = false;

  // Fields for recording MediaStream UMA.
  bool has_recorded_media_stream_frame_depth_metric_ = false;

  // If the RenderProcessHost is being shutdown via Shutdown(), this records the
  // exit code.
  int shutdown_exit_code_;

  IpcSendWatcher ipc_send_watcher_for_testing_;

  // Keeps this process registered with the tracing subsystem.
  std::unique_ptr<TracingServiceController::ClientRegistration>
      tracing_registration_;

  // IOThreadHostImpl owns some IO-thread state associated with this
  // RenderProcessHostImpl. This is mainly to allow various IPCs from the
  // renderer to be handled on the IO thread without a hop to the UI thread.
  //
  // Declare this at then end to ensure it triggers the destruction of the
  // IOThreadHostImpl prior to other members with an IO thread deleter that are
  // bound to a mojo receiver callback using a base::Unretained.  This is
  // necessary to ensure those objects stop receiving mojo messages before their
  // destruction.
  class IOThreadHostImpl;
  friend class IOThreadHostImpl;
  base::Optional<base::SequenceBound<IOThreadHostImpl>> io_thread_host_impl_;

  base::WeakPtrFactory<RenderProcessHostImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(RenderProcessHostImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_PROCESS_HOST_IMPL_H_
