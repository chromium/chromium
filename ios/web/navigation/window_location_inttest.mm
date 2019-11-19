// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "ios/web/public/test/http_server/http_server_util.h"
#import "ios/web/public/test/web_view_interaction_test_util.h"
#import "ios/web/public/web_state.h"
#include "ios/web/public/web_state_observer.h"
#import "ios/web/test/web_int_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
const char kWindowLocationTestURL[] =
    "http://ios/testing/data/http_server_files/window_location.html";

// Button IDs used in the window.location test page.
const char kWindowLocationAssignID[] = "location-assign";
const char kWindowLocationReplaceID[] = "location-replace";
const char kWindowLocationReloadID[] = "location-reload";
const char kWindowLocationSetToDOMStringID[] = "set-location-to-dom-string";

// JavaScript functions on the window.location test page.
NSString* const kUpdateURLScriptFormat = @"updateUrlToLoadText('%s')";
NSString* const kGetURLScript = @"getUrl()";
NSString* const kOnLoadCheckScript = @"isOnLoadTextVisible()";
NSString* const kNoOpCheckScript = @"isNoOpTextVisible()";

// URL of a sample file-based page.
const char kSampleFileBasedURL[] =
    "http://ios/testing/data/http_server_files/chromium_logo_page.html";

}  // namespace

// Test fixture for window.location integration tests.
class WindowLocationTest : public web::WebIntTest {
 protected:
  void SetUp() override {
    web::WebIntTest::SetUp();

    // window.location tests use file-based test pages.
    web::test::SetUpFileBasedHttpServer();

    // Load the window.location test page.
    window_location_url_ =
        web::test::HttpServer::MakeUrl(kWindowLocationTestURL);
    ASSERT_TRUE(LoadUrl(window_location_url()));
  }

  // The URL of the window.location test page.
  const GURL& window_location_url() { return window_location_url_; }

  // Executes JavaScript on the window.location test page to use |url| as the
  // parameter for the window.location calls executed by tapping the buttons on
  // the page.
  void SetWindowLocationUrl(const GURL& url) {
    ASSERT_EQ(window_location_url(), web_state()->GetLastCommittedURL());
    std::string url_spec = url.possibly_invalid_spec();
    NSString* set_url_script =
        [NSString stringWithFormat:kUpdateURLScriptFormat, url_spec.c_str()];
    ExecuteJavaScript(set_url_script);
    NSString* injected_url =
        base::mac::ObjCCastStrict<NSString>(ExecuteJavaScript(kGetURLScript));
    ASSERT_EQ(url_spec, base::SysNSStringToUTF8(injected_url));
  }

  // Executes JavaScript on the window.location test page and returns whether
  // |kOnLoadText| is visible.
  bool IsOnLoadTextVisible() {
    NSNumber* text_visible = base::mac::ObjCCastStrict<NSNumber>(
        ExecuteJavaScript(kOnLoadCheckScript));
    return [text_visible boolValue];
  }

  // Executes JavaScript on the window.location test page and returns whether
  // the no-op text is visible.  It is displayed 0.5 seconds after a button is
  // tapped, and can be used to verify that a navigation did not occur.
  bool IsNoOpTextVisible() {
    NSNumber* text_visible = base::mac::ObjCCastStrict<NSNumber>(
        ExecuteJavaScript(kNoOpCheckScript));
    return [text_visible boolValue];
  }

 private:
  GURL window_location_url_;
};

// Tests that calling window.location.assign() creates a new NavigationItem.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_Assign Assign
#else
#define MAYBE_Assign DISABLED_Assign
#endif
// TODO(crbug.com/721162): Enable this test on device.
TEST_F(WindowLocationTest, MAYBE_Assign) {
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
  GURL sample_url = web::test::HttpServer::MakeUrl(kSampleFileBasedURL);
  SetWindowLocationUrl(sample_url);
  ASSERT_TRUE(ExecuteBlockAndWaitForLoad(sample_url, ^{
    ASSERT_TRUE(web::test::TapWebViewElementWithId(web_state(),
                                                   kWindowLocationAssignID));
  }));

  // Verify that |sample_url| was loaded and that |about_blank_item| was pruned.
  EXPECT_EQ(sample_url, navigation_manager()->GetLastCommittedItem()->GetURL());
  EXPECT_EQ(NSNotFound, GetIndexOfNavigationItem(about_blank_item));
}

// Tests that calling window.location.assign() with an unresolvable URL loads
// about:blank.
TEST_F(WindowLocationTest, WindowLocationAssignUnresolvable) {
  // Attempt to call window.location.assign() using an unresolvable URL.
  GURL unresolvable_url("http:https:not a url");
  SetWindowLocationUrl(unresolvable_url);
  ASSERT_TRUE(
      web::test::TapWebViewElementWithId(web_state(), kWindowLocationAssignID));

  // Wait for the no-op text to appear.
  base::test::ios::WaitUntilCondition(^bool {
    return IsNoOpTextVisible();
  });
}

// Tests that calling window.location.replace() doesn't create a new
// NavigationItem.
// TODO(crbug.com/307072): Enable test when location.replace is fixed.
TEST_F(WindowLocationTest, DISABLED_Replace) {
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
  GURL sample_url = web::test::HttpServer::MakeUrl(kSampleFileBasedURL);
  SetWindowLocationUrl(sample_url);
  ASSERT_TRUE(ExecuteBlockAndWaitForLoad(sample_url, ^{
    ASSERT_TRUE(web::test::TapWebViewElementWithId(web_state(),
                                                   kWindowLocationReplaceID));
  }));

  // Verify that |sample_url| was loaded and that |about_blank_item| was pruned.
  web::NavigationItem* current_item =
      navigation_manager()->GetLastCommittedItem();
  EXPECT_EQ(sample_url, current_item->GetURL());
  EXPECT_EQ(GetIndexOfNavigationItem(current_item) + 1,
            GetIndexOfNavigationItem(about_blank_item));
}

// Tests that calling window.location.replace() with an unresolvable URL is a
// no-op.
TEST_F(WindowLocationTest, WindowLocationReplaceUnresolvable) {
  // Attempt to call window.location.assign() using an unresolvable URL.
  GURL unresolvable_url("http:https:not a url");
  SetWindowLocationUrl(unresolvable_url);
  ASSERT_TRUE(web::test::TapWebViewElementWithId(web_state(),
                                                 kWindowLocationReplaceID));

  // Wait for the no-op text to appear.
  base::test::ios::WaitUntilCondition(^bool {
    return IsNoOpTextVisible();
  });
}

// Tests that calling window.location.reload() causes an onload event to occur.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_WindowLocationReload WindowLocationReload
#else
#define MAYBE_WindowLocationReload DISABLED_WindowLocationReload
#endif
// TODO(crbug.com/721465): Enable this test on device.
TEST_F(WindowLocationTest, MAYBE_WindowLocationReload) {
  // Tap the window.location.reload() button.
  ASSERT_TRUE(ExecuteBlockAndWaitForLoad(window_location_url(), ^{
    ASSERT_TRUE(web::test::TapWebViewElementWithId(web_state(),
                                                   kWindowLocationReloadID));
  }));

  // Verify that |kOnLoadText| is displayed and that no additional
  // NavigationItems are added.
  EXPECT_TRUE(IsOnLoadTextVisible());
  EXPECT_EQ(1, navigation_manager()->GetItemCount());
}

// Tests that calling window.location.assign() creates a new NavigationItem.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_WindowLocationSetToDOMString WindowLocationSetToDOMString
#else
#define MAYBE_WindowLocationSetToDOMString DISABLED_WindowLocationSetToDOMString
#endif
// TODO(crbug.com/731740): This test is disabled because it occasionally times
// out on device.
TEST_F(WindowLocationTest, MAYBE_WindowLocationSetToDOMString) {
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
  GURL sample_url = web::test::HttpServer::MakeUrl(kSampleFileBasedURL);
  SetWindowLocationUrl(sample_url);
  ASSERT_TRUE(ExecuteBlockAndWaitForLoad(sample_url, ^{
    ASSERT_TRUE(web::test::TapWebViewElementWithId(
        web_state(), kWindowLocationSetToDOMStringID));
  }));

  // Verify that |sample_url| was loaded and that |about_blank_item| was pruned.
  EXPECT_EQ(sample_url, navigation_manager()->GetLastCommittedItem()->GetURL());
  EXPECT_EQ(NSNotFound, GetIndexOfNavigationItem(about_blank_item));
}
