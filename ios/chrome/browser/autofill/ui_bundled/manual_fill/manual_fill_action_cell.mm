// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_action_cell.h"

#import "base/logging.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_cell_button.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"

@interface ManualFillActionItem ()

// The action block to be called when the user taps the title.
@property(nonatomic, copy, readonly) void (^action)(void);

// The title for the action.
@property(nonatomic, copy, readonly) NSString* title;

@end

@implementation ManualFillActionItem

- (instancetype)initWithTitle:(NSString*)title action:(void (^)(void))action {
  self = [super initWithType:kItemTypeEnumZero];
  if (self) {
    _title = [title copy];
    _action = [action copy];
    self.cellClass = [ManualFillActionCell class];
  }
  return self;
}

- (void)configureCell:(ManualFillActionCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  cell.accessibilityIdentifier = nil;
  [cell setUpWithTitle:self.title
       accessibilityID:self.accessibilityIdentifier
                action:self.action];
}

@end

@interface ManualFillActionCell ()

// The action block to be called when the user taps the title button.
@property(nonatomic, copy) void (^action)(void);

// The title button of this cell.
@property(nonatomic, strong) UIButton* titleButton;

@end

@implementation ManualFillActionCell

#pragma mark - Public

- (void)prepareForReuse {
  [super prepareForReuse];

  self.action = nil;
  [self.titleButton setTitle:nil forState:UIControlStateNormal];
  self.titleButton.accessibilityIdentifier = nil;
}

- (void)setUpWithTitle:(NSString*)title
       accessibilityID:(NSString*)accessibilityID
                action:(void (^)(void))action {
  if (self.contentView.subviews.count == 0) {
    [self createView];
  }

  UIButtonConfiguration* buttonConfiguration = self.titleButton.configuration;
  DCHECK(buttonConfiguration);
  UIFont* font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  NSDictionary* attributes = @{NSFontAttributeName : font};
  NSAttributedString* attributedTitleString =
      [[NSAttributedString alloc] initWithString:title attributes:attributes];
  buttonConfiguration.attributedTitle = attributedTitleString;
  if (IsKeyboardAccessoryUpgradeEnabled()) {
    buttonConfiguration.baseForegroundColor =
        [UIColor colorNamed:kBlue600Color];
  }
  self.titleButton.configuration = buttonConfiguration;
  self.titleButton.accessibilityIdentifier = accessibilityID;
  self.action = action;
}

#pragma mark - Private

- (void)createView {
  self.selectionStyle = UITableViewCellSelectionStyleNone;

  self.titleButton = [ManualFillCellButton buttonWithType:UIButtonTypeCustom];
  [self.titleButton addTarget:self
                       action:@selector(userDidTapTitleButton:)
             forControlEvents:UIControlEventTouchUpInside];

  [self.contentView addSubview:self.titleButton];

  AddSameConstraintsToSides(self.titleButton, self.contentView,
                            LayoutSides::kTop | LayoutSides::kBottom |
                                LayoutSides::kTrailing | LayoutSides::kLeading);
  [self addInteraction:[[ViewPointerInteraction alloc] init]];
}

- (void)userDidTapTitleButton:(UIButton*)sender {
  if (self.action) {
    self.action();
  }
}

@end
