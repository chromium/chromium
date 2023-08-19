// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/util/accessibility_close_menu_button.h"

#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

@implementation AccessibilityCloseMenuButton

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.accessibilityLabel =
        l10n_util::GetNSString(IDS_IOS_TOOLBAR_CLOSE_MENU);
  }
  return self;
}

- (instancetype)initWithCoder:(NSCoder*)aDecoder {
  self = [super initWithCoder:aDecoder];
  if (self) {
    self.accessibilityLabel =
        l10n_util::GetNSString(IDS_IOS_TOOLBAR_CLOSE_MENU);
  }
  return self;
}

// If accessibilityActivate isn't overriden, VoiceOver isn't able to close the
// menu.
// See crbbug.com/936850.
- (BOOL)accessibilityActivate {
  return [super accessibilityActivate];
}

@end
