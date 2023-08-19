// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/public/cells/popup_menu_footer_item.h"

namespace {
const CGFloat kSeparatorHeight = 1;
const CGFloat kSeparatorMargin = 12;
}  // namespace

@implementation PopupMenuFooterItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [PopupMenuFooterCell class];
  }
  return self;
}

- (void)configureHeaderFooterView:(PopupMenuFooterCell*)footer
                       withStyler:(ChromeTableViewStyler*)styler {
  [super configureHeaderFooterView:footer withStyler:styler];
  // By default the table view footers have a background view. Remove it so it
  // is transparent.
  footer.backgroundView = nil;
}

@end

#pragma mark - PopupMenuFooterCell

@implementation PopupMenuFooterCell

- (instancetype)initWithReuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithReuseIdentifier:reuseIdentifier];
  if (self) {
    UIView* separator = [[UIView alloc] init];
    separator.translatesAutoresizingMaskIntoConstraints = NO;
    separator.backgroundColor =
        [UIColor colorNamed:@"popup_menu_separator_color"];
    [self.contentView addSubview:separator];
    [NSLayoutConstraint activateConstraints:@[
      [separator.heightAnchor constraintEqualToConstant:kSeparatorHeight],
      [separator.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kSeparatorMargin],
      [separator.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kSeparatorMargin],
      [separator.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],
    ]];
  }
  return self;
}

@end
