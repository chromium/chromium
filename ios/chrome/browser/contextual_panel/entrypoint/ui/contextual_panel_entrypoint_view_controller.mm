// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/entrypoint/ui/contextual_panel_entrypoint_view_controller.h"

#import "ios/chrome/browser/contextual_panel/entrypoint/ui/contextual_panel_entrypoint_mutator.h"

@interface ContextualPanelEntrypointViewController () {
  UIImage* image;
}
@end

@implementation ContextualPanelEntrypointViewController

#pragma mark - ContextualPanelEntrypointConsumer

- (void)setEntrypointImage:(UIImage*)image {
  _image = image;
}

#pragma mark - ContextualPanelEntrypointMutator

- (void)userTappedEntrypoint {
  [self.mutator entrypointTapped];
}

@end
