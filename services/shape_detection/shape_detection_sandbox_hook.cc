// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shape_detection/shape_detection_sandbox_hook.h"

#include <dlfcn.h>

#include "build/branding_buildflags.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "services/shape_detection/shape_detection_library_holder.h"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace shape_detection {

bool ShapeDetectionPreSandboxHook(
    sandbox::policy::SandboxLinux::Options options) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Ensure the shape_detection_internal shared library is loaded before the
  // sandbox is initialized.
  const auto path = shape_detection::GetChromeShapeDetectionPath();
  // We don't want to unload the library so not using
  // `ShapeDetectionLibraryHolder` here.
  void* dl =
      dlopen(path.value().c_str(), RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE);
  if (!dl) {
    LOG(ERROR) << "Failed to open Chrome Shape Detection shared library!";
    return false;
  } else {
    DVLOG(1) << "Successfully opened Chrome Shape Detection shared library.";
  }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
  auto* instance = sandbox::policy::SandboxLinux::GetInstance();
  instance->EngageNamespaceSandboxIfPossible();
  return true;
}

}  // namespace shape_detection
