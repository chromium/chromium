// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "base/callback.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/ui_devtools/buildflags.h"
#include "components/viz/host/gpu_host_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_child_process_host_delegate.h"
#include "content/public/browser/gpu_data_manager.h"
#include "gpu/command_buffer/common/activity_flags.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/config/gpu_extra_info.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_mode.h"
#include "gpu/ipc/common/surface_handle.h"
#include "ipc/ipc_sender.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/viz/privileged/mojom/compositing/frame_sink_manager.mojom.h"
#include "services/viz/privileged/mojom/gl/gpu_host.mojom.h"
#include "services/viz/privileged/mojom/gl/gpu_service.mojom.h"
#include "services/viz/privileged/mojom/viz_main.mojom.h"
#include "url/gurl.h"

#if BUILDFLAG(USE_VIZ_DEVTOOLS)
#include "content/browser/gpu/viz_devtools_connector.h"
#endif

namespace base {
class Thread;
}

namespace content {
class BrowserChildProcessHostImpl;

#if defined(OS_MACOSX)
class CATransactionGPUCoordinator;
#endif

class GpuProcessHost : public BrowserChildProcessHostDelegate,
                       public IPC::Sender,
                       public viz::GpuHostImpl::Delegate {
 public:
  static int GetGpuCrashCount();

  // Creates a new GpuProcessHost (if |force_create| is turned on) or gets an
  // existing one, resulting in the launching of a GPU process if required.
  // Returns null on failure. It is not safe to store the pointer once control
  // has returned to the message loop as it can be destroyed. Instead store the
  // associated GPU host ID.  This could return NULL if GPU access is not
  // allowed (blacklisted).
  CONTENT_EXPORT static GpuProcessHost* Get(
      GpuProcessKind kind = GPU_PROCESS_KIND_SANDBOXED,
      bool force_create = true);

  // Returns whether there is an active GPU process or not.
  static void GetHasGpuProcess(base::OnceCallback<void(bool)> callback);

  // Helper function to run a callback on the IO thread. The callback receives
  // the appropriate GpuProcessHost instance. Note that the callback can be
  // called with a null host (e.g. when |force_create| is false, and no
  // GpuProcessHost instance exists).
  CONTENT_EXPORT static void CallOnIO(
      GpuProcessKind kind,
      bool force_create,
      base::OnceCallback<void(GpuProcessHost*)> callback);

  // Get the GPU process host for the GPU process with the given ID. Returns
  // null if the process no longer exists.
  static GpuProcessHost* FromID(int host_id);
  int host_id() const { return host_id_; }
  base::ProcessId process_id() const { return process_id_; }

  // IPC::Sender implementation.
  bool Send(IPC::Message* msg) override;

  // What kind of GPU process, e.g. sandboxed or unsandboxed.
  GpuProcessKind kind();

  // Forcefully terminates the GPU process.
  void ForceShutdown();

  // Asks the GPU process to run a service instance corresponding to the
  // specific interface receiver type carried by |receiver|.
  void RunService(mojo::GenericPendingReceiver receiver);

  CONTENT_EXPORT viz::mojom::GpuService* gpu_service();

  CONTENT_EXPORT int GetIDForTesting() const;

  viz::GpuHostImpl* gpu_host() { return gpu_host_.get(); }

 private:
  enum class GpuTerminationOrigin {
    kUnknownOrigin = 0,
    kOzoneWaylandProxy = 1,
    kMax = 2,
  };

  static bool ValidateHost(GpuProcessHost* host);

  // Increments |crash_count| by one. Before incrementing |crash_count|, for
  // each |forgive_minutes| that has passed since the previous crash remove one
  // old crash.
  static void IncrementCrashCount(int forgive_minutes, int* crash_count);

  GpuProcessHost(int host_id, GpuProcessKind kind);
  ~GpuProcessHost() override;

  bool Init();

  // BrowserChildProcessHostDelegate implementation.
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnChannelConnected(int32_t peer_pid) override;
  void OnProcessLaunched() override;
  void OnProcessLaunchFailed(int error_code) override;
  void OnProcessCrashed(int exit_code) override;

  // viz::GpuHostImpl::Delegate:
  gpu::GPUInfo GetGPUInfo() const override;
  gpu::GpuFeatureInfo GetGpuFeatureInfo() const override;
  void DidInitialize(
      const gpu::GPUInfo& gpu_info,
      const gpu::GpuFeatureInfo& gpu_feature_info,
      const base::Optional<gpu::GPUInfo>& gpu_info_for_hardware_gpu,
      const base::Optional<gpu::GpuFeatureInfo>&
          gpu_feature_info_for_hardware_gpu,
      const gpu::GpuExtraInfo& gpu_extra_info) override;
  void DidFailInitialize() override;
  void DidCreateContextSuccessfully() override;
  void BlockDomainFrom3DAPIs(const GURL& url, gpu::DomainGuilt guilt) override;
  void DisableGpuCompositing() override;
  bool GpuAccessAllowed() const override;
  gpu::ShaderCacheFactory* GetShaderCacheFactory() override;
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
  void RunService(
      const std::string& service_name,
      mojo::PendingReceiver<service_manager::mojom::Service> receiver) override;
#if defined(USE_OZONE)
  void TerminateGpuProcess(const std::string& message) override;
  void SendGpuProcessMessage(IPC::Message* message) override;
#endif

  // Message handlers.
  void OnFieldTrialActivated(const std::string& trial_name);

  bool LaunchGpuProcess();

  void SendOutstandingReplies();

  void BlockLiveOffscreenContexts();

  // Update GPU crash counters.  Disable GPU if crash limit is reached.
  void RecordProcessCrash();

  // The serial number of the GpuProcessHost.
  int host_id_;

  // GPU process id in case GPU is not in-process.
  base::ProcessId process_id_ = base::kNullProcessId;

  // Qeueud messages to send when the process launches.
  base::queue<IPC::Message*> queued_messages_;

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
  static int hardware_accelerated_recent_crash_count_;
  static int swiftshader_recent_crash_count_;
  static int display_compositor_recent_crash_count_;

  // Here the bottom-up destruction order matters:
  // The GPU thread depends on its host so stop the host last.
  // Otherwise, under rare timings when the thread is still in Init(),
  // it could crash as it fails to find a message pipe to the host.
  std::unique_ptr<BrowserChildProcessHostImpl> process_;
  std::unique_ptr<base::Thread> in_process_gpu_thread_;

#if defined(OS_MACOSX)
  scoped_refptr<CATransactionGPUCoordinator> ca_transaction_gpu_coordinator_;
#endif

  // Track the URLs of the pages which have live offscreen contexts,
  // assumed to be associated with untrusted content such as WebGL.
  // For best robustness, when any context lost notification is
  // received, assume all of these URLs are guilty, and block
  // automatic execution of 3D content from those domains.
  std::multiset<GURL> urls_with_live_offscreen_contexts_;

  std::unique_ptr<viz::GpuHostImpl> gpu_host_;

#if BUILDFLAG(USE_VIZ_DEVTOOLS)
  std::unique_ptr<VizDevToolsConnector> devtools_connector_;
#endif

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<GpuProcessHost> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(GpuProcessHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GPU_GPU_PROCESS_HOST_H_
