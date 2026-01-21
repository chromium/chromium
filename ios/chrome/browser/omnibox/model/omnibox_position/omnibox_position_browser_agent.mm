// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/omnibox_position/omnibox_position_browser_agent.h"

#import "base/metrics/histogram_functions.h"
#import "components/omnibox/browser/omnibox_pref_names.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"

namespace {

// Enum for the IOS.Omnibox.SteadyStatePosition histogram.
// Keep in sync with "OmniboxPositionType".
// LINT.IfChange(OmniboxPositionType)
enum class OmniboxPositionType {
  kTop = 0,
  kBottom = 1,
  kMaxValue = kBottom,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:OmniboxPositionType)

const char kOmniboxSteadyStatePositionAtStartup[] =
    "IOS.Omnibox.SteadyStatePositionAtStartup";

const char kOmniboxSteadyStatePositionAtStartupSelected[] =
    "IOS.Omnibox.SteadyStatePositionAtStartup.Selected";

}  // namespace

OmniboxPositionBrowserAgent::OmniboxPositionBrowserAgent(Browser* browser)
    : BrowserUserData(browser) {
  LogOmniboxPosition();
}

OmniboxPositionBrowserAgent::~OmniboxPositionBrowserAgent() = default;

BOOL OmniboxPositionBrowserAgent::IsOmniboxFocused() const {
  return [omnibox_state_provider_ isOmniboxFocused];
}

bool OmniboxPositionBrowserAgent::IsCurrentLayoutBottomOmnibox() {
  return is_current_layout_bottom_omnibox_;
}

void OmniboxPositionBrowserAgent::SetIsCurrentLayoutBottomOmnibox(
    bool is_current_layout_bottom_omnibox) {
  if (is_current_layout_bottom_omnibox_ == is_current_layout_bottom_omnibox) {
    return;
  }
  is_current_layout_bottom_omnibox_ = is_current_layout_bottom_omnibox;
  for (OmniboxPositionBrowserAgentObserver& observer : observers_) {
    observer.OmniboxPositionBrowserAgentHasNewBottomLayout(
        this, is_current_layout_bottom_omnibox_);
  }
}

void OmniboxPositionBrowserAgent::AddObserver(
    OmniboxPositionBrowserAgentObserver* observer) {
  observers_.AddObserver(observer);
}

void OmniboxPositionBrowserAgent::RemoveObserver(
    OmniboxPositionBrowserAgentObserver* observer) {
  observers_.RemoveObserver(observer);
}

void OmniboxPositionBrowserAgent::LogOmniboxPosition() {
  if (!IsBottomOmniboxAvailable()) {
    return;
  }
  static dispatch_once_t once;
  dispatch_once(&once, ^{
    PrefService* local_state = GetApplicationContext()->GetLocalState();
    const bool is_bottom_omnibox =
        local_state->GetBoolean(omnibox::kIsOmniboxInBottomPosition);
    OmniboxPositionType position_type = is_bottom_omnibox
                                            ? OmniboxPositionType::kBottom
                                            : OmniboxPositionType::kTop;
    base::UmaHistogramEnumeration(kOmniboxSteadyStatePositionAtStartup,
                                  position_type);

    if (local_state->GetUserPrefValue(omnibox::kIsOmniboxInBottomPosition)) {
      base::UmaHistogramEnumeration(
          kOmniboxSteadyStatePositionAtStartupSelected, position_type);
    }
  });
}
