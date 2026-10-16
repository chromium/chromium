// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/profile/incognito_session_tracker.h"

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/removing_indexes.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/signin_test_util.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

// Wrapper over a Browser that registers it with the Profile's
// BrowserList on construction and unregister it on destruction. It must
// not outlive the Profile.
class ScopedBrowser {
 public:
  explicit ScopedBrowser(std::unique_ptr<Browser> browser);

  ScopedBrowser(const ScopedBrowser&) = delete;
  ScopedBrowser& operator=(const ScopedBrowser&) = delete;

  ~ScopedBrowser();

  Browser* get() { return browser_.get(); }

  Browser* operator->() { return browser_.get(); }

  Browser& operator*() { return *browser_; }

 private:
  std::unique_ptr<Browser> browser_;
};

ScopedBrowser::ScopedBrowser(std::unique_ptr<Browser> browser)
    : browser_(std::move(browser)) {
  DCHECK(browser_);
  BrowserListFactory::GetForProfile(
      browser_->GetProfile()->GetOriginalProfile())
      ->AddBrowser(browser_.get());
}

ScopedBrowser::~ScopedBrowser() {
  BrowserListFactory::GetForProfile(
      browser_->GetProfile()->GetOriginalProfile())
      ->RemoveBrowser(browser_.get());
}

// Helper object used to count how many times a callback is invoked and
// that stores the last value passed to the callback.
class SessionStateChangedCallbackHelper {
 public:
  SessionStateChangedCallbackHelper() = default;

  SessionStateChangedCallbackHelper(const SessionStateChangedCallbackHelper&) =
      delete;
  SessionStateChangedCallbackHelper& operator=(
      const SessionStateChangedCallbackHelper&) = delete;

  ~SessionStateChangedCallbackHelper() = default;

  // Register a callback with `tracker`.
  void RegisterCallback(IncognitoSessionTracker* tracker) {
    // It is safe to pass base::Unretained(this) since the object will
    // destroy the subscription in its destructor, and invalidate the
    // callback.
    subscription_ = tracker->RegisterCallback(base::BindRepeating(
        &SessionStateChangedCallbackHelper::SessionStateChanged,
        base::Unretained(this)));
  }

  // Check that the count and last value of the parameter are as expected.
  bool ExpectCallCountAndParameter(int count, int value) const {
    return callback_call_count_ == count && value == callback_parameter_;
  }

 private:
  void SessionStateChanged(bool has_incognito_tabs) {
    ++callback_call_count_;
    callback_parameter_ = has_incognito_tabs;
  }

  base::CallbackListSubscription subscription_;
  int callback_call_count_ = 0;
  bool callback_parameter_ = false;
};

// Creates a new WebState and insert it into `browser`'s WebStateList.
void InsertNewTab(Browser* browser) {
  browser->GetWebStateList()->InsertWebState(
      std::make_unique<web::FakeWebState>(web::WebStateID::NewUnique()));
}

}  // anonymous namespace

class IncognitoSessionTrackerTest : public PlatformTest {
 public:
  IncognitoSessionTrackerTest() {
    personal_profile_ =
        profile_manager_.AddProfileWithBuilder(TestProfileIOS::Builder());
    managed_profile1_ = AddManagedProfileWithBuilder(
        profile_manager_, [FakeSystemIdentity fakeManagedIdentity]);
  }

  ProfileManagerIOS* profile_manager() { return &profile_manager_; }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<TestProfileIOS> personal_profile_;
  raw_ptr<TestProfileIOS> managed_profile1_;
};

// Tests that `HasIncognitoSessionTabs()` value changes when off-the-record
// tabs are opened or closed after the object is created.
TEST_F(IncognitoSessionTrackerTest, HasIncognitoSessionTabs) {
  IncognitoSessionTracker tracker(profile_manager());

  // Install a callback that count how many times it is invoked and store the
  // last value it was invoked with.
  SessionStateChangedCallbackHelper callback_helper;
  callback_helper.RegisterCallback(&tracker);

  // As there are no tabs, HasIncognitoSessionTabs() should return false
  // and the callbacks must not have been notified since nothing changed.
  EXPECT_FALSE(tracker.HasIncognitoSessionTabs());
  EXPECT_TRUE(callback_helper.ExpectCallCountAndParameter(0, false));

  // Install a regular Browser for `personal_profile_` ...
  ScopedBrowser scoped_browser_for_personal_profile(
      std::make_unique<TestBrowser>(personal_profile_));

  // ... and insert a few tabs. Then check that HasIncognitoSessionTabs()
  // still returns false and the callbacks must not have been notified.
  InsertNewTab(scoped_browser_for_personal_profile.get());
  InsertNewTab(scoped_browser_for_personal_profile.get());
  InsertNewTab(scoped_browser_for_personal_profile.get());
  EXPECT_FALSE(tracker.HasIncognitoSessionTabs());
  EXPECT_TRUE(callback_helper.ExpectCallCountAndParameter(0, false));

  // Install a new off-the-record Browser for `personal_profile_` ...
  ScopedBrowser otr_scoped_browser_for_personal_profile(
      std::make_unique<TestBrowser>(
          personal_profile_->GetOffTheRecordProfile()));

  // ... and HasIncognitoSessionTabs() should still return false and the
  // callbacks must not have been notified.
  EXPECT_FALSE(tracker.HasIncognitoSessionTabs());
  EXPECT_TRUE(callback_helper.ExpectCallCountAndParameter(0, false));

  // Insert a few tabs in the off-the-record Browser however should cause
  // HasIncognitoSessionTabs() to return true and the callbacks to be
  // notified once.
  InsertNewTab(otr_scoped_browser_for_personal_profile.get());
  InsertNewTab(otr_scoped_browser_for_personal_profile.get());
  InsertNewTab(otr_scoped_browser_for_personal_profile.get());
  EXPECT_TRUE(tracker.HasIncognitoSessionTabs());
  EXPECT_TRUE(callback_helper.ExpectCallCountAndParameter(1, true));

  // Closing some of the off-the-record Browser should not change the value
  // returned by HasIncognitoSessionTabs() and the callbacks must not have
  // been notified.
  otr_scoped_browser_for_personal_profile->GetWebStateList()->CloseWebStateAt(
      0, WebStateList::ClosingReason::kDefault);
  EXPECT_TRUE(tracker.HasIncognitoSessionTabs());
  EXPECT_TRUE(callback_helper.ExpectCallCountAndParameter(1, true));

  // Closing all the off-the-record tabs will however change the value
  // returned by HasIncognitoSessionTabs(), but only at the end of the
  // batch operation.
  {
    WebStateList* const web_state_list =
        otr_scoped_browser_for_personal_profile->GetWebStateList();

    WebStateList::ScopedBatchOperation batch_operation =
        web_state_list->StartBatchOperation();

    web_state_list->CloseWebStatesAtIndices(
        WebStateList::ClosingReason::kDefault,
        RemovingIndexes({
            .start = 0,
            .count = web_state_list->count(),
        }));

    EXPECT_TRUE(tracker.HasIncognitoSessionTabs());
    EXPECT_TRUE(callback_helper.ExpectCallCountAndParameter(1, true));
  }

  // Once the batch operation has completed, the value should have changed.
  EXPECT_FALSE(tracker.HasIncognitoSessionTabs());
  EXPECT_TRUE(callback_helper.ExpectCallCountAndParameter(2, false));

  // Install a new off-the-record Browser for `managed_profile1_` ...
  ScopedBrowser otr_scoped_browser_for_managed_profile1(
      std::make_unique<TestBrowser>(
          managed_profile1_->GetOffTheRecordProfile()));

  // ... and create a few tabs and check that this change the value of
  // HasIncognitoSessionTabs() and has notified the callbacks.
  InsertNewTab(otr_scoped_browser_for_managed_profile1.get());
  InsertNewTab(otr_scoped_browser_for_managed_profile1.get());
  InsertNewTab(otr_scoped_browser_for_managed_profile1.get());
  EXPECT_TRUE(tracker.HasIncognitoSessionTabs());
  EXPECT_TRUE(callback_helper.ExpectCallCountAndParameter(3, true));

  // Dynamically create a new managed profile `managed_profile2`, insert some
  // off-the-record tabs there, and check that even after closing all
  // the tabs in `otr_scoped_browser_for_managed_profile1`, the value returned
  // by HasIncognitoSessionTabs() remains true.
  TestProfileIOS* managed_profile2 = AddManagedProfileWithBuilder(
      profile_manager_, [FakeSystemIdentity identityWithEmail:@"bar@foo.com"]);

  {
    ScopedBrowser otr_scoped_browser_for_managed_profile2_1(
        std::make_unique<TestBrowser>(
            managed_profile2->GetOffTheRecordProfile()));

    ScopedBrowser otr_scoped_browser_for_managed_profile2_2(
        std::make_unique<TestBrowser>(
            managed_profile2->GetOffTheRecordProfile()));

    InsertNewTab(otr_scoped_browser_for_managed_profile2_1.get());
    InsertNewTab(otr_scoped_browser_for_managed_profile2_1.get());
    InsertNewTab(otr_scoped_browser_for_managed_profile2_2.get());

    CloseAllWebStates(
        *otr_scoped_browser_for_managed_profile1->GetWebStateList(),
        WebStateList::ClosingReason::kDefault);

    EXPECT_TRUE(tracker.HasIncognitoSessionTabs());
    EXPECT_TRUE(callback_helper.ExpectCallCountAndParameter(3, true));
  }

  // But destroying those Browser, should consider the tabs as closed and
  // thus HasIncognitoSessionTabs() should return false and notify the
  // callbacks one time.
  EXPECT_FALSE(tracker.HasIncognitoSessionTabs());
  EXPECT_TRUE(callback_helper.ExpectCallCountAndParameter(4, false));
}
