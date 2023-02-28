// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/commands/editing_command_filter.h"
#include "base/feature_list.h"

#include "third_party/blink/public/common/features.h"

namespace blink {

bool IsCommandFilteredOut(const String& command_name) {
#if BUILDFLAG(IS_ANDROID)
  bool extended_shortcuts_enabled = base::FeatureList::IsEnabled(
      blink::features::kAndroidExtendedKeyboardShortcuts);
  if (!extended_shortcuts_enabled) {
    if (command_name == "DeleteToBeginningOfLine") {
      return true;
    }
  }
#endif  // BUILDFLAG(IS_ANDROID)
  return false;
}
}  // namespace blink
