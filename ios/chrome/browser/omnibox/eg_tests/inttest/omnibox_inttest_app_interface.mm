// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/eg_tests/inttest/omnibox_inttest_app_interface.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/omnibox/eg_tests/inttest/fake_suggestions_builder.h"
#import "ios/chrome/browser/omnibox/eg_tests/inttest/omnibox_inttest_coordinator.h"
#import "ios/chrome/common/NSString+Chromium.h"
#import "ios/chrome/test/earl_grey/chrome_coordinator_app_interface.h"
#import "net/base/apple/url_conversions.h"
#import "url/gurl.h"

@implementation OmniboxInttestAppInterface

+ (void)enableFakeSuggestions {
  [OmniboxInttestAppInterface inttestCoordinator].fakeSuggestionEnabled = YES;
}

+ (void)addURLShortcutMatch:(NSString*)shortcutText
       destinationURLString:(NSString*)URLString {
  FakeSuggestionsBuilder* builder =
      [OmniboxInttestAppInterface fakeSuggestionsBuilder];
  builder->AddURLShortcut(shortcutText.cr_UTF16String,
                          URLString.cr_UTF16String);
}

+ (NSURL*)lastURLLoaded {
  return net::NSURLWithGURL(
      [OmniboxInttestAppInterface inttestCoordinator].lastURLLoaded);
}

#pragma mark - Private

+ (OmniboxInttestCoordinator*)inttestCoordinator {
  OmniboxInttestCoordinator* coordinator =
      base::apple::ObjCCastStrict<OmniboxInttestCoordinator>(
          ChromeCoordinatorAppInterface.coordinator);
  return coordinator;
}

+ (FakeSuggestionsBuilder*)fakeSuggestionsBuilder {
  return [OmniboxInttestAppInterface inttestCoordinator].fakeSuggestionsBuilder;
}

@end
