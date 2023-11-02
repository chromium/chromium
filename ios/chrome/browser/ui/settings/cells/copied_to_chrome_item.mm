// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/copied_to_chrome_item.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation CopiedToChromeItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [CopiedToChromeCell class];
  }
  return self;
}

@end

@implementation CopiedToChromeCell

@synthesize textLabel = _textLabel;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    UIView* contentView = self.contentView;

    _textLabel = [[UILabel alloc] init];
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _textLabel.text =
        l10n_util::GetNSString(IDS_IOS_AUTOFILL_DESCRIBE_LOCAL_COPY);
    _textLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _textLabel.adjustsFontForContentSizeCategory = YES;
    _textLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
    [_textLabel
        setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];
    [contentView addSubview:_textLabel];

    _button = [UIButton buttonWithType:UIButtonTypeCustom];
    [_button setTitleColor:[UIColor colorNamed:kBlueColor]
                  forState:UIControlStateNormal];

    _button.translatesAutoresizingMaskIntoConstraints = NO;
    [_button
        setTitle:l10n_util::GetNSString(IDS_AUTOFILL_CLEAR_LOCAL_COPY_BUTTON)
        forState:UIControlStateNormal];
    [contentView addSubview:_button];

    // Set up the constraints.
    [NSLayoutConstraint activateConstraints:@[
      [_textLabel.leadingAnchor
          constraintEqualToAnchor:contentView.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      [_textLabel.trailingAnchor
          constraintLessThanOrEqualToAnchor:_button.leadingAnchor
                                   constant:-kTableViewHorizontalSpacing],
      [_textLabel.centerYAnchor
          constraintEqualToAnchor:contentView.centerYAnchor],
      [_button.trailingAnchor
          constraintEqualToAnchor:contentView.trailingAnchor
                         constant:-kTableViewHorizontalSpacing],
      [_button.firstBaselineAnchor
          constraintEqualToAnchor:_textLabel.firstBaselineAnchor],
    ]];
    AddOptionalVerticalPadding(contentView, _textLabel,
                               kTableViewOneLabelCellVerticalSpacing);
  }
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  [self.button removeTarget:nil
                     action:nil
           forControlEvents:UIControlEventAllEvents];
}

@end
