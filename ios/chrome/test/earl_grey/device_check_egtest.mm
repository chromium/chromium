// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/element_selector.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

// Test suite to verify Internet connectivity.
@interface DeviceCheckTestCase : ChromeTestCase
@end

@implementation DeviceCheckTestCase

// Verifies Internet connectivity by navigating to google.com
// and asserting that the chrome dino page is not shown
- (void)testNetworkConnection {
  [ChromeEarlGrey loadURL:GURL("http://google.com")];
  [ChromeEarlGrey
      waitForWebStateNotContainingElement:
          [ElementSelector
              selectorWithCSSSelector:"[title='" +
                                      l10n_util::GetStringUTF8(
                                          IDS_ERRORPAGE_DINO_ARIA_LABEL) +
                                      "']"]];
}

@end
