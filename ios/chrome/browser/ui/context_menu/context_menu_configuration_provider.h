// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTEXT_MENU_CONTEXT_MENU_CONFIGURATION_PROVIDER_H_
#define IOS_CHROME_BROWSER_UI_CONTEXT_MENU_CONTEXT_MENU_CONFIGURATION_PROVIDER_H_

#import <UIKit/UIKit.h>

namespace web {
struct ContextMenuParams;
class WebState;
}

class Browser;
class GURL;

// Object creating the configuration (action items...) for the context menu.
@interface ContextMenuConfigurationProvider : NSObject

// Instantiates with a `browser`.
- (instancetype)initWithBrowser:(Browser*)browser
             baseViewController:(UIViewController*)baseViewController;

// Returns a configuration for a context menu, based on its associated
// `webState`, `params` and `baseViewController`.
// `params` is copied in order to be used in blocks.
- (UIContextMenuConfiguration*)
    contextMenuConfigurationForWebState:(web::WebState*)webState
                                 params:(web::ContextMenuParams)params;

// Stops the provider.
- (void)stop;

// The URL to be loaded when the user taps on the preview. Empty URL if there is
// nothing to load.
@property(nonatomic, assign, readonly) GURL URLToLoad;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTEXT_MENU_CONTEXT_MENU_CONFIGURATION_PROVIDER_H_
