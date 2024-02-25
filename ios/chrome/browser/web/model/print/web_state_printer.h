// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_PRINT_WEB_STATE_PRINTER_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_PRINT_WEB_STATE_PRINTER_H_

#import <UIKit/UIKit.h>

namespace web {
class WebState;
}

// Protocol implemented to print a WebState. Used to implement the javascript
// command "window.print".
@protocol WebStatePrinter <NSObject>

// Print WebState.
// Print preview will be presented on top of `baseViewController`.
- (void)printWebState:(web::WebState*)webState
    baseViewController:(UIViewController*)baseViewController;

// Print WebState.
// The receiver is in charge of choosing the presenting VC for the print
// preview.
- (void)printWebState:(web::WebState*)webState;

@end

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_PRINT_WEB_STATE_PRINTER_H_
