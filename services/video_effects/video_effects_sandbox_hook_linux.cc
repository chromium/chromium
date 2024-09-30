// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_effects/video_effects_sandbox_hook_linux.h"

#include <dlfcn.h>

#include "sandbox/policy/linux/sandbox_linux.h"
#include "services/on_device_model/ml/chrome_ml.h"

namespace video_effects {

bool VideoEffectsPreSandboxHook(
    sandbox::policy::SandboxLinux::Options options) {
  // Ensure the optimization_guide_internal shared library is loaded before the
  // sandbox is initialized.
  const auto path = ml::GetChromeMLPath();
  // We don't want to unload the library so not using `ChromeMLHolder` here.
  void* ml =
      dlopen(path.value().c_str(), RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE);
  if (!ml) {
    // The library may be missing on developer builds, we should tolerate that.
    // The features in Video Effects Service that require the library will not
    // be usable, but we should gracefully handle failures there as well.

    LOG(ERROR) << "Failed to open Chrome ML shared library!";
  } else {
    DVLOG(1) << "Successfully opened Chrome ML shared library.";
  }

  auto* instance = sandbox::policy::SandboxLinux::GetInstance();
  instance->EngageNamespaceSandboxIfPossible();
  return true;
}

}  // namespace video_effects
