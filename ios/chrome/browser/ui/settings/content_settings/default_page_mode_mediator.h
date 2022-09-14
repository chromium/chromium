// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CONTENT_SETTINGS_DEFAULT_PAGE_MODE_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CONTENT_SETTINGS_DEFAULT_PAGE_MODE_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/settings/content_settings/default_page_mode.h"
#import "ios/chrome/browser/ui/settings/content_settings/default_page_mode_table_view_controller_delegate.h"

namespace feature_engagement {
class Tracker;
}

@protocol DefaultPageModeConsumer;
class HostContentSettingsMap;

// Mediator for the screen allowing the user to choose the default mode
// (Desktop/Mobile) for loading pages.
@interface DefaultPageModeMediator
    : NSObject <DefaultPageModeTableViewControllerDelegate>

// Initializes the mediator with `settingsMaps` for storing the page mode and
// `tracker` for recording its updates.
- (instancetype)initWithSettingsMap:(HostContentSettingsMap*)settingsMap
                            tracker:(feature_engagement::Tracker*)tracker
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@property(nonatomic, weak) id<DefaultPageModeConsumer> consumer;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CONTENT_SETTINGS_DEFAULT_PAGE_MODE_MEDIATOR_H_
