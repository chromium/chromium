// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/whats_new/cells/whats_new_table_view_subtitle_item.h"

#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@implementation WhatsNewTableViewSubtitleItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [WhatsNewTableViewSubtitleCell class];
  }
  return self;
}

- (void)configureHeaderFooterView:(WhatsNewTableViewSubtitleCell*)footer
                       withStyler:(ChromeTableViewStyler*)styler {
  [super configureHeaderFooterView:footer withStyler:styler];

  footer.textLabel.text = self.title;
}

@end

#pragma mark - WhatsNewTableViewSubtitleCell

@implementation WhatsNewTableViewSubtitleCell

@synthesize textLabel = _textLabel;

- (instancetype)initWithReuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithReuseIdentifier:reuseIdentifier];
  if (self) {
    _textLabel = [[UILabel alloc] init];
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _textLabel.font =
        CreateDynamicFont(UIFontTextStyleSubheadline, UIFontWeightRegular);
    _textLabel.adjustsFontForContentSizeCategory = YES;
    _textLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    _textLabel.numberOfLines = 0;

    [self.contentView addSubview:_textLabel];
    AddSameConstraints(_textLabel, self.contentView);
  }
  return self;
}

#pragma mark - UITableViewCell

- (void)prepareForReuse {
  [super prepareForReuse];

  _textLabel.text = nil;
}

#pragma mark - Private

- (NSString*)accessibilityLabel {
  return _textLabel.text;
}

@end
