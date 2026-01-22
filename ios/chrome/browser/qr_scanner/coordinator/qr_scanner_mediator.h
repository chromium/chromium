// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_QR_SCANNER_COORDINATOR_QR_SCANNER_MEDIATOR_H_
#define IOS_CHROME_BROWSER_QR_SCANNER_COORDINATOR_QR_SCANNER_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/scanner/ui_bundled/scanner_mutator.h"

class UrlLoadingBrowserAgent;

// Mediator for the QR Scanner.
@interface QRScannerMediator : NSObject <ScannerMutator>

- (instancetype)initWithLoader:(UrlLoadingBrowserAgent*)loader
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_QR_SCANNER_COORDINATOR_QR_SCANNER_MEDIATOR_H_
