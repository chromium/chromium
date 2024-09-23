// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_COORDINATOR_PANEL_CONTENT_MEDIATOR_H_
#define IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_COORDINATOR_PANEL_CONTENT_MEDIATOR_H_

#import <Foundation/Foundation.h>

@class ChromeBroadcaster;
@protocol PanelContentConsumer;

// Mediator for the PanelContent view.
@interface PanelContentMediator : NSObject

- (instancetype)initWithBroadcaster:(ChromeBroadcaster*)broadcaster
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// The consumer this mediator should inform about any updates.
@property(nonatomic, weak) id<PanelContentConsumer> consumer;

@end

#endif  // IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_COORDINATOR_PANEL_CONTENT_MEDIATOR_H_
