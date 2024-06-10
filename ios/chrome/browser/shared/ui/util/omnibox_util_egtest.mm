// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"

// Tests ios/chrome/browser/shared/ui/util/omnibox_util.h.
@interface OmniboxUtilTestCase : ChromeTestCase
@end

@implementation OmniboxUtilTestCase

- (void)setUp {
  [super setUp];
  [ChromeEarlGrey resetDataForLocalStatePref:prefs::kBottomOmnibox];

  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
}

- (void)tearDown {
  [super tearDown];
  [ChromeEarlGrey resetDataForLocalStatePref:prefs::kBottomOmnibox];
  [ChromeEarlGrey closeAllTabs];
}

/// Go to a web page to have a normal location bar.
- (void)loadPage {
  const GURL pageURL = self.testServer->GetURL("/pony.html");
  [ChromeEarlGrey loadURL:pageURL];
  const char pageContent[] = "pony jokes";  // See pony.html.
  [ChromeEarlGrey waitForWebStateContainingText:pageContent];
}

/// Asserts that IsBottomOmnibox is equal to `shouldBeBottomOmnibox` when the
/// feature is available.
- (void)assertIsBottomOmnibox:(BOOL)shouldBeBottomOmnibox {
  if (![ChromeEarlGrey isBottomOmniboxAvailable]) {
    GREYAssertFalse([ChromeEarlGrey isCurrentLayoutBottomOmnibox],
                    @"Bottom omnibox should not be shown when not available.");
  } else if (shouldBeBottomOmnibox) {
    GREYAssertTrue([ChromeEarlGrey isCurrentLayoutBottomOmnibox],
                   @"Omnibox should be in the bottom toolbar.");
  } else {
    GREYAssertFalse([ChromeEarlGrey isCurrentLayoutBottomOmnibox],
                    @"Omnibox should be in the top toolbar.");
  }
}

#pragma mark - Test cases

// Tests `IsCurrentLayoutBottomOmnibox` on NTP.
- (void)testIsBottomOmniboxOnNTP {
  [ChromeEarlGrey setBoolValue:NO forLocalStatePref:prefs::kBottomOmnibox];
  [ChromeEarlGrey openNewTab];
  [self assertIsBottomOmnibox:NO];

  [ChromeEarlGrey setBoolValue:YES forLocalStatePref:prefs::kBottomOmnibox];
  GREYWaitForAppToIdle(@"App failed to idle");
  [self assertIsBottomOmnibox:NO];
}

// Tests `IsCurrentLayoutBottomOmnibox` on incognito NTP.
- (void)testIsBottomOmniboxOnIncognitoNTP {
  [ChromeEarlGrey setBoolValue:NO forLocalStatePref:prefs::kBottomOmnibox];
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey waitForIncognitoTabCount:1];
  [self assertIsBottomOmnibox:NO];

  [ChromeEarlGrey setBoolValue:YES forLocalStatePref:prefs::kBottomOmnibox];
  GREYWaitForAppToIdle(@"App failed to idle");
  [self assertIsBottomOmnibox:YES];
}

// Tests `IsCurrentLayoutBottomOmnibox` on a web page.
- (void)testIsBottomOmniboxOnWebPage {
  [ChromeEarlGrey setBoolValue:NO forLocalStatePref:prefs::kBottomOmnibox];
  [self loadPage];
  [self assertIsBottomOmnibox:NO];

  [ChromeEarlGrey setBoolValue:YES forLocalStatePref:prefs::kBottomOmnibox];
  GREYWaitForAppToIdle(@"App failed to idle");
  [self assertIsBottomOmnibox:YES];
}

// Tests `IsCurrentLayoutBottomOmnibox` on landscape mode.
- (void)testIsBottomOmniboxOnLandscape {
  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeLeft
                                error:nil];

  [ChromeEarlGrey setBoolValue:NO forLocalStatePref:prefs::kBottomOmnibox];
  [self loadPage];
  [self assertIsBottomOmnibox:NO];

  [ChromeEarlGrey setBoolValue:YES forLocalStatePref:prefs::kBottomOmnibox];
  GREYWaitForAppToIdle(@"App failed to idle");
  [self assertIsBottomOmnibox:NO];
  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationPortrait error:nil];
  [self assertIsBottomOmnibox:YES];
}

@end
