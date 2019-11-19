// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/action_cell.h"

#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_cell_button.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_cell_utils.h"
#import "ios/chrome/browser/ui/list_model/list_model.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
    _enabled = YES;
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
                action:self.action
               enabled:self.enabled
         showSeparator:self.showSeparator];
}

@end

@interface ManualFillActionCell ()

// The action block to be called when the user taps the title button.
@property(nonatomic, copy) void (^action)(void);

// The dynamic constraints for all the lines (i.e. not set in createView).
@property(nonatomic, strong)
    NSMutableArray<NSLayoutConstraint*>* dynamicConstraints;

// The title button of this cell.
@property(nonatomic, strong) UIButton* titleButton;

// Separator line after cell, if needed.
@property(nonatomic, strong) UIView* grayLine;

@end

@implementation ManualFillActionCell

#pragma mark - Public

- (void)prepareForReuse {
  [super prepareForReuse];
  [NSLayoutConstraint deactivateConstraints:self.dynamicConstraints];
  [self.dynamicConstraints removeAllObjects];

  self.action = nil;
  [self.titleButton setTitle:nil forState:UIControlStateNormal];
  self.titleButton.accessibilityIdentifier = nil;
  self.titleButton.enabled = YES;
  self.grayLine.hidden = YES;
}

- (void)setUpWithTitle:(NSString*)title
       accessibilityID:(NSString*)accessibilityID
                action:(void (^)(void))action
               enabled:(BOOL)enabled
         showSeparator:(BOOL)showSeparator {
  if (self.contentView.subviews.count == 0) {
    [self createView];
  }

  self.grayLine.hidden = !showSeparator;

  [self.titleButton setTitle:title forState:UIControlStateNormal];
  self.titleButton.accessibilityIdentifier = accessibilityID;
  self.titleButton.enabled = enabled;
  if (!enabled) {
    [self.titleButton setTitleColor:[UIColor colorNamed:kDisabledTintColor]
                           forState:UIControlStateNormal];
  }
  self.action = action;
  if (enabled) {
    self.dynamicConstraints = [[NSMutableArray alloc] initWithArray:@[
      [self.contentView.topAnchor
          constraintEqualToAnchor:self.titleButton.topAnchor],
      [self.contentView.bottomAnchor
          constraintEqualToAnchor:self.titleButton.bottomAnchor],
    ]];
  } else {
    self.dynamicConstraints = [[NSMutableArray alloc] initWithArray:@[
      [self.titleButton.topAnchor
          constraintEqualToSystemSpacingBelowAnchor:self.contentView.topAnchor
                                         multiplier:1.0],
      [self.contentView.bottomAnchor
          constraintEqualToSystemSpacingBelowAnchor:self.titleButton
                                                        .bottomAnchor
                                         multiplier:1.0],
    ]];
  }
  [NSLayoutConstraint activateConstraints:self.dynamicConstraints];
}

#pragma mark - Private

- (void)createView {
  self.selectionStyle = UITableViewCellSelectionStyleNone;

  UIView* guide = self.contentView;
  self.grayLine = CreateGraySeparatorForContainer(guide);

  self.titleButton = [ManualFillCellButton buttonWithType:UIButtonTypeCustom];
  [self.titleButton addTarget:self
                       action:@selector(userDidTapTitleButton:)
             forControlEvents:UIControlEventTouchUpInside];

  [self.contentView addSubview:self.titleButton];

  NSMutableArray<NSLayoutConstraint*>* staticConstraints =
      [[NSMutableArray alloc] init];
  AppendHorizontalConstraintsForViews(staticConstraints, @[ self.titleButton ],
                                      guide);
  [NSLayoutConstraint activateConstraints:staticConstraints];
}

- (void)userDidTapTitleButton:(UIButton*)sender {
  if (self.action) {
    self.action();
  }
}

@end
