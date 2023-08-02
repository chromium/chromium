// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/js_messaging/web_frames_manager_observer_bridge.h"

#import "base/scoped_observation.h"
#import "ios/web/public/test/fakes/crw_fake_web_frames_manager_observer.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "testing/platform_test.h"

namespace web {

// Test fixture to test WebFramesManagerObserverBridge class.
class WebFramesManagerObserverBridgeTest : public PlatformTest {
 protected:
  WebFramesManagerObserverBridgeTest()
      : fake_web_frame_(FakeWebFrame::CreateMainWebFrame(GURL("example.com"))),
        observer_([[CRWFakeWebFramesManagerObserver alloc] init]),
        observer_bridge_(observer_) {
    scoped_observation_.Observe(&fake_web_frames_manager_);
  }

  std::unique_ptr<FakeWebFrame> fake_web_frame_;
  FakeWebFramesManager fake_web_frames_manager_;
  CRWFakeWebFramesManagerObserver* observer_;
  WebFramesManagerObserverBridge observer_bridge_;
  base::ScopedObservation<WebFramesManager, WebFramesManager::Observer>
      scoped_observation_{&observer_bridge_};
};

// Tests `webFramesManager:frameBecameAvailable:` forwarding.
TEST_F(WebFramesManagerObserverBridgeTest, FrameBecameAvailable) {
  ASSERT_FALSE([observer_ lastAvailableFrame]);
  ASSERT_FALSE([observer_ lastWebFramesManager]);

  observer_bridge_.WebFrameBecameAvailable(&fake_web_frames_manager_,
                                           fake_web_frame_.get());
  ASSERT_TRUE([observer_ lastWebFramesManager]);
  EXPECT_EQ(&fake_web_frames_manager_, [observer_ lastWebFramesManager]);
  ASSERT_TRUE([observer_ lastAvailableFrame]);
  EXPECT_EQ(fake_web_frame_.get(), [observer_ lastAvailableFrame]);
}

// Tests `webFramesManager:frameBecameUnavailable:` forwarding.
TEST_F(WebFramesManagerObserverBridgeTest, FrameBecameUnavailable) {
  ASSERT_TRUE([observer_ lastUnavailableFrameId].empty());
  ASSERT_FALSE([observer_ lastWebFramesManager]);

  observer_bridge_.WebFrameBecameUnavailable(&fake_web_frames_manager_,
                                             fake_web_frame_->GetFrameId());
  EXPECT_TRUE([observer_ lastWebFramesManager]);
  EXPECT_EQ(&fake_web_frames_manager_, [observer_ lastWebFramesManager]);
  EXPECT_EQ(fake_web_frame_->GetFrameId(), [observer_ lastUnavailableFrameId]);
}

}  // namespace web
