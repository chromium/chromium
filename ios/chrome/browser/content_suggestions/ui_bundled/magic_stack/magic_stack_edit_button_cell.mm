// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui_bundled/magic_stack/magic_stack_edit_button_cell.h"

#import "ios/chrome/browser/content_suggestions/ui_bundled/magic_stack/magic_stack_collection_view_audience.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/magic_stack/magic_stack_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_palette.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_trait.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
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
    editButton.layer.cornerRadius = kMagicStackEditButtonWidth / 2;
    editButton.accessibilityIdentifier =
        kMagicStackEditButtonAccessibilityIdentifier;
    editButton.pointerInteractionEnabled = YES;
    _editButton = editButton;
    [_editButton addTarget:self
                    action:@selector(didTapMagicStackEditButton)
          forControlEvents:UIControlEventTouchUpInside];
    [self addSubview:_editButton];

    if (IsNTPBackgroundCustomizationEnabled()) {
      [self registerForTraitChanges:@[ NewTabPageTrait.class ]
                         withAction:@selector(applyBackgroundColors)];
      [self applyBackgroundColors];
    } else {
      editButton.tintColor = [UIColor colorNamed:kTextSecondaryColor];
      editButton.backgroundColor =
          [UIColor colorNamed:@"magic_stack_edit_button_background_color"];
    }

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

// Sets the background using the current color palette, or defaults if none is
// set.
- (void)applyBackgroundColors {
  NewTabPageColorPalette* colorPalette =
      [self.traitCollection objectForNewTabPageTrait];

  if (colorPalette) {
    _editButton.tintColor = colorPalette.tintColor;
    _editButton.backgroundColor = colorPalette.tertiaryColor;
  } else {
    _editButton.tintColor = [UIColor colorNamed:kTextSecondaryColor];
    _editButton.backgroundColor =
        [UIColor colorNamed:@"magic_stack_edit_button_background_color"];
  }
}

@end
