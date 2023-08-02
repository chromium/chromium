// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/testing/earl_grey/matchers.h"

#import "ios/testing/earl_grey/earl_grey_test.h"

namespace testing {

id<GREYMatcher> ButtonWithAccessibilityLabel(NSString* label) {
  return grey_allOf(grey_accessibilityLabel(label),
                    grey_accessibilityTrait(UIAccessibilityTraitButton), nil);
}

id<GREYMatcher> ElementToDismissAlert(NSString* cancel_text) {
  UIUserInterfaceIdiom idiom = [[UIDevice currentDevice] userInterfaceIdiom];
  if (idiom == UIUserInterfaceIdiomPad) {
    // On iPad the context menu is dismissed by tapping on something
    // that isn't the popover. UIKit conveniently labels this element.
    return grey_accessibilityID(@"PopoverDismissRegion");
  } else {
    // On iPhone the context menu is dismissed by tapping on the "Cancel" item.
    return ButtonWithAccessibilityLabel(cancel_text);
  }
}

id<GREYMatcher> ElementWithAccessibilityLabelSubstring(NSString* substring) {
  GREYMatchesBlock matches = ^BOOL(NSObject* element) {
    NSString* accessibilityLabel =
        [(UIAccessibilityElement*)element accessibilityLabel];
    return [accessibilityLabel rangeOfString:substring].location != NSNotFound;
  };
  GREYDescribeToBlock describe = ^void(id<GREYDescription> description) {
    NSString* describeStr = [NSString
        stringWithFormat:@"ElementWithAccessibilityLabelSubstring(\"%@\")",
                         substring];
    [description appendText:describeStr];
  };
  return grey_allOf(
      grey_accessibilityElement(),
      [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                           descriptionBlock:describe],
      nil);
}

id<GREYMatcher> NavigationBarBackButton() {
  return grey_accessibilityID(@"BackButton");
}

}  // namespace testing
