// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/clear_browsing_data/pop_up_menu_control.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/settings/cells/clear_browsing_data_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"

namespace {

// Size of the chevron icon.
constexpr CGFloat kChevronIconSize = 16;

// Height/trailing/leading constraints of the label and the button.
constexpr CGFloat kSubtitleLeadingOffset = 20;
constexpr CGFloat kChevronLeadingOffset = 5;

}  // namespace

@implementation PopUpMenuControl {
  UILabel* _subtitleLabel;
  UILabel* _titleLabel;
  UIImageView* _chevronUpDown;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    self.isAccessibilityElement = YES;
    self.accessibilityTraits |= UIAccessibilityTraitButton;

    _titleLabel = [[UILabel alloc] init];
    _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _titleLabel.adjustsFontForContentSizeCategory = YES;
    _titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
    _titleLabel.numberOfLines = 1;
    [self addSubview:_titleLabel];

    _subtitleLabel = [[UILabel alloc] init];
    _subtitleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _subtitleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _subtitleLabel.adjustsFontForContentSizeCategory = YES;
    _subtitleLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    _subtitleLabel.numberOfLines = 1;
    _subtitleLabel.accessibilityIdentifier = kQuickDeletePopUpButtonIdentifier;
    [self addSubview:_subtitleLabel];

    UIImage* chevronUpDownDefault =
        DefaultSymbolWithPointSize(kChevronUpDown, kChevronIconSize);
    UIImageSymbolConfiguration* configuration = [UIImageSymbolConfiguration
        configurationWithHierarchicalColor:
            [UIColor colorNamed:kTextQuaternaryColor]];
    _chevronUpDown = [[UIImageView alloc]
        initWithImage:[chevronUpDownDefault
                          imageByApplyingSymbolConfiguration:configuration]];
    _chevronUpDown.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_chevronUpDown];

    [NSLayoutConstraint activateConstraints:@[
      // Center elements vertically.
      [_titleLabel.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
      [_subtitleLabel.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
      [_chevronUpDown.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],

      // Make the title be on the left of the subtitle. Make the chevron be on
      // the right of the subtitle. Add some horizontal offset between element.
      [_titleLabel.leadingAnchor
          constraintEqualToAnchor:self.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      [_subtitleLabel.leadingAnchor
          constraintEqualToAnchor:_titleLabel.trailingAnchor
                         constant:kSubtitleLeadingOffset],
      [_chevronUpDown.leadingAnchor
          constraintEqualToAnchor:_subtitleLabel.trailingAnchor
                         constant:kChevronLeadingOffset],
      [_chevronUpDown.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor
                         constant:-kTableViewHorizontalSpacing],

      // Add some vertical offset.
      [_titleLabel.heightAnchor
          constraintEqualToAnchor:self.heightAnchor
                         constant:-kTableViewOneLabelCellVerticalSpacing],
      [_subtitleLabel.heightAnchor
          constraintEqualToAnchor:self.heightAnchor
                         constant:-kTableViewOneLabelCellVerticalSpacing],
      [_chevronUpDown.heightAnchor
          constraintLessThanOrEqualToAnchor:self.heightAnchor
                                   constant:
                                       -kTableViewOneLabelCellVerticalSpacing],

      // Make sure the cell has a minimum height.
      [self.heightAnchor
          constraintGreaterThanOrEqualToConstant:kChromeTableViewCellHeight],
    ]];

    // The chevron should always be visible. The title should be the first to
    // disappear. If there isn't enough space to show all views, then it should
    // be the subtitle.
    [_titleLabel setContentHuggingPriority:UILayoutPriorityDefaultLow
                                   forAxis:UILayoutConstraintAxisHorizontal];
    [_titleLabel
        setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];
    [_subtitleLabel setContentHuggingPriority:UILayoutPriorityDefaultHigh
                                      forAxis:UILayoutConstraintAxisHorizontal];
    [_subtitleLabel
        setContentCompressionResistancePriority:UILayoutPriorityDefaultHigh
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];
    [_chevronUpDown setContentHuggingPriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];
    [_chevronUpDown
        setContentCompressionResistancePriority:UILayoutPriorityRequired
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];
  }
  return self;
}

#pragma mark - UIControl

// Change the background color while the UIControl is highlighted to simulate
// the same behaviour as a cell selection.
- (void)setHighlighted:(BOOL)highlighted {
  [super setHighlighted:highlighted];

  if (highlighted) {
    self.backgroundColor =
        [UIColor systemGray4Color];  // Same colour as a selected TableViewCell.
  } else {
    self.backgroundColor = UIColor.clearColor;
  }
}

// Return `menu` when there is a context menu interaction.
- (UIContextMenuConfiguration*)contextMenuInteraction:
                                   (UIContextMenuInteraction*)interaction
                       configurationForMenuAtLocation:(CGPoint)location {
  __weak __typeof(self) weakSelf = self;
  return [UIContextMenuConfiguration
      configurationWithIdentifier:nil
                  previewProvider:nil
                   actionProvider:^UIMenu*(
                       NSArray<UIMenuElement*>* suggestedActions) {
                     return weakSelf.menu;
                   }];
}

// Override of `[contextMenuInteraction:willEndForConfiguration:animator:]`.
// Update `_subtitleLabel` with the selected UIAction when the pop-up menu gets
// dismissed.
- (void)contextMenuInteraction:(UIContextMenuInteraction*)interaction
       willEndForConfiguration:(UIContextMenuConfiguration*)configuration
                      animator:(id<UIContextMenuInteractionAnimating>)animator {
  if ([_menu.selectedElements count] != 0) {
    [self setSubtitle:_menu.selectedElements[0].title];
  }
  [super contextMenuInteraction:interaction
        willEndForConfiguration:configuration
                       animator:animator];

  // Refocus on the entire control, so the new selection gets read out.
  UIAccessibilityPostNotification(UIAccessibilityLayoutChangedNotification,
                                  self);
}

// Override of `[menuAttachmentPointForConfiguration]`. Adjusts the menu's
// attachment point on the X axis so it's displayed from the `_subtitleLabel`.
- (CGPoint)menuAttachmentPointForConfiguration:
    (UIContextMenuConfiguration*)configuration {
  CGPoint originalPoint =
      [super menuAttachmentPointForConfiguration:configuration];

  CGPoint menuButtonOrigin = _subtitleLabel.frame.origin;
  BOOL isLTR = [UIApplication sharedApplication].userInterfaceLayoutDirection ==
               UIUserInterfaceLayoutDirectionLeftToRight;
  CGFloat adjustedX = menuButtonOrigin.x;
  if (isLTR) {
    CGSize menuButtonSize = _subtitleLabel.frame.size;
    adjustedX += menuButtonSize.width;
  }

  return CGPointMake(adjustedX, originalPoint.y);
}

#pragma mark - Properties

// Sets the UIMenu to be shown when the control is taped.
- (void)setMenu:(UIMenu*)menu {
  // The UIMenu must be of single selection so when
  // `[contextMenuInteraction:willEndForConfiguration:animator:]` gets called,
  // there one and only one selected item in `_menu.selectedElements`.
  CHECK(menu.options == UIMenuOptionsSingleSelection);

  self.contextMenuInteractionEnabled = YES;
  self.showsMenuAsPrimaryAction = YES;
  _menu = menu;

  // Populate `_subtitleLabel` with the selected UIAction.
  if ([_menu.selectedElements count] != 0) {
    [self setSubtitle:_menu.selectedElements[0].title];
  }
}

// Sets the title to be shown on the left.
- (void)setTitle:(NSString*)title {
  _titleLabel.text = title;
  self.accessibilityLabel = title;
}

#pragma mark - Private

// Sets the subtitle to be shown on the right.
- (void)setSubtitle:(NSString*)title {
  _subtitleLabel.text = title;
  self.accessibilityValue = title;
}

@end
