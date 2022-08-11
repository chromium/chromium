// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/query_suggestion_view.h"

#import "ios/chrome/browser/ui/icons/chrome_symbol.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Size constraints for this view.
const CGFloat kViewWidthAnchor = 149.0f;
const CGFloat kViewHeightAnchor = 51.5f;

// Width constraint for the search ImageView.
const CGFloat kQueryImageMaxWidth = 20.0f;

// Size constraint for the query label.
const CGFloat kQueryLabelHeightAnchor = 11.0f;

// Spacing between search ImageView and label.
const CGFloat kQueryImageToLabelHorizontalSpacing = 9.5f;

// Touchdown alpha for `queryLabel` when the user presses on this view.
const CGFloat kTouchDownAlpha = 0.25f;

}  // namespace

@implementation QuerySuggestionConfig
@end

@interface QuerySuggestionView ()

@property(nonatomic, strong) UILabel* queryLabel;

@end

@implementation QuerySuggestionView

- (instancetype)initWithConfiguration:(QuerySuggestionConfig*)config {
  self = [self initWithFrame:CGRectZero];
  if (self) {
    UIImage* searchImage =
        DefaultSymbolTemplateWithPointSize(kSearchSymbol, 16.0);
    UIImageView* searchImageView =
        [[UIImageView alloc] initWithImage:searchImage];
    searchImageView.tintColor = [UIColor colorNamed:kGrey500Color];
    searchImageView.translatesAutoresizingMaskIntoConstraints = NO;
    self.queryLabel = [[UILabel alloc] init];
    self.queryLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    self.queryLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
    self.queryLabel.translatesAutoresizingMaskIntoConstraints = NO;
    self.queryLabel.numberOfLines = 2;
    self.queryLabel.adjustsFontForContentSizeCategory = YES;

    if (!config) {
      // If there is no config, then this is a placeholder tile.
      self.queryLabel.backgroundColor = [UIColor colorNamed:kGrey100Color];
    } else {
      _config = config;
      self.queryLabel.text = config.query;
      self.accessibilityLabel = config.query;
      self.accessibilityTraits |= UIAccessibilityTraitLink;
      self.isAccessibilityElement = YES;
    }

    [self addSubview:searchImageView];
    [self addSubview:self.queryLabel];

    [NSLayoutConstraint activateConstraints:@[
      [searchImageView.widthAnchor
          constraintLessThanOrEqualToConstant:kQueryImageMaxWidth],
      [searchImageView.widthAnchor
          constraintEqualToAnchor:searchImageView.heightAnchor],
      [searchImageView.centerYAnchor
          constraintEqualToAnchor:self.centerYAnchor],
      [searchImageView.leadingAnchor
          constraintEqualToAnchor:self.leadingAnchor],
      [self.queryLabel.leadingAnchor
          constraintEqualToAnchor:searchImageView.trailingAnchor
                         constant:kQueryImageToLabelHorizontalSpacing],
      [self.queryLabel.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor],
      [self.heightAnchor constraintEqualToConstant:kViewHeightAnchor],
      [self.widthAnchor constraintEqualToConstant:kViewWidthAnchor]
    ]];
    if (config) {
      // Let label take up all vertical space for two-line query text.
      [NSLayoutConstraint activateConstraints:@[
        [self.queryLabel.topAnchor constraintEqualToAnchor:self.topAnchor],
        [self.queryLabel.bottomAnchor constraintEqualToAnchor:self.bottomAnchor]
      ]];
    } else {
      // Vertically center and set thin height to visualize text placeholder.
      [NSLayoutConstraint activateConstraints:@[
        [self.queryLabel.centerYAnchor
            constraintEqualToAnchor:self.centerYAnchor],
        [self.queryLabel.heightAnchor
            constraintEqualToConstant:kQueryLabelHeightAnchor],
        [self.queryLabel.trailingAnchor
            constraintEqualToAnchor:self.trailingAnchor]
      ]];
    }
  }
  return self;
}

- (void)addBottomSeparator {
  UIView* grayLine = [[UIView alloc] init];
  grayLine.backgroundColor = [UIColor colorNamed:kSeparatorColor];
  grayLine.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:grayLine];

  id<LayoutGuideProvider> safeArea = self.safeAreaLayoutGuide;
  [NSLayoutConstraint activateConstraints:@[
    // Vertical constraints.
    [grayLine.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
    [grayLine.heightAnchor constraintEqualToConstant:1],
    // Horizontal constraints.
    [grayLine.leadingAnchor constraintEqualToAnchor:safeArea.leadingAnchor],
    [grayLine.trailingAnchor constraintEqualToAnchor:safeArea.trailingAnchor],
  ]];
}

- (void)setHighlighted:(BOOL)highlighted {
  if (highlighted) {
    self.queryLabel.alpha = kTouchDownAlpha;
  } else {
    self.queryLabel.alpha = 1.0;
  }
}

#pragma mark - UIResponder

- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
  [self setHighlighted:YES];
}

- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
  [self setHighlighted:NO];
}

- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
  [self setHighlighted:NO];
}

@end
