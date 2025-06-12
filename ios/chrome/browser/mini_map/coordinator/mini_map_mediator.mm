// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/mini_map/coordinator/mini_map_mediator.h"

#import "base/metrics/histogram_functions.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/web/model/annotations/annotations_util.h"
#import "ios/web/public/annotations/annotations_text_manager.h"
#import "ios/web/public/web_state.h"

namespace {
// The type of address annotations.
const char* const kDecorationAddress = "ADDRESS";

// Enum representing the different outcomes of the consent flow.
// Reported in histogram, do not change order.
enum class ConsentOutcome {
  kConsentNotRequired = 0,
  kConsentSkipped = 1,
  kUserAccepted = 2,
  kUserDeclined = 3,
  kUserOpenedSettings = 4,
  kUserDismissed = 5,
  kConsentIPH = 6,
  kMaxValue = kConsentIPH,
};

// Enum representing the different outcomes of the Mini Map flow.
// Reported in histogram, do not change order.
enum class MiniMapOutcome {
  kNormalOutcome = 0,
  kOpenedURL = 1,
  kReportedAnIssue = 2,
  kOpenedSettings = 3,
  kOpenedQuery = 4,
  kUserDisabled = 5,
  kUserDisabledThenSettings = 6,
  kMaxValue = kUserDisabledThenSettings,
};

}  // namespace

@interface MiniMapMediator ()

@property(nonatomic, assign) PrefService* prefService;

// The WebState that triggered the request.
@property(assign) base::WeakPtr<web::WebState> webState;

@end

@implementation MiniMapMediator

- (instancetype)initWithPrefs:(PrefService*)prefService
                     webState:(web::WebState*)webState {
  self = [super init];
  if (self) {
    _prefService = prefService;
    if (webState) {
      _webState = webState->GetWeakPtr();
    }
  }
  return self;
}

- (void)disconnect {
  self.prefService = nil;
  self.webState = nullptr;
}

- (void)userInitiatedMiniMapWithIPH:(BOOL)showIPH {
  if (!self.prefService) {
    return;
  }

  BOOL shouldPresentIPH = showIPH && ShouldPresentConsentIPH(self.prefService);
  if (showIPH) {
    if (shouldPresentIPH) {
      base::UmaHistogramEnumeration("IOS.MiniMap.ConsentOutcome",
                                    ConsentOutcome::kConsentIPH);
      // Now that the IPH has been presented, set the flag.
      self.prefService->SetBoolean(prefs::kDetectAddressesAccepted, true);
    } else {
      base::UmaHistogramEnumeration("IOS.MiniMap.ConsentOutcome",
                                    ConsentOutcome::kConsentSkipped);
    }
  } else {
    base::UmaHistogramEnumeration("IOS.MiniMap.ConsentOutcome",
                                  ConsentOutcome::kConsentNotRequired);
  }
  [self.delegate showMapWithIPH:shouldPresentIPH];
}

- (void)userOpenedSettingsFromMiniMap {
  base::UmaHistogramEnumeration("IOS.MiniMap.Outcome",
                                MiniMapOutcome::kOpenedSettings);
}

- (void)userReportedAnIssueFromMiniMap {
  base::UmaHistogramEnumeration("IOS.MiniMap.Outcome",
                                MiniMapOutcome::kReportedAnIssue);
}

- (void)userOpenedURLFromMiniMap {
  base::UmaHistogramEnumeration("IOS.MiniMap.Outcome",
                                MiniMapOutcome::kOpenedURL);
}

- (void)userClosedMiniMap {
  base::UmaHistogramEnumeration("IOS.MiniMap.Outcome",
                                MiniMapOutcome::kNormalOutcome);
}

- (void)userOpenedQueryFromMiniMap {
  base::UmaHistogramEnumeration("IOS.MiniMap.Outcome",
                                MiniMapOutcome::kOpenedQuery);
}

- (void)userDisabledOneTapSettingFromMiniMap {
  base::UmaHistogramEnumeration("IOS.MiniMap.Outcome",
                                MiniMapOutcome::kUserDisabled);
  if (!self.prefService) {
    return;
  }
  self.prefService->SetBoolean(prefs::kDetectAddressesEnabled, false);

  if (self.webState) {
    auto* manager =
        web::AnnotationsTextManager::FromWebState(self.webState.get());
    if (manager) {
      manager->RemoveDecorationsWithType(kDecorationAddress);
    }
  }
}

- (void)userDisabledURLSettingFromMiniMap {
  // TODO(crbug.com/420402648): Add metrics.
  if (!self.prefService) {
    return;
  }
  self.prefService->SetBoolean(prefs::kIosMiniMapShowNativeMap, false);
}

- (void)userOpenedSettingsFromDisableConfirmation {
  base::UmaHistogramEnumeration("IOS.MiniMap.Outcome",
                                MiniMapOutcome::kUserDisabledThenSettings);
}

@end
