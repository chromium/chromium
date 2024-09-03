// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/sandbox/hardware_video_encoding_sandbox_hook_linux.h"

#include <dlfcn.h>
#include <sys/stat.h>

#include "base/strings/stringprintf.h"
#include "media/gpu/buildflags.h"

#if BUILDFLAG(USE_VAAPI)
#include "media/gpu/vaapi/vaapi_wrapper.h"
#endif

#if BUILDFLAG(USE_V4L2_CODEC)
#include "media/gpu/v4l2/v4l2_device.h"
#endif

using sandbox::syscall_broker::BrokerFilePermission;

namespace media {

bool HardwareVideoEncodingPreSandboxHook(
    sandbox::policy::SandboxLinux::Options options) {
  sandbox::syscall_broker::BrokerCommandSet command_set;
  std::vector<BrokerFilePermission> permissions;

#if BUILDFLAG(USE_V4L2_CODEC)
  command_set.set(sandbox::syscall_broker::COMMAND_OPEN);

  // Device nodes for V4L2 video encode accelerator drivers.
  // We do not use a FileEnumerator because the device files may not exist
  // yet when the sandbox is created. But since we are restricting access
  // to the video-enc* prefix we know that we cannot
  // authorize a non-encoder device by accident.
  static constexpr size_t MAX_V4L2_ENCODERS = 5;
  static const base::FilePath::CharType kVideoEncBase[] = "/dev/video-enc";
  permissions.push_back(BrokerFilePermission::ReadWrite(kVideoEncBase));
  for (size_t i = 0; i < MAX_V4L2_ENCODERS; i++) {
    std::ostringstream encoderPath;
    encoderPath << kVideoEncBase << i;
    permissions.push_back(BrokerFilePermission::ReadWrite(encoderPath.str()));
  }

  // Image processor used on ARM platforms.
  // TODO(b/248528896): it's possible not all V4L2 platforms need an image
  // processor for video encoding. Look into whether we can restrict this
  // permission to only platforms that need it.
  static const char kDevImageProc0Path[] = "/dev/image-proc0";
  permissions.push_back(BrokerFilePermission::ReadWrite(kDevImageProc0Path));
#elif BUILDFLAG(USE_VAAPI)
  command_set.set(sandbox::syscall_broker::COMMAND_OPEN);
  command_set.set(sandbox::syscall_broker::COMMAND_STAT);
  command_set.set(sandbox::syscall_broker::COMMAND_ACCESS);

  if (options.use_amd_specific_policies) {
    command_set.set(sandbox::syscall_broker::COMMAND_READLINK);

    permissions.push_back(BrokerFilePermission::ReadOnly("/dev/dri"));

    static const char* kDevices[] = {"/sys/dev/char", "/sys/devices"};
    for (const char* item : kDevices) {
      std::string path(item);
      permissions.push_back(
          BrokerFilePermission::StatOnlyWithIntermediateDirs(path));
      permissions.push_back(
          BrokerFilePermission::ReadOnlyRecursive(path + "/"));
    }

    permissions.push_back(
        BrokerFilePermission::ReadOnly("/usr/share/vulkan/icd.d"));
    permissions.push_back(BrokerFilePermission::ReadOnly(
        "/usr/share/vulkan/icd.d/radeon_icd.x86_64.json"));
  }
#endif

  // TODO(b/248528896): figure out if the render node needs to be opened after
  // entering the sandbox or if this can be restricted based on API (VA-API vs.
  // V4L2) or use case (e.g., Chrome vs. ARC++/ARCVM).
  for (int i = 128; i <= 137; ++i) {
    const std::string path = base::StringPrintf("/dev/dri/renderD%d", i);
    struct stat st;
    if (stat(path.c_str(), &st) == 0) {
      permissions.push_back(options.use_amd_specific_policies
                                ? BrokerFilePermission::ReadWrite(path)
                                : BrokerFilePermission::ReadOnly(path));

#if BUILDFLAG(USE_VAAPI)
      uint32_t major = (static_cast<uint32_t>(st.st_rdev) >> 8) & 0xff;
      uint32_t minor = static_cast<uint32_t>(st.st_rdev) & 0xff;
      std::string char_device_path =
          base::StringPrintf("/sys/dev/char/%u:%u/", major, minor);
      permissions.push_back(
          BrokerFilePermission::ReadOnlyRecursive(char_device_path));
#endif  // BUILDFLAG(USE_VAAPI)
    }
  }

  sandbox::policy::SandboxLinux::GetInstance()->StartBrokerProcess(
      command_set, permissions, options);

  // TODO(b/248528896): the hardware video encoding sandbox is really only
  // useful when building with VA-API or V4L2 (otherwise, we're not really doing
  // hardware video encoding). Consider restricting the kHardwareVideoEncoding
  // sandbox type to exist only in those configurations so that the presandbox
  // hook is only reached in those scenarios. As it is now,
  // kHardwareVideoEncoding exists for all ash-chrome builds because
  // chrome/browser/ash/arc/video/gpu_arc_video_service_host.cc is expected to
  // depend on it eventually and that file is built for ash-chrome regardless
  // of VA-API/V4L2. That means that bots like linux-chromeos-rel would end up
  // reaching this presandbox hook.
#if BUILDFLAG(USE_VAAPI)
  VaapiWrapper::PreSandboxInitialization(/*allow_disabling_global_lock=*/true);

  if (options.use_amd_specific_policies) {
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
  }
#endif
  return true;
}

}  // namespace media
