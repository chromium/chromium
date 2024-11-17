// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_TOOLBAR_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_TOOLBAR_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/toolbar/adaptive_toolbar_menus_provider.h"

namespace web {
class WebState;
}
@class BrowserActionFactory;
class OverlayPresenter;
class TemplateURLService;
@protocol ToolbarConsumer;
class WebNavigationBrowserAgent;
class WebStateList;

/// A mediator object that provides the relevant properties of a web state
/// to a consumer.
@interface AdaptiveToolbarMediator : NSObject <AdaptiveToolbarMenusProvider>

/// Whether the search icon should be in dark mode or not.
@property(nonatomic, assign, getter=isIncognito) BOOL incognito;

/// The WebStateList that this mediator listens for any changes on the total
/// number of Webstates.
@property(nonatomic, assign) WebStateList* webStateList;

/// The consumer for this object. This can change during the lifetime of this
/// object and may be nil.
@property(nonatomic, strong) id<ToolbarConsumer> consumer;

/// The overlay presenter for OverlayModality::kWebContentArea.  This mediator
/// listens for overlay presentation events to determine whether the share
/// button should be enabled.
@property(nonatomic, assign) OverlayPresenter* webContentAreaOverlayPresenter;

/// The template url service to use for checking whether search by image is
/// available.
@property(nonatomic, assign) TemplateURLService* templateURLService;

/// Action factory.
@property(nonatomic, strong) BrowserActionFactory* actionFactory;

/// Helper for Web navigation.
@property(nonatomic, assign) WebNavigationBrowserAgent* navigationBrowserAgent;

/// Updates the consumer to conforms to `webState`.
- (void)updateConsumerForWebState:(web::WebState*)webState;
/// Stops observing all objects.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_TOOLBAR_MEDIATOR_H_
