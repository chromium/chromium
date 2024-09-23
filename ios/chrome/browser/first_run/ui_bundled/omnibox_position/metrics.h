// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_OMNIBOX_POSITION_METRICS_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_OMNIBOX_POSITION_METRICS_H_

#import "base/time/time.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_type.h"

namespace segmentation_platform {
class DeviceSwitcherResultDispatcher;
}  // namespace segmentation_platform

// Enum for the IOS.Omnibox.Promo.Events histogram.
// Keep in sync with "OmniboxPositionChoiceScreenEvents".
// LINT.IfChange
enum class OmniboxPositionChoiceScreenEvent {
  kUnknown = 0,               // Never logged.
  kScreenDisplayed = 1,       // Omnibox position choice screen was displayed.
  kTopOptionSelected = 2,     // User selected the top option.
  kBottomOptionSelected = 3,  // User selected the bottom option.
  kPositionValidated = 4,     // User validated the position.
  kPositionDiscarded = 5,     // User discarded the position.
  kScreenSkipped = 6,         // User skipped the screen.
  kPromoRegistered = 7,       // The promo was registered in the promo manager.
  kMaxValue = kPromoRegistered,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml)

// Enum for the IOS.Omnibox.Promo.SelectedPosition histogram.
// Keep in sync with "OmniboxPromoSelectedPositions".
// LINT.IfChange
enum class OmniboxPromoSelectedPosition {
  kTopDefault = 0,        // Top validated while top was default.
  kBottomDefault = 1,     // Bottom validated while bottom was default.
  kTopNotDefault = 2,     // Top validated while bottom was default.
  kBottomNotDefault = 3,  // Bottom validated while top was default.
  kMaxValue = kBottomNotDefault,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml)

/// Records the omnibox position choice screen `event`. Also record the
/// associated user action if needed.
void RecordScreenEvent(OmniboxPositionChoiceScreenEvent event);

/// Records the selected omnibox position.
/// `toolbar_type`: The selected toolbar for the omnibox.
/// `is_default`: Whether the selected option is the default option.
/// `device_switcher_result_dispatcher`: Used to classify user as Safari
/// switcher.
void RecordSelectedPosition(
    ToolbarType toolbar_type,
    BOOL is_default,
    segmentation_platform::DeviceSwitcherResultDispatcher*
        device_switcher_result_dispatcher);

/// Records the time `elapsed` between the screen show and dismiss.
void RecordTimeOpen(base::TimeDelta elapsed);

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_OMNIBOX_POSITION_METRICS_H_
