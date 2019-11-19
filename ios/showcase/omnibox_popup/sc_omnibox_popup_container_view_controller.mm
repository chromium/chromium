// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/omnibox_popup/sc_omnibox_popup_container_view_controller.h"

#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_base_view_controller.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_configuration.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#import "ios/chrome/browser/ui/util/named_guide_util.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
CGFloat kFakeImageWidth = 30;
CGFloat kFakeSpacing = 16;
CGFloat kFakeImageLeadingSpacing = 15;
CGFloat kFakeImageToTextSpacing = 14;
CGFloat kFakeTextBoxWidth = 240;
}  // namespace

@implementation SCOmniboxPopupContainerViewController

- (instancetype)initWithPopupViewController:
    (OmniboxPopupBaseViewController*)popupViewController {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _popupViewController = popupViewController;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  // The omnibox popup uses layout guides from the omnibox container view to
  // position its elements. Showcase needs to set up these layout guides as well
  // so the positioning can be correct.
  UIView* fakeImageView = [[UIView alloc] initWithFrame:CGRectZero];
  fakeImageView.translatesAutoresizingMaskIntoConstraints = NO;
  fakeImageView.backgroundColor = UIColor.blueColor;
  UIView* fakeTextView = [[UIView alloc] initWithFrame:CGRectZero];
  fakeTextView.translatesAutoresizingMaskIntoConstraints = NO;
  fakeTextView.backgroundColor = UIColor.redColor;

  [self.view addSubview:fakeImageView];
  [self.view addSubview:fakeTextView];

  [NSLayoutConstraint activateConstraints:@[
    [fakeImageView.heightAnchor constraintEqualToConstant:kFakeImageWidth],
    [fakeImageView.widthAnchor
        constraintEqualToAnchor:fakeImageView.heightAnchor],
    [fakeImageView.topAnchor constraintEqualToAnchor:self.view.topAnchor
                                            constant:kFakeSpacing],
    [fakeImageView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor
                       constant:kFakeImageLeadingSpacing],

    [fakeTextView.heightAnchor
        constraintEqualToAnchor:fakeImageView.heightAnchor],
    [fakeTextView.widthAnchor constraintEqualToConstant:kFakeTextBoxWidth],
    [fakeTextView.leadingAnchor
        constraintEqualToAnchor:fakeImageView.trailingAnchor
                       constant:kFakeImageToTextSpacing],
    [fakeTextView.topAnchor constraintEqualToAnchor:fakeImageView.topAnchor],
  ]];

  AddNamedGuidesToView(@[ kOmniboxLeadingImageGuide, kOmniboxTextFieldGuide ],
                       self.view);

  [NamedGuide guideWithName:kOmniboxLeadingImageGuide view:self.view]
      .constrainedView = fakeImageView;
  [NamedGuide guideWithName:kOmniboxTextFieldGuide view:self.view]
      .constrainedView = fakeTextView;

  // Popup uses same colors as the toolbar, so the ToolbarConfiguration is
  // used to get the style.
  ToolbarConfiguration* configuration =
      [[ToolbarConfiguration alloc] initWithStyle:NORMAL];

  UIView* containerView = [[UIView alloc] init];
  [containerView addSubview:self.popupViewController.view];
  containerView.backgroundColor = [configuration backgroundColor];
  containerView.translatesAutoresizingMaskIntoConstraints = NO;
  self.popupViewController.view.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(self.popupViewController.view, containerView);

  self.view.backgroundColor = UIColor.whiteColor;

  [self addChildViewController:self.popupViewController];
  [self.view addSubview:containerView];
  [self.popupViewController didMoveToParentViewController:self];
  [NSLayoutConstraint activateConstraints:@[
    [self.view.leadingAnchor
        constraintEqualToAnchor:containerView.leadingAnchor],
    [self.view.trailingAnchor
        constraintEqualToAnchor:containerView.trailingAnchor],
    [self.view.bottomAnchor constraintEqualToAnchor:containerView.bottomAnchor],
    [containerView.topAnchor constraintEqualToAnchor:fakeImageView.bottomAnchor
                                            constant:kFakeSpacing],
  ]];
}

@end
