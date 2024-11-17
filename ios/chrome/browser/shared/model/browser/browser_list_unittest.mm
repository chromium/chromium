// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/browser/browser_list.h"

#import <memory>

#import "base/test/task_environment.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser_list_observer.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

using BrowserType = BrowserList::BrowserType;

class BrowserListTest : public PlatformTest {
 public:
  BrowserListTest() { profile_ = TestProfileIOS::Builder().Build(); }

  BrowserList* browser_list() { return &browser_list_; }

  ProfileIOS* profile() { return profile_.get(); }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  BrowserList browser_list_;
};

// Tests main add/remove logic.
TEST_F(BrowserListTest, AddRemoveBrowsers) {
  // Browser list should start empty
  EXPECT_EQ(0UL, browser_list()->BrowsersOfType(BrowserType::kAll).size());

  TestBrowser browser_1(profile());

  // Adding a browser should result in it appearing in the list.
  browser_list()->AddBrowser(&browser_1);
  std::set<Browser*> browsers =
      browser_list()->BrowsersOfType(BrowserType::kRegular);
  EXPECT_EQ(1UL, browsers.size());
  auto found_browser = browsers.find(&browser_1);
  EXPECT_EQ(&browser_1, *found_browser);

  TestBrowser browser_2(profile());

  // Removing a browser not in the list is a no-op.
  browser_list()->RemoveBrowser(&browser_2);
  EXPECT_EQ(1UL, browser_list()->BrowsersOfType(BrowserType::kRegular).size());

  // More than one browset can be added to the list.
  browser_list()->AddBrowser(&browser_2);
  EXPECT_EQ(2UL, browser_list()->BrowsersOfType(BrowserType::kRegular).size());

  // Removing a browser works -- the list gets smaller, and the removed browser
  // isn't on it.
  browser_list()->RemoveBrowser(&browser_2);
  browsers = browser_list()->BrowsersOfType(BrowserType::kRegular);
  EXPECT_EQ(1UL, browsers.size());
  found_browser = browsers.find(&browser_2);
  EXPECT_EQ(browsers.end(), found_browser);

  // Removing a browser a second time does nothing.
  browser_list()->RemoveBrowser(&browser_2);
  EXPECT_EQ(1UL, browser_list()->BrowsersOfType(BrowserType::kRegular).size());

  // Removing the last browser, even multiple times, works as expected.
  browser_list()->RemoveBrowser(&browser_1);
  browser_list()->RemoveBrowser(&browser_1);
  EXPECT_EQ(0UL, browser_list()->BrowsersOfType(BrowserType::kRegular).size());
}

// Tests regular/incognito/inactive interactions.
TEST_F(BrowserListTest, AddRemoveIncognitoAndInactiveBrowsers) {
  // Incognito browser list starts empty.
  EXPECT_EQ(0UL, browser_list()->BrowsersOfType(BrowserType::kAll).size());

  TestBrowser browser_1(profile());
  Browser* inactive_browser_1 = browser_1.CreateInactiveBrowser();

  ProfileIOS* incognito_profile = profile()->GetOffTheRecordProfile();
  TestBrowser incognito_browser_1(incognito_profile);

  // Adding a regular browser doesn't affect the incognito/inactive lists.
  browser_list()->AddBrowser(&browser_1);
  EXPECT_EQ(1UL, browser_list()->BrowsersOfType(BrowserType::kRegular).size());
  EXPECT_EQ(0UL,
            browser_list()->BrowsersOfType(BrowserType::kIncognito).size());
  EXPECT_EQ(0UL, browser_list()->BrowsersOfType(BrowserType::kInactive).size());
  EXPECT_EQ(1UL, browser_list()->BrowsersOfType(BrowserType::kAll).size());

  // Adding an incognito browser doesn't affect the regular/inactive lists.
  browser_list()->AddBrowser(&incognito_browser_1);
  EXPECT_EQ(1UL, browser_list()->BrowsersOfType(BrowserType::kRegular).size());
  EXPECT_EQ(1UL,
            browser_list()->BrowsersOfType(BrowserType::kIncognito).size());
  EXPECT_EQ(0UL, browser_list()->BrowsersOfType(BrowserType::kInactive).size());
  EXPECT_EQ(2UL, browser_list()->BrowsersOfType(BrowserType::kAll).size());

  // Adding an inactive browser doesn't affect the regular/incognito lists.
  browser_list()->AddBrowser(inactive_browser_1);
  EXPECT_EQ(1UL, browser_list()->BrowsersOfType(BrowserType::kRegular).size());
  EXPECT_EQ(1UL,
            browser_list()->BrowsersOfType(BrowserType::kIncognito).size());
  EXPECT_EQ(1UL, browser_list()->BrowsersOfType(BrowserType::kInactive).size());
  EXPECT_EQ(3UL, browser_list()->BrowsersOfType(BrowserType::kAll).size());

  // An added incognito browser is in the list.
  std::set<Browser*> browsers =
      browser_list()->BrowsersOfType(BrowserType::kIncognito);
  auto found_browser = browsers.find(&incognito_browser_1);
  EXPECT_EQ(&incognito_browser_1, *found_browser);

  // An added inactive browser is in the list.
  std::set<Browser*> inactive_browsers =
      browser_list()->BrowsersOfType(BrowserType::kInactive);
  auto found_inactive_browser = inactive_browsers.find(inactive_browser_1);
  EXPECT_EQ(inactive_browser_1, *found_inactive_browser);

  // Removing browsers from works as expected.
  browser_list()->RemoveBrowser(&browser_1);
  browser_list()->RemoveBrowser(&incognito_browser_1);
  browser_list()->RemoveBrowser(inactive_browser_1);
  EXPECT_EQ(0UL, browser_list()->BrowsersOfType(BrowserType::kAll).size());
}

// Tests that destroyed browsers are auto-removed.
TEST_F(BrowserListTest, AutoRemoveBrowsers) {
  {
    // Create and add scoped browsers
    TestBrowser browser_1(profile());
    browser_list()->AddBrowser(&browser_1);
    EXPECT_EQ(1UL,
              browser_list()->BrowsersOfType(BrowserType::kRegular).size());

    ProfileIOS* incognito_profile = profile()->GetOffTheRecordProfile();
    TestBrowser incognito_browser_1(incognito_profile);
    browser_list()->AddBrowser(&incognito_browser_1);
    EXPECT_EQ(1UL,
              browser_list()->BrowsersOfType(BrowserType::kIncognito).size());
  }

  // Expect that the browsers going out of scope will have triggered removal.
  EXPECT_EQ(0UL, browser_list()->BrowsersOfType(BrowserType::kAll).size());
}

// Tests that values returned from BrowsersOfType aren't affected by subsequent
// changes to the browser list.
TEST_F(BrowserListTest, AllBrowserValuesDontChange) {
  TestBrowser browser_1(profile());

  // Add a browser and get the current set of browsers.
  browser_list()->AddBrowser(&browser_1);
  std::set<Browser*> browsers =
      browser_list()->BrowsersOfType(BrowserType::kAll);
  EXPECT_EQ(1UL, browsers.size());

  // Remove the browser.
  browser_list()->RemoveBrowser(&browser_1);
  EXPECT_EQ(0UL, browser_list()->BrowsersOfType(BrowserType::kAll).size());
  EXPECT_EQ(1UL, browsers.size());
}

// Checks that an observer is informed of additions and removals to both the
// regular and incognito browser lists.
TEST_F(BrowserListTest, BrowserListObserver) {
  TestBrowserListObserver observer;
  browser_list()->AddObserver(&observer);

  TestBrowser browser_1(profile());
  ProfileIOS* incognito_profile = profile()->GetOffTheRecordProfile();
  TestBrowser incognito_browser_1(incognito_profile);

  // Check that a regular addition is observed.
  browser_list()->AddBrowser(&browser_1);
  EXPECT_EQ(&browser_1, observer.GetLastAddedBrowser());
  EXPECT_EQ(1UL, observer.GetLastBrowsers().size());

  // Check that a no-op  removal is *not* observed.
  browser_list()->RemoveBrowser(&incognito_browser_1);
  EXPECT_EQ(nullptr, observer.GetLastRemovedBrowser());

  // Check that a regular removal is observed.
  browser_list()->RemoveBrowser(&browser_1);
  EXPECT_EQ(&browser_1, observer.GetLastRemovedBrowser());
  EXPECT_EQ(0UL, observer.GetLastBrowsers().size());

  // Check that an incognito addition is observed.
  browser_list()->AddBrowser(&incognito_browser_1);
  EXPECT_EQ(&incognito_browser_1, observer.GetLastAddedIncognitoBrowser());
  EXPECT_EQ(1UL, observer.GetLastIncognitoBrowsers().size());

  // Check that an incognito removal is observed.
  browser_list()->RemoveBrowser(&incognito_browser_1);
  EXPECT_EQ(&incognito_browser_1, observer.GetLastRemovedIncognitoBrowser());
  EXPECT_EQ(0UL, observer.GetLastIncognitoBrowsers().size());

  browser_list()->RemoveObserver(&observer);
}

// Checks that destroying the BrowserList  informs the observer.
// TestBrowserListObserver knows to remove itself as an Observer
// when BrowserList::OnBrowserListShutdown() is called.
TEST_F(BrowserListTest, DeleteProfile) {
  TestBrowserListObserver observer;

  // Use a locally scoped BrowserList to control destruction.
  {
    BrowserList browser_list;
    browser_list.AddObserver(&observer);

    // Use a locally scoped Browser to control destruction.
    {
      TestBrowser browser_1(profile());
      browser_list.AddBrowser(&browser_1);

      // Destroy the Browser, nothing should break.
    }

    // Destroy the BrowserList, nothing should break.
  }
}

// Checks that the BrowserList is still functional after the destruction of
// the off-the-record Profile (since this happen during normal
// use of the application).
TEST_F(BrowserListTest, ShutdownOTRProfile) {
  TestBrowserListObserver observer;
  browser_list()->AddObserver(&observer);

  TestBrowser browser_1(profile());

  // Use a block to ensure that the Browser pointing to the off-the-record
  // Profile does not outlive the object (which would cause an
  // use-after-free access in when BrowserList is informed of the Browser
  // destruction).
  {
    ProfileIOS* incognito_profile = profile()->GetOffTheRecordProfile();
    TestBrowser incognito_browser_1(incognito_profile);
    browser_list()->AddBrowser(&incognito_browser_1);

    // Check that adding/removing incognito is observed.
    EXPECT_EQ(&incognito_browser_1, observer.GetLastAddedIncognitoBrowser());
    EXPECT_EQ(1UL, observer.GetLastIncognitoBrowsers().size());

    browser_list()->AddBrowser(&browser_1);
    // Check that a regular addition is observed.
    EXPECT_EQ(&browser_1, observer.GetLastAddedBrowser());
    EXPECT_EQ(1UL, observer.GetLastBrowsers().size());
  }

  // Destroy the off-the-record Profile.
  profile()->DestroyOffTheRecordProfile();
  ASSERT_FALSE(profile()->HasOffTheRecordProfile());

  TestBrowser browser_2(profile());
  browser_list()->AddBrowser(&browser_2);
  // Check that another regular addition is observed.
  EXPECT_EQ(&browser_2, observer.GetLastAddedBrowser());
  EXPECT_EQ(2UL, observer.GetLastBrowsers().size());

  // Check that a regular removal is observed.
  browser_list()->RemoveBrowser(&browser_1);
  EXPECT_EQ(&browser_1, observer.GetLastRemovedBrowser());
  EXPECT_EQ(1UL, observer.GetLastBrowsers().size());

  browser_list()->RemoveObserver(&observer);
}
