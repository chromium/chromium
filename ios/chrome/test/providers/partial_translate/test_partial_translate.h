// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_PROVIDERS_PARTIAL_TRANSLATE_TEST_PARTIAL_TRANSLATE_H_
#define IOS_CHROME_TEST_PROVIDERS_PARTIAL_TRANSLATE_TEST_PARTIAL_TRANSLATE_H_

#import "ios/public/provider/chrome/browser/partial_translate/partial_translate_api.h"

// A protocol to replace the Partial translate providers in tests.
@protocol PartialTranslateControllerFactory

- (id<PartialTranslateController>)
    createTranslateControllerForSourceText:(NSString*)sourceText
                                anchorRect:(CGRect)anchor
                               inIncognito:(BOOL)inIncognito;

- (NSUInteger)maximumCharacterLimit;

@end

namespace ios {
namespace provider {
namespace test {

// Sets the global factory for the tests.
// Resets it if `factory` is nil.
void SetPartialTranslateControllerFactory(
    id<PartialTranslateControllerFactory> factory);

}  // namespace test
}  // namespace provider
}  // namespace ios

#endif  // IOS_CHROME_TEST_PROVIDERS_PARTIAL_TRANSLATE_TEST_PARTIAL_TRANSLATE_H_
