// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSER_CHILD_PROCESS_HOST_IMPL_H_
#define CONTENT_BROWSER_BROWSER_CHILD_PROCESS_HOST_IMPL_H_

#include <stdint.h>

#include <list>
#include <memory>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/memory/writable_shared_memory_region.h"
#include "base/process/process.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event_watcher.h"
#include "build/build_config.h"
#include "content/browser/child_process_launcher.h"
#include "content/browser/tracing/tracing_service_controller.h"
#include "content/common/child_process.mojom.h"
#include "content/common/child_process_host_impl.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/child_process_host_delegate.h"
#include "mojo/public/cpp/system/invitation.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/memory_instrumentation.mojom.h"

#if defined(OS_WIN)
#include "base/win/object_watcher.h"
#endif

namespace base {
class CommandLine;
}

namespace tracing {
class SystemTracingService;
}

namespace content {

class BrowserChildProcessHostIterator;
class BrowserChildProcessObserver;
class BrowserMessageFilter;

// Plugins/workers and other child processes that live on the IO thread use this
// class. RenderProcessHostImpl is the main exception that doesn't use this
/// class because it lives on the UI thread.
class CONTENT_EXPORT BrowserChildProcessHostImpl
    : public BrowserChildProcessHost,
      public ChildProcessHostDelegate,
#if defined(OS_WIN)
      public base::win::ObjectWatcher::Delegate,
#endif
      public ChildProcessLauncher::Client,
      public memory_instrumentation::mojom::CoordinatorConnector {
 public:
  // Constructs a process host with |ipc_mode| determining how IPC is done.
  BrowserChildProcessHostImpl(content::ProcessType process_type,
                              BrowserChildProcessHostDelegate* delegate,
                              ChildProcessHost::IpcMode ipc_mode);

  ~BrowserChildProcessHostImpl() override;

  // Terminates all child processes and deletes each BrowserChildProcessHost
  // instance.
  static void TerminateAll();

  // Copies kEnableFeatures and kDisableFeatures to the command line. Generates
  // them from the FeatureList override state, to take into account overrides
  // from FieldTrials.
  static void CopyFeatureAndFieldTrialFlags(base::CommandLine* cmd_line);

  // Appends kTraceStartup and kTraceRecordMode flags to the command line, if
  // needed.
  static void CopyTraceStartupFlags(base::CommandLine* cmd_line);

  // BrowserChildProcessHost implementation:
  bool Send(IPC::Message* message) override;
  void Launch(std::unique_ptr<SandboxedProcessLauncherDelegate> delegate,
              std::unique_ptr<base::CommandLine> cmd_line,
              bool terminate_on_shutdown) override;
  void LaunchWithPreloadedFiles(
      std::unique_ptr<SandboxedProcessLauncherDelegate> delegate,
      std::unique_ptr<base::CommandLine> cmd_line,
      std::map<std::string, base::FilePath> files_to_preload,
      bool terminate_on_shutdown) override;
  const ChildProcessData& GetData() override;
  ChildProcessHost* GetHost() override;
  ChildProcessTerminationInfo GetTerminationInfo(bool known_dead) override;
  std::unique_ptr<base::PersistentMemoryAllocator> TakeMetricsAllocator()
      override;
  void SetName(const std::u16string& name) override;
  void SetMetricsName(const std::string& metrics_name) override;
  void SetProcess(base::Process process) override;

  // ChildProcessHostDelegate implementation:
  void OnChannelInitialized(IPC::Channel* channel) override;
  void OnChildDisconnected() override;
  const base::Process& GetProcess() override;
  void BindHostReceiver(mojo::GenericPendingReceiver receiver) override;
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnChannelConnected(int32_t peer_pid) override;
  void OnChannelError() override;
  void OnBadMessageReceived(const IPC::Message& message) override;

  // Terminates the process and logs a stack trace after a bad message was
  // received from the child process.
  void TerminateOnBadMessageReceived(const std::string& error);

  // Removes this host from the host list. Calls ChildProcessHost::ForceShutdown
  void ForceShutdown();

  // Adds an IPC message filter.
  void AddFilter(BrowserMessageFilter* filter);

  // Unlike Launch(), AppendExtraCommandLineSwitches will not be called
  // in this function. If AppendExtraCommandLineSwitches has been called before
  // reaching launch, call this function instead so the command line switches
  // won't be appended twice
  void LaunchWithoutExtraCommandLineSwitches(
      std::unique_ptr<SandboxedProcessLauncherDelegate> delegate,
      std::unique_ptr<base::CommandLine> cmd_line,
      std::map<std::string, base::FilePath> files_to_preload,
      bool terminate_on_shutdown);

  static void HistogramBadMessageTerminated(ProcessType process_type);

#if defined(OS_ANDROID)
  void EnableWarmUpConnection();
  void DumpProcessStack();
#endif

  BrowserChildProcessHostDelegate* delegate() const { return delegate_; }

  mojo::OutgoingInvitation* GetInProcessMojoInvitation() {
    return &child_process_host_->GetMojoInvitation().value();
  }

  mojom::ChildProcess* child_process() const {
    return static_cast<ChildProcessHostImpl*>(child_process_host_.get())
        ->child_process();
  }

  typedef std::list<BrowserChildProcessHostImpl*> BrowserChildProcessList;
 private:
  friend class BrowserChildProcessHostIterator;
  friend class BrowserChildProcessObserver;

  void OnProcessConnected();

  static BrowserChildProcessList* GetIterator();

  static void AddObserver(BrowserChildProcessObserver* observer);
  static void RemoveObserver(BrowserChildProcessObserver* observer);

  // Creates the |metrics_allocator_|.
  void CreateMetricsAllocator();

  // Passes the |metrics_allocator_|, if any, to the managed process. This
  // requires the process to have been launched and the IPC channel to be
  // available.
  void ShareMetricsAllocatorToProcess();

  // ChildProcessLauncher::Client implementation.
  void OnProcessLaunched() override;
  void OnProcessLaunchFailed(int error_code) override;
#if defined(OS_ANDROID)
  bool CanUseWarmUpConnection() override;
#endif

  // memory_instrumentation::mojom::CoordinatorConnector implementation:
  void RegisterCoordinatorClient(
      mojo::PendingReceiver<memory_instrumentation::mojom::Coordinator>
          receiver,
      mojo::PendingRemote<memory_instrumentation::mojom::ClientProcess>
          client_process) override;

  // Returns true if the process has successfully launched. Must only be called
  // on the IO thread.
  bool IsProcessLaunched() const;

  static void OnMojoError(
      base::WeakPtr<BrowserChildProcessHostImpl> process,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      const std::string& error);
  static void TerminateProcessForBadMessage(
      base::WeakPtr<BrowserChildProcessHostImpl> process,
      const std::string& error);

#if defined(OS_WIN)
  // ObjectWatcher::Delegate implementation.
  void OnObjectSignaled(HANDLE object) override;
#endif

  ChildProcessData data_;
  std::string metrics_name_;
  BrowserChildProcessHostDelegate* delegate_;
  std::unique_ptr<ChildProcessHost> child_process_host_;
  mojo::Receiver<memory_instrumentation::mojom::CoordinatorConnector>
      coordinator_connector_receiver_{this};

  std::unique_ptr<ChildProcessLauncher> child_process_;

#if defined(OS_WIN)
  // Watches to see if the child process exits before the IPC channel has
  // been connected. Thereafter, its exit is determined by an error on the
  // IPC channel.
  base::win::ObjectWatcher early_exit_watcher_;
#endif

  // The memory allocator, if any, in which the process will write its metrics.
  std::unique_ptr<base::PersistentMemoryAllocator> metrics_allocator_;

  // The shared memory region used by |metrics_allocator_| that should be
  // transferred to the child process.
  base::WritableSharedMemoryRegion metrics_shared_region_;

  bool has_legacy_ipc_channel_ = false;
  bool notify_child_connection_status_ = true;

#if defined(OS_ANDROID)
  // whether the child process can use pre-warmed up connection for better
  // performance.
  bool can_use_warm_up_connection_ = false;
#endif

  // Keeps this process registered with the tracing subsystem.
  std::unique_ptr<TracingServiceController::ClientRegistration>
      tracing_registration_;

#if defined(OS_POSIX) && !defined(OS_ANDROID)
  // For child process to connect to the system tracing service.
  std::unique_ptr<tracing::SystemTracingService> system_tracing_service_;
#endif

  base::WeakPtrFactory<BrowserChildProcessHostImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_CHILD_PROCESS_HOST_IMPL_H_
