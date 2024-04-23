// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/memory/ptr_util.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "ios/web/navigation/navigation_item_impl.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/test/web_view_interaction_test_util.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state.h"
#import "ios/web/test/web_int_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "url/url_canon.h"

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
const char kHistoryStateOperationsTestUrl[] = "/state_operations.html";

// Button IDs used in the window.location test page.
const char kPushStateId[] = "push-state";
const char kReplaceStateId[] = "replace-state";

// JavaScript functions on the history state test page.
NSString* const kUpdateStateParamsScriptFormat =
    @"updateStateParams('%s', '%s', '%s')";
const char kOnLoadCheckScript[] = "isOnLoadPlaceholderTextVisible()";
const char kNoOpCheckScript[] = "isNoOpPlaceholderTextVisible()";

// Wait timeout for state updates.
constexpr base::TimeDelta kWaitForStateUpdateTimeout = base::Seconds(5);

}  // namespace

// Test fixture for integration tests involving html5 window.history state
// operations.
class HistoryStateOperationsTest : public web::WebIntTest {
 protected:
  void SetUp() override {
    web::WebIntTest::SetUp();

    // Load the history state test page.
    test_server_ = std::make_unique<net::EmbeddedTestServer>();
    test_server_->ServeFilesFromSourceDirectory(
        base::FilePath("ios/testing/data/http_server_files/"));
    ASSERT_TRUE(test_server_->Start());

    state_operations_url_ =
        test_server_->GetURL(kHistoryStateOperationsTestUrl);
    ASSERT_TRUE(LoadUrl(state_operations_url()));
  }

  // The URL of the window.location test page.
  const GURL& state_operations_url() { return state_operations_url_; }

  // Reloads the page and waits for the load to finish.
  [[nodiscard]] bool Reload() {
    return ExecuteBlockAndWaitForLoad(GetLastCommittedItem()->GetURL(), ^{
      web_state()->GetNavigationManager()->Reload(web::ReloadType::NORMAL,
                                                  /*check_for_repost=*/false);
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
    web::test::ExecuteJavaScript(web_state(),
                                 base::SysNSStringToUTF8(set_params_script));
  }

  // Returns the state object returned by JavaScript.
  std::string GetJavaScriptState() {
    return web::test::ExecuteJavaScript(web_state(), "window.history.state")
        ->GetString();
  }

  // Executes JavaScript to check whether the onload text is visible.
  bool IsOnLoadTextVisible() {
    return web::test::ExecuteJavaScript(web_state(), kOnLoadCheckScript)
        ->GetBool();
  }

  // Executes JavaScript to check whether the no-op text is visible.
  bool IsNoOpTextVisible() {
    return web::test::ExecuteJavaScript(web_state(), kNoOpCheckScript)
        ->GetBool();
  }

  // Waits for the NoOp text to be visible.
  void WaitForNoOpText() {
    BOOL completed = base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForJSCompletionTimeout, ^{
          return IsNoOpTextVisible();
        });
    EXPECT_TRUE(completed) << "NoOp text failed to be visible.";
  }

  std::unique_ptr<net::EmbeddedTestServer> test_server_;

 private:
  GURL state_operations_url_;
};

// Tests that calling window.history.pushState() is a no-op for unresolvable
// URLs.
TEST_F(HistoryStateOperationsTest, NoOpPushUnresolvable) {
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
TEST_F(HistoryStateOperationsTest, NoOpReplaceUnresolvable) {
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
TEST_F(HistoryStateOperationsTest, NoOpPushDifferentScheme) {
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
TEST_F(HistoryStateOperationsTest, NoOpRelaceDifferentScheme) {
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
TEST_F(HistoryStateOperationsTest, NoOpPushDifferentOrigin) {
  // Perform a window.history.pushState() with a URL with a different origin.
  // This will clear the OnLoad and NoOp text, so checking below that the NoOp
  // text is displayed and the OnLoad text is empty ensures that no navigation
  // occurred as the result of the pushState() call.
  std::string empty_state;
  std::string empty_title;
  std::string new_port_string = base::NumberToString(test_server_->port() + 1);
  GURL::Replacements port_replacement;
  port_replacement.SetPortStr(new_port_string);
  GURL different_origin_url =
      state_operations_url().ReplaceComponents(port_replacement);
  ASSERT_TRUE(IsOnLoadTextVisible());
  SetStateParams(empty_state, empty_title, different_origin_url);
  ASSERT_TRUE(web::test::TapWebViewElementWithId(web_state(), kPushStateId));
  WaitForNoOpText();
}

// Tests that calling window.history.replaceState() is a no-op for URLs with a
// origin differing from that of the current page.
TEST_F(HistoryStateOperationsTest, NoOpReplaceDifferentOrigin) {
  // Perform a window.history.replaceState() with a URL with a different origin.
  // This will clear the OnLoad and NoOp text, so checking below that the NoOp
  // text is displayed and the OnLoad text is empty ensures that no navigation
  // occurred as the result of the pushState() call.
  std::string empty_state;
  std::string empty_title;
  std::string new_port_string = base::NumberToString(test_server_->port() + 1);
  GURL::Replacements port_replacement;
  port_replacement.SetPortStr(new_port_string);
  GURL different_origin_url =
      state_operations_url().ReplaceComponents(port_replacement);
  ASSERT_TRUE(IsOnLoadTextVisible());
  SetStateParams(empty_state, empty_title, different_origin_url);
  ASSERT_TRUE(web::test::TapWebViewElementWithId(web_state(), kReplaceStateId));
  WaitForNoOpText();
}

// Tests that calling window.history.replaceState() with a new state object
// replaces the state object for the current NavigationItem.
TEST_F(HistoryStateOperationsTest, StateReplacement) {
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
// TODO(crbug.com/40519813): Enable this test on device.
TEST_F(HistoryStateOperationsTest, MAYBE_StateReplacementReload) {
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
TEST_F(HistoryStateOperationsTest, StateReplacementBackForward) {
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

  // WebKit doesn't trigger onload on back. NavigationManagerImpl inherits
  // this behavior.
  WaitForNoOpText();

  BOOL completed = base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForJSCompletionTimeout, ^{
        return GetJavaScriptState() == new_state;
      });
  EXPECT_TRUE(completed) << "Failed to validate JavaScript state.";
}

// Tests that calling window.history.pushState() creates a new NavigationItem
// and prunes trailing items.
TEST_F(HistoryStateOperationsTest, PushState) {
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
TEST_F(HistoryStateOperationsTest, ReplaceStatePostRequest) {
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
TEST_F(HistoryStateOperationsTest, ReplaceStateNoHashChangeEvent) {
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
TEST_F(HistoryStateOperationsTest, ReplaceStateThenReload) {
  GURL url = test_server_->GetURL("/onload_replacestate_reload.html");
  ASSERT_TRUE(LoadUrl(url));
  GURL new_url = test_server_->GetURL("/pony.html");
  BOOL completed = base::test::ios::WaitUntilConditionOrTimeout(
      kWaitForStateUpdateTimeout, ^{
        return GetLastCommittedItem()->GetURL() == new_url;
      });
  EXPECT_TRUE(completed);
}
