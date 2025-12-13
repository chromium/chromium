// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/incognito/incognito_grid_view_controller.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/features.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_commands.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_constants.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_view.h"
#import "ios/chrome/browser/shared/public/commands/tab_grid_commands.h"
#import "ios/chrome/browser/tab_switcher/tab_grid/base_grid/ui/base_grid_view_controller+subclassing.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/grid_commands.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/incognito_grid_commands.h"
#import "ios/chrome/common/material_timing.h"
#import "ios/chrome/common/ui/animations/radial_wipe_animation.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@implementation IncognitoGridViewController {
  // A view to obscure incognito content when the user isn't authorized to
  // see it.
  IncognitoReauthView* _blockingView;

  // A view to obscure incognito tabs to hide empty state strings during close
  // incognito animations.
  UIView* _blackBackgroundView;

  // The object responsible for animating the tabs closure.
  RadialWipeAnimation* _radialWipeAnimation;
}

#pragma mark - Parent's functions

- (UIContextMenuConfiguration*)collectionView:(UICollectionView*)collectionView
    contextMenuConfigurationForItemsAtIndexPaths:
        (NSArray<NSIndexPath*>*)indexPaths
                                           point:(CGPoint)point {
  // Don't allow long-press previews when the incognito reauth view is blocking
  // the content.
  if (self.contentNeedsAuthentication) {
    return nil;
  }

  return [super collectionView:collectionView
      contextMenuConfigurationForItemsAtIndexPaths:indexPaths
                                             point:point];
}

- (NSArray<UIDragItem*>*)collectionView:(UICollectionView*)collectionView
           itemsForBeginningDragSession:(id<UIDragSession>)session
                            atIndexPath:(NSIndexPath*)indexPath {
  if (self.contentNeedsAuthentication) {
    // Don't support dragging items if the drag&drop handler is not set.
    return @[];
  }
  return [super collectionView:collectionView
      itemsForBeginningDragSession:session
                       atIndexPath:indexPath];
}

#pragma mark - IncognitoReauthConsumer

- (void)setItemsRequireAuthentication:(BOOL)require {
  self.contentNeedsAuthentication = require;

  if (require) {
    // If the blocking view is already installed, early return.
    if ([_blockingView superview]) {
      return;
    }

    if (!_blockingView) {
      _blockingView = [self createBlockingView];
    }

    [self.view addSubview:_blockingView];
    _blockingView.hidden = NO;
    _blockingView.alpha = 1;
    AddSameConstraints(self.collectionView.frameLayoutGuide, _blockingView);

    // Dismiss modals related to this view controller.
    [self.incognitoGridHandler dismissIncognitoGridModals];
  } else {
    __weak IncognitoGridViewController* weakSelf = self;
    [UIView animateWithDuration:kMaterialDuration1
        animations:^{
          [weakSelf hideBlockingView];
        }
        completion:^(BOOL finished) {
          [weakSelf blockingViewDidHide:finished];
        }];
  }
}

- (void)setItemsRequireAuthentication:(BOOL)require
                withPrimaryButtonText:(NSString*)text
                   accessibilityLabel:(NSString*)accessibilityLabel {
  [self setItemsRequireAuthentication:require];
  if (require) {
    [_blockingView setAuthenticateButtonText:text
                          accessibilityLabel:accessibilityLabel];
  } else {
    // No primary button text or accessibility label should be set when
    // authentication is not required.
    CHECK(!text);
    CHECK(!accessibilityLabel);
  }
}

#pragma mark - Private

// Creates and configures a IncognitoReauthView.
- (IncognitoReauthView*)createBlockingView {
  IncognitoReauthView* blockingView = [[IncognitoReauthView alloc] init];
  blockingView.translatesAutoresizingMaskIntoConstraints = NO;
  blockingView.layer.zPosition = FLT_MAX;

  [blockingView.authenticateButton
             addTarget:self.reauthHandler
                action:@selector(authenticateIncognitoContent)
      forControlEvents:UIControlEventTouchUpInside];

  if (IsIOSSoftLockEnabled()) {
    id<GridCommands> gridHandler = self.gridHandler;
    __weak IncognitoGridViewController* weakSelf = self;
    [blockingView.secondaryButton
               addAction:[UIAction actionWithHandler:^(UIAction* action) {
                 base::UmaHistogramEnumeration(
                     kIncognitoLockOverlayInteractionHistogram,
                     IncognitoLockOverlayInteraction::
                         kCloseIncognitoTabsButtonClicked);
                 base::RecordAction(base::UserMetricsAction(
                     "IOS.IncognitoLock.Overlay.CloseIncognitoTabs"));
                 [gridHandler closeAllItems];
                 [weakSelf animateClosure];
               }]
        forControlEvents:UIControlEventTouchUpInside];
  } else {
    // No need to show tab switcher button when already in the tab switcher.
    blockingView.secondaryButton.hidden = YES;
    // Hide the logo.
    blockingView.logoView.hidden = YES;
  }

  return blockingView;
}

// Sets properties that should be animated to remove the blocking view.
- (void)hideBlockingView {
  _blockingView.alpha = 0;
}

// Cleans up after blocking view animation completed.
- (void)blockingViewDidHide:(BOOL)finished {
  if (self.contentNeedsAuthentication) {
    _blockingView.hidden = NO;
    _blockingView.alpha = 1;
  } else {
    [_blockingView removeFromSuperview];
    [_blackBackgroundView removeFromSuperview];
  }
}

// Animates closing Incognito tabs by with a wipe effect on the blocking view.
- (void)animateClosure {
  UIWindow* window = self.view.window;
  [window setUserInteractionEnabled:NO];
  if (!_blackBackgroundView) {
    _blackBackgroundView = [[UIView alloc] init];
    _blackBackgroundView.backgroundColor = UIColor.blackColor;
    _blackBackgroundView.translatesAutoresizingMaskIntoConstraints = NO;
    _blackBackgroundView.layer.zPosition = FLT_MAX;
  }
  [self.view insertSubview:_blackBackgroundView belowSubview:_blockingView];
  AddSameConstraints(self.collectionView.frameLayoutGuide,
                     _blackBackgroundView);

  NSMutableArray<UIView*>* targetViews =
      [[NSMutableArray alloc] initWithObjects:_blockingView, nil];
  _radialWipeAnimation =
      [[RadialWipeAnimation alloc] initWithWindow:window
                                      targetViews:targetViews];
  _radialWipeAnimation.type = RadialWipeAnimationType::kHideTarget;

  __weak IncognitoGridViewController* weakSelf = self;
  [_radialWipeAnimation animateWithCompletion:^{
    [weakSelf onTabsClosureAnimationCompleted:window];
  }];
}

// After animation is completed, update reauth state, switch to normal tab grid,
// clean up animation object, and re-enable user interaction.
- (void)onTabsClosureAnimationCompleted:(UIWindow*)window {
  [self.reauthHandler manualAuthenticationOverride];
  [self.tabGridHandler showPage:TabGridPageRegularTabs animated:YES];
  [window setUserInteractionEnabled:YES];
  _radialWipeAnimation = nil;
}

@end
