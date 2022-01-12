// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/toolbar/adaptive_toolbar_menus_provider.h"

namespace web {
class WebState;
}
@protocol ApplicationCommands;
@protocol BrowserCommands;
@protocol LoadQueryCommands;
class OverlayPresenter;
class PrefService;
class TemplateURLService;
@protocol ToolbarConsumer;
class UrlLoadingBrowserAgent;
class WebStateList;

// A mediator object that provides the relevant properties of a web state
// to a consumer.
@interface ToolbarMediator : NSObject <AdaptiveToolbarMenusProvider>

// Whether the search icon should be in dark mode or not.
@property(nonatomic, assign, getter=isIncognito) BOOL incognito;

// The WebStateList that this mediator listens for any changes on the total
// number of Webstates.
@property(nonatomic, assign) WebStateList* webStateList;

// Command handler.
@property(nonatomic, weak)
    id<ApplicationCommands, BrowserCommands, LoadQueryCommands>
        commandHandler;

// The consumer for this object. This can change during the lifetime of this
// object and may be nil.
@property(nonatomic, strong) id<ToolbarConsumer> consumer;

// The overlay presenter for OverlayModality::kWebContentArea.  This mediator
// listens for overlay presentation events to determine whether the share button
// should be enabled.
@property(nonatomic, assign) OverlayPresenter* webContentAreaOverlayPresenter;

// Pref service to retrieve preference values.
@property(nonatomic, assign) PrefService* prefService;

// The template url service to use for checking whether search by image is
// available.
@property(nonatomic, assign) TemplateURLService* templateURLService;

// The URL loading service, used to load the reverse image search.
@property(nonatomic, assign) UrlLoadingBrowserAgent* URLLoadingBrowserAgent;

// Updates the consumer to conforms to |webState|.
- (void)updateConsumerForWebState:(web::WebState*)webState;

// Stops observing all objects.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_MEDIATOR_H_
