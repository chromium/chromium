// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/glic/coordinator/glic_consent_mediator.h"

#import <memory>

#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/intelligence/glic/coordinator/glic_consent_mediator_delegate.h"
#import "ios/chrome/browser/intelligence/glic/metrics/glic_metrics.h"
#import "ios/chrome/browser/intelligence/glic/ui/glic_consent_view_controller.h"
#import "ios/chrome/browser/intelligence/glic/ui/glic_constants.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "url/gurl.h"

@implementation GLICConsentMediator {
  raw_ptr<Browser> _browser;
}

- (instancetype)initWithBrowser:(Browser*)browser {
  self = [super init];
  if (self) {
    _browser = browser;
  }
  return self;
}

#pragma mark - GLICConsentMutator

// Did consent to GLIC.
- (void)didConsentGLIC {
  base::UmaHistogramEnumeration(kGLICConsentTypeHistogram,
                                GLICConsentType::kAccept);
  _browser->GetProfile()->GetPrefs()->SetBoolean(prefs::kIOSGLICConsent, true);
  [_delegate dismissGLICConsentUI];
}

// Did dismisses the Consent UI.
- (void)didRefuseGLICConsent {
  base::UmaHistogramEnumeration(kGLICConsentTypeHistogram,
                                GLICConsentType::kCancel);
  [_delegate dismissGLICConsentUI];
}

// Did close GLIC Promo UI.
- (void)didCloseGLICPromo {
  [_delegate dismissGLICConsentUI];
}

// Handle tap on learn about your choices.
- (void)handleLearnAboutYourChoicesTapped {
  OpenNewTabCommand* command = [OpenNewTabCommand
      commandWithURLFromChrome:GURL(base::SysNSStringToUTF8(
                                   kGLICLearnAboutYourChoices))
                   inIncognito:_browser->GetProfile()->IsOffTheRecord()];

  [HandlerForProtocol(_browser->GetCommandDispatcher(), ApplicationCommands)
      openURLInNewTab:command];
}

@end
