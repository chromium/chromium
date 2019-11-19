// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/shell/test/earl_grey/web_view_shell_matchers.h"

#import <EarlGrey/EarlGrey.h>

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/web_view/shell/shell_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

id<GREYMatcher> AddressFieldText(const std::string& text) {
  MatchesBlock matches = ^BOOL(UIView* view) {
    if (![view isKindOfClass:[UITextField class]]) {
      return NO;
    }
    if (![[view accessibilityLabel]
            isEqualToString:kWebViewShellAddressFieldAccessibilityLabel]) {
      return NO;
    }
    UITextField* text_field = base::mac::ObjCCastStrict<UITextField>(view);
    NSString* error_message = [NSString
        stringWithFormat:
            @"Address field text did not match. expected: %@, actual: %@",
            base::SysUTF8ToNSString(text), text_field.text];
    bool success = base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForUIElementTimeout, ^{
          return base::SysNSStringToUTF8(text_field.text) == text;
        });
    GREYAssert(success, error_message);
    return YES;
  };

  DescribeToBlock describe = ^(id<GREYDescription> description) {
    [description appendText:@"address field containing "];
    [description appendText:base::SysUTF8ToNSString(text)];
  };

  return [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                              descriptionBlock:describe];
}

id<GREYMatcher> BackButton() {
  return grey_accessibilityLabel(kWebViewShellBackButtonAccessibilityLabel);
}

id<GREYMatcher> ForwardButton() {
  return grey_accessibilityLabel(kWebViewShellForwardButtonAccessibilityLabel);
}

id<GREYMatcher> AddressField() {
  return grey_accessibilityLabel(kWebViewShellAddressFieldAccessibilityLabel);
}

}  // namespace ios_web_view
