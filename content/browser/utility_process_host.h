// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_UTILITY_PROCESS_HOST_H_
#define CONTENT_BROWSER_UTILITY_PROCESS_HOST_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/environment.h"
#include "base/memory/weak_ptr.h"
#include "base/process/launch.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "content/browser/child_process_launcher.h"
#include "content/common/child_process.mojom.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_child_process_host_delegate.h"
#include "content/public/common/zygote/zygote_buildflags.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

#if BUILDFLAG(USE_ZYGOTE)
#include "content/public/common/zygote/zygote_handle.h"
#endif  // BUILDFLAG(USE_ZYGOTE)

namespace base {
class Thread;
}  // namespace base

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH) || \
    BUILDFLAG(IS_MAC)
namespace viz {
class GpuClient;
}  // namespace viz
#endif

namespace content {
class BrowserChildProcessHostImpl;
class InProcessChildThreadParams;
struct ChildProcessData;

typedef base::Thread* (*UtilityMainThreadFactoryFunction)(
    const InProcessChildThreadParams&);

// This class acts as the browser-side host to a utility child process.  A
// utility process is a short-lived process that is created to run a specific
// task.  This class lives solely on the IO thread.
// If you need a single method call in the process, use StartFooBar(p).
// If you need multiple batches of work to be done in the process, use
// StartBatchMode(), then multiple calls to StartFooBar(p), then finish with
// EndBatchMode().
// If you need to bind Mojo interfaces, use Start() to start the child
// process and then call BindInterface().
//
// Note: If your class keeps a ptr to an object of this type, grab a weak ptr to
// avoid a use after free since this object is deleted synchronously but the
// client notification is asynchronous.  See http://crbug.com/108871.
class CONTENT_EXPORT UtilityProcessHost
    : public BrowserChildProcessHostDelegate {
 public:
  static void RegisterUtilityMainThreadFactory(
      UtilityMainThreadFactoryFunction create);

  // Interface which may be passed to a UtilityProcessHost on construction. All
  // methods are called from the IO thread.
  class Client {
   public:
    virtual ~Client() {}

    virtual void OnProcessLaunched(const base::Process& process) {}
    virtual void OnProcessTerminatedNormally() {}
    virtual void OnProcessCrashed() {}
  };

  // This class is self-owned. It must be instantiated using new, and shouldn't
  // be deleted manually.
  // TODO(crbug.com/40254698): Make it clearer the caller of the
  // constructor do not own memory. A static method to create them + private
  // constructor could be better.
  UtilityProcessHost();
  explicit UtilityProcessHost(std::unique_ptr<Client> client);

  UtilityProcessHost(const UtilityProcessHost&) = delete;
  UtilityProcessHost& operator=(const UtilityProcessHost&) = delete;

  ~UtilityProcessHost() override;

  base::WeakPtr<UtilityProcessHost> AsWeakPtr();

  // Makes the process run with a specific sandbox type, or unsandboxed if
  // Sandbox::kNoSandbox is specified.
  void SetSandboxType(sandbox::mojom::Sandbox sandbox_type);

  // Returns information about the utility child process.
  const ChildProcessData& GetData();
#if BUILDFLAG(IS_POSIX)
  void SetEnv(const base::EnvironmentMap& env);
#endif

  // Starts the utility process.
  bool Start();

  // Sets the name of the process to appear in the task manager.
  void SetName(const std::u16string& name);

  // Sets the name used for metrics reporting. This should not be a localized
  // name. This is recorded to metrics, so update UtilityProcessNameHash enum in
  // enums.xml if new values are passed here.
  void SetMetricsName(const std::string& metrics_name);

  void set_child_flags(int flags) { child_flags_ = flags; }

  // Provides extra switches to append to the process's command line.
  void SetExtraCommandLineSwitches(std::vector<std::string> switches);

  // Allows the child process to bind viz.mojom.Gpu.
  void SetAllowGpuClient();

#if BUILDFLAG(IS_WIN)
  // Specifies libraries to preload before the sandbox is locked down. Paths
  // should be absolute.
  void SetPreloadLibraries(const std::vector<base::FilePath>& preloads);
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)
  // Adds to ChildProcessLauncherFileData::files_to_preload, which maps |key| ->
  // |file| in the new process's base::FileDescriptorStore.
  void AddFileToPreload(std::string key,
                        absl::variant<base::FilePath, base::ScopedFD> file);
#endif

#if BUILDFLAG(USE_ZYGOTE)
  void SetZygoteForTesting(ZygoteCommunication* handle);
#endif  // BUILDFLAG(USE_ZYGOTE)

  // Returns a control interface for the running child process.
  mojom::ChildProcess* GetChildProcess();

 private:
  // Starts the child process if needed, returns true on success.
  bool StartProcess();

  // BrowserChildProcessHostDelegate:
  void OnProcessLaunched() override;
  void OnProcessLaunchFailed(int error_code) override;
  void OnProcessCrashed(int exit_code) override;
  std::optional<std::string> GetServiceName() override;
  void BindHostReceiver(mojo::GenericPendingReceiver receiver) override;

  // Launch the child process with switches that will setup this sandbox type.
  sandbox::mojom::Sandbox sandbox_type_;

  // ChildProcessHost flags to use when starting the child process.
  int child_flags_;

  // Map of environment variables to values.
  base::EnvironmentMap env_;

  // True if StartProcess() has been called.
  bool started_;

  // The process name used to identify the process in task manager.
  std::u16string name_;

  // The non-localized name used for metrics reporting.
  std::string metrics_name_;

  // Child process host implementation.
  std::unique_ptr<BrowserChildProcessHostImpl> process_;

  // Used in single-process mode instead of |process_|.
  std::unique_ptr<base::Thread> in_process_thread_;

  // Extra command line switches to append.
  std::vector<std::string> extra_switches_;

#if BUILDFLAG(IS_WIN)
  // Libraries to load before sandbox lockdown. Only used on Windows.
  std::vector<base::FilePath> preload_libraries_;
#endif  // BUILDFLAG(IS_WIN)

  // Extra files and file descriptors to preload in the new process.
  std::unique_ptr<ChildProcessLauncherFileData> file_data_;

#if BUILDFLAG(USE_ZYGOTE)
  std::optional<raw_ptr<ZygoteCommunication>> zygote_for_testing_;
#endif  // BUILDFLAG(USE_ZYGOTE)

  // Indicates whether the process has been successfully launched yet, or if
  // launch failed.
  enum class LaunchState {
    kLaunchInProgress,
    kLaunchComplete,
    kLaunchFailed,
  };
  LaunchState launch_state_ = LaunchState::kLaunchInProgress;

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH) || \
    BUILDFLAG(IS_MAC)
  bool allowed_gpu_;
  std::unique_ptr<viz::GpuClient, base::OnTaskRunnerDeleter> gpu_client_;
#endif

  std::unique_ptr<Client> client_;

  // Used to vend weak pointers, and should always be declared last.
  base::WeakPtrFactory<UtilityProcessHost> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_UTILITY_PROCESS_HOST_H_
