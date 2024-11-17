// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/browser/all_web_state_list_observation_registrar.h"

#import "base/memory/raw_ptr.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

class TestRegisteredWebStateListObserver : public WebStateListObserver {
 public:
  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) override {
    switch (change.type()) {
      case WebStateListChange::Type::kStatusOnly:
        // Do nothing when a WebState is selected and its status is updated.
        break;
      case WebStateListChange::Type::kDetach:
        // Do nothing when a WebState is detached.
        break;
      case WebStateListChange::Type::kMove:
        // Do nothing when a WebState is moved.
        break;
      case WebStateListChange::Type::kReplace:
        // Do nothing when a WebState is replaced.
        break;
      case WebStateListChange::Type::kInsert:
        insertion_count_++;
        break;
      case WebStateListChange::Type::kGroupCreate:
        // Do nothing when a group is created.
        break;
      case WebStateListChange::Type::kGroupVisualDataUpdate:
        // Do nothing when a tab group's visual data are updated.
        break;
      case WebStateListChange::Type::kGroupMove:
        // Do nothing when a tab group is moved.
        break;
      case WebStateListChange::Type::kGroupDelete:
        // Do nothing when a group is deleted.
        break;
    }
  }
  int insertion_count_;
};

class AllWebStateListObservationRegistrarTest : public PlatformTest {
 protected:
  AllWebStateListObservationRegistrarTest()
      : owned_observer_(std::make_unique<TestRegisteredWebStateListObserver>()),
        observer_(owned_observer_.get()) {
    TestProfileIOS::Builder test_profile_builder;
    profile_ = std::move(test_profile_builder).Build();

    browser_list_ = BrowserListFactory::GetForProfile(profile_.get());
  }

  void AppendNewWebState(Browser* browser) {
    auto fake_web_state = std::make_unique<web::FakeWebState>();
    browser->GetWebStateList()->InsertWebState(std::move(fake_web_state));
  }

  base::test::TaskEnvironment task_environment_;
  // Unique pointer to an observer moved into the registrar under test.
  std::unique_ptr<TestRegisteredWebStateListObserver> owned_observer_;
  // Weak pointer to the the moved observer
  raw_ptr<TestRegisteredWebStateListObserver> observer_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<BrowserList> browser_list_;
};

// Test
TEST_F(AllWebStateListObservationRegistrarTest, RegisterAllLists) {
  TestBrowser regular_browser_0(profile_.get());
  browser_list_->AddBrowser(&regular_browser_0);
  TestBrowser incognito_browser_0(profile_->GetOffTheRecordProfile());
  browser_list_->AddBrowser(&incognito_browser_0);

  AllWebStateListObservationRegistrar registrar(browser_list_,
                                                std::move(owned_observer_));
  // Should observe both insertions.
  AppendNewWebState(&regular_browser_0);
  EXPECT_EQ(1, observer_->insertion_count_);
  AppendNewWebState(&incognito_browser_0);
  EXPECT_EQ(2, observer_->insertion_count_);

  // Create a second regular browser and add it.
  TestBrowser regular_browser_1(profile_.get());
  browser_list_->AddBrowser(&regular_browser_1);
  AppendNewWebState(&regular_browser_1);
  // Expect observed insertion.
  EXPECT_EQ(3, observer_->insertion_count_);

  // Create a second incognito  browser and add it.
  TestBrowser incognito_browser_1(profile_->GetOffTheRecordProfile());
  browser_list_->AddBrowser(&incognito_browser_1);
  AppendNewWebState(&incognito_browser_1);
  // Expect observed insertion.
  EXPECT_EQ(4, observer_->insertion_count_);

  // Remove a regular browser
  browser_list_->RemoveBrowser(&regular_browser_1);
  AppendNewWebState(&regular_browser_1);
  // Expect no observed insertion.
  EXPECT_EQ(4, observer_->insertion_count_);

  // Remove an incognito browser
  browser_list_->RemoveBrowser(&incognito_browser_1);
  AppendNewWebState(&incognito_browser_1);
  // Expect no observed insertion.
  EXPECT_EQ(4, observer_->insertion_count_);
}

TEST_F(AllWebStateListObservationRegistrarTest, RegisterRegularLists) {
  TestBrowser regular_browser_0(profile_.get());
  browser_list_->AddBrowser(&regular_browser_0);
  TestBrowser incognito_browser_0(profile_->GetOffTheRecordProfile());
  browser_list_->AddBrowser(&incognito_browser_0);

  AllWebStateListObservationRegistrar registrar(
      browser_list_, std::move(owned_observer_),
      AllWebStateListObservationRegistrar::Mode::REGULAR);
  // Should observe only the reugular insertions.
  AppendNewWebState(&regular_browser_0);
  EXPECT_EQ(1, observer_->insertion_count_);
  AppendNewWebState(&incognito_browser_0);
  EXPECT_EQ(1, observer_->insertion_count_);

  // Create a second regular browser and add it.
  TestBrowser regular_browser_1(profile_.get());
  browser_list_->AddBrowser(&regular_browser_1);
  AppendNewWebState(&regular_browser_1);
  // Expect observed insertion.
  EXPECT_EQ(2, observer_->insertion_count_);

  // Create a second incognito  browser and add it.
  TestBrowser incognito_browser_1(profile_->GetOffTheRecordProfile());
  browser_list_->AddBrowser(&incognito_browser_1);
  AppendNewWebState(&incognito_browser_0);
  // Expect no observed insertion.
  EXPECT_EQ(2, observer_->insertion_count_);
}

TEST_F(AllWebStateListObservationRegistrarTest, RegisterIncognitoLists) {
  TestBrowser regular_browser_0(profile_.get());
  browser_list_->AddBrowser(&regular_browser_0);
  TestBrowser incognito_browser_0(profile_->GetOffTheRecordProfile());
  browser_list_->AddBrowser(&incognito_browser_0);

  AllWebStateListObservationRegistrar registrar(
      browser_list_, std::move(owned_observer_),
      AllWebStateListObservationRegistrar::Mode::INCOGNITO);
  // Should observe only the incognito insertions.
  AppendNewWebState(&regular_browser_0);
  EXPECT_EQ(0, observer_->insertion_count_);
  AppendNewWebState(&incognito_browser_0);
  EXPECT_EQ(1, observer_->insertion_count_);

  // Create a second regular browser and add it.
  TestBrowser regular_browser_1(profile_.get());
  browser_list_->AddBrowser(&regular_browser_1);
  AppendNewWebState(&regular_browser_1);
  // Expect no observed insertion.
  EXPECT_EQ(1, observer_->insertion_count_);

  // Create a second incognito  browser and add it.
  TestBrowser incognito_browser_1(profile_->GetOffTheRecordProfile());
  browser_list_->AddBrowser(&incognito_browser_1);
  AppendNewWebState(&incognito_browser_0);
  // Expect observed insertion.
  EXPECT_EQ(2, observer_->insertion_count_);
}

TEST_F(AllWebStateListObservationRegistrarTest, DeleteWithObservers) {
  // Test that deleting a registrar with active observers is safe.
  TestBrowser regular_browser_0(profile_.get());
  browser_list_->AddBrowser(&regular_browser_0);

  {
    AllWebStateListObservationRegistrar registrar(browser_list_,
                                                  std::move(owned_observer_));
  }
}

// Tests that deleting the profile is safe.
TEST_F(AllWebStateListObservationRegistrarTest, DeleteProfile) {
  // Create some browsers and a registrar, as above.
  TestBrowser regular_browser_0(profile_.get());
  browser_list_->AddBrowser(&regular_browser_0);
  TestBrowser incognito_browser_0(profile_->GetOffTheRecordProfile());
  browser_list_->AddBrowser(&incognito_browser_0);

  AllWebStateListObservationRegistrar registrar(browser_list_,
                                                std::move(owned_observer_));
  AppendNewWebState(&regular_browser_0);
  AppendNewWebState(&incognito_browser_0);
  TestBrowser regular_browser_1(profile_.get());
  browser_list_->AddBrowser(&regular_browser_1);
  AppendNewWebState(&regular_browser_1);

  // Now delete the profile. Nothing should explode.
  profile_.reset();
}
