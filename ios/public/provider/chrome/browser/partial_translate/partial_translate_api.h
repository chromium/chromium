// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_PARTIAL_TRANSLATE_PARTIAL_TRANSLATE_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_PARTIAL_TRANSLATE_PARTIAL_TRANSLATE_API_H_

#import <UIKit/UIKit.h>

@protocol PartialTranslateController <NSObject>

// Presents the PartialTranslateController on top of viewController.
// `flowCompletionHandler` will be called at the end of the feature either
// - immediately with a value of NO if it fails
// - after the PartialTranslateController is dismissed with a value of YES if it
// succeeded. Note: Once this method is called, it is the responsibility of the
// caller to retain the PartialTranslateController until `flowCompletionHandler`
// is called.
- (void)presentOnViewController:(UIViewController*)viewController
          flowCompletionHandler:(void (^)(BOOL))flowCompletionHandler;

@end

namespace ios {
namespace provider {

// Creates a PartialTranslateController to present the translate string for
// `source_text`. `anchor` is the position of `sourceText` in window
// coordinates. `incognito` is true if `source_text` was retrieved in an
// incognito tab.
id<PartialTranslateController> NewPartialTranslateController(
    NSString* source_text,
    const CGRect& anchor,
    BOOL incognito);

// The maximum length for the partial translate feature. Creating a
// PartialTranslateController with a longer string and trying to present it will
// fail.
NSUInteger PartialTranslateLimitMaxCharacters();

}  // namespace provider
}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_PARTIAL_TRANSLATE_PARTIAL_TRANSLATE_API_H_
