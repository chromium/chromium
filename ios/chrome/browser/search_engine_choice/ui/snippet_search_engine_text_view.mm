// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/search_engine_choice/ui/snippet_search_engine_text_view.h"

#import "base/check.h"
#import "base/check_op.h"
#import "ios/chrome/browser/search_engine_choice/ui/constants.h"
#import "ios/chrome/browser/search_engine_choice/ui/expandable_label_view.h"
#import "ios/chrome/browser/search_engine_choice/ui/search_engine_current_default_pill_view.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {

// Margin between the name and snippet labels when
// `SearchEngineCurrentDefaultState::kNone`.
constexpr CGFloat kTextVerticalMarginWithCurrentDefaultNone = 2;
// Margin between the name and snippet labels when
// `SearchEngineCurrentDefaultState::kOtherIsDefault` or
// `SearchEngineCurrentDefaultState::kIsDefault`.
constexpr CGFloat kTextVerticalMarginWithCurrentDefault = 0;

}  // namespace

@implementation SnippetSearchEngineTextView {
  // Current default view. The view exists only for
  // SearchEngineCurrentDefaultState::kOtherIsDefault and for
  // SearchEngineCurrentDefaultState::kIsDefault.
  SearchEngineCurrentDefaultPillView* _currentDefaultPillView;
  UILabel* _searchEngineName;
  ExpandableLabelView* _expandableSnippetView;
  // Layout guide to compute the height of self.
  UILayoutGuide* _heightLayoutGuide;
  // Layout guide used to verticaly center all views inside self.
  UILayoutGuide* _verticalTextLayoutGuide;
  // Layout guide between the top of the first view and the bottom of the
  // one line bottom of `_expandableSnippetView`.
  UILayoutGuide* _oneLineVerticalLayoutGuide;
}

- (instancetype)initWithCurrentDefaultState:
    (SearchEngineCurrentDefaultState)currentDefaultState {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _heightLayoutGuide = [[UILayoutGuide alloc] init];
    [self addLayoutGuide:_heightLayoutGuide];
    _verticalTextLayoutGuide = [[UILayoutGuide alloc] init];
    [self addLayoutGuide:_verticalTextLayoutGuide];
    _oneLineVerticalLayoutGuide = [[UILayoutGuide alloc] init];
    [self addLayoutGuide:_oneLineVerticalLayoutGuide];

    _searchEngineName = [[UILabel alloc] init];
    _searchEngineName.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_searchEngineName];
    _expandableSnippetView = [[ExpandableLabelView alloc] init];
    _expandableSnippetView.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_expandableSnippetView];
    if (currentDefaultState != SearchEngineCurrentDefaultState::kNone) {
      _currentDefaultPillView =
          [[SearchEngineCurrentDefaultPillView alloc] init];
      _currentDefaultPillView.translatesAutoresizingMaskIntoConstraints = NO;
      _currentDefaultPillView.alpha =
          (currentDefaultState ==
           SearchEngineCurrentDefaultState::kOtherIsDefault)
              ? 0
              : 1;
      [self addSubview:_currentDefaultPillView];
    }
    if (_currentDefaultPillView) {
      [NSLayoutConstraint activateConstraints:@[
        [_currentDefaultPillView.topAnchor
            constraintEqualToAnchor:_heightLayoutGuide.topAnchor],
        [_currentDefaultPillView.leadingAnchor
            constraintEqualToAnchor:self.leadingAnchor],
        [_currentDefaultPillView.trailingAnchor
            constraintLessThanOrEqualToAnchor:self.trailingAnchor],
        [_searchEngineName.topAnchor
            constraintEqualToAnchor:_currentDefaultPillView.bottomAnchor],
      ]];
    } else {
      [_searchEngineName.topAnchor
          constraintEqualToAnchor:_heightLayoutGuide.topAnchor]
          .active = YES;
    }
    [_heightLayoutGuide.bottomAnchor
        constraintEqualToAnchor:_expandableSnippetView.bottomAnchor]
        .active = YES;
    UIView* firstVisibleLabel = nil;
    CGFloat textVerticalMargin = -1;
    switch (currentDefaultState) {
      case SearchEngineCurrentDefaultState::kNone:
        firstVisibleLabel = _searchEngineName;
        textVerticalMargin = kTextVerticalMarginWithCurrentDefaultNone;
        break;
      case SearchEngineCurrentDefaultState::kOtherIsDefault:
        firstVisibleLabel = _searchEngineName;
        textVerticalMargin = kTextVerticalMarginWithCurrentDefault;
        break;
      case SearchEngineCurrentDefaultState::kIsDefault:
        firstVisibleLabel = _currentDefaultPillView;
        textVerticalMargin = kTextVerticalMarginWithCurrentDefault;
        break;
    }
    CHECK(firstVisibleLabel);
    CHECK_NE(textVerticalMargin, -1);

    [NSLayoutConstraint activateConstraints:@[
      [_oneLineVerticalLayoutGuide.bottomAnchor
          constraintEqualToAnchor:_expandableSnippetView.oneLineBottomAnchor],
      [_oneLineVerticalLayoutGuide.topAnchor
          constraintEqualToAnchor:firstVisibleLabel.topAnchor],

      [self.heightAnchor
          constraintEqualToAnchor:_heightLayoutGuide.heightAnchor],

      [_searchEngineName.leadingAnchor
          constraintEqualToAnchor:self.leadingAnchor],
      [_searchEngineName.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor],

      [_expandableSnippetView.topAnchor
          constraintEqualToAnchor:_searchEngineName.bottomAnchor
                         constant:textVerticalMargin],
      [_expandableSnippetView.leadingAnchor
          constraintEqualToAnchor:self.leadingAnchor],
      [_expandableSnippetView.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor],
      [_expandableSnippetView.bottomAnchor
          constraintEqualToAnchor:_verticalTextLayoutGuide.bottomAnchor],

      [_verticalTextLayoutGuide.topAnchor
          constraintEqualToAnchor:firstVisibleLabel.topAnchor],
      [_verticalTextLayoutGuide.centerYAnchor
          constraintEqualToAnchor:self.centerYAnchor],
    ]];
  }
  return self;
}

#pragma mark - Properties

- (NSString*)searchEngineName {
  return _searchEngineName.text;
}

- (void)setSearchEngineName:(NSString*)searchEngineName {
  _searchEngineName.text = searchEngineName;
}

- (NSString*)snippetText {
  return _expandableSnippetView.text;
}

- (void)setSnippetText:(NSString*)snippetText {
  _expandableSnippetView.text = snippetText;
}

- (BOOL)isSnippetExpandable {
  return _expandableSnippetView.isExpandable;
}

- (BOOL)snippetExpanded {
  return _expandableSnippetView.expanded;
}

- (void)setSnippetExpanded:(BOOL)expanded {
  _expandableSnippetView.expanded = expanded;
}

- (NSLayoutYAxisAnchor*)oneLineCenterVerticalLayoutGuide {
  return _oneLineVerticalLayoutGuide.centerYAnchor;
}

- (NSLayoutDimension*)searchEngineNameLabelHeight {
  return _searchEngineName.heightAnchor;
}

@end
