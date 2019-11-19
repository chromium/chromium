// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_QR_SCANNER_QR_SCANNER_VIEW_H_
#define IOS_CHROME_BROWSER_UI_QR_SCANNER_QR_SCANNER_VIEW_H_

#import "ios/chrome/browser/ui/scanner/scanner_view.h"

// The view rendering the QR Scanner UI. The view contains the camera
// preview, a semi-transparent overlay with a transparent viewport, border
// around the viewport, the close and flash controls, and a label instructing
// the user to correctly position the QR code or bar code.
@interface QRScannerView : ScannerView

@end

#endif  // IOS_CHROME_BROWSER_UI_QR_SCANNER_QR_SCANNER_VIEW_H_
