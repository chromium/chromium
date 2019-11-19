// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "base/test/scoped_feature_list.h"
#include "ios/web/common/features.h"
#import "ios/web/navigation/navigation_item_impl.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "ios/web/public/test/http_server/http_server_util.h"
#import "ios/web/public/test/web_view_interaction_test_util.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state.h"
#import "ios/web/test/web_int_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "url/url_canon.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::ASCIIToUTF16;

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
const char kHistoryStateOperationsTestUrl[] =
    "http://ios/testing/data/http_server_files/state_operations.html";

// Button IDs used in the window.location test page.
const char kPushStateId[] = "push-state";
const char kReplaceStateId[] = "replace-state";

// JavaScript functions on the history state test page.
NSString* const kUpdateStateParamsScriptFormat =
    @"updateStateParams('%s', '%s', '%s')";
NSString* const kOnLoadCheckScript = @"isOnLoadPlaceholderTextVisible()";
NSString* const kNoOpCheckScript = @"isNoOpPlaceholderTextVisible()";

// Wait timeout for state updates.
const NSTimeInterval kWaitForStateUpdateTimeout = 5.0;

// HistoryStateOperationsTest is parameterized on this enum to test both
// LegacyNavigationManager and WKBasedNavigationManager.
enum class NavigationManagerChoice {
  LEGACY,
  WK_BASED,
};

}  // namespace

// Test fixture for integration tests involving html5 window.history state
// operations.
class HistoryStateOperationsTest
    : public web::WebIntTest,
      public ::testing::WithParamInterface<NavigationManagerChoice> {
 protected:
  void SetUp() override {
    if (GetParam() == NavigationManagerChoice::LEGACY) {
      scoped_feature_list_.InitAndDisableFeature(
          web::features::kSlimNavigationManager);
    } else {
      scoped_feature_list_.InitAndEnableFeature(
          web::features::kSlimNavigationManager);
    }
    web::WebIntTest::SetUp();

    // History state tests use file-based test pages.
    web::test::SetUpFileBasedHttpServer();

    // Load the history state test page.
    state_operations_url_ =
        web::test::HttpServer::MakeUrl(kHistoryStateOperationsTestUrl);
    ASSERT_TRUE(LoadUrl(state_operations_url()));
  }

  // The URL of the window.location test page.
  const GURL& state_operations_url() { return state_operations_url_; }

  // Reloads the page and waits for the load to finish.
  bool Reload() WARN_UNUSED_RESULT {
    return ExecuteBlockAndWaitForLoad(GetLastCommittedItem()->GetURL(), ^{
      // TODO(crbug.com/677364): Use NavigationManager::Reload() once it no
      // longer requires a web delegate.
      web_state()->ExecuteJavaScript(ASCIIToUTF16("window.location.reload()"));
    });
  }

  // Sets the parameters to use for state operations on the test page.  This
  // function executes a script that populates JavaScript values on the test
  // page.  When the "push-state" or "replace-state" buttons are tapped, these
  // parameters will be passed to their corresponding JavaScript function calls.
  void SetStateParams(const std::string& state_object,
                      const std::string& title,
                      const GURL& url) {
    ASSERT_EQ(state_operations_url(), GetLastCommittedItem()->GetURL());
    std::string url_spec = url.possibly_invalid_spec();
    NSString* set_params_script = [NSString
        stringWithFormat:kUpdateStateParamsScriptFormat, state_object.c_str(),
                         title.c_str(), url_spec.c_str()];
    ExecuteJavaScript(set_params_script);
  }

  // Returns the state object returned by JavaScript.
  std::string GetJavaScriptState() {
    return base::SysNSStringToUTF8(ExecuteJavaScript(@"window.history.state"));
  }

  // Executes JavaScript to check whether the onload text is visible.
  bool IsOnLoadTextVisible() {
    return [ExecuteJavaScript(kOnLoadCheckScript) boolValue];
  }

  // Executes JavaScript to check whether the no-op text is visible.
  bool IsNoOpTextVisible() {
    return [ExecuteJavaScript(kNoOpCheckScript) boolValue];
  }

  // Waits for the NoOp text to be visible.
  void WaitForNoOpText() {
    BOOL completed = base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForJSCompletionTimeout, ^{
          return IsNoOpTextVisible();
        });
    EXPECT_TRUE(completed) << "NoOp text failed to be visible.";
  }

 private:
  GURL state_operations_url_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that calling window.history.pushState() is a no-op for unresolvable
// URLs.
TEST_P(HistoryStateOperationsTest, NoOpPushUnresolvable) {
  // Perform a window.history.pushState() with an unresolvable URL.  This will
  // clear the OnLoad and NoOp text, so checking below that the NoOp text is
  // displayed and the OnLoad text is empty ensures that no navigation occurred
  // as the result of the pushState() call.
  std::string empty_state;
  std::string empty_title;
  GURL unresolvable_url("http://www.google.invalid");
  SetStateParams(empty_state, empty_title, unresolvable_url);
  ASSERT_TRUE(web::test::TapWebViewElementWithId(web_state(), kPushStateId));
  WaitForNoOpText();
}

// Tests that calling window.history.replaceState() is a no-op for unresolvable
// URLs.
TEST_P(HistoryStateOperationsTest, NoOpReplaceUnresolvable) {
  // Perform a window.history.replaceState() with an unresolvable URL.  This
  // will clear the OnLoad and NoOp text, so checking below that the NoOp text
  // is displayed and the OnLoad text is empty ensures that no navigation
  // occurred as the result of the pushState() call.
  std::string empty_state;
  std::string empty_title;
  GURL unresolvable_url("http://www.google.invalid");
  SetStateParams(empty_state, empty_title, unresolvable_url);
  ASSERT_TRUE(web::test::TapWebViewElementWithId(web_state(), kReplaceStateId));
  WaitForNoOpText();
}

// Tests that calling window.history.pushState() is a no-op for URLs with a
// different scheme.
TEST_P(HistoryStateOperationsTest, NoOpPushDifferentScheme) {
  // Perform a window.history.pushState() with a URL with a different scheme.
  // This will clear the OnLoad and NoOp text, so checking below that the NoOp
  // text is displayed and the OnLoad text is empty ensures that no navigation
  // occurred as the result of the pushState() call.
  std::string empty_state;
  std::string empty_title;
  GURL different_scheme_url("https://google.com");
  ASSERT_TRUE(IsOnLoadTextVisible());
  SetStateParams(empty_state, empty_title, different_scheme_url);
  ASSERT_TRUE(web::test::TapWebViewElementWithId(web_state(), kPushStateId));
  WaitForNoOpText();
}

// Tests that calling window.history.replaceState() is a no-op for URLs with a
// different scheme.
TEST_P(HistoryStateOperationsTest, NoOpRelaceDifferentScheme) {
  // Perform a window.history.replaceState() with a URL with a different scheme.
  // This will clear the OnLoad and NoOp text, so checking below that the NoOp
  // text is displayed and the OnLoad text is empty ensures that no navigation
  // occurred as the result of the pushState() call.
  std::string empty_state;
  std::string empty_title;
  GURL different_scheme_url("https://google.com");
  ASSERT_TRUE(IsOnLoadTextVisible());
  SetStateParams(empty_state, empty_title, different_scheme_url);
  ASSERT_TRUE(web::test::TapWebViewElementWithId(web_state(), kReplaceStateId));
  WaitForNoOpText();
}

// Tests that calling window.history.pushState() is a no-op for URLs with a
// origin differing from that of the current page.
TEST_P(HistoryStateOperationsTest, NoOpPushDifferentOrigin) {
  // Perform a window.history.pushState() with a URL with a different origin.
  // This will clear the OnLoad and NoOp text, so checking below that the NoOp
  // text is displayed and the OnLoad text is empty ensures that no navigation
  // occurred as the result of the pushState() call.
  std::string empty_state;
  std::string empty_title;
  std::string new_port_string = base::NumberToString(
      web::test::HttpServer::GetSharedInstance().GetPort() + 1);
  url::Replacements<char> port_replacement;
  port_replacement.SetPort(new_port_string.c_str(),
                           url::Component(0, new_port_string.length()));
  GURL different_origin_url =
      state_operations_url().ReplaceComponents(port_replacement);
  ASSERT_TRUE(IsOnLoadTextVisible());
  SetStateParams(empty_state, empty_title, different_origin_url);
  ASSERT_TRUE(web::test::TapWebViewElementWithId(web_state(), kPushStateId));
  WaitForNoOpText();
}

// Tests that calling window.history.replaceState() is a no-op for URLs with a
// origin differing from that of the current page.
TEST_P(HistoryStateOperationsTest, NoOpReplaceDifferentOrigin) {
  // Perform a window.history.replaceState() with a URL with a different origin.
  // This will clear the OnLoad and NoOp text, so checking below that the NoOp
  // text is displayed and the OnLoad text is empty ensures that no navigation
  // occurred as the result of the pushState() call.
  std::string empty_state;
  std::string empty_title;
  std::string new_port_string = base::NumberToString(
      web::test::HttpServer::GetSharedInstance().GetPort() + 1);
  url::Replacements<char> port_replacement;
  port_replacement.SetPort(new_port_string.c_str(),
                           url::Component(0, new_port_string.length()));
  GURL different_origin_url =
      state_operations_url().ReplaceComponents(port_replacement);
  ASSERT_TRUE(IsOnLoadTextVisible());
  SetStateParams(empty_state, empty_title, different_origin_url);
  ASSERT_TRUE(web::test::TapWebViewElementWithId(web_state(), kReplaceStateId));
  WaitForNoOpText();
}

// Tests that calling window.history.replaceState() with only a new title
// successfully replaces the current NavigationItem's title.
// TODO(crbug.com/677356): Enable this test once the NavigationItem's title is
// updated from within the web layer.
TEST_P(HistoryStateOperationsTest, DISABLED_TitleReplacement) {
  // Navigate to about:blank then navigate back to the test page.  The created
  // NavigationItem can be used later to verify that the title is replaced
  // rather than pushed.
  GURL about_blank("about:blank");
  ASSERT_TRUE(LoadUrl(about_blank));
  web::NavigationItem* about_blank_item = GetLastCommittedItem();
  EXPECT_TRUE(ExecuteBlockAndWaitForLoad(state_operations_url(), ^{
    navigation_manager()->GoBack();
  }));
  EXPECT_EQ(state_operations_url(), GetLastCommittedItem()->GetURL());
  // Set up the state parameters and tap the replace state button.
  std::string empty_state;
  std::string new_title("NEW TITLE");
  GURL empty_url;
  SetStateParams(empty_state, new_title, empty_url);
  ASSERT_TRUE(web::test::TapWebViewElementWithId(web_state(), kReplaceStateId));
  // Wait for the title to be reflected in the NavigationItem.
  BOOL completed = base::test::ios::WaitUntilConditionOrTimeout(
      kWaitForStateUpdateTimeout, ^{
        return GetLastCommittedItem()->GetTitle() == ASCIIToUTF16(new_title);
      });
  EXPECT_TRUE(completed) << "Failed to validate NavigationItem title.";
  // Verify that the forward navigation was not pruned.
  EXPECT_EQ(GetIndexOfNavigationItem(GetLastCommittedItem()) + 1,
            GetIndexOfNavigationItem(about_blank_item));
}

// Tests that calling window.history.replaceState() with a new state object
// replaces the state object for the current NavigationItem.
TEST_P(HistoryStateOperationsTest, StateReplacement) {
  // Navigate to about:blank then navigate back to the test page.  The created
  // NavigationItem can be used later to verify that the state is replaced
  // rather than pushed.
  GURL about_blank("about:blank");
  ASSERT_TRUE(LoadUrl(about_blank));
  web::NavigationItem* about_blank_item = GetLastCommittedItem();
  EXPECT_TRUE(ExecuteBlockAndWaitForLoad(state_operations_url(), ^{
    navigation_manager()->GoBack();
  }));
  ASSERT_EQ(state_operations_url(), GetLastCommittedItem()->GetURL());
  // Set up the state parameters and tap the replace state button.
  std::string new_state("STATE OBJECT");
  std::string empty_title;
  GURL empty_url;
  SetStateParams(new_state, empty_title, empty_url);
  ASSERT_TRUE(web::test::TapWebViewElementWithId(web_state(), kReplaceStateId));
  // Verify that the state is reflected in the JavaScript context.
  BOOL verify_java_script_context_completed =
      base::test::ios::WaitUntilConditionOrTimeout(
          base::test::ios::kWaitForJSCompletionTimeout, ^{
            return GetJavaScriptState() == new_state;
          });
  EXPECT_TRUE(verify_java_script_context_completed)
      << "Failed to validate JavaScript state.";
  // Verify that the state is reflected in the latest NavigationItem.
  std::string serialized_state("\"STATE OBJECT\"");
  BOOL verify_navigation_item_completed =
      base::test::ios::WaitUntilConditionOrTimeout(
          kWaitForStateUpdateTimeout, ^{
            web::NavigationItemImpl* item =
                static_cast<web::NavigationItemImpl*>(GetLastCommittedItem());
            std::string item_state =
                base::SysNSStringToUTF8(item->GetSerializedStateObject());
            return item_state == serialized_state;
          });
  EXPECT_TRUE(verify_navigation_item_completed)
      << "Failed to validate NavigationItem state.";
  // Verify that the forward navigation was not pruned.
  EXPECT_EQ(GetIndexOfNavigationItem(GetLastCommittedItem()) + 1,
            GetIndexOfNavigationItem(about_blank_item));
}

// Tests that the state object is reset to the correct value after reloading a
// page whose state has been replaced.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_StateReplacementReload StateReplacementReload
#else
#define MAYBE_StateReplacementReload DISABLED_StateReplacementReload
#endif
// TODO(crbug.com/720381): Enable this test on device.
TEST_P(HistoryStateOperationsTest, MAYBE_StateReplacementReload) {
  // Set up the state parameters and tap the replace state button.
  std::string new_state("STATE OBJECT");
  std::string empty_title;
  GURL empty_url;
  SetStateParams(new_state, empty_title, empty_url);
  ASSERT_TRUE(web::test::TapWebViewElementWithId(web_state(), kReplaceStateId));
  // Reload the page and check that the state object is present.
  EXPECT_TRUE(Reload());
  ASSERT_TRUE(IsOnLoadTextVisible());
  BOOL completed = base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForJSCompletionTimeout, ^{
        return GetJavaScriptState() == new_state;
      });
  EXPECT_TRUE(completed) << "Failed to validate JavaScript state.";
}

// Tests that the state object is correctly set for a page after a back/forward
// navigation.
TEST_P(HistoryStateOperationsTest, StateReplacementBackForward) {
  // Navigate to about:blank then navigate back to the test page.  The created
  // NavigationItem can be used later to verify that the state is replaced
  // rather than pushed.
  GURL about_blank("about:blank");
  ASSERT_TRUE(LoadUrl(about_blank));
  ASSERT_TRUE(ExecuteBlockAndWaitForLoad(state_operations_url(), ^{
    navigation_manager()->GoBack();
  }));
  ASSERT_EQ(state_operations_url(), GetLastCommittedItem()->GetURL());
  // Set up the state parameters and tap the replace state button.
  std::string new_state("STATE OBJECT");
  std::string empty_title("");
  GURL empty_url("");
  SetStateParams(new_state, empty_title, empty_url);
  ASSERT_TRUE(web::test::TapWebViewElementWithId(web_state(), kReplaceStateId));
  // Go forward and back, then check that the state object is present.
  ASSERT_TRUE(ExecuteBlockAndWaitForLoad(about_blank, ^{
    navigation_manager()->GoForward();
  }));
  ASSERT_TRUE(ExecuteBlockAndWaitForLoad(state_operations_url(), ^{
    navigation_manager()->GoBack();
  }));

  if (GetParam() == NavigationManagerChoice::LEGACY) {
    ASSERT_TRUE(IsOnLoadTextVisible());
  } else {
    // WebKit doesn't trigger onload on back. WKBasedNavigationManager inherits
    // this behavior.
    WaitForNoOpText();
  }

  BOOL completed = base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForJSCompletionTimeout, ^{
        return GetJavaScriptState() == new_state;
      });
  EXPECT_TRUE(completed) << "Failed to validate JavaScript state.";
}

// Tests that calling window.history.pushState() creates a new NavigationItem
// and prunes trailing items.
TEST_P(HistoryStateOperationsTest, PushState) {
  // Navigate to about:blank then navigate back to the test page.  The created
  // NavigationItem can be used later to verify that the state is replaced
  // rather than pushed.
  GURL about_blank("about:blank");
  ASSERT_TRUE(LoadUrl(about_blank));
  web::NavigationItem* about_blank_item = GetLastCommittedItem();
  ASSERT_TRUE(ExecuteBlockAndWaitForLoad(state_operations_url(), ^{
    navigation_manager()->GoBack();
  }));
  ASSERT_EQ(state_operations_url(), GetLastCommittedItem()->GetURL());
  web::NavigationItem* non_pushed_item = GetLastCommittedItem();
  // Set up the state parameters and tap the replace state button.
  std::string empty_state;
  std::string empty_title;
  GURL new_url = state_operations_url().Resolve("path");
  SetStateParams(empty_state, empty_title, new_url);
  ASSERT_TRUE(web::test::TapWebViewElementWithId(web_state(), kPushStateId));
  // Verify that the url with the path is pushed.
  BOOL completed = base::test::ios::WaitUntilConditionOrTimeout(
      kWaitForStateUpdateTimeout, ^{
        return GetLastCommittedItem()->GetURL() == new_url;
      });
  EXPECT_TRUE(completed) << "Failed to validate current url.";
  // Verify that a new NavigationItem was created and that the forward item was
  // pruned.
  EXPECT_EQ(GetIndexOfNavigationItem(non_pushed_item) + 1,
            GetIndexOfNavigationItem(GetLastCommittedItem()));
  EXPECT_EQ(NSNotFound, GetIndexOfNavigationItem(about_blank_item));
}

// Tests that performing a replaceState() on a page created with a POST request
// resets the page to a GET request.
TEST_P(HistoryStateOperationsTest, ReplaceStatePostRequest) {
  // Add POST data to the current NavigationItem.
  NSData* post_data = [NSData data];
  static_cast<web::NavigationItemImpl*>(GetLastCommittedItem())
      ->SetPostData(post_data);
  ASSERT_TRUE(GetLastCommittedItem()->HasPostData());
  // Set up the state parameters and tap the replace state button.
  std::string new_state("STATE OBJECT");
  std::string empty_title;
  GURL new_url = state_operations_url().Resolve("path");
  SetStateParams(new_state, empty_title, new_url);
  ASSERT_TRUE(web::test::TapWebViewElementWithId(web_state(), kReplaceStateId));
  // Verify that url has been replaced.
  BOOL completed = base::test::ios::WaitUntilConditionOrTimeout(
      kWaitForStateUpdateTimeout, ^{
        return GetLastCommittedItem()->GetURL() == new_url;
      });
  EXPECT_TRUE(completed) << "Failed to validate current url.";
  // Verify that the NavigationItem no longer has POST data.
  EXPECT_FALSE(GetLastCommittedItem()->HasPostData());
}

// Tests that performing a replaceState() on a page where only the URL fragment
// is updated does not trigger a hashchange event.
TEST_P(HistoryStateOperationsTest, ReplaceStateNoHashChangeEvent) {
  // Set up the state parameters and tap the replace state button.
  std::string empty_state;
  std::string empty_title;
  GURL new_url = state_operations_url().Resolve("#hash");
  SetStateParams(empty_state, empty_title, new_url);
  ASSERT_TRUE(web::test::TapWebViewElementWithId(web_state(), kReplaceStateId));
  // Verify that url has been replaced.
  BOOL completed = base::test::ios::WaitUntilConditionOrTimeout(
      kWaitForStateUpdateTimeout, ^{
        return GetLastCommittedItem()->GetURL() == new_url;
      });
  EXPECT_TRUE(completed) << "Failed to validate current url.";
  // Verify that the hashchange event was not fired.
  EXPECT_FALSE(static_cast<web::NavigationItemImpl*>(GetLastCommittedItem())
                   ->IsCreatedFromHashChange());
}

// Regression test for crbug.com/788464.
TEST_P(HistoryStateOperationsTest, ReplaceStateThenReload) {
  GURL url = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/"
      "onload_replacestate_reload.html");
  ASSERT_TRUE(LoadUrl(url));
  GURL new_url = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/pony.html");
  BOOL completed = base::test::ios::WaitUntilConditionOrTimeout(
      kWaitForStateUpdateTimeout, ^{
        return GetLastCommittedItem()->GetURL() == new_url;
      });
  EXPECT_TRUE(completed);
}

INSTANTIATE_TEST_SUITE_P(ProgrammaticHistoryStateOperationsTest,
                         HistoryStateOperationsTest,
                         ::testing::Values(NavigationManagerChoice::LEGACY,
                                           NavigationManagerChoice::WK_BASED));
