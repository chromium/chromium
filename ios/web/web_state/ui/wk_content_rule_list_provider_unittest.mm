// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/wk_content_rule_list_provider.h"

#import <WebKit/WebKit.h>

#import <set>
#import <string>
#import <string_view>
#import <vector>

#import "base/apple/foundation_util.h"
#import "base/files/scoped_temp_dir.h"
#import "base/functional/callback_helpers.h"
#import "base/strings/strcat.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/bind.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/test/test_url_constants.h"
#import "ios/web/web_state/ui/wk_content_rule_list_util.h"
#import "net/socket/stream_socket.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/embedded_test_server_connection_listener.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

using base::test::TestFuture;

// Serves a minimal HTML document for any request, so that a `WKWebView` can
// load pages from a scheme WebKit does not handle natively (such as the WebUI
// schemes declared by the embedder).
@interface FakeWebUISchemeHandler : NSObject <WKURLSchemeHandler>
@end

@implementation FakeWebUISchemeHandler

- (void)webView:(WKWebView*)webView
    startURLSchemeTask:(id<WKURLSchemeTask>)urlSchemeTask {
  NSData* html = [@"<html><body>Privileged</body></html>"
      dataUsingEncoding:NSUTF8StringEncoding];
  NSURLResponse* response =
      [[NSURLResponse alloc] initWithURL:urlSchemeTask.request.URL
                                MIMEType:@"text/html"
                   expectedContentLength:html.length
                        textEncodingName:@"UTF-8"];
  [urlSchemeTask didReceiveResponse:response];
  [urlSchemeTask didReceiveData:html];
  [urlSchemeTask didFinish];
}

- (void)webView:(WKWebView*)webView
    stopURLSchemeTask:(id<WKURLSchemeTask>)urlSchemeTask {
}

@end

namespace web {

// A valid, but minimal, content rule list for testing.
constexpr char kValidTestRuleListJson[] = R"([
  {
    "trigger": {"url-filter": ".*"},
    "action": {"type": "block"}
  }
])";
// An invalid JSON string for testing compilation failures.
constexpr char kInvalidTestRuleListJson[] = "this is not valid json";

class WKContentRuleListProviderTest : public PlatformTest {
 public:
  WKContentRuleListProviderTest() {
    CHECK(temp_dir_.CreateUniqueTempDir());
    provider_ =
        std::make_unique<WKContentRuleListProvider>(temp_dir_.GetPath());
  }

 protected:
  // Helper to check for a rule list's presence in the store and wait.
  // Returns a gtest AssertionResult for use with EXPECT_TRUE.
  [[nodiscard]] testing::AssertionResult CheckStoreForRuleListSync(
      const std::string& identifier,
      bool expect_found = true) {
    TestFuture<WKContentRuleList*, NSError*> future;
    // Create a temporary store that points to the same path as the provider's
    // store to independently verify the file system state.
    WKContentRuleListStore* store = [WKContentRuleListStore
        storeWithURL:base::apple::FilePathToNSURL(temp_dir_.GetPath())];
    CHECK(store);

    void (^completion_block)(WKContentRuleList*, NSError*) =
        base::CallbackToBlock(future.GetCallback());
    [store
        lookUpContentRuleListForIdentifier:base::SysUTF8ToNSString(identifier)
                         completionHandler:completion_block];
    auto [rule_list, error] = future.Get();

    const bool was_found = (rule_list != nil);

    // First, handle any unexpected WebKit errors. This is always a failure.
    if (error && (![error.domain isEqualToString:WKErrorDomain] ||
                  error.code != WKErrorContentRuleListStoreLookUpFailed)) {
      return testing::AssertionFailure()
             << "Unexpected error looking up content rule list with identifier "
                "'"
             << identifier
             << "': " << base::SysNSStringToUTF8(error.description);
    }

    // Now, check if the outcome matches the expectation.
    if (was_found == expect_found) {
      return testing::AssertionSuccess();
    }

    // The outcome did not match the expectation, return a specific failure.
    if (expect_found) {
      return testing::AssertionFailure()
             << "Expected to find content rule list with identifier '"
             << identifier << "', but it was not found.";
    } else {
      return testing::AssertionFailure()
             << "Expected content rule list with identifier '" << identifier
             << "' to be absent, but it was found.";
    }
  }

  // Helper to create or update a rule list and verify the operation's success.
  [[nodiscard]] testing::AssertionResult UpdateRuleListSync(
      const WKContentRuleListProvider::RuleListKey& key,
      const std::string& rules_list) {
    TestFuture<NSError*> future;
    provider_->UpdateRuleList(key, rules_list, future.GetCallback());
    NSError* error = future.Get();
    if (error) {
      return testing::AssertionFailure()
             << "UpdateRuleList failed with error: "
             << base::SysNSStringToUTF8(error.description);
    }
    return testing::AssertionSuccess();
  }

  // Helper to remove a rule list and verify the operation's success.
  [[nodiscard]] testing::AssertionResult RemoveRuleListSync(
      const WKContentRuleListProvider::RuleListKey& key) {
    TestFuture<NSError*> future;
    provider_->RemoveRuleList(key, future.GetCallback());
    NSError* error = future.Get();
    if (error) {
      return testing::AssertionFailure()
             << "RemoveRuleList failed with error: "
             << base::SysNSStringToUTF8(error.description);
    }
    return testing::AssertionSuccess();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  base::ScopedTempDir temp_dir_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<WKContentRuleListProvider> provider_;
};

// Tests that a new list can be successfully created and then removed.
TEST_F(WKContentRuleListProviderTest, CreationAndRemoval) {
  const std::string key = "test_key";
  // 1. Create a valid list.
  ASSERT_TRUE(UpdateRuleListSync(key, kValidTestRuleListJson));
  EXPECT_TRUE(CheckStoreForRuleListSync(key));

  // 2. Remove the list.
  EXPECT_TRUE(RemoveRuleListSync(key));
  EXPECT_TRUE(CheckStoreForRuleListSync(key, /*expect_found=*/false));
}

// Tests UMA logging for successful creation and removal.
TEST_F(WKContentRuleListProviderTest, UMA_CreationAndRemovalSuccess) {
  const std::string key = "uma_test_key";
  ASSERT_TRUE(UpdateRuleListSync(key, kValidTestRuleListJson));

  histogram_tester_.ExpectUniqueSample(
      "IOS.ContentRuleListProvider.Compile.Success." + key, true, 1);
  histogram_tester_.ExpectTotalCount(
      "IOS.ContentRuleListProvider.Compile.Time." + key, 1);

  ASSERT_TRUE(RemoveRuleListSync(key));
  histogram_tester_.ExpectUniqueSample(
      "IOS.ContentRuleListProvider.Remove.Success." + key, true, 1);
}

// Tests UMA logging for a failed compilation.
TEST_F(WKContentRuleListProviderTest, UMA_CreationFailure) {
  const std::string key = "uma_test_key";
  TestFuture<NSError*> future;
  provider_->UpdateRuleList(key, kInvalidTestRuleListJson,
                            future.GetCallback());
  ASSERT_NE(nil, future.Get());

  histogram_tester_.ExpectUniqueSample(
      "IOS.ContentRuleListProvider.Compile.Success." + key, false, 1);
  histogram_tester_.ExpectTotalCount(
      "IOS.ContentRuleListProvider.Compile.Time." + key, 0);
}

// Tests that an existing list can be successfully updated.
TEST_F(WKContentRuleListProviderTest, UpdateExistingList) {
  const std::string key = "test_key";
  // 1. Create an initial list.
  ASSERT_TRUE(UpdateRuleListSync(key, kValidTestRuleListJson));
  ASSERT_TRUE(CheckStoreForRuleListSync(key));

  // 2. Update the list with new (but still valid) rules.
  EXPECT_TRUE(UpdateRuleListSync(key, kValidTestRuleListJson));
  EXPECT_TRUE(CheckStoreForRuleListSync(key));
}

// Tests that attempting to create an invalid list fails gracefully.
TEST_F(WKContentRuleListProviderTest, CreationFailure) {
  const std::string key = "test_key";
  TestFuture<NSError*> future;
  provider_->UpdateRuleList(key, kInvalidTestRuleListJson,
                            future.GetCallback());

  NSError* error = future.Get();
  ASSERT_NE(nil, error);
  EXPECT_NSEQ(WKErrorDomain, error.domain);
  EXPECT_EQ(WKErrorContentRuleListStoreCompileFailed, error.code);
  EXPECT_TRUE(CheckStoreForRuleListSync(key, /*expect_found=*/false));
}

// Tests that a compilation failure for one list does not affect other existing
// lists.
TEST_F(WKContentRuleListProviderTest, UpdateFailureDoesNotAffectExistingLists) {
  const std::string key = "valid_key";
  // 1. Create a valid list.
  ASSERT_TRUE(UpdateRuleListSync(key, kValidTestRuleListJson));
  ASSERT_TRUE(CheckStoreForRuleListSync(key));

  // 2. Attempt to update the valid list with invalid JSON.
  TestFuture<NSError*> future;
  provider_->UpdateRuleList(key, kInvalidTestRuleListJson,
                            future.GetCallback());

  // 3. Verify the outcome.
  // The update should fail.
  ASSERT_NE(nil, future.Get());
  // The original valid list should still be present in the store.
  EXPECT_TRUE(CheckStoreForRuleListSync(key));
}

// Static List Compilation Tests

// Tests that the JSON from CreateLocalBlockingJsonRuleList compiles.
TEST_F(WKContentRuleListProviderTest, StaticBlockLocalListCompiles) {
  const std::string key = "BlockLocalResources";
  std::string rules =
      base::SysNSStringToUTF8(CreateLocalBlockingJsonRuleList());
  EXPECT_TRUE(UpdateRuleListSync(key, rules));
  EXPECT_TRUE(CheckStoreForRuleListSync(key));
}

// Tests that the JSON from CreateMixedContentAutoUpgradeJsonRuleList compiles.
TEST_F(WKContentRuleListProviderTest, StaticMixedContentListCompiles) {
  const std::string key = "MixedContentUpgrade";
  std::string rules =
      base::SysNSStringToUTF8(CreateMixedContentAutoUpgradeJsonRuleList());
  EXPECT_TRUE(UpdateRuleListSync(key, rules));
  EXPECT_TRUE(CheckStoreForRuleListSync(key));
}

// Tests that multiple concurrent update requests all succeed.
TEST_F(WKContentRuleListProviderTest, MultipleConcurrentUpdatesSucceed) {
  const int kUpdateCount = 3;
  std::vector<TestFuture<NSError*>> futures(kUpdateCount);
  std::vector<std::string> keys;

  // Start all updates without waiting.
  for (int i = 0; i < kUpdateCount; ++i) {
    std::string key = "key_" + base::NumberToString(i);
    keys.push_back(key);
    provider_->UpdateRuleList(key, kValidTestRuleListJson,
                              futures[i].GetCallback());
  }

  // Verify that all updates completed successfully.
  for (int i = 0; i < kUpdateCount; ++i) {
    EXPECT_EQ(nil, futures[i].Get());
    EXPECT_TRUE(CheckStoreForRuleListSync(keys[i]));
  }
}

namespace {

// Returns a JavaScript snippet that attempts an outbound network connection
// to `target_url` using the appropriate API for `scheme` (`Image` for
// `http`/`https`, `WebSocket` for `ws`/`wss`) and returns a Promise that
// resolves upon completion.
NSString* CreateProbeScript(std::string_view scheme,
                            const std::string& target_url) {
  if (scheme == "ws" || scheme == "wss") {
    return [NSString
        stringWithFormat:@"return new Promise((resolve) => {"
                          "  let ws = new WebSocket('%s');"
                          "  ws.onerror = () => resolve('error');"
                          "  ws.onopen = () => resolve('open');"
                          "  setTimeout(() => resolve('timeout'), 1000);"
                          "});",
                         target_url.c_str()];
  }
  return [NSString
      stringWithFormat:@"return new Promise((resolve) => {"
                        "  let img = new Image();"
                        "  img.onload = () => resolve('loaded');"
                        "  img.onerror = () => resolve('error');"
                        "  img.src = '%s';"
                        "  setTimeout(() => resolve('timeout'), 1000);"
                        "});",
                       target_url.c_str()];
}

// A lightweight connection listener that tracks accepted sockets atomically
// across threads without thread hops.
class TestConnectionListener
    : public net::test_server::EmbeddedTestServerConnectionListener {
 public:
  std::unique_ptr<net::StreamSocket> AcceptedSocket(
      std::unique_ptr<net::StreamSocket> socket) override {
    socket_count_++;
    return socket;
  }
  void ReadFromSocket(const net::StreamSocket& socket, int rv) override {}

  int socket_count() const { return socket_count_.load(); }
  void Reset() { socket_count_ = 0; }

 private:
  std::atomic<int> socket_count_ = 0;
};

}  // namespace

// Parameterized test fixture covering all URL schemes that should be blocked
// from privileged pages.
class WKContentRuleListProviderSchemeTest
    : public WKContentRuleListProviderTest,
      public ::testing::WithParamInterface<const char*> {
 protected:
  // Registers a `WebClient` declaring app-specific schemes (`testwebui://`,
  // the test equivalent of the production `chrome://` scheme) so that pages
  // served from those schemes are covered by the generated rule list.
  ScopedTestingWebClient web_client_{std::make_unique<FakeWebClient>()};
};

// Tests that the local blocking content rule list blocks outbound network
// connections for all disallowed schemes (`http`, `https`, `ws`, `wss`) from
// privileged pages (`file://` and app-specific WebUI pages), while permitting
// connections from unprivileged pages.
TEST_P(WKContentRuleListProviderSchemeTest,
       LocalBlockingRuleBlocksSchemeFromPrivilegedPages) {
  const std::string_view scheme = GetParam();

  const std::string key = "BlockLocalResources";
  ASSERT_TRUE(UpdateRuleListSync(
      key, base::SysNSStringToUTF8(CreateLocalBlockingJsonRuleList())));

  WKContentRuleListStore* store = [WKContentRuleListStore
      storeWithURL:base::apple::FilePathToNSURL(temp_dir_.GetPath())];
  TestFuture<WKContentRuleList*, NSError*> lookup_future;
  [store lookUpContentRuleListForIdentifier:base::SysUTF8ToNSString(key)
                          completionHandler:base::CallbackToBlock(
                                                lookup_future.GetCallback())];
  auto [rule_list, lookup_error] = lookup_future.Get();
  ASSERT_TRUE(rule_list);

  WKWebViewConfiguration* configuration = [[WKWebViewConfiguration alloc] init];
  [configuration.userContentController addContentRuleList:rule_list];
  // WebKit cannot load WebUI scheme URLs on its own, so the page content is
  // served by a scheme handler, which gives the page a real WebUI origin.
  [configuration setURLSchemeHandler:[[FakeWebUISchemeHandler alloc] init]
                        forURLScheme:base::SysUTF8ToNSString(kTestWebUIScheme)];
  WKWebView* web_view = [[WKWebView alloc] initWithFrame:CGRectZero
                                           configuration:configuration];

  net::EmbeddedTestServer test_server;
  TestConnectionListener connection_listener;
  test_server.SetConnectionListener(&connection_listener);
  test_server.RegisterRequestHandler(base::BindRepeating(
      [](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        return std::make_unique<net::test_server::BasicHttpResponse>();
      }));
  ASSERT_TRUE(test_server.Start());

  // The probe target only differs from the test server URL by its scheme, so
  // that `ws`/`wss` probes reach the same endpoint as `http`/`https` probes.
  GURL::Replacements replacements;
  replacements.SetSchemeStr(scheme);
  const GURL target_url =
      test_server.GetURL("/probe").ReplaceComponents(replacements);
  NSString* probe_script = CreateProbeScript(scheme, target_url.spec());

  // Privileged page (vector): a `file://` page cannot connect to the target.
  connection_listener.Reset();
  ASSERT_TRUE(test::LoadHtml(web_view, @"<html><body>Privileged</body></html>",
                             [NSURL URLWithString:@"file:///privileged.html"]));
  std::ignore = test::ExecuteAsyncJavaScript(web_view, probe_script, nil);
  EXPECT_EQ(0, connection_listener.socket_count())
      << scheme << ":// connection should have been blocked from file:// page.";

  // Privileged page (vector): a WebUI page, the test counterpart of the
  // `chrome://` pages registered by the embedder, cannot connect either.
  connection_listener.Reset();
  NSURL* web_ui_url = [NSURL
      URLWithString:base::SysUTF8ToNSString(
                        base::StrCat({kTestWebUIScheme, "://privileged/"}))];
  [web_view loadRequest:[NSURLRequest requestWithURL:web_ui_url]];
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^{
        return !web_view.loading && [web_view.URL isEqual:web_ui_url];
      }));
  std::ignore = test::ExecuteAsyncJavaScript(web_view, probe_script, nil);
  EXPECT_EQ(0, connection_listener.socket_count())
      << scheme << ":// connection should have been blocked from "
      << kTestWebUIScheme << ":// page.";

  // Unprivileged page (control): connections to the target are allowed.
  connection_listener.Reset();
  GURL http_page_url = test_server.GetURL("/page.html");
  ASSERT_TRUE(test::LoadHtml(
      web_view, @"<html><body>Unprivileged</body></html>",
      [NSURL URLWithString:base::SysUTF8ToNSString(http_page_url.spec())]));
  std::ignore = test::ExecuteAsyncJavaScript(web_view, probe_script, nil);
  EXPECT_GT(connection_listener.socket_count(), 0)
      << scheme << ":// connection should be allowed from unprivileged page.";
}

INSTANTIATE_TEST_SUITE_P(AllBlockedSchemes,
                         WKContentRuleListProviderSchemeTest,
                         ::testing::Values("http", "https", "ws", "wss"));

}  // namespace web
