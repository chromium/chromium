// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui_bundled/cells/content_suggestions_most_visited_action_item.h"

@implementation ContentSuggestionsMostVisitedActionItem

#pragma mark - Accessors

- (void)setTitle:(NSString*)title {
  if ([_title isEqualToString:title]) {
    return;
  }
  _title = title;
  [self updateAccessibilityTraits];
}

- (void)setCount:(NSInteger)count {
  if (_count == count) {
    return;
  }
  _count = count;
  [self updateAccessibilityTraits];
}

- (void)setDisabled:(BOOL)disabled {
  if (_disabled == disabled) {
    return;
  }
  _disabled = disabled;
  [self updateAccessibilityTraits];
}

#pragma mark - Private

// Updates self.accessibilityTraits based on the current property values.
- (void)updateAccessibilityTraits {
  if (self.disabled) {
    self.accessibilityTraits =
        super.accessibilityTraits | UIAccessibilityTraitNotEnabled;
  } else {
    self.accessibilityTraits =
        super.accessibilityTraits & ~UIAccessibilityTraitNotEnabled;
  }
}

@end
