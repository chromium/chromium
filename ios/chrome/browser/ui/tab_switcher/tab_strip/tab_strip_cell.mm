// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_strip/tab_strip_cell.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/image/image_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/web/public/web_state_id.h"

namespace {

// The size of the xmark symbol image.
NSInteger kXmarkSymbolPointSize = 13;

const CGFloat kFaviconLeadingMargin = 16;
const CGFloat kCloseButtonMargin = 16;
const CGFloat kTitleInset = 10.0;
const CGFloat kFontSize = 14.0;

const CGFloat kFaviconSize = 16.0;

UIImage* DefaultFavicon() {
  return DefaultSymbolWithPointSize(kGlobeAmericasSymbol, 14);
}

}  // namespace

@implementation TabStripCell {
  UIButton* _closeButton;
  UILabel* _titleLabel;
  UIImageView* _faviconView;
}

- (instancetype)initWithFrame:(CGRect)frame {
  if ((self = [super initWithFrame:frame])) {
    // TODO(crbug.com/1490555): Remove the border once we get closer to the
    // design.
    self.layer.borderColor = UIColor.blackColor.CGColor;
    self.layer.borderWidth = 1;

    _faviconView = [[UIImageView alloc] initWithImage:DefaultFavicon()];
    _faviconView.contentMode = UIViewContentModeScaleAspectFit;

    [self.contentView addSubview:_faviconView];
    _faviconView.translatesAutoresizingMaskIntoConstraints = NO;
    [NSLayoutConstraint activateConstraints:@[
      [_faviconView.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kFaviconLeadingMargin],
      [_faviconView.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],
      [_faviconView.widthAnchor constraintEqualToConstant:kFaviconSize],
      [_faviconView.heightAnchor
          constraintEqualToAnchor:_faviconView.widthAnchor],
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
                         constant:-kCloseButtonMargin],
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

- (void)setTitle:(NSString*)title {
  _titleLabel.text = title;
}

- (void)setFaviconImage:(UIImage*)image {
  if (!image) {
    _faviconView.image = DefaultFavicon();
  } else {
    _faviconView.image = image;
  }
}

#pragma mark - UICollectionViewCell

- (void)prepareForReuse {
  [super prepareForReuse];
  _titleLabel.text = nil;
  self.selected = NO;
  [self setFaviconImage:nil];
}

#pragma mark - Private

// Selector registered to the close button.
- (void)closeButtonTapped:(id)sender {
  [self.delegate closeButtonTappedForCell:self];
}

- (void)setSelected:(BOOL)selected {
  [super setSelected:selected];
  self.backgroundColor = selected ? UIColor.blueColor : UIColor.whiteColor;
  // Style the favicon tint color.
  _faviconView.tintColor = selected ? [UIColor colorNamed:kCloseButtonColor]
                                    : [UIColor colorNamed:kGrey500Color];
  // Style the close button tint color.
  _closeButton.tintColor = selected ? [UIColor colorNamed:kCloseButtonColor]
                                    : [UIColor colorNamed:kGrey500Color];
  // Style the title tint color.
  _titleLabel.textColor = selected ? [UIColor colorNamed:kTextPrimaryColor]
                                   : [UIColor colorNamed:kGrey600Color];
}

@end
