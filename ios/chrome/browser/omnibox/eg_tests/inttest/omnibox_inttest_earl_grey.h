// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_EG_TESTS_INTTEST_OMNIBOX_INTTEST_EARL_GREY_H_
#define IOS_CHROME_BROWSER_OMNIBOX_EG_TESTS_INTTEST_OMNIBOX_INTTEST_EARL_GREY_H_

#import <Foundation/Foundation.h>

#import "ios/testing/earl_grey/base_eg_test_helper_impl.h"

#define OmniboxInttestEarlGrey \
  [OmniboxInttestEarlGreyImpl invokedFromFile:@"" __FILE__ lineNumber:__LINE__]

class GURL;

/// Methods used for the EarlGrey tests when the omnibox is presented by the
/// `OmniboxInttestCoordinator`.
@interface OmniboxInttestEarlGreyImpl : BaseEGTestHelperImpl

/// Focuses the omnibox.
- (void)focusOmnibox;

/// Focuses the omnibox and type `text`.
- (void)focusOmniboxAndType:(NSString*)text;

/// Adds an URL shortcut match with `shortcutText` and `shortcutURL`.
- (void)addURLShortcutMatch:(NSString*)shortcutText
             destinationURL:(const GURL&)shortcutURL;

/// Asserts that`URL`is loaded by the omnibox.
- (void)assertURLLoaded:(const GURL&)URL;

/// Asserts that `searchTerms` are loaded by the omnibox.
- (void)assertSearchLoaded:(NSString*)searchTerms;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_EG_TESTS_INTTEST_OMNIBOX_INTTEST_EARL_GREY_H_
