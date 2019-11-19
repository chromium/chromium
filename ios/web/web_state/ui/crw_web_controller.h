// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_UI_CRW_WEB_CONTROLLER_H_
#define IOS_WEB_WEB_STATE_UI_CRW_WEB_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/web/navigation/crw_session_controller.h"
#include "ios/web/public/deprecated/url_verification_constants.h"
#import "ios/web/web_state/ui/crw_touch_tracking_recognizer.h"
#import "ios/web/web_state/ui/crw_web_view_navigation_proxy.h"

namespace web {

enum class NavigationInitiationType;
enum class WKNavigationState;

}  // namespace web

@class CRWJSInjector;
@protocol CRWNativeContentHolder;
@protocol CRWScrollableContent;
@protocol CRWSwipeRecognizerProvider;
@class CRWWebViewContentView;
@protocol CRWWebViewProxy;
class GURL;
@class WKWebView;

namespace web {
class NavigationItem;
class WebState;
class WebStateImpl;
}

// Manages a view that can be used either for rendering web content in a web
// view, or native content in a view provided by a NativeContentProvider.
// CRWWebController also transparently evicts and restores the internal web
// view based on memory pressure, and manages access to interact with the
// web view.
// This is an abstract class which must not be instantiated directly.
// TODO(stuartmorgan): Move all of the navigation APIs out of this class.
@interface CRWWebController
    : NSObject <CRWSessionControllerDelegate, CRWTouchTrackingDelegate>

// Whether or not a UIWebView is allowed to exist in this CRWWebController.
// Defaults to NO; this should be enabled before attempting to access the view.
@property(nonatomic, assign) BOOL webUsageEnabled;

@property(nonatomic, weak) id<CRWSwipeRecognizerProvider>
    swipeRecognizerProvider;

// The container view used to display content.  If the view has been purged due
// to low memory, this will recreate it.
@property(weak, nonatomic, readonly) UIView* view;

// The web view proxy associated with this controller.
@property(strong, nonatomic, readonly) id<CRWWebViewProxy> webViewProxy;

// The web view navigation proxy associated with this controller.
@property(weak, nonatomic, readonly) id<CRWWebViewNavigationProxy>
    webViewNavigationProxy;

// The fraction of the page load that has completed as a number between 0.0
// (nothing loaded) and 1.0 (fully loaded).
@property(nonatomic, readonly) double loadingProgress;

// YES if the web process backing WebView is believed to currently be crashed.
@property(nonatomic, readonly, assign, getter=isWebProcessCrashed)
    BOOL webProcessCrashed;

// Whether the WebController is visible. Returns YES after wasShown call and
// NO after wasHidden() call.
@property(nonatomic, assign, getter=isVisible) BOOL visible;

// A Boolean value indicating whether horizontal swipe gestures will trigger
// back-forward list navigations.
@property(nonatomic) BOOL allowsBackForwardNavigationGestures;

// JavaScript injector.
@property(nonatomic, strong, readonly) CRWJSInjector* jsInjector;

// Whether the WebController should attempt to keep the render process alive.
@property(nonatomic, assign, getter=shouldKeepRenderProcessAlive)
    BOOL keepsRenderProcessAlive;

// Designated initializer. Initializes web controller with |webState|. The
// calling code must retain the ownership of |webState|.
- (instancetype)initWithWebState:(web::WebStateImpl*)webState;

// Returns the latest navigation item created for new navigation, which is
// stored in navigation context.
- (web::NavigationItemImpl*)lastPendingItemForNewNavigation;

// Replaces the currently displayed content with |contentView|.  The content
// view will be dismissed for the next navigation.
- (void)showTransientContentView:(UIView<CRWScrollableContent>*)contentView;

// Clear the transient content view, if one is shown. This is a delegate
// method for WebStateImpl::ClearTransientContent(). Callers should use the
// WebStateImpl API instead of calling this method directly.
- (void)clearTransientContentView;

// Removes the back WebView. DANGER: this method is exposed for the sole purpose
// of allowing WKBasedNavigationManagerImpl to reset the back-forward history.
// Please reconsider before using this method.
- (void)removeWebView;

// Call when the CRWWebController needs go away. Caller must reset the delegate
// before calling.
- (void)close;

// Returns YES if there is currently a live view in the tab (e.g., the view
// hasn't been discarded due to low memory).
// NOTE: This should be used for metrics-gathering only; for any other purpose
// callers should not know or care whether the view is live.
- (BOOL)isViewAlive;

// Returns YES if the current live view is a web view with HTML.
// TODO(crbug.com/949651): Remove once JSFindInPageManager is removed.
- (BOOL)contentIsHTML;

// Returns the CRWWebController's view of the current URL. Moreover, this method
// will set the trustLevel enum to the appropriate level from a security point
// of view. The caller has to handle the case where |trustLevel| is not
// appropriate, as this method won't display any error to the user.
- (GURL)currentURLWithTrustLevel:(web::URLVerificationTrustLevel*)trustLevel;

// Reloads web view. |isRendererInitiated| is YES for renderer-initiated
// navigation. |isRendererInitiated| is NO for browser-initiated navigation.
- (void)reloadWithRendererInitiatedNavigation:(BOOL)isRendererInitiated;

// Loads the URL indicated by current session state.
- (void)loadCurrentURLWithRendererInitiatedNavigation:(BOOL)rendererInitiated;

// Loads the URL indicated by current session state if the current page has not
// loaded yet. This method should never be called directly. Use
// NavigationManager::LoadIfNecessary() instead.
- (void)loadCurrentURLIfNecessary;

// Loads |data| of type |MIMEType| and replaces last committed URL with the
// given |URL|.
// If a load is in progress, it will be stopped before the data is loaded.
- (void)loadData:(NSData*)data
        MIMEType:(NSString*)MIMEType
          forURL:(const GURL&)URL;

// Stops loading the page.
- (void)stopLoading;

// Requires that the next load rebuild the web view. This is expensive, and
// should be used only in the case where something has changed that the web view
// only checks on creation, such that the whole object needs to be rebuilt.
- (void)requirePageReconstruction;

// Records the state (scroll position, form values, whatever can be harvested)
// from the current page into the current session entry.
- (void)recordStateInHistory;

// Notifies the CRWWebController that it has been shown.
- (void)wasShown;

// Notifies the CRWWebController that it has been hidden.
- (void)wasHidden;

// Returns the object holding the native controller (if any) currently managing
// the content.
- (id<CRWNativeContentHolder>)nativeContentHolder;

// Called when NavigationManager has completed go to index same-document
// navigation. Updates HTML5 history state, current document URL and sends
// approprivate navigation and loading WebStateObserver callbacks.
- (void)didFinishGoToIndexSameDocumentNavigationWithType:
            (web::NavigationInitiationType)type
                                          hasUserGesture:(BOOL)hasUserGesture;

// Instructs WKWebView to navigate to the given navigation item. |wk_item| and
// |item| must point to the same navigation item. Calling this method may
// result in an iframe navigation.
- (void)goToBackForwardListItem:(WKBackForwardListItem*)item
                 navigationItem:(web::NavigationItem*)item
       navigationInitiationType:(web::NavigationInitiationType)type
                 hasUserGesture:(BOOL)hasUserGesture;

// Takes snapshot of web view with |rect|. |rect| should be in self.view's
// coordinate system.  |completion| is always called, but |snapshot| may be nil.
// Prior to iOS 11, |completion| is called with a nil
// snapshot. |completion| may be called more than once.
- (void)takeSnapshotWithRect:(CGRect)rect
                  completion:(void (^)(UIImage* snapshot))completion;

// Creates a web view if it's not yet created. Returns the web view.
- (WKWebView*)ensureWebViewCreated;

@end

#pragma mark Testing

@interface CRWWebController (UsedOnlyForTesting)  // Testing or internal API.

@property(nonatomic, readonly) web::WebState* webState;
@property(nonatomic, readonly) web::WebStateImpl* webStateImpl;
// Returns the current page loading phase.
// TODO(crbug.com/956511): Remove this once refactor is done.
@property(nonatomic, readonly, assign) web::WKNavigationState navigationState;

// Injects a CRWWebViewContentView for testing.  Takes ownership of
// |webViewContentView|.
- (void)injectWebViewContentView:(CRWWebViewContentView*)webViewContentView;
- (void)resetInjectedWebViewContentView;

// Returns whether any observers are registered with the CRWWebController.
- (BOOL)hasObservers;

// Loads the HTML into the page at the given URL.
- (void)loadHTML:(NSString*)HTML forURL:(const GURL&)URL;

@end

#endif  // IOS_WEB_WEB_STATE_UI_CRW_WEB_CONTROLLER_H_
