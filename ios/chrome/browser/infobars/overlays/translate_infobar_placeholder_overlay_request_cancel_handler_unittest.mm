// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/translate_infobar_placeholder_overlay_request_cancel_handler.h"

#import "components/translate/core/browser/translate_infobar_delegate.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/infobar_type.h"
#import "ios/chrome/browser/infobars/overlays/default_infobar_overlay_request_factory.h"
#import "ios/chrome/browser/infobars/overlays/fake_translate_overlay_tab_helper.h"
#import "ios/chrome/browser/infobars/overlays/infobar_overlay_request_inserter.h"
#import "ios/chrome/browser/infobars/overlays/infobar_overlay_util.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/overlays/public/common/infobars/infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/infobar_banner_placeholder_request_config.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/translate_infobar_banner_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/overlay_presenter.h"
#import "ios/chrome/browser/overlays/public/overlay_request_queue.h"
#import "ios/chrome/browser/overlays/test/fake_overlay_presentation_context.h"
#import "ios/chrome/browser/translate/fake_translate_infobar_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using translate_infobar_overlays::TranslateBannerRequestConfig;
using translate_infobar_overlays::PlaceholderRequestCancelHandler;

// Test fixture for PlaceholderRequestCancelHandler.
class TranslateInfobarPlaceholderOverlayRequestCancelHandlerTest
    : public PlatformTest {
 public:
  TranslateInfobarPlaceholderOverlayRequestCancelHandlerTest() {
    browser_state_ = TestChromeBrowserState::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(browser_state_.get());
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state_ = web_state.get();
    // Set up WebState and InfoBarManager.
    web_state->SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    browser_->GetWebStateList()->InsertWebState(0, std::move(web_state),
                                                WebStateList::INSERT_ACTIVATE,
                                                WebStateOpener());
    InfoBarManagerImpl::CreateForWebState(web_state_);
    InfobarOverlayRequestInserter::CreateForWebState(
        web_state_, &DefaultInfobarOverlayRequestFactory);
    FakeTranslateOverlayTabHelper::CreateForWebState(web_state_);

    // Set up the OverlayPresenter's presentation context so that presentation
    // can be faked.
    OverlayPresenter::FromBrowser(browser_.get(),
                                  OverlayModality::kInfobarBanner)
        ->SetPresentationContext(&presentation_context_);

    std::unique_ptr<FakeTranslateInfoBarDelegate> delegate =
        delegate_factory_.CreateFakeTranslateInfoBarDelegate("fr", "en");
    delegate_ = delegate.get();
    std::unique_ptr<InfoBarIOS> infobar = std::make_unique<InfoBarIOS>(
        InfobarType::kInfobarTypeTranslate, std::move(delegate));
    infobar_ = infobar.get();
    InfoBarManagerImpl::FromWebState(web_state_)
        ->AddInfoBar(std::move(infobar));
  }

  ~TranslateInfobarPlaceholderOverlayRequestCancelHandlerTest() override {
    // Need to destroy Browser first so that OverlayPresenter unregister
    // itself as an observer of FakeOverlayPresentationContext.
    browser_.reset();
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<TestBrowser> browser_;
  web::FakeWebState* web_state_;
  FakeTranslateInfoBarDelegateFactory delegate_factory_;
  FakeOverlayPresentationContext presentation_context_;
  FakeTranslateInfoBarDelegate* delegate_ = nullptr;
  InfoBarIOS* infobar_ = nullptr;
};

// Test that when TranslationFinished() is called by the handler's observer, the
// placeholder request is cancelled.
TEST_F(TranslateInfobarPlaceholderOverlayRequestCancelHandlerTest,
       AfterTranslate) {
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<InfobarBannerPlaceholderRequestConfig>(
          infobar_);
  OverlayRequest* request_ptr = request.get();
  OverlayRequestQueue* queue = OverlayRequestQueue::FromWebState(
      web_state_, OverlayModality::kInfobarBanner);
  std::unique_ptr<PlaceholderRequestCancelHandler> placeholder_cancel_handler =
      std::make_unique<PlaceholderRequestCancelHandler>(
          request.get(), queue,
          TranslateOverlayTabHelper::FromWebState(web_state_), infobar_);

  queue->AddRequest(std::move(request), std::move(placeholder_cancel_handler));

  EXPECT_EQ(1U, queue->size());
  EXPECT_TRUE(queue->front_request() == request_ptr);

  static_cast<FakeTranslateOverlayTabHelper*>(
      FakeTranslateOverlayTabHelper::FromWebState(web_state_))
      ->CallTranslationFinished(true);

  EXPECT_EQ(0U, queue->size());
  EXPECT_EQ(FakeOverlayPresentationContext::PresentationState::kCancelled,
            presentation_context_.GetPresentationState(request_ptr));
}
