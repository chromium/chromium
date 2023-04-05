// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_POPUP_DEBUG_INFO_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_POPUP_DEBUG_INFO_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/omnibox/popup/autocomplete_controller_observer_bridge.h"
#import "ios/chrome/browser/ui/omnibox/popup/popup_debug_info_consumer.h"
#import "ios/chrome/browser/ui/omnibox/popup/remote_suggestions_service_observer_bridge.h"

/// View controller used to display omnibox and popup related debug info.
@interface PopupDebugInfoViewController
    : UIViewController <PopupDebugInfoConsumer,
                        RemoteSuggestionsServiceObserver,
                        AutocompleteControllerObserver>

- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_POPUP_DEBUG_INFO_VIEW_CONTROLLER_H_
