// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/whats_new/whats_new_mediator.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/notreached.h"
#import "base/strings/strcat.h"
#import "ios/chrome/browser/default_browser/utils.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/ui/whats_new/data_source/whats_new_data_source.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_mediator_consumer.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_util.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/public/provider/chrome/browser/password_auto_fill/password_auto_fill_api.h"
#import "url/gurl.h"

@interface WhatsNewMediator ()

@property(nonatomic, strong) NSMutableArray<WhatsNewItem*>* chromeTipEntries;
@property(nonatomic, strong) WhatsNewItem* useChromeByDefaultEntry;

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
      // Save use chrome by default entry separately.
      if (item.type == WhatsNewType::kUseChromeByDefault) {
        self.useChromeByDefaultEntry = item;
        continue;
      }
      [self.chromeTipEntries addObject:item];
    }
  }
  return self;
}

#pragma mark - WhatsNewDetailViewActionHandler

- (void)didTapActionButton:(WhatsNewType)type
             primaryAction:(WhatsNewPrimaryAction)primaryAction {
  const char* type_str = WhatsNewTypeToString(type);
  if (!type_str) {
    return;
  }

  std::string metric =
      base::StrCat({"WhatsNew.", type_str, ".PrimaryActionTapped"});
  base::RecordAction(base::UserMetricsAction(metric.c_str()));

  switch (primaryAction) {
    case WhatsNewPrimaryAction::kIOSSettings:
      // Handles actions that open iOS Settings.
      [self openSettingsURLString];
      break;
    case WhatsNewPrimaryAction::kPrivacySettings:
      // Handles actions that open privacy in Chrome settings.
      [self.handler
          showPrivacySettingsFromViewController:self.baseViewController];
      break;
    case WhatsNewPrimaryAction::kChromeSettings:
      // Handles actions that open Chrome Settings.
      [self.handler showSettingsFromViewController:self.baseViewController];
      break;
    case WhatsNewPrimaryAction::kIOSSettingsPasswords:
      // Handles actions that open Passwords in iOS Settings.
      ios::provider::PasswordsInOtherAppsOpensSettings();
      break;
    case WhatsNewPrimaryAction::kNoAction:
    case WhatsNewPrimaryAction::kError:
      NOTREACHED();
      break;
  };
}

- (void)didTapLearnMoreButton:(const GURL&)learnMoreURL
                         type:(WhatsNewType)type {
  UrlLoadParams params = UrlLoadParams::InNewTab(learnMoreURL);
  params.web_params.transition_type = ui::PAGE_TRANSITION_AUTO_BOOKMARK;
  self.urlLoadingAgent->Load(params);
  [self recordLearnMoreInteraction:type];
}

- (void)didTapInstructions:(WhatsNewType)type {
  const char* type_str = WhatsNewTypeToStringM116(type);
  if (!type_str) {
    return;
  }

  std::string metric =
      base::StrCat({"WhatsNew.", type_str, ".InstructionsTapped"});
  base::RecordAction(base::UserMetricsAction(metric.c_str()));
  base::UmaHistogramEnumeration("IOS.WhatsNew.InstructionsShown", type);
}

#pragma mark - WhatsNewTableViewActionHandler

- (void)recordWhatsNewInteraction:(WhatsNewItem*)item {
  const char* type = WhatsNewTypeToString(item.type);
  if (!type) {
    return;
  }

  std::string metric = base::StrCat({"WhatsNew.", type});
  base::RecordAction(base::UserMetricsAction(metric.c_str()));
  base::UmaHistogramEnumeration("IOS.WhatsNew.Shown", item.type);
}

#pragma mark - Properties

- (void)setConsumer:(id<WhatsNewMediatorConsumer>)consumer {
  _consumer = consumer;

  [self updateConsumer];
}

#pragma mark Private

// Returns a `WhatsNewItem` representing a highlighted chrome tip. By default,
// it will be the `WhatsNewType::kUseChromeByDefault` otherwise it will choose a
// random chrome tip.
- (WhatsNewItem*)whatsNewChromeTipItem {
  // Return a random chrome tip if chrome is already the default browser or if
  // What's New M116 is enabled.
  if (IsChromeLikelyDefaultBrowser() || IsWhatsNewM116Enabled()) {
    int entryIndex = arc4random_uniform(self.chromeTipEntries.count);
    return self.chromeTipEntries[entryIndex];
  }

  return self.useChromeByDefaultEntry;
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

// Update the consumer with What's New items.
- (void)updateConsumer {
  [self.consumer setWhatsNewProperties:[self whatsNewChromeTipItem]
                          featureItems:[self whatsNewFeatureItems]];
}

// Record when a user tap on learn more.
- (void)recordLearnMoreInteraction:(WhatsNewType)type {
  const char* type_str = WhatsNewTypeToString(type);
  if (!type_str) {
    return;
  }

  std::string metric =
      base::StrCat({"WhatsNew.", type_str, ".LearnMoreTapped"});
  base::RecordAction(base::UserMetricsAction(metric.c_str()));
}

@end
