// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_util.h"

#import "ios/chrome/browser/infobars/model/test/fake_infobar_ios.h"
#import "ios/chrome/browser/overlays/model/public/common/infobars/infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_queue.h"
#import "ios/chrome/browser/overlays/model/test/overlay_test_macros.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/platform_test.h"

namespace {
DEFINE_TEST_OVERLAY_REQUEST_CONFIG(NonInfobarConfig);

}

using InfobarOverlayUtilTest = PlatformTest;

// Tests that GetOverlayRequestInfobar() returns the InfoBar used to configure
// the request, or null if the request was not configured with an InfoBar.
TEST_F(InfobarOverlayUtilTest, GetOverlayRequestInfobar) {
  FakeInfobarIOS infobar;

  auto infobar_request =
      OverlayRequest::CreateWithConfig<InfobarOverlayRequestConfig>(
          &infobar, InfobarOverlayType::kBanner, infobar.high_priority());
  EXPECT_EQ(&infobar, GetOverlayRequestInfobar(infobar_request.get()));

  auto non_infobar_request =
      OverlayRequest::CreateWithConfig<NonInfobarConfig>();
  EXPECT_FALSE(GetOverlayRequestInfobar(non_infobar_request.get()));
}

// Tests that GetOverlayRequestInfobarType() returns the InfoBar's type.
TEST_F(InfobarOverlayUtilTest, GetOverlayRequestInfobarType) {
  FakeInfobarIOS confirm_infobar;
  auto confirm_infobar_request =
      OverlayRequest::CreateWithConfig<InfobarOverlayRequestConfig>(
          &confirm_infobar, InfobarOverlayType::kBanner,
          confirm_infobar.high_priority());
  EXPECT_EQ(InfobarType::kInfobarTypeConfirm,
            GetOverlayRequestInfobarType(confirm_infobar_request.get()));

  FakeInfobarIOS translate_infobar(InfobarType::kInfobarTypeTranslate);
  auto translate_infobar_request =
      OverlayRequest::CreateWithConfig<InfobarOverlayRequestConfig>(
          &translate_infobar, InfobarOverlayType::kBanner,
          translate_infobar.high_priority());
  EXPECT_EQ(InfobarType::kInfobarTypeTranslate,
            GetOverlayRequestInfobarType(translate_infobar_request.get()));
}

// Tests that GetOverlayRequestInfobarOverlayType() returns the InfoBar's
// overlay type.
TEST_F(InfobarOverlayUtilTest, GetOverlayRequestInfobarOverlayType) {
  FakeInfobarIOS infobar;
  auto banner_request =
      OverlayRequest::CreateWithConfig<InfobarOverlayRequestConfig>(
          &infobar, InfobarOverlayType::kBanner, infobar.high_priority());
  EXPECT_EQ(InfobarOverlayType::kBanner,
            GetOverlayRequestInfobarOverlayType(banner_request.get()));

  auto modal_request =
      OverlayRequest::CreateWithConfig<InfobarOverlayRequestConfig>(
          &infobar, InfobarOverlayType::kModal, infobar.high_priority());
  EXPECT_EQ(InfobarOverlayType::kModal,
            GetOverlayRequestInfobarOverlayType(modal_request.get()));
}

// Tests that GetInfobarOverlayRequestIndex() returns the correct indices.
TEST_F(InfobarOverlayUtilTest, GetInfobarOverlayRequestIndex) {
  web::FakeWebState web_state;
  FakeInfobarIOS infobar0;
  FakeInfobarIOS infobar1;
  FakeInfobarIOS infobar2;
  FakeInfobarIOS infobar4;

  OverlayRequestQueue::CreateForWebState(&web_state);
  OverlayRequestQueue* queue =
      OverlayRequestQueue::FromWebState(&web_state, OverlayModality::kTesting);
  queue->AddRequest(
      OverlayRequest::CreateWithConfig<InfobarOverlayRequestConfig>(
          &infobar0, InfobarOverlayType::kBanner, infobar0.high_priority()));
  queue->AddRequest(
      OverlayRequest::CreateWithConfig<InfobarOverlayRequestConfig>(
          &infobar1, InfobarOverlayType::kBanner, infobar1.high_priority()));
  queue->AddRequest(
      OverlayRequest::CreateWithConfig<InfobarOverlayRequestConfig>(
          &infobar2, InfobarOverlayType::kBanner, infobar2.high_priority()));

  size_t index = 0;
  EXPECT_TRUE(GetInfobarOverlayRequestIndex(queue, &infobar0, &index));
  EXPECT_EQ(0U, index);
  EXPECT_TRUE(GetInfobarOverlayRequestIndex(queue, &infobar1, &index));
  EXPECT_EQ(1U, index);
  EXPECT_TRUE(GetInfobarOverlayRequestIndex(queue, &infobar2, &index));
  EXPECT_EQ(2U, index);

  EXPECT_FALSE(GetInfobarOverlayRequestIndex(queue, &infobar4, &index));
}
