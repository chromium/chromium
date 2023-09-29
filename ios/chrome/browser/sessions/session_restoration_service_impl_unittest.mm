// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/session_restoration_service_impl.h"

#import "ios/chrome/browser/sessions/test_session_restoration_observer.h"
#import "testing/platform_test.h"

using SessionRestorationServiceImplTest = PlatformTest;

// Tests that adding and removing observer works correctly.
TEST_F(SessionRestorationServiceImplTest, ObserverRegistration) {
  SessionRestorationServiceImpl service;
  TestSessionRestorationObserver observer;
  ASSERT_FALSE(observer.IsInObserverList());

  // Check that registering/unregistering the observer works.
  service.AddObserver(&observer);
  EXPECT_TRUE(observer.IsInObserverList());

  service.RemoveObserver(&observer);
  EXPECT_FALSE(observer.IsInObserverList());
}
