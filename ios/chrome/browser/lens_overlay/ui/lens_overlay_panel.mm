// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_panel.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// The width of the border outline that surrounds the results page.
const CGFloat kSidePannelOutlineBorderWidth = 1.0;

// The corner radius of the outline that surrounds the results page.
const CGFloat kSidePannelOutlineCornerRadius = 13.0;

// The lateral inset ammount of the border outlining the results page.
const CGFloat kSidePannelOutlineLateralInset = 8.0;

// The bottom inset ammount of the border outlining the results page.
const CGFloat kSidePannelOutlineBottomInset = 8.0;

}  // namespace

@implementation LensOverlayPanel {
  // The content view being presented in the side panel.
  // It is not intended for the ovelay to own the UI it presents.
  __weak UIViewController* _contentViewController;
  // Whether to inset the content or not.
  BOOL _insetContent;

  // The outline border of the results page.
  UIView* _borderView;
}

- (instancetype)initWithContent:(UIViewController*)contentViewController
                   insetContent:(BOOL)insetContent {
  self = [super init];
  if (self) {
    _contentViewController = contentViewController;
    _insetContent = insetContent;
  }

  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  // To ensure the elements within this side panel adapt properly to its limited
  // width, explicitly set its horizontal size class to compact.
  self.traitOverrides.horizontalSizeClass = UIUserInterfaceSizeClassCompact;
  _borderView = [self createBorderView];
  [self.view addSubview:_borderView];
  AddSameConstraintsWithInsets(_borderView, self.view,
                               [self insetsForInnerOutline]);
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];

  if (_contentViewController) {
    [self addChildViewController:_contentViewController];
    //    _contentViewController.view.userInteractionEnabled = NO;
    _contentViewController.view.translatesAutoresizingMaskIntoConstraints = NO;
    [_borderView addSubview:_contentViewController.view];
    [_contentViewController didMoveToParentViewController:self];

    AddSameConstraints(_contentViewController.view, _borderView);
  }
}

- (void)viewWillDisappear:(BOOL)animated {
  [_contentViewController removeFromParentViewController];
  [_contentViewController.view removeFromSuperview];
  [super viewWillDisappear:animated];
}

#pragma mark - Private

// The ammount of insets for the outline relative to the bounds.
- (NSDirectionalEdgeInsets)insetsForInnerOutline {
  if (!_insetContent) {
    return NSDirectionalEdgeInsetsZero;
  }
  return NSDirectionalEdgeInsetsMake(0, kSidePannelOutlineLateralInset,
                                     kSidePannelOutlineBottomInset,
                                     kSidePannelOutlineLateralInset);
}

// Creates a new border outline view.
- (UIView*)createBorderView {
  UIView* borderView = [[UIView alloc] init];
  borderView.translatesAutoresizingMaskIntoConstraints = NO;
  borderView.layer.cornerRadius = kSidePannelOutlineCornerRadius;
  borderView.clipsToBounds = YES;

  if (_insetContent) {
    borderView.layer.borderWidth = kSidePannelOutlineBorderWidth;
    borderView.layer.borderColor = [UIColor colorNamed:kGrey200Color].CGColor;
  }

  return borderView;
}

@end
