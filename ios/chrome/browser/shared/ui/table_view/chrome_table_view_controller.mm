// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_empty_table_view_background.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_empty_view.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_loading_view.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

@interface ChromeTableViewController ()

// The view displayed by [self addEmptyTableViewWith...:].
@property(nonatomic, strong) UIView<ChromeEmptyTableViewBackground>* emptyView;

@end

@implementation ChromeTableViewController

#pragma mark - UIViewController

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
  // Make sure the large title appears after rotating back to portrait
  // mode.
  [coordinator
      animateAlongsideTransition:^(
          id<UIViewControllerTransitionCoordinatorContext> context) {
        [self.navigationController.navigationBar sizeToFit];
      }
                      completion:nil];
}

- (void)viewSafeAreaInsetsDidChange {
  [super viewSafeAreaInsetsDidChange];
  // The safe area insets aren't propagated to the inner scroll view. Manually
  // set the content insets.
  [self updateEmptyViewInsets];
}

#pragma mark - Accessors

- (void)setEmptyView:(TableViewEmptyView*)emptyView {
  if (_emptyView == emptyView) {
    return;
  }
  _emptyView = emptyView;
  [self updateEmptyViewInsets];
  self.tableView.backgroundView = _emptyView;
}

#pragma mark - Public

- (void)addEmptyTableViewWithMessage:(NSString*)message image:(UIImage*)image {
  self.emptyView = [[TableViewEmptyView alloc] initWithFrame:self.view.bounds
                                                     message:message
                                                       image:image];
  self.emptyView.tintColor = [UIColor colorNamed:kPlaceholderImageTintColor];
}

#pragma mark - Private

// Sets the empty view's insets to the sum of the top offset and the safe area
// insets.
- (void)updateEmptyViewInsets {
  UIEdgeInsets safeAreaInsets = self.view.safeAreaInsets;
  _emptyView.scrollViewContentInsets =
      UIEdgeInsetsMake(safeAreaInsets.top, safeAreaInsets.left,
                       safeAreaInsets.bottom, safeAreaInsets.right);
}

@end
