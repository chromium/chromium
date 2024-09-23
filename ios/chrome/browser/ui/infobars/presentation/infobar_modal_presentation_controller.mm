// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/presentation/infobar_modal_presentation_controller.h"

#import "ios/chrome/browser/overlays/ui_bundled/overlay_presentation_util.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/infobars/presentation/infobar_modal_positioner.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {
// The rounded corner radius for the container view.
const CGFloat kContainerCornerRadius = 13.0;
}  // namespace

@interface InfobarModalPresentationController ()
// Delegate used to position the ModalInfobar.
@property(nonatomic, weak) id<InfobarModalPositioner> modalPositioner;
@end

@implementation InfobarModalPresentationController

- (instancetype)
    initWithPresentedViewController:(UIViewController*)presentedViewController
           presentingViewController:(UIViewController*)presentingViewController
                    modalPositioner:
                        (id<InfobarModalPositioner>)modalPositioner {
  self = [super initWithPresentedViewController:presentedViewController
                       presentingViewController:presentingViewController];
  if (self) {
    _modalPositioner = modalPositioner;
  }
  return self;
}

- (BOOL)shouldPresentInFullscreen {
  // Don't present in fullscreen when modals are shown using OverlayPresenter
  // so that banners presented are inserted into the correct place in the view
  // hierarchy.  Returning NO adds the container view as a sibling view in front
  // of the presenting view controller's view.
  return NO;
}

- (void)presentationTransitionWillBegin {
  // Add a gesture recognizer to endEditing (thus hiding the keyboard) if a user
  // taps outside the keyboard while one its being presented. Set
  // cancelsTouchesInView to NO so the presented Modal can handle the gesture as
  // well. (e.g. Selecting a row in a TableViewController.)
  UITapGestureRecognizer* tap =
      [[UITapGestureRecognizer alloc] initWithTarget:self.presentedView
                                              action:@selector(endEditing:)];
  tap.cancelsTouchesInView = NO;
  [self.containerView addGestureRecognizer:tap];
}

- (void)containerViewWillLayoutSubviews {
  self.presentedView.frame =
      ContainedModalFrameThatFit(self.modalPositioner, self.containerView);

  // Style the presented and container views.
  self.presentedView.layer.cornerRadius = kContainerCornerRadius;
  self.presentedView.layer.masksToBounds = YES;
  self.presentedView.clipsToBounds = YES;
  self.containerView.backgroundColor =
      [UIColor colorNamed:kScrimBackgroundColor];

  [super containerViewWillLayoutSubviews];
}

@end
