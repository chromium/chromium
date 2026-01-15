// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_camera_handler.h"

#import <AVFoundation/AVFoundation.h>
#import <UIKit/UIKit.h>

#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

// Test fixture for GeminiCameraHandler.
class GeminiCameraHandlerTest : public PlatformTest {
 protected:
  GeminiCameraHandlerTest() {
    pref_service_.registry()->RegisterBooleanPref(
        prefs::kIOSGeminiCameraSetting, true);
    handler_ = [[GeminiCameraHandler alloc] initWithPrefService:&pref_service_];
  }

  GeminiCameraHandler* handler_;
  TestingPrefServiceSimple pref_service_;
};

// Tests that the handler conforms to the GeminiCameraDelegate protocol.
TEST_F(GeminiCameraHandlerTest, TestConformsToProtocol) {
  EXPECT_TRUE([handler_ conformsToProtocol:@protocol(GeminiCameraDelegate)]);
}

// Tests that openCameraFromViewController can be called without crashing.
TEST_F(GeminiCameraHandlerTest, TestOpenCamera) {
  // Mock AVCaptureDevice to simulate an authorized state.
  id mockDevice = OCMClassMock([AVCaptureDevice class]);
  OCMStub([mockDevice authorizationStatusForMediaType:AVMediaTypeVideo])
      .andReturn(AVAuthorizationStatusAuthorized);

  UIViewController* mockViewController = OCMClassMock([UIViewController class]);
  [handler_ openCameraFromViewController:mockViewController withCompletion:nil];
}
