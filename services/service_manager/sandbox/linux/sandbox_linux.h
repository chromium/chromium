// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_SANDBOX_LINUX_SANDBOX_LINUX_H_
#define SERVICES_SERVICE_MANAGER_SANDBOX_LINUX_SANDBOX_LINUX_H_

#include <memory>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "base/posix/global_descriptors.h"
#include "sandbox/linux/syscall_broker/broker_command.h"
#include "sandbox/linux/syscall_broker/broker_file_permission.h"
#include "services/service_manager/sandbox/export.h"
#include "services/service_manager/sandbox/linux/sandbox_seccomp_bpf_linux.h"
#include "services/service_manager/sandbox/sandbox_type.h"
#include "services/service_manager/sandbox/sanitizer_buildflags.h"

#if BUILDFLAG(USING_SANITIZER)
#include <sanitizer/common_interface_defs.h>
#endif

namespace base {
template <typename T>
struct DefaultSingletonTraits;
class Thread;
}  // namespace base

namespace sandbox {
namespace syscall_broker {
class BrokerProcess;
}  // namespace syscall_broker
class SetuidSandboxClient;
}  // namespace sandbox

namespace service_manager {

// A singleton class to represent and change our sandboxing state for the
// three main Linux sandboxes.
// The sandboxing model allows using two layers of sandboxing. The first layer
// can be implemented either with unprivileged namespaces or with the setuid
// sandbox. This class provides a way to engage the namespace sandbox, but does
// not deal with the legacy setuid sandbox directly.
// The second layer is mainly based on seccomp-bpf and is engaged with
// InitializeSandbox(). InitializeSandbox() is also responsible for "sealing"
// the first layer of sandboxing. That is, InitializeSandbox must always be
// called to have any meaningful sandboxing at all.
class SERVICE_MANAGER_SANDBOX_EXPORT SandboxLinux {
 public:
  // This is a list of sandbox IPC methods which the renderer may send to the
  // sandbox host. See
  // https://chromium.googlesource.com/chromium/src/+/master/docs/linux_sandbox_ipc.md
  // This isn't the full list, values < 32 are reserved for methods called from
  // Skia, and values < 64 are reserved for libc_interceptor.cc.
  enum LinuxSandboxIPCMethods {
    DEPRECATED_METHOD_GET_FALLBACK_FONT_FOR_CHAR = 64,
    DEPRECATED_METHOD_GET_CHILD_WITH_INODE,
    DEPRECATED_METHOD_GET_STYLE_FOR_STRIKE,
    METHOD_MAKE_SHARED_MEMORY_SEGMENT,
    DEPRECATED_METHOD_MATCH_WITH_FALLBACK,
  };

  // These form a bitmask which describes the conditions of the Linux sandbox.
  // Note: this doesn't strictly give you the current status, it states
  // what will be enabled when the relevant processes are initialized.
  enum Status {
    // SUID sandbox active.
    kSUID = 1 << 0,

    // Sandbox is using a new PID namespace.
    kPIDNS = 1 << 1,

    // Sandbox is using a new network namespace.
    kNetNS = 1 << 2,

    // seccomp-bpf sandbox active.
    kSeccompBPF = 1 << 3,

    // The Yama LSM module is present and enforcing.
    kYama = 1 << 4,

    // seccomp-bpf sandbox is active and the kernel supports TSYNC.
    kSeccompTSYNC = 1 << 5,

    // User namespace sandbox active.
    kUserNS = 1 << 6,

    // A flag that denotes an invalid sandbox status.
    kInvalid = 1 << 31,
  };

  // SandboxLinux Options are a superset of SandboxSecompBPF Options.
  struct Options : public SandboxSeccompBPF::Options {
    // When running with a zygote, the namespace sandbox will have already
    // been engaged prior to initializing SandboxLinux itself, and need not
    // be done so again. Set to true to indicate that there isn't a zygote
    // for this process and the step is to be performed here explicitly.
    bool engage_namespace_sandbox = false;

    // Allow starting the sandbox with multiple threads already running. This
    // will enable TSYNC for seccomp-BPF, which syncs the seccomp-BPF policy
    // across all running threads.
    bool allow_threads_during_sandbox_init = false;

    // Enables the CHECK for open directories. The open directory check is only
    // useful for the chroot jail (from the semantic layer of the sandbox), and
    // can safely be disabled if we are only enabling the seccomp-BPF layer.
    bool check_for_open_directories = true;
  };

  // Callers can provide this hook to run code right before the policy
  // is passed to the BPF compiler and the sandbox is engaged. If
  // pre_sandbox_hook() returns true, the sandbox will be engaged
  // afterwards, otherwise the process is terminated.
  using PreSandboxHook = base::OnceCallback<bool(Options)>;

  // Get our singleton instance.
  static SandboxLinux* GetInstance();

  // Do some initialization that can only be done before any of the sandboxes
  // are enabled. If using the setuid sandbox, this should be called manually
  // before the setuid sandbox is engaged.
  // Security: When this runs, it is imperative that either InitializeSandbox()
  // runs as well or that all file descriptors returned in
  // GetFileDescriptorsToClose() get closed.
  // Otherwise file descriptors that bypass the security of the setuid sandbox
  // would be kept open. One must be particularly careful if a process performs
  // a fork().
  void PreinitializeSandbox();

  // Check that the current process is the init process of a new PID
  // namespace and then proceed to drop access to the file system by using
  // a new unprivileged namespace. This is a layer-1 sandbox.
  // In order for this sandbox to be effective, it must be "sealed" by calling
  // InitializeSandbox().
  // Terminates the process in case the sandboxing operations cannot complete
  // successfully.
  void EngageNamespaceSandbox(bool from_zygote);

  // Performs the same actions as EngageNamespaceSandbox, but is allowed to
  // to fail. This is useful when sandboxed non-renderer processes could
  // benefit from extra sandboxing but is not strictly required on systems that
  // don't support unprivileged user namespaces.
  // Zygote should use EngageNamespaceSandbox instead.
  bool EngageNamespaceSandboxIfPossible();

  // Return a list of file descriptors to close if PreinitializeSandbox() ran
  // but InitializeSandbox() won't. Avoid using.
  // TODO(jln): get rid of this hack.
  std::vector<int> GetFileDescriptorsToClose();

  // Seal an eventual layer-1 sandbox and initialize the layer-2 sandbox with
  // an adequate policy depending on the process type and command line
  // arguments.
  // Currently the layer-2 sandbox is composed of seccomp-bpf and address space
  // limitations.
  // This function should only be called without any thread running.
  bool InitializeSandbox(SandboxType sandbox_type,
                         PreSandboxHook hook,
                         const Options& options);

  // Stop |thread| in a way that can be trusted by the sandbox.
  void StopThread(base::Thread* thread);

  // Returns the status of the renderer, worker and ppapi sandbox. Can only
  // be queried after going through PreinitializeSandbox(). This is a bitmask
  // and uses the constants defined in "enum Status" above. Since the
  // status needs to be provided before the sandboxes are actually started,
  // this returns what will actually happen once InitializeSandbox()
  // is called from inside these processes.
  int GetStatus();

  // Returns true if the current process is single-threaded or if the number
  // of threads cannot be determined.
  bool IsSingleThreaded() const;

  // Returns true if we started Seccomp BPF.
  bool seccomp_bpf_started() const;

  // Simple accessor for our instance of the setuid sandbox. Will never return
  // NULL.
  // There is no StartSetuidSandbox(), the SetuidSandboxClient instance should
  // be used directly.
  sandbox::SetuidSandboxClient* setuid_sandbox_client() const;

  // Check the policy and eventually start the seccomp-bpf sandbox. Fine to be
  // called with threads, as long as
  // |options.allow_threads_during_sandbox_init| is true and the kernel
  // supports seccomp's TSYNC feature. If TSYNC is not available we treat
  // multiple threads as a fatal error.
  bool StartSeccompBPF(service_manager::SandboxType sandbox_type,
                       PreSandboxHook hook,
                       const Options& options);

  // Limit the address space of the current process (and its children) to make
  // some vulnerabilities harder to exploit. Writes the errno due to setrlimit
  // (including 0 if no error) into |error|.
  bool LimitAddressSpace(int* error);

  // Returns a file descriptor to proc. The file descriptor is no longer valid
  // after the sandbox has been sealed.
  int proc_fd() const {
    DCHECK_NE(-1, proc_fd_);
    return proc_fd_;
  }

#if BUILDFLAG(USING_SANITIZER)
  __sanitizer_sandbox_arguments* sanitizer_args() const {
    return sanitizer_args_.get();
  }
#endif

  // A BrokerProcess is a helper that is started before the sandbox is engaged,
  // typically from a pre-sandbox hook, that will serve requests to access
  // files over an IPC channel. The client  of this runs from a SIGSYS handler
  // triggered by the seccomp-bpf sandbox.
  // |client_sandbox_policy| is the policy being run by the client, and is
  // used to derive the equivalent broker-side policy.
  // |broker_side_hook| is an alternate pre-sandbox hook to be run before the
  // broker itself gets sandboxed, to which the broker side policy and
  // |options| are passed.
  // Crashes the process if the broker can not be started since continuation
  // is impossible (and presumably unsafe).
  // This should never be destroyed, as after the sandbox is started it is
  // vital to the process.
  void StartBrokerProcess(
      const sandbox::syscall_broker::BrokerCommandSet& allowed_command_set,
      std::vector<sandbox::syscall_broker::BrokerFilePermission> permissions,
      PreSandboxHook broker_side_hook,
      const Options& options);

  sandbox::syscall_broker::BrokerProcess* broker_process() const {
    return broker_process_;
  }

 private:
  friend struct base::DefaultSingletonTraits<SandboxLinux>;

  SandboxLinux();
  ~SandboxLinux();

  // We must have been pre_initialized_ before using these.
  bool seccomp_bpf_supported() const;
  bool seccomp_bpf_with_tsync_supported() const;

  // Returns true if it can be determined that the current process has open
  // directories that are not managed by the SandboxLinux class. This would
  // be a vulnerability as it would allow to bypass the setuid sandbox.
  bool HasOpenDirectories() const;

  // The last part of the initialization is to make sure any temporary "hole"
  // in the sandbox is closed. For now, this consists of closing proc_fd_.
  void SealSandbox();

  // GetStatus() makes promises as to how the sandbox will behave. This
  // checks that no promises have been broken.
  void CheckForBrokenPromises(service_manager::SandboxType sandbox_type);

  // Stop |thread| and make sure it does not appear in /proc/self/tasks/
  // anymore.
  void StopThreadAndEnsureNotCounted(base::Thread* thread) const;

  // Engages the namespace sandbox as described for EngageNamespaceSandbox.
  // Returns false if it fails to transition to a new user namespace, but
  // after transitioning to a new user namespace we don't allow this function
  // to fail.
  bool EngageNamespaceSandboxInternal(bool from_zygote);

  // A file descriptor to /proc. It's dangerous to have it around as it could
  // allow for sandbox bypasses. It needs to be closed before we consider
  // ourselves sandboxed.
  int proc_fd_;

  bool seccomp_bpf_started_;
  // The value returned by GetStatus(). Gets computed once and then cached.
  int sandbox_status_flags_;
  // Did PreinitializeSandbox() run?
  bool pre_initialized_;
  bool seccomp_bpf_supported_;             // Accurate if pre_initialized_.
  bool seccomp_bpf_with_tsync_supported_;  // Accurate if pre_initialized_.
  bool yama_is_enforcing_;                 // Accurate if pre_initialized_.
  bool initialize_sandbox_ran_;            // InitializeSandbox() was called.
  std::unique_ptr<sandbox::SetuidSandboxClient> setuid_sandbox_client_;
#if BUILDFLAG(USING_SANITIZER)
  std::unique_ptr<__sanitizer_sandbox_arguments> sanitizer_args_;
#endif
  sandbox::syscall_broker::BrokerProcess* broker_process_;  // Leaked as global.

  DISALLOW_COPY_AND_ASSIGN(SandboxLinux);
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_SANDBOX_LINUX_SANDBOX_LINUX_H_
