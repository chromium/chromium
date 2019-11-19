// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/gpu_sandbox_hook_linux.h"

#include <dlfcn.h>
#include <errno.h>

#include <memory>
#include <sstream>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_enumerator.h"
#include "base/files/scoped_file.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "content/public/common/content_switches.h"
#include "media/gpu/buildflags.h"
#include "sandbox/linux/bpf_dsl/policy.h"
#include "sandbox/linux/syscall_broker/broker_command.h"
#include "sandbox/linux/syscall_broker/broker_file_permission.h"
#include "sandbox/linux/syscall_broker/broker_process.h"
#include "services/service_manager/embedder/set_process_title.h"
#include "services/service_manager/sandbox/chromecast_sandbox_whitelist_buildflags.h"
#include "services/service_manager/sandbox/linux/bpf_cros_amd_gpu_policy_linux.h"
#include "services/service_manager/sandbox/linux/bpf_cros_arm_gpu_policy_linux.h"
#include "services/service_manager/sandbox/linux/bpf_gpu_policy_linux.h"
#include "services/service_manager/sandbox/linux/sandbox_linux.h"

using sandbox::bpf_dsl::Policy;
using sandbox::syscall_broker::BrokerFilePermission;
using sandbox::syscall_broker::BrokerProcess;

namespace content {
namespace {

inline bool IsChromeOS() {
#if defined(OS_CHROMEOS)
  return true;
#else
  return false;
#endif
}

inline bool UseChromecastSandboxWhitelist() {
#if BUILDFLAG(ENABLE_CHROMECAST_GPU_SANDBOX_WHITELIST)
  return true;
#else
  return false;
#endif
}

inline bool IsArchitectureArm() {
#if defined(ARCH_CPU_ARM_FAMILY)
  return true;
#else
  return false;
#endif
}

inline bool UseV4L2Codec() {
#if BUILDFLAG(USE_V4L2_CODEC)
  return true;
#else
  return false;
#endif
}

inline bool UseLibV4L2() {
#if BUILDFLAG(USE_LIBV4L2)
  return true;
#else
  return false;
#endif
}

#if defined(OS_CHROMEOS) && defined(__aarch64__)
static const char kLibGlesPath[] = "/usr/lib64/libGLESv2.so.2";
static const char kLibEglPath[] = "/usr/lib64/libEGL.so.1";
static const char kLibMaliPath[] = "/usr/lib64/libmali.so";
static const char kLibTegraPath[] = "/usr/lib64/libtegrav4l2.so";
static const char kLibV4l2Path[] = "/usr/lib64/libv4l2.so";
static const char kLibV4lEncPluginPath[] =
    "/usr/lib64/libv4l/plugins/libv4l-encplugin.so";
#else
static const char kLibGlesPath[] = "/usr/lib/libGLESv2.so.2";
static const char kLibEglPath[] = "/usr/lib/libEGL.so.1";
static const char kLibMaliPath[] = "/usr/lib/libmali.so";
static const char kLibTegraPath[] = "/usr/lib/libtegrav4l2.so";
static const char kLibV4l2Path[] = "/usr/lib/libv4l2.so";
static const char kLibV4lEncPluginPath[] =
    "/usr/lib/libv4l/plugins/libv4l-encplugin.so";
#endif

constexpr int dlopen_flag = RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE;

void AddV4L2GpuWhitelist(
    std::vector<BrokerFilePermission>* permissions,
    const service_manager::SandboxSeccompBPF::Options& options) {
  if (options.accelerated_video_decode_enabled) {
    // Device nodes for V4L2 video decode accelerator drivers.
    // We do not use a FileEnumerator because the device files may not exist
    // yet when the sandbox is created. But since we are restricting access
    // to the video-dec* and media-dec* prefixes we know that we cannot
    // authorize a non-decoder device by accident.
    static constexpr size_t MAX_V4L2_DECODERS = 5;
    static const base::FilePath::CharType kDevicePath[] =
        FILE_PATH_LITERAL("/dev/");
    static const base::FilePath::CharType kVideoDecBase[] = "video-dec";
    static const base::FilePath::CharType kMediaDecBase[] = "media-dec";
    for (size_t i = 0; i < MAX_V4L2_DECODERS; i++) {
      std::ostringstream decoderPath;
      decoderPath << kDevicePath << kVideoDecBase << i;
      permissions->push_back(
          BrokerFilePermission::ReadWrite(decoderPath.str()));

      std::ostringstream mediaDevicePath;
      mediaDevicePath << kDevicePath << kMediaDecBase << i;
      permissions->push_back(
          BrokerFilePermission::ReadWrite(mediaDevicePath.str()));
    }
  }

  // Image processor used on ARM platforms.
  static const char kDevImageProc0Path[] = "/dev/image-proc0";
  permissions->push_back(BrokerFilePermission::ReadWrite(kDevImageProc0Path));

  if (options.accelerated_video_encode_enabled) {
    // Device node for V4L2 video encode accelerator drivers.
    static const char kDevVideoEncPath[] = "/dev/video-enc";
    permissions->push_back(BrokerFilePermission::ReadWrite(kDevVideoEncPath));
  }

  // Device node for V4L2 JPEG decode accelerator drivers.
  static const char kDevJpegDecPath[] = "/dev/jpeg-dec";
  permissions->push_back(BrokerFilePermission::ReadWrite(kDevJpegDecPath));

  // Device node for V4L2 JPEG encode accelerator drivers.
  static const char kDevJpegEncPath[] = "/dev/jpeg-enc";
  permissions->push_back(BrokerFilePermission::ReadWrite(kDevJpegEncPath));
}

void AddArmMaliGpuWhitelist(std::vector<BrokerFilePermission>* permissions) {
  // Device file needed by the ARM GPU userspace.
  static const char kMali0Path[] = "/dev/mali0";

  permissions->push_back(BrokerFilePermission::ReadWrite(kMali0Path));

  // Non-privileged render nodes for format enumeration.
  // https://dri.freedesktop.org/docs/drm/gpu/drm-uapi.html#render-nodes
  base::FileEnumerator enumerator(
      base::FilePath(FILE_PATH_LITERAL("/dev/dri/")), false /* recursive */,
      base::FileEnumerator::FILES, FILE_PATH_LITERAL("renderD*"));
  for (base::FilePath name = enumerator.Next(); !name.empty();
       name = enumerator.Next()) {
    permissions->push_back(BrokerFilePermission::ReadWrite(name.value()));
  }
}

void AddImgPvrGpuWhitelist(std::vector<BrokerFilePermission>* permissions) {
  // Device node needed by the IMG GPU userspace.
  static const char kPvrSyncPath[] = "/dev/pvr_sync";

  permissions->push_back(BrokerFilePermission::ReadWrite(kPvrSyncPath));
}

void AddAmdGpuWhitelist(std::vector<BrokerFilePermission>* permissions) {
  static const char* const kReadOnlyList[] = {"/etc/ld.so.cache",
                                              "/usr/lib64/libEGL.so.1",
                                              "/usr/lib64/libGLESv2.so.2"};
  for (const char* item : kReadOnlyList)
    permissions->push_back(BrokerFilePermission::ReadOnly(item));

  static const char* const kReadWriteList[] = {
      "/dev/dri",
      "/dev/dri/card0",
      "/dev/dri/controlD64",
      "/dev/dri/renderD128",
      "/sys/class/drm/card0/device/config",
      "/sys/class/drm/controlD64/device/config",
      "/sys/class/drm/renderD128/device/config",
      "/usr/share/libdrm/amdgpu.ids"};
  for (const char* item : kReadWriteList)
    permissions->push_back(BrokerFilePermission::ReadWrite(item));

  static const char* kDevices[] = {"/sys/dev/char", "/sys/devices"};
  for (const char* item : kDevices) {
    std::string path(item);
    permissions->push_back(
        BrokerFilePermission::StatOnlyWithIntermediateDirs(path));
    permissions->push_back(BrokerFilePermission::ReadOnlyRecursive(path + "/"));
  }
}

void AddArmGpuWhitelist(std::vector<BrokerFilePermission>* permissions) {
  // On ARM we're enabling the sandbox before the X connection is made,
  // so we need to allow access to |.Xauthority|.
  static const char kXAuthorityPath[] = "/home/chronos/.Xauthority";
  static const char kLdSoCache[] = "/etc/ld.so.cache";

  // Files needed by the ARM GPU userspace.
  permissions->push_back(BrokerFilePermission::ReadOnly(kXAuthorityPath));
  permissions->push_back(BrokerFilePermission::ReadOnly(kLdSoCache));
  permissions->push_back(BrokerFilePermission::ReadOnly(kLibGlesPath));
  permissions->push_back(BrokerFilePermission::ReadOnly(kLibEglPath));

  AddArmMaliGpuWhitelist(permissions);
}

// Need to look in vendor paths for custom vendor implementations.
static const char* const kWhitelistedChromecastPaths[] = {
    "/oem_cast_shlib/", "/system/vendor/lib/", "/system/lib/"};

void AddChromecastArmGpuWhitelist(
    std::vector<BrokerFilePermission>* permissions) {
  // Device file needed by the ARM GPU userspace.
  static const char kMali0Path[] = "/dev/mali0";
  permissions->push_back(BrokerFilePermission::ReadWrite(kMali0Path));

  // Files needed by the ARM GPU userspace.
  static const char* const kReadOnlyLibraries[] = {"libGLESv2.so.2",
                                                   "libEGL.so.1"};

  for (const char* library : kReadOnlyLibraries) {
    for (const char* path : kWhitelistedChromecastPaths) {
      const std::string library_path(std::string(path) + std::string(library));
      permissions->push_back(BrokerFilePermission::ReadOnly(library_path));
    }
  }

  static const char kLdSoCache[] = "/etc/ld.so.cache";
  permissions->push_back(BrokerFilePermission::ReadOnly(kLdSoCache));
}

void AddStandardGpuWhiteList(std::vector<BrokerFilePermission>* permissions) {
  static const char kDriCardBasePath[] = "/dev/dri/card";
  static const char kNvidiaCtlPath[] = "/dev/nvidiactl";
  static const char kNvidiaDeviceBasePath[] = "/dev/nvidia";
  static const char kNvidiaDeviceModeSetPath[] = "/dev/nvidia-modeset";
  static const char kNvidiaParamsPath[] = "/proc/driver/nvidia/params";
  static const char kDevShm[] = "/dev/shm/";

  // For shared memory.
  permissions->push_back(
      BrokerFilePermission::ReadWriteCreateTemporaryRecursive(kDevShm));

  // For DRI cards.
  for (int i = 0; i <= 9; ++i) {
    permissions->push_back(BrokerFilePermission::ReadWrite(
        base::StringPrintf("%s%d", kDriCardBasePath, i)));
  }

  // For Nvidia GLX driver.
  permissions->push_back(BrokerFilePermission::ReadWrite(kNvidiaCtlPath));
  for (int i = 0; i < 10; ++i) {
    permissions->push_back(BrokerFilePermission::ReadWrite(
        base::StringPrintf("%s%d", kNvidiaDeviceBasePath, i)));
  }
  permissions->push_back(
      BrokerFilePermission::ReadWrite(kNvidiaDeviceModeSetPath));
  permissions->push_back(BrokerFilePermission::ReadOnly(kNvidiaParamsPath));
}

std::vector<BrokerFilePermission> FilePermissionsForGpu(
    const service_manager::SandboxSeccompBPF::Options& options) {
  // All GPU process policies need this file brokered out.
  static const char kDriRcPath[] = "/etc/drirc";
  std::vector<BrokerFilePermission> permissions = {
      BrokerFilePermission::ReadOnly(kDriRcPath)};

  if (IsChromeOS()) {
    if (UseV4L2Codec())
      AddV4L2GpuWhitelist(&permissions, options);
    if (IsArchitectureArm()) {
      AddImgPvrGpuWhitelist(&permissions);
      AddArmGpuWhitelist(&permissions);
      return permissions;
    }
    if (options.use_amd_specific_policies) {
      AddAmdGpuWhitelist(&permissions);
      return permissions;
    }
  }

  if (UseChromecastSandboxWhitelist()) {
    if (UseV4L2Codec())
      AddV4L2GpuWhitelist(&permissions, options);

    if (IsArchitectureArm()) {
      AddChromecastArmGpuWhitelist(&permissions);
      return permissions;
    }
  }

  AddStandardGpuWhiteList(&permissions);
  return permissions;
}

void LoadArmGpuLibraries() {
  // Preload the Mali library.
  if (UseChromecastSandboxWhitelist()) {
    for (const char* path : kWhitelistedChromecastPaths) {
      const std::string library_path(std::string(path) +
                                     std::string("libMali.so"));
      if (dlopen(library_path.c_str(), dlopen_flag))
        break;
    }
  } else {
    dlopen(kLibMaliPath, dlopen_flag);

    // Preload the Tegra V4L2 (video decode acceleration) library.
    dlopen(kLibTegraPath, dlopen_flag);
  }
}

bool LoadAmdGpuLibraries() {
  // Preload the amdgpu-dependent libraries.
  if (nullptr == dlopen("libglapi.so", dlopen_flag)) {
    LOG(ERROR) << "dlopen(libglapi.so) failed with error: " << dlerror();
    return false;
  }

  const char* radeonsi_lib = "/usr/lib64/dri/radeonsi_dri.so";
#if defined(DRI_DRIVER_DIR)
  radeonsi_lib = DRI_DRIVER_DIR "/radeonsi_dri.so";
#endif
  if (nullptr == dlopen(radeonsi_lib, dlopen_flag)) {
    LOG(ERROR) << "dlopen(radeonsi_dri.so) failed with error: " << dlerror();
    return false;
  }
  return true;
}

bool IsAcceleratedVideoEnabled(
    const service_manager::SandboxSeccompBPF::Options& options) {
  return options.accelerated_video_encode_enabled ||
         options.accelerated_video_decode_enabled;
}

void LoadV4L2Libraries(
    const service_manager::SandboxSeccompBPF::Options& options) {
  if (IsAcceleratedVideoEnabled(options) && UseLibV4L2()) {
    dlopen(kLibV4l2Path, dlopen_flag);

    if (options.accelerated_video_encode_enabled) {
      // This is a device-specific encoder plugin.
      dlopen(kLibV4lEncPluginPath, dlopen_flag);
    }
  }
}

bool LoadLibrariesForGpu(
    const service_manager::SandboxSeccompBPF::Options& options) {
  if (IsChromeOS()) {
    if (UseV4L2Codec())
      LoadV4L2Libraries(options);
    if (IsArchitectureArm()) {
      LoadArmGpuLibraries();
      return true;
    }
    if (options.use_amd_specific_policies)
      return LoadAmdGpuLibraries();
  } else if (UseChromecastSandboxWhitelist() && IsArchitectureArm()) {
    LoadArmGpuLibraries();
  }
  return true;
}

sandbox::syscall_broker::BrokerCommandSet CommandSetForGPU(
    const service_manager::SandboxLinux::Options& options) {
  sandbox::syscall_broker::BrokerCommandSet command_set;
  command_set.set(sandbox::syscall_broker::COMMAND_ACCESS);
  command_set.set(sandbox::syscall_broker::COMMAND_OPEN);
  command_set.set(sandbox::syscall_broker::COMMAND_STAT);
  if (IsChromeOS() && options.use_amd_specific_policies) {
    command_set.set(sandbox::syscall_broker::COMMAND_READLINK);
  }
  return command_set;
}

bool BrokerProcessPreSandboxHook(
    service_manager::SandboxLinux::Options options) {
  // Oddly enough, we call back into gpu to invoke this service manager
  // method, since it is part of the embedder component, and the service
  // mananger's sandbox component is a lower layer that can't depend on it.
  service_manager::SetProcessTitleFromCommandLine(nullptr);
  return true;
}

}  // namespace

bool GpuProcessPreSandboxHook(service_manager::SandboxLinux::Options options) {
  service_manager::SandboxLinux::GetInstance()->StartBrokerProcess(
      CommandSetForGPU(options), FilePermissionsForGpu(options),
      base::BindOnce(BrokerProcessPreSandboxHook), options);

  if (!LoadLibrariesForGpu(options))
    return false;

  // TODO(tsepez): enable namspace sandbox here once crashes are understood.

  errno = 0;
  return true;
}

}  // namespace content
