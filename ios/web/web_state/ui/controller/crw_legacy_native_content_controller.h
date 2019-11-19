// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_UI_CONTROLLER_CRW_LEGACY_NATIVE_CONTENT_CONTROLLER_H_
#define IOS_WEB_WEB_STATE_UI_CONTROLLER_CRW_LEGACY_NATIVE_CONTENT_CONTROLLER_H_

#include <UIKit/UIKit.h>

#import "ios/web/public/deprecated/crw_native_content_holder.h"

namespace web {
class NavigationContextImpl;
class NavigationItemImpl;
class WebStateImpl;
}
@protocol CRWLegacyNativeContentControllerDelegate;
@protocol CRWNativeContent;
class GURL;

// Object managing the native content controller and its interactions with other
// objects.
@interface CRWLegacyNativeContentController : NSObject <CRWNativeContentHolder>

- (instancetype)initWithWebState:(web::WebStateImpl*)webState
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@property(nonatomic, weak) id<CRWLegacyNativeContentControllerDelegate>
    delegate;

// Return YES if there is a native content controller currently initialized.
- (BOOL)hasController;

// Returns |YES| if |URL| should be loaded in a native view.
- (BOOL)shouldLoadURLInNativeView:(const GURL&)URL;

// Informs the native controller if web usage is allowed or not.
- (void)setNativeControllerWebUsageEnabled:(BOOL)webUsageEnabled;

// Loads the current URL in a native controller if using the legacy navigation
// stack. If the new navigation stack is used, start loading a placeholder
// into the web view, upon the completion of which the native controller will
// be triggered.
- (void)loadCurrentURLInNativeViewWithRendererInitiatedNavigation:
    (BOOL)rendererInitiated;

// Notifies the native content that the navigation associated with |context| and
// |item| did finish.
- (void)webViewDidFinishNavigationWithContext:
            (web::NavigationContextImpl*)context
                                      andItem:(web::NavigationItemImpl*)item;

// Lets the native content handle its part when there is a cancellation error.
- (void)handleCancelledErrorForContext:(web::NavigationContextImpl*)context;

// Lets the native content handle its part when there is an SSL error.
- (void)handleSSLError;

// Stops the loading of the Native Content.
- (void)stopLoading;

// Removes the current native controller.
- (void)resetNativeController;

// *****************************************
// ** Calls related to the NativeContent. **
// *****************************************

// Notifies the CRWNativeContent that it has been shown.
- (void)wasShown;

// Notifies the CRWNativeContent that it has been hidden.
- (void)wasHidden;

// A native content controller should do any clean up at this time when
// WebController closes.
- (void)close;

// The URL represented by the content being displayed.
- (const GURL&)URL;

// Reloads any displayed data to ensure the view is up to date.
- (void)reload;

// The content inset and offset of the native content.
- (CGPoint)contentOffset;
- (UIEdgeInsets)contentInset;

@end

#endif  // IOS_WEB_WEB_STATE_UI_CONTROLLER_CRW_LEGACY_NATIVE_CONTENT_CONTROLLER_H_
