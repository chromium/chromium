// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SERVICES_NAMESPACE_SANDBOX_H_
#define SANDBOX_LINUX_SERVICES_NAMESPACE_SANDBOX_H_

#include <sys/types.h>

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "sandbox/sandbox_export.h"

namespace sandbox {

// Helper class for starting a process inside a new user, PID, and network
// namespace. Before using a namespace sandbox, check for namespaces support
// using Credentials::CanCreateProcessInNewUserNS.
//
// A typical use for "A" launching a sandboxed process "B" would be:
// 1. A sets up a command line and launch options for process B.
// 2. A launches B with LaunchProcess.
// 3. B should be prepared to assume the role of init(1). In particular, apart
//    from SIGKILL and SIGSTOP, B cannot receive any signal for which it does
//    not have an explicit signal handler registered.
//    If B dies, all the processes in the namespace will die.
//    B can fork() and the parent can assume the role of init(1), by using
//    CreateInitProcessReaper().
// 4. B chroots using Credentials::MoveToNewUserNS() and
//    Credentials::DropFileSystemAccess()
// 5. B drops capabilities gained by entering the new user namespace with
//    Credentials::DropAllCapabilities().
class SANDBOX_EXPORT NamespaceSandbox {
 public:
  struct Options {
    Options();
    ~Options();

    // Bitmask of namespace types. Must be some combination of CLONE_NEWUSER
    // (required), CLONE_NEWPID, and CLONE_NEWNET. Defaults to all of the above.
    int ns_types;

    // Fail if any of the namespace types are not supported. Defaults to false.
    bool fail_on_unsupported_ns_type;
  };

  NamespaceSandbox() = delete;
  NamespaceSandbox(const NamespaceSandbox&) = delete;
  NamespaceSandbox& operator=(const NamespaceSandbox&) = delete;

  // Launch a new process inside its own user/PID/network namespaces (depending
  // on kernel support). Requires at a minimum that user namespaces are
  // supported (use Credentials::CanCreateProcessInNewUserNS to check this).
  //
  // pre_exec_delegate and clone_flags fields of LaunchOptions should be nullptr
  // and 0, respectively, since this function makes a copy of options and
  // overrides them.
  static base::Process LaunchProcess(const base::CommandLine& cmdline,
                                     const base::LaunchOptions& launch_options);
  static base::Process LaunchProcess(const std::vector<std::string>& argv,
                                     const base::LaunchOptions& launch_options);

  // Versions which take namespace sandbox options. These allow fine grained
  // control over the types of namespaces used.
  static base::Process LaunchProcessWithOptions(
      const base::CommandLine& cmdline,
      const base::LaunchOptions& launch_options,
      const Options& ns_sandbox_options);
  static base::Process LaunchProcessWithOptions(
      const std::vector<std::string>& argv,
      const base::LaunchOptions& launch_options,
      const Options& ns_sandbox_options);

  // Forks a process in its own PID namespace. The child process is the init
  // process inside of the PID namespace, so if the child needs to fork further,
  // it should call CreateInitProcessReaper, which turns the init process into a
  // reaper process.
  //
  // Otherwise, the child should setup handlers for signals which should
  // terminate the process using InstallDefaultTerminationSignalHandlers or
  // InstallTerminationSignalHandler. This works around the fact that init
  // processes ignore such signals unless they have an explicit handler set.
  //
  // This function requries CAP_SYS_ADMIN. If |drop_capabilities_in_child| is
  // true, then capabilities are dropped in the child.
  static pid_t ForkInNewPidNamespace(bool drop_capabilities_in_child);

  // Installs a signal handler for:
  //
  // SIGHUP, SIGINT, SIGABRT, SIGQUIT, SIGPIPE, SIGTERM, SIGUSR1, SIGUSR2
  //
  // that exits with SignalExitCode(sig). These are signals whose default action
  // is to terminate the program (apart from SIGILL, SIGFPE, and SIGSEGV, which
  // will still terminate the process if e.g. an illegal instruction is
  // encountered, etc.).
  //
  // If any of these already had a signal handler installed, this function will
  // not override them.
  static void InstallDefaultTerminationSignalHandlers();

  // Installs a signal handler for |sig| which exits with |exit_code|. If a
  // signal handler was already present for |sig|, does nothing and returns
  // false.
  static bool InstallTerminationSignalHandler(int sig, int exit_code);

  // Returns an exit code corresponding to the process being killed by sig. This
  // is the same as exit code that NaCl's default signal handler uses.
  static int SignalExitCode(int sig) { return -sig & 0xff; }

  // Returns whether the namespace sandbox created a new user, PID, and network
  // namespace. In particular, InNewUserNamespace should return true iff the
  // process was started via this class.
  static bool InNewUserNamespace();
  static bool InNewPidNamespace();
  static bool InNewNetNamespace();
};

}  // namespace sandbox

#endif  // SANDBOX_LINUX_SERVICES_NAMESPACE_SANDBOX_H_
