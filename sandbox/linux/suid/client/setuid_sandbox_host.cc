// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "sandbox/linux/suid/client/setuid_sandbox_host.h"

#include <fcntl.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/launch.h"
#include "base/process/process_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "sandbox/linux/suid/common/sandbox.h"
#include "sandbox/linux/suid/common/suid_unsafe_environment_variables.h"

namespace sandbox {

namespace {

// Set an environment variable that reflects the API version we expect from the
// setuid sandbox. Old versions of the sandbox will ignore this.
void SetSandboxAPIEnvironmentVariable(base::Environment* env) {
  env->SetVar(kSandboxEnvironmentApiRequest,
              base::NumberToString(kSUIDSandboxApiNumber));
}

// Unset environment variables that are expected to be set by the setuid
// sandbox. This is to allow nesting of one instance of the SUID sandbox
// inside another.
void UnsetExpectedEnvironmentVariables(base::EnvironmentMap* env_map) {
  DCHECK(env_map);
  const base::NativeEnvironmentString environment_vars[] = {
      kSandboxDescriptorEnvironmentVarName, kSandboxHelperPidEnvironmentVarName,
      kSandboxEnvironmentApiProvides,       kSandboxPIDNSEnvironmentVarName,
      kSandboxNETNSEnvironmentVarName,
  };

  for (size_t i = 0; i < std::size(environment_vars); ++i) {
    // Setting values in EnvironmentMap to an empty-string will make
    // sure that they get unset from the environment via AlterEnvironment().
    (*env_map)[environment_vars[i]] = base::NativeEnvironmentString();
  }
}

// Wrapper around a shared C function.
// Returns the "saved" environment variable name corresponding to |envvar|
// in a new string or NULL.
std::string* CreateSavedVariableName(const char* env_var) {
  char* const saved_env_var = SandboxSavedEnvironmentVariable(env_var);
  if (!saved_env_var)
    return nullptr;
  std::string* saved_env_var_copy = new std::string(saved_env_var);
  // SandboxSavedEnvironmentVariable is the C function that we wrap and uses
  // malloc() to allocate memory.
  free(saved_env_var);
  return saved_env_var_copy;
}

// The ELF loader will clear many environment variables so we save them to
// different names here so that the SUID sandbox can resolve them for the
// renderer.
void SaveSUIDUnsafeEnvironmentVariables(base::Environment* env) {
  for (unsigned i = 0; kSUIDUnsafeEnvironmentVariables[i]; ++i) {
    const char* env_var = kSUIDUnsafeEnvironmentVariables[i];
    // Get the saved environment variable corresponding to envvar.
    std::unique_ptr<std::string> saved_env_var(
        CreateSavedVariableName(env_var));
    if (!saved_env_var)
      continue;

    std::string value;
    if (env->GetVar(env_var, &value))
      env->SetVar(*saved_env_var, value);
    else
      env->UnSetVar(*saved_env_var);
  }
}

const char* GetDevelSandboxPath() {
  return getenv("CHROME_DEVEL_SANDBOX");
}

}  // namespace

std::unique_ptr<SetuidSandboxHost> SetuidSandboxHost::Create() {
  // Private constructor.
  return base::WrapUnique(new SetuidSandboxHost(base::Environment::Create()));
}

SetuidSandboxHost::SetuidSandboxHost(std::unique_ptr<base::Environment> env)
    : env_(std::move(env)) {
  DCHECK(env_);
}

SetuidSandboxHost::~SetuidSandboxHost() = default;

// Check if CHROME_DEVEL_SANDBOX is set but empty. This currently disables
// the setuid sandbox. TODO(jln): fix this (crbug.com/245376).
bool SetuidSandboxHost::IsDisabledViaEnvironment() {
  const char* devel_sandbox_path = GetDevelSandboxPath();
  return devel_sandbox_path && (*devel_sandbox_path == '\0');
}

base::FilePath SetuidSandboxHost::GetSandboxBinaryPath() {
  base::FilePath sandbox_binary;
  base::FilePath exe_dir;
  if (base::PathService::Get(base::DIR_EXE, &exe_dir)) {
    base::FilePath sandbox_candidate = exe_dir.AppendASCII("chrome-sandbox");
    if (base::PathExists(sandbox_candidate))
      sandbox_binary = sandbox_candidate;
  }

  // In user-managed builds, including development builds, an environment
  // variable is required to enable the sandbox. See
  // https://chromium.googlesource.com/chromium/src/+/main/docs/linux/suid_sandbox_development.md
  struct stat st;
  if (sandbox_binary.empty() && stat(base::kProcSelfExe, &st) == 0 &&
      st.st_uid == getuid()) {
    const char* devel_sandbox_path = GetDevelSandboxPath();
    if (devel_sandbox_path) {
      sandbox_binary = base::FilePath(devel_sandbox_path);
    }
  }

  return sandbox_binary;
}

void SetuidSandboxHost::PrependWrapper(base::CommandLine* cmd_line) {
  std::string sandbox_binary(GetSandboxBinaryPath().value());
  struct stat st;
  if (sandbox_binary.empty() || stat(sandbox_binary.c_str(), &st) != 0) {
    LOG(FATAL) << "The SUID sandbox helper binary is missing: "
               << sandbox_binary
               << " Aborting now. See "
                  "https://chromium.googlesource.com/"
                  "chromium/src/+/master/docs/"
                  "linux/suid_sandbox_development.md.";
  }

  if (access(sandbox_binary.c_str(), X_OK) != 0 || (st.st_uid != 0) ||
      ((st.st_mode & S_ISUID) == 0) || ((st.st_mode & S_IXOTH)) == 0) {
    LOG(FATAL) << "The SUID sandbox helper binary was found, but is not "
                  "configured correctly. Rather than run without sandboxing "
                  "I'm aborting now. You need to make sure that "
               << sandbox_binary << " is owned by root and has mode 4755.";
  }

  cmd_line->PrependWrapper(sandbox_binary);
}

void SetuidSandboxHost::SetupLaunchOptions(
    base::LaunchOptions* options,
    base::ScopedFD* dummy_fd) {
  DCHECK(options);

  // Launching a setuid binary requires PR_SET_NO_NEW_PRIVS to not be used.
  options->allow_new_privs = true;
  UnsetExpectedEnvironmentVariables(&options->environment);

  // Set dummy_fd to the reading end of a closed pipe.
  int pipe_fds[2];
  PCHECK(0 == pipe(pipe_fds));
  PCHECK(0 == IGNORE_EINTR(close(pipe_fds[1])));
  dummy_fd->reset(pipe_fds[0]);

  // We no longer need a dummy socket for discovering the child's PID,
  // but the sandbox is still hard-coded to expect a file descriptor at
  // kZygoteIdFd. Fixing this requires a sandbox API change. :(
  options->fds_to_remap.push_back(std::make_pair(dummy_fd->get(), kZygoteIdFd));
}

void SetuidSandboxHost::SetupLaunchEnvironment() {
  SaveSUIDUnsafeEnvironmentVariables(env_.get());
  SetSandboxAPIEnvironmentVariable(env_.get());
}

}  // namespace sandbox
