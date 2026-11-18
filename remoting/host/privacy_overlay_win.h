// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_PRIVACY_OVERLAY_WIN_H_
#define REMOTING_HOST_PRIVACY_OVERLAY_WIN_H_

#include <optional>
#include <string_view>

#include "base/auto_reset.h"

namespace remoting {

// Test override budgets for `PrivacyOverlayWin`'s Disconnect button dialog:
// `max_width` is the soft dialog width budget that triggers `BS_MULTILINE`
// wrapping, `hard_max_width` is the ceiling the dialog may widen up to when a
// single unbroken word exceeds `max_width`, and `max_height` (when > 0)
// overrides the monitor height budget.
struct ButtonDialogWidthOverrideForTesting {
  int max_width = 0;
  int hard_max_width = 0;
  int max_height = 0;
};

// Overrides the Disconnect button dialog's monitor size budgets in unit tests
// so narrow-monitor multi-line button wrapping, unbroken-word outer-margin
// reclamation, and short-monitor height capping can be tested end-to-end.
// `max_width` must be strictly positive, `hard_max_width` (when specified) must
// be >= `max_width`, and `max_height` (when specified) must be strictly
// positive.
base::AutoReset<std::optional<ButtonDialogWidthOverrideForTesting>>
ScopedOverrideMaxButtonDialogWidthForTesting(
    int max_width,
    std::optional<int> hard_max_width = std::nullopt,
    std::optional<int> max_height = std::nullopt);

// Overrides the localized Disconnect button label read during `BeginOverlay()`
// in unit tests so multi-word wrapping and single-word widening can be tested
// end-to-end independent of the host OS locale's `IDD_DISCONNECT` resource
// string. `button_text` must be non-empty and its backing storage must outlive
// the returned `AutoReset`.
base::AutoReset<std::optional<std::wstring_view>>
ScopedOverrideDisconnectButtonTextForTesting(std::wstring_view button_text);

}  // namespace remoting

#endif  // REMOTING_HOST_PRIVACY_OVERLAY_WIN_H_
