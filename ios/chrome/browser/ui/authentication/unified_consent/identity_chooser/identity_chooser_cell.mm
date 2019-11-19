// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/unified_consent/identity_chooser/identity_chooser_cell.h"

#include "base/i18n/rtl.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/identity_chooser/identity_view.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Identity view.
const CGFloat kLeadingMargin = 8.;
const CGFloat kIdentityViewVerticalMargin = 7.;
// Checkmark margin.
const CGFloat kCheckmarkMagin = 26.;
}  // namespace

@interface IdentityChooserCell ()
@property(nonatomic, strong) IdentityView* identityView;
@end

@implementation IdentityChooserCell

@synthesize identityView = _identityView;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    _identityView = [[IdentityView alloc] initWithFrame:CGRectZero];
    _identityView.translatesAutoresizingMaskIntoConstraints = NO;
    _identityView.minimumVerticalMargin = kIdentityViewVerticalMargin;
    [self.contentView addSubview:_identityView];
    LayoutSides sideFlags = LayoutSides::kLeading | LayoutSides::kTrailing |
                            LayoutSides::kBottom | LayoutSides::kTop;
    ChromeDirectionalEdgeInsets insets =
        ChromeDirectionalEdgeInsetsMake(0, kLeadingMargin, 0, 0);
    AddSameConstraintsToSidesWithInsets(_identityView, self.contentView,
                                        sideFlags, insets);
  }
  return self;
}

- (void)configureCellWithTitle:(NSString*)title
                      subtitle:(NSString*)subtitle
                         image:(UIImage*)image
                       checked:(BOOL)checked {
  [self.identityView setTitle:title subtitle:subtitle];
  [self.identityView setAvatar:image];
  self.accessoryType = checked ? UITableViewCellAccessoryCheckmark
                               : UITableViewCellAccessoryNone;
  if (checked) {
    self.directionalLayoutMargins =
        NSDirectionalEdgeInsetsMake(0, 0, 0, kCheckmarkMagin);
  } else {
    self.directionalLayoutMargins = NSDirectionalEdgeInsetsZero;
  }
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.accessibilityIdentifier = nil;
}

@end
