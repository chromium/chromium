// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/zygote/zygote_main.h"

#include <dlfcn.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <utility>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/posix/eintr_wrapper.h"
#include "base/posix/unix_domain_socket.h"
#include "base/rand_util.h"
#include "base/strings/safe_sprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "sandbox/linux/services/credentials.h"
#include "sandbox/linux/services/init_process_reaper.h"
#include "sandbox/linux/services/libc_interceptor.h"
#include "sandbox/linux/services/namespace_sandbox.h"
#include "sandbox/linux/services/thread_helpers.h"
#include "sandbox/linux/suid/client/setuid_sandbox_client.h"
#include "services/service_manager/embedder/descriptors.h"
#include "services/service_manager/embedder/switches.h"
#include "services/service_manager/sandbox/linux/sandbox_debug_handling_linux.h"
#include "services/service_manager/sandbox/linux/sandbox_linux.h"
#include "services/service_manager/sandbox/sandbox.h"
#include "services/service_manager/sandbox/switches.h"
#include "services/service_manager/zygote/common/common_sandbox_support_linux.h"
#include "services/service_manager/zygote/common/zygote_commands_linux.h"
#include "services/service_manager/zygote/common/zygote_fork_delegate_linux.h"
#include "services/service_manager/zygote/zygote_linux.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace service_manager {

namespace {

void CloseFds(const std::vector<int>& fds) {
  for (const auto& it : fds) {
    PCHECK(0 == IGNORE_EINTR(close(it)));
  }
}

base::OnceClosure ClosureFromTwoClosures(base::OnceClosure one,
                                         base::OnceClosure two) {
  return base::BindOnce(
      [](base::OnceClosure one, base::OnceClosure two) {
        if (!one.is_null())
          std::move(one).Run();
        if (!two.is_null())
          std::move(two).Run();
      },
      std::move(one), std::move(two));
}

}  // namespace

// This function triggers the static and lazy construction of objects that need
// to be created before imposing the sandbox.
static void ZygotePreSandboxInit() {
  base::RandUint64();

  base::SysInfo::AmountOfPhysicalMemory();
  base::SysInfo::NumberOfProcessors();

  // ICU DateFormat class (used in base/time_format.cc) needs to get the
  // Olson timezone ID by accessing the zoneinfo files on disk. After
  // TimeZone::createDefault is called once here, the timezone ID is
  // cached and there's no more need to access the file system.
  std::unique_ptr<icu::TimeZone> zone(icu::TimeZone::createDefault());
}

static bool CreateInitProcessReaper(
    base::OnceClosure post_fork_parent_callback) {
  // The current process becomes init(1), this function returns from a
  // newly created process.
  if (!sandbox::CreateInitProcessReaper(std::move(post_fork_parent_callback))) {
    LOG(ERROR) << "Error creating an init process to reap zombies";
    return false;
  }
  return true;
}

// Enter the setuid sandbox. This requires the current process to have been
// created through the setuid sandbox.
static bool EnterSuidSandbox(sandbox::SetuidSandboxClient* setuid_sandbox,
                             base::OnceClosure post_fork_parent_callback) {
  DCHECK(setuid_sandbox);
  DCHECK(setuid_sandbox->IsSuidSandboxChild());

  // Use the SUID sandbox.  This still allows the seccomp sandbox to
  // be enabled by the process later.

  if (!setuid_sandbox->IsSuidSandboxUpToDate()) {
    LOG(WARNING) << "You are using a wrong version of the setuid binary!\n"
                    "Please read "
                    "https://chromium.googlesource.com/chromium/src/+/master/"
                    "docs/linux_suid_sandbox_development.md."
                    "\n\n";
  }

  if (!setuid_sandbox->ChrootMe())
    return false;

  if (setuid_sandbox->IsInNewPIDNamespace()) {
    CHECK_EQ(1, getpid())
        << "The SUID sandbox created a new PID namespace but Zygote "
           "is not the init process. Please, make sure the SUID "
           "binary is up to date.";
  }

  if (getpid() == 1) {
    // The setuid sandbox has created a new PID namespace and we need
    // to assume the role of init.
    CHECK(CreateInitProcessReaper(std::move(post_fork_parent_callback)));
  }

  CHECK(service_manager::SandboxDebugHandling::SetDumpableStatusAndHandlers());
  return true;
}

static void DropAllCapabilities(int proc_fd) {
  CHECK(sandbox::Credentials::DropAllCapabilities(proc_fd));
}

static void EnterNamespaceSandbox(service_manager::SandboxLinux* linux_sandbox,
                                  base::OnceClosure post_fork_parent_callback) {
  linux_sandbox->EngageNamespaceSandbox(true /* from_zygote */);
  if (getpid() == 1) {
    CHECK(CreateInitProcessReaper(ClosureFromTwoClosures(
        base::BindOnce(DropAllCapabilities, linux_sandbox->proc_fd()),
        std::move(post_fork_parent_callback))));
  }
}

static void EnterLayerOneSandbox(service_manager::SandboxLinux* linux_sandbox,
                                 const bool using_layer1_sandbox,
                                 base::OnceClosure post_fork_parent_callback) {
  DCHECK(linux_sandbox);

  ZygotePreSandboxInit();

// Check that the pre-sandbox initialization didn't spawn threads.
// It's not just our code which may do so - some system-installed libraries
// are known to be culprits, e.g. lttng.
#if !defined(THREAD_SANITIZER)
  CHECK(sandbox::ThreadHelpers::IsSingleThreaded());
#endif

  sandbox::SetuidSandboxClient* setuid_sandbox =
      linux_sandbox->setuid_sandbox_client();
  if (setuid_sandbox->IsSuidSandboxChild()) {
    CHECK(
        EnterSuidSandbox(setuid_sandbox, std::move(post_fork_parent_callback)))
        << "Failed to enter setuid sandbox";
  } else if (sandbox::NamespaceSandbox::InNewUserNamespace()) {
    EnterNamespaceSandbox(linux_sandbox, std::move(post_fork_parent_callback));
  } else {
    CHECK(!using_layer1_sandbox);
  }
}

bool ZygoteMain(
    std::vector<std::unique_ptr<ZygoteForkDelegate>> fork_delegates) {
  sandbox::SetAmZygoteOrRenderer(true, GetSandboxFD());

  auto* linux_sandbox = service_manager::SandboxLinux::GetInstance();

  // Skip pre-initializing sandbox when sandbox is disabled for
  // https://crbug.com/444900.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          service_manager::switches::kNoSandbox)) {
    // This will pre-initialize the various sandboxes that need it.
    linux_sandbox->PreinitializeSandbox();
  }

  const bool using_setuid_sandbox =
      linux_sandbox->setuid_sandbox_client()->IsSuidSandboxChild();
  const bool using_namespace_sandbox =
      sandbox::NamespaceSandbox::InNewUserNamespace();
  const bool using_layer1_sandbox =
      using_setuid_sandbox || using_namespace_sandbox;

  if (using_setuid_sandbox) {
    linux_sandbox->setuid_sandbox_client()->CloseDummyFile();
  }

  if (using_layer1_sandbox) {
    // Let the ZygoteHost know we're booting up.
    if (!base::UnixDomainSocket::SendMsg(
            kZygoteSocketPairFd, kZygoteBootMessage, sizeof(kZygoteBootMessage),
            std::vector<int>())) {
      // This is not a CHECK failure because the browser process could either
      // crash or quickly exit while the zygote is starting. In either case a
      // zygote crash is not useful. https://crbug.com/692227
      PLOG(ERROR) << "Failed sending zygote boot message";
      _exit(1);
    }
  }

  VLOG(1) << "ZygoteMain: initializing " << fork_delegates.size()
          << " fork delegates";
  for (const auto& fork_delegate : fork_delegates) {
    fork_delegate->Init(GetSandboxFD(), using_layer1_sandbox);
  }

  // Turn on the first layer of the sandbox if the configuration warrants it.
  EnterLayerOneSandbox(
      linux_sandbox, using_layer1_sandbox,
      base::BindOnce(CloseFds, linux_sandbox->GetFileDescriptorsToClose()));

  const int sandbox_flags = linux_sandbox->GetStatus();
  const bool setuid_sandbox_engaged =
      !!(sandbox_flags & service_manager::SandboxLinux::kSUID);
  CHECK_EQ(using_setuid_sandbox, setuid_sandbox_engaged);

  const bool namespace_sandbox_engaged =
      !!(sandbox_flags & service_manager::SandboxLinux::kUserNS);
  CHECK_EQ(using_namespace_sandbox, namespace_sandbox_engaged);

  Zygote zygote(sandbox_flags, std::move(fork_delegates),
                base::GlobalDescriptors::Descriptor(
                    static_cast<uint32_t>(kSandboxIPCChannel), GetSandboxFD()));

  // This function call can return multiple times, once per fork().
  return zygote.ProcessRequests();
}

}  // namespace service_manager
