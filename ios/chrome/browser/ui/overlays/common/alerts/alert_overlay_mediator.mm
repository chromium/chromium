// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/common/alerts/alert_overlay_mediator.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/alert_view_controller/alert_consumer.h"
#import "ios/chrome/browser/ui/overlays/common/alerts/alert_overlay_mediator+subclassing.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation AlertOverlayMediator

#pragma mark - Accessors

- (void)setConsumer:(id<AlertConsumer>)consumer {
  if (_consumer == consumer)
    return;
  _consumer = consumer;
  NSString* alertTitle = self.alertTitle;
  [_consumer setTitle:alertTitle];
  NSString* alertMessage = self.alertMessage;
  [_consumer setMessage:alertMessage];
  [_consumer setTextFieldConfigurations:self.alertTextFieldConfigurations];
  NSArray<AlertAction*>* alertActions = self.alertActions;
  [_consumer setActions:alertActions];
  DCHECK_GT(alertTitle.length + alertMessage.length, 0U);
  DCHECK_GT(alertActions.count, 0U);
}

@end

@implementation AlertOverlayMediator (Subclassing)

- (NSString*)alertTitle {
  // Subclasses implement.
  return nil;
}

- (NSString*)alertMessage {
  // Subclasses implement.
  return nil;
}

- (NSArray<TextFieldConfiguration*>*)alertTextFieldConfigurations {
  // Subclasses implement.
  return nil;
}

- (NSArray<AlertAction*>*)alertActions {
  // Subclasses implement.
  return nil;
}

@end
