// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_STATE_IMPL_WIN_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_STATE_IMPL_WIN_H_

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

}  // namespace content::internal

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_STATE_IMPL_WIN_H_
