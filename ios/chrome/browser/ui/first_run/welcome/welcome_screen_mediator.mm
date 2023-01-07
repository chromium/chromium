// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/welcome/welcome_screen_mediator.h"

#import "components/metrics/metrics_pref_names.h"
#import "components/prefs/pref_service.h"
#import "components/web_resource/web_resource_pref_names.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/policy/policy_util.h"
#import "ios/chrome/browser/ui/first_run/first_run_util.h"
#import "ios/chrome/browser/ui/first_run/welcome/welcome_screen_consumer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface WelcomeScreenMediator ()

@end

@implementation WelcomeScreenMediator

#pragma mark - Public

- (instancetype)init {
  self = [super init];
  if (self) {
    RecordMetricsReportingDefaultState();
    _UMAReportingUserChoice = kDefaultMetricsReportingCheckboxValue;
  }
  return self;
}

- (BOOL)isCheckboxSelectedByDefault {
  return kDefaultMetricsReportingCheckboxValue;
}

- (void)setMetricsReportingEnabled:(BOOL)enabled {
  GetApplicationContext()->GetLocalState()->SetBoolean(
      metrics::prefs::kMetricsReportingEnabled, enabled);
}

- (void)acceptToS {
  PrefService* prefs = GetApplicationContext()->GetLocalState();
  // Sets a LocalState pref marking EULA as accepted.
  if (!prefs->GetBoolean(prefs::kEulaAccepted)) {
    prefs->SetBoolean(prefs::kEulaAccepted, true);
    prefs->CommitPendingWrite();
  }
}

#pragma mark - Properties

- (void)setConsumer:(id<WelcomeScreenConsumer>)consumer {
  _consumer = consumer;
  self.consumer.isManaged = IsApplicationManagedByPlatform();
}

@end
