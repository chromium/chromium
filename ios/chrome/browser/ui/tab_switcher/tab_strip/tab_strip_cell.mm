// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_strip/tab_strip_cell.h"

#import <MaterialComponents/MaterialActivityIndicator.h>

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/image/image_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

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
  MDCActivityIndicator* _activityIndicator;
}

- (instancetype)initWithFrame:(CGRect)frame {
  if ((self = [super initWithFrame:frame])) {
    // TODO(crbug.com/1490555): Remove the border once we get closer to the
    // design.
    self.layer.borderColor = UIColor.blackColor.CGColor;
    self.layer.borderWidth = 1;

    UIView* contentView = self.contentView;

    UILayoutGuide* leadingImageGuide = [[UILayoutGuide alloc] init];
    [self addLayoutGuide:leadingImageGuide];

    _faviconView = [self createFaviconView];
    [contentView addSubview:_faviconView];

    _activityIndicator = [self createActivityIndicatior];
    [contentView addSubview:_activityIndicator];

    _closeButton = [self createCloseButton];
    [contentView addSubview:_closeButton];

    _titleLabel = [self createTitleLabel];
    [contentView addSubview:_titleLabel];

    [NSLayoutConstraint activateConstraints:@[
      [leadingImageGuide.leadingAnchor
          constraintEqualToAnchor:contentView.leadingAnchor
                         constant:kFaviconLeadingMargin],
      [leadingImageGuide.centerYAnchor
          constraintEqualToAnchor:contentView.centerYAnchor],
      [leadingImageGuide.widthAnchor constraintEqualToConstant:kFaviconSize],
      [leadingImageGuide.heightAnchor
          constraintEqualToAnchor:leadingImageGuide.widthAnchor],
    ]];

    AddSameConstraints(leadingImageGuide, _faviconView);
    AddSameConstraints(leadingImageGuide, _activityIndicator);

    [NSLayoutConstraint activateConstraints:@[
      [_closeButton.trailingAnchor
          constraintEqualToAnchor:contentView.trailingAnchor
                         constant:-kCloseButtonMargin],
      [_closeButton.centerYAnchor
          constraintEqualToAnchor:contentView.centerYAnchor],
    ]];

    [NSLayoutConstraint activateConstraints:@[
      [_titleLabel.leadingAnchor
          constraintEqualToAnchor:leadingImageGuide.trailingAnchor
                         constant:kTitleInset],
      [_titleLabel.trailingAnchor
          constraintLessThanOrEqualToAnchor:_closeButton.leadingAnchor
                                   constant:-kTitleInset],
      [_titleLabel.centerYAnchor
          constraintEqualToAnchor:contentView.centerYAnchor],
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

- (void)setLoading:(BOOL)loading {
  if (_loading == loading) {
    return;
  }
  _loading = loading;
  if (loading) {
    _activityIndicator.hidden = NO;
    [_activityIndicator startAnimating];
    _faviconView.hidden = YES;
    _faviconView.image = DefaultFavicon();
  } else {
    _activityIndicator.hidden = YES;
    [_activityIndicator stopAnimating];
    _faviconView.hidden = NO;
  }
}

#pragma mark - Accessor

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

// Returns a new favicon view.
- (UIImageView*)createFaviconView {
  UIImageView* faviconView =
      [[UIImageView alloc] initWithImage:DefaultFavicon()];
  faviconView.translatesAutoresizingMaskIntoConstraints = NO;
  faviconView.contentMode = UIViewContentModeScaleAspectFit;
  return faviconView;
}

// Returns a new close button.
- (UIButton*)createCloseButton {
  UIImage* close =
      DefaultSymbolTemplateWithPointSize(kXMarkSymbol, kXmarkSymbolPointSize);
  UIButton* closeButton = [UIButton buttonWithType:UIButtonTypeCustom];
  closeButton.translatesAutoresizingMaskIntoConstraints = NO;
  [closeButton setImage:close forState:UIControlStateNormal];
  [closeButton addTarget:self
                  action:@selector(closeButtonTapped:)
        forControlEvents:UIControlEventTouchUpInside];
  return closeButton;
}

// Returns a new title label.
- (UILabel*)createTitleLabel {
  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  titleLabel.font = [UIFont systemFontOfSize:kFontSize
                                      weight:UIFontWeightMedium];
  return titleLabel;
}

// Returns a new Activity Indicator.
- (MDCActivityIndicator*)createActivityIndicatior {
  MDCActivityIndicator* activityIndicator = [[MDCActivityIndicator alloc] init];
  activityIndicator.translatesAutoresizingMaskIntoConstraints = NO;
  activityIndicator.hidden = YES;
  return activityIndicator;
}

@end
