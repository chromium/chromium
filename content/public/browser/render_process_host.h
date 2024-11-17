// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RENDER_PROCESS_HOST_H_
#define CONTENT_PUBLIC_BROWSER_RENDER_PROCESS_HOST_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/clang_profiling_buildflags.h"
#include "base/containers/heap_array.h"
#include "base/containers/id_map.h"
#include "base/functional/function_ref.h"
#include "base/memory/safety_checks.h"
#include "base/process/kill.h"
#include "base/process/process.h"
#include "base/supports_user_data.h"
#include "base/tracing/protos/chrome_track_event.pbzero.h"
#include "build/build_config.h"
#include "content/common/buildflags.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_exposed_isolation_level.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "media/media_buildflags.h"
#include "media/mojo/mojom/video_decode_perf_history.mojom-forward.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/network_isolation_key.h"
#include "services/network/public/mojom/cross_origin_embedder_policy.mojom-forward.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"
#include "services/network/public/mojom/restricted_cookie_manager.mojom-forward.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "third_party/blink/public/mojom/background_sync/background_sync.mojom.h"
#include "third_party/blink/public/mojom/buckets/bucket_manager_host.mojom-forward.h"
#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom-forward.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_manager.mojom.h"
#include "third_party/blink/public/mojom/filesystem/file_system.mojom-forward.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-forward.h"
#include "third_party/blink/public/mojom/locks/lock_manager.mojom-forward.h"
#include "third_party/blink/public/mojom/notifications/notification_service.mojom-forward.h"
#include "third_party/blink/public/mojom/payments/payment_app.mojom-forward.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-forward.h"
#include "third_party/blink/public/mojom/quota/quota_manager_host.mojom-forward.h"
#include "third_party/blink/public/mojom/websockets/websocket_connector.mojom-forward.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_proto.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"
#include "ui/gfx/native_widget_types.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/public/browser/android/child_process_importance.h"
#endif

#if BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
#include "media/mojo/mojom/stable/stable_video_decoder.mojom-forward.h"
#endif  // BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)

#if BUILDFLAG(IS_FUCHSIA)
#include "media/mojo/mojom/fuchsia_media.mojom-forward.h"
#endif

class GURL;

namespace base {
class PersistentMemoryAllocator;
class TimeDelta;
class Token;
#if BUILDFLAG(IS_ANDROID)
namespace android {
enum class ChildBindingState;
}
#endif
}  // namespace base

namespace blink {
class StorageKey;
}  // namespace blink

namespace IPC {
class ChannelProxy;
}  // namespace IPC

namespace network {
struct CrossOriginEmbedderPolicy;
struct DocumentIsolationPolicy;
}  // namespace network

namespace storage {
struct BucketLocator;
}

namespace url {
class Origin;
}  // namespace url

namespace content {
class BrowserContext;
class BucketContext;
class IsolationContext;
class ProcessLock;
class RenderFrameHost;
class RenderProcessHostObserver;
class RenderProcessHostPriorityClient;
class SiteInfo;
class StoragePartition;
struct GlobalRenderFrameHostId;
#if BUILDFLAG(IS_ANDROID)
enum class ChildProcessImportance;
#endif
#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
class BrowserMessageFilter;
#endif

namespace mojom {
class Renderer;
}  // namespace mojom

// Interface that represents the browser side of the browser <-> renderer
// communication channel. There will generally be one RenderProcessHost per
// renderer process.
class CONTENT_EXPORT RenderProcessHost : public IPC::Sender,
                                         public IPC::Listener,
                                         public base::SupportsUserData {
  // Do not remove this macro!
  // The macro is maintained by the memory safety team.
  ADVANCED_MEMORY_SAFETY_CHECKS();

 public:
  using iterator = base::IDMap<RenderProcessHost*>::iterator;

  // Crash reporting mode for ShutdownForBadMessage.
  enum class CrashReportMode {
    NO_CRASH_DUMP,
    GENERATE_CRASH_DUMP,
  };

  enum class NotificationServiceCreatorType {
    kDocument,
    kDedicatedWorker,
    kSharedWorker,
    kServiceWorker
  };

  // General functions ---------------------------------------------------------

  ~RenderProcessHost() override {}

  // Initialize the new renderer process, returning true on success. This must
  // be called once before the object can be used, but can be called after
  // that with no effect. Therefore, if the caller isn't sure about whether
  // the process has been created, it should just call Init().
  virtual bool Init() = 0;

  // Ensures that a Channel exists and is at least queueing outgoing messages
  // if there isn't a render process connected to it yet. This may be used to
  // ensure that in the event of a renderer crash and restart, subsequent
  // messages sent via Send() will eventually reach the new process.
  virtual void EnableSendQueue() = 0;

  // Gets the next available routing id.
  virtual int GetNextRoutingID() = 0;

  // These methods add or remove listener for a specific message routing ID.
  // Used for refcounting, each holder of this object must AddRoute and
  // RemoveRoute. This object should be allocated on the heap; when no
  // listeners own it any more, it will delete itself.
  virtual void AddRoute(int32_t routing_id, IPC::Listener* listener) = 0;
  virtual void RemoveRoute(int32_t routing_id) = 0;

  // Add and remove observers for lifecycle events. The order in which
  // notifications are sent to observers is undefined. Observers must be sure to
  // remove the observer before they go away.
  virtual void AddObserver(RenderProcessHostObserver* observer) = 0;
  virtual void RemoveObserver(RenderProcessHostObserver* observer) = 0;

  // Called when a received message cannot be decoded. Terminates the renderer.
  // Most callers should not call this directly, but instead should call
  // bad_message::BadMessageReceived() or an equivalent method outside of the
  // content module.
  //
  // If |crash_report_mode| is GENERATE_CRASH_DUMP, then a browser crash dump
  // will be reported as well.
  virtual void ShutdownForBadMessage(CrashReportMode crash_report_mode) = 0;

  // Recompute Priority state. RenderProcessHostPriorityClient should call this
  // when their individual priority changes.
  virtual void UpdateClientPriority(
      RenderProcessHostPriorityClient* client) = 0;

  // Number of visible (ie |!is_hidden|) RenderProcessHostPriorityClients.
  virtual int VisibleClientCount() = 0;

  // Get computed frame depth from RenderProcessHostPriorityClients.
  virtual unsigned int GetFrameDepth() = 0;

  // Get computed viewport intersection state from
  // RenderProcessHostPriorityClients.
  virtual bool GetIntersectsViewport() = 0;

  // Called when a video capture stream or an audio stream is added or removed
  // and used to determine if the process should be backgrounded or not.
  virtual void OnMediaStreamAdded() = 0;
  virtual void OnMediaStreamRemoved() = 0;

  // Called when a service worker is executing in the process and may need
  // to respond to events from other processes in a timely manner.  This is
  // used to determine if the process should be backgrounded or not.
  virtual void OnForegroundServiceWorkerAdded() = 0;
  virtual void OnForegroundServiceWorkerRemoved() = 0;

  // This is an experimental code that keeps the renderer process foregrounded
  // from CommitNavigation to DOMContentLoaded (crbug/351953350). This is used
  // to determine if the process should be backgrounded or not.
  virtual void OnBoostForLoadingAdded() = 0;
  virtual void OnBoostForLoadingRemoved() = 0;

  // Indicates whether the current RenderProcessHost is exclusively hosting
  // guest RenderFrames. Not all guest RenderFrames are created equal.  A guest,
  // as indicated by BrowserPluginGuest::IsGuest, may coexist with other
  // non-guest RenderFrames in the same process if IsForGuestsOnly() is false.
  virtual bool IsForGuestsOnly() = 0;

  // Indicates whether the current RenderProcessHost is running with JavaScript
  // JIT disabled.
  virtual bool IsJitDisabled() = 0;

  // Indicates whether the current RenderProcessHost is running with v8
  // optimizations disabled. This is distinct from IsJitDisabled() -
  // IsJitDisabled() disables all JIT compilation in the renderer, while
  // AreV8OptimizationsDisabled() only disables the higher-tier V8 optimizers,
  // leaving the basic JIT compiler in V8 (and the wasm JIT compiler) enabled.
  virtual bool AreV8OptimizationsDisabled() = 0;

  // Indicates whether the current RenderProcessHost exclusively hosts PDF
  // content.
  virtual bool IsPdf() = 0;

  // Returns the storage partition associated with this process.
  virtual StoragePartition* GetStoragePartition() = 0;

  // Terminate the associated renderer process without running unload handlers,
  // waiting for the process to exit, etc. If supported by the OS, set the
  // process exit code to |exit_code|. Returns false if the shutdown request
  // will not be dispatched. If called before
  // RenderProcessHostObserver::RenderProcessReady,
  // RenderProcessHostObserver::RenderProcessExited may never be called since
  // Shutdown() will race with child process startup.
  virtual bool Shutdown(int exit_code) = 0;

  // Returns true if shutdown was started by calling |Shutdown()|.
  virtual bool ShutdownRequested() = 0;

  // Try to shut down the associated renderer process as fast as possible.
  // If a non-zero |page_count| value is provided, then a fast shutdown will
  // only happen if the count matches the active view count. If
  // |skip_unload_handlers| is false and this renderer has any RenderViews with
  // unload handlers, then this function does nothing. Otherwise, the function
  // will ingnore checking for those handlers. Returns true if it was able to do
  // fast shutdown.
  virtual bool FastShutdownIfPossible(size_t page_count = 0,
                                      bool skip_unload_handlers = false) = 0;

  // Returns true if fast shutdown was started for the renderer.
  virtual bool FastShutdownStarted() = 0;

  // Returns the process object associated with the child process.  In certain
  // tests or single-process mode, this will actually represent the current
  // process.
  //
  // NOTE: this is not necessarily valid immediately after calling Init, as
  // Init starts the process asynchronously.  It's guaranteed to be valid after
  // the first IPC arrives or RenderProcessReady was called on a
  // RenderProcessHostObserver for this. At that point, IsReady() returns true.
  virtual const base::Process& GetProcess() = 0;

  // Returns whether the process is ready. The process is ready once both
  // conditions (which can happen in arbitrary order) are true:
  // 1- the launcher reported a successful launch
  // 2- the channel is connected.
  //
  // After that point, GetHandle() is valid, and deferred messages have been
  // sent.
  virtual bool IsReady() = 0;

  // Returns the user browser context associated with this renderer process.
  virtual content::BrowserContext* GetBrowserContext() = 0;

  // Returns whether this process is using the same StoragePartition as
  // |partition|.
  virtual bool InSameStoragePartition(StoragePartition* partition) = 0;

  // Returns the unique ID for this child process host. This can be used later
  // in a call to FromID() to get back to this object (this is used to avoid
  // sending non-threadsafe pointers to other threads).
  //
  // This ID will be unique across all child process hosts, including workers,
  // plugins, etc.
  //
  // This will never return ChildProcessHost::kInvalidUniqueID.
  virtual int GetID() const = 0;

  // Returns a SafeRef to `this`. It should only be used in non-owning cases,
  // where the caller is not expected to outlive `this`.
  // This method is public so that it can be called from within //content, and
  // used by MockRenderProcessHost. It isn't meant to be called outside of
  // //content.
  virtual base::SafeRef<RenderProcessHost> GetSafeRef() const = 0;

  // Returns true iff the Init() was called and the process hasn't died yet.
  //
  // Note that even if IsInitializedAndNotDead() returns true, then (for a short
  // duration after calling Init()) the process might not be fully spawned
  // *yet*.  For example - IsReady() might return false and GetProcess() might
  // still return an invalid process with a null handle.
  virtual bool IsInitializedAndNotDead() = 0;

  // Returns true iff the decision has been made to delete `this`.
  //
  // If this returns true, then no new child processes will be associated
  // with `this`.
  virtual bool IsDeletingSoon() = 0;

  // Returns the renderer channel.
  virtual IPC::ChannelProxy* GetChannel() = 0;

#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
  // Adds a message filter to the IPC channel.
  virtual void AddFilter(BrowserMessageFilter* filter) = 0;
#endif

  // Sets whether this render process is blocked. This means that input events
  // should not be sent to it, nor other timely signs of life expected from it.
  virtual void SetBlocked(bool blocked) = 0;
  virtual bool IsBlocked() = 0;

  using BlockStateChangedCallbackList = base::RepeatingCallbackList<void(bool)>;
  using BlockStateChangedCallback = BlockStateChangedCallbackList::CallbackType;
  virtual base::CallbackListSubscription RegisterBlockStateChangedCallback(
      const BlockStateChangedCallback& cb) = 0;

  // Schedules the host for deletion and removes it from the all_hosts list.
  virtual void Cleanup() = 0;

  // Track the count of pending views that are being swapped back in.  Called
  // by listeners to register and unregister pending views to prevent the
  // process from exiting.
  virtual void AddPendingView() = 0;
  virtual void RemovePendingView() = 0;

  // Adds and removes priority clients.
  virtual void AddPriorityClient(
      RenderProcessHostPriorityClient* priority_client) = 0;
  virtual void RemovePriorityClient(
      RenderProcessHostPriorityClient* priority_client) = 0;

#if !BUILDFLAG(IS_ANDROID)
  // Sets a process priority override. This overrides the entire built-in
  // priority setting mechanism for the process.
  // TODO(pmonette): Make this work well on Android.
  virtual void SetPriorityOverride(base::Process::Priority priority) = 0;
  virtual bool HasPriorityOverride() = 0;
  virtual void ClearPriorityOverride() = 0;
#endif

#if BUILDFLAG(IS_ANDROID)
  // Return the highest importance of all widgets in this process.
  virtual ChildProcessImportance GetEffectiveImportance() = 0;

  // Return the highest binding this process has.
  virtual base::android::ChildBindingState GetEffectiveChildBindingState() = 0;

  // Dumps the stack of this render process without crashing it.
  virtual void DumpProcessStack() = 0;
#endif

  virtual void PauseSocketManagerForRenderFrameHost(
      const GlobalRenderFrameHostId& render_frame_host_id) = 0;
  virtual void ResumeSocketManagerForRenderFrameHost(
      const GlobalRenderFrameHostId& render_frame_host_id) = 0;

  // Sets a flag indicating that the process can be abnormally terminated.
  virtual void SetSuddenTerminationAllowed(bool allowed) = 0;
  // Returns true if the process can be abnormally terminated.
  virtual bool SuddenTerminationAllowed() = 0;

  // Returns how long the child has been idle. The definition of idle
  // depends on when a derived class calls mark_child_process_activity_time().
  // This is a rough indicator and its resolution should not be better than
  // 10 milliseconds.
  virtual base::TimeDelta GetChildProcessIdleTime() = 0;

  // Checks that the given renderer is allowed to request `url`; if not, `url`
  // will be set to "about:blank#blocked".
  //
  // `empty_allowed` must be `false` when filtering URLs for navigations. The
  // browser typically treats a navigation to an empty URL as a navigation to
  // the home page, but this is often a privileged page, e.g. chrome://newtab/,
  // which is a security problem that this method is specifically trying to
  // block.
  //
  // This method return whether or not the URL was blocked so that callers can
  // distinguish between the blocked case and a literal request to navigate to
  // "about:blank#blocked".
  enum class FilterURLResult {
    kAllowed,
    kBlocked,
  };
  virtual FilterURLResult FilterURL(bool empty_allowed, GURL* url) = 0;

  virtual void EnableAudioDebugRecordings(const base::FilePath& file) = 0;
  virtual void DisableAudioDebugRecordings() = 0;

  using WebRtcRtpPacketCallback =
      base::RepeatingCallback<void(base::HeapArray<uint8_t> packet_header,
                                   size_t packet_length,
                                   bool incoming)>;

  using WebRtcStopRtpDumpCallback =
      base::OnceCallback<void(bool incoming, bool outgoing)>;

  // Starts passing RTP packets to |packet_callback| and returns the callback
  // used to stop dumping.
  virtual WebRtcStopRtpDumpCallback StartRtpDump(
      bool incoming,
      bool outgoing,
      WebRtcRtpPacketCallback packet_callback) = 0;

  // Asks the renderer process to bind |receiver|. |receiver| arrives in the
  // renderer process and is carried through the following flow, stopping if any
  // step decides to bind it:
  //
  //   1. IO thread, |ChildProcessImpl::BindReceiver()| (child_thread_impl.cc)
  //   2. IO thread ,|ContentClient::BindChildProcessInterface()|
  //   3. Main thread, |ChildThreadImpl::OnBindReceiver()| (virtual)
  //   4. Possibly more steps, depending on the ChildThreadImpl subclass.
  virtual void BindReceiver(mojo::GenericPendingReceiver receiver) = 0;

  // Extracts any persistent-memory-allocator used for renderer metrics.
  // Ownership is passed to the caller. To support sharing of histogram data
  // between the Renderer and the Browser, the allocator is created when the
  // process is created and later retrieved by the SubprocessMetricsProvider
  // for management.
  virtual std::unique_ptr<base::PersistentMemoryAllocator>
  TakeMetricsAllocator() = 0;

  // Returns the time of the last call to Init that was completed successfully
  // (after a new renderer process was created); further calls to Init would
  // change this value only when they caused the new process to be created after
  // a crash.
  virtual const base::TimeTicks& GetLastInitTime() = 0;

  // Returns the priority of this process.
  virtual base::Process::Priority GetPriority() = 0;

  // Returns a list of durations for active KeepAlive requests.
  // For debugging only. TODO(wjmaclean): Remove once the causes behind
  // https://crbug.com/1148542 are known.
  virtual std::string GetKeepAliveDurations() const = 0;

  // Returns the number of active Shutdown-Delay requests.
  // For debugging only. TODO(wjmaclean): Remove once the causes behind
  // https://crbug.com/1148542 are known.
  virtual size_t GetShutdownDelayRefCount() const = 0;

  // Diagnostic code for https://crbug/1148542. This will be removed prior to
  // resolving that issue. It counts all RenderFrameHosts that have not been
  // destroyed, including speculative ones and pending deletion ones. This
  // is included to allow MockRenderProcessHost to override them, and should not
  // be called from outside of content/.
  virtual int GetRenderFrameHostCount() const = 0;

  // Calls |on_frame| for every RenderFrameHost whose frames live in this
  // process. Note that speculative RenderFrameHosts will be skipped.
  virtual void ForEachRenderFrameHost(
      base::FunctionRef<void(RenderFrameHost*)> on_frame) = 0;

  // Register/unregister a RenderFrameHost instance whose frame lives in this
  // process. RegisterRenderFrameHost and UnregisterRenderFrameHost are the
  // implementation details and should be called only from within //content.
  virtual void RegisterRenderFrameHost(
      const GlobalRenderFrameHostId& render_frame_host_id,
      bool is_outermost_main_frame) = 0;
  virtual void UnregisterRenderFrameHost(
      const GlobalRenderFrameHostId& render_frame_host_id,
      bool is_outermost_main_frame) = 0;

  // "Worker ref count" is similar to "Keep alive ref count", but is specific to
  // workers since they do not have pre-defined timeouts. Also affected by
  // DisableRefCounts() in the same manner as for
  // Increment/DecrementKeepAliveRefCount() functions.
  //
  // List of users:
  //  - Service Worker:
  //    While there are service workers who live in this process, they wish
  //    the renderer process to be alive. The ref count is incremented when this
  //    process is allocated to the worker, and decremented when worker's
  //    shutdown sequence is completed.
  //  - Shared Worker:
  //    While there are shared workers who live in this process, they wish
  //    the renderer process to be alive. The ref count is incremented when
  //    a shared worker is created in the process, and decremented when
  //    it is terminated (it self-destructs when it no longer has clients).
  //  - Shared Storage Worklet:
  //    (https://github.com/pythagoraskitty/shared-storage)
  //    While there are shared storage worklets who live in this process, they
  //    wish the renderer process to be alive. The ref count is incremented when
  //    a shared storage worklet is created in the process, and decremented when
  //    it is terminated (after the document is destroyed, the worklet
  //    will be destroyed when all pending operations have finished or a timeout
  //    is reached). Note that this is only relevant for the implementation of
  //    shared storage worklets that run in the renderer process. The actual
  //    process that the shared storage worklets are going to use is determined
  //    by the concrete implementation of
  //    `SharedStorageRenderThreadWorkletDriver`.
  //  - Auction Worklets (on Android only):
  //    (https://github.com/WICG/turtledove/blob/main/FLEDGE.md)
  //    Keeps the renderer alive if there are any worklets using it.
  virtual void IncrementWorkerRefCount() = 0;
  virtual void DecrementWorkerRefCount() = 0;

  // "Pending reuse" ref count may be used to keep a process alive because we
  // know that it will be reused soon.  Unlike the keep alive ref count, it is
  // not time-based, and unlike the worker ref count above, it is not used for
  // workers.  It is intentionally kept separate from the other ref counts to
  // ease debugging, so that it's easier to tell what kept a particular process
  // alive.
  virtual void IncrementPendingReuseRefCount() = 0;
  virtual void DecrementPendingReuseRefCount() = 0;

  // Sets all the various process lifetime ref counts to zero (e.g., keep alive,
  // worker, etc). Called when the browser context will be destroyed so this
  // RenderProcessHost can immediately die.
  //
  // After this is called, the Increment/DecrementKeepAliveRefCount() functions
  // and Increment/DecrementWorkerRefCount() functions must not be called.
  virtual void DisableRefCounts() = 0;

  // Returns true if DisableRefCounts() was called.
  virtual bool AreRefCountsDisabled() = 0;

  // Acquires the |mojom::Renderer| interface to the render process. This is for
  // internal use only, and is only exposed here to support
  // MockRenderProcessHost usage in tests.
  virtual mojom::Renderer* GetRendererInterface() = 0;

  // Whether this process is locked out from ever being reused for sites other
  // than the ones it currently has.
  virtual bool MayReuseHost() = 0;

  // Indicates whether this RenderProcessHost is "unused".  This starts out as
  // true for new processes and becomes false after one of the following:
  // (1) This process commits any page.
  // (2) This process is given to a SiteInstance that already has a site
  //     assigned.
  // Note that a process hosting ServiceWorkers will be implicitly handled by
  // (2) during ServiceWorker initialization, and SharedWorkers will be handled
  // by (1) since a page needs to commit before it can create a SharedWorker.
  //
  // While a process is unused, it is still suitable to host a URL that
  // requires a dedicated process.
  virtual bool IsUnused() = 0;
  virtual void SetIsUsed() = 0;

  // Return true if the host has not been used. This is stronger than IsUnused()
  // in that it checks if this RPH has ever been used to render at all, rather
  // than just no being suitable to host a URL that requires a dedicated
  // process.
  // TODO(alexmos): can this be unified with IsUnused()? See also
  // crbug.com/738634.
  virtual bool HostHasNotBeenUsed() = 0;

  // Returns true if this is a spare RenderProcessHost.
  virtual bool IsSpare() const = 0;

  // Locks this RenderProcessHost to documents compatible with |process_lock|.
  // This method is public so that it can be called from within //content, and
  // used by MockRenderProcessHost. It isn't meant to be called outside of
  // //content.
  virtual void SetProcessLock(const IsolationContext& isolation_context,
                              const ProcessLock& process_lock) = 0;

  // Returns the ProcessLock associated with this process.
  // This method is public so that it can be called from within //content, and
  // used by MockRenderProcessHost. It isn't meant to be called outside of
  // //content.
  virtual ProcessLock GetProcessLock() const = 0;

  // Returns true if this process is locked to a particular site-specific
  // ProcessLock.  See the SetProcessLock() call above.
  virtual bool IsProcessLockedToSiteForTesting() = 0;

  // The following several methods are for internal use only, and are only
  // exposed here to support MockRenderProcessHost usage in tests.
  virtual void DelayProcessShutdown(
      const base::TimeDelta& subframe_shutdown_timeout,
      const base::TimeDelta& unload_handler_timeout,
      const SiteInfo& site_info) = 0;
  virtual void StopTrackingProcessForShutdownDelay() = 0;
  virtual void BindCacheStorage(
      const network::CrossOriginEmbedderPolicy& cross_origin_embedder_policy,
      mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
          coep_reporter_remote,
      const network::DocumentIsolationPolicy& document_isolation_policy,
      const storage::BucketLocator& bucket_locator,
      mojo::PendingReceiver<blink::mojom::CacheStorage> receiver) = 0;
  virtual void BindFileSystemManager(
      const blink::StorageKey& storage_key,
      mojo::PendingReceiver<blink::mojom::FileSystemManager> receiver) = 0;
  virtual void BindFileSystemAccessManager(
      const blink::StorageKey& storage_key,
      mojo::PendingReceiver<blink::mojom::FileSystemAccessManager>
          receiver) = 0;
  virtual void GetSandboxedFileSystemForBucket(
      const storage::BucketLocator& bucket_locator,
      const std::vector<std::string>& directory_path_components,
      blink::mojom::FileSystemAccessManager::GetSandboxedFileSystemCallback
          callback) = 0;
  virtual void BindIndexedDB(
      const blink::StorageKey& storage_key,
      BucketContext& bucket_context,
      mojo::PendingReceiver<blink::mojom::IDBFactory> receiver) = 0;
  virtual void BindBucketManagerHost(
      base::WeakPtr<BucketContext> bucket_context,
      mojo::PendingReceiver<blink::mojom::BucketManagerHost> receiver) = 0;
  virtual void BindRestrictedCookieManagerForServiceWorker(
      const blink::StorageKey& storage_key,
      mojo::PendingReceiver<network::mojom::RestrictedCookieManager>
          receiver) = 0;
  virtual void BindVideoDecodePerfHistory(
      mojo::PendingReceiver<media::mojom::VideoDecodePerfHistory> receiver) = 0;
#if BUILDFLAG(IS_FUCHSIA)
  virtual void BindMediaCodecProvider(
      mojo::PendingReceiver<media::mojom::FuchsiaMediaCodecProvider>
          receiver) = 0;
#endif
  virtual void CreateOneShotSyncService(
      const url::Origin& origin,
      mojo::PendingReceiver<blink::mojom::OneShotBackgroundSyncService>
          receiver) = 0;
  virtual void CreatePeriodicSyncService(
      const url::Origin& origin,
      mojo::PendingReceiver<blink::mojom::PeriodicBackgroundSyncService>
          receiver) = 0;
  virtual void BindQuotaManagerHost(
      const blink::StorageKey& storage_key,
      mojo::PendingReceiver<blink::mojom::QuotaManagerHost> receiver) = 0;
  virtual void CreateLockManager(
      const blink::StorageKey& storage_key,
      mojo::PendingReceiver<blink::mojom::LockManager> receiver) = 0;
  virtual void CreatePermissionService(
      const url::Origin& origin,
      mojo::PendingReceiver<blink::mojom::PermissionService> receiver) = 0;
  virtual void CreatePaymentManagerForOrigin(
      const url::Origin& origin,
      mojo::PendingReceiver<payments::mojom::PaymentManager> receiver) = 0;
  // `rfh_id` is the id for RenderFrameHost for the `receiver` if
  // the notification service is created by a document, or the id for the
  // ancestor RenderFrameHost of the worker if the notification service is
  // created by a dedicated worker, or empty value otherwise.
  virtual void CreateNotificationService(
      GlobalRenderFrameHostId rfh_id,
      NotificationServiceCreatorType creator_type,
      const blink::StorageKey& storage_key,
      mojo::PendingReceiver<blink::mojom::NotificationService> receiver) = 0;
  virtual void CreateWebSocketConnector(
      const blink::StorageKey& storage_key,
      mojo::PendingReceiver<blink::mojom::WebSocketConnector> receiver) = 0;

#if BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
  virtual void CreateStableVideoDecoder(
      mojo::PendingReceiver<media::stable::mojom::StableVideoDecoder>
          receiver) = 0;
#endif  // BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)

  // Returns the current number of active views in this process.  Excludes
  // any RenderViewHosts that are swapped out.
  size_t GetActiveViewCount();

  // Returns the cross-origin isolation mode used by content in this process.
  //
  // Unlike WebExposedIsolationInfo, this is not guaranteed to be the same for
  // all processes in a BrowsingInstance; frames that are not delegated the
  // "cross-origin-isolated" permissions policy will have a kNotIsolated
  // isolation level, even if their WebExposedIsolationInfo is isolated.
  // Additionally, content that is cross-origin to a kIsolatedApplication main
  // frame will return kIsolated, as the application isolation level cannot be
  // inherited cross-origin.
  //
  // RenderFrameHost::GetWebExposedIsolationLevel() should typically be used
  // instead of this function if running in the context a frame so that
  // Permissions Policy can be taken into account. This function should be used
  // in contexts that don't have an associated frame like shared/service
  // workers. Once Permissions Policy applies to workers, a worker-specific
  // API to access isolation capability may need to be introduced which should
  // be used instead of this.
  //
  // Note that the embedder can force-enable APIs in frames even if they
  // lack the necessary privilege. This function doesn't account for that;
  // use content::IsIsolatedContext(RenderProcessHost*) to handle this case.
  WebExposedIsolationLevel GetWebExposedIsolationLevel();

  // Posts |task|, if this RenderProcessHost is ready or when it becomes ready
  // (see RenderProcessHost::IsReady method).  The |task| might not run at all
  // (e.g. if |render_process_host| is destroyed before becoming ready).  This
  // function can only be called on the browser's UI thread (and the |task| will
  // be posted back on the UI thread).
  void PostTaskWhenProcessIsReady(base::OnceClosure task);

  // Forces the renderer process to crash ASAP.
  virtual void ForceCrash() {}

  // Returns a string that contains information useful for debugging
  // crashes related to RenderProcessHost objects staying alive longer than
  // the BrowserContext they are associated with.
  virtual std::string GetInfoForBrowserContextDestructionCrashReporting() = 0;

  using TraceProto = perfetto::protos::pbzero::RenderProcessHost;
  // Write a representation of this object into a trace.
  virtual void WriteIntoTrace(
      perfetto::TracedProto<TraceProto> proto) const = 0;

#if BUILDFLAG(CLANG_PROFILING_INSIDE_SANDBOX)
  // Ask the renderer process to dump its profiling data to disk. Invokes
  // |callback| once this has completed.
  virtual void DumpProfilingData(base::OnceClosure callback) {}
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Reinitializes the child process's logging with the given settings. This
  // is needed on Chrome OS, which switches to a log file in the user's home
  // directory once they log in.
  virtual void ReinitializeLogging(uint32_t logging_dest,
                                   base::ScopedFD log_file_descriptor) = 0;
#endif

  // Asks the renderer process to prioritize energy efficiency because the
  // embedder is in battery saver mode. This signal is propagated to blink and
  // v8. The default state is `false`, meaning the power/speed tuning is left up
  // to the different components to figure out.
  virtual void SetBatterySaverMode(bool battery_saver_mode_enabled) = 0;

  // Return the memory usage of this process. On Android this value is
  // provided by the renderer periodically. On other platforms this value is
  // read from the OS but is cached for a short duration so we don't incur
  // a cost on every call.
  virtual uint64_t GetPrivateMemoryFootprint() = 0;

  // Static management functions -----------------------------------------------

  // Flag to run the renderer in process.  This is primarily
  // for debugging purposes.  When running "in process", the
  // browser maintains a single RenderProcessHost which communicates
  // to a RenderProcess which is instantiated in the same process
  // with the Browser.  All IPC between the Browser and the
  // Renderer is the same, it's just not crossing a process boundary.
  static bool run_renderer_in_process();

  // This also calls out to ContentBrowserClient::GetApplicationLocale and
  // modifies the current process' command line.
  // NOTE: This function is fundamentally unsafe and *should not be used*. By
  // the time a ContentBrowserClient exists in test fixtures, it is already
  // unsafe to modify the command-line. The command-line should only be modified
  // from SetUpCommandLine, which is before the ContentClient is created. See
  // crbug.com/1197147
  static void SetRunRendererInProcess(bool value);

  // This forces a renderer that is running "in process" to shut down.
  static void ShutDownInProcessRenderer();

  // Allows iteration over all the RenderProcessHosts in the browser. Note
  // that each host may not be active, and therefore may have nullptr channels.
  static iterator AllHostsIterator();

  // Returns the RenderProcessHost given its ID.  Returns nullptr if the ID does
  // not correspond to a live RenderProcessHost.
  static RenderProcessHost* FromID(int render_process_id);

  // Returns the RenderProcessHost given its renderer's service instance ID,
  // generated randomly when launching the renderer. Returns nullptr if the
  // instance does not correspond to a live RenderProcessHost.
  static RenderProcessHost* FromRendererInstanceId(
      const base::Token& instance_id);

  // Returns true if the process limit is reached. In that case, site instances
  // will be assigned to an existing process host instead of a new one when
  // possible (see MayReuseAndIsSuitable).
  static bool IsProcessLimitReached();

  // Overrides the default heuristic for limiting the max renderer process
  // count.  This is useful for unit testing process limit behaviors.  It is
  // also used to allow a command line parameter to configure the max number of
  // renderer processes and should only be called once during startup.
  // A value of zero means to use the default heuristic.
  static void SetMaxRendererProcessCount(size_t count);

  // Returns the current maximum number of renderer process hosts kept by the
  // content module.
  static size_t GetMaxRendererProcessCount();

  // TODO(siggi): Remove once https://crbug.com/806661 is resolved.
  using AnalyzeHungRendererFunction = void (*)(const base::Process& renderer);
  static void SetHungRendererAnalysisFunction(
      AnalyzeHungRendererFunction analyze_hung_renderer);

  // Counts current RenderProcessHost(s), ignoring all spare processes.
  static int GetCurrentRenderProcessCountForTesting();

  // Allows tests to override host interface binding behavior. Any interface
  // binding request which would normally pass through the RPH's internal
  // IOThreadHostImpl::BindHostReceiver() will pass through |callback| first if
  // non-null. |callback| is only called from the IO thread.
  using BindHostReceiverInterceptor =
      base::RepeatingCallback<void(int render_process_id,
                                   mojo::GenericPendingReceiver* receiver)>;
  static void InterceptBindHostReceiverForTesting(
      BindHostReceiverInterceptor callback);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RENDER_PROCESS_HOST_H_
