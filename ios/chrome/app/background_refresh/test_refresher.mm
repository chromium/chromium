// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/background_refresh/test_refresher.h"

#import "base/logging.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/background_refresh/background_refresh_metrics.h"

@implementation TestRefresher {
  __weak AppState* _appState;
}

@synthesize identifier = _identifier;

- (instancetype)initWithAppState:(AppState*)appState {
  if ((self = [super init])) {
    _identifier = @"TestRefresher";
    _appState = appState;
  }
  return self;
}

#pragma mark AppRefreshProvider

- (void)handleRefreshWithCompletion:(ProceduralBlock)completion {
  // TODO(crbug.com/354918403): If this provider is used outside of canary, it
  // *must* post the logging task (which reads from AppState) to the main
  // thread!
  InitStageDuringBackgroundRefreshActions stage =
      InitStageDuringBackgroundRefreshActions::kUnknown;
  switch (_appState.initStage) {
    case InitStageStart:
      stage = InitStageDuringBackgroundRefreshActions::kInitStageStart;
      break;
    case InitStageBrowserBasic:
      stage = InitStageDuringBackgroundRefreshActions::kInitStageBrowserBasic;
      break;
    case InitStageSafeMode:
      stage = InitStageDuringBackgroundRefreshActions::kInitStageSafeMode;
      break;
    case InitStageVariationsSeed:
      stage = InitStageDuringBackgroundRefreshActions::kInitStageVariationsSeed;
      break;
    case InitStageBrowserObjectsForBackgroundHandlers:
      stage = InitStageDuringBackgroundRefreshActions::
          kInitStageBrowserObjectsForBackgroundHandlers;
      break;
    case InitStageEnterprise:
      stage = InitStageDuringBackgroundRefreshActions::kInitStageEnterprise;
      break;
    case InitStageBrowserObjectsForUI:
      stage = InitStageDuringBackgroundRefreshActions::
          kInitStageBrowserObjectsForUI;
      break;
    case InitStageNormalUI:
      stage = InitStageDuringBackgroundRefreshActions::kInitStageNormalUI;
      break;
    case InitStageFirstRun:
      stage = InitStageDuringBackgroundRefreshActions::kInitStageFirstRun;
      break;
    case InitStageChoiceScreen:
      stage = InitStageDuringBackgroundRefreshActions::kInitStageChoiceScreen;
      break;
    case InitStageFinal:
      stage = InitStageDuringBackgroundRefreshActions::kInitStageFinal;
      break;
    default:
      stage = InitStageDuringBackgroundRefreshActions::kUnknown;
  };

  base::UmaHistogramEnumeration(kInitStageDuringBackgroundRefreshHistogram,
                                stage);

  completion();
}

@end
