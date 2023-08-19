// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_strip/tab_strip_cell.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/image/image_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {

// The size of the xmark symbol image.
NSInteger kXmarkSymbolPointSize = 13;

// Tab close button insets.
const CGFloat kFaviconInset = 28;
const CGFloat kTitleInset = 10.0;
const CGFloat kFontSize = 14.0;

}  // namespace

@implementation TabStripCell

- (instancetype)initWithFrame:(CGRect)frame {
  if ((self = [super initWithFrame:frame])) {
    [self setupBackgroundViews];

    UIImage* favicon = [[UIImage imageNamed:@"default_world_favicon"]
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    _faviconView = [[UIImageView alloc] initWithImage:favicon];
    [self.contentView addSubview:_faviconView];
    _faviconView.translatesAutoresizingMaskIntoConstraints = NO;
    [NSLayoutConstraint activateConstraints:@[
      [_faviconView.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kFaviconInset],
      [_faviconView.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],
    ]];

    UIImage* close =
        DefaultSymbolTemplateWithPointSize(kXMarkSymbol, kXmarkSymbolPointSize);
    _closeButton = [UIButton buttonWithType:UIButtonTypeCustom];
    [_closeButton setImage:close forState:UIControlStateNormal];
    [self.contentView addSubview:_closeButton];
    _closeButton.translatesAutoresizingMaskIntoConstraints = NO;
    [NSLayoutConstraint activateConstraints:@[
      [_closeButton.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kFaviconInset],
      [_closeButton.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],
    ]];
    [_closeButton addTarget:self
                     action:@selector(closeButtonTapped:)
           forControlEvents:UIControlEventTouchUpInside];

    _titleLabel = [[UILabel alloc] init];
    _titleLabel.font = [UIFont systemFontOfSize:kFontSize
                                         weight:UIFontWeightMedium];
    [self.contentView addSubview:_titleLabel];
    _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [NSLayoutConstraint activateConstraints:@[
      [_titleLabel.leadingAnchor
          constraintEqualToAnchor:_faviconView.trailingAnchor
                         constant:kTitleInset],
      [_titleLabel.trailingAnchor
          constraintLessThanOrEqualToAnchor:_closeButton.leadingAnchor
                                   constant:-kTitleInset],
      [_titleLabel.centerYAnchor
          constraintEqualToAnchor:_faviconView.centerYAnchor],
    ]];
  }
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.titleLabel.text = nil;
  self.itemIdentifier = nil;
  self.selected = NO;
  self.faviconView = nil;
}

- (void)setupBackgroundViews {
  self.backgroundView = [[UIImageView alloc]
      initWithImage:[UIImage imageNamed:@"tabstrip_background_tab"]];
  self.selectedBackgroundView = [[UIImageView alloc]
      initWithImage:[UIImage imageNamed:@"tabstrip_foreground_tab"]];
}

- (BOOL)hasIdentifier:(NSString*)identifier {
  return [self.itemIdentifier isEqualToString:identifier];
}

#pragma mark - UIView

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  [self setupBackgroundViews];
}

#pragma mark - Private

// Selector registered to the close button.
- (void)closeButtonTapped:(id)sender {
  [self.delegate closeButtonTappedForCell:self];
}

- (void)setSelected:(BOOL)selected {
  [super setSelected:selected];
  // Style the favicon tint color.
  self.faviconView.tintColor = selected ? [UIColor colorNamed:kCloseButtonColor]
                                        : [UIColor colorNamed:kGrey500Color];
  // Style the close button tint color.
  self.closeButton.tintColor = selected ? [UIColor colorNamed:kCloseButtonColor]
                                        : [UIColor colorNamed:kGrey500Color];
  // Style the title tint color.
  self.titleLabel.textColor = selected ? [UIColor colorNamed:kTextPrimaryColor]
                                       : [UIColor colorNamed:kGrey600Color];
}

@end
