// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/coordinator/panel_content_mediator.h"

#import <memory>

#import "ios/chrome/browser/broadcaster/ui_bundled/chrome_broadcast_observer.h"
#import "ios/chrome/browser/broadcaster/ui_bundled/chrome_broadcaster.h"
#import "ios/chrome/browser/contextual_panel/ui/panel_content_consumer.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/toolbar/ui_bundled/fullscreen/toolbars_size.h"
#import "ios/chrome/browser/toolbar/ui_bundled/fullscreen/toolbars_size_observer.h"
#import "ios/chrome/browser/toolbar/ui_bundled/fullscreen/toolbars_size_observer_bridge.h"

@interface PanelContentMediator () <ChromeBroadcastObserver,
                                    ToolbarsSizeObserving>

@end

@implementation PanelContentMediator {
  // The broadcaster to use to receive updates on the toolbar's height.
  __weak ChromeBroadcaster* _broadcaster;
  __weak ToolbarsSize* _toolbarsSize;
  std::unique_ptr<ToolbarsSizeObserverBridge> _toolbarsUIObserverBridge;
}

- (instancetype)initWithBroadcaster:(ChromeBroadcaster*)broadcaster
                       toolbarsSize:(ToolbarsSize*)toolbarsSize {
  self = [super init];
  if (self) {
    if (IsRefactorToolbarsSize()) {
      _toolbarsUIObserverBridge =
          std::make_unique<ToolbarsSizeObserverBridge>(self);
      _toolbarsSize = toolbarsSize;
      [_toolbarsSize addObserver:_toolbarsUIObserverBridge.get()];
    } else {
      _broadcaster = broadcaster;
    }
  }
  return self;
}

- (void)dealloc {
  if (IsRefactorToolbarsSize()) {
    if (_toolbarsSize) {
      [_toolbarsSize removeObserver:_toolbarsUIObserverBridge.get()];
    }
  }
}

- (void)setConsumer:(id<PanelContentConsumer>)consumer {
  _consumer = consumer;

  if (IsRefactorToolbarsSize()) {
    if (_toolbarsSize) {
      [self.consumer
          updateBottomToolbarHeight:_toolbarsSize.expandedBottomToolbarHeight];
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
  CHECK(!IsRefactorToolbarsSize());
  [self.consumer updateBottomToolbarHeight:height];
}

#pragma mark - ToolbarUIObserving

- (void)OnBottomToolbarHeightChanged {
  if (IsRefactorToolbarsSize()) {
    [self.consumer
        updateBottomToolbarHeight:_toolbarsSize.expandedBottomToolbarHeight];
  }
}

- (void)OnTopToolbarHeightChanged {
}

@end
