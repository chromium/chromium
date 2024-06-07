// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/model/overlays/translate_overlay_tab_helper.h"

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/model/overlays/default_infobar_overlay_request_factory.h"
#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_request_inserter.h"
#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_util.h"
#import "ios/chrome/browser/infobars/model/test/fake_infobar_ios.h"
#import "ios/chrome/browser/overlays/model/public/default/default_infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/model/public/infobar_banner/infobar_banner_placeholder_request_config.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_queue.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_queue_util.h"
#import "ios/chrome/browser/overlays/model/test/overlay_test_macros.h"
#import "ios/chrome/browser/passwords/model/test/mock_ios_chrome_save_passwords_infobar_delegate.h"
#import "ios/chrome/browser/translate/model/fake_translate_infobar_delegate.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/platform_test.h"

namespace {
DEFINE_TEST_OVERLAY_REQUEST_CONFIG(FakeConfig);

// Creates a matcher callback for ConfigType and config's InfoBar.
template <class ConfigType>
base::RepeatingCallback<bool(OverlayRequest*)> ConfigAndInfoBarMatcher(
    InfoBarIOS* infobar) {
  return base::BindRepeating(
      [](InfoBarIOS* infobar, OverlayRequest* request) -> bool {
        return GetOverlayRequestInfobar(request) == infobar &&
               request->GetConfig<ConfigType>();
      },
      infobar);
}
}  // namespace

// Test fixture for TranslateInfobarOverlayTranslateOverlayTabHelper.
class TranslateInfobarOverlayTranslateOverlayTabHelperTest
    : public PlatformTest {
 public:
  TranslateInfobarOverlayTranslateOverlayTabHelperTest() {
    web_state_.SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    OverlayRequestQueue::CreateForWebState(&web_state_);
    InfoBarManagerImpl::CreateForWebState(&web_state_);
    InfobarOverlayRequestInserter::CreateForWebState(
        &web_state_, &DefaultInfobarOverlayRequestFactory);
    TranslateOverlayTabHelper::CreateForWebState(&web_state_);

    std::unique_ptr<FakeTranslateInfoBarDelegate> delegate =
        delegate_factory_.CreateFakeTranslateInfoBarDelegate("fr", "en");
    delegate_ = delegate.get();
    std::unique_ptr<InfoBarIOS> infobar = std::make_unique<InfoBarIOS>(
        InfobarType::kInfobarTypeTranslate, std::move(delegate));
    infobar_ = infobar.get();
    InfoBarManagerImpl::FromWebState(&web_state_)
        ->AddInfoBar(std::move(infobar));
  }

  ~TranslateInfobarOverlayTranslateOverlayTabHelperTest() override {
    InfoBarManagerImpl::FromWebState(&web_state_)->ShutDown();
  }

  // Returns the front request of `web_state_`'s OverlayRequestQueue.
  OverlayRequest* front_request() {
    return OverlayRequestQueue::FromWebState(&web_state_,
                                             OverlayModality::kInfobarBanner)
        ->front_request();
  }

 protected:
  web::FakeWebState web_state_;
  FakeTranslateInfoBarDelegateFactory delegate_factory_;
  raw_ptr<FakeTranslateInfoBarDelegate> delegate_ = nullptr;
  raw_ptr<InfoBarIOS> infobar_ = nullptr;
};

// Tests that the inserter adds a placeholder request when Translate begins.
TEST_F(TranslateInfobarOverlayTranslateOverlayTabHelperTest,
       InsertPlaceholderDuringTranslate) {
  InsertParams params(infobar_);
  params.overlay_type = InfobarOverlayType::kBanner;
  params.insertion_index = 0;
  params.source = InfobarOverlayInsertionSource::kInfoBarManager;
  InfobarOverlayRequestInserter::FromWebState(&web_state_)
      ->InsertOverlayRequest(params);

  delegate_->TriggerOnTranslateStepChanged(
      translate::TRANSLATE_STEP_TRANSLATING, translate::TranslateErrors::NONE);
  OverlayRequestQueue* queue = OverlayRequestQueue::FromWebState(
      &web_state_, OverlayModality::kInfobarBanner);
  EXPECT_EQ(2U, queue->size());
  EXPECT_TRUE(
      queue->GetRequest(1)->GetConfig<InfobarBannerPlaceholderRequestConfig>());
}

// Tests that a placeholder is not added to the queue if there is no existing
// banner request in the queue (e.g. auto-translate is on or Tranlsate is
// triggered from the modal).
TEST_F(TranslateInfobarOverlayTranslateOverlayTabHelperTest,
       NoPlaceholderIfNoBannerDuringTranslate) {
  OverlayRequestQueue* queue = OverlayRequestQueue::FromWebState(
      &web_state_, OverlayModality::kInfobarBanner);

  delegate_->TriggerOnTranslateStepChanged(
      translate::TRANSLATE_STEP_TRANSLATING, translate::TranslateErrors::NONE);
  EXPECT_EQ(0U, queue->size());
}

// Tests that the second Translate banner is inserted behind an existing
// placeholder request when Translate finishes.
TEST_F(TranslateInfobarOverlayTranslateOverlayTabHelperTest,
       InsertBehindPlaceholderAfterTranslateFinishes) {
  OverlayRequestQueue* queue = OverlayRequestQueue::FromWebState(
      &web_state_, OverlayModality::kInfobarBanner);
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<InfobarBannerPlaceholderRequestConfig>(
          infobar_);
  queue->AddRequest(std::move(request), nullptr);
  std::unique_ptr<OverlayRequest> second_request =
      OverlayRequest::CreateWithConfig<FakeConfig>();
  queue->AddRequest(std::move(second_request), nullptr);

  delegate_->TriggerOnTranslateStepChanged(
      translate::TRANSLATE_STEP_AFTER_TRANSLATE,
      translate::TranslateErrors::NONE);

  size_t second_banner_index = 0;
  EXPECT_TRUE(GetIndexOfMatchingRequest(
      queue, &second_banner_index,
      ConfigAndInfoBarMatcher<DefaultInfobarOverlayRequestConfig>(infobar_)));
  EXPECT_EQ(1U, second_banner_index);
}

// Tests that the inserter adds a banner request to the end of the queue after
// Translate finishes and there is no placeholder in the queue.
TEST_F(TranslateInfobarOverlayTranslateOverlayTabHelperTest,
       InsertWithoutPlaceholderAfterTranslateFinishes) {
  OverlayRequestQueue* queue = OverlayRequestQueue::FromWebState(
      &web_state_, OverlayModality::kInfobarBanner);
  queue->AddRequest(OverlayRequest::CreateWithConfig<FakeConfig>());

  delegate_->TriggerOnTranslateStepChanged(
      translate::TRANSLATE_STEP_AFTER_TRANSLATE,
      translate::TranslateErrors::NONE);

  EXPECT_EQ(2U, queue->size());
  EXPECT_TRUE(
      queue->GetRequest(1)->GetConfig<DefaultInfobarOverlayRequestConfig>());
}
