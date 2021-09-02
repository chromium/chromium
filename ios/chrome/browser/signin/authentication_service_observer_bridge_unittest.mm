// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/authentication_service_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

using AuthenticationServiceObserverBridgeTest = PlatformTest;

// Tests that |OnPrimaryAccountRestricted| is forwarded from the service.
TEST_F(AuthenticationServiceObserverBridgeTest, primaryAccountRestricted) {
  id<AuthenticationServiceObserving> observer =
      OCMStrictProtocolMock(@protocol(AuthenticationServiceObserving));
  AuthenticationServiceObserverBridge bridge(observer);

  OCMExpect([observer primaryAccountRestricted]);
  bridge.OnPrimaryAccountRestricted();
}
