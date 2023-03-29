// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/whats_new/whats_new_mediator.h"

#import "base/metrics/user_metrics.h"
#import "base/notreached.h"
#import "ios/chrome/browser/default_browser/utils.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/ui/whats_new/data_source/whats_new_data_source.h"
#import "ios/chrome/browser/ui/whats_new/feature_flags.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_mediator_consumer.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/public/provider/chrome/browser/password_auto_fill/password_auto_fill_api.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The highlighted feature type.
WhatsNewType kHighlightedFeature = WhatsNewType::kSearchTabs;
}  // namespace

@interface WhatsNewMediator ()

@property(nonatomic, strong) WhatsNewItem* highlightedFeatureEntry;
@property(nonatomic, strong) NSMutableArray<WhatsNewItem*>* featureShortEntries;
@property(nonatomic, strong) NSMutableArray<WhatsNewItem*>* chromeTipEntries;
@property(nonatomic, strong) WhatsNewItem* useChromeByDefaultEntry;

@end

// The mediator to display What's New data.
@implementation WhatsNewMediator

#pragma mark - Public

- (instancetype)init {
  self = [super init];
  if (self) {
    // Serialize What's New Features
    self.featureShortEntries = [[NSMutableArray alloc] init];
    for (WhatsNewItem* item in WhatsNewFeatureEntries(WhatsNewFilePath())) {
      // Save the highlighted feature entry separately.
      if (item.type == kHighlightedFeature) {
        self.highlightedFeatureEntry = item;
        continue;
      }
      [self.featureShortEntries addObject:item];
    }

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

- (void)didTapActionButton:(WhatsNewType)type {
  switch (type) {
    case WhatsNewType::kAddPasswordManually:
      base::RecordAction(base::UserMetricsAction(
          "WhatsNew.AddPasswordManually.PrimaryActionTapped"));
      [self.handler showSettingsFromViewController:self.baseViewController];
      break;
    case WhatsNewType::kUseChromeByDefault:
      base::RecordAction(base::UserMetricsAction(
          "WhatsNew.UseChromeByDefault.PrimaryActionTapped"));
      [self openSettingsURLString];
      break;
    case WhatsNewType::kPasswordsInOtherApps:
      base::RecordAction(base::UserMetricsAction(
          "WhatsNew.PasswordsInOtherApps.PrimaryActionTapped"));
      ios::provider::PasswordsInOtherAppsOpensSettings();
      break;
    default:
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

#pragma mark - WhatsNewTableViewActionHandler

- (void)recordWhatsNewInteraction:(WhatsNewItem*)item {
  switch (item.type) {
    case WhatsNewType::kSearchTabs:
      base::RecordAction(base::UserMetricsAction("WhatsNew.SearchTabs"));
      break;
    case WhatsNewType::kNewOverflowMenu:
      base::RecordAction(base::UserMetricsAction("WhatsNew.NewOverflowMenu"));
      break;
    case WhatsNewType::kSharedHighlighting:
      base::RecordAction(
          base::UserMetricsAction("WhatsNew.SharedHighlighting"));
      break;
    case WhatsNewType::kAddPasswordManually:
      base::RecordAction(
          base::UserMetricsAction("WhatsNew.AddPasswordManually"));
      break;
    case WhatsNewType::kUseChromeByDefault:
      base::RecordAction(
          base::UserMetricsAction("WhatsNew.UseChromeByDefault"));
      break;
    case WhatsNewType::kPasswordsInOtherApps:
      base::RecordAction(
          base::UserMetricsAction("WhatsNew.PasswordsInOtherApps"));
      break;
    case WhatsNewType::kAutofill:
      base::RecordAction(base::UserMetricsAction("WhatsNew.Autofill"));
      break;
    default:
      NOTREACHED();
      break;
  };
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
  // Return a random chrome tip if chrome is already the default browser.
  if (IsChromeLikelyDefaultBrowser()) {
    int entryIndex = arc4random_uniform(self.chromeTipEntries.count);
    return self.chromeTipEntries[entryIndex];
  }

  return self.useChromeByDefaultEntry;
}

// Returns an Array of `WhatsNewItem` features.
- (NSArray<WhatsNewItem*>*)whatsNewFeatureItems {
  if (IsWhatsNewModuleBasedLayout()) {
    return self.featureShortEntries;
  }

  return WhatsNewFeatureEntries(WhatsNewFilePath());
}

// Returns a `WhatsNewItem` representing the highlighted feature.
- (WhatsNewItem*)whatsNewHighlightedFeatureItem {
  return self.highlightedFeatureEntry;
}

// Called to allow the user to go to Chrome's settings.
- (void)openSettingsURLString {
  [[UIApplication sharedApplication]
                openURL:[NSURL URLWithString:UIApplicationOpenSettingsURLString]
                options:{}
      completionHandler:nil];
}

// Update the consumer with What's New items and whether to display them as
// module or cell based.
- (void)updateConsumer {
  [self.consumer setWhatsNewProperties:[self whatsNewHighlightedFeatureItem]
                             chromeTip:[self whatsNewChromeTipItem]
                          featureItems:[self whatsNewFeatureItems]
                         isModuleBased:IsWhatsNewModuleBasedLayout()];
}

// Record when a user tap on learn more.
- (void)recordLearnMoreInteraction:(WhatsNewType)type {
  switch (type) {
    case WhatsNewType::kSearchTabs:
      base::RecordAction(
          base::UserMetricsAction("WhatsNew.SearchTabs.LearnMoreTapped"));
      break;
    case WhatsNewType::kSharedHighlighting:
      base::RecordAction(base::UserMetricsAction(
          "WhatsNew.SharedHighlighting.LearnMoreTapped"));
      break;
    case WhatsNewType::kAddPasswordManually:
      base::RecordAction(base::UserMetricsAction(
          "WhatsNew.AddPasswordManually.LearnMoreTapped"));
      break;
    case WhatsNewType::kUseChromeByDefault:
      base::RecordAction(base::UserMetricsAction(
          "WhatsNew.UseChromeByDefault.LearnMoreTapped"));
      break;
    case WhatsNewType::kPasswordsInOtherApps:
      base::RecordAction(base::UserMetricsAction(
          "WhatsNew.PasswordsInOtherApps.LearnMoreTapped"));
      break;
    case WhatsNewType::kAutofill:
      base::RecordAction(
          base::UserMetricsAction("WhatsNew.Autofill.LearnMoreTapped"));
      break;
    case WhatsNewType::kNewOverflowMenu:
    default:
      NOTREACHED();
      break;
  };
}

@end
