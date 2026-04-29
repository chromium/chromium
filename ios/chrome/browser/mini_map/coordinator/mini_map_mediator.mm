// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/mini_map/coordinator/mini_map_mediator.h"

#import "base/memory/raw_ptr.h"
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

// Returns the histogram name for the consent outcome based on the query type.
std::string_view MiniMapConsentOutcomeHistogram(MiniMapQueryType type) {
  switch (type) {
    case MiniMapQueryType::kText:
      return "IOS.MiniMap.ConsentOutcome";
    case MiniMapQueryType::kURL:
      return "IOS.MiniMap.Link.ConsentOutcome";
  }
}

// Returns the histogram name for the flow outcome based on the query type.
std::string_view MiniMapOutcomeHistogram(MiniMapQueryType type) {
  switch (type) {
    case MiniMapQueryType::kText:
      return "IOS.MiniMap.Outcome";
    case MiniMapQueryType::kURL:
      return "IOS.MiniMap.Link.Outcome";
  }
}

}  // namespace

@interface MiniMapMediator ()
@end

@implementation MiniMapMediator {
  MiniMapQueryType _type;
  raw_ptr<PrefService> _prefService;
  base::WeakPtr<web::WebState> _webState;
}

- (instancetype)initWithPrefs:(PrefService*)prefService
                     webState:(web::WebState*)webState
                         type:(MiniMapQueryType)type {
  self = [super init];
  if (self) {
    _prefService = prefService;
    if (webState) {
      _webState = webState->GetWeakPtr();
    }
    _type = type;
  }
  return self;
}

- (void)disconnect {
  _prefService = nil;
  _webState = nullptr;
}

- (void)userInitiatedMiniMapWithIPH:(BOOL)showIPH {
  if (!_prefService) {
    return;
  }

  BOOL shouldPresentIPH = showIPH && ShouldPresentConsentIPH(_prefService);
  if (showIPH) {
    if (shouldPresentIPH) {
      base::UmaHistogramEnumeration(MiniMapConsentOutcomeHistogram(_type),
                                    ConsentOutcome::kConsentIPH);
      // Now that the IPH has been presented, set the flag.
      _prefService->SetBoolean(prefs::kDetectAddressesAccepted, true);
    } else {
      base::UmaHistogramEnumeration(MiniMapConsentOutcomeHistogram(_type),
                                    ConsentOutcome::kConsentSkipped);
    }
  } else {
    base::UmaHistogramEnumeration(MiniMapConsentOutcomeHistogram(_type),
                                  ConsentOutcome::kConsentNotRequired);
  }
  [self.delegate showMapWithIPH:shouldPresentIPH];
}

- (void)userOpenedSettingsFromMiniMap {
  base::UmaHistogramEnumeration(MiniMapOutcomeHistogram(_type),
                                MiniMapOutcome::kOpenedSettings);
}

- (void)userReportedAnIssueFromMiniMap {
  base::UmaHistogramEnumeration(MiniMapOutcomeHistogram(_type),
                                MiniMapOutcome::kReportedAnIssue);
}

- (void)userOpenedURLFromMiniMap {
  base::UmaHistogramEnumeration(MiniMapOutcomeHistogram(_type),
                                MiniMapOutcome::kOpenedURL);
}

- (void)userClosedMiniMap {
  base::UmaHistogramEnumeration(MiniMapOutcomeHistogram(_type),
                                MiniMapOutcome::kNormalOutcome);
}

- (void)userOpenedQueryFromMiniMap {
  base::UmaHistogramEnumeration(MiniMapOutcomeHistogram(_type),
                                MiniMapOutcome::kOpenedQuery);
}

- (void)userDisabledOneTapSettingFromMiniMap {
  base::UmaHistogramEnumeration(MiniMapOutcomeHistogram(_type),
                                MiniMapOutcome::kUserDisabled);
  if (!_prefService) {
    return;
  }
  _prefService->SetBoolean(prefs::kDetectAddressesEnabled, false);

  if (_webState) {
    auto* manager = web::AnnotationsTextManager::FromWebState(_webState.get());
    if (manager) {
      manager->RemoveDecorationsWithType(kDecorationAddress);
    }
  }
}

- (void)userDisabledURLSettingFromMiniMap {
  base::UmaHistogramEnumeration(MiniMapOutcomeHistogram(_type),
                                MiniMapOutcome::kUserDisabled);
  if (!_prefService) {
    return;
  }
  _prefService->SetBoolean(prefs::kIosMiniMapShowNativeMap, false);
}

- (void)userOpenedSettingsFromDisableConfirmation {
  base::UmaHistogramEnumeration(MiniMapOutcomeHistogram(_type),
                                MiniMapOutcome::kUserDisabledThenSettings);
}

@end
