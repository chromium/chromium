// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OVERLAYS_COMMON_ALERTS_TEST_ALERT_OVERLAY_MEDIATOR_TEST_H_
#define IOS_CHROME_BROWSER_UI_OVERLAYS_COMMON_ALERTS_TEST_ALERT_OVERLAY_MEDIATOR_TEST_H_

#include "testing/platform_test.h"

@class AlertOverlayMediator;
@class FakeAlertConsumer;

// Test fixture for AlertOverlayMediator subclasses.
class AlertOverlayMediatorTest : public PlatformTest {
 protected:
  // The consumer that is set up by the mediator provided to SetMediator().
  FakeAlertConsumer* consumer() const { return consumer_; }

  // Sets the mediator being tested by this fixture.  Setting to a new value
  // will update |consumer_| and set it as |mediator|'s consumer.
  void SetMediator(AlertOverlayMediator* mediator);

 private:
  AlertOverlayMediator* mediator_ = nil;
  FakeAlertConsumer* consumer_ = nil;
};

#endif  // IOS_CHROME_BROWSER_UI_OVERLAYS_COMMON_ALERTS_TEST_ALERT_OVERLAY_MEDIATOR_TEST_H_
