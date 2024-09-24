// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/incognito/incognito_grid_view_controller.h"

#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_commands.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_view.h"
#import "ios/chrome/browser/tabs/model/inactive_tabs/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/base_grid_view_controller+subclassing.h"
#import "ios/chrome/common/material_timing.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@implementation IncognitoGridViewController {
  // A view to obscure incognito content when the user isn't authorized to
  // see it.
  IncognitoReauthView* _blockingView;
}

#pragma mark - Parent's functions

// Returns a configured header for the given index path.
- (UICollectionReusableView*)headerForSectionAtIndexPath:
    (NSIndexPath*)indexPath {
  if (IsInactiveTabButtonRefactoringEnabled()) {
    // With the refactoring, the base class does the right thing.
    return [super headerForSectionAtIndexPath:indexPath];
  }
  if (self.mode == TabGridMode::kNormal) {
    return nil;
  }
  return [super headerForSectionAtIndexPath:indexPath];
}

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
    if (!_blockingView) {
      _blockingView = [[IncognitoReauthView alloc] init];
      _blockingView.translatesAutoresizingMaskIntoConstraints = NO;
      _blockingView.layer.zPosition = FLT_MAX;
      // No need to show tab switcher button when already in the tab switcher.
      _blockingView.tabSwitcherButton.hidden = YES;
      // Hide the logo.
      _blockingView.logoView.hidden = YES;

      [_blockingView.authenticateButton
                 addTarget:self.reauthHandler
                    action:@selector(authenticateIncognitoContent)
          forControlEvents:UIControlEventTouchUpInside];
    }

    [self.view addSubview:_blockingView];
    _blockingView.alpha = 1;
    AddSameConstraints(self.collectionView.frameLayoutGuide, _blockingView);
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

#pragma mark - Private

// Sets properties that should be animated to remove the blocking view.
- (void)hideBlockingView {
  _blockingView.alpha = 0;
}

// Cleans up after blocking view animation completed.
- (void)blockingViewDidHide:(BOOL)finished {
  if (self.contentNeedsAuthentication) {
    _blockingView.alpha = 1;
  } else {
    [_blockingView removeFromSuperview];
  }
}

@end
