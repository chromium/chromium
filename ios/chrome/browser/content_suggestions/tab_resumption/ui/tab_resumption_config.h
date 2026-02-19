// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_TAB_RESUMPTION_UI_TAB_RESUMPTION_CONFIG_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_TAB_RESUMPTION_UI_TAB_RESUMPTION_CONFIG_H_

#import <UIKit/UIKit.h>

#import <string>

#import "components/segmentation_platform/public/trigger.h"
#import "ios/chrome/browser/content_suggestions/magic_stack/ui/magic_stack_module.h"
#import "ios/web/public/web_state.h"

namespace base {
class Time;
}  // namespace base

class GURL;
@protocol TabResumptionCommands;
@class ShopCardData;

// Tab resumption item types.
enum TabResumptionItemType {
  // Last tab synced from another devices.
  kLastSyncedTab,
  // Most recent opened tab on the current device.
  kMostRecentTab,
};

// Item used to display the tab resumption tile.
@interface TabResumptionConfig : MagicStackModule

// The updates to properties must be reflected in the copy method.
// LINT.IfChange(Copy)
// The type of the tab.
@property(nonatomic, readonly) TabResumptionItemType itemType;

// The name of the session to which the tab belongs.
@property(nonatomic, copy) NSString* sessionName;

// A weak pointer to the web state if the tab is local.
@property(nonatomic, assign) base::WeakPtr<web::WebState> localWebState;

// The title of the tab.
@property(nonatomic, copy) NSString* tabTitle;

// The reason the tab was displayed in Tab Resumption, if any.
@property(nonatomic, copy) NSString* reason;

// The URL of the tab.
@property(nonatomic, assign) const GURL& tabURL;

// The time when the tab was synced.
@property(nonatomic, assign) base::Time syncedTime;

// The favicon image of the tab if any.
@property(nonatomic, strong) UIImage* faviconImage;

// The image representing the content of the tab if any.
@property(nonatomic, strong) UIImage* contentImage;

// Command handler for user actions.
@property(nonatomic, weak) id<TabResumptionCommands> commandHandler;

// The URL key used to log metrics when displaying or activating the item.
@property(nonatomic, assign) const std::string& URLKey;

// An ID used to collect metrics associated with the triggering visit for model
// training purposes.
@property(nonatomic, assign) segmentation_platform::TrainingRequestId requestID;

// ShopCard related information to render the ShopCard variants of
// tab-resumption.
@property(nonatomic, strong) ShopCardData* shopCardData;
// LINT.ThenChange(tab_resumption_config.mm:Copy)

// The Item's designated initializer.
- (instancetype)initWithItemType:(TabResumptionItemType)itemType
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Replaces all `self` properties by `config` ones.
- (void)reconfigureWithConfig:(TabResumptionConfig*)config;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_TAB_RESUMPTION_UI_TAB_RESUMPTION_CONFIG_H_
