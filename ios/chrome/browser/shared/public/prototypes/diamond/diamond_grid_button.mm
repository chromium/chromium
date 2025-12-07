// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/public/prototypes/diamond/diamond_grid_button.h"

#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/public/prototypes/diamond/utils.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// The duration of the animation to update label color.
constexpr CGFloat kAnimationDuration = 0.25;

// The font size of the label.
constexpr CGFloat kFontSize = 11;

}  // namespace

@interface DiamondGridButton () <WebStateListObserving>
@end

@implementation DiamondGridButton {
  raw_ptr<WebStateList> _webStateList;
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;
  std::unique_ptr<
      base::ScopedObservation<WebStateList, WebStateListObserverBridge>>
      _webStateListObservation;

  UIImageView* _symbolView;

  UILabel* _tabCountLabel;
}

- (void)setup {
  // The app starts in the TabGrid.
  [self didEnterTabGrid];

  NSNotificationCenter* notificationCenter =
      [NSNotificationCenter defaultCenter];
  [notificationCenter addObserver:self
                         selector:@selector(didEnterTabGrid)
                             name:kDiamondEnterTabGridNotification
                           object:nil];
  [notificationCenter addObserver:self
                         selector:@selector(didLeaveTabGrid)
                             name:kDiamondLeaveTabGridNotification
                           object:nil];

  _symbolView = [[UIImageView alloc] init];
  _symbolView.translatesAutoresizingMaskIntoConstraints = NO;
  _symbolView.tintColor = UIColor.whiteColor;
  [self addSubview:_symbolView];
  AddSameCenterConstraints(_symbolView, self.imageView);

  _tabCountLabel = [[UILabel alloc] init];
  _tabCountLabel.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:_tabCountLabel];
  AddSameCenterConstraints(_tabCountLabel, self.imageView);
  _tabCountLabel.textColor = UIColor.whiteColor;
}

- (void)configureWithWebStateList:(WebStateList*)webStateList {
  _webStateList = webStateList;
  _webStateListObservation.reset();
  _webStateListObserver.reset();

  _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
  _webStateListObservation = std::make_unique<
      base::ScopedObservation<WebStateList, WebStateListObserverBridge>>(
      _webStateListObserver.get());
  _webStateListObservation->Observe(_webStateList);

  [self updateCount];
}

#pragma mark - WebStateListObserving

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  [self updateCount];
}

- (void)webStateListDestroyed:(WebStateList*)webStateList {
  _webStateListObservation.reset();
  _webStateListObserver.reset();
}

#pragma mark - Private

// Updates the count of tabs.
- (void)updateCount {
  _tabCountLabel.attributedText =
      TextForTabCount(_webStateList->count(), kFontSize);
}

// Callback when exiting the grid.
- (void)didLeaveTabGrid {
  [_symbolView setSymbolImage:GetDefaultAppBarSymbol(kAppSymbol)
        withContentTransition:[NSSymbolReplaceContentTransition
                                  replaceOffUpTransition]];
  UILabel* label = _tabCountLabel;
  [UIView transitionWithView:label
                    duration:kAnimationDuration
                     options:UIViewAnimationOptionTransitionCrossDissolve
                  animations:^{
                    label.textColor = UIColor.whiteColor;
                  }
                  completion:nil];
}

// Callback when entering the grid.
- (void)didEnterTabGrid {
  [_symbolView setSymbolImage:GetDefaultAppBarSymbol(kAppFillSymbol)
        withContentTransition:[NSSymbolReplaceContentTransition
                                  replaceOffUpTransition]];
  UILabel* label = _tabCountLabel;
  [UIView transitionWithView:label
                    duration:kAnimationDuration
                     options:UIViewAnimationOptionTransitionCrossDissolve
                  animations:^{
                    label.textColor = UIColor.blackColor;
                  }
                  completion:nil];
}

@end
