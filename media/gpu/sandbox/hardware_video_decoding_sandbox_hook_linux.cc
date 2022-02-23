// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/sandbox/hardware_video_decoding_sandbox_hook_linux.h"

#include <dlfcn.h>
#include <sys/stat.h>

#include "base/strings/stringprintf.h"
#include "media/gpu/buildflags.h"

#if BUILDFLAG(USE_VAAPI)
#include "media/gpu/vaapi/vaapi_wrapper.h"
#endif

using sandbox::syscall_broker::BrokerFilePermission;

namespace media {

bool HardwareVideoDecodingPreSandboxHook(
    sandbox::policy::SandboxLinux::Options options) {
  sandbox::syscall_broker::BrokerCommandSet command_set;
  std::vector<BrokerFilePermission> permissions;

#if BUILDFLAG(USE_V4L2_CODEC)
  command_set.set(sandbox::syscall_broker::COMMAND_OPEN);

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
#elif BUILDFLAG(USE_VAAPI)
  command_set.set(sandbox::syscall_broker::COMMAND_OPEN);

  if (options.use_amd_specific_policies) {
    command_set.set(sandbox::syscall_broker::COMMAND_ACCESS);
    command_set.set(sandbox::syscall_broker::COMMAND_STAT);
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
  }

  // TODO(b/195769334): for now, this is only needed for two use cases: the
  // legacy VaapiVideoDecodeAccelerator and AMD. However, we'll likely need this
  // unconditionally so that we can allocate dma-bufs.
  for (int i = 128; i <= 137; ++i) {
    const std::string path = base::StringPrintf("/dev/dri/renderD%d", i);
    struct stat st;
    if (stat(path.c_str(), &st) == 0) {
      permissions.push_back(options.use_amd_specific_policies
                                ? BrokerFilePermission::ReadWrite(path)
                                : BrokerFilePermission::ReadOnly(path));
    }
  }
#endif  // BUILDFLAG(USE_V4L2_CODEC)

  sandbox::policy::SandboxLinux::GetInstance()->StartBrokerProcess(
      command_set, permissions, sandbox::policy::SandboxLinux::PreSandboxHook(),
      options);

  // TODO(b/195769334): the hardware video decoding sandbox is really only
  // useful when building with VA-API or V4L2 (otherwise, we're not really doing
  // hardware video decoding). Consider restricting the kHardwareVideoDecoding
  // sandbox type to exist only in those configurations so that the presandbox
  // hook is only reached in those scenarios. As it is now,
  // kHardwareVideoDecoding exists for all ash-chrome builds because
  // chrome/browser/ash/arc/video/gpu_arc_video_service_host.cc depends on it
  // and that file is built for ash-chrome regardless of VA-API/V4L2. That means
  // that bots like linux-chromeos-rel end up reaching this presandbox hook.
#if BUILDFLAG(USE_VAAPI)
  VaapiWrapper::PreSandboxInitialization();

  if (options.use_amd_specific_policies) {
    const char* radeonsi_lib = "/usr/lib64/dri/radeonsi_dri.so";
#if defined(DRI_DRIVER_DIR)
    radeonsi_lib = DRI_DRIVER_DIR "/radeonsi_dri.so";
#endif
    if (nullptr ==
        dlopen(radeonsi_lib, RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE)) {
      LOG(ERROR) << "dlopen(radeonsi_dri.so) failed with error: " << dlerror();
      return false;
    }
  }
#elif BUILDFLAG(USE_V4L2_CODEC) && BUILDFLAG(USE_LIBV4L2)
#if defined(__aarch64__)
  dlopen("/usr/lib64/libv4l2.so", RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE);
#else
  dlopen("/usr/lib/libv4l2.so", RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE);
#endif  // defined(__aarch64__)
#endif  // BUILDFLAG(USE_VAAPI)

  return true;
}

}  // namespace media
