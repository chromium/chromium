// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/coordinator/panel_content_mediator.h"

#import <memory>

#import "ios/chrome/browser/broadcaster/ui_bundled/chrome_broadcast_observer.h"
#import "ios/chrome/browser/broadcaster/ui_bundled/chrome_broadcaster.h"
#import "ios/chrome/browser/contextual_panel/ui/panel_content_consumer.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/toolbar/ui_bundled/fullscreen/toolbar_ui.h"
#import "ios/chrome/browser/toolbar/ui_bundled/fullscreen/toolbar_ui_observer.h"
#import "ios/chrome/browser/toolbar/ui_bundled/fullscreen/toolbar_ui_observer_bridge.h"

@interface PanelContentMediator () <ChromeBroadcastObserver, ToolbarUIObserving>

@end

@implementation PanelContentMediator {
  // The broadcaster to use to receive updates on the toolbar's height.
  __weak ChromeBroadcaster* _broadcaster;
  __weak ToolbarUIState* _toolbarUIState;
  std::unique_ptr<ToolbarUIObserverBridge> _toolbarUIObserverBridge;
}

- (instancetype)initWithBroadcaster:(ChromeBroadcaster*)broadcaster
                     toolbarUIState:(ToolbarUIState*)toolbarUIState {
  self = [super init];
  if (self) {
    if (IsRefactorToolbarUI()) {
      _toolbarUIObserverBridge =
          std::make_unique<ToolbarUIObserverBridge>(self);
      _toolbarUIState = toolbarUIState;
      [_toolbarUIState addObserver:_toolbarUIObserverBridge.get()];
    } else {
      _broadcaster = broadcaster;
    }
  }
  return self;
}

- (void)dealloc {
  if (IsRefactorToolbarUI()) {
    if (_toolbarUIState) {
      [_toolbarUIState removeObserver:_toolbarUIObserverBridge.get()];
    }
  }
}

- (void)setConsumer:(id<PanelContentConsumer>)consumer {
  _consumer = consumer;

  if (IsRefactorToolbarUI()) {
    if (_toolbarUIState) {
      [self.consumer
          updateBottomToolbarHeight:_toolbarUIState
                                        .expandedBottomToolbarHeight];
    }
  } else {
    // Wait for a consumer so the data can be passed straight along to the
    // consumer.
    [_broadcaster addObserver:self
                  forSelector:@selector(broadcastExpandedBottomToolbarHeight:)];
  }
}

#pragma mark - ChromeBroadcastObserver

- (void)broadcastExpandedBottomToolbarHeight:(CGFloat)height {
  CHECK(!IsRefactorToolbarUI());
  [self.consumer updateBottomToolbarHeight:height];
}

#pragma mark - ToolbarUIObserving

- (void)OnBottomToolbarHeightChanged {
  if (IsRefactorToolbarUI()) {
    [self.consumer
        updateBottomToolbarHeight:_toolbarUIState.expandedBottomToolbarHeight];
  }
}

- (void)OnTopToolbarHeightChanged {
}

@end
