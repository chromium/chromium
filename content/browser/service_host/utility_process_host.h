// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_HOST_UTILITY_PROCESS_HOST_H_
#define CONTENT_BROWSER_SERVICE_HOST_UTILITY_PROCESS_HOST_H_

#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "base/environment.h"
#include "base/process/launch.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "content/browser/child_process_launcher.h"
#include "content/common/child_process.mojom.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_child_process_host_delegate.h"
#include "content/public/common/zygote/zygote_buildflags.h"
#include "media/media_buildflags.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"

#if BUILDFLAG(USE_ZYGOTE)
#include "content/public/common/zygote/zygote_handle.h"
#endif  // BUILDFLAG(USE_ZYGOTE)

#if BUILDFLAG(IS_WIN)
#include "base/synchronization/waitable_event.h"
#endif  // BUILDFLAG(IS_WIN)

namespace base {
class Thread;
}  // namespace base

#if BUILDFLAG(ENABLE_GPU_CHANNEL_MEDIA_CAPTURE)
namespace viz {
class GpuClient;
}  // namespace viz
#endif  // BUILDFLAG(ENABLE_GPU_CHANNEL_MEDIA_CAPTURE)

namespace content {
class BrowserChildProcessHostImpl;
class InProcessChildThreadParams;

typedef base::Thread* (*UtilityMainThreadFactoryFunction)(
    const InProcessChildThreadParams&);

// This class acts as the browser-side host to a utility child process hosting a
// mojo service. This class lives solely on the UI thread. If you need to bind
// a Mojo interface, specify it via `WithBoundServiceInterfaceOnChildProcess` on
// the `Options` passed in.
class CONTENT_EXPORT UtilityProcessHost final
    : public BrowserChildProcessHostDelegate {
 public:
  static void RegisterUtilityMainThreadFactory(
      UtilityMainThreadFactoryFunction create);

  // Interface which may be passed to a UtilityProcessHost on construction. All
  // methods are called from the IO thread.
  class Client {
   public:
    enum class CrashType {
      // Indicates this crash occurred very early in process lifetime, before
      // UtilityMain was able to execute and fully lock down the sandbox or
      // start IPC. It typically indicates a critical failure to bootstrap the
      // sandbox or OS services (e.g. a loader issue). No untrusted data has yet
      // been processed by the utility process at this point so it is safe to
      // assume that the crash is not attacker controlled. This type of crash is
      // only reported on Windows.
      kPreIpcInitialization,
      // Indicates the crash occurred after IPC was initialized and service(s)
      // have started running in the process and started processing potentially
      // untrusted data.
      kPostIpcInitialization,
    };
    virtual ~Client() = default;

    // Called when the OS has reported that the process has successfully
    // launched.
    virtual void OnProcessLaunched(const base::Process& process) {}
    // Called when the process has terminated normally.
    virtual void OnProcessTerminatedNormally() {}
    // Called when the process has terminated due to a crash. The `type` field
    // indicates the type of crash. See above.
    virtual void OnProcessCrashed(CrashType type) {}
  };

  struct CONTENT_EXPORT Options {
    Options();
    ~Options();

    Options(const Options&) = delete;
    Options& operator=(const Options&) = delete;

    Options(Options&&);
    Options& operator=(Options&&);

    // Makes the process run with a specific sandbox type, or unsandboxed if
    // Sandbox::kNoSandbox is specified.
    Options& WithSandboxType(sandbox::mojom::Sandbox sandbox_type);

    // Sets the name of the process to appear in the task manager.
    Options& WithName(const std::u16string& name);

    // Sets the name used for metrics reporting. This should not be a localized
    // name. This is recorded to metrics, so update UtilityProcessNameHash enum
    // in enums.xml if new values are passed here.
    Options& WithMetricsName(const std::string& metrics_name);

    Options& WithChildFlags(int flags);

    // Provides extra switches to append to the process's command line.
    Options& WithExtraCommandLineSwitches(std::vector<std::string> switches);

#if BUILDFLAG(IS_WIN)
    // Specifies libraries to preload before the sandbox is locked down. Paths
    // should be absolute.
    Options& WithPreloadLibraries(const std::vector<base::FilePath>& preloads);
#endif  // BUILDFLAG(IS_WIN)

    // Allows the child process to bind viz.mojom.Gpu.
    Options& WithGpuClientAllowed();

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)
    // Adds to ChildProcessLauncherFileData::files_to_preload, which maps |key|
    // -> |file| in the new process's base::FileDescriptorStore.
    Options& WithFileToPreload(
        std::string key,
        std::variant<base::FilePath, base::ScopedFD> file);
#endif

#if BUILDFLAG(IS_POSIX)
    Options& WithEnvironment(const base::EnvironmentMap& env);
#endif

#if BUILDFLAG(USE_ZYGOTE)
    Options& WithZygoteForTesting(ZygoteCommunication* handle);
#endif  // BUILDFLAG(USE_ZYGOTE)

    // Requests that the process bind a receiving pipe targeting the interface
    // named by `receiver`. Calls to this method generally end up in
    // `ChildThreadImpl::OnBindReceiver()` and the option is used for testing
    // only.
    Options& WithBoundReceiverOnChildProcessForTesting(
        mojo::GenericPendingReceiver receiver);

    // Requests that the utility process bind a receiving pipe targeting the
    // service interface named by `receiver`.
    Options& WithBoundServiceInterfaceOnChildProcess(
        mojo::GenericPendingReceiver receiver);

    // Passes the contents of this Options object to a newly returned Options
    // value. This can be called when moving an in-line built Options object
    // directly into a call to `Start`.
    Options Pass();

   private:
    friend class UtilityProcessHost;

    sandbox::mojom::Sandbox sandbox_type_;

    // Map of environment variables to values.
    base::EnvironmentMap env_;

    // The non-localized name used for metrics reporting.
    std::string metrics_name_;

    // ChildProcessHost flags to use when starting the child process.
    int child_flags_;

    // Extra command line switches to append.
    std::vector<std::string> extra_switches_;

#if BUILDFLAG(IS_WIN)
    // Libraries to load before sandbox lockdown. Only used on Windows.
    std::vector<base::FilePath> preload_libraries_;
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(USE_ZYGOTE)
    std::optional<raw_ptr<ZygoteCommunication>> zygote_for_testing_;
#endif  // BUILDFLAG(USE_ZYGOTE)

#if BUILDFLAG(ENABLE_GPU_CHANNEL_MEDIA_CAPTURE)
    // Whether or not to bind viz::mojom::Gpu to the utility process.
    bool allowed_gpu_;
#endif  // BUILDFLAG(ENABLE_GPU_CHANNEL_MEDIA_CAPTURE)

    // A mojo receiver to bind once the process starts.
    std::optional<mojo::GenericPendingReceiver> receiver_to_bind_;

    // A mojo service interface to bind once the process starts.
    std::optional<mojo::GenericPendingReceiver> service_interface_to_bind_;

    // Extra files and file descriptors to preload in the new process.
    std::unique_ptr<ChildProcessLauncherFileData> file_data_;

    // The process name used to identify the process in task manager.
    std::u16string name_;
  };

  // Creates and starts a new UtilityProcessHost with the specified `Options`.
  // Pass a `client` if delegate callbacks are needed.
  static bool Start(Options options, std::unique_ptr<Client> client = nullptr);

  UtilityProcessHost(const UtilityProcessHost&) = delete;
  UtilityProcessHost& operator=(const UtilityProcessHost&) = delete;

 private:
  UtilityProcessHost(Options options, std::unique_ptr<Client> client);

  ~UtilityProcessHost() override;

  // Returns a control interface for the running child process.
  mojom::ChildProcess* GetChildProcess();

  void MaybeBindMojoInterfaces();

  // Starts the child process if needed, returns true on success.
  bool StartProcess();

  // BrowserChildProcessHostDelegate:
  void OnProcessLaunched() override;
  void OnProcessLaunchFailed(int error_code) override;
  void OnProcessCrashed(int exit_code) override;
  std::optional<std::string> GetServiceName() override;
  void BindHostReceiver(mojo::GenericPendingReceiver receiver) override;

  Options options_;

  // Child process host implementation.
  std::unique_ptr<BrowserChildProcessHostImpl> process_;

  // Used in single-process mode instead of |process_|.
  std::unique_ptr<base::Thread> in_process_thread_;

  // Indicates whether the process has been successfully launched yet, or if
  // launch failed.
  enum class LaunchState {
    kLaunchInProgress,
    kLaunchComplete,
    kLaunchFailed,
  };
  LaunchState launch_state_ = LaunchState::kLaunchInProgress;

#if BUILDFLAG(ENABLE_GPU_CHANNEL_MEDIA_CAPTURE)
  std::unique_ptr<viz::GpuClient, base::OnTaskRunnerDeleter> gpu_client_;
#endif  // BUILDFLAG(ENABLE_GPU_CHANNEL_MEDIA_CAPTURE)

  std::unique_ptr<Client> client_;

#if BUILDFLAG(IS_WIN)
  // An event that is passed to the utility process. Only set for sandboxed
  // processes. The utility process uses this to signal that it has reached
  // UtilityMain.
  std::optional<base::WaitableEvent> bootstrap_signal_event_;
#endif  // BUILDFLAG(IS_WIN)
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_HOST_UTILITY_PROCESS_HOST_H_
