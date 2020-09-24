// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_MOCK_RENDER_PROCESS_HOST_H_
#define CONTENT_PUBLIC_TEST_MOCK_RENDER_PROCESS_HOST_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_factory.h"
#include "ipc/ipc_test_sink.h"
#include "media/media_buildflags.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/network_isolation_key.h"

#if defined(OS_ANDROID)
#include "content/public/browser/android/child_process_importance.h"
#endif

namespace network {
namespace mojom {

class URLLoaderFactory;

}  // namespace mojom
}  // namespace network

namespace content {

class MockRenderProcessHostFactory;
class SiteInstance;
class StoragePartition;

// A mock render process host that has no corresponding renderer process.  All
// IPC messages are sent into the message sink for inspection by tests.
class MockRenderProcessHost : public RenderProcessHost {
 public:
  using InterfaceBinder =
      base::RepeatingCallback<void(mojo::ScopedMessagePipeHandle)>;

  explicit MockRenderProcessHost(BrowserContext* browser_context,
                                 bool is_for_guests_only = false);
  ~MockRenderProcessHost() override;

  // Provides access to all IPC messages that would have been sent to the
  // renderer via this RenderProcessHost.
  IPC::TestSink& sink() { return sink_; }

  // Provides test access to how many times a bad message has been received.
  int bad_msg_count() const { return bad_msg_count_; }

  // Provides tests a way to simulate this render process crashing.
  void SimulateCrash();
  void SimulateRenderProcessExit(base::TerminationStatus termination_status,
                                 int exit_code);

  using CreateNetworkFactoryCallback = base::RepeatingCallback<void(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
      int process_id,
      mojo::PendingRemote<network::mojom::URLLoaderFactory> original_factory)>;
  static void SetNetworkFactory(
      const CreateNetworkFactoryCallback& url_loader_factory_callback);

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
  void UpdateFrameWithPriority(
      base::Optional<FramePriority> previous_priority,
      base::Optional<FramePriority> new_priority) override;
  int VisibleClientCount() override;
  unsigned int GetFrameDepth() override;
  bool GetIntersectsViewport() override;
  bool IsForGuestsOnly() override;
  void OnMediaStreamAdded() override;
  void OnMediaStreamRemoved() override;
  void OnForegroundServiceWorkerAdded() override;
  void OnForegroundServiceWorkerRemoved() override;
  StoragePartition* GetStoragePartition() override;
  virtual void AddWord(const base::string16& word);
  bool Shutdown(int exit_code) override;
  bool ShutdownRequested() override;
  bool FastShutdownIfPossible(size_t page_count,
                              bool skip_unload_handlers) override;
  bool FastShutdownStarted() override;
  const base::Process& GetProcess() override;
  bool IsReady() override;
  int GetID() override;
  bool IsInitializedAndNotDead() override;
  void SetBlocked(bool blocked) override;
  bool IsBlocked() override;
  std::unique_ptr<BlockStateChangedCallbackList::Subscription>
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
  void SetSuddenTerminationAllowed(bool allowed) override;
  bool SuddenTerminationAllowed() override;
  BrowserContext* GetBrowserContext() override;
  bool InSameStoragePartition(StoragePartition* partition) override;
  IPC::ChannelProxy* GetChannel() override;
  void AddFilter(BrowserMessageFilter* filter) override;
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
  size_t GetKeepAliveRefCount() const;
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
      const network::CrossOriginEmbedderPolicy&,
      mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>,
      const url::Origin& origin,
      mojo::PendingReceiver<blink::mojom::CacheStorage> receiver) override;
  void BindFileSystemManager(
      const url::Origin& origin,
      mojo::PendingReceiver<blink::mojom::FileSystemManager> receiver)
      override {}
  void BindNativeFileSystemManager(
      const url::Origin& origin,
      mojo::PendingReceiver<blink::mojom::NativeFileSystemManager> receiver)
      override {}
  void BindIndexedDB(
      const url::Origin& origin,
      mojo::PendingReceiver<blink::mojom::IDBFactory> receiver) override;
  void BindRestrictedCookieManagerForServiceWorker(
      const url::Origin& origin,
      mojo::PendingReceiver<network::mojom::RestrictedCookieManager> receiver)
      override {}
  void BindVideoDecodePerfHistory(
      mojo::PendingReceiver<media::mojom::VideoDecodePerfHistory> receiver)
      override {}
  void BindQuotaManagerHost(
      int render_frame_id,
      const url::Origin& origin,
      mojo::PendingReceiver<blink::mojom::QuotaManagerHost> receiver) override {
  }
  void CreateLockManager(
      int render_frame_id,
      const url::Origin& origin,
      mojo::PendingReceiver<blink::mojom::LockManager> receiver) override {}
  void CreateOneShotSyncService(
      mojo::PendingReceiver<blink::mojom::OneShotBackgroundSyncService>
          receiver) override {}
  void CreatePeriodicSyncService(
      mojo::PendingReceiver<blink::mojom::PeriodicBackgroundSyncService>
          receiver) override {}
  void CreatePermissionService(
      const url::Origin& origin,
      mojo::PendingReceiver<blink::mojom::PermissionService> receiver)
      override {}
  void CreatePaymentManagerForOrigin(
      const url::Origin& origin,
      mojo::PendingReceiver<payments::mojom::PaymentManager> receiver)
      override {}
  void CreateNotificationService(
      const url::Origin& origin,
      mojo::PendingReceiver<blink::mojom::NotificationService> receiver)
      override {}
  void CreateWebSocketConnector(
      const url::Origin& origin,
      mojo::PendingReceiver<blink::mojom::WebSocketConnector> receiver)
      override {}

  void CleanupNetworkServicePluginExceptionsUponDestruction() override;
  std::string GetInfoForBrowserContextDestructionCrashReporting() override;

  // IPC::Sender via RenderProcessHost.
  bool Send(IPC::Message* msg) override;

  // IPC::Listener via RenderProcessHost.
  bool OnMessageReceived(const IPC::Message& msg) override;
  void OnChannelConnected(int32_t peer_pid) override;

  // Attaches the factory object so we can remove this object in its destructor
  // and prevent MockRenderProcessHostFacotry from deleting it.
  void SetFactory(const MockRenderProcessHostFactory* factory) {
    factory_ = factory;
  }

  void set_is_process_backgrounded(bool is_process_backgrounded) {
    is_process_backgrounded_ = is_process_backgrounded;
  }

  void SetProcess(base::Process&& new_process) {
    process = std::move(new_process);
  }

  void OverrideBinderForTesting(const std::string& interface_name,
                                const InterfaceBinder& binder);

  void OverrideRendererInterfaceForTesting(
      std::unique_ptr<mojo::AssociatedRemote<mojom::Renderer>>
          renderer_interface);

  bool is_renderer_locked_to_site() const {
    return is_renderer_locked_to_site_;
  }

  int foreground_service_worker_count() const {
    return foreground_service_worker_count_;
  }

 private:
  // Stores IPC messages that would have been sent to the renderer.
  IPC::TestSink sink_;
  int bad_msg_count_;
  const MockRenderProcessHostFactory* factory_;
  int id_;
  bool has_connection_;
  BrowserContext* browser_context_;
  base::ObserverList<RenderProcessHostObserver>::Unchecked observers_;

  base::flat_set<PriorityClient*> priority_clients_;
  int prev_routing_id_;
  base::IDMap<IPC::Listener*> listeners_;
  bool shutdown_requested_;
  bool fast_shutdown_started_;
  bool deletion_callback_called_;
  bool is_for_guests_only_;
  bool is_process_backgrounded_;
  bool is_unused_;
  base::Process process;
  int keep_alive_ref_count_;
  int foreground_service_worker_count_;
  std::unique_ptr<mojo::AssociatedRemote<mojom::Renderer>> renderer_interface_;
  std::map<std::string, InterfaceBinder> binder_overrides_;
  bool is_renderer_locked_to_site_ = false;
  std::unique_ptr<network::mojom::URLLoaderFactory> url_loader_factory_;
  mojo::PendingReceiver<blink::mojom::CacheStorage> cache_storage_receiver_;
  mojo::PendingReceiver<blink::mojom::IDBFactory> idb_factory_receiver_;
  base::WeakPtrFactory<MockRenderProcessHost> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MockRenderProcessHost);
};

class MockRenderProcessHostFactory : public RenderProcessHostFactory {
 public:
  MockRenderProcessHostFactory();
  ~MockRenderProcessHostFactory() override;

  RenderProcessHost* CreateRenderProcessHost(
      BrowserContext* browser_context,
      SiteInstance* site_instance) override;

  // Removes the given MockRenderProcessHost from the MockRenderProcessHost list
  // without deleting it. When a test deletes a MockRenderProcessHost, we need
  // to remove it from |processes_| to prevent it from being deleted twice.
  void Remove(MockRenderProcessHost* host) const;

  // Retrieve the current list of mock processes.
  std::vector<std::unique_ptr<MockRenderProcessHost>>* GetProcesses() {
    return &processes_;
  }

 private:
  // A list of MockRenderProcessHosts created by this object. This list is used
  // for deleting all MockRenderProcessHosts that have not deleted by a test in
  // the destructor and prevent them from being leaked.
  mutable std::vector<std::unique_ptr<MockRenderProcessHost>> processes_;

  // A mock URLLoaderFactory which just fails to create a loader.
  std::unique_ptr<network::mojom::URLLoaderFactory>
      default_mock_url_loader_factory_;

  DISALLOW_COPY_AND_ASSIGN(MockRenderProcessHostFactory);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_MOCK_RENDER_PROCESS_HOST_H_
