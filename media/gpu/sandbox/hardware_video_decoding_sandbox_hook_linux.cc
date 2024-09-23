// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/sandbox/hardware_video_decoding_sandbox_hook_linux.h"

#include <dlfcn.h>
#include <sys/stat.h>

#include "base/process/process_metrics.h"
#include "base/strings/stringprintf.h"
#include "media/gpu/buildflags.h"
#include "sandbox/policy/linux/bpf_hardware_video_decoding_policy_linux.h"

#if BUILDFLAG(USE_VAAPI)
#include "media/gpu/vaapi/vaapi_wrapper.h"
#endif

using sandbox::syscall_broker::BrokerFilePermission;

// TODO(b/195769334): the hardware video decoding sandbox is really only useful
// when building with VA-API or V4L2 (otherwise, we're not really doing hardware
// video decoding). Consider restricting the kHardwareVideoDecoding sandbox type
// to exist only in those configurations so that the presandbox hook is only
// compiled in those scenarios. As it is now, kHardwareVideoDecoding exists for
// all ash-chrome builds because
// chrome/browser/ash/arc/video/gpu_arc_video_service_host.cc depends on it and
// that file is built for ash-chrome regardless of VA-API/V4L2. That means that
// bots like linux-chromeos-rel end up compiling this presandbox hook (thus the
// NOTREACHED_IN_MIGRATION()s in some places here).

namespace media {
namespace {

void AllowAccessToRenderNodes(std::vector<BrokerFilePermission>& permissions,
                              bool include_sys_dev_char,
                              bool read_write) {
  for (int i = 128; i <= 137; ++i) {
    const std::string path = base::StringPrintf("/dev/dri/renderD%d", i);
    struct stat st;
    if (stat(path.c_str(), &st) == 0) {
      permissions.push_back(read_write ? BrokerFilePermission::ReadWrite(path)
                                       : BrokerFilePermission::ReadOnly(path));

      if (include_sys_dev_char) {
        uint32_t major = (static_cast<uint32_t>(st.st_rdev) >> 8) & 0xff;
        uint32_t minor = static_cast<uint32_t>(st.st_rdev) & 0xff;
        std::string char_device_path =
            base::StringPrintf("/sys/dev/char/%u:%u/", major, minor);
        permissions.push_back(
            BrokerFilePermission::ReadOnlyRecursive(char_device_path));
      }
    }
  }
}

bool HardwareVideoDecodingPreSandboxHookForVaapiOnIntel(
    sandbox::syscall_broker::BrokerCommandSet& command_set,
    std::vector<BrokerFilePermission>& permissions) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // This should only be needed in order for GbmDeviceWrapper in
  // platform_video_frame_utils.cc to be able to initialize minigbm after
  // entering the sandbox. Since minigbm is only needed for buffer allocation on
  // ash-chrome, we restrict this to that platform.
  //
  // TODO(b/210759684): we should open the render nodes for both libva and
  // minigbm before entering the sandbox so that we can remove this permission.
  command_set.set(sandbox::syscall_broker::COMMAND_OPEN);

  // This is added because libdrm does a stat() on a sysfs path on behalf of
  // libva to determine if a particular FD refers to a DRM device (more details
  // in b/271788848#comment2).
  //
  // TODO(b/210759684): we probably will need to do this for Linux as well.
  command_set.set(sandbox::syscall_broker::COMMAND_STAT);

  // This is added because libdrm calls access() from drmGetMinorType() that is
  // called from drmGetNodeTypeFromFd(). libva calls drmGetNodeTypeFromFd()
  // during initialization.
  //
  // TODO(b/210759684): we probably will need to do this for Linux as well.
  command_set.set(sandbox::syscall_broker::COMMAND_ACCESS);

  AllowAccessToRenderNodes(permissions, /*include_sys_dev_char=*/true,
                           /*read_write=*/false);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(USE_VAAPI)
  VaapiWrapper::PreSandboxInitialization(/*allow_disabling_global_lock=*/true);
  return true;
#else
  NOTREACHED();
#endif  // BUILDFLAG(USE_VAAPI)
}

bool HardwareVideoDecodingPreSandboxHookForVaapiOnAMD(
    sandbox::syscall_broker::BrokerCommandSet& command_set,
    std::vector<BrokerFilePermission>& permissions) {
  command_set.set(sandbox::syscall_broker::COMMAND_OPEN);
  command_set.set(sandbox::syscall_broker::COMMAND_STAT);
  command_set.set(sandbox::syscall_broker::COMMAND_READLINK);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // This is added because libdrm calls access() from drmGetMinorType() that is
  // called from drmGetNodeTypeFromFd(). libva calls drmGetNodeTypeFromFd()
  // during initialization.
  //
  // TODO(b/210759684): we probably will need to do this for Linux as well.
  command_set.set(sandbox::syscall_broker::COMMAND_ACCESS);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  AllowAccessToRenderNodes(permissions, /*include_sys_dev_char=*/true,
                           /*read_write=*/true);
  permissions.push_back(BrokerFilePermission::ReadOnly("/dev/dri"));

  permissions.push_back(
      BrokerFilePermission::ReadOnly("/usr/share/vulkan/icd.d"));
  permissions.push_back(BrokerFilePermission::ReadOnly(
      "/usr/share/vulkan/icd.d/radeon_icd.x86_64.json"));

  constexpr int kDlopenFlags = RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE;
  const char* radeonsi_lib = "/usr/lib64/dri/radeonsi_dri.so";
#if defined(DRI_DRIVER_DIR)
  radeonsi_lib = DRI_DRIVER_DIR "/radeonsi_dri.so";
#endif
  if (nullptr == dlopen(radeonsi_lib, kDlopenFlags)) {
    LOG(ERROR) << "dlopen(radeonsi_dri.so) failed with error: " << dlerror();
    return false;
  }

  // minigbm may use the DRI driver (requires Mesa 24.0 or older) or the
  // Vulkan driver (requires VK_EXT_image_drm_format_modifier).  Preload the
  // Vulkan driver as well but ignore failures.
  dlopen("libvulkan.so.1", kDlopenFlags);
  dlopen("libvulkan_radeon.so", kDlopenFlags);

#if BUILDFLAG(USE_VAAPI)
  VaapiWrapper::PreSandboxInitialization(/*allow_disabling_global_lock=*/true);
  return true;
#else
  NOTREACHED();
#endif  // BUILDFLAG(USE_VAAPI)
}

bool HardwareVideoDecodingPreSandboxHookForV4L2(
    sandbox::syscall_broker::BrokerCommandSet& command_set,
    std::vector<BrokerFilePermission>& permissions) {
#if BUILDFLAG(USE_V4L2_CODEC)
  command_set.set(sandbox::syscall_broker::COMMAND_OPEN);

  // TODO(b/210759684): we should open the render node for minigbm before
  // entering the sandbox so that we can remove this permission.
  AllowAccessToRenderNodes(permissions, /*include_sys_dev_char=*/false,
                           /*read_write=*/false);

  // Device nodes for V4L2 video decode accelerator drivers.
  // We do not use a FileEnumerator because the device files may not exist yet
  // when the sandbox is created. But since we are restricting access to the
  // video-dec* and media-dec* prefixes we know that we cannot authorize a
  // non-decoder device by accident.
  static constexpr size_t MAX_V4L2_DECODERS = 5;
  static const base::FilePath::CharType kDevicePath[] =
      FILE_PATH_LITERAL("/dev/");
  static const base::FilePath::CharType kVideoDecBase[] = "video-dec";
  static const base::FilePath::CharType kMediaDecBase[] = "media-dec";
  for (size_t i = 0; i < MAX_V4L2_DECODERS; i++) {
    std::ostringstream decoderPath;
    decoderPath << kDevicePath << kVideoDecBase << i;
    permissions.push_back(BrokerFilePermission::ReadWrite(decoderPath.str()));

    std::ostringstream mediaDevicePath;
    mediaDevicePath << kDevicePath << kMediaDecBase << i;
    permissions.push_back(
        BrokerFilePermission::ReadWrite(mediaDevicePath.str()));
  }

  // Image processor used on ARM platforms.
  // TODO(b/195769334): not all V4L2 platforms need an image processor for video
  // decoding. Look into whether we can restrict this permission to only
  // platforms that need it.
  static const char kDevImageProc0Path[] = "/dev/image-proc0";
  permissions.push_back(BrokerFilePermission::ReadWrite(kDevImageProc0Path));

  // Files needed for protected DMA allocations.
  static const char kDmaHeapPath[] = "/dev/dma_heap/restricted_mtk_cma";
  permissions.push_back(BrokerFilePermission::ReadWrite(kDmaHeapPath));
  return true;
#else
  NOTREACHED();
#endif  // BUILDFLAG(USE_V4L2_CODEC)
}

}  // namespace

// TODO(b/195769334): consider using the type of client to decide if we should
// allow opening the render node after entering the sandbox:
//
// - If the client is ARC++/ARCVM, the render node only needs to be opened after
//   entering the sandbox for two cases: the legacy VaapiVideoDecodeAccelerator
//   and AMD.
//
// - If the client is a Chrome renderer process, the render node needs to be
//   opened after entering the sandbox on ChromeOS to allocate output buffers
//   (at least).
bool HardwareVideoDecodingPreSandboxHook(
    sandbox::policy::SandboxLinux::Options options) {
  using HardwareVideoDecodingProcessPolicy =
      sandbox::policy::HardwareVideoDecodingProcessPolicy;
  using PolicyType =
      sandbox::policy::HardwareVideoDecodingProcessPolicy::PolicyType;

  // When decoding many video streams at once, the video utility process can hit
  // FD limits. Increase the limit of maximum FDs allowed to (at least) 8192.
  // IncreaseFdLimitTo() will only increase the FD limit to a value in:
  // [max(soft limit, requested value), min(hard limit, requested value)], never
  // decrease it. See https://man7.org/linux/man-pages/man2/getrlimit.2.html for
  // context on resource limits.
  constexpr unsigned int kAttemptedFdSoftLimit = 1u << 13;
  base::IncreaseFdLimitTo(kAttemptedFdSoftLimit);

  const PolicyType policy_type =
      HardwareVideoDecodingProcessPolicy::ComputePolicyType(
          options.use_amd_specific_policies);

  sandbox::syscall_broker::BrokerCommandSet command_set;
  std::vector<BrokerFilePermission> permissions;

  bool result_for_platform_policy;
  switch (policy_type) {
    case PolicyType::kVaapiOnIntel:
      result_for_platform_policy =
          HardwareVideoDecodingPreSandboxHookForVaapiOnIntel(command_set,
                                                             permissions);
      break;
    case PolicyType::kVaapiOnAMD:
      result_for_platform_policy =
          HardwareVideoDecodingPreSandboxHookForVaapiOnAMD(command_set,
                                                           permissions);
      break;
    case PolicyType::kV4L2:
      result_for_platform_policy =
          HardwareVideoDecodingPreSandboxHookForV4L2(command_set, permissions);
      break;
  }
  if (!result_for_platform_policy)
    return false;

  // TODO(b/210759684): should this still be called if |command_set| or
  // |permissions| is empty?
  sandbox::policy::SandboxLinux::GetInstance()->StartBrokerProcess(
      command_set, permissions, options);
  return true;
}

}  // namespace media
