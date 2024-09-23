// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/send_tab_to_self/send_tab_to_self_modal_presentation_controller.h"

#import "base/check.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_presentation_util.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/infobars/presentation/infobar_modal_positioner.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {
// The rounded corner radius for the container view.
const CGFloat kContainerCornerRadius = 13.0;
}  // namespace

@implementation SendTabToSelfModalPresentationController

- (void)containerViewWillLayoutSubviews {
  self.presentedView.frame =
      ContainedModalFrameThatFit(self.modalPositioner, self.containerView);

  // Style the presented and container views.
  self.presentedView.layer.cornerRadius = kContainerCornerRadius;
  self.presentedView.layer.masksToBounds = YES;
  self.presentedView.clipsToBounds = YES;
  self.containerView.backgroundColor =
      [UIColor colorNamed:kScrimBackgroundColor];
}

@end
