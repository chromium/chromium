// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/cells/table_view_identity_cell.h"

#import "ios/chrome/browser/authentication/ui_bundled/views/identity_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"

namespace {
// Checkmark margin.
const CGFloat kCheckmarkMagin = 26.;
// Leading margin for the separator.
const CGFloat kSeparatorMargin = 80;
// Duration of the selection animation.
const CGFloat kSelectionAnimationDuration = 0.25;
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
                       managed:(BOOL)managed
             identityViewStyle:(IdentityViewStyle)identityViewStyle
                    titleColor:(UIColor*)titleColor {
  [self configureCellWithTitle:title
                      subtitle:subtitle
                         image:image
                       checked:checked
                       managed:managed
             identityViewStyle:identityViewStyle
                    titleColor:titleColor
                    completion:nil];
}

- (void)configureCellWithTitle:(NSString*)title
                      subtitle:(NSString*)subtitle
                         image:(UIImage*)image
                       checked:(BOOL)checked
                       managed:(BOOL)managed
             identityViewStyle:(IdentityViewStyle)identityViewStyle
                    titleColor:(UIColor*)titleColor
                    completion:(ProceduralBlock)completion {
  self.identityView.style = identityViewStyle;
  [self.identityView setTitle:title subtitle:subtitle managed:managed];
  [self.identityView setAvatar:image];
  self.identityView.titleColor = titleColor;

  void (^layoutUpdateBlock)(void) = ^{
    self.identityView.style =
        checked && identityViewStyle == IdentityViewStyleConsistency
            ? IdentityViewStyleConsistencyContained
            : identityViewStyle;
    if (checked && identityViewStyle != IdentityViewStyleConsistency) {
      self.directionalLayoutMargins =
          NSDirectionalEdgeInsetsMake(0, 0, 0, kCheckmarkMagin);
    } else {
      self.directionalLayoutMargins = NSDirectionalEdgeInsetsZero;
    }
    self.accessoryType = checked ? UITableViewCellAccessoryCheckmark
                                 : UITableViewCellAccessoryNone;
    [self layoutIfNeeded];
  };

  [UIView animateWithDuration:kSelectionAnimationDuration
                        delay:0
       usingSpringWithDamping:0.7
        initialSpringVelocity:0.5
                      options:UIViewAnimationOptionCurveEaseInOut
                   animations:layoutUpdateBlock
                   completion:^(BOOL finished) {
                     if (completion) {
                       completion();
                     }
                   }];
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.accessibilityIdentifier = nil;
  self.accessibilityLabel = nil;
  self.identityView.style = IdentityViewStyleDefault;
}

@end
