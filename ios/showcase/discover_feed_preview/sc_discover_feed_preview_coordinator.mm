// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/discover_feed_preview/sc_discover_feed_preview_coordinator.h"

#import "ios/chrome/browser/ui/context_menu/link_preview/link_preview_view_controller.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

// The wrapper view controller that centers the preview so that the preview
// won't be covered by the top bar of the showcase navigation view controller.
@interface PreviewContainerViewController : UIViewController
@end

@implementation PreviewContainerViewController
- (void)viewDidLoad {
  [super viewDidLoad];

  UIStackView* containerStack = [[UIStackView alloc] init];
  containerStack.translatesAutoresizingMaskIntoConstraints = NO;
  containerStack.axis = UILayoutConstraintAxisVertical;

  LinkPreviewViewController* previewViewController =
      [[LinkPreviewViewController alloc] initWithView:[[UIView alloc] init]
                                               origin:@"test.url"];

  [self addChildViewController:previewViewController];
  [containerStack addArrangedSubview:previewViewController.view];
  [self didMoveToParentViewController:previewViewController];

  [self.view addSubview:containerStack];
  AddSameCenterConstraints(containerStack, self.view);
  [NSLayoutConstraint activateConstraints:@[
    [containerStack.widthAnchor constraintEqualToConstant:300],
    [containerStack.heightAnchor constraintGreaterThanOrEqualToConstant:100]
  ]];

  UIView* containerView = self.view;
  containerView.backgroundColor = [UIColor whiteColor];

  // Set loading state to yes so the progress bar will be shown.
  [previewViewController setLoadingState:YES];
}

@end

@implementation SCLinkPreviewCoordinator

@synthesize baseViewController;

- (void)start {
  [self.baseViewController
      pushViewController:[[PreviewContainerViewController alloc] init]
                animated:YES];
}

@end
