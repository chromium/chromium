// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/ui_bundled/toolbar_progress_bar.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

/// The duration for the appear & disappear animations.
const CGFloat kAppearanceAnimationDuration = 0.3f;

}  // namespace

@implementation ToolbarProgressBar

- (void)setHidden:(BOOL)hidden
         animated:(BOOL)animated
       completion:(void (^)(BOOL finished))userCompletion {
  CGFloat alpha = hidden ? 0 : 1;

  if (!animated) {
    self.hidden = hidden;
    self.alpha = alpha;
    if (userCompletion) {
      userCompletion(YES);
    }
    return;
  }
  if (!hidden) {
    self.hidden = NO;
  }
  __weak __typeof(self) weakSelf = self;
  [UIView animateWithDuration:kAppearanceAnimationDuration
      animations:^{
        weakSelf.alpha = alpha;
      }
      completion:^(BOOL finished) {
        weakSelf.hidden = hidden;
        if (userCompletion) {
          userCompletion(YES);
        }
      }];
}

#pragma mark - UIAccessibility

- (NSString*)accessibilityValue {
  return l10n_util::GetNSStringF(
      IDS_IOS_PROGRESS_BAR_ACCESSIBILITY,
      base::SysNSStringToUTF16([super accessibilityValue]));
}

@end
