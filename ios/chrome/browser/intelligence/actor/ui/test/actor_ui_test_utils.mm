// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/ui/test/actor_ui_test_utils.h"

namespace intelligence::actor {

UIView* FindViewByAccessibilityIdentifier(UIView* root_view,
                                          NSString* accessibility_identifier) {
  if (!root_view || !accessibility_identifier) {
    return nil;
  }

  if ([root_view.accessibilityIdentifier
          isEqualToString:accessibility_identifier]) {
    return root_view;
  }

  for (UIView* subview in root_view.subviews) {
    UIView* found_view =
        FindViewByAccessibilityIdentifier(subview, accessibility_identifier);
    if (found_view) {
      return found_view;
    }
  }
  return nil;
}

}  // namespace intelligence::actor
