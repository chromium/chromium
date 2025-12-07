// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SHORTCUTS_COORDINATOR_SHORTCUTS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SHORTCUTS_COORDINATOR_SHORTCUTS_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/content_suggestions/ui_bundled/shortcuts/ui/shortcuts_commands.h"

namespace feature_engagement {
class Tracker;
}  // namespace feature_engagement

namespace signin {
class IdentityManager;
}  // namespace signin

@protocol ApplicationCommands;
@protocol BrowserCoordinatorCommands;
@protocol ContentSuggestionsConsumer;
@class ContentSuggestionsMetricsRecorder;
@protocol NewTabPageActionsDelegate;
class ReadingListModel;
@class ShortcutsConfig;
@protocol ShortcutsMediatorDelegate;
@protocol WhatsNewCommands;

// Mediator for managing the state of the Shortcuts Magic Stack module
@interface ShortcutsMediator : NSObject <ShortcutsCommands>

// The latest config for the Shortcuts module.
@property(nonatomic, strong, readonly) ShortcutsConfig* shortcutsConfig;

// Recorder for content suggestions metrics.
@property(nonatomic, weak)
    ContentSuggestionsMetricsRecorder* contentSuggestionsMetricsRecorder;

// Consumer for this mediator.
@property(nonatomic, weak) id<ContentSuggestionsConsumer> consumer;

// Delegate used to communicate events back to the owner of this class.
@property(nonatomic, weak) id<ShortcutsMediatorDelegate> delegate;

// Delegate for reporting content suggestions actions to the NTP.
@property(nonatomic, weak) id<NewTabPageActionsDelegate> NTPActionsDelegate;

// Dispatcher.
@property(nonatomic, weak)
    id<ApplicationCommands, BrowserCoordinatorCommands, WhatsNewCommands>
        dispatcher;

// Default initializer.
- (instancetype)initWithReadingListModel:(ReadingListModel*)readingListModel
                featureEngagementTracker:(feature_engagement::Tracker*)tracker
                         identityManager:
                             (signin::IdentityManager*)identityManager
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects this mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SHORTCUTS_COORDINATOR_SHORTCUTS_MEDIATOR_H_
