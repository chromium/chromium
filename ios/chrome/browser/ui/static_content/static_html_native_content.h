// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_STATIC_CONTENT_STATIC_HTML_NATIVE_CONTENT_H_
#define IOS_CHROME_BROWSER_UI_STATIC_CONTENT_STATIC_HTML_NATIVE_CONTENT_H_

#import <UIKit/UIKit.h>

#import "ios/web/public/deprecated/crw_native_content.h"
#import "ios/web/public/navigation/navigation_manager.h"
#include "ui/base/page_transition_types.h"

class GURL;
@class StaticHtmlViewController;

namespace web {
class BrowserState;
}

@class OverscrollActionsController;

// A |CRWNativeContent| that displays html pages either located in the
// application bundle, or obtained by a |HtmlGenerator|.
@interface StaticHtmlNativeContent : NSObject<CRWNativeContent>

- (instancetype)initWithStaticHTMLViewController:
                    (StaticHtmlViewController*)HTMLViewController
                                             URL:(const GURL&)URL
    NS_DESIGNATED_INITIALIZER;

// Convenience initializer. Creates a StaticHtmlNativeContent that displays
// the resource at |resourcePath|.
// |resourcePath| is the location of the static page to display, relative to the
// root of the application bundle.
// |browserState| is the user browser state and must not be null.
// |URL| is the url of the page.
- (instancetype)initWithResourcePathResource:(NSString*)resourcePath
                                browserState:(web::BrowserState*)browserState
                                         url:(const GURL&)URL;

- (instancetype)init NS_UNAVAILABLE;

// The scrollview of the native view.
- (UIScrollView*)scrollView;

// The overscroll actions controller of the native content.
// Setting this value to non-nil will enable Overscroll Actions.
// Invalidated when |close| is called on this object.
@property(nonatomic, strong)
    OverscrollActionsController* overscrollActionsController;

@end

#endif  // IOS_CHROME_BROWSER_UI_STATIC_CONTENT_STATIC_HTML_NATIVE_CONTENT_H_
