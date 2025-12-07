// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/toolbars/tab_grid_toolbar_scrolling_background.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/toolbars/tab_grid_toolbar_background.h"

@implementation TabGridToolbarScrollingBackground {
  TabGridToolbarBackground* _incognitoTabsBackground;
  TabGridToolbarBackground* _regularTabsBackground;
  TabGridToolbarBackground* _tabGroupsBackground;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    self.pagingEnabled = YES;
    self.contentInsetAdjustmentBehavior =
        UIScrollViewContentInsetAdjustmentNever;
    // Explictly setting scrollability to false, users should not be able to
    // scroll on the toolbars to change pages.
    self.scrollEnabled = NO;

    _incognitoTabsBackground =
        [[TabGridToolbarBackground alloc] initWithFrame:self.frame];
    _incognitoTabsBackground.translatesAutoresizingMaskIntoConstraints = NO;

    _regularTabsBackground =
        [[TabGridToolbarBackground alloc] initWithFrame:self.frame];
    _regularTabsBackground.translatesAutoresizingMaskIntoConstraints = NO;

    _tabGroupsBackground =
        [[TabGridToolbarBackground alloc] initWithFrame:self.frame];
    _tabGroupsBackground.translatesAutoresizingMaskIntoConstraints = NO;

    UIStackView* gridsStack = [[UIStackView alloc] initWithArrangedSubviews:@[
      _incognitoTabsBackground, _regularTabsBackground, _tabGroupsBackground
    ]];
    gridsStack.translatesAutoresizingMaskIntoConstraints = NO;
    gridsStack.distribution = UIStackViewDistributionEqualSpacing;

    [self addSubview:gridsStack];
    [NSLayoutConstraint activateConstraints:@[
      [gridsStack.topAnchor constraintEqualToAnchor:self.topAnchor],
      [gridsStack.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
      [gridsStack.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
      [gridsStack.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
      [gridsStack.heightAnchor constraintEqualToAnchor:self.heightAnchor],

      [_incognitoTabsBackground.widthAnchor
          constraintEqualToAnchor:self.widthAnchor],

      [_regularTabsBackground.widthAnchor
          constraintEqualToAnchor:self.widthAnchor],

      [_tabGroupsBackground.widthAnchor
          constraintEqualToAnchor:self.widthAnchor]
    ]];
  }
  return self;
}

- (void)updateBackgroundsForPage:(TabGridPage)page
            scrolledToEdgeHidden:(BOOL)scrolledToEdge
    scrolledBackgroundViewHidden:(BOOL)scrolledBackgroundViewHidden {
  switch (page) {
    case TabGridPageIncognitoTabs:
      [_incognitoTabsBackground setScrolledOverContentBackgroundViewHidden:
                                    scrolledBackgroundViewHidden];
      [_incognitoTabsBackground
          setScrolledToEdgeBackgroundViewHidden:scrolledToEdge];
      break;
    case TabGridPageRegularTabs:
      [_regularTabsBackground setScrolledOverContentBackgroundViewHidden:
                                  scrolledBackgroundViewHidden];
      [_regularTabsBackground
          setScrolledToEdgeBackgroundViewHidden:scrolledToEdge];
      break;
    case TabGridPageTabGroups:
      [_tabGroupsBackground setScrolledOverContentBackgroundViewHidden:
                                scrolledBackgroundViewHidden];
      [_tabGroupsBackground
          setScrolledToEdgeBackgroundViewHidden:scrolledToEdge];
      break;
  }
}

- (void)hideIncognitoToolbarBackground:(BOOL)hidden {
  _incognitoTabsBackground.alpha = hidden ? 0 : 1;
}

@end
