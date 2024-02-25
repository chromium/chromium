// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/entrypoint/coordinator/contextual_panel_entrypoint_mediator.h"

#import "ios/chrome/browser/contextual_panel/entrypoint/coordinator/contextual_panel_entrypoint_mediator_delegate.h"
#import "ios/chrome/browser/contextual_panel/entrypoint/ui/contextual_panel_entrypoint_consumer.h"

@implementation ContextualPanelEntrypointMediator

- (void)disconnect {
  // Reset observations.
}

#pragma mark - ContextualPanelEntrypointMutator

- (void)entrypointTapped {
  // Do something.
}

// TODO: Observe CP service when that's implemented.

@end
