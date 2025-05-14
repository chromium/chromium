// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/functional/overloaded.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "content/browser/fenced_frame/fenced_frame_config.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/shared_storage/shared_storage_browsertest_base.h"
#include "content/browser/shared_storage/shared_storage_document_service_impl.h"
#include "content/browser/shared_storage/shared_storage_event_params.h"
#include "content/browser/shared_storage/shared_storage_features.h"
#include "content/browser/shared_storage/shared_storage_runtime_manager.h"
#include "content/browser/shared_storage/shared_storage_worklet_host.h"
#include "content/browser/shared_storage/test_shared_storage_observer.h"
#include "content/browser/shared_storage/test_shared_storage_runtime_manager.h"
#include "content/browser/shared_storage/test_shared_storage_worklet_host.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/shared_storage_test_utils.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_select_url_fenced_frame_config_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "content/test/fenced_frame_test_utils.h"
#include "net/base/net_errors.h"
#include "net/base/schemeful_site.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "services/network/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/chrome_debug_urls.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/common/fenced_frame/fenced_frame_utils.h"
#include "third_party/blink/public/common/fenced_frame/redacted_fenced_frame_config.h"
#include "third_party/blink/public/common/shared_storage/shared_storage_utils.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "url/url_constants.h"

namespace content {

using testing::Pair;
using testing::UnorderedElementsAre;
using SharedStorageReportingMap = base::flat_map<std::string, ::GURL>;
using SharedStorageUrlSpecWithMetadata =
    SharedStorageEventParams::SharedStorageUrlSpecWithMetadata;
using Access = TestSharedStorageObserver::Access;
using AccessScope = blink::SharedStorageAccessScope;
using AccessMethod = TestSharedStorageObserver::AccessMethod;
using OperationFinishedInfo = TestSharedStorageObserver::OperationFinishedInfo;

namespace {

constexpr double kSelectURLOverallBitBudget = 12.0;

constexpr double kSelectURLSiteBitBudget = 6.0;

bool IsErrorMessage(const content::WebContentsConsoleObserver::Message& msg) {
  return msg.log_level == blink::mojom::ConsoleMessageLevel::kError;
}

bool IsInfoMessage(const content::WebContentsConsoleObserver::Message& msg) {
  return msg.log_level == blink::mojom::ConsoleMessageLevel::kInfo;
}

}  // namespace

class SharedStorageFencedFrameInteractionBrowserTestBase
    : public SharedStorageBrowserTestBase {
 public:
  using FencedFrameNavigationTarget = std::variant<GURL, std::string>;

  // TODO(crbug.com/40256120): This function should be removed. Use
  // `CreateFencedFrame` in fenced_frame_test_util.h instead.
  FrameTreeNode* CreateFencedFrame(FrameTreeNode* root,
                                   const FencedFrameNavigationTarget& target) {
    size_t initial_child_count = root->child_count();

    EXPECT_TRUE(ExecJs(root,
                       "var f = document.createElement('fencedframe');"
                       "document.body.appendChild(f);"));

    EXPECT_EQ(initial_child_count + 1, root->child_count());
    FrameTreeNode* fenced_frame_root_node =
        GetFencedFrameRootNode(root->child_at(initial_child_count));

    TestFrameNavigationObserver observer(fenced_frame_root_node);

    EvalJsResult result = NavigateFencedFrame(root, target);

    observer.Wait();

    return fenced_frame_root_node;
  }

  FrameTreeNode* CreateFencedFrame(const FencedFrameNavigationTarget& target) {
    return CreateFencedFrame(PrimaryFrameTreeNodeRoot(), target);
  }

  EvalJsResult NavigateFencedFrame(FrameTreeNode* root,
                                   const FencedFrameNavigationTarget& target) {
    return EvalJs(
        root,
        std::visit(base::Overloaded{
                       [](const GURL& url) {
                         return JsReplace(
                             "f.config = new FencedFrameConfig($1);", url);
                       },
                       [](const std::string& config) {
                         return JsReplace("f.config = window[$1]", config);
                       },
                   },
                   target));
  }

  // Precondition: There is exactly one existing fenced frame.
  void NavigateExistingFencedFrame(FrameTreeNode* existing_fenced_frame,
                                   const FencedFrameNavigationTarget& target) {
    FrameTreeNode* root = PrimaryFrameTreeNodeRoot();
    TestFrameNavigationObserver observer(existing_fenced_frame);

    EXPECT_TRUE(ExecJs(root, "var f = document.querySelector('fencedframe');"));
    EvalJsResult result = NavigateFencedFrame(root, target);

    observer.Wait();
  }
};

class SharedStorageFencedFrameInteractionBrowserTest
    : public SharedStorageFencedFrameInteractionBrowserTestBase {
 public:
  bool ResolveSelectURLToConfig() override { return true; }
};

IN_PROC_BROWSER_TEST_F(SharedStorageFencedFrameInteractionBrowserTest,
                       SelectURL_FinishBeforeStartingFencedFrameNavigation) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  EXPECT_EQ(1u, test_runtime_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_runtime_manager().GetKeepAliveWorkletHostsCount());

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("Start executing simple_module.js",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("Finish executing simple_module.js",
            base::UTF16ToUTF8(console_observer.messages()[1].message));

  EXPECT_TRUE(ExecJs(shell(), JsReplace("window.resolveSelectURLToConfig = $1;",
                                        ResolveSelectURLToConfig())));

  TestSelectURLFencedFrameConfigObserver config_observer(GetStoragePartition());
  // There is 1 more "worklet operation": `selectURL()`.
  test_runtime_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EvalJsResult result = EvalJs(shell(), R"(
      (async function() {
        window.select_url_result = await sharedStorage.selectURL(
          'test-url-selection-operation',
          [
            {
              url: "fenced_frames/title0.html"
            },
            {
              url: "fenced_frames/title1.html",
              reportingMetadata: {
                "click": "fenced_frames/report1.html"
              }
            },
            {
              url: "fenced_frames/title2.html"
            }
          ],
          {
            data: {'mockResult': 1},
            resolveToConfig: resolveSelectURLToConfig
          }
        );
        if (resolveSelectURLToConfig &&
            !(select_url_result instanceof FencedFrameConfig)) {
          throw new Error('selectURL() did not return a FencedFrameConfig.');
        }
        return window.select_url_result;
      })()
    )");

  EXPECT_TRUE(result.error.empty());
  const std::optional<GURL>& observed_urn_uuid = config_observer.GetUrnUuid();
  EXPECT_TRUE(observed_urn_uuid.has_value());
  EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid.value()));

  test_runtime_manager().GetAttachedWorkletHost()->WaitForWorkletResponses();

  ASSERT_TRUE(config_observer.ConfigObserved());
  const std::optional<FencedFrameConfig>& fenced_frame_config =
      config_observer.GetConfig();
  EXPECT_TRUE(fenced_frame_config.has_value());
  EXPECT_EQ(fenced_frame_config->urn_uuid(), observed_urn_uuid.value());

  SharedStorageBudgetMetadata* metadata =
      GetSharedStorageBudgetMetadata(observed_urn_uuid.value());
  EXPECT_TRUE(metadata);
  EXPECT_EQ(metadata->site,
            net::SchemefulSite(https_server()->GetOrigin("a.test")));
  EXPECT_DOUBLE_EQ(metadata->budget_to_charge, std::log2(3));

  EXPECT_THAT(GetSharedStorageReportingMap(observed_urn_uuid.value()),
              UnorderedElementsAre(
                  Pair("click", https_server()->GetURL(
                                    "a.test", "/fenced_frames/report1.html"))));

  GURL url0 = https_server()->GetURL("a.test", "/fenced_frames/title0.html");
  GURL url1 = https_server()->GetURL("a.test", "/fenced_frames/title1.html");
  GURL url2 = https_server()->GetURL("a.test", "/fenced_frames/title2.html");

  EXPECT_EQ(6u, console_observer.messages().size());
  EXPECT_EQ("Start executing 'test-url-selection-operation'",
            base::UTF16ToUTF8(console_observer.messages()[2].message));
  EXPECT_EQ(base::StrCat({"[\"", url0.spec(), "\",\"", url1.spec(), "\",\"",
                          url2.spec(), "\"]"}),
            base::UTF16ToUTF8(console_observer.messages()[3].message));
  EXPECT_EQ("{\"mockResult\":1}",
            base::UTF16ToUTF8(console_observer.messages()[4].message));
  EXPECT_EQ("Finish executing 'test-url-selection-operation'",
            base::UTF16ToUTF8(console_observer.messages()[5].message));

  FrameTreeNode* root = PrimaryFrameTreeNodeRoot();

  EXPECT_TRUE(ExecJs(root,
                     "var f = document.createElement('fencedframe');"
                     "document.body.appendChild(f);"));

  EXPECT_EQ(1U, root->child_count());
  FrameTreeNode* fenced_frame_root_node =
      GetFencedFrameRootNode(root->child_at(0));

  TestFrameNavigationObserver observer(fenced_frame_root_node);

  EvalJsResult navigation_result = NavigateFencedFrame(
      root, ResolveSelectURLToConfig()
                ? FencedFrameNavigationTarget("select_url_result")
                : FencedFrameNavigationTarget(observed_urn_uuid.value()));

  observer.Wait();

  EXPECT_EQ(
      https_server()->GetURL("a.test", "/fenced_frames/title1.html"),
      fenced_frame_root_node->current_frame_host()->GetLastCommittedURL());

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     1);
}

IN_PROC_BROWSER_TEST_F(SharedStorageFencedFrameInteractionBrowserTest,
                       SelectURL_FinishAfterStartingFencedFrameNavigation) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  EXPECT_EQ(1u, test_runtime_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_runtime_manager().GetKeepAliveWorkletHostsCount());

  // Configure the worklet host to defer processing the subsequent
  // `selectURL()` response.
  test_runtime_manager()
      .GetAttachedWorkletHost()
      ->set_should_defer_worklet_messages(true);

  EXPECT_TRUE(ExecJs(shell(), JsReplace("window.resolveSelectURLToConfig = $1;",
                                        ResolveSelectURLToConfig())));

  TestSelectURLFencedFrameConfigObserver config_observer(GetStoragePartition());
  // There is 1 more "worklet operation": `selectURL()`.
  test_runtime_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EvalJsResult result = EvalJs(shell(), R"(
      (async function() {
        window.select_url_result = await sharedStorage.selectURL(
          'test-url-selection-operation',
          [
            {
              url: "fenced_frames/title0.html"
            },
            {
              url: "fenced_frames/title1.html",
              reportingMetadata: {
                "click": "fenced_frames/report1.html"
              }
            },
            {
              url: "fenced_frames/title2.html"
            }
          ],
          {
            data: {'mockResult': 1},
            resolveToConfig: resolveSelectURLToConfig
          }
        );
        if (resolveSelectURLToConfig &&
            !(select_url_result instanceof FencedFrameConfig)) {
          throw new Error('selectURL() did not return a FencedFrameConfig.');
        }
        return window.select_url_result;
      })()
    )");

  EXPECT_TRUE(result.error.empty());
  const std::optional<GURL>& observed_urn_uuid = config_observer.GetUrnUuid();
  EXPECT_TRUE(observed_urn_uuid.has_value());
  EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid.value()));

  test_runtime_manager().GetAttachedWorkletHost()->WaitForWorkletResponses();

  FrameTreeNode* root = PrimaryFrameTreeNodeRoot();

  EXPECT_TRUE(ExecJs(root,
                     "var f = document.createElement('fencedframe');"
                     "document.body.appendChild(f);"));

  EXPECT_EQ(1U, root->child_count());
  FrameTreeNode* fenced_frame_root_node =
      GetFencedFrameRootNode(root->child_at(0));

  TestFrameNavigationObserver observer(fenced_frame_root_node);

  EvalJsResult navigation_result = NavigateFencedFrame(
      root, ResolveSelectURLToConfig()
                ? FencedFrameNavigationTarget("select_url_result")
                : FencedFrameNavigationTarget(observed_urn_uuid.value()));

  // After the previous EvalJs, the NavigationRequest should have been created,
  // but may not have begun. Wait for BeginNavigation() and expect it to be
  // deferred on fenced frame url mapping.
  NavigationRequest* request = fenced_frame_root_node->navigation_request();
  if (!request->is_deferred_on_fenced_frame_url_mapping_for_testing()) {
    base::RunLoop run_loop;
    request->set_begin_navigation_callback_for_testing(  // IN-TEST
        run_loop.QuitWhenIdleClosure());
    run_loop.Run();

    EXPECT_TRUE(
        request
            ->is_deferred_on_fenced_frame_url_mapping_for_testing()  // IN-TEST
    );
  }

  FencedFrameURLMapping& url_mapping =
      root->current_frame_host()->GetPage().fenced_frame_urls_map();
  FencedFrameURLMappingTestPeer url_mapping_test_peer(&url_mapping);

  EXPECT_TRUE(
      url_mapping_test_peer.HasObserver(observed_urn_uuid.value(), request));

  // Execute the deferred messages. This should finish the url mapping and
  // resume the deferred navigation.
  test_runtime_manager()
      .GetAttachedWorkletHost()
      ->ExecutePendingWorkletMessages();

  observer.Wait();

  ASSERT_TRUE(config_observer.ConfigObserved());
  const std::optional<FencedFrameConfig>& fenced_frame_config =
      config_observer.GetConfig();
  EXPECT_TRUE(fenced_frame_config.has_value());
  EXPECT_EQ(fenced_frame_config->urn_uuid(), observed_urn_uuid.value());

  SharedStorageBudgetMetadata* metadata =
      GetSharedStorageBudgetMetadata(observed_urn_uuid.value());
  EXPECT_TRUE(metadata);
  EXPECT_EQ(metadata->site,
            net::SchemefulSite(https_server()->GetOrigin("a.test")));
  EXPECT_DOUBLE_EQ(metadata->budget_to_charge, std::log2(3));

  EXPECT_THAT(GetSharedStorageReportingMap(observed_urn_uuid.value()),
              UnorderedElementsAre(
                  Pair("click", https_server()->GetURL(
                                    "a.test", "/fenced_frames/report1.html"))));

  EXPECT_EQ(
      https_server()->GetURL("a.test", "/fenced_frames/title1.html"),
      fenced_frame_root_node->current_frame_host()->GetLastCommittedURL());

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     1);

  histogram_tester_.ExpectTotalCount(
      "Storage.SharedStorage.Timing.UrlMappingDuringNavigation", 1);
}

// Tests that the URN from SelectURL() is valid in different
// context in the page, but it's not valid in a new page.
IN_PROC_BROWSER_TEST_F(SharedStorageFencedFrameInteractionBrowserTest,
                       SelectURL_URNLifetime) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  std::optional<GURL> urn_uuid =
      SelectFrom8URLsInContext(url::Origin::Create(main_url));
  ASSERT_TRUE(urn_uuid.has_value());
  EXPECT_TRUE(blink::IsValidUrnUuidURL(*urn_uuid));

  FrameTreeNode* iframe_node = PrimaryFrameTreeNodeRoot()->child_at(0);

  // Navigate the iframe to about:blank.
  TestFrameNavigationObserver observer(iframe_node);
  EXPECT_TRUE(ExecJs(iframe_node, JsReplace("window.location.href=$1",
                                            GURL(url::kAboutBlankURL))));
  observer.Wait();

  // Verify that the `urn_uuid` is still valid in the main page.
  FrameTreeNode* fenced_frame_root_node = CreateFencedFrame(*urn_uuid);
  EXPECT_EQ(
      https_server()->GetURL("a.test", "/fenced_frames/title1.html"),
      fenced_frame_root_node->current_frame_host()->GetLastCommittedURL());

  // Navigate to a new page. Verify that the `urn_uuid` is not valid in this
  // new page.
  GURL new_page_main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), new_page_main_url));

  fenced_frame_root_node = CreateFencedFrame(*urn_uuid);
  EXPECT_NE(
      https_server()->GetURL("a.test", "/fenced_frames/title1.html"),
      fenced_frame_root_node->current_frame_host()->GetLastCommittedURL());

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     1);
}

// Tests that if the URN mapping is not finished before the keep-alive timeout,
// the mapping will be considered to be failed when the timeout is reached.
IN_PROC_BROWSER_TEST_F(SharedStorageFencedFrameInteractionBrowserTest,
                       SelectURL_NotFinishBeforeKeepAliveTimeout) {
  // The test assumes pages get deleted after navigation. To ensure this,
  // disable back/forward cache.
  content::DisableBackForwardCacheForTesting(
      shell()->web_contents(),
      content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  GURL url = https_server()->GetURL("a.test", kPageWithBlankIframePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  RenderFrameHost* iframe =
      PrimaryFrameTreeNodeRoot()->child_at(0)->current_frame_host();

  EXPECT_TRUE(ExecJs(iframe, R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  EXPECT_EQ(1u, test_runtime_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_runtime_manager().GetKeepAliveWorkletHostsCount());

  // Configure the worklet host to defer processing the subsequent
  // `selectURL()` response.
  test_runtime_manager()
      .GetAttachedWorkletHost()
      ->set_should_defer_worklet_messages(true);

  EXPECT_TRUE(ExecJs(iframe, kGenerateURLsListScript));
  EXPECT_TRUE(ExecJs(iframe, JsReplace("window.resolveSelectURLToConfig = $1;",
                                       ResolveSelectURLToConfig())));

  TestSelectURLFencedFrameConfigObserver config_observer(GetStoragePartition());
  // There is 1 more "worklet operation": `selectURL()`.
  test_runtime_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EvalJsResult result = EvalJs(iframe, R"(
      (async function() {
        const urls = generateUrls(8);
        window.select_url_result = await sharedStorage.selectURL(
          'test-url-selection-operation',
          urls,
          {
            data: {'mockResult': 1},
            resolveToConfig: resolveSelectURLToConfig
          }
        );
        if (resolveSelectURLToConfig &&
            !(select_url_result instanceof FencedFrameConfig)) {
          throw new Error('selectURL() did not return a FencedFrameConfig.');
        }
        return window.select_url_result;
      })()
    )");

  EXPECT_TRUE(result.error.empty());
  const std::optional<GURL>& observed_urn_uuid = config_observer.GetUrnUuid();
  EXPECT_TRUE(observed_urn_uuid.has_value());
  EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid.value()));

  FrameTreeNode* root = PrimaryFrameTreeNodeRoot();
  if (ResolveSelectURLToConfig()) {
    // Preserve the config in a variable. It is then installed to the new fenced
    // frame. Without this step, the config will be gone after navigating the
    // iframe to about::blank.
    EXPECT_TRUE(ExecJs(root, R"(var fenced_frame_config = document
                                        .getElementById('test_iframe')
                                        .contentWindow
                                        .select_url_result;)"));
  }

  // Navigate away to let the subframe's worklet enter keep-alive.
  NavigateIframeToURL(shell()->web_contents(), "test_iframe",
                      GURL(url::kAboutBlankURL));

  EXPECT_EQ(0u, test_runtime_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(1u, test_runtime_manager().GetKeepAliveWorkletHostsCount());

  test_runtime_manager().GetKeepAliveWorkletHost()->WaitForWorkletResponses();

  EXPECT_TRUE(ExecJs(root,
                     "var f = document.createElement('fencedframe');"
                     "document.body.appendChild(f);"));

  EXPECT_EQ(2U, root->child_count());
  FrameTreeNode* fenced_frame_root_node =
      GetFencedFrameRootNode(root->child_at(1));

  TestFrameNavigationObserver observer(fenced_frame_root_node);

  EvalJsResult navigation_result = NavigateFencedFrame(
      root, ResolveSelectURLToConfig()
                ? FencedFrameNavigationTarget("fenced_frame_config")
                : FencedFrameNavigationTarget(observed_urn_uuid.value()));

  // After the previous EvalJs, the NavigationRequest should have been created,
  // but may not have begun. Wait for BeginNavigation() and expect it to be
  // deferred on fenced frame url mapping.
  NavigationRequest* request = fenced_frame_root_node->navigation_request();
  if (!request
           ->is_deferred_on_fenced_frame_url_mapping_for_testing()  // IN-TEST
  ) {
    base::RunLoop run_loop;
    request->set_begin_navigation_callback_for_testing(  // IN-TEST
        run_loop.QuitWhenIdleClosure());
    run_loop.Run();

    EXPECT_TRUE(
        request
            ->is_deferred_on_fenced_frame_url_mapping_for_testing()  // IN-TEST
    );
  }

  ASSERT_FALSE(config_observer.ConfigObserved());
  const std::optional<FencedFrameConfig>& fenced_frame_config =
      config_observer.GetConfig();
  EXPECT_FALSE(fenced_frame_config.has_value());

  // Fire the keep-alive timer. This will terminate the keep-alive, and the
  // deferred navigation will resume to navigate to the default url (at index
  // 0).
  test_runtime_manager().GetKeepAliveWorkletHost()->FireKeepAliveTimerNow();

  EXPECT_EQ(0u, test_runtime_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_runtime_manager().GetKeepAliveWorkletHostsCount());

  observer.Wait();

  SharedStorageBudgetMetadata* metadata =
      GetSharedStorageBudgetMetadata(observed_urn_uuid.value());
  EXPECT_TRUE(metadata);
  EXPECT_EQ(metadata->site,
            net::SchemefulSite(https_server()->GetOrigin("a.test")));
  EXPECT_DOUBLE_EQ(metadata->budget_to_charge, 0.0);

  EXPECT_THAT(GetSharedStorageReportingMap(observed_urn_uuid.value()),
              UnorderedElementsAre(
                  Pair("click", https_server()->GetURL(
                                    "a.test", "/fenced_frames/report0.html")),
                  Pair("mouse interaction",
                       https_server()->GetURL("a.test",
                                              "/fenced_frames/report1.html"))));

  EXPECT_EQ(
      https_server()->GetURL("a.test", "/fenced_frames/title0.html"),
      fenced_frame_root_node->current_frame_host()->GetLastCommittedURL());

  // The worklet execution sequence for `selectURL()` doesn't complete, so the
  // `kTimingSelectUrlExecutedInWorkletHistogram` histogram isn't recorded.
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     0);

  // The worklet is destructed. The config corresponds to the unresolved URN is
  // populated in the destructor of `SharedStorageWorkletHost`.
  ASSERT_TRUE(config_observer.ConfigObserved());
  EXPECT_TRUE(fenced_frame_config.has_value());
  EXPECT_EQ(fenced_frame_config->urn_uuid(), observed_urn_uuid.value());
}

IN_PROC_BROWSER_TEST_F(SharedStorageFencedFrameInteractionBrowserTest,
                       SelectURL_WorkletReturnInvalidIndex) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());
  console_observer.SetFilter(base::BindRepeating(IsErrorMessage));

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  EXPECT_EQ(1u, test_runtime_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_runtime_manager().GetKeepAliveWorkletHostsCount());

  EXPECT_TRUE(ExecJs(shell(), JsReplace("window.resolveSelectURLToConfig = $1;",
                                        ResolveSelectURLToConfig())));

  TestSelectURLFencedFrameConfigObserver config_observer(GetStoragePartition());
  // There is 1 more "worklet operation": `selectURL()`.
  test_runtime_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EvalJsResult result = EvalJs(shell(), R"(
      (async function() {
        window.select_url_result = await sharedStorage.selectURL(
          'test-url-selection-operation',
          [
            {
              url: "fenced_frames/title0.html"
            },
            {
              url: "fenced_frames/title1.html",
              reportingMetadata:
              {
                "click": "fenced_frames/report1.html"
              }
            },
            {
              url: "fenced_frames/title2.html"
            }
          ],
          {
            data: {'mockResult': 3},
            resolveToConfig: resolveSelectURLToConfig
          }
        );
        if (resolveSelectURLToConfig &&
            !(select_url_result instanceof FencedFrameConfig)) {
          throw new Error('selectURL() did not return a FencedFrameConfig.');
        }
        return window.select_url_result;
      })()
    )");

  EXPECT_TRUE(result.error.empty());
  const std::optional<GURL>& observed_urn_uuid = config_observer.GetUrnUuid();
  EXPECT_TRUE(observed_urn_uuid.has_value());
  EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid.value()));

  test_runtime_manager().GetAttachedWorkletHost()->WaitForWorkletResponses();

  ASSERT_TRUE(config_observer.ConfigObserved());
  const std::optional<FencedFrameConfig>& fenced_frame_config =
      config_observer.GetConfig();
  EXPECT_TRUE(fenced_frame_config.has_value());
  EXPECT_EQ(fenced_frame_config->urn_uuid(), observed_urn_uuid->spec());

  EXPECT_EQ(
      "Promise resolved to a number outside the length of the input urls.",
      base::UTF16ToUTF8(console_observer.messages().back().message));

  SharedStorageBudgetMetadata* metadata =
      GetSharedStorageBudgetMetadata(observed_urn_uuid.value());
  EXPECT_TRUE(metadata);
  EXPECT_EQ(metadata->site,
            net::SchemefulSite(https_server()->GetOrigin("a.test")));
  EXPECT_DOUBLE_EQ(metadata->budget_to_charge, std::log2(3));

  EXPECT_TRUE(GetSharedStorageReportingMap(observed_urn_uuid.value()).empty());

  FrameTreeNode* root = PrimaryFrameTreeNodeRoot();

  EXPECT_TRUE(ExecJs(root,
                     "var f = document.createElement('fencedframe');"
                     "document.body.appendChild(f);"));

  EXPECT_EQ(1U, root->child_count());
  FrameTreeNode* fenced_frame_root_node =
      GetFencedFrameRootNode(root->child_at(0));

  TestFrameNavigationObserver observer(fenced_frame_root_node);

  EvalJsResult navigation_result = NavigateFencedFrame(
      root, ResolveSelectURLToConfig()
                ? FencedFrameNavigationTarget("select_url_result")
                : FencedFrameNavigationTarget(observed_urn_uuid.value()));

  observer.Wait();

  EXPECT_EQ(
      https_server()->GetURL("a.test", "/fenced_frames/title0.html"),
      fenced_frame_root_node->current_frame_host()->GetLastCommittedURL());

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     1);
}

IN_PROC_BROWSER_TEST_F(SharedStorageFencedFrameInteractionBrowserTest,
                       SelectURL_DuplicateUrl) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  EXPECT_EQ(1u, test_runtime_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_runtime_manager().GetKeepAliveWorkletHostsCount());

  EXPECT_TRUE(ExecJs(shell(), JsReplace("window.resolveSelectURLToConfig = $1;",
                                        ResolveSelectURLToConfig())));

  TestSelectURLFencedFrameConfigObserver config_observer(GetStoragePartition());
  // There is 1 more "worklet operation": `selectURL()`.
  test_runtime_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EvalJsResult result = EvalJs(shell(), R"(
      (async function() {
        window.select_url_result = await sharedStorage.selectURL(
          'test-url-selection-operation',
          [
            {
              url: "fenced_frames/title0.html"
            },
            {
              url: "fenced_frames/title1.html",
              reportingMetadata:
              {
                "click": "fenced_frames/report1.html"
              }
            },
            {
              url: "fenced_frames/title2.html"
            }
          ],
          {
            data: {'mockResult': 1},
            resolveToConfig: resolveSelectURLToConfig
          }
        );
        if (resolveSelectURLToConfig &&
            !(select_url_result instanceof FencedFrameConfig)) {
          throw new Error('selectURL() did not return a FencedFrameConfig.');
        }
        return window.select_url_result;
      })()
    )");

  EXPECT_TRUE(result.error.empty());
  const std::optional<GURL>& observed_urn_uuid = config_observer.GetUrnUuid();
  EXPECT_TRUE(observed_urn_uuid.has_value());
  EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid.value()));

  test_runtime_manager().GetAttachedWorkletHost()->WaitForWorkletResponses();

  ASSERT_TRUE(config_observer.ConfigObserved());
  const std::optional<FencedFrameConfig>& fenced_frame_config =
      config_observer.GetConfig();
  EXPECT_TRUE(fenced_frame_config.has_value());
  EXPECT_EQ(fenced_frame_config->urn_uuid(), observed_urn_uuid.value());

  EXPECT_EQ("Finish executing 'test-url-selection-operation'",
            base::UTF16ToUTF8(console_observer.messages().back().message));

  SharedStorageBudgetMetadata* metadata =
      GetSharedStorageBudgetMetadata(observed_urn_uuid.value());
  EXPECT_TRUE(metadata);
  EXPECT_EQ(metadata->site,
            net::SchemefulSite(https_server()->GetOrigin("a.test")));
  EXPECT_DOUBLE_EQ(metadata->budget_to_charge, std::log2(3));

  EXPECT_THAT(GetSharedStorageReportingMap(observed_urn_uuid.value()),
              UnorderedElementsAre(
                  Pair("click", https_server()->GetURL(
                                    "a.test", "/fenced_frames/report1.html"))));

  FrameTreeNode* root = PrimaryFrameTreeNodeRoot();

  EXPECT_TRUE(ExecJs(root,
                     "var f = document.createElement('fencedframe');"
                     "document.body.appendChild(f);"));

  EXPECT_EQ(1U, root->child_count());
  FrameTreeNode* fenced_frame_root_node =
      GetFencedFrameRootNode(root->child_at(0));

  TestFrameNavigationObserver observer(fenced_frame_root_node);

  EvalJsResult navigation_result = NavigateFencedFrame(
      root, ResolveSelectURLToConfig()
                ? FencedFrameNavigationTarget("select_url_result")
                : FencedFrameNavigationTarget(observed_urn_uuid.value()));

  observer.Wait();

  EXPECT_EQ(
      https_server()->GetURL("a.test", "/fenced_frames/title1.html"),
      fenced_frame_root_node->current_frame_host()->GetLastCommittedURL());

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     1);
}

IN_PROC_BROWSER_TEST_F(SharedStorageFencedFrameInteractionBrowserTest,
                       FencedFrameNavigateSelf_NoBudgetWithdrawal) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  url::Origin shared_storage_origin =
      url::Origin::Create(https_server()->GetURL("b.test", kSimplePagePath));

  std::optional<GURL> urn_uuid =
      SelectFrom8URLsInContext(shared_storage_origin);
  ASSERT_TRUE(urn_uuid.has_value());

  FrameTreeNode* fenced_frame_root_node = CreateFencedFrame(*urn_uuid);

  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin), kBudgetAllowed);
  EXPECT_DOUBLE_EQ(
      RemainingBudgetViaJSForFrame(PrimaryFrameTreeNodeRoot()->child_at(0)),
      kBudgetAllowed);

  TestFrameNavigationObserver observer(fenced_frame_root_node);
  EXPECT_TRUE(ExecJs(fenced_frame_root_node, "location.reload()"));
  observer.Wait();

  // No budget withdrawal as the fenced frame did not initiate a top navigation.
  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin), kBudgetAllowed);
  EXPECT_DOUBLE_EQ(
      RemainingBudgetViaJSForFrame(PrimaryFrameTreeNodeRoot()->child_at(0)),
      kBudgetAllowed);

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     1);
}

IN_PROC_BROWSER_TEST_F(SharedStorageFencedFrameInteractionBrowserTest,
                       FencedFrameNavigateTop_BudgetWithdrawal) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  url::Origin shared_storage_origin =
      url::Origin::Create(https_server()->GetURL("b.test", kSimplePagePath));

  std::optional<GURL> urn_uuid =
      SelectFrom8URLsInContext(shared_storage_origin);
  ASSERT_TRUE(urn_uuid.has_value());

  FrameTreeNode* fenced_frame_root_node = CreateFencedFrame(*urn_uuid);

  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin), kBudgetAllowed);
  EXPECT_DOUBLE_EQ(
      RemainingBudgetViaJSForFrame(PrimaryFrameTreeNodeRoot()->child_at(0)),
      kBudgetAllowed);

  GURL new_page_url = https_server()->GetURL("c.test", kSimplePagePath);

  TestNavigationObserver top_navigation_observer(shell()->web_contents());
  EXPECT_TRUE(
      ExecJs(fenced_frame_root_node,
             JsReplace("window.open($1, '_unfencedTop')", new_page_url)));
  top_navigation_observer.Wait();

  // After the top navigation, log(8)=3 bits should have been withdrawn from the
  // original shared storage origin.
  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin),
                   kBudgetAllowed - 3);
  EXPECT_DOUBLE_EQ(RemainingBudgetViaJSForOrigin(shared_storage_origin),
                   kBudgetAllowed - 3);

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     1);
}

IN_PROC_BROWSER_TEST_F(
    SharedStorageFencedFrameInteractionBrowserTest,
    FencedFrameNavigateFromParentToRegularURLAndThenOpenPopup_NoBudgetWithdrawal) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  url::Origin shared_storage_origin =
      url::Origin::Create(https_server()->GetURL("b.test", kSimplePagePath));

  std::optional<GURL> urn_uuid =
      SelectFrom8URLsInContext(shared_storage_origin);
  ASSERT_TRUE(urn_uuid.has_value());

  FrameTreeNode* fenced_frame_root_node = CreateFencedFrame(*urn_uuid);

  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin), kBudgetAllowed);
  EXPECT_DOUBLE_EQ(
      RemainingBudgetViaJSForFrame(PrimaryFrameTreeNodeRoot()->child_at(0)),
      kBudgetAllowed);

  GURL new_frame_url = https_server()->GetURL("c.test", kFencedFramePath);

  TestFrameNavigationObserver observer(fenced_frame_root_node);
  std::string navigate_fenced_frame_script = JsReplace(
      "var f = document.getElementsByTagName('fencedframe')[0]; f.config = new "
      "FencedFrameConfig($1);",
      new_frame_url);

  EXPECT_TRUE(ExecJs(shell(), navigate_fenced_frame_script));
  observer.Wait();

  GURL new_page_url = https_server()->GetURL("d.test", kSimplePagePath);

  OpenPopup(fenced_frame_root_node, new_page_url, /*name=*/"");

  // No budget withdrawal as the initial fenced frame was navigated away by its
  // parent before it triggers a top navigation.
  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin), kBudgetAllowed);
  url::Origin new_frame_origin = url::Origin::Create(new_frame_url);
  EXPECT_DOUBLE_EQ(GetRemainingBudget(new_frame_origin), kBudgetAllowed);
  EXPECT_DOUBLE_EQ(RemainingBudgetViaJSForOrigin(shared_storage_origin),
                   kBudgetAllowed);
  EXPECT_DOUBLE_EQ(RemainingBudgetViaJSForOrigin(new_frame_origin),
                   kBudgetAllowed);

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     1);
}

IN_PROC_BROWSER_TEST_F(
    SharedStorageFencedFrameInteractionBrowserTest,
    FencedFrameNavigateSelfAndThenNavigateTop_BudgetWithdrawal) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  url::Origin shared_storage_origin =
      url::Origin::Create(https_server()->GetURL("b.test", kSimplePagePath));

  std::optional<GURL> urn_uuid =
      SelectFrom8URLsInContext(shared_storage_origin);
  ASSERT_TRUE(urn_uuid.has_value());

  FrameTreeNode* fenced_frame_root_node = CreateFencedFrame(*urn_uuid);

  {
    GURL new_frame_url = https_server()->GetURL("c.test", kFencedFramePath);

    TestFrameNavigationObserver observer(fenced_frame_root_node);
    EXPECT_TRUE(ExecJs(fenced_frame_root_node,
                       JsReplace("window.location.href=$1", new_frame_url)));
    observer.Wait();
  }

  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin), kBudgetAllowed);
  EXPECT_DOUBLE_EQ(
      RemainingBudgetViaJSForFrame(PrimaryFrameTreeNodeRoot()->child_at(0)),
      kBudgetAllowed);

  {
    GURL new_page_url = https_server()->GetURL("d.test", kSimplePagePath);

    TestNavigationObserver top_navigation_observer(shell()->web_contents());
    EXPECT_TRUE(
        ExecJs(fenced_frame_root_node,
               JsReplace("window.open($1, '_unfencedTop')", new_page_url)));
    top_navigation_observer.Wait();
  }

  // After the top navigation, log(8)=3 bits should have been withdrawn from the
  // original shared storage origin.
  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin),
                   kBudgetAllowed - 3);
  EXPECT_DOUBLE_EQ(RemainingBudgetViaJSForOrigin(shared_storage_origin),
                   kBudgetAllowed - 3);

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     1);
}

// TODO(crbug.com/40233168): Reenable this test when it is possible to create a
// nested fenced frame with no reporting metadata, that can call _unfencedTop.
IN_PROC_BROWSER_TEST_F(SharedStorageFencedFrameInteractionBrowserTest,
                       DISABLED_NestedFencedFrameNavigateTop_BudgetWithdrawal) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  url::Origin shared_storage_origin =
      url::Origin::Create(https_server()->GetURL("b.test", kSimplePagePath));

  std::optional<GURL> urn_uuid =
      SelectFrom8URLsInContext(shared_storage_origin);
  ASSERT_TRUE(urn_uuid.has_value());

  FrameTreeNode* fenced_frame_root_node = CreateFencedFrame(*urn_uuid);

  GURL nested_fenced_frame_url =
      https_server()->GetURL("c.test", kFencedFramePath);
  FrameTreeNode* nested_fenced_frame_root_node =
      CreateFencedFrame(fenced_frame_root_node, nested_fenced_frame_url);

  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin), kBudgetAllowed);
  EXPECT_DOUBLE_EQ(
      RemainingBudgetViaJSForFrame(PrimaryFrameTreeNodeRoot()->child_at(0)),
      kBudgetAllowed);

  GURL new_page_url = https_server()->GetURL("d.test", kSimplePagePath);
  TestNavigationObserver top_navigation_observer(shell()->web_contents());
  EXPECT_TRUE(
      ExecJs(nested_fenced_frame_root_node,
             JsReplace("window.open($1, '_unfencedTop')", new_page_url)));
  top_navigation_observer.Wait();

  // After the top navigation, log(8)=3 bits should have been withdrawn from the
  // original shared storage origin.
  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin),
                   kBudgetAllowed - 3);
  EXPECT_DOUBLE_EQ(RemainingBudgetViaJSForOrigin(shared_storage_origin),
                   kBudgetAllowed - 3);

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     1);
}

IN_PROC_BROWSER_TEST_F(
    SharedStorageFencedFrameInteractionBrowserTest,
    NestedFencedFrameNavigateTop_BudgetWithdrawalFromTwoMetadata) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  url::Origin shared_storage_origin_1 =
      url::Origin::Create(https_server()->GetURL("b.test", kSimplePagePath));

  std::optional<GURL> urn_uuid_1 =
      SelectFrom8URLsInContext(shared_storage_origin_1);
  ASSERT_TRUE(urn_uuid_1.has_value());
  FrameTreeNode* fenced_frame_root_node_1 = CreateFencedFrame(*urn_uuid_1);

  url::Origin shared_storage_origin_2 =
      url::Origin::Create(https_server()->GetURL("c.test", kSimplePagePath));

  std::optional<GURL> urn_uuid_2 = SelectFrom8URLsInContext(
      shared_storage_origin_2, fenced_frame_root_node_1);
  ASSERT_TRUE(urn_uuid_2.has_value());

  FrameTreeNode* fenced_frame_root_node_2 =
      CreateFencedFrame(fenced_frame_root_node_1, *urn_uuid_2);

  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin_1), kBudgetAllowed);
  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin_2), kBudgetAllowed);

  GURL new_page_url = https_server()->GetURL("d.test", kSimplePagePath);
  TestNavigationObserver top_navigation_observer(shell()->web_contents());
  EXPECT_TRUE(
      ExecJs(fenced_frame_root_node_2,
             JsReplace("window.open($1, '_unfencedTop')", new_page_url)));
  top_navigation_observer.Wait();

  // After the top navigation, log(8)=3 bits should have been withdrawn from
  // both `shared_storage_origin_1` and `shared_storage_origin_2`.
  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin_1),
                   kBudgetAllowed - 3);
  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin_2),
                   kBudgetAllowed - 3);
}

IN_PROC_BROWSER_TEST_F(
    SharedStorageFencedFrameInteractionBrowserTest,
    SharedStorageNotAllowedInFencedFrameNotOriginatedFromSharedStorage) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  GURL fenced_frame_url =
      https_server()->GetURL("a.test", "/fenced_frames/title1.html");

  FrameTreeNode* fenced_frame_root_node_1 =
      static_cast<RenderFrameHostImpl*>(
          fenced_frame_test_helper_.CreateFencedFrame(
              shell()->web_contents()->GetPrimaryMainFrame(), fenced_frame_url,
              net::OK, blink::FencedFrame::DeprecatedFencedFrameMode::kDefault))
          ->frame_tree_node();

  EvalJsResult result = EvalJs(fenced_frame_root_node_1, R"(
      sharedStorage.worklet.addModule('/shared_storage/simple_module.js');
    )");

  EXPECT_THAT(
      result.error,
      testing::HasSubstr(
          "The \"shared-storage\" Permissions Policy denied the method"));
}

IN_PROC_BROWSER_TEST_F(SharedStorageFencedFrameInteractionBrowserTest,
                       SelectURLNotAllowedInNestedFencedFrame) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  url::Origin shared_storage_origin_1 =
      url::Origin::Create(https_server()->GetURL("b.test", kSimplePagePath));

  std::optional<GURL> urn_uuid_1 =
      SelectFrom8URLsInContext(shared_storage_origin_1);
  ASSERT_TRUE(urn_uuid_1.has_value());
  FrameTreeNode* fenced_frame_root_node_1 = CreateFencedFrame(*urn_uuid_1);

  url::Origin shared_storage_origin_2 =
      url::Origin::Create(https_server()->GetURL("c.test", kSimplePagePath));

  std::optional<GURL> urn_uuid_2 = SelectFrom8URLsInContext(
      shared_storage_origin_2, fenced_frame_root_node_1);
  ASSERT_TRUE(urn_uuid_2.has_value());

  FrameTreeNode* fenced_frame_root_node_2 =
      CreateFencedFrame(fenced_frame_root_node_1, *urn_uuid_2);

  EXPECT_TRUE(ExecJs(fenced_frame_root_node_2, R"(
      sharedStorage.worklet.addModule('/shared_storage/simple_module.js');
    )"));
  EXPECT_TRUE(ExecJs(fenced_frame_root_node_2,
                     JsReplace("window.resolveSelectURLToConfig = $1;",
                               ResolveSelectURLToConfig())));

  EvalJsResult result = EvalJs(fenced_frame_root_node_2, R"(
      sharedStorage.selectURL(
        'test-url-selection-operation',
        [
          {
            url: "fenced_frames/title0.html"
          }
        ],
        {
          data: {'mockResult': 0},
          resolveToConfig: resolveSelectURLToConfig
        }
      );
    )");

  EXPECT_THAT(result.error,
              testing::HasSubstr(
                  "selectURL() is called in a context with a fenced frame "
                  "depth (2) exceeding the maximum allowed number (1)."));
}

IN_PROC_BROWSER_TEST_F(SharedStorageFencedFrameInteractionBrowserTest,
                       IframeInFencedFrameNavigateTop_BudgetWithdrawal) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  url::Origin shared_storage_origin =
      url::Origin::Create(https_server()->GetURL("b.test", kSimplePagePath));

  std::optional<GURL> urn_uuid =
      SelectFrom8URLsInContext(shared_storage_origin);
  ASSERT_TRUE(urn_uuid.has_value());

  FrameTreeNode* fenced_frame_root_node = CreateFencedFrame(*urn_uuid);

  GURL nested_fenced_frame_url =
      https_server()->GetURL("c.test", kFencedFramePath);
  FrameTreeNode* nested_fenced_frame_root_node =
      CreateIFrame(fenced_frame_root_node, nested_fenced_frame_url);

  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin), kBudgetAllowed);
  EXPECT_DOUBLE_EQ(
      RemainingBudgetViaJSForFrame(PrimaryFrameTreeNodeRoot()->child_at(0)),
      kBudgetAllowed);

  GURL new_page_url = https_server()->GetURL("d.test", kSimplePagePath);
  TestNavigationObserver top_navigation_observer(shell()->web_contents());
  EXPECT_TRUE(
      ExecJs(nested_fenced_frame_root_node,
             JsReplace("window.open($1, '_unfencedTop')", new_page_url)));
  top_navigation_observer.Wait();

  // After the top navigation, log(8)=3 bits should have been withdrawn from the
  // original shared storage origin.
  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin),
                   kBudgetAllowed - 3);
  EXPECT_DOUBLE_EQ(RemainingBudgetViaJSForOrigin(shared_storage_origin),
                   kBudgetAllowed - 3);

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     1);
}

IN_PROC_BROWSER_TEST_F(SharedStorageFencedFrameInteractionBrowserTest,
                       FencedFrame_PopupTwice_BudgetWithdrawalOnce) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  url::Origin shared_storage_origin =
      url::Origin::Create(https_server()->GetURL("b.test", kSimplePagePath));

  std::optional<GURL> urn_uuid =
      SelectFrom8URLsInContext(shared_storage_origin);
  ASSERT_TRUE(urn_uuid.has_value());

  FrameTreeNode* fenced_frame_root_node = CreateFencedFrame(*urn_uuid);

  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin), kBudgetAllowed);
  EXPECT_DOUBLE_EQ(
      RemainingBudgetViaJSForFrame(PrimaryFrameTreeNodeRoot()->child_at(0)),
      kBudgetAllowed);

  OpenPopup(fenced_frame_root_node,
            https_server()->GetURL("c.test", kSimplePagePath), /*name=*/"");

  // After the popup, log(8)=3 bits should have been withdrawn from the
  // original shared storage origin.
  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin),
                   kBudgetAllowed - 3);

  OpenPopup(fenced_frame_root_node,
            https_server()->GetURL("c.test", kSimplePagePath), /*name=*/"");

  // The budget can only be withdrawn once for each urn_uuid.
  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin),
                   kBudgetAllowed - 3);
  EXPECT_DOUBLE_EQ(RemainingBudgetViaJSForOrigin(shared_storage_origin),
                   kBudgetAllowed - 3);

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     1);
}

IN_PROC_BROWSER_TEST_F(
    SharedStorageFencedFrameInteractionBrowserTest,
    TwoFencedFrames_DifferentURNs_EachPopupOnce_BudgetWithdrawalTwice) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  url::Origin shared_storage_origin = url::Origin::Create(main_url);

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  EXPECT_TRUE(ExecJs(shell(), kGenerateURLsListScript));
  EXPECT_TRUE(ExecJs(shell(), "window.urls = generateUrls(8);"));
  EXPECT_TRUE(ExecJs(shell(), JsReplace("window.resolveSelectURLToConfig = $1;",
                                        ResolveSelectURLToConfig())));

  // There are 2 more "worklet operations": both `selectURL()`.
  test_runtime_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(2);

  TestSelectURLFencedFrameConfigObserver config_observer_1(
      GetStoragePartition());
  EvalJsResult result_1 = EvalJs(shell(), R"(
      (async function() {
        window.select_url_result_1 = await sharedStorage.selectURL(
          'test-url-selection-operation',
          urls,
          {
            data: {'mockResult': 1},
            resolveToConfig: resolveSelectURLToConfig,
            keepAlive: true
          }
        );
        if (resolveSelectURLToConfig &&
            !(select_url_result_1 instanceof FencedFrameConfig)) {
          throw new Error('selectURL() did not return a FencedFrameConfig.');
        }
        return window.select_url_result_1;
      })()
    )");

  EXPECT_TRUE(result_1.error.empty());
  const std::optional<GURL>& observed_urn_uuid_1 =
      config_observer_1.GetUrnUuid();
  EXPECT_TRUE(observed_urn_uuid_1.has_value());
  EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid_1.value()));

  TestSelectURLFencedFrameConfigObserver config_observer_2(
      GetStoragePartition());
  EvalJsResult result_2 = EvalJs(shell(), R"(
      (async function() {
        window.select_url_result_2 = await sharedStorage.selectURL(
          'test-url-selection-operation',
          urls,
          {
            data: {'mockResult': 1},
            resolveToConfig: resolveSelectURLToConfig,
            keepAlive: true
          }
        );
        if (resolveSelectURLToConfig &&
            !(select_url_result_2 instanceof FencedFrameConfig)) {
          throw new Error('selectURL() did not return a FencedFrameConfig.');
        }
        return window.select_url_result_2;
      })()
    )");

  EXPECT_TRUE(result_2.error.empty());
  const std::optional<GURL>& observed_urn_uuid_2 =
      config_observer_2.GetUrnUuid();
  EXPECT_TRUE(observed_urn_uuid_2.has_value());
  EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid_2.value()));

  test_runtime_manager().GetAttachedWorkletHost()->WaitForWorkletResponses();

  ASSERT_TRUE(config_observer_1.ConfigObserved());
  const std::optional<FencedFrameConfig>& fenced_frame_config_1 =
      config_observer_1.GetConfig();
  EXPECT_TRUE(fenced_frame_config_1.has_value());
  EXPECT_EQ(fenced_frame_config_1->urn_uuid(), observed_urn_uuid_1.value());

  ASSERT_TRUE(config_observer_2.ConfigObserved());
  const std::optional<FencedFrameConfig>& fenced_frame_config_2 =
      config_observer_2.GetConfig();
  EXPECT_TRUE(fenced_frame_config_2.has_value());
  EXPECT_EQ(fenced_frame_config_2->urn_uuid(), observed_urn_uuid_2.value());

  FrameTreeNode* fenced_frame_root_node_1 = CreateFencedFrame(
      ResolveSelectURLToConfig()
          ? FencedFrameNavigationTarget("select_url_result_1")
          : FencedFrameNavigationTarget(observed_urn_uuid_1.value()));
  FrameTreeNode* fenced_frame_root_node_2 = CreateFencedFrame(
      ResolveSelectURLToConfig()
          ? FencedFrameNavigationTarget("select_url_result_2")
          : FencedFrameNavigationTarget(observed_urn_uuid_2.value()));

  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin), kBudgetAllowed);
  EXPECT_DOUBLE_EQ(RemainingBudgetViaJSForFrame(PrimaryFrameTreeNodeRoot()),
                   kBudgetAllowed);

  OpenPopup(fenced_frame_root_node_1,
            https_server()->GetURL("b.test", kSimplePagePath), /*name=*/"");

  // After the popup, log(8)=3 bits should have been withdrawn from the
  // original shared storage origin.
  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin),
                   kBudgetAllowed - 3);

  OpenPopup(fenced_frame_root_node_2,
            https_server()->GetURL("b.test", kSimplePagePath), /*name=*/"");

  // After the popup, log(8)=3 bits should have been withdrawn from the
  // original shared storage origin. The budget for `shared_storage_origin` can
  // be charged once for each distinct URN, and therefore here it gets charged
  // twice.
  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin),
                   kBudgetAllowed - 3 - 3);
  EXPECT_DOUBLE_EQ(RemainingBudgetViaJSForOrigin(shared_storage_origin),
                   kBudgetAllowed - 3 - 3);

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     2);
}

IN_PROC_BROWSER_TEST_F(
    SharedStorageFencedFrameInteractionBrowserTest,
    TwoFencedFrames_SameURNs_EachPopupOnce_BudgetWithdrawalOnce) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  url::Origin shared_storage_origin = url::Origin::Create(main_url);

  std::optional<GURL> urn_uuid =
      SelectFrom8URLsInContext(shared_storage_origin);
  ASSERT_TRUE(urn_uuid.has_value());

  FrameTreeNode* fenced_frame_root_node_1 = CreateFencedFrame(*urn_uuid);
  FrameTreeNode* fenced_frame_root_node_2 = CreateFencedFrame(*urn_uuid);

  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin), kBudgetAllowed);
  EXPECT_DOUBLE_EQ(
      RemainingBudgetViaJSForFrame(PrimaryFrameTreeNodeRoot()->child_at(0)),
      kBudgetAllowed);

  OpenPopup(fenced_frame_root_node_1,
            https_server()->GetURL("b.test", kSimplePagePath), /*name=*/"");

  // After the popup, log(8)=3 bits should have been withdrawn from the
  // original shared storage origin.
  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin),
                   kBudgetAllowed - 3);

  OpenPopup(fenced_frame_root_node_2,
            https_server()->GetURL("b.test", kSimplePagePath), /*name=*/"");

  // The budget can only be withdrawn once for each urn_uuid.
  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin),
                   kBudgetAllowed - 3);
  EXPECT_DOUBLE_EQ(RemainingBudgetViaJSForOrigin(shared_storage_origin),
                   kBudgetAllowed - 3);

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     1);
}

IN_PROC_BROWSER_TEST_F(SharedStorageFencedFrameInteractionBrowserTest,
                       SelectURL_InsufficientBudget) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  url::Origin shared_storage_origin = url::Origin::Create(main_url);

  WebContentsConsoleObserver console_observer(shell()->web_contents());
  console_observer.SetFilter(base::BindRepeating(IsInfoMessage));

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  EXPECT_TRUE(ExecJs(shell(), kGenerateURLsListScript));
  EXPECT_TRUE(ExecJs(shell(), "window.urls = generateUrls(8);"));
  EXPECT_TRUE(ExecJs(shell(), JsReplace("window.resolveSelectURLToConfig = $1;",
                                        ResolveSelectURLToConfig())));

  TestSelectURLFencedFrameConfigObserver config_observer_1(
      GetStoragePartition());

  // There are 2 more "worklet operations": both `selectURL()`.
  test_runtime_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(2);

  EvalJsResult result_1 = EvalJs(shell(), R"(
      (async function() {
        window.select_url_result_1 = await sharedStorage.selectURL(
          'test-url-selection-operation',
          urls,
          {
            data: {'mockResult': 1},
            resolveToConfig: resolveSelectURLToConfig,
            keepAlive: true
          }
        );
        if (resolveSelectURLToConfig &&
            !(select_url_result_1 instanceof FencedFrameConfig)) {
          throw new Error('selectURL() did not return a FencedFrameConfig.');
        }
        return window.select_url_result_1;
      })()
    )");

  EXPECT_TRUE(result_1.error.empty());
  const std::optional<GURL>& observed_urn_uuid_1 =
      config_observer_1.GetUrnUuid();
  EXPECT_TRUE(observed_urn_uuid_1.has_value());
  EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid_1.value()));

  FrameTreeNode* fenced_frame_root_node_1 = CreateFencedFrame(
      ResolveSelectURLToConfig()
          ? FencedFrameNavigationTarget("select_url_result_1")
          : FencedFrameNavigationTarget(observed_urn_uuid_1.value()));
  OpenPopup(fenced_frame_root_node_1,
            https_server()->GetURL("b.test", kSimplePagePath), /*name=*/"");

  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin),
                   kBudgetAllowed - 3);

  TestSelectURLFencedFrameConfigObserver config_observer_2(
      GetStoragePartition());
  EvalJsResult result_2 = EvalJs(shell(), R"(
      (async function() {
        window.select_url_result_2 = await sharedStorage.selectURL(
          'test-url-selection-operation',
          urls,
          {
            data: {'mockResult': 1},
            resolveToConfig: resolveSelectURLToConfig
          }
        );
        if (resolveSelectURLToConfig &&
            !(select_url_result_2 instanceof FencedFrameConfig)) {
          throw new Error('selectURL() did not return a FencedFrameConfig.');
        }
        return window.select_url_result_2;
      })()
    )");

  EXPECT_TRUE(result_2.error.empty());
  const std::optional<GURL>& observed_urn_uuid_2 =
      config_observer_2.GetUrnUuid();
  EXPECT_TRUE(observed_urn_uuid_2.has_value());
  EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid_2.value()));

  test_runtime_manager().GetAttachedWorkletHost()->WaitForWorkletResponses();

  ASSERT_TRUE(config_observer_1.ConfigObserved());
  const std::optional<FencedFrameConfig>& fenced_frame_config_1 =
      config_observer_1.GetConfig();
  EXPECT_TRUE(fenced_frame_config_1.has_value());
  EXPECT_EQ(fenced_frame_config_1->urn_uuid(), observed_urn_uuid_1.value());

  ASSERT_TRUE(config_observer_2.ConfigObserved());
  const std::optional<FencedFrameConfig>& fenced_frame_config_2 =
      config_observer_2.GetConfig();
  EXPECT_TRUE(fenced_frame_config_2.has_value());
  EXPECT_EQ(fenced_frame_config_2->urn_uuid(), observed_urn_uuid_2.value());

  EXPECT_EQ("Insufficient budget for selectURL().",
            base::UTF16ToUTF8(console_observer.messages().back().message));

  // The failed mapping due to insufficient budget (i.e. `urn_uuid_2`) should
  // not incur any budget withdrawal on subsequent top navigation from inside
  // the fenced frame.
  FrameTreeNode* fenced_frame_root_node_2 = CreateFencedFrame(
      ResolveSelectURLToConfig()
          ? FencedFrameNavigationTarget("select_url_result_2")
          : FencedFrameNavigationTarget(observed_urn_uuid_2.value()));
  OpenPopup(fenced_frame_root_node_2,
            https_server()->GetURL("c.test", kSimplePagePath), /*name=*/"");

  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin),
                   kBudgetAllowed - 3);
  EXPECT_DOUBLE_EQ(RemainingBudgetViaJSForOrigin(shared_storage_origin),
                   kBudgetAllowed - 3);

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram,
                     kSelectUrlBudgetStatusHistogram});
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     2);
  histogram_tester_.ExpectBucketCount(
      kSelectUrlBudgetStatusHistogram,
      blink::SharedStorageSelectUrlBudgetStatus::kSufficientBudget, 1);
  histogram_tester_.ExpectBucketCount(
      kSelectUrlBudgetStatusHistogram,
      blink::SharedStorageSelectUrlBudgetStatus::
          kInsufficientSiteNavigationBudget,
      1);
}

// When number of urn mappings limit has been reached, subsequent `selectURL()`
// calls will fail.
IN_PROC_BROWSER_TEST_F(SharedStorageFencedFrameInteractionBrowserTest,
                       SelectURL_Fails_ExceedNumOfUrnMappingsLimit) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // `selectURL()` succeeds when map is not full.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  EXPECT_TRUE(ExecJs(shell(), JsReplace("window.resolveSelectURLToConfig = $1;",
                                        ResolveSelectURLToConfig())));
  EXPECT_TRUE(ExecJs(shell(), "window.keepWorklet = true;"));

  // There is 1 more "worklet operation": `selectURL()`.
  test_runtime_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  std::string select_url_script = R"(
      sharedStorage.selectURL(
        'test-url-selection-operation',
        [
          {
            url: "fenced_frames/title0.html"
          }
        ],
        {
          data: {'mockResult': 0},
          resolveToConfig: resolveSelectURLToConfig,
          keepAlive: keepWorklet
        }
      );
    )";
  EXPECT_TRUE(ExecJs(shell(), select_url_script));

  test_runtime_manager().GetAttachedWorkletHost()->WaitForWorkletResponses();

  FencedFrameURLMapping& fenced_frame_url_mapping =
      root->current_frame_host()->GetPage().fenced_frame_urls_map();
  FencedFrameURLMappingTestPeer fenced_frame_url_mapping_test_peer(
      &fenced_frame_url_mapping);

  // Fill the map until its size reaches the limit.
  GURL url("https://a.test");
  fenced_frame_url_mapping_test_peer.FillMap(url);

  // No need to keep the worklet after the next select operation.
  EXPECT_TRUE(ExecJs(shell(), "window.keepWorklet = false;"));

  EvalJsResult extra_result = EvalJs(shell(), select_url_script);

  // `selectURL()` fails when map is full.
  std::string expected_error = base::StrCat(
      {"a JavaScript error: \"OperationError: ",
       "sharedStorage.selectURL() failed because number of urn::uuid to url ",
       "mappings has reached the limit.\"\n"});
  EXPECT_EQ(expected_error, extra_result.error);
}

class SharedStorageFencedFrameDocumentGetFeatureDisabledBrowserTest
    : public SharedStorageFencedFrameInteractionBrowserTest {
 public:
  SharedStorageFencedFrameDocumentGetFeatureDisabledBrowserTest() {
    fenced_frame_feature_.InitAndDisableFeature(
        blink::features::kFencedFramesLocalUnpartitionedDataAccess);
  }

 private:
  base::test::ScopedFeatureList fenced_frame_feature_;
};

IN_PROC_BROWSER_TEST_F(
    SharedStorageFencedFrameDocumentGetFeatureDisabledBrowserTest,
    GetDisabledInFencedFrameWithFeatureOff) {
  GURL main_frame_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));

  EXPECT_TRUE(ExecJs(shell(), R"(
    sharedStorage.set('test', 'apple');
  )"));

  FrameTreeNode* fenced_frame_root_node =
      CreateFencedFrame(https_server()->GetURL("a.test", kFencedFramePath));

  EvalJsResult get_result = EvalJs(fenced_frame_root_node, R"(
    sharedStorage.get('test');
  )");

  EXPECT_THAT(
      get_result.error,
      testing::HasSubstr("Cannot call get() in a fenced frame with feature "
                         "FencedFramesLocalUnpartitionedDataAccess disabled."));

  // Check that a histogram was logged for the failed get() operation.
  content::FetchHistogramsFromChildProcesses();
  histogram_tester_.ExpectTotalCount(
      blink::kSharedStorageGetInFencedFrameOutcome, 1);
  histogram_tester_.ExpectBucketCount(
      blink::kSharedStorageGetInFencedFrameOutcome,
      blink::SharedStorageGetInFencedFrameOutcome::kFeatureDisabled, 1);
}

class SharedStorageFencedFrameDocumentGetBrowserTest
    : public SharedStorageFencedFrameInteractionBrowserTest {
 public:
  SharedStorageFencedFrameDocumentGetBrowserTest() {
    fenced_frame_feature_.InitAndEnableFeature(
        /*feature=*/
        blink::features::kFencedFramesLocalUnpartitionedDataAccess);
  }

  void SetUpOnMainThread() override {
    SharedStorageFencedFrameInteractionBrowserTest::SetUpOnMainThread();

    // Bypass fenced storage read attestation check.
    ON_CALL(browser_client(), IsFencedStorageReadAllowed)
        .WillByDefault(testing::Return(true));
  }

 private:
  base::test::ScopedFeatureList fenced_frame_feature_;
};

IN_PROC_BROWSER_TEST_F(SharedStorageFencedFrameDocumentGetBrowserTest,
                       GetAllowedInNetworkRestrictedFencedFrame) {
  GURL main_frame_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));

  EXPECT_TRUE(ExecJs(shell(), R"(
    sharedStorage.set('test', 'apple');
  )"));

  FrameTreeNode* fenced_frame_root_node =
      CreateFencedFrame(https_server()->GetURL("a.test", kFencedFramePath));

  EvalJsResult get_result = EvalJs(fenced_frame_root_node, R"(
    (async () => {
      await window.fence.disableUntrustedNetwork();
      return sharedStorage.get('test');
    })();
  )");

  EXPECT_EQ(get_result, "apple");

  // Check that a histogram was logged for the get() result.
  content::FetchHistogramsFromChildProcesses();
  histogram_tester_.ExpectTotalCount(
      blink::kSharedStorageGetInFencedFrameOutcome, 1);
  histogram_tester_.ExpectBucketCount(
      blink::kSharedStorageGetInFencedFrameOutcome,
      blink::SharedStorageGetInFencedFrameOutcome::kSuccess, 1);
}

IN_PROC_BROWSER_TEST_F(SharedStorageFencedFrameDocumentGetBrowserTest,
                       GetRejectsInFencedFrameWithoutRestrictedNetwork) {
  GURL main_frame_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));

  EXPECT_TRUE(ExecJs(shell(), R"(
    sharedStorage.set('test', 'apple');
  )"));

  FrameTreeNode* fenced_frame_root_node =
      CreateFencedFrame(https_server()->GetURL("a.test", kFencedFramePath));

  EvalJsResult get_result = EvalJs(fenced_frame_root_node, R"(
    sharedStorage.get('test');
  )");

  EXPECT_THAT(
      get_result.error,
      testing::HasSubstr(
          "sharedStorage.get() is not allowed in a fenced frame until network "
          "access for it and all descendent frames has been revoked with "
          "window.fence.disableUntrustedNetwork()"));

  // Check that a histogram was logged for the get() result.
  content::FetchHistogramsFromChildProcesses();
  histogram_tester_.ExpectTotalCount(
      blink::kSharedStorageGetInFencedFrameOutcome, 1);
  histogram_tester_.ExpectBucketCount(
      blink::kSharedStorageGetInFencedFrameOutcome,
      blink::SharedStorageGetInFencedFrameOutcome::kWithoutRevokeNetwork, 1);
}

IN_PROC_BROWSER_TEST_F(SharedStorageFencedFrameDocumentGetBrowserTest,
                       GetInFencedFrameOnlyFetchesValuesFromCurrentOrigin) {
  // sharedStorage.set() for a.test
  GURL main_frame_url1 = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url1));
  EXPECT_TRUE(ExecJs(shell(), R"(
    sharedStorage.set('test', 'apple');
  )"));

  // sharedStorage.set() for b.test
  GURL main_frame_url2 = https_server()->GetURL("b.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url2));
  EXPECT_TRUE(ExecJs(shell(), R"(
    sharedStorage.set('test', 'banana');
  )"));

  // An a.test fenced frame embedded in b.test should only read a.test's set
  // values.
  FrameTreeNode* fenced_frame_root_node =
      CreateFencedFrame(https_server()->GetURL("a.test", kFencedFramePath));

  EvalJsResult get_result = EvalJs(fenced_frame_root_node, R"(
    (async () => {
      await window.fence.disableUntrustedNetwork();
      return sharedStorage.get('test');
    })();
  )");

  EXPECT_EQ(get_result, "apple");
}

IN_PROC_BROWSER_TEST_F(SharedStorageFencedFrameDocumentGetBrowserTest,
                       GetRejectsInMainFrame) {
  GURL main_frame_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));

  EXPECT_TRUE(ExecJs(shell(), R"(
    sharedStorage.set('test', 'apple');
  )"));

  EvalJsResult get_result_main_frame = EvalJs(shell(), R"(
    sharedStorage.get('test');
  )");

  EXPECT_THAT(
      get_result_main_frame.error,
      testing::HasSubstr("Cannot call get() outside of a fenced frame."));

  // The "Blink.FencedFrame.SharedStorageGetInFencedFrameOutcome" histogram
  // should not log since get() was not called from within a fenced frame.
  content::FetchHistogramsFromChildProcesses();
  histogram_tester_.ExpectTotalCount(
      blink::kSharedStorageGetInFencedFrameOutcome, 0);
}

IN_PROC_BROWSER_TEST_F(SharedStorageFencedFrameDocumentGetBrowserTest,
                       GetRejectsInIFrame) {
  GURL main_frame_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));

  EXPECT_TRUE(ExecJs(shell(), R"(
    sharedStorage.set('test', 'apple');
  )"));

  FrameTreeNode* iframe_root =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), main_frame_url);

  EvalJsResult get_result_iframe = EvalJs(iframe_root, R"(
    sharedStorage.get('test');
  )");

  EXPECT_THAT(
      get_result_iframe.error,
      testing::HasSubstr("Cannot call get() outside of a fenced frame."));

  // The "Blink.FencedFrame.SharedStorageGetInFencedFrameOutcome" histogram
  // should not log since get() was not called from within a fenced frame.
  content::FetchHistogramsFromChildProcesses();
  histogram_tester_.ExpectTotalCount(
      blink::kSharedStorageGetInFencedFrameOutcome, 0);
}

IN_PROC_BROWSER_TEST_F(
    SharedStorageFencedFrameDocumentGetBrowserTest,
    GetAllowedInNetworkRestrictedNestedFencedFrameIfParentStillHasNetwork) {
  GURL main_frame_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));

  EXPECT_TRUE(ExecJs(shell(), R"(
    sharedStorage.set('test', 'apple');
  )"));

  GURL fenced_frame_url = https_server()->GetURL("a.test", kFencedFramePath);
  // The parent fenced frame never calls window.fence.disableUntrustedNetwork().
  FrameTreeNode* fenced_frame_root_node = CreateFencedFrame(fenced_frame_url);

  FrameTreeNode* nested_fenced_frame_root_node =
      CreateFencedFrame(fenced_frame_root_node, fenced_frame_url);

  EvalJsResult get_result = EvalJs(nested_fenced_frame_root_node, R"(
    (async () => {
      await window.fence.disableUntrustedNetwork();
      return sharedStorage.get('test');
    })();
  )");

  EXPECT_EQ(get_result, "apple");
}

IN_PROC_BROWSER_TEST_F(SharedStorageFencedFrameDocumentGetBrowserTest,
                       GetNotAllowedInSandboxedIframeInFencedFrameTree) {
  GURL main_frame_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));

  GURL fenced_frame_url = https_server()->GetURL("a.test", kFencedFramePath);
  FrameTreeNode* fenced_frame_root_node = CreateFencedFrame(fenced_frame_url);

  FrameTreeNode* nested_iframe_node =
      CreateIFrame(fenced_frame_root_node, fenced_frame_url,
                   /*sandbox_flags=*/"allow-scripts");

  EXPECT_TRUE(ExecJs(fenced_frame_root_node, R"(
      window.fence.disableUntrustedNetwork();
  )"));

  EvalJsResult get_result = EvalJs(nested_iframe_node, R"(
      sharedStorage.get('test');
  )");

  EXPECT_THAT(get_result.error,
              testing::HasSubstr("is not allowed in an opaque origin context"));

  // The "Blink.FencedFrame.SharedStorageGetInFencedFrameOutcome" histogram
  // should not log since opaque origins are treated as being outside of a
  // fenced frame tree.
  content::FetchHistogramsFromChildProcesses();
  histogram_tester_.ExpectTotalCount(
      blink::kSharedStorageGetInFencedFrameOutcome, 0);
}

IN_PROC_BROWSER_TEST_F(
    SharedStorageFencedFrameDocumentGetBrowserTest,
    GetNotAllowedInNetworkRestrictedParentFencedFrameIfChildStillHasNetwork) {
  GURL main_frame_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));

  EXPECT_TRUE(ExecJs(shell(), R"(
    sharedStorage.set('test', 'apple');
  )"));

  GURL fenced_frame_url = https_server()->GetURL("a.test", kFencedFramePath);
  FrameTreeNode* fenced_frame_root_node = CreateFencedFrame(fenced_frame_url);

  CreateFencedFrame(fenced_frame_root_node, fenced_frame_url);

  // Note that we do *not* await the call to disableUntrustedNetwork, because we
  // need to operate in the top frame while the nested frame still hasn't
  // disabled network access.
  EXPECT_TRUE(ExecJs(fenced_frame_root_node, R"(
    (async () => {
      window.fence.disableUntrustedNetwork();
    })();
  )"));

  // Wait before calling sharedStorage.get() in case the fenced frame was given
  // access to Shared Storage without disableUntrustedNetwork() actually
  // resolving.
  base::RunLoop disable_network_wait;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, disable_network_wait.QuitClosure(), base::Milliseconds(500));
  disable_network_wait.Run();

  EvalJsResult get_result = EvalJs(fenced_frame_root_node, R"(
    sharedStorage.get('test');
  )");

  EXPECT_THAT(
      get_result.error,
      testing::HasSubstr(
          "sharedStorage.get() is not allowed in a fenced frame until network "
          "access for it and all descendent frames has been revoked with "
          "window.fence.disableUntrustedNetwork()"));

  // Check that a histogram was logged for the get() result.
  content::FetchHistogramsFromChildProcesses();
  histogram_tester_.ExpectTotalCount(
      blink::kSharedStorageGetInFencedFrameOutcome, 1);
  histogram_tester_.ExpectBucketCount(
      blink::kSharedStorageGetInFencedFrameOutcome,
      blink::SharedStorageGetInFencedFrameOutcome::kWithoutRevokeNetwork, 1);
}

class SharedStorageSelectURLNotAllowedInFencedFrameBrowserTest
    : public SharedStorageFencedFrameInteractionBrowserTest {
 public:
  SharedStorageSelectURLNotAllowedInFencedFrameBrowserTest() {
    shared_storage_feature_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{network::features::kSharedStorageAPI,
          {{"SharedStorageBitBudget", base::NumberToString(kBudgetAllowed)},
           {"SharedStorageMaxAllowedFencedFrameDepthForSelectURL", "0"}}}},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList shared_storage_feature_;
};

IN_PROC_BROWSER_TEST_F(SharedStorageSelectURLNotAllowedInFencedFrameBrowserTest,
                       SelectURLNotAllowedInFencedFrame) {
  GURL main_frame_url = https_server()->GetURL("a.test", kSimplePagePath);

  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));

  url::Origin shared_storage_origin =
      url::Origin::Create(https_server()->GetURL("b.test", kSimplePagePath));
  std::optional<GURL> urn_uuid =
      SelectFrom8URLsInContext(shared_storage_origin);
  ASSERT_TRUE(urn_uuid.has_value());

  FrameTreeNode* fenced_frame_node = CreateFencedFrame(*urn_uuid);

  EXPECT_TRUE(ExecJs(fenced_frame_node, R"(
      sharedStorage.worklet.addModule('/shared_storage/simple_module.js');
    )"));

  EXPECT_EQ(2u, test_runtime_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_runtime_manager().GetKeepAliveWorkletHostsCount());

  EXPECT_TRUE(ExecJs(fenced_frame_node,
                     JsReplace("window.resolveSelectURLToConfig = $1;",
                               ResolveSelectURLToConfig())));
  EvalJsResult result = EvalJs(fenced_frame_node, R"(
      sharedStorage.selectURL(
        'test-url-selection-operation',
        [
          {
            url: "fenced_frames/title0.html"
          }
        ],
        {
          data: {'mockResult': 0},
          resolveToConfig: resolveSelectURLToConfig
        }
      );
    )");

  EXPECT_THAT(result.error,
              testing::HasSubstr(
                  "selectURL() is called in a context with a fenced frame "
                  "depth (1) exceeding the maximum allowed number (0)."));
}

class SharedStorageReportEventBrowserTest
    : public SharedStorageFencedFrameInteractionBrowserTest {
  void FinishSetup() override {
    https_server()->ServeFilesFromSourceDirectory(GetTestDataFilePath());
    https_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  }
};

IN_PROC_BROWSER_TEST_F(SharedStorageReportEventBrowserTest,
                       SelectURL_ReportEvent) {
  net::test_server::ControllableHttpResponse response1(
      https_server(), "/fenced_frames/report1.html");
  net::test_server::ControllableHttpResponse response2(
      https_server(), "/fenced_frames/report2.html");
  ASSERT_TRUE(https_server()->Start());

  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  url::Origin shared_storage_origin = url::Origin::Create(main_url);

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  EXPECT_TRUE(ExecJs(shell(), JsReplace("window.resolveSelectURLToConfig = $1;",
                                        ResolveSelectURLToConfig())));

  TestSelectURLFencedFrameConfigObserver config_observer(GetStoragePartition());
  // There is 1 more "worklet operation": `selectURL()`.
  test_runtime_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EvalJsResult result = EvalJs(shell(), R"(
      (async function() {
        window.select_url_result = await sharedStorage.selectURL(
          'test-url-selection-operation',
          [
            {
              url: "fenced_frames/title0.html"
            },
            {
              url: "fenced_frames/title1.html",
              reportingMetadata: {
                "click": "fenced_frames/report1.html",
                "mouse interaction": "fenced_frames/report2.html"
              }
            }
          ],
          {
            data: {'mockResult': 1},
            resolveToConfig: resolveSelectURLToConfig
          }
        );
        if (resolveSelectURLToConfig &&
            !(select_url_result instanceof FencedFrameConfig)) {
          throw new Error('selectURL() did not return a FencedFrameConfig.');
        }
        return window.select_url_result;
      })()
    )");

  EXPECT_TRUE(result.error.empty());
  const std::optional<GURL>& observed_urn_uuid = config_observer.GetUrnUuid();
  EXPECT_TRUE(observed_urn_uuid.has_value());
  EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid.value()));

  if (!ResolveSelectURLToConfig()) {
    EXPECT_EQ(result.ExtractString(), observed_urn_uuid->spec());
  }

  test_runtime_manager().GetAttachedWorkletHost()->WaitForWorkletResponses();

  FrameTreeNode* fenced_frame_root_node = CreateFencedFrame(
      ResolveSelectURLToConfig()
          ? FencedFrameNavigationTarget("select_url_result")
          : FencedFrameNavigationTarget(observed_urn_uuid.value()));

  std::string event_data1 = "this is a click";
  EXPECT_TRUE(
      ExecJs(fenced_frame_root_node,
             JsReplace("window.fence.reportEvent({"
                       "  eventType: 'click',"
                       "  eventData: $1,"
                       "  destination: ['shared-storage-select-url']});",
                       event_data1)));

  response1.WaitForRequest();
  EXPECT_EQ(response1.http_request()->content, event_data1);

  std::string event_data2 = "this is a mouse interaction";
  EXPECT_TRUE(
      ExecJs(fenced_frame_root_node,
             JsReplace("window.fence.reportEvent({"
                       "  eventType: 'mouse interaction',"
                       "  eventData: $1,"
                       "  destination: ['shared-storage-select-url']});",
                       event_data2)));

  response2.WaitForRequest();
  EXPECT_EQ(response2.http_request()->content, event_data2);

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     1);
}

class SharedStorageSelectURLLimitBrowserTestBase
    : public SharedStorageBrowserTestBase {
 public:
  virtual bool LimitSelectURLCalls() const { return true; }

  // Precondition: `addModule('shared_storage/simple_module.js')` has been
  // called in the main frame.
  void RunSuccessfulSelectURLInMainFrame(
      std::string host_str,
      int num_urls,
      WebContentsConsoleObserver* console_observer,
      const std::u16string& saved_query_name = u"") {
    std::optional<std::pair<GURL, double>> result_pair =
        RunSelectURLExtractingMappedURLAndBudgetToCharge(
            shell(), host_str, num_urls, saved_query_name);
    ASSERT_TRUE(result_pair.has_value());

    GURL expected_mapped_url = https_server()->GetURL(
        host_str, base::StrCat({"/fenced_frames/title",
                                base::NumberToString(num_urls - 1), ".html"}));
    EXPECT_EQ(result_pair->first, expected_mapped_url);
    EXPECT_DOUBLE_EQ(result_pair->second, std::log2(num_urls));

    EXPECT_EQ("Finish executing 'test-url-selection-operation'",
              base::UTF16ToUTF8(console_observer->messages().back().message));
  }

  // Precondition: `addModule('shared_storage/simple_module.js')` has NOT been
  // called in `iframe_node`.
  void RunSuccessfulSelectURLInIframe(
      FrameTreeNode* iframe_node,
      int num_urls,
      WebContentsConsoleObserver* console_observer,
      const std::u16string& saved_query_name = u"") {
    std::string host_str =
        iframe_node->current_frame_host()->GetLastCommittedURL().host();
    EXPECT_TRUE(ExecJs(iframe_node, R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

    std::optional<std::pair<GURL, double>> result_pair =
        RunSelectURLExtractingMappedURLAndBudgetToCharge(
            iframe_node, host_str, num_urls, saved_query_name);
    ASSERT_TRUE(result_pair.has_value());

    GURL expected_mapped_url = https_server()->GetURL(
        host_str, base::StrCat({"/fenced_frames/title",
                                base::NumberToString(num_urls - 1), ".html"}));
    EXPECT_EQ(result_pair->first, expected_mapped_url);
    EXPECT_DOUBLE_EQ(result_pair->second, std::log2(num_urls));

    EXPECT_EQ("Finish executing 'test-url-selection-operation'",
              base::UTF16ToUTF8(console_observer->messages().back().message));
  }

  // Precondition: `addModule('shared_storage/simple_module.js')` has been
  // called in the `execution_target`.
  std::optional<std::pair<GURL, double>>
  RunSelectURLExtractingMappedURLAndBudgetToCharge(
      const ToRenderFrameHost& execution_target,
      std::string host_str,
      int num_urls,
      const std::u16string& saved_query_name = u"") {
    TestSelectURLFencedFrameConfigObserver config_observer(
        GetStoragePartition());

    // There is 1 "worklet operation": `selectURL()`.
    test_runtime_manager()
        .GetAttachedWorkletHostForFrame(execution_target.render_frame_host())
        ->SetExpectedWorkletResponsesCount(1);

    EvalJsResult result =
        RunSelectURLScript(execution_target, num_urls, saved_query_name);

    EXPECT_TRUE(result.error.empty()) << result.error;
    const std::optional<GURL>& observed_urn_uuid = config_observer.GetUrnUuid();
    if (!observed_urn_uuid.has_value()) {
      return std::nullopt;
    }
    EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid.value()));

    if (!ResolveSelectURLToConfig()) {
      EXPECT_EQ(result.ExtractString(), observed_urn_uuid->spec());
    }

    test_runtime_manager()
        .GetAttachedWorkletHostForFrame(execution_target.render_frame_host())
        ->WaitForWorkletResponses();

    const std::optional<FencedFrameConfig>& config =
        config_observer.GetConfig();
    if (!config.has_value()) {
      return std::nullopt;
    }
    EXPECT_TRUE(config->mapped_url().has_value());

    SharedStorageBudgetMetadata* metadata =
        GetSharedStorageBudgetMetadata(observed_urn_uuid.value());
    if (!metadata) {
      return std::nullopt;
    }
    EXPECT_EQ(metadata->site,
              net::SchemefulSite(https_server()->GetOrigin(host_str)));

    return std::make_pair(config->mapped_url()->GetValueIgnoringVisibility(),
                          metadata->budget_to_charge);
  }

 private:
  EvalJsResult RunSelectURLScript(const ToRenderFrameHost& execution_target,
                                  int num_urls,
                                  const std::u16string& saved_query_name = u"",
                                  bool keep_alive_after_operation = true) {
    EXPECT_TRUE(ExecJs(execution_target, kGenerateURLsListScript));
    EXPECT_TRUE(
        ExecJs(execution_target, JsReplace("window.numUrls = $1;", num_urls)));
    EXPECT_TRUE(ExecJs(execution_target,
                       JsReplace("window.resolveSelectURLToConfig = $1;",
                                 ResolveSelectURLToConfig())));
    EXPECT_TRUE(ExecJs(
        execution_target,
        JsReplace("window.keepWorklet = $1;", keep_alive_after_operation)));
    EXPECT_TRUE(
        ExecJs(execution_target,
               JsReplace("window.savedQueryName = $1;", saved_query_name)));

    EvalJsResult result = EvalJs(execution_target, R"(
      (async function() {
        const urls = generateUrls(numUrls);
        window.select_url_result = await sharedStorage.selectURL(
          'test-url-selection-operation',
          urls,
          {
            data: {'mockResult': numUrls - 1},
            resolveToConfig: resolveSelectURLToConfig,
            keepAlive: keepWorklet,
            savedQuery: savedQueryName
          }
        );
        if (resolveSelectURLToConfig &&
            !(select_url_result instanceof FencedFrameConfig)) {
          throw new Error('selectURL() did not return a FencedFrameConfig.');
        }
        return window.select_url_result;
      })()
    )");
    return result;
  }
};

class SharedStorageSelectURLLimitBrowserTest
    : public SharedStorageSelectURLLimitBrowserTestBase,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  SharedStorageSelectURLLimitBrowserTest() {
    if (LimitSelectURLCalls()) {
      select_url_limit_feature_list_.InitWithFeaturesAndParameters(
          /*enabled_features=*/
          {{features::kSharedStorageSelectURLLimit,
            {{"SharedStorageSelectURLBitBudgetPerPageLoad",
              base::NumberToString(kSelectURLOverallBitBudget)},
             {"SharedStorageSelectURLBitBudgetPerSitePerPageLoad",
              base::NumberToString(kSelectURLSiteBitBudget)}}}},
          /*disabled_features=*/{});
    } else {
      select_url_limit_feature_list_.InitAndDisableFeature(
          features::kSharedStorageSelectURLLimit);
    }

    fenced_frame_api_change_feature_.InitWithFeatureState(
        blink::features::kFencedFramesAPIChanges, ResolveSelectURLToConfig());
  }

  bool LimitSelectURLCalls() const override { return std::get<0>(GetParam()); }

  bool ResolveSelectURLToConfig() override { return std::get<1>(GetParam()); }

 private:
  base::test::ScopedFeatureList select_url_limit_feature_list_;
  base::test::ScopedFeatureList fenced_frame_api_change_feature_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         SharedStorageSelectURLLimitBrowserTest,
                         testing::Combine(testing::Bool(), testing::Bool()),
                         [](const auto& info) {
                           return base::StrCat(
                               {"LimitSelectURLCalls",
                                std::get<0>(info.param) ? "Enabled"
                                                        : "Disabled",
                                "_ResolveSelectURLTo",
                                std::get<1>(info.param) ? "Config" : "URN"});
                         });

IN_PROC_BROWSER_TEST_P(SharedStorageSelectURLLimitBrowserTest,
                       SelectURL_MainFrame_SameEntropy_SiteLimitReached) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  // This test relies on the assumption that `kSelectURLOverallBitBudget` is set
  // to be greater than or equal to `kSelectURLSiteBitBudget`.
  EXPECT_GE(kSelectURLOverallBitBudget, kSelectURLSiteBitBudget);

  // Here each call to `selectURL()` will have 8 input URLs, and hence
  // 3 = log2(8) bits of entropy.
  int call_limit = kSelectURLSiteBitBudget / 3;

  for (int i = 0; i < call_limit; i++) {
    RunSuccessfulSelectURLInMainFrame("a.test", /*num_urls=*/8,
                                      &console_observer);
  }

  if (LimitSelectURLCalls()) {
    // The limit for `selectURL()` has now been reached for "a.test". Make one
    // more call, which will return the default URL due to insufficient site
    // pageload budget.
    std::optional<std::pair<GURL, double>> result_pair =
        RunSelectURLExtractingMappedURLAndBudgetToCharge(shell(), "a.test",
                                                         /*num_urls=*/8);
    ASSERT_TRUE(result_pair.has_value());

    GURL expected_mapped_url =
        https_server()->GetURL("a.test", "/fenced_frames/title0.html");
    EXPECT_EQ(result_pair->first, expected_mapped_url);
    EXPECT_DOUBLE_EQ(result_pair->second, 0.0);

    EXPECT_EQ("Insufficient budget for selectURL().",
              base::UTF16ToUTF8(console_observer.messages().back().message));
  } else {
    // The `selectURL()` limit is disabled. The call will run successfully.
    RunSuccessfulSelectURLInMainFrame("a.test", /*num_urls=*/8,
                                      &console_observer);
  }

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram,
                     kSelectUrlBudgetStatusHistogram});

  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     call_limit + 1);

  if (LimitSelectURLCalls()) {
    histogram_tester_.ExpectBucketCount(
        kSelectUrlBudgetStatusHistogram,
        blink::SharedStorageSelectUrlBudgetStatus::kSufficientBudget,
        call_limit);
    histogram_tester_.ExpectBucketCount(
        kSelectUrlBudgetStatusHistogram,
        blink::SharedStorageSelectUrlBudgetStatus::
            kInsufficientSitePageloadBudget,
        1);
  } else {
    histogram_tester_.ExpectUniqueSample(
        kSelectUrlBudgetStatusHistogram,
        blink::SharedStorageSelectUrlBudgetStatus::kSufficientBudget,
        call_limit + 1);
  }
}

IN_PROC_BROWSER_TEST_P(SharedStorageSelectURLLimitBrowserTest,
                       SelectURL_MainFrame_DifferentEntropy_SiteLimitReached) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  // This test relies on the assumptions that `kSelectURLOverallBitBudget` is
  // set to be greater than or equal to `kSelectURLSiteBitBudget` and that the
  // latter is at least 3.
  EXPECT_GE(kSelectURLOverallBitBudget, kSelectURLSiteBitBudget);
  EXPECT_GE(kSelectURLSiteBitBudget, 3);

  // Here the first call to `selectURL()` will have 8 input URLs, and hence
  // 3 = log2(8) bits of entropy, and the subsequent calls will each have 4
  // input URLs, and hence 2 = log2(4) bits of entropy.
  int input4_call_limit = (kSelectURLSiteBitBudget - 3) / 2;

  RunSuccessfulSelectURLInMainFrame("a.test", /*num_urls=*/8,
                                    &console_observer);

  for (int i = 0; i < input4_call_limit; i++) {
    RunSuccessfulSelectURLInMainFrame("a.test", /*num_urls=*/4,
                                      &console_observer);
  }

  if (LimitSelectURLCalls()) {
    // The limit for `selectURL()` has now been reached for "a.test". Make one
    // more call, which will return the default URL due to insufficient site
    // pageload budget.
    std::optional<std::pair<GURL, double>> result_pair =
        RunSelectURLExtractingMappedURLAndBudgetToCharge(shell(), "a.test",
                                                         /*num_urls=*/4);
    ASSERT_TRUE(result_pair.has_value());

    GURL expected_mapped_url =
        https_server()->GetURL("a.test", "/fenced_frames/title0.html");
    EXPECT_EQ(result_pair->first, expected_mapped_url);
    EXPECT_DOUBLE_EQ(result_pair->second, 0.0);

    EXPECT_EQ("Insufficient budget for selectURL().",
              base::UTF16ToUTF8(console_observer.messages().back().message));
  } else {
    // The `selectURL()` limit is disabled. The call will run successfully.
    RunSuccessfulSelectURLInMainFrame("a.test", /*num_urls=*/4,
                                      &console_observer);
  }

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram,
                     kSelectUrlBudgetStatusHistogram});

  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     input4_call_limit + 2);

  if (LimitSelectURLCalls()) {
    histogram_tester_.ExpectBucketCount(
        kSelectUrlBudgetStatusHistogram,
        blink::SharedStorageSelectUrlBudgetStatus::kSufficientBudget,
        input4_call_limit + 1);
    histogram_tester_.ExpectBucketCount(
        kSelectUrlBudgetStatusHistogram,
        blink::SharedStorageSelectUrlBudgetStatus::
            kInsufficientSitePageloadBudget,
        1);
  } else {
    histogram_tester_.ExpectUniqueSample(
        kSelectUrlBudgetStatusHistogram,
        blink::SharedStorageSelectUrlBudgetStatus::kSufficientBudget,
        input4_call_limit + 2);
  }
}

IN_PROC_BROWSER_TEST_P(
    SharedStorageSelectURLLimitBrowserTest,
    SelectURL_IframesSharingCommonSite_SameEntropy_SiteLimitReached) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  // This test relies on the assumption that `kSelectURLOverallBitBudget` is set
  // to be greater than or equal to `kSelectURLSiteBitBudget`.
  EXPECT_GE(kSelectURLOverallBitBudget, kSelectURLSiteBitBudget);

  // Here each call to `selectURL()` will have 8 input URLs, and hence
  // 3 = log2(8) bits of entropy.
  int call_limit = kSelectURLSiteBitBudget / 3;

  GURL iframe_url = https_server()->GetURL("b.test", kSimplePagePath);

  for (int i = 0; i < call_limit; i++) {
    // Create a new iframe.
    FrameTreeNode* iframe_node =
        CreateIFrame(PrimaryFrameTreeNodeRoot(), iframe_url);

    RunSuccessfulSelectURLInIframe(iframe_node, /*num_urls=*/8,
                                   &console_observer);
  }

  // Create a new iframe.
  FrameTreeNode* iframe_node =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), iframe_url);

  if (LimitSelectURLCalls()) {
    EXPECT_TRUE(ExecJs(iframe_node, R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

    // The limit for `selectURL()` has now been reached for "b.test". Make one
    // more call, which will return the default URL due to insufficient site
    // pageload budget.
    std::optional<std::pair<GURL, double>> result_pair =
        RunSelectURLExtractingMappedURLAndBudgetToCharge(iframe_node, "b.test",
                                                         /*num_urls=*/8);
    ASSERT_TRUE(result_pair.has_value());

    GURL expected_mapped_url =
        https_server()->GetURL("b.test", "/fenced_frames/title0.html");
    EXPECT_EQ(result_pair->first, expected_mapped_url);
    EXPECT_DOUBLE_EQ(result_pair->second, 0.0);

    EXPECT_EQ("Insufficient budget for selectURL().",
              base::UTF16ToUTF8(console_observer.messages().back().message));
  } else {
    // The `selectURL()` limit is disabled. The call will run successfully.
    RunSuccessfulSelectURLInIframe(iframe_node, /*num_urls=*/8,
                                   &console_observer);
  }

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram,
                     kSelectUrlBudgetStatusHistogram});
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     call_limit + 1);

  if (LimitSelectURLCalls()) {
    histogram_tester_.ExpectBucketCount(
        kSelectUrlBudgetStatusHistogram,
        blink::SharedStorageSelectUrlBudgetStatus::kSufficientBudget,
        call_limit);
    histogram_tester_.ExpectBucketCount(
        kSelectUrlBudgetStatusHistogram,
        blink::SharedStorageSelectUrlBudgetStatus::
            kInsufficientSitePageloadBudget,
        1);
  } else {
    histogram_tester_.ExpectUniqueSample(
        kSelectUrlBudgetStatusHistogram,
        blink::SharedStorageSelectUrlBudgetStatus::kSufficientBudget,
        call_limit + 1);
  }
}

IN_PROC_BROWSER_TEST_P(
    SharedStorageSelectURLLimitBrowserTest,
    SelectURL_IframesSharingCommonSite_DifferentEntropy_SiteLimitReached) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL iframe_url = https_server()->GetURL("b.test", kSimplePagePath);

  // Create a new iframe.
  FrameTreeNode* first_iframe_node =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), iframe_url);

  RunSuccessfulSelectURLInIframe(first_iframe_node, /*num_urls=*/8,
                                 &console_observer);

  // This test relies on the assumptions that `kSelectURLOverallBitBudget` is
  // set to be greater than or equal to `kSelectURLSiteBitBudget` and that the
  // latter is at least 3.
  EXPECT_GE(kSelectURLOverallBitBudget, kSelectURLSiteBitBudget);
  EXPECT_GE(kSelectURLSiteBitBudget, 3);

  // Here the first call to `selectURL()` will have 8 input URLs, and hence
  // 3 = log2(8) bits of entropy, and the subsequent calls will each have 4
  // input URLs, and hence 2 = log2(4) bits of entropy.
  int input4_call_limit = (kSelectURLSiteBitBudget - 3) / 2;

  for (int i = 0; i < input4_call_limit; i++) {
    // Create a new iframe.
    FrameTreeNode* iframe_node =
        CreateIFrame(PrimaryFrameTreeNodeRoot(), iframe_url);

    RunSuccessfulSelectURLInIframe(iframe_node, /*num_urls=*/4,
                                   &console_observer);
  }

  // Create a new iframe.
  FrameTreeNode* last_iframe_node =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), iframe_url);

  if (LimitSelectURLCalls()) {
    EXPECT_TRUE(ExecJs(last_iframe_node, R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

    // The limit for `selectURL()` has now been reached for "b.test". Make one
    // more call, which will return the default URL due to insufficient site
    // pageload budget.
    std::optional<std::pair<GURL, double>> result_pair =
        RunSelectURLExtractingMappedURLAndBudgetToCharge(last_iframe_node,
                                                         "b.test",
                                                         /*num_urls=*/4);
    ASSERT_TRUE(result_pair.has_value());

    GURL expected_mapped_url =
        https_server()->GetURL("b.test", "/fenced_frames/title0.html");
    EXPECT_EQ(result_pair->first, expected_mapped_url);
    EXPECT_DOUBLE_EQ(result_pair->second, 0.0);

    EXPECT_EQ("Insufficient budget for selectURL().",
              base::UTF16ToUTF8(console_observer.messages().back().message));
  } else {
    // The `selectURL()` limit is disabled. The call will run successfully.
    RunSuccessfulSelectURLInIframe(last_iframe_node, /*num_urls=*/4,
                                   &console_observer);
  }

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram,
                     kSelectUrlBudgetStatusHistogram});
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     input4_call_limit + 2);

  if (LimitSelectURLCalls()) {
    histogram_tester_.ExpectBucketCount(
        kSelectUrlBudgetStatusHistogram,
        blink::SharedStorageSelectUrlBudgetStatus::kSufficientBudget,
        input4_call_limit + 1);
    histogram_tester_.ExpectBucketCount(
        kSelectUrlBudgetStatusHistogram,
        blink::SharedStorageSelectUrlBudgetStatus::
            kInsufficientSitePageloadBudget,
        1);
  } else {
    histogram_tester_.ExpectUniqueSample(
        kSelectUrlBudgetStatusHistogram,
        blink::SharedStorageSelectUrlBudgetStatus::kSufficientBudget,
        input4_call_limit + 2);
  }
}

IN_PROC_BROWSER_TEST_P(
    SharedStorageSelectURLLimitBrowserTest,
    SelectURL_IframesDifferentSite_SameEntropy_OverallLimitNotReached) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  // This test relies on the assumption that `kSelectURLOverallBitBudget` is set
  // to be strictly greater than `kSelectURLSiteBitBudget`, enough for at
  // least one 8-URL call to `selectURL()` beyond the per-site limit.
  EXPECT_GE(kSelectURLOverallBitBudget, kSelectURLSiteBitBudget + 3);

  // Here each call to `selectURL()` will have 8 input URLs, and hence
  // 3 = log2(8) bits of entropy.
  int per_site_call_limit = kSelectURLSiteBitBudget / 3;

  GURL iframe_url1 = https_server()->GetURL("b.test", kSimplePagePath);

  for (int i = 0; i < per_site_call_limit; i++) {
    // Create a new iframe.
    FrameTreeNode* iframe_node =
        CreateIFrame(PrimaryFrameTreeNodeRoot(), iframe_url1);

    RunSuccessfulSelectURLInIframe(iframe_node, /*num_urls=*/8,
                                   &console_observer);
  }

  // Create a new iframe.
  FrameTreeNode* penultimate_iframe_node =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), iframe_url1);

  if (LimitSelectURLCalls()) {
    EXPECT_TRUE(ExecJs(penultimate_iframe_node, R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

    // The limit for `selectURL()` has now been reached for "b.test". Make one
    // more call, which will return the default URL due to insufficient site
    // pageload budget.
    std::optional<std::pair<GURL, double>> result_pair =
        RunSelectURLExtractingMappedURLAndBudgetToCharge(
            penultimate_iframe_node, "b.test",
            /*num_urls=*/4);
    ASSERT_TRUE(result_pair.has_value());

    GURL expected_mapped_url =
        https_server()->GetURL("b.test", "/fenced_frames/title0.html");
    EXPECT_EQ(result_pair->first, expected_mapped_url);
    EXPECT_DOUBLE_EQ(result_pair->second, 0.0);

    EXPECT_EQ("Insufficient budget for selectURL().",
              base::UTF16ToUTF8(console_observer.messages().back().message));
  } else {
    // The `selectURL()` limit is disabled. The call will run successfully.
    RunSuccessfulSelectURLInIframe(penultimate_iframe_node,
                                   /*num_urls=*/4, &console_observer);
  }

  // Create a new iframe with a different site.
  GURL iframe_url2 = https_server()->GetURL("c.test", kSimplePagePath);
  FrameTreeNode* last_iframe_node =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), iframe_url2);

  // If enabled, the limit for `selectURL()` has now been reached for "b.test",
  // but not for "c.test". Make one more call, which will be successful
  // regardless of whether the limit is enabled.
  RunSuccessfulSelectURLInIframe(last_iframe_node, /*num_urls=*/8,
                                 &console_observer);

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram,
                     kSelectUrlBudgetStatusHistogram});
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     per_site_call_limit + 2);

  if (LimitSelectURLCalls()) {
    histogram_tester_.ExpectBucketCount(
        kSelectUrlBudgetStatusHistogram,
        blink::SharedStorageSelectUrlBudgetStatus::kSufficientBudget,
        per_site_call_limit + 1);
    histogram_tester_.ExpectBucketCount(
        kSelectUrlBudgetStatusHistogram,
        blink::SharedStorageSelectUrlBudgetStatus::
            kInsufficientSitePageloadBudget,
        1);
  } else {
    histogram_tester_.ExpectUniqueSample(
        kSelectUrlBudgetStatusHistogram,
        blink::SharedStorageSelectUrlBudgetStatus::kSufficientBudget,
        per_site_call_limit + 2);
  }
}

IN_PROC_BROWSER_TEST_P(
    SharedStorageSelectURLLimitBrowserTest,
    SelectURL_IframesDifferentSite_DifferentEntropy_OverallLimitReached) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  // This test relies on the assumptions that `kSelectURLOverallBitBudget` is
  // set to be strictly greater than `kSelectURLSiteBitBudget` and that the
  // latter is at least 3.
  EXPECT_GT(kSelectURLOverallBitBudget, kSelectURLSiteBitBudget);
  EXPECT_GE(kSelectURLSiteBitBudget, 3);

  int num_site_limit = kSelectURLOverallBitBudget / kSelectURLSiteBitBudget;

  // We will run out of chars if we have too many sites.
  EXPECT_LT(num_site_limit, 25);

  // For each site, the first call to `selectURL()` will have 8 input URLs,
  // and hence 3 = log2(8) bits of entropy, whereas the subsequent calls for
  // that site will have 2 input URLs, and hence 1 = log2(2) bit of entropy.
  int per_site_input2_call_limit = kSelectURLSiteBitBudget - 3;

  for (int i = 0; i < num_site_limit; i++) {
    std::string iframe_host = base::StrCat({std::string(1, 'b' + i), ".test"});
    GURL iframe_url = https_server()->GetURL(iframe_host, kSimplePagePath);

    // Create a new iframe.
    FrameTreeNode* first_loop_iframe_node =
        CreateIFrame(PrimaryFrameTreeNodeRoot(), iframe_url);

    RunSuccessfulSelectURLInIframe(first_loop_iframe_node,
                                   /*num_urls=*/8, &console_observer);

    for (int j = 0; j < per_site_input2_call_limit; j++) {
      // Create a new iframe.
      FrameTreeNode* loop_iframe_node =
          CreateIFrame(PrimaryFrameTreeNodeRoot(), iframe_url);

      RunSuccessfulSelectURLInIframe(loop_iframe_node,
                                     /*num_urls=*/2, &console_observer);
    }

    // Create a new iframe.
    FrameTreeNode* last_loop_iframe_node =
        CreateIFrame(PrimaryFrameTreeNodeRoot(), iframe_url);

    if (LimitSelectURLCalls()) {
      EXPECT_TRUE(ExecJs(last_loop_iframe_node, R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

      // The limit for `selectURL()` has now been reached for `iframe_host`.
      // Make one more call, which will return the default URL due to
      // insufficient site pageload budget.
      std::optional<std::pair<GURL, double>> result_pair =
          RunSelectURLExtractingMappedURLAndBudgetToCharge(
              last_loop_iframe_node, iframe_host,
              /*num_urls=*/2);
      ASSERT_TRUE(result_pair.has_value());

      GURL expected_mapped_url =
          https_server()->GetURL(iframe_host, "/fenced_frames/title0.html");
      EXPECT_EQ(result_pair->first, expected_mapped_url);
      EXPECT_DOUBLE_EQ(result_pair->second, 0.0);

      EXPECT_EQ("Insufficient budget for selectURL().",
                base::UTF16ToUTF8(console_observer.messages().back().message));

    } else {
      // The `selectURL()` limit is disabled. The call will run successfully.
      RunSuccessfulSelectURLInIframe(last_loop_iframe_node,
                                     /*num_urls=*/2, &console_observer);
    }
  }

  std::string iframe_host =
      base::StrCat({std::string(1, 'b' + num_site_limit), ".test"});
  GURL iframe_url = https_server()->GetURL(iframe_host, kSimplePagePath);

  // Create a new iframe.
  FrameTreeNode* final_iframe_node =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), iframe_url);

  if (LimitSelectURLCalls()) {
    EXPECT_TRUE(ExecJs(final_iframe_node, R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

    // The overall pageload limit for `selectURL()` has now been reached. Make
    // one more call, which will return the default URL due to insufficient
    // overall pageload budget.
    std::optional<std::pair<GURL, double>> result_pair =
        RunSelectURLExtractingMappedURLAndBudgetToCharge(final_iframe_node,
                                                         iframe_host,
                                                         /*num_urls=*/2);
    ASSERT_TRUE(result_pair.has_value());

    GURL expected_mapped_url =
        https_server()->GetURL(iframe_host, "/fenced_frames/title0.html");
    EXPECT_EQ(result_pair->first, expected_mapped_url);
    EXPECT_DOUBLE_EQ(result_pair->second, 0.0);

    EXPECT_EQ("Insufficient budget for selectURL().",
              base::UTF16ToUTF8(console_observer.messages().back().message));

  } else {
    // The `selectURL()` limit is disabled. The call will run successfully.
    RunSuccessfulSelectURLInIframe(final_iframe_node,
                                   /*num_urls=*/2, &console_observer);
  }

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram,
                     kSelectUrlBudgetStatusHistogram});
  histogram_tester_.ExpectTotalCount(
      kTimingSelectUrlExecutedInWorkletHistogram,
      num_site_limit * (2 + per_site_input2_call_limit) + 1);

  if (LimitSelectURLCalls()) {
    histogram_tester_.ExpectBucketCount(
        kSelectUrlBudgetStatusHistogram,
        blink::SharedStorageSelectUrlBudgetStatus::kSufficientBudget,
        num_site_limit * (1 + per_site_input2_call_limit));
    histogram_tester_.ExpectBucketCount(
        kSelectUrlBudgetStatusHistogram,
        blink::SharedStorageSelectUrlBudgetStatus::
            kInsufficientSitePageloadBudget,
        num_site_limit - 1);
    histogram_tester_.ExpectBucketCount(
        kSelectUrlBudgetStatusHistogram,
        blink::SharedStorageSelectUrlBudgetStatus::
            kInsufficientOverallPageloadBudget,
        2);
  } else {
    histogram_tester_.ExpectUniqueSample(
        kSelectUrlBudgetStatusHistogram,
        blink::SharedStorageSelectUrlBudgetStatus::kSufficientBudget,
        num_site_limit * (2 + per_site_input2_call_limit) + 1);
  }
}

class SharedStorageSelectURLSavedQueryBrowserTest
    : public SharedStorageSelectURLLimitBrowserTestBase {
 public:
  SharedStorageSelectURLSavedQueryBrowserTest() {
    select_url_limit_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{features::kSharedStorageSelectURLLimit,
          {{"SharedStorageSelectURLBitBudgetPerPageLoad",
            base::NumberToString(kSelectURLOverallBitBudget)},
           {"SharedStorageSelectURLBitBudgetPerSitePerPageLoad",
            base::NumberToString(kSelectURLSiteBitBudget)}}}},
        /*disabled_features=*/{});
    select_url_saved_query_feature_.InitAndEnableFeature(
        blink::features::kSharedStorageSelectURLSavedQueries);
    fenced_frame_api_change_feature_.InitWithFeatureState(
        blink::features::kFencedFramesAPIChanges, ResolveSelectURLToConfig());
  }

  bool ResolveSelectURLToConfig() override { return true; }

  // Precondition: `addModule('shared_storage/simple_module.js')` has been
  // called in the main frame.
  void RunSuccessfulSelectURLFromPreviouslySavedQueryInMainFrame(
      std::string host_str,
      int num_urls,
      WebContentsConsoleObserver* console_observer,
      const std::u16string& saved_query_name) {
    CHECK(!saved_query_name.empty());
    size_t num_previous_messages = console_observer->messages().size();

    std::optional<std::pair<GURL, double>> result_pair =
        RunSelectURLExtractingMappedURLAndBudgetToCharge(
            shell(), host_str, num_urls, saved_query_name);
    ASSERT_TRUE(result_pair.has_value());

    GURL expected_mapped_url = https_server()->GetURL(
        host_str, base::StrCat({"/fenced_frames/title",
                                base::NumberToString(num_urls - 1), ".html"}));
    EXPECT_EQ(result_pair->first, expected_mapped_url);
    EXPECT_DOUBLE_EQ(result_pair->second, std::log2(num_urls));

    // There should be no new console messages, since the saved index was
    // retrieved instead of running a worklet operation.
    int num_new_messages =
        console_observer->messages().size() - num_previous_messages;
    EXPECT_EQ(num_new_messages, 0);
  }

  // Precondition: `addModule('shared_storage/simple_module.js')` has NOT been
  // called in `iframe_node`.
  void RunSuccessfulSelectURLFromPreviouslySavedQueryInIframe(
      FrameTreeNode* iframe_node,
      int num_urls,
      WebContentsConsoleObserver* console_observer,
      const std::u16string& saved_query_name) {
    CHECK(!saved_query_name.empty());
    size_t num_previous_messages = console_observer->messages().size();

    std::string host_str =
        iframe_node->current_frame_host()->GetLastCommittedURL().host();
    EXPECT_TRUE(ExecJs(iframe_node, R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

    std::optional<std::pair<GURL, double>> result_pair =
        RunSelectURLExtractingMappedURLAndBudgetToCharge(
            iframe_node, host_str, num_urls, saved_query_name);
    ASSERT_TRUE(result_pair.has_value());

    GURL expected_mapped_url = https_server()->GetURL(
        host_str, base::StrCat({"/fenced_frames/title",
                                base::NumberToString(num_urls - 1), ".html"}));
    EXPECT_EQ(result_pair->first, expected_mapped_url);
    EXPECT_DOUBLE_EQ(result_pair->second, std::log2(num_urls));

    // `addModule` should generate 2 new console nessages. There should be no
    // new console messages from `selectURL`, since the saved index was
    // retrieved instead of running a worklet operation.
    int num_new_messages =
        console_observer->messages().size() - num_previous_messages;
    EXPECT_EQ(num_new_messages, 2);
  }

  std::vector<SharedStorageUrlSpecWithMetadata> GetExpectedUrlsWithMetadata(
      const std::string& host,
      size_t num_urls) {
    std::vector<SharedStorageUrlSpecWithMetadata> expected_urls_with_metadata;
    for (size_t i = 0; i < num_urls; ++i) {
      expected_urls_with_metadata.push_back(
          {https_server()->GetURL(
               host, base::StrCat({"/fenced_frames/title",
                                   base::NumberToString(i), ".html"})),
           {{"click", https_server()
                          ->GetURL(host, base::StrCat({"/fenced_frames/report",
                                                       base::NumberToString(i),
                                                       ".html"}))
                          .spec()},
            {"mouse interaction",
             https_server()
                 ->GetURL(host,
                          base::StrCat({"/fenced_frames/report",
                                        base::NumberToString(i + 1), ".html"}))
                 .spec()}}});
    }
    return expected_urls_with_metadata;
  }

 private:
  base::test::ScopedFeatureList select_url_limit_feature_list_;
  base::test::ScopedFeatureList select_url_saved_query_feature_;
  base::test::ScopedFeatureList fenced_frame_api_change_feature_;
};

IN_PROC_BROWSER_TEST_F(SharedStorageSelectURLSavedQueryBrowserTest,
                       SelectURL_MainFrame_SiteLimitReached_ReuseSavedQueries) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  // This test relies on the assumption that `kSelectURLOverallBitBudget` is set
  // to be greater than or equal to `kSelectURLSiteBitBudget`.
  EXPECT_GE(kSelectURLOverallBitBudget, kSelectURLSiteBitBudget);

  // Here each call to `selectURL()` will have 8 input URLs, and hence
  // 3 = log2(8) bits of entropy.
  int call_limit = kSelectURLSiteBitBudget / 3;

  for (int call = 0; call < call_limit; call++) {
    RunSuccessfulSelectURLInMainFrame("a.test", /*num_urls=*/8,
                                      &console_observer,
                                      u"query" + base::NumberToString16(call));
  }

  // The limit for `selectURL()` has now been reached for "a.test". Make one
  // more call without using the previously saved queries. This will return the
  // default URL due to insufficient site pageload budget.
  std::optional<std::pair<GURL, double>> result_pair =
      RunSelectURLExtractingMappedURLAndBudgetToCharge(shell(), "a.test",
                                                       /*num_urls=*/8);
  ASSERT_TRUE(result_pair.has_value());

  GURL expected_mapped_url =
      https_server()->GetURL("a.test", "/fenced_frames/title0.html");
  EXPECT_EQ(result_pair->first, expected_mapped_url);
  EXPECT_DOUBLE_EQ(result_pair->second, 0.0);

  EXPECT_EQ("Insufficient budget for selectURL().",
            base::UTF16ToUTF8(console_observer.messages().back().message));

  // Making calls using previously saved queries will succeed despite the lack
  // of budget.
  for (int call = 0; call < call_limit; call++) {
    RunSuccessfulSelectURLFromPreviouslySavedQueryInMainFrame(
        "a.test", /*num_urls=*/8, &console_observer,
        u"query" + base::NumberToString16(call));
  }

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram,
                     kSelectUrlBudgetStatusHistogram});

  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     2 * call_limit + 1);
  histogram_tester_.ExpectBucketCount(
      kSelectUrlBudgetStatusHistogram,
      blink::SharedStorageSelectUrlBudgetStatus::kSufficientBudget,
      2 * call_limit);
  histogram_tester_.ExpectBucketCount(
      kSelectUrlBudgetStatusHistogram,
      blink::SharedStorageSelectUrlBudgetStatus::
          kInsufficientSitePageloadBudget,
      1);

  std::string origin_str = url::Origin::Create(main_url).Serialize();

  std::vector<SharedStorageUrlSpecWithMetadata> expected_urls_with_metadata =
      GetExpectedUrlsWithMetadata("a.test", /*num_urls=*/8);

  ASSERT_EQ(static_cast<int>(urn_uuids_observed().size()), 2 * call_limit + 1);

  std::vector<Access> expected_accesses;
  expected_accesses.push_back(
      {AccessScope::kWindow, AccessMethod::kAddModule, MainFrameId(),
       origin_str,
       SharedStorageEventParams::CreateForAddModule(
           https_server()->GetURL("a.test", "/shared_storage/simple_module.js"),
           /*worklet_ordinal_id=*/0, GetFirstWorkletHostDevToolsToken())});

  std::vector<OperationFinishedInfo> expected_finished_infos;
  for (int call = 0; call < call_limit; call++) {
    expected_accesses.push_back(
        {AccessScope::kWindow, AccessMethod::kSelectURL, MainFrameId(),
         origin_str,
         SharedStorageEventParams::CreateForSelectURLForTesting(
             "test-url-selection-operation", /*operation_id=*/call,
             /*keep_alive=*/true,
             SharedStorageEventParams::PrivateAggregationConfigWrapper(),
             blink::CloneableMessage(), expected_urls_with_metadata,
             ResolveSelectURLToConfig(),
             /*saved_query=*/
             base::StrCat({"query", base::NumberToString(call)}),
             urn_uuids_observed()[call], /*worklet_ordinal_id=*/0,
             GetFirstWorkletHostDevToolsToken())});
    expected_finished_infos.push_back(
        {base::TimeDelta(), AccessMethod::kSelectURL, /*operation_id=*/call,
         /*worklet_ordinal_id=*/0, GetFirstWorkletHostDevToolsToken(),
         MainFrameId(), origin_str});
  }
  expected_accesses.push_back(
      {AccessScope::kWindow, AccessMethod::kSelectURL, MainFrameId(),
       origin_str,
       SharedStorageEventParams::CreateForSelectURLForTesting(
           "test-url-selection-operation", /*operation_id=*/call_limit,
           /*keep_alive=*/true,
           SharedStorageEventParams::PrivateAggregationConfigWrapper(),
           blink::CloneableMessage(), expected_urls_with_metadata,
           ResolveSelectURLToConfig(),
           /*saved_query=*/std::string(), urn_uuids_observed()[call_limit],
           /*worklet_ordinal_id=*/0, GetFirstWorkletHostDevToolsToken())});
  expected_finished_infos.push_back(
      {base::TimeDelta(), AccessMethod::kSelectURL, /*operation_id=*/call_limit,
       /*worklet_ordinal_id=*/0, GetFirstWorkletHostDevToolsToken(),
       MainFrameId(), origin_str});
  for (int call = 0; call < call_limit; call++) {
    expected_accesses.push_back(
        {AccessScope::kWindow, AccessMethod::kSelectURL, MainFrameId(),
         origin_str,
         SharedStorageEventParams::CreateForSelectURLForTesting(
             "test-url-selection-operation",
             /*operation_id=*/call_limit + 1 + call,
             /*keep_alive=*/true,
             SharedStorageEventParams::PrivateAggregationConfigWrapper(),
             blink::CloneableMessage(), expected_urls_with_metadata,
             ResolveSelectURLToConfig(),
             /*saved_query=*/
             base::StrCat({"query", base::NumberToString(call)}),
             urn_uuids_observed()[call_limit + 1 + call],
             /*worklet_ordinal_id=*/0, GetFirstWorkletHostDevToolsToken())});
    expected_finished_infos.push_back(
        {base::TimeDelta(), AccessMethod::kSelectURL,
         /*operation_id=*/call_limit + 1 + call, /*worklet_ordinal_id=*/0,
         GetFirstWorkletHostDevToolsToken(), MainFrameId(), origin_str});
  }

  ExpectAccessObserved(expected_accesses);
  ExpectOperationFinishedInfosObserved(expected_finished_infos);
}

IN_PROC_BROWSER_TEST_F(
    SharedStorageSelectURLSavedQueryBrowserTest,
    SelectURL_CrossOriginIframesSharingCommonSite_SiteLimitReached_ReuseSavedQueries) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  // This test relies on the assumption that `kSelectURLOverallBitBudget` is set
  // to be greater than or equal to `kSelectURLSiteBitBudget`.
  EXPECT_GE(kSelectURLOverallBitBudget, kSelectURLSiteBitBudget);

  // Here each call to `selectURL()` will have 8 input URLs, and hence
  // 3 = log2(8) bits of entropy.
  int call_limit = kSelectURLSiteBitBudget / 3;

  GURL iframe_url;

  for (int call = 0; call < call_limit; call++) {
    std::string iframe_host =
        base::StrCat({"subdomain", base::NumberToString(call), ".b.test"});

    iframe_url = https_server()->GetURL(iframe_host, kSimplePagePath);

    // Create a new iframe.
    FrameTreeNode* iframe_node =
        CreateIFrame(PrimaryFrameTreeNodeRoot(), iframe_url);

    RunSuccessfulSelectURLInIframe(iframe_node, /*num_urls=*/8,
                                   &console_observer,
                                   u"query" + base::NumberToString16(call));
  }

  iframe_url = https_server()->GetURL("b.test", kSimplePagePath);

  // Create a new iframe.
  FrameTreeNode* iframe_node =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), iframe_url);

  EXPECT_TRUE(ExecJs(iframe_node, R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  // The limit for `selectURL()` has now been reached for "b.test". Make one
  // more call, which will return the default URL due to insufficient site
  // pageload budget.
  std::optional<std::pair<GURL, double>> result_pair =
      RunSelectURLExtractingMappedURLAndBudgetToCharge(iframe_node, "b.test",
                                                       /*num_urls=*/8);
  ASSERT_TRUE(result_pair.has_value());

  GURL expected_mapped_url =
      https_server()->GetURL("b.test", "/fenced_frames/title0.html");
  EXPECT_EQ(result_pair->first, expected_mapped_url);
  EXPECT_DOUBLE_EQ(result_pair->second, 0.0);

  EXPECT_EQ("Insufficient budget for selectURL().",
            base::UTF16ToUTF8(console_observer.messages().back().message));

  for (int call = 0; call < call_limit; call++) {
    std::string iframe_host =
        base::StrCat({"subdomain", base::NumberToString(call), ".b.test"});

    iframe_url = https_server()->GetURL(iframe_host, kSimplePagePath);

    // Create a new iframe.
    iframe_node = CreateIFrame(PrimaryFrameTreeNodeRoot(), iframe_url);

    RunSuccessfulSelectURLFromPreviouslySavedQueryInIframe(
        iframe_node, /*num_urls=*/8, &console_observer,
        u"query" + base::NumberToString16(call));
  }

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram,
                     kSelectUrlBudgetStatusHistogram});
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     2 * call_limit + 1);
  histogram_tester_.ExpectBucketCount(
      kSelectUrlBudgetStatusHistogram,
      blink::SharedStorageSelectUrlBudgetStatus::kSufficientBudget,
      2 * call_limit);
  histogram_tester_.ExpectBucketCount(
      kSelectUrlBudgetStatusHistogram,
      blink::SharedStorageSelectUrlBudgetStatus::
          kInsufficientSitePageloadBudget,
      1);

  ASSERT_EQ(static_cast<int>(urn_uuids_observed().size()), 2 * call_limit + 1);

  std::map<int, base::UnguessableToken>& cached_worklet_devtools_tokens =
      GetCachedWorkletHostDevToolsTokens();
  ASSERT_EQ(static_cast<int>(cached_worklet_devtools_tokens.size()),
            2 * call_limit + 1);

  std::vector<Access> expected_accesses;
  std::vector<OperationFinishedInfo> expected_finished_infos;
  std::string host;
  std::string origin_str;
  for (int call = 0; call < call_limit; call++) {
    host = base::StrCat({"subdomain", base::NumberToString(call), ".b.test"});
    origin_str = https_server()->GetOrigin(host).Serialize();
    expected_accesses.push_back(
        {AccessScope::kWindow, AccessMethod::kAddModule, MainFrameId(),
         origin_str,
         SharedStorageEventParams::CreateForAddModule(
             https_server()->GetURL(host, "/shared_storage/simple_module.js"),
             /*worklet_ordinal_id=*/call,
             cached_worklet_devtools_tokens[call])});
    expected_accesses.push_back(
        {AccessScope::kWindow, AccessMethod::kSelectURL, MainFrameId(),
         origin_str,
         SharedStorageEventParams::CreateForSelectURLForTesting(
             "test-url-selection-operation", /*operation_id=*/0,
             /*keep_alive=*/true,
             SharedStorageEventParams::PrivateAggregationConfigWrapper(),
             blink::CloneableMessage(),
             GetExpectedUrlsWithMetadata(host, /*num_urls=*/8),
             ResolveSelectURLToConfig(),
             /*saved_query=*/
             base::StrCat({"query", base::NumberToString(call)}),
             urn_uuids_observed()[call], /*worklet_ordinal_id=*/call,
             cached_worklet_devtools_tokens[call])});
    expected_finished_infos.push_back(
        {base::TimeDelta(), AccessMethod::kSelectURL, /*operation_id=*/0,
         /*worklet_ordinal_id=*/call, cached_worklet_devtools_tokens[call],
         MainFrameId(), origin_str});
  }
  origin_str = https_server()->GetOrigin("b.test").Serialize();
  expected_accesses.push_back(
      {AccessScope::kWindow, AccessMethod::kAddModule, MainFrameId(),
       origin_str,
       SharedStorageEventParams::CreateForAddModule(
           https_server()->GetURL("b.test", "/shared_storage/simple_module.js"),
           /*worklet_ordinal_id=*/call_limit,
           cached_worklet_devtools_tokens[call_limit])});
  expected_accesses.push_back(
      {AccessScope::kWindow, AccessMethod::kSelectURL, MainFrameId(),
       origin_str,
       SharedStorageEventParams::CreateForSelectURLForTesting(
           "test-url-selection-operation", /*operation_id=*/0,
           /*keep_alive=*/true,
           SharedStorageEventParams::PrivateAggregationConfigWrapper(),
           blink::CloneableMessage(),
           GetExpectedUrlsWithMetadata("b.test", /*num_urls=*/8),
           ResolveSelectURLToConfig(),
           /*saved_query=*/std::string(), urn_uuids_observed()[call_limit],
           /*worklet_ordinal_id=*/call_limit,
           cached_worklet_devtools_tokens[call_limit])});
  expected_finished_infos.push_back(
      {base::TimeDelta(), AccessMethod::kSelectURL, /*operation_id=*/0,
       /*worklet_ordinal_id=*/call_limit,
       cached_worklet_devtools_tokens[call_limit], MainFrameId(), origin_str});
  for (int call = 0; call < call_limit; call++) {
    host = base::StrCat({"subdomain", base::NumberToString(call), ".b.test"});
    origin_str = https_server()->GetOrigin(host).Serialize();
    expected_accesses.push_back(
        {AccessScope::kWindow, AccessMethod::kAddModule, MainFrameId(),
         origin_str,
         SharedStorageEventParams::CreateForAddModule(
             https_server()->GetURL(host, "/shared_storage/simple_module.js"),
             /*worklet_ordinal_id=*/call_limit + 1 + call,
             cached_worklet_devtools_tokens[call_limit + 1 + call])});
    expected_accesses.push_back(
        {AccessScope::kWindow, AccessMethod::kSelectURL, MainFrameId(),
         origin_str,
         SharedStorageEventParams::CreateForSelectURLForTesting(
             "test-url-selection-operation", /*operation_id=*/0,
             /*keep_alive=*/true,
             SharedStorageEventParams::PrivateAggregationConfigWrapper(),
             blink::CloneableMessage(),
             GetExpectedUrlsWithMetadata(host, /*num_urls=*/8),
             ResolveSelectURLToConfig(),
             /*saved_query=*/
             base::StrCat({"query", base::NumberToString(call)}),
             urn_uuids_observed()[call_limit + 1 + call],
             /*worklet_ordinal_id=*/call_limit + 1 + call,
             cached_worklet_devtools_tokens[call_limit + 1 + call])});
    expected_finished_infos.push_back(
        {base::TimeDelta(), AccessMethod::kSelectURL, /*operation_id=*/0,
         /*worklet_ordinal_id=*/call_limit + 1 + call,
         cached_worklet_devtools_tokens[call_limit + 1 + call], MainFrameId(),
         origin_str});
  }

  ExpectAccessObserved(expected_accesses);
}

class SharedStorageContextBrowserTest
    : public SharedStorageFencedFrameInteractionBrowserTestBase {
 public:
  SharedStorageContextBrowserTest() {
    fenced_frame_api_change_feature_.InitAndEnableFeature(
        blink::features::kFencedFramesAPIChanges);
  }

  ~SharedStorageContextBrowserTest() override = default;

  void GenerateFencedFrameConfig(std::string hostname,
                                 bool keep_alive_after_operation = true) {
    EXPECT_TRUE(ExecJs(shell(), JsReplace("window.keepWorklet = $1;",
                                          keep_alive_after_operation)));
    EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

    TestSelectURLFencedFrameConfigObserver config_observer(
        GetStoragePartition());
    GURL fenced_frame_url = https_server()->GetURL(hostname, kFencedFramePath);

    // There is 1 more "worklet operation": `selectURL()`.
    test_runtime_manager()
        .GetAttachedWorkletHost()
        ->SetExpectedWorkletResponsesCount(1);

    EvalJsResult result = EvalJs(shell(), JsReplace(R"(
      (async function() {
        window.fencedFrameConfig = await sharedStorage.selectURL(
          'test-url-selection-operation',
          [
            {
              url: $1,
            }
          ],
          {
            data: {'mockResult': 0},
            resolveToConfig: true,
            keepAlive: keepWorklet
          }
        );
        if (!(fencedFrameConfig instanceof FencedFrameConfig)) {
          throw new Error('selectURL() did not return a FencedFrameConfig.');
        }
        return window.fencedFrameConfig;
      })()
    )",
                                                    fenced_frame_url.spec()));

    EXPECT_TRUE(result.error.empty());
    const std::optional<GURL>& observed_urn_uuid = config_observer.GetUrnUuid();
    ASSERT_TRUE(observed_urn_uuid.has_value());
    EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid.value()));

    test_runtime_manager().GetAttachedWorkletHost()->WaitForWorkletResponses();

    ASSERT_TRUE(config_observer.ConfigObserved());
    const std::optional<FencedFrameConfig>& fenced_frame_config =
        config_observer.GetConfig();
    EXPECT_TRUE(fenced_frame_config.has_value());
    EXPECT_EQ(fenced_frame_config->urn_uuid(), observed_urn_uuid.value());
  }

 private:
  base::test::ScopedFeatureList fenced_frame_api_change_feature_;
};

// Tests that `blink::FencedFrameConfig::context` can be set and then accessed
// via `sharedStorage.context`. The context must be set prior to fenced frame
// navigation to the config.
IN_PROC_BROWSER_TEST_F(SharedStorageContextBrowserTest,
                       EmbedderContextSetBeforeNavigation_Defined) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  // Generate a config using `selectURL()`.
  GenerateFencedFrameConfig("b.test");

  EXPECT_EQ("Finish executing 'test-url-selection-operation'",
            base::UTF16ToUTF8(console_observer.messages().back().message));

  // Set the context in the config.
  const std::string kEmbedderContext = "some context";
  EXPECT_TRUE(
      ExecJs(shell(),
             JsReplace("window.fencedFrameConfig.setSharedStorageContext($1);",
                       kEmbedderContext)));

  // Create and navigate a fenced frame to the config.
  FrameTreeNode* fenced_frame_root_node =
      CreateFencedFrame(FencedFrameNavigationTarget("fencedFrameConfig"));
  ASSERT_TRUE(fenced_frame_root_node);

  // Try to retrieve the context from the root fenced frame's worklet.
  GURL script_url;
  ExecuteScriptInWorklet(fenced_frame_root_node, R"(
    console.log(sharedStorage.context);
  )",
                         &script_url, /*expected_total_host_count=*/2u);

  // The root fenced frame will have access to the context.
  EXPECT_EQ(kEmbedderContext,
            base::UTF16ToUTF8(console_observer.messages().back().message));

  // Create and navigate to a nested iframe that is same-origin to the root
  // fenced frame.
  GURL same_origin_url = https_server()->GetURL("b.test", kFencedFramePath);
  FrameTreeNode* same_origin_nested_iframe =
      CreateIFrame(fenced_frame_root_node, same_origin_url);
  ASSERT_TRUE(same_origin_nested_iframe);

  // Try to retrieve the context from the nested same-origin iframe's worklet.
  ExecuteScriptInWorklet(same_origin_nested_iframe, R"(
    console.log(sharedStorage.context);
  )",
                         &script_url, /*expected_total_host_count=*/3u);

  // A same-origin child of the fenced frame will have access to the context.
  EXPECT_EQ(kEmbedderContext,
            base::UTF16ToUTF8(console_observer.messages().back().message));

  // Create and navigate to a nested iframe that is cross-origin to the root
  // fenced frame.
  GURL cross_origin_url = https_server()->GetURL("c.test", kFencedFramePath);
  FrameTreeNode* cross_origin_nested_iframe =
      CreateIFrame(fenced_frame_root_node, cross_origin_url);
  ASSERT_TRUE(cross_origin_nested_iframe);

  // Try to retrieve the context from the nested cross-origin iframe's worklet.
  ExecuteScriptInWorklet(cross_origin_nested_iframe, R"(
    console.log(sharedStorage.context);
  )",
                         &script_url, /*expected_total_host_count=*/4u);

  // A cross-origin child of the fenced frame will not have access to the
  // context.
  EXPECT_EQ("undefined",
            base::UTF16ToUTF8(console_observer.messages().back().message));
}

// Tests that `blink::FencedFrameConfig::context`, when not set and then
// accessed via `sharedStorage.context`, will be undefined.
IN_PROC_BROWSER_TEST_F(SharedStorageContextBrowserTest,
                       EmbedderContextSetAfterNavigation_Undefined) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  // Generate a config using `selectURL()`.
  GenerateFencedFrameConfig("b.test");

  EXPECT_EQ("Finish executing 'test-url-selection-operation'",
            base::UTF16ToUTF8(console_observer.messages().back().message));

  // Create and navigate a fenced frame to the config.
  FrameTreeNode* fenced_frame_root_node =
      CreateFencedFrame(FencedFrameNavigationTarget("fencedFrameConfig"));
  ASSERT_TRUE(fenced_frame_root_node);

  // Set the context in the config. Since the fenced frame has already been
  // navigated to the config and we are not going to navigate to it again, this
  // context will not be propagated to the browser process and so won't be
  // accessible via `sharedStorage.context`.
  const std::string kEmbedderContext = "some context";
  EXPECT_TRUE(
      ExecJs(shell(),
             JsReplace("window.fencedFrameConfig.setSharedStorageContext($1);",
                       kEmbedderContext)));

  // Try to retrieve the context from the root fenced frame's worklet.
  GURL script_url;
  ExecuteScriptInWorklet(fenced_frame_root_node, R"(
    console.log(sharedStorage.context);
  )",
                         &script_url, /*expected_total_host_count=*/2u);

  // The root fenced frame will see that the context is undefined because it was
  // not set before fenced frame navigation.
  EXPECT_EQ("undefined",
            base::UTF16ToUTF8(console_observer.messages().back().message));
}

// Tests that `blink::FencedFrameConfig::context`, when set after a first
// navigation to the config and before a second fenced frame navigation to the
// same config, is updated, as seen via `sharedStorage.context`.
IN_PROC_BROWSER_TEST_F(SharedStorageContextBrowserTest,
                       EmbedderContextNavigateTwice_ContextUpdated) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  // Generate a config using `selectURL()`.
  GenerateFencedFrameConfig("b.test");

  EXPECT_EQ("Finish executing 'test-url-selection-operation'",
            base::UTF16ToUTF8(console_observer.messages().back().message));

  // Set the context in the config.
  const std::string kEmbedderContext = "some context";
  EXPECT_TRUE(
      ExecJs(shell(),
             JsReplace("window.fencedFrameConfig.setSharedStorageContext($1);",
                       kEmbedderContext)));

  // Create and navigate a fenced frame to the config.
  FrameTreeNode* fenced_frame_root_node =
      CreateFencedFrame(FencedFrameNavigationTarget("fencedFrameConfig"));
  ASSERT_TRUE(fenced_frame_root_node);

  // Try to retrieve the context from the root fenced frame's worklet.
  GURL script_url;
  ExecuteScriptInWorklet(fenced_frame_root_node, R"(
    console.log(sharedStorage.context);
  )",
                         &script_url, /*expected_total_host_count=*/2u);

  // The root fenced frame will have access to the context.
  EXPECT_EQ(kEmbedderContext,
            base::UTF16ToUTF8(console_observer.messages().back().message));

  // Set the context in the config.
  const std::string kNewEmbedderContext = "some different context";
  EXPECT_TRUE(
      ExecJs(shell(),
             JsReplace("window.fencedFrameConfig.setSharedStorageContext($1);",
                       kNewEmbedderContext)));

  // Navigate the fenced frame again to the updated config.
  NavigateExistingFencedFrame(fenced_frame_root_node,
                              FencedFrameNavigationTarget("fencedFrameConfig"));

  // Try to retrieve the context from the root fenced frame's worklet.
  ExecuteScriptInWorklet(fenced_frame_root_node, R"(
    console.log(sharedStorage.context);
  )",
                         &script_url, /*expected_total_host_count=*/2u);

  // The root fenced frame will have access to the updated context.
  EXPECT_EQ(kNewEmbedderContext,
            base::UTF16ToUTF8(console_observer.messages().back().message));
}

// Tests that `blink::FencedFrameConfig::context` can be set and then accessed
// via `sharedStorage.context`, but that any context string exceeding the length
// limit is truncated.
IN_PROC_BROWSER_TEST_F(SharedStorageContextBrowserTest,
                       EmbedderContextExceedsLengthLimit_Truncated) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  // Generate a config using `selectURL()`.
  GenerateFencedFrameConfig("b.test");

  EXPECT_EQ("Finish executing 'test-url-selection-operation'",
            base::UTF16ToUTF8(console_observer.messages().back().message));

  // Set the context in the config.
  const std::string kLongEmbedderContext(
      blink::kFencedFrameConfigSharedStorageContextMaxLength, 'x');
  EXPECT_TRUE(
      ExecJs(shell(),
             JsReplace("window.fencedFrameConfig.setSharedStorageContext($1);",
                       kLongEmbedderContext + 'X')));

  // Create and navigate a fenced frame to the config.
  FrameTreeNode* fenced_frame_root_node =
      CreateFencedFrame(FencedFrameNavigationTarget("fencedFrameConfig"));
  ASSERT_TRUE(fenced_frame_root_node);

  // Try to retrieve the context from the root fenced frame's worklet.
  GURL script_url;
  ExecuteScriptInWorklet(fenced_frame_root_node, R"(
    console.log(sharedStorage.context);
  )",
                         &script_url, /*expected_total_host_count=*/2u);

  // The root fenced frame will have access to the context, which will be
  // truncated.
  EXPECT_EQ(kLongEmbedderContext,
            base::UTF16ToUTF8(console_observer.messages().back().message));
}

}  // namespace content
