// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_UI_CRW_WEB_CONTROLLER_H_
#define IOS_WEB_WEB_STATE_UI_CRW_WEB_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "base/values.h"
#import "ios/web/web_state/ui/crw_touch_tracking_recognizer.h"
#import "ios/web/web_state/ui/crw_web_view_navigation_proxy.h"

namespace web {

enum class NavigationInitiationType;
enum Permission : NSUInteger;
enum PermissionState : NSUInteger;
enum class WKNavigationState;

}  // namespace web

@protocol CRWScrollableContent;
@class CRWWebViewContentView;
@protocol CRWFindInteraction;
@protocol CRWWebViewDownload;
@protocol CRWWebViewDownloadDelegate;
@protocol CRWWebViewProxy;
class GURL;
@class WKWebView;

namespace web {
class NavigationItem;
class NavigationItemImpl;
class WebState;
class WebStateImpl;
}

// Manages a view that can be used either for rendering web content in a web
// view. CRWWebController also transparently evicts and restores the internal
// web view based on memory pressure, and manages access to interact with the
// web view.
// This is an abstract class which must not be instantiated directly.
// TODO(stuartmorgan): Move all of the navigation APIs out of this class.
@interface CRWWebController : NSObject <CRWTouchTrackingDelegate>

// Whether or not a UIWebView is allowed to exist in this CRWWebController.
// Defaults to NO; this should be enabled before attempting to access the view.
@property(nonatomic, assign) BOOL webUsageEnabled;

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

// Whether or not the user is currently interacting with the web content
// presented by this controller.
@property(nonatomic, readonly, assign, getter=isUserInteracting)
    BOOL userInteracting;

// Whether the WebController is visible. Returns YES after wasShown call and
// NO after wasHidden() call.
@property(nonatomic, assign, getter=isVisible) BOOL visible;

// A Boolean value indicating whether horizontal swipe gestures will trigger
// back-forward list navigations.
@property(nonatomic) BOOL allowsBackForwardNavigationGestures;

// Whether the WebController should attempt to keep the render process alive.
@property(nonatomic, assign, getter=shouldKeepRenderProcessAlive)
    BOOL keepsRenderProcessAlive;

// Whether or not the web page is in fullscreen mode.
@property(nonatomic, readonly, getter=isWebPageInFullscreenMode)
    BOOL webPageInFullscreenMode;

// Designated initializer. Initializes web controller with `webState`. The
// calling code must retain the ownership of `webState`.
- (instancetype)initWithWebState:(web::WebStateImpl*)webState;

// Returns the latest navigation item created for new navigation, which is
// stored in navigation context.
- (web::NavigationItemImpl*)lastPendingItemForNewNavigation;

// Removes the back WebView. DANGER: this method is exposed for the sole purpose
// of allowing NavigationManagerImpl to reset the back-forward history. Please
// reconsider before using this method.
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
- (BOOL)contentIsHTML;

// Returns the CRWWebController's view of the current URL. During navigations,
// this may not be the same as the navigation manager's view of the current URL.
- (GURL)currentURL;

// Reloads web view. `isRendererInitiated` is YES for renderer-initiated
// navigation. `isRendererInitiated` is NO for browser-initiated navigation.
- (void)reloadWithRendererInitiatedNavigation:(BOOL)isRendererInitiated;

// Loads the URL indicated by current session state.
- (void)loadCurrentURLWithRendererInitiatedNavigation:(BOOL)rendererInitiated;

// Loads the URL indicated by current session state if the current page has not
// loaded yet. This method should never be called directly. Use
// NavigationManager::LoadIfNecessary() instead.
- (void)loadCurrentURLIfNecessary;

// Loads `data` of type `MIMEType` and replaces last committed URL with the
// given `URL`.
// If a load is in progress, it will be stopped before the data is loaded.
- (void)loadData:(NSData*)data
        MIMEType:(NSString*)MIMEType
          forURL:(const GURL&)URL;

// Loads the web content from the HTML you provide as if the HTML were the
// response to the request. This method does not create a new navigation entry
// if `URL` matches the current page's URL. This method creates a new navigation
// entry if `URL` differs from the current page's URL.
- (void)loadSimulatedRequest:(const GURL&)URL
          responseHTMLString:(NSString*)responseHTMLString;

// Loads the web content from the data you provide as if the data were the
// response to the request. This method does not create a new navigation entry
// if `URL` matches the current page's URL. This method creates a new navigation
// entry if `URL` differs from the current page's URL.
- (void)loadSimulatedRequest:(const GURL&)URL
                responseData:(NSData*)responseData
                    MIMEType:(NSString*)MIMEType;

// Stops loading the page.
- (void)stopLoading;

// Records the state (scroll position, form values, whatever can be harvested)
// from the current page into the current session entry.
- (void)recordStateInHistory;

// Notifies the CRWWebController that it has been shown.
- (void)wasShown;

// Notifies the CRWWebController that it has been hidden.
- (void)wasHidden;

// Instructs WKWebView to navigate to the given navigation item. `wk_item` and
// `item` must point to the same navigation item. Calling this method may
// result in an iframe navigation.
- (void)goToBackForwardListItem:(WKBackForwardListItem*)item
                 navigationItem:(web::NavigationItem*)item
       navigationInitiationType:(web::NavigationInitiationType)type
                 hasUserGesture:(BOOL)hasUserGesture;

// Takes snapshot of web view with `rect`. `rect` should be in self.view's
// coordinate system.  `completion` is always called, but `snapshot` may be nil.
// Prior to iOS 11, `completion` is called with a nil
// snapshot. `completion` may be called more than once.
- (void)takeSnapshotWithRect:(CGRect)rect
                  completion:(void (^)(UIImage* snapshot))completion;

// Creates PDF representation of the web page and invokes the `completion` with
// the NSData of the PDF or nil if a PDF couldn't be generated.
- (void)createFullPagePDFWithCompletion:
    (void (^)(NSData* PDFDocumentData))completion;

// Tries to dismiss the presented states of the media (fullscreen or Picture in
// Picture).
- (void)closeMediaPresentations;

// Creates a web view if it's not yet created. Returns the web view.
- (WKWebView*)ensureWebViewCreated;

// Removes the webView from the view hierarchy. The `shutdown` parameter
// indicates if this method was called in a shutdown context.
- (void)removeWebViewFromViewHierarchyForShutdown:(BOOL)shutdown;
// Adds the webView back in the view hierarchy.
- (void)addWebViewToViewHierarchy;

// Injects an opaque NSData block into a WKWebView to restore or serialize. Only
// supported on iOS15+. On earlier iOS versions, `setSessionStateData` is
// a no-op, and `sessionStateData` will return nil.
- (BOOL)setSessionStateData:(NSData*)data;
- (NSData*)sessionStateData;

// Gets and sets the web state's state of a permission; for example, the one to
// use the camera on the device. Only works on iOS 15+.
- (web::PermissionState)stateForPermission:(web::Permission)permission;
- (void)setState:(web::PermissionState)state
    forPermission:(web::Permission)permission;

// Gets a mapping of all permissions and their states. Only works on iOS 15+.
- (NSDictionary<NSNumber*, NSNumber*>*)statesForAllPermissions;

// Downloads the current page as a file at `destination` path.
// `completion_handler` is used to retrieve the created CRWWebViewDownload, so
// the caller can manage the launched download.
- (void)downloadCurrentPageToDestinationPath:(NSString*)destination
                                    delegate:
                                        (id<CRWWebViewDownloadDelegate>)delegate
                                     handler:(void (^)(id<CRWWebViewDownload>))
                                                 handler;

// Returns whether the Find interaction is supported and can be enabled.
- (BOOL)findInteractionSupported;

// Returns whether the Find interaction is enabled on the contained web view, if
// any.
- (BOOL)findInteractionEnabled;

// Sets the value of `findInteractionEnabled` to `enabled` on the contained web
// view, if any.
- (void)setFindInteractionEnabled:(BOOL)enabled;

// Returns the Find interaction of the contained web view, if any.
- (id<CRWFindInteraction>)findInteraction API_AVAILABLE(ios(16));

// Returns an opaque activity item that can be passed to a
// UIActivityViewController to add additional share action for the current URL.
- (id)activityItem;

// Returns the page theme color.
- (UIColor*)themeColor;

// Returns the under page background color.
- (UIColor*)underPageBackgroundColor;

#pragma mark Fullscreen Message Handlers

// Handles the viewport fit value, `isCover` is true when the "viewport-fit" is
// equal to "cover".
- (void)handleViewportFit:(BOOL)isCover;

#pragma mark Navigation Message Handlers

// Handles a navigation hash change message for the current webpage.
- (void)handleNavigationHashChange;

// Handles a navigation will change message for the current webpage.
- (void)handleNavigationWillChangeState;

// Handles a navigation did push state message for the current webpage.
- (void)handleNavigationDidPushStateMessage:(base::Value::Dict*)dict;

// Handles a navigation did replace state message for the current webpage.
- (void)handleNavigationDidReplaceStateMessage:(base::Value::Dict*)dict;

// Retrieves the existing web frames in `contentWorld`.
- (void)retrieveExistingFramesInContentWorld:(WKContentWorld*)contentWorld;

// Do not call this function directly, instead use
// WebState::ExecuteUserJavaScript.
- (void)executeUserJavaScript:(NSString*)javascript
            completionHandler:(void (^)(id result, NSError* error))completion;

@end

#pragma mark Testing

@interface CRWWebController (UsedOnlyForTesting)  // Testing or internal API.

@property(nonatomic, readonly) web::WebState* webState;
@property(nonatomic, readonly) web::WebStateImpl* webStateImpl;
// Returns the current page loading phase.
// TODO(crbug.com/40624624): Remove this once refactor is done.
@property(nonatomic, readonly, assign) web::WKNavigationState navigationState;
// YES if the web container view fill the screen.
@property(nonatomic, readonly) BOOL isCover;

// Injects a CRWWebViewContentView for testing.  Takes ownership of
// `webViewContentView`.
- (void)injectWebViewContentView:(CRWWebViewContentView*)webViewContentView;
- (void)resetInjectedWebViewContentView;

// Returns whether any observers are registered with the CRWWebController.
- (BOOL)hasObservers;

// Loads the HTML into the page at the given URL.
- (void)loadHTML:(NSString*)HTML forURL:(const GURL&)URL;

// Executes `javascript` in the current page.
// Prefer `WebFrame::CallJavaScriptFunction` if possible, otherwise
// use `WebState::ExecuteJavaScript`.
- (void)executeJavaScript:(NSString*)javascript
        completionHandler:(void (^)(id result, NSError* error))completion;

@end

#endif  // IOS_WEB_WEB_STATE_UI_CRW_WEB_CONTROLLER_H_
