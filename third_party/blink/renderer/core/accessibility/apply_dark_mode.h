// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ACCESSIBILITY_APPLY_DARK_MODE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ACCESSIBILITY_APPLY_DARK_MODE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_settings.h"

namespace blink {

// Extract dark mode settings from |settings| and modify them as needed
// based on |root|.
DarkModeSettings CORE_EXPORT BuildDarkModeSettings(const Settings& settings,
                                                   const LayoutView& root);

// Determine whether the page with the provided |root| should have its colors
// inverted, based on the provided |policy|.
//
// This method does not check whether Dark Mode is enabled overall.
bool CORE_EXPORT ShouldApplyDarkModeFilterToPage(DarkModePagePolicy policy,
                                                 const LayoutView& root);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ACCESSIBILITY_APPLY_DARK_MODE_H_
