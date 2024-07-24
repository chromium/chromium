// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CHILD_PROCESS_LAUNCHER_H_
#define CONTENT_BROWSER_CHILD_PROCESS_LAUNCHER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/process/kill.h"
#include "base/process/process.h"
#include "base/process/process_metrics.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/child_process_launcher_helper.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_termination_info.h"
#include "content/public/common/result_codes.h"
#include "mojo/public/cpp/system/invitation.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_proto.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/public/browser/android/child_process_importance.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_types.h"
#endif

#if BUILDFLAG(IS_POSIX)
#include "base/files/scoped_file.h"
#endif

namespace base {
class CommandLine;
class UnsafeSharedMemoryRegion;
class ReadOnlySharedMemoryRegion;
#if BUILDFLAG(IS_ANDROID)
namespace android {
enum class ChildBindingState;
}
#endif
}  // namespace base

namespace perfetto {
namespace protos {
namespace pbzero {
class ChildProcessLauncherPriority;
}
}  // namespace protos
}  // namespace perfetto

namespace content {

class SandboxedProcessLauncherDelegate;

// Note: These codes are listed in a histogram and any new codes should be added
// at the end.
enum LaunchResultCode {
  // Launch start code, to not overlap with sandbox::ResultCode.
  LAUNCH_RESULT_START = 1001,
  // Launch success.
  LAUNCH_RESULT_SUCCESS,
  // Generic launch failure.
  LAUNCH_RESULT_FAILURE,
  // Placeholder for last item of the enum.
  LAUNCH_RESULT_CODE_LAST_CODE
};

#if BUILDFLAG(IS_WIN)
static_assert(static_cast<int>(LAUNCH_RESULT_START) >
                  static_cast<int>(sandbox::SBOX_ERROR_LAST),
              "LaunchResultCode must not overlap with sandbox::ResultCode");
#endif

struct RenderProcessPriority {
  RenderProcessPriority(bool visible,
                        bool has_media_stream,
                        bool has_foreground_service_worker,
                        unsigned int frame_depth,
                        bool intersects_viewport,
                        bool boost_for_pending_views,
                        bool boost_for_loading
#if BUILDFLAG(IS_ANDROID)
                        ,
                        ChildProcessImportance importance
#endif
#if !BUILDFLAG(IS_ANDROID)
                        ,
                        std::optional<base::Process::Priority> priority_override
#endif
                        )
      : visible(visible),
        has_media_stream(has_media_stream),
        has_foreground_service_worker(has_foreground_service_worker),
        frame_depth(frame_depth),
        intersects_viewport(intersects_viewport),
        boost_for_pending_views(boost_for_pending_views),
        boost_for_loading(boost_for_loading)
#if BUILDFLAG(IS_ANDROID)
        ,
        importance(importance)
#endif
#if !BUILDFLAG(IS_ANDROID)
        ,
        priority_override(priority_override)
#endif
  {
  }

  // Returns true if the child process is backgrounded.
  // DEPRECATED NOTICE: Use GetProcessPriority() instead.
  bool is_background() const;

  // Returns the process priority for this child process.
  base::Process::Priority GetProcessPriority() const;

  bool operator==(const RenderProcessPriority& other) const;
  bool operator!=(const RenderProcessPriority& other) const;

  using TraceProto = perfetto::protos::pbzero::ChildProcessLauncherPriority;
  void WriteIntoTrace(perfetto::TracedProto<TraceProto> proto) const;

  // Prefer `GetProcessPriority()` to inspecting these fields individually (to
  // ensure all priority logic is consistent).

  // |visible| is true if the process is responsible for one or more widget(s)
  // in foreground tabs. The notion of "visible" is determined by the embedder
  // but is ideally a widget in a non-minimized, non-background, non-occluded
  // tab (i.e. with pixels visible on the screen).
  bool visible;

  // |has_media_stream| is true when the process is responsible for "hearable"
  // content.
  bool has_media_stream;

  // |has_foreground_service_worker| is true when the process has a service
  // worker that may need to service timely events from other, possibly visible,
  // processes.
  bool has_foreground_service_worker;

  // |frame_depth| is the depth of the shallowest frame this process is
  // responsible for which has |visible| visibility. It only makes sense to
  // compare this property for two RenderProcessPriority instances with matching
  // |visible| properties.
  unsigned int frame_depth;

  // |intersects_viewport| is true if this process is responsible for a frame
  // which intersects a viewport which has |visible| visibility. It only makes
  // sense to compare this property for two RenderProcessPriority instances
  // with matching |visible| properties.
  bool intersects_viewport;

  // |boost_for_pending_views| is true if this process is responsible for a
  // pending view (this is used to boost priority of a process responsible for
  // foreground content which hasn't yet been added as a visible widget -- i.e.
  // during navigation).
  bool boost_for_pending_views;

  // |boost_for_loading| is true if this process is responsible for committing
  // navigation and initial loading.
  bool boost_for_loading;

#if BUILDFLAG(IS_ANDROID)
  ChildProcessImportance importance;
#endif

#if !BUILDFLAG(IS_ANDROID)
  // If this is set then the built-in process priority calculation system is
  // ignored, and an externally computed process priority is used.
  std::optional<base::Process::Priority> priority_override;
#endif
};

// Data to pass as file descriptors.
struct ChildProcessLauncherFileData {
  ChildProcessLauncherFileData();
  ChildProcessLauncherFileData(const ChildProcessLauncherFileData& others) =
      delete;
  ChildProcessLauncherFileData& operator=(const ChildProcessLauncherFileData&) =
      delete;
  ~ChildProcessLauncherFileData();

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)
  // Files opened by the browser and passed as corresponding file descriptors
  // in the child process. If a FilePath is provided, the file will be opened
  // and the descriptor cached for future process launches. If a ScopedFD is
  // provided, it will be passed to the new process and closed in the current
  // one.
  //
  // The files will be stored in base::FileDescriptorStore in the new process,
  // with the corresponding key.
  //
  // Currently only supported on Linux, ChromeOS and Android platforms.
  // TODO(crbug.com/40253015): this currently silently fails on Android.
  std::map<std::string, absl::variant<base::FilePath, base::ScopedFD>>
      files_to_preload;
#endif
};

// Launches a process asynchronously and notifies the client of the process
// handle when it's available.  It's used to avoid blocking the calling thread
// on the OS since often it can take > 100 ms to create the process.
class CONTENT_EXPORT ChildProcessLauncher {
 public:
  class CONTENT_EXPORT Client {
   public:
    // Will be called on the thread that the ChildProcessLauncher was
    // constructed on.
    virtual void OnProcessLaunched() = 0;

    virtual void OnProcessLaunchFailed(int error_code) {}

#if BUILDFLAG(IS_ANDROID)
    // Whether the process can use pre-warmed up connection.
    virtual bool CanUseWarmUpConnection();
#endif

   protected:
    virtual ~Client() {}
  };

  // Launches the process asynchronously, calling the client when the result is
  // ready.  Deleting this object before the process is created is safe, since
  // the callback won't be called.  If the process is still running by the time
  // this object destructs, it will be terminated.
  // Takes ownership of cmd_line.
  //
  // If `process_error_callback` is provided, it will be called if a Mojo error
  // is encountered when processing messages from the child process. This
  // callback must be safe to call from any thread.
  ChildProcessLauncher(
      std::unique_ptr<SandboxedProcessLauncherDelegate> delegate,
      std::unique_ptr<base::CommandLine> cmd_line,
      int child_process_id,
      Client* client,
      mojo::OutgoingInvitation mojo_invitation,
      const mojo::ProcessErrorCallback& process_error_callback,
      std::unique_ptr<ChildProcessLauncherFileData> file_data,
      base::UnsafeSharedMemoryRegion = {},
      base::ReadOnlySharedMemoryRegion = {},
      bool terminate_on_shutdown = true);

  ChildProcessLauncher(const ChildProcessLauncher&) = delete;
  ChildProcessLauncher& operator=(const ChildProcessLauncher&) = delete;

  ~ChildProcessLauncher();

  // True if the process is being launched and so the handle isn't available.
  bool IsStarting();

  // Getter for the process.  Only call after the process has started.
  const base::Process& GetProcess() const;

  // Call this when the child process exits to know what happened to it.
  // |known_dead| can be true if we already know the process is dead as it can
  // help the implemention figure the proper TerminationStatus.
  // On Linux, the use of |known_dead| is subtle and can be crucial if an
  // accurate status is important. With |known_dead| set to false, a dead
  // process could be seen as running. With |known_dead| set to true, the
  // process will be killed if it was still running. See ZygoteHostImpl for
  // more discussion of Linux implementation details.
  ChildProcessTerminationInfo GetChildTerminationInfo(bool known_dead);

#if BUILDFLAG(IS_ANDROID)
  // Changes whether the render process runs in the background or not.  Only
  // call this after the process has started.
  void SetRenderProcessPriority(const RenderProcessPriority& priority);
#else
  // Changes whether the process runs in the background or not.  Only call
  // this after the process has started.
  void SetProcessPriority(base::Process::Priority priority);
#endif  // BUILDFLAG(IS_ANDROID)

  // Terminates the process associated with this ChildProcessLauncher.
  // Returns true if the process was stopped, false if the process had not been
  // started yet or could not be stopped.
  // Note that |exit_code| is not used on Android.
  bool Terminate(int exit_code);

  // Similar to Terminate() but takes in a |process|.
  // On Android |process| must have been started by ChildProcessLauncher for
  // this method to work.
  static bool TerminateProcess(const base::Process& process, int exit_code);

  // Replaces the ChildProcessLauncher::Client for testing purposes. Returns the
  // previous client.
  Client* ReplaceClientForTest(Client* client);

#if BUILDFLAG(IS_ANDROID)
  // Returns the highest binding state for the ChildProcessConnection.
  base::android::ChildBindingState GetEffectiveChildBindingState();

  // Dumps the stack of the child process without crashing it.
  void DumpProcessStack();
#endif
 private:
  friend class internal::ChildProcessLauncherHelper;

  // Notifies the client about the result of the operation.
  void Notify(internal::ChildProcessLauncherHelper::Process process,
#if BUILDFLAG(IS_WIN)
              DWORD last_error,
#endif
              int error_code);

  raw_ptr<Client> client_;

  // The process associated with this ChildProcessLauncher. Set in Notify by
  // ChildProcessLauncherHelper once the process was started.
  internal::ChildProcessLauncherHelper::Process process_;
  base::TimeTicks process_start_time_;

  ChildProcessTerminationInfo termination_info_;
  bool starting_;

  // Controls whether the child process should be terminated on browser
  // shutdown. Default behavior is to terminate the child.
  const bool terminate_child_on_shutdown_;

  // Indicates if the child process should be launched with elevated privileges.
  // Can only be true on Windows.
  bool should_launch_elevated_ = false;

  scoped_refptr<internal::ChildProcessLauncherHelper> helper_;

  base::WeakPtrFactory<ChildProcessLauncher> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_CHILD_PROCESS_LAUNCHER_H_
