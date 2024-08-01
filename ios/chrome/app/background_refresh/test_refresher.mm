// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/background_refresh/test_refresher.h"

#import "base/logging.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "ios/chrome/app/application_delegate/app_state.h"

@implementation TestRefresher {
  __weak AppState* _appState;
}

@synthesize identifier = _identifier;

- (instancetype)initWithAppState:(AppState*)appState {
  if (self = [super init]) {
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
  std::string stage;
  switch (_appState.initStage) {
    case InitStageStart:
      stage = "InitStageStart";
      break;
    case InitStageBrowserBasic:
      stage = "InitStageBrowserBasic";
      break;
    case InitStageSafeMode:
      stage = "InitStageSafeMode";
      break;
    case InitStageVariationsSeed:
      stage = "InitStageVariationsSeed";
      break;
    case InitStageBrowserObjectsForBackgroundHandlers:
      stage = "InitStageBrowserObjectsForBackgroundHandlers";
      break;
    case InitStageEnterprise:
      stage = "InitStageEnterprise";
      break;
    case InitStageBrowserObjectsForUI:
      stage = "InitStageBrowserObjectsForUI";
      break;
    case InitStageNormalUI:
      stage = "InitStageNormalUI";
      break;
    case InitStageFirstRun:
      stage = "InitStageFirstRun";
      break;
    case InitStageChoiceScreen:
      stage = "InitStageChoiceScreen";
      break;
    case InitStageFinal:
      stage = "InitStageFinal";
      break;
    default:
      stage = base::SysNSStringToUTF8([NSString
          stringWithFormat:@"Unknown stage (%lu)",
                           static_cast<unsigned long>(_appState.initStage)]);
  };

  LOG(ERROR) << "REFRESH: Handling App Refresh -- " << stage;

  completion();
}

@end
