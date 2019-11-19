// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/audio_sandbox_hook_linux.h"

#include <dlfcn.h>
#include <unistd.h>
#include <string>
#include <vector>

#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "sandbox/linux/syscall_broker/broker_command.h"
#include "sandbox/linux/syscall_broker/broker_file_permission.h"

using sandbox::syscall_broker::BrokerFilePermission;
using sandbox::syscall_broker::MakeBrokerCommandSet;

namespace audio {

namespace {

#if defined(USE_ALSA)
void AddAlsaFilePermissions(std::vector<BrokerFilePermission>* permissions) {
  base::FilePath home_dir;
  base::PathService::Get(base::DIR_HOME, &home_dir);
  const base::FilePath asoundrc =
      home_dir.Append(FILE_PATH_LITERAL(".asoundrc"));
  const std::string read_only_filenames[]{"/etc/asound.conf", "/proc/cpuinfo",
                                          "/etc/group", "/etc/nsswitch.conf",
                                          asoundrc.value()};
  for (const auto& filename : read_only_filenames)
    permissions->push_back(BrokerFilePermission::ReadOnly(filename));

  permissions->push_back(
      BrokerFilePermission::ReadOnlyRecursive("/usr/share/alsa/"));
  permissions->push_back(
      BrokerFilePermission::ReadWriteCreateRecursive("/dev/snd/"));

  static const base::FilePath::CharType dev_aload_path[] =
      FILE_PATH_LITERAL("/dev/aloadC");
  for (int i = 0; i <= 31; ++i) {
    permissions->push_back(BrokerFilePermission::ReadWrite(
        base::StringPrintf("%s%d", dev_aload_path, i)));
  }
}
#endif

#if defined(USE_PULSEAUDIO)
// Utility function used to grant permissions on paths used by PulseAudio which
// are specified through environment variables. |recursive_only| is used to
// determine if the path itself should be allowed access or only its content.
void AllowAccessToEnvSpecifiedPath(
    base::StringPiece variable_name,
    std::vector<BrokerFilePermission>* permissions,
    bool recursive_only) {
  std::unique_ptr<base::Environment> env(base::Environment::Create());

  std::string path_value;
  if (!env->GetVar(variable_name, &path_value))
    return;

  const base::FilePath pa_config_path(path_value);
  if (pa_config_path.empty())
    return;

  if (!recursive_only) {
    permissions->push_back(BrokerFilePermission::ReadWriteCreate(
        pa_config_path.StripTrailingSeparators().value()));
  }
  permissions->push_back(BrokerFilePermission::ReadWriteCreateRecursive(
      pa_config_path.AsEndingWithSeparator().value()));
}

void AddPulseAudioFilePermissions(
    std::vector<BrokerFilePermission>* permissions) {
  base::FilePath home_dir;
  base::PathService::Get(base::DIR_HOME, &home_dir);
  const base::FilePath xauthority_path =
      home_dir.Append(FILE_PATH_LITERAL(".Xauthority"));

  // Calling read() system call on /proc/self/exe returns broker process' path,
  // and it's used by pulse audio for creating a new context.
  const std::string read_only_filenames[]{
      "/etc/machine-id", "/proc/self/exe",
      "/usr/lib/x86_64-linux-gnu/gconv/gconv-modules.cache",
      "/usr/lib/x86_64-linux-gnu/gconv/gconv-modules", xauthority_path.value()};
  for (const auto& filename : read_only_filenames)
    permissions->push_back(BrokerFilePermission::ReadOnly(filename));

  // In certain situations, pulse runs stat() on the home directory.
  permissions->push_back(
      BrokerFilePermission::StatOnlyWithIntermediateDirs(home_dir.value()));

  permissions->push_back(
      BrokerFilePermission::ReadOnlyRecursive("/etc/pulse/"));

  // At times, Pulse tries to create the directory even if the directory already
  // exists and fails if the mkdir() operation returns anything other than
  // "success" or "exists".
  const char* pulse_home_dirs[] = {".pulse", ".config/pulse"};
  for (const char* pulse_home_dir : pulse_home_dirs) {
    const base::FilePath pulse_home_path = home_dir.Append(pulse_home_dir);
    permissions->push_back(
        BrokerFilePermission::ReadWriteCreate(pulse_home_path.value()));
    permissions->push_back(BrokerFilePermission::ReadWriteCreateRecursive(
        pulse_home_path.AsEndingWithSeparator().value()));
  }
  // Pulse might also need to create directories in tmp of the form
  // "/tmp/pulse-<random string>".
  permissions->push_back(
      BrokerFilePermission::ReadWriteCreateRecursive("/tmp/"));
  const char* env_tmp_paths[] = {"TMPDIR", "TMP", "TEMP", "TEMPDIR"};
  for (const char* env_tmp_path : env_tmp_paths) {
    AllowAccessToEnvSpecifiedPath(env_tmp_path, permissions,
                                  /*recursive_only=*/true);
  }
  // Read up the Pulse paths specified via environment variable and allow for
  // read/write/create recursively on the directory.
  const char* env_pulse_paths[] = {"PULSE_CONFIG_PATH", "PULSE_RUNTIME_PATH",
                                   "PULSE_STATE_PATH"};
  for (const char* env_pulse_path : env_pulse_paths) {
    AllowAccessToEnvSpecifiedPath(env_pulse_path, permissions,
                                  /*recursive_only=*/false);
  }

  const std::string run_user_path =
      base::StringPrintf("/run/user/%d", getuid());
  permissions->push_back(BrokerFilePermission::ReadWriteCreate(run_user_path));
  permissions->push_back(
      BrokerFilePermission::ReadWriteCreate(run_user_path + "/pulse"));
  permissions->push_back(BrokerFilePermission::ReadWriteCreateRecursive(
      run_user_path + "/pulse/"));
}
#endif

std::vector<BrokerFilePermission> GetAudioFilePermissions() {
  std::vector<BrokerFilePermission> permissions{
      BrokerFilePermission::ReadOnly("/dev/urandom"),
      BrokerFilePermission::ReadOnly("/sys/devices/system/cpu"),
      BrokerFilePermission::ReadOnlyRecursive("/usr/share/locale/"),
      BrokerFilePermission::ReadWriteCreateRecursive("/dev/shm/")};

#if defined(USE_PULSEAUDIO)
  AddPulseAudioFilePermissions(&permissions);
#endif
#if defined(USE_ALSA)
  AddAlsaFilePermissions(&permissions);
#endif

  return permissions;
}

void LoadAudioLibraries() {
  const std::string libraries[]{"libasound.so.2", "libpulse.so.0",
                                "libnss_files.so.2"};
  for (const auto& library_name : libraries) {
    if (nullptr ==
        dlopen(library_name.c_str(), RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE)) {
      LOG(WARNING) << "dlopen: failed to open " << library_name
                   << " with error: " << dlerror();
    }
  }
}

}  // namespace

bool AudioPreSandboxHook(service_manager::SandboxLinux::Options options) {
  LoadAudioLibraries();
  auto* instance = service_manager::SandboxLinux::GetInstance();
  instance->StartBrokerProcess(MakeBrokerCommandSet({
                                 sandbox::syscall_broker::COMMAND_ACCESS,
#if defined(USE_PULSEAUDIO)
                                     sandbox::syscall_broker::COMMAND_MKDIR,
#endif
                                     sandbox::syscall_broker::COMMAND_OPEN,
                                     sandbox::syscall_broker::COMMAND_READLINK,
                                     sandbox::syscall_broker::COMMAND_STAT,
                                     sandbox::syscall_broker::COMMAND_UNLINK,
                               }),
                               GetAudioFilePermissions(),
                               service_manager::SandboxLinux::PreSandboxHook(),
                               options);

  // TODO(https://crbug.com/850878) enable namespace sandbox. Currently, if
  // enabled, connect() on pulse native socket fails with ENOENT (called from
  // pa_context_connect).

  return true;
}

}  // namespace audio
