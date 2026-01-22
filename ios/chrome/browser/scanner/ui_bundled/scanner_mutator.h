// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SCANNER_UI_BUNDLED_SCANNER_MUTATOR_H_
#define IOS_CHROME_BROWSER_SCANNER_UI_BUNDLED_SCANNER_MUTATOR_H_

#import <Foundation/Foundation.h>

// Mutator for the Scanner.
@protocol ScannerMutator <NSObject>

// Loads the query scanned by the scanner.
- (void)loadScannerQuery:(NSString*)query;

@end

#endif  // IOS_CHROME_BROWSER_SCANNER_UI_BUNDLED_SCANNER_MUTATOR_H_
