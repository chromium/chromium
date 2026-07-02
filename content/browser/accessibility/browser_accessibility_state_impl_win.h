// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_STATE_IMPL_WIN_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_STATE_IMPL_WIN_H_

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "content/common/content_export.h"
#include "ui/accessibility/ax_mode.h"

namespace content::internal {

CONTENT_EXPORT int HashUiaClientProcessName(std::string_view process_name);
CONTENT_EXPORT void RecordUiaClientDisconnectedHistogram(
    std::string_view process_name);
CONTENT_EXPORT void RecordUiaClientProcessHistogramsForModeChange(
    ui::AXMode old_mode,
    ui::AXMode new_mode,
    std::vector<std::string> process_names);

// Returns true if the specified JAWS version (from `fsdomsrv.dll`'s product
// version, e.g. 2026.2606.132) still relies on the synthetic tab selection
// event that Chromium fires on window activation to restore per-tab settings.
// Newer versions of JAWS detect tab changes on their own. See
// https://crbug.com/505781387.
CONTENT_EXPORT bool DoesJawsVersionNeedTabSelectionEvent(uint16_t major,
                                                         uint16_t minor,
                                                         uint16_t build);

}  // namespace content::internal

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_STATE_IMPL_WIN_H_
