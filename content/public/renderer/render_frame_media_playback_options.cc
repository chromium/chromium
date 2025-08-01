// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/renderer/render_frame_media_playback_options.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include "ui/base/device_form_factor.h"
#endif

namespace content {
bool IsBackgroundMediaSuspendEnabled() {
#if BUILDFLAG(IS_ANDROID)
  // For Android devices, do not suspend background media for large form
  // factors.
  auto device_form_factor = ui::GetDeviceFormFactor();

  return !(device_form_factor == ui::DEVICE_FORM_FACTOR_TABLET ||
           device_form_factor == ui::DEVICE_FORM_FACTOR_DESKTOP);
#else
  // For non-Android devices, always allow background media to play
  return false;
#endif
}
}  // namespace content
