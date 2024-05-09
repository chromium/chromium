// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/linux/sandbox_linux.h"

#include <dirent.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_macros.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/set_process_title.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_id_name_manager.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ppapi/buildflags/buildflags.h"
#include "sandbox/constants.h"
#include "sandbox/linux/seccomp-bpf-helpers/sigsys_handlers.h"
#include "sandbox/linux/services/credentials.h"
#include "sandbox/linux/services/libc_interceptor.h"
#include "sandbox/linux/services/namespace_sandbox.h"
#include "sandbox/linux/services/proc_util.h"
#include "sandbox/linux/services/resource_limits.h"
#include "sandbox/linux/services/syscall_wrappers.h"
#include "sandbox/linux/services/thread_helpers.h"
#include "sandbox/linux/services/yama.h"
#include "sandbox/linux/suid/client/setuid_sandbox_client.h"
#include "sandbox/linux/syscall_broker/broker_client.h"
#include "sandbox/linux/syscall_broker/broker_command.h"
#include "sandbox/linux/syscall_broker/broker_process.h"
#include "sandbox/linux/system_headers/landlock.h"
#include "sandbox/linux/system_headers/linux_stat.h"
#include "sandbox/policy/features.h"
#include "sandbox/policy/linux/bpf_broker_policy_linux.h"
#include "sandbox/policy/linux/sandbox_seccomp_bpf_linux.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"
#include "sandbox/policy/sandbox.h"
#include "sandbox/policy/sandbox_type.h"
#include "sandbox/policy/switches.h"
#include "sandbox/sandbox_buildflags.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

#if BUILDFLAG(USING_SANITIZER)
#include <sanitizer/common_interface_defs.h>
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/assistant/buildflags.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace sandbox {
namespace policy {

namespace {

// The state of Landlock support on the system.
// Used to report through UMA.
enum LandlockState {
  kEnabled = 0,
  kDisabled = 1,
  kNotSupported = 2,
  kUnknown = 3,
  kMaxValue = kUnknown,
};

void LogSandboxStarted(const std::string& sandbox_name) {
  const std::string process_type =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kProcessType);
  const std::string activated_sandbox =
      "Activated " + sandbox_name +
      " sandbox for process type: " + process_type + ".";
  VLOG(1) << activated_sandbox;
}

bool IsRunningTSAN() {
#if defined(THREAD_SANITIZER)
  return true;
#else
  return false;
#endif
}

// In processes which must bring up GPU drivers before sandbox initialization,
// we can't ensure that other threads won't be running already.
bool ShouldAllowThreadsDuringSandboxInit(const std::string& process_type,
                                         sandbox::mojom::Sandbox sandbox_type) {
  if (process_type == switches::kGpuProcess) {
    return true;
  }

  if (process_type == switches::kUtilityProcess &&
      sandbox_type == sandbox::mojom::Sandbox::kOnDeviceModelExecution) {
    return true;
  }

  return false;
}

// Get a file descriptor to /proc. Either duplicate |proc_fd| or try to open
// it by using the filesystem directly.
// TODO(jln): get rid of this ugly interface.
base::ScopedFD OpenProc(int proc_fd) {
  int ret_proc_fd = -1;
  if (proc_fd >= 0) {
    // If a handle to /proc is available, use it. This allows to bypass file
    // system restrictions.
    ret_proc_fd =
        HANDLE_EINTR(openat(proc_fd, ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC));
  } else {
    // Otherwise, make an attempt to access the file system directly.
    ret_proc_fd = HANDLE_EINTR(
        openat(AT_FDCWD, "/proc/", O_RDONLY | O_DIRECTORY | O_CLOEXEC));
  }
  DCHECK_LE(0, ret_proc_fd);
  return base::ScopedFD(ret_proc_fd);
}

bool UpdateProcessTypeAndEnableSandbox(
    SandboxLinux::Options options,
    const syscall_broker::BrokerSandboxConfig& policy) {
  base::CommandLine::StringVector exec =
      base::CommandLine::ForCurrentProcess()->GetArgs();
  base::CommandLine::Reset();
  base::CommandLine::Init(0, nullptr);
  base::CommandLine::ForCurrentProcess()->InitFromArgv(exec);

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  std::string new_process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);
  if (!new_process_type.empty()) {
    new_process_type.append("-broker");
  } else {
    new_process_type = "broker";
  }

  VLOG(3) << "UpdateProcessTypeAndEnableSandbox: Updating process type to "
          << new_process_type;
  command_line->AppendSwitchASCII(switches::kProcessType, new_process_type);

  // Update the process title. The argv was already cached by the call to
  // SetProcessTitleFromCommandLine in content_main_runner.cc, so we can pass
  // NULL here (we don't have the original argv at this point).
  base::SetProcessTitleFromCommandLine(nullptr);

  return SandboxSeccompBPF::StartSandboxWithExternalPolicy(
      std::make_unique<BrokerProcessPolicy>(policy.allowed_command_set),
      base::ScopedFD());
}

}  // namespace

SandboxLinux::SandboxLinux()
    : proc_fd_(-1),
      seccomp_bpf_started_(false),
      sandbox_status_flags_(kInvalid),
      pre_initialized_(false),
      seccomp_bpf_supported_(false),
      seccomp_bpf_with_tsync_supported_(false),
      yama_is_enforcing_(false),
      initialize_sandbox_ran_(false),
      setuid_sandbox_client_(SetuidSandboxClient::Create()),
      broker_process_(nullptr) {
  if (!setuid_sandbox_client_) {
    LOG(FATAL) << "Failed to instantiate the setuid sandbox client.";
  }
#if BUILDFLAG(USING_SANITIZER)
  sanitizer_args_ = std::make_unique<__sanitizer_sandbox_arguments>();
  *sanitizer_args_ = {0};
#endif
}

SandboxLinux::~SandboxLinux() {
  if (pre_initialized_) {
    CHECK(initialize_sandbox_ran_);
  }
}

SandboxLinux* SandboxLinux::GetInstance() {
  SandboxLinux* instance = base::Singleton<SandboxLinux>::get();
  CHECK(instance);
  return instance;
}

void SandboxLinux::PreinitializeSandbox() {
  CHECK(!pre_initialized_);
  seccomp_bpf_supported_ = false;
#if BUILDFLAG(USING_SANITIZER)
  // Sanitizers need to open some resources before the sandbox is enabled.
  // This should not fork, not launch threads, not open a directory.
  __sanitizer_sandbox_on_notify(sanitizer_args());
  sanitizer_args_.reset();
#endif

  // Open proc_fd_. It would break the security of the setuid sandbox if it was
  // not closed.
  // If SandboxLinux::PreinitializeSandbox() runs, InitializeSandbox() must run
  // as well.
  proc_fd_ = HANDLE_EINTR(open("/proc", O_DIRECTORY | O_RDONLY | O_CLOEXEC));
  CHECK_GE(proc_fd_, 0);
  // We "pre-warm" the code that detects supports for seccomp BPF.
  if (SandboxSeccompBPF::IsSeccompBPFDesired()) {
    if (!SandboxSeccompBPF::SupportsSandbox()) {
      VLOG(1) << "Lacking support for seccomp-bpf sandbox.";
    } else {
      seccomp_bpf_supported_ = true;
    }

    if (SandboxSeccompBPF::SupportsSandboxWithTsync()) {
      seccomp_bpf_with_tsync_supported_ = true;
    }
  }

  // Yama is a "global", system-level status. We assume it will not regress
  // after startup.
  const int yama_status = Yama::GetStatus();
  yama_is_enforcing_ = (yama_status & Yama::STATUS_PRESENT) &&
                       (yama_status & Yama::STATUS_ENFORCING);
  pre_initialized_ = true;
}

void SandboxLinux::EngageNamespaceSandbox(bool from_zygote) {
  CHECK(EngageNamespaceSandboxInternal(from_zygote));
}

bool SandboxLinux::EngageNamespaceSandboxIfPossible() {
  return EngageNamespaceSandboxInternal(false /* from_zygote */);
}

std::vector<int> SandboxLinux::GetFileDescriptorsToClose() {
  std::vector<int> fds;
  if (proc_fd_ >= 0) {
    fds.push_back(proc_fd_);
  }
  return fds;
}

int SandboxLinux::GetStatus() {
  if (!pre_initialized_) {
    return 0;
  }
  if (sandbox_status_flags_ == kInvalid) {
    // Initialize sandbox_status_flags_.
    sandbox_status_flags_ = 0;
    if (setuid_sandbox_client_->IsSandboxed()) {
      sandbox_status_flags_ |= kSUID;
      if (setuid_sandbox_client_->IsInNewPIDNamespace())
        sandbox_status_flags_ |= kPIDNS;
      if (setuid_sandbox_client_->IsInNewNETNamespace())
        sandbox_status_flags_ |= kNetNS;
    } else if (NamespaceSandbox::InNewUserNamespace()) {
      sandbox_status_flags_ |= kUserNS;
      if (NamespaceSandbox::InNewPidNamespace())
        sandbox_status_flags_ |= kPIDNS;
      if (NamespaceSandbox::InNewNetNamespace())
        sandbox_status_flags_ |= kNetNS;
    }

    // We report whether the sandbox will be activated when renderers, workers
    // and PPAPI plugins go through sandbox initialization.
    if (seccomp_bpf_supported()) {
      sandbox_status_flags_ |= kSeccompBPF;
    }

    if (seccomp_bpf_with_tsync_supported()) {
      sandbox_status_flags_ |= kSeccompTSYNC;
    }

    if (yama_is_enforcing_) {
      sandbox_status_flags_ |= kYama;
    }
  }

  return sandbox_status_flags_;
}

// Threads are counted via /proc/self/task. This is a little hairy because of
// PID namespaces and existing sandboxes, so "self" must really be used instead
// of using the pid.
bool SandboxLinux::IsSingleThreaded() const {
  base::ScopedFD proc_fd(OpenProc(proc_fd_));

  CHECK(proc_fd.is_valid()) << "Could not count threads, the sandbox was not "
                            << "pre-initialized properly.";

  const bool is_single_threaded =
      ThreadHelpers::IsSingleThreaded(proc_fd.get());

  return is_single_threaded;
}

bool SandboxLinux::seccomp_bpf_started() const {
  return seccomp_bpf_started_;
}

SetuidSandboxClient* SandboxLinux::setuid_sandbox_client() const {
  return setuid_sandbox_client_.get();
}

// For seccomp-bpf, we use the SandboxSeccompBPF class.
bool SandboxLinux::StartSeccompBPF(sandbox::mojom::Sandbox sandbox_type,
                                   PreSandboxHook hook,
                                   const Options& options) {
  CHECK(!seccomp_bpf_started_);
  CHECK(pre_initialized_);
#if BUILDFLAG(USE_SECCOMP_BPF)
  if (!seccomp_bpf_supported())
    return false;

  if (IsUnsandboxedSandboxType(sandbox_type) ||
      !SandboxSeccompBPF::IsSeccompBPFDesired() ||
      !SandboxSeccompBPF::SupportsSandbox()) {
    return true;
  }

  if (hook)
    CHECK(std::move(hook).Run(options));

  // If we allow threads *and* have multiple threads, try to use TSYNC.
  SandboxBPF::SeccompLevel seccomp_level =
      options.allow_threads_during_sandbox_init && !IsSingleThreaded()
          ? SandboxBPF::SeccompLevel::MULTI_THREADED
          : SandboxBPF::SeccompLevel::SINGLE_THREADED;

  bool force_disable_spectre_variant2_mitigation =
      base::FeatureList::IsEnabled(
          features::kForceDisableSpectreVariant2MitigationInNetworkService) &&
      sandbox_type == sandbox::mojom::Sandbox::kNetwork;

  // If the kernel supports the sandbox, and if the command line says we
  // should enable it, enable it or die.
  std::unique_ptr<BPFBasePolicy> policy =
      SandboxSeccompBPF::PolicyForSandboxType(sandbox_type, options);
  SandboxSeccompBPF::StartSandboxWithExternalPolicy(
      std::move(policy), OpenProc(proc_fd_), seccomp_level,
      force_disable_spectre_variant2_mitigation);
  SandboxSeccompBPF::RunSandboxSanityChecks(sandbox_type, options);
  seccomp_bpf_started_ = true;
  LogSandboxStarted("seccomp-bpf");
  return true;
#else
  return false;
#endif
}

bool SandboxLinux::InitializeSandbox(sandbox::mojom::Sandbox sandbox_type,
                                     SandboxLinux::PreSandboxHook hook,
                                     const Options& options) {
  DCHECK(!initialize_sandbox_ran_);
  initialize_sandbox_ran_ = true;

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  const std::string process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);

  // We need to make absolutely sure that our sandbox is "sealed" before
  // returning.
  absl::Cleanup sandbox_sealer = [this] { SealSandbox(); };
  // Make sure that this function enables sandboxes as promised by GetStatus().
  absl::Cleanup sandbox_promise_keeper = [this, sandbox_type] {
    CheckForBrokenPromises(sandbox_type);
  };

  const bool has_threads = !IsSingleThreaded();

  // For now, restrict the |options.allow_threads_during_sandbox_init| option to
  // the GPU process
  DCHECK(process_type == switches::kGpuProcess ||
         !options.allow_threads_during_sandbox_init);
  if (has_threads && !options.allow_threads_during_sandbox_init) {
    std::string error_message =
        "InitializeSandbox() called with multiple threads in process " +
        process_type + ".";
    // TSAN starts a helper thread, so we don't start the sandbox and don't
    // even report an error about it.
    if (IsRunningTSAN())
      return false;

    // Only a few specific processes are allowed to call InitializeSandbox()
    // with multiple threads running.
    bool sandbox_failure_fatal =
        !ShouldAllowThreadsDuringSandboxInit(process_type, sandbox_type);
    // This can be disabled with the '--gpu-sandbox-failures-fatal' flag.
    // Setting the flag with no value or any value different than 'yes' or 'no'
    // is equal to setting '--gpu-sandbox-failures-fatal=yes'.
    if (process_type == switches::kGpuProcess &&
        command_line->HasSwitch(switches::kGpuSandboxFailuresFatal)) {
      const std::string switch_value =
          command_line->GetSwitchValueASCII(switches::kGpuSandboxFailuresFatal);
      sandbox_failure_fatal = switch_value != "no";
    }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
    CHECK(process_type != switches::kGpuProcess || sandbox_failure_fatal);
#endif

    if (sandbox_failure_fatal && !IsUnsandboxedSandboxType(sandbox_type)) {
      error_message += " Try waiting for /proc to be updated.";
      LOG(ERROR) << error_message;

      for (const auto& id :
           base::ThreadIdNameManager::GetInstance()->GetIds()) {
        LOG(ERROR) << "ThreadId=" << id << " name:"
                   << base::ThreadIdNameManager::GetInstance()->GetName(id);
      }
      // This will return if /proc/self eventually reports this process is
      // single-threaded, or crash if it does not after a number of retries.
      ThreadHelpers::AssertSingleThreaded();
    } else {
      LOG(WARNING) << error_message;
      return false;
    }
  }

  // At this point we are either single threaded, or we won't be engaging the
  // semantic layer of the sandbox and we won't care if there are file
  // descriptors left open.

  // Pre-initialize if not already done.
  if (!pre_initialized_)
    PreinitializeSandbox();

  // Turn on the namespace sandbox if our caller wants it (and the zygote hasn't
  // done so already).
  if (options.engage_namespace_sandbox)
    EngageNamespaceSandbox(false /* from_zygote */);

  // Check for open directories, which can break the semantic sandbox layer. In
  // some cases the caller doesn't want to enable the semantic sandbox layer,
  // and this CHECK should be skipped. In this case, the caller should unset
  // |options.check_for_open_directories|.
  CHECK(!options.check_for_open_directories || !HasOpenDirectories())
      << "InitializeSandbox() called after unexpected directories have been "
      << "opened. This breaks the security of the setuid sandbox.";

  InitLibcLocaltimeFunctions();

#if !BUILDFLAG(IS_CHROMEOS)
  if (!IsUnsandboxedSandboxType(sandbox_type)) {
    // No sandboxed process should make use of getaddrinfo() as it is impossible
    // to sandbox (e.g. glibc loads arbitrary third party DNS resolution
    // libraries).
    // On ChromeOS none of these third party libraries are installed, so there
    // is no need to discourage getaddrinfo().
    // TODO(crbug.com/40220505): in the future this should depend on the
    // libraries listed in /etc/nsswitch.conf, and should be a
    // SandboxLinux::Options option.
    DiscourageGetaddrinfo();
  }
#endif  // BUILDFLAG(IS_LINUX)

  // Attempt to limit the future size of the address space of the process.
  // Fine to call with multiple threads as we don't use RLIMIT_STACK.
  int error = 0;
  const bool limited_as = LimitAddressSpace(&error);
  if (error) {
    // Restore errno. Internally to |LimitAddressSpace|, the errno due to
    // setrlimit may be lost.
    errno = error;
    PCHECK(limited_as);
  }

  return StartSeccompBPF(sandbox_type, std::move(hook), options);
}

void SandboxLinux::StopThread(base::Thread* thread) {
  DCHECK(thread);
  StopThreadAndEnsureNotCounted(thread);
}

bool SandboxLinux::seccomp_bpf_supported() const {
  CHECK(pre_initialized_);
  return seccomp_bpf_supported_;
}

bool SandboxLinux::seccomp_bpf_with_tsync_supported() const {
  CHECK(pre_initialized_);
  return seccomp_bpf_with_tsync_supported_;
}

rlim_t GetProcessDataSizeLimit(sandbox::mojom::Sandbox sandbox_type) {
#if defined(ARCH_CPU_64_BITS)
  if (sandbox_type == sandbox::mojom::Sandbox::kGpu ||
      sandbox_type == sandbox::mojom::Sandbox::kRenderer) {
    // Allow the GPU/RENDERER process's sandbox to access more physical memory
    // if it's available on the system.
    //
    // Renderer processes are allowed to access 16 GB; the GPU process, up
    // to 64 GB.
    constexpr rlim_t GB = 1024 * 1024 * 1024;
    const rlim_t physical_memory = base::SysInfo::AmountOfPhysicalMemory();
    rlim_t limit;
    if (sandbox_type == sandbox::mojom::Sandbox::kGpu &&
        physical_memory > 64 * GB) {
      limit = 64 * GB;
    } else if (sandbox_type == sandbox::mojom::Sandbox::kGpu &&
               physical_memory > 32 * GB) {
      limit = 32 * GB;
    } else if (physical_memory > 16 * GB) {
      limit = 16 * GB;
    } else {
      limit = 8 * GB;
    }

    if (sandbox_type == sandbox::mojom::Sandbox::kRenderer &&
        base::FeatureList::IsEnabled(
            sandbox::policy::features::kHigherRendererMemoryLimit)) {
      limit *= 2;
    }

    return limit;
  }
#endif

  return static_cast<rlim_t>(kDataSizeLimit);
}

bool SandboxLinux::LimitAddressSpace(int* error) {
#if !defined(ADDRESS_SANITIZER) && !defined(MEMORY_SANITIZER) && \
    !defined(THREAD_SANITIZER) && !defined(LEAK_SANITIZER)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  sandbox::mojom::Sandbox sandbox_type =
      SandboxTypeFromCommandLine(*command_line);
  if (sandbox_type == sandbox::mojom::Sandbox::kNoSandbox) {
    return false;
  }

  // Unfortunately, it does not appear possible to set RLIMIT_AS such that it
  // will both (a) be high enough to support V8's and WebAssembly's address
  // space requirements while also (b) being low enough to mitigate exploits
  // using integer overflows that require large allocations, heap spray, or
  // other memory-hungry attack modes.

  rlim_t process_data_size_limit = GetProcessDataSizeLimit(sandbox_type);
  // Fine to call with multiple threads as we don't use RLIMIT_STACK.
  *error = ResourceLimits::Lower(RLIMIT_DATA, process_data_size_limit);

  // Cache the resource limit before turning on the sandbox.
  base::SysInfo::AmountOfVirtualMemory();

  return *error == 0;
#else
  base::SysInfo::AmountOfVirtualMemory();
  return false;
#endif  // !defined(ADDRESS_SANITIZER) && !defined(MEMORY_SANITIZER) &&
        // !defined(THREAD_SANITIZER) && !defined(LEAK_SANITIZER)
}

void SandboxLinux::StartBrokerProcess(
    const syscall_broker::BrokerCommandSet& allowed_command_set,
    std::vector<syscall_broker::BrokerFilePermission> permissions,
    const Options& options) {
  // Use EACCES as the policy's default error number to remain consistent with
  // other LSMs like AppArmor and Landlock. Some userspace code, such as
  // glibc's |dlopen|, expect to see EACCES rather than EPERM. See
  // crbug.com/1233028 for an example.
  auto policy = std::make_optional<syscall_broker::BrokerSandboxConfig>(
      allowed_command_set, std::move(permissions), EACCES);
  // Leaked at shutdown, so use bare |new|.
  broker_process_ = new syscall_broker::BrokerProcess(
      std::move(policy),
      syscall_broker::BrokerProcess::BrokerType::SIGNAL_BASED);

  // The initialization callback will perform generic initialization and then
  // call broker_sandboxer_callback.
  CHECK(broker_process_->Fork(
      base::BindOnce(&UpdateProcessTypeAndEnableSandbox, options)));
}

bool SandboxLinux::ShouldBrokerHandleSyscall(int sysno) const {
  return broker_process_->IsSyscallAllowed(sysno);
}

bpf_dsl::ResultExpr SandboxLinux::HandleViaBroker(int sysno) const {
  const bpf_dsl::ResultExpr handle_via_broker =
      bpf_dsl::Trap(syscall_broker::BrokerClient::SIGSYS_Handler,
                    broker_process_->GetBrokerClientSignalBased());
  if (sysno == __NR_fstatat_default) {
    // This may be an fstatat(fd, "", stat_buf, AT_EMPTY_PATH), which should be
    // rewritten as fstat(fd, stat_buf). This should be consistent with how the
    // baseline policy handles fstatat().
    // Note that this will cause some legitimate but strange invocations of
    // fstatat() to fail, see https://crbug.com/1243290#c8 for details.
    const bpf_dsl::Arg<int> flags(3);
    return bpf_dsl::If((flags & AT_EMPTY_PATH) == AT_EMPTY_PATH,
                       RewriteFstatatSIGSYS(BPFBasePolicy::GetFSDeniedErrno()))
        .Else(handle_via_broker);
  } else {
    return handle_via_broker;
  }
}

bool SandboxLinux::HasOpenDirectories() const {
  return ProcUtil::HasOpenDirectory(proc_fd_);
}

void SandboxLinux::SealSandbox() {
  if (proc_fd_ >= 0) {
    int ret = IGNORE_EINTR(close(proc_fd_));
    CHECK_EQ(0, ret);
    proc_fd_ = -1;
  }
}

void SandboxLinux::CheckForBrokenPromises(
    sandbox::mojom::Sandbox sandbox_type) {
  if (sandbox_type != sandbox::mojom::Sandbox::kRenderer
#if BUILDFLAG(ENABLE_PPAPI)
      && sandbox_type != sandbox::mojom::Sandbox::kPpapi
#endif
  ) {
    return;
  }
  // Make sure that any promise made with GetStatus() wasn't broken.
  bool promised_seccomp_bpf_would_start =
      (sandbox_status_flags_ != kInvalid) && (GetStatus() & kSeccompBPF);
  CHECK(!promised_seccomp_bpf_would_start || seccomp_bpf_started_);
}

void SandboxLinux::StopThreadAndEnsureNotCounted(base::Thread* thread) const {
  DCHECK(thread);
  base::ScopedFD proc_fd(OpenProc(proc_fd_));
  PCHECK(proc_fd.is_valid());
  CHECK(ThreadHelpers::StopThreadAndWatchProcFS(proc_fd.get(), thread));
}

bool SandboxLinux::EngageNamespaceSandboxInternal(bool from_zygote) {
  CHECK(pre_initialized_);
  CHECK(IsSingleThreaded())
      << "The process cannot have multiple threads when engaging the namespace "
         "sandbox, because the thread engaging the sandbox cannot ensure that "
         "other threads close all their open directories.";

  if (from_zygote) {
    // Check being in a new PID namespace created by the namespace sandbox and
    // being the init process.
    CHECK(NamespaceSandbox::InNewPidNamespace());
    const pid_t pid = getpid();
    CHECK_EQ(1, pid);
  }

  // After we successfully move to a new user ns, we don't allow this function
  // to fail.
  if (!Credentials::MoveToNewUserNS()) {
    return false;
  }

  // Note: this requires SealSandbox() to be called later in this process to be
  // safe, as this class is keeping a file descriptor to /proc/.
  CHECK(Credentials::DropFileSystemAccess(proc_fd_));

  // Now we drop all capabilities that we can. In the zygote process, we need
  // to keep CAP_SYS_ADMIN, to place each child in its own PID namespace
  // later on.
  std::vector<Credentials::Capability> caps;
  if (from_zygote) {
    caps.push_back(Credentials::Capability::SYS_ADMIN);
  }
  CHECK(Credentials::SetCapabilities(proc_fd_, caps));
  return true;
}

void SandboxLinux::ReportLandlockStatus() {
  LandlockState landlock_state = LandlockState::kUnknown;
  const int landlock_version =
      landlock_create_ruleset(nullptr, 0, LANDLOCK_CREATE_RULESET_VERSION);
  if (landlock_version <= 0) {
    const int err = errno;
    switch (err) {
      case ENOSYS: {
        DVLOG(1) << "Landlock not supported by the kernel.";
        landlock_state = LandlockState::kNotSupported;
        break;
      }
      case EOPNOTSUPP: {
        DVLOG(1) << "Landlock supported by the kernel but disabled.";
        landlock_state = LandlockState::kDisabled;
        break;
      }
      default: {
        DVLOG(1) << "Could not determine Landlock state.";
        landlock_state = LandlockState::kUnknown;
      }
    }
  } else {
    DVLOG(1) << "Landlock enabled; Version " << landlock_version;
    landlock_state = LandlockState::kEnabled;
  }

  UMA_HISTOGRAM_ENUMERATION("Security.Sandbox.LandlockState", landlock_state);
}

}  // namespace policy
}  // namespace sandbox
