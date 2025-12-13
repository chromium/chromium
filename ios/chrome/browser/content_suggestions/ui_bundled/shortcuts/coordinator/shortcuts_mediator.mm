// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui_bundled/shortcuts/coordinator/shortcuts_mediator.h"

#import "base/apple/foundation_util.h"
#import "base/ios/crb_protocol_observers.h"
#import "base/memory/raw_ptr.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/reading_list/core/reading_list_model.h"
#import "components/reading_list/ios/reading_list_model_bridge_observer.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_constants.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_consumer.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_metrics_recorder.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/shortcuts/coordinator/shortcuts_mediator_delegate.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/shortcuts/ui/shortcuts_action_item.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/shortcuts/ui/shortcuts_commands.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/shortcuts/ui/shortcuts_config.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/shortcuts/ui/shortcuts_consumer.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/shortcuts/ui/shortcuts_consumer_source.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/shortcuts/ui/shortcuts_tile_view.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_actions_delegate.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/whats_new_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/whats_new/coordinator/whats_new_util.h"

@interface ShortcutsConsumerList : CRBProtocolObservers <ShortcutsConsumer>
@end

@implementation ShortcutsConsumerList
@end

@interface ShortcutsMediator () <ReadingListModelBridgeObserver,
                                 ShortcutsConsumerSource>
@end

@implementation ShortcutsMediator {
  std::unique_ptr<ReadingListModelBridge> _readingListModelBridge;
  // Item for the reading list action item.  Reference is used to update the
  // reading list count.
  ShortcutsActionItem* _readingListItem;
  // Indicates if reading list model is loaded. Readlist cannot be triggered
  // until it is.
  BOOL _readingListModelIsLoaded;
  // Number of unread items in reading list model.
  NSInteger _readingListUnreadCount;
  //  ShortcutsConfig* _shortcutsConfig;
  raw_ptr<feature_engagement::Tracker> _tracker;
  raw_ptr<signin::IdentityManager> _identityManager;
  ShortcutsConsumerList* _consumers;
}

- (instancetype)initWithReadingListModel:(ReadingListModel*)readingListModel
                featureEngagementTracker:(feature_engagement::Tracker*)tracker
                         identityManager:
                             (signin::IdentityManager*)identityManager {
  self = [super init];
  if (self) {
    _readingListModelBridge =
        std::make_unique<ReadingListModelBridge>(self, readingListModel);
    _tracker = tracker;
    _identityManager = identityManager;

    _shortcutsConfig = [[ShortcutsConfig alloc] init];
    _shortcutsConfig.shortcutItems = [self shortcutItems];
    _shortcutsConfig.consumerSource = self;
    _shortcutsConfig.commandHandler = self;
    _consumers = [ShortcutsConsumerList
        observersWithProtocol:@protocol(ShortcutsConsumer)];
  }
  return self;
}

- (void)disconnect {
  _readingListModelBridge.reset();
  _tracker = nil;
  _identityManager = nil;
}

- (NSArray<ShortcutsActionItem*>*)shortcutItems {
  _readingListItem = [[ShortcutsActionItem alloc]
      initWithCollectionShortcutType:NTPCollectionShortcutTypeReadingList];
  _readingListItem.count = _readingListUnreadCount;
  _readingListItem.disabled = !_readingListModelIsLoaded;
  NSArray<ShortcutsActionItem*>* shortcuts = @[
    [self shouldShowWhatsNewActionItem]
        ? [[ShortcutsActionItem alloc]
              initWithCollectionShortcutType:NTPCollectionShortcutTypeWhatsNew]
        : [[ShortcutsActionItem alloc]
              initWithCollectionShortcutType:NTPCollectionShortcutTypeBookmark],
    _readingListItem,
    [[ShortcutsActionItem alloc]
        initWithCollectionShortcutType:NTPCollectionShortcutTypeRecentTabs],
    [[ShortcutsActionItem alloc]
        initWithCollectionShortcutType:NTPCollectionShortcutTypeHistory]
  ];
  return shortcuts;
}

#pragma mark - ShortcutsConsumerSource

- (void)addConsumer:(id<ShortcutsConsumer>)consumer {
  [_consumers addObserver:consumer];
}

#pragma mark - ReadingListModelBridgeObserver

- (void)readingListModelLoaded:(const ReadingListModel*)model {
  [self readingListModelDidApplyChanges:model];
}

#pragma mark - ShortcutsCommands

- (void)shortcutsTapped:(UIGestureRecognizer*)sender {
  ContentSuggestionsShortcutTileView* shortcutView =
      static_cast<ContentSuggestionsShortcutTileView*>(sender.view);

  ShortcutsActionItem* shortcutsItem =
      base::apple::ObjCCastStrict<ShortcutsActionItem>(shortcutView.config);
  if (shortcutsItem.disabled) {
    return;
  }
  [self.NTPActionsDelegate shortcutTileOpened];
  [self.delegate
      logMagicStackEngagementForType:ContentSuggestionsModuleType::kShortcuts];
  [self.contentSuggestionsMetricsRecorder
      recordShortcutTileTapped:shortcutsItem.collectionShortcutType];
  switch (shortcutsItem.collectionShortcutType) {
    case NTPCollectionShortcutTypeBookmark:
      [self.dispatcher showBookmarksManager];
      break;
    case NTPCollectionShortcutTypeReadingList:
      [self.dispatcher showReadingList];
      break;
    case NTPCollectionShortcutTypeRecentTabs:
      [self.dispatcher showRecentTabs];
      break;
    case NTPCollectionShortcutTypeHistory:
      [self.dispatcher showHistory];
      break;
    case NTPCollectionShortcutTypeWhatsNew:
      [self.dispatcher showWhatsNew];
      break;
  }
  return;
}

#pragma mark - Private

// Updates the config with the latest state of the ReadingListModel.
- (void)readingListModelDidApplyChanges:(const ReadingListModel*)model {
  _readingListUnreadCount = model->unread_size();
  _readingListModelIsLoaded = model->loaded();
  if (_readingListItem) {
    _shortcutsConfig.shortcutItems = [self shortcutItems];
    [_consumers shortcutsItemConfigDidChange:_readingListItem];
  }
}

// YES if the "What's New" tile should be shown in the Shortcuts module.
- (BOOL)shouldShowWhatsNewActionItem {
  if (WasWhatsNewUsed()) {
    return NO;
  }

  // TODO(crbug.com/41483080): The FET is not ready upon app launch in the NTP.
  // Consequently, we must load a URL first and then load the NTP where the FET
  // becomes ready.
  DCHECK(_tracker);
  if (!_tracker->WouldTriggerHelpUI(
          feature_engagement::kIPHWhatsNewUpdatedFeature)) {
    return NO;
  }

  BOOL isSignedIn =
      _identityManager->HasPrimaryAccount(signin::ConsentLevel::kSignin);

  return !isSignedIn;
}

@end
