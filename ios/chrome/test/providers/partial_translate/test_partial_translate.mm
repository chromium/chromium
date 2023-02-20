// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "ios/public/provider/chrome/browser/partial_translate/partial_translate_api.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

id<PartialTranslateController> NewPartialTranslateController(
    NSString* source_text,
    const CGRect& anchor,
    BOOL incognito) {
  // Partial translate is not supported in tests.
  return nil;
}

NSUInteger PartialTranslateLimitMaxCharacters() {
  return 0;
}
