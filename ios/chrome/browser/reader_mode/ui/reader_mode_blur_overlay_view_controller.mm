// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/ui/reader_mode_blur_overlay_view_controller.h"

#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@implementation ReaderModeBlurOverlayViewController {
  UIVisualEffectView* _blurView;
}

- (void)loadView {
  self.view = [[UIView alloc] init];
  _blurView = [[UIVisualEffectView alloc] initWithEffect:nil];
  _blurView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_blurView];
  AddSameConstraints(self.view, _blurView);
}

- (void)animateInWithCompletion:(ProceduralBlock)completion {
  UIBlurEffect* blurEffect =
      [UIBlurEffect effectWithStyle:UIBlurEffectStyleRegular];
  __weak UIVisualEffectView* blurView = _blurView;
  [UIView animateWithDuration:0.25
      animations:^{
        blurView.effect = blurEffect;
      }
      completion:^(BOOL finished) {
        if (completion) {
          completion();
        }
      }];
}

@end
