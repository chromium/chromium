// Copyright 2012 The Chromium Authors
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
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_factory.h"
#include "content/public/browser/storage_partition_config.h"
#include "ipc/ipc_test_sink.h"
#include "media/media_buildflags.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "net/base/network_isolation_key.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/child_process_binding_types.h"
#include "content/public/browser/android/child_process_importance.h"
#include "services/network/public/mojom/attribution.mojom-forward.h"
#endif

namespace blink {
class StorageKey;
}  // namespace blink

namespace content {

class MockRenderProcessHostFactory;
class ProcessLock;
class RenderProcessHostPriorityClient;
class SiteInfo;
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
  MockRenderProcessHost(BrowserContext* browser_context,
                        const StoragePartitionConfig& storage_partition_config,
                        bool is_for_guests_only);

  MockRenderProcessHost(const MockRenderProcessHost&) = delete;
  MockRenderProcessHost& operator=(const MockRenderProcessHost&) = delete;

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

  // Simulates async launch happening.
  void SimulateReady();

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
  void OnMediaStreamAdded() override;
  void OnMediaStreamRemoved() override;
  void OnForegroundServiceWorkerAdded() override;
  void OnForegroundServiceWorkerRemoved() override;
  void OnBoostForLoadingAdded() override;
  void OnBoostForLoadingRemoved() override;
  StoragePartition* GetStoragePartition() override;
  virtual void AddWord(const std::u16string& word);
  bool Shutdown(int exit_code) override;
  bool ShutdownRequested() override;
  bool FastShutdownIfPossible(size_t page_count,
                              bool skip_unload_handlers) override;
  bool FastShutdownStarted() override;
  const base::Process& GetProcess() override;
  bool IsReady() override;
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
  void SetSuddenTerminationAllowed(bool allowed) override;
  bool SuddenTerminationAllowed() override;
  BrowserContext* GetBrowserContext() override;
  bool InSameStoragePartition(StoragePartition* partition) override;
  IPC::ChannelProxy* GetChannel() override;
#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
  void AddFilter(BrowserMessageFilter* filter) override;
#endif
  base::TimeDelta GetChildProcessIdleTime() override;
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
  size_t GetWorkerRefCount() const;
  std::string GetKeepAliveDurations() const override;
  size_t GetShutdownDelayRefCount() const override;
  int GetRenderFrameHostCount() const override;
  void DisableRefCounts() override;
  void ForEachRenderFrameHost(
      base::FunctionRef<void(RenderFrameHost*)> on_render_frame_host) override;
  void RegisterRenderFrameHost(
      const GlobalRenderFrameHostId& render_frame_host_id,
      bool is_outermost_main_frame) override;
  void UnregisterRenderFrameHost(
      const GlobalRenderFrameHostId& render_frame_host_id,
      bool is_outermost_main_frame) override;
  void IncrementWorkerRefCount() override;
  void DecrementWorkerRefCount() override;
  void IncrementPendingReuseRefCount() override;
  void DecrementPendingReuseRefCount() override;
  bool AreRefCountsDisabled() override;
  mojom::Renderer* GetRendererInterface() override;

  bool MayReuseHost() override;
  bool IsUnused() override;
  void SetIsUsed() override;

  bool HostHasNotBeenUsed() override;
  bool IsSpare() const override;
  void SetProcessLock(const IsolationContext& isolation_context,
                      const ProcessLock& process_lock) override;
  ProcessLock GetProcessLock() const override;
  bool IsProcessLockedToSiteForTesting() override;
  void DelayProcessShutdown(const base::TimeDelta& subframe_shutdown_timeout,
                            const base::TimeDelta& unload_handler_timeout,
                            const SiteInfo& site_info) override {}
  void StopTrackingProcessForShutdownDelay() override {}
  void BindCacheStorage(
      const network::CrossOriginEmbedderPolicy&,
      mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>,
      const network::DocumentIsolationPolicy& document_isolation_policy,
      const storage::BucketLocator& bucket,
      mojo::PendingReceiver<blink::mojom::CacheStorage> receiver) override;
  void BindFileSystemManager(
      const blink::StorageKey& storage_key,
      mojo::PendingReceiver<blink::mojom::FileSystemManager> receiver)
      override {}
  void BindFileSystemAccessManager(
      const blink::StorageKey& storage_key,
      mojo::PendingReceiver<blink::mojom::FileSystemAccessManager> receiver)
      override {}
  void GetSandboxedFileSystemForBucket(
      const storage::BucketLocator& bucket,
      const std::vector<std::string>& directory_path_components,
      blink::mojom::FileSystemAccessManager::GetSandboxedFileSystemCallback
          callback) override;
  void BindIndexedDB(
      const blink::StorageKey& storage_key,
      BucketContext& bucket_context,
      mojo::PendingReceiver<blink::mojom::IDBFactory> receiver) override;
  void BindBucketManagerHost(
      base::WeakPtr<BucketContext> bucket_context,
      mojo::PendingReceiver<blink::mojom::BucketManagerHost> receiver)
      override {}
  void BindRestrictedCookieManagerForServiceWorker(
      const blink::StorageKey& storage_key,
      mojo::PendingReceiver<network::mojom::RestrictedCookieManager> receiver)
      override {}
  void BindVideoDecodePerfHistory(
      mojo::PendingReceiver<media::mojom::VideoDecodePerfHistory> receiver)
      override {}
  void BindQuotaManagerHost(
      const blink::StorageKey& storage_key,
      mojo::PendingReceiver<blink::mojom::QuotaManagerHost> receiver) override {
  }
#if BUILDFLAG(IS_FUCHSIA)
  void BindMediaCodecProvider(
      mojo::PendingReceiver<media::mojom::FuchsiaMediaCodecProvider> receiver)
      override {}
#endif
  void CreateLockManager(
      const blink::StorageKey& storage_key,
      mojo::PendingReceiver<blink::mojom::LockManager> receiver) override {}
  void CreateOneShotSyncService(
      const url::Origin& origin,
      mojo::PendingReceiver<blink::mojom::OneShotBackgroundSyncService>
          receiver) override {}
  void CreatePeriodicSyncService(
      const url::Origin& origin,
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
      GlobalRenderFrameHostId rfh_id,
      RenderProcessHost::NotificationServiceCreatorType creator_type,
      const blink::StorageKey& storage_key,
      mojo::PendingReceiver<blink::mojom::NotificationService> receiver)
      override {}
  void CreateWebSocketConnector(
      const blink::StorageKey& storage_key,
      mojo::PendingReceiver<blink::mojom::WebSocketConnector> receiver)
      override {}
#if BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
  void CreateStableVideoDecoder(
      mojo::PendingReceiver<media::stable::mojom::StableVideoDecoder> receiver)
      override {}
#endif  // BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)

  std::string GetInfoForBrowserContextDestructionCrashReporting() override;
  void WriteIntoTrace(perfetto::TracedProto<TraceProto> proto) const override;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void ReinitializeLogging(uint32_t logging_dest,
                           base::ScopedFD log_file_descriptor) override;
#endif

  void SetBatterySaverMode(bool battery_saver_mode_enabled) override {}
  uint64_t GetPrivateMemoryFootprint() override;

  void PauseSocketManagerForRenderFrameHost(
      const GlobalRenderFrameHostId& render_frame_host_id) override {}
  void ResumeSocketManagerForRenderFrameHost(
      const GlobalRenderFrameHostId& render_frame_host_id) override {}

  // IPC::Sender via RenderProcessHost.
  bool Send(IPC::Message* msg) override;

  // IPC::Listener via RenderProcessHost.
  bool OnMessageReceived(const IPC::Message& msg) override;
  void OnChannelConnected(int32_t peer_pid) override;

  void set_priority(base::Process::Priority priority) { priority_ = priority; }

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
  int id_;
  bool has_connection_;
  raw_ptr<BrowserContext, DanglingUntriaged> browser_context_;
  base::ObserverList<RenderProcessHostObserver> observers_;

  StoragePartitionConfig storage_partition_config_;
  base::flat_set<raw_ptr<RenderProcessHostPriorityClient, CtnExperimental>>
      priority_clients_;
  int prev_routing_id_;
  base::IDMap<IPC::Listener*> listeners_;
  bool shutdown_requested_;
  bool fast_shutdown_started_;
  bool within_process_died_observer_ = false;
  bool delayed_cleanup_ = false;
  bool deletion_callback_called_;
  bool is_for_guests_only_;
  base::Process::Priority priority_;
  bool is_unused_;
  bool is_ready_ = false;
  base::Process process;
  int worker_ref_count_;
  int pending_reuse_ref_count_;
  int foreground_service_worker_count_;
  std::unique_ptr<mojo::AssociatedRemote<mojom::Renderer>> renderer_interface_;
  std::map<std::string, InterfaceBinder> binder_overrides_;
  bool is_renderer_locked_to_site_ = false;
  std::set<GlobalRenderFrameHostId> render_frame_host_id_set_;
  mojo::PendingReceiver<blink::mojom::CacheStorage> cache_storage_receiver_;
  mojo::PendingReceiver<blink::mojom::IDBFactory> idb_factory_receiver_;
  base::WeakPtrFactory<MockRenderProcessHost> weak_ptr_factory_{this};
};

class MockRenderProcessHostFactory : public RenderProcessHostFactory {
 public:
  MockRenderProcessHostFactory();

  MockRenderProcessHostFactory(const MockRenderProcessHostFactory&) = delete;
  MockRenderProcessHostFactory& operator=(const MockRenderProcessHostFactory&) =
      delete;

  ~MockRenderProcessHostFactory() override;

  RenderProcessHost* CreateRenderProcessHost(
      BrowserContext* browser_context,
      SiteInstance* site_instance) override;

  // Removes the given MockRenderProcessHost from the MockRenderProcessHost
  // list.
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
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_MOCK_RENDER_PROCESS_HOST_H_
