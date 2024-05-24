// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_matchers.h"

#import "base/containers/contains.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_accessibility_identifier_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "url/gurl.h"

namespace omnibox {

id<GREYMatcher> PopupRowPrimaryTextMatcher() {
  return grey_allOf(
      grey_accessibilityID(kOmniboxPopupRowPrimaryTextAccessibilityIdentifier),
      grey_sufficientlyVisible(), nil);
}

id<GREYMatcher> PopupRowSecondaryTextMatcher() {
  return grey_allOf(grey_accessibilityID(
                        kOmniboxPopupRowSecondaryTextAccessibilityIdentifier),
                    grey_sufficientlyVisible(), nil);
}

id<GREYMatcher> PopupRowAtIndex(NSIndexPath* index) {
  NSString* accessibilityID = [OmniboxPopupAccessibilityIdentifierHelper
      accessibilityIdentifierForRowAtIndexPath:index];
  return grey_allOf(grey_accessibilityID(accessibilityID), nil);
}

id<GREYMatcher> PopupRowWithUrlMatcher(GURL url) {
  NSString* url_string = base::SysUTF8ToNSString(url.GetContent());
  id<GREYMatcher> url_matcher = grey_allOf(
      grey_descendant(
          chrome_test_util::StaticTextWithAccessibilityLabel(url_string)),
      grey_sufficientlyVisible(), nil);
  return grey_allOf(chrome_test_util::OmniboxPopupRow(), url_matcher, nil);
}

id<GREYMatcher> ClearButtonMatcher() {
  return chrome_test_util::ButtonWithAccessibilityLabelId(
      IDS_IOS_ACCNAME_CLEAR_TEXT);
}

}  // namespace omnibox
