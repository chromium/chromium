// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/coordinator/panel_content_mediator.h"

#import "ios/chrome/browser/contextual_panel/ui/panel_content_consumer.h"
#import "ios/chrome/browser/ui/broadcaster/chrome_broadcast_observer.h"
#import "ios/chrome/browser/ui/broadcaster/chrome_broadcaster.h"

@interface PanelContentMediator () <ChromeBroadcastObserver>

@end

@implementation PanelContentMediator {
  // The broadcaster to use to receive updates on the toolbar's height.
  __weak ChromeBroadcaster* _broadcaster;
}

- (instancetype)initWithBroadcaster:(ChromeBroadcaster*)broadcaster {
  self = [super init];
  if (self) {
    _broadcaster = broadcaster;
  }
  return self;
}

- (void)setConsumer:(id<PanelContentConsumer>)consumer {
  _consumer = consumer;

  // Wait for a consumer so the data can be passed straight along to the
  // consumer.
  [_broadcaster addObserver:self
                forSelector:@selector(broadcastExpandedBottomToolbarHeight:)];
}

#pragma mark - ChromeBroadcastObserver

- (void)broadcastExpandedBottomToolbarHeight:(CGFloat)height {
  [self.consumer updateBottomToolbarHeight:height];
}

@end
