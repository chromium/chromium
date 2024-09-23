// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/find_bar/ui_bundled/find_bar_text_field.h"

#import "base/i18n/rtl.h"
#import "ios/chrome/browser/find_in_page/model/find_in_page_controller.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {
// Find bar left padding
const CGFloat kFindBarLeftPadding = 16;
}  // anonymous namespace

@implementation FindBarTextField

@synthesize overlayWidth = _overlayWidth;

#pragma mark - UIView

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.textAlignment = NSTextAlignmentNatural;
    self.accessibilityLabel =
        l10n_util::GetNSStringWithFixup(IDS_IOS_PLACEHOLDER_FIND_IN_PAGE);
  }
  return self;
}

#pragma mark - Public methods

- (void)setOverlayWidth:(CGFloat)overlayWidth {
  _overlayWidth = overlayWidth;
  [self setNeedsLayout];
}

#pragma mark - UITextField

- (CGRect)textRectForBounds:(CGRect)bounds {
  return [self editingRectForBounds:bounds];
}

- (CGRect)editingRectForBounds:(CGRect)bounds {
  // Reduce the width by the width of the overlay + padding for both sides of
  // the text.
  bounds.size.width -= _overlayWidth + 2 * kFindBarLeftPadding;
  bounds.origin.x += kFindBarLeftPadding;

  // Shift the text to the right side of the overlay for RTL languages.
  if (base::i18n::IsRTL())
    bounds.origin.x += _overlayWidth;
  return bounds;
}

@end
