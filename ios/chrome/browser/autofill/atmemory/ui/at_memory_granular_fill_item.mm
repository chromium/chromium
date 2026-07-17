// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_granular_fill_item.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@implementation AtMemoryGranularFillItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [AtMemoryGranularFillCell class];
  }
  return self;
}

- (void)setAttributeValue:(id)attributeValue {
  if ([attributeValue isKindOfClass:[NSString class]]) {
    _attributeValue = @[ (NSString*)attributeValue ];
  } else if ([attributeValue isKindOfClass:[NSArray class]]) {
    _attributeValue = [(NSArray*)attributeValue copy];
  } else {
    _attributeValue = nil;
  }
}

#pragma mark - TableViewItem

- (void)configureCell:(LegacyTableViewCell*)cell {
  [super configureCell:cell];
  cell.selectionStyle = UITableViewCellSelectionStyleNone;
  AtMemoryGranularFillCell* granularFillCell =
      base::apple::ObjCCastStrict<AtMemoryGranularFillCell>(cell);
  [granularFillCell setAttributeName:self.attributeName
                      attributeValue:self.attributeValue
                              target:self.target
                              action:self.action];
}

- (LegacyTableViewCell*)cellForTableView:(UITableView*)tableView {
  RegisterTableViewCell<AtMemoryGranularFillCell>(tableView);
  return DequeueTableViewCell<AtMemoryGranularFillCell>(tableView);
}

@end

@implementation AtMemoryGranularFillCell {
  // The label displaying the attribute name.
  UILabel* _label;
  // The horizontal stack view containing the value chips.
  UIStackView* _chipsStack;
}

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    self.selectionStyle = UITableViewCellSelectionStyleNone;

    UIStackView* fieldStack = [[UIStackView alloc] init];
    fieldStack.translatesAutoresizingMaskIntoConstraints = NO;
    fieldStack.axis = UILayoutConstraintAxisVertical;
    fieldStack.spacing = 8.0;
    fieldStack.alignment = UIStackViewAlignmentLeading;
    [self.contentView addSubview:fieldStack];

    _label = [[UILabel alloc] init];
    _label.translatesAutoresizingMaskIntoConstraints = NO;
    _label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    _label.textColor = [UIColor colorNamed:kTextSecondaryColor];
    [fieldStack addArrangedSubview:_label];

    _chipsStack = [[UIStackView alloc] init];
    _chipsStack.translatesAutoresizingMaskIntoConstraints = NO;
    _chipsStack.axis = UILayoutConstraintAxisHorizontal;
    _chipsStack.spacing = 8.0;
    _chipsStack.alignment = UIStackViewAlignmentCenter;
    [fieldStack addArrangedSubview:_chipsStack];

    AddSameConstraintsWithInsets(
        fieldStack, self.contentView,
        NSDirectionalEdgeInsetsMake(12.0, 16.0, 12.0, 16.0));
  }
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  for (UIView* view in _chipsStack.arrangedSubviews) {
    [_chipsStack removeArrangedSubview:view];
    [view removeFromSuperview];
  }
}

- (void)setAttributeName:(NSString*)attributeName
          attributeValue:(NSArray<NSString*>*)chipTexts
                  target:(id)target
                  action:(SEL)action {
  _label.text = attributeName;

  for (UIView* view in _chipsStack.arrangedSubviews) {
    [_chipsStack removeArrangedSubview:view];
    [view removeFromSuperview];
  }

  for (NSString* text in chipTexts) {
    UIButton* button = [UIButton buttonWithType:UIButtonTypeSystem];
    button.translatesAutoresizingMaskIntoConstraints = NO;

    UIButtonConfiguration* config =
        [UIButtonConfiguration filledButtonConfiguration];
    config.title = text;
    config.baseForegroundColor = [UIColor colorNamed:kTextPrimaryColor];
    config.baseBackgroundColor = [UIColor tertiarySystemFillColor];
    config.cornerStyle = UIButtonConfigurationCornerStyleCapsule;
    config.contentInsets = NSDirectionalEdgeInsetsMake(8.0, 16.0, 8.0, 16.0);
    button.configuration = config;

    if (target && action) {
      [button addTarget:target
                    action:action
          forControlEvents:UIControlEventTouchUpInside];
    }

    [_chipsStack addArrangedSubview:button];
  }
}

@end
