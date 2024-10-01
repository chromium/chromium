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
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"

@interface TestRefresherTask : NSObject <AppRefreshProviderTask>

- (instancetype)initWithAppState:(AppState*)appState;

@end

@implementation TestRefresher {
  TestRefresherTask* _task;
}

@synthesize identifier = _identifier;

- (instancetype)initWithAppState:(AppState*)appState {
  if ((self = [super init])) {
    _identifier = @"TestRefresher";
    _task = [[TestRefresherTask alloc] initWithAppState:(AppState*)appState];
  }
  return self;
}

#pragma mark AppRefreshProvider

// This provider runs on the main thread.
- (scoped_refptr<base::SingleThreadTaskRunner>)taskThread {
  return web::GetUIThreadTaskRunner({});
}

- (id<AppRefreshProviderTask>)task {
  return _task;
}

@end

@implementation TestRefresherTask {
  SEQUENCE_CHECKER(_sequenceChecker);
  __weak AppState* _appState;
}

- (instancetype)initWithAppState:(AppState*)appState {
  if ((self = [super init])) {
    _appState = appState;
  }
  return self;
}

- (void)execute {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
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
}

@end
