// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_mediator.h"

#import <AuthenticationServices/AuthenticationServices.h>

#import <vector>

#import "base/apple/foundation_util.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/ios/ios_util.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feed/core/v2/public/ios/pref_names.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/strings/grit/components_strings.h"
#import "components/url_formatter/elide_url.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/ntp/shared/metrics/home_metrics.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_actions_delegate.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/most_visited_tiles_config.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/most_visited_tiles_mediator.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/shortcuts_mediator.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_delegate.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_consumer.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_ranking_model.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/placeholder_config.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_mediator.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

@interface ContentSuggestionsMediator ()
// Browser reference.
@property(nonatomic, assign) Browser* browser;

@end

@implementation ContentSuggestionsMediator {
  // Local State prefs.
  raw_ptr<PrefService> _localState;
}

#pragma mark - Public

- (instancetype)init {
  self = [super init];
  if (self) {
    _localState = GetApplicationContext()->GetLocalState();
  }

  return self;
}

+ (void)registerProfilePrefs:(user_prefs::PrefRegistrySyncable*)registry {
  registry->RegisterInt64Pref(prefs::kIosDiscoverFeedLastRefreshTime, 0);
  registry->RegisterInt64Pref(prefs::kIosDiscoverFeedLastUnseenRefreshTime, 0);
}

- (void)disconnect {
  _localState = nullptr;
}

#pragma mark - MagicStackRankingModelDelegate

- (void)magicStackRankingModel:(MagicStackRankingModel*)model
      didGetLatestRankingOrder:(NSArray<MagicStackModule*>*)rank {
  [self.magicStackConsumer populateItems:rank];
}

- (void)magicStackRankingModel:(MagicStackRankingModel*)model
                 didInsertItem:(MagicStackModule*)item
                       atIndex:(NSUInteger)index {
  [self.magicStackConsumer insertItem:item atIndex:index];
}

- (void)magicStackRankingModel:(MagicStackRankingModel*)model
                didReplaceItem:(MagicStackModule*)oldItem
                      withItem:(MagicStackModule*)item {
  [self.magicStackConsumer replaceItem:oldItem withItem:item];
}

- (void)magicStackRankingModel:(MagicStackRankingModel*)model
                 didRemoveItem:(MagicStackModule*)item {
  [self.magicStackConsumer removeItem:item];
}

- (void)magicStackRankingModel:(MagicStackRankingModel*)model
            didReconfigureItem:(MagicStackModule*)item {
  [self.magicStackConsumer reconfigureItem:item];
}

#pragma mark - Private

- (void)configureConsumer {
  if (!self.consumer) {
    return;
  }
  if (!ShouldPutMostVisitedSitesInMagicStack() &&
      self.mostVisitedTilesMediator.mostVisitedConfig) {
    [self.consumer setMostVisitedTilesConfig:self.mostVisitedTilesMediator
                                                 .mostVisitedConfig];
  }
}

- (void)setConsumer:(id<ContentSuggestionsConsumer>)consumer {
  _consumer = consumer;
  [self configureConsumer];
}

- (void)setMagicStackConsumer:(id<MagicStackConsumer>)magicStackConsumer {
  _magicStackConsumer = magicStackConsumer;
  [self.magicStackRankingModel fetchLatestMagicStackRanking];
}

@end
