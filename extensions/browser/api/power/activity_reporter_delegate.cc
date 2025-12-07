// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/power/activity_reporter_delegate.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "extensions/browser/api/power/activity_reporter_delegate_ash.h"
#endif

namespace extensions {

// static
std::unique_ptr<ActivityReporterDelegate>
ActivityReporterDelegate::GetDelegate() {
#if BUILDFLAG(IS_CHROMEOS)
  return std::make_unique<ActivityReporterDelegateAsh>();
#else
#error Unsupported platform
#endif
}

}  // namespace extensions
