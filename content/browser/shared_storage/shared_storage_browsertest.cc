// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/metrics_hashes.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/metrics/dwa/dwa_recorder.h"
#include "content/browser/fenced_frame/fenced_frame_config.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/shared_storage/shared_storage_browsertest_base.h"
#include "content/browser/shared_storage/shared_storage_document_service_impl.h"
#include "content/browser/shared_storage/shared_storage_event_params.h"
#include "content/browser/shared_storage/shared_storage_runtime_manager.h"
#include "content/browser/shared_storage/shared_storage_worklet_host.h"
#include "content/browser/shared_storage/test_shared_storage_observer.h"
#include "content/browser/shared_storage/test_shared_storage_runtime_manager.h"
#include "content/browser/shared_storage/test_shared_storage_worklet_host.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/common/page_type.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/shared_storage_test_utils.h"
#include "content/public/test/test_devtools_protocol_client.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_select_url_fenced_frame_config_observer.h"
#include "content/public/test/test_shared_storage_header_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "content/test/fenced_frame_test_utils.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/schemeful_site.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/chrome_debug_urls.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/common/fenced_frame/fenced_frame_utils.h"
#include "third_party/blink/public/common/fenced_frame/redacted_fenced_frame_config.h"
#include "third_party/blink/public/common/messaging/cloneable_message.h"
#include "third_party/blink/public/common/shared_storage/shared_storage_utils.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "third_party/blink/public/mojom/loader/code_cache.mojom.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage.mojom.h"
#include "ui/base/page_transition_types.h"
#include "url/url_constants.h"

namespace content {

using testing::ElementsAre;
using testing::Pair;
using testing::UnorderedElementsAre;
using SharedStorageReportingMap = base::flat_map<std::string, ::GURL>;
using SharedStorageUrlSpecWithMetadata =
    SharedStorageEventParams::SharedStorageUrlSpecWithMetadata;
using AccessScope = blink::SharedStorageAccessScope;
using AccessMethod = TestSharedStorageObserver::AccessMethod;

namespace {

using WorkletHosts = SharedStorageRuntimeManager::WorkletHosts;

constexpr char kSharedStorageWorkletExpiredMessage[] =
    "The sharedStorage worklet cannot execute further operations because the "
    "previous operation did not include the option \'keepAlive: true\'.";

constexpr char kTitle1Path[] = "/title1.html";

constexpr char kTitle2Path[] = "/title2.html";

constexpr char kTitle3Path[] = "/title3.html";

constexpr char kTitle4Path[] = "/title4.html";

constexpr char kPngPath[] = "/shared_storage/pixel.png";

constexpr char kSharedStorageTrustedOriginsPath[] =
    "/.well-known/shared-storage/trusted-origins";

constexpr char kDestroyedStatusHistogram[] =
    "Storage.SharedStorage.Worklet.DestroyedStatus";

constexpr char kTimingKeepAliveDurationHistogram[] =
    "Storage.SharedStorage.Worklet.Timing."
    "KeepAliveEndedDueToOperationsFinished.KeepAliveDuration";

constexpr char kErrorTypeHistogram[] =
    "Storage.SharedStorage.Worklet.Error.Type";

constexpr char kTimingUsefulResourceHistogram[] =
    "Storage.SharedStorage.Worklet.Timing.UsefulResourceDuration";

constexpr char kTimingRunExecutedInWorkletHistogram[] =
    "Storage.SharedStorage.Document.Timing.Run.ExecutedInWorklet";

constexpr char kEmptyAccessControlAllowOriginReplacement[] = "";

constexpr char kEmptySharedStorageCrossOriginAllowedReplacement[] = "";

std::string ReplacePortInString(std::string str, uint16_t port) {
  const std::string kToReplace("{{port}}");
  size_t index = str.find(kToReplace);
  while (index != std::string::npos) {
    str = str.replace(index, kToReplace.size(), base::NumberToString(port));
    index = str.find(kToReplace);
  }
  return str;
}

auto describe_param = [](const auto& info) {
  return base::StrCat({"ResolveSelectURLTo", info.param ? "Config" : "URN"});
};

FrameTreeNode* PrimaryFrameTreeNodeRootFromShell(Shell* shell) {
  return static_cast<WebContentsImpl*>(shell->web_contents())
      ->GetPrimaryFrameTree()
      .root();
}

bool IsLocalRoot(RenderFrameHost* rfh) {
  CHECK(rfh);
  return static_cast<RenderFrameHostImpl*>(rfh)->is_local_root();
}

std::string ValueToString(const base::Value* value) {
  if (!value) {
    return "[[NULL]]";
  }
  switch (value->type()) {
    case base::Value::Type::STRING:
      return value->GetString();
    case base::Value::Type::NONE:
      return "[[NONE]]";
    case base::Value::Type::BOOLEAN:
      return value->GetBool() ? "true" : "false";
    case base::Value::Type::INTEGER:
      return base::NumberToString(value->GetInt());
    case base::Value::Type::DOUBLE:
      return base::NumberToString(value->GetDouble());
    case base::Value::Type::BINARY: {
      const std::vector<uint8_t>& value_blob = value->GetBlob();
      return std::string(value_blob.begin(), value_blob.end());
    }
    case base::Value::Type::LIST:
      return base::WriteJson(value->GetList()).value_or("[[LIST]]");
    case base::Value::Type::DICT:
      return base::WriteJson(value->GetDict()).value_or("[[DICT]]");
  }
  NOTREACHED();
}

std::string SerializeVectorOfMapOfStrings(
    const std::vector<std::map<std::string, std::string>>& input_vector) {
  std::ostringstream oss;
  oss << "[";
  if (!input_vector.empty()) {
    oss << "\n";
  }
  for (const auto& input_map : input_vector) {
    oss << "  {";
    for (const auto& map_pair : input_map) {
      oss << " {" << map_pair.first << ", " << map_pair.second << "} ";
    }
    oss << "}\n";
  }
  oss << "]";
  return oss.str();
}

class TestSharedStorageDevToolsClient : public TestDevToolsProtocolClient {
 public:
  explicit TestSharedStorageDevToolsClient(RenderFrameHost* rfh) {
    AttachToFrameTreeHost(rfh);
    SendCommandSync("Storage.setSharedStorageTracking",
                    base::Value::Dict().Set("enable", true));
  }
  ~TestSharedStorageDevToolsClient() override { DetachProtocolClient(); }

  void set_expected_notification_method(
      const std::string& expected_notification_method) {
    expected_notification_method_ = expected_notification_method;
  }

  void DispatchProtocolMessage(DevToolsAgentHost* agent_host,
                               base::span<const uint8_t> message) override {
    std::string_view message_str(reinterpret_cast<const char*>(message.data()),
                                 message.size());
    base::Value parsed = *base::JSONReader::Read(
        message_str, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
    std::optional<int> id = parsed.GetDict().FindInt("id");
    if (!id) {
      const std::string* notification = parsed.GetDict().FindString("method");
      ASSERT_TRUE(notification);
      if (expected_notification_method_ == *notification) {
        base::Value* params = parsed.GetDict().Find("params");
        ASSERT_TRUE(params);
        params_for_notifications_with_expected_method_.push_back(
            std::move(*params).TakeDict());
      }
    }

    TestDevToolsProtocolClient::DispatchProtocolMessage(agent_host,
                                                        std::move(message));
  }

  std::vector<std::map<std::string, std::string>>
  GetSelectedParamsAsStringsForNotificationsWithExpectedMethod(
      std::vector<std::string> selected_key_paths) const {
    std::sort(selected_key_paths.begin(), selected_key_paths.end());
    std::vector<std::map<std::string, std::string>>
        selected_params_for_notifications_;
    for (const base::Value::Dict& params :
         params_for_notifications_with_expected_method_) {
      selected_params_for_notifications_.push_back(
          std::map<std::string, std::string>());
      for (const std::string& path : selected_key_paths) {
        const base::Value* param = params.FindByDottedPath(path);
        if (param) {
          selected_params_for_notifications_.back().emplace(
              path, ValueToString(param));
        }
      }
    }
    return selected_params_for_notifications_;
  }

 private:
  std::string expected_notification_method_;
  std::vector<base::Value::Dict> params_for_notifications_with_expected_method_;
};

}  // namespace

class SharedStorageTrustedOriginsResponse
    : public net::test_server::BasicHttpResponse {
 public:
  SharedStorageTrustedOriginsResponse(
      const base::Value* json_trusted_origins_list,
      uint16_t port,
      bool force_server_error)
      : force_server_error_(force_server_error) {
    if (json_trusted_origins_list) {
      json_trusted_origins_list_str_ =
          base::WriteJson(*json_trusted_origins_list);
      if (json_trusted_origins_list_str_) {
        json_trusted_origins_list_str_ =
            ReplacePortInString(*json_trusted_origins_list_str_, port);
      }
    }
  }

  SharedStorageTrustedOriginsResponse(
      const SharedStorageTrustedOriginsResponse&) = delete;
  SharedStorageTrustedOriginsResponse& operator=(
      const SharedStorageTrustedOriginsResponse&) = delete;

  ~SharedStorageTrustedOriginsResponse() override = default;

  void SendResponse(
      base::WeakPtr<net::test_server::HttpResponseDelegate> delegate) override {
    if (!json_trusted_origins_list_str_ || force_server_error_) {
      set_code(net::HttpStatusCode::HTTP_INTERNAL_SERVER_ERROR);
      set_content_type("text/plain");
      set_content("");
    } else {
      set_code(net::HttpStatusCode::HTTP_OK);
      set_content_type("application/json");
      AddCustomHeader("Access-Control-Allow-Origin", "*");
      set_content(*json_trusted_origins_list_str_);
    }
    delegate->SendResponseHeaders(code(), GetHttpReasonPhrase(code()),
                                  BuildHeaders());
    delegate->SendContents(content(), base::DoNothing());
  }

 private:
  bool force_server_error_;
  std::optional<std::string> json_trusted_origins_list_str_;
};

class SharedStorageBrowserTest : public SharedStorageBrowserTestBase,
                                 public testing::WithParamInterface<bool> {
 public:
  SharedStorageBrowserTest() {
    fenced_frame_api_change_feature_.InitWithFeatureState(
        blink::features::kFencedFramesAPIChanges, ResolveSelectURLToConfig());
    custom_data_origin_feature_.InitAndEnableFeature(
        blink::features::kSharedStorageCreateWorkletCustomDataOrigin);
    dwa_feature_.InitAndEnableFeature(metrics::dwa::kDwaFeature);
  }

  bool ResolveSelectURLToConfig() override { return GetParam(); }

  mojo_base::BigBuffer GetCodeCacheDataForUrl(RenderFrameHost* rfh,
                                              const GURL& url) {
    mojo::PendingRemote<blink::mojom::CodeCacheHost> pending_code_cache_host;
    RenderFrameHostImpl::From(rfh)->CreateCodeCacheHost(
        pending_code_cache_host.InitWithNewPipeAndPassReceiver());

    mojo::Remote<blink::mojom::CodeCacheHost> inspecting_code_cache_host(
        std::move(pending_code_cache_host));

    base::test::TestFuture<base::Time, mojo_base::BigBuffer> code_cache_future;
    inspecting_code_cache_host->FetchCachedCode(
        blink::mojom::CodeCacheType::kJavascript, url,
        code_cache_future.GetCallback());

    auto [response_time, data] = code_cache_future.Take();
    return std::move(data);
  }

  ~SharedStorageBrowserTest() override = default;

  void set_trusted_origins_list_index(size_t trusted_origins_list_index) {
    trusted_origins_list_index_ = trusted_origins_list_index;
  }

  void set_force_server_error(bool force_server_error) {
    force_server_error_ = force_server_error;
  }

  void RegisterCustomRequestHandlers() override {
    RegisterSharedStorageTrustedOriginsRequestHandler();
  }

  void RegisterSharedStorageTrustedOriginsRequestHandler() {
    https_server()->RegisterRequestHandler(base::BindRepeating(
        &SharedStorageBrowserTest::HandleSharedStorageTrustedOriginsRequest,
        base::Unretained(this), BuildWellKnownTrustedOriginsLists()));
  }

  // Virtual so that a derived class can build more a realistic vector of
  // values. We use base::Value instead of base::Value::List, even though a
  // correctly formatted entry would be a base::Value::List, so that a derived
  // class can test the parse error that would happen if the JSON returned isn't
  // a list.
  //
  // SharedStorageBrowserTest::BuildWellKnownTrustedOriginsLists builds a vector
  // with a single list entry that simply allowlists all combinations of script
  // and context origins.
  virtual std::vector<base::Value> BuildWellKnownTrustedOriginsLists() {
    std::vector<base::Value> trusted_origins_lists;
    trusted_origins_lists.push_back(static_cast<base::Value>(
        base::Value::List().Append(base::Value::Dict()
                                       .Set("scriptOrigin", "*")
                                       .Set("contextOrigin", "*"))));
    return trusted_origins_lists;
  }

  std::unique_ptr<net::test_server::HttpResponse>
  HandleSharedStorageTrustedOriginsRequest(
      const std::vector<base::Value>& json_well_known_trusted_origin_lists,
      const net::test_server::HttpRequest& request) {
    const auto& path = request.GetURL().GetPath();
    if (path != kSharedStorageTrustedOriginsPath ||
        json_well_known_trusted_origin_lists.empty()) {
      return nullptr;
    }
    size_t index = trusted_origins_list_index_ %
                   json_well_known_trusted_origin_lists.size();
    return std::make_unique<SharedStorageTrustedOriginsResponse>(
        &json_well_known_trusted_origin_lists[index], port(),
        force_server_error_);
  }

  bool NavigateToUrlMaybeWaitForRfhDeleted(Shell* shell, GURL url) {
    auto* rfh = shell->web_contents()->GetPrimaryMainFrame();
    bool result;
    if (rfh->ShouldChangeRenderFrameHostOnSameSiteNavigation()) {
      RenderFrameDeletedObserver observer(rfh);
      result = NavigateToURL(shell, url);
      observer.WaitUntilDeleted();
    } else {
      result = NavigateToURL(shell, url);
    }
    return result;
  }

 private:
  base::test::ScopedFeatureList fenced_frame_api_change_feature_;
  base::test::ScopedFeatureList custom_data_origin_feature_;
  base::test::ScopedFeatureList dwa_feature_;
  size_t trusted_origins_list_index_ = 0;
  bool force_server_error_ = false;
};

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest, AddModule_Success) {
  // The test assumes pages get deleted after navigation. To ensure this,
  // disable back/forward cache.
  content::DisableBackForwardCacheForTesting(
      shell()->web_contents(),
      content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));
  GlobalRenderFrameHostId initial_main_frame_id = MainFrameId();

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

  // Navigate again to record histograms.
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));
  WaitForHistograms(
      {kDestroyedStatusHistogram, kTimingUsefulResourceHistogram});

  histogram_tester_.ExpectUniqueSample(
      kDestroyedStatusHistogram,
      blink::SharedStorageWorkletDestroyedStatus::kDidNotEnterKeepAlive, 1);
  histogram_tester_.ExpectTotalCount(kTimingUsefulResourceHistogram, 1);

  ExpectAccessObserved(
      {{AccessScope::kWindow, AccessMethod::kAddModule, initial_main_frame_id,
        url::Origin::Create(url).Serialize(),
        SharedStorageEventParams::CreateForAddModule(
            https_server()->GetURL("a.test",
                                   "/shared_storage/simple_module.js"),
            /*worklet_ordinal=*/0, GetFirstWorkletHostDevToolsToken())}});
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest, AddModule_ScriptNotFound) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  std::string expected_error = base::StrCat(
      {"a JavaScript error: \"OperationError: Failed to load ",
       https_server()
           ->GetURL("a.test", "/shared_storage/nonexistent_module.js")
           .spec(),
       " HTTP status = 404 Not Found.\"\n"});

  EXPECT_THAT(EvalJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/nonexistent_module.js');
    )"),
              EvalJsResult::ErrorIs(expected_error));

  EXPECT_EQ(1u, test_runtime_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_runtime_manager().GetKeepAliveWorkletHostsCount());
  EXPECT_EQ(0u, console_observer.messages().size());

  ExpectAccessObserved(
      {{AccessScope::kWindow, AccessMethod::kAddModule, MainFrameId(),
        url::Origin::Create(url).Serialize(),
        SharedStorageEventParams::CreateForAddModule(
            https_server()->GetURL("a.test",
                                   "/shared_storage/nonexistent_module.js"),
            /*worklet_ordinal=*/0, GetFirstWorkletHostDevToolsToken())}});
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest, AddModule_RedirectNotAllowed) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  std::string expected_error = base::StrCat(
      {"a JavaScript error: \"OperationError: Unexpected redirect on ",
       https_server()
           ->GetURL("a.test",
                    "/server-redirect?shared_storage/simple_module.js")
           .spec(),
       ".\"\n"});

  EvalJsResult result = EvalJs(shell(), R"(
      sharedStorage.worklet.addModule(
          '/server-redirect?shared_storage/simple_module.js');
    )");

  EXPECT_THAT(result, content::EvalJsResult::ErrorIs(expected_error));

  EXPECT_EQ(1u, test_runtime_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_runtime_manager().GetKeepAliveWorkletHostsCount());
  EXPECT_EQ(0u, console_observer.messages().size());

  ExpectAccessObserved(
      {{AccessScope::kWindow, AccessMethod::kAddModule, MainFrameId(),
        url::Origin::Create(url).Serialize(),
        SharedStorageEventParams::CreateForAddModule(
            https_server()->GetURL(
                "a.test", "/server-redirect?shared_storage/simple_module.js"),
            /*worklet_ordinal=*/0, GetFirstWorkletHostDevToolsToken())}});
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest,
                       AddModule_ScriptExecutionFailure) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_THAT(EvalJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/erroneous_module.js');
    )"),
              EvalJsResult::ErrorIs(testing::HasSubstr(
                  "ReferenceError: undefinedVariable is not defined")));

  EXPECT_EQ(1u, test_runtime_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_runtime_manager().GetKeepAliveWorkletHostsCount());
  EXPECT_EQ(1u, console_observer.messages().size());
  EXPECT_EQ("Start executing erroneous_module.js",
            base::UTF16ToUTF8(console_observer.messages()[0].message));

  ExpectAccessObserved(
      {{AccessScope::kWindow, AccessMethod::kAddModule, MainFrameId(),
        url::Origin::Create(url).Serialize(),
        SharedStorageEventParams::CreateForAddModule(
            https_server()->GetURL("a.test",
                                   "/shared_storage/erroneous_module.js"),
            /*worklet_ordinal=*/0, GetFirstWorkletHostDevToolsToken())}});
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest,
                       AddModule_MultipleAddModuleFailure) {
  // The test assumes pages get deleted after navigation. To ensure this,
  // disable back/forward cache.
  content::DisableBackForwardCacheForTesting(
      shell()->web_contents(),
      content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));
  GlobalRenderFrameHostId initial_main_frame_id = MainFrameId();

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  EXPECT_THAT(EvalJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"),
              EvalJsResult::ErrorIs(testing::HasSubstr(
                  "addModule() can only be invoked once per worklet")));

  EXPECT_EQ(1u, test_runtime_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_runtime_manager().GetKeepAliveWorkletHostsCount());
  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("Start executing simple_module.js",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("Finish executing simple_module.js",
            base::UTF16ToUTF8(console_observer.messages()[1].message));

  // Navigate again to record histograms.
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));
  WaitForHistograms(
      {kDestroyedStatusHistogram, kTimingUsefulResourceHistogram});

  histogram_tester_.ExpectUniqueSample(
      kDestroyedStatusHistogram,
      blink::SharedStorageWorkletDestroyedStatus::kDidNotEnterKeepAlive, 1);
  histogram_tester_.ExpectTotalCount(kTimingUsefulResourceHistogram, 1);

  std::string origin_str = url::Origin::Create(url).Serialize();
  ExpectAccessObserved(
      {{AccessScope::kWindow, AccessMethod::kAddModule, initial_main_frame_id,
        origin_str,
        SharedStorageEventParams::CreateForAddModule(
            https_server()->GetURL("a.test",
                                   "/shared_storage/simple_module.js"),
            /*worklet_ordinal=*/0, GetFirstWorkletHostDevToolsToken())}});
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest,
                       AddModue_TheThirdTimeCompilesWithV8CodeCache) {
  if (blink::features::IsPersistentCacheForCodeCacheEnabled()) {
    GTEST_SKIP() << "SharedStorage does not use a CodeCache when "
                    "UsePersistentCacheForCodeCache is enabled.";
  }

  // The test assumes pages get deleted after navigation. To ensure this,
  // disable back/forward cache.
  content::DisableBackForwardCacheForTesting(
      shell()->web_contents(),
      content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  GURL module_url = https_server()->GetURL(
      "a.test", "/shared_storage/large_cacheable_script.js");

  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Initially, the code cache has no data.
  mojo_base::BigBuffer code_cache_data0 = GetCodeCacheDataForUrl(
      shell()->web_contents()->GetPrimaryMainFrame(), module_url);
  EXPECT_EQ(code_cache_data0.size(), 0u);

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule(
        'shared_storage/large_cacheable_script.js');
    )"));

  mojo_base::BigBuffer code_cache_data1 = GetCodeCacheDataForUrl(
      shell()->web_contents()->GetPrimaryMainFrame(), module_url);
  EXPECT_GT(code_cache_data1.size(), 0u);

  // After the first script loading, the code cache has some data.
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule(
        'shared_storage/large_cacheable_script.js');
    )"));

  // After the second script loading, the code cache has more data. This implies
  // that the code cache wasn't used for the second compilation. This is
  // expected, as we won't store the cached code entirely for first seen URLs.
  mojo_base::BigBuffer code_cache_data2 = GetCodeCacheDataForUrl(
      shell()->web_contents()->GetPrimaryMainFrame(), module_url);
  EXPECT_GT(code_cache_data2.size(), code_cache_data1.size());

  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule(
        'shared_storage/large_cacheable_script.js');
    )"));

  // After the third script loading, the code cache does not change. This
  // implies that the code cache was used for the third compilation.
  mojo_base::BigBuffer code_cache_data3 = GetCodeCacheDataForUrl(
      shell()->web_contents()->GetPrimaryMainFrame(), module_url);
  EXPECT_EQ(code_cache_data3.size(), code_cache_data2.size());
  EXPECT_TRUE(std::equal(code_cache_data3.begin(), code_cache_data3.end(),
                         code_cache_data2.begin()));
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest, RunOperation_Success) {
  metrics::dwa::DwaRecorder::Get()->EnableRecording();
  metrics::dwa::DwaRecorder::Get()->Purge();
  ASSERT_THAT(metrics::dwa::DwaRecorder::Get()->GetEntriesForTesting(),
              testing::IsEmpty());

  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

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

  // There is 1 more "worklet operation": `run()`.
  test_runtime_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.run(
          'test-operation', {data: {'customKey': 'customValue'}});
    )"));

  test_runtime_manager().GetAttachedWorkletHost()->WaitForWorkletResponses();

  EXPECT_EQ(5u, console_observer.messages().size());
  EXPECT_EQ("Start executing 'test-operation'",
            base::UTF16ToUTF8(console_observer.messages()[2].message));
  EXPECT_EQ("{\"customKey\":\"customValue\"}",
            base::UTF16ToUTF8(console_observer.messages()[3].message));
  EXPECT_EQ("Finish executing 'test-operation'",
            base::UTF16ToUTF8(console_observer.messages()[4].message));

  WaitForHistograms({kTimingRunExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingRunExecutedInWorkletHistogram, 1);

  const auto& entries =
      metrics::dwa::DwaRecorder::Get()->GetEntriesForTesting();
  EXPECT_EQ(entries.size(), 1u);
  EXPECT_THAT(entries[0]->event_hash,
              base::HashMetricName("SharedStorage.RunFinishedInWorklet"));
  EXPECT_THAT(
      entries[0]->content_hash,
      base::HashMetricName(
          net::registry_controlled_domains::GetDomainAndRegistry(
              url,
              net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)));
  EXPECT_EQ(entries[0]->metrics.size(), 1u);

  std::string origin_str = url::Origin::Create(url).Serialize();
  ExpectAccessObserved(
      {{AccessScope::kWindow, AccessMethod::kAddModule, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForAddModule(
            https_server()->GetURL("a.test",
                                   "/shared_storage/simple_module.js"),
            /*worklet_ordinal=*/0, GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kWindow, AccessMethod::kRun, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForRunForTesting(
            "test-operation", /*operation_id=*/0,
            /*keep_alive=*/false,
            SharedStorageEventParams::PrivateAggregationConfigWrapper(),
            blink::CloneableMessage(), GetFirstWorkletHostDevToolsToken())}});

  ExpectOperationFinishedInfosObserved(
      {{base::TimeDelta(), AccessMethod::kRun, /*operation_id=*/0,
        GetFirstWorkletHostDevToolsToken(), MainFrameId(), origin_str}});
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest,
                       RunOperation_Failure_RunOperationBeforeAddModule) {
  // The test assumes pages get deleted after navigation. To ensure this,
  // disable back/forward cache.
  content::DisableBackForwardCacheForTesting(
      shell()->web_contents(),
      content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));
  GlobalRenderFrameHostId initial_main_frame_id = MainFrameId();

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_THAT(
      EvalJs(shell(), R"(
      sharedStorage.run(
          'test-operation', {data: {'customKey': 'customValue'},
                             keepAlive: true});
    )"),
      EvalJsResult::ErrorIs(testing::HasSubstr(
          "sharedStorage.worklet.addModule() has to be called before run()")));

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

  content::FetchHistogramsFromChildProcesses();

  // Navigate to terminate the worklet.
  EXPECT_TRUE(
      NavigateToUrlMaybeWaitForRfhDeleted(shell(), GURL(url::kAboutBlankURL)));

  histogram_tester_.ExpectUniqueSample(
      kDestroyedStatusHistogram,
      blink::SharedStorageWorkletDestroyedStatus::kDidNotEnterKeepAlive, 1);
  histogram_tester_.ExpectBucketCount(
      kErrorTypeHistogram, blink::SharedStorageWorkletErrorType::kSuccess, 1);
  histogram_tester_.ExpectBucketCount(
      kErrorTypeHistogram, blink::SharedStorageWorkletErrorType::kRunWebVisible,
      1);
  histogram_tester_.ExpectTotalCount(kTimingUsefulResourceHistogram, 1);

  std::string origin_str = url::Origin::Create(url).Serialize();
  ExpectAccessObserved(
      {{AccessScope::kWindow, AccessMethod::kAddModule, initial_main_frame_id,
        origin_str,
        SharedStorageEventParams::CreateForAddModule(
            https_server()->GetURL("a.test",
                                   "/shared_storage/simple_module.js"),
            /*worklet_ordinal=*/0, GetFirstWorkletHostDevToolsToken())}});
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest,
                       RunOperation_Failure_InvalidOptionsArgument) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  EXPECT_THAT(EvalJs(shell(), R"(
      function testFunction() {}

      sharedStorage.run(
          'test-operation', {data: {'customKey': testFunction}});
    )"),
              EvalJsResult::ErrorIs(testing::HasSubstr(
                  "function testFunction() {} could not be cloned")));

  histogram_tester_.ExpectTotalCount(kTimingRunExecutedInWorkletHistogram, 0);

  std::string origin_str = url::Origin::Create(url).Serialize();
  ExpectAccessObserved(
      {{AccessScope::kWindow, AccessMethod::kAddModule, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForAddModule(
            https_server()->GetURL("a.test",
                                   "/shared_storage/simple_module.js"),
            /*worklet_ordinal=*/0, GetFirstWorkletHostDevToolsToken())}});
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest,
                       RunOperation_VerifyUndefinedData) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

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

  // There is 1 more "worklet operation": `run()`.
  test_runtime_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.run('test-operation', /*options=*/{});
    )"));

  test_runtime_manager().GetAttachedWorkletHost()->WaitForWorkletResponses();

  EXPECT_EQ(5u, console_observer.messages().size());
  EXPECT_EQ("Start executing 'test-operation'",
            base::UTF16ToUTF8(console_observer.messages()[2].message));
  EXPECT_EQ("undefined",
            base::UTF16ToUTF8(console_observer.messages()[3].message));
  EXPECT_EQ("Finish executing 'test-operation'",
            base::UTF16ToUTF8(console_observer.messages()[4].message));
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest,
                       RunOperation_Failure_BlobDataTypeNotSupportedInWorklet) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

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

  // There is 1 more "worklet operation": `run()`.
  test_runtime_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EXPECT_TRUE(ExecJs(shell(), R"(
      const blob = new Blob(["abc"], {type: 'text/plain'});
      sharedStorage.run('test-operation', /*options=*/{data: blob});
    )"));

  test_runtime_manager().GetAttachedWorkletHost()->WaitForWorkletResponses();

  EXPECT_EQ(3u, console_observer.messages().size());
  EXPECT_EQ("Cannot deserialize data.",
            base::UTF16ToUTF8(console_observer.messages()[2].message));
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest,
                       RunOperation_VerifyCryptoKeyData) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

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

  // There is 1 more "worklet operation": `run()`.
  test_runtime_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EXPECT_TRUE(ExecJs(shell(), R"(
      const myPromise = new Promise((resolve, reject) => {
        crypto.subtle.generateKey(
          {
            name: "AES-GCM",
            length: 256,
          },
          true,
          ["encrypt", "decrypt"]
        ).then((key) => {
          sharedStorage.run('test-operation', /*options=*/{data: key})
                       .then(() => { resolve(); });
        });
      });
    )"));

  test_runtime_manager().GetAttachedWorkletHost()->WaitForWorkletResponses();

  EXPECT_EQ(5u, console_observer.messages().size());
  EXPECT_EQ("Start executing 'test-operation'",
            base::UTF16ToUTF8(console_observer.messages()[2].message));
  EXPECT_EQ(
      "CryptoKey, algorithm: {\"length\":256,\"name\":\"AES-GCM\"} usages: "
      "[\"encrypt\",\"decrypt\"] extractable: true",
      base::UTF16ToUTF8(console_observer.messages()[3].message));
  EXPECT_EQ("Finish executing 'test-operation'",
            base::UTF16ToUTF8(console_observer.messages()[4].message));
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest,
                       RunOperation_Failure_ErrorInRunOperation) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule(
          'shared_storage/erroneous_function_module.js');
    )"));

  EXPECT_EQ(1u, test_runtime_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_runtime_manager().GetKeepAliveWorkletHostsCount());

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("Start executing erroneous_function_module.js",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ(blink::mojom::ConsoleMessageLevel::kInfo,
            console_observer.messages()[0].log_level);
  EXPECT_EQ("Finish executing erroneous_function_module.js",
            base::UTF16ToUTF8(console_observer.messages()[1].message));
  EXPECT_EQ(blink::mojom::ConsoleMessageLevel::kInfo,
            console_observer.messages()[0].log_level);

  // There is 1 more "worklet operation": `run()`.
  test_runtime_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.run(
          'test-operation', {data: {'customKey': 'customValue'}});
    )"));

  test_runtime_manager().GetAttachedWorkletHost()->WaitForWorkletResponses();

  EXPECT_EQ(4u, console_observer.messages().size());
  EXPECT_EQ("Start executing 'test-operation'",
            base::UTF16ToUTF8(console_observer.messages()[2].message));
  EXPECT_EQ(blink::mojom::ConsoleMessageLevel::kInfo,
            console_observer.messages()[2].log_level);
  EXPECT_THAT(
      base::UTF16ToUTF8(console_observer.messages()[3].message),
      testing::HasSubstr("ReferenceError: undefinedVariable is not defined"));
  EXPECT_EQ(blink::mojom::ConsoleMessageLevel::kError,
            console_observer.messages()[3].log_level);

  WaitForHistograms({kTimingRunExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingRunExecutedInWorkletHistogram, 1);

  std::string origin_str = url::Origin::Create(url).Serialize();
  ExpectAccessObserved(
      {{AccessScope::kWindow, AccessMethod::kAddModule, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForAddModule(
            https_server()->GetURL(
                "a.test", "/shared_storage/erroneous_function_module.js"),
            /*worklet_ordinal=*/0, GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kWindow, AccessMethod::kRun, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForRunForTesting(
            "test-operation", /*operation_id=*/0, /*keep_alive=*/false,
            SharedStorageEventParams::PrivateAggregationConfigWrapper(),
            blink::CloneableMessage(), GetFirstWorkletHostDevToolsToken())}});

  ExpectOperationFinishedInfosObserved(
      {{base::TimeDelta(), AccessMethod::kRun, /*operation_id=*/0,
        GetFirstWorkletHostDevToolsToken(), MainFrameId(), origin_str}});
}

IN_PROC_BROWSER_TEST_P(
    SharedStorageBrowserTest,
    RunOperation_SecondRunOperationAfterKeepAliveTrueRun_Success) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

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

  // There is 1 more "worklet operation": `run()`.
  test_runtime_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.run(
          'test-operation', {data: {'customKey': 'customValue'},
                             keepAlive: true});
    )"));

  EXPECT_EQ(1u, test_runtime_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_runtime_manager().GetKeepAliveWorkletHostsCount());

  test_runtime_manager().GetAttachedWorkletHost()->WaitForWorkletResponses();

  EXPECT_EQ(5u, console_observer.messages().size());
  EXPECT_EQ("Start executing 'test-operation'",
            base::UTF16ToUTF8(console_observer.messages()[2].message));
  EXPECT_EQ("{\"customKey\":\"customValue\"}",
            base::UTF16ToUTF8(console_observer.messages()[3].message));
  EXPECT_EQ("Finish executing 'test-operation'",
            base::UTF16ToUTF8(console_observer.messages()[4].message));

  // There is 1 more "worklet operation": `run()`.
  test_runtime_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EXPECT_EQ(1u, test_runtime_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_runtime_manager().GetKeepAliveWorkletHostsCount());

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.run(
          'test-operation', {data: {'customKey': 'customValue'}});
    )"));

  test_runtime_manager().GetAttachedWorkletHost()->WaitForWorkletResponses();

  EXPECT_EQ(8u, console_observer.messages().size());
  EXPECT_EQ("Start executing 'test-operation'",
            base::UTF16ToUTF8(console_observer.messages()[5].message));
  EXPECT_EQ("{\"customKey\":\"customValue\"}",
            base::UTF16ToUTF8(console_observer.messages()[6].message));
  EXPECT_EQ("Finish executing 'test-operation'",
            base::UTF16ToUTF8(console_observer.messages()[7].message));

  WaitForHistograms({kTimingRunExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingRunExecutedInWorkletHistogram, 2);

  std::string origin_str = url::Origin::Create(url).Serialize();
  ExpectAccessObserved(
      {{AccessScope::kWindow, AccessMethod::kAddModule, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForAddModule(
            https_server()->GetURL("a.test",
                                   "/shared_storage/simple_module.js"),
            /*worklet_ordinal=*/0, GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kWindow, AccessMethod::kRun, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForRunForTesting(
            "test-operation", /*operation_id=*/0, /*keep_alive=*/true,
            SharedStorageEventParams::PrivateAggregationConfigWrapper(),
            blink::CloneableMessage(), GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kWindow, AccessMethod::kRun, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForRunForTesting(
            "test-operation", /*operation_id=*/1, /*keep_alive=*/false,
            SharedStorageEventParams::PrivateAggregationConfigWrapper(),
            blink::CloneableMessage(), GetFirstWorkletHostDevToolsToken())}});

  ExpectOperationFinishedInfosObserved(
      {{base::TimeDelta(), AccessMethod::kRun, /*operation_id=*/0,
        GetFirstWorkletHostDevToolsToken(), MainFrameId(), origin_str},
       {base::TimeDelta(), AccessMethod::kRun, /*operation_id=*/1,
        GetFirstWorkletHostDevToolsToken(), MainFrameId(), origin_str}});
}

IN_PROC_BROWSER_TEST_P(
    SharedStorageBrowserTest,
    RunOperation_SecondRunOperationAfterKeepAliveFalseRun_Failure) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

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

  // There is 1 more "worklet operation": `run()`.
  test_runtime_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.run(
          'test-operation', {data: {'customKey': 'customValue'},
                             keepAlive: false});
    )"));

  EXPECT_EQ(1u, test_runtime_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_runtime_manager().GetKeepAliveWorkletHostsCount());

  test_runtime_manager().GetAttachedWorkletHost()->WaitForWorkletResponses();

  EXPECT_EQ(0u, test_runtime_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_runtime_manager().GetKeepAliveWorkletHostsCount());

  EXPECT_EQ(5u, console_observer.messages().size());
  EXPECT_EQ("Start executing 'test-operation'",
            base::UTF16ToUTF8(console_observer.messages()[2].message));
  EXPECT_EQ("{\"customKey\":\"customValue\"}",
            base::UTF16ToUTF8(console_observer.messages()[3].message));
  EXPECT_EQ("Finish executing 'test-operation'",
            base::UTF16ToUTF8(console_observer.messages()[4].message));

  EXPECT_THAT(EvalJs(shell(), R"(
      sharedStorage.run(
          'test-operation', {data: {'customKey': 'customValue'}});
    )"),
              EvalJsResult::ErrorIs(
                  testing::HasSubstr(kSharedStorageWorkletExpiredMessage)));

  WaitForHistograms({kTimingRunExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingRunExecutedInWorkletHistogram, 1);

  std::string origin_str = url::Origin::Create(url).Serialize();
  ExpectAccessObserved(
      {{AccessScope::kWindow, AccessMethod::kAddModule, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForAddModule(
            https_server()->GetURL("a.test",
                                   "/shared_storage/simple_module.js"),
            /*worklet_ordinal=*/0, GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kWindow, AccessMethod::kRun, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForRunForTesting(
            "test-operation", /*operation_id=*/0, /*keep_alive=*/false,
            SharedStorageEventParams::PrivateAggregationConfigWrapper(),
            blink::CloneableMessage(), GetFirstWorkletHostDevToolsToken())}});
}

IN_PROC_BROWSER_TEST_P(
    SharedStorageBrowserTest,
    RunOperation_SecondRunOperationAfterKeepAliveDefaultRun_Failure) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

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

  // There is 1 more "worklet operation": `run()`.
  test_runtime_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.run(
          'test-operation', {data: {'customKey': 'customValue'}});
    )"));

  EXPECT_EQ(1u, test_runtime_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_runtime_manager().GetKeepAliveWorkletHostsCount());

  test_runtime_manager().GetAttachedWorkletHost()->WaitForWorkletResponses();

  EXPECT_EQ(0u, test_runtime_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_runtime_manager().GetKeepAliveWorkletHostsCount());

  EXPECT_EQ(5u, console_observer.messages().size());
  EXPECT_EQ("Start executing 'test-operation'",
            base::UTF16ToUTF8(console_observer.messages()[2].message));
  EXPECT_EQ("{\"customKey\":\"customValue\"}",
            base::UTF16ToUTF8(console_observer.messages()[3].message));
  EXPECT_EQ("Finish executing 'test-operation'",
            base::UTF16ToUTF8(console_observer.messages()[4].message));

  EXPECT_THAT(EvalJs(shell(), R"(
      sharedStorage.run(
          'test-operation', {data: {'customKey': 'customValue'}});
    )"),
              EvalJsResult::ErrorIs(
                  testing::HasSubstr(kSharedStorageWorkletExpiredMessage)));

  WaitForHistograms({kTimingRunExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingRunExecutedInWorkletHistogram, 1);

  std::string origin_str = url::Origin::Create(url).Serialize();
  ExpectAccessObserved(
      {{AccessScope::kWindow, AccessMethod::kAddModule, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForAddModule(
            https_server()->GetURL("a.test",
                                   "/shared_storage/simple_module.js"),
            /*worklet_ordinal=*/0, GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kWindow, AccessMethod::kRun, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForRunForTesting(
            "test-operation", /*operation_id=*/0, /*keep_alive=*/false,
            SharedStorageEventParams::PrivateAggregationConfigWrapper(),
            blink::CloneableMessage(), GetFirstWorkletHostDevToolsToken())}});
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest, WorkletDestroyed) {
  // The test assumes pages get deleted after navigation. To ensure this,
  // disable back/forward cache.
  content::DisableBackForwardCacheForTesting(
      shell()->web_contents(),
      content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));
  GlobalRenderFrameHostId initial_main_frame_id = MainFrameId();

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  EXPECT_EQ(1u, test_runtime_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_runtime_manager().GetKeepAliveWorkletHostsCount());

  EXPECT_TRUE(
      NavigateToUrlMaybeWaitForRfhDeleted(shell(), GURL(url::kAboutBlankURL)));

  EXPECT_EQ(0u, test_runtime_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_runtime_manager().GetKeepAliveWorkletHostsCount());

  WaitForHistograms(
      {kDestroyedStatusHistogram, kTimingUsefulResourceHistogram});

  histogram_tester_.ExpectUniqueSample(
      kDestroyedStatusHistogram,
      blink::SharedStorageWorkletDestroyedStatus::kDidNotEnterKeepAlive, 1);
  histogram_tester_.ExpectTotalCount(kTimingUsefulResourceHistogram, 1);

  ExpectAccessObserved(
      {{AccessScope::kWindow, AccessMethod::kAddModule, initial_main_frame_id,
        url::Origin::Create(url).Serialize(),
        SharedStorageEventParams::CreateForAddModule(
            https_server()->GetURL("a.test",
                                   "/shared_storage/simple_module.js"),
            /*worklet_ordinal=*/0, GetFirstWorkletHostDevToolsToken())}});
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest, TwoWorklets) {
  // The test assumes pages get deleted after navigation. To ensure this,
  // disable back/forward cache.
  content::DisableBackForwardCacheForTesting(
      shell()->web_contents(),
      content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  GURL url = https_server()->GetURL("a.test", kPageWithBlankIframePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));
  GlobalRenderFrameHostId initial_main_frame_id = MainFrameId();

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  RenderFrameHost* iframe =
      PrimaryFrameTreeNodeRoot()->child_at(0)->current_frame_host();

  EXPECT_TRUE(ExecJs(iframe, R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module2.js');
    )"));

  EXPECT_EQ(1u, test_runtime_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_runtime_manager().GetKeepAliveWorkletHostsCount());

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  EXPECT_EQ(2u, test_runtime_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_runtime_manager().GetKeepAliveWorkletHostsCount());

  NavigateIframeToURL(shell()->web_contents(), "test_iframe",
                      GURL(url::kAboutBlankURL));

  EXPECT_EQ(1u, test_runtime_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_runtime_manager().GetKeepAliveWorkletHostsCount());

  EXPECT_EQ(3u, console_observer.messages().size());
  EXPECT_EQ("Executing simple_module2.js",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("Start executing simple_module.js",
            base::UTF16ToUTF8(console_observer.messages()[1].message));
  EXPECT_EQ("Finish executing simple_module.js",
            base::UTF16ToUTF8(console_observer.messages()[2].message));

  // Navigate again to record histograms.
  EXPECT_TRUE(
      NavigateToUrlMaybeWaitForRfhDeleted(shell(), GURL(url::kAboutBlankURL)));
  WaitForHistograms(
      {kDestroyedStatusHistogram, kTimingUsefulResourceHistogram});

  histogram_tester_.ExpectUniqueSample(
      kDestroyedStatusHistogram,
      blink::SharedStorageWorkletDestroyedStatus::kDidNotEnterKeepAlive, 2);
  histogram_tester_.ExpectTotalCount(kTimingUsefulResourceHistogram, 2);

  std::map<int, base::UnguessableToken>& cached_worklet_devtools_tokens =
      GetCachedWorkletHostDevToolsTokens();
  EXPECT_EQ(cached_worklet_devtools_tokens.size(), 2u);

  std::string origin_str = url::Origin::Create(url).Serialize();
  ExpectAccessObserved(
      {{AccessScope::kWindow, AccessMethod::kAddModule, initial_main_frame_id,
        origin_str,
        SharedStorageEventParams::CreateForAddModule(
            https_server()->GetURL("a.test",
                                   "/shared_storage/simple_module2.js"),
            /*worklet_ordinal=*/0, cached_worklet_devtools_tokens[0])},
       {AccessScope::kWindow, AccessMethod::kAddModule, initial_main_frame_id,
        origin_str,
        SharedStorageEventParams::CreateForAddModule(
            https_server()->GetURL("a.test",
                                   "/shared_storage/simple_module.js"),
            /*worklet_ordinal=*/1, cached_worklet_devtools_tokens[1])}});
}

IN_PROC_BROWSER_TEST_P(
    SharedStorageBrowserTest,
    KeepAlive_StartBeforeAddModuleComplete_EndAfterAddModuleComplete) {
  // The test assumes pages get deleted after navigation. To ensure this,
  // disable back/forward cache.
  content::DisableBackForwardCacheForTesting(
      shell()->web_contents(),
      content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));
  GlobalRenderFrameHostId initial_main_frame_id = MainFrameId();

  test_runtime_manager()
      .ConfigureShouldDeferWorkletMessagesOnWorkletHostCreation(true);

  WebContentsConsoleObserver console_observer(shell()->web_contents());
  EvalJsResult result = EvalJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )",
                               EXECUTE_SCRIPT_NO_RESOLVE_PROMISES);

  // Navigate to trigger keep-alive
  EXPECT_TRUE(
      NavigateToUrlMaybeWaitForRfhDeleted(shell(), GURL(url::kAboutBlankURL)));

  EXPECT_EQ(0u, test_runtime_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(1u, test_runtime_manager().GetKeepAliveWorkletHostsCount());

  test_runtime_manager().GetKeepAliveWorkletHost()->WaitForAddModule();

  // Three pending messages are expected: two for console.log and one for
  // `addModule()` response.
  EXPECT_EQ(3u, test_runtime_manager()
                    .GetKeepAliveWorkletHost()
                    ->pending_worklet_messages()
                    .size());

  // Execute all the deferred messages. This will terminate the keep-alive.
  test_runtime_manager()
      .GetKeepAliveWorkletHost()
      ->ExecutePendingWorkletMessages();

  EXPECT_EQ(0u, test_runtime_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_runtime_manager().GetKeepAliveWorkletHostsCount());

  // Expect no console logging, as messages logged during keep-alive are
  // dropped.
  EXPECT_EQ(0u, console_observer.messages().size());

  WaitForHistograms({kDestroyedStatusHistogram, kTimingUsefulResourceHistogram,
                     kTimingKeepAliveDurationHistogram});

  histogram_tester_.ExpectUniqueSample(
      kDestroyedStatusHistogram,
      blink::SharedStorageWorkletDestroyedStatus::
          kKeepAliveEndedDueToOperationsFinished,
      1);
  histogram_tester_.ExpectTotalCount(kTimingKeepAliveDurationHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTimingUsefulResourceHistogram, 1);

  ExpectAccessObserved(
      {{AccessScope::kWindow, AccessMethod::kAddModule, initial_main_frame_id,
        url::Origin::Create(url).Serialize(),
        SharedStorageEventParams::CreateForAddModule(
            https_server()->GetURL("a.test",
                                   "/shared_storage/simple_module.js"),
            /*worklet_ordinal=*/0, GetFirstWorkletHostDevToolsToken())}});
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest,
                       KeepAlive_StartBeforeAddModuleComplete_EndAfterTimeout) {
  // The test assumes pages get deleted after navigation. To ensure this,
  // disable back/forward cache.
  content::DisableBackForwardCacheForTesting(
      shell()->web_contents(),
      content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));
  GlobalRenderFrameHostId initial_main_frame_id = MainFrameId();

  test_runtime_manager()
      .ConfigureShouldDeferWorkletMessagesOnWorkletHostCreation(true);

  WebContentsConsoleObserver console_observer(shell()->web_contents());
  EvalJsResult result = EvalJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )",
                               EXECUTE_SCRIPT_NO_RESOLVE_PROMISES);

  // Navigate to trigger keep-alive
  EXPECT_TRUE(
      NavigateToUrlMaybeWaitForRfhDeleted(shell(), GURL(url::kAboutBlankURL)));

  EXPECT_EQ(0u, test_runtime_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(1u, test_runtime_manager().GetKeepAliveWorkletHostsCount());

  test_runtime_manager().GetKeepAliveWorkletHost()->WaitForAddModule();

  // Three pending messages are expected: two for console.log and one for
  // `addModule()` response.
  EXPECT_EQ(3u, test_runtime_manager()
                    .GetKeepAliveWorkletHost()
                    ->pending_worklet_messages()
                    .size());

  // Fire the keep-alive timer. This will terminate the keep-alive.
  test_runtime_manager().GetKeepAliveWorkletHost()->FireKeepAliveTimerNow();

  EXPECT_EQ(0u, test_runtime_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_runtime_manager().GetKeepAliveWorkletHostsCount());

  WaitForHistograms(
      {kDestroyedStatusHistogram, kTimingUsefulResourceHistogram});

  histogram_tester_.ExpectUniqueSample(
      kDestroyedStatusHistogram,
      blink::SharedStorageWorkletDestroyedStatus::kKeepAliveEndedDueToTimeout,
      1);
  histogram_tester_.ExpectTotalCount(kTimingKeepAliveDurationHistogram, 0);
  histogram_tester_.ExpectUniqueSample(kTimingUsefulResourceHistogram, 100, 1);

  ExpectAccessObserved(
      {{AccessScope::kWindow, AccessMethod::kAddModule, initial_main_frame_id,
        url::Origin::Create(url).Serialize(),
        SharedStorageEventParams::CreateForAddModule(
            https_server()->GetURL("a.test",
                                   "/shared_storage/simple_module.js"),
            /*worklet_ordinal=*/0, GetFirstWorkletHostDevToolsToken())}});
}

IN_PROC_BROWSER_TEST_P(
    SharedStorageBrowserTest,
    KeepAlive_StartBeforeRunOperationComplete_EndAfterRunOperationComplete) {
  // The test assumes pages get deleted after navigation. To ensure this,
  // disable back/forward cache.
  content::DisableBackForwardCacheForTesting(
      shell()->web_contents(),
      content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));
  GlobalRenderFrameHostId initial_main_frame_id = MainFrameId();

  WebContentsConsoleObserver console_observer(shell()->web_contents());
  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  EXPECT_EQ(2u, console_observer.messages().size());

  // Configure the worklet host to defer processing the subsequent `run()`
  // response.
  test_runtime_manager()
      .GetAttachedWorkletHost()
      ->set_should_defer_worklet_messages(true);

  // There is 1 more "worklet operation": `run()`.
  test_runtime_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.run(
          'test-operation', {data: {'customKey': 'customValue'}})
    )"));

  // Navigate to trigger keep-alive
  EXPECT_TRUE(
      NavigateToUrlMaybeWaitForRfhDeleted(shell(), GURL(url::kAboutBlankURL)));

  EXPECT_EQ(0u, test_runtime_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(1u, test_runtime_manager().GetKeepAliveWorkletHostsCount());

  test_runtime_manager().GetKeepAliveWorkletHost()->WaitForWorkletResponses();

  // Four pending messages are expected: three for console.log and one for
  // `run()` response.
  EXPECT_EQ(4u, test_runtime_manager()
                    .GetKeepAliveWorkletHost()
                    ->pending_worklet_messages()
                    .size());

  // Execute all the deferred messages. This will terminate the keep-alive.
  test_runtime_manager()
      .GetKeepAliveWorkletHost()
      ->ExecutePendingWorkletMessages();

  EXPECT_EQ(0u, test_runtime_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_runtime_manager().GetKeepAliveWorkletHostsCount());

  // Expect no more console logging, as messages logged during keep-alive was
  // dropped.
  EXPECT_EQ(2u, console_observer.messages().size());

  WaitForHistograms({kDestroyedStatusHistogram, kTimingUsefulResourceHistogram,
                     kTimingKeepAliveDurationHistogram,
                     kTimingRunExecutedInWorkletHistogram});

  histogram_tester_.ExpectUniqueSample(
      kDestroyedStatusHistogram,
      blink::SharedStorageWorkletDestroyedStatus::
          kKeepAliveEndedDueToOperationsFinished,
      1);
  histogram_tester_.ExpectTotalCount(kTimingKeepAliveDurationHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTimingUsefulResourceHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTimingRunExecutedInWorkletHistogram, 1);

  std::string origin_str = url::Origin::Create(url).Serialize();
  ExpectAccessObserved(
      {{AccessScope::kWindow, AccessMethod::kAddModule, initial_main_frame_id,
        origin_str,
        SharedStorageEventParams::CreateForAddModule(
            https_server()->GetURL("a.test",
                                   "/shared_storage/simple_module.js"),
            /*worklet_ordinal=*/0, GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kWindow, AccessMethod::kRun, initial_main_frame_id,
        origin_str,
        SharedStorageEventParams::CreateForRunForTesting(
            "test-operation", /*operation_id=*/0, /*keep_alive=*/false,
            SharedStorageEventParams::PrivateAggregationConfigWrapper(),
            blink::CloneableMessage(), GetFirstWorkletHostDevToolsToken())}});

  ExpectOperationFinishedInfosObserved(
      {{base::TimeDelta(), AccessMethod::kRun, /*operation_id=*/0,
        GetFirstWorkletHostDevToolsToken(),
        /*main_frame_id=*/GlobalRenderFrameHostId(), origin_str}});
}

IN_PROC_BROWSER_TEST_P(
    SharedStorageBrowserTest,
    KeepAlive_StartBeforeSelectURLComplete_EndAfterSelectURLComplete) {
  // The test assumes pages get deleted after navigation. To ensure this,
  // disable back/forward cache.
  content::DisableBackForwardCacheForTesting(
      shell()->web_contents(),
      content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));
  GlobalRenderFrameHostId initial_main_frame_id = MainFrameId();

  WebContentsConsoleObserver console_observer(shell()->web_contents());
  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  EXPECT_EQ(2u, console_observer.messages().size());

  EXPECT_TRUE(ExecJs(shell(), JsReplace("window.resolveSelectURLToConfig = $1;",
                                        ResolveSelectURLToConfig())));

  // Configure the worklet host to defer processing the subsequent `selectURL()`
  // response.
  test_runtime_manager()
      .GetAttachedWorkletHost()
      ->set_should_defer_worklet_messages(true);

  // There is 1 more "worklet operation": `selectURL()`.
  test_runtime_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EvalJsResult result = EvalJs(shell(), R"(
      (async function() {
        window.select_url_result = await sharedStorage.selectURL(
          'test-url-selection-operation',
          [
            {url: 'fenced_frames/title0.html'},
            {url: 'fenced_frames/title1.html'}
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

  EXPECT_TRUE(result.is_ok());

  // Navigate to trigger keep-alive
  EXPECT_TRUE(
      NavigateToUrlMaybeWaitForRfhDeleted(shell(), GURL(url::kAboutBlankURL)));

  EXPECT_EQ(0u, test_runtime_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(1u, test_runtime_manager().GetKeepAliveWorkletHostsCount());

  test_runtime_manager().GetKeepAliveWorkletHost()->WaitForWorkletResponses();

  // Four pending messages are expected: four for console.log and one for
  // `selectURL()` response.
  EXPECT_EQ(5u, test_runtime_manager()
                    .GetKeepAliveWorkletHost()
                    ->pending_worklet_messages()
                    .size());

  // Execute all the deferred messages. This will terminate the keep-alive.
  test_runtime_manager()
      .GetKeepAliveWorkletHost()
      ->ExecutePendingWorkletMessages();

  EXPECT_EQ(0u, test_runtime_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_runtime_manager().GetKeepAliveWorkletHostsCount());

  // Expect no more console logging, as messages logged during keep-alive was
  // dropped.
  EXPECT_EQ(2u, console_observer.messages().size());

  WaitForHistograms({kDestroyedStatusHistogram, kTimingUsefulResourceHistogram,
                     kTimingKeepAliveDurationHistogram,
                     kTimingSelectUrlExecutedInWorkletHistogram,
                     kErrorTypeHistogram});

  histogram_tester_.ExpectBucketCount(
      kErrorTypeHistogram, blink::SharedStorageWorkletErrorType::kSuccess, 1);

  // Since we have navigated away from the page that called `selectURL()`, we
  // can't do anything with the resulting Fenced Frame config. So we log this
  // outcome as a web-visible "error".
  histogram_tester_.ExpectBucketCount(
      kErrorTypeHistogram,
      blink::SharedStorageWorkletErrorType::kSelectURLWebVisible, 1);

  histogram_tester_.ExpectUniqueSample(
      kDestroyedStatusHistogram,
      blink::SharedStorageWorkletDestroyedStatus::
          kKeepAliveEndedDueToOperationsFinished,
      1);
  histogram_tester_.ExpectTotalCount(kTimingKeepAliveDurationHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTimingUsefulResourceHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     1);

  ASSERT_EQ(urn_uuids_observed().size(), 1u);

  std::string origin_str = url::Origin::Create(url).Serialize();
  ExpectAccessObserved(
      {{AccessScope::kWindow, AccessMethod::kAddModule, initial_main_frame_id,
        origin_str,
        SharedStorageEventParams::CreateForAddModule(
            https_server()->GetURL("a.test",
                                   "/shared_storage/simple_module.js"),
            /*worklet_ordinal=*/0, GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kWindow, AccessMethod::kSelectURL, initial_main_frame_id,
        origin_str,
        SharedStorageEventParams::CreateForSelectURLForTesting(
            "test-url-selection-operation", /*operation_id=*/0,
            /*keep_alive=*/false,
            SharedStorageEventParams::PrivateAggregationConfigWrapper(),
            blink::CloneableMessage(),
            std::vector<SharedStorageUrlSpecWithMetadata>(
                {{https_server()->GetURL("a.test",
                                         "/fenced_frames/title0.html"),
                  {}},
                 {https_server()->GetURL("a.test",
                                         "/fenced_frames/title1.html"),
                  {}}}),
            ResolveSelectURLToConfig(),
            /*saved_query=*/std::string(), urn_uuids_observed()[0],
            GetFirstWorkletHostDevToolsToken())}});

  ExpectOperationFinishedInfosObserved(
      {{base::TimeDelta(), AccessMethod::kSelectURL, /*operation_id=*/0,
        GetFirstWorkletHostDevToolsToken(),
        /*main_frame_id=*/GlobalRenderFrameHostId(), origin_str}});
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest, KeepAlive_SubframeWorklet) {
  // The test assumes pages get deleted after navigation. To ensure this,
  // disable back/forward cache.
  content::DisableBackForwardCacheForTesting(
      shell()->web_contents(),
      content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  GURL url = https_server()->GetURL("a.test", kPageWithBlankIframePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));
  GlobalRenderFrameHostId initial_main_frame_id = MainFrameId();

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  // Configure the worklet host for the subframe to defer worklet responses.
  test_runtime_manager()
      .ConfigureShouldDeferWorkletMessagesOnWorkletHostCreation(true);

  RenderFrameHost* iframe =
      PrimaryFrameTreeNodeRoot()->child_at(0)->current_frame_host();

  EvalJsResult result = EvalJs(iframe, R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )",
                               EXECUTE_SCRIPT_NO_RESOLVE_PROMISES);

  // Navigate away to let the subframe's worklet enter keep-alive.
  NavigateIframeToURL(shell()->web_contents(), "test_iframe",
                      GURL(url::kAboutBlankURL));

  EXPECT_EQ(0u, test_runtime_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(1u, test_runtime_manager().GetKeepAliveWorkletHostsCount());

  // Ensure that the response is deferred.
  test_runtime_manager().GetKeepAliveWorkletHost()->WaitForAddModule();

  // Three pending messages are expected: two for console.log and one for
  // `addModule()` response.
  EXPECT_EQ(3u, test_runtime_manager()
                    .GetKeepAliveWorkletHost()
                    ->pending_worklet_messages()
                    .size());

  // Configure the worklet host for the main frame to handle worklet responses
  // directly.
  test_runtime_manager()
      .ConfigureShouldDeferWorkletMessagesOnWorkletHostCreation(false);

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module2.js');
    )"));

  EXPECT_EQ(1u, test_runtime_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(1u, test_runtime_manager().GetKeepAliveWorkletHostsCount());

  // Execute all the deferred messages. This will terminate the keep-alive.
  test_runtime_manager()
      .GetKeepAliveWorkletHost()
      ->ExecutePendingWorkletMessages();

  EXPECT_EQ(1u, test_runtime_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_runtime_manager().GetKeepAliveWorkletHostsCount());

  // Expect loggings only from executing top document's worklet.
  EXPECT_EQ(1u, console_observer.messages().size());
  EXPECT_EQ("Executing simple_module2.js",
            base::UTF16ToUTF8(console_observer.messages()[0].message));

  // Navigate again to record histograms.
  EXPECT_TRUE(
      NavigateToUrlMaybeWaitForRfhDeleted(shell(), GURL(url::kAboutBlankURL)));
  WaitForHistograms({kDestroyedStatusHistogram, kTimingUsefulResourceHistogram,
                     kTimingKeepAliveDurationHistogram});

  histogram_tester_.ExpectBucketCount(
      kDestroyedStatusHistogram,
      blink::SharedStorageWorkletDestroyedStatus::
          kKeepAliveEndedDueToOperationsFinished,
      1);
  histogram_tester_.ExpectBucketCount(
      kDestroyedStatusHistogram,
      blink::SharedStorageWorkletDestroyedStatus::kDidNotEnterKeepAlive, 1);
  histogram_tester_.ExpectTotalCount(kTimingKeepAliveDurationHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTimingUsefulResourceHistogram, 2);

  std::map<int, base::UnguessableToken>& cached_worklet_devtools_tokens =
      GetCachedWorkletHostDevToolsTokens();
  EXPECT_EQ(cached_worklet_devtools_tokens.size(), 2u);

  std::string origin_str = url::Origin::Create(url).Serialize();
  ExpectAccessObserved(
      {{AccessScope::kWindow, AccessMethod::kAddModule, initial_main_frame_id,
        origin_str,
        SharedStorageEventParams::CreateForAddModule(
            https_server()->GetURL("a.test",
                                   "/shared_storage/simple_module.js"),
            /*worklet_ordinal=*/0, cached_worklet_devtools_tokens[0])},
       {AccessScope::kWindow, AccessMethod::kAddModule, initial_main_frame_id,
        origin_str,
        SharedStorageEventParams::CreateForAddModule(
            https_server()->GetURL("a.test",
                                   "/shared_storage/simple_module2.js"),
            /*worklet_ordinal=*/1, cached_worklet_devtools_tokens[1])}});
}

IN_PROC_BROWSER_TEST_P(
    SharedStorageBrowserTest,
    RenderProcessHostDestroyedDuringWorkletKeepAlive_SameOrigin) {
  // The test assumes pages gets deleted after navigation, letting the worklet
  // enter keep-alive phase. To ensure this, disable back/forward cache.
  content::DisableBackForwardCacheForTesting(
      shell()->web_contents(),
      content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  test_runtime_manager()
      .ConfigureShouldDeferWorkletMessagesOnWorkletHostCreation(true);

  EvalJsResult result = EvalJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )",
                               EXECUTE_SCRIPT_NO_RESOLVE_PROMISES);

  // Navigate to trigger keep-alive
  EXPECT_TRUE(NavigateToUrlMaybeWaitForRfhDeleted(
      shell(), https_server()->GetURL("c.test", kSimplePagePath)));

  EXPECT_EQ(0u, test_runtime_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(1u, test_runtime_manager().GetKeepAliveWorkletHostsCount());

  // The BrowserContext will be destroyed right after this test body, which will
  // cause the RenderProcessHost to be destroyed before the keep-alive
  // SharedStorageWorkletHost. Expect no fatal error.
}

IN_PROC_BROWSER_TEST_P(
    SharedStorageBrowserTest,
    RenderProcessHostDestroyedDuringWorkletKeepAlive_CrossOrigin) {
  // The test assumes pages gets deleted after navigation, letting the worklet
  // enter keep-alive phase. To ensure this, disable back/forward cache.
  content::DisableBackForwardCacheForTesting(
      shell()->web_contents(),
      content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  test_runtime_manager()
      .ConfigureShouldDeferWorkletMessagesOnWorkletHostCreation(true);

  GURL module_script_url = https_server()->GetURL(
      "b.test", net::test_server::GetFilePathWithReplacements(
                    "/shared_storage/module_with_custom_header.js",
                    SharedStorageCrossOriginWorkletResponseHeaderReplacement(
                        "Access-Control-Allow-Origin: *",
                        "Shared-Storage-Cross-Origin-Worklet-Allowed: ?1")));

  EvalJsResult result = EvalJs(
      shell(),
      JsReplace("sharedStorage.createWorklet($1)", module_script_url.spec()),
      EXECUTE_SCRIPT_NO_RESOLVE_PROMISES);

  // Navigate to trigger keep-alive
  EXPECT_TRUE(NavigateToUrlMaybeWaitForRfhDeleted(
      shell(), https_server()->GetURL("c.test", kSimplePagePath)));

  EXPECT_EQ(0u, test_runtime_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(1u, test_runtime_manager().GetKeepAliveWorkletHostsCount());

  // The BrowserContext will be destroyed right after this test body, which will
  // cause the RenderProcessHost to be destroyed before the keep-alive
  // SharedStorageWorkletHost. Expect no fatal error.
}

// Test that there's no need to charge budget if the input urls' size is 1.
// This specifically tests the operation success scenario.
IN_PROC_BROWSER_TEST_P(
    SharedStorageBrowserTest,
    SelectURL_BudgetMetadata_OperationSuccess_SingleInputURL) {
  metrics::dwa::DwaRecorder::Get()->EnableRecording();
  metrics::dwa::DwaRecorder::Get()->Purge();
  ASSERT_THAT(metrics::dwa::DwaRecorder::Get()->GetEntriesForTesting(),
              testing::IsEmpty());

  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

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
              url: "fenced_frames/title0.html",
              reportingMetadata: {
                "click": "fenced_frames/report1.html",
                "mouse interaction": "fenced_frames/report2.html"
              }
            }
          ],
          {
            data: {'mockResult': 0},
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

  EXPECT_TRUE(result.is_ok());
  const std::optional<GURL>& observed_urn_uuid = config_observer.GetUrnUuid();
  EXPECT_TRUE(observed_urn_uuid.has_value());
  EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid.value()));

  if (!ResolveSelectURLToConfig()) {
    EXPECT_EQ(result.ExtractString(), observed_urn_uuid->spec());
  }

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
  EXPECT_DOUBLE_EQ(metadata->budget_to_charge, 0.0);

  EXPECT_THAT(GetSharedStorageReportingMap(observed_urn_uuid.value()),
              UnorderedElementsAre(
                  Pair("click", https_server()->GetURL(
                                    "a.test", "/fenced_frames/report1.html")),
                  Pair("mouse interaction",
                       https_server()->GetURL("a.test",
                                              "/fenced_frames/report2.html"))));

  EXPECT_EQ("Finish executing 'test-url-selection-operation'",
            base::UTF16ToUTF8(console_observer.messages().back().message));

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram,
                     kSelectUrlBudgetStatusHistogram});
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     1);
  histogram_tester_.ExpectUniqueSample(
      kSelectUrlBudgetStatusHistogram,
      blink::SharedStorageSelectUrlBudgetStatus::kSufficientBudget, 1);

  const auto& entries =
      metrics::dwa::DwaRecorder::Get()->GetEntriesForTesting();
  EXPECT_EQ(entries.size(), 1u);
  EXPECT_THAT(entries[0]->event_hash,
              base::HashMetricName("SharedStorage.SelectUrlFinishedInWorklet"));
  EXPECT_THAT(
      entries[0]->content_hash,
      base::HashMetricName(
          net::registry_controlled_domains::GetDomainAndRegistry(
              main_url,
              net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)));
  EXPECT_EQ(entries[0]->metrics.size(), 1u);

  ASSERT_EQ(urn_uuids_observed().size(), 1u);

  std::string origin_str = url::Origin::Create(main_url).Serialize();
  ExpectAccessObserved(
      {{AccessScope::kWindow, AccessMethod::kAddModule, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForAddModule(
            https_server()->GetURL("a.test",
                                   "/shared_storage/simple_module.js"),
            /*worklet_ordinal=*/0, GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kWindow, AccessMethod::kSelectURL, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForSelectURLForTesting(
            "test-url-selection-operation", /*operation_id=*/0,
            /*keep_alive=*/false,
            SharedStorageEventParams::PrivateAggregationConfigWrapper(),
            blink::CloneableMessage(),
            std::vector<SharedStorageUrlSpecWithMetadata>(
                {{https_server()->GetURL("a.test",
                                         "/fenced_frames/title0.html"),
                  {{"click",
                    https_server()
                        ->GetURL("a.test", "/fenced_frames/report1.html")
                        .spec()},
                   {"mouse interaction",
                    https_server()
                        ->GetURL("a.test", "/fenced_frames/report2.html")
                        .spec()}}}}),
            ResolveSelectURLToConfig(),
            /*saved_query=*/std::string(), urn_uuids_observed()[0],
            GetFirstWorkletHostDevToolsToken())}});
}

// Test that there's no need to charge budget if the input urls' size is 1.
// This specifically tests the operation failure scenario.
IN_PROC_BROWSER_TEST_P(
    SharedStorageBrowserTest,
    SelectURL_BudgetMetadata_OperationFailure_SingleInputURL) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

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
              url: "fenced_frames/title0.html",
              reportingMetadata: {
                "click": "fenced_frames/report1.html"
              }
            }
          ],
          {
            data: {'mockResult': -1},
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

  EXPECT_TRUE(result.is_ok());
  const std::optional<GURL>& observed_urn_uuid = config_observer.GetUrnUuid();
  EXPECT_TRUE(observed_urn_uuid.has_value());
  EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid.value()));

  if (!ResolveSelectURLToConfig()) {
    EXPECT_EQ(result.ExtractString(), observed_urn_uuid->spec());
  }

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
  EXPECT_DOUBLE_EQ(metadata->budget_to_charge, 0.0);

  EXPECT_THAT(GetSharedStorageReportingMap(observed_urn_uuid.value()),
              UnorderedElementsAre(
                  Pair("click", https_server()->GetURL(
                                    "a.test", "/fenced_frames/report1.html"))));

  EXPECT_EQ(
      "Promise resolved to a number outside the length of the input urls.",
      base::UTF16ToUTF8(console_observer.messages().back().message));

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     1);

  ASSERT_EQ(urn_uuids_observed().size(), 1u);

  std::string origin_str = url::Origin::Create(main_url).Serialize();
  ExpectAccessObserved(
      {{AccessScope::kWindow, AccessMethod::kAddModule, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForAddModule(
            https_server()->GetURL("a.test",
                                   "/shared_storage/simple_module.js"),
            /*worklet_ordinal=*/0, GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kWindow, AccessMethod::kSelectURL, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForSelectURLForTesting(
            "test-url-selection-operation", /*operation_id=*/0,
            /*keep_alive=*/false,
            SharedStorageEventParams::PrivateAggregationConfigWrapper(),
            blink::CloneableMessage(),
            std::vector<SharedStorageUrlSpecWithMetadata>(
                {{https_server()->GetURL("a.test",
                                         "/fenced_frames/title0.html"),
                  {{"click",
                    https_server()
                        ->GetURL("a.test", "/fenced_frames/report1.html")
                        .spec()}}}}),
            ResolveSelectURLToConfig(),
            /*saved_query=*/std::string(), urn_uuids_observed()[0],
            GetFirstWorkletHostDevToolsToken())}});
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest,
                       SelectURL_BudgetMetadata_Origin) {
  EXPECT_TRUE(NavigateToURL(
      shell(), https_server()->GetURL("a.test", kPageWithBlankIframePath)));

  GURL iframe_url = https_server()->GetURL("b.test", kSimplePagePath);
  NavigateIframeToURL(shell()->web_contents(), "test_iframe", iframe_url);

  RenderFrameHost* iframe =
      PrimaryFrameTreeNodeRoot()->child_at(0)->current_frame_host();

  EXPECT_TRUE(ExecJs(iframe, R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  EXPECT_EQ(1u, test_runtime_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_runtime_manager().GetKeepAliveWorkletHostsCount());

  EXPECT_TRUE(ExecJs(iframe, JsReplace("window.resolveSelectURLToConfig = $1;",
                                       ResolveSelectURLToConfig())));

  TestSelectURLFencedFrameConfigObserver config_observer(GetStoragePartition());
  // There is 1 more "worklet operation": `selectURL()`.
  test_runtime_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EvalJsResult result = EvalJs(iframe, R"(
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

  EXPECT_TRUE(result.is_ok());
  const std::optional<GURL>& observed_urn_uuid = config_observer.GetUrnUuid();
  EXPECT_TRUE(observed_urn_uuid.has_value());
  EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid.value()));

  if (!ResolveSelectURLToConfig()) {
    EXPECT_EQ(result.ExtractString(), observed_urn_uuid->spec());
  }

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
            net::SchemefulSite(https_server()->GetOrigin("b.test")));
  EXPECT_DOUBLE_EQ(metadata->budget_to_charge, std::log2(3));

  SharedStorageReportingMap reporting_map =
      GetSharedStorageReportingMap(observed_urn_uuid.value());
  EXPECT_FALSE(reporting_map.empty());
  EXPECT_EQ(1U, reporting_map.size());
  EXPECT_EQ("click", reporting_map.begin()->first);
  EXPECT_EQ(https_server()->GetURL("b.test", "/fenced_frames/report1.html"),
            reporting_map.begin()->second);

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     1);

  ASSERT_EQ(urn_uuids_observed().size(), 1u);

  std::string origin_str = url::Origin::Create(iframe_url).Serialize();
  ExpectAccessObserved(
      {{AccessScope::kWindow, AccessMethod::kAddModule, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForAddModule(
            https_server()->GetURL("b.test",
                                   "/shared_storage/simple_module.js"),
            /*worklet_ordinal=*/0, GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kWindow, AccessMethod::kSelectURL, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForSelectURLForTesting(
            "test-url-selection-operation", /*operation_id=*/0,
            /*keep_alive=*/false,
            SharedStorageEventParams::PrivateAggregationConfigWrapper(),
            blink::CloneableMessage(),
            std::vector<SharedStorageUrlSpecWithMetadata>(
                {{https_server()->GetURL("b.test",
                                         "/fenced_frames/title0.html"),
                  {}},
                 {https_server()->GetURL("b.test",
                                         "/fenced_frames/title1.html"),
                  {{"click",
                    https_server()
                        ->GetURL("b.test", "/fenced_frames/report1.html")
                        .spec()}}},
                 {https_server()->GetURL("b.test",
                                         "/fenced_frames/title2.html"),
                  {}}}),
            ResolveSelectURLToConfig(),
            /*saved_query=*/std::string(), urn_uuids_observed()[0],
            GetFirstWorkletHostDevToolsToken())}});
}

IN_PROC_BROWSER_TEST_P(
    SharedStorageBrowserTest,
    SelectURL_SecondSelectURLAfterKeepAliveTrueSelectURL_Success) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  EXPECT_TRUE(ExecJs(shell(), JsReplace("window.resolveSelectURLToConfig = $1;",
                                        ResolveSelectURLToConfig())));

  TestSelectURLFencedFrameConfigObserver config_observer1(
      GetStoragePartition());
  // There is 1 more "worklet operation": `selectURL()`.
  test_runtime_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EvalJsResult result1 = EvalJs(shell(), R"(
      (async function() {
        window.select_url_result = await sharedStorage.selectURL(
          'test-url-selection-operation',
          [
            {
              url: "fenced_frames/title0.html"
            }
          ],
          {
            data: {'mockResult': 0},
            resolveToConfig: resolveSelectURLToConfig,
            keepAlive: true
          }
        );
        if (resolveSelectURLToConfig &&
            !(select_url_result instanceof FencedFrameConfig)) {
          throw new Error('selectURL() did not return a FencedFrameConfig.');
        }
        return window.select_url_result;
      })()
    )");

  EXPECT_TRUE(result1.is_ok());
  const std::optional<GURL>& observed_urn_uuid1 = config_observer1.GetUrnUuid();
  EXPECT_TRUE(observed_urn_uuid1.has_value());
  EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid1.value()));

  if (!ResolveSelectURLToConfig()) {
    EXPECT_EQ(result1.ExtractString(), observed_urn_uuid1->spec());
  }

  test_runtime_manager().GetAttachedWorkletHost()->WaitForWorkletResponses();

  ASSERT_TRUE(config_observer1.ConfigObserved());
  const std::optional<FencedFrameConfig>& fenced_frame_config1 =
      config_observer1.GetConfig();
  EXPECT_TRUE(fenced_frame_config1.has_value());
  EXPECT_EQ(fenced_frame_config1->urn_uuid(), observed_urn_uuid1.value());

  EXPECT_EQ("Finish executing 'test-url-selection-operation'",
            base::UTF16ToUTF8(console_observer.messages().back().message));

  TestSelectURLFencedFrameConfigObserver config_observer2(
      GetStoragePartition());

  // There is 1 more "worklet operation": `selectURL()`.
  test_runtime_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EvalJsResult result2 = EvalJs(shell(), R"(
      (async function() {
        window.select_url_result = await sharedStorage.selectURL(
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
        if (resolveSelectURLToConfig &&
            !(select_url_result instanceof FencedFrameConfig)) {
          throw new Error('selectURL() did not return a FencedFrameConfig.');
        }
        return window.select_url_result;
      })()
    )");

  EXPECT_TRUE(result2.is_ok());
  const std::optional<GURL>& observed_urn_uuid2 = config_observer2.GetUrnUuid();
  EXPECT_TRUE(observed_urn_uuid2.has_value());
  EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid2.value()));

  if (!ResolveSelectURLToConfig()) {
    EXPECT_EQ(result2.ExtractString(), observed_urn_uuid2->spec());
  }

  test_runtime_manager().GetAttachedWorkletHost()->WaitForWorkletResponses();

  ASSERT_TRUE(config_observer2.ConfigObserved());
  const std::optional<FencedFrameConfig>& fenced_frame_config2 =
      config_observer2.GetConfig();
  EXPECT_TRUE(fenced_frame_config2.has_value());
  EXPECT_EQ(fenced_frame_config2->urn_uuid(), observed_urn_uuid2.value());

  EXPECT_EQ("Finish executing 'test-url-selection-operation'",
            base::UTF16ToUTF8(console_observer.messages().back().message));

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     2);

  ASSERT_EQ(urn_uuids_observed().size(), 2u);

  std::string origin_str = url::Origin::Create(main_url).Serialize();
  ExpectAccessObserved(
      {{AccessScope::kWindow, AccessMethod::kAddModule, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForAddModule(
            https_server()->GetURL("a.test",
                                   "/shared_storage/simple_module.js"),
            /*worklet_ordinal=*/0, GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kWindow, AccessMethod::kSelectURL, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForSelectURLForTesting(
            "test-url-selection-operation", /*operation_id=*/0,
            /*keep_alive=*/true,
            SharedStorageEventParams::PrivateAggregationConfigWrapper(),
            blink::CloneableMessage(),
            std::vector<SharedStorageUrlSpecWithMetadata>(
                {{https_server()->GetURL("a.test",
                                         "/fenced_frames/title0.html"),
                  {}}}),
            ResolveSelectURLToConfig(),
            /*saved_query=*/std::string(), urn_uuids_observed()[0],
            GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kWindow, AccessMethod::kSelectURL, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForSelectURLForTesting(
            "test-url-selection-operation", /*operation_id=*/1,
            /*keep_alive=*/false,
            SharedStorageEventParams::PrivateAggregationConfigWrapper(),
            blink::CloneableMessage(),
            std::vector<SharedStorageUrlSpecWithMetadata>(
                {{https_server()->GetURL("a.test",
                                         "/fenced_frames/title0.html"),
                  {}}}),
            ResolveSelectURLToConfig(),
            /*saved_query=*/std::string(), urn_uuids_observed()[1],
            GetFirstWorkletHostDevToolsToken())}});
}

IN_PROC_BROWSER_TEST_P(
    SharedStorageBrowserTest,
    SelectURL_SecondSelectURLAfterKeepAliveFalseSelectURL_Failure) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  EXPECT_TRUE(ExecJs(shell(), JsReplace("window.resolveSelectURLToConfig = $1;",
                                        ResolveSelectURLToConfig())));

  TestSelectURLFencedFrameConfigObserver config_observer1(
      GetStoragePartition());

  // There is 1 more "worklet operation": `selectURL()`.
  test_runtime_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EvalJsResult result1 = EvalJs(shell(), R"(
      (async function() {
        window.select_url_result = await sharedStorage.selectURL(
          'test-url-selection-operation',
          [
            {
              url: "fenced_frames/title0.html"
            }
          ],
          {
            data: {'mockResult': 0},
            resolveToConfig: resolveSelectURLToConfig,
            keepAlive: false
          }
        );
        if (resolveSelectURLToConfig &&
            !(select_url_result instanceof FencedFrameConfig)) {
          throw new Error('selectURL() did not return a FencedFrameConfig.');
        }
        return window.select_url_result;
      })()
    )");

  EXPECT_TRUE(result1.is_ok());
  const std::optional<GURL>& observed_urn_uuid1 = config_observer1.GetUrnUuid();
  EXPECT_TRUE(observed_urn_uuid1.has_value());
  EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid1.value()));

  if (!ResolveSelectURLToConfig()) {
    EXPECT_EQ(result1.ExtractString(), observed_urn_uuid1->spec());
  }

  test_runtime_manager().GetAttachedWorkletHost()->WaitForWorkletResponses();

  ASSERT_TRUE(config_observer1.ConfigObserved());
  const std::optional<FencedFrameConfig>& fenced_frame_config1 =
      config_observer1.GetConfig();
  EXPECT_TRUE(fenced_frame_config1.has_value());
  EXPECT_EQ(fenced_frame_config1->urn_uuid(), observed_urn_uuid1.value());

  EXPECT_EQ("Finish executing 'test-url-selection-operation'",
            base::UTF16ToUTF8(console_observer.messages().back().message));

  TestSelectURLFencedFrameConfigObserver config_observer2(
      GetStoragePartition());

  EXPECT_THAT(EvalJs(shell(), R"(
      (async function() {
        window.select_url_result = await sharedStorage.selectURL(
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
        if (resolveSelectURLToConfig &&
            !(select_url_result instanceof FencedFrameConfig)) {
          throw new Error('selectURL() did not return a FencedFrameConfig.');
        }
        return window.select_url_result;
      })()
    )"),
              EvalJsResult::ErrorIs(
                  testing::HasSubstr(kSharedStorageWorkletExpiredMessage)));

  EXPECT_FALSE(config_observer2.ConfigObserved());

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     1);

  ASSERT_EQ(urn_uuids_observed().size(), 1u);

  std::string origin_str = url::Origin::Create(main_url).Serialize();
  ExpectAccessObserved(
      {{AccessScope::kWindow, AccessMethod::kAddModule, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForAddModule(
            https_server()->GetURL("a.test",
                                   "/shared_storage/simple_module.js"),
            /*worklet_ordinal=*/0, GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kWindow, AccessMethod::kSelectURL, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForSelectURLForTesting(
            "test-url-selection-operation", /*operation_id=*/0,
            /*keep_alive=*/false,
            SharedStorageEventParams::PrivateAggregationConfigWrapper(),
            blink::CloneableMessage(),
            std::vector<SharedStorageUrlSpecWithMetadata>(
                {{https_server()->GetURL("a.test",
                                         "/fenced_frames/title0.html"),
                  {}}}),
            ResolveSelectURLToConfig(),
            /*saved_query=*/std::string(), urn_uuids_observed()[0],
            GetFirstWorkletHostDevToolsToken())}});
}

IN_PROC_BROWSER_TEST_P(
    SharedStorageBrowserTest,
    SelectURL_SecondSelectURLAfterKeepAliveDefaultSelectURL_Failure) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  EXPECT_TRUE(ExecJs(shell(), JsReplace("window.resolveSelectURLToConfig = $1;",
                                        ResolveSelectURLToConfig())));

  TestSelectURLFencedFrameConfigObserver config_observer1(
      GetStoragePartition());

  // There is 1 more "worklet operation": `selectURL()`.
  test_runtime_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  const char select_url_script[] = R"(
      (async function() {
        window.select_url_result = await sharedStorage.selectURL(
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
        if (resolveSelectURLToConfig &&
            !(select_url_result instanceof FencedFrameConfig)) {
          throw new Error('selectURL() did not return a FencedFrameConfig.');
        }
        return window.select_url_result;
      })()
    )";

  EvalJsResult result1 = EvalJs(shell(), select_url_script);

  EXPECT_TRUE(result1.is_ok());
  const std::optional<GURL>& observed_urn_uuid1 = config_observer1.GetUrnUuid();
  EXPECT_TRUE(observed_urn_uuid1.has_value());
  EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid1.value()));

  if (!ResolveSelectURLToConfig()) {
    EXPECT_EQ(result1.ExtractString(), observed_urn_uuid1->spec());
  }

  test_runtime_manager().GetAttachedWorkletHost()->WaitForWorkletResponses();

  ASSERT_TRUE(config_observer1.ConfigObserved());
  const std::optional<FencedFrameConfig>& fenced_frame_config1 =
      config_observer1.GetConfig();
  EXPECT_TRUE(fenced_frame_config1.has_value());
  EXPECT_EQ(fenced_frame_config1->urn_uuid(), observed_urn_uuid1.value());

  EXPECT_EQ("Finish executing 'test-url-selection-operation'",
            base::UTF16ToUTF8(console_observer.messages().back().message));

  TestSelectURLFencedFrameConfigObserver config_observer2(
      GetStoragePartition());

  EXPECT_THAT(EvalJs(shell(), select_url_script),
              EvalJsResult::ErrorIs(
                  testing::HasSubstr(kSharedStorageWorkletExpiredMessage)));

  EXPECT_FALSE(config_observer2.ConfigObserved());

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     1);

  ASSERT_EQ(urn_uuids_observed().size(), 1u);

  std::string origin_str = url::Origin::Create(main_url).Serialize();
  ExpectAccessObserved(
      {{AccessScope::kWindow, AccessMethod::kAddModule, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForAddModule(
            https_server()->GetURL("a.test",
                                   "/shared_storage/simple_module.js"),
            /*worklet_ordinal=*/0, GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kWindow, AccessMethod::kSelectURL, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForSelectURLForTesting(
            "test-url-selection-operation", /*operation_id=*/0,
            /*keep_alive=*/false,
            SharedStorageEventParams::PrivateAggregationConfigWrapper(),
            blink::CloneableMessage(),
            std::vector<SharedStorageUrlSpecWithMetadata>(
                {{https_server()->GetURL("a.test",
                                         "/fenced_frames/title0.html"),
                  {}}}),
            ResolveSelectURLToConfig(),
            /*saved_query=*/std::string(), urn_uuids_observed()[0],
            GetFirstWorkletHostDevToolsToken())}});
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest,
                       SelectURL_SelectURLAfterKeepAliveFalseRun_Failure) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  // There is 1 more "worklet operation": `run()`.
  test_runtime_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.run(
          'test-operation', {data: {'customKey': 'customValue'},
                             keepAlive: false});
    )"));

  test_runtime_manager().GetAttachedWorkletHost()->WaitForWorkletResponses();

  EXPECT_TRUE(ExecJs(shell(), JsReplace("window.resolveSelectURLToConfig = $1;",
                                        ResolveSelectURLToConfig())));

  TestSelectURLFencedFrameConfigObserver config_observer(GetStoragePartition());

  EXPECT_THAT(EvalJs(shell(), R"(
      (async function() {
        window.select_url_result = await sharedStorage.selectURL(
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
        if (resolveSelectURLToConfig &&
            !(select_url_result instanceof FencedFrameConfig)) {
          throw new Error('selectURL() did not return a FencedFrameConfig.');
        }
        return window.select_url_result;
      })()
    )"),
              EvalJsResult::ErrorIs(
                  testing::HasSubstr(kSharedStorageWorkletExpiredMessage)));

  EXPECT_FALSE(config_observer.ConfigObserved());

  EXPECT_EQ(0u, test_runtime_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_runtime_manager().GetKeepAliveWorkletHostsCount());
  EXPECT_EQ(5u, console_observer.messages().size());
  EXPECT_EQ("Start executing simple_module.js",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("Finish executing simple_module.js",
            base::UTF16ToUTF8(console_observer.messages()[1].message));
  EXPECT_EQ("Start executing 'test-operation'",
            base::UTF16ToUTF8(console_observer.messages()[2].message));
  EXPECT_EQ("{\"customKey\":\"customValue\"}",
            base::UTF16ToUTF8(console_observer.messages()[3].message));
  EXPECT_EQ("Finish executing 'test-operation'",
            base::UTF16ToUTF8(console_observer.messages()[4].message));

  WaitForHistograms({kTimingRunExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingRunExecutedInWorkletHistogram, 1);

  std::string origin_str = url::Origin::Create(main_url).Serialize();
  ExpectAccessObserved(
      {{AccessScope::kWindow, AccessMethod::kAddModule, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForAddModule(
            https_server()->GetURL("a.test",
                                   "/shared_storage/simple_module.js"),
            /*worklet_ordinal=*/0, GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kWindow, AccessMethod::kRun, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForRunForTesting(
            "test-operation", /*operation_id=*/0, /*keep_alive=*/false,
            SharedStorageEventParams::PrivateAggregationConfigWrapper(),
            blink::CloneableMessage(), GetFirstWorkletHostDevToolsToken())}});
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest,
                       SelectURL_SelectURLAfterKeepAliveTrueRun_Success) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  // There is 1 more "worklet operation": `run()`.
  test_runtime_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.run(
          'test-operation', {data: {'customKey': 'customValue'},
                             keepAlive: true});
    )"));

  test_runtime_manager().GetAttachedWorkletHost()->WaitForWorkletResponses();

  EXPECT_EQ(5u, console_observer.messages().size());
  EXPECT_EQ("Start executing simple_module.js",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("Finish executing simple_module.js",
            base::UTF16ToUTF8(console_observer.messages()[1].message));
  EXPECT_EQ("Start executing 'test-operation'",
            base::UTF16ToUTF8(console_observer.messages()[2].message));
  EXPECT_EQ("{\"customKey\":\"customValue\"}",
            base::UTF16ToUTF8(console_observer.messages()[3].message));
  EXPECT_EQ("Finish executing 'test-operation'",
            base::UTF16ToUTF8(console_observer.messages()[4].message));

  EXPECT_TRUE(ExecJs(shell(), JsReplace("window.resolveSelectURLToConfig = $1;",
                                        ResolveSelectURLToConfig())));

  TestSelectURLFencedFrameConfigObserver config_observer(GetStoragePartition());

  EXPECT_EQ(1u, test_runtime_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_runtime_manager().GetKeepAliveWorkletHostsCount());

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
            }
          ],
          {
            data: {'mockResult': 0},
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

  EXPECT_TRUE(result.is_ok());
  const std::optional<GURL>& observed_urn_uuid = config_observer.GetUrnUuid();
  EXPECT_TRUE(observed_urn_uuid.has_value());
  EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid.value()));

  if (!ResolveSelectURLToConfig()) {
    EXPECT_EQ(result.ExtractString(), observed_urn_uuid->spec());
  }

  test_runtime_manager().GetAttachedWorkletHost()->WaitForWorkletResponses();

  ASSERT_TRUE(config_observer.ConfigObserved());
  const std::optional<FencedFrameConfig>& fenced_frame_config =
      config_observer.GetConfig();
  EXPECT_TRUE(fenced_frame_config.has_value());
  EXPECT_EQ(fenced_frame_config->urn_uuid(), observed_urn_uuid.value());

  EXPECT_EQ("Finish executing 'test-url-selection-operation'",
            base::UTF16ToUTF8(console_observer.messages().back().message));

  EXPECT_EQ(0u, test_runtime_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_runtime_manager().GetKeepAliveWorkletHostsCount());

  WaitForHistograms({kTimingRunExecutedInWorkletHistogram,
                     kTimingSelectUrlExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingRunExecutedInWorkletHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     1);

  ASSERT_EQ(urn_uuids_observed().size(), 1u);

  std::string origin_str = url::Origin::Create(main_url).Serialize();
  ExpectAccessObserved(
      {{AccessScope::kWindow, AccessMethod::kAddModule, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForAddModule(
            https_server()->GetURL("a.test",
                                   "/shared_storage/simple_module.js"),
            /*worklet_ordinal=*/0, GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kWindow, AccessMethod::kRun, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForRunForTesting(
            "test-operation", /*operation_id=*/0, /*keep_alive=*/true,
            SharedStorageEventParams::PrivateAggregationConfigWrapper(),
            blink::CloneableMessage(), GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kWindow, AccessMethod::kSelectURL, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForSelectURLForTesting(
            "test-url-selection-operation", /*operation_id=*/1,
            /*keep_alive=*/false,
            SharedStorageEventParams::PrivateAggregationConfigWrapper(),
            blink::CloneableMessage(),
            std::vector<SharedStorageUrlSpecWithMetadata>(
                {{https_server()->GetURL("a.test",
                                         "/fenced_frames/title0.html"),
                  {}}}),
            ResolveSelectURLToConfig(),
            /*saved_query=*/std::string(), urn_uuids_observed()[0],
            GetFirstWorkletHostDevToolsToken())}});
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest,
                       SelectURL_SelectURLAfterKeepAliveDefaultRun_Failure) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  // There is 1 more "worklet operation": `run()`.
  test_runtime_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.run(
          'test-operation', {data: {'customKey': 'customValue'}});
    )"));

  test_runtime_manager().GetAttachedWorkletHost()->WaitForWorkletResponses();

  EXPECT_TRUE(ExecJs(shell(), JsReplace("window.resolveSelectURLToConfig = $1;",
                                        ResolveSelectURLToConfig())));

  TestSelectURLFencedFrameConfigObserver config_observer(GetStoragePartition());

  EXPECT_THAT(EvalJs(shell(), R"(
      (async function() {
        window.select_url_result = await sharedStorage.selectURL(
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
        if (resolveSelectURLToConfig &&
            !(select_url_result instanceof FencedFrameConfig)) {
          throw new Error('selectURL() did not return a FencedFrameConfig.');
        }
        return window.select_url_result;
      })()
    )"),
              EvalJsResult::ErrorIs(
                  testing::HasSubstr(kSharedStorageWorkletExpiredMessage)));

  EXPECT_FALSE(config_observer.ConfigObserved());

  EXPECT_EQ(0u, test_runtime_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_runtime_manager().GetKeepAliveWorkletHostsCount());
  EXPECT_EQ(5u, console_observer.messages().size());
  EXPECT_EQ("Start executing simple_module.js",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("Finish executing simple_module.js",
            base::UTF16ToUTF8(console_observer.messages()[1].message));
  EXPECT_EQ("Start executing 'test-operation'",
            base::UTF16ToUTF8(console_observer.messages()[2].message));
  EXPECT_EQ("{\"customKey\":\"customValue\"}",
            base::UTF16ToUTF8(console_observer.messages()[3].message));
  EXPECT_EQ("Finish executing 'test-operation'",
            base::UTF16ToUTF8(console_observer.messages()[4].message));

  WaitForHistograms({kTimingRunExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingRunExecutedInWorkletHistogram, 1);

  std::string origin_str = url::Origin::Create(main_url).Serialize();
  ExpectAccessObserved(
      {{AccessScope::kWindow, AccessMethod::kAddModule, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForAddModule(
            https_server()->GetURL("a.test",
                                   "/shared_storage/simple_module.js"),
            /*worklet_ordinal=*/0, GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kWindow, AccessMethod::kRun, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForRunForTesting(
            "test-operation", /*operation_id=*/0, /*keep_alive=*/false,
            SharedStorageEventParams::PrivateAggregationConfigWrapper(),
            blink::CloneableMessage(), GetFirstWorkletHostDevToolsToken())}});
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest,
                       RunOperation_RunAfterKeepAliveTrueSelectURL_Success) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("Start executing simple_module.js",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("Finish executing simple_module.js",
            base::UTF16ToUTF8(console_observer.messages()[1].message));

  EXPECT_TRUE(ExecJs(shell(), JsReplace("window.resolveSelectURLToConfig = $1;",
                                        ResolveSelectURLToConfig())));

  TestSelectURLFencedFrameConfigObserver config_observer(GetStoragePartition());

  EXPECT_EQ(1u, test_runtime_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_runtime_manager().GetKeepAliveWorkletHostsCount());

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
            }
          ],
          {
            data: {'mockResult': 0},
            resolveToConfig: resolveSelectURLToConfig,
            keepAlive: true
          }
        );
        if (resolveSelectURLToConfig &&
            !(select_url_result instanceof FencedFrameConfig)) {
          throw new Error('selectURL() did not return a FencedFrameConfig.');
        }
        return window.select_url_result;
      })()
    )");

  EXPECT_TRUE(result.is_ok());
  const std::optional<GURL>& observed_urn_uuid = config_observer.GetUrnUuid();
  EXPECT_TRUE(observed_urn_uuid.has_value());
  EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid.value()));

  if (!ResolveSelectURLToConfig()) {
    EXPECT_EQ(result.ExtractString(), observed_urn_uuid->spec());
  }

  test_runtime_manager().GetAttachedWorkletHost()->WaitForWorkletResponses();

  ASSERT_TRUE(config_observer.ConfigObserved());
  const std::optional<FencedFrameConfig>& fenced_frame_config =
      config_observer.GetConfig();
  EXPECT_TRUE(fenced_frame_config.has_value());
  EXPECT_EQ(fenced_frame_config->urn_uuid(), observed_urn_uuid.value());

  EXPECT_EQ("Finish executing 'test-url-selection-operation'",
            base::UTF16ToUTF8(console_observer.messages().back().message));

  EXPECT_EQ(1u, test_runtime_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_runtime_manager().GetKeepAliveWorkletHostsCount());

  // There is 1 more "worklet operation": `run()`.
  test_runtime_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.run(
          'test-operation', {data: {'customKey': 'customValue'}});
    )"));

  test_runtime_manager().GetAttachedWorkletHost()->WaitForWorkletResponses();

  EXPECT_EQ("Finish executing 'test-operation'",
            base::UTF16ToUTF8(console_observer.messages().back().message));

  EXPECT_EQ(0u, test_runtime_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_runtime_manager().GetKeepAliveWorkletHostsCount());

  WaitForHistograms({kTimingRunExecutedInWorkletHistogram,
                     kTimingSelectUrlExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingRunExecutedInWorkletHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     1);

  ASSERT_EQ(urn_uuids_observed().size(), 1u);

  std::string origin_str = url::Origin::Create(main_url).Serialize();
  ExpectAccessObserved(
      {{AccessScope::kWindow, AccessMethod::kAddModule, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForAddModule(
            https_server()->GetURL("a.test",
                                   "/shared_storage/simple_module.js"),
            /*worklet_ordinal=*/0, GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kWindow, AccessMethod::kSelectURL, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForSelectURLForTesting(
            "test-url-selection-operation", /*operation_id=*/0,
            /*keep_alive=*/true,
            SharedStorageEventParams::PrivateAggregationConfigWrapper(),
            blink::CloneableMessage(),
            std::vector<SharedStorageUrlSpecWithMetadata>(
                {{https_server()->GetURL("a.test",
                                         "/fenced_frames/title0.html"),
                  {}}}),
            ResolveSelectURLToConfig(),
            /*saved_query=*/std::string(), urn_uuids_observed()[0],
            GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kWindow, AccessMethod::kRun, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForRunForTesting(
            "test-operation", /*operation_id=*/1, /*keep_alive=*/false,
            SharedStorageEventParams::PrivateAggregationConfigWrapper(),
            blink::CloneableMessage(), GetFirstWorkletHostDevToolsToken())}});
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest,
                       RunOperation_RunAfterKeepAliveFalseSelectURL_Failure) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("Start executing simple_module.js",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("Finish executing simple_module.js",
            base::UTF16ToUTF8(console_observer.messages()[1].message));

  EXPECT_TRUE(ExecJs(shell(), JsReplace("window.resolveSelectURLToConfig = $1;",
                                        ResolveSelectURLToConfig())));

  TestSelectURLFencedFrameConfigObserver config_observer(GetStoragePartition());

  EXPECT_EQ(1u, test_runtime_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_runtime_manager().GetKeepAliveWorkletHostsCount());

  // There is 1 more "worklet operation": `selectURL()`.
  test_runtime_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EvalJsResult result1 = EvalJs(shell(), R"(
      (async function() {
        window.select_url_result = await sharedStorage.selectURL(
          'test-url-selection-operation',
          [
            {
              url: "fenced_frames/title0.html"
            }
          ],
          {
            data: {'mockResult': 0},
            resolveToConfig: resolveSelectURLToConfig,
            keepAlive: false
          }
        );
        if (resolveSelectURLToConfig &&
            !(select_url_result instanceof FencedFrameConfig)) {
          throw new Error('selectURL() did not return a FencedFrameConfig.');
        }
        return window.select_url_result;
      })()
    )");

  EXPECT_TRUE(result1.is_ok());
  const std::optional<GURL>& observed_urn_uuid = config_observer.GetUrnUuid();
  EXPECT_TRUE(observed_urn_uuid.has_value());
  EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid.value()));

  if (!ResolveSelectURLToConfig()) {
    EXPECT_EQ(result1.ExtractString(), observed_urn_uuid->spec());
  }

  test_runtime_manager().GetAttachedWorkletHost()->WaitForWorkletResponses();

  ASSERT_TRUE(config_observer.ConfigObserved());
  const std::optional<FencedFrameConfig>& fenced_frame_config =
      config_observer.GetConfig();
  EXPECT_TRUE(fenced_frame_config.has_value());
  EXPECT_EQ(fenced_frame_config->urn_uuid(), observed_urn_uuid.value());

  EXPECT_EQ("Finish executing 'test-url-selection-operation'",
            base::UTF16ToUTF8(console_observer.messages().back().message));

  EXPECT_EQ(0u, test_runtime_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_runtime_manager().GetKeepAliveWorkletHostsCount());

  EXPECT_THAT(EvalJs(shell(), R"(
      sharedStorage.run(
          'test-operation', {data: {'customKey': 'customValue'}});
    )"),
              EvalJsResult::ErrorIs(
                  testing::HasSubstr(kSharedStorageWorkletExpiredMessage)));

  EXPECT_EQ(0u, test_runtime_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_runtime_manager().GetKeepAliveWorkletHostsCount());

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     1);

  ASSERT_EQ(urn_uuids_observed().size(), 1u);

  std::string origin_str = url::Origin::Create(main_url).Serialize();
  ExpectAccessObserved(
      {{AccessScope::kWindow, AccessMethod::kAddModule, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForAddModule(
            https_server()->GetURL("a.test",
                                   "/shared_storage/simple_module.js"),
            /*worklet_ordinal=*/0, GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kWindow, AccessMethod::kSelectURL, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForSelectURLForTesting(
            "test-url-selection-operation", /*operation_id=*/0,
            /*keep_alive=*/false,
            SharedStorageEventParams::PrivateAggregationConfigWrapper(),
            blink::CloneableMessage(),
            std::vector<SharedStorageUrlSpecWithMetadata>(
                {{https_server()->GetURL("a.test",
                                         "/fenced_frames/title0.html"),
                  {}}}),
            ResolveSelectURLToConfig(),
            /*saved_query=*/std::string(), urn_uuids_observed()[0],
            GetFirstWorkletHostDevToolsToken())}});
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest,
                       RunOperation_RunAfterKeepAliveDefaultSelectURL_Failure) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("Start executing simple_module.js",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("Finish executing simple_module.js",
            base::UTF16ToUTF8(console_observer.messages()[1].message));

  EXPECT_TRUE(ExecJs(shell(), JsReplace("window.resolveSelectURLToConfig = $1;",
                                        ResolveSelectURLToConfig())));

  TestSelectURLFencedFrameConfigObserver config_observer(GetStoragePartition());

  EXPECT_EQ(1u, test_runtime_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_runtime_manager().GetKeepAliveWorkletHostsCount());

  // There is 1 more "worklet operation": `selectURL()`.
  test_runtime_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EvalJsResult result1 = EvalJs(shell(), R"(
      (async function() {
        window.select_url_result = await sharedStorage.selectURL(
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
        if (resolveSelectURLToConfig &&
            !(select_url_result instanceof FencedFrameConfig)) {
          throw new Error('selectURL() did not return a FencedFrameConfig.');
        }
        return window.select_url_result;
      })()
    )");

  EXPECT_TRUE(result1.is_ok());
  const std::optional<GURL>& observed_urn_uuid = config_observer.GetUrnUuid();
  EXPECT_TRUE(observed_urn_uuid.has_value());
  EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid.value()));

  if (!ResolveSelectURLToConfig()) {
    EXPECT_EQ(result1.ExtractString(), observed_urn_uuid->spec());
  }

  test_runtime_manager().GetAttachedWorkletHost()->WaitForWorkletResponses();

  ASSERT_TRUE(config_observer.ConfigObserved());
  const std::optional<FencedFrameConfig>& fenced_frame_config =
      config_observer.GetConfig();
  EXPECT_TRUE(fenced_frame_config.has_value());
  EXPECT_EQ(fenced_frame_config->urn_uuid(), observed_urn_uuid.value());

  EXPECT_EQ("Finish executing 'test-url-selection-operation'",
            base::UTF16ToUTF8(console_observer.messages().back().message));

  EXPECT_EQ(0u, test_runtime_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_runtime_manager().GetKeepAliveWorkletHostsCount());

  EXPECT_THAT(EvalJs(shell(), R"(
      sharedStorage.run(
          'test-operation', {data: {'customKey': 'customValue'}});
    )"),
              EvalJsResult::ErrorIs(
                  testing::HasSubstr(kSharedStorageWorkletExpiredMessage)));

  EXPECT_EQ(0u, test_runtime_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_runtime_manager().GetKeepAliveWorkletHostsCount());

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     1);

  ASSERT_EQ(urn_uuids_observed().size(), 1u);

  std::string origin_str = url::Origin::Create(main_url).Serialize();
  ExpectAccessObserved(
      {{AccessScope::kWindow, AccessMethod::kAddModule, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForAddModule(
            https_server()->GetURL("a.test",
                                   "/shared_storage/simple_module.js"),
            /*worklet_ordinal=*/0, GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kWindow, AccessMethod::kSelectURL, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForSelectURLForTesting(
            "test-url-selection-operation", /*operation_id=*/0,
            /*keep_alive=*/false,
            SharedStorageEventParams::PrivateAggregationConfigWrapper(),
            blink::CloneableMessage(),
            std::vector<SharedStorageUrlSpecWithMetadata>(
                {{https_server()->GetURL("a.test",
                                         "/fenced_frames/title0.html"),
                  {}}}),
            ResolveSelectURLToConfig(),
            /*saved_query=*/std::string(), urn_uuids_observed()[0],
            GetFirstWorkletHostDevToolsToken())}});
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest,
                       SelectURL_ReportingMetadata_EmptyReportEvent) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

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
              url: "fenced_frames/title0.html",
              reportingMetadata: {
                "": "fenced_frames/report1.html"
              }
            }
          ],
          {
            data: {'mockResult': 0},
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

  EXPECT_TRUE(result.is_ok());
  const std::optional<GURL>& observed_urn_uuid = config_observer.GetUrnUuid();
  EXPECT_TRUE(observed_urn_uuid.has_value());
  EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid.value()));

  if (!ResolveSelectURLToConfig()) {
    EXPECT_EQ(result.ExtractString(), observed_urn_uuid->spec());
  }

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
  EXPECT_DOUBLE_EQ(metadata->budget_to_charge, 0.0);

  EXPECT_THAT(GetSharedStorageReportingMap(observed_urn_uuid.value()),
              UnorderedElementsAre(
                  Pair("", https_server()->GetURL(
                               "a.test", "/fenced_frames/report1.html"))));

  EXPECT_EQ("Finish executing 'test-url-selection-operation'",
            base::UTF16ToUTF8(console_observer.messages().back().message));

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     1);

  ASSERT_EQ(urn_uuids_observed().size(), 1u);

  std::string origin_str = url::Origin::Create(main_url).Serialize();
  ExpectAccessObserved(
      {{AccessScope::kWindow, AccessMethod::kAddModule, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForAddModule(
            https_server()->GetURL("a.test",
                                   "/shared_storage/simple_module.js"),
            /*worklet_ordinal=*/0, GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kWindow, AccessMethod::kSelectURL, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForSelectURLForTesting(
            "test-url-selection-operation", /*operation_id=*/0,
            /*keep_alive=*/false,
            SharedStorageEventParams::PrivateAggregationConfigWrapper(),
            blink::CloneableMessage(),
            std::vector<SharedStorageUrlSpecWithMetadata>(
                {{https_server()->GetURL("a.test",
                                         "/fenced_frames/title0.html"),
                  {{"", https_server()
                            ->GetURL("a.test", "/fenced_frames/report1.html")
                            .spec()}}}}),
            ResolveSelectURLToConfig(),
            /*saved_query=*/std::string(), urn_uuids_observed()[0],
            GetFirstWorkletHostDevToolsToken())}});
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest, SetAppendOperationInDocument) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.set('key0', 'value0');

      sharedStorage.set('key1', 'value1');
      sharedStorage.set('key1', 'value111');

      sharedStorage.set('key2', 'value2');
      sharedStorage.set('key2', 'value222', {ignoreIfPresent: true});

      sharedStorage.set('key3', 'value3');
      sharedStorage.append('key3', 'value333');
    )"));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      console.log(await sharedStorage.get('key0'));
      console.log(await sharedStorage.get('key1'));
      console.log(await sharedStorage.get('key2'));
      console.log(await sharedStorage.get('key3'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(5u, console_observer.messages().size());
  EXPECT_EQ("value0",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("value111",
            base::UTF16ToUTF8(console_observer.messages()[1].message));
  EXPECT_EQ("value2",
            base::UTF16ToUTF8(console_observer.messages()[2].message));
  EXPECT_EQ("value3value333",
            base::UTF16ToUTF8(console_observer.messages()[3].message));
  EXPECT_EQ("4", base::UTF16ToUTF8(console_observer.messages()[4].message));

  WaitForHistograms({kTimingRunExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingRunExecutedInWorkletHistogram, 1);

  std::string origin_str = url::Origin::Create(url).Serialize();
  ExpectAccessObserved(
      {{AccessScope::kWindow, AccessMethod::kSet, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForSet("key0", "value0", false)},
       {AccessScope::kWindow, AccessMethod::kSet, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForSet("key1", "value1", false)},
       {AccessScope::kWindow, AccessMethod::kSet, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForSet("key1", "value111", false)},
       {AccessScope::kWindow, AccessMethod::kSet, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForSet("key2", "value2", false)},
       {AccessScope::kWindow, AccessMethod::kSet, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForSet("key2", "value222", true)},
       {AccessScope::kWindow, AccessMethod::kSet, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForSet("key3", "value3", false)},
       {AccessScope::kWindow, AccessMethod::kAppend, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForAppend("key3", "value333")},
       {AccessScope::kWindow, AccessMethod::kAddModule, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForAddModule(
            out_script_url,
            /*worklet_ordinal=*/0, GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kWindow, AccessMethod::kRun, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForRunForTesting(
            "test-operation", /*operation_id=*/0, /*keep_alive=*/true,
            SharedStorageEventParams::PrivateAggregationConfigWrapper(),
            blink::CloneableMessage(), GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kGet, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForGet(
            "key0", GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kGet, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForGet(
            "key1", GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kGet, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForGet(
            "key2", GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kGet, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForGet(
            "key3", GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kLength,
        MainFrameId(), origin_str,
        SharedStorageEventParams::CreateWithWorkletToken(
            GetFirstWorkletHostDevToolsToken())}});
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest, DeleteOperationInDocument) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.set('key0', 'value0');
      sharedStorage.delete('key0');
    )"));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      console.log(await sharedStorage.length());
      console.log(await sharedStorage.get('key0'));
    )",
                         &out_script_url);

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("0", base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ(blink::mojom::ConsoleMessageLevel::kInfo,
            console_observer.messages()[0].log_level);
  EXPECT_EQ("undefined",
            base::UTF16ToUTF8(console_observer.messages()[1].message));
  EXPECT_EQ(blink::mojom::ConsoleMessageLevel::kInfo,
            console_observer.messages()[1].log_level);

  WaitForHistograms({kTimingRunExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingRunExecutedInWorkletHistogram, 1);

  std::string origin_str = url::Origin::Create(url).Serialize();
  ExpectAccessObserved(
      {{AccessScope::kWindow, AccessMethod::kSet, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForSet("key0", "value0", false)},
       {AccessScope::kWindow, AccessMethod::kDelete, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForDelete("key0")},
       {AccessScope::kWindow, AccessMethod::kAddModule, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForAddModule(
            out_script_url,
            /*worklet_ordinal=*/0, GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kWindow, AccessMethod::kRun, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForRunForTesting(
            "test-operation", /*operation_id=*/0, /*keep_alive=*/true,
            SharedStorageEventParams::PrivateAggregationConfigWrapper(),
            blink::CloneableMessage(), GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kLength,
        MainFrameId(), origin_str,
        SharedStorageEventParams::CreateWithWorkletToken(
            GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kGet, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForGet(
            "key0", GetFirstWorkletHostDevToolsToken())}});
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest, ClearOperationInDocument) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.set('key0', 'value0');
      sharedStorage.clear();
    )"));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(1u, console_observer.messages().size());
  EXPECT_EQ("0", base::UTF16ToUTF8(console_observer.messages()[0].message));

  WaitForHistograms({kTimingRunExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingRunExecutedInWorkletHistogram, 1);

  std::string origin_str = url::Origin::Create(url).Serialize();
  ExpectAccessObserved(
      {{AccessScope::kWindow, AccessMethod::kSet, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForSet("key0", "value0", false)},
       {AccessScope::kWindow, AccessMethod::kClear, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForClear()},
       {AccessScope::kWindow, AccessMethod::kAddModule, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForAddModule(
            out_script_url,
            /*worklet_ordinal=*/0, GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kWindow, AccessMethod::kRun, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForRunForTesting(
            "test-operation", /*operation_id=*/0, /*keep_alive=*/true,
            SharedStorageEventParams::PrivateAggregationConfigWrapper(),
            blink::CloneableMessage(), GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kLength,
        MainFrameId(), origin_str,
        SharedStorageEventParams::CreateWithWorkletToken(
            GetFirstWorkletHostDevToolsToken())}});
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest, SetAppendOperationInWorklet) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      sharedStorage.set('key0', 'value0');

      sharedStorage.set('key1', 'value1');
      sharedStorage.set('key1', 'value111');

      sharedStorage.set('key2', 'value2');
      sharedStorage.set('key2', 'value222', {ignoreIfPresent: true});

      sharedStorage.set('key3', 'value3');
      sharedStorage.append('key3', 'value333');

      console.log(await sharedStorage.get('key0'));
      console.log(await sharedStorage.get('key1'));
      console.log(await sharedStorage.get('key2'));
      console.log(await sharedStorage.get('key3'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(5u, console_observer.messages().size());
  EXPECT_EQ("value0",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("value111",
            base::UTF16ToUTF8(console_observer.messages()[1].message));
  EXPECT_EQ("value2",
            base::UTF16ToUTF8(console_observer.messages()[2].message));
  EXPECT_EQ("value3value333",
            base::UTF16ToUTF8(console_observer.messages()[3].message));
  EXPECT_EQ("4", base::UTF16ToUTF8(console_observer.messages()[4].message));

  WaitForHistograms({kTimingRunExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingRunExecutedInWorkletHistogram, 1);

  std::string origin_str = url::Origin::Create(url).Serialize();
  ExpectAccessObserved(
      {{AccessScope::kWindow, AccessMethod::kAddModule, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForAddModule(
            out_script_url,
            /*worklet_ordinal=*/0, GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kWindow, AccessMethod::kRun, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForRunForTesting(
            "test-operation", /*operation_id=*/0, /*keep_alive=*/true,
            SharedStorageEventParams::PrivateAggregationConfigWrapper(),
            blink::CloneableMessage(), GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kSet, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForSet(
            "key0", "value0", false, GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kSet, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForSet(
            "key1", "value1", false, GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kSet, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForSet(
            "key1", "value111", false, GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kSet, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForSet(
            "key2", "value2", false, GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kSet, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForSet(
            "key2", "value222", true, GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kSet, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForSet(
            "key3", "value3", false, GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kAppend,
        MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForAppend(
            "key3", "value333", GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kGet, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForGet(
            "key0", GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kGet, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForGet(
            "key1", GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kGet, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForGet(
            "key2", GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kGet, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForGet(
            "key3", GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kLength,
        MainFrameId(), origin_str,
        SharedStorageEventParams::CreateWithWorkletToken(
            GetFirstWorkletHostDevToolsToken())}});
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest,
                       AppendOperationFailedInWorklet) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      await sharedStorage.set('k', 'a'.repeat(2621439));

      // This will fail due to the storage quota being reached.
      await sharedStorage.append('k', 'a');
    )",
                         &out_script_url);

  EXPECT_EQ(1u, console_observer.messages().size());
  EXPECT_THAT(base::UTF16ToUTF8(console_observer.messages()[0].message),
              testing::HasSubstr("sharedStorage.append() failed"));
  EXPECT_EQ(blink::mojom::ConsoleMessageLevel::kError,
            console_observer.messages()[0].log_level);

  WaitForHistograms({kTimingRunExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingRunExecutedInWorkletHistogram, 1);

  std::string origin_str = url::Origin::Create(url).Serialize();
  ExpectAccessObserved(
      {{AccessScope::kWindow, AccessMethod::kAddModule, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForAddModule(
            out_script_url,
            /*worklet_ordinal=*/0, GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kWindow, AccessMethod::kRun, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForRunForTesting(
            "test-operation", /*operation_id=*/0, /*keep_alive=*/true,
            SharedStorageEventParams::PrivateAggregationConfigWrapper(),
            blink::CloneableMessage(), GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kSet, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForSet(
            "k", std::string(2621439, 'a'), false,
            GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kAppend,
        MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForAppend(
            "k", "a", GetFirstWorkletHostDevToolsToken())}});
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest, DeleteOperationInWorklet) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      sharedStorage.set('key0', 'value0');
      console.log(await sharedStorage.length());
      console.log(await sharedStorage.get('key0'));

      sharedStorage.delete('key0');

      console.log(await sharedStorage.length());
      console.log(await sharedStorage.get('key0'));
    )",
                         &out_script_url);

  EXPECT_EQ(4u, console_observer.messages().size());
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("value0",
            base::UTF16ToUTF8(console_observer.messages()[1].message));
  EXPECT_EQ("0", base::UTF16ToUTF8(console_observer.messages()[2].message));
  EXPECT_EQ("undefined",
            base::UTF16ToUTF8(console_observer.messages()[3].message));
  EXPECT_EQ(blink::mojom::ConsoleMessageLevel::kInfo,
            console_observer.messages()[0].log_level);
  EXPECT_EQ(blink::mojom::ConsoleMessageLevel::kInfo,
            console_observer.messages()[1].log_level);
  EXPECT_EQ(blink::mojom::ConsoleMessageLevel::kInfo,
            console_observer.messages()[2].log_level);
  EXPECT_EQ(blink::mojom::ConsoleMessageLevel::kInfo,
            console_observer.messages()[3].log_level);

  WaitForHistograms({kTimingRunExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingRunExecutedInWorkletHistogram, 1);

  std::string origin_str = url::Origin::Create(url).Serialize();
  ExpectAccessObserved(
      {{AccessScope::kWindow, AccessMethod::kAddModule, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForAddModule(
            out_script_url,
            /*worklet_ordinal=*/0, GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kWindow, AccessMethod::kRun, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForRunForTesting(
            "test-operation", /*operation_id=*/0, /*keep_alive=*/true,
            SharedStorageEventParams::PrivateAggregationConfigWrapper(),
            blink::CloneableMessage(), GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kSet, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForSet(
            "key0", "value0", false, GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kLength,
        MainFrameId(), origin_str,
        SharedStorageEventParams::CreateWithWorkletToken(
            GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kGet, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForGet(
            "key0", GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kDelete,
        MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForDelete(
            "key0", GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kLength,
        MainFrameId(), origin_str,
        SharedStorageEventParams::CreateWithWorkletToken(
            GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kGet, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForGet(
            "key0", GetFirstWorkletHostDevToolsToken())}});
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest, ClearOperationInWorklet) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      sharedStorage.set('key0', 'value0');
      console.log(await sharedStorage.length());
      console.log(await sharedStorage.get('key0'));

      sharedStorage.clear();

      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(3u, console_observer.messages().size());
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("value0",
            base::UTF16ToUTF8(console_observer.messages()[1].message));
  EXPECT_EQ("0", base::UTF16ToUTF8(console_observer.messages()[2].message));

  WaitForHistograms({kTimingRunExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingRunExecutedInWorkletHistogram, 1);

  std::string origin_str = url::Origin::Create(url).Serialize();
  ExpectAccessObserved(
      {{AccessScope::kWindow, AccessMethod::kAddModule, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForAddModule(
            out_script_url,
            /*worklet_ordinal=*/0, GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kWindow, AccessMethod::kRun, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForRunForTesting(
            "test-operation", /*operation_id=*/0, /*keep_alive=*/true,
            SharedStorageEventParams::PrivateAggregationConfigWrapper(),
            blink::CloneableMessage(), GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kSet, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForSet(
            "key0", "value0", false, GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kLength,
        MainFrameId(), origin_str,
        SharedStorageEventParams::CreateWithWorkletToken(
            GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kGet, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForGet(
            "key0", GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kClear, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateWithWorkletToken(
            GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kLength,
        MainFrameId(), origin_str,
        SharedStorageEventParams::CreateWithWorkletToken(
            GetFirstWorkletHostDevToolsToken())}});
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest, ConsoleErrorInWorklet) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      console.error('error0');
    )",
                         &out_script_url);

  EXPECT_EQ(1u, console_observer.messages().size());
  EXPECT_EQ(blink::mojom::ConsoleMessageLevel::kError,
            console_observer.messages()[0].log_level);
  EXPECT_EQ("error0",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest, GetOperationInWorklet) {
  base::SimpleTestClock clock;
  base::RunLoop loop;
  static_cast<StoragePartitionImpl*>(GetStoragePartition())
      ->GetSharedStorageManager()
      ->OverrideClockForTesting(&clock, loop.QuitClosure());
  loop.Run();
  clock.SetNow(base::Time::Now());

  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.set('key0', 'value0');
    )"));

  WebContentsConsoleObserver console_observer(shell()->web_contents());
  GURL script_url =
      https_server()->GetURL("a.test", "/shared_storage/getter_module.js");

  EXPECT_TRUE(ExecJs(
      shell(), JsReplace("sharedStorage.worklet.addModule($1)", script_url)));

  // There is 1 more "worklet operation": `run()`.
  test_runtime_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EXPECT_TRUE(ExecJs(shell(), R"(
        sharedStorage.run('get-operation', {data: {'key': 'key0'},
                                            keepAlive: true});
      )"));

  test_runtime_manager().GetAttachedWorkletHost()->WaitForWorkletResponses();

  // Advance clock so that key will expire.
  clock.Advance(base::Days(kStalenessThresholdDays) + base::Seconds(1));

  // There is 1 more "worklet operation": `run()`.
  test_runtime_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EXPECT_TRUE(ExecJs(shell(), R"(
        sharedStorage.run('get-operation', {data: {'key': 'key0'}});
      )"));

  test_runtime_manager().GetAttachedWorkletHost()->WaitForWorkletResponses();

  EXPECT_EQ(4u, console_observer.messages().size());
  EXPECT_EQ("sharedStorage.length(): 1",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("sharedStorage.get('key0'): value0",
            base::UTF16ToUTF8(console_observer.messages()[1].message));
  EXPECT_EQ("sharedStorage.length(): 0",
            base::UTF16ToUTF8(console_observer.messages()[2].message));
  EXPECT_EQ("sharedStorage.get('key0'): undefined",
            base::UTF16ToUTF8(console_observer.messages()[3].message));
  EXPECT_EQ(blink::mojom::ConsoleMessageLevel::kInfo,
            console_observer.messages()[0].log_level);
  EXPECT_EQ(blink::mojom::ConsoleMessageLevel::kInfo,
            console_observer.messages()[1].log_level);
  EXPECT_EQ(blink::mojom::ConsoleMessageLevel::kInfo,
            console_observer.messages()[2].log_level);
  EXPECT_EQ(blink::mojom::ConsoleMessageLevel::kInfo,
            console_observer.messages()[3].log_level);

  WaitForHistograms({kTimingRunExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingRunExecutedInWorkletHistogram, 2);

  std::string origin_str = url::Origin::Create(url).Serialize();
  ExpectAccessObserved(
      {{AccessScope::kWindow, AccessMethod::kSet, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForSet("key0", "value0", false)},
       {AccessScope::kWindow, AccessMethod::kAddModule, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForAddModule(
            script_url,
            /*worklet_ordinal=*/0, GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kWindow, AccessMethod::kRun, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForRunForTesting(
            "get-operation", /*operation_id=*/0, /*keep_alive=*/true,
            SharedStorageEventParams::PrivateAggregationConfigWrapper(),
            blink::CloneableMessage(), GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kLength,
        MainFrameId(), origin_str,
        SharedStorageEventParams::CreateWithWorkletToken(
            GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kGet, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForGet(
            "key0", GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kWindow, AccessMethod::kRun, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForRunForTesting(
            "get-operation", /*operation_id=*/1, /*keep_alive=*/false,
            SharedStorageEventParams::PrivateAggregationConfigWrapper(),
            blink::CloneableMessage(), GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kLength,
        MainFrameId(), origin_str,
        SharedStorageEventParams::CreateWithWorkletToken(
            GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kGet, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForGet(
            "key0", GetFirstWorkletHostDevToolsToken())}});
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest,
                       AccessStorageInSameOriginDocument) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Cache the main frame ID for comparison below, since it will change with
  // navigation.
  GlobalRenderFrameHostId cached_main_frame_id = MainFrameId();

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.set('key0', 'value0');
    )"));

  EXPECT_TRUE(
      NavigateToURL(shell(), https_server()->GetURL("a.test", "/title1.html")));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(1u, console_observer.messages().size());
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[0].message));

  WaitForHistograms({kTimingRunExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingRunExecutedInWorkletHistogram, 1);

  std::string origin_str = url::Origin::Create(url).Serialize();
  ExpectAccessObserved(
      {{AccessScope::kWindow, AccessMethod::kSet, cached_main_frame_id,
        origin_str,
        SharedStorageEventParams::CreateForSet("key0", "value0", false)},
       {AccessScope::kWindow, AccessMethod::kAddModule, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForAddModule(
            out_script_url,
            /*worklet_ordinal=*/0, GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kWindow, AccessMethod::kRun, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForRunForTesting(
            "test-operation", /*operation_id=*/0, /*keep_alive=*/true,
            SharedStorageEventParams::PrivateAggregationConfigWrapper(),
            blink::CloneableMessage(), GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kLength,
        MainFrameId(), origin_str,
        SharedStorageEventParams::CreateWithWorkletToken(
            GetFirstWorkletHostDevToolsToken())}});
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest,
                       AccessStorageInDifferentOriginDocument) {
  GURL url1 = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url1));

  // Cache the main frame ID for comparison below, since it will change with
  // navigation.
  GlobalRenderFrameHostId cached_main_frame_id = MainFrameId();

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.set('key0', 'value0');
    )"));

  GURL url2 = https_server()->GetURL("b.test", "/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), url2));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(1u, console_observer.messages().size());
  EXPECT_EQ("0", base::UTF16ToUTF8(console_observer.messages()[0].message));

  WaitForHistograms({kTimingRunExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingRunExecutedInWorkletHistogram, 1);

  std::string origin2_str = url::Origin::Create(url2).Serialize();
  ExpectAccessObserved(
      {{AccessScope::kWindow, AccessMethod::kSet, cached_main_frame_id,
        url::Origin::Create(url1).Serialize(),
        SharedStorageEventParams::CreateForSet("key0", "value0", false)},
       {AccessScope::kWindow, AccessMethod::kAddModule, MainFrameId(),
        origin2_str,
        SharedStorageEventParams::CreateForAddModule(
            out_script_url,
            /*worklet_ordinal=*/0, GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kWindow, AccessMethod::kRun, MainFrameId(), origin2_str,
        SharedStorageEventParams::CreateForRunForTesting(
            "test-operation", /*operation_id=*/0, /*keep_alive=*/true,
            SharedStorageEventParams::PrivateAggregationConfigWrapper(),
            blink::CloneableMessage(), GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kLength,
        MainFrameId(), origin2_str,
        SharedStorageEventParams::CreateWithWorkletToken(
            GetFirstWorkletHostDevToolsToken())}});
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest, KeysAndEntriesOperation) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.set('key0', 'value0');
      sharedStorage.set('key1', 'value1');
      sharedStorage.set('key2', 'value2');
    )"));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      for await (const key of sharedStorage.keys()) {
        console.log(key);
      }
      for await (const [key, value] of sharedStorage.entries()) {
        console.log(key + ';' + value);
      }
    )",
                         &out_script_url);

  EXPECT_EQ(6u, console_observer.messages().size());
  EXPECT_EQ("key0", base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("key1", base::UTF16ToUTF8(console_observer.messages()[1].message));
  EXPECT_EQ("key2", base::UTF16ToUTF8(console_observer.messages()[2].message));
  EXPECT_EQ("key0;value0",
            base::UTF16ToUTF8(console_observer.messages()[3].message));
  EXPECT_EQ("key1;value1",
            base::UTF16ToUTF8(console_observer.messages()[4].message));
  EXPECT_EQ("key2;value2",
            base::UTF16ToUTF8(console_observer.messages()[5].message));

  WaitForHistograms({kTimingRunExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingRunExecutedInWorkletHistogram, 1);

  std::string origin_str = url::Origin::Create(url).Serialize();
  ExpectAccessObserved(
      {{AccessScope::kWindow, AccessMethod::kSet, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForSet("key0", "value0", false)},
       {AccessScope::kWindow, AccessMethod::kSet, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForSet("key1", "value1", false)},
       {AccessScope::kWindow, AccessMethod::kSet, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForSet("key2", "value2", false)},
       {AccessScope::kWindow, AccessMethod::kAddModule, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForAddModule(
            out_script_url,
            /*worklet_ordinal=*/0, GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kWindow, AccessMethod::kRun, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForRunForTesting(
            "test-operation", /*operation_id=*/0, /*keep_alive=*/true,
            SharedStorageEventParams::PrivateAggregationConfigWrapper(),
            blink::CloneableMessage(), GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kKeys, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateWithWorkletToken(
            GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kEntries,
        MainFrameId(), origin_str,
        SharedStorageEventParams::CreateWithWorkletToken(
            GetFirstWorkletHostDevToolsToken())}});
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest, ValuesOperation) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.set('key0', 'value0');
      sharedStorage.set('key1', 'value1');
      sharedStorage.set('key2', 'value2');
    )"));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      for await (const value of sharedStorage.values()) {
        console.log(value);
      }
    )",
                         &out_script_url);

  EXPECT_EQ(3u, console_observer.messages().size());
  EXPECT_EQ("value0",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("value1",
            base::UTF16ToUTF8(console_observer.messages()[1].message));
  EXPECT_EQ("value2",
            base::UTF16ToUTF8(console_observer.messages()[2].message));

  WaitForHistograms({kTimingRunExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingRunExecutedInWorkletHistogram, 1);

  std::string origin_str = url::Origin::Create(url).Serialize();
  ExpectAccessObserved(
      {{AccessScope::kWindow, AccessMethod::kSet, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForSet("key0", "value0", false)},
       {AccessScope::kWindow, AccessMethod::kSet, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForSet("key1", "value1", false)},
       {AccessScope::kWindow, AccessMethod::kSet, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForSet("key2", "value2", false)},
       {AccessScope::kWindow, AccessMethod::kAddModule, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForAddModule(
            out_script_url,
            /*worklet_ordinal=*/0, GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kWindow, AccessMethod::kRun, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForRunForTesting(
            "test-operation", /*operation_id=*/0, /*keep_alive=*/true,
            SharedStorageEventParams::PrivateAggregationConfigWrapper(),
            blink::CloneableMessage(), GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kValues,
        MainFrameId(), origin_str,
        SharedStorageEventParams::CreateWithWorkletToken(
            GetFirstWorkletHostDevToolsToken())}});
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest,
                       KeysAndEntriesOperation_MultipleBatches) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_TRUE(ExecJs(shell(), R"(
      for (let i = 0; i < 150; ++i) {
        sharedStorage.set('key' + i.toString().padStart(3, '0'),
                          'value' + i.toString().padStart(3, '0'));
      }
    )"));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      for await (const key of sharedStorage.keys()) {
        console.log(key);
      }
      for await (const [key, value] of sharedStorage.entries()) {
        console.log(key + ';' + value);
      }
    )",
                         &out_script_url);

  EXPECT_EQ(300u, console_observer.messages().size());
  std::string origin_str = url::Origin::Create(url).Serialize();
  std::vector<TestSharedStorageObserver::Access> expected_accesses;
  for (int i = 0; i < 150; ++i) {
    std::string zero_padded_i = base::NumberToString(i);
    zero_padded_i.insert(zero_padded_i.begin(), 3 - zero_padded_i.size(), '0');

    std::string padded_key = base::StrCat({"key", zero_padded_i});
    std::string padded_value = base::StrCat({"value", zero_padded_i});
    EXPECT_EQ(padded_key,
              base::UTF16ToUTF8(console_observer.messages()[i].message));
    EXPECT_EQ(base::JoinString({padded_key, padded_value}, ";"),
              base::UTF16ToUTF8(console_observer.messages()[i + 150].message));

    expected_accesses.emplace_back(AccessScope::kWindow, AccessMethod::kSet,
                                   MainFrameId(), origin_str,
                                   SharedStorageEventParams::CreateForSet(
                                       padded_key, padded_value, false));
  }

  WaitForHistograms({kTimingRunExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingRunExecutedInWorkletHistogram, 1);

  expected_accesses.emplace_back(AccessScope::kWindow, AccessMethod::kAddModule,
                                 MainFrameId(), origin_str,
                                 SharedStorageEventParams::CreateForAddModule(
                                     out_script_url, /*worklet_ordinal=*/0,
                                     GetFirstWorkletHostDevToolsToken()));
  expected_accesses.emplace_back(
      AccessScope::kWindow, AccessMethod::kRun, MainFrameId(), origin_str,
      SharedStorageEventParams::CreateForRunForTesting(
          "test-operation", /*operation_id=*/0, /*keep_alive=*/true,
          SharedStorageEventParams::PrivateAggregationConfigWrapper(),
          blink::CloneableMessage(), GetFirstWorkletHostDevToolsToken()));
  expected_accesses.emplace_back(
      AccessScope::kSharedStorageWorklet, AccessMethod::kKeys, MainFrameId(),
      origin_str,
      SharedStorageEventParams::CreateWithWorkletToken(
          GetFirstWorkletHostDevToolsToken()));
  expected_accesses.emplace_back(
      AccessScope::kSharedStorageWorklet, AccessMethod::kEntries, MainFrameId(),
      origin_str,
      SharedStorageEventParams::CreateWithWorkletToken(
          GetFirstWorkletHostDevToolsToken()));
  ExpectAccessObserved(expected_accesses);
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest, WebLocksUsageHistograms) {
  // The test assumes pages gets deleted after navigation. To ensure this,
  // disable back/forward cache.
  content::DisableBackForwardCacheForTesting(
      shell()->web_contents(),
      content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));
  GlobalRenderFrameHostId initial_main_frame_id = MainFrameId();

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      await sharedStorage.set('key0', 'value0');
      await sharedStorage.set('key0', 'value0');

      await navigator.locks.request("lock1", async (lock) => {
        await sharedStorage.set('key0', 'value0', { withLock: 'lock2' });
      });

      await sharedStorage.batchUpdate([
        new SharedStorageSetMethod('key0', 'value0'),
        new SharedStorageAppendMethod('key1', 'value1')
      ], { withLock: 'lock1' });
    )",
                         &out_script_url);

  // Navigate again to record histograms.
  EXPECT_TRUE(
      NavigateToUrlMaybeWaitForRfhDeleted(shell(), GURL(url::kAboutBlankURL)));

  histogram_tester_.ExpectBucketCount(
      "Storage.SharedStorage.UpdateMethod.HasLockOption", true, 1);
  histogram_tester_.ExpectBucketCount(
      "Storage.SharedStorage.UpdateMethod.HasLockOption", false, 2);
  histogram_tester_.ExpectUniqueSample(
      "Storage.SharedStorage.BatchUpdateMethod.HasLockOption", true, 1);
  histogram_tester_.ExpectUniqueSample(
      "Storage.SharedStorage.Worklet.NavigatorLocksInvoked", true, 1);

  std::string origin_str = url::Origin::Create(url).Serialize();
  ExpectAccessObserved(
      {{AccessScope::kWindow, AccessMethod::kAddModule, initial_main_frame_id,
        origin_str,
        SharedStorageEventParams::CreateForAddModule(
            out_script_url,
            /*worklet_ordinal=*/0, GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kWindow, AccessMethod::kRun, initial_main_frame_id,
        origin_str,
        SharedStorageEventParams::CreateForRunForTesting(
            "test-operation", /*operation_id=*/0, /*keep_alive=*/true,
            SharedStorageEventParams::PrivateAggregationConfigWrapper(),
            blink::CloneableMessage(), GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kSet,
        initial_main_frame_id, origin_str,
        SharedStorageEventParams::CreateForSet(
            "key0", "value0",
            /*ignore_if_present=*/false, GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kSet,
        initial_main_frame_id, origin_str,
        SharedStorageEventParams::CreateForSet(
            "key0", "value0",
            /*ignore_if_present=*/false, GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kSet,
        initial_main_frame_id, origin_str,
        SharedStorageEventParams::CreateForSet(
            "key0", "value0", /*ignore_if_present=*/false,
            GetFirstWorkletHostDevToolsToken(),
            /*with_lock=*/"lock2",
            /*batch_update_id=*/std::nullopt)},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kBatchUpdate,
        initial_main_frame_id, origin_str,
        SharedStorageEventParams::CreateForBatchUpdate(
            GetFirstWorkletHostDevToolsToken(),
            /*with_lock=*/"lock1",
            /*batch_update_id=*/0,
            /*batch_size=*/2u)},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kSet,
        initial_main_frame_id, origin_str,
        SharedStorageEventParams::CreateForSet(
            "key0", "value0",
            /*ignore_if_present=*/false, GetFirstWorkletHostDevToolsToken(),
            /*with_lock=*/std::nullopt,
            /*batch_update_id=*/0)},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kAppend,
        initial_main_frame_id, origin_str,
        SharedStorageEventParams::CreateForAppend(
            "key1", "value1", GetFirstWorkletHostDevToolsToken(),
            /*with_lock=*/std::nullopt,
            /*batch_update_id=*/0)}});
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest, MulipleBatchUpdates) {
  // The test assumes pages gets deleted after navigation. To ensure this,
  // disable back/forward cache.
  content::DisableBackForwardCacheForTesting(
      shell()->web_contents(),
      content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));
  GlobalRenderFrameHostId initial_main_frame_id = MainFrameId();

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      await sharedStorage.batchUpdate([
        new SharedStorageSetMethod('key0', 'value0'),
        new SharedStorageAppendMethod('key1', 'value1')
      ]);

      await sharedStorage.batchUpdate([
        new SharedStorageSetMethod('key2', 'value2'),
      ], { withLock: 'lock1' });

      await sharedStorage.batchUpdate([
        new SharedStorageSetMethod("key1", "value0", {ignoreIfPresent: true}),
        new SharedStorageAppendMethod("key1", "value1"),
        new SharedStorageDeleteMethod("key2"),
        new SharedStorageClearMethod()
      ], { withLock: 'lock1' });
    )",
                         &out_script_url);

  // Navigate again to record histograms.
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  histogram_tester_.ExpectBucketCount(
      "Storage.SharedStorage.BatchUpdateMethod.HasLockOption", false, 1);
  histogram_tester_.ExpectBucketCount(
      "Storage.SharedStorage.BatchUpdateMethod.HasLockOption", true, 2);

  std::string origin_str = url::Origin::Create(url).Serialize();
  ExpectAccessObserved(
      {{AccessScope::kWindow, AccessMethod::kAddModule, initial_main_frame_id,
        origin_str,
        SharedStorageEventParams::CreateForAddModule(
            out_script_url,
            /*worklet_ordinal=*/0, GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kWindow, AccessMethod::kRun, initial_main_frame_id,
        origin_str,
        SharedStorageEventParams::CreateForRunForTesting(
            "test-operation", /*operation_id=*/0, /*keep_alive=*/true,
            SharedStorageEventParams::PrivateAggregationConfigWrapper(),
            blink::CloneableMessage(), GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kBatchUpdate,
        initial_main_frame_id, origin_str,
        SharedStorageEventParams::CreateForBatchUpdate(
            GetFirstWorkletHostDevToolsToken(),
            /*with_lock=*/std::nullopt,
            /*batch_update_id=*/0,
            /*batch_size=*/2u)},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kSet,
        initial_main_frame_id, origin_str,
        SharedStorageEventParams::CreateForSet(
            "key0", "value0", /*ignore_if_present=*/false,
            GetFirstWorkletHostDevToolsToken(),
            /*with_lock=*/std::nullopt,
            /*batch_update_id=*/0)},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kAppend,
        initial_main_frame_id, origin_str,
        SharedStorageEventParams::CreateForAppend(
            "key1", "value1", GetFirstWorkletHostDevToolsToken(),
            /*with_lock=*/std::nullopt,
            /*batch_update_id=*/0)},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kBatchUpdate,
        initial_main_frame_id, origin_str,
        SharedStorageEventParams::CreateForBatchUpdate(
            GetFirstWorkletHostDevToolsToken(),
            /*with_lock=*/"lock1",
            /*batch_update_id=*/1,
            /*batch_size=*/1u)},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kSet,
        initial_main_frame_id, origin_str,
        SharedStorageEventParams::CreateForSet(
            "key2", "value2", /*ignore_if_present=*/false,
            GetFirstWorkletHostDevToolsToken(),
            /*with_lock=*/std::nullopt,
            /*batch_update_id=*/1)},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kBatchUpdate,
        initial_main_frame_id, origin_str,
        SharedStorageEventParams::CreateForBatchUpdate(
            GetFirstWorkletHostDevToolsToken(),
            /*with_lock=*/"lock1",
            /*batch_update_id=*/2,
            /*batch_size=*/4u)},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kSet,
        initial_main_frame_id, origin_str,
        SharedStorageEventParams::CreateForSet(
            "key1", "value0",
            /*ignore_if_present=*/true, GetFirstWorkletHostDevToolsToken(),
            /*with_lock=*/std::nullopt,
            /*batch_update_id=*/2)},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kAppend,
        initial_main_frame_id, origin_str,
        SharedStorageEventParams::CreateForAppend(
            "key1", "value1", GetFirstWorkletHostDevToolsToken(),
            /*with_lock=*/std::nullopt,
            /*batch_update_id=*/2)},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kDelete,
        initial_main_frame_id, origin_str,
        SharedStorageEventParams::CreateForDelete(
            "key2", GetFirstWorkletHostDevToolsToken(),
            /*with_lock=*/std::nullopt,
            /*batch_update_id=*/2)},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kClear,
        initial_main_frame_id, origin_str,
        SharedStorageEventParams::CreateForClear(
            GetFirstWorkletHostDevToolsToken(),
            /*with_lock=*/std::nullopt,
            /*batch_update_id=*/2)}});
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest, EmptyBatchUpdate) {
  // The test assumes pages gets deleted after navigation. To ensure this,
  // disable back/forward cache.
  content::DisableBackForwardCacheForTesting(
      shell()->web_contents(),
      content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));
  GlobalRenderFrameHostId initial_main_frame_id = MainFrameId();

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      await sharedStorage.batchUpdate([], { withLock: 'lock1' });
    )",
                         &out_script_url);

  // Navigate again to record histograms.
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  histogram_tester_.ExpectUniqueSample(
      "Storage.SharedStorage.BatchUpdateMethod.HasLockOption", true, 1);

  // The empty batchUpdate does not receive an event notification.
  std::string origin_str = url::Origin::Create(url).Serialize();
  ExpectAccessObserved(
      {{AccessScope::kWindow, AccessMethod::kAddModule, initial_main_frame_id,
        origin_str,
        SharedStorageEventParams::CreateForAddModule(
            out_script_url,
            /*worklet_ordinal=*/0, GetFirstWorkletHostDevToolsToken())},
       {AccessScope::kWindow, AccessMethod::kRun, initial_main_frame_id,
        origin_str,
        SharedStorageEventParams::CreateForRunForTesting(
            "test-operation", /*operation_id=*/0, /*keep_alive=*/true,
            SharedStorageEventParams::PrivateAggregationConfigWrapper(),
            blink::CloneableMessage(), GetFirstWorkletHostDevToolsToken())}});
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest,
                       CreateWorklet_SameOrigin_Success) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  GURL module_script_url = https_server()->GetURL(
      "a.test", net::test_server::GetFilePathWithReplacements(
                    "/shared_storage/module_with_custom_header.js",
                    SharedStorageCrossOriginWorkletResponseHeaderReplacement(
                        kEmptyAccessControlAllowOriginReplacement,
                        kEmptySharedStorageCrossOriginAllowedReplacement)));

  EXPECT_TRUE(ExecJs(shell(), JsReplace("sharedStorage.createWorklet($1)",
                                        module_script_url.spec())));

  TestSharedStorageWorkletHost* worklet_host =
      test_runtime_manager().GetAttachedWorkletHost();
  EXPECT_EQ(shell()->web_contents()->GetPrimaryMainFrame()->GetProcess(),
            worklet_host->GetProcessHost());

  EXPECT_EQ(blink::mojom::SharedStorageWorkletCreationMethod::kCreateWorklet,
            worklet_host->creation_method());
}

IN_PROC_BROWSER_TEST_P(
    SharedStorageBrowserTest,
    CreateWorklet_CrossOriginScript_DefaultDataOrigin_FailedCors) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  GURL module_script_url = https_server()->GetURL(
      "b.test", net::test_server::GetFilePathWithReplacements(
                    "/shared_storage/module_with_custom_header.js",
                    SharedStorageCrossOriginWorkletResponseHeaderReplacement(
                        kEmptyAccessControlAllowOriginReplacement,
                        "Shared-Storage-Cross-Origin-Worklet-Allowed: ?1")));

  EXPECT_THAT(EvalJs(shell(), JsReplace("sharedStorage.createWorklet($1)",
                                        module_script_url.spec())),
              EvalJsResult::ErrorIs(testing::HasSubstr("Failed to load")));
}

IN_PROC_BROWSER_TEST_P(
    SharedStorageBrowserTest,
    CreateWorklet_CrossOriginScript_ContextDataOrigin_FailedCors) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  GURL module_script_url = https_server()->GetURL(
      "b.test", net::test_server::GetFilePathWithReplacements(
                    "/shared_storage/module_with_custom_header.js",
                    SharedStorageCrossOriginWorkletResponseHeaderReplacement(
                        kEmptyAccessControlAllowOriginReplacement,
                        "Shared-Storage-Cross-Origin-Worklet-Allowed: ?1")));

  EXPECT_THAT(
      EvalJs(
          shell(),
          JsReplace(
              "sharedStorage.createWorklet($1, {dataOrigin: 'context-origin'})",
              module_script_url.spec())),
      EvalJsResult::ErrorIs(testing::HasSubstr("Failed to load")));
}

IN_PROC_BROWSER_TEST_P(
    SharedStorageBrowserTest,
    CreateWorklet_CrossOriginScript_ScriptDataOrigin_FailedCors) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  GURL module_script_url = https_server()->GetURL(
      "b.test", net::test_server::GetFilePathWithReplacements(
                    "/shared_storage/module_with_custom_header.js",
                    SharedStorageCrossOriginWorkletResponseHeaderReplacement(
                        kEmptyAccessControlAllowOriginReplacement,
                        "Shared-Storage-Cross-Origin-Worklet-Allowed: ?1")));

  EXPECT_THAT(
      EvalJs(
          shell(),
          JsReplace(
              "sharedStorage.createWorklet($1, {dataOrigin: 'script-origin'})",
              module_script_url.spec())),
      EvalJsResult::ErrorIs(testing::HasSubstr("Failed to load")));
}

IN_PROC_BROWSER_TEST_P(
    SharedStorageBrowserTest,
    CreateWorklet_CrossOriginScript_CustomDataOrigin_FailedCors) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  GURL module_script_url = https_server()->GetURL(
      "b.test", net::test_server::GetFilePathWithReplacements(
                    "/shared_storage/module_with_custom_header.js",
                    SharedStorageCrossOriginWorkletResponseHeaderReplacement(
                        kEmptyAccessControlAllowOriginReplacement,
                        kEmptySharedStorageCrossOriginAllowedReplacement)));

  url::Origin custom_data_origin =
      url::Origin::Create(https_server()->GetURL("c.test", kSimplePagePath));

  EXPECT_THAT(
      EvalJs(
          shell(),
          JsReplace("sharedStorage.createWorklet($1, {dataOrigin: $2})",
                    module_script_url.spec(), custom_data_origin.Serialize())),
      EvalJsResult::ErrorIs(testing::HasSubstr("Failed to load")));
}

IN_PROC_BROWSER_TEST_P(
    SharedStorageBrowserTest,
    CreateWorklet_CrossOriginScript_ScriptDataOrigin_FailedSharedStorageWorkletAllowedResponseHeaderCheck) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  GURL module_script_url = https_server()->GetURL(
      "b.test", net::test_server::GetFilePathWithReplacements(
                    "/shared_storage/module_with_custom_header.js",
                    SharedStorageCrossOriginWorkletResponseHeaderReplacement(
                        "Access-Control-Allow-Origin: *",
                        kEmptySharedStorageCrossOriginAllowedReplacement)));

  EXPECT_THAT(
      EvalJs(
          shell(),
          JsReplace(
              "sharedStorage.createWorklet($1, {dataOrigin: 'script-origin'})",
              module_script_url.spec())),
      EvalJsResult::ErrorIs(testing::HasSubstr("Failed to load")));
}

IN_PROC_BROWSER_TEST_P(
    SharedStorageBrowserTest,
    CreateWorklet_CrossOriginScript_ContextDataOrigin_NoSharedStorageWorkletAllowedResponseHeader_Success) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  GURL module_script_url = https_server()->GetURL(
      "b.test", net::test_server::GetFilePathWithReplacements(
                    "/shared_storage/module_with_custom_header.js",
                    SharedStorageCrossOriginWorkletResponseHeaderReplacement(
                        "Access-Control-Allow-Origin: *",
                        kEmptySharedStorageCrossOriginAllowedReplacement)));

  EXPECT_TRUE(ExecJs(
      shell(),
      JsReplace(
          "sharedStorage.createWorklet($1, {dataOrigin: 'context-origin'})",
          module_script_url.spec())));

  TestSharedStorageWorkletHost* worklet_host =
      test_runtime_manager().GetAttachedWorkletHost();

  // The worklet host should reuse the main frame's process because the context
  // origin is being used as the data origin.
  bool use_new_process =
      (shell()->web_contents()->GetPrimaryMainFrame()->GetProcess() !=
       worklet_host->GetProcessHost());

  EXPECT_FALSE(use_new_process);

  EXPECT_EQ(blink::mojom::SharedStorageWorkletCreationMethod::kCreateWorklet,
            worklet_host->creation_method());

  std::string data_origin_str = url::Origin::Create(url).Serialize();
  ExpectAccessObserved(
      {{AccessScope::kWindow, AccessMethod::kCreateWorklet, MainFrameId(),
        data_origin_str,
        SharedStorageEventParams::CreateForCreateWorklet(
            module_script_url, "context-origin",
            /*worklet_ordinal=*/0, GetFirstWorkletHostDevToolsToken())}});
}

IN_PROC_BROWSER_TEST_P(
    SharedStorageBrowserTest,
    CreateWorklet_CrossOriginScript_DefaultDataOrigin_NoSharedStorageWorkletAllowedResponseHeader_Success) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  GURL module_script_url = https_server()->GetURL(
      "b.test", net::test_server::GetFilePathWithReplacements(
                    "/shared_storage/module_with_custom_header.js",
                    SharedStorageCrossOriginWorkletResponseHeaderReplacement(
                        "Access-Control-Allow-Origin: *",
                        kEmptySharedStorageCrossOriginAllowedReplacement)));

  EXPECT_TRUE(ExecJs(shell(), JsReplace("sharedStorage.createWorklet($1)",
                                        module_script_url.spec())));

  TestSharedStorageWorkletHost* worklet_host =
      test_runtime_manager().GetAttachedWorkletHost();

  // The worklet host should reuse the main frame's process because the context
  // origin is being used as the data origin.
  bool use_new_process =
      (shell()->web_contents()->GetPrimaryMainFrame()->GetProcess() !=
       worklet_host->GetProcessHost());

  EXPECT_FALSE(use_new_process);

  EXPECT_EQ(blink::mojom::SharedStorageWorkletCreationMethod::kCreateWorklet,
            worklet_host->creation_method());

  // The default data origin is the context origin.
  std::string data_origin_str = url::Origin::Create(url).Serialize();
  ExpectAccessObserved(
      {{AccessScope::kWindow, AccessMethod::kCreateWorklet, MainFrameId(),
        data_origin_str,
        SharedStorageEventParams::CreateForCreateWorklet(
            module_script_url, "context-origin",
            /*worklet_ordinal=*/0, GetFirstWorkletHostDevToolsToken())}});
}

IN_PROC_BROWSER_TEST_P(
    SharedStorageBrowserTest,
    CreateWorklet_CrossOriginScript_ScriptDataOrigin_Success) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  GURL module_script_url = https_server()->GetURL(
      "b.test", net::test_server::GetFilePathWithReplacements(
                    "/shared_storage/module_with_custom_header.js",
                    SharedStorageCrossOriginWorkletResponseHeaderReplacement(
                        "Access-Control-Allow-Origin: *",
                        "Shared-Storage-Cross-Origin-Worklet-Allowed: ?1")));

  EXPECT_TRUE(ExecJs(
      shell(),
      JsReplace(
          "sharedStorage.createWorklet($1, {dataOrigin: 'script-origin'})",
          module_script_url.spec())));

  TestSharedStorageWorkletHost* worklet_host =
      test_runtime_manager().GetAttachedWorkletHost();

  // The worklet host should reuse the main frame's process on Android without
  // strict site isolation; otherwise, it should use a new process.
  bool expected_use_new_process = AreAllSitesIsolatedForTesting();
  bool actual_use_new_process =
      (shell()->web_contents()->GetPrimaryMainFrame()->GetProcess() !=
       worklet_host->GetProcessHost());

  EXPECT_EQ(expected_use_new_process, actual_use_new_process);

  EXPECT_EQ(blink::mojom::SharedStorageWorkletCreationMethod::kCreateWorklet,
            worklet_host->creation_method());

  std::string data_origin_str =
      url::Origin::Create(module_script_url).Serialize();
  ExpectAccessObserved(
      {{AccessScope::kWindow, AccessMethod::kCreateWorklet, MainFrameId(),
        data_origin_str,
        SharedStorageEventParams::CreateForCreateWorklet(
            module_script_url, "script-origin",
            /*worklet_ordinal=*/0, GetFirstWorkletHostDevToolsToken())}});
}

IN_PROC_BROWSER_TEST_P(
    SharedStorageBrowserTest,
    CreateWorklet_CrossOriginScript_CustomDataOrigin_Success) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  GURL module_script_url = https_server()->GetURL(
      "b.test", net::test_server::GetFilePathWithReplacements(
                    "/shared_storage/module_with_custom_header.js",
                    SharedStorageCrossOriginWorkletResponseHeaderReplacement(
                        "Access-Control-Allow-Origin: *",
                        kEmptySharedStorageCrossOriginAllowedReplacement)));

  url::Origin custom_data_origin =
      url::Origin::Create(https_server()->GetURL("c.test", kSimplePagePath));

  EXPECT_TRUE(ExecJs(
      shell(),
      JsReplace("sharedStorage.createWorklet($1, {dataOrigin: $2})",
                module_script_url.spec(), custom_data_origin.Serialize())));

  TestSharedStorageWorkletHost* worklet_host =
      test_runtime_manager().GetAttachedWorkletHost();

  // The worklet host should reuse the main frame's process on Android without
  // strict site isolation; otherwise, it should use a new process.
  bool expected_use_new_process = AreAllSitesIsolatedForTesting();
  bool actual_use_new_process =
      (shell()->web_contents()->GetPrimaryMainFrame()->GetProcess() !=
       worklet_host->GetProcessHost());

  EXPECT_EQ(expected_use_new_process, actual_use_new_process);

  EXPECT_EQ(blink::mojom::SharedStorageWorkletCreationMethod::kCreateWorklet,
            worklet_host->creation_method());

  std::string custom_data_origin_str = custom_data_origin.Serialize();
  ExpectAccessObserved(
      {{AccessScope::kWindow, AccessMethod::kCreateWorklet, MainFrameId(),
        custom_data_origin_str,
        SharedStorageEventParams::CreateForCreateWorklet(
            module_script_url, custom_data_origin_str,
            /*worklet_ordinal=*/0, GetFirstWorkletHostDevToolsToken())}});
}

// Start a worklet under b.test using the script origin as the data
// originb(cross-origin to the main frame's origin), and then append a subframe
// under b.test. Assert that they share the same process.
IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest,
                       CreateWorkletAndSubframe_CrossOrigin) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  GURL module_script_url = https_server()->GetURL(
      "b.test", net::test_server::GetFilePathWithReplacements(
                    "/shared_storage/module_with_custom_header.js",
                    SharedStorageCrossOriginWorkletResponseHeaderReplacement(
                        "Access-Control-Allow-Origin: *",
                        "Shared-Storage-Cross-Origin-Worklet-Allowed: ?1")));

  EXPECT_TRUE(ExecJs(
      shell(),
      JsReplace(
          "sharedStorage.createWorklet($1, {dataOrigin: 'script-origin'})",
          module_script_url.spec())));

  TestSharedStorageWorkletHost* worklet_host =
      test_runtime_manager().GetAttachedWorkletHost();

  GURL iframe_url = https_server()->GetURL("b.test", "/empty.thml");
  FrameTreeNode* iframe_node =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), iframe_url);

  EXPECT_EQ(worklet_host->GetProcessHost(),
            iframe_node->current_frame_host()->GetProcess());
}

// Append a subframe under b.test (cross-origin to the main frame's origin), and
// then start a worklet under b.test using the script origin as the data origin.
// Assert that they share the same process.
IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest,
                       CreateSubframeAndWorklet_CrossOrigin) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  GURL iframe_url = https_server()->GetURL("b.test", "/empty.thml");
  FrameTreeNode* iframe_node =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), iframe_url);

  GURL module_script_url = https_server()->GetURL(
      "b.test", net::test_server::GetFilePathWithReplacements(
                    "/shared_storage/module_with_custom_header.js",
                    SharedStorageCrossOriginWorkletResponseHeaderReplacement(
                        "Access-Control-Allow-Origin: *",
                        "Shared-Storage-Cross-Origin-Worklet-Allowed: ?1")));

  EXPECT_TRUE(ExecJs(
      shell(),
      JsReplace(
          "sharedStorage.createWorklet($1, {dataOrigin: 'script-origin'})",
          module_script_url.spec())));

  TestSharedStorageWorkletHost* worklet_host =
      test_runtime_manager().GetAttachedWorkletHost();

  EXPECT_EQ(worklet_host->GetProcessHost(),
            iframe_node->current_frame_host()->GetProcess());
}

// Start one worklet under b.test using the script origin as the data origin
// (cross-origin to the main frame's origin), and then start another worklet
// under b.test. Assert that they share the same process.
IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest,
                       CreateTwoWorklets_CrossOrigin) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  GURL module_script_url = https_server()->GetURL(
      "b.test", net::test_server::GetFilePathWithReplacements(
                    "/shared_storage/module_with_custom_header.js",
                    SharedStorageCrossOriginWorkletResponseHeaderReplacement(
                        "Access-Control-Allow-Origin: *",
                        "Shared-Storage-Cross-Origin-Worklet-Allowed: ?1")));

  EXPECT_TRUE(ExecJs(
      shell(),
      JsReplace(
          "sharedStorage.createWorklet($1, {dataOrigin: 'script-origin'})",
          module_script_url.spec())));

  EXPECT_TRUE(ExecJs(
      shell(),
      JsReplace(
          "sharedStorage.createWorklet($1, {dataOrigin: 'script-origin'})",
          module_script_url.spec())));

  std::vector<TestSharedStorageWorkletHost*> worklet_hosts =
      test_runtime_manager().GetAttachedWorkletHosts();

  EXPECT_EQ(worklet_hosts.size(), 2u);
  EXPECT_EQ(worklet_hosts[0]->GetProcessHost(),
            worklet_hosts[1]->GetProcessHost());
}

// Start a worklet under b.test via createWorklet() using the script origin as
// the data origin, and then start a worklet under b.test's iframe. Assert that
// the data stored in the first worklet can be retrieved in the second worklet.
IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest,
                       CrossOriginWorklet_VerifyDataOrigin) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  GURL module_script_url = https_server()->GetURL(
      "b.test", net::test_server::GetFilePathWithReplacements(
                    "/shared_storage/module_with_custom_header.js",
                    SharedStorageCrossOriginWorkletResponseHeaderReplacement(
                        "Access-Control-Allow-Origin: *",
                        "Shared-Storage-Cross-Origin-Worklet-Allowed: ?1")));

  EXPECT_TRUE(ExecJs(shell(), JsReplace(R"(
      new Promise((resolve, reject) => {
        sharedStorage.createWorklet($1, {dataOrigin: 'script-origin'})
        .then((worklet) => {
          window.testWorklet = worklet;
          resolve();
        });
      })
    )",
                                        module_script_url.spec())));

  // Expect the run() operation.
  test_runtime_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EXPECT_TRUE(ExecJs(shell(), R"(
      window.testWorklet.run('test-operation', {
        data: {
          'set-key': 'key0',
          'set-value': 'value0'
        },
        keepAlive: true
      });
    )"));

  test_runtime_manager().GetAttachedWorkletHost()->WaitForWorkletResponses();

  GURL iframe_url = https_server()->GetURL("b.test", "/empty.thml");
  FrameTreeNode* iframe_node =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), iframe_url);

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(iframe_node->current_frame_host(), R"(
      console.log(await sharedStorage.get('key0'));
    )",
                         &out_script_url, /*expected_total_host_count=*/2);

  EXPECT_EQ(1u, console_observer.messages().size());
  EXPECT_EQ("value0",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
}

// Start a worklet with b.test script in a.test's context via createWorklet(),
// and then start a worklet with same-origin script in a.test's context. Assert
// that the data stored in the first worklet can be retrieved in the second
// worklet.
IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest,
                       CrossOriginScript_ContextDataOrigin_VerifyDataOrigin) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  GURL module_script_url = https_server()->GetURL(
      "b.test", net::test_server::GetFilePathWithReplacements(
                    "/shared_storage/module_with_custom_header.js",
                    SharedStorageCrossOriginWorkletResponseHeaderReplacement(
                        "Access-Control-Allow-Origin: *",
                        "Shared-Storage-Cross-Origin-Worklet-Allowed: ?1")));

  EXPECT_TRUE(ExecJs(shell(), JsReplace(R"(
      new Promise((resolve, reject) => {
        sharedStorage.createWorklet($1, {dataOrigin: 'context-origin'})
        .then((worklet) => {
          window.testWorklet = worklet;
          resolve();
        });
      })
    )",
                                        module_script_url.spec())));

  // Expect the run() operation.
  test_runtime_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EXPECT_TRUE(ExecJs(shell(), R"(
      window.testWorklet.run('test-operation', {
        data: {
          'set-key': 'key0',
          'set-value': 'value0'
        },
        keepAlive: true
      });
    )"));

  test_runtime_manager().GetAttachedWorkletHost()->WaitForWorkletResponses();

  GURL iframe_url = https_server()->GetURL("a.test", "/empty.thml");
  FrameTreeNode* iframe_node =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), iframe_url);

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(iframe_node->current_frame_host(), R"(
      console.log(await sharedStorage.get('key0'));
    )",
                         &out_script_url, /*expected_total_host_count=*/2);

  EXPECT_EQ(1u, console_observer.messages().size());
  EXPECT_EQ("value0",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
}

// Start a worklet with b.test script in a.test's context via createWorklet(),
// and then start a worklet with same-origin script in a.test's context. Assert
// that the data stored in the first worklet can be retrieved in the second
// worklet.
IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest,
                       CrossOriginScript_DefaultDataOrigin_VerifyDataOrigin) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  GURL module_script_url = https_server()->GetURL(
      "b.test", net::test_server::GetFilePathWithReplacements(
                    "/shared_storage/module_with_custom_header.js",
                    SharedStorageCrossOriginWorkletResponseHeaderReplacement(
                        "Access-Control-Allow-Origin: *",
                        "Shared-Storage-Cross-Origin-Worklet-Allowed: ?1")));

  EXPECT_TRUE(ExecJs(shell(), JsReplace(R"(
      new Promise((resolve, reject) => {
        sharedStorage.createWorklet($1)
        .then((worklet) => {
          window.testWorklet = worklet;
          resolve();
        });
      })
    )",
                                        module_script_url.spec())));

  // Expect the run() operation.
  test_runtime_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EXPECT_TRUE(ExecJs(shell(), R"(
      window.testWorklet.run('test-operation', {
        data: {
          'set-key': 'key0',
          'set-value': 'value0'
        },
        keepAlive: true
      });
    )"));

  test_runtime_manager().GetAttachedWorkletHost()->WaitForWorkletResponses();

  GURL iframe_url = https_server()->GetURL("a.test", "/empty.thml");
  FrameTreeNode* iframe_node =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), iframe_url);

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(iframe_node->current_frame_host(), R"(
      console.log(await sharedStorage.get('key0'));
    )",
                         &out_script_url, /*expected_total_host_count=*/2);

  EXPECT_EQ(1u, console_observer.messages().size());
  EXPECT_EQ("value0",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
}

// Start a worklet with b.test script in a.test's context with data origin
// c.test via createWorklet(), and then start a worklet with same-origin script
// in c.test's context. Assert that the data stored in the first worklet can be
// retrieved in the second worklet.
IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest,
                       CrossOriginScript_CustomDataOrigin_VerifyDataOrigin) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  GURL module_script_url = https_server()->GetURL(
      "b.test", net::test_server::GetFilePathWithReplacements(
                    "/shared_storage/module_with_custom_header.js",
                    SharedStorageCrossOriginWorkletResponseHeaderReplacement(
                        "Access-Control-Allow-Origin: *",
                        kEmptySharedStorageCrossOriginAllowedReplacement)));

  GURL custom_data_origin_url =
      https_server()->GetURL("c.test", kSimplePagePath);
  url::Origin custom_data_origin = url::Origin::Create(custom_data_origin_url);

  EXPECT_TRUE(ExecJs(shell(), JsReplace(R"(
      new Promise((resolve, reject) => {
        sharedStorage.createWorklet($1, {dataOrigin: $2})
        .then((worklet) => {
          window.testWorklet = worklet;
          resolve();
        });
      })
    )",
                                        module_script_url.spec(),
                                        custom_data_origin.Serialize())));

  // Expect the run() operation.
  test_runtime_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EXPECT_TRUE(ExecJs(shell(), R"(
      window.testWorklet.run('test-operation', {
        data: {
          'set-key': 'key0',
          'set-value': 'value0'
        },
        keepAlive: true
      });
    )"));

  test_runtime_manager().GetAttachedWorkletHost()->WaitForWorkletResponses();

  FrameTreeNode* iframe_node =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), custom_data_origin_url);

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(iframe_node->current_frame_host(), R"(
      console.log(await sharedStorage.get('key0'));
    )",
                         &out_script_url, /*expected_total_host_count=*/2);

  EXPECT_EQ(1u, console_observer.messages().size());
  EXPECT_EQ("value0",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest,
                       AddModule_CrossOriginScript_FailedCors) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  GURL module_script_url = https_server()->GetURL(
      "b.test", net::test_server::GetFilePathWithReplacements(
                    "/shared_storage/module_with_custom_header.js",
                    SharedStorageCrossOriginWorkletResponseHeaderReplacement(
                        kEmptyAccessControlAllowOriginReplacement,
                        kEmptySharedStorageCrossOriginAllowedReplacement)));

  EXPECT_THAT(EvalJs(shell(), JsReplace("sharedStorage.worklet.addModule($1)",
                                        module_script_url.spec())),
              EvalJsResult::ErrorIs(testing::HasSubstr("Failed to load")));
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest,
                       AddModule_CrossOriginScript_Success) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  GURL module_script_url = https_server()->GetURL(
      "b.test", net::test_server::GetFilePathWithReplacements(
                    "/shared_storage/module_with_custom_header.js",
                    SharedStorageCrossOriginWorkletResponseHeaderReplacement(
                        "Access-Control-Allow-Origin: *",
                        kEmptySharedStorageCrossOriginAllowedReplacement)));

  EXPECT_TRUE(ExecJs(shell(), JsReplace("sharedStorage.worklet.addModule($1)",
                                        module_script_url.spec())));
}

// Start a worklet with b.test script (cross-origin to the main frame's origin),
// but a.test data and then append a subframe under b.test. Assert that they
// share the same process.
IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest,
                       CreateWorkletAndSubframe_AddModule_CrossOriginScript) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  GURL module_script_url = https_server()->GetURL(
      "b.test", net::test_server::GetFilePathWithReplacements(
                    "/shared_storage/module_with_custom_header.js",
                    SharedStorageCrossOriginWorkletResponseHeaderReplacement(
                        "Access-Control-Allow-Origin: *",
                        kEmptySharedStorageCrossOriginAllowedReplacement)));

  EXPECT_TRUE(ExecJs(shell(), JsReplace("sharedStorage.worklet.addModule($1)",
                                        module_script_url.spec())));

  TestSharedStorageWorkletHost* worklet_host =
      test_runtime_manager().GetAttachedWorkletHost();

  GURL iframe_url = https_server()->GetURL("a.test", "/empty.thml");
  FrameTreeNode* iframe_node =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), iframe_url);

  EXPECT_EQ(worklet_host->GetProcessHost(),
            iframe_node->current_frame_host()->GetProcess());

  EXPECT_EQ(blink::mojom::SharedStorageWorkletCreationMethod::kAddModule,
            worklet_host->creation_method());
}

// Start a worklet with b.test script but a.test data, and then start a worklet
// under a.test's iframe. Assert that the data stored in the first worklet can
// be retrieved in the second worklet.
IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest,
                       AddModule_CrossOriginScript_VerifyDataOrigin) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  GURL module_script_url = https_server()->GetURL(
      "b.test", net::test_server::GetFilePathWithReplacements(
                    "/shared_storage/module_with_custom_header.js",
                    SharedStorageCrossOriginWorkletResponseHeaderReplacement(
                        "Access-Control-Allow-Origin: *",
                        kEmptySharedStorageCrossOriginAllowedReplacement)));

  EXPECT_TRUE(ExecJs(shell(), JsReplace("sharedStorage.worklet.addModule($1)",
                                        module_script_url.spec())));

  // Expect the run() operation.
  test_runtime_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.run('test-operation', {
        data: {
          'set-key': 'key0',
          'set-value': 'value0'
        },
        keepAlive: true
      });
    )"));

  test_runtime_manager().GetAttachedWorkletHost()->WaitForWorkletResponses();

  GURL iframe_url = https_server()->GetURL("a.test", "/empty.thml");
  FrameTreeNode* iframe_node =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), iframe_url);

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(iframe_node->current_frame_host(), R"(
      console.log(await sharedStorage.get('key0'));
    )",
                         &out_script_url, /*expected_total_host_count=*/2);

  EXPECT_EQ(1u, console_observer.messages().size());
  EXPECT_EQ("value0",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
}

INSTANTIATE_TEST_SUITE_P(All,
                         SharedStorageBrowserTest,
                         testing::Bool(),
                         describe_param);

class SharedStorageAllowURNsInIframesBrowserTest
    : public SharedStorageBrowserTestBase,
      public testing::WithParamInterface<bool> {
 public:
  SharedStorageAllowURNsInIframesBrowserTest() {
    allow_urns_in_frames_feature_.InitAndEnableFeature(
        blink::features::kAllowURNsInIframes);
    fenced_frame_api_change_feature_.InitWithFeatureState(
        blink::features::kFencedFramesAPIChanges, ResolveSelectURLToConfig());
  }

  bool ResolveSelectURLToConfig() override { return GetParam(); }

 private:
  base::test::ScopedFeatureList allow_urns_in_frames_feature_;
  base::test::ScopedFeatureList fenced_frame_api_change_feature_;
};

IN_PROC_BROWSER_TEST_P(SharedStorageAllowURNsInIframesBrowserTest,
                       RenderSelectURLResultInIframe) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  url::Origin shared_storage_origin =
      url::Origin::Create(https_server()->GetURL("b.test", kSimplePagePath));

  std::optional<GURL> urn_uuid =
      SelectFrom8URLsInContext(shared_storage_origin);
  ASSERT_TRUE(urn_uuid.has_value());

  FrameTreeNode* iframe_node =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), *urn_uuid);

  EXPECT_EQ(iframe_node->current_url(),
            https_server()->GetURL("b.test", "/fenced_frames/title1.html"));

  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin), kBudgetAllowed);

  GURL new_page_url = https_server()->GetURL("c.test", kSimplePagePath);

  TestNavigationObserver top_navigation_observer(shell()->web_contents());
  EXPECT_TRUE(
      ExecJs(iframe_node, JsReplace("top.location = $1", new_page_url)));
  top_navigation_observer.Wait();

  // After the top navigation, log(8)=3 bits should have been withdrawn from the
  // original shared storage origin.
  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin),
                   kBudgetAllowed - 3);
}

INSTANTIATE_TEST_SUITE_P(All,
                         SharedStorageAllowURNsInIframesBrowserTest,
                         testing::Bool(),
                         describe_param);

IN_PROC_BROWSER_TEST_P(
    SharedStorageBrowserTest,
    StringRoundTrip_SetThenGet_UnpairedSurrogatesArePreserved) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());
  GURL script_url =
      https_server()->GetURL("a.test", "/shared_storage/round_trip.js");

  EXPECT_TRUE(ExecJs(
      shell(), JsReplace("sharedStorage.worklet.addModule($1)", script_url)));

  std::vector<std::tuple<std::vector<uint16_t>, bool>> string_test_cases = {
      {std::vector<uint16_t>({0x0068}), true},          // letter 'h'
      {std::vector<uint16_t>({0xd800}), false},         // lone high surrogate
      {std::vector<uint16_t>({0xdc00}), false},         // lone low surrogate
      {std::vector<uint16_t>({0xd800, 0xdc00}), true},  // surrogate pair
  };

  for (size_t i = 0; i < string_test_cases.size(); ++i) {
    const std::vector<uint16_t>& char_code_array =
        std::get<0>(string_test_cases[i]);
    std::u16string test_string =
        std::u16string(char_code_array.begin(), char_code_array.end());
    base::Value::List char_code_values;
    for (uint16_t char_code : char_code_array) {
      char_code_values.Append(static_cast<int>(char_code));
    }

    const bool expected_valid = std::get<1>(string_test_cases[i]);

    // Check validity of UTF-16.
    std::string base_output;
    EXPECT_EQ(expected_valid,
              base::UTF16ToUTF8(test_string.c_str(), test_string.size(),
                                &base_output));

    // The dummy assignment is necessary, because otherwise under the hood,
    // `ExecJs` makes a call that tries to evaluate the most recent script
    // result as a `base::Value`, and `char_code_values` causes that to fail.
    EXPECT_TRUE(ExecJs(shell(), JsReplace(R"(window.charCodeArray = $1;
                                             window.dummyAssignment = 0;)",
                                          std::move(char_code_values))));

    // We will wait for 1 "worklet operation": `run()`.
    test_runtime_manager()
        .GetAttachedWorkletHost()
        ->SetExpectedWorkletResponsesCount(1);

    EXPECT_TRUE(ExecJs(shell(), R"(
        sharedStorage.run('set-get-operation',
                          {data: {'key': 'asValue',
                                  'valueCharCodeArray' : charCodeArray},
                           keepAlive: true});
      )"));

    test_runtime_manager().GetAttachedWorkletHost()->WaitForWorkletResponses();

    EXPECT_EQ(i + 1, console_observer.messages().size());
    EXPECT_THAT(base::UTF16ToUTF8(console_observer.messages().back().message),
                testing::HasSubstr("was retrieved: true"));
  }
}

IN_PROC_BROWSER_TEST_P(
    SharedStorageBrowserTest,
    StringRoundTrip_SetThenKeys_UnpairedSurrogatesArePreserved) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());
  GURL script_url =
      https_server()->GetURL("a.test", "/shared_storage/round_trip.js");

  EXPECT_TRUE(ExecJs(
      shell(), JsReplace("sharedStorage.worklet.addModule($1)", script_url)));

  std::vector<std::tuple<std::vector<uint16_t>, bool>> string_test_cases = {
      {std::vector<uint16_t>({0x0068}), true},          // letter 'h'
      {std::vector<uint16_t>({0xd800}), false},         // lone high surrogate
      {std::vector<uint16_t>({0xdc00}), false},         // lone low surrogate
      {std::vector<uint16_t>({0xd800, 0xdc00}), true},  // surrogate pair
  };

  for (size_t i = 0; i < string_test_cases.size(); ++i) {
    const std::vector<uint16_t>& char_code_array =
        std::get<0>(string_test_cases[i]);
    std::u16string test_string =
        std::u16string(char_code_array.begin(), char_code_array.end());
    base::Value::List char_code_values;
    for (uint16_t char_code : char_code_array) {
      char_code_values.Append(static_cast<int>(char_code));
    }

    const bool expected_valid = std::get<1>(string_test_cases[i]);

    // Check validity of UTF-16.
    std::string base_output;
    EXPECT_EQ(expected_valid,
              base::UTF16ToUTF8(test_string.c_str(), test_string.size(),
                                &base_output));

    // The dummy assignment is necessary, because otherwise under the hood,
    // `ExecJs` makes a call that tries to evaluate the most recent script
    // result as a `base::Value`, and `char_code_values` causes that to fail.
    EXPECT_TRUE(ExecJs(shell(), JsReplace(R"(window.charCodeArray = $1;
                                             window.dummyAssignment = 0;)",
                                          std::move(char_code_values))));

    // We will wait for 1 "worklet operation": `run()`.
    test_runtime_manager()
        .GetAttachedWorkletHost()
        ->SetExpectedWorkletResponsesCount(1);

    EXPECT_TRUE(ExecJs(shell(), R"(
        sharedStorage.run('set-keys-operation',
                          {data: {'keyCharCodeArray' : charCodeArray,
                                  'value': 'asKey'},
                           keepAlive: true});
      )"));

    test_runtime_manager().GetAttachedWorkletHost()->WaitForWorkletResponses();

    EXPECT_EQ(i + 1, console_observer.messages().size());
    EXPECT_THAT(base::UTF16ToUTF8(console_observer.messages().back().message),
                testing::HasSubstr("was retrieved: true"));
  }
}

IN_PROC_BROWSER_TEST_P(
    SharedStorageBrowserTest,
    StringRoundTrip_AppendThenDelete_UnpairedSurrogatesArePreserved) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());
  GURL script_url =
      https_server()->GetURL("a.test", "/shared_storage/round_trip.js");

  EXPECT_TRUE(ExecJs(
      shell(), JsReplace("sharedStorage.worklet.addModule($1)", script_url)));

  std::vector<std::tuple<std::vector<uint16_t>, bool>> string_test_cases = {
      {std::vector<uint16_t>({0x0068}), true},          // letter 'h'
      {std::vector<uint16_t>({0xd800}), false},         // lone high surrogate
      {std::vector<uint16_t>({0xdc00}), false},         // lone low surrogate
      {std::vector<uint16_t>({0xd800, 0xdc00}), true},  // surrogate pair
  };

  for (size_t i = 0; i < string_test_cases.size(); ++i) {
    const std::vector<uint16_t>& char_code_array =
        std::get<0>(string_test_cases[i]);
    std::u16string test_string =
        std::u16string(char_code_array.begin(), char_code_array.end());
    base::Value::List char_code_values;
    for (uint16_t char_code : char_code_array) {
      char_code_values.Append(static_cast<int>(char_code));
    }

    const bool expected_valid = std::get<1>(string_test_cases[i]);

    // Check validity of UTF-16.
    std::string base_output;
    EXPECT_EQ(expected_valid,
              base::UTF16ToUTF8(test_string.c_str(), test_string.size(),
                                &base_output));

    // The dummy assignment is necessary, because otherwise under the hood,
    // `ExecJs` makes a call that tries to evaluate the most recent script
    // result as a `base::Value`, and `char_code_values` causes that to fail.
    EXPECT_TRUE(ExecJs(shell(), JsReplace(R"(window.charCodeArray = $1;
                                             window.dummyAssignment = 0;)",
                                          std::move(char_code_values))));

    // We will wait for 1 "worklet operation": `run()`.
    test_runtime_manager()
        .GetAttachedWorkletHost()
        ->SetExpectedWorkletResponsesCount(1);

    EXPECT_TRUE(ExecJs(shell(), R"(
        sharedStorage.run('append-delete-operation',
                          {data: {'keyCharCodeArray' : charCodeArray,
                                  'value': 'asKey'},
                           keepAlive: true});
      )"));

    test_runtime_manager().GetAttachedWorkletHost()->WaitForWorkletResponses();

    EXPECT_EQ(i + 1, console_observer.messages().size());
    EXPECT_EQ(u"delete success: true",
              console_observer.messages().back().message);
  }
}

IN_PROC_BROWSER_TEST_P(
    SharedStorageBrowserTest,
    StringRoundTrip_AppendThenEntries_UnpairedSurrogatesArePreserved) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());
  GURL script_url =
      https_server()->GetURL("a.test", "/shared_storage/round_trip.js");

  EXPECT_TRUE(ExecJs(
      shell(), JsReplace("sharedStorage.worklet.addModule($1)", script_url)));

  std::vector<std::tuple<std::vector<uint16_t>, bool>> string_test_cases = {
      {std::vector<uint16_t>({0x0068}), true},          // letter 'h'
      {std::vector<uint16_t>({0xd800}), false},         // lone high surrogate
      {std::vector<uint16_t>({0xdc00}), false},         // lone low surrogate
      {std::vector<uint16_t>({0xd800, 0xdc00}), true},  // surrogate pair
  };

  for (size_t i = 0; i < string_test_cases.size(); ++i) {
    const std::vector<uint16_t>& char_code_array =
        std::get<0>(string_test_cases[i]);
    std::u16string test_string =
        std::u16string(char_code_array.begin(), char_code_array.end());
    base::Value::List char_code_values;
    for (uint16_t char_code : char_code_array) {
      char_code_values.Append(static_cast<int>(char_code));
    }

    const bool expected_valid = std::get<1>(string_test_cases[i]);

    // Check validity of UTF-16.
    std::string base_output;
    EXPECT_EQ(expected_valid,
              base::UTF16ToUTF8(test_string.c_str(), test_string.size(),
                                &base_output));

    // The dummy assignment is necessary, because otherwise under the hood,
    // `ExecJs` makes a call that tries to evaluate the most recent script
    // result as a `base::Value`, and `char_code_values` causes that to fail.
    EXPECT_TRUE(ExecJs(shell(), JsReplace(R"(window.charCodeArray = $1;
                                             window.dummyAssignment = 0;)",
                                          std::move(char_code_values))));

    // We will wait for 1 "worklet operation": `run()`.
    test_runtime_manager()
        .GetAttachedWorkletHost()
        ->SetExpectedWorkletResponsesCount(1);

    EXPECT_TRUE(ExecJs(shell(), R"(
        sharedStorage.run('append-entries-operation',
                          {data: {'key': 'asValue',
                                  'valueCharCodeArray': charCodeArray},
                           keepAlive: true});
      )"));

    test_runtime_manager().GetAttachedWorkletHost()->WaitForWorkletResponses();

    EXPECT_EQ(i + 1, console_observer.messages().size());
    EXPECT_THAT(base::UTF16ToUTF8(console_observer.messages().back().message),
                testing::HasSubstr("was retrieved: true"));
  }
}

class SharedStorageHeaderObserverBrowserTest
    : public SharedStorageBrowserTestBase {
 public:
  using OperationResult = storage::SharedStorageManager::OperationResult;

  void FinishSetup() override {
    https_server()->ServeFilesFromSourceDirectory(GetTestDataFilePath());
    https_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);

    auto observer = std::make_unique<TestSharedStorageHeaderObserver>(
        GetStoragePartition());
    observer_ = observer->GetMutableWeakPtr();
    static_cast<StoragePartitionImpl*>(GetStoragePartition())
        ->OverrideSharedStorageHeaderObserverForTesting(std::move(observer));
  }

  bool NavigateToURLWithResponse(
      Shell* window,
      const GURL& url,
      net::test_server::ControllableHttpResponse& response,
      net::HttpStatusCode http_status,
      const std::string& content_type = std::string("text/html"),
      const std::string& content = std::string(),
      const std::vector<std::string>& cookies = {},
      const std::vector<std::string>& extra_headers = {}) {
    auto* web_contents = window->web_contents();
    DCHECK(web_contents);

    // Prepare for the navigation.
    WaitForLoadStop(web_contents);
    TestNavigationObserver same_tab_observer(
        web_contents,
        /*expected_number_of_navigations=*/1,
        MessageLoopRunner::QuitMode::IMMEDIATE,
        /*ignore_uncommitted_navigations=*/false);
    if (!blink::IsRendererDebugURL(url)) {
      same_tab_observer.set_expected_initial_url(url);
    }

    // This mimics behavior of Shell::LoadURL...
    NavigationController::LoadURLParams params(url);
    params.transition_type = ui::PageTransitionFromInt(
        ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
    web_contents->GetController().LoadURLWithParams(params);
    web_contents->GetOutermostWebContents()->Focus();

    response.WaitForRequest();

    response.Send(http_status, content_type, content, cookies, extra_headers);
    response.Done();

    // Wait until the expected number of navigations finish.
    same_tab_observer.Wait();

    if (!IsLastCommittedEntryOfPageType(web_contents, PAGE_TYPE_NORMAL)) {
      return false;
    }

    bool is_same_url = web_contents->GetLastCommittedURL() == url;
    if (!is_same_url) {
      DLOG(WARNING) << "Expected URL " << url << " but observed "
                    << web_contents->GetLastCommittedURL();
    }
    return is_same_url;
  }

  std::string ReplacePortInString(std::string str) {
    return ::content::ReplacePortInString(str, port());
  }

  void SetUpResponsesAndNavigateMainPage(
      std::string main_hostname,
      std::string subresource_or_subframe_hostname,
      std::optional<std::string> shared_storage_permissions = std::nullopt,
      bool is_image = false,
      std::vector<std::string> redirect_hostnames = {}) {
    subresource_or_subframe_content_type_ =
        is_image ? "image/png" : "text/plain;charset=UTF-8";
    const char* subresource_or_subframe_path =
        is_image ? kPngPath : kTitle1Path;
    subresource_or_subframe_response_ =
        std::make_unique<net::test_server::ControllableHttpResponse>(
            https_server(), subresource_or_subframe_path);

    std::unique_ptr<net::test_server::ControllableHttpResponse> main_response;

    if (shared_storage_permissions.has_value()) {
      main_response =
          std::make_unique<net::test_server::ControllableHttpResponse>(
              https_server(), kSimplePagePath);
    }

    DCHECK_LT(redirect_hostnames.size(), 4u)
        << "You need to add more paths to "
           "`SetUpResponsesAndNavigateMainPage()`. Currently there are enough "
           "for up to 3 redirects.";
    std::vector<std::string> paths({kTitle2Path, kTitle3Path, kTitle4Path});
    for (size_t i = 0; i < redirect_hostnames.size(); i++) {
      auto response =
          std::make_unique<net::test_server::ControllableHttpResponse>(
              https_server(), paths[i]);
      redirected_responses_.push_back(std::move(response));
    }

    ASSERT_TRUE(https_server()->Start());

    main_url_ =
        https_server()->GetURL(std::move(main_hostname), kSimplePagePath);
    subresource_or_subframe_url_ =
        https_server()->GetURL(std::move(subresource_or_subframe_hostname),
                               subresource_or_subframe_path);
    subresource_or_subframe_origin_ =
        url::Origin::Create(subresource_or_subframe_url_);
    for (size_t i = 0; i < redirect_hostnames.size(); i++) {
      redirect_urls_.emplace_back(
          https_server()->GetURL(redirect_hostnames[i], paths[i]));
      redirect_origins_.emplace_back(
          url::Origin::Create(redirect_urls_.back()));
    }

    if (shared_storage_permissions.has_value()) {
      EXPECT_TRUE(NavigateToURLWithResponse(
          shell(), main_url_, *main_response,
          /*http_status=*/net::HTTP_OK,
          /*content_type=*/"text/plain;charset=UTF-8",
          /*content=*/{}, /*cookies=*/{}, /*extra_headers=*/
          {"Permissions-Policy: shared-storage=" +
           ReplacePortInString(
               std::move(shared_storage_permissions.value()))}));
    } else {
      EXPECT_TRUE(NavigateToURL(shell(), main_url_));
    }
  }

  void WaitForRequestAndSendResponse(
      net::test_server::ControllableHttpResponse& response,
      bool expect_writable_header,
      net::HttpStatusCode http_status,
      const std::string& content_type,
      const std::vector<std::string>& extra_headers) {
    response.WaitForRequest();
    if (expect_writable_header) {
      ASSERT_TRUE(base::Contains(response.http_request()->headers,
                                 "Sec-Shared-Storage-Writable"));
    } else {
      EXPECT_FALSE(base::Contains(response.http_request()->headers,
                                  "Sec-Shared-Storage-Writable"));
    }
    EXPECT_EQ(response.http_request()->content, "");
    response.Send(http_status, content_type,
                  /*content=*/{}, /*cookies=*/{}, extra_headers);
    response.Done();
  }

  void WaitForSubresourceOrSubframeRequestAndSendResponse(
      bool expect_writable_header,
      net::HttpStatusCode http_status,
      const std::vector<std::string>& extra_headers) {
    WaitForRequestAndSendResponse(
        *subresource_or_subframe_response_, expect_writable_header, http_status,
        subresource_or_subframe_content_type_, extra_headers);
  }

  void WaitForRedirectRequestAndSendResponse(
      bool expect_writable_header,
      net::HttpStatusCode http_status,
      const std::vector<std::string>& extra_headers,
      size_t redirect_index = 0) {
    ASSERT_LT(redirect_index, redirected_responses_.size());
    WaitForRequestAndSendResponse(
        *redirected_responses_[redirect_index], expect_writable_header,
        http_status, subresource_or_subframe_content_type_, extra_headers);
  }

  void FetchWithSharedStorageWritable(const ToRenderFrameHost& execution_target,
                                      const GURL& url) {
    EXPECT_TRUE(ExecJs(execution_target,
                       JsReplace(R"(
      fetch($1, {sharedStorageWritable: true});
    )",
                                 url.spec()),
                       EvalJsOptions::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  }

  void StartServerAndLoadMainURLWithSameOriginSubresourceOrSubframe(
      std::string main_hostname,
      std::string main_path) {
    ASSERT_TRUE(https_server()->Start());
    main_url_ =
        https_server()->GetURL(std::move(main_hostname), std::move(main_path));
    subresource_or_subframe_origin_ = url::Origin::Create(main_url_);
    EXPECT_TRUE(NavigateToURL(shell(), main_url_));
  }

  void CreateSharedStorageWritableImage(
      const ToRenderFrameHost& execution_target,
      const GURL& url) {
    EXPECT_TRUE(ExecJs(execution_target, JsReplace(R"(
      let img = document.createElement('img');
      img.src = $1;
      img.sharedStorageWritable = true;
      document.body.appendChild(img);
    )",
                                                   url.spec())));
  }

  void CreateSharedStorageWritableIframe(
      const ToRenderFrameHost& execution_target,
      const GURL& url) {
    EXPECT_TRUE(ExecJs(execution_target, JsReplace(R"(
      let frame = document.createElement('iframe');
      frame.sharedStorageWritable = true;
      frame.src = $1;
      document.body.appendChild(frame);
    )",
                                                   url.spec())));
  }

 protected:
  base::WeakPtr<TestSharedStorageHeaderObserver> observer_;
  std::unique_ptr<net::test_server::ControllableHttpResponse>
      subresource_or_subframe_response_;
  std::vector<std::unique_ptr<net::test_server::ControllableHttpResponse>>
      redirected_responses_;
  GURL main_url_;
  GURL subresource_or_subframe_url_;
  std::vector<GURL> redirect_urls_;
  url::Origin subresource_or_subframe_origin_;
  std::vector<url::Origin> redirect_origins_;
  std::string subresource_or_subframe_content_type_;
};

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Fetch_SameOrigin_PermissionsDefault_ClearSetAppend) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"a.test");

  FetchWithSharedStorageWritable(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Shared-Storage-Write: clear, "
       "set;key=\"hello\";value=\"world\";ignore_if_present, "
       "append;key=hello;value=there"});

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      console.log(await sharedStorage.get('hello'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("worldthere",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[1].message));

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(1);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front(),
            subresource_or_subframe_origin_);
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Fetch_SameOrigin_PermissionsNone) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"a.test",
      /*shared_storage_permissions=*/"()");

  FetchWithSharedStorageWritable(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/false,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: delete;key=a, set;value=wont;key=set"});

  // No operations are invoked.
  ASSERT_TRUE(observer_);
  EXPECT_TRUE(observer_->header_results().empty());
  EXPECT_TRUE(observer_->operations().empty());
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Fetch_SameOrigin_PermissionsAll_ClearSetAppend) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"a.test",
      /*shared_storage_permissions=*/"*");

  FetchWithSharedStorageWritable(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Shared-Storage-Write: clear, "
       "set;key=\"hello\";value=\"world\";ignore_if_present, "
       "append;key=hello;value=there"});

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      console.log(await sharedStorage.get('hello'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("worldthere",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[1].message));

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(1);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front(),
            subresource_or_subframe_origin_);

  std::vector<MethodWithOptionsPtr> methods_with_options;
  methods_with_options.push_back(MojomClearMethod());
  methods_with_options.push_back(MojomSetMethod(/*key=*/u"hello",
                                                /*value=*/u"world",
                                                /*ignore_if_present=*/true));
  methods_with_options.push_back(
      MojomAppendMethod(/*key=*/u"hello", /*value=*/u"there"));
  EXPECT_EQ(observer_->operations().size(), 1u);
  EXPECT_EQ(observer_->operations()[0],
            HeaderOperationSuccess(subresource_or_subframe_origin_,
                                   std::move(methods_with_options)));
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Fetch_SameOrigin_PermissionsSelf_ClearSetAppend) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"a.test",
      /*shared_storage_permissions=*/"self");

  FetchWithSharedStorageWritable(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Shared-Storage-Write: clear, "
       "set;key=\"hello\";value=\"world\";ignore_if_present, "
       "append;key=hello;value=there"});

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      console.log(await sharedStorage.get('hello'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("worldthere",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[1].message));

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(1);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front(),
            subresource_or_subframe_origin_);

  std::vector<MethodWithOptionsPtr> methods_with_options;
  methods_with_options.push_back(MojomClearMethod());
  methods_with_options.push_back(MojomSetMethod(/*key=*/u"hello",
                                                /*value=*/u"world",
                                                /*ignore_if_present=*/true));
  methods_with_options.push_back(
      MojomAppendMethod(/*key=*/u"hello", /*value=*/u"there"));
  EXPECT_EQ(observer_->operations().size(), 1u);
  EXPECT_EQ(observer_->operations()[0],
            HeaderOperationSuccess(subresource_or_subframe_origin_,
                                   std::move(methods_with_options)));
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Fetch_CrossOrigin_PermissionsDefault_ClearSetAppend) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"b.test");

  FetchWithSharedStorageWritable(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Shared-Storage-Write: clear, "
       "set;key=\"hello\";value=\"world\";ignore_if_present, "
       "append;key=hello;value=there"});

  // Create an iframe that's same-origin to the fetch URL.
  FrameTreeNode* iframe_node =
      CreateIFrame(PrimaryFrameTreeNodeRoot(),
                   https_server()->GetURL("b.test", kTitle2Path));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(iframe_node, R"(
      console.log(await sharedStorage.get('hello'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("worldthere",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[1].message));

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(1);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front(),
            subresource_or_subframe_origin_);

  std::vector<MethodWithOptionsPtr> methods_with_options;
  methods_with_options.push_back(MojomClearMethod());
  methods_with_options.push_back(MojomSetMethod(/*key=*/u"hello",
                                                /*value=*/u"world",
                                                /*ignore_if_present=*/true));
  methods_with_options.push_back(
      MojomAppendMethod(/*key=*/u"hello", /*value=*/u"there"));
  EXPECT_EQ(observer_->operations().size(), 1u);
  EXPECT_EQ(observer_->operations()[0],
            HeaderOperationSuccess(subresource_or_subframe_origin_,
                                   std::move(methods_with_options)));
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Fetch_CrossOrigin_PermissionsNone) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"b.test",
      /*shared_storage_permissions=*/"()");

  FetchWithSharedStorageWritable(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/false,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: delete;key=a, set;value=wont;key=set"});

  // No operations are invoked.
  ASSERT_TRUE(observer_);
  EXPECT_TRUE(observer_->header_results().empty());
  EXPECT_TRUE(observer_->operations().empty());
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Fetch_CrossOrigin_PermissionsAll_ClearSetAppend) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"b.test",
      /*shared_storage_permissions=*/"*");

  FetchWithSharedStorageWritable(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Shared-Storage-Write: clear, "
       "set;key=\"hello\";value=\"world\";ignore_if_present, "
       "append;key=hello;value=there"});

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(1);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front(),
            subresource_or_subframe_origin_);

  std::vector<MethodWithOptionsPtr> methods_with_options;
  methods_with_options.push_back(MojomClearMethod());
  methods_with_options.push_back(MojomSetMethod(/*key=*/u"hello",
                                                /*value=*/u"world",
                                                /*ignore_if_present=*/true));
  methods_with_options.push_back(
      MojomAppendMethod(/*key=*/u"hello", /*value=*/u"there"));
  EXPECT_EQ(observer_->operations().size(), 1u);
  EXPECT_EQ(observer_->operations()[0],
            HeaderOperationSuccess(subresource_or_subframe_origin_,
                                   std::move(methods_with_options)));

  // Create an iframe that's same-origin to the fetch URL.
  FrameTreeNode* iframe_node =
      CreateIFrame(PrimaryFrameTreeNodeRoot(),
                   https_server()->GetURL("b.test", kTitle2Path));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(iframe_node, R"(
      console.log(await sharedStorage.get('hello'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("worldthere",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[1].message));
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Fetch_CrossOrigin_PermissionsSelf) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"b.test",
      /*shared_storage_permissions=*/"self");

  FetchWithSharedStorageWritable(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/false,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: delete;key=a, set;value=wont;key=set"});

  // No operations are invoked.
  ASSERT_TRUE(observer_);
  EXPECT_TRUE(observer_->header_results().empty());
  EXPECT_TRUE(observer_->operations().empty());
}

IN_PROC_BROWSER_TEST_F(
    SharedStorageHeaderObserverBrowserTest,
    Fetch_CrossOrigin_Redirect_InititalAllowed_FinalAllowed_WriteInitial) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"b.test",
      /*shared_storage_permissions=*/
      "(self \"https://b.test:{{port}}\" \"https://c.test:{{port}}\")",
      /*is_image=*/false,
      /*redirect_hostnames=*/std::vector<std::string>({"c.test"}));

  FetchWithSharedStorageWritable(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_FOUND,
      /*extra_headers=*/
      {base::StrCat({"Location: ", redirect_urls_.back().spec()}),
       "Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: clear, "
       "set;key=\"hello\";value=\"world\";ignore_if_present, "
       "append;key=hello;value=there"});

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(1);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front(),
            subresource_or_subframe_origin_);

  std::vector<MethodWithOptionsPtr> methods_with_options1;
  methods_with_options1.push_back(MojomClearMethod());
  methods_with_options1.push_back(MojomSetMethod(/*key=*/u"hello",
                                                 /*value=*/u"world",
                                                 /*ignore_if_present=*/true));
  methods_with_options1.push_back(
      MojomAppendMethod(/*key=*/u"hello", /*value=*/u"there"));
  EXPECT_EQ(observer_->operations().size(), 1u);
  EXPECT_EQ(observer_->operations()[0],
            HeaderOperationSuccess(subresource_or_subframe_origin_,
                                   std::move(methods_with_options1)));

  WaitForRedirectRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Access-Control-Allow-Origin: *"});

  // There won't be additional operations invoked.
  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front(),
            subresource_or_subframe_origin_);

  std::vector<MethodWithOptionsPtr> methods_with_options2;
  methods_with_options2.push_back(MojomClearMethod());
  methods_with_options2.push_back(MojomSetMethod(/*key=*/u"hello",
                                                 /*value=*/u"world",
                                                 /*ignore_if_present=*/true));
  methods_with_options2.push_back(
      MojomAppendMethod(/*key=*/u"hello", /*value=*/u"there"));
  EXPECT_EQ(observer_->operations().size(), 1u);
  EXPECT_EQ(observer_->operations()[0],
            HeaderOperationSuccess(subresource_or_subframe_origin_,
                                   std::move(methods_with_options2)));

  // Create an iframe that's same-origin to the original fetch URL.
  FrameTreeNode* iframe_node1 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), subresource_or_subframe_url_);

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(iframe_node1, R"(
      console.log(await sharedStorage.get('hello'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("worldthere",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[1].message));
}

IN_PROC_BROWSER_TEST_F(
    SharedStorageHeaderObserverBrowserTest,
    Fetch_CrossOrigin_Redirect_InititalAllowed_FinalAllowed_WriteFinal) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"b.test",
      /*shared_storage_permissions=*/
      "(self \"https://b.test:{{port}}\" \"https://c.test:{{port}}\")",
      /*is_image=*/false,
      /*redirect_hostnames=*/std::vector<std::string>({"c.test"}));

  FetchWithSharedStorageWritable(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_FOUND,
      /*extra_headers=*/
      {base::StrCat({"Location: ", redirect_urls_.back().spec()}),
       "Access-Control-Allow-Origin: *"});

  WaitForRedirectRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: delete;key=a, set;value=will;key=set"});

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(1);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().back(), redirect_origins_.back());

  std::vector<MethodWithOptionsPtr> methods_with_options;
  methods_with_options.push_back(MojomDeleteMethod(/*key=*/u"a"));
  methods_with_options.push_back(MojomSetMethod(/*key=*/u"set",
                                                /*value=*/u"will",
                                                /*ignore_if_present=*/false));
  EXPECT_EQ(observer_->operations().size(), 1u);
  EXPECT_EQ(observer_->operations()[0],
            HeaderOperationSuccess(redirect_origins_.back(),
                                   std::move(methods_with_options)));

  // Create an iframe that's same-origin to the original fetch URL.
  FrameTreeNode* iframe_node1 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), subresource_or_subframe_url_);

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(iframe_node1, R"(
      console.log(await sharedStorage.get('set'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(2u, console_observer.messages().size());

  // Nothing was set in b.test's shared storage.
  EXPECT_EQ("undefined",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("0", base::UTF16ToUTF8(console_observer.messages()[1].message));

  // Create an iframe that's same-origin to the redirect URL.
  FrameTreeNode* iframe_node2 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), redirect_urls_.back());

  ExecuteScriptInWorklet(iframe_node2, R"(
      console.log(await sharedStorage.get('set'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url, /*expected_total_host_count=*/2u);

  // The entry was set in c.test's shared storage.
  EXPECT_EQ(4u, console_observer.messages().size());
  EXPECT_EQ("will", base::UTF16ToUTF8(console_observer.messages()[2].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[3].message));
}

IN_PROC_BROWSER_TEST_F(
    SharedStorageHeaderObserverBrowserTest,
    Fetch_CrossOrigin_Redirect_InititalAllowed_FinalAllowed_WriteBoth) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"b.test",
      /*shared_storage_permissions=*/
      "(self \"https://b.test:{{port}}\" \"https://c.test:{{port}}\")",
      /*is_image=*/false,
      /*redirect_hostnames=*/std::vector<std::string>({"c.test"}));

  FetchWithSharedStorageWritable(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_FOUND,
      /*extra_headers=*/
      {base::StrCat({"Location: ", redirect_urls_.back().spec()}),
       "Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: clear, "
       "set;key=\"hello\";value=\"world\";ignore_if_present, "
       "append;key=hello;value=there"});

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(1);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front(),
            subresource_or_subframe_origin_);

  std::vector<MethodWithOptionsPtr> methods_with_options1;
  methods_with_options1.push_back(MojomClearMethod());
  methods_with_options1.push_back(MojomSetMethod(/*key=*/u"hello",
                                                 /*value=*/u"world",
                                                 /*ignore_if_present=*/true));
  methods_with_options1.push_back(
      MojomAppendMethod(/*key=*/u"hello", /*value=*/u"there"));
  EXPECT_EQ(observer_->operations().size(), 1u);
  EXPECT_EQ(observer_->operations()[0],
            HeaderOperationSuccess(subresource_or_subframe_origin_,
                                   std::move(methods_with_options1)));

  WaitForRedirectRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: delete;key=a, set;value=will;key=set"});

  // There will now have been a total of 2 operations (1 previous, 1 current).
  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(2);

  EXPECT_EQ(observer_->header_results().size(), 2u);
  EXPECT_EQ(observer_->header_results().back(), redirect_origins_.back());

  std::vector<MethodWithOptionsPtr> methods_with_options2;
  methods_with_options2.push_back(MojomDeleteMethod(/*key=*/u"a"));
  methods_with_options2.push_back(MojomSetMethod(/*key=*/u"set",
                                                 /*value=*/u"will",
                                                 /*ignore_if_present=*/false));
  EXPECT_EQ(observer_->operations().size(), 2u);
  EXPECT_EQ(observer_->operations()[1],
            HeaderOperationSuccess(redirect_origins_.back(),
                                   std::move(methods_with_options2)));

  // Create an iframe that's same-origin to the original fetch URL.
  FrameTreeNode* iframe_node1 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), subresource_or_subframe_url_);

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(iframe_node1, R"(
      console.log(await sharedStorage.get('hello'));
      console.log(await sharedStorage.get('set'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(3u, console_observer.messages().size());
  EXPECT_EQ("worldthere",
            base::UTF16ToUTF8(console_observer.messages()[0].message));

  // Only one entry was set in b.test's shared storage.
  EXPECT_EQ("undefined",
            base::UTF16ToUTF8(console_observer.messages()[1].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[2].message));

  // Create an iframe that's same-origin to the redirect URL.
  FrameTreeNode* iframe_node2 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), redirect_urls_.back());

  ExecuteScriptInWorklet(iframe_node2, R"(
      console.log(await sharedStorage.get('set'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url, /*expected_total_host_count=*/2u);

  // One entry was set in c.test's shared storage.
  EXPECT_EQ(5u, console_observer.messages().size());
  EXPECT_EQ("will", base::UTF16ToUTF8(console_observer.messages()[3].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[4].message));
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Fetch_CrossOrigin_Redirect_InititalAllowed_FinalDenied) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"b.test",
      /*shared_storage_permissions=*/
      "(self \"https://b.test:{{port}}\")",
      /*is_image=*/false,
      /*redirect_hostnames=*/std::vector<std::string>({"c.test"}));

  FetchWithSharedStorageWritable(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_FOUND,
      /*extra_headers=*/
      {base::StrCat({"Location: ", redirect_urls_.back().spec()}),
       "Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: clear, "
       "set;key=\"hello\";value=\"world\";ignore_if_present, "
       "append;key=hello;value=there"});

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(1);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front(),
            subresource_or_subframe_origin_);

  std::vector<MethodWithOptionsPtr> methods_with_options1;
  methods_with_options1.push_back(MojomClearMethod());
  methods_with_options1.push_back(MojomSetMethod(/*key=*/u"hello",
                                                 /*value=*/u"world",
                                                 /*ignore_if_present=*/true));
  methods_with_options1.push_back(
      MojomAppendMethod(/*key=*/u"hello", /*value=*/u"there"));
  EXPECT_EQ(observer_->operations().size(), 1u);
  EXPECT_EQ(observer_->operations()[0],
            HeaderOperationSuccess(subresource_or_subframe_origin_,
                                   std::move(methods_with_options1)));

  WaitForRedirectRequestAndSendResponse(
      /*expect_writable_header=*/false,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: delete;key=a, set;value=wont;key=set"});

  // No new operations are invoked.
  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front(),
            subresource_or_subframe_origin_);

  std::vector<MethodWithOptionsPtr> methods_with_options2;
  methods_with_options2.push_back(MojomClearMethod());
  methods_with_options2.push_back(MojomSetMethod(/*key=*/u"hello",
                                                 /*value=*/u"world",
                                                 /*ignore_if_present=*/true));
  methods_with_options2.push_back(
      MojomAppendMethod(/*key=*/u"hello", /*value=*/u"there"));
  EXPECT_EQ(observer_->operations().size(), 1u);
  EXPECT_EQ(observer_->operations()[0],
            HeaderOperationSuccess(subresource_or_subframe_origin_,
                                   std::move(methods_with_options2)));

  // Create an iframe that's same-origin to the original fetch URL.
  FrameTreeNode* iframe_node1 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), subresource_or_subframe_url_);

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(iframe_node1, R"(
      console.log(await sharedStorage.get('hello'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("worldthere",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[1].message));

  // Create an iframe that's same-origin to the redirect URL.
  FrameTreeNode* iframe_node2 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), redirect_urls_.back());

  EXPECT_THAT(EvalJs(iframe_node2, R"(
        sharedStorage.worklet.addModule('/shared_storage/simple_module.js');
      )"),
              EvalJsResult::ErrorIs(testing::HasSubstr(
                  "The \"shared-storage\" Permissions Policy "
                  "denied the method")));
}

IN_PROC_BROWSER_TEST_F(
    SharedStorageHeaderObserverBrowserTest,
    Fetch_CrossOrigin_Redirect_InititalAllowed_IntermediateDenied_FinalAllowed_WriteInitialAndFinal) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"b.test",
      /*shared_storage_permissions=*/
      "(self \"https://b.test:{{port}}\" \"https://d.test:{{port}}\")",
      /*is_image=*/false,
      /*redirect_hostnames=*/std::vector<std::string>({"c.test", "d.test"}));

  FetchWithSharedStorageWritable(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_FOUND,
      /*extra_headers=*/
      {base::StrCat({"Location: ", redirect_urls_.front().spec()}),
       "Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: clear, "
       "set;key=\"hello\";value=\"world\";ignore_if_present, "
       "append;key=hello;value=there"});

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(1);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front(),
            subresource_or_subframe_origin_);

  std::vector<MethodWithOptionsPtr> methods_with_options1;
  methods_with_options1.push_back(MojomClearMethod());
  methods_with_options1.push_back(MojomSetMethod(/*key=*/u"hello",
                                                 /*value=*/u"world",
                                                 /*ignore_if_present=*/true));
  methods_with_options1.push_back(
      MojomAppendMethod(/*key=*/u"hello", /*value=*/u"there"));
  EXPECT_EQ(observer_->operations().size(), 1u);
  EXPECT_EQ(observer_->operations()[0],
            HeaderOperationSuccess(subresource_or_subframe_origin_,
                                   std::move(methods_with_options1)));

  WaitForRedirectRequestAndSendResponse(
      /*expect_writable_header=*/false,
      /*http_status=*/net::HTTP_TEMPORARY_REDIRECT,
      /*extra_headers=*/
      {base::StrCat({"Location: ", redirect_urls_.back().spec()}),
       "Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: append;key=wont;value=set"},
      /*redirect_index=*/0);

  WaitForRedirectRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: delete;key=a, set;value=will;key=set"},
      /*redirect_index=*/1);

  // There will now have been a total of 2 operations (1 previous, 1 current).
  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(2);

  EXPECT_EQ(observer_->header_results().size(), 2u);
  EXPECT_EQ(observer_->header_results().back(), redirect_origins_.back());

  std::vector<MethodWithOptionsPtr> methods_with_options2;
  methods_with_options2.push_back(MojomDeleteMethod(/*key=*/u"a"));
  methods_with_options2.push_back(MojomSetMethod(/*key=*/u"set",
                                                 /*value=*/u"will",
                                                 /*ignore_if_present=*/false));
  EXPECT_EQ(observer_->operations().size(), 2u);
  EXPECT_EQ(observer_->operations()[1],
            HeaderOperationSuccess(redirect_origins_.back(),
                                   std::move(methods_with_options2)));

  // Create an iframe that's same-origin to the original fetch URL.
  FrameTreeNode* iframe_node1 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), subresource_or_subframe_url_);

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(iframe_node1, R"(
      console.log(await sharedStorage.get('hello'));
      console.log(await sharedStorage.get('set'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(3u, console_observer.messages().size());
  EXPECT_EQ("worldthere",
            base::UTF16ToUTF8(console_observer.messages()[0].message));

  // Only one entry was set in b.test's shared storage.
  EXPECT_EQ("undefined",
            base::UTF16ToUTF8(console_observer.messages()[1].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[2].message));

  // Create an iframe that's same-origin to the first redirect URL.
  FrameTreeNode* iframe_node2 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), redirect_urls_.front());

  // c.test does not have permission to use shared storage.
  EXPECT_THAT(EvalJs(iframe_node2, R"(
        sharedStorage.worklet.addModule('/shared_storage/simple_module.js');
      )"),
              EvalJsResult::ErrorIs(testing::HasSubstr(
                  "The \"shared-storage\" Permissions Policy "
                  "denied the method")));

  // Create an iframe that's same-origin to the second redirect URL.
  FrameTreeNode* iframe_node3 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), redirect_urls_.back());

  ExecuteScriptInWorklet(iframe_node3, R"(
      console.log(await sharedStorage.get('set'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url, /*expected_total_host_count=*/2u);

  // One entry was set in d.test's shared storage.
  EXPECT_EQ(5u, console_observer.messages().size());
  EXPECT_EQ("will", base::UTF16ToUTF8(console_observer.messages()[3].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[4].message));
}

IN_PROC_BROWSER_TEST_F(
    SharedStorageHeaderObserverBrowserTest,
    Fetch_CrossOrigin_Redirect_InitialDenied_FinalAllowed_WriteFinal) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"b.test",
      /*shared_storage_permissions=*/
      "(self \"https://c.test:{{port}}\")",
      /*is_image=*/false,
      /*redirect_hostnames=*/std::vector<std::string>({"c.test"}));

  FetchWithSharedStorageWritable(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/false,
      /*http_status=*/net::HTTP_FOUND,
      /*extra_headers=*/
      {base::StrCat({"Location: ", redirect_urls_.back().spec()}),
       "Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: clear, "
       "set;key=\"hello\";value=\"world\";ignore_if_present, "
       "append;key=hello;value=there"});

  ASSERT_TRUE(observer_);

  // No operations are invoked.
  EXPECT_TRUE(observer_->header_results().empty());
  EXPECT_TRUE(observer_->operations().empty());

  WaitForRedirectRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: delete;key=a, set;value=will;key=set"});

  observer_->WaitForOperations(1);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front(), redirect_origins_.back());

  std::vector<MethodWithOptionsPtr> methods_with_options;
  methods_with_options.push_back(MojomDeleteMethod(/*key=*/u"a"));
  methods_with_options.push_back(MojomSetMethod(/*key=*/u"set",
                                                /*value=*/u"will",
                                                /*ignore_if_present=*/false));
  EXPECT_EQ(observer_->operations().size(), 1u);
  EXPECT_EQ(observer_->operations()[0],
            HeaderOperationSuccess(redirect_origins_.back(),
                                   std::move(methods_with_options)));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  // Create an iframe that's same-origin to the original fetch URL.
  FrameTreeNode* iframe_node1 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), subresource_or_subframe_url_);

  EXPECT_THAT(EvalJs(iframe_node1, R"(
        sharedStorage.worklet.addModule('/shared_storage/simple_module.js');
      )"),
              EvalJsResult::ErrorIs(testing::HasSubstr(
                  "The \"shared-storage\" Permissions Policy "
                  "denied the method")));

  // Create an iframe that's same-origin to the redirect URL.
  FrameTreeNode* iframe_node2 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), redirect_urls_.back());

  GURL out_script_url;
  ExecuteScriptInWorklet(iframe_node2, R"(
      console.log(await sharedStorage.get('set'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  // one key was set in c.test's shared storage.
  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("will", base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[1].message));
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Fetch_SameOrigin_PermissionsDefault_VerifyDelete) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"a.test");

  WebContentsConsoleObserver console_observer(shell()->web_contents());
  EXPECT_TRUE(ExecJs(shell(), R"(sharedStorage.set('hello', 'world');)"));

  GURL script_url =
      https_server()->GetURL("a.test", "/shared_storage/getter_module.js");

  EXPECT_TRUE(ExecJs(
      shell(), JsReplace("sharedStorage.worklet.addModule($1)", script_url)));

  // There is 1 more "worklet operation": `run()`.
  test_runtime_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EXPECT_TRUE(ExecJs(shell(), R"(
        sharedStorage.run('get-operation', {data: {'key': 'hello'},
                                            keepAlive: true});
      )"));

  test_runtime_manager().GetAttachedWorkletHost()->WaitForWorkletResponses();

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("sharedStorage.length(): 1",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("sharedStorage.get('hello'): world",
            base::UTF16ToUTF8(console_observer.messages()[1].message));

  FetchWithSharedStorageWritable(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Shared-Storage-Write: \"delete\";key=hello"});

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(1);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front(),
            subresource_or_subframe_origin_);

  std::vector<MethodWithOptionsPtr> methods_with_options;
  methods_with_options.push_back(MojomDeleteMethod(/*key=*/u"hello"));
  EXPECT_EQ(observer_->operations().size(), 1u);
  EXPECT_EQ(observer_->operations()[0],
            HeaderOperationSuccess(subresource_or_subframe_origin_,
                                   std::move(methods_with_options)));

  // There is 1 more "worklet operation": `run()`.
  test_runtime_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EXPECT_TRUE(ExecJs(shell(), R"(
        sharedStorage.run('get-operation', {data: {'key': 'hello'}});
      )"));

  test_runtime_manager().GetAttachedWorkletHost()->WaitForWorkletResponses();

  EXPECT_EQ(4u, console_observer.messages().size());
  EXPECT_EQ("sharedStorage.length(): 0",
            base::UTF16ToUTF8(console_observer.messages()[2].message));
  EXPECT_EQ("sharedStorage.get('hello'): undefined",
            base::UTF16ToUTF8(console_observer.messages()[3].message));
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Fetch_SameOrigin_PermissionsDefault_VerifyClear) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"a.test");

  WebContentsConsoleObserver console_observer(shell()->web_contents());
  EXPECT_TRUE(ExecJs(shell(), R"(sharedStorage.set('hello', 'world');)"));

  GURL script_url =
      https_server()->GetURL("a.test", "/shared_storage/getter_module.js");

  EXPECT_TRUE(ExecJs(
      shell(), JsReplace("sharedStorage.worklet.addModule($1)", script_url)));

  // There is 1 more "worklet operation": `run()`.
  test_runtime_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EXPECT_TRUE(ExecJs(shell(), R"(
        sharedStorage.run('get-operation', {data: {'key': 'hello'},
                                            keepAlive: true});
      )"));

  test_runtime_manager().GetAttachedWorkletHost()->WaitForWorkletResponses();

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("sharedStorage.length(): 1",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("sharedStorage.get('hello'): world",
            base::UTF16ToUTF8(console_observer.messages()[1].message));

  FetchWithSharedStorageWritable(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Shared-Storage-Write: \"clear\""});

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(1);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front(),
            subresource_or_subframe_origin_);

  std::vector<MethodWithOptionsPtr> methods_with_options;
  methods_with_options.push_back(MojomClearMethod());
  EXPECT_EQ(observer_->operations().size(), 1u);
  EXPECT_EQ(observer_->operations()[0],
            HeaderOperationSuccess(subresource_or_subframe_origin_,
                                   std::move(methods_with_options)));

  // There is 1 more "worklet operation": `run()`.
  test_runtime_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EXPECT_TRUE(ExecJs(shell(), R"(
        sharedStorage.run('get-operation', {data: {'key': 'hello'}});
      )"));

  test_runtime_manager().GetAttachedWorkletHost()->WaitForWorkletResponses();

  EXPECT_EQ(4u, console_observer.messages().size());
  EXPECT_EQ("sharedStorage.length(): 0",
            base::UTF16ToUTF8(console_observer.messages()[2].message));
  EXPECT_EQ("sharedStorage.get('hello'): undefined",
            base::UTF16ToUTF8(console_observer.messages()[3].message));
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Fetch_SameOrigin_PermissionsDefault_MultipleSet_Bytes) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"a.test");

  FetchWithSharedStorageWritable(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Shared-Storage-Write: set;key=:aGVsbG8=:;value=:d29ybGQ=:, "
       "set;value=:ZnJpZW5k:;key=:aGVsbG8=:;ignore_if_present=?0, "
       "set;ignore_if_present;key=hello;value=there"});

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(1);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front(),
            subresource_or_subframe_origin_);

  std::vector<MethodWithOptionsPtr> methods_with_options;
  methods_with_options.push_back(MojomSetMethod(/*key=*/u"hello",
                                                /*value=*/u"world",
                                                /*ignore_if_present=*/false));
  methods_with_options.push_back(MojomSetMethod(/*key=*/u"hello",
                                                /*value=*/u"friend",
                                                /*ignore_if_present=*/false));
  methods_with_options.push_back(MojomSetMethod(/*key=*/u"hello",
                                                /*value=*/u"there",
                                                /*ignore_if_present=*/true));
  EXPECT_EQ(observer_->operations().size(), 1u);
  EXPECT_EQ(observer_->operations()[0],
            HeaderOperationSuccess(subresource_or_subframe_origin_,
                                   std::move(methods_with_options)));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      console.log(await sharedStorage.get('hello'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("friend",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[1].message));
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       NetworkServiceRestarts_HeaderObserverContinuesWorking) {
  subresource_or_subframe_response_ =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server(), kTitle1Path);
  ASSERT_TRUE(https_server()->Start());

  if (IsInProcessNetworkService()) {
    return;
  }

  main_url_ = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url_));
  ASSERT_TRUE(observer_);

  SimulateNetworkServiceCrash();
  static_cast<StoragePartitionImpl*>(GetStoragePartition())
      ->FlushNetworkInterfaceForTesting();

  // We should still have an `observer_`.
  ASSERT_TRUE(observer_);

  // We need to reinitialize and renavigate to `main_url_` after network service
  // restart, if we want to prevent the shared storage operations below from
  // being deferred then dropped due to switching to a new
  // `NavigationOrDocumentHandle` for the main frame which hasn't yet seen a
  // commit.
  main_url_ = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url_));

  // We will still have an `observer_` after renavigating.
  ASSERT_TRUE(observer_);

  // We also need to reinitialize `subresource_or_subframe_url_` after network
  // service restart. Fetching with `sharedStorageWritable` works as expected.
  subresource_or_subframe_url_ = https_server()->GetURL("a.test", kTitle1Path);
  FetchWithSharedStorageWritable(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Shared-Storage-Write: clear, "
       "set;key=\"hello\";value=\"world\";ignore_if_present, "
       "append;key=hello;value=there"});

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(1);

  subresource_or_subframe_origin_ =
      url::Origin::Create(subresource_or_subframe_url_);
  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front(),
            subresource_or_subframe_origin_);

  std::vector<MethodWithOptionsPtr> methods_with_options;
  methods_with_options.push_back(MojomClearMethod());
  methods_with_options.push_back(MojomSetMethod(/*key=*/u"hello",
                                                /*value=*/u"world",
                                                /*ignore_if_present=*/true));
  methods_with_options.push_back(
      MojomAppendMethod(/*key=*/u"hello", /*value=*/u"there"));
  EXPECT_EQ(observer_->operations().size(), 1u);
  EXPECT_EQ(observer_->operations()[0],
            HeaderOperationSuccess(subresource_or_subframe_origin_,
                                   std::move(methods_with_options)));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      console.log(await sharedStorage.get('hello'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("worldthere",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[1].message));
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       InvalidHeader_NoOperationsInvoked) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"a.test");

  FetchWithSharedStorageWritable(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Shared-Storage-Write: clear, invalid?item"});

  // No operations are invoked.
  ASSERT_TRUE(observer_);
  EXPECT_TRUE(observer_->header_results().empty());
  EXPECT_TRUE(observer_->operations().empty());

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(1u, console_observer.messages().size());
  EXPECT_EQ("0", base::UTF16ToUTF8(console_observer.messages()[0].message));
}

IN_PROC_BROWSER_TEST_F(
    SharedStorageHeaderObserverBrowserTest,
    ParsableUnrecognizedItemSkipped_RecognizedOperationsInvoked) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"a.test");

  FetchWithSharedStorageWritable(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Shared-Storage-Write: clear, unrecognized;unknown_param=1,"
       "set;key=\"hello\";value=\"world\";ignore_if_present, "
       "append;key=hello;value=there"});

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(1);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front(),
            subresource_or_subframe_origin_);

  std::vector<MethodWithOptionsPtr> methods_with_options;
  methods_with_options.push_back(MojomClearMethod());
  methods_with_options.push_back(MojomSetMethod(/*key=*/u"hello",
                                                /*value=*/u"world",
                                                /*ignore_if_present=*/true));
  methods_with_options.push_back(
      MojomAppendMethod(/*key=*/u"hello", /*value=*/u"there"));
  EXPECT_EQ(observer_->operations().size(), 1u);
  EXPECT_EQ(observer_->operations()[0],
            HeaderOperationSuccess(subresource_or_subframe_origin_,
                                   std::move(methods_with_options)));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      console.log(await sharedStorage.get('hello'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("worldthere",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[1].message));
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       ExtraParametersIgnored) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"a.test");

  FetchWithSharedStorageWritable(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Shared-Storage-Write: clear;unknown_param=1,"
       "set;another_unknown=willIgnore;key=\"hello\";value=\"world\", "
       "append;key=extra;key=hello;value=there;ignore_if_present;pi=3.14,"
       "delete;value=ignored;key=toDelete;ignore_if_present=?0"});

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      console.log(await sharedStorage.get('hello'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("worldthere",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[1].message));

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(1);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front(),
            subresource_or_subframe_origin_);

  std::vector<MethodWithOptionsPtr> methods_with_options;
  methods_with_options.push_back(MojomClearMethod());
  methods_with_options.push_back(MojomSetMethod(/*key=*/u"hello",
                                                /*value=*/u"world",
                                                /*ignore_if_present=*/false));
  methods_with_options.push_back(
      MojomAppendMethod(/*key=*/u"hello", /*value=*/u"there"));
  methods_with_options.push_back(MojomDeleteMethod(/*key=*/u"toDelete"));
  EXPECT_EQ(observer_->operations().size(), 1u);
  EXPECT_EQ(observer_->operations()[0],
            HeaderOperationSuccess(subresource_or_subframe_origin_,
                                   std::move(methods_with_options)));
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       KeyOrValueLengthInvalid_ItemSkipped) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"a.test");

  FetchWithSharedStorageWritable(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {base::StrCat({"Shared-Storage-Write: clear, ",
                     "set;key=\"\";value=v,append;key=\"\";value=v,",
                     "delete;key=\"\",clear"})});

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(1);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front(),
            subresource_or_subframe_origin_);

  std::vector<MethodWithOptionsPtr> methods_with_options;
  methods_with_options.push_back(MojomClearMethod());
  methods_with_options.push_back(MojomClearMethod());
  EXPECT_EQ(observer_->operations().size(), 1u);
  EXPECT_EQ(observer_->operations()[0],
            HeaderOperationSuccess(subresource_or_subframe_origin_,
                                   std::move(methods_with_options)));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(1u, console_observer.messages().size());
  EXPECT_EQ("0", base::UTF16ToUTF8(console_observer.messages()[0].message));
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Image_SameOrigin_PermissionsDefault) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"a.test",
      /*shared_storage_permissions=*/std::nullopt,
      /*is_image=*/true);

  CreateSharedStorageWritableImage(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Shared-Storage-Write: set;key=a;value=b"});

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      console.log(await sharedStorage.get('a'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("b", base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[1].message));

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(1);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front(),
            subresource_or_subframe_origin_);

  std::vector<MethodWithOptionsPtr> methods_with_options;
  methods_with_options.push_back(MojomSetMethod(/*key=*/u"a", /*value=*/u"b",
                                                /*ignore_if_present=*/false));
  EXPECT_EQ(observer_->operations().size(), 1u);
  EXPECT_EQ(observer_->operations()[0],
            HeaderOperationSuccess(subresource_or_subframe_origin_,
                                   std::move(methods_with_options)));
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Image_SameOrigin_PermissionsNone) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"a.test",
      /*shared_storage_permissions=*/"()",
      /*is_image=*/true);

  CreateSharedStorageWritableImage(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/false,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: set;key=a;value=b"});

  // No operations are invoked.
  ASSERT_TRUE(observer_);
  EXPECT_TRUE(observer_->header_results().empty());
  EXPECT_TRUE(observer_->operations().empty());
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Image_SameOrigin_PermissionsAll) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"a.test",
      /*shared_storage_permissions=*/"*",
      /*is_image=*/true);

  CreateSharedStorageWritableImage(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: set;key=a;value=b"});

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      console.log(await sharedStorage.get('a'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("b", base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[1].message));

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(1);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front(),
            subresource_or_subframe_origin_);

  std::vector<MethodWithOptionsPtr> methods_with_options;
  methods_with_options.push_back(MojomSetMethod(/*key=*/u"a", /*value=*/u"b",
                                                /*ignore_if_present=*/false));
  EXPECT_EQ(observer_->operations().size(), 1u);
  EXPECT_EQ(observer_->operations()[0],
            HeaderOperationSuccess(subresource_or_subframe_origin_,
                                   std::move(methods_with_options)));
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Image_SameOrigin_PermissionsSelf) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"a.test",
      /*shared_storage_permissions=*/"self",
      /*is_image=*/true);

  CreateSharedStorageWritableImage(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: set;key=a;value=b"});

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      console.log(await sharedStorage.get('a'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("b", base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[1].message));

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(1);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front(),
            subresource_or_subframe_origin_);

  std::vector<MethodWithOptionsPtr> methods_with_options;
  methods_with_options.push_back(MojomSetMethod(/*key=*/u"a", /*value=*/u"b",
                                                /*ignore_if_present=*/false));
  EXPECT_EQ(observer_->operations().size(), 1u);
  EXPECT_EQ(observer_->operations()[0],
            HeaderOperationSuccess(subresource_or_subframe_origin_,
                                   std::move(methods_with_options)));
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Image_CrossOrigin_PermissionsDefault) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"b.test",
      /*shared_storage_permissions=*/std::nullopt,
      /*is_image=*/true);

  CreateSharedStorageWritableImage(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Shared-Storage-Write: set;key=a;value=b"});

  // Create iframe that's same-origin to the image URL.
  FrameTreeNode* iframe_node =
      CreateIFrame(PrimaryFrameTreeNodeRoot(),
                   https_server()->GetURL("b.test", kTitle2Path));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(iframe_node, R"(
      console.log(await sharedStorage.get('a'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("b", base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[1].message));

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(1);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front(),
            subresource_or_subframe_origin_);

  std::vector<MethodWithOptionsPtr> methods_with_options;
  methods_with_options.push_back(MojomSetMethod(/*key=*/u"a", /*value=*/u"b",
                                                /*ignore_if_present=*/false));
  EXPECT_EQ(observer_->operations().size(), 1u);
  EXPECT_EQ(observer_->operations()[0],
            HeaderOperationSuccess(subresource_or_subframe_origin_,
                                   std::move(methods_with_options)));
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Image_CrossOrigin_PermissionsNone) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"b.test",
      /*shared_storage_permissions=*/"()",
      /*is_image=*/true);

  CreateSharedStorageWritableImage(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/false,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: set;key=a;value=b"});

  // No operations are invoked.
  ASSERT_TRUE(observer_);
  EXPECT_TRUE(observer_->header_results().empty());
  EXPECT_TRUE(observer_->operations().empty());
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Image_CrossOrigin_PermissionsAll) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"b.test",
      /*shared_storage_permissions=*/"*",
      /*is_image=*/true);

  CreateSharedStorageWritableImage(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: set;key=a;value=b"});

  // Create iframe that's same-origin to the image URL.
  FrameTreeNode* iframe_node =
      CreateIFrame(PrimaryFrameTreeNodeRoot(),
                   https_server()->GetURL("b.test", kTitle2Path));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(iframe_node, R"(
      console.log(await sharedStorage.get('a'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("b", base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[1].message));

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(1);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front(),
            subresource_or_subframe_origin_);

  std::vector<MethodWithOptionsPtr> methods_with_options;
  methods_with_options.push_back(MojomSetMethod(/*key=*/u"a", /*value=*/u"b",
                                                /*ignore_if_present=*/false));
  EXPECT_EQ(observer_->operations().size(), 1u);
  EXPECT_EQ(observer_->operations()[0],
            HeaderOperationSuccess(subresource_or_subframe_origin_,
                                   std::move(methods_with_options)));
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Image_CrossOrigin_PermissionsSelf) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"b.test",
      /*shared_storage_permissions=*/"self",
      /*is_image=*/true);

  CreateSharedStorageWritableImage(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/false,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: set;key=a;value=b"});

  // No operations are invoked.
  ASSERT_TRUE(observer_);
  EXPECT_TRUE(observer_->header_results().empty());
  EXPECT_TRUE(observer_->operations().empty());
}

IN_PROC_BROWSER_TEST_F(
    SharedStorageHeaderObserverBrowserTest,
    Image_CrossOrigin_Redirect_InititalAllowed_FinalAllowed_WriteInitial) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"b.test",
      /*shared_storage_permissions=*/
      "(self \"https://b.test:{{port}}\" \"https://c.test:{{port}}\")",
      /*is_image=*/true,
      /*redirect_hostnames=*/std::vector<std::string>({"c.test"}));

  CreateSharedStorageWritableImage(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_FOUND,
      /*extra_headers=*/
      {base::StrCat({"Location: ", redirect_urls_.back().spec()}),
       "Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: clear, "
       "set;key=\"hello\";value=\"world\";ignore_if_present, "
       "append;key=hello;value=there"});

  WaitForRedirectRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Access-Control-Allow-Origin: *"});

  // Create an iframe that's same-origin to the original image URL.
  FrameTreeNode* iframe_node2 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), subresource_or_subframe_url_);

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(iframe_node2, R"(
      console.log(await sharedStorage.get('hello'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("worldthere",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[1].message));

  // There won't be additional operations invoked from the redirect, just the
  // original 3.
  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(1);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front(),
            subresource_or_subframe_origin_);

  std::vector<MethodWithOptionsPtr> methods_with_options;
  methods_with_options.push_back(MojomClearMethod());
  methods_with_options.push_back(MojomSetMethod(/*key=*/u"hello",
                                                /*value=*/u"world",
                                                /*ignore_if_present=*/true));
  methods_with_options.push_back(
      MojomAppendMethod(/*key=*/u"hello", /*value=*/u"there"));
  EXPECT_EQ(observer_->operations().size(), 1u);
  EXPECT_EQ(observer_->operations()[0],
            HeaderOperationSuccess(subresource_or_subframe_origin_,
                                   std::move(methods_with_options)));
}

IN_PROC_BROWSER_TEST_F(
    SharedStorageHeaderObserverBrowserTest,
    Image_CrossOrigin_Redirect_InititalAllowed_FinalAllowed_WriteFinal) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"b.test",
      /*shared_storage_permissions=*/
      "(self \"https://b.test:{{port}}\" \"https://c.test:{{port}}\")",
      /*is_image=*/true,
      /*redirect_hostnames=*/std::vector<std::string>({"c.test"}));

  CreateSharedStorageWritableImage(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_FOUND,
      /*extra_headers=*/
      {base::StrCat({"Location: ", redirect_urls_.back().spec()}),
       "Access-Control-Allow-Origin: *"});

  WaitForRedirectRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: delete;key=a, set;value=will;key=set"});

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(1);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().back(), redirect_origins_.back());

  std::vector<MethodWithOptionsPtr> methods_with_options;
  methods_with_options.push_back(MojomDeleteMethod(/*key=*/u"a"));
  methods_with_options.push_back(MojomSetMethod(/*key=*/u"set",
                                                /*value=*/u"will",
                                                /*ignore_if_present=*/false));
  EXPECT_EQ(observer_->operations().size(), 1u);
  EXPECT_EQ(observer_->operations()[0],
            HeaderOperationSuccess(redirect_origins_.back(),
                                   std::move(methods_with_options)));

  // Create an iframe that's same-origin to the original image URL.
  FrameTreeNode* iframe_node1 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), subresource_or_subframe_url_);

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(iframe_node1, R"(
      console.log(await sharedStorage.get('set'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(2u, console_observer.messages().size());

  // Nothing was set in b.test's shared storage.
  EXPECT_EQ("undefined",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("0", base::UTF16ToUTF8(console_observer.messages()[1].message));

  // Create another iframe that's same-origin to the redirect URL.
  FrameTreeNode* iframe_node2 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), redirect_urls_.back());

  ExecuteScriptInWorklet(iframe_node2, R"(
      console.log(await sharedStorage.get('set'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url, /*expected_total_host_count=*/2u);

  // The entry was set in c.test's shared storage.
  EXPECT_EQ(4u, console_observer.messages().size());
  EXPECT_EQ("will", base::UTF16ToUTF8(console_observer.messages()[2].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[3].message));
}

IN_PROC_BROWSER_TEST_F(
    SharedStorageHeaderObserverBrowserTest,
    Image_CrossOrigin_Redirect_InititalAllowed_FinalAllowed_WriteBoth) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"b.test",
      /*shared_storage_permissions=*/
      "(self \"https://b.test:{{port}}\" \"https://c.test:{{port}}\")",
      /*is_image=*/true,
      /*redirect_hostnames=*/std::vector<std::string>({"c.test"}));

  CreateSharedStorageWritableImage(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_FOUND,
      /*extra_headers=*/
      {base::StrCat({"Location: ", redirect_urls_.back().spec()}),
       "Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: clear, "
       "set;key=\"hello\";value=\"world\";ignore_if_present, "
       "append;key=hello;value=there"});

  WaitForRedirectRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: delete;key=a, set;value=will;key=set"});

  // There will now have been a total of 2 operations (1 previous, 1 current).
  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(2);

  EXPECT_EQ(observer_->header_results().size(), 2u);
  EXPECT_EQ(observer_->header_results().back(), redirect_origins_.back());
  EXPECT_EQ(observer_->operations().size(), 2u);

  std::vector<MethodWithOptionsPtr> methods_with_options1;
  methods_with_options1.push_back(MojomClearMethod());
  methods_with_options1.push_back(MojomSetMethod(/*key=*/u"hello",
                                                 /*value=*/u"world",
                                                 /*ignore_if_present=*/true));
  methods_with_options1.push_back(
      MojomAppendMethod(/*key=*/u"hello", /*value=*/u"there"));
  EXPECT_EQ(observer_->operations()[0],
            HeaderOperationSuccess(subresource_or_subframe_origin_,
                                   std::move(methods_with_options1)));

  std::vector<MethodWithOptionsPtr> methods_with_options2;
  methods_with_options2.push_back(MojomDeleteMethod(/*key=*/u"a"));
  methods_with_options2.push_back(MojomSetMethod(/*key=*/u"set",
                                                 /*value=*/u"will",
                                                 /*ignore_if_present=*/false));
  EXPECT_EQ(observer_->operations()[1],
            HeaderOperationSuccess(redirect_origins_.back(),
                                   std::move(methods_with_options2)));

  // Create an iframe that's same-origin to the original image URL.
  FrameTreeNode* iframe_node1 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), subresource_or_subframe_url_);

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(iframe_node1, R"(
      console.log(await sharedStorage.get('hello'));
      console.log(await sharedStorage.get('set'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(3u, console_observer.messages().size());
  EXPECT_EQ("worldthere",
            base::UTF16ToUTF8(console_observer.messages()[0].message));

  // Only one entry was set in b.test's shared storage.
  EXPECT_EQ("undefined",
            base::UTF16ToUTF8(console_observer.messages()[1].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[2].message));

  // Create another iframe that's same-origin to the redirect URL.
  FrameTreeNode* iframe_node2 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), redirect_urls_.back());

  ExecuteScriptInWorklet(iframe_node2, R"(
      console.log(await sharedStorage.get('set'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url, /*expected_total_host_count=*/2u);

  // One entry was set in c.test's shared storage.
  EXPECT_EQ(5u, console_observer.messages().size());
  EXPECT_EQ("will", base::UTF16ToUTF8(console_observer.messages()[3].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[4].message));
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Image_CrossOrigin_Redirect_InititalAllowed_FinalDenied) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"b.test",
      /*shared_storage_permissions=*/
      "(self \"https://b.test:{{port}}\")",
      /*is_image=*/true,
      /*redirect_hostnames=*/std::vector<std::string>({"c.test"}));

  CreateSharedStorageWritableImage(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_FOUND,
      /*extra_headers=*/
      {base::StrCat({"Location: ", redirect_urls_.back().spec()}),
       "Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: clear, "
       "set;key=\"hello\";value=\"world\";ignore_if_present, "
       "append;key=hello;value=there"});

  WaitForRedirectRequestAndSendResponse(
      /*expect_writable_header=*/false,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: delete;key=a, set;value=wont;key=set"});

  // There won't be additional operations invoked from the redirect, just the
  // original 3.
  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(1);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front(),
            subresource_or_subframe_origin_);

  std::vector<MethodWithOptionsPtr> methods_with_options;
  methods_with_options.push_back(MojomClearMethod());
  methods_with_options.push_back(MojomSetMethod(/*key=*/u"hello",
                                                /*value=*/u"world",
                                                /*ignore_if_present=*/true));
  methods_with_options.push_back(
      MojomAppendMethod(/*key=*/u"hello", /*value=*/u"there"));
  EXPECT_EQ(observer_->operations().size(), 1u);
  EXPECT_EQ(observer_->operations()[0],
            HeaderOperationSuccess(subresource_or_subframe_origin_,
                                   std::move(methods_with_options)));

  // Create an iframe that's same-origin to the original image URL.
  FrameTreeNode* iframe_node1 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), subresource_or_subframe_url_);

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(iframe_node1, R"(
      console.log(await sharedStorage.get('hello'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("worldthere",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[1].message));

  // Create another iframe that's same-origin to the redirect URL.
  FrameTreeNode* iframe_node2 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), redirect_urls_.back());

  EXPECT_THAT(EvalJs(iframe_node2, R"(
        sharedStorage.worklet.addModule('/shared_storage/simple_module.js');
      )"),
              EvalJsResult::ErrorIs(testing::HasSubstr(
                  "The \"shared-storage\" Permissions Policy "
                  "denied the method")));
}

IN_PROC_BROWSER_TEST_F(
    SharedStorageHeaderObserverBrowserTest,
    Image_CrossOrigin_Redirect_InititalAllowed_IntermediateDenied_FinalAllowed_WriteInitialAndFinal) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"b.test",
      /*shared_storage_permissions=*/
      "(self \"https://b.test:{{port}}\" \"https://d.test:{{port}}\")",
      /*is_image=*/true,
      /*redirect_hostnames=*/std::vector<std::string>({"c.test", "d.test"}));

  CreateSharedStorageWritableImage(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_FOUND,
      /*extra_headers=*/
      {base::StrCat({"Location: ", redirect_urls_.front().spec()}),
       "Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: clear, "
       "set;key=\"hello\";value=\"world\";ignore_if_present, "
       "append;key=hello;value=there"});

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(1);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front(),
            subresource_or_subframe_origin_);

  std::vector<MethodWithOptionsPtr> methods_with_options1;
  methods_with_options1.push_back(MojomClearMethod());
  methods_with_options1.push_back(MojomSetMethod(/*key=*/u"hello",
                                                 /*value=*/u"world",
                                                 /*ignore_if_present=*/true));
  methods_with_options1.push_back(
      MojomAppendMethod(/*key=*/u"hello", /*value=*/u"there"));
  EXPECT_EQ(observer_->operations().size(), 1u);
  EXPECT_EQ(observer_->operations()[0],
            HeaderOperationSuccess(subresource_or_subframe_origin_,
                                   std::move(methods_with_options1)));

  WaitForRedirectRequestAndSendResponse(
      /*expect_writable_header=*/false,
      /*http_status=*/net::HTTP_TEMPORARY_REDIRECT,
      /*extra_headers=*/
      {base::StrCat({"Location: ", redirect_urls_.back().spec()}),
       "Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: append;key=wont;value=set"},
      /*redirect_index=*/0);

  WaitForRedirectRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: delete;key=a, set;value=will;key=set"},
      /*redirect_index=*/1);

  // There will now have been a total of 2 operations (1 previous, 1 current).
  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(2);

  EXPECT_EQ(observer_->header_results().size(), 2u);
  EXPECT_EQ(observer_->header_results().back(), redirect_origins_.back());

  std::vector<MethodWithOptionsPtr> methods_with_options2;
  methods_with_options2.push_back(MojomDeleteMethod(/*key=*/u"a"));
  methods_with_options2.push_back(MojomSetMethod(/*key=*/u"set",
                                                 /*value=*/u"will",
                                                 /*ignore_if_present=*/false));
  EXPECT_EQ(observer_->operations().size(), 2u);
  EXPECT_EQ(observer_->operations()[1],
            HeaderOperationSuccess(redirect_origins_.back(),
                                   std::move(methods_with_options2)));

  // Create an iframe that's same-origin to the original image URL.
  FrameTreeNode* iframe_node1 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), subresource_or_subframe_url_);

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(iframe_node1, R"(
      console.log(await sharedStorage.get('hello'));
      console.log(await sharedStorage.get('set'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(3u, console_observer.messages().size());
  EXPECT_EQ("worldthere",
            base::UTF16ToUTF8(console_observer.messages()[0].message));

  // Only one entry was set in b.test's shared storage.
  EXPECT_EQ("undefined",
            base::UTF16ToUTF8(console_observer.messages()[1].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[2].message));

  // Create an iframe that's same-origin to the first redirect URL.
  FrameTreeNode* iframe_node2 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), redirect_urls_.front());

  // c.test does not have permission to use shared storage.
  EXPECT_THAT(EvalJs(iframe_node2, R"(
        sharedStorage.worklet.addModule('/shared_storage/simple_module.js');
      )"),
              EvalJsResult::ErrorIs(testing::HasSubstr(
                  "The \"shared-storage\" Permissions Policy "
                  "denied the method")));

  // Create an iframe that's same-origin to the second redirect URL.
  FrameTreeNode* iframe_node3 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), redirect_urls_.back());

  ExecuteScriptInWorklet(iframe_node3, R"(
      console.log(await sharedStorage.get('set'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url, /*expected_total_host_count=*/2u);

  // One entry was set in d.test's shared storage.
  EXPECT_EQ(5u, console_observer.messages().size());
  EXPECT_EQ("will", base::UTF16ToUTF8(console_observer.messages()[3].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[4].message));
}

IN_PROC_BROWSER_TEST_F(
    SharedStorageHeaderObserverBrowserTest,
    Image_CrossOrigin_Redirect_InitialDenied_FinalAllowed_WriteFinal) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"b.test",
      /*shared_storage_permissions=*/
      "(self \"https://c.test:{{port}}\")",
      /*is_image=*/true,
      /*redirect_hostnames=*/std::vector<std::string>({"c.test"}));

  CreateSharedStorageWritableImage(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/false,
      /*http_status=*/net::HTTP_FOUND,
      /*extra_headers=*/
      {base::StrCat({"Location: ", redirect_urls_.back().spec()}),
       "Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: clear, "
       "set;key=\"hello\";value=\"world\";ignore_if_present, "
       "append;key=hello;value=there"});

  ASSERT_TRUE(observer_);

  // No operations are invoked.
  EXPECT_TRUE(observer_->header_results().empty());
  EXPECT_TRUE(observer_->operations().empty());

  WaitForRedirectRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: delete;key=a, set;value=will;key=set"});

  observer_->WaitForOperations(1);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front(), redirect_origins_.back());

  std::vector<MethodWithOptionsPtr> methods_with_options;
  methods_with_options.push_back(MojomDeleteMethod(/*key=*/u"a"));
  methods_with_options.push_back(MojomSetMethod(/*key=*/u"set",
                                                /*value=*/u"will",
                                                /*ignore_if_present=*/false));
  EXPECT_EQ(observer_->operations().size(), 1u);
  EXPECT_EQ(observer_->operations()[0],
            HeaderOperationSuccess(redirect_origins_.back(),
                                   std::move(methods_with_options)));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  // Create an iframe that's same-origin to the original image URL.
  FrameTreeNode* iframe_node1 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), subresource_or_subframe_url_);

  EXPECT_THAT(EvalJs(iframe_node1, R"(
        sharedStorage.worklet.addModule('/shared_storage/simple_module.js');
      )"),
              EvalJsResult::ErrorIs(testing::HasSubstr(
                  "The \"shared-storage\" Permissions Policy "
                  "denied the method")));

  // Create an iframe that's same-origin to the redirect URL.
  FrameTreeNode* iframe_node2 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), redirect_urls_.back());

  GURL out_script_url;
  ExecuteScriptInWorklet(iframe_node2, R"(
      console.log(await sharedStorage.get('set'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  // one key was set in c.test's shared storage.
  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("will", base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[1].message));
}

IN_PROC_BROWSER_TEST_F(
    SharedStorageHeaderObserverBrowserTest,
    Image_ContentAttributeIncluded_Set_2ndImageCached_NotSet) {
  WebContentsConsoleObserver console_observer(shell()->web_contents());

  StartServerAndLoadMainURLWithSameOriginSubresourceOrSubframe(
      /*main_hostname=*/"a.test",
      /*main_path=*/
      "/shared_storage/page-with-shared-storage-writable-image.html");

  // Wait for the image onload to fire.
  EXPECT_TRUE(console_observer.Wait());
  EXPECT_EQ(1u, console_observer.messages().size());
  EXPECT_EQ("Image Loaded",
            base::UTF16ToUTF8(console_observer.messages()[0].message));

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      console.log(await sharedStorage.get('a'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(3u, console_observer.messages().size());
  EXPECT_EQ("b", base::UTF16ToUTF8(console_observer.messages()[1].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[2].message));

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(1);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front(),
            subresource_or_subframe_origin_);

  std::vector<MethodWithOptionsPtr> methods_with_options1;
  methods_with_options1.push_back(
      MojomAppendMethod(/*key=*/u"a", /*value=*/u"b"));
  EXPECT_EQ(observer_->operations().size(), 1u);
  EXPECT_EQ(observer_->operations()[0],
            HeaderOperationSuccess(subresource_or_subframe_origin_,
                                   std::move(methods_with_options1)));

  EXPECT_EQ(
      true,
      EvalJs(
          shell(),
          JsReplace(
              R"(
      new Promise((resolve, reject) => {
        let img = document.createElement('img');
        img.src = $1;
        img.onload = () => resolve(true);
        img.sharedStorageWritable = true;
        document.body.appendChild(img);
      })
    )",
              https_server()
                  ->GetURL("a.test",
                           "/shared_storage/shared-storage-writable-pixel.png")
                  .spec())));

  // Create an iframe that's same-origin in order to run a second worklet.
  FrameTreeNode* iframe_node =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), main_url_);

  ExecuteScriptInWorklet(iframe_node, R"(
      console.log(await sharedStorage.get('a'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url, /*expected_total_host_count=*/2u);

  // The value 'b' for the key 'a' is unchanged (nothing is appended to it).
  EXPECT_EQ(6u, console_observer.messages().size());
  EXPECT_EQ("Image Loaded",
            base::UTF16ToUTF8(console_observer.messages()[3].message));
  EXPECT_EQ("b", base::UTF16ToUTF8(console_observer.messages()[4].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[5].message));

  // No new operations are invoked.
  ASSERT_TRUE(observer_);
  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front(),
            subresource_or_subframe_origin_);

  std::vector<MethodWithOptionsPtr> methods_with_options2;
  methods_with_options2.push_back(
      MojomAppendMethod(/*key=*/u"a", /*value=*/u"b"));
  EXPECT_EQ(observer_->operations().size(), 1u);
  EXPECT_EQ(observer_->operations()[0],
            HeaderOperationSuccess(subresource_or_subframe_origin_,
                                   std::move(methods_with_options2)));
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Image_ContentAttributeNotIncluded_NotSet) {
  WebContentsConsoleObserver console_observer(shell()->web_contents());
  StartServerAndLoadMainURLWithSameOriginSubresourceOrSubframe(
      /*main_hostname=*/"a.test",
      /*main_path=*/
      "/shared_storage/page-with-non-shared-storage-writable-image.html");

  // Wait for the image onload to fire.
  EXPECT_TRUE(console_observer.Wait());
  EXPECT_EQ(1u, console_observer.messages().size());
  EXPECT_EQ("Image Loaded",
            base::UTF16ToUTF8(console_observer.messages()[0].message));

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      console.log(await sharedStorage.get('a'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(3u, console_observer.messages().size());
  EXPECT_EQ("undefined",
            base::UTF16ToUTF8(console_observer.messages()[1].message));
  EXPECT_EQ("0", base::UTF16ToUTF8(console_observer.messages()[2].message));

  // No operations are invoked.
  ASSERT_TRUE(observer_);
  EXPECT_TRUE(observer_->header_results().empty());
  EXPECT_TRUE(observer_->operations().empty());
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Iframe_SameOrigin_PermissionsDefault) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"a.test",
      /*shared_storage_permissions=*/std::nullopt,
      /*is_image=*/false);

  CreateSharedStorageWritableIframe(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Shared-Storage-Write: set;key=a;value=b"});

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      console.log(await sharedStorage.get('a'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("b", base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[1].message));

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(1);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front(),
            subresource_or_subframe_origin_);

  std::vector<MethodWithOptionsPtr> methods_with_options;
  methods_with_options.push_back(MojomSetMethod(/*key=*/u"a", /*value=*/u"b",
                                                /*ignore_if_present=*/false));
  EXPECT_EQ(observer_->operations().size(), 1u);
  EXPECT_EQ(observer_->operations()[0],
            HeaderOperationSuccess(subresource_or_subframe_origin_,
                                   std::move(methods_with_options)));
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Iframe_SameOrigin_PermissionsNone) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"a.test",
      /*shared_storage_permissions=*/"()",
      /*is_image=*/false);

  CreateSharedStorageWritableIframe(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/false,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: set;key=a;value=b"});

  // No operations are invoked.
  ASSERT_TRUE(observer_);
  EXPECT_TRUE(observer_->header_results().empty());
  EXPECT_TRUE(observer_->operations().empty());
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Iframe_SameOrigin_PermissionsAll) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"a.test",
      /*shared_storage_permissions=*/"*",
      /*is_image=*/false);

  CreateSharedStorageWritableIframe(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: set;key=a;value=b"});

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      console.log(await sharedStorage.get('a'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("b", base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[1].message));

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(1);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front(),
            subresource_or_subframe_origin_);

  std::vector<MethodWithOptionsPtr> methods_with_options;
  methods_with_options.push_back(MojomSetMethod(/*key=*/u"a", /*value=*/u"b",
                                                /*ignore_if_present=*/false));
  EXPECT_EQ(observer_->operations().size(), 1u);
  EXPECT_EQ(observer_->operations()[0],
            HeaderOperationSuccess(subresource_or_subframe_origin_,
                                   std::move(methods_with_options)));
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Iframe_SameOrigin_PermissionsSelf) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"a.test",
      /*shared_storage_permissions=*/"self",
      /*is_image=*/false);

  CreateSharedStorageWritableIframe(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: set;key=a;value=b"});

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      console.log(await sharedStorage.get('a'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("b", base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[1].message));

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(1);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front(),
            subresource_or_subframe_origin_);

  std::vector<MethodWithOptionsPtr> methods_with_options;
  methods_with_options.push_back(MojomSetMethod(/*key=*/u"a", /*value=*/u"b",
                                                /*ignore_if_present=*/false));
  EXPECT_EQ(observer_->operations().size(), 1u);
  EXPECT_EQ(observer_->operations()[0],
            HeaderOperationSuccess(subresource_or_subframe_origin_,
                                   std::move(methods_with_options)));
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Iframe_CrossOrigin_PermissionsDefault) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"b.test",
      /*shared_storage_permissions=*/std::nullopt,
      /*is_image=*/false);

  CreateSharedStorageWritableIframe(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Shared-Storage-Write: set;key=a;value=b"});

  // Create another iframe that's same-origin to the first iframe URL.
  FrameTreeNode* iframe_node2 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(),
                   https_server()->GetURL("b.test", kTitle2Path));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(iframe_node2, R"(
      console.log(await sharedStorage.get('a'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("b", base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[1].message));

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(1);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front(),
            subresource_or_subframe_origin_);

  std::vector<MethodWithOptionsPtr> methods_with_options;
  methods_with_options.push_back(MojomSetMethod(/*key=*/u"a", /*value=*/u"b",
                                                /*ignore_if_present=*/false));
  EXPECT_EQ(observer_->operations().size(), 1u);
  EXPECT_EQ(observer_->operations()[0],
            HeaderOperationSuccess(subresource_or_subframe_origin_,
                                   std::move(methods_with_options)));
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Iframe_CrossOrigin_PermissionsNone) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"b.test",
      /*shared_storage_permissions=*/"()",
      /*is_image=*/false);

  CreateSharedStorageWritableIframe(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/false,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: set;key=a;value=b"});

  // No operations are invoked.
  ASSERT_TRUE(observer_);
  EXPECT_TRUE(observer_->header_results().empty());
  EXPECT_TRUE(observer_->operations().empty());
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Iframe_CrossOrigin_PermissionsAll) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"b.test",
      /*shared_storage_permissions=*/"*",
      /*is_image=*/false);

  CreateSharedStorageWritableIframe(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: set;key=a;value=b"});

  // Create another iframe that's same-origin to the first iframe URL.
  FrameTreeNode* iframe_node2 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(),
                   https_server()->GetURL("b.test", kTitle2Path));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(iframe_node2, R"(
      console.log(await sharedStorage.get('a'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("b", base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[1].message));

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(1);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front(),
            subresource_or_subframe_origin_);

  std::vector<MethodWithOptionsPtr> methods_with_options;
  methods_with_options.push_back(MojomSetMethod(/*key=*/u"a", /*value=*/u"b",
                                                /*ignore_if_present=*/false));
  EXPECT_EQ(observer_->operations().size(), 1u);
  EXPECT_EQ(observer_->operations()[0],
            HeaderOperationSuccess(subresource_or_subframe_origin_,
                                   std::move(methods_with_options)));
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Iframe_CrossOrigin_PermissionsSelf) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"b.test",
      /*shared_storage_permissions=*/"self",
      /*is_image=*/false);

  CreateSharedStorageWritableIframe(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/false,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: set;key=a;value=b"});

  // No operations are invoked.
  ASSERT_TRUE(observer_);
  EXPECT_TRUE(observer_->header_results().empty());
  EXPECT_TRUE(observer_->operations().empty());
}

IN_PROC_BROWSER_TEST_F(
    SharedStorageHeaderObserverBrowserTest,
    Iframe_CrossOrigin_Redirect_InititalAllowed_FinalAllowed_WriteInitial) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"b.test",
      /*shared_storage_permissions=*/
      "(self \"https://b.test:{{port}}\" \"https://c.test:{{port}}\")",
      /*is_image=*/false,
      /*redirect_hostnames=*/std::vector<std::string>({"c.test"}));

  CreateSharedStorageWritableIframe(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_FOUND,
      /*extra_headers=*/
      {base::StrCat({"Location: ", redirect_urls_.back().spec()}),
       "Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: clear, "
       "set;key=\"hello\";value=\"world\";ignore_if_present, "
       "append;key=hello;value=there"});

  WaitForRedirectRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Access-Control-Allow-Origin: *"});

  // Create another iframe that's same-origin to the original iframe URL.
  FrameTreeNode* iframe_node2 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), subresource_or_subframe_url_);

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(iframe_node2, R"(
      console.log(await sharedStorage.get('hello'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("worldthere",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[1].message));

  // There won't be additional operations invoked from the redirect, just the
  // original 3.
  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(1);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front(),
            subresource_or_subframe_origin_);

  std::vector<MethodWithOptionsPtr> methods_with_options;
  methods_with_options.push_back(MojomClearMethod());
  methods_with_options.push_back(MojomSetMethod(/*key=*/u"hello",
                                                /*value=*/u"world",
                                                /*ignore_if_present=*/true));
  methods_with_options.push_back(
      MojomAppendMethod(/*key=*/u"hello", /*value=*/u"there"));
  EXPECT_EQ(observer_->operations().size(), 1u);
  EXPECT_EQ(observer_->operations()[0],
            HeaderOperationSuccess(subresource_or_subframe_origin_,
                                   std::move(methods_with_options)));
}

IN_PROC_BROWSER_TEST_F(
    SharedStorageHeaderObserverBrowserTest,
    Iframe_CrossOrigin_Redirect_InititalAllowed_FinalAllowed_WriteFinal) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"b.test",
      /*shared_storage_permissions=*/
      "(self \"https://b.test:{{port}}\" \"https://c.test:{{port}}\")",
      /*is_image=*/false,
      /*redirect_hostnames=*/std::vector<std::string>({"c.test"}));

  CreateSharedStorageWritableIframe(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_FOUND,
      /*extra_headers=*/
      {base::StrCat({"Location: ", redirect_urls_.back().spec()}),
       "Access-Control-Allow-Origin: *"});

  WaitForRedirectRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: delete;key=a, set;value=will;key=set"});

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(1);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().back(), redirect_origins_.back());

  std::vector<MethodWithOptionsPtr> methods_with_options;
  methods_with_options.push_back(MojomDeleteMethod(/*key=*/u"a"));
  methods_with_options.push_back(MojomSetMethod(/*key=*/u"set",
                                                /*value=*/u"will",
                                                /*ignore_if_present=*/false));
  EXPECT_EQ(observer_->operations().size(), 1u);
  EXPECT_EQ(observer_->operations()[0],
            HeaderOperationSuccess(redirect_origins_.back(),
                                   std::move(methods_with_options)));

  // Create another iframe that's same-origin to the original iframe URL.
  FrameTreeNode* iframe_node2 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), subresource_or_subframe_url_);

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(iframe_node2, R"(
      console.log(await sharedStorage.get('set'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(2u, console_observer.messages().size());

  // Nothing was set in b.test's shared storage.
  EXPECT_EQ("undefined",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("0", base::UTF16ToUTF8(console_observer.messages()[1].message));

  // Create another iframe that's same-origin to the redirect URL.
  FrameTreeNode* iframe_node3 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), redirect_urls_.back());

  ExecuteScriptInWorklet(iframe_node3, R"(
      console.log(await sharedStorage.get('set'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url, /*expected_total_host_count=*/2u);

  // The entry was set in c.test's shared storage.
  EXPECT_EQ(4u, console_observer.messages().size());
  EXPECT_EQ("will", base::UTF16ToUTF8(console_observer.messages()[2].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[3].message));
}

IN_PROC_BROWSER_TEST_F(
    SharedStorageHeaderObserverBrowserTest,
    Iframe_CrossOrigin_Redirect_InititalAllowed_FinalAllowed_WriteBoth) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"b.test",
      /*shared_storage_permissions=*/
      "(self \"https://b.test:{{port}}\" \"https://c.test:{{port}}\")",
      /*is_image=*/false,
      /*redirect_hostnames=*/std::vector<std::string>({"c.test"}));

  CreateSharedStorageWritableIframe(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_FOUND,
      /*extra_headers=*/
      {base::StrCat({"Location: ", redirect_urls_.back().spec()}),
       "Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: clear, "
       "set;key=\"hello\";value=\"world\";ignore_if_present, "
       "append;key=hello;value=there"});

  WaitForRedirectRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: delete;key=a, set;value=will;key=set"});

  // There will now have been a total of 2 operations (1 previous, 1 current).
  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(2);

  EXPECT_EQ(observer_->header_results().size(), 2u);
  EXPECT_EQ(observer_->header_results().back(), redirect_origins_.back());
  EXPECT_EQ(observer_->operations().size(), 2u);

  std::vector<MethodWithOptionsPtr> methods_with_options1;
  methods_with_options1.push_back(MojomClearMethod());
  methods_with_options1.push_back(MojomSetMethod(/*key=*/u"hello",
                                                 /*value=*/u"world",
                                                 /*ignore_if_present=*/true));
  methods_with_options1.push_back(
      MojomAppendMethod(/*key=*/u"hello", /*value=*/u"there"));
  EXPECT_EQ(observer_->operations()[0],
            HeaderOperationSuccess(subresource_or_subframe_origin_,
                                   std::move(methods_with_options1)));

  std::vector<MethodWithOptionsPtr> methods_with_options2;
  methods_with_options2.push_back(MojomDeleteMethod(/*key=*/u"a"));
  methods_with_options2.push_back(MojomSetMethod(/*key=*/u"set",
                                                 /*value=*/u"will",
                                                 /*ignore_if_present=*/false));
  EXPECT_EQ(observer_->operations()[1],
            HeaderOperationSuccess(redirect_origins_.back(),
                                   std::move(methods_with_options2)));

  // Create another iframe that's same-origin to the original iframe URL.
  FrameTreeNode* iframe_node2 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), subresource_or_subframe_url_);

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(iframe_node2, R"(
      console.log(await sharedStorage.get('hello'));
      console.log(await sharedStorage.get('set'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(3u, console_observer.messages().size());
  EXPECT_EQ("worldthere",
            base::UTF16ToUTF8(console_observer.messages()[0].message));

  // Only one entry was set in b.test's shared storage.
  EXPECT_EQ("undefined",
            base::UTF16ToUTF8(console_observer.messages()[1].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[2].message));

  // Create another iframe that's same-origin to the redirect URL.
  FrameTreeNode* iframe_node3 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), redirect_urls_.back());

  ExecuteScriptInWorklet(iframe_node3, R"(
      console.log(await sharedStorage.get('set'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url, /*expected_total_host_count=*/2u);

  // One entry was set in c.test's shared storage.
  EXPECT_EQ(5u, console_observer.messages().size());
  EXPECT_EQ("will", base::UTF16ToUTF8(console_observer.messages()[3].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[4].message));
}

IN_PROC_BROWSER_TEST_F(
    SharedStorageHeaderObserverBrowserTest,
    Iframe_CrossOrigin_Redirect_InititalAllowed_FinalDenied) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"b.test",
      /*shared_storage_permissions=*/
      "(self \"https://b.test:{{port}}\")",
      /*is_image=*/false,
      /*redirect_hostnames=*/std::vector<std::string>({"c.test"}));

  CreateSharedStorageWritableIframe(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_FOUND,
      /*extra_headers=*/
      {base::StrCat({"Location: ", redirect_urls_.back().spec()}),
       "Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: clear, "
       "set;key=\"hello\";value=\"world\";ignore_if_present, "
       "append;key=hello;value=there"});

  WaitForRedirectRequestAndSendResponse(
      /*expect_writable_header=*/false,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: delete;key=a, set;value=wont;key=set"});

  // There won't be additional operations invoked from the redirect, just the
  // original 3.
  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(1);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front(),
            subresource_or_subframe_origin_);

  std::vector<MethodWithOptionsPtr> methods_with_options;
  methods_with_options.push_back(MojomClearMethod());
  methods_with_options.push_back(MojomSetMethod(/*key=*/u"hello",
                                                /*value=*/u"world",
                                                /*ignore_if_present=*/true));
  methods_with_options.push_back(
      MojomAppendMethod(/*key=*/u"hello", /*value=*/u"there"));
  EXPECT_EQ(observer_->operations().size(), 1u);
  EXPECT_EQ(observer_->operations()[0],
            HeaderOperationSuccess(subresource_or_subframe_origin_,
                                   std::move(methods_with_options)));

  // Create an iframe that's same-origin to the original iframe URL.
  FrameTreeNode* iframe_node1 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), subresource_or_subframe_url_);

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(iframe_node1, R"(
      console.log(await sharedStorage.get('hello'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("worldthere",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[1].message));

  // Create another iframe that's same-origin to the redirect URL.
  FrameTreeNode* iframe_node2 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), redirect_urls_.back());

  EXPECT_THAT(EvalJs(iframe_node2, R"(
        sharedStorage.worklet.addModule('/shared_storage/simple_module.js');
      )"),
              EvalJsResult::ErrorIs(testing::HasSubstr(
                  "The \"shared-storage\" Permissions Policy "
                  "denied the method")));
}

IN_PROC_BROWSER_TEST_F(
    SharedStorageHeaderObserverBrowserTest,
    Iframe_CrossOrigin_Redirect_InititalAllowed_IntermediateDenied_FinalAllowed_WriteInitialAndFinal) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"b.test",
      /*shared_storage_permissions=*/
      "(self \"https://b.test:{{port}}\" \"https://d.test:{{port}}\")",
      /*is_image=*/false,
      /*redirect_hostnames=*/std::vector<std::string>({"c.test", "d.test"}));

  CreateSharedStorageWritableIframe(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_FOUND,
      /*extra_headers=*/
      {base::StrCat({"Location: ", redirect_urls_.front().spec()}),
       "Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: clear, "
       "set;key=\"hello\";value=\"world\";ignore_if_present, "
       "append;key=hello;value=there"});

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(1);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front(),
            subresource_or_subframe_origin_);

  std::vector<MethodWithOptionsPtr> methods_with_options1;
  methods_with_options1.push_back(MojomClearMethod());
  methods_with_options1.push_back(MojomSetMethod(/*key=*/u"hello",
                                                 /*value=*/u"world",
                                                 /*ignore_if_present=*/true));
  methods_with_options1.push_back(
      MojomAppendMethod(/*key=*/u"hello", /*value=*/u"there"));
  EXPECT_EQ(observer_->operations().size(), 1u);
  EXPECT_EQ(observer_->operations()[0],
            HeaderOperationSuccess(subresource_or_subframe_origin_,
                                   std::move(methods_with_options1)));

  WaitForRedirectRequestAndSendResponse(
      /*expect_writable_header=*/false,
      /*http_status=*/net::HTTP_TEMPORARY_REDIRECT,
      /*extra_headers=*/
      {base::StrCat({"Location: ", redirect_urls_.back().spec()}),
       "Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: append;key=wont;value=set"},
      /*redirect_index=*/0);

  WaitForRedirectRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: delete;key=a, set;value=will;key=set"},
      /*redirect_index=*/1);

  // There will now have been a total of 2 operations (1 previous, 1 current).
  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(2);

  EXPECT_EQ(observer_->header_results().size(), 2u);
  EXPECT_EQ(observer_->header_results().back(), redirect_origins_.back());

  std::vector<MethodWithOptionsPtr> methods_with_options2;
  methods_with_options2.push_back(MojomDeleteMethod(/*key=*/u"a"));
  methods_with_options2.push_back(MojomSetMethod(/*key=*/u"set",
                                                 /*value=*/u"will",
                                                 /*ignore_if_present=*/false));
  EXPECT_EQ(observer_->operations().size(), 2u);
  EXPECT_EQ(observer_->operations()[1],
            HeaderOperationSuccess(redirect_origins_.back(),
                                   std::move(methods_with_options2)));

  // Create an iframe that's same-origin to the original iframe URL.
  FrameTreeNode* iframe_node2 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), subresource_or_subframe_url_);

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(iframe_node2, R"(
      console.log(await sharedStorage.get('hello'));
      console.log(await sharedStorage.get('set'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(3u, console_observer.messages().size());
  EXPECT_EQ("worldthere",
            base::UTF16ToUTF8(console_observer.messages()[0].message));

  // Only one entry was set in b.test's shared storage.
  EXPECT_EQ("undefined",
            base::UTF16ToUTF8(console_observer.messages()[1].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[2].message));

  // Create an iframe that's same-origin to the first redirect URL.
  FrameTreeNode* iframe_node3 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), redirect_urls_.front());

  // c.test does not have permission to use shared storage.
  EXPECT_THAT(EvalJs(iframe_node3, R"(
        sharedStorage.worklet.addModule('/shared_storage/simple_module.js');
      )"),
              EvalJsResult::ErrorIs(testing::HasSubstr(
                  "The \"shared-storage\" Permissions Policy "
                  "denied the method")));

  // Create an iframe that's same-origin to the second redirect URL.
  FrameTreeNode* iframe_node4 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), redirect_urls_.back());

  ExecuteScriptInWorklet(iframe_node4, R"(
      console.log(await sharedStorage.get('set'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url, /*expected_total_host_count=*/2u);

  // One entry was set in d.test's shared storage.
  EXPECT_EQ(5u, console_observer.messages().size());
  EXPECT_EQ("will", base::UTF16ToUTF8(console_observer.messages()[3].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[4].message));
}

IN_PROC_BROWSER_TEST_F(
    SharedStorageHeaderObserverBrowserTest,
    Iframe_CrossOrigin_Redirect_InitialDenied_FinalAllowed_WriteFinal) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"b.test",
      /*shared_storage_permissions=*/
      "(self \"https://c.test:{{port}}\")",
      /*is_image=*/false,
      /*redirect_hostnames=*/std::vector<std::string>({"c.test"}));

  CreateSharedStorageWritableIframe(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/false,
      /*http_status=*/net::HTTP_FOUND,
      /*extra_headers=*/
      {base::StrCat({"Location: ", redirect_urls_.back().spec()}),
       "Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: clear, "
       "set;key=\"hello\";value=\"world\";ignore_if_present, "
       "append;key=hello;value=there"});

  ASSERT_TRUE(observer_);

  // No operations are invoked.
  EXPECT_TRUE(observer_->header_results().empty());
  EXPECT_TRUE(observer_->operations().empty());

  WaitForRedirectRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: delete;key=a, set;value=will;key=set"});

  observer_->WaitForOperations(1);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front(), redirect_origins_.back());

  std::vector<MethodWithOptionsPtr> methods_with_options;
  methods_with_options.push_back(MojomDeleteMethod(/*key=*/u"a"));
  methods_with_options.push_back(MojomSetMethod(/*key=*/u"set",
                                                /*value=*/u"will",
                                                /*ignore_if_present=*/false));
  EXPECT_EQ(observer_->operations().size(), 1u);
  EXPECT_EQ(observer_->operations()[0],
            HeaderOperationSuccess(redirect_origins_.back(),
                                   std::move(methods_with_options)));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  // Create an iframe that's same-origin to the original iframe URL.
  FrameTreeNode* iframe_node2 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), subresource_or_subframe_url_);

  EXPECT_THAT(EvalJs(iframe_node2, R"(
        sharedStorage.worklet.addModule('/shared_storage/simple_module.js');
      )"),
              EvalJsResult::ErrorIs(testing::HasSubstr(
                  "The \"shared-storage\" Permissions Policy "
                  "denied the method")));

  // Create an iframe that's same-origin to the redirect URL.
  FrameTreeNode* iframe_node3 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), redirect_urls_.back());

  GURL out_script_url;
  ExecuteScriptInWorklet(iframe_node3, R"(
      console.log(await sharedStorage.get('set'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  // one key was set in c.test's shared storage.
  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("will", base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[1].message));
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Iframe_ContentAttributeIncluded_Set) {
  WebContentsConsoleObserver console_observer(shell()->web_contents());

  StartServerAndLoadMainURLWithSameOriginSubresourceOrSubframe(
      /*main_hostname=*/"a.test",
      /*main_path=*/
      "/shared_storage/page-with-shared-storage-writable-iframe.html");

  EXPECT_TRUE(console_observer.Wait());
  EXPECT_EQ(1u, console_observer.messages().size());
  EXPECT_EQ("Iframe Loaded",
            base::UTF16ToUTF8(console_observer.messages()[0].message));

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      console.log(await sharedStorage.get('a'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(3u, console_observer.messages().size());
  EXPECT_EQ("b", base::UTF16ToUTF8(console_observer.messages()[1].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[2].message));

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(1);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front(),
            subresource_or_subframe_origin_);

  std::vector<MethodWithOptionsPtr> methods_with_options;
  methods_with_options.push_back(MojomSetMethod(/*key=*/u"a", /*value=*/u"b",
                                                /*ignore_if_present=*/false));
  EXPECT_EQ(observer_->operations().size(), 1u);
  EXPECT_EQ(observer_->operations()[0],
            HeaderOperationSuccess(subresource_or_subframe_origin_,
                                   std::move(methods_with_options)));
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Iframe_ContentAttributeNotIncluded_NotSet) {
  WebContentsConsoleObserver console_observer(shell()->web_contents());

  StartServerAndLoadMainURLWithSameOriginSubresourceOrSubframe(
      /*main_hostname=*/"a.test",
      /*main_path=*/
      "/shared_storage/page-with-non-shared-storage-writable-iframe.html");

  EXPECT_TRUE(console_observer.Wait());
  EXPECT_EQ(1u, console_observer.messages().size());
  EXPECT_EQ("Iframe Loaded",
            base::UTF16ToUTF8(console_observer.messages()[0].message));

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      console.log(await sharedStorage.get('a'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(3u, console_observer.messages().size());
  EXPECT_EQ("undefined",
            base::UTF16ToUTF8(console_observer.messages()[1].message));
  EXPECT_EQ("0", base::UTF16ToUTF8(console_observer.messages()[2].message));

  // No operations are invoked.
  ASSERT_TRUE(observer_);
  EXPECT_TRUE(observer_->header_results().empty());
  EXPECT_TRUE(observer_->operations().empty());
}

class SharedStorageCreateWorkletCustomDataOriginBrowserTest
    : public SharedStorageBrowserTest {
 public:
  ~SharedStorageCreateWorkletCustomDataOriginBrowserTest() override = default;

  std::vector<base::Value> BuildWellKnownTrustedOriginsLists() override {
    std::vector<base::Value> trusted_origins_lists;
    // We expect failure for script with origin "https://b.test:{{port}}" and
    // context origin "https://a.test:{{port}}" when one of the following values
    // is served.
    trusted_origins_lists.push_back(static_cast<base::Value>(
        base::Value::Dict()
            .Set("scriptOrigin", "https://b.test:{{port}}")
            .Set("contextOrigin", "https://a.test:{{port}}")));
    trusted_origins_lists.push_back(
        static_cast<base::Value>(base::Value::List()));
    trusted_origins_lists.push_back(static_cast<base::Value>(
        base::Value::List().Append(base::Value::List()
                                       .Append("https://b.test:{{port}}")
                                       .Append("https://a.test:{{port}}"))));
    trusted_origins_lists.push_back(static_cast<base::Value>(
        base::Value::List().Append(base::Value::Dict().Set(
            "contextOrigin", "https://a.test:{{port}}"))));
    trusted_origins_lists.push_back(
        static_cast<base::Value>(base::Value::List().Append(
            base::Value::Dict()
                .Set("scriptOrigin", base::Value::List())
                .Set("contextOrigin", "https://a.test:{{port}}"))));
    trusted_origins_lists.push_back(static_cast<base::Value>(
        base::Value::List().Append(base::Value::Dict().Set(
            "scriptOrigin", "https://b.test:{{port}}"))));
    trusted_origins_lists.push_back(
        static_cast<base::Value>(base::Value::List().Append(
            base::Value::Dict()
                .Set("scriptOrigin", "https://b.test:{{port}}")
                .Set("contextOrigin", base::Value::List()))));
    trusted_origins_lists.push_back(static_cast<base::Value>(
        base::Value::List()
            .Append(base::Value::Dict()
                        .Set("scriptOrigin", "https://a.test:{{port}}")
                        .Set("contextOrigin", "*"))
            .Append(base::Value::Dict()
                        .Set("scriptOrigin", "*")
                        .Set("contextOrigin", "https://b.test:{{port}}"))));
    // We expect success for script with origin "https://b.test:{{port}}" and
    // context origin "https://a.test:{{port}}" when one of the following values
    // is served.
    trusted_origins_lists.push_back(
        static_cast<base::Value>(base::Value::List().Append(
            base::Value::Dict()
                .Set("scriptOrigin", "https://b.test:{{port}}")
                .Set("contextOrigin", "https://a.test:{{port}}"))));
    trusted_origins_lists.push_back(
        static_cast<base::Value>(base::Value::List().Append(
            base::Value::Dict()
                .Set("scriptOrigin", "https://b.test:{{port}}")
                .Set("contextOrigin", "*"))));
    trusted_origins_lists.push_back(
        static_cast<base::Value>(base::Value::List().Append(
            base::Value::Dict()
                .Set("scriptOrigin", "*")
                .Set("contextOrigin", "https://a.test:{{port}}"))));
    trusted_origins_lists.push_back(
        static_cast<base::Value>(base::Value::List().Append(
            base::Value::Dict()
                .Set("scriptOrigin", base::Value::List()
                                         .Append("https://x.test:{{port}}")
                                         .Append("https://b.test:{{port}}"))
                .Set("contextOrigin",
                     base::Value::List()
                         .Append("https://a.test:{{port}}")
                         .Append("https://y.test:{{port}}")))));
    trusted_origins_lists.push_back(
        static_cast<base::Value>(base::Value::List().Append(
            base::Value::Dict()
                .Set("scriptOrigin", base::Value::List()
                                         .Append("https://b.test:{{port}}")
                                         .Append("https://y.test:{{port}}")
                                         .Append("https://x.test:{{port}}"))
                .Set("contextOrigin",
                     base::Value::List()
                         .Append("https://y.test:{{port}}")
                         .Append("*")
                         .Append("https://z.test:{{port}}")))));
    trusted_origins_lists.push_back(
        static_cast<base::Value>(base::Value::List().Append(
            base::Value::Dict()
                .Set("scriptOrigin", base::Value::List()
                                         .Append("https://x.test:{{port}}")
                                         .Append("https://y.test:{{port}}")
                                         .Append("*"))
                .Set("contextOrigin",
                     base::Value::List()
                         .Append("https://y.test:{{port}}")
                         .Append("https://a.test:{{port}}")))));
    return trusted_origins_lists;
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         SharedStorageCreateWorkletCustomDataOriginBrowserTest,
                         testing::Bool(),
                         describe_param);

IN_PROC_BROWSER_TEST_P(SharedStorageCreateWorkletCustomDataOriginBrowserTest,
                       CrossOriginScript_Failure_ServerError) {
  set_force_server_error(true);
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  GURL module_script_url = https_server()->GetURL(
      "b.test", "/shared_storage/module_with_cors_header.js");

  url::Origin custom_data_origin =
      url::Origin::Create(https_server()->GetURL("c.test", kSimplePagePath));

  EXPECT_THAT(
      EvalJs(
          shell(),
          JsReplace("sharedStorage.createWorklet($1, {dataOrigin: $2})",
                    module_script_url.spec(), custom_data_origin.Serialize())),
      EvalJsResult::ErrorIs(testing::HasSubstr(
          "no response, an invalid response, or an unexpected mime type")));
}

IN_PROC_BROWSER_TEST_P(SharedStorageCreateWorkletCustomDataOriginBrowserTest,
                       CrossOriginScript_Failure_NotAList) {
  set_trusted_origins_list_index(0);
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  GURL module_script_url = https_server()->GetURL(
      "b.test", "/shared_storage/module_with_cors_header.js");

  url::Origin custom_data_origin =
      url::Origin::Create(https_server()->GetURL("c.test", kSimplePagePath));

  EXPECT_THAT(
      EvalJs(
          shell(),
          JsReplace("sharedStorage.createWorklet($1, {dataOrigin: $2})",
                    module_script_url.spec(), custom_data_origin.Serialize())),
      EvalJsResult::ErrorIs(testing::HasSubstr(
          "because there was no parse result or the result was not a list")));
}

IN_PROC_BROWSER_TEST_P(SharedStorageCreateWorkletCustomDataOriginBrowserTest,
                       CrossOriginScript_Failure_EmptyList) {
  set_trusted_origins_list_index(1);
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  GURL module_script_url = https_server()->GetURL(
      "b.test", "/shared_storage/module_with_cors_header.js");

  url::Origin custom_data_origin =
      url::Origin::Create(https_server()->GetURL("c.test", kSimplePagePath));

  EXPECT_THAT(
      EvalJs(
          shell(),
          JsReplace("sharedStorage.createWorklet($1, {dataOrigin: $2})",
                    module_script_url.spec(), custom_data_origin.Serialize())),
      EvalJsResult::ErrorIs(testing::HasSubstr("is an empty list")));
}

IN_PROC_BROWSER_TEST_P(SharedStorageCreateWorkletCustomDataOriginBrowserTest,
                       CrossOriginScript_Failure_NonDictItem) {
  set_trusted_origins_list_index(2);
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  GURL module_script_url = https_server()->GetURL(
      "b.test", "/shared_storage/module_with_cors_header.js");

  url::Origin custom_data_origin =
      url::Origin::Create(https_server()->GetURL("c.test", kSimplePagePath));

  EXPECT_THAT(
      EvalJs(
          shell(),
          JsReplace("sharedStorage.createWorklet($1, {dataOrigin: $2})",
                    module_script_url.spec(), custom_data_origin.Serialize())),
      EvalJsResult::ErrorIs(
          testing::HasSubstr("non-dictionary item was encountered")));
}

IN_PROC_BROWSER_TEST_P(SharedStorageCreateWorkletCustomDataOriginBrowserTest,
                       CrossOriginScript_Failure_ScriptOriginKeyNotFound) {
  set_trusted_origins_list_index(3);
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  GURL module_script_url = https_server()->GetURL(
      "b.test", "/shared_storage/module_with_cors_header.js");

  url::Origin custom_data_origin =
      url::Origin::Create(https_server()->GetURL("c.test", kSimplePagePath));

  EXPECT_THAT(
      EvalJs(
          shell(),
          JsReplace("sharedStorage.createWorklet($1, {dataOrigin: $2})",
                    module_script_url.spec(), custom_data_origin.Serialize())),
      EvalJsResult::ErrorIs(testing::HasSubstr(
          "dictionary item's `scriptOrigin` key was not found")));
}

IN_PROC_BROWSER_TEST_P(SharedStorageCreateWorkletCustomDataOriginBrowserTest,
                       CrossOriginScript_Failure_ScriptOriginValueEmptyList) {
  set_trusted_origins_list_index(4);
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  GURL module_script_url = https_server()->GetURL(
      "b.test", "/shared_storage/module_with_cors_header.js");

  url::Origin custom_data_origin =
      url::Origin::Create(https_server()->GetURL("c.test", kSimplePagePath));

  EXPECT_THAT(
      EvalJs(
          shell(),
          JsReplace("sharedStorage.createWorklet($1, {dataOrigin: $2})",
                    module_script_url.spec(), custom_data_origin.Serialize())),
      EvalJsResult::ErrorIs(testing::HasSubstr(
          "`scriptOrigin` key was not found, or its value was an empty list")));
}

IN_PROC_BROWSER_TEST_P(SharedStorageCreateWorkletCustomDataOriginBrowserTest,
                       CrossOriginScript_Failure_ContextOriginKeyNotFound) {
  set_trusted_origins_list_index(5);
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  GURL module_script_url = https_server()->GetURL(
      "b.test", "/shared_storage/module_with_cors_header.js");

  url::Origin custom_data_origin =
      url::Origin::Create(https_server()->GetURL("c.test", kSimplePagePath));

  EXPECT_THAT(
      EvalJs(
          shell(),
          JsReplace("sharedStorage.createWorklet($1, {dataOrigin: $2})",
                    module_script_url.spec(), custom_data_origin.Serialize())),
      EvalJsResult::ErrorIs(testing::HasSubstr(
          "dictionary item's `contextOrigin` key was not found")));
}

IN_PROC_BROWSER_TEST_P(SharedStorageCreateWorkletCustomDataOriginBrowserTest,
                       CrossOriginScript_Failure_ContextOriginValueEmptyList) {
  set_trusted_origins_list_index(6);
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  GURL module_script_url = https_server()->GetURL(
      "b.test", "/shared_storage/module_with_cors_header.js");

  url::Origin custom_data_origin =
      url::Origin::Create(https_server()->GetURL("c.test", kSimplePagePath));

  EXPECT_THAT(
      EvalJs(
          shell(),
          JsReplace("sharedStorage.createWorklet($1, {dataOrigin: $2})",
                    module_script_url.spec(), custom_data_origin.Serialize())),
      EvalJsResult::ErrorIs(testing::HasSubstr(
          "`contextOrigin` key was not found, or its value was "
          "an empty list")));
}

IN_PROC_BROWSER_TEST_P(SharedStorageCreateWorkletCustomDataOriginBrowserTest,
                       CrossOriginScript_Failure_NotAllowed) {
  set_trusted_origins_list_index(7);
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  GURL module_script_url = https_server()->GetURL(
      "b.test", "/shared_storage/module_with_cors_header.js");

  url::Origin custom_data_origin =
      url::Origin::Create(https_server()->GetURL("c.test", kSimplePagePath));

  EXPECT_THAT(
      EvalJs(
          shell(),
          JsReplace("sharedStorage.createWorklet($1, {dataOrigin: $2})",
                    module_script_url.spec(), custom_data_origin.Serialize())),
      EvalJsResult::ErrorIs(testing::HasSubstr("has not been allowed")));
}

IN_PROC_BROWSER_TEST_P(SharedStorageCreateWorkletCustomDataOriginBrowserTest,
                       CrossOriginScript_Success_FullySpecified) {
  set_trusted_origins_list_index(8);
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  GURL module_script_url = https_server()->GetURL(
      "b.test", "/shared_storage/module_with_cors_header.js");

  url::Origin custom_data_origin =
      url::Origin::Create(https_server()->GetURL("c.test", kSimplePagePath));

  EXPECT_TRUE(ExecJs(
      shell(),
      JsReplace("sharedStorage.createWorklet($1, {dataOrigin: $2})",
                module_script_url.spec(), custom_data_origin.Serialize())));

  std::string custom_data_origin_str = custom_data_origin.Serialize();
  ExpectAccessObserved(
      {{AccessScope::kWindow, AccessMethod::kCreateWorklet, MainFrameId(),
        custom_data_origin_str,
        SharedStorageEventParams::CreateForCreateWorklet(
            module_script_url, custom_data_origin_str,
            /*worklet_ordinal=*/0, GetFirstWorkletHostDevToolsToken())}});
}

IN_PROC_BROWSER_TEST_P(
    SharedStorageCreateWorkletCustomDataOriginBrowserTest,
    CrossOriginScript_Success_SpecifiedScriptOrigin_WildcardContextOrigin) {
  set_trusted_origins_list_index(9);
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  GURL module_script_url = https_server()->GetURL(
      "b.test", "/shared_storage/module_with_cors_header.js");

  url::Origin custom_data_origin =
      url::Origin::Create(https_server()->GetURL("c.test", kSimplePagePath));

  EXPECT_TRUE(ExecJs(
      shell(),
      JsReplace("sharedStorage.createWorklet($1, {dataOrigin: $2})",
                module_script_url.spec(), custom_data_origin.Serialize())));

  std::string custom_data_origin_str = custom_data_origin.Serialize();
  ExpectAccessObserved(
      {{AccessScope::kWindow, AccessMethod::kCreateWorklet, MainFrameId(),
        custom_data_origin_str,
        SharedStorageEventParams::CreateForCreateWorklet(
            module_script_url, custom_data_origin_str,
            /*worklet_ordinal=*/0, GetFirstWorkletHostDevToolsToken())}});
}

IN_PROC_BROWSER_TEST_P(
    SharedStorageCreateWorkletCustomDataOriginBrowserTest,
    CrossOriginScript_Success_WildcardScriptOrigin_SpecifiedContextOrigin) {
  set_trusted_origins_list_index(10);
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  GURL module_script_url = https_server()->GetURL(
      "b.test", "/shared_storage/module_with_cors_header.js");

  url::Origin custom_data_origin =
      url::Origin::Create(https_server()->GetURL("c.test", kSimplePagePath));

  EXPECT_TRUE(ExecJs(
      shell(),
      JsReplace("sharedStorage.createWorklet($1, {dataOrigin: $2})",
                module_script_url.spec(), custom_data_origin.Serialize())));

  std::string custom_data_origin_str = custom_data_origin.Serialize();
  ExpectAccessObserved(
      {{AccessScope::kWindow, AccessMethod::kCreateWorklet, MainFrameId(),
        custom_data_origin_str,
        SharedStorageEventParams::CreateForCreateWorklet(
            module_script_url, custom_data_origin_str,
            /*worklet_ordinal=*/0, GetFirstWorkletHostDevToolsToken())}});
}

IN_PROC_BROWSER_TEST_P(SharedStorageCreateWorkletCustomDataOriginBrowserTest,
                       CrossOriginScript_Success_Lists_FullySpecified) {
  set_trusted_origins_list_index(11);
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  GURL module_script_url = https_server()->GetURL(
      "b.test", "/shared_storage/module_with_cors_header.js");

  url::Origin custom_data_origin =
      url::Origin::Create(https_server()->GetURL("c.test", kSimplePagePath));

  EXPECT_TRUE(ExecJs(
      shell(),
      JsReplace("sharedStorage.createWorklet($1, {dataOrigin: $2})",
                module_script_url.spec(), custom_data_origin.Serialize())));

  std::string custom_data_origin_str = custom_data_origin.Serialize();
  ExpectAccessObserved(
      {{AccessScope::kWindow, AccessMethod::kCreateWorklet, MainFrameId(),
        custom_data_origin_str,
        SharedStorageEventParams::CreateForCreateWorklet(
            module_script_url, custom_data_origin_str,
            /*worklet_ordinal=*/0, GetFirstWorkletHostDevToolsToken())}});
}

IN_PROC_BROWSER_TEST_P(
    SharedStorageCreateWorkletCustomDataOriginBrowserTest,
    CrossOriginScript_Success_Lists_SpecifiedScriptOrigin_WildcardContextOrigin) {
  set_trusted_origins_list_index(12);
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  GURL module_script_url = https_server()->GetURL(
      "b.test", "/shared_storage/module_with_cors_header.js");

  url::Origin custom_data_origin =
      url::Origin::Create(https_server()->GetURL("c.test", kSimplePagePath));

  EXPECT_TRUE(ExecJs(
      shell(),
      JsReplace("sharedStorage.createWorklet($1, {dataOrigin: $2})",
                module_script_url.spec(), custom_data_origin.Serialize())));

  std::string custom_data_origin_str = custom_data_origin.Serialize();
  ExpectAccessObserved(
      {{AccessScope::kWindow, AccessMethod::kCreateWorklet, MainFrameId(),
        custom_data_origin_str,
        SharedStorageEventParams::CreateForCreateWorklet(
            module_script_url, custom_data_origin_str,
            /*worklet_ordinal=*/0, GetFirstWorkletHostDevToolsToken())}});
}

IN_PROC_BROWSER_TEST_P(
    SharedStorageCreateWorkletCustomDataOriginBrowserTest,
    CrossOriginScript_Success_Lists_WildcardScriptOrigin_SpecifiedContextOrigin) {
  set_trusted_origins_list_index(13);
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  GURL module_script_url = https_server()->GetURL(
      "b.test", "/shared_storage/module_with_cors_header.js");

  url::Origin custom_data_origin =
      url::Origin::Create(https_server()->GetURL("c.test", kSimplePagePath));

  EXPECT_TRUE(ExecJs(
      shell(),
      JsReplace("sharedStorage.createWorklet($1, {dataOrigin: $2})",
                module_script_url.spec(), custom_data_origin.Serialize())));

  std::string custom_data_origin_str = custom_data_origin.Serialize();
  ExpectAccessObserved(
      {{AccessScope::kWindow, AccessMethod::kCreateWorklet, MainFrameId(),
        custom_data_origin_str,
        SharedStorageEventParams::CreateForCreateWorklet(
            module_script_url, custom_data_origin_str,
            /*worklet_ordinal=*/0, GetFirstWorkletHostDevToolsToken())}});
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest,
                       TwoWorkletsInSameFrame_OrdinalIDsAreCorrect) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url1;
  ExecuteScriptInWorkletUsingCreateWorklet(shell(), R"(
      sharedStorage.set('key0', 'value0');
      sharedStorage.append('key0', 'value1');

      console.log(await sharedStorage.get('key0'));
      console.log(await sharedStorage.length());
    )",
                                           &out_script_url1);

  GURL out_script_url2;
  ExecuteScriptInWorkletUsingCreateWorklet(shell(), R"(
      sharedStorage.set('key1', 'value2');
      sharedStorage.append('key1', 'value3');

      console.log(await sharedStorage.get('key1'));
      console.log(await sharedStorage.length());
    )",
                                           &out_script_url2,
                                           /*expected_total_host_count=*/2u);

  EXPECT_EQ(4u, console_observer.messages().size());
  EXPECT_EQ("value0value1",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[1].message));
  EXPECT_EQ("value2value3",
            base::UTF16ToUTF8(console_observer.messages()[2].message));
  EXPECT_EQ("2", base::UTF16ToUTF8(console_observer.messages()[3].message));

  WaitForHistograms({kTimingRunExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingRunExecutedInWorkletHistogram, 2);

  std::map<int, base::UnguessableToken>& cached_worklet_devtools_tokens =
      GetCachedWorkletHostDevToolsTokens();
  EXPECT_EQ(cached_worklet_devtools_tokens.size(), 2u);

  std::string origin_str = url::Origin::Create(url).Serialize();
  ExpectAccessObserved(
      {{AccessScope::kWindow, AccessMethod::kCreateWorklet, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForCreateWorklet(
            out_script_url1, "context-origin",
            /*worklet_ordinal=*/0, cached_worklet_devtools_tokens[0])},
       {AccessScope::kWindow, AccessMethod::kRun, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForRunForTesting(
            "test-operation", /*operation_id=*/0, /*keep_alive=*/true,
            SharedStorageEventParams::PrivateAggregationConfigWrapper(),
            blink::CloneableMessage(), cached_worklet_devtools_tokens[0])},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kSet, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForSet(
            "key0", "value0", false, cached_worklet_devtools_tokens[0])},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kAppend,
        MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForAppend(
            "key0", "value1", cached_worklet_devtools_tokens[0])},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kGet, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForGet(
            "key0", cached_worklet_devtools_tokens[0])},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kLength,
        MainFrameId(), origin_str,
        SharedStorageEventParams::CreateWithWorkletToken(
            cached_worklet_devtools_tokens[0])},
       {AccessScope::kWindow, AccessMethod::kCreateWorklet, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForCreateWorklet(
            out_script_url2, "context-origin",
            /*worklet_ordinal=*/1, cached_worklet_devtools_tokens[1])},
       {AccessScope::kWindow, AccessMethod::kRun, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForRunForTesting(
            "test-operation", /*operation_id=*/0, /*keep_alive=*/true,
            SharedStorageEventParams::PrivateAggregationConfigWrapper(),
            blink::CloneableMessage(), cached_worklet_devtools_tokens[1])},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kSet, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForSet(
            "key1", "value2", false, cached_worklet_devtools_tokens[1])},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kAppend,
        MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForAppend(
            "key1", "value3", cached_worklet_devtools_tokens[1])},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kGet, MainFrameId(),
        origin_str,
        SharedStorageEventParams::CreateForGet(
            "key1", cached_worklet_devtools_tokens[1])},
       {AccessScope::kSharedStorageWorklet, AccessMethod::kLength,
        MainFrameId(), origin_str,
        SharedStorageEventParams::CreateWithWorkletToken(
            cached_worklet_devtools_tokens[1])}});

  ExpectOperationFinishedInfosObserved(
      {{base::TimeDelta(), AccessMethod::kRun, /*operation_id=*/0,
        cached_worklet_devtools_tokens[0], MainFrameId(), origin_str},
       {base::TimeDelta(), AccessMethod::kRun, /*operation_id=*/0,
        cached_worklet_devtools_tokens[1], MainFrameId(), origin_str}});
}

IN_PROC_BROWSER_TEST_P(
    SharedStorageBrowserTest,
    TwoWindows_DevToolsAccessNotificationsFilteredByMainFrame) {
  GURL url1 = https_server()->GetURL("a.test", kPageWithBlankIframePath);
  GURL url2 = https_server()->GetURL("b.test", kPageWithBlankIframePath);
  GURL iframe_url1 = https_server()->GetURL("c.test", kSimplePagePath);
  GURL iframe_url2 = https_server()->GetURL("d.test", kSimplePagePath);

  EXPECT_TRUE(NavigateToURL(shell(), url1));
  RenderFrameHost* main_rfh1 = PrimaryFrameTreeNodeRoot()->current_frame_host();
  TestSharedStorageDevToolsClient main_frame_devtools_client1(main_rfh1);
  std::string expected_method = "Storage.sharedStorageAccessed";
  main_frame_devtools_client1.set_expected_notification_method(expected_method);

  EXPECT_TRUE(
      NavigateIframeToURL(shell()->web_contents(), "test_iframe", iframe_url1));
  RenderFrameHost* iframe1 =
      PrimaryFrameTreeNodeRoot()->child_at(0)->current_frame_host();
  TestSharedStorageDevToolsClient iframe_devtools_client1(iframe1);
  iframe_devtools_client1.set_expected_notification_method(expected_method);

  EXPECT_TRUE(ExecJs(iframe1, "sharedStorage.delete('key0')"));

  Shell* shell2 = Shell::CreateNewWindow(
      shell()->web_contents()->GetBrowserContext(), url2, nullptr, gfx::Size());
  ASSERT_TRUE(WaitForLoadStop(shell2->web_contents()));

  RenderFrameHost* main_rfh2 =
      PrimaryFrameTreeNodeRootFromShell(shell2)->current_frame_host();
  TestSharedStorageDevToolsClient main_frame_devtools_client2(main_rfh2);
  main_frame_devtools_client2.set_expected_notification_method(expected_method);

  EXPECT_TRUE(
      NavigateIframeToURL(shell2->web_contents(), "test_iframe", iframe_url2));
  RenderFrameHost* iframe2 = PrimaryFrameTreeNodeRootFromShell(shell2)
                                 ->child_at(0)
                                 ->current_frame_host();
  TestSharedStorageDevToolsClient iframe_devtools_client2(iframe2);
  iframe_devtools_client2.set_expected_notification_method(expected_method);

  EXPECT_TRUE(ExecJs(iframe2, "sharedStorage.set('key2', 'value2')"));

  GURL out_script_url1;
  ExecuteScriptInWorklet(main_rfh1, R"(
      sharedStorage.set('key0', 'value0');
      sharedStorage.append('key0', 'value1');
    )",
                         &out_script_url1);

  GURL out_script_url2;
  ExecuteScriptInWorkletUsingCreateWorklet(main_rfh2, R"(
      sharedStorage.delete('key1');
      sharedStorage.clear();
    )",
                                           &out_script_url2,
                                           /*expected_total_host_count=*/2u);

  std::vector<std::string> selected_key_paths(
      {"method", "ownerSite", "params.operationId", "params.operationName",
       "params.ignoreIfPresent", "params.key", "params.value",
       "params.workletOrdinal", "scope"});
  std::vector<std::map<std::string, std::string>>
      selected_params_observed_main1 =
          main_frame_devtools_client1
              .GetSelectedParamsAsStringsForNotificationsWithExpectedMethod(
                  selected_key_paths);

  // `main_frame_devtools_client1` receives the expected shared storage
  // notifications for `shell()`, including any notifications from subframes of
  // this main frame, but no further "Storage.sharedStorageAccessed"
  // notifications. In particular, it does not receive those from `shell2`.
  ASSERT_EQ(selected_params_observed_main1.size(), 5u)
      << SerializeVectorOfMapOfStrings(selected_params_observed_main1);
  EXPECT_THAT(
      selected_params_observed_main1[0],
      ElementsAre(Pair("method", "delete"), Pair("ownerSite", "https://c.test"),
                  Pair("params.key", "key0"), Pair("scope", "window")));
  EXPECT_THAT(
      selected_params_observed_main1[1],
      ElementsAre(Pair("method", "addModule"),
                  Pair("ownerSite", "https://a.test"),
                  Pair("params.workletOrdinal", "0"), Pair("scope", "window")));
  EXPECT_THAT(
      selected_params_observed_main1[2],
      ElementsAre(Pair("method", "run"), Pair("ownerSite", "https://a.test"),
                  Pair("params.operationId", "0"),
                  Pair("params.operationName", "test-operation"),
                  Pair("scope", "window")));
  EXPECT_THAT(
      selected_params_observed_main1[3],
      ElementsAre(Pair("method", "set"), Pair("ownerSite", "https://a.test"),
                  Pair("params.ignoreIfPresent", "false"),
                  Pair("params.key", "key0"), Pair("params.value", "value0"),
                  Pair("scope", "sharedStorageWorklet")));
  EXPECT_THAT(
      selected_params_observed_main1[4],
      ElementsAre(Pair("method", "append"), Pair("ownerSite", "https://a.test"),
                  Pair("params.key", "key0"), Pair("params.value", "value1"),
                  Pair("scope", "sharedStorageWorklet")));

  std::vector<std::map<std::string, std::string>>
      selected_params_observed_main2 =
          main_frame_devtools_client2
              .GetSelectedParamsAsStringsForNotificationsWithExpectedMethod(
                  selected_key_paths);

  // `main_frame_devtools_client2` receives the expected shared storage
  // notifications for `shell2`, including any notifications from subframes of
  // this main frame, but no further "Storage.sharedStorageAccessed"
  // notifications. In particular, it does not receive those from `shell()`.
  ASSERT_EQ(selected_params_observed_main2.size(), 5u)
      << SerializeVectorOfMapOfStrings(selected_params_observed_main2);
  EXPECT_THAT(
      selected_params_observed_main2[0],
      ElementsAre(Pair("method", "set"), Pair("ownerSite", "https://d.test"),
                  Pair("params.ignoreIfPresent", "false"),
                  Pair("params.key", "key2"), Pair("params.value", "value2"),
                  Pair("scope", "window")));
  EXPECT_THAT(
      selected_params_observed_main2[1],
      ElementsAre(Pair("method", "createWorklet"),
                  Pair("ownerSite", "https://b.test"),
                  Pair("params.workletOrdinal", "1"), Pair("scope", "window")));
  EXPECT_THAT(
      selected_params_observed_main2[2],
      ElementsAre(Pair("method", "run"), Pair("ownerSite", "https://b.test"),
                  Pair("params.operationId", "0"),
                  Pair("params.operationName", "test-operation"),
                  Pair("scope", "window")));
  EXPECT_THAT(
      selected_params_observed_main2[3],
      ElementsAre(Pair("method", "delete"), Pair("ownerSite", "https://b.test"),
                  Pair("params.key", "key1"),
                  Pair("scope", "sharedStorageWorklet")));
  EXPECT_THAT(
      selected_params_observed_main2[4],
      ElementsAre(Pair("method", "clear"), Pair("ownerSite", "https://b.test"),
                  Pair("scope", "sharedStorageWorklet")));

  ASSERT_EQ(IsLocalRoot(iframe1), IsLocalRoot(iframe2));

  if (!IsLocalRoot(iframe1)) {
    // In this case, `iframe_devtools_client1` and `iframe_devtools_client2` are
    // actually attached to their respective main frame hosts. They will have
    // received the same notifications as above.
    return;
  }

  std::vector<std::map<std::string, std::string>>
      selected_params_observed_iframe1 =
          iframe_devtools_client1
              .GetSelectedParamsAsStringsForNotificationsWithExpectedMethod(
                  selected_key_paths);

  std::vector<std::map<std::string, std::string>>
      selected_params_observed_iframe2 =
          iframe_devtools_client2
              .GetSelectedParamsAsStringsForNotificationsWithExpectedMethod(
                  std::move(selected_key_paths));

  // Neither `iframe_devtools_client1` nor `iframe_devtools_client2` receives
  // any shared storage notifications. The notifications for the subframes'
  // events were received by their respective main frame clients above.
  EXPECT_TRUE(selected_params_observed_iframe1.empty())
      << SerializeVectorOfMapOfStrings(selected_params_observed_iframe1);
  EXPECT_TRUE(selected_params_observed_iframe2.empty())
      << SerializeVectorOfMapOfStrings(selected_params_observed_iframe2);
}

IN_PROC_BROWSER_TEST_P(
    SharedStorageBrowserTest,
    TwoWindows_DevToolsOperationFinishedNotificationsFilteredByMainFrame) {
  GURL url1 = https_server()->GetURL("a.test", kPageWithBlankIframePath);
  GURL url2 = https_server()->GetURL("b.test", kPageWithBlankIframePath);
  GURL iframe_url1 = https_server()->GetURL("c.test", kSimplePagePath);
  GURL iframe_url2 = https_server()->GetURL("d.test", kSimplePagePath);

  EXPECT_TRUE(NavigateToURL(shell(), url1));
  RenderFrameHost* main_rfh1 = PrimaryFrameTreeNodeRoot()->current_frame_host();
  TestSharedStorageDevToolsClient main_frame_devtools_client1(main_rfh1);
  std::string expected_method =
      "Storage.sharedStorageWorkletOperationExecutionFinished";
  main_frame_devtools_client1.set_expected_notification_method(expected_method);

  EXPECT_TRUE(
      NavigateIframeToURL(shell()->web_contents(), "test_iframe", iframe_url1));
  RenderFrameHost* iframe1 =
      PrimaryFrameTreeNodeRoot()->child_at(0)->current_frame_host();
  TestSharedStorageDevToolsClient iframe_devtools_client1(iframe1);
  iframe_devtools_client1.set_expected_notification_method(expected_method);

  EXPECT_TRUE(ExecJs(iframe1, R"(
      sharedStorage.createWorklet('shared_storage/simple_module.js')
        .then((worklet) => worklet.run("test-operation", {keepAlive: true}));
  )"));

  Shell* shell2 = Shell::CreateNewWindow(
      shell()->web_contents()->GetBrowserContext(), url2, nullptr, gfx::Size());
  ASSERT_TRUE(WaitForLoadStop(shell2->web_contents()));

  RenderFrameHost* main_rfh2 =
      PrimaryFrameTreeNodeRootFromShell(shell2)->current_frame_host();
  TestSharedStorageDevToolsClient main_frame_devtools_client2(main_rfh2);
  main_frame_devtools_client2.set_expected_notification_method(expected_method);

  EXPECT_TRUE(
      NavigateIframeToURL(shell2->web_contents(), "test_iframe", iframe_url2));
  RenderFrameHost* iframe2 = PrimaryFrameTreeNodeRootFromShell(shell2)
                                 ->child_at(0)
                                 ->current_frame_host();
  TestSharedStorageDevToolsClient iframe_devtools_client2(iframe2);
  iframe_devtools_client2.set_expected_notification_method(expected_method);

  EXPECT_TRUE(ExecJs(iframe2, R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js')
        .then(() => sharedStorage.selectURL("test-url-selection-operation",
                                            [{url: 'fenced_frames/title0.html'},
                                            {url: 'fenced_frames/title1.html'}],
                                            {keepAlive: true}));
  )"));

  EXPECT_TRUE(ExecJs(main_rfh1, R"(
      sharedStorage.createWorklet('shared_storage/simple_module.js')
        .then((worklet) => worklet.selectURL("test-url-selection-operation",
                                            [{url: 'fenced_frames/title0.html'},
                                            {url: 'fenced_frames/title1.html'}],
                                            {keepAlive: true}));
  )"));

  GURL out_script_url;
  ExecuteScriptInWorkletUsingCreateWorklet(main_rfh2, R"(
      sharedStorage.clear();
    )",
                                           &out_script_url,
                                           /*expected_total_host_count=*/4u);

  // Ensure that execution of all 4 operations has finished.
  WaitForHistogramsWithCounts(
      {std::make_tuple(kTimingRunExecutedInWorkletHistogram, 2),
       std::make_tuple(kTimingSelectUrlExecutedInWorkletHistogram, 2)});

  std::vector<std::string> selected_key_paths(
      {"method", "operationId", "ownerOrigin"});
  std::vector<std::map<std::string, std::string>>
      selected_params_observed_main1 =
          main_frame_devtools_client1
              .GetSelectedParamsAsStringsForNotificationsWithExpectedMethod(
                  selected_key_paths);

  std::string main_origin_str1 = url::Origin::Create(url1).Serialize();
  std::string main_origin_str2 = url::Origin::Create(url2).Serialize();
  std::string iframe_origin_str1 = url::Origin::Create(iframe_url1).Serialize();
  std::string iframe_origin_str2 = url::Origin::Create(iframe_url2).Serialize();

  // `main_frame_devtools_client1` receives the expected shared storage
  // notifications for `shell()`, including any notifications from subframes of
  // this main frame, but no further "Storage.sharedStorageAccessed"
  // notifications. In particular, it does not receive those from `shell2`.
  ASSERT_EQ(selected_params_observed_main1.size(), 2u)
      << SerializeVectorOfMapOfStrings(selected_params_observed_main1);
  if (selected_params_observed_main1[0]["ownerOrigin"] != iframe_origin_str1) {
    // The order in which the operations finish executing is nondeterministic.
    // If necessary, swap to put the iframe's results first.
    std::swap(selected_params_observed_main1[0],
              selected_params_observed_main1[1]);
  }
  EXPECT_THAT(selected_params_observed_main1[0],
              ElementsAre(Pair("method", "run"), Pair("operationId", "0"),
                          Pair("ownerOrigin", iframe_origin_str1)));
  EXPECT_THAT(selected_params_observed_main1[1],
              ElementsAre(Pair("method", "selectURL"), Pair("operationId", "0"),
                          Pair("ownerOrigin", main_origin_str1)));

  std::vector<std::map<std::string, std::string>>
      selected_params_observed_main2 =
          main_frame_devtools_client2
              .GetSelectedParamsAsStringsForNotificationsWithExpectedMethod(
                  selected_key_paths);

  // `main_frame_devtools_client2` receives the expected shared storage
  // notifications for `shell2`, including any notifications from subframes of
  // this main frame, but no further "Storage.sharedStorageAccessed"
  // notifications. In particular, it does not receive those from `shell()`.
  ASSERT_EQ(selected_params_observed_main2.size(), 2u)
      << SerializeVectorOfMapOfStrings(selected_params_observed_main2);
  if (selected_params_observed_main2[0]["ownerOrigin"] != iframe_origin_str2) {
    // The order in which the operations finish executing is nondeterministic.
    // If necessary, swap to put the iframe's results first.
    std::swap(selected_params_observed_main2[0],
              selected_params_observed_main2[1]);
  }
  EXPECT_THAT(selected_params_observed_main2[0],
              ElementsAre(Pair("method", "selectURL"), Pair("operationId", "0"),
                          Pair("ownerOrigin", iframe_origin_str2)));
  EXPECT_THAT(selected_params_observed_main2[1],
              ElementsAre(Pair("method", "run"), Pair("operationId", "0"),
                          Pair("ownerOrigin", main_origin_str2)));

  ASSERT_EQ(IsLocalRoot(iframe1), IsLocalRoot(iframe2));

  if (!IsLocalRoot(iframe1)) {
    // In this case, `iframe_devtools_client1` and `iframe_devtools_client2` are
    // actually attached to their respective main frame hosts. They will have
    // received the same notifications as above.
    return;
  }

  std::vector<std::map<std::string, std::string>>
      selected_params_observed_iframe1 =
          iframe_devtools_client1
              .GetSelectedParamsAsStringsForNotificationsWithExpectedMethod(
                  selected_key_paths);

  std::vector<std::map<std::string, std::string>>
      selected_params_observed_iframe2 =
          iframe_devtools_client2
              .GetSelectedParamsAsStringsForNotificationsWithExpectedMethod(
                  std::move(selected_key_paths));

  // Neither `iframe_devtools_client1` nor `iframe_devtools_client2` receives
  // any shared storage notifications. The notifications for the subframes'
  // events were received by their respective main frame clients above.
  EXPECT_TRUE(selected_params_observed_iframe1.empty())
      << SerializeVectorOfMapOfStrings(selected_params_observed_iframe1);
  EXPECT_TRUE(selected_params_observed_iframe2.empty())
      << SerializeVectorOfMapOfStrings(selected_params_observed_iframe2);
}

}  // namespace content
