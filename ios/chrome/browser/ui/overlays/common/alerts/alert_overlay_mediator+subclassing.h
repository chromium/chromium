// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OVERLAYS_COMMON_ALERTS_ALERT_OVERLAY_MEDIATOR_SUBCLASSING_H_
#define IOS_CHROME_BROWSER_UI_OVERLAYS_COMMON_ALERTS_ALERT_OVERLAY_MEDIATOR_SUBCLASSING_H_

#import "ios/chrome/browser/ui/overlays/common/alerts/alert_overlay_mediator.h"

@class AlertAction;
@class TextFieldConfiguration;

// Category that exposes the values to pass to the consumer.  Subclasses must
// implement these functions using the data from their request configurations.
@interface AlertOverlayMediator (Subclassing)

// The title to supply to the AlertConsumer.  Default values is nil.
@property(nonatomic, readonly) NSString* alertTitle;

// The message to supply to the AlertConsumer.  Default values is nil.
@property(nonatomic, readonly) NSString* alertMessage;

// The text field configurations to supply to the AlertConsumer.  Default values
// is nil.
@property(nonatomic, readonly)
    NSArray<TextFieldConfiguration*>* alertTextFieldConfigurations;

// The alert actions to supply to the AlertConsumer.  Default values is nil.
@property(nonatomic, readonly) NSArray<AlertAction*>* alertActions;

@end

#endif  // IOS_CHROME_BROWSER_UI_OVERLAYS_COMMON_ALERTS_ALERT_OVERLAY_MEDIATOR_SUBCLASSING_H_
