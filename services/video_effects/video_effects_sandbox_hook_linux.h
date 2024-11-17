// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_EFFECTS_VIDEO_EFFECTS_SANDBOX_HOOK_LINUX_H_
#define SERVICES_VIDEO_EFFECTS_VIDEO_EFFECTS_SANDBOX_HOOK_LINUX_H_

#include "sandbox/policy/linux/sandbox_linux.h"

namespace video_effects {

// Loads the Chrome ML (optimization_guide_internal.so) library.
bool VideoEffectsPreSandboxHook(sandbox::policy::SandboxLinux::Options options);

}  // namespace video_effects

#endif  // SERVICES_VIDEO_EFFECTS_VIDEO_EFFECTS_SANDBOX_HOOK_LINUX_H_
