// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/devtools/simple_devtools_protocol_client/simple_devtools_protocol_client.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "headless/lib/browser/headless_web_contents_impl.h"
#include "headless/public/switches.h"
#include "headless/test/headless_browser_test.h"
#include "headless/test/headless_browser_test_utils.h"
#include "headless/test/headless_devtooled_browsertest.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/chrome_debug_urls.h"
#include "third_party/inspector_protocol/crdtp/dispatch.h"
#include "url/gurl.h"

#define EXPECT_SIZE_EQ(expected, actual)               \
  do {                                                 \
    EXPECT_EQ((expected).width(), (actual).width());   \
    EXPECT_EQ((expected).height(), (actual).height()); \
  } while (false)

using simple_devtools_protocol_client::SimpleDevToolsProtocolClient;

using testing::ElementsAre;
using testing::NotNull;
using testing::UnorderedElementsAre;

namespace headless {

class HeadlessDevToolsClientNavigationTest
    : public HeadlessDevTooledBrowserTest {
 public:
  void RunDevTooledTest() override {
    ASSERT_TRUE(embedded_test_server()->Start());

    devtools_client_.AddEventHandler(
        "Page.loadEventFired",
        base::BindRepeating(
            &HeadlessDevToolsClientNavigationTest::OnLoadEventFired,
            base::Unretained(this)));
    SendCommandSync(devtools_client_, "Page.enable");

    devtools_client_.SendCommand(
        "Page.navigate",
        Param("url", embedded_test_server()->GetURL("/hello.html").spec()));
  }

  void OnLoadEventFired(const base::Value::Dict& params) {
    devtools_client_.SendCommand("Page.disable");
    FinishAsynchronousTest();
  }
};

HEADLESS_DEVTOOLED_TEST_F(HeadlessDevToolsClientNavigationTest);

class HeadlessDevToolsClientWindowManagementTest
    : public HeadlessDevTooledBrowserTest {
 public:
  int window_id() {
    return HeadlessWebContentsImpl::From(web_contents_)->window_id();
  }

  void SetWindowBounds(
      const gfx::Rect& rect,
      SimpleDevToolsProtocolClient::ResponseCallback callback) {
    base::Value::Dict params;
    params.Set("windowId", window_id());
    params.SetByDottedPath("bounds.left", rect.x());
    params.SetByDottedPath("bounds.top", rect.y());
    params.SetByDottedPath("bounds.width", rect.width());
    params.SetByDottedPath("bounds.height", rect.height());
    params.SetByDottedPath("bounds.windowState", "normal");

    browser_devtools_client_.SendCommand(
        "Browser.setWindowBounds", std::move(params), std::move(callback));
  }

  void SetWindowState(const std::string& window_state,
                      SimpleDevToolsProtocolClient::ResponseCallback callback) {
    base::Value::Dict params;
    params.Set("windowId", window_id());
    params.SetByDottedPath("bounds.windowState", window_state);

    browser_devtools_client_.SendCommand(
        "Browser.setWindowBounds", std::move(params), std::move(callback));
  }

  void GetWindowBounds(
      SimpleDevToolsProtocolClient::ResponseCallback callback) {
    browser_devtools_client_.SendCommand("Browser.getWindowBounds",
                                         Param("windowId", window_id()),
                                         std::move(callback));
  }

  void CheckWindowBounds(const gfx::Rect& bounds,
                         const std::string window_state,
                         base::Value::Dict result) {
    gfx::Rect actual_bounds(DictInt(result, "result.bounds.left"),
                            DictInt(result, "result.bounds.top"),
                            DictInt(result, "result.bounds.width"),
                            DictInt(result, "result.bounds.height"));

    std::string actual_window_state =
        DictString(result, "result.bounds.windowState");

    // Mac does not support repositioning, as we don't show any actual window.
#if !BUILDFLAG(IS_MAC)
    EXPECT_EQ(bounds.x(), actual_bounds.x());
    EXPECT_EQ(bounds.y(), actual_bounds.y());
#endif  // !BUILDFLAG(IS_MAC)
    EXPECT_EQ(bounds.width(), actual_bounds.width());
    EXPECT_EQ(bounds.height(), actual_bounds.height());
    EXPECT_EQ(window_state, actual_window_state);
  }
};

class HeadlessDevToolsClientChangeWindowBoundsTest
    : public HeadlessDevToolsClientWindowManagementTest {
  gfx::Rect new_bounds() { return gfx::Rect(100, 200, 300, 400); }

  void RunDevTooledTest() override {
    SetWindowBounds(
        new_bounds(),
        base::BindOnce(
            &HeadlessDevToolsClientChangeWindowBoundsTest::OnSetWindowBounds,
            base::Unretained(this)));
  }

  void OnSetWindowBounds(base::Value::Dict result) {
    GetWindowBounds(base::BindOnce(
        &HeadlessDevToolsClientChangeWindowBoundsTest::OnGetWindowBounds,
        base::Unretained(this)));
  }

  void OnGetWindowBounds(base::Value::Dict result) {
    CheckWindowBounds(new_bounds(), "normal", std::move(result));
    FinishAsynchronousTest();
  }
};

HEADLESS_DEVTOOLED_TEST_F(HeadlessDevToolsClientChangeWindowBoundsTest);

class HeadlessDevToolsClientOuterSizeTest
    : public HeadlessDevToolsClientWindowManagementTest {
  void RunDevTooledTest() override {
    SetWindowBounds(
        gfx::Rect(100, 200, 800, 600),
        base::BindOnce(&HeadlessDevToolsClientOuterSizeTest::OnSetWindowBounds,
                       base::Unretained(this)));
  }

  void OnSetWindowBounds(base::Value::Dict) {
    EXPECT_EQ(800, Evaluate("window.outerWidth"));
    EXPECT_EQ(600, Evaluate("window.outerHeight"));

    FinishAsynchronousTest();
  }

  int Evaluate(const std::string& expression) {
    base::Value::Dict result = SendCommandSync(
        devtools_client_, "Runtime.evaluate", Param("expression", expression));
    return DictInt(result, "result.result.value");
  }
};

HEADLESS_DEVTOOLED_TEST_F(HeadlessDevToolsClientOuterSizeTest);

class HeadlessDevToolsClientChangeWindowStateTest
    : public HeadlessDevToolsClientWindowManagementTest {
 public:
  explicit HeadlessDevToolsClientChangeWindowStateTest(
      const std::string& window_state)
      : window_state_(window_state) {}

  void RunDevTooledTest() override {
    SetWindowState(
        window_state_,
        base::BindOnce(
            &HeadlessDevToolsClientChangeWindowStateTest::OnSetWindowState,
            base::Unretained(this)));
  }

  void OnSetWindowState(base::Value::Dict) {
    GetWindowBounds(base::BindOnce(
        &HeadlessDevToolsClientChangeWindowStateTest::OnGetWindowState,
        base::Unretained(this)));
  }

  void OnGetWindowState(base::Value::Dict result) {
    HeadlessBrowser::Options::Builder builder;
    const HeadlessBrowser::Options kDefaultOptions = builder.Build();
    CheckWindowBounds(gfx::Rect(kDefaultOptions.window_size), window_state_,
                      std::move(result));
    FinishAsynchronousTest();
  }

 protected:
  std::string window_state_;
};

class HeadlessDevToolsClientMinimizeWindowTest
    : public HeadlessDevToolsClientChangeWindowStateTest {
 public:
  HeadlessDevToolsClientMinimizeWindowTest()
      : HeadlessDevToolsClientChangeWindowStateTest("minimized") {}
};

HEADLESS_DEVTOOLED_TEST_F(HeadlessDevToolsClientMinimizeWindowTest);

class HeadlessDevToolsClientMaximizeWindowTest
    : public HeadlessDevToolsClientChangeWindowStateTest {
 public:
  HeadlessDevToolsClientMaximizeWindowTest()
      : HeadlessDevToolsClientChangeWindowStateTest("maximized") {}
};

HEADLESS_DEVTOOLED_TEST_F(HeadlessDevToolsClientMaximizeWindowTest);

class HeadlessDevToolsClientFullscreenWindowTest
    : public HeadlessDevToolsClientChangeWindowStateTest {
 public:
  HeadlessDevToolsClientFullscreenWindowTest()
      : HeadlessDevToolsClientChangeWindowStateTest("fullscreen") {}
};

HEADLESS_DEVTOOLED_TEST_F(HeadlessDevToolsClientFullscreenWindowTest);

class HeadlessDevToolsClientEvalTest : public HeadlessDevTooledBrowserTest {
 public:
  void RunDevTooledTest() override {
    base::Value::Dict result = SendCommandSync(
        devtools_client_, "Runtime.evaluate", Param("expression", "1 + 2"));

    EXPECT_THAT(result, DictHasValue("result.result.value", 3));

    FinishAsynchronousTest();
  }
};

HEADLESS_DEVTOOLED_TEST_F(HeadlessDevToolsClientEvalTest);

class HeadlessDevToolsNavigationControlTest
    : public HeadlessDevTooledBrowserTest {
 public:
  void RunDevTooledTest() override {
    ASSERT_TRUE(embedded_test_server()->Start());

    SendCommandSync(devtools_client_, "Page.enable");
    SendCommandSync(devtools_client_, "Network.enable");

    base::Value::List patterns;
    patterns.Append(Param("urlPattern", "*"));
    devtools_client_.SendCommand("Network.setRequestInterception",
                                 Param("patterns", std::move(patterns)));

    devtools_client_.AddEventHandler(
        "Network.requestIntercepted",
        base::BindRepeating(
            &HeadlessDevToolsNavigationControlTest::OnRequestIntercepted,
            base::Unretained(this)));

    devtools_client_.AddEventHandler(
        "Page.frameStoppedLoading",
        base::BindRepeating(
            &HeadlessDevToolsNavigationControlTest::OnFrameStoppedLoading,
            base::Unretained(this)));

    devtools_client_.SendCommand(
        "Page.navigate",
        Param("url", embedded_test_server()->GetURL("/hello.html").spec()));
  }

  void OnRequestIntercepted(const base::Value::Dict& params) {
    if (DictBool(params, "params.isNavigationRequest"))
      navigation_requested_ = true;

    // Allow the navigation to proceed.
    devtools_client_.SendCommand(
        "Network.continueInterceptedRequest",
        Param("interceptionId", DictString(params, "params.interceptionId")));
  }

  void OnFrameStoppedLoading(const base::Value::Dict& params) {
    EXPECT_TRUE(navigation_requested_);
    FinishAsynchronousTest();
  }

 private:
  bool navigation_requested_ = false;
};

HEADLESS_DEVTOOLED_TEST_F(HeadlessDevToolsNavigationControlTest);

class HeadlessCrashObserverTest : public HeadlessDevTooledBrowserTest {
 public:
  void RunDevTooledTest() override {
    devtools_client_.AddEventHandler(
        "Inspector.targetCrashed",
        base::BindRepeating(&HeadlessCrashObserverTest::OnTargetCrashed,
                            base::Unretained(this)));

    SendCommandSync(devtools_client_, "Inspector.enable");

    devtools_client_.SendCommand("Page.navigate",
                                 Param("url", blink::kChromeUICrashURL));
  }

  void OnTargetCrashed(const base::Value::Dict&) { FinishAsynchronousTest(); }

  // Make sure we don't fail because the renderer crashed!
  void RenderProcessExited(base::TerminationStatus status,
                           int exit_code) override {
#if BUILDFLAG(IS_WIN) && defined(ADDRESS_SANITIZER)
    // TODO(crbug.com/40577245): Make ASan not interfere and expect a crash.
    // ASan's normal error exit code is 1, which base categorizes as the process
    // being killed.
    EXPECT_EQ(base::TERMINATION_STATUS_PROCESS_WAS_KILLED, status);
#elif BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_FUCHSIA)
    EXPECT_EQ(base::TERMINATION_STATUS_PROCESS_CRASHED, status);
#else
    EXPECT_EQ(base::TERMINATION_STATUS_ABNORMAL_TERMINATION, status);
#endif
  }

 private:
  content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes_;
};

// TODO(crbug.com/40206073): HeadlessCrashObserverTest.RunAsyncTest is flaky on
// Win debug.
#if BUILDFLAG(IS_WIN) && !defined(NDEBUG)
DISABLED_HEADLESS_DEVTOOLED_TEST_F(HeadlessCrashObserverTest);
#else
HEADLESS_DEVTOOLED_TEST_F(HeadlessCrashObserverTest);
#endif

class HeadlessDevToolsNetworkBlockedUrlTest
    : public HeadlessDevTooledBrowserTest {
 public:
  void RunDevTooledTest() override {
    ASSERT_TRUE(embedded_test_server()->Start());

    devtools_client_.AddEventHandler(
        "Network.requestWillBeSent",
        base::BindRepeating(
            &HeadlessDevToolsNetworkBlockedUrlTest::OnRequestWillBeSent,
            base::Unretained(this)));

    devtools_client_.AddEventHandler(
        "Network.responseReceived",
        base::BindRepeating(
            &HeadlessDevToolsNetworkBlockedUrlTest::OnResponseReceived,
            base::Unretained(this)));

    devtools_client_.AddEventHandler(
        "Network.loadingFailed",
        base::BindRepeating(
            &HeadlessDevToolsNetworkBlockedUrlTest::OnLoadingFailed,
            base::Unretained(this)));

    SendCommandSync(devtools_client_, "Network.enable");

    devtools_client_.AddEventHandler(
        "Page.loadEventFired",
        base::BindRepeating(
            &HeadlessDevToolsNetworkBlockedUrlTest::OnLoadEventFired,
            base::Unretained(this)));

    SendCommandSync(devtools_client_, "Page.enable");

    base::Value::List urls;
    urls.Append("dom_tree_test.css");
    devtools_client_.SendCommand("Network.setBlockedURLs",
                                 Param("urls", std::move(urls)));

    devtools_client_.SendCommand(
        "Page.navigate",
        Param("url",
              embedded_test_server()->GetURL("/dom_tree_test.html").spec()));
  }

  std::string GetUrlPath(const std::string& url) const {
    GURL gurl(url);
    return gurl.path();
  }

  void OnRequestWillBeSent(const base::Value::Dict& params) {
    std::string path = GetUrlPath(DictString(params, "params.request.url"));
    requests_to_be_sent_.push_back(path);
    request_id_to_path_[DictString(params, "params.requestId")] = path;
  }

  void OnResponseReceived(const base::Value::Dict& params) {
    responses_received_.push_back(
        GetUrlPath(DictString(params, "params.response.url")));
  }

  void OnLoadingFailed(const base::Value::Dict& params) {
    failures_.push_back(
        request_id_to_path_[DictString(params, "params.requestId")]);
    EXPECT_EQ("inspector", DictString(params, "params.blockedReason"));
  }

  void OnLoadEventFired(const base::Value::Dict&) {
    EXPECT_THAT(
        requests_to_be_sent_,
        testing::UnorderedElementsAre("/dom_tree_test.html",
                                      "/dom_tree_test.css", "/iframe.html"));
    EXPECT_THAT(responses_received_,
                ElementsAre("/dom_tree_test.html", "/iframe.html"));
    EXPECT_THAT(failures_, ElementsAre("/dom_tree_test.css"));

    FinishAsynchronousTest();
  }

  std::map<std::string, std::string> request_id_to_path_;
  std::vector<std::string> requests_to_be_sent_;
  std::vector<std::string> responses_received_;
  std::vector<std::string> failures_;
};

HEADLESS_DEVTOOLED_TEST_F(HeadlessDevToolsNetworkBlockedUrlTest);

class DevToolsNetworkOfflineEmulationTest
    : public HeadlessDevTooledBrowserTest {
  void RunDevTooledTest() override {
    ASSERT_TRUE(embedded_test_server()->Start());

    devtools_client_.AddEventHandler(
        "Network.loadingFailed",
        base::BindRepeating(
            &DevToolsNetworkOfflineEmulationTest::OnLoadingFailed,
            base::Unretained(this)));

    SendCommandSync(devtools_client_, "Network.enable");

    base::Value::Dict params;
    params.Set("offline", true);
    params.Set("latency", 0);
    params.Set("downloadThroughput", 0);
    params.Set("uploadThroughput", 0);
    devtools_client_.SendCommand("Network.emulateNetworkConditions",
                                 std::move(params));

    devtools_client_.SendCommand(
        "Page.navigate",
        Param("url", embedded_test_server()->GetURL("/hello.html").spec()));
  }

  void OnLoadingFailed(const base::Value::Dict& params) {
    EXPECT_THAT(params, DictHasValue("params.errorText",
                                     "net::ERR_INTERNET_DISCONNECTED"));
    EXPECT_EQ("net::ERR_INTERNET_DISCONNECTED",
              DictString(params, "params.errorText"));

    FinishAsynchronousTest();
  }
};

HEADLESS_DEVTOOLED_TEST_F(DevToolsNetworkOfflineEmulationTest);

class DomTreeExtractionBrowserTest : public HeadlessDevTooledBrowserTest {
 public:
  void RunDevTooledTest() override {
    ASSERT_TRUE(embedded_test_server()->Start());

    devtools_client_.AddEventHandler(
        "Page.loadEventFired",
        base::BindRepeating(&DomTreeExtractionBrowserTest::OnLoadEventFired,
                            base::Unretained(this)));

    SendCommandSync(devtools_client_, "Page.enable");

    devtools_client_.SendCommand(
        "Page.navigate",
        Param("url",
              embedded_test_server()->GetURL("/dom_tree_test.html").spec()));
  }

  void OnLoadEventFired(const base::Value::Dict&) {
    SendCommandSync(devtools_client_, "Page.disable");

    base::Value::List css_whitelist;
    css_whitelist.Append("color");
    css_whitelist.Append("display");
    css_whitelist.Append("font-style");
    css_whitelist.Append("font-family");
    css_whitelist.Append("margin-left");
    css_whitelist.Append("margin-right");
    css_whitelist.Append("margin-top");
    css_whitelist.Append("margin-bottom");
    devtools_client_.SendCommand(
        "DOMSnapshot.getSnapshot",
        Param("computedStyleWhitelist", css_whitelist),
        base::BindOnce(&DomTreeExtractionBrowserTest::OnGetSnapshotDone,
                       base::Unretained(this)));
  }

  void OnGetSnapshotDone(base::Value::Dict result) {
    std::vector<base::Value::Dict> dom_nodes;
    GetDomNodes(result, &dom_nodes);

    std::vector<base::Value::Dict> computed_styles;
    GetComputedStyles(result, &computed_styles);

    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath source_root_dir;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root_dir);

    CompareToGolden(
        dom_nodes,
        source_root_dir.Append(FILE_PATH_LITERAL(
            "headless/test/dom_tree_extraction_expected_nodes.txt")));

    CompareToGolden(
        computed_styles,
        source_root_dir.Append(FILE_PATH_LITERAL(
            "headless/test/dom_tree_extraction_expected_styles.txt")));

    FinishAsynchronousTest();
  }

  void GetDomNodes(const base::Value::Dict& result,
                   std::vector<base::Value::Dict>* dom_nodes) {
    GURL::Replacements replace_port;
    replace_port.SetPortStr("");

    const base::Value::List* dom_nodes_list =
        result.FindListByDottedPath("result.domNodes");
    ASSERT_NE(dom_nodes_list, nullptr);
    ASSERT_GT(dom_nodes_list->size(), 0ul);

    const base::Value::List* layout_tree_nodes_list =
        result.FindListByDottedPath("result.layoutTreeNodes");
    ASSERT_NE(layout_tree_nodes_list, nullptr);
    ASSERT_GT(layout_tree_nodes_list->size(), 0ul);

    // For convenience, flatten the dom tree into an array of dicts.
    dom_nodes->reserve(dom_nodes_list->size());
    for (const auto& dom_node : *dom_nodes_list) {
      ASSERT_TRUE(dom_node.is_dict());
      dom_nodes->push_back(dom_node.GetDict().Clone());
      base::Value::Dict& node_dict = dom_nodes->back();

      // Node IDs are assigned in a non deterministic way.
      if (node_dict.Find("backendNodeId"))
        node_dict.Set("backendNodeId", "?");

      // Frame IDs are random.
      if (node_dict.Find("frameId"))
        node_dict.Set("frameId", "?");

      // Ports are random.
      if (base::Value* base_url_value = node_dict.Find("baseURL")) {
        node_dict.Set("baseURL", GURL(base_url_value->GetString())
                                     .ReplaceComponents(replace_port)
                                     .spec());
      }

      if (base::Value* document_url_value = node_dict.Find("documentURL")) {
        node_dict.Set("documentURL", GURL(document_url_value->GetString())
                                         .ReplaceComponents(replace_port)
                                         .spec());
      }

      // Golden file expects scrollOffsetXY to have fractional part.
      // TODO(kvitekp): Consider updating golden files.
      if (std::optional<double> x = node_dict.FindDouble("scrollOffsetX")) {
        node_dict.Set("scrollOffsetX", *x);
      }

      if (std::optional<double> y = node_dict.FindDouble("scrollOffsetY")) {
        node_dict.Set("scrollOffsetY", *y);
      }

      // Merge LayoutTreeNode data into the dom_node dictionary.
      if (std::optional<int> layout_node_index =
              node_dict.FindInt("layoutNodeIndex")) {
        ASSERT_LE(0, *layout_node_index);
        ASSERT_GT(layout_tree_nodes_list->size(),
                  static_cast<size_t>(*layout_node_index));

        const base::Value::Dict& layout_tree_node =
            (*layout_tree_nodes_list)[*layout_node_index].GetDict();

        if (const base::Value::Dict* bounding_box =
                layout_tree_node.FindDict("boundingBox")) {
          node_dict.Set("boundingBox", bounding_box->Clone());
          FixBoundingBox(node_dict);
        }

        if (const std::string* layout_text =
                layout_tree_node.FindString("layoutText")) {
          node_dict.Set("layoutText", *layout_text);
        }

        if (std::optional<int> style_index =
                layout_tree_node.FindInt("styleIndex")) {
          node_dict.Set("styleIndex", *style_index);
        }

        if (const base::Value::List* inline_text_nodes =
                layout_tree_node.FindList("inlineTextNodes")) {
          base::Value::List list = inline_text_nodes->Clone();
          for (auto& list_entry : list) {
            ASSERT_TRUE(list_entry.is_dict());
            FixBoundingBox(list_entry.GetDict());
          }
          node_dict.Set("inlineTextNodes", std::move(list));
        }
      }
    }
  }

  void FixBoundingBox(base::Value::Dict& dict) {
    if (base::Value::Dict* bounding_box = dict.FindDict("boundingBox")) {
      // The golden file expects double values in boundingBox.
      // TODO(kvitekp): Consider updating the golden and just
      // cloning the |boundingBox| dictionary here.
      bounding_box->Set("x", *bounding_box->FindDouble("x"));
      bounding_box->Set("y", *bounding_box->FindDouble("y"));
      bounding_box->Set("width", *bounding_box->FindDouble("width"));
      bounding_box->Set("height", *bounding_box->FindDouble("height"));
    }
  }

  void GetComputedStyles(const base::Value::Dict& result,
                         std::vector<base::Value::Dict>* computed_styles) {
    const base::Value::List* computed_styles_list =
        result.FindListByDottedPath("result.computedStyles");
    ASSERT_NE(computed_styles_list, nullptr);
    ASSERT_GT(computed_styles_list->size(), 0ul);

    computed_styles->reserve(computed_styles_list->size());
    for (const auto& computed_style : *computed_styles_list) {
      ASSERT_TRUE(computed_style.is_dict());
      const base::Value::List* properties_list =
          computed_style.GetDict().FindList("properties");
      ASSERT_NE(properties_list, nullptr);

      base::Value::Dict computed_style_dict;
      for (const auto& property : *properties_list) {
        ASSERT_TRUE(property.is_dict());
        const base::Value::Dict& property_dict = property.GetDict();
        computed_style_dict.Set(*property_dict.FindString("name"),
                                *property_dict.FindString("value"));
      }
      computed_styles->push_back(std::move(computed_style_dict));
    }
  }

  void CompareToGolden(const std::vector<base::Value::Dict>& entries,
                       base::FilePath expected_filepath) {
    std::string expected_entries;
    ASSERT_TRUE(base::ReadFileToString(expected_filepath, &expected_entries));

    std::string actual_entries;
    for (const base::Value::Dict& entry : entries) {
      std::string entry_json;
      base::JSONWriter::WriteWithOptions(
          entry, base::JSONWriter::OPTIONS_PRETTY_PRINT, &entry_json);
      actual_entries += entry_json;
    }

#if BUILDFLAG(IS_WIN)
    ASSERT_TRUE(base::RemoveChars(actual_entries, "\r", &actual_entries));
#endif

    EXPECT_EQ(expected_entries, actual_entries);
  }
};

// TODO(crbug.com/40697467): Fix this test on Fuchsia and re-enable.
// NOTE: These macros expand to: DomTreeExtractionBrowserTest.RunAsyncTest
#if BUILDFLAG(IS_FUCHSIA)
DISABLED_HEADLESS_DEVTOOLED_TEST_F(DomTreeExtractionBrowserTest);
#else
HEADLESS_DEVTOOLED_TEST_F(DomTreeExtractionBrowserTest);
#endif

class BlockedByClient_NetworkObserver_Test
    : public HeadlessDevTooledBrowserTest {
 public:
  void RunDevTooledTest() override {
    ASSERT_TRUE(embedded_test_server()->Start());

    devtools_client_.AddEventHandler(
        "Network.requestIntercepted",
        base::BindRepeating(
            &BlockedByClient_NetworkObserver_Test::OnRequestIntercepted,
            base::Unretained(this)));

    devtools_client_.AddEventHandler(
        "Network.requestWillBeSent",
        base::BindRepeating(
            &BlockedByClient_NetworkObserver_Test::OnRequestWillBeSent,
            base::Unretained(this)));

    devtools_client_.AddEventHandler(
        "Network.loadingFailed",
        base::BindRepeating(
            &BlockedByClient_NetworkObserver_Test::OnLoadingFailed,
            base::Unretained(this)));

    SendCommandSync(devtools_client_, "Network.enable");

    // Intercept all network requests.
    base::Value::List patterns;
    patterns.Append(Param("urlPattern", "*"));
    devtools_client_.SendCommand("Network.setRequestInterception",
                                 Param("patterns", std::move(patterns)));

    devtools_client_.AddEventHandler(
        "Page.loadEventFired",
        base::BindRepeating(
            &BlockedByClient_NetworkObserver_Test::OnLoadEventFired,
            base::Unretained(this)));

    SendCommandSync(devtools_client_, "Page.enable");

    devtools_client_.SendCommand(
        "Page.navigate", Param("url", embedded_test_server()
                                          ->GetURL("/resource_cancel_test.html")
                                          .spec()));
  }

  void OnRequestIntercepted(const base::Value::Dict& params) {
    const std::string url = DictString(params, "params.request.url");

    urls_seen_.push_back(GURL(url).ExtractFileName());

    base::Value::Dict continue_intercept_params;
    continue_intercept_params.Set("interceptionId",
                                  DictString(params, "params.interceptionId"));

    // TODO(kvitekp): comment does not match the following code, review!!!

    // We *abort* fetching Ahem.ttf, and *fail* for test.jpg
    // to verify that both ways result in a failed loading event,
    // which we'll observe in OnLoadingFailed below.
    // Also, we abort iframe2.html because it turns out frame interception
    // uses a very different codepath than other resources.
    if (EndsWith(url, "/test.jpg", base::CompareCase::SENSITIVE)) {
      continue_intercept_params.Set("errorReason", "BlockedByClient");
    } else if (EndsWith(url, "/Ahem.ttf", base::CompareCase::SENSITIVE)) {
      continue_intercept_params.Set("errorReason", "BlockedByClient");
    } else if (EndsWith(url, "/iframe2.html", base::CompareCase::SENSITIVE)) {
      continue_intercept_params.Set("errorReason", "BlockedByClient");
    }

    devtools_client_.SendCommand("Network.continueInterceptedRequest",
                                 std::move(continue_intercept_params));
  }

  void OnRequestWillBeSent(const base::Value::Dict& params) {
    // Here, we just record the URLs (filenames) for each request ID, since
    // we won't have access to them in OnLoadingFailed below.
    urls_by_id_[DictString(params, "params.requestId")] =
        GURL(DictString(params, "params.request.url")).ExtractFileName();
  }

  void OnLoadingFailed(const base::Value::Dict& params) {
    // Record the failed loading events so we can verify below that we
    // received the events.
    urls_that_failed_to_load_.push_back(
        urls_by_id_[DictString(params, "params.requestId")]);
    EXPECT_EQ("inspector", DictString(params, "params.blockedReason"));
  }

  void OnLoadEventFired(const base::Value::Dict&) {
    EXPECT_THAT(urls_that_failed_to_load_,
                UnorderedElementsAre("test.jpg", "Ahem.ttf", "iframe2.html"));
    EXPECT_THAT(urls_seen_, UnorderedElementsAre("resource_cancel_test.html",
                                                 "dom_tree_test.css",
                                                 "test.jpg", "iframe.html",
                                                 "iframe2.html", "Ahem.ttf"));

    FinishAsynchronousTest();
  }

 private:
  std::vector<std::string> urls_seen_;
  std::vector<std::string> urls_that_failed_to_load_;
  std::map<std::string, std::string> urls_by_id_;
};

HEADLESS_DEVTOOLED_TEST_F(BlockedByClient_NetworkObserver_Test);

class DevtoolsInterceptionWithAuthProxyTest
    : public HeadlessDevTooledBrowserTest {
 public:
  DevtoolsInterceptionWithAuthProxyTest()
      : proxy_server_(net::SpawnedTestServer::TYPE_BASIC_AUTH_PROXY,
                      base::FilePath(FILE_PATH_LITERAL("headless/test/data"))) {
  }

  void SetUp() override {
    ASSERT_TRUE(proxy_server_.Start());
    HeadlessDevTooledBrowserTest::SetUp();
  }

  void RunDevTooledTest() override {
    ASSERT_TRUE(embedded_test_server()->Start());

    devtools_client_.AddEventHandler(
        "Network.requestIntercepted",
        base::BindRepeating(
            &DevtoolsInterceptionWithAuthProxyTest::OnRequestIntercepted,
            base::Unretained(this)));

    SendCommandSync(devtools_client_, "Network.enable");

    // Intercept all network requests.
    base::Value::List patterns;
    patterns.Append(Param("urlPattern", "*"));
    devtools_client_.SendCommand("Network.setRequestInterception",
                                 Param("patterns", std::move(patterns)));

    devtools_client_.AddEventHandler(
        "Page.loadEventFired",
        base::BindRepeating(
            &DevtoolsInterceptionWithAuthProxyTest::OnLoadEventFired,
            base::Unretained(this)));

    SendCommandSync(devtools_client_, "Page.enable");

    devtools_client_.SendCommand(
        "Page.navigate",
        Param("url",
              embedded_test_server()->GetURL("/dom_tree_test.html").spec()));
  }

  void OnRequestIntercepted(const base::Value::Dict& params) {
    base::Value::Dict continue_intercept_params;
    continue_intercept_params.Set("interceptionId",
                                  DictString(params, "params.interceptionId"));

    if (DictHas(params, "params.authChallenge")) {
      auth_challenge_seen_ = true;

      base::Value::Dict auth_challenge_response;
      auth_challenge_response.Set("response", "ProvideCredentials");
      auth_challenge_response.Set("username", "foo");
      auth_challenge_response.Set("password", "bar");
      continue_intercept_params.Set("authChallengeResponse",
                                    std::move(auth_challenge_response));
    } else {
      GURL url(DictString(params, "params.request.url"));
      files_loaded_.insert(url.path());
    }

    devtools_client_.SendCommand("Network.continueInterceptedRequest",
                                 std::move(continue_intercept_params));
  }

  void OnLoadEventFired(const base::Value::Dict&) {
    EXPECT_TRUE(auth_challenge_seen_);
    EXPECT_THAT(files_loaded_,
                ElementsAre("/Ahem.ttf", "/dom_tree_test.css",
                            "/dom_tree_test.html", "/iframe.html"));

    FinishAsynchronousTest();
  }

  void CustomizeHeadlessBrowserContext(
      HeadlessBrowserContext::Builder& builder) override {
    std::unique_ptr<net::ProxyConfig> proxy_config(new net::ProxyConfig);
    proxy_config->proxy_rules().ParseFromString(
        proxy_server_.host_port_pair().ToString());
    // TODO(crbug.com/40600992): Don't rely on proxying localhost.
    proxy_config->proxy_rules().bypass_rules.AddRulesToSubtractImplicit();
    builder.SetProxyConfig(std::move(proxy_config));
  }

 private:
  net::SpawnedTestServer proxy_server_;
  bool auth_challenge_seen_ = false;
  std::set<std::string> files_loaded_;
};

#if BUILDFLAG(IS_FUCHSIA)
// TODO(crbug.com/40697469): Reenable on Fuchsia when fixed.
// NOTE: This macro expands to:
//   DevtoolsInterceptionWithAuthProxyTest.RunAsyncTest
DISABLED_HEADLESS_DEVTOOLED_TEST_F(DevtoolsInterceptionWithAuthProxyTest);
#else
HEADLESS_DEVTOOLED_TEST_F(DevtoolsInterceptionWithAuthProxyTest);
#endif

class NavigatorLanguages : public HeadlessDevTooledBrowserTest {
 public:
  void RunDevTooledTest() override {
    devtools_client_.SendCommand(
        "Runtime.evaluate",
        Param("expression", "JSON.stringify(navigator.languages)"),
        base::BindOnce(&NavigatorLanguages::OnEvaluateResult,
                       base::Unretained(this)));
  }

  void OnEvaluateResult(base::Value::Dict result) {
    EXPECT_THAT(result, DictHasValue("result.result.value",
                                     "[\"en-UK\",\"DE\",\"FR\"]"));
    FinishAsynchronousTest();
  }

  void CustomizeHeadlessBrowserContext(
      HeadlessBrowserContext::Builder& builder) override {
    builder.SetAcceptLanguage("en-UK, DE, FR");
  }
};

HEADLESS_DEVTOOLED_TEST_F(NavigatorLanguages);

class AcceptLanguagesSwitch : public HeadlessDevTooledBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    HeadlessDevTooledBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kAcceptLang, "cz-CZ");
  }

  void RunDevTooledTest() override {
    devtools_client_.SendCommand(
        "Runtime.evaluate",
        Param("expression", "JSON.stringify(navigator.languages)"),
        base::BindOnce(&AcceptLanguagesSwitch::OnEvaluateResult,
                       base::Unretained(this)));
  }

  void OnEvaluateResult(base::Value::Dict result) {
    EXPECT_THAT(result, DictHasValue("result.result.value", "[\"cz-CZ\"]"));
    FinishAsynchronousTest();
  }
};

HEADLESS_DEVTOOLED_TEST_F(AcceptLanguagesSwitch);

}  // namespace headless
