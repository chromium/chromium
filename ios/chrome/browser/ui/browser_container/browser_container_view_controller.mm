// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_container/browser_container_view_controller.h"

#import "base/check.h"
#import "ios/chrome/browser/ui/link_to_text/link_to_text_mediator.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

  [self addLinkToTextInEditMenu];
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
    // TODO(crbug.com/801165): On iOS10, UIDocumentMenuViewController and
    // WKFileUploadPanel somehow combine to call dismiss twice instead of once.
    // The second call would dismiss the BrowserContainerViewController itself,
    // so look for that case and return early.
    //
    // TODO(crbug.com/852367): A similar bug exists on all iOS versions with
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

#pragma mark - Link to Text methods

- (void)addLinkToTextInEditMenu {
  if (!base::FeatureList::IsEnabled(kSharedHighlightingIOS)) {
    return;
  }

  NSString* title = l10n_util::GetNSString(IDS_IOS_SHARE_LINK_TO_TEXT);
  UIMenuItem* menuItem =
      [[UIMenuItem alloc] initWithTitle:title action:@selector(linkToText:)];
  RegisterEditMenuItem(menuItem);
}

- (BOOL)canPerformAction:(SEL)action withSender:(id)sender {
  if (action == @selector(linkToText:) && self.linkToTextDelegate) {
    return [self.linkToTextDelegate shouldOfferLinkToText];
  }
  return [super canPerformAction:action withSender:sender];
}

- (void)linkToText:(UIMenuItem*)item {
  DCHECK(base::FeatureList::IsEnabled(kSharedHighlightingIOS));
  DCHECK(self.linkToTextDelegate);
  [self.linkToTextDelegate handleLinkToTextSelection];
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
