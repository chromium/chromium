// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_PRINT_WEB_STATE_PRINTER_H_
#define IOS_CHROME_BROWSER_WEB_PRINT_WEB_STATE_PRINTER_H_

#import <Foundation/Foundation.h>

namespace web {
class WebState;
}

// Protocol implemented to print a WebState. Used to implement the javascript
// command "window.print".
@protocol WebStatePrinter <NSObject>

- (void)printWebState:(web::WebState*)webState;

@end

#endif  // IOS_CHROME_BROWSER_WEB_PRINT_WEB_STATE_PRINTER_H_
