// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "ios/chrome/test/providers/partial_translate/test_partial_translate.h"

#import "ios/public/provider/chrome/browser/partial_translate/partial_translate_api.h"

namespace {
id<PartialTranslateControllerFactory> g_partial_translate_controller_factory;
}

namespace ios {
namespace provider {

id<PartialTranslateController> NewPartialTranslateController(
    NSString* source_text,
    const CGRect& anchor,
    BOOL incognito) {
  return [g_partial_translate_controller_factory
      createTranslateControllerForSourceText:source_text
                                  anchorRect:anchor
                                 inIncognito:incognito];
}

NSUInteger PartialTranslateLimitMaxCharacters() {
  return [g_partial_translate_controller_factory maximumCharacterLimit];
}

namespace test {
void SetPartialTranslateControllerFactory(
    id<PartialTranslateControllerFactory> factory) {
  g_partial_translate_controller_factory = factory;
}
}  // namespace test

}  // namespace provider
}  // namespace ios
