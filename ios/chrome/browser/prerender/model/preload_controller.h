// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PRERENDER_MODEL_PRELOAD_CONTROLLER_H_
#define IOS_CHROME_BROWSER_PRERENDER_MODEL_PRELOAD_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import <memory>

#import "components/prefs/pref_change_registrar.h"
#import "ios/chrome/browser/net/model/connection_type_observer_bridge.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"
#import "ios/web/public/navigation/referrer.h"
#import "ios/web/public/web_state_delegate_bridge.h"
#import "ui/base/page_transition_types.h"
#import "url/gurl.h"

@protocol PreloadControllerDelegate;

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
- (instancetype)initWithProfile:(ProfileIOS*)profile;

// Called when the profile this object was initialized with is being
// destroyed.
- (void)profileDestroyed;

// Prerenders the given `url` with the given `transition`.  Normally, prerender
// requests are fulfilled after a short delay, to prevent unnecessary prerenders
// while the user is typing.  If `immediately` is YES, this method starts
// prerendering immediately, with no delay. `currentWebState` is used to create
// a new WebState for the prerender with the same session. `immediately` should
// be set to YES only when there is a very high confidence that the user will
// navigate to the given `url`.
//
// If there is already an existing request for `url`, this method does nothing
// and does not reset the delay timer.  If there is an existing request for a
// different URL, this method cancels that request and queues this request
// instead.
- (void)prerenderURL:(const GURL&)url
            referrer:(const web::Referrer&)referrer
          transition:(ui::PageTransition)transition
     currentWebState:(web::WebState*)currentWebState
         immediately:(BOOL)immediately;

// Cancels any outstanding prerender requests and destroys any prerendered Tabs.
- (void)cancelPrerender;

// Returns whether `webState` is the WebState used for pre-rendering.
- (BOOL)isWebStatePrerendered:(web::WebState*)webState;

// Returns the currently prerendered WebState, or nil if none exists.  After
// this method is called, the PrerenderController reverts to a non-prerendering
// state.
- (std::unique_ptr<web::WebState>)releasePrerenderContents;

@end

#endif  // IOS_CHROME_BROWSER_PRERENDER_MODEL_PRELOAD_CONTROLLER_H_
