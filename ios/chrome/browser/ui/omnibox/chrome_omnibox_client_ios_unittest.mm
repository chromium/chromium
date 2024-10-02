// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/chrome_omnibox_client_ios.h"

#import "base/test/task_environment.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/feature_engagement/test/test_tracker.h"
#import "components/omnibox/browser/autocomplete_provider.h"
#import "components/omnibox/browser/fake_autocomplete_provider.h"
#import "components/omnibox/browser/shortcuts_backend.h"
#import "components/omnibox/common/omnibox_features.h"
#import "ios/chrome/browser/autocomplete/model/shortcuts_backend_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/ui/omnibox/test_web_location_bar.h"
#import "ios/chrome/test/block_cleanup_test.h"
#import "ios/testing/nserror_util.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

class ChromeOmniboxClientIOSTest
    : public PlatformTest,
      public ShortcutsBackend::ShortcutsBackendObserver {
 public:
  ChromeOmniboxClientIOSTest() = default;
  ChromeOmniboxClientIOSTest(const ChromeOmniboxClientIOSTest&) = delete;
  ChromeOmniboxClientIOSTest& operator=(const ChromeOmniboxClientIOSTest&) =
      delete;

  void SetUp() override;
  void TearDown() override;

  void OnShortcutsLoaded() override;
  void OnShortcutsChanged() override;

  void InitShortcutsBackend();
  bool ShortcutExists(const std::u16string& terms) const;
  ShortcutsBackend* shortcuts_backend() { return shortcuts_backend_.get(); }

  void UseAutocompleteMatch(const std::u16string& input_text,
                            const AutocompleteMatch& match);
  void FinishCurrentNavigationSuccessfully();
  void FailCurrentNavigation();
  void RedirectNavigationWithBackButton();

  const ShortcutsBackend::ShortcutMap& shortcuts_map() const {
    return shortcuts_backend_->shortcuts_map();
  }
  bool changed_notified() const { return changed_notified_; }
  void set_changed_notified(bool changed_notified) {
    changed_notified_ = changed_notified;
  }

 private:
  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<TestWebLocationBar> web_location_bar_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<feature_engagement::Tracker> tracker_;
  std::unique_ptr<ChromeOmniboxClientIOS> chrome_omnibox_client_ios_;

  std::unique_ptr<web::FakeWebState> web_state_;
  std::unique_ptr<web::FakeNavigationContext> navigation_context_;

  scoped_refptr<ShortcutsBackend> shortcuts_backend_;

  bool load_notified_ = false;
  bool changed_notified_ = false;
};

void ChromeOmniboxClientIOSTest::SetUp() {
  PlatformTest::SetUp();
  load_notified_ = false;
  changed_notified_ = false;

  TestProfileIOS::Builder builder;
  builder.AddTestingFactory(ios::ShortcutsBackendFactory::GetInstance(),
                            ios::ShortcutsBackendFactory::GetDefaultFactory());
  profile_ = std::move(builder).Build();
  web_location_bar_ = std::make_unique<TestWebLocationBar>();
  tracker_ = feature_engagement::CreateTestTracker();
  chrome_omnibox_client_ios_ = std::make_unique<ChromeOmniboxClientIOS>(
      web_location_bar_.get(), profile_.get(), tracker_.get());

  web_state_ = std::make_unique<web::FakeWebState>();
  web_location_bar_->SetWebState(web_state_.get());
  navigation_context_ = std::make_unique<web::FakeNavigationContext>();
  navigation_context_->SetPageTransition(ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);

  shortcuts_backend_ =
      ios::ShortcutsBackendFactory::GetInstance()->GetForProfile(
          profile_.get());
  ASSERT_TRUE(shortcuts_backend_.get());
  shortcuts_backend_->AddObserver(this);
}

void ChromeOmniboxClientIOSTest::TearDown() {
  PlatformTest::TearDown();
  shortcuts_backend_->RemoveObserver(this);
}

void ChromeOmniboxClientIOSTest::InitShortcutsBackend() {
  ASSERT_TRUE(shortcuts_backend_);
  ASSERT_FALSE(load_notified_);
  ASSERT_FALSE(shortcuts_backend_->initialized());
  shortcuts_backend_->Init();
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(load_notified_);
  EXPECT_TRUE(shortcuts_backend_->initialized());
}

void ChromeOmniboxClientIOSTest::OnShortcutsLoaded() {
  load_notified_ = true;
}

void ChromeOmniboxClientIOSTest::OnShortcutsChanged() {
  changed_notified_ = true;
}

bool ChromeOmniboxClientIOSTest::ShortcutExists(
    const std::u16string& terms) const {
  return shortcuts_map().find(terms) != shortcuts_map().end();
}

void ChromeOmniboxClientIOSTest::UseAutocompleteMatch(
    const std::u16string& input_text,
    const AutocompleteMatch& match) {
  chrome_omnibox_client_ios_->OnAutocompleteAccept(
      match.destination_url, match.post_content.get(),
      WindowOpenDisposition::CURRENT_TAB, match.transition, match.type,
      base::TimeTicks(), false, false, input_text, match, match,
      IDNA2008DeviationCharacter::kNone);
}

void ChromeOmniboxClientIOSTest::FinishCurrentNavigationSuccessfully() {
  navigation_context_->SetError(nil);
  web_state_->OnNavigationFinished(navigation_context_.get());
}

void ChromeOmniboxClientIOSTest::FailCurrentNavigation() {
  navigation_context_->SetError(
      testing::NSErrorWithLocalizedDescription(@"error"));
  web_state_->OnNavigationFinished(navigation_context_.get());
}

void ChromeOmniboxClientIOSTest::RedirectNavigationWithBackButton() {
  navigation_context_->SetPageTransition(ui::PAGE_TRANSITION_FORWARD_BACK);
}

// Tests that successful navigations are added to the shortcuts database.
TEST_F(ChromeOmniboxClientIOSTest, SuccessfulNavigationAddsShortcut) {
  InitShortcutsBackend();

  scoped_refptr<FakeAutocompleteProvider> bookmark_provider =
      new FakeAutocompleteProvider(AutocompleteProvider::Type::TYPE_BOOKMARK);
  AutocompleteMatch bookmark_match(bookmark_provider.get(), 400, true,
                                   AutocompleteMatchType::BOOKMARK_TITLE);

  // Navigate to `bookmark_match` with `search_terms`.
  std::u16string search_terms = u"input";
  UseAutocompleteMatch(search_terms, bookmark_match);

  ASSERT_FALSE(changed_notified());
  ASSERT_FALSE(ShortcutExists(search_terms));

  FinishCurrentNavigationSuccessfully();
  // Verify that the successful navigation is added to the database.
  ASSERT_TRUE(ShortcutExists(search_terms));
  ASSERT_TRUE(changed_notified());
}

// Tests that unfinished navigations or failed navigations are not added in the
// shortcuts database.
TEST_F(ChromeOmniboxClientIOSTest, UnsuccessfulNavigationDontAddShortcut) {
  InitShortcutsBackend();

  scoped_refptr<FakeAutocompleteProvider> bookmark_provider =
      new FakeAutocompleteProvider(AutocompleteProvider::Type::TYPE_BOOKMARK);
  AutocompleteMatch bookmark_match(bookmark_provider.get(), 400, true,
                                   AutocompleteMatchType::BOOKMARK_TITLE);

  // Navigate using `search_terms`.
  std::u16string search_terms = u"input";
  UseAutocompleteMatch(search_terms, bookmark_match);

  // Navigation fails, verify that it's not added to the database.
  FailCurrentNavigation();
  ASSERT_FALSE(changed_notified());
  ASSERT_FALSE(ShortcutExists(search_terms));

  // Navigate using `search_terms_2`.
  std::u16string search_terms_2 = u"input2";
  UseAutocompleteMatch(search_terms_2, bookmark_match);

  // Navigate using `search_terms_3`, interrupting the previous navigation.
  std::u16string search_terms_3 = u"input3";
  UseAutocompleteMatch(search_terms_3, bookmark_match);

  ASSERT_FALSE(changed_notified());
  ASSERT_FALSE(ShortcutExists(search_terms_2));
  ASSERT_FALSE(ShortcutExists(search_terms_3));
  FinishCurrentNavigationSuccessfully();

  // Verify that the interrupted navigation is not added to the database.
  ASSERT_FALSE(ShortcutExists(search_terms_2));
  // Verify that the successful navigation is added to the database.
  ASSERT_TRUE(ShortcutExists(search_terms_3));
  ASSERT_TRUE(changed_notified());
}

// Tests that non omnibox successful navigation are not added in the shortcuts
// database.
TEST_F(ChromeOmniboxClientIOSTest, SuccessfulNonOmniboxDontAddShortcut) {
  InitShortcutsBackend();

  scoped_refptr<FakeAutocompleteProvider> bookmark_provider =
      new FakeAutocompleteProvider(AutocompleteProvider::Type::TYPE_BOOKMARK);
  AutocompleteMatch bookmark_match(bookmark_provider.get(), 400, true,
                                   AutocompleteMatchType::BOOKMARK_TITLE);

  // Navigate to `bookmark_match` with `search_terms`.
  std::u16string search_terms = u"input";
  UseAutocompleteMatch(search_terms, bookmark_match);

  ASSERT_FALSE(changed_notified());
  ASSERT_FALSE(ShortcutExists(search_terms));

  // Navigation redirected by pressing the back button.
  RedirectNavigationWithBackButton();
  // Back button navigation is successful.
  FinishCurrentNavigationSuccessfully();

  // Verify that the back button navigation is not added to the database.
  ASSERT_FALSE(ShortcutExists(search_terms));
  ASSERT_FALSE(changed_notified());
}
