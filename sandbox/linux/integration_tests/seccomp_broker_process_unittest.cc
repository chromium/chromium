// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <errno.h>
#include <fcntl.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <memory>
#include <tuple>
#include <type_traits>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/files/file_path_watcher_inotify.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/posix/eintr_wrapper.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/bpf_dsl/policy.h"
#include "sandbox/linux/bpf_dsl/seccomp_macros.h"
#include "sandbox/linux/seccomp-bpf/bpf_tests.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf_test_runner.h"
#include "sandbox/linux/syscall_broker/broker_client.h"
#include "sandbox/linux/syscall_broker/broker_command.h"
#include "sandbox/linux/syscall_broker/broker_file_permission.h"
#include "sandbox/linux/syscall_broker/broker_process.h"
#include "sandbox/linux/system_headers/linux_seccomp.h"
#include "sandbox/linux/system_headers/linux_stat.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"
#include "sandbox/linux/tests/scoped_temporary_file.h"
#include "sandbox/linux/tests/test_utils.h"
#include "sandbox/linux/tests/unit_tests.h"
#include "testing/gtest/include/gtest/gtest-param-test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

using bpf_dsl::Allow;
using bpf_dsl::Arg;
using bpf_dsl::Error;
using bpf_dsl::If;
using bpf_dsl::ResultExpr;
using bpf_dsl::Trap;

using BrokerProcess = syscall_broker::BrokerProcess;
using BrokerType = syscall_broker::BrokerProcess::BrokerType;
using BrokerCommandSet = syscall_broker::BrokerCommandSet;
using BrokerFilePermission = syscall_broker::BrokerFilePermission;

// Test a trap handler that makes use of a broker process to open().

class InitializedOpenBroker {
 public:
  explicit InitializedOpenBroker(
      BrokerType broker_type = BrokerType::SIGNAL_BASED) {
    syscall_broker::BrokerCommandSet command_set;
    command_set.set(syscall_broker::COMMAND_OPEN);
    command_set.set(syscall_broker::COMMAND_ACCESS);
    std::vector<BrokerFilePermission> permissions = {
        BrokerFilePermission::ReadOnly("/proc/allowed"),
        BrokerFilePermission::ReadOnly("/proc/cpuinfo")};
    broker_process_ = std::make_unique<BrokerProcess>(
        syscall_broker::BrokerSandboxConfig(command_set, permissions, EPERM),
        broker_type);
    BPF_ASSERT(broker_process_->Fork(base::BindOnce(
        [](const syscall_broker::BrokerSandboxConfig&) { return true; })));
  }

  InitializedOpenBroker(const InitializedOpenBroker&) = delete;
  InitializedOpenBroker& operator=(const InitializedOpenBroker&) = delete;

  BrokerProcess* broker_process() const { return broker_process_.get(); }

 private:
  std::unique_ptr<BrokerProcess> broker_process_;
};

intptr_t BrokerOpenTrapHandler(const struct arch_seccomp_data& args,
                               void* aux) {
  BPF_ASSERT(aux);
  BrokerProcess* broker_process = static_cast<BrokerProcess*>(aux);
  switch (args.nr) {
    case __NR_faccessat:  // access is a wrapper of faccessat in android
    case __NR_faccessat2:
      BPF_ASSERT(static_cast<int>(args.args[0]) == AT_FDCWD);
      return broker_process->GetBrokerClientSignalBased()->Access(
          reinterpret_cast<const char*>(args.args[1]),
          static_cast<int>(args.args[2]));
#if defined(__NR_access)
    case __NR_access:
      return broker_process->GetBrokerClientSignalBased()->Access(
          reinterpret_cast<const char*>(args.args[0]),
          static_cast<int>(args.args[1]));
#endif
#if defined(__NR_open)
    case __NR_open:
      return broker_process->GetBrokerClientSignalBased()->Open(
          reinterpret_cast<const char*>(args.args[0]),
          static_cast<int>(args.args[1]));
#endif
    case __NR_openat:
      // We only call open() so if we arrive here, it's because glibc uses
      // the openat() system call.
      BPF_ASSERT(static_cast<int>(args.args[0]) == AT_FDCWD);
      return broker_process->GetBrokerClientSignalBased()->Open(
          reinterpret_cast<const char*>(args.args[1]),
          static_cast<int>(args.args[2]));
    default:
      BPF_ASSERT(false);
      return -ENOSYS;
  }
}

class DenyOpenPolicy : public bpf_dsl::Policy {
 public:
  explicit DenyOpenPolicy(InitializedOpenBroker* iob) : iob_(iob) {}

  DenyOpenPolicy(const DenyOpenPolicy&) = delete;
  DenyOpenPolicy& operator=(const DenyOpenPolicy&) = delete;

  ~DenyOpenPolicy() override {}

  ResultExpr EvaluateSyscall(int sysno) const override {
    DCHECK(SandboxBPF::IsValidSyscallNumber(sysno));

    switch (sysno) {
      case __NR_faccessat:
      case __NR_faccessat2:
#if defined(__NR_access)
      case __NR_access:
#endif
#if defined(__NR_open)
      case __NR_open:
#endif
      case __NR_openat:
        // We get a InitializedOpenBroker class, but our trap handler wants
        // the BrokerProcess object.
        return Trap(BrokerOpenTrapHandler, iob_->broker_process());
      default:
        return Allow();
    }
  }

 private:
  raw_ptr<InitializedOpenBroker> iob_;
};

// We use a InitializedOpenBroker class, so that we can run unsandboxed
// code in its constructor, which is the only way to do so in a BPF_TEST.
BPF_TEST(SandboxBPF,
         UseOpenBroker,
         DenyOpenPolicy,
         InitializedOpenBroker /* (*BPF_AUX) */) {
  BrokerProcess* broker_process = BPF_AUX->broker_process();
  BPF_ASSERT(broker_process != nullptr);

  // First, use the broker "manually"
  BPF_ASSERT(broker_process->GetBrokerClientSignalBased()->Open(
                 "/proc/denied", O_RDONLY) == -EPERM);
  BPF_ASSERT(broker_process->GetBrokerClientSignalBased()->Access(
                 "/proc/denied", R_OK) == -EPERM);
  BPF_ASSERT(broker_process->GetBrokerClientSignalBased()->Open(
                 "/proc/allowed", O_RDONLY) == -ENOENT);
  BPF_ASSERT(broker_process->GetBrokerClientSignalBased()->Access(
                 "/proc/allowed", R_OK) == -ENOENT);

  // Now use glibc's open() as an external library would.
  BPF_ASSERT(open("/proc/denied", O_RDONLY) == -1);
  BPF_ASSERT(errno == EPERM);

  BPF_ASSERT(open("/proc/allowed", O_RDONLY) == -1);
  BPF_ASSERT(errno == ENOENT);

  // Also test glibc's openat(), some versions of libc use it transparently
  // instead of open().
  BPF_ASSERT(openat(AT_FDCWD, "/proc/denied", O_RDONLY) == -1);
  BPF_ASSERT(errno == EPERM);

  BPF_ASSERT(openat(AT_FDCWD, "/proc/allowed", O_RDONLY) == -1);
  BPF_ASSERT(errno == ENOENT);

  // And test glibc's access().
  BPF_ASSERT(access("/proc/denied", R_OK) == -1);
  BPF_ASSERT(errno == EPERM);

  BPF_ASSERT(access("/proc/allowed", R_OK) == -1);
  BPF_ASSERT(errno == ENOENT);

  // This is also white listed and does exist.
  int cpu_info_access = access("/proc/cpuinfo", R_OK);
  BPF_ASSERT(cpu_info_access == 0);
  int cpu_info_fd = open("/proc/cpuinfo", O_RDONLY);
  BPF_ASSERT(cpu_info_fd >= 0);
  char buf[1024];
  BPF_ASSERT(HANDLE_EINTR(read(cpu_info_fd, buf, sizeof(buf))) > 0);
}

// The rest of the tests do not run under thread sanitizer, as TSAN starts up an
// extra thread which triggers a sandbox assertion. BPF_TESTs do not run under
// TSAN.
#if !defined(THREAD_SANITIZER)

namespace {
// Our fake errno must be less than 255 or various libc implementations will
// not accept this as a valid error number. E.g. bionic accepts up to 255, glibc
// and musl up to 4096.
const int kFakeErrnoSentinel = 254;

void ConvertKernelStatToLibcStat(default_stat_struct& in_stat,
                                 struct stat& out_stat) {
  out_stat.st_dev = in_stat.st_dev;
  out_stat.st_ino = in_stat.st_ino;
  out_stat.st_mode = in_stat.st_mode;
  out_stat.st_nlink = in_stat.st_nlink;
  out_stat.st_uid = in_stat.st_uid;
  out_stat.st_gid = in_stat.st_gid;
  out_stat.st_rdev = in_stat.st_rdev;
  out_stat.st_size = in_stat.st_size;
  out_stat.st_blksize = in_stat.st_blksize;
  out_stat.st_blocks = in_stat.st_blocks;
  out_stat.st_atim.tv_sec = in_stat.st_atime_;
  out_stat.st_atim.tv_nsec = in_stat.st_atime_nsec_;
  out_stat.st_mtim.tv_sec = in_stat.st_mtime_;
  out_stat.st_mtim.tv_nsec = in_stat.st_mtime_nsec_;
  out_stat.st_ctim.tv_sec = in_stat.st_ctime_;
  out_stat.st_ctim.tv_nsec = in_stat.st_ctime_nsec_;
}
}  // namespace

// There are a variety of ways to make syscalls in a sandboxed process. One is
// to directly make the syscalls, one is to make the syscalls through libc
// (which oftens uses different underlying syscalls per platform and kernel
// version). With the signals-based broker, the sandboxed process can also make
// calls directly to the broker over the existing IPC channel.
// This interface encompasses the available syscalls so we can test every method
// of making syscalls.
class Syscaller {
 public:
  virtual ~Syscaller() = default;

  virtual int Open(const char* filepath, int flags) = 0;
  virtual int Access(const char* filepath, int mode) = 0;
  // NOTE: we use struct stat instead of default_stat_struct, to make the libc
  // syscaller simpler. Copying from default_stat_struct (the structure returned
  // from a stat sycall) to struct stat (the structure exposed by a libc to its
  // users) is simpler than going in the opposite direction.
  virtual int Stat(const char* filepath,
                   bool follow_links,
                   struct stat* statbuf) = 0;
  virtual int Rename(const char* oldpath, const char* newpath) = 0;
  virtual int Readlink(const char* path, char* buf, size_t bufsize) = 0;
  virtual int Mkdir(const char* pathname, mode_t mode) = 0;
  virtual int Rmdir(const char* path) = 0;
  virtual int Unlink(const char* path) = 0;
  virtual int InotifyAddWatch(int fd, const char* path, uint32_t mask) = 0;
};

class IPCSyscaller : public Syscaller {
 public:
  explicit IPCSyscaller(BrokerProcess* broker) : broker_(broker) {}
  ~IPCSyscaller() override = default;

  int Open(const char* filepath, int flags) override {
    return broker_->GetBrokerClientSignalBased()->Open(filepath, flags);
  }

  int Access(const char* filepath, int mode) override {
    return broker_->GetBrokerClientSignalBased()->Access(filepath, mode);
  }

  int Stat(const char* filepath,
           bool follow_links,
           struct stat* statbuf) override {
    default_stat_struct buf;
    int ret = broker_->GetBrokerClientSignalBased()->DefaultStatForTesting(
        filepath, follow_links, &buf);
    if (ret >= 0)
      ConvertKernelStatToLibcStat(buf, *statbuf);
    return ret;
  }

  int Rename(const char* oldpath, const char* newpath) override {
    return broker_->GetBrokerClientSignalBased()->Rename(oldpath, newpath);
  }

  int Readlink(const char* path, char* buf, size_t bufsize) override {
    return broker_->GetBrokerClientSignalBased()->Readlink(path, buf, bufsize);
  }

  int Mkdir(const char* pathname, mode_t mode) override {
    return broker_->GetBrokerClientSignalBased()->Mkdir(pathname, mode);
  }

  int Rmdir(const char* path) override {
    return broker_->GetBrokerClientSignalBased()->Rmdir(path);
  }

  int Unlink(const char* path) override {
    return broker_->GetBrokerClientSignalBased()->Unlink(path);
  }

  int InotifyAddWatch(int fd, const char* path, uint32_t mask) override {
    return broker_->GetBrokerClientSignalBased()->InotifyAddWatch(fd, path,
                                                                  mask);
  }

 private:
  raw_ptr<BrokerProcess> broker_;
};

// Only use syscall(...) on x64 to avoid having to reimplement a libc-like
// layer that uses different syscalls on different architectures.
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
     BUILDFLAG(IS_ANDROID)) &&                        \
    defined(__x86_64__)
#define DIRECT_SYSCALLER_ENABLED
#endif

#if defined(DIRECT_SYSCALLER_ENABLED)
class DirectSyscaller : public Syscaller {
 public:
  ~DirectSyscaller() override = default;

  int Open(const char* filepath, int flags) override {
    int ret = syscall(__NR_open, filepath, flags);
    if (ret < 0)
      return -errno;
    return ret;
  }

  int Access(const char* filepath, int mode) override {
    int ret = syscall(__NR_access, filepath, mode);
    if (ret < 0)
      return -errno;
    return ret;
  }

  int Stat(const char* filepath,
           bool follow_links,
           struct stat* statbuf) override {
    struct kernel_stat buf;
    int ret = syscall(__NR_newfstatat, AT_FDCWD, filepath, &buf,
                      follow_links ? 0 : AT_SYMLINK_NOFOLLOW);
    if (ret < 0)
      return -errno;

    ConvertKernelStatToLibcStat(buf, *statbuf);
    return ret;
  }

  int Rename(const char* oldpath, const char* newpath) override {
    int ret = syscall(__NR_rename, oldpath, newpath);
    if (ret < 0)
      return -errno;
    return ret;
  }

  int Readlink(const char* path, char* buf, size_t bufsize) override {
    int ret = syscall(__NR_readlink, path, buf, bufsize);
    if (ret < 0)
      return -errno;
    return ret;
  }

  int Mkdir(const char* pathname, mode_t mode) override {
    int ret = syscall(__NR_mkdir, pathname, mode);
    if (ret < 0)
      return -errno;
    return ret;
  }

  int Rmdir(const char* path) override {
    int ret = syscall(__NR_rmdir, path);
    if (ret < 0)
      return -errno;
    return ret;
  }

  int Unlink(const char* path) override {
    int ret = syscall(__NR_unlink, path);
    if (ret < 0)
      return -errno;
    return ret;
  }

  int InotifyAddWatch(int fd, const char* path, uint32_t mask) override {
    int ret = syscall(__NR_inotify_add_watch, fd, path, mask);
    if (ret < 0)
      return -errno;
    return ret;
  }
};
#endif  // defined(DIRECT_SYSCALLER_ENABLED)

class LibcSyscaller : public Syscaller {
 public:
  ~LibcSyscaller() override = default;

  int Open(const char* filepath, int flags) override {
    int ret = HANDLE_EINTR(open(filepath, flags, 0600));
    if (ret < 0)
      return -errno;
    return ret;
  }
  int Access(const char* filepath, int mode) override {
    int ret = access(filepath, mode);
    if (ret < 0)
      return -errno;
    return ret;
  }

  int Stat(const char* filepath,
           bool follow_links,
           struct stat* statbuf) override {
    int ret = follow_links ? stat(filepath, statbuf) : lstat(filepath, statbuf);
    if (ret < 0)
      return -errno;
    return ret;
  }

  int Rename(const char* oldpath, const char* newpath) override {
    int ret = rename(oldpath, newpath);
    if (ret < 0)
      return -errno;
    return ret;
  }

  int Readlink(const char* path, char* buf, size_t bufsize) override {
    int ret = readlink(path, buf, bufsize);
    if (ret < 0)
      return -errno;
    return ret;
  }

  int Mkdir(const char* pathname, mode_t mode) override {
    int ret = mkdir(pathname, mode);
    if (ret < 0)
      return -errno;
    return ret;
  }

  int Rmdir(const char* path) override {
    int ret = rmdir(path);
    if (ret < 0)
      return -errno;
    return ret;
  }

  int Unlink(const char* path) override {
    int ret = unlink(path);
    if (ret < 0)
      return -errno;
    return ret;
  }

  int InotifyAddWatch(int fd, const char* path, uint32_t mask) override {
    int ret = inotify_add_watch(fd, path, mask);
    if (ret < 0)
      return -errno;
    return ret;
  }
};

enum class SyscallerType {
  IPCSyscaller = 0,
  DirectSyscaller,
  LibcSyscaller,
  NoSyscaller
};

// The testing infrastructure for the broker integration tests is built on the
// same infrastructure that BPF_TEST or SANDBOX_TEST uses. Each individual test
// starts up a child process, which itself starts up a broker process and
// sandboxes itself. To create a test, implement this delegate and call
// RunAllBrokerTests() in a TEST(). The bulk of the test body will be in
// RunTestInSandboxedChild().
class BrokerTestDelegate {
 public:
  struct BrokerParams {
    int denied_errno = kFakeErrnoSentinel;
    syscall_broker::BrokerCommandSet allowed_command_set;
    std::vector<BrokerFilePermission> permissions;
  };

  virtual ~BrokerTestDelegate() = default;

  // Called in the parent test process before starting the child process. Should
  // use GTEST's ASSERT macros.
  virtual void ParentSetUp() {}

  // Sets up the test in the child process before applying the sandbox.
  // |allowed_command_set| and |permissions| should be filled in with the
  // desired commands and permissions the broker should allow. Should use
  // BPF_ASSERT.
  virtual BrokerParams ChildSetUpPreSandbox() = 0;

  // Gets called in the sandboxed process with the pid of the newly started
  // broker.
  virtual void OnBrokerStarted(pid_t broker_pid) {}

  // Runs the test after setting up the sandbox in the forked process.
  // Assertions should use BPF_ASSERT.
  virtual void RunTestInSandboxedChild(Syscaller* syscaller) = 0;

  // Called in the parent test process after the child dies. Should perform
  // cleanup, and can also ASSERT like a normal GTEST. Note that modifications
  // of class state in the above two functions will not be visible here, as they
  // ran in the forked child.
  virtual void ParentTearDown() {
    ASSERT_FALSE(TestUtils::CurrentProcessHasChildren());
  }
};

namespace syscall_broker {
// A BPF policy for our BPF_TEST that defers to the broker for syscalls that
// take paths, and allows everything else.
class HandleFilesystemViaBrokerPolicy : public bpf_dsl::Policy {
 public:
  explicit HandleFilesystemViaBrokerPolicy(BrokerProcess* broker_process,
                                           int denied_errno)
      : broker_process_(broker_process), denied_errno_(denied_errno) {}

  HandleFilesystemViaBrokerPolicy(const HandleFilesystemViaBrokerPolicy&) =
      delete;
  HandleFilesystemViaBrokerPolicy& operator=(
      const HandleFilesystemViaBrokerPolicy&) = delete;

  ~HandleFilesystemViaBrokerPolicy() override = default;

  ResultExpr EvaluateSyscall(int sysno) const override {
    DCHECK(SandboxBPF::IsValidSyscallNumber(sysno));
    // Broker everything that we're supposed to broker.
    if (broker_process_->IsSyscallAllowed(sysno)) {
      return Trap(BrokerClient::SIGSYS_Handler,
                  broker_process_->GetBrokerClientSignalBased());
    }

    // Otherwise, if this is a syscall that takes a pathname but isn't an
    // allowed command, deny it.
    if (broker_process_->IsSyscallBrokerable(sysno,
                                             /*fast_check_in_client=*/false)) {
      return Error(denied_errno_);
    }

    if (sysno == __NR_statx) {
      const Arg<int> mask(3);
      return If(mask == STATX_BASIC_STATS, Error(ENOSYS))
          .Else(Error(denied_errno_));
    }

    // Allow everything else that doesn't take a pathname.
    return Allow();
  }

 private:
  raw_ptr<BrokerProcess> broker_process_;
  int denied_errno_;
};
}  // namespace syscall_broker

// This implements BPFTesterDelegate to layer the broker integration tests on
// top of BPF_TEST infrastructure.
class BPFTesterBrokerDelegate : public BPFTesterDelegate {
 public:
  explicit BPFTesterBrokerDelegate(bool fast_check_in_client,
                                   BrokerTestDelegate* broker_test_delegate,
                                   SyscallerType syscaller_type,
                                   BrokerType broker_type)
      : fast_check_in_client_(fast_check_in_client),
        broker_test_delegate_(broker_test_delegate),
        syscaller_type_(syscaller_type),
        broker_type_(broker_type) {}
  ~BPFTesterBrokerDelegate() override = default;

  std::unique_ptr<bpf_dsl::Policy> GetSandboxBPFPolicy() override {
    BrokerTestDelegate::BrokerParams broker_params =
        broker_test_delegate_->ChildSetUpPreSandbox();

    auto policy = std::make_optional<syscall_broker::BrokerSandboxConfig>(
        broker_params.allowed_command_set, broker_params.permissions,
        broker_params.denied_errno);
    broker_process_ = std::make_unique<BrokerProcess>(
        std::move(policy), broker_type_, fast_check_in_client_);
    BPF_ASSERT(broker_process_->Fork(base::BindOnce(
        [](const syscall_broker::BrokerSandboxConfig&) { return true; })));
    broker_test_delegate_->OnBrokerStarted(broker_process_->broker_pid());

    BPF_ASSERT(TestUtils::CurrentProcessHasChildren());

    CreateSyscaller();

    return std::unique_ptr<bpf_dsl::Policy>(
        new syscall_broker::HandleFilesystemViaBrokerPolicy(
            broker_process_.get(), broker_params.denied_errno));
  }

  void RunTestFunction() override {
    broker_test_delegate_->RunTestInSandboxedChild(syscaller_.get());
  }

  void CreateSyscaller() {
    BPF_ASSERT(broker_process_->GetBrokerClientSignalBased());
    switch (syscaller_type_) {
      case SyscallerType::IPCSyscaller:
        syscaller_ = std::make_unique<IPCSyscaller>(broker_process_.get());
        break;
      case SyscallerType::DirectSyscaller:
#if defined(DIRECT_SYSCALLER_ENABLED)
        syscaller_ = std::make_unique<DirectSyscaller>();
#else
        CHECK(false) << "Requested instantiation of DirectSyscaller on a "
                        "platform that doesn't support it";
#endif
        break;
      case SyscallerType::LibcSyscaller:
        syscaller_ = std::make_unique<LibcSyscaller>();
        break;
      case SyscallerType::NoSyscaller:
        syscaller_ = nullptr;
        break;
    }
  }

 private:
  bool fast_check_in_client_;
  raw_ptr<BrokerTestDelegate> broker_test_delegate_;
  SyscallerType syscaller_type_;
  BrokerType broker_type_;

  std::unique_ptr<BrokerProcess> broker_process_;
  std::unique_ptr<Syscaller> syscaller_;
};

namespace {
struct BrokerTestConfiguration {
  std::string test_name;
  bool fast_check_in_client;
  SyscallerType syscaller_type;
  BrokerType broker_type;
};

// Lists out all the broker configurations we want to test.
const std::vector<BrokerTestConfiguration> broker_test_configs = {
    {"FastCheckInClient_IPCSyscaller", true, SyscallerType::IPCSyscaller,
     BrokerType::SIGNAL_BASED},
#if defined(DIRECT_SYSCALLER_ENABLED)
    {"FastCheckInClient_DirectSyscaller", true, SyscallerType::DirectSyscaller,
     BrokerType::SIGNAL_BASED},
#endif
    {"FastCheckInClient_LibcSyscaller", true, SyscallerType::LibcSyscaller,
     BrokerType::SIGNAL_BASED},
    {"NoFastCheckInClient_IPCSyscaller", false, SyscallerType::IPCSyscaller,
     BrokerType::SIGNAL_BASED},
#if defined(DIRECT_SYSCALLER_ENABLED)
    {"NoFastCheckInClient_DirectSyscaller", false,
     SyscallerType::DirectSyscaller, BrokerType::SIGNAL_BASED},
#endif
    {"NoFastCheckInClient_LibcSyscaller", false, SyscallerType::LibcSyscaller,
     BrokerType::SIGNAL_BASED}};
}  // namespace

void RunSingleBrokerTest(BrokerTestDelegate* test_delegate,
                         const BrokerTestConfiguration& test_config) {
  test_delegate->ParentSetUp();
  sandbox::SandboxBPFTestRunner bpf_test_runner(new BPFTesterBrokerDelegate(
      test_config.fast_check_in_client, test_delegate,
      test_config.syscaller_type, test_config.broker_type));
  sandbox::UnitTests::RunTestInProcess(&bpf_test_runner, DEATH_SUCCESS());
  test_delegate->ParentTearDown();
}

template <typename T>
void RunAllBrokerTests() {
  for (const BrokerTestConfiguration& test_config : broker_test_configs) {
    SCOPED_TRACE(test_config.test_name);
    auto test_delegate = std::make_unique<T>();
    RunSingleBrokerTest(test_delegate.get(), test_config);
  }
}

template <typename T>
void RunIPCBrokerTests() {
  for (const BrokerTestConfiguration& test_config : broker_test_configs) {
    if (test_config.syscaller_type != SyscallerType::IPCSyscaller)
      continue;

    SCOPED_TRACE(test_config.test_name);
    auto test_delegate = std::make_unique<T>();
    RunSingleBrokerTest(test_delegate.get(), test_config);
  }
}

// Tests that a SIGNALS_BASED broker responds with -EFAULT when open() or
// access() are called with nullptr.
class TestOpenAccessNullDelegate final : public BrokerTestDelegate {
 public:
  BrokerParams ChildSetUpPreSandbox() override {
    BrokerParams params;
    params.allowed_command_set = syscall_broker::MakeBrokerCommandSet(
        {syscall_broker::COMMAND_ACCESS, syscall_broker::COMMAND_OPEN});
    return params;
  }

  void RunTestInSandboxedChild(Syscaller* syscaller) override {
    int fd = syscaller->Open(nullptr, O_RDONLY);
    BPF_ASSERT_EQ(fd, -EFAULT);

    int ret = syscaller->Access(nullptr, F_OK);
    BPF_ASSERT_EQ(ret, -EFAULT);
  }
};

TEST(BrokerProcessIntegrationTest, TestOpenAccessNull) {
  RunIPCBrokerTests<TestOpenAccessNullDelegate>();
}

// Tests open()/access() for files that do not exist, are not allowed by
// allowlist, and are allowed by allowlist but not accessible.
template <int DENIED_ERRNO>
class TestOpenFilePermsDelegate final : public BrokerTestDelegate {
 public:
  BrokerParams ChildSetUpPreSandbox() override {
    BrokerParams params;
    params.denied_errno = DENIED_ERRNO;

    params.allowed_command_set = syscall_broker::MakeBrokerCommandSet(
        {syscall_broker::COMMAND_ACCESS, syscall_broker::COMMAND_OPEN});

    params.permissions = {
        BrokerFilePermission::ReadOnly(kR_AllowListed),
        BrokerFilePermission::ReadOnly(kR_AllowListedButDenied),
        BrokerFilePermission::WriteOnly(kW_AllowListed),
        BrokerFilePermission::ReadWrite(kRW_AllowListed)};
    return params;
  }

  void RunTestInSandboxedChild(Syscaller* syscaller) override {
    int fd = -1;
    fd = syscaller->Open(kR_AllowListed, O_RDONLY);
    BPF_ASSERT_EQ(fd, -ENOENT);
    fd = syscaller->Open(kR_AllowListed, O_WRONLY);
    BPF_ASSERT_EQ(fd, -DENIED_ERRNO);
    fd = syscaller->Open(kR_AllowListed, O_RDWR);
    BPF_ASSERT_EQ(fd, -DENIED_ERRNO);
    int ret = -1;
    ret = syscaller->Access(kR_AllowListed, F_OK);
    BPF_ASSERT_EQ(ret, -ENOENT);
    ret = syscaller->Access(kR_AllowListed, R_OK);
    BPF_ASSERT_EQ(ret, -ENOENT);
    ret = syscaller->Access(kR_AllowListed, W_OK);
    BPF_ASSERT_EQ(ret, -DENIED_ERRNO);
    ret = syscaller->Access(kR_AllowListed, R_OK | W_OK);
    BPF_ASSERT_EQ(ret, -DENIED_ERRNO);
    ret = syscaller->Access(kR_AllowListed, X_OK);
    BPF_ASSERT_EQ(ret, -DENIED_ERRNO);
    ret = syscaller->Access(kR_AllowListed, R_OK | X_OK);
    BPF_ASSERT_EQ(ret, -DENIED_ERRNO);

    // Android sometimes runs tests as root.
    // This part of the test requires a process that doesn't have
    // CAP_DAC_OVERRIDE. We check against a root euid as a proxy for that.
    if (geteuid()) {
      fd = syscaller->Open(kR_AllowListedButDenied, O_RDONLY);
      // The broker process will allow this, but the normal permission system
      // won't.
      BPF_ASSERT_EQ(fd, -EACCES);
      fd = syscaller->Open(kR_AllowListedButDenied, O_WRONLY);
      BPF_ASSERT_EQ(fd, -DENIED_ERRNO);
      fd = syscaller->Open(kR_AllowListedButDenied, O_RDWR);
      BPF_ASSERT_EQ(fd, -DENIED_ERRNO);
      ret = syscaller->Access(kR_AllowListedButDenied, F_OK);
      // The normal permission system will let us check that the file exists.
      BPF_ASSERT_EQ(ret, 0);
      ret = syscaller->Access(kR_AllowListedButDenied, R_OK);
      BPF_ASSERT_EQ(ret, -EACCES);
      ret = syscaller->Access(kR_AllowListedButDenied, W_OK);
      BPF_ASSERT_EQ(ret, -DENIED_ERRNO);
      ret = syscaller->Access(kR_AllowListedButDenied, R_OK | W_OK);
      BPF_ASSERT_EQ(ret, -DENIED_ERRNO);
      ret = syscaller->Access(kR_AllowListedButDenied, X_OK);
      BPF_ASSERT_EQ(ret, -DENIED_ERRNO);
      ret = syscaller->Access(kR_AllowListedButDenied, R_OK | X_OK);
      BPF_ASSERT_EQ(ret, -DENIED_ERRNO);
    }

    fd = syscaller->Open(kW_AllowListed, O_RDONLY);
    BPF_ASSERT_EQ(fd, -DENIED_ERRNO);
    fd = syscaller->Open(kW_AllowListed, O_WRONLY);
    BPF_ASSERT_EQ(fd, -ENOENT);
    fd = syscaller->Open(kW_AllowListed, O_RDWR);
    BPF_ASSERT_EQ(fd, -DENIED_ERRNO);
    ret = syscaller->Access(kW_AllowListed, F_OK);
    BPF_ASSERT_EQ(ret, -ENOENT);
    ret = syscaller->Access(kW_AllowListed, R_OK);
    BPF_ASSERT_EQ(ret, -DENIED_ERRNO);
    ret = syscaller->Access(kW_AllowListed, W_OK);
    BPF_ASSERT_EQ(ret, -ENOENT);
    ret = syscaller->Access(kW_AllowListed, R_OK | W_OK);
    BPF_ASSERT_EQ(ret, -DENIED_ERRNO);
    ret = syscaller->Access(kW_AllowListed, X_OK);
    BPF_ASSERT_EQ(ret, -DENIED_ERRNO);
    ret = syscaller->Access(kW_AllowListed, R_OK | X_OK);
    BPF_ASSERT_EQ(ret, -DENIED_ERRNO);

    fd = syscaller->Open(kRW_AllowListed, O_RDONLY);
    BPF_ASSERT_EQ(fd, -ENOENT);
    fd = syscaller->Open(kRW_AllowListed, O_WRONLY);
    BPF_ASSERT_EQ(fd, -ENOENT);
    fd = syscaller->Open(kRW_AllowListed, O_RDWR);
    BPF_ASSERT_EQ(fd, -ENOENT);
    ret = syscaller->Access(kRW_AllowListed, F_OK);
    BPF_ASSERT_EQ(ret, -ENOENT);
    ret = syscaller->Access(kRW_AllowListed, R_OK);
    BPF_ASSERT_EQ(ret, -ENOENT);
    ret = syscaller->Access(kRW_AllowListed, W_OK);
    BPF_ASSERT_EQ(ret, -ENOENT);
    ret = syscaller->Access(kRW_AllowListed, R_OK | W_OK);
    BPF_ASSERT_EQ(ret, -ENOENT);
    ret = syscaller->Access(kRW_AllowListed, X_OK);
    BPF_ASSERT_EQ(ret, -DENIED_ERRNO);
    ret = syscaller->Access(kRW_AllowListed, R_OK | X_OK);
    BPF_ASSERT_EQ(ret, -DENIED_ERRNO);

    fd = syscaller->Open(k_NotAllowlisted, O_RDONLY);
    BPF_ASSERT_EQ(fd, -DENIED_ERRNO);
    fd = syscaller->Open(k_NotAllowlisted, O_WRONLY);
    BPF_ASSERT_EQ(fd, -DENIED_ERRNO);
    fd = syscaller->Open(k_NotAllowlisted, O_RDWR);
    BPF_ASSERT_EQ(fd, -DENIED_ERRNO);
    ret = syscaller->Access(k_NotAllowlisted, F_OK);
    BPF_ASSERT_EQ(ret, -DENIED_ERRNO);
    ret = syscaller->Access(k_NotAllowlisted, R_OK);
    BPF_ASSERT_EQ(ret, -DENIED_ERRNO);
    ret = syscaller->Access(k_NotAllowlisted, W_OK);
    BPF_ASSERT_EQ(ret, -DENIED_ERRNO);
    ret = syscaller->Access(k_NotAllowlisted, R_OK | W_OK);
    BPF_ASSERT_EQ(ret, -DENIED_ERRNO);
    ret = syscaller->Access(k_NotAllowlisted, X_OK);
    BPF_ASSERT_EQ(ret, -DENIED_ERRNO);
    ret = syscaller->Access(k_NotAllowlisted, R_OK | X_OK);
    BPF_ASSERT_EQ(ret, -DENIED_ERRNO);

    // We have some extra sanity check for clearly wrong values.
    fd = syscaller->Open(kRW_AllowListed, O_RDONLY | O_WRONLY | O_RDWR);
    BPF_ASSERT_EQ(fd, -DENIED_ERRNO);

    // It makes no sense to allow O_CREAT in a 2-parameters open. Ensure this
    // is denied.
    fd = syscaller->Open(kRW_AllowListed, O_RDWR | O_CREAT);
    BPF_ASSERT_EQ(fd, -DENIED_ERRNO);
  }

 private:
  const char* const kR_AllowListed = "/proc/DOESNOTEXIST1";
  // We can't debug the init process, and shouldn't be able to access
  // its auxv file.
  const char* kR_AllowListedButDenied = "/proc/1/auxv";
  const char* kW_AllowListed = "/proc/DOESNOTEXIST2";
  const char* kRW_AllowListed = "/proc/DOESNOTEXIST3";
  const char* k_NotAllowlisted = "/proc/DOESNOTEXIST4";
};

TEST(BrokerProcessIntegrationTest, TestOpenFilePermsEPERM) {
  RunAllBrokerTests<TestOpenFilePermsDelegate<EPERM>>();
}

TEST(BrokerProcessIntegrationTest, TestOpenFilePermsENOENT) {
  RunAllBrokerTests<TestOpenFilePermsDelegate<ENOENT>>();
}

class BadPathsDelegate final : public BrokerTestDelegate {
 public:
  BrokerParams ChildSetUpPreSandbox() override {
    BrokerParams params;
    params.allowed_command_set = syscall_broker::MakeBrokerCommandSet(
        {syscall_broker::COMMAND_ACCESS, syscall_broker::COMMAND_OPEN});
    params.permissions = {BrokerFilePermission::ReadOnlyRecursive("/proc/")};
    return params;
  }

  void RunTestInSandboxedChild(Syscaller* syscaller) override {
    // Open cpuinfo via the broker.
    int cpuinfo_fd = syscaller->Open(kFileCpuInfo, O_RDONLY);
    base::ScopedFD cpuinfo_fd_closer(cpuinfo_fd);
    BPF_ASSERT_GE(cpuinfo_fd, 0);

    int fd = -1;
    int can_access;

    can_access = syscaller->Access(kNotAbsPath, R_OK);
    BPF_ASSERT_EQ(can_access, -kFakeErrnoSentinel);
    fd = syscaller->Open(kNotAbsPath, O_RDONLY);
    BPF_ASSERT_EQ(fd, -kFakeErrnoSentinel);

    can_access = syscaller->Access(kDotDotStart, R_OK);
    BPF_ASSERT_EQ(can_access, -kFakeErrnoSentinel);
    fd = syscaller->Open(kDotDotStart, O_RDONLY);
    BPF_ASSERT_EQ(fd, -kFakeErrnoSentinel);

    can_access = syscaller->Access(kDotDotMiddle, R_OK);
    BPF_ASSERT_EQ(can_access, -kFakeErrnoSentinel);
    fd = syscaller->Open(kDotDotMiddle, O_RDONLY);
    BPF_ASSERT_EQ(fd, -kFakeErrnoSentinel);

    can_access = syscaller->Access(kDotDotEnd, R_OK);
    BPF_ASSERT_EQ(can_access, -kFakeErrnoSentinel);
    fd = syscaller->Open(kDotDotEnd, O_RDONLY);
    BPF_ASSERT_EQ(fd, -kFakeErrnoSentinel);

    can_access = syscaller->Access(kTrailingSlash, R_OK);
    BPF_ASSERT_EQ(can_access, -kFakeErrnoSentinel);
    fd = syscaller->Open(kTrailingSlash, O_RDONLY);
    BPF_ASSERT_EQ(fd, -kFakeErrnoSentinel);
  }

 private:
  const char* const kFileCpuInfo = "/proc/cpuinfo";
  const char* const kNotAbsPath = "proc/cpuinfo";
  const char* const kDotDotStart = "/../proc/cpuinfo";
  const char* const kDotDotMiddle = "/proc/self/../cpuinfo";
  const char* const kDotDotEnd = "/proc/..";
  const char* const kTrailingSlash = "/proc/";
};

TEST(BrokerProcessIntegrationTest, BadPaths) {
  RunAllBrokerTests<BadPathsDelegate>();
}

template <bool recursive>
class OpenCpuinfoDelegate final : public BrokerTestDelegate {
 public:
  BrokerParams ChildSetUpPreSandbox() override {
    // Open cpuinfo directly.
    int cpu_info_fd = HANDLE_EINTR(open(kFileCpuInfo, O_RDONLY));
    BPF_ASSERT_GE(cpu_info_fd, 0);
    memset(cpuinfo_buf_, 1, sizeof(cpuinfo_buf_));
    read_len_unsandboxed_ =
        HANDLE_EINTR(read(cpu_info_fd, cpuinfo_buf_, sizeof(cpuinfo_buf_)));
    BPF_ASSERT_GT(read_len_unsandboxed_, 0);

    BrokerParams params;
    params.allowed_command_set = syscall_broker::MakeBrokerCommandSet(
        {syscall_broker::COMMAND_ACCESS, syscall_broker::COMMAND_OPEN});
    params.permissions.push_back(
        recursive ? BrokerFilePermission::ReadOnlyRecursive(kDirProc)
                  : BrokerFilePermission::ReadOnly(kFileCpuInfo));
    return params;
  }

  void RunTestInSandboxedChild(Syscaller* syscaller) override {
    int fd = syscaller->Open(kFileCpuInfo, O_RDWR);
    BPF_ASSERT_EQ(fd, -kFakeErrnoSentinel);

    // Check we can read /proc/cpuinfo.
    int can_access = syscaller->Access(kFileCpuInfo, R_OK);
    BPF_ASSERT_EQ(can_access, 0);
    can_access = syscaller->Access(kFileCpuInfo, W_OK);
    BPF_ASSERT_EQ(can_access, -kFakeErrnoSentinel);
    // Check we can not write /proc/cpuinfo.

    // Open cpuinfo via the broker.
    int cpuinfo_fd = syscaller->Open(kFileCpuInfo, O_RDONLY);
    base::ScopedFD cpuinfo_fd_closer(cpuinfo_fd);
    BPF_ASSERT_GE(cpuinfo_fd, 0);
    char buf[3];
    memset(buf, 0, sizeof(buf));
    int read_len_sandboxed = HANDLE_EINTR(read(cpuinfo_fd, buf, sizeof(buf)));
    BPF_ASSERT_GT(read_len_sandboxed, 0);

    // The following is not guaranteed true, but will be in practice.
    BPF_ASSERT_EQ(read_len_sandboxed, read_len_unsandboxed_);
    // Compare the cpuinfo as returned by the broker with the one we opened
    // ourselves.
    BPF_ASSERT_EQ(memcmp(buf, cpuinfo_buf_, read_len_sandboxed), 0);
  }

 private:
  const char* const kFileCpuInfo = "/proc/cpuinfo";
  const char* const kDirProc = "/proc/";

  int read_len_unsandboxed_ = -1;
  char cpuinfo_buf_[3];
};

TEST(BrokerProcessIntegrationTest, OpenCpuinfoRecursive) {
  RunAllBrokerTests<OpenCpuinfoDelegate</*recursive=*/true>>();
}

TEST(BrokerProcessIntegrationTest, OpenCpuinfoNonRecursive) {
  RunAllBrokerTests<OpenCpuinfoDelegate</*recursive=*/false>>();
}

class OpenFileRWDelegate final : public BrokerTestDelegate {
 public:
  OpenFileRWDelegate() : tempfile_name_(tempfile.full_file_name()) {}

  BrokerParams ChildSetUpPreSandbox() override {
    BrokerParams params;
    params.allowed_command_set = syscall_broker::MakeBrokerCommandSet(
        {syscall_broker::COMMAND_ACCESS, syscall_broker::COMMAND_OPEN});
    params.permissions = {BrokerFilePermission::ReadWrite(tempfile_name_)};
    return params;
  }

  void RunTestInSandboxedChild(Syscaller* syscaller) override {
    // Check we can access that file with read or write.
    int can_access = syscaller->Access(tempfile_name_, R_OK | W_OK);
    BPF_ASSERT_EQ(can_access, 0);

    int tempfile2 = -1;
    tempfile2 = syscaller->Open(tempfile_name_, O_RDWR);
    BPF_ASSERT_GE(tempfile2, 0);

    // Write to the descriptor opened by the broker.
    char test_text[] = "TESTTESTTEST";

    ssize_t len = HANDLE_EINTR(write(tempfile2, test_text, sizeof(test_text)));
    BPF_ASSERT_EQ(len, static_cast<ssize_t>(sizeof(test_text)));

    // Read back from the original file descriptor what we wrote through
    // the descriptor provided by the broker.
    char buf[1024];
    len = HANDLE_EINTR(read(tempfile.fd(), buf, sizeof(buf)));

    BPF_ASSERT_EQ(len, static_cast<ssize_t>(sizeof(test_text)));
    BPF_ASSERT_EQ(memcmp(test_text, buf, sizeof(test_text)), 0);

    BPF_ASSERT_EQ(close(tempfile2), 0);
  }

 private:
  ScopedTemporaryFile tempfile;
  const char* const tempfile_name_;
};

TEST(BrokerProcessIntegrationTest, OpenFileRW) {
  RunAllBrokerTests<OpenFileRWDelegate>();
}

class BrokerDiedDelegate final : public BrokerTestDelegate {
 public:
  BrokerParams ChildSetUpPreSandbox() override {
    BrokerParams params;
    params.allowed_command_set = syscall_broker::MakeBrokerCommandSet(
        {syscall_broker::COMMAND_ACCESS, syscall_broker::COMMAND_OPEN});
    params.permissions = {BrokerFilePermission::ReadOnly(kCpuInfo)};
    return params;
  }

  void RunTestInSandboxedChild(Syscaller* syscaller) override {
    BPF_ASSERT_GT(broker_pid_, 0);

    // Kill our own broker and check that we get ENOSYS or ENOMEM for our
    // syscalls.
    BPF_ASSERT_EQ(kill(broker_pid_, SIGKILL), 0);

    int ret = syscaller->Access(kCpuInfo, O_RDONLY);
    BPF_ASSERT(ret == -ENOSYS || ret == -ENOMEM);

    ret = syscaller->Open(kCpuInfo, O_RDONLY);
    BPF_ASSERT(ret == -ENOSYS || ret == -ENOMEM);
  }

  void OnBrokerStarted(pid_t pid) override { broker_pid_ = pid; }

 private:
  const char* const kCpuInfo = "/proc/cpuinfo";

  pid_t broker_pid_ = -1;
};

TEST(BrokerProcessIntegrationTest, BrokerDied) {
  RunAllBrokerTests<BrokerDiedDelegate>();
}

class OpenComplexFlagsDelegate final : public BrokerTestDelegate {
 public:
  BrokerParams ChildSetUpPreSandbox() override {
    BrokerParams params;
    params.allowed_command_set = syscall_broker::MakeBrokerCommandSet(
        {syscall_broker::COMMAND_ACCESS, syscall_broker::COMMAND_OPEN});
    params.permissions = {BrokerFilePermission::ReadOnly(kCpuInfo)};
    return params;
  }

  void RunTestInSandboxedChild(Syscaller* syscaller) override {
    // Tests that files opened without O_CLOEXEC (resp. O_NONBLOCK) do not have
    // O_CLOEXEC (resp. O_NONBLOCK) on their file description, and vice versa.
    int fd = -1;
    int ret = 0;
    fd = syscaller->Open(kCpuInfo, O_RDONLY);
    BPF_ASSERT_GE(fd, 0);
    ret = fcntl(fd, F_GETFL);
    BPF_ASSERT_NE(-1, ret);
    // The description shouldn't have the O_CLOEXEC attribute, nor O_NONBLOCK.
    BPF_ASSERT_EQ(0, ret & (O_CLOEXEC | O_NONBLOCK));
    ret = fcntl(fd, F_GETFD);
    BPF_ASSERT_NE(-1, ret);
    // The descriptor also should not have FD_CLOEXEC.
    BPF_ASSERT_EQ(FD_CLOEXEC & ret, 0);
    BPF_ASSERT_EQ(0, close(fd));

    fd = syscaller->Open(kCpuInfo, O_RDONLY | O_CLOEXEC);
    BPF_ASSERT_GE(fd, 0);
    ret = fcntl(fd, F_GETFD);
    BPF_ASSERT_NE(-1, ret);
    // Important: use F_GETFD, not F_GETFL. The O_CLOEXEC flag in F_GETFL
    // is actually not used by the kernel.
    BPF_ASSERT(FD_CLOEXEC & ret);
    BPF_ASSERT_EQ(0, close(fd));

    fd = syscaller->Open(kCpuInfo, O_RDONLY | O_NONBLOCK);
    BPF_ASSERT_GE(fd, 0);
    ret = fcntl(fd, F_GETFL);
    BPF_ASSERT_NE(-1, ret);
    BPF_ASSERT(O_NONBLOCK & ret);
    BPF_ASSERT_EQ(0, close(fd));
  }

 private:
  const char* const kCpuInfo = "/proc/cpuinfo";
};

TEST(BrokerProcessIntegrationTest, OpenComplexFlags) {
  RunAllBrokerTests<OpenComplexFlagsDelegate>();
}

class RewriteProcSelfDelegate final : public BrokerTestDelegate {
 public:
  BrokerParams ChildSetUpPreSandbox() override {
    pre_sandbox_status_fd_.reset(HANDLE_EINTR(open(kProcSelfStatus, O_RDONLY)));
    BPF_ASSERT(pre_sandbox_status_fd_.is_valid());

    struct stat sb;
    BPF_ASSERT_GE(fstat(pre_sandbox_status_fd_.get(), &sb), 0);
    pre_sandbox_status_ino_ = sb.st_ino;

    BrokerParams params;
    params.allowed_command_set = syscall_broker::MakeBrokerCommandSet(
        {syscall_broker::COMMAND_ACCESS, syscall_broker::COMMAND_OPEN});
    params.permissions.push_back(
        BrokerFilePermission::ReadOnly(kProcSelfStatus));
    return params;
  }

  void RunTestInSandboxedChild(Syscaller* syscaller) override {
    base::ScopedFD status_fd(syscaller->Open(kProcSelfStatus, O_RDONLY));
    BPF_ASSERT(status_fd.is_valid());

    // The fd opened by the broker should have the same inode number as the fd
    // opened in this process pre-sandbox.
    struct stat sb;
    BPF_ASSERT_GE(fstat(status_fd.get(), &sb), 0);
    BPF_ASSERT_EQ(pre_sandbox_status_ino_, sb.st_ino);
  }

 private:
  static constexpr char kProcSelfStatus[] = "/proc/self/status";

  ino_t pre_sandbox_status_ino_ = 0;
  base::ScopedFD pre_sandbox_status_fd_;
};

TEST(BrokerProcessIntegrationTest, RewriteProcSelf) {
  RunAllBrokerTests<RewriteProcSelfDelegate>();
}

class CreateFileDelegate final : public BrokerTestDelegate {
 public:
  void ParentSetUp() override {
    // Create two temporary files and delete them, but store their file paths
    // for later usage.
    ScopedTemporaryFile temp_file;
    ScopedTemporaryFile perm_file;
    temp_str_ = temp_file.full_file_name();
    perm_str_ = perm_file.full_file_name();
    tempfile_name_ = temp_str_.c_str();
    permfile_name_ = perm_str_.c_str();

    {
      ScopedTemporaryFile tempfile;
      existing_temp_file_str_ = tempfile.full_file_name();
    }
    // Create a conflict for the temp filename.
    base::ScopedFD fd(
        open(existing_temp_file_str_.c_str(), O_RDWR | O_CREAT, 0600));
    ASSERT_GE(fd.get(), 0);
  }

  BrokerParams ChildSetUpPreSandbox() override {
    BrokerParams params;
    params.allowed_command_set = syscall_broker::MakeBrokerCommandSet(
        {syscall_broker::COMMAND_ACCESS, syscall_broker::COMMAND_OPEN});
    // Note that "temporariness" is determined by the permissions given below.
    params.permissions = {
        BrokerFilePermission::ReadWriteCreateTemporary(existing_temp_file_str_),
        BrokerFilePermission::ReadWriteCreateTemporary(tempfile_name_),
        BrokerFilePermission::ReadWriteCreate(permfile_name_),
    };
    return params;
  }

  void RunTestInSandboxedChild(Syscaller* syscaller) override {
    int fd = -1;

    // Opening a temp file using O_CREAT but not O_EXCL must not be allowed
    // by the broker so as to prevent spying on any pre-existing files.
    fd = syscaller->Open(tempfile_name_, O_RDWR | O_CREAT);
    BPF_ASSERT_EQ(fd, -kFakeErrnoSentinel);

    // Opening a temp file in a normal way must not be allowed by the broker,
    // either.
    fd = syscaller->Open(tempfile_name_, O_RDWR);
    BPF_ASSERT_EQ(fd, -kFakeErrnoSentinel);

    // Opening a temp file with both O_CREAT and O_EXCL is allowed since the
    // file is known not to exist outside the scope of ScopedTemporaryFile.
    fd = syscaller->Open(tempfile_name_, O_RDWR | O_CREAT | O_EXCL);
    BPF_ASSERT_GE(fd, 0);
    close(fd);

    // Opening a temp file with both O_CREAT and O_EXCL is allowed but fails
    // per the OS when there is a conflict with a pre-existing file.
    fd = syscaller->Open(existing_temp_file_str_.c_str(),
                         O_RDWR | O_CREAT | O_EXCL);
    BPF_ASSERT_EQ(fd, -EEXIST);

    // Opening a new permanent file without specifying O_EXCL is allowed.
    fd = syscaller->Open(permfile_name_, O_RDWR | O_CREAT);
    BPF_ASSERT_GE(fd, 0);
    close(fd);

    // Opening an existing permanent file without specifying O_EXCL is allowed.
    fd = syscaller->Open(permfile_name_, O_RDWR | O_CREAT);
    BPF_ASSERT_GE(fd, 0);
    close(fd);

    // Opening an existing file with O_EXCL is allowed but fails per the OS.
    fd = syscaller->Open(permfile_name_, O_RDWR | O_CREAT | O_EXCL);
    BPF_ASSERT_EQ(fd, -EEXIST);

    const char kTestText[] = "TESTTESTTEST";

    fd = syscaller->Open(permfile_name_, O_RDWR);
    BPF_ASSERT_GE(fd, 0);
    {
      // Write to the descriptor opened by the broker and close.
      base::ScopedFD scoped_fd(fd);
      ssize_t len = HANDLE_EINTR(write(fd, kTestText, sizeof(kTestText)));
      BPF_ASSERT_EQ(len, static_cast<ssize_t>(sizeof(kTestText)));
    }

    int fd_check = open(permfile_name_, O_RDONLY);
    BPF_ASSERT_GE(fd_check, 0);
    {
      base::ScopedFD scoped_fd(fd_check);
      char buf[1024];
      ssize_t len = HANDLE_EINTR(read(fd_check, buf, sizeof(buf)));
      BPF_ASSERT_EQ(len, static_cast<ssize_t>(sizeof(kTestText)));
      BPF_ASSERT_EQ(memcmp(kTestText, buf, sizeof(kTestText)), 0);
    }
  }

  void ParentTearDown() override {
    // Cleanup.
    unlink(existing_temp_file_str_.c_str());
    unlink(tempfile_name_);
    unlink(permfile_name_);
    BrokerTestDelegate::ParentTearDown();
  }

 private:
  std::string existing_temp_file_str_;
  std::string temp_str_;
  std::string perm_str_;
  const char* tempfile_name_;
  const char* permfile_name_;
};

TEST(BrokerProcessIntegrationTest, CreateFile) {
  RunAllBrokerTests<CreateFileDelegate>();
}

// StatFileDelegate is the base class for all the Stat() tests.
class StatFileDelegate : public BrokerTestDelegate {
 public:
  BrokerParams ChildSetUpPreSandbox() override {
    BPF_ASSERT_EQ(12, HANDLE_EINTR(write(tmp_file_.fd(), "blahblahblah", 12)));
    memset(&sb_, 0, sizeof(sb_));
    return BrokerParams();
  }

 protected:
  ScopedTemporaryFile tmp_file_;

  std::string temp_str_ = tmp_file_.full_file_name();
  const char* const tempfile_name_ = temp_str_.c_str();

  const char* const nonesuch_name = "/mbogo/fictitious/nonesuch";
  const char* const leading_path1 = "/mbogo/fictitious";
  const char* const leading_path2 = "/mbogo";
  const char* const leading_path3 = "/";
  const char* const bad_leading_path1 = "/mbog";
  const char* const bad_leading_path2 = "/mboga";
  const char* const bad_leading_path3 = "/mbogos";
  const char* const bad_leading_path4 = "/mbogo/fictitiou";
  const char* const bad_leading_path5 = "/mbogo/fictitioux";
  const char* const bad_leading_path6 = "/mbogo/fictitiousa";

  struct stat sb_;
};

// Actual file with permissions to see file but command not allowed.
template <bool follow_links>
class StatFileNoCommandDelegate final : public StatFileDelegate {
 public:
  BrokerParams ChildSetUpPreSandbox() override {
    BrokerParams params = StatFileDelegate::ChildSetUpPreSandbox();
    params.allowed_command_set = syscall_broker::MakeBrokerCommandSet({});
    params.permissions = {BrokerFilePermission::ReadOnly(tempfile_name_)};
    return params;
  }
  void RunTestInSandboxedChild(Syscaller* syscaller) override {
    int ret = syscaller->Stat(tempfile_name_, follow_links, &sb_);
    BPF_ASSERT_EQ(-kFakeErrnoSentinel, ret);
  }
};

TEST(BrokerProcessIntegrationTest, StatFileNoCommandFollowLinks) {
  RunAllBrokerTests<StatFileNoCommandDelegate</*follow_links=*/true>>();
}

TEST(BrokerProcessIntegrationTest, StatFileNoCommandNoFollowLinks) {
  RunAllBrokerTests<StatFileNoCommandDelegate</*follow_links=*/false>>();
}

// Allows the STAT command without any file permissions.
template <bool follow_links>
class StatFilesNoPermissionDelegate final : public StatFileDelegate {
 public:
  BrokerParams ChildSetUpPreSandbox() override {
    BrokerParams params = StatFileDelegate::ChildSetUpPreSandbox();
    params.allowed_command_set =
        syscall_broker::MakeBrokerCommandSet({syscall_broker::COMMAND_STAT});
    params.permissions = {};
    return params;
  }

  void RunTestInSandboxedChild(Syscaller* syscaller) override {
    // Nonexistent file with no permissions to see file.
    BPF_ASSERT_EQ(-kFakeErrnoSentinel,
                  syscaller->Stat(nonesuch_name, follow_links, &sb_));
    // Actual file with no permission to see file.
    BPF_ASSERT_EQ(-kFakeErrnoSentinel,
                  syscaller->Stat(tempfile_name_, follow_links, &sb_));
  }
};

TEST(BrokerProcessIntegrationTest, StatFilesNoPermissionFollowLinks) {
  RunAllBrokerTests<StatFilesNoPermissionDelegate</*follow_links=*/true>>();
}

TEST(BrokerProcessIntegrationTest, StatFilesNoPermissionNoFollowLinks) {
  RunAllBrokerTests<StatFilesNoPermissionDelegate</*follow_links=*/false>>();
}

// Nonexistent file with permissions to see file.
template <bool follow_links>
class StatNonexistentFileWithPermissionsDelegate final
    : public StatFileDelegate {
 public:
  BrokerParams ChildSetUpPreSandbox() override {
    BrokerParams params = StatFileDelegate::ChildSetUpPreSandbox();
    params.allowed_command_set =
        syscall_broker::MakeBrokerCommandSet({syscall_broker::COMMAND_STAT});
    params.permissions = {BrokerFilePermission::ReadOnly(nonesuch_name)};
    return params;
  }

  void RunTestInSandboxedChild(Syscaller* syscaller) override {
    BPF_ASSERT_EQ(-ENOENT, syscaller->Stat(nonesuch_name, follow_links, &sb_));

    // Gets denied all the way back to root since no create permission.
    BPF_ASSERT_EQ(-kFakeErrnoSentinel,
                  syscaller->Stat(leading_path1, follow_links, &sb_));
    BPF_ASSERT_EQ(-kFakeErrnoSentinel,
                  syscaller->Stat(leading_path2, follow_links, &sb_));
    BPF_ASSERT_EQ(-kFakeErrnoSentinel,
                  syscaller->Stat(leading_path3, follow_links, &sb_));

    // Not fooled by substrings.
    BPF_ASSERT_EQ(-kFakeErrnoSentinel,
                  syscaller->Stat(bad_leading_path1, follow_links, &sb_));
    BPF_ASSERT_EQ(-kFakeErrnoSentinel,
                  syscaller->Stat(bad_leading_path2, follow_links, &sb_));
    BPF_ASSERT_EQ(-kFakeErrnoSentinel,
                  syscaller->Stat(bad_leading_path3, follow_links, &sb_));
    BPF_ASSERT_EQ(-kFakeErrnoSentinel,
                  syscaller->Stat(bad_leading_path4, follow_links, &sb_));
    BPF_ASSERT_EQ(-kFakeErrnoSentinel,
                  syscaller->Stat(bad_leading_path5, follow_links, &sb_));
    BPF_ASSERT_EQ(-kFakeErrnoSentinel,
                  syscaller->Stat(bad_leading_path6, follow_links, &sb_));
  }
};

TEST(BrokerProcessIntegrationTest,
     StatNonexistentFileWithPermissionsFollowLinks) {
  RunAllBrokerTests<
      StatNonexistentFileWithPermissionsDelegate</*follow_links=*/true>>();
}

TEST(BrokerProcessIntegrationTest,
     StatNonexistentFileWithPermissionsNoFollowLinks) {
  RunAllBrokerTests<
      StatNonexistentFileWithPermissionsDelegate</*follow_links=*/false>>();
}

// Nonexistent file with permissions to create file.
template <bool follow_links>
class StatNonexistentFileWithCreatePermissionsDelegate final
    : public StatFileDelegate {
 public:
  BrokerParams ChildSetUpPreSandbox() override {
    BrokerParams params = StatFileDelegate::ChildSetUpPreSandbox();
    params.allowed_command_set =
        syscall_broker::MakeBrokerCommandSet({syscall_broker::COMMAND_STAT});
    params.permissions = {BrokerFilePermission::ReadWriteCreate(nonesuch_name)};
    return params;
  }

  void RunTestInSandboxedChild(Syscaller* syscaller) override {
    BPF_ASSERT_EQ(-ENOENT, syscaller->Stat(nonesuch_name, follow_links, &sb_));

    // Gets ENOENT all the way back to root since it has create permission.
    BPF_ASSERT_EQ(-ENOENT, syscaller->Stat(leading_path1, follow_links, &sb_));
    BPF_ASSERT_EQ(-ENOENT, syscaller->Stat(leading_path2, follow_links, &sb_));

    // But can always get the root.
    BPF_ASSERT_EQ(0, syscaller->Stat(leading_path3, follow_links, &sb_));

    // Not fooled by substrings.
    BPF_ASSERT_EQ(-kFakeErrnoSentinel,
                  syscaller->Stat(bad_leading_path1, follow_links, &sb_));
    BPF_ASSERT_EQ(-kFakeErrnoSentinel,
                  syscaller->Stat(bad_leading_path2, follow_links, &sb_));
    BPF_ASSERT_EQ(-kFakeErrnoSentinel,
                  syscaller->Stat(bad_leading_path3, follow_links, &sb_));
    BPF_ASSERT_EQ(-kFakeErrnoSentinel,
                  syscaller->Stat(bad_leading_path4, follow_links, &sb_));
    BPF_ASSERT_EQ(-kFakeErrnoSentinel,
                  syscaller->Stat(bad_leading_path5, follow_links, &sb_));
    BPF_ASSERT_EQ(-kFakeErrnoSentinel,
                  syscaller->Stat(bad_leading_path6, follow_links, &sb_));
  }
};

TEST(BrokerProcessIntegrationTest,
     StatNonexistentFileWithCreatePermissionsFollowLinks) {
  RunAllBrokerTests<StatNonexistentFileWithCreatePermissionsDelegate<
      /*follow_links=*/true>>();
}

TEST(BrokerProcessIntegrationTest,
     StatNonexistentFileWithCreatePermissionsNoFollowLinks) {
  RunAllBrokerTests<StatNonexistentFileWithCreatePermissionsDelegate<
      /*follow_links=*/false>>();
}

// Actual file with permissions to see file.
template <bool follow_links>
class StatFileWithPermissionsDelegate final : public StatFileDelegate {
 public:
  BrokerParams ChildSetUpPreSandbox() override {
    BrokerParams params = StatFileDelegate::ChildSetUpPreSandbox();
    params.allowed_command_set =
        syscall_broker::MakeBrokerCommandSet({syscall_broker::COMMAND_STAT});
    params.permissions = {BrokerFilePermission::ReadOnly(tempfile_name_)};
    return params;
  }

  void RunTestInSandboxedChild(Syscaller* syscaller) override {
    BPF_ASSERT_EQ(0, syscaller->Stat(tempfile_name_, follow_links, &sb_));

    // Following fields may never be consistent but should be non-zero.
    // Don't trust the platform to define fields with any particular
    // sign.
    BPF_ASSERT_NE(0u, static_cast<unsigned int>(sb_.st_dev));
    BPF_ASSERT_NE(0u, static_cast<unsigned int>(sb_.st_ino));
    BPF_ASSERT_NE(0u, static_cast<unsigned int>(sb_.st_mode));
    BPF_ASSERT_NE(0u, static_cast<unsigned int>(sb_.st_blksize));
    BPF_ASSERT_NE(0u, static_cast<unsigned int>(sb_.st_blocks));

    // We are the ones that made the file.
    BPF_ASSERT_EQ(geteuid(), sb_.st_uid);
    BPF_ASSERT_EQ(getegid(), sb_.st_gid);

    // Wrote 12 bytes above which should fit in one block.
    BPF_ASSERT_EQ(12, sb_.st_size);

    // Can't go backwards in time, 1500000000 was some time ago.
    BPF_ASSERT_LT(1500000000u, static_cast<unsigned int>(sb_.st_atime));
    BPF_ASSERT_LT(1500000000u, static_cast<unsigned int>(sb_.st_mtime));
    BPF_ASSERT_LT(1500000000u, static_cast<unsigned int>(sb_.st_ctime));
  }
};

TEST(BrokerProcessIntegrationTest, StatFileWithPermissionsFollowLinks) {
  RunAllBrokerTests<StatFileWithPermissionsDelegate</*follow_links=*/true>>();
}

TEST(BrokerProcessIntegrationTest, StatFileWithPermissionsNoFollowLinks) {
  RunAllBrokerTests<StatFileWithPermissionsDelegate</*follow_links=*/false>>();
}

class RenameTestDelegate : public BrokerTestDelegate {
 public:
  void ParentSetUp() override {
    {
      // Just to generate names and ensure they do not exist upon scope exit.
      ScopedTemporaryFile oldfile;
      ScopedTemporaryFile newfile;
      oldpath_ = oldfile.full_file_name();
      newpath_ = newfile.full_file_name();
    }

    // Now make a file using old path name.
    int fd = open(oldpath_.c_str(), O_RDWR | O_CREAT, 0600);
    EXPECT_TRUE(fd > 0);
    close(fd);

    EXPECT_TRUE(access(oldpath_.c_str(), F_OK) == 0);
    EXPECT_TRUE(access(newpath_.c_str(), F_OK) < 0);
  }

  void ParentTearDown() override {
    unlink(oldpath_.c_str());
    unlink(newpath_.c_str());
    BrokerTestDelegate::ParentTearDown();
  }

 protected:
  void ExpectRenamed() {
    EXPECT_TRUE(access(oldpath_.c_str(), 0) < 0);
    EXPECT_TRUE(access(newpath_.c_str(), 0) == 0);
  }

  void ExpectNotRenamed() {
    EXPECT_TRUE(access(oldpath_.c_str(), 0) == 0);
    EXPECT_TRUE(access(newpath_.c_str(), 0) < 0);
  }

  std::string oldpath_;
  std::string newpath_;
};

// Check rename fails with write permissions to both files but command
// itself is not allowed.
class RenameNoCommandDelegate final : public RenameTestDelegate {
 public:
  BrokerParams ChildSetUpPreSandbox() override {
    BrokerParams params;
    params.allowed_command_set = syscall_broker::MakeBrokerCommandSet({});
    params.permissions = {BrokerFilePermission::ReadWriteCreate(oldpath_),
                          BrokerFilePermission::ReadWriteCreate(newpath_)};
    return params;
  }

  void RunTestInSandboxedChild(Syscaller* syscaller) override {
    BPF_ASSERT_EQ(-kFakeErrnoSentinel,
                  syscaller->Rename(oldpath_.c_str(), newpath_.c_str()));
  }

  void ParentTearDown() override {
    ExpectNotRenamed();
    RenameTestDelegate::ParentTearDown();
  }
};

TEST(BrokerProcessIntegrationTest, RenameNoCommand) {
  RunAllBrokerTests<RenameNoCommandDelegate>();
}
// Check rename fails when no permission to new file.
class RenameNoPermNewDelegate final : public RenameTestDelegate {
 public:
  BrokerParams ChildSetUpPreSandbox() override {
    BrokerParams params;
    params.allowed_command_set =
        syscall_broker::MakeBrokerCommandSet({syscall_broker::COMMAND_RENAME});
    params.permissions = {BrokerFilePermission::ReadWriteCreate(newpath_)};
    return params;
  }

  void RunTestInSandboxedChild(Syscaller* syscaller) override {
    BPF_ASSERT_EQ(-kFakeErrnoSentinel,
                  syscaller->Rename(oldpath_.c_str(), newpath_.c_str()));
  }

  void ParentTearDown() override {
    ExpectNotRenamed();
    RenameTestDelegate::ParentTearDown();
  }
};

TEST(BrokerProcessIntegrationTest, RenameNoPermNew) {
  RunAllBrokerTests<RenameNoPermNewDelegate>();
}
// Check rename fails when no permission to old file.
class RenameNoPermOldDelegate final : public RenameTestDelegate {
 public:
  BrokerParams ChildSetUpPreSandbox() override {
    BrokerParams params;
    params.allowed_command_set =
        syscall_broker::MakeBrokerCommandSet({syscall_broker::COMMAND_RENAME});
    params.permissions = {BrokerFilePermission::ReadWriteCreate(oldpath_)};
    return params;
  }

  void RunTestInSandboxedChild(Syscaller* syscaller) override {
    BPF_ASSERT_EQ(-kFakeErrnoSentinel,
                  syscaller->Rename(oldpath_.c_str(), newpath_.c_str()));
  }

  void ParentTearDown() override {
    ExpectNotRenamed();
    RenameTestDelegate::ParentTearDown();
  }
};

TEST(BrokerProcessIntegrationTest, RenameNoPermOld) {
  RunAllBrokerTests<RenameNoPermOldDelegate>();
}

// Check rename fails when only read permission to first file.
class RenameReadPermNewDelegate final : public RenameTestDelegate {
 public:
  BrokerParams ChildSetUpPreSandbox() override {
    BrokerParams params;
    params.allowed_command_set =
        syscall_broker::MakeBrokerCommandSet({syscall_broker::COMMAND_RENAME});
    params.permissions = {BrokerFilePermission::ReadWriteCreate(oldpath_),
                          BrokerFilePermission::ReadOnly(newpath_)};
    return params;
  }

  void RunTestInSandboxedChild(Syscaller* syscaller) override {
    BPF_ASSERT_EQ(-kFakeErrnoSentinel,
                  syscaller->Rename(oldpath_.c_str(), newpath_.c_str()));
  }

  void ParentTearDown() override {
    ExpectNotRenamed();
    RenameTestDelegate::ParentTearDown();
  }
};

TEST(BrokerProcessIntegrationTest, RenameReadPermNew) {
  RunAllBrokerTests<RenameReadPermNewDelegate>();
}

// Check rename fails when only read permission to second file.
class RenameReadPermOldDelegate final : public RenameTestDelegate {
 public:
  BrokerParams ChildSetUpPreSandbox() override {
    BrokerParams params;
    params.allowed_command_set =
        syscall_broker::MakeBrokerCommandSet({syscall_broker::COMMAND_RENAME});
    params.permissions = {BrokerFilePermission::ReadOnly(oldpath_),
                          BrokerFilePermission::ReadWriteCreate(newpath_)};
    return params;
  }

  void RunTestInSandboxedChild(Syscaller* syscaller) override {
    BPF_ASSERT_EQ(-kFakeErrnoSentinel,
                  syscaller->Rename(oldpath_.c_str(), newpath_.c_str()));
  }

  void ParentTearDown() override {
    ExpectNotRenamed();
    RenameTestDelegate::ParentTearDown();
  }
};

TEST(BrokerProcessIntegrationTest, RenameReadPermOld) {
  RunAllBrokerTests<RenameReadPermOldDelegate>();
}

// Check rename passes with write permissions to both files.
class RenameWritePermsBothDelegate final : public RenameTestDelegate {
 public:
  BrokerParams ChildSetUpPreSandbox() override {
    BrokerParams params;
    params.allowed_command_set =
        syscall_broker::MakeBrokerCommandSet({syscall_broker::COMMAND_RENAME});
    params.permissions = {BrokerFilePermission::ReadWriteCreate(oldpath_),
                          BrokerFilePermission::ReadWriteCreate(newpath_)};
    return params;
  }

  void RunTestInSandboxedChild(Syscaller* syscaller) override {
    BPF_ASSERT_EQ(0, syscaller->Rename(oldpath_.c_str(), newpath_.c_str()));
  }

  void ParentTearDown() override {
    ExpectRenamed();
    RenameTestDelegate::ParentTearDown();
  }
};

TEST(BrokerProcessIntegrationTest, RenameWritePermsBoth) {
  RunAllBrokerTests<RenameWritePermsBothDelegate>();
}

// The base class of all the Readlink() tests.
class ReadlinkTestDelegate : public BrokerTestDelegate {
 public:
  void ParentSetUp() override {
    {
      // Just to generate names and ensure they do not exist upon scope exit.
      ScopedTemporaryFile oldfile;
      ScopedTemporaryFile newfile;
      oldpath_ = oldfile.full_file_name();
      newpath_ = newfile.full_file_name();
    }

    // Now make a link from old to new path name.
    EXPECT_TRUE(symlink(oldpath_.c_str(), newpath_.c_str()) == 0);
  }

  void ParentTearDown() override {
    unlink(oldpath_.c_str());
    unlink(newpath_.c_str());
    BrokerTestDelegate::ParentTearDown();
  }

 protected:
  const char* const nonesuch_name = "/mbogo/nonesuch";

  std::string oldpath_;
  std::string newpath_;

  char readlink_buf_[1024];
};

// Actual file with permissions to see file but command itself not allowed.
class ReadlinkNoCommandDelegate final : public ReadlinkTestDelegate {
 public:
  BrokerParams ChildSetUpPreSandbox() override {
    BrokerParams params;
    params.allowed_command_set = syscall_broker::MakeBrokerCommandSet({});
    params.permissions = {BrokerFilePermission::ReadOnly(newpath_)};
    return params;
  }

  void RunTestInSandboxedChild(Syscaller* syscaller) override;
};

TEST(BrokerProcessIntegrationTest, ReadlinkNoCommand) {
  RunAllBrokerTests<ReadlinkNoCommandDelegate>();
}

void ReadlinkNoCommandDelegate::RunTestInSandboxedChild(Syscaller* syscaller) {
  BPF_ASSERT_EQ(-kFakeErrnoSentinel,
                syscaller->Readlink(newpath_.c_str(), readlink_buf_,
                                    sizeof(readlink_buf_)));
}

// Nonexistent file with no permissions to see file.
class ReadlinkNonexistentNoPermissionsDelegate final
    : public ReadlinkTestDelegate {
 public:
  BrokerParams ChildSetUpPreSandbox() override {
    BrokerParams params;
    params.allowed_command_set = syscall_broker::MakeBrokerCommandSet(
        {syscall_broker::COMMAND_READLINK});
    params.permissions = {};
    return params;
  }

  void RunTestInSandboxedChild(Syscaller* syscaller) override {
    BPF_ASSERT_EQ(-kFakeErrnoSentinel,
                  syscaller->Readlink(nonesuch_name, readlink_buf_,
                                      sizeof(readlink_buf_)));
  }
};

TEST(BrokerProcessIntegrationTest, ReadlinkNonexistentNoPermissions) {
  RunAllBrokerTests<ReadlinkNonexistentNoPermissionsDelegate>();
}

// Actual file with no permissions to see file.
class ReadlinkNoPermissionsDelegate final : public ReadlinkTestDelegate {
 public:
  BrokerParams ChildSetUpPreSandbox() override {
    BrokerParams params;
    params.allowed_command_set = syscall_broker::MakeBrokerCommandSet(
        {syscall_broker::COMMAND_READLINK});
    params.permissions = {};
    return params;
  }

  void RunTestInSandboxedChild(Syscaller* syscaller) override {
    BPF_ASSERT_EQ(-kFakeErrnoSentinel,
                  syscaller->Readlink(newpath_.c_str(), readlink_buf_,
                                      sizeof(readlink_buf_)));
  }
};

TEST(BrokerProcessIntegrationTest, ReadlinkNoPermissions) {
  RunAllBrokerTests<ReadlinkNoPermissionsDelegate>();
}

// Nonexistent file with permissions to see file.
class ReadlinkNonexistentWithPermissionsDelegate final
    : public ReadlinkTestDelegate {
 public:
  BrokerParams ChildSetUpPreSandbox() override {
    BrokerParams params;
    params.allowed_command_set = syscall_broker::MakeBrokerCommandSet(
        {syscall_broker::COMMAND_READLINK});
    params.permissions = {BrokerFilePermission::ReadOnly(nonesuch_name)};
    return params;
  }
  void RunTestInSandboxedChild(Syscaller* syscaller) override {
    BPF_ASSERT_EQ(-ENOENT, syscaller->Readlink(nonesuch_name, readlink_buf_,
                                               sizeof(readlink_buf_)));
  }
};

TEST(BrokerProcessIntegrationTest, ReadlinkNonexistentWithPermissions) {
  RunAllBrokerTests<ReadlinkNonexistentWithPermissionsDelegate>();
}

// Actual file with permissions to see file.
class ReadlinkFileWithPermissionsDelegate final : public ReadlinkTestDelegate {
 public:
  BrokerParams ChildSetUpPreSandbox() override {
    BrokerParams params;
    params.allowed_command_set = syscall_broker::MakeBrokerCommandSet(
        {syscall_broker::COMMAND_READLINK});
    params.permissions = {BrokerFilePermission::ReadOnly(newpath_)};
    return params;
  }
  void RunTestInSandboxedChild(Syscaller* syscaller) override {
    ssize_t retlen = syscaller->Readlink(newpath_.c_str(), readlink_buf_,
                                         sizeof(readlink_buf_));
    BPF_ASSERT(retlen == static_cast<ssize_t>(oldpath_.length()));
    BPF_ASSERT_EQ(0, memcmp(oldpath_.c_str(), readlink_buf_, retlen));
  }
};

TEST(BrokerProcessIntegrationTest, ReadlinkFileWithPermissions) {
  RunAllBrokerTests<ReadlinkFileWithPermissionsDelegate>();
}

// Actual file with permissions to see file, but too small a buffer.
class ReadlinkFileWithPermissionsSmallBufferDelegate final
    : public ReadlinkTestDelegate {
 public:
  BrokerParams ChildSetUpPreSandbox() override {
    BrokerParams params;
    params.allowed_command_set = syscall_broker::MakeBrokerCommandSet(
        {syscall_broker::COMMAND_READLINK});
    params.permissions = {BrokerFilePermission::ReadOnly(newpath_)};
    return params;
  }
  void RunTestInSandboxedChild(Syscaller* syscaller) override {
    BPF_ASSERT_EQ(4, syscaller->Readlink(newpath_.c_str(), readlink_buf_, 4));
  }
};

TEST(BrokerProcessIntegrationTest, ReadlinkFileWithPermissionsSmallBuffer) {
  RunAllBrokerTests<ReadlinkFileWithPermissionsSmallBufferDelegate>();
}

// The base class of all the Mkdir() tests.
class MkdirTestDelegate : public BrokerTestDelegate {
 public:
  void ParentSetUp() override {
    ScopedTemporaryFile file;
    path_ = file.full_file_name();
  }

  void ParentTearDown() override {
    rmdir(path_.c_str());
    BrokerTestDelegate::ParentTearDown();
  }

 protected:
  const char* nonesuch_name = "/mbogo/nonesuch";

  std::string path_;
};

// Actual file with permissions to use but command itself not allowed.
class MkdirNoCommandDelegate final : public MkdirTestDelegate {
 public:
  BrokerParams ChildSetUpPreSandbox() override {
    BrokerParams params;
    params.allowed_command_set = syscall_broker::MakeBrokerCommandSet({});
    params.permissions = {BrokerFilePermission::ReadWrite(path_),
                          BrokerFilePermission::ReadWrite(nonesuch_name)};
    return params;
  }

  void RunTestInSandboxedChild(Syscaller* syscaller) override {
    BPF_ASSERT_EQ(-kFakeErrnoSentinel, syscaller->Mkdir(path_.c_str(), 0600));
  }
};

TEST(BrokerProcessIntegrationTest, MkdirNoCommand) {
  RunAllBrokerTests<MkdirNoCommandDelegate>();
}

// Nonexistent file with no permissions to see file.
class MkdirNonexistentNoPermissionsDelegate final : public MkdirTestDelegate {
 public:
  BrokerParams ChildSetUpPreSandbox() override {
    BrokerParams params;
    params.allowed_command_set =
        syscall_broker::MakeBrokerCommandSet({syscall_broker::COMMAND_MKDIR});
    params.permissions = {};
    return params;
  }

  void RunTestInSandboxedChild(Syscaller* syscaller) override {
    BPF_ASSERT_EQ(-kFakeErrnoSentinel, syscaller->Mkdir(nonesuch_name, 0600));
  }
};

TEST(BrokerProcessIntegrationTest, MkdirNonexistentNoPermissions) {
  RunAllBrokerTests<MkdirNonexistentNoPermissionsDelegate>();
}

// Actual file with no permissions to see file.
class MkdirFileNoPermissionsDelegate final : public MkdirTestDelegate {
 public:
  BrokerParams ChildSetUpPreSandbox() override {
    BrokerParams params;
    params.allowed_command_set =
        syscall_broker::MakeBrokerCommandSet({syscall_broker::COMMAND_MKDIR});
    params.permissions = {};
    return params;
  }

  void RunTestInSandboxedChild(Syscaller* syscaller) override {
    BPF_ASSERT_EQ(-kFakeErrnoSentinel, syscaller->Mkdir(path_.c_str(), 0600));
  }
};

TEST(BrokerProcessIntegrationTest, MkdirFileNoPermissions) {
  RunAllBrokerTests<MkdirFileNoPermissionsDelegate>();
}

// Nonexistent file with insufficient permissions to see file.
class MkdirNonexistentROPermissionsDelegate final : public MkdirTestDelegate {
 public:
  BrokerParams ChildSetUpPreSandbox() override {
    BrokerParams params;
    params.allowed_command_set =
        syscall_broker::MakeBrokerCommandSet({syscall_broker::COMMAND_MKDIR});
    params.permissions = {BrokerFilePermission::ReadOnly(path_),
                          BrokerFilePermission::ReadOnly(nonesuch_name)};
    return params;
  }

  void RunTestInSandboxedChild(Syscaller* syscaller) override {
    BPF_ASSERT_EQ(-kFakeErrnoSentinel, syscaller->Mkdir(nonesuch_name, 0600));
  }
};

TEST(BrokerProcessIntegrationTest, MkdirNonexistentROPermissions) {
  RunAllBrokerTests<MkdirNonexistentROPermissionsDelegate>();
}

// Actual file with insufficient permissions to see file.
class MkdirFileROPermissionsDelegate final : public MkdirTestDelegate {
 public:
  BrokerParams ChildSetUpPreSandbox() override {
    BrokerParams params;
    params.allowed_command_set =
        syscall_broker::MakeBrokerCommandSet({syscall_broker::COMMAND_MKDIR});
    params.permissions = {BrokerFilePermission::ReadOnly(path_),
                          BrokerFilePermission::ReadOnly(nonesuch_name)};
    return params;
  }

  void RunTestInSandboxedChild(Syscaller* syscaller) override {
    BPF_ASSERT_EQ(-kFakeErrnoSentinel, syscaller->Mkdir(path_.c_str(), 0600));
  }
};

TEST(BrokerProcessIntegrationTest, MkdirFileROPermissions) {
  RunAllBrokerTests<MkdirFileROPermissionsDelegate>();
}

// Nonexistent file with insufficient permissions to see file, case 2.
class MkdirNonExistentRWPermissionsDelegate final : public MkdirTestDelegate {
 public:
  BrokerParams ChildSetUpPreSandbox() override {
    BrokerParams params;
    params.allowed_command_set =
        syscall_broker::MakeBrokerCommandSet({syscall_broker::COMMAND_MKDIR});
    params.permissions = {BrokerFilePermission::ReadWrite(path_),
                          BrokerFilePermission::ReadWrite(nonesuch_name)};
    return params;
  }

  void RunTestInSandboxedChild(Syscaller* syscaller) override {
    BPF_ASSERT_EQ(-kFakeErrnoSentinel, syscaller->Mkdir(nonesuch_name, 0600));
  }
};

TEST(BrokerProcessIntegrationTest, MkdirNonExistentRWPermissions) {
  RunAllBrokerTests<MkdirNonExistentRWPermissionsDelegate>();
}

// Actual file with insufficient permissions to see file, case 2.
class MkdirFileRWPermissionsDelegate final : public MkdirTestDelegate {
 public:
  BrokerParams ChildSetUpPreSandbox() override {
    BrokerParams params;
    params.allowed_command_set =
        syscall_broker::MakeBrokerCommandSet({syscall_broker::COMMAND_MKDIR});
    params.permissions = {BrokerFilePermission::ReadWrite(path_),
                          BrokerFilePermission::ReadWrite(nonesuch_name)};
    return params;
  }

  void RunTestInSandboxedChild(Syscaller* syscaller) override {
    BPF_ASSERT_EQ(-kFakeErrnoSentinel, syscaller->Mkdir(path_.c_str(), 0600));
  }
};

TEST(BrokerProcessIntegrationTest, MkdirFileRWPermissions) {
  RunAllBrokerTests<MkdirFileRWPermissionsDelegate>();
}

// Nonexistent file with permissions to see file.
class MkdirNonExistentRWCPermissionsDelegate final : public MkdirTestDelegate {
 public:
  BrokerParams ChildSetUpPreSandbox() override {
    BrokerParams params;
    params.allowed_command_set =
        syscall_broker::MakeBrokerCommandSet({syscall_broker::COMMAND_MKDIR});
    params.permissions = {BrokerFilePermission::ReadWriteCreate(path_),
                          BrokerFilePermission::ReadWriteCreate(nonesuch_name)};
    return params;
  }

  void RunTestInSandboxedChild(Syscaller* syscaller) override {
    BPF_ASSERT_EQ(-2, syscaller->Mkdir(nonesuch_name, 0600));
  }
};

TEST(BrokerProcessIntegrationTest, MkdirNonExistentRWCPermissions) {
  RunAllBrokerTests<MkdirNonExistentRWCPermissionsDelegate>();
}

// Actual file with permissions to see file.
class MkdirFileRWCPermissionsDelegate final : public MkdirTestDelegate {
 public:
  BrokerParams ChildSetUpPreSandbox() override {
    BrokerParams params;
    params.allowed_command_set =
        syscall_broker::MakeBrokerCommandSet({syscall_broker::COMMAND_MKDIR});
    params.permissions = {BrokerFilePermission::ReadWriteCreate(path_),
                          BrokerFilePermission::ReadWriteCreate(nonesuch_name)};
    return params;
  }

  void RunTestInSandboxedChild(Syscaller* syscaller) override {
    BPF_ASSERT_EQ(0, syscaller->Mkdir(path_.c_str(), 0600));
  }
};

TEST(BrokerProcessIntegrationTest, MkdirFileRWCPermissions) {
  RunAllBrokerTests<MkdirFileRWCPermissionsDelegate>();
}

class RmdirTestDelegate : public BrokerTestDelegate {
 public:
  void ParentSetUp() override {
    {
      // Generate name and ensure it does not exist upon scope exit.
      ScopedTemporaryFile file;
      path_ = file.full_file_name();
    }

    const char* const path_name = path_.c_str();

    EXPECT_EQ(0, mkdir(path_name, 0600));
    EXPECT_EQ(0, access(path_name, F_OK));
  }

  void ParentTearDown() override {
    rmdir(path_.c_str());
    BrokerTestDelegate::ParentTearDown();
  }

 protected:
  const char* const nonesuch_name = "/mbogo/nonesuch";

  std::string path_;
};

// Actual file with permissions to use but command itself not allowed.
class RmdirNoCommandDelegate final : public RmdirTestDelegate {
 public:
  BrokerParams ChildSetUpPreSandbox() override {
    BrokerParams params;
    params.allowed_command_set = syscall_broker::MakeBrokerCommandSet({});
    params.permissions = {BrokerFilePermission::ReadWrite(path_),
                          BrokerFilePermission::ReadWrite(nonesuch_name)};
    return params;
  }

  void RunTestInSandboxedChild(Syscaller* syscaller) override {
    BPF_ASSERT_EQ(-kFakeErrnoSentinel, syscaller->Rmdir(path_.c_str()));
  }

  void ParentTearDown() override {
    EXPECT_EQ(0, access(path_.c_str(), 0));
    RmdirTestDelegate::ParentTearDown();
  }
};

TEST(BrokerProcessIntegrationTest, RmdirNoCommand) {
  RunAllBrokerTests<RmdirNoCommandDelegate>();
}

// Nonexistent file with no permissions to see file.
class RmdirNonexistentNoPermissionsDelegate final : public RmdirTestDelegate {
 public:
  BrokerParams ChildSetUpPreSandbox() override {
    BrokerParams params;
    params.allowed_command_set =
        syscall_broker::MakeBrokerCommandSet({syscall_broker::COMMAND_RMDIR});
    params.permissions = {};
    return params;
  }

  void RunTestInSandboxedChild(Syscaller* syscaller) override {
    BPF_ASSERT_EQ(-kFakeErrnoSentinel, syscaller->Rmdir(nonesuch_name));
  }

  void ParentTearDown() override {
    EXPECT_EQ(0, access(path_.c_str(), 0));
    RmdirTestDelegate::ParentTearDown();
  }
};

TEST(BrokerProcessIntegrationTest, RmdirNonexistentNoPermissions) {
  RunAllBrokerTests<RmdirNonexistentNoPermissionsDelegate>();
}

// Actual file with no permissions to see file.
class RmdirFileNoPermissionsDelegate final : public RmdirTestDelegate {
 public:
  BrokerParams ChildSetUpPreSandbox() override {
    BrokerParams params;
    params.allowed_command_set =
        syscall_broker::MakeBrokerCommandSet({syscall_broker::COMMAND_RMDIR});
    params.permissions = {};
    return params;
  }

  void RunTestInSandboxedChild(Syscaller* syscaller) override {
    BPF_ASSERT_EQ(-kFakeErrnoSentinel, syscaller->Rmdir(path_.c_str()));
  }

  void ParentTearDown() override {
    EXPECT_EQ(0, access(path_.c_str(), 0));
    RmdirTestDelegate::ParentTearDown();
  }
};

TEST(BrokerProcessIntegrationTest, RmdirFileNoPermissions) {
  RunAllBrokerTests<RmdirFileNoPermissionsDelegate>();
}

// Nonexistent file with insufficient permissions to see file.
class RmdirNonexistentROPermissionsDelegate final : public RmdirTestDelegate {
 public:
  BrokerParams ChildSetUpPreSandbox() override {
    BrokerParams params;
    params.allowed_command_set =
        syscall_broker::MakeBrokerCommandSet({syscall_broker::COMMAND_RMDIR});
    params.permissions = {BrokerFilePermission::ReadOnly(path_),
                          BrokerFilePermission::ReadOnly(nonesuch_name)};
    return params;
  }

  void RunTestInSandboxedChild(Syscaller* syscaller) override {
    BPF_ASSERT_EQ(-kFakeErrnoSentinel, syscaller->Rmdir(nonesuch_name));
  }

  void ParentTearDown() override {
    EXPECT_EQ(0, access(path_.c_str(), 0));
    RmdirTestDelegate::ParentTearDown();
  }
};

TEST(BrokerProcessIntegrationTest, RmdirNonexistentROPermissions) {
  RunAllBrokerTests<RmdirNonexistentROPermissionsDelegate>();
}

// Actual file with insufficient permissions to see file.
class RmdirFileROPermissionsDelegate final : public RmdirTestDelegate {
 public:
  BrokerParams ChildSetUpPreSandbox() override {
    BrokerParams params;
    params.allowed_command_set =
        syscall_broker::MakeBrokerCommandSet({syscall_broker::COMMAND_RMDIR});
    params.permissions = {BrokerFilePermission::ReadOnly(path_),
                          BrokerFilePermission::ReadOnly(nonesuch_name)};
    return params;
  }

  void RunTestInSandboxedChild(Syscaller* syscaller) override {
    BPF_ASSERT_EQ(-kFakeErrnoSentinel, syscaller->Rmdir(path_.c_str()));
  }

  void ParentTearDown() override {
    EXPECT_EQ(0, access(path_.c_str(), 0));
    RmdirTestDelegate::ParentTearDown();
  }
};

TEST(BrokerProcessIntegrationTest, RmdirFileROPermissions) {
  RunAllBrokerTests<RmdirFileROPermissionsDelegate>();
}

// Nonexistent file with insufficient permissions to see file, case 2.
class RmdirNonExistentRWPermissionsDelegate final : public RmdirTestDelegate {
 public:
  BrokerParams ChildSetUpPreSandbox() override {
    BrokerParams params;
    params.allowed_command_set =
        syscall_broker::MakeBrokerCommandSet({syscall_broker::COMMAND_RMDIR});
    params.permissions = {BrokerFilePermission::ReadWrite(path_),
                          BrokerFilePermission::ReadWrite(nonesuch_name)};
    return params;
  }

  void RunTestInSandboxedChild(Syscaller* syscaller) override {
    BPF_ASSERT_EQ(-kFakeErrnoSentinel, syscaller->Rmdir(nonesuch_name));
  }

  void ParentTearDown() override {
    EXPECT_EQ(0, access(path_.c_str(), 0));
    RmdirTestDelegate::ParentTearDown();
  }
};

TEST(BrokerProcessIntegrationTest, RmdirNonExistentRWPermissions) {
  RunAllBrokerTests<RmdirNonExistentRWPermissionsDelegate>();
}

// Actual file with insufficient permissions to see file, case 2.
class RmdirFileRWPermissionsDelegate final : public RmdirTestDelegate {
 public:
  BrokerParams ChildSetUpPreSandbox() override {
    BrokerParams params;
    params.allowed_command_set =
        syscall_broker::MakeBrokerCommandSet({syscall_broker::COMMAND_RMDIR});
    params.permissions = {BrokerFilePermission::ReadWrite(path_),
                          BrokerFilePermission::ReadWrite(nonesuch_name)};
    return params;
  }

  void RunTestInSandboxedChild(Syscaller* syscaller) override {
    BPF_ASSERT_EQ(-kFakeErrnoSentinel, syscaller->Rmdir(path_.c_str()));
  }

  void ParentTearDown() override {
    EXPECT_EQ(0, access(path_.c_str(), 0));
    RmdirTestDelegate::ParentTearDown();
  }
};

TEST(BrokerProcessIntegrationTest, RmdirFileRWPermissions) {
  RunAllBrokerTests<RmdirFileRWPermissionsDelegate>();
}

// Nonexistent file with permissions to see file.
class RmdirNonExistentRWCPermissionsDelegate final : public RmdirTestDelegate {
 public:
  BrokerParams ChildSetUpPreSandbox() override {
    BrokerParams params;
    params.allowed_command_set =
        syscall_broker::MakeBrokerCommandSet({syscall_broker::COMMAND_RMDIR});
    params.permissions = {BrokerFilePermission::ReadWriteCreate(path_),
                          BrokerFilePermission::ReadWriteCreate(nonesuch_name)};
    return params;
  }

  void RunTestInSandboxedChild(Syscaller* syscaller) override {
    BPF_ASSERT_EQ(-2, syscaller->Rmdir(nonesuch_name));
  }

  void ParentTearDown() override {
    EXPECT_EQ(0, access(path_.c_str(), 0));
    RmdirTestDelegate::ParentTearDown();
  }
};

TEST(BrokerProcessIntegrationTest, RmdirNonExistentRWCPermissions) {
  RunAllBrokerTests<RmdirNonExistentRWCPermissionsDelegate>();
}

// Actual file with permissions to see file.
class RmdirFileRWCPermissionsDelegate final : public RmdirTestDelegate {
 public:
  BrokerParams ChildSetUpPreSandbox() override {
    BrokerParams params;
    params.allowed_command_set =
        syscall_broker::MakeBrokerCommandSet({syscall_broker::COMMAND_RMDIR});
    params.permissions = {BrokerFilePermission::ReadWriteCreate(path_),
                          BrokerFilePermission::ReadWriteCreate(nonesuch_name)};
    return params;
  }

  void RunTestInSandboxedChild(Syscaller* syscaller) override {
    BPF_ASSERT_EQ(0, syscaller->Rmdir(path_.c_str()));
  }

  void ParentTearDown() override {
    EXPECT_EQ(-1, access(path_.c_str(), 0));
    RmdirTestDelegate::ParentTearDown();
  }
};

TEST(BrokerProcessIntegrationTest, RmdirFileRWCPermissions) {
  RunAllBrokerTests<RmdirFileRWCPermissionsDelegate>();
}

// The base class for all the Unlink() tests.
class UnlinkTestDelegate : public BrokerTestDelegate {
 public:
  void ParentSetUp() override {
    {
      // Generate name and ensure it does not exist upon scope exit.
      ScopedTemporaryFile file;
      path_ = file.full_file_name();
    }

    int fd = open(path_.c_str(), O_RDWR | O_CREAT, 0600);
    EXPECT_TRUE(fd >= 0);
    close(fd);
    EXPECT_EQ(0, access(path_.c_str(), F_OK));
  }

  void ParentTearDown() override {
    unlink(path_.c_str());
    BrokerTestDelegate::ParentTearDown();
  }

 protected:
  const char* const nonesuch_name = "/mbogo/nonesuch";

  std::string path_;
};

// Actual file with permissions to use but command itself not allowed.
class UnlinkNoCommandDelegate final : public UnlinkTestDelegate {
 public:
  BrokerParams ChildSetUpPreSandbox() override {
    BrokerParams params;
    params.allowed_command_set = syscall_broker::MakeBrokerCommandSet({});
    params.permissions = {BrokerFilePermission::ReadWrite(path_),
                          BrokerFilePermission::ReadWrite(nonesuch_name)};
    return params;
  }

  void RunTestInSandboxedChild(Syscaller* syscaller) override {
    BPF_ASSERT_EQ(-kFakeErrnoSentinel, syscaller->Unlink(path_.c_str()));
  }

  void ParentTearDown() override {
    EXPECT_EQ(0, access(path_.c_str(), 0));
    UnlinkTestDelegate::ParentTearDown();
  }
};

TEST(BrokerProcessIntegrationTest, UnlinkNoCommand) {
  RunAllBrokerTests<UnlinkNoCommandDelegate>();
}

// Nonexistent file with no permissions to see file.
class UnlinkNonexistentNoPermissionsDelegate final : public UnlinkTestDelegate {
 public:
  BrokerParams ChildSetUpPreSandbox() override {
    BrokerParams params;
    params.allowed_command_set =
        syscall_broker::MakeBrokerCommandSet({syscall_broker::COMMAND_UNLINK});
    params.permissions = {};
    return params;
  }

  void RunTestInSandboxedChild(Syscaller* syscaller) override {
    BPF_ASSERT_EQ(-kFakeErrnoSentinel, syscaller->Unlink(nonesuch_name));
  }

  void ParentTearDown() override {
    EXPECT_EQ(0, access(path_.c_str(), 0));
    UnlinkTestDelegate::ParentTearDown();
  }
};

TEST(BrokerProcessIntegrationTest, UnlinkNonexistentNoPermissions) {
  RunAllBrokerTests<UnlinkNonexistentNoPermissionsDelegate>();
}

// Actual file with no permissions to see file.
class UnlinkFileNoPermissionsDelegate final : public UnlinkTestDelegate {
 public:
  BrokerParams ChildSetUpPreSandbox() override {
    BrokerParams params;
    params.allowed_command_set =
        syscall_broker::MakeBrokerCommandSet({syscall_broker::COMMAND_UNLINK});
    params.permissions = {};
    return params;
  }

  void RunTestInSandboxedChild(Syscaller* syscaller) override {
    BPF_ASSERT_EQ(-kFakeErrnoSentinel, syscaller->Unlink(path_.c_str()));
  }

  void ParentTearDown() override {
    EXPECT_EQ(0, access(path_.c_str(), 0));
    UnlinkTestDelegate::ParentTearDown();
  }
};

TEST(BrokerProcessIntegrationTest, UnlinkFileNoPermissions) {
  RunAllBrokerTests<UnlinkFileNoPermissionsDelegate>();
}

// Nonexistent file with insufficient permissions to see file.
class UnlinkNonexistentROPermissionsDelegate final : public UnlinkTestDelegate {
 public:
  BrokerParams ChildSetUpPreSandbox() override {
    BrokerParams params;
    params.allowed_command_set =
        syscall_broker::MakeBrokerCommandSet({syscall_broker::COMMAND_UNLINK});
    params.permissions = {BrokerFilePermission::ReadOnly(path_),
                          BrokerFilePermission::ReadOnly(nonesuch_name)};
    return params;
  }

  void RunTestInSandboxedChild(Syscaller* syscaller) override {
    BPF_ASSERT_EQ(-kFakeErrnoSentinel, syscaller->Unlink(nonesuch_name));
  }

  void ParentTearDown() override {
    EXPECT_EQ(0, access(path_.c_str(), 0));
    UnlinkTestDelegate::ParentTearDown();
  }
};

TEST(BrokerProcessIntegrationTest, UnlinkNonexistentROPermissions) {
  RunAllBrokerTests<UnlinkNonexistentROPermissionsDelegate>();
}

// Actual file with insufficient permissions to see file.
class UnlinkFileROPermissionsDelegate final : public UnlinkTestDelegate {
 public:
  BrokerParams ChildSetUpPreSandbox() override {
    BrokerParams params;
    params.allowed_command_set =
        syscall_broker::MakeBrokerCommandSet({syscall_broker::COMMAND_UNLINK});
    params.permissions = {BrokerFilePermission::ReadOnly(path_),
                          BrokerFilePermission::ReadOnly(nonesuch_name)};
    return params;
  }

  void RunTestInSandboxedChild(Syscaller* syscaller) override {
    BPF_ASSERT_EQ(-kFakeErrnoSentinel, syscaller->Unlink(path_.c_str()));
  }

  void ParentTearDown() override {
    EXPECT_EQ(0, access(path_.c_str(), 0));
    UnlinkTestDelegate::ParentTearDown();
  }
};

TEST(BrokerProcessIntegrationTest, UnlinkFileROPermissions) {
  RunAllBrokerTests<UnlinkFileROPermissionsDelegate>();
}

// Nonexistent file with insufficient permissions to see file, case 2.
class UnlinkNonExistentRWPermissionsDelegate final : public UnlinkTestDelegate {
 public:
  BrokerParams ChildSetUpPreSandbox() override {
    BrokerParams params;
    params.allowed_command_set =
        syscall_broker::MakeBrokerCommandSet({syscall_broker::COMMAND_UNLINK});
    params.permissions = {BrokerFilePermission::ReadWrite(path_),
                          BrokerFilePermission::ReadWrite(nonesuch_name)};
    return params;
  }

  void RunTestInSandboxedChild(Syscaller* syscaller) override {
    BPF_ASSERT_EQ(-kFakeErrnoSentinel, syscaller->Unlink(nonesuch_name));
  }

  void ParentTearDown() override {
    EXPECT_EQ(0, access(path_.c_str(), 0));
    UnlinkTestDelegate::ParentTearDown();
  }
};

TEST(BrokerProcessIntegrationTest, UnlinkNonExistentRWPermissions) {
  RunAllBrokerTests<UnlinkNonExistentRWPermissionsDelegate>();
}

// Actual file with insufficient permissions to see file, case 2.
class UnlinkFileRWPermissionsDelegate final : public UnlinkTestDelegate {
 public:
  BrokerParams ChildSetUpPreSandbox() override {
    BrokerParams params;
    params.allowed_command_set =
        syscall_broker::MakeBrokerCommandSet({syscall_broker::COMMAND_UNLINK});
    params.permissions = {BrokerFilePermission::ReadWrite(path_),
                          BrokerFilePermission::ReadWrite(nonesuch_name)};
    return params;
  }

  void RunTestInSandboxedChild(Syscaller* syscaller) override {
    BPF_ASSERT_EQ(-kFakeErrnoSentinel, syscaller->Unlink(path_.c_str()));
  }

  void ParentTearDown() override {
    EXPECT_EQ(0, access(path_.c_str(), 0));
    UnlinkTestDelegate::ParentTearDown();
  }
};

TEST(BrokerProcessIntegrationTest, UnlinkFileRWPermissions) {
  RunAllBrokerTests<UnlinkFileRWPermissionsDelegate>();
}

// Nonexistent file with permissions to see file.
class UnlinkNonExistentRWCPermissionsDelegate final
    : public UnlinkTestDelegate {
 public:
  BrokerParams ChildSetUpPreSandbox() override {
    BrokerParams params;
    params.allowed_command_set =
        syscall_broker::MakeBrokerCommandSet({syscall_broker::COMMAND_UNLINK});
    params.permissions = {BrokerFilePermission::ReadWriteCreate(path_),
                          BrokerFilePermission::ReadWriteCreate(nonesuch_name)};
    return params;
  }

  void RunTestInSandboxedChild(Syscaller* syscaller) override {
    BPF_ASSERT_EQ(-2, syscaller->Unlink(nonesuch_name));
  }

  void ParentTearDown() override {
    EXPECT_EQ(0, access(path_.c_str(), 0));
    UnlinkTestDelegate::ParentTearDown();
  }
};

TEST(BrokerProcessIntegrationTest, UnlinkNonExistentRWCPermissions) {
  RunAllBrokerTests<UnlinkNonExistentRWCPermissionsDelegate>();
}

// Actual file with permissions to see file.
class UnlinkFileRWCPermissionsDelegate final : public UnlinkTestDelegate {
 public:
  BrokerParams ChildSetUpPreSandbox() override {
    BrokerParams params;
    params.allowed_command_set =
        syscall_broker::MakeBrokerCommandSet({syscall_broker::COMMAND_UNLINK});
    params.permissions = {BrokerFilePermission::ReadWriteCreate(path_),
                          BrokerFilePermission::ReadWriteCreate(nonesuch_name)};
    return params;
  }

  void RunTestInSandboxedChild(Syscaller* syscaller) override {
    BPF_ASSERT_EQ(0, syscaller->Unlink(path_.c_str()));
  }

  void ParentTearDown() override {
    EXPECT_EQ(-1, access(path_.c_str(), 0));
    UnlinkTestDelegate::ParentTearDown();
  }
};

TEST(BrokerProcessIntegrationTest, UnlinkFileRWCPermissions) {
  RunAllBrokerTests<UnlinkFileRWCPermissionsDelegate>();
}

// Parent class for the inotify_add_watch() tests.
class InotifyAddWatchDelegate : public BrokerTestDelegate {
 public:
  const uint32_t kBadMask =
      IN_CREATE | IN_DELETE | IN_CLOSE_WRITE | IN_MOVE | IN_ONLYDIR;
  const uint32_t kGoodMask = kBadMask | IN_ATTRIB;

  static constexpr char kNestedTempDirName[] = "nested_temp_dir";
  static constexpr char kBadPrefixName[] = "nested_t";

  void ParentSetUp() override {
    // Create two nested temp dirs.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDirUnderPath(
        base::FilePath(kTempDirForTests)));
    temp_dir_str_ = temp_dir_.GetPath().MaybeAsASCII();
    ASSERT_FALSE(temp_dir_str_.empty());

    ASSERT_TRUE(nested_temp_dir_.Set(
        temp_dir_.GetPath().AppendASCII(kNestedTempDirName)));
    nested_temp_dir_str_ = nested_temp_dir_.GetPath().MaybeAsASCII();
    ASSERT_FALSE(nested_temp_dir_str_.empty());

    temp_file_ = base::CreateAndOpenTemporaryFileInDir(
        nested_temp_dir_.GetPath(), &temp_file_path_);
    temp_file_path_str_ = temp_file_path_.MaybeAsASCII();
    ASSERT_FALSE(temp_file_path_str_.empty());
  }

 protected:
  // Parent temp directory.
  base::ScopedTempDir temp_dir_;
  std::string temp_dir_str_;

  // A directory nested under |temp_dir_|.
  base::ScopedTempDir nested_temp_dir_;
  std::string nested_temp_dir_str_;

  // In |nested_temp_dir_|.
  base::FilePath temp_file_path_;
  std::string temp_file_path_str_;
  base::File temp_file_;
};

// Try inotify_add_watch() without the relevant broker command.
class InotifyAddWatchNoCommandDelegate final : public InotifyAddWatchDelegate {
 public:
  BrokerParams ChildSetUpPreSandbox() override {
    BrokerParams params;
    params.allowed_command_set = syscall_broker::MakeBrokerCommandSet({});
    params.permissions = {
        BrokerFilePermission::InotifyAddWatchWithIntermediateDirs(
            temp_file_path_str_),
        BrokerFilePermission::ReadWriteCreateRecursive(nested_temp_dir_str_ +
                                                       "/")};
    return params;
  }

  void RunTestInSandboxedChild(Syscaller* syscaller) override {
    base::ScopedFD inotify_instance(inotify_init());
    BPF_ASSERT(inotify_instance.is_valid());
    BPF_ASSERT_EQ(
        -kFakeErrnoSentinel,
        syscaller->InotifyAddWatch(inotify_instance.get(),
                                   nested_temp_dir_str_.c_str(), kGoodMask));
  }
};

TEST(BrokerProcessIntegrationTest, InotifyAddWatchNoCommand) {
  RunAllBrokerTests<InotifyAddWatchNoCommandDelegate>();
}

// Try inotify_add_watch() without the relevant permissions.
class InotifyAddWatchNoPermissionsDelegate final
    : public InotifyAddWatchDelegate {
 public:
  BrokerParams ChildSetUpPreSandbox() override {
    BrokerParams params;
    params.allowed_command_set = syscall_broker::MakeBrokerCommandSet(
        {syscall_broker::COMMAND_INOTIFY_ADD_WATCH});
    params.permissions = {};
    return params;
  }

  void RunTestInSandboxedChild(Syscaller* syscaller) override {
    base::ScopedFD inotify_instance(inotify_init());
    BPF_ASSERT(inotify_instance.is_valid());
    BPF_ASSERT_EQ(
        -kFakeErrnoSentinel,
        syscaller->InotifyAddWatch(inotify_instance.get(),
                                   nested_temp_dir_str_.c_str(), kGoodMask));
  }
};

TEST(BrokerProcessIntegrationTest, InotifyAddWatchNoPermissions) {
  RunAllBrokerTests<InotifyAddWatchNoPermissionsDelegate>();
}

// Try inotify_add_watch() with a variety of bad arguments.
class InotifyAddWatchBadArgumentsDelegate final
    : public InotifyAddWatchDelegate {
 public:
  void ParentSetUp() override {
    InotifyAddWatchDelegate::ParentSetUp();

    ASSERT_TRUE(
        other_temp_dir_.Set(temp_dir_.GetPath().AppendASCII("other_temp_dir")));
    other_temp_dir_str_ = other_temp_dir_.GetPath().MaybeAsASCII();
    ASSERT_FALSE(other_temp_dir_str_.empty());

    base::FilePath bad_prefix = temp_dir_.GetPath().AppendASCII(kBadPrefixName);
    bad_prefix_str_ = bad_prefix.MaybeAsASCII();
    ASSERT_FALSE(bad_prefix_str_.empty());
  }

  BrokerParams ChildSetUpPreSandbox() override {
    BrokerParams params;
    params.allowed_command_set = syscall_broker::MakeBrokerCommandSet(
        {syscall_broker::COMMAND_INOTIFY_ADD_WATCH});
    params.permissions = {
        BrokerFilePermission::InotifyAddWatchWithIntermediateDirs(
            temp_file_path_str_)};
    return params;
  }

  void RunTestInSandboxedChild(Syscaller* syscaller) override {
    base::ScopedFD inotify_instance(inotify_init());
    BPF_ASSERT(inotify_instance.is_valid());
    // Watch the correct directory with bad flags.
    BPF_ASSERT_EQ(
        -kFakeErrnoSentinel,
        syscaller->InotifyAddWatch(inotify_instance.get(),
                                   nested_temp_dir_str_.c_str(), kBadMask));

    // Try to watch an unintended directory, should fail.
    BPF_ASSERT_EQ(
        -kFakeErrnoSentinel,
        syscaller->InotifyAddWatch(inotify_instance.get(),
                                   other_temp_dir_str_.c_str(), kGoodMask));

    // Try to access a prefix that isn't a full directory.
    BPF_ASSERT_EQ(-kFakeErrnoSentinel, syscaller->InotifyAddWatch(
                                           inotify_instance.get(),
                                           bad_prefix_str_.c_str(), kGoodMask));
  }

 protected:
  // Another directory nested under |temp_dir_| with no sandbox permissions.
  base::ScopedTempDir other_temp_dir_;
  std::string other_temp_dir_str_;

  // A prefix of |nested_temp_dir_| that doesn't match a valid directory.
  std::string bad_prefix_str_;
};

TEST(BrokerProcessIntegrationTest, InotifyAddWatchBadArguments) {
  RunAllBrokerTests<InotifyAddWatchBadArgumentsDelegate>();
}

// Use inottify_add_watch() successfully and verify it actually works.
class InotifyAddWatchSuccessDelegate final : public InotifyAddWatchDelegate {
 public:
  BrokerParams ChildSetUpPreSandbox() override {
    BrokerParams params;
    params.allowed_command_set = syscall_broker::MakeBrokerCommandSet(
        {syscall_broker::COMMAND_INOTIFY_ADD_WATCH,
         syscall_broker::COMMAND_UNLINK});
    params.permissions = {
        BrokerFilePermission::InotifyAddWatchWithIntermediateDirs(
            temp_file_path_str_),
        BrokerFilePermission::ReadWriteCreateRecursive(nested_temp_dir_str_ +
                                                       "/")};
    return params;
  }

  void RunTestInSandboxedChild(Syscaller* syscaller) override {
    base::ScopedFD inotify_instance(inotify_init());
    BPF_ASSERT(inotify_instance.is_valid());
    // This inotify_add_watch() call should succeed.
    int wd = syscaller->InotifyAddWatch(
        inotify_instance.get(), nested_temp_dir_str_.c_str(), kGoodMask);
    BPF_ASSERT_GE(wd, 0);

    // Unlinking the file generates an inotify notification.
    BPF_ASSERT_GE(unlink(temp_file_path_str_.c_str()), 0);

    // Read one inotify message and verify it names the correct watch descriptor
    // |wd|. The test will timeout if no inotify notifications are ever
    // generated.
    std::vector<char> buf(4096);
    BPF_ASSERT_GE(read(inotify_instance.get(), buf.data(), buf.size()), 0);
    struct inotify_event* event =
        reinterpret_cast<struct inotify_event*>(buf.data());
    BPF_ASSERT_EQ(event->wd, wd);

    // Removing the watch should succeed.
    BPF_ASSERT_GE(inotify_rm_watch(inotify_instance.get(), wd), 0);
  }
};

TEST(BrokerProcessIntegrationTest, InotifyAddWatchSuccess) {
  RunAllBrokerTests<InotifyAddWatchSuccessDelegate>();
}

// Tests base::FilePathWatcher which uses inotify on Linux.
// This is used in the network service sandbox.
class BaseFilePathWatcherDelegate final : public InotifyAddWatchDelegate {
 public:
  BrokerParams ChildSetUpPreSandbox() override {
    // Prewarm file accesses.
    base::GetMaxNumberOfInotifyWatches();

    BrokerParams params;
    params.allowed_command_set = syscall_broker::MakeBrokerCommandSet(
        {syscall_broker::COMMAND_INOTIFY_ADD_WATCH,
         syscall_broker::COMMAND_OPEN});
    params.permissions = {
        BrokerFilePermission::InotifyAddWatchWithIntermediateDirs(
            temp_file_path_str_),
        BrokerFilePermission::ReadWriteCreateRecursive(nested_temp_dir_str_ +
                                                       "/")};
    return params;
  }

  void RunTestInSandboxedChild(Syscaller* syscaller) override {
    base::test::SingleThreadTaskEnvironment task_environment(
        base::test::TaskEnvironment::MainThreadType::IO);

    // Watch the file and wait for a notification about that file from
    // FilePathWatcher.
    base::RunLoop run_loop;
    base::FilePathWatcher file_watcher_;
    BPF_ASSERT(file_watcher_.Watch(
        temp_file_path_, base::FilePathWatcher::Type::kNonRecursive,
        base::BindLambdaForTesting([&](const base::FilePath& path, bool error) {
          BPF_ASSERT_EQ(temp_file_path_, path);
          run_loop.Quit();
        })));

    // Our inotify file path watcher requires a file to be opened for writing
    // *after* adding the watch, and then closed, in order to generate a
    // notification. The actual call to Write() isn't even strictly necessary.
    // Another way to generate a notification is
    // base::DeleteFile(temp_file_path_).
    base::File temp_file_again(temp_file_path_, base::File::FLAG_OPEN |
                                                    base::File::FLAG_READ |
                                                    base::File::FLAG_WRITE);
    char buf2[] = "a";
    BPF_ASSERT_EQ(temp_file_again.Write(0, buf2, sizeof(buf2)), sizeof(buf2));
    temp_file_again.Flush();
    temp_file_again.Close();
    // Wait until we receive a notification about the file modification.
    // Failure results in a test timeout.
    run_loop.Run();
  }
};

TEST(BrokerProcessIntegrationTest, BaseFilePathWatcherInotifyTest) {
  const std::vector<BrokerTestConfiguration> inotify_test_configs = {
      {"FastCheckInClient_NoSyscaller", true, SyscallerType::NoSyscaller,
       BrokerType::SIGNAL_BASED},
      {"NoFastCheckInClient_NoSyscaller", false, SyscallerType::NoSyscaller,
       BrokerType::SIGNAL_BASED},
  };

  for (const BrokerTestConfiguration& test_config : inotify_test_configs) {
    SCOPED_TRACE(test_config.test_name);
    auto test_delegate = std::make_unique<BaseFilePathWatcherDelegate>();
    RunSingleBrokerTest(test_delegate.get(), test_config);
  }
}

#endif  // !defined(THREAD_SANITIZER)
}  // namespace sandbox
