// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SCANNER_SCANNER_PRESENTING_H_
#define IOS_CHROME_BROWSER_UI_SCANNER_SCANNER_PRESENTING_H_

// ScannerPresenting contains methods that control how a scanner UI is
// dismissed on screen.
@protocol ScannerPresenting

// Asks the implementer to dismiss the given |controller| and call the given
// |completion| afterwards.
- (void)dismissScannerViewController:(UIViewController*)controller
                          completion:(void (^)(void))completion;

@end

#endif  // IOS_CHROME_BROWSER_UI_SCANNER_SCANNER_PRESENTING_H_
