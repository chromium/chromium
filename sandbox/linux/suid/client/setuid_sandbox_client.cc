// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/suid/client/setuid_sandbox_client.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <string>
#include <utility>

#include "base/environment.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_number_conversions.h"
#include "sandbox/linux/suid/common/sandbox.h"

namespace {

bool IsFileSystemAccessDenied() {
  // We would rather check "/" instead of "/proc/self/exe" here, but
  // that gives false positives when running as root.  See
  // https://codereview.chromium.org/2578483002/#msg3
  base::ScopedFD proc_self_exe(HANDLE_EINTR(open("/proc/self/exe", O_RDONLY)));
  return !proc_self_exe.is_valid();
}

int GetHelperApi(base::Environment* env) {
  std::string api_string;
  int api_number = 0;  // Assume API version 0 if no environment was found.
  if (env->GetVar(sandbox::kSandboxEnvironmentApiProvides, &api_string) &&
      !base::StringToInt(api_string, &api_number)) {
    // It's an error if we could not convert the API number.
    api_number = -1;
  }
  return api_number;
}

// Convert |var_name| from the environment |env| to an int.
// Return -1 if the variable does not exist or the value cannot be converted.
int EnvToInt(base::Environment* env, const char* var_name) {
  std::string var_string;
  int var_value = -1;
  if (env->GetVar(var_name, &var_string) &&
      !base::StringToInt(var_string, &var_value)) {
    var_value = -1;
  }
  return var_value;
}

pid_t GetHelperPID(base::Environment* env) {
  return EnvToInt(env, sandbox::kSandboxHelperPidEnvironmentVarName);
}

// Get the IPC file descriptor used to communicate with the setuid helper.
int GetIPCDescriptor(base::Environment* env) {
  return EnvToInt(env, sandbox::kSandboxDescriptorEnvironmentVarName);
}

}  // namespace

namespace sandbox {

std::unique_ptr<SetuidSandboxClient> SetuidSandboxClient::Create() {
  // Private constructor.
  return base::WrapUnique(new SetuidSandboxClient(base::Environment::Create()));
}

SetuidSandboxClient::SetuidSandboxClient(std::unique_ptr<base::Environment> env)
    : env_(std::move(env)) {
  DCHECK(env_);
}

SetuidSandboxClient::~SetuidSandboxClient() = default;

void SetuidSandboxClient::CloseDummyFile() {
  // When we're launched through the setuid sandbox, SetupLaunchOptions
  // arranges for kZygoteIdFd to be a dummy file descriptor to satisfy an
  // ancient setuid sandbox ABI requirement. However, the descriptor is no
  // longer needed, so we can simply close it right away now.
  CHECK(IsSuidSandboxChild());

  // Sanity check that kZygoteIdFd refers to a pipe.
  struct stat st;
  PCHECK(0 == fstat(kZygoteIdFd, &st));
  CHECK(S_ISFIFO(st.st_mode));

  PCHECK(0 == IGNORE_EINTR(close(kZygoteIdFd)));
}

bool SetuidSandboxClient::ChrootMe() {
  int ipc_fd = GetIPCDescriptor(env_.get());

  if (ipc_fd < 0) {
    LOG(ERROR) << "Failed to obtain the sandbox IPC descriptor";
    return false;
  }

  if (HANDLE_EINTR(write(ipc_fd, &kMsgChrootMe, 1)) != 1) {
    PLOG(ERROR) << "Failed to write to chroot pipe";
    return false;
  }

  // We need to reap the chroot helper process in any event.
  pid_t helper_pid = GetHelperPID(env_.get());
  // If helper_pid is -1 we wait for any child.
  if (HANDLE_EINTR(waitpid(helper_pid, NULL, 0)) < 0) {
    PLOG(ERROR) << "Failed to wait for setuid helper to die";
    return false;
  }

  char reply;
  if (HANDLE_EINTR(read(ipc_fd, &reply, 1)) != 1) {
    PLOG(ERROR) << "Failed to read from chroot pipe";
    return false;
  }

  if (reply != kMsgChrootSuccessful) {
    LOG(ERROR) << "Error code reply from chroot helper";
    return false;
  }

  // We now consider ourselves "fully sandboxed" as far as the
  // setuid sandbox is concerned.
  CHECK(IsFileSystemAccessDenied());
  sandboxed_ = true;
  return true;
}

bool SetuidSandboxClient::IsSuidSandboxUpToDate() const {
  return GetHelperApi(env_.get()) == kSUIDSandboxApiNumber;
}

bool SetuidSandboxClient::IsSuidSandboxChild() const {
  return GetIPCDescriptor(env_.get()) >= 0;
}

bool SetuidSandboxClient::IsInNewPIDNamespace() const {
  return env_->HasVar(kSandboxPIDNSEnvironmentVarName);
}

bool SetuidSandboxClient::IsInNewNETNamespace() const {
  return env_->HasVar(kSandboxNETNSEnvironmentVarName);
}

bool SetuidSandboxClient::IsSandboxed() const {
  return sandboxed_;
}

}  // namespace sandbox
