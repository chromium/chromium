// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/coordinator/main_toolbar_mediator.h"

#import "base/metrics/histogram_functions.h"
#import "components/omnibox/browser/omnibox_pref_names.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/browser_layout_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/utils/observable_boolean.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"

namespace layout_state {
class MainToolbarMediatorPassKeyFactory {
 public:
  static base::PassKey<MainToolbarMediatorPassKeyFactory> CreateKey() {
    return base::PassKey<MainToolbarMediatorPassKeyFactory>();
  }
};
}  // namespace layout_state

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

// Logs the omnibox position at startup.
void LogOmniboxPosition(PrefService* local_state) {
  if (!IsBottomOmniboxAvailable()) {
    return;
  }
  static dispatch_once_t once;
  dispatch_once(&once, ^{
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

// Helper function to return the domain passkey used to mutate the layout state.
inline LayoutStateToolbarPassKey PassKey() {
  return layout_state::MainToolbarMediatorPassKeyFactory::CreateKey();
}

}  // namespace

@interface MainToolbarMediator () <BooleanObserver>
@end

@implementation MainToolbarMediator {
  PrefBackedBoolean* _bottomOmniboxPref;
  __weak BrowserLayoutState* _browserLayoutState;
}

- (instancetype)initWithPrefService:(PrefService*)prefService
                 browserLayoutState:(BrowserLayoutState*)browserLayoutState {
  self = [super init];
  if (self) {
    CHECK(prefService);
    CHECK(browserLayoutState);
    _browserLayoutState = browserLayoutState;
    _bottomOmniboxPref = [[PrefBackedBoolean alloc]
        initWithPrefService:prefService
                   prefName:omnibox::kIsOmniboxInBottomPosition];
    [_bottomOmniboxPref setObserver:self];

    // Log the initial startup metrics.
    LogOmniboxPosition(prefService);

    if (IsChromeNextIaEnabled()) {
      // Set the initial toolbar position.
      [_browserLayoutState setToolbarPosition:[self isBottomOmniboxPrefEnabled]
                                                  ? ToolbarPosition::kBottom
                                                  : ToolbarPosition::kTop
                                      passKey:PassKey()];
    }
  }
  return self;
}

- (void)disconnect {
  [_bottomOmniboxPref stop];
  _bottomOmniboxPref = nil;
  _browserLayoutState = nil;
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  if (observableBoolean == _bottomOmniboxPref) {
    if (IsChromeNextIaEnabled()) {
      [_browserLayoutState setToolbarPosition:[self isBottomOmniboxPrefEnabled]
                                                  ? ToolbarPosition::kBottom
                                                  : ToolbarPosition::kTop
                                      passKey:PassKey()];
    }
  }
}

#pragma mark - Private

// Returns whether the bottom omnibox preference is enabled.
- (BOOL)isBottomOmniboxPrefEnabled {
  return IsBottomOmniboxAvailable() && _bottomOmniboxPref.value;
}

@end
