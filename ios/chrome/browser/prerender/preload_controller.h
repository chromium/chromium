// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PRERENDER_PRELOAD_CONTROLLER_H_
#define IOS_CHROME_BROWSER_PRERENDER_PRELOAD_CONTROLLER_H_

#import <UIKit/UIKit.h>

#include <memory>

#include "components/prefs/pref_change_registrar.h"
#import "ios/chrome/browser/net/connection_type_observer_bridge.h"
#import "ios/web/public/deprecated/crw_native_content_provider.h"
#include "ios/web/public/navigation/referrer.h"
#import "ios/web/public/web_state_delegate_bridge.h"
#import "net/url_request/url_fetcher.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

@protocol PreloadControllerDelegate;

namespace ios {
class ChromeBrowserState;
}

namespace web {
class WebState;
}

// PreloadController owns and manages a Tab that contains a prerendered
// webpage.  This class contains methods to queue and cancel prerendering for a
// given URL as well as a method to return the prerendered Tab.
@interface PreloadController : NSObject

@property(nonatomic, weak) id<PreloadControllerDelegate> delegate;

// The URL of the currently prerendered Tab.  Empty if there is no prerendered
// Tab.
@property(nonatomic, readonly, assign) GURL prerenderedURL;

// Whether prerendering is currently enabled.
@property(nonatomic, readonly, getter=isEnabled) BOOL enabled;

// Designated initializer.
- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState;

// Called when the browser state this object was initialized with is being
// destroyed.
- (void)browserStateDestroyed;

// Prerenders the given |url| with the given |transition|.  Normally, prerender
// requests are fulfilled after a short delay, to prevent unnecessary prerenders
// while the user is typing.  If |immediately| is YES, this method starts
// prerendering immediately, with no delay.  |immediately| should be set to YES
// only when there is a very high confidence that the user will navigate to the
// given |url|.
//
// If there is already an existing request for |url|, this method does nothing
// and does not reset the delay timer.  If there is an existing request for a
// different URL, this method cancels that request and queues this request
// instead.
- (void)prerenderURL:(const GURL&)url
            referrer:(const web::Referrer&)referrer
          transition:(ui::PageTransition)transition
         immediately:(BOOL)immediately;

// Cancels any outstanding prerender requests and destroys any prerendered Tabs.
- (void)cancelPrerender;

// Returns whether |webState| is the WebState used for pre-rendering.
- (BOOL)isWebStatePrerendered:(web::WebState*)webState;

// Returns the currently prerendered WebState, or nil if none exists.  After
// this method is called, the PrerenderController reverts to a non-prerendering
// state.
- (std::unique_ptr<web::WebState>)releasePrerenderContents;

@end

#endif  // IOS_CHROME_BROWSER_PRERENDER_PRELOAD_CONTROLLER_H_
