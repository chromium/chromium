// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/cells/table_view_identity_cell.h"

#import "ios/chrome/browser/ui/authentication/views/identity_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"

namespace {
// Checkmark margin.
const CGFloat kCheckmarkMagin = 26.;
// Leading margin for the separator.
const CGFloat kSeparatorMargin = 80;
}  // namespace

@interface TableViewIdentityCell ()
@property(nonatomic, strong) IdentityView* identityView;
@end

@implementation TableViewIdentityCell

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    _identityView = [[IdentityView alloc] initWithFrame:CGRectZero];
    _identityView.translatesAutoresizingMaskIntoConstraints = NO;
    [self.customSeparator.leadingAnchor
        constraintEqualToAnchor:self.contentView.leadingAnchor
                       constant:kSeparatorMargin]
        .active = YES;
    [self.contentView addSubview:_identityView];
    AddSameConstraints(_identityView, self.contentView);
    [self addInteraction:[[ViewPointerInteraction alloc] init]];
  }
  return self;
}

- (void)configureCellWithTitle:(NSString*)title
                      subtitle:(NSString*)subtitle
                         image:(UIImage*)image
                       checked:(BOOL)checked
             identityViewStyle:(IdentityViewStyle)identityViewStyle
                    titleColor:(UIColor*)titleColor {
  self.identityView.style = identityViewStyle;
  [self.identityView setTitle:title subtitle:subtitle];
  [self.identityView setAvatar:image];
  self.identityView.titleColor = titleColor;
  self.accessoryType = checked ? UITableViewCellAccessoryCheckmark
                               : UITableViewCellAccessoryNone;
  if (checked && identityViewStyle != IdentityViewStyleConsistency) {
    self.directionalLayoutMargins =
        NSDirectionalEdgeInsetsMake(0, 0, 0, kCheckmarkMagin);
  } else {
    self.directionalLayoutMargins = NSDirectionalEdgeInsetsZero;
  }
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.accessibilityIdentifier = nil;
  self.identityView.style = IdentityViewStyleDefault;
}

@end
