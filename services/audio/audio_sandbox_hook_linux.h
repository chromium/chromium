// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_AUDIO_SANDBOX_HOOK_LINUX_H_
#define SERVICES_AUDIO_AUDIO_SANDBOX_HOOK_LINUX_H_

#include "sandbox/policy/linux/sandbox_linux.h"

namespace audio {

// Load audio shared libraries and setup allowed commands and filesystem
// permissions for audio service sandboxed process.
bool AudioPreSandboxHook(sandbox::policy::SandboxLinux::Options options);

}  // namespace audio

#endif  // SERVICES_AUDIO_AUDIO_SANDBOX_HOOK_LINUX_H_
