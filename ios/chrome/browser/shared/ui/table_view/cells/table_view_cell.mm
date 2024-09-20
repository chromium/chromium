// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_cell.h"

#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/common/ui/util/ui_util.h"

namespace {
const CGFloat kTableViewCustomSeparatorHeight = 0.5;
}  // namespace

@interface TableViewCell ()
@end

@implementation TableViewCell

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    _customSeparator = [[UIView alloc] init];
    _customSeparator.translatesAutoresizingMaskIntoConstraints = NO;
    _customSeparator.backgroundColor = [UIColor colorNamed:kSeparatorColor];
    // Defaults to hidden until a custom separator is explicitly set.
    _customSeparator.hidden = YES;

    [self addSubview:_customSeparator];

    NSArray* constraints = @[
      [_customSeparator.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor],
      [_customSeparator.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
      [_customSeparator.heightAnchor
          constraintEqualToConstant:AlignValueToPixel(
                                        kTableViewCustomSeparatorHeight)],
      [_customSeparator.leadingAnchor
          constraintEqualToAnchor:self.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
    ];
    for (NSLayoutConstraint* constraint in constraints) {
      // Have a priority higher than the default high but don't make it required
      // to allow subclass to override it.
      constraint.priority = UILayoutPriorityDefaultHigh + 1;
    }
    [NSLayoutConstraint activateConstraints:constraints];
  }
  return self;
}

- (void)setUseCustomSeparator:(BOOL)useCustomSeparator {
  _useCustomSeparator = useCustomSeparator;
  self.customSeparator.hidden = !useCustomSeparator;
}

#pragma mark - UITableViewCell

- (void)prepareForReuse {
  [super prepareForReuse];
  self.useCustomSeparator = NO;
}
@end
