// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/segmentation_platform/otr_web_state_observer.h"

#import "base/scoped_observation.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state_manager.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace segmentation_platform {

namespace {

class MockObserverClient : public OTRWebStateObserver::ObserverClient {
 public:
  ~MockObserverClient() override = default;

  MOCK_METHOD(void, OnOTRWebStateCountChanged, (bool));

  void Observe(OTRWebStateObserver* web_state_observer) {
    observation_.Observe(web_state_observer);
  }

 private:
  base::ScopedObservation<OTRWebStateObserver,
                          OTRWebStateObserver::ObserverClient>
      observation_{this};
};

}  // namespace

class OTRWebStateObserverTest : public PlatformTest {
 public:
  OTRWebStateObserverTest() {}
  ~OTRWebStateObserverTest() override = default;

  void SetUp() override {
    PlatformTest::SetUp();

    auto state = TestChromeBrowserState::Builder().Build();
    browser_state_ = state.get();
    browser_state_manager_ =
        std::make_unique<TestChromeBrowserStateManager>(std::move(state));
    observer_ =
        std::make_unique<OTRWebStateObserver>(browser_state_manager_.get());

    BrowserList* browser_list =
        BrowserListFactory::GetForBrowserState(browser_state_);
    browser_ = std::make_unique<TestBrowser>(browser_state_);
    browser_list->AddBrowser(browser_.get());

    otr_browser_state_ = browser_state_->GetOffTheRecordChromeBrowserState();
    otr_browser_ = std::make_unique<TestBrowser>(otr_browser_state_);
    BrowserList* otr_browser_list =
        BrowserListFactory::GetForBrowserState(otr_browser_state_);
    otr_browser_list->AddIncognitoBrowser(otr_browser_.get());
  }

  void TearDown() override {
    PlatformTest::TearDown();

    otr_browser_.reset();
    browser_.reset();
    observer_->TearDown();
    observer_.reset();
    browser_state_manager_.reset();
  }

  void AddWebState(int index) {
    web::WebState::CreateParams create_params(browser_state_);
    auto web_state = web::WebState::Create(create_params);

    browser_->GetWebStateList()->InsertWebState(index, std::move(web_state),
                                                WebStateList::INSERT_NO_FLAGS,
                                                WebStateOpener());
  }

  void AddOtrWebState(int index) {
    web::WebState::CreateParams create_params(otr_browser_state_);
    auto web_state = web::WebState::Create(create_params);

    otr_browser_->GetWebStateList()->InsertWebState(
        index, std::move(web_state), WebStateList::INSERT_NO_FLAGS,
        WebStateOpener());
  }

 protected:
  web::WebTaskEnvironment task_environment_;

  std::unique_ptr<TestChromeBrowserStateManager> browser_state_manager_;
  raw_ptr<TestChromeBrowserState> browser_state_;
  raw_ptr<ChromeBrowserState> otr_browser_state_;
  std::unique_ptr<OTRWebStateObserver> observer_;
  std::unique_ptr<TestBrowser> browser_;
  std::unique_ptr<TestBrowser> otr_browser_;
};

TEST_F(OTRWebStateObserverTest, CreationNotifiesNoOTR) {
  MockObserverClient observer_client;
  EXPECT_CALL(observer_client, OnOTRWebStateCountChanged(false));
  observer_client.Observe(observer_.get());
}

TEST_F(OTRWebStateObserverTest, CreateWithOTR) {
  AddOtrWebState(0);

  MockObserverClient observer_client;
  EXPECT_CALL(observer_client, OnOTRWebStateCountChanged(true));
  observer_client.Observe(observer_.get());

  EXPECT_CALL(observer_client, OnOTRWebStateCountChanged(true));
  AddOtrWebState(1);

  EXPECT_CALL(observer_client, OnOTRWebStateCountChanged(true));
  AddWebState(0);
  otr_browser_->GetWebStateList()->CloseWebStateAt(
      1, WebStateList::CLOSE_NO_FLAGS);

  EXPECT_CALL(observer_client, OnOTRWebStateCountChanged(false));
  otr_browser_->GetWebStateList()->CloseWebStateAt(
      0, WebStateList::CLOSE_NO_FLAGS);
}

TEST_F(OTRWebStateObserverTest, AddOtrAfterCreation) {
  MockObserverClient observer_client;
  EXPECT_CALL(observer_client, OnOTRWebStateCountChanged(false));
  observer_client.Observe(observer_.get());

  EXPECT_CALL(observer_client, OnOTRWebStateCountChanged(true));
  AddWebState(0);
  AddOtrWebState(0);
}

}  // namespace segmentation_platform
