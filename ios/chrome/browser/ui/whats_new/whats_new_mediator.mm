// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/whats_new/whats_new_mediator.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/notreached.h"
#import "base/strings/strcat.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/lens_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_lens_input_selection_command.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/whats_new_commands.h"
#import "ios/chrome/browser/ui/lens/lens_entrypoint.h"
#import "ios/chrome/browser/ui/whats_new/data_source/whats_new_data_source.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_mediator_consumer.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_util.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/public/provider/chrome/browser/password_auto_fill/password_auto_fill_api.h"
#import "url/gurl.h"

@interface WhatsNewMediator ()

@property(nonatomic, strong) NSMutableArray<WhatsNewItem*>* chromeTipEntries;

@end

// The mediator to display What's New data.
@implementation WhatsNewMediator

#pragma mark - Public

- (instancetype)init {
  self = [super init];
  if (self) {
    // Serialize What's New Chrome Tips
    self.chromeTipEntries = [[NSMutableArray alloc] init];
    for (WhatsNewItem* item in WhatsNewChromeTipEntries(WhatsNewFilePath())) {
      [self.chromeTipEntries addObject:item];
    }
  }
  return self;
}

#pragma mark - WhatsNewDetailViewActionHandler

- (void)didTapActionButton:(WhatsNewType)type
             primaryAction:(WhatsNewPrimaryAction)primaryAction
        baseViewController:(UIViewController*)baseViewController {
  base::UmaHistogramEnumeration("IOS.WhatsNew.PrimaryActionTapped", type);

  switch (primaryAction) {
    case WhatsNewPrimaryAction::kIOSSettings:
      // Handles actions that open iOS Settings.
      [self openSettingsURLString];
      break;
    case WhatsNewPrimaryAction::kPrivacySettings:
      // Handles actions that open privacy in Chrome settings.
      [self.applicationHandler
          showPrivacySettingsFromViewController:baseViewController];
      break;
    case WhatsNewPrimaryAction::kChromeSettings:
      // Handles actions that open Chrome Settings.
      [self.applicationHandler
          showSettingsFromViewController:baseViewController];
      break;
    case WhatsNewPrimaryAction::kIOSSettingsPasswords:
      // Handles actions that open Passwords in iOS Settings.
      ios::provider::PasswordsInOtherAppsOpensSettings();
      break;
    case WhatsNewPrimaryAction::kLens:
      // Handles actions that open Lens.
      // TODO(crbug.com/40943329): Add the Lens promo that contains the
      // button that triggers the Lens action.
      [self openLens];
      break;
    case WhatsNewPrimaryAction::kSafeBrowsingSettings:
      // Handles actions that open ESB in Chrome settings.
      [self.applicationHandler
          showSafeBrowsingSettingsFromViewController:baseViewController];
      break;
    case WhatsNewPrimaryAction::kChromePasswordManager:
      // Handles actions that open Chrome Password Manager.
      [self.settingsHandler showSavedPasswordsSettingsFromViewController:nil
                                                        showCancelButton:NO];
      break;
    case WhatsNewPrimaryAction::kNoAction:
    case WhatsNewPrimaryAction::kError:
      NOTREACHED_IN_MIGRATION();
      break;
  };
}

- (void)didTapLearnMoreButton:(const GURL&)learnMoreURL
                         type:(WhatsNewType)type {
  UrlLoadParams params = UrlLoadParams::InNewTab(learnMoreURL);
  params.web_params.transition_type = ui::PAGE_TRANSITION_AUTO_BOOKMARK;
  self.urlLoadingAgent->Load(params);
  base::UmaHistogramEnumeration("IOS.WhatsNew.LearnMoreTapped", type);
}

- (void)didTapInstructions:(WhatsNewType)type {
  base::UmaHistogramEnumeration("IOS.WhatsNew.InstructionsShown", type);
}

#pragma mark - WhatsNewTableViewActionHandler

- (void)recordWhatsNewInteraction:(WhatsNewItem*)item {
  base::UmaHistogramEnumeration("IOS.WhatsNew.Shown", item.type);
}

#pragma mark - Properties

- (void)setConsumer:(id<WhatsNewMediatorConsumer>)consumer {
  _consumer = consumer;

  [self updateConsumer];
}

#pragma mark Private

// Returns a `WhatsNewItem` representing a highlighted chrome tip.
- (WhatsNewItem*)whatsNewChromeTipItem {
  // Return a random chrome tip.
  int entryIndex = arc4random_uniform(self.chromeTipEntries.count);
  return self.chromeTipEntries[entryIndex];
}

// Returns an Array of `WhatsNewItem` features.
- (NSArray<WhatsNewItem*>*)whatsNewFeatureItems {
  return WhatsNewFeatureEntries(WhatsNewFilePath());
}

// Called to allow the user to go to Chrome's settings.
- (void)openSettingsURLString {
  [[UIApplication sharedApplication]
                openURL:[NSURL URLWithString:UIApplicationOpenSettingsURLString]
                options:{}
      completionHandler:nil];
}

// Called to opens Lens.
- (void)openLens {
  // Dismiss the What's New modal since Lens must be displayed in a fullscreen
  // modal.
  [self.whatsNewHandler dismissWhatsNew];
  OpenLensInputSelectionCommand* command = [[OpenLensInputSelectionCommand
      alloc]
          initWithEntryPoint:LensEntrypoint::WhatsNewPromo
           presentationStyle:LensInputSelectionPresentationStyle::SlideFromRight
      presentationCompletion:nil];
  [self.lensHandler openLensInputSelection:command];
}

// Update the consumer with What's New items.
- (void)updateConsumer {
  [self.consumer setWhatsNewProperties:[self whatsNewChromeTipItem]
                          featureItems:[self whatsNewFeatureItems]];
}

@end
