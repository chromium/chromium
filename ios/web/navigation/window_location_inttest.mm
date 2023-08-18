// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/apple/foundation_util.h"
#import "base/memory/ptr_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/test/web_view_interaction_test_util.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/test/web_int_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

namespace {

// URL for the test window.location test file.  The page at this URL contains
// several buttons that trigger window.location commands.  The page supports
// several JavaScript functions:
// - updateUrlToLoadText(), which takes a URL and updates a div on the page to
//   contain that text.  This URL is used as the parameter for window.location
//   function calls triggered by button taps.
// - getUrl(), which returns the URL that was set via updateUrlToLoadText().
// - isOnLoadTextVisible(), which returns whether a placeholder string is
//   present on the page.  This string is added to the page in the onload event
//   and is removed once a button is tapped.  Verifying that the onload text is
//   visible after tapping a button is equivalent to checking that a load has
//   occurred as the result of the button tap.
const char kWindowLocationTestURL[] = "/window_location.html";

// Button IDs used in the window.location test page.
const char kWindowLocationAssignID[] = "location-assign";
const char kWindowLocationReplaceID[] = "location-replace";
const char kWindowLocationReloadID[] = "location-reload";
const char kWindowLocationSetToDOMStringID[] = "set-location-to-dom-string";

// JavaScript functions on the window.location test page.
NSString* const kUpdateURLScriptFormat = @"updateUrlToLoadText('%s')";
const char kGetURLScript[] = "getUrl()";
const char kOnLoadCheckScript[] = "isOnLoadTextVisible()";
const char kNoOpCheckScript[] = "isNoOpTextVisible()";

// URL of a sample file-based page.
const char kSampleFileBasedURL[] = "/chromium_logo_page.html";

}  // namespace

// Test fixture for window.location integration tests.
class WindowLocationTest : public web::WebIntTest {
 protected:
  void SetUp() override {
    web::WebIntTest::SetUp();

    test_server_ = std::make_unique<net::EmbeddedTestServer>();
    test_server_->ServeFilesFromSourceDirectory(
        base::FilePath("ios/testing/data/http_server_files/"));
    ASSERT_TRUE(test_server_->Start());

    // Load the window.location test page.
    window_location_url_ = test_server_->GetURL(kWindowLocationTestURL);
    ASSERT_TRUE(LoadUrl(window_location_url()));
  }

  // The URL of the window.location test page.
  const GURL& window_location_url() { return window_location_url_; }

  // Executes JavaScript on the window.location test page to use `url` as the
  // parameter for the window.location calls executed by tapping the buttons on
  // the page.
  void SetWindowLocationUrl(const GURL& url) {
    ASSERT_EQ(window_location_url(), web_state()->GetLastCommittedURL());
    std::string url_spec = url.possibly_invalid_spec();
    NSString* set_url_script =
        [NSString stringWithFormat:kUpdateURLScriptFormat, url_spec.c_str()];
    web::test::ExecuteJavaScript(web_state(),
                                 base::SysNSStringToUTF8(set_url_script));
    std::unique_ptr<base::Value> injected_url =
        web::test::ExecuteJavaScript(web_state(), kGetURLScript);
    ASSERT_TRUE(injected_url->is_string());
    ASSERT_EQ(url_spec, injected_url->GetString());
  }

  // Executes JavaScript on the window.location test page and returns whether
  // `kOnLoadText` is visible.
  bool IsOnLoadTextVisible() {
    std::unique_ptr<base::Value> text_visible =
        web::test::ExecuteJavaScript(web_state(), kOnLoadCheckScript);
    return text_visible->GetBool();
  }

  // Executes JavaScript on the window.location test page and returns whether
  // the no-op text is visible.  It is displayed 0.5 seconds after a button is
  // tapped, and can be used to verify that a navigation did not occur.
  bool IsNoOpTextVisible() {
    std::unique_ptr<base::Value> text_visible =
        web::test::ExecuteJavaScript(web_state(), kNoOpCheckScript);
    return text_visible->GetBool();
  }

  std::unique_ptr<net::EmbeddedTestServer> test_server_;

 private:
  GURL window_location_url_;
};

// Tests that calling window.location.assign() creates a new NavigationItem.
TEST_F(WindowLocationTest, Assign) {
  // Navigate to about:blank so there is a forward entry to prune.
  GURL about_blank("about:blank");
  ASSERT_TRUE(LoadUrl(about_blank));
  web::NavigationItem* about_blank_item =
      navigation_manager()->GetLastCommittedItem();

  // Navigate back to the window.location test page.
  ASSERT_TRUE(ExecuteBlockAndWaitForLoad(window_location_url(), ^{
    navigation_manager()->GoBack();
  }));

  // Set the window.location test URL and tap the window.location.assign()
  // button.
  GURL sample_url = test_server_->GetURL(kSampleFileBasedURL);
  SetWindowLocationUrl(sample_url);
  auto block = ^{
    ASSERT_TRUE(web::test::TapWebViewElementWithId(web_state(),
                                                   kWindowLocationAssignID));
  };
  ASSERT_TRUE(ExecuteBlockAndWaitForLoad(sample_url, block));

  // Verify that `sample_url` was loaded and that `about_blank_item` was pruned.
  EXPECT_EQ(sample_url, navigation_manager()->GetLastCommittedItem()->GetURL());
  EXPECT_EQ(NSNotFound, GetIndexOfNavigationItem(about_blank_item));
}

// Tests that calling window.location.replace() doesn't create a new
// NavigationItem.
TEST_F(WindowLocationTest, Replace) {
  // Navigate to about:blank so there is a forward entry.
  GURL about_blank("about:blank");
  ASSERT_TRUE(LoadUrl(about_blank));
  web::NavigationItem* about_blank_item =
      navigation_manager()->GetLastCommittedItem();

  // Navigate back to the window.location test page.
  ASSERT_TRUE(ExecuteBlockAndWaitForLoad(window_location_url(), ^{
    navigation_manager()->GoBack();
  }));

  // Set the window.location test URL and tap the window.location.replace()
  // button.
  GURL sample_url = test_server_->GetURL(kSampleFileBasedURL);
  SetWindowLocationUrl(sample_url);
  auto block = ^{
    ASSERT_TRUE(web::test::TapWebViewElementWithId(web_state(),
                                                   kWindowLocationReplaceID));
  };
  ASSERT_TRUE(ExecuteBlockAndWaitForLoad(sample_url, block));

  // Verify that `sample_url` was loaded and that `about_blank_item` was pruned.
  web::NavigationItem* current_item =
      navigation_manager()->GetLastCommittedItem();
  EXPECT_EQ(sample_url, current_item->GetURL());
  EXPECT_EQ(GetIndexOfNavigationItem(current_item) + 1,
            GetIndexOfNavigationItem(about_blank_item));
}

// Tests that calling window.location.reload() causes an onload event to occur.
TEST_F(WindowLocationTest, WindowLocationReload) {
  // Tap the window.location.reload() button.
  auto block = ^{
    ASSERT_TRUE(web::test::TapWebViewElementWithId(web_state(),
                                                   kWindowLocationReloadID));
  };
  ASSERT_TRUE(ExecuteBlockAndWaitForLoad(window_location_url(), block));

  // Verify that `kOnLoadText` is displayed and that no additional
  // NavigationItems are added.
  EXPECT_TRUE(IsOnLoadTextVisible());
  EXPECT_EQ(1, navigation_manager()->GetItemCount());
}

// Tests that calling window.location.assign() creates a new NavigationItem.
TEST_F(WindowLocationTest, WindowLocationSetToDOMString) {
  // Navigate to about:blank so there is a forward entry to prune.
  GURL about_blank("about:blank");
  ASSERT_TRUE(LoadUrl(about_blank));
  web::NavigationItem* about_blank_item =
      navigation_manager()->GetLastCommittedItem();

  // Navigate back to the window.location test page.
  ASSERT_TRUE(ExecuteBlockAndWaitForLoad(window_location_url(), ^{
    navigation_manager()->GoBack();
  }));

  // Set the window.location test URL and tap the window.location.assign()
  // button.
  GURL sample_url = test_server_->GetURL(kSampleFileBasedURL);
  SetWindowLocationUrl(sample_url);
  auto block = ^{
    ASSERT_TRUE(web::test::TapWebViewElementWithId(
        web_state(), kWindowLocationSetToDOMStringID));
  };
  ASSERT_TRUE(ExecuteBlockAndWaitForLoad(sample_url, block));

  // Verify that `sample_url` was loaded and that `about_blank_item` was pruned.
  EXPECT_EQ(sample_url, navigation_manager()->GetLastCommittedItem()->GetURL());
  EXPECT_EQ(NSNotFound, GetIndexOfNavigationItem(about_blank_item));
}
