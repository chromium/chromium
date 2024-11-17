// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_container/browser_container_view_controller.h"

#import "base/check.h"
#import "base/feature_list.h"
#import "base/notreached.h"
#import "ios/chrome/browser/link_to_text/ui_bundled/link_to_text_delegate.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/browser_container/browser_edit_menu_handler.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@interface BrowserContainerViewController ()
// Properties backing public setters.
@property(nonatomic, strong) UIView* contentView;
@property(nonatomic, strong) UIViewController* contentViewController;
// BrowserContainerConsumer backing properties.
@property(nonatomic, assign, getter=isContentBlocked) BOOL contentBlocked;
// The view inserted into the hierarchy when self.contentBlocked is set to YES.
@property(nonatomic, strong) UIView* contentBlockingView;
@end

@implementation BrowserContainerViewController

- (void)dealloc {
  DCHECK(![_contentView superview] || [_contentView superview] == self.view);
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];

  // OverlayContainerView should cover all subviews of BrowserContainerView. The
  // ScreenTime container must be above the WebContentArea overlay container.
  [self.view
      bringSubviewToFront:self.webContentsOverlayContainerViewController.view];
  [self.view bringSubviewToFront:self.screenTimeViewController.view];
}

- (void)dismissViewControllerAnimated:(BOOL)animated
                           completion:(void (^)())completion {
  if (!self.presentedViewController) {
    // TODO(crbug.com/41364311): On iOS10, UIDocumentMenuViewController and
    // WKFileUploadPanel somehow combine to call dismiss twice instead of once.
    // The second call would dismiss the BrowserContainerViewController itself,
    // so look for that case and return early.
    //
    // TODO(crbug.com/40580587): A similar bug exists on all iOS versions with
    // WKFileUploadPanel and UIDocumentPickerViewController. See also
    // https://crbug.com/811671.
    //
    // Return early whenever this method is invoked but no VC appears to be
    // presented.  These cases will always end up dismissing the
    // BrowserContainerViewController itself, which would put the app into an
    // unresponsive state.
    return;
  }
  [super dismissViewControllerAnimated:animated completion:completion];
}

- (void)buildMenuWithBuilder:(id<UIMenuBuilder>)builder {
  [super buildMenuWithBuilder:builder];

  DCHECK(self.browserEditMenuHandler);
  [self.browserEditMenuHandler buildMenuWithBuilder:builder];
}

#pragma mark - Public

- (void)setContentViewController:(UIViewController*)contentViewController {
  if (_contentViewController == contentViewController)
    return;

  [self removeOldContentViewController];
  _contentViewController = contentViewController;

  if (contentViewController) {
    [contentViewController willMoveToParentViewController:self];
    [self addChildViewController:contentViewController];
    [self.view insertSubview:contentViewController.view atIndex:0];
    if (_contentView) {
      [self.view insertSubview:contentViewController.view
                  aboveSubview:self.contentView];
    } else {
      [self.view insertSubview:contentViewController.view atIndex:0];
    }
    [contentViewController didMoveToParentViewController:self];
  }
}

- (void)setContentView:(UIView*)contentView {
  [self removeOldContentViewController];

  if (_contentView == contentView)
    return;

  [self removeOldContentView];
  _contentView = contentView;

  _contentView.clipsToBounds = YES;

  if (contentView)
    [self.view insertSubview:contentView atIndex:0];
}

- (void)setContentBlocked:(BOOL)contentBlocked {
  if (_contentBlocked == contentBlocked)
    return;
  if (_contentBlocked) {
    // If the content was previously blocked, remove the blocking view before
    // resetting to `contentBlocked`.
    [self.contentBlockingView removeFromSuperview];
    self.contentBlockingView = nil;
  }
  _contentBlocked = contentBlocked;
  if (_contentBlocked) {
    // Install the blocking view.
    self.contentBlockingView = [[UIView alloc] initWithFrame:CGRectZero];
    self.contentBlockingView.backgroundColor =
        [UIColor secondarySystemBackgroundColor];
    UIView* overlayContainerView =
        self.webContentsOverlayContainerViewController.view;
    if (overlayContainerView) {
      [self.view insertSubview:self.contentBlockingView
                  belowSubview:overlayContainerView];
    } else {
      [self.view addSubview:self.contentBlockingView];
    }
    self.contentBlockingView.translatesAutoresizingMaskIntoConstraints = NO;
    AddSameConstraints(self.contentBlockingView, self.view);
  }
}


#pragma mark - Private

// Unloads and nils any any previous content viewControllers if they exist.
- (void)removeOldContentViewController {
  if (_contentViewController) {
    [_contentViewController willMoveToParentViewController:nil];
    [_contentViewController.view removeFromSuperview];
    [_contentViewController removeFromParentViewController];
    _contentViewController = nil;
  }
}

// Unloads and nils any any previous content views if they exist.
- (void)removeOldContentView {
  if (_contentView) {
    DCHECK(![_contentView superview] || [_contentView superview] == self.view);
    [_contentView removeFromSuperview];
    _contentView = nil;
  }
}

@end
