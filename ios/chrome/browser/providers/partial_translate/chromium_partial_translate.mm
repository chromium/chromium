// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "ios/public/provider/chrome/browser/partial_translate/partial_translate_api.h"

namespace ios {
namespace provider {

id<PartialTranslateController> NewPartialTranslateController(
    NSString* source_text,
    const CGRect& anchor,
    BOOL incognito) {
  // Partial translate is not supported in Chromium.
  return nil;
}

NSUInteger PartialTranslateLimitMaxCharacters() {
  return 0;
}

}  // namespace provider
}  // namespace ios
