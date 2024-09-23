// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_WEB_VIEW_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_WEB_VIEW_H_

#import <UIKit/UIKit.h>
#import <WebKit/WebKit.h>

#import "cwv_export.h"

NS_ASSUME_NONNULL_BEGIN

@class CWVAutofillController;
@class CWVBackForwardList;
@class CWVBackForwardListItem;
@class CWVFindInPageController;
@class CWVScriptCommand;
@class CWVTranslationController;
@class CWVWebViewConfiguration;
@protocol CWVNavigationDelegate;
@protocol CWVUIDelegate;
@class CWVSSLStatus;

// A web view component (like WKWebView) which uses iOS Chromium's web view
// implementation.
//
// In addition to WKWebView features, it allows Translate, Find In Page,
// Customizable Context Menus, and maybe more.
//
// Concrete instances can be created through CWV.
CWV_EXPORT
@interface CWVWebView : UIView

// The configuration of the web view.
@property(nonatomic, readonly) CWVWebViewConfiguration* configuration;

// This web view's navigation delegate.
@property(nonatomic, weak, nullable) id<CWVNavigationDelegate>
    navigationDelegate;

// This web view's translation controller.
@property(nonatomic, readonly) CWVTranslationController* translationController;

// This web view's UI delegate
@property(nonatomic, weak, nullable) id<CWVUIDelegate> UIDelegate;

// Whether or not this web view can go backwards or forwards. KVO Compliant.
@property(nonatomic, readonly) BOOL canGoBack;
@property(nonatomic, readonly) BOOL canGoForward;

// Whether or not this web view is loading a page. KVO compliant.
@property(nonatomic, readonly, getter=isLoading) BOOL loading;

// The URL displayed in the URL bar. KVO Compliant.
//
// You should use |lastCommittedURL| instead for most of purposes other than
// rendering the URL bar.
//
// |visibleURL| and |lastCommittedURL| are the same in most cases, but with
// these exceptions:
//
// - The request was made by -loadRequest: method.
//   |visibleURL| changes to the requested URL immediately when -loadRequest:
//   was called. |lastCommittedURL| changes only after the navigation is
//   committed (i.e., the server started to respond with data and the displayed
//   page has actually changed.)
//
// - It has navigated to a page with a bad SSL certificate.
//   (not implemented for CWVWebView)
//   |visibleURL| is the bad cert page URL. |lastCommittedURL| is the previous
//   page URL.
@property(nonatomic, readonly) NSURL* visibleURL;

// A human-friendly string which represents the location of the document
// currently being loaded. KVO compliant.
//
// You can display this string instead of |visibleURL| in the URL bar. This is
// usually the scheme followed by the host name, without the path e.g.,
// @"https://example.com". Precisely speaking:
//
// - Internationalized domain names (IDN) are presented in Unicode if they're
//   regarded safe. Domain names with RTL characters will still be in
//   ACE/punycode for now (crbug.com/650760). See
//   https://dev.chromium.org/developers/design-documents/idn-in-google-chrome
//   for details.
// - Omits the path for standard schemes, excepting file and filesystem.
// - Omits the port if it is the default for the scheme.
@property(nonatomic, readonly) NSString* visibleLocationString;

// The URL of the current document. KVO Compliant.
//
// See the comment of |visibleURL| above for the difference between |visibleURL|
// and |lastCommittedURL|.
@property(nonatomic, readonly, nullable) NSURL* lastCommittedURL;

// The SSL status displayed in the URL bar. KVO compliant.
// It is nil when no page is loaded on the web view.
@property(nonatomic, readonly, nullable) CWVSSLStatus* visibleSSLStatus;

// The current page title. KVO compliant.
@property(nonatomic, readonly, copy, nullable) NSString* title;

// Page loading progress from 0.0 to 1.0. KVO compliant.
//
// It is 0.0 initially before the first navigation starts. After a navigation
// completes, it remains at 1.0 until a new navigation starts, at which point it
// is reset to 0.0.
@property(nonatomic, readonly) double estimatedProgress;

// The scroll view associated with the web view.
//
// It is reset on state restoration.
@property(nonatomic, readonly) UIScrollView* scrollView;

// A Boolean value indicating whether horizontal swipe gestures will trigger
// back-forward list navigations.
@property(nonatomic) BOOL allowsBackForwardNavigationGestures;

// The web view's autofill controller.
@property(nonatomic, readonly) CWVAutofillController* autofillController;

// The web view's find in page controller.
@property(nonatomic, readonly)
    CWVFindInPageController* findInPageController API_AVAILABLE(ios(16.0));

// An equivalent of
// https://developer.apple.com/documentation/webkit/wkwebview/1414977-backforwardlist
@property(nonatomic, readonly, nonnull) CWVBackForwardList* backForwardList;

// Enables Chrome's custom logic to handle long press and force touch. Defaults
// to NO.
// This class property setting should only be changed BEFORE any
// CWVWebViewConfiguration instance is initialized.
@property(nonatomic, class) BOOL chromeContextMenuEnabled;

// Whether or not to use the new session storage. Defaults to NO.
// This class property setting should only be changed BEFORE any
// CWVWebViewConfiguration instance is initialized.
@property(nonatomic, class) BOOL useOptimizedSessionStorage;

// Whether or not to enable debugging by Safari Web Inspector.
// Defaults to NO.
@property(nonatomic, class) BOOL webInspectorEnabled;

// Normally ios/web_view/ CHECKs IsOptedInForAccountStorage() early on. Setting
// this to true will cause the CHECK to be skipped, which potentially fixes
// crbug.com/347862165.
@property(nonatomic, class) BOOL skipAccountStorageCheckEnabled;

// Set this to customize the underlying WKWebView's inputAccessoryView. Setting
// to nil means to use the WKWebView's default inputAccessoryView instead.
//
// In order to be displayed properly, this UIView must:
// - Set |translatesAutoresizingMaskIntoConstraints| to |NO|.
// - Return a non-zero CGSize in |intrinsicContentSize|.
//
// Explicitly redeclared this property to allow customization according to
// https://developer.apple.com/documentation/uikit/uiresponder/1621119-inputaccessoryview?language=objc
@property(nonatomic, strong, nullable) UIView* inputAccessoryView;

// Allows full customization of the user agent.
// Similar to -[WKWebView customUserAgent], but applies to all instances.
// If non-nil, this is used instead of |userAgentProduct|.
@property(nonatomic, class, copy, nullable) NSString* customUserAgent;

// The User Agent product string used to build the full User Agent.
// Deprecated. Use |customUserAgent| instead.
+ (NSString*)userAgentProduct;

// Customizes the User Agent string by inserting |product|. It should be of the
// format "product/1.0". For example:
// "Mozilla/5.0 (iPhone; CPU iPhone OS 10_3 like Mac OS X) AppleWebKit/603.1.30
// (KHTML, like Gecko) <product> Mobile/16D32 Safari/602.1" where <product>
// will be replaced with |product| or empty string if not set.
//
// NOTE: It is recommended to set |product| before initializing any web views.
// Setting |product| is only guaranteed to affect web views which have not yet
// been initialized. However, exisiting web views could also be affected
// depending upon their internal state.
//
// Deprecated. Use |customUserAgent| instead.
+ (void)setUserAgentProduct:(NSString*)product;

// Use this method to set the necessary credentials used to communicate with
// the Google API for features such as translate. See this link for more info:
// https://support.google.com/googleapi/answer/6158857
// This method must be called before any |CWVWebViews| are instantiated for
// the keys to be used.
+ (void)setGoogleAPIKey:(NSString*)googleAPIKey
               clientID:(NSString*)clientID
           clientSecret:(NSString*)clientSecret;

- (instancetype)initWithFrame:(CGRect)frame
                configuration:(CWVWebViewConfiguration*)configuration;

// If |wkConfiguration| is provided, the underlying WKWebView is
// initialized with |wkConfiguration|, and assigned to
// |*createdWKWebView| if |createdWKWebView| is not nil.
// |*createdWKWebView| will be provided only if |wkConfiguration| is provided,
// otherwise it will always be reset to nil.
// IMPORTANT NOTE: Also create a new WKUserContentController and set it in the
// |wkConfiguration| before calling this method.
//
// IMPORTANT: Use |*createdWKWebView| just as a return value of
// -[WKNavigationDelegate
// webView:createWebViewWithConfiguration:...], but for nothing
// else. e.g., You must not access its properties/methods.
- (instancetype)initWithFrame:(CGRect)frame
                configuration:(CWVWebViewConfiguration*)configuration
              WKConfiguration:(nullable WKWebViewConfiguration*)wkConfiguration
             createdWKWebView:(WKWebView* _Nullable* _Nullable)createdWebView
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Navigates backwards or forwards by one page.  Does nothing if the
// corresponding |canGoBack| or |canGoForward| method returns NO.
- (void)goBack;
- (void)goForward;

// Navigates to the specified |item| in the |self.backForwardList| and returns
// YES. Does nothing and returns NO when |item| is the current item, or it
// belongs to an expired list, or the list does not contain |item|.
- (BOOL)goToBackForwardListItem:(CWVBackForwardListItem*)item;

// Reloads the current page.
- (void)reload;

// Stops loading the current page.
- (void)stopLoading;

// Loads the given URL request in this web view.
// Unlike WKWebView, this method supports HTTPBody.
- (void)loadRequest:(NSURLRequest*)request;

// Evaluates a JavaScript string in the main frame of the page content world.
// `completion` is invoked with the result of evaluating the script and a
// boolean representing success (`YES`) or failure (`NO`) of the evaluation.
//
// Evaluation of `javaScriptString` will fail (and return NO to `completion`)
// if there is no current internal representation of the main frame. This can
// occur when the web view is navigating or if the current page content does
// not allow JavaScript execution (ex: JS disabled or PDF content).
- (void)evaluateJavaScript:(NSString*)javaScriptString
         completionHandler:(nullable void (^)(id result,
                            NSError* __nullable error))completion;

// DEPRECATED: Use `evaluateJavaScript:completionHandler` instead. These
// methods are the same, but `evaluateJavaScript:completionHandler` provides
// better Swift type compatibility.
- (void)evaluateJavaScript:(NSString*)javaScriptString
                completion:(void (^)(id result, NSError* error))completion;

// DEPRECATED: Use `CWVUserContentController addMessageHandler:forCommand:`
// instead.
// Adds a message handler for messages sent from JavaScript from *any*
// CWVWebView.
// `handler` will be called each time a message is sent with the corresponding
// value of `command`. To send messages from JavaScript, use the WebKit
// message handler `CWVWebViewMessage` and provide values for the `command` and
// `payload` keys.
// `command` must be a string and match the registered handler `command` string
// `payload` must be a dictionary.
//
// Example call from JavaScript:
//
//  let message = {
//    'command': 'myFeatureMessage',
//    'payload' : {'key1':'value1', 'key2':42}
//  }
//  window.webkit.messageHandlers['CWVWebViewMessage'].postMessage(message);
//
// NOTE: Only a single `handler` may be registered for a given `command`.
- (void)addMessageHandler:(void (^)(NSDictionary* payload))handler
               forCommand:(NSString*)command;

// DEPRECATED: Use `CWVUserContentController removeMessageHandlerForCommand:`
// instead.
// Removes the message handler associated with `command` previously added with
// `addMessageHandler:forCommand:`.
- (void)removeMessageHandlerForCommand:(NSString*)command;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_WEB_VIEW_H_
