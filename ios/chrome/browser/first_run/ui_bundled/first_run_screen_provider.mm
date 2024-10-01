// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/first_run_screen_provider.h"

#import "base/notreached.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/policy/model/browser_state_policy_connector.h"
#import "ios/chrome/browser/search_engine_choice/model/search_engine_choice_util.h"
#import "ios/chrome/browser/search_engines/model/search_engine_choice_service_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/screen/screen_provider+protected.h"
#import "ios/chrome/browser/ui/screen/screen_type.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_ui_util.h"
#import "ios/public/provider/chrome/browser/signin/choice_api.h"

@implementation FirstRunScreenProvider

- (instancetype)initForProfile:(ProfileIOS*)profile {
  NSMutableArray* screens = [NSMutableArray array];
  [screens addObject:@(kSignIn)];
  [screens addObject:@(kHistorySync)];

  if (ShouldDisplaySearchEngineChoiceScreen(
          *profile, /*is_first_run_entrypoint=*/true,
          /*app_started_via_external_intent=*/false)) {
    [screens addObject:@(kChoice)];
  }

  [screens addObject:@(kDefaultBrowserPromo)];

  DockingPromoDisplayTriggerArm experimentArm =
      DockingPromoExperimentTypeEnabled();

  if (IsDockingPromoEnabled() &&
      experimentArm == DockingPromoDisplayTriggerArm::kDuringFRE) {
    [screens addObject:@(kDockingPromo)];
  }

  [screens addObject:@(kStepsCompleted)];
  return [super initWithScreens:screens];
}

@end
