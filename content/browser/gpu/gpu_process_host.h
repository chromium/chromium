// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_GPU_PROCESS_HOST_H_
#define CONTENT_BROWSER_GPU_GPU_PROCESS_HOST_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/atomicops.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/viz/host/gpu_host_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_child_process_host_delegate.h"
#include "content/public/browser/gpu_data_manager.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/shm_count.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_mode.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"
#include "services/viz/privileged/mojom/compositing/frame_sink_manager.mojom.h"
#include "services/viz/privileged/mojom/gl/gpu_host.mojom.h"
#include "services/viz/privileged/mojom/gl/gpu_service.mojom.h"
#include "services/viz/privileged/mojom/viz_main.mojom.h"
#include "ui/gfx/gpu_extra_info.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include "services/viz/privileged/mojom/gl/info_collection_gpu_service.mojom.h"
#endif

namespace base {
class Thread;
}

namespace content {
class BrowserChildProcessHostImpl;

#if BUILDFLAG(IS_MAC)
class BrowserChildProcessBackgroundedBridge;
class CATransactionGPUCoordinator;
#endif

class GpuProcessHost : public BrowserChildProcessHostDelegate,
                       public viz::GpuHostImpl::Delegate {
 public:
  static int GetGpuCrashCount();

  // Creates a new GpuProcessHost (if |force_create| is turned on) or gets an
  // existing one, resulting in the launching of a GPU process if required.
  // Returns null on failure. It is not safe to store the pointer once control
  // has returned to the message loop as it can be destroyed. Instead store the
  // associated GPU host ID.  This could return NULL if GPU access is not
  // allowed (blocklisted).
  CONTENT_EXPORT static GpuProcessHost* Get(
      GpuProcessKind kind = GPU_PROCESS_KIND_SANDBOXED,
      bool force_create = true);

  GpuProcessHost(const GpuProcessHost&) = delete;
  GpuProcessHost& operator=(const GpuProcessHost&) = delete;

  // Returns whether there is an active GPU process or not.
  static void GetHasGpuProcess(base::OnceCallback<void(bool)> callback);

  // Helper function to run a callback on the UI thread. The callback receives
  // the appropriate GpuProcessHost instance. Note that the callback can be
  // called with a null host (e.g. when |force_create| is false, and no
  // GpuProcessHost instance exists).
  CONTENT_EXPORT static void CallOnUI(
      const base::Location& location,
      GpuProcessKind kind,
      bool force_create,
      base::OnceCallback<void(GpuProcessHost*)> callback);

  // Get the GPU process host for the GPU process with the given ID. Returns
  // null if the process no longer exists.
  static GpuProcessHost* FromID(int host_id);
  int host_id() const { return host_id_; }
  base::ProcessId process_id() const { return process_id_; }

  // What kind of GPU process, e.g. sandboxed or unsandboxed.
  GpuProcessKind kind();

  // Forcefully terminates the GPU process.
  void ForceShutdown();

  // Dumps the stack of the child process without crashing it.
  // Only implemented on Android.
  void DumpProcessStack();

  // Asks the GPU process to run a service instance corresponding to the
  // specific interface receiver type carried by |receiver|. The interface must
  // have the [ServiceSandbox=sandbox.mojom.Sandbox.kGpu] mojom attribute.
  template <typename Interface>
  void RunService(mojo::PendingReceiver<Interface> receiver) {
    // Note: consult chrome-security before changing these asserts.
    using ProvidedSandboxType = decltype(Interface::kServiceSandbox);
    static_assert(
        std::is_same<ProvidedSandboxType, const sandbox::mojom::Sandbox>::value,
        "This interface does not declare a proper ServiceSandbox attribute. "
        "See //docs/mojo_and_services.md (Specifying a sandbox).");
    static_assert(Interface::kServiceSandbox == sandbox::mojom::Sandbox::kGpu,
                  "This interface must have [ServiceSandbox=kGpu].");
    return RunServiceImpl(std::move(receiver));
  }

  CONTENT_EXPORT viz::mojom::GpuService* gpu_service();

#if BUILDFLAG(IS_WIN)
  CONTENT_EXPORT viz::mojom::InfoCollectionGpuService*
  info_collection_gpu_service();
#endif

  CONTENT_EXPORT int GetIDForTesting() const;

  viz::GpuHostImpl* gpu_host() { return gpu_host_.get(); }

#if BUILDFLAG(IS_MAC)
  BrowserChildProcessBackgroundedBridge*
  browser_child_process_backgrounded_bridge_for_testing() {
    return browser_child_process_backgrounded_bridge_.get();
  }
#endif

 private:
  enum class GpuTerminationOrigin {
    kUnknownOrigin = 0,
    kOzoneWaylandProxy = 1,
    kMax = 2,
  };

  static bool ValidateHost(GpuProcessHost* host);

  // Increments |recent_crash_count_| by one. Before incrementing, remove one
  // old crash for each forgiveness interval that has passed since the previous
  // crash. If |gpu_mode| doesn't match |last_crash_mode_|, first reset the
  // crash count.
  static void IncrementCrashCount(gpu::GpuMode gpu_mode);

  GpuProcessHost(int host_id, GpuProcessKind kind);
  ~GpuProcessHost() override;

  bool Init();

  // BrowserChildProcessHostDelegate implementation.
  void OnProcessLaunched() override;
  void OnProcessLaunchFailed(int error_code) override;
  void OnProcessCrashed(int exit_code) override;

  // viz::GpuHostImpl::Delegate:
  gpu::GPUInfo GetGPUInfo() const override;
  gpu::GpuFeatureInfo GetGpuFeatureInfo() const override;
  void DidInitialize(
      const gpu::GPUInfo& gpu_info,
      const gpu::GpuFeatureInfo& gpu_feature_info,
      const std::optional<gpu::GPUInfo>& gpu_info_for_hardware_gpu,
      const std::optional<gpu::GpuFeatureInfo>&
          gpu_feature_info_for_hardware_gpu,
      const gfx::GpuExtraInfo& gpu_extra_info) override;
  void DidFailInitialize() override;
  void DidCreateContextSuccessfully() override;
  void MaybeShutdownGpuProcess() override;
  void DidUpdateGPUInfo(const gpu::GPUInfo& gpu_info) override;
#if BUILDFLAG(IS_WIN)
  void DidUpdateOverlayInfo(const gpu::OverlayInfo& overlay_info) override;
  void DidUpdateDXGIInfo(gfx::mojom::DXGIInfoPtr dxgi_info) override;
#endif
  std::string GetIsolationKey(
      int32_t process_id,
      const blink::WebGPUExecutionContextToken& token) override;
  void BlockDomainsFrom3DAPIs(const std::set<GURL>& urls,
                              gpu::DomainGuilt guilt) override;
  void DisableGpuCompositing() override;
  bool GpuAccessAllowed() const override;
  gpu::GpuDiskCacheFactory* GetGpuDiskCacheFactory() override;
  void RecordLogMessage(int32_t severity,
                        const std::string& header,
                        const std::string& message) override;
  void BindDiscardableMemoryReceiver(
      mojo::PendingReceiver<
          discardable_memory::mojom::DiscardableSharedMemoryManager> receiver)
      override;
  void BindInterface(const std::string& interface_name,
                     mojo::ScopedMessagePipeHandle interface_pipe) override;
  void BindHostReceiver(mojo::GenericPendingReceiver generic_receiver) override;
#if BUILDFLAG(IS_OZONE)
  void TerminateGpuProcess(const std::string& message) override;
#endif

  bool LaunchGpuProcess();

  void SendOutstandingReplies();

  void BlockLiveOffscreenContexts();

  // Update GPU crash counters.  Disable GPU if crash limit is reached.
  void RecordProcessCrash();
  int GetFallbackCrashLimit() const;

  void RunServiceImpl(mojo::GenericPendingReceiver receiver);

#if !BUILDFLAG(IS_ANDROID)
  // Memory pressure handler, called by |memory_pressure_listener_|.
  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel level);
#endif

  // The serial number of the GpuProcessHost.
  int host_id_;

  // GPU process id in case GPU is not in-process.
  base::ProcessId process_id_ = base::kNullProcessId;

  // Whether the GPU process is valid, set to false after Send() failed.
  bool valid_;

  // Whether we are running a GPU thread inside the browser process instead
  // of a separate GPU process.
  bool in_process_;

  GpuProcessKind kind_;

  gpu::GpuMode mode_ = gpu::GpuMode::UNKNOWN;

  // Whether we actually launched a GPU process.
  bool process_launched_;

  GpuTerminationOrigin termination_origin_ =
      GpuTerminationOrigin::kUnknownOrigin;

  // Time Init started.  Used to log total GPU process startup time to UMA.
  base::TimeTicks init_start_time_;

  // The GPU process reported failure to initialize.
  bool did_fail_initialize_ = false;

  // The total number of GPU process crashes.
  static base::subtle::Atomic32 gpu_crash_count_;
  static bool crashed_before_;
  static int recent_crash_count_;
  static gpu::GpuMode last_crash_mode_;

  // Here the bottom-up destruction order matters:
  // The GPU thread depends on its host so stop the host last.
  // Otherwise, under rare timings when the thread is still in Init(),
  // it could crash as it fails to find a message pipe to the host.
  std::unique_ptr<BrowserChildProcessHostImpl> process_;
  std::unique_ptr<base::Thread> in_process_gpu_thread_;

#if BUILDFLAG(IS_MAC)
  scoped_refptr<CATransactionGPUCoordinator> ca_transaction_gpu_coordinator_;

  // Ensures the backgrounded state of the GPU process mirrors the backgrounded
  // state of the browser process.
  std::unique_ptr<BrowserChildProcessBackgroundedBridge>
      browser_child_process_backgrounded_bridge_;
#endif

  // Track the URLs of the pages which have live offscreen contexts,
  // assumed to be associated with untrusted content such as WebGL.
  // For best robustness, when any context lost notification is
  // received, assume all of these URLs are guilty, and block
  // automatic execution of 3D content from those domains.
  std::multiset<GURL> urls_with_live_offscreen_contexts_;

#if !BUILDFLAG(IS_ANDROID)
  // Responsible for forwarding the memory pressure notifications from the
  // browser process to the GPU process.
  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;
#endif

  std::unique_ptr<viz::GpuHostImpl> gpu_host_;

  base::WeakPtrFactory<GpuProcessHost> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_GPU_GPU_PROCESS_HOST_H_
