// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/icons/chrome_symbol.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kArrowClockWiseSymbol = @"arrow_clockwise";
NSString* const kIncognitoSymbol = @"incognito";
NSString* const kSquareNumberSymbol = @"square_number";
NSString* const kTranslateSymbol = @"translate";

UIImage* CustomSymbolWithConfiguration(NSString* symbolName,
                                       UIImageConfiguration* configuration) {
  return [UIImage imageNamed:symbolName
                    inBundle:nil
           withConfiguration:configuration];
}
