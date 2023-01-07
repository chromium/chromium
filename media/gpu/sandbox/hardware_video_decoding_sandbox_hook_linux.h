// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_SANDBOX_HARDWARE_VIDEO_DECODING_SANDBOX_HOOK_LINUX_H_
#define MEDIA_GPU_SANDBOX_HARDWARE_VIDEO_DECODING_SANDBOX_HOOK_LINUX_H_

#include "sandbox/policy/linux/sandbox_linux.h"

namespace media {

bool HardwareVideoDecodingPreSandboxHook(
    sandbox::policy::SandboxLinux::Options options);

}  // namespace media

#endif  // MEDIA_GPU_SANDBOX_HARDWARE_VIDEO_DECODING_SANDBOX_HOOK_LINUX_H_
