// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/translate/translate_infobar_language_tab_strip_view.h"

#import "ios/chrome/browser/ui/translate/translate_infobar_language_tab_strip_view_delegate.h"
#import "ios/chrome/browser/ui/translate/translate_infobar_language_tab_view_delegate.h"
#import "ios/chrome/browser/ui/translate/translate_infobar_view.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// The point where the gradient on the leading edge of the scroll view stops.
CGFloat kScrollViewLeadingGradientStop = 0.025;

// The point where the gradient on the trailing edge of the scroll view starts.
CGFloat kScrollViewTrailingGradientStart = 0.975;

}  // namespace

@interface TranslateInfobarLanguageTabStripView () <
    TranslateInfobarLanguageTabViewDelegate,
    UIScrollViewDelegate>

// Scroll view holding the source and the target language tab views.
@property(nonatomic, weak) UIScrollView* languagesScrollView;

// Used for the fading effect on the edges of the scroll view.
@property(nonatomic, weak) CAGradientLayer* gradientLayer;

// Source language tab view.
@property(nonatomic, weak) TranslateInfobarLanguageTabView* sourceLanguageTab;

// Target language tab view.
@property(nonatomic, weak) TranslateInfobarLanguageTabView* targetLanguageTab;

@end

@implementation TranslateInfobarLanguageTabStripView

#pragma mark - UIView

- (void)willMoveToSuperview:(UIView*)newSuperview {
  // Create and add subviews the first time this moves to a superview.
  if (newSuperview && !self.subviews.count) {
    [self setupSubviews];
  }
  [super willMoveToSuperview:newSuperview];
}

- (void)layoutSubviews {
  [super layoutSubviews];

  // Scroll the scroll view so that the selected language tab is visible.
  if (self.sourceLanguageTabState ==
      TranslateInfobarLanguageTabViewStateSelected) {
    [self.languagesScrollView scrollRectToVisible:self.sourceLanguageTab.frame
                                         animated:YES];
  } else if (self.targetLanguageTabState ==
             TranslateInfobarLanguageTabViewStateSelected) {
    [self.languagesScrollView scrollRectToVisible:self.targetLanguageTab.frame
                                         animated:YES];
  }

  [self updateLanguageScrollViewGradient];
}

#pragma mark - Properties

- (void)setSourceLanguage:(NSString*)sourceLanguage {
  _sourceLanguage = sourceLanguage;
  self.sourceLanguageTab.title = sourceLanguage;

  [self updateLanguageScrollViewGradient];
}

- (void)setTargetLanguage:(NSString*)targetLanguage {
  _targetLanguage = targetLanguage;
  self.targetLanguageTab.title = targetLanguage;

  [self updateLanguageScrollViewGradient];
}

- (void)setSourceLanguageTabState:
    (TranslateInfobarLanguageTabViewState)sourceLanguageTabState {
  _sourceLanguageTabState = sourceLanguageTabState;

  self.sourceLanguageTab.state = sourceLanguageTabState;
}

- (void)setTargetLanguageTabState:
    (TranslateInfobarLanguageTabViewState)targetLanguageTabState {
  _targetLanguageTabState = targetLanguageTabState;

  self.targetLanguageTab.state = targetLanguageTabState;
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  [self updateLanguageScrollViewGradient];
}

#pragma mark - TranslateInfobarLanguageTabViewDelegate

- (void)translateInfobarTabViewDidTap:(TranslateInfobarLanguageTabView*)sender {
  [self.languagesScrollView scrollRectToVisible:sender.frame animated:YES];

  if (sender == self.targetLanguageTab) {
    [self.delegate translateInfobarTabStripViewDidTapTargetLangugage:self];
  } else {
    [self.delegate translateInfobarTabStripViewDidTapSourceLangugage:self];
  }
}

#pragma mark - Private

- (void)setupSubviews {
  UIScrollView* languagesScrollView = [[UIScrollView alloc] init];
  self.languagesScrollView = languagesScrollView;
  self.languagesScrollView.translatesAutoresizingMaskIntoConstraints = NO;
  self.languagesScrollView.showsVerticalScrollIndicator = NO;
  self.languagesScrollView.showsHorizontalScrollIndicator = NO;
  self.languagesScrollView.bounces = YES;
  self.languagesScrollView.delegate = self;
  [self addSubview:self.languagesScrollView];

  self.gradientLayer = [CAGradientLayer layer];
  self.gradientLayer.colors = @[
    (id)UIColor.clearColor.CGColor, (id)UIColor.whiteColor.CGColor,
    (id)UIColor.whiteColor.CGColor, (id)UIColor.clearColor.CGColor
  ];
  // The following two lines make the gradient horizontal.
  self.gradientLayer.startPoint = CGPointMake(0.0, 0.5);
  self.gradientLayer.endPoint = CGPointMake(1.0, 0.5);
  self.languagesScrollView.layer.mask = self.gradientLayer;

  TranslateInfobarLanguageTabView* sourceLanguageTab =
      [[TranslateInfobarLanguageTabView alloc] init];
  self.sourceLanguageTab = sourceLanguageTab;
  self.sourceLanguageTab.translatesAutoresizingMaskIntoConstraints = NO;
  self.sourceLanguageTab.title = self.sourceLanguage;
  self.sourceLanguageTab.state = self.sourceLanguageTabState;
  self.sourceLanguageTab.delegate = self;
  [self.languagesScrollView addSubview:self.sourceLanguageTab];

  TranslateInfobarLanguageTabView* targetLanguageTab =
      [[TranslateInfobarLanguageTabView alloc] init];
  self.targetLanguageTab = targetLanguageTab;
  self.targetLanguageTab.translatesAutoresizingMaskIntoConstraints = NO;
  self.targetLanguageTab.title = self.targetLanguage;
  self.targetLanguageTab.state = self.targetLanguageTabState;
  self.targetLanguageTab.delegate = self;
  [self.languagesScrollView addSubview:self.targetLanguageTab];

  ApplyVisualConstraintsWithMetrics(
      @[
        @"H:|[scrollView]|",
        @"H:|[sourceLanguage][targetLanguage]|",
        @"V:|[scrollView(infobarHeight)]|",
        @"V:|[sourceLanguage(infobarHeight)]|",
        @"V:|[targetLanguage(infobarHeight)]|",
      ],
      @{
        @"scrollView" : self.languagesScrollView,
        @"sourceLanguage" : self.sourceLanguageTab,
        @"targetLanguage" : self.targetLanguageTab,
      },
      @{
        @"infobarHeight" : @(kInfobarHeight),
      });
}

// Updates the scroll view's gradient locations based on whether or not the
// scroll view has content under the leading fold, trailing fold, or both.
- (void)updateLanguageScrollViewGradient {
  // Lay out the scroll view's subviews if needed.
  [self.languagesScrollView layoutIfNeeded];

  CGFloat scrollViewWidth = CGRectGetWidth(self.languagesScrollView.frame);
  CGFloat contentWidth = self.languagesScrollView.contentSize.width;
  CGFloat scrollOffset = self.languagesScrollView.contentOffset.x;
  BOOL hasContentUnderLeadingFold = (scrollOffset > 0);
  BOOL hasContentUnderTrailingFold =
      (scrollOffset + scrollViewWidth < contentWidth);
  self.gradientLayer.locations = @[
    @0, hasContentUnderLeadingFold ? @(kScrollViewLeadingGradientStop) : @0,
    hasContentUnderTrailingFold ? @(kScrollViewTrailingGradientStart) : @1, @1
  ];

  // Disable animation; or updating the gradient's frame will be animated.
  [CATransaction begin];
  [CATransaction setDisableActions:YES];

  // Update the gradient's frame.
  self.gradientLayer.frame = CGRectMake(
      scrollOffset, 0, CGRectGetWidth(self.languagesScrollView.bounds),
      CGRectGetHeight(self.languagesScrollView.bounds));

  // Reenable animation.
  [CATransaction commit];
}

@end
