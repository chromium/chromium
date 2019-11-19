// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OVERLAYS_COMMON_ALERTS_ALERT_OVERLAY_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_OVERLAYS_COMMON_ALERTS_ALERT_OVERLAY_MEDIATOR_H_

#import <Foundation/Foundation.h>

@protocol AlertConsumer;
@protocol AlertOverlayMediatorDataSource;
@protocol AlertOverlayMediatorDelegate;

// Mediator superclass for configuring AlertConsumers.
@interface AlertOverlayMediator : NSObject

// The consumer to be updated by this mediator.  Setting to a new value updates
// the new consumer.
@property(nonatomic, weak) id<AlertConsumer> consumer;

// The mediator's delegate.
@property(nonatomic, weak) id<AlertOverlayMediatorDelegate> delegate;

// The mediator's data source.
@property(nonatomic, weak) id<AlertOverlayMediatorDataSource> dataSource;

@end

// Protocol used by the actions set up by the JavaScriptDialogOverlayMediator.
@protocol AlertOverlayMediatorDelegate <NSObject>

// Called by |mediator| to dismiss the alert UI when an action is tapped.
- (void)stopDialogForMediator:(AlertOverlayMediator*)mediator;

@end

// Protocol used to provide text field information to the actions set up by the
// JavaScriptDialogOverlayMediator.
@protocol AlertOverlayMediatorDataSource <NSObject>

// Called by |mediator| to fetch the text field input at |index| for the alert
// set up by |mediator|.
- (NSString*)textFieldInputForMediator:(AlertOverlayMediator*)mediator
                        textFieldIndex:(NSUInteger)index;

@end

#endif  // IOS_CHROME_BROWSER_UI_OVERLAYS_COMMON_ALERTS_ALERT_OVERLAY_MEDIATOR_H_
