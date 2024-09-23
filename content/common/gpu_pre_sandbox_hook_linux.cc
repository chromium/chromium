// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/common/gpu_pre_sandbox_hook_linux.h"

#include <dlfcn.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>

#include <memory>
#include <sstream>
#include <utility>
#include <vector>

#include "base/base_paths.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "content/public/common/content_switches.h"
#include "media/gpu/buildflags.h"
#include "sandbox/linux/bpf_dsl/policy.h"
#include "sandbox/linux/syscall_broker/broker_command.h"
#include "sandbox/linux/syscall_broker/broker_file_permission.h"
#include "sandbox/linux/syscall_broker/broker_process.h"
#include "sandbox/policy/chromecast_sandbox_allowlist_buildflags.h"
#include "sandbox/policy/linux/bpf_cros_amd_gpu_policy_linux.h"
#include "sandbox/policy/linux/bpf_cros_arm_gpu_policy_linux.h"
#include "sandbox/policy/linux/bpf_gpu_policy_linux.h"
#include "sandbox/policy/linux/sandbox_linux.h"

#if BUILDFLAG(USE_V4L2_CODEC)
#include "media/gpu/v4l2/v4l2_device.h"
#endif

using sandbox::bpf_dsl::Policy;
using sandbox::syscall_broker::BrokerFilePermission;
using sandbox::syscall_broker::BrokerProcess;

namespace content {
namespace {

inline bool IsChromeOS() {
  // TODO(b/206464999): for now, we're making the LaCrOS and Ash GPU sandboxes
  // behave similarly. However, the LaCrOS GPU sandbox could probably be made
  // tighter.
#if BUILDFLAG(IS_CHROMEOS)
  return true;
#else
  return false;
#endif
}

inline bool UseChromecastSandboxAllowlist() {
#if BUILDFLAG(ENABLE_CHROMECAST_GPU_SANDBOX_ALLOWLIST)
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

inline bool UseV4L2Codec(
    const sandbox::policy::SandboxSeccompBPF::Options& options) {
#if BUILDFLAG(USE_V4L2_CODEC)
  return options.accelerated_video_decode_enabled ||
      options.accelerated_video_encode_enabled;
#else
  return false;
#endif
}

#if BUILDFLAG(IS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY)
static const char kMaliConfPath[] = "/etc/mali_platform.conf";
#endif

#if BUILDFLAG(IS_CHROMEOS) && defined(__aarch64__)
static const char kLibGlesPath[] = "/usr/lib64/libGLESv2.so.2";
static const char kLibEglPath[] = "/usr/lib64/libEGL.so.1";
static const char kLibMaliPath[] = "/usr/lib64/libmali.so";
static const char kLibTegraPath[] = "/usr/lib64/libtegrav4l2.so";
#else
static const char kLibGlesPath[] = "/usr/lib/libGLESv2.so.2";
static const char kLibEglPath[] = "/usr/lib/libEGL.so.1";
static const char kLibMaliPath[] = "/usr/lib/libmali.so";
static const char kLibTegraPath[] = "/usr/lib/libtegrav4l2.so";
#endif

constexpr int dlopen_flag = RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE;

void AddStandardChromeOsPermissions(
    std::vector<BrokerFilePermission>* permissions) {
  // For the ANGLE passthrough command decoder.
  static const char* const kReadOnlyList[] = {"libEGL.so", "libGLESv2.so"};
  for (const char* item : kReadOnlyList) {
    base::FilePath module_dir;
    if (base::PathService::Get(base::DIR_MODULE, &module_dir)) {
      std::string lib_path = module_dir.Append(item).MaybeAsASCII();
      if (!lib_path.empty()) {
        permissions->push_back(BrokerFilePermission::ReadOnly(lib_path));
      }
    }
  }
}

void AddV4L2GpuPermissions(
    std::vector<BrokerFilePermission>* permissions,
    const sandbox::policy::SandboxSeccompBPF::Options& options) {
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
    // See comments above for why we don't use a FileEnumerator.
    static constexpr size_t MAX_V4L2_ENCODERS = 5;
    static const base::FilePath::CharType kVideoEncBase[] = "/dev/video-enc";
    permissions->push_back(BrokerFilePermission::ReadWrite(kVideoEncBase));
    for (size_t i = 0; i < MAX_V4L2_ENCODERS; i++) {
      std::ostringstream encoderPath;
      encoderPath << kVideoEncBase << i;
      permissions->push_back(
          BrokerFilePermission::ReadWrite(encoderPath.str()));
    }
  }

  // Device node for V4L2 JPEG decode accelerator drivers.
  static const char kDevJpegDecPath[] = "/dev/jpeg-dec";
  permissions->push_back(BrokerFilePermission::ReadWrite(kDevJpegDecPath));

  // Device node for V4L2 JPEG encode accelerator drivers.
  static const char kDevJpegEncPath[] = "/dev/jpeg-enc";
  permissions->push_back(BrokerFilePermission::ReadWrite(kDevJpegEncPath));

  // Additional device nodes for V4L2 JPEG decode encode accelerator drivers,
  // as ChromeOS can have both /dev/jpeg-dec and /dev/jpeg-decN naming styles.
  // See comments above for why we don't use a FileEnumerator.
  static constexpr size_t MAX_V4L2_JPEG_NODES = 5;
  for (size_t i = 0; i < MAX_V4L2_JPEG_NODES; i++) {
    std::ostringstream jpegDecPath;
    jpegDecPath << kDevJpegDecPath << i;
    permissions->push_back(
        BrokerFilePermission::ReadWrite(jpegDecPath.str()));

    std::ostringstream jpegEncPath;
    jpegEncPath << kDevJpegEncPath << i;
    permissions->push_back(
        BrokerFilePermission::ReadWrite(jpegEncPath.str()));
  }

  if (UseChromecastSandboxAllowlist()) {
    static const char kAmlogicAvcEncoderPath[] = "/dev/amvenc_avc";
    permissions->push_back(
        BrokerFilePermission::ReadWrite(kAmlogicAvcEncoderPath));
  }
}

void AddArmMaliGpuPermissions(std::vector<BrokerFilePermission>* permissions) {
  // Device file needed by the ARM GPU userspace.
  static const char kMali0Path[] = "/dev/mali0";

  permissions->push_back(BrokerFilePermission::ReadWrite(kMali0Path));
  // Need to be able to dlopen libmali.so from libEGL.so.
  permissions->push_back(BrokerFilePermission::ReadOnly(kLibMaliPath));

#if BUILDFLAG(IS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY)
  // Files needed for protected DMA allocations.
  static const char kDmaHeapPath[] = "/dev/dma_heap/restricted_mtk_cma";
  permissions->push_back(BrokerFilePermission::ReadWrite(kDmaHeapPath));
  permissions->push_back(BrokerFilePermission::ReadOnly(kMaliConfPath));
#endif

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

void AddImgPvrGpuPermissions(std::vector<BrokerFilePermission>* permissions) {
  // Device node needed by the IMG GPU userspace.
  static const char kPvrSyncPath[] = "/dev/pvr_sync";

  permissions->push_back(BrokerFilePermission::ReadWrite(kPvrSyncPath));
}

void AddDrmGpuDevPermissions(std::vector<BrokerFilePermission>* permissions,
                             const std::string& path) {
  struct stat st;

  if (stat(path.c_str(), &st) == 0) {
    permissions->push_back(BrokerFilePermission::ReadWrite(path));

    uint32_t major = (static_cast<uint32_t>(st.st_rdev) >> 8) & 0xff;
    uint32_t minor = static_cast<uint32_t>(st.st_rdev) & 0xff;
    std::string char_device_path =
        base::StringPrintf("/sys/dev/char/%u:%u/", major, minor);
    permissions->push_back(
        BrokerFilePermission::ReadOnlyRecursive(char_device_path));
  }
}

void AddDrmGpuPermissions(std::vector<BrokerFilePermission>* permissions) {
  permissions->push_back(BrokerFilePermission::ReadOnly("/dev/dri"));
  for (int i = 0; i <= 9; ++i) {
    AddDrmGpuDevPermissions(permissions,
                            base::StringPrintf("/dev/dri/card%d", i));
    AddDrmGpuDevPermissions(permissions,
                            base::StringPrintf("/dev/dri/renderD%d", i + 128));
  }
}

void AddAmdGpuPermissions(std::vector<BrokerFilePermission>* permissions) {
  static const char* const kReadOnlyList[] = {
      "/etc/ld.so.cache",
      // To support threads in mesa we use --gpu-sandbox-start-early and
      // that requires the following libs and files to be accessible.
      "/usr/lib64/libEGL.so.1",
      "/usr/lib64/libGLESv2.so.2",
      "/usr/lib64/libglapi.so.0",
      "/usr/lib64/libgallium_dri.so",
      "/usr/lib64/dri/r300_dri.so",
      "/usr/lib64/dri/r600_dri.so",
      "/usr/lib64/dri/radeonsi_dri.so",
      // Allow libglvnd files and libs.
      "/usr/share/glvnd/egl_vendor.d",
      "/usr/share/glvnd/egl_vendor.d/50_mesa.json",
      "/usr/lib64/libEGL_mesa.so.0",
      "/usr/lib64/libGLdispatch.so.0"};
  for (const char* item : kReadOnlyList)
    permissions->push_back(BrokerFilePermission::ReadOnly(item));

  AddDrmGpuPermissions(permissions);

  // NOTE: control nodes are probably not required:
  // NOTE: amdgpu.ids should probably be read-only:
  static const char* const kReadWriteList[] = {
      "/dev/dri/controlD64",
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

void AddNvidiaGpuPermissions(std::vector<BrokerFilePermission>* permissions) {
  static const char* const kReadOnlyList[] = {
      // To support threads in mesa we use --gpu-sandbox-start-early and
      // that requires the following libs and files to be accessible.
      "/etc/ld.so.cache",
      "/usr/lib64/libgallium_dri.so",
      "/usr/lib64/dri/nouveau_dri.so",
      "/usr/lib64/dri/radeonsi_dri.so",
      "/usr/lib64/dri/swrast_dri.so",
      "/usr/lib64/libEGL.so.1",
      "/usr/lib64/libEGL_mesa.so.0",
      "/usr/lib64/libGLESv2.so.2",
      "/usr/lib64/libGLdispatch.so.0",
      "/usr/lib64/libdrm_amdgpu.so.1",
      "/usr/lib64/libdrm_nouveau.so.2",
      "/usr/lib64/libdrm_radeon.so.1",
      "/usr/lib64/libelf.so.1",
      "/usr/lib64/libglapi.so.0",
      "/usr/share/glvnd/egl_vendor.d",
      "/usr/share/glvnd/egl_vendor.d/50_mesa.json"};
  for (const char* item : kReadOnlyList) {
    permissions->push_back(BrokerFilePermission::ReadOnly(item));
  }

  AddDrmGpuPermissions(permissions);
}

void AddIntelGpuPermissions(std::vector<BrokerFilePermission>* permissions) {
  static const char* const kReadOnlyList[] = {
      // To support threads in mesa we use --gpu-sandbox-start-early and
      // that requires the following libs and files to be accessible.
      "/usr/lib64/libgallium_dri.so",
      "/usr/lib64/libEGL.so.1", "/usr/lib64/libGLESv2.so.2",
      "/usr/lib64/libelf.so.1", "/usr/lib64/libglapi.so.0",
      "/usr/lib64/libdrm_amdgpu.so.1", "/usr/lib64/libdrm_radeon.so.1",
      "/usr/lib64/libdrm_nouveau.so.2", "/usr/lib64/dri/crocus_dri.so",
      "/usr/lib64/dri/i965_dri.so", "/usr/lib64/dri/iris_dri.so",
      "/usr/lib64/dri/swrast_dri.so", "/usr/lib64/libzstd.so.1",
      // Allow libglvnd files and libs.
      "/usr/share/glvnd/egl_vendor.d",
      "/usr/share/glvnd/egl_vendor.d/50_mesa.json",
      "/usr/lib64/libEGL_mesa.so.0", "/usr/lib64/libGLdispatch.so.0",
      // Case of when the only libc++abi.so.1 is preloaded.
      // See: crbug.com/1366646
      "/usr/lib64/libc++.so.1"};
  for (const char* item : kReadOnlyList)
    permissions->push_back(BrokerFilePermission::ReadOnly(item));

  AddDrmGpuPermissions(permissions);
}

void AddVirtIOGpuPermissions(std::vector<BrokerFilePermission>* permissions) {
  static const char* const kReadOnlyList[] = {
      "/etc/ld.so.cache",
      // To support threads in mesa we use --gpu-sandbox-start-early and
      // that requires the following libs and files to be accessible.
      // "/sys", "/sys/dev", "/sys/dev/char", "/sys/devices" are probed in order
      // to use kms_swrast.
      "/sys",
      "/sys/dev",
      "/usr/lib64/libdrm_amdgpu.so.1",
      "/usr/lib64/libdrm_radeon.so.1",
      "/usr/lib64/libdrm_nouveau.so.2",
      "/usr/lib64/libelf.so.1",
      "/usr/lib64/libEGL.so.1",
      "/usr/lib64/libGLESv2.so.2",
      "/usr/lib64/libEGL_mesa.so.0",
      "/usr/lib64/libGLdispatch.so.0",
      "/usr/lib64/libglapi.so.0",
      "/usr/lib64/libc++.so.1",
      "/usr/lib64/libgallium_dri.so",
      // If kms_swrast_dri is not usable, swrast_dri is used instead.
      "/usr/lib64/dri/swrast_dri.so",
      "/usr/lib64/dri/kms_swrast_dri.so",
      "/usr/lib64/dri/virtio_gpu_dri.so",
      "/usr/share/glvnd/egl_vendor.d",
      "/usr/share/glvnd/egl_vendor.d/50_mesa.json",
  };

  for (const char* item : kReadOnlyList) {
    permissions->push_back(BrokerFilePermission::ReadOnly(item));
  }

  static const char* kDevices[] = {"/sys/dev/char", "/sys/devices"};
  for (const char* item : kDevices) {
    std::string path(item);
    permissions->push_back(
        BrokerFilePermission::StatOnlyWithIntermediateDirs(path));
    permissions->push_back(BrokerFilePermission::ReadOnly(path));
    permissions->push_back(BrokerFilePermission::ReadOnlyRecursive(path + "/"));
  }

  AddDrmGpuPermissions(permissions);
}

void AddArmGpuPermissions(std::vector<BrokerFilePermission>* permissions) {
  static const char kLdSoCache[] = "/etc/ld.so.cache";

  // Files needed by the ARM GPU userspace.
  permissions->push_back(BrokerFilePermission::ReadOnly(kLdSoCache));
  permissions->push_back(BrokerFilePermission::ReadOnly(kLibGlesPath));
  permissions->push_back(BrokerFilePermission::ReadOnly(kLibEglPath));

  AddArmMaliGpuPermissions(permissions);
}

// Need to look in vendor paths for custom vendor implementations.
static const char* const kAllowedChromecastPaths[] = {
    "/oem_cast_shlib/", "/system/vendor/lib/", "/system/lib/",
    "/system/chrome/lib/"};

void AddChromecastArmGpuPermissions(
    std::vector<BrokerFilePermission>* permissions) {
  // Device file needed by the ARM GPU userspace.
  static const char kMali0Path[] = "/dev/mali0";
  permissions->push_back(BrokerFilePermission::ReadWrite(kMali0Path));

  // Files needed by the ARM GPU userspace.
  static const char* const kReadOnlyLibraries[] = {"libGLESv2.so.2",
                                                   "libEGL.so.1",
                                                   // Allow ANGLE libraries.
                                                   "libGLESv2.so", "libEGL.so"};

  for (const char* library : kReadOnlyLibraries) {
    for (const char* path : kAllowedChromecastPaths) {
      const std::string library_path(std::string(path) + std::string(library));
      permissions->push_back(BrokerFilePermission::ReadOnly(library_path));
    }
  }

  static const char kLdSoCache[] = "/etc/ld.so.cache";
  permissions->push_back(BrokerFilePermission::ReadOnly(kLdSoCache));

  base::FileEnumerator enumerator(
      base::FilePath(FILE_PATH_LITERAL("/dev/dri/")), false /* recursive */,
      base::FileEnumerator::FILES, FILE_PATH_LITERAL("renderD*"));
  for (base::FilePath name = enumerator.Next(); !name.empty();
       name = enumerator.Next()) {
    permissions->push_back(BrokerFilePermission::ReadWrite(name.value()));
  }
}

void AddVulkanICDPermissions(std::vector<BrokerFilePermission>* permissions) {
  static const char* const kReadOnlyICDPrefixes[] = {"/usr/share/vulkan/icd.d",
                                                     "/etc/vulkan/icd.d"};

  static const char* const kReadOnlyICDList[] = {
      "intel_icd.x86_64.json", "nvidia_icd.json", "radeon_icd.x86_64.json",
      "mali_icd.json", "freedreno_icd.aarch64.json"};

  for (std::string prefix : kReadOnlyICDPrefixes) {
    permissions->push_back(BrokerFilePermission::ReadOnly(prefix));
    for (const char* json : kReadOnlyICDList) {
      permissions->push_back(
          BrokerFilePermission::ReadOnly(prefix + "/" + json));
    }
  }
}

void AddStandardGpuPermissions(std::vector<BrokerFilePermission>* permissions) {
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

  // For SwiftShader
  base::FilePath module_path;
  if (base::PathService::Get(base::DIR_MODULE, &module_path)) {
    std::string sw_path =
        module_path.Append("libvk_swiftshader.so").MaybeAsASCII();
    if (!sw_path.empty()) {
      permissions->push_back(BrokerFilePermission::ReadOnly(sw_path));
    }
  }
}

std::vector<BrokerFilePermission> FilePermissionsForGpu(
    const sandbox::policy::SandboxSeccompBPF::Options& options) {
  // All GPU process policies need this file brokered out.
  static const char kDriRcPath[] = "/etc/drirc";
  std::vector<BrokerFilePermission> permissions = {
      BrokerFilePermission::ReadOnly(kDriRcPath)};

  AddVulkanICDPermissions(&permissions);

  if (IsChromeOS()) {
    // Permissions are additive, there can be multiple GPUs in the system.
    AddStandardChromeOsPermissions(&permissions);
    if (UseV4L2Codec(options))
      AddV4L2GpuPermissions(&permissions, options);
    if (IsArchitectureArm()) {
      AddImgPvrGpuPermissions(&permissions);
      AddArmGpuPermissions(&permissions);
      // Add standard DRM permissions for snapdragon:
      AddDrmGpuPermissions(&permissions);
      // Following discrete GPUs can be plugged in via USB4 on ARM systems.
    }
    if (options.use_amd_specific_policies) {
      AddAmdGpuPermissions(&permissions);
    }
    if (options.use_intel_specific_policies) {
      AddIntelGpuPermissions(&permissions);
    }
    if (options.use_nvidia_specific_policies) {
      AddStandardGpuPermissions(&permissions);
      AddNvidiaGpuPermissions(&permissions);
    }
    if (options.use_virtio_specific_policies) {
      AddVirtIOGpuPermissions(&permissions);
    }
    return permissions;
  }

  if (UseChromecastSandboxAllowlist()) {
    if (UseV4L2Codec(options))
      AddV4L2GpuPermissions(&permissions, options);

    if (IsArchitectureArm()) {
      AddChromecastArmGpuPermissions(&permissions);
      return permissions;
    }
  }

  AddStandardGpuPermissions(&permissions);
  return permissions;
}

void LoadArmGpuLibraries() {
#if BUILDFLAG(IS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY)
  // This environmental variable needs to be set before we load libMali if we
  // want to instantiate protected Vulkan device queues.
  static const char kMaliConfVar[] = "MALI_PLATFORM_CONFIG";
  // Note this function will only fail if we run out of memory entirely, in
  // which case we would have much bigger problems, so we don't bother to check
  // the return value.
  setenv(kMaliConfVar, kMaliConfPath, 1);
#endif

  // Preload the Mali library.
  if (UseChromecastSandboxAllowlist()) {
    for (const char* path : kAllowedChromecastPaths) {
      const std::string library_path(std::string(path) +
                                     std::string("libMali.so"));
      if (dlopen(library_path.c_str(), dlopen_flag))
        break;
    }
  } else {
    bool is_mali = dlopen(kLibMaliPath, dlopen_flag) != nullptr;

    // Preload the Tegra V4L2 (video decode acceleration) library.
    bool is_tegra = dlopen(kLibTegraPath, dlopen_flag) != nullptr;

    // Preload mesa related libraries for devices which use mesa
    // (ie. not mali or tegra):
    if (!is_mali && !is_tegra &&
        (nullptr != dlopen("libglapi.so.0", dlopen_flag))) {
      const char* driver_paths[] = {
        "/usr/lib64/libgallium_dri.so",
#if defined(DRI_DRIVER_DIR)
        DRI_DRIVER_DIR "/msm_dri.so",
        DRI_DRIVER_DIR "/panfrost_dri.so",
        DRI_DRIVER_DIR "/mediatek_dri.so",
        DRI_DRIVER_DIR "/rockchip_dri.so",
        DRI_DRIVER_DIR "/asahi_dri.so",
#else
        "/usr/lib64/dri/msm_dri.so",
        "/usr/lib64/dri/panfrost_dri.so",
        "/usr/lib64/dri/mediatek_dri.so",
        "/usr/lib64/dri/rockchip_dri.so",
        "/usr/lib64/dri/asahi_dri.so",
        "/usr/lib/dri/msm_dri.so",
        "/usr/lib/dri/panfrost_dri.so",
        "/usr/lib/dri/mediatek_dri.so",
        "/usr/lib/dri/rockchip_dri.so",
        "/usr/lib/dri/asahi_dri.so",
#endif
        nullptr
      };

      for (int i = 0; driver_paths[i] != nullptr; i++)
        dlopen(driver_paths[i], dlopen_flag);
    }
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

bool LoadNvidiaLibraries() {
  // The driver may lazily load several XCB libraries. It's not an error on
  // wayland-only systems for them to be missing.
  const char* kLibraries[] = {
      "libxcb-dri3.so.0",
      "libxcb-glx.so.0",
      "libxcb-present.so.0",
      "libxcb-sync.so.1",
  };
  for (const auto* library : kLibraries) {
    if (!dlopen(library, dlopen_flag))
      LOG(WARNING) << "dlopen(" << library
                   << ") failed with error: " << dlerror();
  }
  return true;
}

void LoadVulkanLibraries() {
  // Try to preload Vulkan libraries. Failure is not an error as not all may be
  // present.
  dlopen("libvulkan.so.1", dlopen_flag);
  dlopen("libvulkan_radeon.so", dlopen_flag);
  dlopen("libvulkan_intel.so", dlopen_flag);
  dlopen("libGLX_nvidia.so.0", dlopen_flag);
  dlopen("libvulkan_freedreno.so", dlopen_flag);
}

void LoadChromecastV4L2Libraries() {
  for (const char* path : kAllowedChromecastPaths) {
    const std::string library_path(std::string(path) +
                                   std::string("libvpcodec.so"));
    if (dlopen(library_path.c_str(), dlopen_flag))
      break;
  }
}

bool LoadLibrariesForGpu(
    const sandbox::policy::SandboxSeccompBPF::Options& options) {
  LoadVulkanLibraries();
  if (IsArchitectureArm()) {
    LoadArmGpuLibraries();
  }
  if (IsChromeOS()) {
    if (options.use_amd_specific_policies) {
      if (!LoadAmdGpuLibraries())
        return false;
    }
  } else {
    if (UseChromecastSandboxAllowlist() && IsArchitectureArm()) {
      if (UseV4L2Codec(options)) {
        LoadChromecastV4L2Libraries();
      }
    }
  }
  if (options.use_nvidia_specific_policies)
    return LoadNvidiaLibraries();
  return true;
}

sandbox::syscall_broker::BrokerCommandSet CommandSetForGPU(
    const sandbox::policy::SandboxLinux::Options& options) {
  sandbox::syscall_broker::BrokerCommandSet command_set;
  command_set.set(sandbox::syscall_broker::COMMAND_ACCESS);
  command_set.set(sandbox::syscall_broker::COMMAND_OPEN);
  command_set.set(sandbox::syscall_broker::COMMAND_STAT);
  if (IsChromeOS() &&
      (options.use_amd_specific_policies ||
       options.use_intel_specific_policies ||
       options.use_nvidia_specific_policies ||
       options.use_virtio_specific_policies || IsArchitectureArm())) {
    command_set.set(sandbox::syscall_broker::COMMAND_READLINK);
  }
  return command_set;
}

}  // namespace

bool GpuPreSandboxHook(sandbox::policy::SandboxLinux::Options options) {
  sandbox::policy::SandboxLinux::GetInstance()->StartBrokerProcess(
      CommandSetForGPU(options), FilePermissionsForGpu(options), options);

  if (!LoadLibrariesForGpu(options))
    return false;

  // TODO(tsepez): enable namspace sandbox here once crashes are understood.

  errno = 0;
  return true;
}

}  // namespace content
