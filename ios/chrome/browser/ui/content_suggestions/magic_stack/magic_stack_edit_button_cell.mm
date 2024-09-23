// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_edit_button_cell.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_collection_view_audience.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

@implementation MagicStackEditButtonCell {
  UIButton* _editButton;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.accessibilityIdentifier =
        kMagicStackEditButtonContainerAccessibilityIdentifier;

    // Add Edit Button.
    UIButton* editButton = [UIButton buttonWithType:UIButtonTypeSystem];
    editButton.translatesAutoresizingMaskIntoConstraints = NO;
    UIImage* image = DefaultSymbolTemplateWithPointSize(
        kSliderHorizontalSymbol, kMagicStackEditButtonIconPointSize);
    [editButton setImage:image forState:UIControlStateNormal];
    editButton.tintColor = [UIColor colorNamed:kSolidBlackColor];
    editButton.backgroundColor =
        [UIColor colorNamed:@"magic_stack_edit_button_background_color"];
    editButton.layer.cornerRadius = kMagicStackEditButtonWidth / 2;
    editButton.accessibilityIdentifier =
        kMagicStackEditButtonAccessibilityIdentifier;
    editButton.pointerInteractionEnabled = YES;
    _editButton = editButton;
    [_editButton addTarget:self
                    action:@selector(didTapMagicStackEditButton)
          forControlEvents:UIControlEventTouchUpInside];
    [self addSubview:_editButton];

    [NSLayoutConstraint activateConstraints:@[
      [_editButton.leadingAnchor
          constraintEqualToAnchor:self.leadingAnchor
                         constant:kMagicStackEditButtonMargin],
      [_editButton.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor
                         constant:-kMagicStackEditButtonMargin],
      [_editButton.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
      [_editButton.widthAnchor
          constraintEqualToConstant:kMagicStackEditButtonWidth],
      [_editButton.heightAnchor constraintEqualToAnchor:editButton.widthAnchor]
    ]];
  }
  return self;
}

#pragma mark - UICollectionViewCell

- (void)prepareForReuse {
  [super prepareForReuse];
  self.audience = nil;
}

#pragma mark - Private

- (void)didTapMagicStackEditButton {
  [self.audience didTapMagicStackEditButton];
}

@end
