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

#include "base/containers/flat_set.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/process/process.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "content/browser/cache_storage/cache_storage_dispatcher_host.h"
#include "content/browser/child_process_launcher.h"
#include "content/browser/dom_storage/session_storage_namespace_impl.h"
#include "content/browser/media/video_decoder_proxy.h"
#include "content/browser/renderer_host/embedded_frame_sink_provider_impl.h"
#include "content/browser/renderer_host/frame_sink_provider_impl.h"
#include "content/browser/renderer_host/media/renderer_audio_output_stream_factory_context_impl.h"
#include "content/common/associated_interfaces.mojom.h"
#include "content/common/child_control.mojom.h"
#include "content/common/content_export.h"
#include "content/common/media/media_stream.mojom.h"
#include "content/common/media/renderer_audio_output_stream_factory.mojom.h"
#include "content/common/renderer.mojom.h"
#include "content/common/renderer_host.mojom.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/service_manager_connection.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_platform_file.h"
#include "media/media_buildflags.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/bindings/associated_binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/cpp/system/invitation.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/mojom/service.mojom.h"
#include "services/viz/public/interfaces/compositing/compositing_mode_watcher.mojom.h"
#include "services/ws/public/mojom/gpu.mojom.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/mojom/associated_interfaces/associated_interfaces.mojom.h"
#include "third_party/blink/public/mojom/dom_storage/storage_partition_service.mojom.h"
#include "third_party/blink/public/mojom/filesystem/file_system.mojom.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gl/gpu_switching_observer.h"

#if defined(OS_ANDROID)
#include "content/public/browser/android/child_process_importance.h"
#endif

namespace base {
class CommandLine;
class MessageLoop;
class SharedPersistentMemoryAllocator;
}

namespace viz {
class GpuClient;
}

namespace content {
class BrowserPluginMessageFilter;
class ChildConnection;
class FileSystemManagerImpl;
class IndexedDBDispatcherHost;
class InProcessChildThreadParams;
class MediaStreamTrackMetricsHost;
class P2PSocketDispatcherHost;
class PermissionServiceContext;
class PeerConnectionTrackerHost;
class PluginRegistryImpl;
class PushMessagingManager;
class RenderFrameMessageFilter;
class RenderProcessHostFactory;
class RenderWidgetHelper;
class RenderWidgetHost;
class RenderWidgetHostImpl;
class ResourceMessageFilter;
class ServiceWorkerDispatcherHost;
class SiteInstance;
class SiteInstanceImpl;
class StoragePartition;
class StoragePartitionImpl;
struct ChildProcessTerminationInfo;

typedef base::Thread* (*RendererMainThreadFactoryFunction)(
    const InProcessChildThreadParams& params);

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
      public ui::GpuSwitchingObserver,
      public mojom::RouteProvider,
      public blink::mojom::AssociatedInterfaceProvider,
      public mojom::RendererHost {
 public:
  // Special depth used when there are no PriorityClients.
  static const unsigned int kMaxFrameDepthForPriority;

  // Create a new RenderProcessHost.
  // If |storage_partition_impl| is null, the default partition from the
  // browser_context is used, using |site_instance| (for which a null value is
  // legal). |site_instance| is not used if |storage_partition_impl| is not
  // null.
  static RenderProcessHost* CreateRenderProcessHost(
      BrowserContext* browser_context,
      StoragePartitionImpl* storage_partition_impl,
      SiteInstance* site_instance,
      bool is_for_guests_only);

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
  int VisibleClientCount() const override;
  unsigned int GetFrameDepth() const override;
  bool GetIntersectsViewport() const override;
  bool IsForGuestsOnly() const override;
  StoragePartition* GetStoragePartition() const override;
  bool Shutdown(int exit_code) override;
  bool FastShutdownIfPossible(size_t page_count = 0,
                              bool skip_unload_handlers = false) override;
  const base::Process& GetProcess() const override;
  bool IsReady() const override;
  BrowserContext* GetBrowserContext() const override;
  bool InSameStoragePartition(StoragePartition* partition) const override;
  int GetID() const override;
  bool IsInitializedAndNotDead() const override;
  void SetIgnoreInputEvents(bool ignore_input_events) override;
  bool IgnoreInputEvents() const override;
  void Cleanup() override;
  void AddPendingView() override;
  void RemovePendingView() override;
  void AddWidget(RenderWidgetHost* widget) override;
  void RemoveWidget(RenderWidgetHost* widget) override;
#if defined(OS_ANDROID)
  ChildProcessImportance GetEffectiveImportance() override;
#endif
  void SetSuddenTerminationAllowed(bool enabled) override;
  bool SuddenTerminationAllowed() const override;
  IPC::ChannelProxy* GetChannel() override;
  void AddFilter(BrowserMessageFilter* filter) override;
  bool FastShutdownStarted() const override;
  base::TimeDelta GetChildProcessIdleTime() const override;
  void FilterURL(bool empty_allowed, GURL* url) override;
  void EnableAudioDebugRecordings(const base::FilePath& file) override;
  void DisableAudioDebugRecordings() override;
  void SetEchoCanceller3(
      bool enable,
      base::OnceCallback<void(bool, const std::string&)> callback) override;
  WebRtcStopRtpDumpCallback StartRtpDump(
      bool incoming,
      bool outgoing,
      const WebRtcRtpPacketCallback& packet_callback) override;
  void SetWebRtcEventLogOutput(int lid, bool enabled) override;
  void BindInterface(const std::string& interface_name,
                     mojo::ScopedMessagePipeHandle interface_pipe) override;
  const service_manager::Identity& GetChildIdentity() const override;
  std::unique_ptr<base::SharedPersistentMemoryAllocator> TakeMetricsAllocator()
      override;
  const base::TimeTicks& GetInitTimeForNavigationMetrics() const override;
  bool IsProcessBackgrounded() const override;
  void IncrementKeepAliveRefCount(
      RenderProcessHost::KeepAliveClientType) override;
  void DecrementKeepAliveRefCount(
      RenderProcessHost::KeepAliveClientType) override;
  void DisableKeepAliveRefCount() override;
  bool IsKeepAliveRefCountDisabled() override;
  void PurgeAndSuspend() override;
  void Resume() override;
  mojom::Renderer* GetRendererInterface() override;
  resource_coordinator::ProcessResourceCoordinator*
  GetProcessResourceCoordinator() override;
  void CreateURLLoaderFactory(
      const url::Origin& origin,
      network::mojom::URLLoaderFactoryRequest request) override;

  void SetIsNeverSuitableForReuse() override;
  bool MayReuseHost() override;
  bool IsUnused() override;
  void SetIsUsed() override;

  bool HostHasNotBeenUsed() override;
  void LockToOrigin(const GURL& lock_url) override;
  void BindCacheStorage(blink::mojom::CacheStorageRequest request,
                        const url::Origin& origin) override;
  void CleanupCorbExceptionForPluginUponDestruction() override;

  mojom::RouteProvider* GetRemoteRouteProvider();

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

  // Implementation of FilterURL below that can be shared with the mock class.
  static void FilterURL(RenderProcessHost* rph, bool empty_allowed, GURL* url);

  // Returns true if |host| is suitable for rendering a page in the given
  // |browser_context|, where the page would utilize |site_url| as its
  // SiteInstance site URL, and its process would be locked to |lock_url|.
  // |site_url| and |lock_url| may differ in cases where an effective URL is
  // not the actual site that the process is locked to, which happens for
  // hosted apps.
  static bool IsSuitableHost(RenderProcessHost* host,
                             BrowserContext* browser_context,
                             const GURL& site_url,
                             const GURL& lock_url);

  // Returns an existing RenderProcessHost for |url| in |browser_context|,
  // if one exists.  Otherwise a new RenderProcessHost should be created and
  // registered using RegisterProcessHostForSite().
  // This should only be used for process-per-site mode, which can be enabled
  // globally with a command line flag or per-site, as determined by
  // SiteInstanceImpl::ShouldUseProcessPerSite.
  // Important: |url| should be a full URL and *not* a site URL.
  static RenderProcessHost* GetSoleProcessHostForURL(
      BrowserContext* browser_context,
      const GURL& url);

  // Variant of the above that takes in a SiteInstance site URL and the
  // process's origin lock URL, when they are known.
  static RenderProcessHost* GetSoleProcessHostForSite(
      BrowserContext* browser_context,
      const GURL& site_url,
      const GURL& lock_url);

  // Registers the given |process| to be used for all sites identified by
  // |site_instance| within |browser_context|.
  // This should only be used for process-per-site mode, which can be enabled
  // globally with a command line flag or per-site, as determined by
  // SiteInstanceImpl::ShouldUseProcessPerSite.
  static void RegisterSoleProcessHostForSite(BrowserContext* browser_context,
                                             RenderProcessHost* process,
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

  static base::MessageLoop* GetInProcessRendererThreadForTesting();

  // This forces a renderer that is running "in process" to shut down.
  static void ShutDownInProcessRenderer();

  static void RegisterRendererMainThreadFactory(
      RendererMainThreadFactoryFunction create);

  // Allows external code to supply a function which creates a
  // StoragePartitionService. Used for supplying test versions of the
  // service.
  using CreateStoragePartitionServiceFunction =
      void (*)(RenderProcessHostImpl* rph,
               blink::mojom::StoragePartitionServiceRequest request);
  static void SetCreateStoragePartitionServiceFunction(
      CreateStoragePartitionServiceFunction function);

  RenderFrameMessageFilter* render_frame_message_filter_for_testing() const {
    return render_frame_message_filter_.get();
  }

  void SetBrowserPluginMessageFilterSubFilterForTesting(
      scoped_refptr<BrowserMessageFilter> message_filter) const;

  void set_is_for_guests_only_for_testing(bool is_for_guests_only) {
    is_for_guests_only_ = is_for_guests_only;
  }

#if defined(OS_POSIX) && !defined(OS_ANDROID) && !defined(OS_MACOSX)
  // Launch the zygote early in the browser startup.
  static void EarlyZygoteLaunch();
#endif  // defined(OS_POSIX) && !defined(OS_ANDROID) && !defined(OS_MACOSX)

  void RecomputeAndUpdateWebKitPreferences();

  RendererAudioOutputStreamFactoryContext*
  GetRendererAudioOutputStreamFactoryContext() override;

  // Called when a video capture stream or an audio stream is added or removed
  // and used to determine if the process should be backgrounded or not.
  void OnMediaStreamAdded() override;
  void OnMediaStreamRemoved() override;
  int get_media_stream_count_for_testing() const { return media_stream_count_; }

  // Sets the global factory used to create new RenderProcessHosts in unit
  // tests.  It may be nullptr, in which case the default RenderProcessHost will
  // be created (this is the behavior if you don't call this function).  The
  // factory must be set back to nullptr before it's destroyed; ownership is not
  // transferred.
  static void set_render_process_host_factory_for_testing(
      const RenderProcessHostFactory* rph_factory);
  // Gets the global factory used to create new RenderProcessHosts in unit
  // tests.
  static const RenderProcessHostFactory*
  get_render_process_host_factory_for_testing();

  // Tracks which sites frames are hosted in which RenderProcessHosts.
  static void AddFrameWithSite(BrowserContext* browser_context,
                               RenderProcessHost* render_process_host,
                               const GURL& site_url);
  static void RemoveFrameWithSite(BrowserContext* browser_context,
                                  RenderProcessHost* render_process_host,
                                  const GURL& site_url);

  // Tracks which sites navigations are expected to commit in which
  // RenderProcessHosts.
  static void AddExpectedNavigationToSite(
      BrowserContext* browser_context,
      RenderProcessHost* render_process_host,
      const GURL& site_url);
  static void RemoveExpectedNavigationToSite(
      BrowserContext* browser_context,
      RenderProcessHost* render_process_host,
      const GURL& site_url);

  // Return the spare RenderProcessHost, if it exists. There is at most one
  // globally-used spare RenderProcessHost at any time.
  static RenderProcessHost* GetSpareRenderProcessHostForTesting();

  // Discards the spare RenderProcessHost.  After this call,
  // GetSpareRenderProcessHostForTesting will return nullptr.
  static void DiscardSpareRenderProcessHostForTesting();

  // Returns true if a spare RenderProcessHost should be kept at all times.
  static bool IsSpareProcessKeptAtAllTimes();

  PermissionServiceContext& permission_service_context() {
    return *permission_service_context_;
  }

  bool is_initialized() const { return is_initialized_; }

  // Ensures that this process is kept alive for the specified amount of time.
  // This is used to ensure that unload handlers have a chance to execute
  // before the process shuts down.
  void DelayProcessShutdownForUnload(const base::TimeDelta& timeout);

  // Binds request to the FileSystemManager instance owned by the render process
  // host, and is used by workers via RendererInterfaceBinders.
  void BindFileSystemManager(blink::mojom::FileSystemManagerRequest request);
  FileSystemManagerImpl* GetFileSystemManagerForTesting() {
    return file_system_manager_impl_.get();
  }

  // Adds a CORB (Cross-Origin Read Blocking) exception for |process_id|.  The
  // exception will be removed when the corresponding RenderProcessHostImpl is
  // destroyed (see |cleanup_corb_exception_for_plugin_upon_destruction_|).
  static void AddCorbExceptionForPlugin(int process_id);

 protected:
  // A proxy for our IPC::Channel that lives on the IO thread.
  std::unique_ptr<IPC::ChannelProxy> channel_;

  // True if fast shutdown has been performed on this RPH.
  bool fast_shutdown_started_;

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
  class ConnectionFilterController;
  class ConnectionFilterImpl;

  // Use CreateRenderProcessHost() instead of calling this constructor
  // directly.
  RenderProcessHostImpl(BrowserContext* browser_context,
                        StoragePartitionImpl* storage_partition_impl,
                        bool is_for_guests_only);

  // Initializes a new IPC::ChannelProxy in |channel_|, which will be connected
  // to the next child process launched for this host, if any.
  void InitializeChannelProxy();

  // Resets |channel_|, removing it from the attachment broker if necessary.
  // Always call this in lieu of directly resetting |channel_|.
  void ResetChannelProxy();

  // Creates and adds the IO thread message filters.
  void CreateMessageFilters();

  // Registers Mojo interfaces to be exposed to the renderer.
  void RegisterMojoInterfaces();

  // mojom::RouteProvider:
  void GetRoute(int32_t routing_id,
                blink::mojom::AssociatedInterfaceProviderAssociatedRequest
                    request) override;

  // blink::mojom::AssociatedInterfaceProvider:
  void GetAssociatedInterface(
      const std::string& name,
      blink::mojom::AssociatedInterfaceAssociatedRequest request) override;

  // mojom::RendererHost
  using BrowserHistogramCallback =
      mojom::RendererHost::GetBrowserHistogramCallback;
  void GetBrowserHistogram(const std::string& name,
                           BrowserHistogramCallback callback) override;
  void SuddenTerminationChanged(bool enabled) override;

  void BindRouteProvider(mojom::RouteProviderAssociatedRequest request);

  void CreateEmbeddedFrameSinkProvider(
      blink::mojom::EmbeddedFrameSinkProviderRequest request);
  void BindFrameSinkProvider(mojom::FrameSinkProviderRequest request);
  void BindCompositingModeReporter(
      viz::mojom::CompositingModeReporterRequest request);
  void CreateStoragePartitionService(
      blink::mojom::StoragePartitionServiceRequest request);
  void CreateRendererHost(mojom::RendererHostAssociatedRequest request);
  void BindVideoDecoderService(media::mojom::InterfaceFactoryRequest request);

  // Control message handlers.
  void OnUserMetricsRecordAction(const std::string& action);
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

  // GpuSwitchingObserver implementation.
  void OnGpuSwitched() override;

  void RecordKeepAliveDuration(RenderProcessHost::KeepAliveClientType,
                               base::TimeTicks start,
                               base::TimeTicks end);

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
  // that SiteInstance.
  static RenderProcessHost* FindReusableProcessHostForSiteInstance(
      SiteInstanceImpl* site_instance);

  void CreateMediaStreamDispatcherHost(
      MediaStreamManager* media_stream_manager,
      mojom::MediaStreamDispatcherHostRequest request);
  void CreateMediaStreamTrackMetricsHost(
      mojom::MediaStreamTrackMetricsHostRequest request);

  void OnRegisterAecDumpConsumer(int id);
  void OnUnregisterAecDumpConsumer(int id);
  void RegisterAecDumpConsumerOnUIThread(int id);
  void UnregisterAecDumpConsumerOnUIThread(int id);
  void EnableAecDumpForId(const base::FilePath& file, int id);
  // Sends |file_for_transit| to the render process.
  void SendAecDumpFileToRenderer(int id,
                                 IPC::PlatformFileForTransit file_for_transit);
  void SendDisableAecDumpToRenderer();
  base::FilePath GetAecDumpFilePathWithExtensions(const base::FilePath& file);
  base::SequencedTaskRunner& GetAecDumpFileTaskRunner();
  void OnAec3Enabled();
  void NotifyRendererIfLockedToSite();

  static void OnMojoError(int render_process_id, const std::string& error);

  template <typename InterfaceType>
  using AddInterfaceCallback =
      base::Callback<void(mojo::InterfaceRequest<InterfaceType>)>;

  template <typename CallbackType>
  struct InterfaceGetter;

  template <typename InterfaceType>
  struct InterfaceGetter<AddInterfaceCallback<InterfaceType>> {
    static void GetInterfaceOnUIThread(
        base::WeakPtr<RenderProcessHostImpl> weak_host,
        const AddInterfaceCallback<InterfaceType>& callback,
        mojo::InterfaceRequest<InterfaceType> request) {
      if (!weak_host)
        return;
      callback.Run(std::move(request));
    }
  };

  // Helper to bind an interface callback whose lifetime is limited to that of
  // the render process currently hosted by the RPHI. Callbacks added by this
  // method will never run beyond the next invocation of Cleanup().
  template <typename CallbackType>
  void AddUIThreadInterface(service_manager::BinderRegistry* registry,
                            const CallbackType& callback) {
    registry->AddInterface(
        base::Bind(&InterfaceGetter<CallbackType>::GetInterfaceOnUIThread,
                   instance_weak_factory_->GetWeakPtr(), callback),
        base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::UI}));
  }

  // Callback to unblock process shutdown after waiting for unload handlers to
  // execute.
  void CancelProcessShutdownDelayForUnload();

  mojo::OutgoingInvitation mojo_invitation_;

  std::unique_ptr<ChildConnection> child_connection_;
  int connection_filter_id_ =
      ServiceManagerConnection::kInvalidConnectionFilterId;
  scoped_refptr<ConnectionFilterController> connection_filter_controller_;
  service_manager::mojom::ServicePtr test_service_;

  size_t keep_alive_ref_count_;

  // TODO(panicker): Remove these after investigation in
  // https://crbug.com/823482.
  static const size_t kNumKeepAliveClients = 4;
  size_t keep_alive_client_count_[kNumKeepAliveClients];
  base::TimeTicks keep_alive_client_start_time_[kNumKeepAliveClients];

  // Set in DisableKeepAliveRefCount(). When true, |keep_alive_ref_count_| must
  // no longer be modified.
  bool is_keep_alive_ref_count_disabled_;

  // Whether this host is never suitable for reuse as determined in the
  // MayReuseHost() function.
  bool is_never_suitable_for_reuse_ = false;

  // The registered IPC listener objects. When this list is empty, we should
  // delete ourselves.
  base::IDMap<IPC::Listener*> listeners_;

  // Mojo interfaces provided to the child process are registered here if they
  // need consistent delivery ordering with legacy IPC, and are process-wide in
  // nature (e.g. metrics, memory usage).
  std::unique_ptr<blink::AssociatedInterfaceRegistry> associated_interfaces_;

  mojo::AssociatedBinding<mojom::RouteProvider> route_provider_binding_;
  mojo::AssociatedBindingSet<blink::mojom::AssociatedInterfaceProvider, int32_t>
      associated_interface_provider_bindings_;

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
  // |intersects_viewport_| similar to |frame_depth_| can be used to rank
  // processes of same visibility. It indicates process has frames that
  // intersect with the viewport.
  bool intersects_viewport_ = false;
#if defined(OS_ANDROID)
  // Highest importance of all clients that contribute priority.
  ChildProcessImportance effective_importance_ = ChildProcessImportance::NORMAL;
#endif

  // Clients that contribute priority to this proces.
  base::flat_set<PriorityClient*> priority_clients_;

  // The set of widgets in this RenderProcessHostImpl.
  std::set<RenderWidgetHostImpl*> widgets_;

  ChildProcessLauncherPriority priority_;

  // Used to allow a RenderWidgetHost to intercept various messages on the
  // IO thread.
  scoped_refptr<RenderWidgetHelper> widget_helper_;

  scoped_refptr<RenderFrameMessageFilter> render_frame_message_filter_;

  // The filter for messages coming from the browser plugin.
  scoped_refptr<BrowserPluginMessageFilter> bp_message_filter_;

  // Used in single-process mode.
  std::unique_ptr<base::Thread> in_process_renderer_;

  // True after Init() has been called.
  bool is_initialized_ = false;

  // True after ProcessDied(), until the next call to Init().
  bool is_dead_ = false;

  // PlzNavigate
  // Stores the time at which the first call to Init happened.
  base::TimeTicks init_time_;

  // Used to launch and terminate the process without blocking the UI thread.
  std::unique_ptr<ChildProcessLauncher> child_process_launcher_;

  // The globally-unique identifier for this RPH.
  const int id_;

  // A secondary ID used by the Service Manager to distinguish different
  // incarnations of the same RPH from each other. Unlike |id_| this is not
  // globally unique, but it is guaranteed to change every time ProcessDied() is
  // called.
  int instance_id_ = 1;

  BrowserContext* const browser_context_;

  // Owned by |browser_context_|.
  StoragePartitionImpl* storage_partition_impl_;

  // The observers watching our lifetime.
  base::ObserverList<RenderProcessHostObserver>::Unchecked observers_;

  // True if the process can be shut down suddenly.  If this is true, then we're
  // sure that all the RenderViews in the process can be shutdown suddenly.  If
  // it's false, then specific RenderViews might still be allowed to be shutdown
  // suddenly by checking their SuddenTerminationAllowed() flag.  This can occur
  // if one WebContents has an unload event listener but another WebContents in
  // the same process doesn't.
  bool sudden_termination_allowed_;

  // Set to true if we shouldn't send input events.  We actually do the
  // filtering for this at the render widget level.
  bool ignore_input_events_;

  // Records the last time we regarded the child process active.
  base::TimeTicks child_process_activity_time_;

  // Indicates whether this RenderProcessHost is exclusively hosting guest
  // RenderFrames.
  bool is_for_guests_only_;

  // Indicates whether this RenderProcessHost is unused, meaning that it has
  // not committed any web content, and it has not been given to a SiteInstance
  // that has a site assigned.
  bool is_unused_;

  // Prevents the class from being added as a GpuDataManagerImpl observer more
  // than once.
  bool gpu_observer_registered_;

  // Set if a call to Cleanup is required once the RenderProcessHostImpl is no
  // longer within the RenderProcessHostObserver::RenderProcessExited callbacks.
  bool delayed_cleanup_needed_;

  // Indicates whether RenderProcessHostImpl is currently iterating and calling
  // through RenderProcessHostObserver::RenderProcessExited.
  bool within_process_died_observer_;

  std::unique_ptr<RendererAudioOutputStreamFactoryContextImpl,
                  BrowserThread::DeleteOnIOThread>
      audio_output_stream_factory_context_;

  std::unique_ptr<P2PSocketDispatcherHost> p2p_socket_dispatcher_host_;

  // Must be accessed on UI thread.
  std::vector<int> aec_dump_consumers_;
  base::OnceCallback<void(bool, const std::string&)> aec3_set_callback_;

  WebRtcStopRtpDumpCallback stop_rtp_dump_callback_;

  scoped_refptr<base::SequencedTaskRunner>
      audio_debug_recordings_file_task_runner_;

  std::unique_ptr<MediaStreamTrackMetricsHost, BrowserThread::DeleteOnIOThread>
      media_stream_track_metrics_host_;

  std::unique_ptr<VideoDecoderProxy> video_decoder_proxy_;

  // Forwards messages between WebRTCInternals in the browser process
  // and PeerConnectionTracker in the renderer process.
  // It holds a raw pointer to webrtc_eventlog_host_, and therefore should be
  // defined below it so it is destructed first.
  scoped_refptr<PeerConnectionTrackerHost> peer_connection_tracker_host_;

  // Records the time when the process starts surviving for workers for UMA.
  base::TimeTicks keep_alive_start_time_;

  // Context shared for each mojom::PermissionService instance created for this
  // RPH.
  std::unique_ptr<PermissionServiceContext> permission_service_context_;

  // The memory allocator, if any, in which the renderer will write its metrics.
  std::unique_ptr<base::SharedPersistentMemoryAllocator> metrics_allocator_;

  std::unique_ptr<IndexedDBDispatcherHost, BrowserThread::DeleteOnIOThread>
      indexed_db_factory_;

  std::unique_ptr<ServiceWorkerDispatcherHost, BrowserThread::DeleteOnIOThread>
      service_worker_dispatcher_host_;

  scoped_refptr<CacheStorageDispatcherHost> cache_storage_dispatcher_host_;

  bool channel_connected_;
  bool sent_render_process_ready_;

#if defined(OS_ANDROID)
  // UI thread is the source of sync IPCs and all shutdown signals.
  // Therefore a proper shutdown event to unblock the UI thread is not
  // possible without massive refactoring shutdown code.
  // Luckily Android never performs a clean shutdown. So explicitly
  // ignore this problem.
  base::WaitableEvent never_signaled_;
#endif

  scoped_refptr<ResourceMessageFilter> resource_message_filter_;
  std::unique_ptr<FileSystemManagerImpl, BrowserThread::DeleteOnIOThread>
      file_system_manager_impl_;
  std::unique_ptr<viz::GpuClient, BrowserThread::DeleteOnIOThread> gpu_client_;
  std::unique_ptr<PushMessagingManager, BrowserThread::DeleteOnIOThread>
      push_messaging_manager_;

  std::unique_ptr<EmbeddedFrameSinkProviderImpl> embedded_frame_sink_provider_;
  std::unique_ptr<PluginRegistryImpl, BrowserThread::DeleteOnIOThread>
      plugin_registry_;

  mojom::ChildControlPtr child_control_interface_;
  mojom::RouteProviderAssociatedPtr remote_route_provider_;
  mojom::RendererAssociatedPtr renderer_interface_;
  mojo::AssociatedBinding<mojom::RendererHost> renderer_host_binding_;

  // Tracks active audio and video streams within the render process; used to
  // determine if if a process should be backgrounded.
  int media_stream_count_ = 0;

  std::unique_ptr<resource_coordinator::ProcessResourceCoordinator>
      process_resource_coordinator_;

  // A WeakPtrFactory which is reset every time Cleanup() runs. Used to vend
  // WeakPtrs which are invalidated any time the RPHI is recycled.
  std::unique_ptr<base::WeakPtrFactory<RenderProcessHostImpl>>
      instance_weak_factory_;

  FrameSinkProviderImpl frame_sink_provider_;
  std::unique_ptr<mojo::Binding<viz::mojom::CompositingModeReporter>>
      compositing_mode_reporter_;

  bool cleanup_corb_exception_for_plugin_upon_destruction_ = false;

  // Fields for recording MediaStream UMA.
  bool has_recorded_media_stream_frame_depth_metric_ = false;

  base::WeakPtrFactory<RenderProcessHostImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RenderProcessHostImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_PROCESS_HOST_IMPL_H_
