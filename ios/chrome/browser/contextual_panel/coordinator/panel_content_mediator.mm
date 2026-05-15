// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/coordinator/panel_content_mediator.h"

#import <memory>

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/broadcaster/ui_bundled/chrome_broadcast_observer.h"
#import "ios/chrome/browser/broadcaster/ui_bundled/chrome_broadcaster.h"
#import "ios/chrome/browser/contextual_panel/ui/panel_content_consumer.h"
#import "ios/chrome/browser/fullscreen/model/fullscreen_browser_agent.h"
#import "ios/chrome/browser/fullscreen/model/fullscreen_browser_agent_observer_bridge.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/fullscreen/toolbars_size.h"
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/fullscreen/toolbars_size_observer.h"
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/fullscreen/toolbars_size_observer_bridge.h"

@interface PanelContentMediator () <ChromeBroadcastObserver,
                                    ToolbarsSizeObserving,
                                    FullscreenBrowserAgentObserving>

@end

@implementation PanelContentMediator {
  // The broadcaster to use to receive updates on the toolbar's height.
  __weak ChromeBroadcaster* _broadcaster;
  __weak ToolbarsSize* _toolbarsSize;
  std::unique_ptr<ToolbarsSizeObserverBridge> _toolbarsUIObserverBridge;
  raw_ptr<FullscreenBrowserAgent> _fullscreenBrowserAgent;
  std::unique_ptr<FullscreenBrowserAgentObserverBridge>
      _fullscreenBrowserAgentObserverBridge;
}

- (instancetype)initWithBroadcaster:(ChromeBroadcaster*)broadcaster
                       toolbarsSize:(ToolbarsSize*)toolbarsSize
             fullscreenBrowserAgent:
                 (FullscreenBrowserAgent*)fullscreenBrowserAgent {
  self = [super init];
  if (self) {
    if (IsFullscreenRefactoringEnabled()) {
      _fullscreenBrowserAgent = fullscreenBrowserAgent;
      _fullscreenBrowserAgentObserverBridge =
          std::make_unique<FullscreenBrowserAgentObserverBridge>(
              self, _fullscreenBrowserAgent);
    } else if (IsRefactorToolbarsSize()) {
      _toolbarsUIObserverBridge =
          std::make_unique<ToolbarsSizeObserverBridge>(self, toolbarsSize);
      _toolbarsSize = toolbarsSize;
      [_toolbarsSize addObserver:_toolbarsUIObserverBridge.get()];
    } else {
      _broadcaster = broadcaster;
    }
  }
  return self;
}

- (void)disconnect {
  if (IsFullscreenRefactoringEnabled()) {
    _fullscreenBrowserAgentObserverBridge.reset();
    _fullscreenBrowserAgent = nullptr;
  } else if (IsRefactorToolbarsSize()) {
    if (_toolbarsSize) {
      [_toolbarsSize removeObserver:_toolbarsUIObserverBridge.get()];
      _toolbarsUIObserverBridge.reset();
      _toolbarsSize = nil;
    }
  } else {
    [_broadcaster
        removeObserver:self
           forSelector:@selector(broadcastExpandedBottomToolbarHeight:)];
    _broadcaster = nil;
  }
}

- (void)dealloc {
  [self disconnect];
}

- (void)setConsumer:(id<PanelContentConsumer>)consumer {
  _consumer = consumer;

  if (IsFullscreenRefactoringEnabled()) {
    [self.consumer
        updateBottomToolbarHeight:_fullscreenBrowserAgent->max_insets().bottom];
  } else if (IsRefactorToolbarsSize()) {
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

#pragma mark - ToolbarsSizeObserving

- (void)toolbarsSizeDidChangeBottomToolbarHeight:(ToolbarsSize*)toolbarsSize {
  if (IsRefactorToolbarsSize()) {
    [self.consumer
        updateBottomToolbarHeight:toolbarsSize.expandedBottomToolbarHeight];
  }
}

#pragma mark - FullscreenBrowserAgentObserving

- (void)fullscreenDidUpdateObscuredInsetRange:(FullscreenBrowserAgent*)agent {
  [self.consumer updateBottomToolbarHeight:agent->max_insets().bottom];
}

@end
