// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/animations/fake_tab_grid_transition_animation.h"

@implementation FakeTransitionAnimation

#pragma mark - TabGridTransitionAnimation

- (void)animateWithCompletion:(ProceduralBlock)completion {
  self.animationCount++;
  completion();
}

@end
