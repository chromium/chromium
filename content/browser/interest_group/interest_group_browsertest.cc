// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <sstream>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/escape.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/web_package/web_bundle_builder.h"
#include "content/browser/fenced_frame/fenced_frame.h"
#include "content/browser/fenced_frame/fenced_frame_url_mapping.h"
#include "content/browser/interest_group/ad_auction_service_impl.h"
#include "content/browser/interest_group/interest_group_manager_impl.h"
#include "content/browser/interest_group/test_interest_group_observer.h"
#include "content/browser/private_aggregation/private_aggregation_manager_impl.h"
#include "content/browser/private_aggregation/private_aggregation_test_utils.h"
#include "content/browser/renderer_host/page_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_monitor.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "content/shell/browser/shell.h"
#include "content/test/fenced_frame_test_utils.h"
#include "net/base/isolation_info.h"
#include "net/base/network_isolation_key.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/ip_address_space.mojom.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/interest_group/ad_auction_constants.h"
#include "third_party/blink/public/common/interest_group/ad_display_size_utils.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/common/interest_group/test_interest_group_builder.h"
#include "third_party/blink/public/mojom/interest_group/ad_auction_service.mojom.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "ui/display/screen.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace content {

namespace {

using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::Optional;

// Returned by test Javascript code when join or leave promises complete without
// throwing an exception.
const char kSuccess[] = "success";

// Helpers for delivering an argument as either promise or value depending on
// which is used.
const char kFeedDirect[] = R"(
  function maybePromise(val) {
    return val;
  }
)";

const char kFeedPromise[] = R"(
  function maybePromise(val) {
    return new Promise((resolve, reject) => {
        setTimeout(() => { resolve(val); }, 1);
    });
  }
)";

// Convenience helper to parse JSON to a base::Value. CHECKs on failure, rather
// than letting callers handle it.
base::Value JsonToValue(const std::string& json) {
  absl::optional<base::Value> metadata =
      base::JSONReader::Read(json, base::JSON_PARSE_RFC);
  CHECK(metadata);
  return std::move(metadata).value();
}

// Creates base::Value representations of ads and adComponents arrays from the
// provided InterestGroup::Ads.
base::Value::List MakeAdsValue(
    const std::vector<blink::InterestGroup::Ad>& ads) {
  base::Value::List list;
  for (const auto& ad : ads) {
    base::Value::Dict entry;
    entry.Set("renderUrl", ad.render_url.spec());
    if (ad.size_group) {
      entry.Set("sizeGroup", std::move(ad.size_group.value()));
    }
    if (ad.metadata)
      entry.Set("metadata", JsonToValue(*ad.metadata));
    list.Append(std::move(entry));
  }
  return list;
}

base::Value::Dict StringDoubleMapToDict(
    const base::flat_map<std::string, double>& map) {
  base::Value::Dict dict;
  for (const auto& pair : map) {
    dict.Set(pair.first, pair.second);
  }
  return dict;
}

base::Value::List SellerCapabilitiesToList(
    blink::SellerCapabilitiesType capabilities) {
  base::Value::List list;
  for (blink::SellerCapabilities capability : capabilities) {
    if (capability == blink::SellerCapabilities::kInterestGroupCounts) {
      list.Append("interest-group-counts");
    } else if (capability == blink::SellerCapabilities::kLatencyStats) {
      list.Append("latency-stats");
    } else {
      ADD_FAILURE() << "Unknown seller capability "
                    << static_cast<uint32_t>(capability);
    }
  }
  return list;
}

base::Value::Dict SellerCapabilitiesToDict(
    const absl::optional<
        base::flat_map<url::Origin, blink::SellerCapabilitiesType>>& map,
    blink::SellerCapabilitiesType all_sellers_capabilities) {
  base::Value::Dict dict;
  if (map) {
    for (const auto& [origin, capabilities] : *map) {
      dict.Set(origin.Serialize(), SellerCapabilitiesToList(capabilities));
    }
  }
  if (!all_sellers_capabilities.Empty()) {
    dict.Set("*", SellerCapabilitiesToList(all_sellers_capabilities));
  }
  return dict;
}

base::Value::Dict InterestGroupSizeToDict(const blink::AdSize& size) {
  base::Value::Dict output;
  output.Set("width", base::NumberToString(size.width) +
                          blink::ConvertAdSizeUnitToString(size.width_units));
  output.Set("height", base::NumberToString(size.height) +
                           blink::ConvertAdSizeUnitToString(size.height_units));
  return output;
}

base::Value::Dict AdSizesToDict(
    const base::flat_map<std::string, blink::AdSize>& map) {
  base::Value::Dict dict;
  for (const auto& [size_name, size] : map) {
    dict.Set(size_name, InterestGroupSizeToDict(size));
  }
  return dict;
}

base::Value::Dict SizeGroupsToDict(
    const base::flat_map<std::string, std::vector<std::string>>& map) {
  base::Value::Dict dict;
  for (const auto& [group_name, group] : map) {
    base::Value::List size_list;
    for (const std::string& size : group) {
      size_list.Append(size);
    }
    dict.Set(group_name, std::move(size_list));
  }
  return dict;
}

bool IsErrorMessage(const content::WebContentsConsoleObserver::Message& msg) {
  return msg.log_level == blink::mojom::ConsoleMessageLevel::kError;
}

class AllowlistedOriginContentBrowserClient
    : public ContentBrowserTestContentBrowserClient {
 public:
  explicit AllowlistedOriginContentBrowserClient() = default;

  AllowlistedOriginContentBrowserClient(
      const AllowlistedOriginContentBrowserClient&) = delete;
  AllowlistedOriginContentBrowserClient& operator=(
      const AllowlistedOriginContentBrowserClient&) = delete;

  void SetAllowList(base::flat_set<url::Origin>&& allow_list) {
    allow_list_ = allow_list;
  }

  void AddToAllowList(const std::vector<url::Origin>& add_to_allow_list) {
    allow_list_.insert(add_to_allow_list.begin(), add_to_allow_list.end());
  }

  // ContentBrowserClient overrides:
  bool IsInterestGroupAPIAllowed(
      content::RenderFrameHost* render_frame_host,
      ContentBrowserClient::InterestGroupApiOperation operation,
      const url::Origin& top_frame_origin,
      const url::Origin& api_origin) override {
    return allow_list_.contains(top_frame_origin) &&
           allow_list_.contains(api_origin);
  }

 private:
  base::flat_set<url::Origin> allow_list_;
};

// A special path for updates that allows deferring the server response. Only
// update requests to this path can be deferred, because the path must be
// registered before the EmbeddedTestServer starts.
constexpr char kDeferredUpdateResponsePath[] =
    "/interest_group/update_deferred.json";

constexpr char kFledgeHeader[] = "X-Allow-FLEDGE";

// Allows registering responses to network requests.
class NetworkResponder {
 public:
  using ResponseHeaders = std::vector<std::pair<std::string, std::string>>;

  explicit NetworkResponder(
      net::EmbeddedTestServer& server,
      const std::string& relative_url = kDeferredUpdateResponsePath)
      : controllable_response_(&server, relative_url) {
    server.RegisterRequestHandler(base::BindRepeating(
        &NetworkResponder::RequestHandler, base::Unretained(this)));
  }

  NetworkResponder(const NetworkResponder&) = delete;
  NetworkResponder& operator=(const NetworkResponder&) = delete;

  void RegisterNetworkResponse(
      const std::string& url_path,
      const std::string& body,
      const std::string& mime_type = "application/json",
      ResponseHeaders extra_response_headers = {}) {
    base::AutoLock auto_lock(response_map_lock_);
    Response response;
    response.body = body;
    response.mime_type = mime_type;
    response.extra_response_headers = std::move(extra_response_headers);
    response_map_[url_path] = response;
  }

  struct SubresourceResponse {
    SubresourceResponse(const std::string& subresource_url,
                        const std::string& payload,
                        const std::string& content_type = "application/json")
        : subresource_url(subresource_url),
          payload(payload),
          content_type(content_type) {}

    std::string subresource_url;
    std::string payload;
    std::string content_type;
  };

  static SubresourceResponse DirectFromSellerPerBuyerSignals(
      const url::Origin& buyer_origin,
      const std::string& payload,
      const std::string& prefix = "/direct_from_seller_signals") {
    return NetworkResponder::SubresourceResponse(
        /*subresource_url=*/base::StringPrintf(
            "%s?perBuyerSignals=%s", prefix.c_str(),
            base::EscapeQueryParamValue(buyer_origin.Serialize(),
                                        /*use_plus=*/false)
                .c_str()),
        /*payload=*/
        payload);
  }

  struct SubresourceBundle {
    SubresourceBundle(const GURL& bundle_url,
                      const std::vector<SubresourceResponse>& subresources)
        : bundle_url(bundle_url.spec()), subresources(subresources) {}

    std::string bundle_url;
    std::vector<SubresourceResponse> subresources;
  };

  // Serves DirectFromSellerSignals subresource bundles from `bundles` with the
  // Access-Control-Allow-Origin (on both bundle and subresources) set to
  // `allow_origin`.
  void RegisterDirectFromSellerSignalsResponse(
      const std::vector<SubresourceBundle>& bundles,
      const std::string& allow_origin) {
    for (const SubresourceBundle& bundle : bundles) {
      web_package::WebBundleBuilder builder;
      for (const SubresourceResponse& response : bundle.subresources) {
        // NOTE: Upper-case characters are *not* allowed.
        builder.AddExchange(response.subresource_url,
                            {{":status", "200"},
                             {"content-type", response.content_type},
                             {"x-allow-fledge", "true"},
                             {"x-fledge-auction-only", "true"},
                             {"access-control-allow-credentials", "true"},
                             {"access-control-allow-origin", allow_origin}},
                            response.payload);
      }
      std::vector<uint8_t> bundle_bytes = builder.CreateBundle();
      std::string body(reinterpret_cast<const char*>(bundle_bytes.data()),
                       bundle_bytes.size());
      RegisterNetworkResponse(GURL(bundle.bundle_url).path(), body,
                              /*mime_type=*/"application/webbundle",
                              /*extra_response_headers=*/
                              {{"X-Content-Type-Options", "nosniff"},
                               {"Access-Control-Allow-Credentials", "true"},
                               {"Access-Control-Allow-Origin", allow_origin}});
    }
  }

  static std::string ProduceHtmlWithSubresourceBundles(
      const std::vector<SubresourceBundle>& bundles) {
    constexpr char kHtmlTemplate[] = R"(<!DOCTYPE html>
<meta charset="utf-8">
<title>Page with subresource bundle directFromSellerSignals</title>
%s
<body>
  <p>This page has a subresource bundle for passing directFromSellerSignals to
  navigator.runAdAuction().</p>
</body>)";

    // Include credentials to test that this works with DirectFromSellerSignals.
    constexpr char kScriptWebBundleTemplate[] = R"(
<script type="webbundle">
{
  "source": $1,
  "credentials": "include",
  "resources": $2
}
</script>
)";

    std::string script_tags;
    for (const SubresourceBundle& bundle : bundles) {
      base::Value::List subresources;
      for (const SubresourceResponse& subresource : bundle.subresources)
        subresources.Append(subresource.subresource_url);
      script_tags += JsReplace(kScriptWebBundleTemplate, bundle.bundle_url,
                               std::move(subresources));
    }

    return base::StringPrintf(kHtmlTemplate, script_tags.c_str());
  }

  // Registers an HTML response at `page_url` that inclues <script
  // type="webbundle"> tags for each fo the SubresourceBundles in `bundles`.
  // Each bundle is loaded using credentials.
  void RegisterHtmlWithSubresourceBundles(
      const std::vector<SubresourceBundle>& bundles,
      std::string page_url) {
    RegisterNetworkResponse(page_url,
                            ProduceHtmlWithSubresourceBundles(bundles),
                            /*mime_type=*/"text/html");
  }

  // Register a response that's a bidder script. Takes the body of the
  // generateBid() method.
  void RegisterBidderScript(const std::string& url_path,
                            const std::string& generate_bid_body) {
    std::string script = base::StringPrintf(R"(
function generateBid(
    interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
    unusedBrowserSignals) {
  %s
})",
                                            generate_bid_body.c_str());
    RegisterNetworkResponse(url_path, script, "application/javascript");
  }

  // Perform the deferred response -- the test hangs if the client isn't waiting
  // on a response to kDeferredUpdateResponsePath.
  void DoDeferredUpdateResponse(
      const std::string& response,
      const std::string& content_type = "application/json") {
    controllable_response_.WaitForRequest();
    controllable_response_.Send(net::HTTP_OK, content_type, response,
                                /*cookies=*/{},
                                /*extra_headers=*/{std::string(kFledgeHeader)});
    controllable_response_.Done();
  }

  // Wait for and get the received request.
  const net::test_server::HttpRequest* GetRequest() {
    controllable_response_.WaitForRequest();
    return controllable_response_.http_request();
  }

  bool HasReceivedRequest() {
    return controllable_response_.has_received_request();
  }

 private:
  struct Response {
    std::string body;
    std::string mime_type;
    ResponseHeaders extra_response_headers;
  };

  std::unique_ptr<net::test_server::HttpResponse> RequestHandler(
      const net::test_server::HttpRequest& request) {
    base::AutoLock auto_lock(response_map_lock_);
    const auto it = response_map_.find(request.GetURL().path());
    if (it == response_map_.end())
      return nullptr;
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->AddCustomHeader(kFledgeHeader, "true");
    response->set_code(net::HTTP_OK);
    response->set_content(it->second.body);
    response->set_content_type(it->second.mime_type);
    for (const auto& header : it->second.extra_response_headers) {
      response->AddCustomHeader(header.first, header.second);
    }
    return std::move(response);
  }

  // EmbeddedTestServer RequestHandlers can't be added after the server has
  // started, but tests may want to specify network responses after the server
  // starts in the fixture. A handler is therefore registered that uses
  // `response_map_` to serve network responses.
  base::Lock response_map_lock_;

  // For each HTTPS request, we see if any path in the map matches the request
  // path. If so, the server returns the mapped value string as the response.
  base::flat_map<std::string, Response> response_map_
      GUARDED_BY(response_map_lock_);

  net::test_server::ControllableHttpResponse controllable_response_;
};

// Handle well-known requests. Frame origins are expected to be of the form
// "allow-join...", "allow-leave...", or "no-cors...".
std::unique_ptr<net::test_server::HttpResponse> HandleWellKnownRequest(
    const net::test_server::HttpRequest& request) {
  if (!base::StartsWith(request.relative_url,
                        "/.well-known/interest-group/permissions/?origin=")) {
    return nullptr;
  }

  // .well-known requests should advertise they accept JSON responses.
  const auto accept_header =
      request.headers.find(net::HttpRequestHeaders::kAccept);
  DCHECK(accept_header != request.headers.end());
  EXPECT_EQ(accept_header->second, "application/json");

  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_content_type("application/json");
  response->set_content("{}");

  const auto host_header = request.headers.find(net::HttpRequestHeaders::kHost);
  DCHECK(host_header != request.headers.end());
  if (base::StartsWith(host_header->second, "allow-join.")) {
    response->set_content(R"({"joinAdInterestGroup" : true})");
    response->AddCustomHeader("Access-Control-Allow-Origin", "*");
  } else if (base::StartsWith(host_header->second, "allow-leave.")) {
    response->set_content(R"({"leaveAdInterestGroup" : true})");
    response->AddCustomHeader("Access-Control-Allow-Origin", "*");
  } else if (base::StartsWith(host_header->second, "no-cors.")) {
    response->set_content(
        R"({"joinAdInterestGroup" : true, "leaveAdInterestGroup" : true})");
  } else {
    NOTREACHED();
  }
  return response;
}

class InterestGroupBrowserTest : public ContentBrowserTest {
 public:
  InterestGroupBrowserTest() {
    feature_list_.InitWithFeatures(
        /*`enabled_features`=*/
        {blink::features::kInterestGroupStorage,
         blink::features::kAdInterestGroupAPI, blink::features::kParakeet,
         blink::features::kFledge, blink::features::kAllowURNsInIframes,
         blink::features::kBiddingAndScoringDebugReportingAPI,
         features::kBackForwardCache},
        /*disabled_features=*/
        {blink::features::kFencedFrames});
  }

  ~InterestGroupBrowserTest() override { content_browser_client_.reset(); }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::test_server::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_->RegisterRequestHandler(
        base::BindRepeating(&HandleWellKnownRequest));
    https_server_->AddDefaultHandlers(GetTestDataFilePath());
    https_server_->RegisterRequestMonitor(base::BindRepeating(
        &InterestGroupBrowserTest::OnHttpsTestServerRequestMonitor,
        base::Unretained(this)));
    network_responder_ = CreateNetworkResponder();
    ASSERT_TRUE(https_server_->Start());
    manager_ = static_cast<InterestGroupManagerImpl*>(
        shell()
            ->web_contents()
            ->GetBrowserContext()
            ->GetDefaultStoragePartition()
            ->GetInterestGroupManager());
    content_browser_client_ =
        std::make_unique<AllowlistedOriginContentBrowserClient>();
    content_browser_client_->SetAllowList(
        {https_server_->GetOrigin("a.test"), https_server_->GetOrigin("b.test"),
         https_server_->GetOrigin("c.test"),
         // Magic interest group origins used in cross-site join/leave tests.
         https_server_->GetOrigin("allow-join.a.test"),
         https_server_->GetOrigin("allow-leave.a.test"),
         https_server_->GetOrigin("no-cors.a.test"),
         // HTTP origins like those below aren't supported for FLEDGE -- some
         // tests verify that HTTP origins are rejected, even if somehow they
         // are allowed by the allowlist.
         https_server_->GetOrigin("a.test"), https_server_->GetOrigin("b.test"),
         https_server_->GetOrigin("c.test")});
  }

  virtual std::unique_ptr<NetworkResponder> CreateNetworkResponder() {
    return std::make_unique<NetworkResponder>(*https_server_);
  }

  // Attempts to join the specified interest group. Returns kSuccess if the
  // operation claims to have succeeded, and the exception message on failure.
  //
  // If `execution_target` is non-null, uses it as the target. Otherwise, uses
  // shell().
  [[nodiscard]] std::string JoinInterestGroup(
      url::Origin owner,
      std::string name,
      absl::optional<ToRenderFrameHost> execution_target = absl::nullopt) {
    return EvalJs(execution_target ? *execution_target : shell(),
                  JsReplace(R"(
    (async function() {
      try {
        await navigator.joinAdInterestGroup(
          {name: $1, owner: $2}, /*joinDurationSec=*/ 300);
        return 'success';
      } catch (e) {
        return e.toString();
      }
    })())",
                            name, owner))
        .ExtractString();
  }

  // Just like JoinInterestGroup() above, but also verifies that the interest
  // group was joined or not, depending on the return value.
  [[nodiscard]] std::string JoinInterestGroupAndVerify(
      const url::Origin& owner,
      const std::string& name,
      absl::optional<ToRenderFrameHost> execution_target = absl::nullopt) {
    absl::optional<StorageInterestGroup> initial_interest_group =
        GetInterestGroup(owner, name);

    std::string result = JoinInterestGroup(owner, name, execution_target);

    absl::optional<StorageInterestGroup> final_interest_group =
        GetInterestGroup(owner, name);

    if (result == kSuccess) {
      // On or after success, the user should have joined the interest group,
      // which should have been overwritten with the new interest group.
      while (!final_interest_group) {
        base::RunLoop().RunUntilIdle();
        final_interest_group = GetInterestGroup(owner, name);
      }

      if (final_interest_group) {
        if (!initial_interest_group) {
          EXPECT_EQ(1,
                    final_interest_group->bidding_browser_signals->join_count);
        } else {
          EXPECT_EQ(
              initial_interest_group->bidding_browser_signals->join_count + 1,
              final_interest_group->bidding_browser_signals->join_count);
        }

        // Check that the interest group is as expected.
        blink::InterestGroup expected_group;
        expected_group.owner = owner;
        expected_group.name = name;
        expected_group.priority = 0;
        // Don't compare the expiration.
        expected_group.expiry = final_interest_group->interest_group.expiry;
        EXPECT_TRUE(final_interest_group->interest_group.IsEqualForTesting(
            expected_group));
      }
    } else {
      // On failure, nothing should have changed.
      if (!initial_interest_group) {
        EXPECT_FALSE(final_interest_group);
      } else {
        EXPECT_EQ(initial_interest_group->bidding_browser_signals->join_count,
                  final_interest_group->bidding_browser_signals->join_count);
        EXPECT_TRUE(final_interest_group->interest_group.IsEqualForTesting(
            initial_interest_group->interest_group));
      }
    }

    return result;
  }

  // The `trusted_bidding_signals_keys` and `ads` fields of `group` will be
  // ignored in favor of the passed in values.
  // If `execution_target` is non-null, uses it as the target. Otherwise, uses
  // shell().
  [[nodiscard]] std::string JoinInterestGroup(
      const blink::InterestGroup& group,
      const absl::optional<ToRenderFrameHost> execution_target =
          absl::nullopt) {
    base::Value::Dict dict;
    dict.Set("name", group.name);
    dict.Set("owner", group.owner.Serialize());
    dict.Set("priority", group.priority);
    dict.Set("enableBiddingSignalsPrioritization",
             group.enable_bidding_signals_prioritization);
    if (group.priority_vector)
      dict.Set("priorityVector", StringDoubleMapToDict(*group.priority_vector));
    if (group.priority_signals_overrides) {
      dict.Set("prioritySignalsOverrides",
               StringDoubleMapToDict(*group.priority_signals_overrides));
    }
    dict.Set("sellerCapabilities",
             SellerCapabilitiesToDict(group.seller_capabilities,
                                      group.all_sellers_capabilities));
    if (group.bidding_url)
      dict.Set("biddingLogicUrl", group.bidding_url->spec());
    if (group.bidding_wasm_helper_url)
      dict.Set("biddingWasmHelperUrl", group.bidding_wasm_helper_url->spec());
    if (group.update_url) {
      // It doesn't really make sense to set `update_url` without one of these
      // being true.
      DCHECK(set_update_url_ || set_daily_update_url_);
      if (set_update_url_) {
        dict.Set("updateUrl", group.update_url->spec());
      }
      if (set_daily_update_url_) {
        dict.Set("dailyUpdateUrl", group.update_url->spec());
      }
    }
    if (group.trusted_bidding_signals_url) {
      dict.Set("trustedBiddingSignalsUrl",
               group.trusted_bidding_signals_url->spec());
    }
    if (group.user_bidding_signals)
      dict.Set("userBiddingSignals", JsonToValue(*group.user_bidding_signals));
    if (group.trusted_bidding_signals_keys) {
      base::Value::List keys;
      for (const auto& key : *group.trusted_bidding_signals_keys) {
        keys.Append(key);
      }
      dict.Set("trustedBiddingSignalsKeys", std::move(keys));
    }
    if (group.ads)
      dict.Set("ads", MakeAdsValue(*group.ads));
    if (group.ad_components)
      dict.Set("adComponents", MakeAdsValue(*group.ad_components));
    if (group.ad_sizes) {
      dict.Set("adSizes", AdSizesToDict(*group.ad_sizes));
    }
    if (group.size_groups) {
      dict.Set("sizeGroups", SizeGroupsToDict(*group.size_groups));
    }
    switch (group.execution_mode) {
      case blink::InterestGroup::ExecutionMode::kCompatibilityMode:
        dict.Set("executionMode", "compatibility");
        break;
      case blink::InterestGroup::ExecutionMode::kGroupedByOriginMode:
        dict.Set("executionMode", "group-by-origin");
        break;
      case blink::InterestGroup::ExecutionMode::kFrozenContext:
        dict.Set("executionMode", "frozenContext");
        break;
    }

    std::string interest_group_string;
    CHECK(base::JSONWriter::Write(dict, &interest_group_string));

    return EvalJs(execution_target ? *execution_target : shell(),
                  base::StringPrintf(R"(
    (async function() {
      try {
        await navigator.joinAdInterestGroup(
          %s, /*join_duration_sec=*/ 300);
        return 'success';
      } catch (e) {
        return e.toString();
      }
    })())",
                                     interest_group_string.c_str()))
        .ExtractString();
  }

  // If `execution_target` is non-null, uses it as the target. Otherwise, uses
  // shell().
  EvalJsResult UpdateInterestGroupsInJS(const absl::optional<ToRenderFrameHost>
                                            execution_target = absl::nullopt) {
    return EvalJs(execution_target ? *execution_target : shell(), R"(
(function() {
  try {
    navigator.updateAdInterestGroups();
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())");
  }

  // Attempts to leave the specified interest group. Returns kSuccess if the
  // operation claims to have succeeded, and the exception message on failure.
  //
  // If `execution_target` is non-null, uses it as the target. Otherwise, uses
  // shell().
  [[nodiscard]] std::string LeaveInterestGroup(
      url::Origin owner,
      std::string name,
      const absl::optional<ToRenderFrameHost> execution_target =
          absl::nullopt) {
    return EvalJs(execution_target ? *execution_target : shell(),
                  JsReplace(R"(
    (async function() {
      try {
        await navigator.leaveAdInterestGroup({name: $1, owner: $2});
        return 'success';
      } catch (e) {
        return e.toString();
      }
    })())",
                            name, owner))
        .ExtractString();
  }

  // Just like LeaveInterestGroupInJS(), but also verifies that the interest
  // group was left or not, depending on the return value.
  [[nodiscard]] std::string LeaveInterestGroupAndVerify(
      const url::Origin& owner,
      const std::string& name,
      const absl::optional<ToRenderFrameHost> execution_target =
          absl::nullopt) {
    int initial_count = GetJoinCount(owner, name);
    std::string result = LeaveInterestGroup(owner, name, execution_target);
    int final_count = GetJoinCount(owner, name);
    if (result == kSuccess) {
      // On or after success, the user should no longer be in the interest
      // group.
      while (final_count > 0) {
        base::RunLoop().RunUntilIdle();
        final_count = GetJoinCount(owner, name);
      }

      EXPECT_EQ(0, final_count);
    } else {
      // On failure, nothing should have changed.
      EXPECT_EQ(initial_count, final_count);
    }

    return result;
  }

  std::vector<url::Origin> GetAllInterestGroupsOwners() {
    std::vector<url::Origin> interest_group_owners;
    base::RunLoop run_loop;
    manager_->GetAllInterestGroupOwners(base::BindLambdaForTesting(
        [&run_loop, &interest_group_owners](std::vector<url::Origin> owners) {
          interest_group_owners = std::move(owners);
          run_loop.Quit();
        }));
    run_loop.Run();
    return interest_group_owners;
  }

  std::vector<StorageInterestGroup> GetInterestGroupsForOwner(
      const url::Origin& owner) {
    std::vector<StorageInterestGroup> interest_groups;
    base::RunLoop run_loop;
    manager_->GetInterestGroupsForOwner(
        owner, base::BindLambdaForTesting(
                   [&run_loop, &interest_groups](
                       std::vector<StorageInterestGroup> groups) {
                     interest_groups = std::move(groups);
                     run_loop.Quit();
                   }));
    run_loop.Run();
    return interest_groups;
  }

  std::vector<blink::InterestGroupKey> GetAllInterestGroups() {
    std::vector<blink::InterestGroupKey> interest_groups;
    for (const auto& owner : GetAllInterestGroupsOwners()) {
      for (const auto& storage_group : GetInterestGroupsForOwner(owner)) {
        interest_groups.emplace_back(storage_group.interest_group.owner,
                                     storage_group.interest_group.name);
      }
    }
    return interest_groups;
  }

  absl::optional<StorageInterestGroup> GetInterestGroup(
      const url::Origin& owner,
      const std::string& name) {
    absl::optional<StorageInterestGroup> result;
    base::RunLoop run_loop;
    manager_->GetInterestGroup(
        owner, name,
        base::BindLambdaForTesting(
            [&run_loop, &result](absl::optional<StorageInterestGroup> group) {
              result = std::move(group);
              run_loop.Quit();
            }));
    run_loop.Run();
    return result;
  }

  int GetJoinCount(const url::Origin& owner, const std::string& name) {
    absl::optional<StorageInterestGroup> group = GetInterestGroup(owner, name);
    if (!group)
      return 0;
    return group->bidding_browser_signals->join_count;
  }

  // If `execution_target` is non-null, uses it as the target. Otherwise, uses
  // shell().
  [[nodiscard]] std::string JoinInterestGroupAndVerify(
      const blink::InterestGroup& group,
      const absl::optional<ToRenderFrameHost> execution_target =
          absl::nullopt) {
    absl::optional<StorageInterestGroup> initial_interest_group =
        GetInterestGroup(group.owner, group.name);

    std::string result = JoinInterestGroup(group, execution_target);

    absl::optional<StorageInterestGroup> final_interest_group =
        GetInterestGroup(group.owner, group.name);

    if (result == kSuccess) {
      // On success, the user should have joined the interest group, which
      // should have been overwritten with `group`.
      EXPECT_TRUE(final_interest_group);
      if (final_interest_group) {
        if (!initial_interest_group) {
          EXPECT_EQ(1,
                    final_interest_group->bidding_browser_signals->join_count);
        } else {
          EXPECT_EQ(
              initial_interest_group->bidding_browser_signals->join_count + 1,
              final_interest_group->bidding_browser_signals->join_count);
        }
        // Check that the interest group in the store matches `group`, except
        // the expiration.
        blink::InterestGroup expected_group = group;
        expected_group.expiry = final_interest_group->interest_group.expiry;
        EXPECT_TRUE(final_interest_group->interest_group.IsEqualForTesting(
            expected_group));
      }
    } else {
      // On failure, nothing should have changed.
      if (!initial_interest_group) {
        EXPECT_FALSE(final_interest_group);
      } else {
        EXPECT_EQ(initial_interest_group->bidding_browser_signals->join_count,
                  final_interest_group->bidding_browser_signals->join_count);
        EXPECT_TRUE(final_interest_group->interest_group.IsEqualForTesting(
            initial_interest_group->interest_group));
      }
    }

    return result;
  }

  // Simplified method to join an interest group for tests that only care about
  // a few fields.
  [[nodiscard]] std::string JoinInterestGroupAndVerify(
      const url::Origin& owner,
      const std::string& name,
      double priority,
      blink::InterestGroup::ExecutionMode execution_mode =
          blink::InterestGroup::ExecutionMode::kCompatibilityMode,
      absl::optional<GURL> bidding_url = absl::nullopt,
      absl::optional<std::vector<blink::InterestGroup::Ad>> ads = absl::nullopt,
      absl::optional<std::vector<blink::InterestGroup::Ad>> ad_components =
          absl::nullopt,
      absl::optional<ToRenderFrameHost> execution_target = absl::nullopt) {
    return JoinInterestGroupAndVerify(
        blink::InterestGroup(
            /*expiry=*/base::Time(), owner, name, priority,
            /*enable_bidding_signals_prioritization=*/false,
            /*priority_vector=*/absl::nullopt,
            /*priority_signals_overrides=*/absl::nullopt,
            /*seller_capabilities=*/absl::nullopt,
            /*all_sellers_capabilities=*/
            {}, execution_mode, std::move(bidding_url),
            /*bidding_wasm_helper_url=*/absl::nullopt,
            /*update_url=*/absl::nullopt,
            /*trusted_bidding_signals_url=*/absl::nullopt,
            /*trusted_bidding_signals_keys=*/absl::nullopt,
            /*user_bidding_signals=*/absl::nullopt, std::move(ads),
            std::move(ad_components),
            /*ad_sizes=*/{},
            /*size_groups=*/{}),
        execution_target);
  }

  // If `execution_target` is non-null, uses it as the target. Otherwise, uses
  // shell().
  [[nodiscard]] content::EvalJsResult RunAuctionAndWait(
      const std::string& auction_config_json,
      const absl::optional<ToRenderFrameHost> execution_target =
          absl::nullopt) {
    return EvalJs(execution_target ? *execution_target : shell(),
                  base::StringPrintf(
                      R"(
(async function() {
  try {
    return await navigator.runAdAuction(%s);
  } catch (e) {
    return e.toString();
  }
})())",
                      auction_config_json.c_str()));
  }

  // Wrapper around RunAuctionAndWait that assumes the result is a URN URL and
  // returns the mapped URL.
  [[nodiscard]] std::string RunAuctionAndWaitForUrl(
      const std::string& auction_config_json,
      const absl::optional<ToRenderFrameHost> execution_target =
          absl::nullopt) {
    auto result = RunAuctionAndWait(auction_config_json, execution_target);
    GURL urn_url = GURL(result.ExtractString());
    EXPECT_TRUE(urn_url.is_valid());
    EXPECT_EQ(url::kUrnScheme, urn_url.scheme_piece());

    TestFencedFrameURLMappingResultObserver observer;
    ConvertFencedFrameURNToURL(urn_url, &observer);
    EXPECT_TRUE(observer.mapped_url()) << urn_url;
    return observer.mapped_url()->spec();
  }

  // Navigates an iframe with the id="test_iframe" to the provided URL and
  // checks that the last committed url is the expected url. There must only be
  // one iframe in the main document.
  void NavigateIframeAndCheckURL(WebContents* web_contents,
                                 const GURL& url,
                                 const GURL& expected_commit_url) {
    FrameTreeNode* parent =
        FrameTreeNode::From(web_contents->GetPrimaryMainFrame());
    CHECK(parent->child_count() > 0u);
    FrameTreeNode* iframe = parent->child_at(0);
    TestFrameNavigationObserver nav_observer(iframe->current_frame_host());
    const std::string kIframeId = "test_iframe";
    EXPECT_TRUE(BeginNavigateIframeToURL(web_contents, kIframeId, url));
    nav_observer.Wait();
    EXPECT_EQ(expected_commit_url, nav_observer.last_committed_url());
    EXPECT_TRUE(nav_observer.last_navigation_succeeded());
  }

  // Wrapper around RunAuctionAndWait that assumes the result is a URN URL and
  // tries to navigate to it. Checks that the mapped URL equals `expected_url`.
  void RunAuctionAndWaitForURLAndNavigateIframe(
      const std::string& auction_config_json,
      GURL expected_url) {
    auto result = RunAuctionAndWait(auction_config_json,
                                    /*execution_target=*/absl::nullopt);
    GURL urn_url = GURL(result.ExtractString());
    EXPECT_TRUE(urn_url.is_valid());
    EXPECT_EQ(url::kUrnScheme, urn_url.scheme_piece());

    TestFencedFrameURLMappingResultObserver observer;
    ConvertFencedFrameURNToURL(urn_url, &observer);
    EXPECT_TRUE(observer.mapped_url()) << urn_url;

    NavigateIframeAndCheckURL(web_contents(), urn_url, expected_url);
    EXPECT_EQ(expected_url, observer.mapped_url());
  }

  // If `execution_target` is non-null, uses it as the target. Otherwise, uses
  // shell().
  [[nodiscard]] content::EvalJsResult CreateAdRequestAndWait(
      const absl::optional<ToRenderFrameHost> execution_target =
          absl::nullopt) {
    return EvalJs(execution_target ? *execution_target : shell(),
                  R"(
(async function() {
  try {
    return await navigator.createAdRequest({
      adRequestUrl: "https://example.site",
      adProperties: [
        { width: "24", height: "48", slot: "first",
          lang: "en-us", adType: "test-ad2", bidFloor: 42.0 }],
      publisherCode: "pubCode123",
      targeting: { interests: ["interest1", "interest2"] },
      anonymizedProxiedSignals: [],
      fallbackSource: "https://fallback.site"
    });
  } catch (e) {
    return e.toString();
  }
})())");
  }

  // If `execution_target` is non-null, uses it as the target. Otherwise, uses
  // shell().
  [[nodiscard]] content::EvalJsResult FinalizeAdAndWait(
      const absl::optional<ToRenderFrameHost> execution_target =
          absl::nullopt) {
    return EvalJs(execution_target ? *execution_target : shell(),
                  R"(
(async function() {
  try {
    return await navigator.createAdRequest({
      adRequestUrl: "https://example.site",
      adProperties: [
        { width: "24", height: "48", slot: "first",
          lang: "en-us", adType: "test-ad2", bidFloor: 42.0 }],
      publisherCode: "pubCode123",
      targeting: { interests: ["interest1", "interest2"] },
      anonymizedProxiedSignals: [],
      fallbackSource: "https://fallback.site"
    }).then(ads => {
      return navigator.finalizeAd(ads, {
        seller: "https://example.site",
        decisionLogicUrl: "https://example.site/script.js",
        perBuyerSignals: {"example.site": { randomParam: "value1" }},
        auctionSignals: "pubCode123",
        sellerSignals: { someKey: "sellerValue" }
      });
    });
  } catch (e) {
    return e.toString();
  }
})())");
  }

  // Waits until the `condition` callback over the interest groups returns true.
  void WaitForInterestGroupsSatisfying(
      const url::Origin& owner,
      base::RepeatingCallback<bool(const std::vector<StorageInterestGroup>&)>
          condition) {
    while (true) {
      if (condition.Run(GetInterestGroupsForOwner(owner)))
        break;
    }
  }

  // Waits for `url` to be requested by `https_server_`, or any other server
  // that OnHttpsTestServerRequestMonitor() has been configured to monitor.
  // `url`'s hostname is replaced with "127.0.0.1", since the embedded test
  // server always claims requests were for 127.0.0.1, rather than revealing the
  // hostname that was actually associated with a request.
  void WaitForUrl(const GURL& url) {
    GURL::Replacements replacements;
    replacements.SetHostStr("127.0.0.1");
    GURL wait_for_url = url.ReplaceComponents(replacements);

    {
      base::AutoLock auto_lock(requests_lock_);
      if (received_https_test_server_requests_.count(wait_for_url) > 0u)
        return;
      wait_for_url_ = wait_for_url;
      request_run_loop_ = std::make_unique<base::RunLoop>();
    }

    request_run_loop_->Run();
    request_run_loop_.reset();
  }

  void OnHttpsTestServerRequestMonitor(
      const net::test_server::HttpRequest& request) {
    base::AutoLock auto_lock(requests_lock_);
    received_https_test_server_requests_.insert(request.GetURL());
    if (wait_for_url_ == request.GetURL()) {
      wait_for_url_ = GURL();
      request_run_loop_->Quit();
    }
  }

  void ClearReceivedRequests() {
    base::AutoLock auto_lock(requests_lock_);
    received_https_test_server_requests_.clear();
  }

  bool HasServerSeenUrl(const GURL& url) {
    GURL::Replacements replacements;
    replacements.SetHostStr("127.0.0.1");
    GURL look_for_url = url.ReplaceComponents(replacements);
    base::AutoLock auto_lock(requests_lock_);
    return received_https_test_server_requests_.find(look_for_url) !=
           received_https_test_server_requests_.end();
  }

  bool HasServerSeenUrls(const std::vector<GURL>& urls) {
    for (const auto& url : urls) {
      if (!HasServerSeenUrl(url))
        return false;
    }
    return true;
  }

  void ExpectNotAllowedToJoinOrUpdateInterestGroup(
      const url::Origin& origin,
      RenderFrameHost* execution_target) {
    EXPECT_EQ(
        "NotAllowedError: Failed to execute 'joinAdInterestGroup' on "
        "'Navigator': Feature join-ad-interest-group is not enabled by "
        "Permissions Policy",
        EvalJs(execution_target, JsReplace(
                                     R"(
(async function() {
  try {
    await navigator.joinAdInterestGroup(
        {
          name: 'cars',
          owner: $1,
        },
        /*joinDurationSec=*/1);
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())",
                                     origin)));

    EXPECT_EQ(
        "NotAllowedError: Failed to execute 'updateAdInterestGroups' on "
        "'Navigator': Feature join-ad-interest-group is not enabled by "
        "Permissions Policy",
        UpdateInterestGroupsInJS(execution_target));
  }

  // If `execution_target` is non-null, uses it as the target. Otherwise, uses
  // shell().
  void ExpectNotAllowedToLeaveInterestGroup(const url::Origin& origin,
                                            std::string name,
                                            RenderFrameHost* execution_target) {
    EXPECT_EQ(
        "NotAllowedError: Failed to execute 'leaveAdInterestGroup' on "
        "'Navigator': Feature join-ad-interest-group is not enabled by "
        "Permissions Policy",
        EvalJs(execution_target,
               base::StringPrintf(R"(
(async function() {
  try {
    await navigator.leaveAdInterestGroup({name: '%s', owner: '%s'});
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())",
                                  name.c_str(), origin.Serialize().c_str())));
  }

  void ExpectNotAllowedToRunAdAuction(const url::Origin& origin,
                                      const GURL& url,
                                      RenderFrameHost* execution_target) {
    EXPECT_EQ(
        "NotAllowedError: Failed to execute 'runAdAuction' on 'Navigator': "
        "Feature run-ad-auction is not enabled by Permissions Policy",
        RunAuctionAndWait(JsReplace(
                              R"(
{
  seller: $1,
  decisionLogicUrl: $2,
  interestGroupBuyers: [$1],
}
                              )",
                              origin, url),
                          execution_target));
  }

  std::string WarningPermissionsPolicy(std::string feature, std::string api) {
    return base::StringPrintf(
        "In the future, Permissions Policy feature %s will not be enabled by "
        "default in cross-origin iframes or same-origin iframes nested in "
        "cross-origin iframes. Calling %s will be rejected with "
        "NotAllowedError if it is not explicitly enabled",
        feature.c_str(), api.c_str());
  }

  void ConvertFencedFrameURNToURL(
      const GURL& urn_url,
      TestFencedFrameURLMappingResultObserver* observer,
      const absl::optional<ToRenderFrameHost> execution_target =
          absl::nullopt) {
    ToRenderFrameHost adapter(execution_target ? *execution_target : shell());
    FencedFrameURLMapping& fenced_frame_urls_map =
        static_cast<RenderFrameHostImpl*>(adapter.render_frame_host())
            ->GetPage()
            .fenced_frame_urls_map();
    fenced_frame_urls_map.ConvertFencedFrameURNToURL(urn_url, observer);
  }

  absl::optional<GURL> ConvertFencedFrameURNToURLInJS(
      const GURL& urn_url,
      bool send_reports = false,
      const absl::optional<ToRenderFrameHost> execution_target =
          absl::nullopt) {
    ToRenderFrameHost adapter(execution_target ? *execution_target : shell());
    EvalJsResult result =
        EvalJs(adapter, JsReplace("navigator.deprecatedURNToURL($1, $2)",
                                  urn_url, send_reports));
    if (!result.error.empty() || result.value.is_none())
      return absl::nullopt;
    return GURL(result.ExtractString());
  }

  bool ReplaceInURNInJS(
      const GURL& urn_url,
      const base::flat_map<std::string, std::string> replacements,
      std::string* error_out = nullptr) {
    base::Value::Dict replacement_value;
    for (const auto& replacement : replacements)
      replacement_value.Set(replacement.first, replacement.second);
    EvalJsResult result = EvalJs(
        shell(), JsReplace(R"(
    (async function() {
      await navigator.deprecatedReplaceInURN($1, $2);
      return 'done';
    })())",
                           urn_url, base::Value(std::move(replacement_value))));
    if (error_out != nullptr) {
      *error_out = result.error;
    }
    return result.error == "" && result == "done";
  }

  void AttachInterestGroupObserver() {
    DCHECK(!observer_);
    observer_ = std::make_unique<TestInterestGroupObserver>();
    manager_->AddInterestGroupObserver(observer_.get());
  }

  void WaitForAccessObserved(
      const std::vector<TestInterestGroupObserver::Entry>& expected) {
    observer_->WaitForAccesses(expected);
  }

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

 protected:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<AllowlistedOriginContentBrowserClient>
      content_browser_client_;
  std::unique_ptr<TestInterestGroupObserver> observer_;
  raw_ptr<InterestGroupManagerImpl, DanglingUntriaged> manager_;
  base::Lock requests_lock_;
  std::set<GURL> received_https_test_server_requests_
      GUARDED_BY(requests_lock_);
  std::unique_ptr<base::RunLoop> request_run_loop_;
  GURL wait_for_url_ GUARDED_BY(requests_lock_);
  std::unique_ptr<NetworkResponder> network_responder_;

  // These determine, when joining an interest group in Javascript using a
  // blink::InterestGroup with a non-null update_url field, whether `updateUrl`,
  // `dailyUpdateUrl`, or both are set on the Javascript `interestGroup` object.
  //
  // TODO(https://crbug.com/1420080): Remove these once support for
  // `dailyUpdateUrl` has been removed, and always set `updateUrl` only.
  bool set_update_url_ = true;
  bool set_daily_update_url_ = false;
};

// At the moment, InterestGroups use either:
//   a. Web-exposed `FencedFrameConfig` objects or URN urls, when fenced frames
//      are enabled
//   b. Normal URLs when fenced frames are not enabled
// This means they require ads be loaded in fenced frames when Chrome is running
// with the option enabled. This fixture is parameterized over whether the test
// should call `navigator.runAdAuction()` with a request to have the promise
// resolve to a JS `FencedFrameConfig` object or a URN.
class InterestGroupFencedFrameBrowserTest : public InterestGroupBrowserTest {
 public:
  InterestGroupFencedFrameBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kFencedFrames, {}},
         {features::kPrivacySandboxAdsAPIsOverride, {}},
         {blink::features::kPrivateAggregationApi,
          {{"fledge_extensions_enabled", "true"}}},
         // This feature allows `runAdAuction()`'s promise to resolve to a
         // `FencedFrameConfig` object upon developer request.
         {blink::features::kFencedFramesAPIChanges, {}}},
        /*disabled_features=*/{});
  }

  ~InterestGroupFencedFrameBrowserTest() override = default;

  // Runs the specified auction using RunAuctionAndWait(), expecting a success
  // resulting in a URN URL. Then navigates a pre-existing fenced frame to that
  // URL, expecting `expected_ad_url` to be loaded in the fenced frame.
  //
  // If `execution_target` is non-null, uses it as the target. Otherwise, uses
  // shell().
  //
  // The target must already contain a single fenced frame.
  void RunAuctionAndNavigateFencedFrame(
      const GURL& expected_ad_url,
      const std::string& auction_config_json,
      absl::optional<ToRenderFrameHost> execution_target = absl::nullopt) {
    if (!execution_target)
      execution_target = shell();

    // For this test, we:
    //   1. Run the ad auction, specifically requesting that the returned
    //      promise resolve to a config object that we immediately navigate to
    //   2. Wait for the navigation to finish
    TestFrameNavigationObserver observer(
        GetFencedFrameRenderFrameHost(*execution_target));
    content::EvalJsResult eval_result =
        EvalJs(execution_target ? *execution_target : shell(),
               base::StringPrintf(
                   R"(
(async function() {
try {
  const auction_config = %s;
  auction_config.resolveToConfig = true;

  const fenced_frame_config = await navigator.runAdAuction(auction_config);
  if (!(fenced_frame_config instanceof FencedFrameConfig)) {
    throw new Error('runAdAuction() did not return a FencedFrameConfig');
  }

  document.querySelector('fencedframe').config = fenced_frame_config;
} catch (e) {
  return e.toString();
}
})())",
                   auction_config_json.c_str()));

    ASSERT_TRUE(eval_result.value.is_none())
        << "Expected string, but got " << eval_result.value;
    WaitForFencedFrameNavigation(expected_ad_url, *execution_target, observer);
  }

  // Navigates the only fenced frame in `execution_target` to `url` and invokes
  // `WaitForFencedFrameNavigation()`.
  void NavigateFencedFrameAndWait(const GURL& url,
                                  const GURL& expected_url,
                                  const ToRenderFrameHost& execution_target) {
    TestFrameNavigationObserver observer(
        GetFencedFrameRenderFrameHost(execution_target));

    EXPECT_TRUE(ExecJs(execution_target,
                       JsReplace("document.querySelector('fencedframe').config "
                                 "= new FencedFrameConfig($1);",
                                 url)));

    WaitForFencedFrameNavigation(expected_url, execution_target, observer);
  }

  // Waits for a fenced frame navigation to complete in `execution_target`,
  // expecting the frame to navigate to `expected_url`. Also checks that the URL
  // is actually requested from the test server if `expected_url` is an HTTPS
  // URL. `observer` must be set up before the navigation-initiating code is
  // run. We wait on it in this method.
  void WaitForFencedFrameNavigation(const GURL& expected_url,
                                    const ToRenderFrameHost& execution_target,
                                    TestFrameNavigationObserver& observer) {
    // If the URL is HTTPS, wait for the URL to be requested, to make sure the
    // fenced frame actually made the request and, in the MPArch case, to make
    // sure the load actually started. On regression, this is likely to hang.
    if (expected_url.SchemeIs(url::kHttpsScheme)) {
      WaitForUrl(expected_url);
    } else {
      // The only other URLs this should be used with are about:blank URLs.
      ASSERT_EQ(GURL(url::kAboutBlankURL), expected_url);
    }

    // Wait for the load to complete.
    observer.Wait();

    RenderFrameHost* fenced_frame_host =
        GetFencedFrameRenderFrameHost(execution_target);
    // Verify that the URN was resolved to the correct URL.
    EXPECT_EQ(expected_url, fenced_frame_host->GetLastCommittedURL());

    // Make sure the URL was successfully committed. If the page failed to load
    // the URL will be `expected_url`, but IsErrorDocument() will be true, and
    // the last committed origin will be opaque.
    EXPECT_FALSE(fenced_frame_host->IsErrorDocument());
    // If scheme is HTTP or HTTPS, check the last committed origin here. If
    // scheme is about:blank, don't do so, since url::Origin::Create() will
    // return an opaque origin in that case.
    if (expected_url.SchemeIsHTTPOrHTTPS()) {
      EXPECT_EQ(url::Origin::Create(expected_url),
                fenced_frame_host->GetLastCommittedOrigin());
    }
  }

  // Returns the RenderFrameHostImpl for a fenced frame in `execution_target`,
  // which is assumed to contain only one fenced frame and no iframes.
  RenderFrameHostImpl* GetFencedFrameRenderFrameHost(
      const ToRenderFrameHost& execution_target) {
    return GetFencedFrame(execution_target)->GetInnerRoot();
  }

  // Returns FencedFrame in `execution_target` frame. Requires that
  // `execution_target` have one and only one FencedFrame. MPArch only, as the
  // ShadowDOM implementation doesn't use the FencedFrame class.
  FencedFrame* GetFencedFrame(const ToRenderFrameHost& execution_target) {
    std::vector<FencedFrame*> fenced_frames =
        static_cast<RenderFrameHostImpl*>(execution_target.render_frame_host())
            ->GetFencedFrames();
    CHECK_EQ(1u, fenced_frames.size());
    return fenced_frames[0];
  }

  // When using default bidding and decision logic:
  // Navigates the main frame, adds an interest group with a single component
  // URL, and runs an auction where an ad with that component URL wins.
  // Navigates a fenced frame to the winning render URL (which contains a nested
  // fenced frame), and navigates that fenced frame to the component ad URL.
  // Provides a common starting state for testing behavior of component ads and
  // fenced frames.
  //
  // Writes URN for the component ad to `component_ad_urn`, if non-null.
  void RunBasicAuctionWithAdComponents(
      const GURL& ad_component_url,
      GURL* component_ad_urn = nullptr,
      std::string bidding_logic = "bidding_logic.js",
      std::string decision_logic = "decision_logic.js") {
    GURL test_url =
        https_server_->GetURL("a.test", "/fenced_frames/basic.html");
    ASSERT_TRUE(NavigateToURL(shell(), test_url));

    GURL ad_url = https_server_->GetURL("c.test", "/fenced_frames/basic.html");
    EXPECT_EQ(
        kSuccess,
        JoinInterestGroupAndVerify(
            /*owner=*/url::Origin::Create(test_url),
            /*name=*/"cars",
            /*priority=*/0.0, /*execution_mode=*/
            blink::InterestGroup::ExecutionMode::kCompatibilityMode,
            /*bidding_url=*/
            https_server_->GetURL("a.test", "/interest_group/" + bidding_logic),
            /*ads=*/{{{ad_url, /*metadata=*/absl::nullopt}}},
            /*ad_components=*/
            {{{ad_component_url, /*metadata=*/absl::nullopt}}}));

    ASSERT_NO_FATAL_FAILURE(RunAuctionAndNavigateFencedFrame(
        ad_url,
        JsReplace(R"({
          seller: $1,
          decisionLogicUrl: $2,
          interestGroupBuyers: [$1]
        })",
                  url::Origin::Create(test_url),
                  https_server_->GetURL("a.test",
                                        "/interest_group/" + decision_logic)),
        /*execution_target=*/absl::nullopt));

    // Get first component URL from the fenced frame.
    RenderFrameHost* ad_frame = GetFencedFrameRenderFrameHost(shell());
    absl::optional<std::vector<GURL>> components =
        GetAdAuctionComponentsInJS(ad_frame, 1);
    ASSERT_TRUE(components);
    ASSERT_EQ(1u, components->size());
    EXPECT_EQ(url::kUrnScheme, (*components)[0].scheme_piece());
    if (component_ad_urn)
      *component_ad_urn = (*components)[0];

    // Load the ad component in the nested fenced frame. The load should
    // succeed.
    NavigateFencedFrameAndWait((*components)[0], ad_component_url, ad_frame);
  }

  absl::optional<std::vector<GURL>> GetAdAuctionComponentsInJS(
      const ToRenderFrameHost& execution_target,
      int num_params) {
    auto result = EvalJs(
        execution_target,
        base::StringPrintf("navigator.adAuctionComponents(%i)", num_params));
    // Return nullopt if an exception was thrown, as should be the case for
    // loading pages that are not the result of an auction.
    if (!result.error.empty())
      return absl::nullopt;

    // Otherwise, adAuctionComponents should always return a list, since it
    // forces its input to be a number, and clamps it to the expected range.
    EXPECT_TRUE(result.value.is_list());
    if (!result.value.is_list())
      return absl::nullopt;

    std::vector<GURL> out;
    for (const auto& value : result.value.GetList()) {
      if (!value.is_string()) {
        ADD_FAILURE() << "Expected string: " << value;
        return std::vector<GURL>();
      }
      GURL url(value.GetString());
      if (!url.is_valid() || !url.SchemeIs(url::kUrnScheme)) {
        ADD_FAILURE() << "Expected valid URN URL: " << value;
        return std::vector<GURL>();
      }
      out.emplace_back(std::move(url));
    }
    return out;
  }

  // Validates that navigator.adAuctionComponents() returns URNs that map to
  // `expected_ad_component_urls`. `expected_ad_component_urls` is padded with
  // about:blank URLs up to blink::kMaxAdAuctionAdComponents. Calls
  // adAuctionComponents() with a number of different input parameters to get a
  // list of URNs and checks them against FencedFrameURLMapping to make sure
  // they're mapped to `expected_ad_component_urls`, and in the same order.
  void CheckAdComponents(std::vector<GURL> expected_ad_component_urls,
                         RenderFrameHostImpl* render_frame_host) {
    while (expected_ad_component_urls.size() <
           blink::kMaxAdAuctionAdComponents) {
      expected_ad_component_urls.emplace_back(url::kAboutBlankURL);
    }

    absl::optional<std::vector<GURL>> all_component_urls =
        GetAdAuctionComponentsInJS(render_frame_host,
                                   blink::kMaxAdAuctionAdComponents);
    ASSERT_TRUE(all_component_urls);
    ASSERT_EQ(blink::kMaxAdAuctionAdComponents, all_component_urls->size());
    for (size_t i = 0; i < all_component_urls->size(); ++i) {
      // All ad component URLs should use the URN scheme.
      EXPECT_EQ(url::kUrnScheme, (*all_component_urls)[i].scheme_piece());

      // All ad component URLs should be unique.
      for (size_t j = 0; j < i; ++j)
        EXPECT_NE((*all_component_urls)[i], (*all_component_urls)[j]);

      // Check URNs are mapped to the values in `expected_ad_component_urls`.
      TestFencedFrameURLMappingResultObserver observer;
      ConvertFencedFrameURNToURL((*all_component_urls)[i], &observer,
                                 render_frame_host);
      EXPECT_TRUE(observer.mapped_url());
      EXPECT_EQ(expected_ad_component_urls[i], observer.mapped_url());
    }

    // Make sure smaller values passed to GetAdAuctionComponentsInJS() return
    // the first elements of the full kMaxAdAuctionAdComponents element list
    // retrieved above.
    for (size_t i = 0; i < blink::kMaxAdAuctionAdComponents; ++i) {
      absl::optional<std::vector<GURL>> component_urls =
          GetAdAuctionComponentsInJS(render_frame_host, i);
      ASSERT_TRUE(component_urls);
      EXPECT_THAT(*component_urls,
                  testing::ElementsAreArray(all_component_urls->begin(),
                                            all_component_urls->begin() + i));
    }

    // Test clamping behavior.
    EXPECT_EQ(std::vector<GURL>(),
              GetAdAuctionComponentsInJS(render_frame_host, -32769));
    EXPECT_EQ(std::vector<GURL>(),
              GetAdAuctionComponentsInJS(render_frame_host, -2));
    EXPECT_EQ(std::vector<GURL>(),
              GetAdAuctionComponentsInJS(render_frame_host, -1));
    EXPECT_EQ(all_component_urls,
              GetAdAuctionComponentsInJS(render_frame_host,
                                         blink::kMaxAdAuctionAdComponents + 1));
    EXPECT_EQ(all_component_urls,
              GetAdAuctionComponentsInJS(render_frame_host,
                                         blink::kMaxAdAuctionAdComponents + 2));
    EXPECT_EQ(all_component_urls,
              GetAdAuctionComponentsInJS(render_frame_host, 32768));
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

// Make sure that FLEDGE has protections against making local network requests..
class InterestGroupLocalNetworkBrowserTest : public InterestGroupBrowserTest {
 protected:
  InterestGroupLocalNetworkBrowserTest()
      : remote_test_server_(net::test_server::EmbeddedTestServer::TYPE_HTTPS) {
    feature_list_.InitAndEnableFeature(
        features::kPrivateNetworkAccessRespectPreflightResults);

    remote_test_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    remote_test_server_.AddDefaultHandlers(GetTestDataFilePath());
    remote_test_server_.RegisterRequestMonitor(base::BindRepeating(
        &InterestGroupBrowserTest::OnHttpsTestServerRequestMonitor,
        base::Unretained(this)));
    EXPECT_TRUE(remote_test_server_.Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        network::switches::kIpAddressSpaceOverrides,
        base::StringPrintf(
            "%s=public",
            remote_test_server_.host_port_pair().ToString().c_str()));
  }

  void SetUpOnMainThread() override {
    InterestGroupBrowserTest::SetUpOnMainThread();

    // Extend allow list to include the remote server.
    content_browser_client_->AddToAllowList(
        {url::Origin::Create(remote_test_server_.GetURL("a.test", "/")),
         url::Origin::Create(remote_test_server_.GetURL("b.test", "/")),
         url::Origin::Create(remote_test_server_.GetURL("c.test", "/"))});
  }

 protected:
  // Test server which is treated as remote, due to command line options. Can't
  // use "Content-Security-Policy: treat-as-public-address", because that would
  // block all local requests, including loading the seller script, even if the
  // seller script had the same header.
  net::test_server::EmbeddedTestServer remote_test_server_;

  base::test::ScopedFeatureList feature_list_;
};

// More restricted Permissions Policy is set for features join-ad-interest-group
// and run-ad-auction (EnableForSelf instead of EnableForAll) when runtime flag
// kAdInterestGroupAPIRestrictedPolicyByDefault is enabled.
class InterestGroupRestrictedPermissionsPolicyBrowserTest
    : public InterestGroupBrowserTest {
 public:
  InterestGroupRestrictedPermissionsPolicyBrowserTest() {
    feature_list_.InitAndEnableFeature(
        blink::features::kAdInterestGroupAPIRestrictedPolicyByDefault);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       SameOriginJoinLeaveInterestGroup) {
  GURL test_url_a = https_server_->GetURL("a.test", "/echo");
  url::Origin test_origin_a = url::Origin::Create(test_url_a);
  ASSERT_TRUE(test_url_a.SchemeIs(url::kHttpsScheme));
  ASSERT_TRUE(NavigateToURL(shell(), test_url_a));

  // This join should succeed.
  EXPECT_EQ(kSuccess, JoinInterestGroupAndVerify(test_origin_a, "cars"));

  // This join should fail and throw an exception since a.test is not the same
  // origin as the bidding_url, bid.a.test.
  EXPECT_EQ(
      base::StringPrintf(
          "TypeError: Failed to execute 'joinAdInterestGroup' on 'Navigator': "
          "biddingUrl 'https://bid.a.test/' for AuctionAdInterestGroup with "
          "owner '%s' and name 'bicycles' biddingUrl must have the same origin "
          "as the InterestGroup owner and have no fragment identifier or "
          "embedded credentials.",
          test_origin_a.Serialize().c_str()),
      JoinInterestGroupAndVerify(blink::TestInterestGroupBuilder(
                                     /*owner=*/test_origin_a,
                                     /*name=*/"bicycles")
                                     .SetBiddingUrl(GURL("https://bid.a.test"))
                                     .Build()));

  // This join should fail and throw an exception since a.test is not the same
  // origin as the update_url, update.a.test.
  EXPECT_EQ(
      base::StringPrintf(
          "TypeError: Failed to execute 'joinAdInterestGroup' on 'Navigator': "
          "updateUrl 'https://update.a.test/' for AuctionAdInterestGroup with "
          "owner '%s' and name 'tricycles' updateUrl must have the same origin "
          "as the InterestGroup owner and have no fragment identifier or "
          "embedded credentials.",
          test_origin_a.Serialize().c_str()),
      JoinInterestGroupAndVerify(
          blink::TestInterestGroupBuilder(
              /*owner=*/test_origin_a,
              /*name=*/"tricycles")
              .SetUpdateUrl(GURL("https://update.a.test"))
              .Build()));

  // This join should fail and throw an exception since a.test is not the same
  // origin as the trusted_bidding_signals_url, signals.a.test.
  EXPECT_EQ(base::StringPrintf(
                "TypeError: Failed to execute 'joinAdInterestGroup' on "
                "'Navigator': trustedBiddingSignalsUrl "
                "'https://signals.a.test/' for AuctionAdInterestGroup with "
                "owner '%s' and name 'four-wheelers' trustedBiddingSignalsUrl "
                "must have the same origin as the InterestGroup owner and have "
                "no query string, fragment identifier or embedded credentials.",
                test_origin_a.Serialize().c_str()),
            JoinInterestGroupAndVerify(
                blink::TestInterestGroupBuilder(
                    /*owner=*/test_origin_a,
                    /*name=*/"four-wheelers")
                    .SetTrustedBiddingSignalsUrl(GURL("https://signals.a.test"))
                    .Build()));

  // This join should silently fail since d.test is not allowlisted for the API,
  // and allowlist checks only happen in the browser process, so don't throw an
  // exception. Can't use JoinInterestGroupAndVerify() because of the silent
  // failure.
  GURL test_url_d = https_server_->GetURL("d.test", "/echo");
  url::Origin test_origin_d = url::Origin::Create(test_url_d);
  ASSERT_TRUE(NavigateToURL(shell(), test_url_d));
  EXPECT_EQ(kSuccess, JoinInterestGroup(test_origin_d, "toys"));

  // Another successful join.
  GURL test_url_b = https_server_->GetURL("b.test", "/echo");
  url::Origin test_origin_b = url::Origin::Create(test_url_b);
  ASSERT_TRUE(NavigateToURL(shell(), test_url_b));
  EXPECT_EQ(kSuccess, JoinInterestGroupAndVerify(test_origin_b, "trucks"));

  // Check that only the a.test and b.test interest groups were added to
  // the database.
  std::vector<blink::InterestGroupKey> expected_groups = {
      {test_origin_a, "cars"}, {test_origin_b, "trucks"}};
  std::vector<blink::InterestGroupKey> received_groups;
  received_groups = GetAllInterestGroups();
  EXPECT_THAT(received_groups,
              testing::UnorderedElementsAreArray(expected_groups));

  // Now test leaving
  // Test that we can't leave an interest group from a site not allowedlisted
  // for the API.
  // Inject an interest group into the DB for that site so we can try
  // to remove it.
  manager_->JoinInterestGroup(blink::TestInterestGroupBuilder(
                                  /*owner=*/test_origin_d,
                                  /*name=*/"candy")
                                  .Build(),
                              test_origin_d.GetURL());

  ASSERT_TRUE(NavigateToURL(shell(), test_url_d));
  // This leave should do nothing because `origin_d` is not allowed by privacy
  // sandbox. Can't use LeaveInterestGroupAndVerify() because it returns "true"
  // but doesn't actually leave the interest group.
  EXPECT_EQ(kSuccess, LeaveInterestGroup(test_origin_d, "candy"));

  ASSERT_TRUE(NavigateToURL(shell(), test_url_b));
  // This leave should do nothing because there is not interest group of that
  // name.
  EXPECT_EQ(kSuccess, LeaveInterestGroupAndVerify(test_origin_b, "cars"));

  // This leave should succeed.
  ASSERT_TRUE(NavigateToURL(shell(), test_url_a));
  EXPECT_EQ(kSuccess, LeaveInterestGroupAndVerify(test_origin_a, "cars"));

  // We expect that `test_origin_b` and the (injected) `test_origin_d` interest
  // groups remain.
  expected_groups = {{test_origin_b, "trucks"}, {test_origin_d, "candy"}};
  received_groups = GetAllInterestGroups();
  EXPECT_THAT(received_groups,
              testing::UnorderedElementsAreArray(expected_groups));
}

// Test the case of a cross-origin iframe joining and leaving same-origin
// interest groups. This should succeed without any .well-known fetches needed.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       SameOriginIframeJoinLeaveInterestGroup) {
  const char kGroup[] = "goats";

  // b.test iframes a.test. The iframe should be able to successfully join and
  // leave a.test interest group without needing any .well-known fetches.
  GURL main_url =
      https_server_->GetURL("b.test",
                            "/cross_site_iframe_factory.html?b.test("
                            "a.test{allow-join-ad-interest-group}"
                            ")");
  ASSERT_TRUE(NavigateToURL(shell(), main_url));

  url::Origin group_origin = https_server_->GetOrigin("a.test");

  FrameTreeNode* parent =
      FrameTreeNode::From(web_contents()->GetPrimaryMainFrame());
  ASSERT_GT(parent->child_count(), 0u);
  RenderFrameHost* iframe = parent->child_at(0)->current_frame_host();

  // Both joining and leaving should work.
  EXPECT_EQ(kSuccess,
            JoinInterestGroupAndVerify(
                group_origin, kGroup,
                /*priority=*/0, /*execution_mode=*/
                blink::InterestGroup::ExecutionMode::kCompatibilityMode,
                /*bidding_url=*/absl::nullopt,
                /*ads=*/absl::nullopt,
                /*ad_components=*/absl::nullopt, iframe));
  EXPECT_EQ(kSuccess,
            LeaveInterestGroupAndVerify(group_origin, kGroup, iframe));
}

// Test cross-origin joining/leaving of interest groups, in the case an IG owner
// only allows cross-origin joining. Only allow joining to make sure join and
// leave permissions are tracked separately.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       CrossOriginJoinAllowedByWellKnownFetch) {
  const char kGroup[] = "aardvarks";

  url::Origin allow_join_origin = url::Origin::Create(
      GURL(https_server_->GetURL("allow-join.a.test", "/")));

  // Navigate to a cross-origin URL.
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("b.test", "/echo")));

  // Joining a group cross-origin should succeed.
  EXPECT_EQ(kSuccess, JoinInterestGroupAndVerify(allow_join_origin, kGroup));

  // Leaving the group should fail.
  EXPECT_EQ("NotAllowedError: Permission to leave interest group denied.",
            LeaveInterestGroupAndVerify(allow_join_origin, kGroup));
}

// Test cross-origin joining/leaving of interest groups, in the case an IG owner
// only allows cross-origin leaving. Only allow leaving to make sure leave and
// leave permissions are tracked separately.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       CrossOriginLeaveAllowedByWellKnownFetch) {
  // Group that's joined with a same-origin request.
  const char kJoinSucceedsGroup[] = "join-succeeds-group";
  // Group where joining fails due to the request being cross-origin, and
  // cross-origin joins being blocked.
  const char kJoinFailsGroup[] = "join-fails-group";

  GURL allow_leave_url =
      GURL(https_server_->GetURL("allow-leave.a.test", "/echo"));
  url::Origin allow_leave_origin = url::Origin::Create(allow_leave_url);

  // Navigate to the origin that allows leaving only, and join one of its
  // groups, which should succeed.
  ASSERT_TRUE(NavigateToURL(shell(), allow_leave_url));
  EXPECT_EQ(kSuccess,
            JoinInterestGroupAndVerify(allow_leave_origin, kJoinSucceedsGroup));

  // Navigate to a cross-origin URL.
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("b.test", "/echo")));

  // Try to join an `allow_leave_origin` group, which should fail.
  EXPECT_EQ("NotAllowedError: Permission to join interest group denied.",
            JoinInterestGroupAndVerify(allow_leave_origin, kJoinFailsGroup));

  // Leaving the group that was successfully joined earlier should succeed.
  EXPECT_EQ(kSuccess, LeaveInterestGroupAndVerify(allow_leave_origin,
                                                  kJoinSucceedsGroup));
}

// Test cross-origin joining/leaving of interest groups from an iframe, in the
// case an IG owner only allows cross-origin joining. Only allow joining to make
// sure join and leave permissions are tracked separately.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       CrossOriginIframeJoinAllowedByWellKnownFetch) {
  const char kGroup[] = "aardvarks";

  url::Origin allow_join_origin = url::Origin::Create(
      GURL(https_server_->GetURL("allow-join.a.test", "/")));

  // allow-join.a.test iframes b.test. The iframe should require preflights to
  // be able to join or leave allow-join.a.test's interest groups. In this test,
  // the preflights only allow joins.
  GURL main_url =
      https_server_->GetURL("allow-join.a.test",
                            "/cross_site_iframe_factory.html?allow-join.a.test("
                            "b.test{allow-join-ad-interest-group}"
                            ")");
  ASSERT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* parent =
      FrameTreeNode::From(web_contents()->GetPrimaryMainFrame());
  ASSERT_GT(parent->child_count(), 0u);
  RenderFrameHost* iframe = parent->child_at(0)->current_frame_host();

  // Joining a group cross-origin should succeed.
  EXPECT_EQ(kSuccess,
            JoinInterestGroupAndVerify(
                allow_join_origin, kGroup,
                /*priority=*/0, /*execution_mode=*/
                blink::InterestGroup::ExecutionMode::kCompatibilityMode,
                /*bidding_url=*/absl::nullopt,
                /*ads=*/absl::nullopt,
                /*ad_components=*/absl::nullopt, iframe));

  // Leaving the group should fail.
  EXPECT_EQ("NotAllowedError: Permission to leave interest group denied.",
            LeaveInterestGroupAndVerify(allow_join_origin, kGroup, iframe));
}

// Test cross-origin joining/leaving of interest groups from an iframe, in the
// case an IG owner only allows cross-origin leaving. Only allow leaving to make
// sure join and leave permissions are tracked separately.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       CrossOriginIframeLeaveAllowedByWellKnownFetch) {
  // Group that's joined with a same-origin request.
  const char kJoinSucceedsGroup[] = "join-succeeds-group";
  // Group where joining fails due to the request being cross-origin, and
  // cross-origin joins being blocked.
  const char kJoinFailsGroup[] = "join-fails-group";

  url::Origin allow_leave_origin =
      https_server_->GetOrigin("allow-leave.a.test");

  // allow-leave.a.test iframes b.test. The iframe should require preflights to
  // be able to join or leave allow-leave.a.test's interest groups. In this
  // test, the preflights only allow leaves.
  GURL main_url = https_server_->GetURL(
      "allow-leave.a.test",
      "/cross_site_iframe_factory.html?allow-leave.a.test("
      "b.test{allow-join-ad-interest-group}"
      ")");
  ASSERT_TRUE(NavigateToURL(shell(), main_url));

  // The main frame joins kJoinSucceedsGroup, which should succeed.
  EXPECT_EQ(kSuccess,
            JoinInterestGroupAndVerify(allow_leave_origin, kJoinSucceedsGroup));

  FrameTreeNode* parent =
      FrameTreeNode::From(web_contents()->GetPrimaryMainFrame());
  ASSERT_GT(parent->child_count(), 0u);
  RenderFrameHost* iframe = parent->child_at(0)->current_frame_host();

  // Try to join an `allow_leave_origin` group from the iframe, which should
  // fail.
  EXPECT_EQ("NotAllowedError: Permission to join interest group denied.",
            JoinInterestGroupAndVerify(
                allow_leave_origin, kJoinFailsGroup,
                /*priority=*/0.0, /*execution_mode=*/
                blink::InterestGroup::ExecutionMode::kCompatibilityMode,
                /*bidding_url=*/absl::nullopt,
                /*ads=*/absl::nullopt,
                /*ad_components=*/
                absl::nullopt,
                /*execution_target=*/iframe));

  // Leaving the group from the iframe that was successfully joined earlier
  // should succeed.
  EXPECT_EQ(kSuccess, LeaveInterestGroupAndVerify(allow_leave_origin,
                                                  kJoinSucceedsGroup, iframe));
}

// Test the case cross-origin joining/leaving of interest groups is blocked by
// the ContentBrowserClient, but allowed by the .well-known URL. In this case,
// the .well-known URL should be fetched, and the return value should be derived
// from that fetch returned, but the database should not updated, regardless of
// whether the .well-known URL allows it. This can happen if, for example,
// cookies blocking is enabled for a site.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       CrossOriginJoinLeaveBlockedByContentBrowserClient) {
  const char kGroup1[] = "aardvarks";
  const char kGroup2[] = "wombats";

  // Interest groups operations are not allowed on "*.d.test" by the
  // ContentBrowserClient. One allows only joins, one only leaves, which should
  // affect return values, but not whether the page can actually join or leave
  // cross-origin interest groups.
  url::Origin allow_join_origin = https_server_->GetOrigin("allow-join.d.test");
  url::Origin allow_leave_origin =
      https_server_->GetOrigin("allow-leave.d.test");

  // Join kGroup2 directly for both origins, so can check leave calls have no
  // effect.
  blink::InterestGroup interest_group;
  interest_group.owner = allow_join_origin;
  interest_group.name = kGroup2;
  interest_group.expiry = base::Time::Now() + base::Days(1);
  // The joining URL doesn't actually matter.
  manager_->JoinInterestGroup(
      interest_group,
      /*joining_url=*/https_server_->GetURL("allow-join.d.test", "/"));
  interest_group.owner = allow_leave_origin;
  manager_->JoinInterestGroup(
      interest_group,
      /*joining_url=*/https_server_->GetURL("allow-leave.d.test", "/"));

  // Navigate to a cross-origin URL.
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("b.test", "/echo")));

  // Join/leave calls for `allow_join_origin` should claim joining succeeded,
  // and leaving failed, but neither call should actually affect what interest
  // groups the user is in.
  EXPECT_EQ(kSuccess, JoinInterestGroup(allow_join_origin, kGroup1));
  EXPECT_EQ("NotAllowedError: Permission to leave interest group denied.",
            LeaveInterestGroup(allow_join_origin, kGroup2));

  // Join/leave calls for `allow_leave_origin` should claim joining failed, and
  // leaving succeeded, but neither call should actually affect what interest
  // groups the user is in.
  EXPECT_EQ("NotAllowedError: Permission to join interest group denied.",
            JoinInterestGroup(allow_leave_origin, kGroup1));
  EXPECT_EQ(kSuccess, LeaveInterestGroup(allow_leave_origin, kGroup2));

  // The user should still be in kGroup2, but not kGroup1, for both origins.
  std::vector<blink::InterestGroupKey> expected_groups = {
      {allow_join_origin, kGroup2}, {allow_leave_origin, kGroup2}};
  EXPECT_THAT(GetAllInterestGroups(),
              testing::UnorderedElementsAreArray(expected_groups));
}

// Test cross-origin joining of interest groups requires CORS.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, CrossOriginJoinNoCors) {
  const char kGroup[] = "aardvarks";

  url::Origin no_cors_origin =
      url::Origin::Create(GURL(https_server_->GetURL("no-cors.a.test", "/")));

  // Navigate to a cross-origin URL.
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("b.test", "/echo")));

  // Joining a group should fail.
  EXPECT_EQ("NotAllowedError: Permission to join interest group denied.",
            JoinInterestGroupAndVerify(no_cors_origin, kGroup));
}

// Test cross-origin leaving of interest groups requires CORS.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, CrossOriginLeaveNoCors) {
  const char kGroup[] = "aardvarks";

  GURL no_cors_url = GURL(https_server_->GetURL("no-cors.a.test", "/echo"));
  url::Origin no_cors_origin = url::Origin::Create(no_cors_url);

  // Navigate to `no_cors_url` and join an IG, which should succeed, since it's
  // a same-origin join.
  ASSERT_TRUE(NavigateToURL(shell(), no_cors_url));
  EXPECT_EQ(kSuccess, JoinInterestGroupAndVerify(no_cors_origin, kGroup));

  // Navigate to a cross-origin URL.
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("b.test", "/echo")));

  // Leaving the group should fail.
  EXPECT_EQ("NotAllowedError: Permission to leave interest group denied.",
            LeaveInterestGroupAndVerify(no_cors_origin, kGroup));
}

// Test the renderer restricting the number of active cross-origin joins per
// frame. One page tries to join kMaxActiveCrossSiteJoins+1 cross-origin
// interest groups from 3 different origins, the last two requests are to two
// distinct origins. All but the last are sent to the browser process. The
// results in two .well-known permissions requests. While those two requests are
// pending, the last request is held back in the renderer.
//
// Then the site joins and leaves a same-origin interest group, which should
// bypass the queue. Then one of the hung .well-known requests completes, which
// should allow the final cross-origin join to send out its .well-known request.
//
// Then a cross-origin leave request is issued for the group just joined, which
// should not wait before sending the request to the browser process, since
// leaves and joins are throttled separately. The browser process then leaves
// the group immediately, using the cached result of the previous .well-known
// fetch.
//
// The remaining two .well-known requests for the joins are then completed,
// which should result in all pending joins completing successfully.
//
// The title of the page is updated when each promise completes successfully, to
// allow waiting on promises that were created earlier in the test run.
//
// Only 3 cross-site origins are used to limit the test to 3 simultaneous
// .well-known requests. Using too many would run into the network stack's
// request throttling code.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, CrossOriginJoinQueue) {
  // This matches the value in navigator_auction.cc in blink/.
  const int kMaxActiveCrossSiteJoins = 20;

  // Since this is using another port from `cross_origin_server` below, the
  // hostname doesn't matter, but use a different one, just in case.
  GURL main_url = https_server_->GetURL("a.test", "/echo");
  url::Origin main_origin = url::Origin::Create(main_url);

  std::vector<std::unique_ptr<net::test_server::ControllableHttpResponse>>
      permissions_responses;
  net::EmbeddedTestServer cross_origin_server(
      net::test_server::EmbeddedTestServer::TYPE_HTTPS);
  cross_origin_server.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  // There should be 3 .well-known requests for the cross-origin joins. The
  // cross-origin leave should use a cached result.
  for (int i = 0; i < 3; ++i) {
    permissions_responses.emplace_back(
        std::make_unique<net::test_server::ControllableHttpResponse>(
            &cross_origin_server,
            "/.well-known/interest-group/permissions/?origin=" +
                base::EscapeQueryParamValue(main_origin.Serialize(),
                                            /*use_plus=*/false)));
  }
  ASSERT_TRUE(cross_origin_server.Start());

  // Navigate to a cross-origin URL.
  ASSERT_TRUE(NavigateToURL(shell(), main_url));

  for (int i = 0; i < kMaxActiveCrossSiteJoins + 1; ++i) {
    const char* other_origin_host = "0.b.test";
    if (i == kMaxActiveCrossSiteJoins - 1) {
      other_origin_host = "1.b.test";
    } else if (i == kMaxActiveCrossSiteJoins) {
      other_origin_host = "2.b.test";
    }
    url::Origin other_origin = cross_origin_server.GetOrigin(other_origin_host);
    content_browser_client_->AddToAllowList({other_origin});

    ExecuteScriptAsync(shell(),
                       JsReplace(R"(
navigator.joinAdInterestGroup(
    {name: $1, owner: $2}, /*joinDurationSec=*/ 300)
    .then(() => {
      // Append the first character of the owner's host to the title.
      document.title += (new URL($2)).host[0];
    });)",
                                 base::NumberToString(i), other_origin));

    // Wait for .well-known requests to be made for "0.b.test" and "1.b.test".
    // Need to wait for them immediately after the Javascript calls that should
    // trigger the requests to prevent their order from being racily reversed at
    // the network layer.
    if (i == 0) {
      permissions_responses[0]->WaitForRequest();
      EXPECT_TRUE(base::StartsWith(
          permissions_responses[0]->http_request()->headers.at("Host"),
          "0.b.test"));
    } else if (i == kMaxActiveCrossSiteJoins - 1) {
      permissions_responses[1]->WaitForRequest();
      EXPECT_TRUE(base::StartsWith(
          permissions_responses[1]->http_request()->headers.at("Host"),
          "1.b.test"));
    }
  }

  // Clear title, as each successful join modifies the title, so need a basic
  // title to start with. Can't set an empty title, so use "_" instead.
  ExecuteScriptAsync(shell(), "document.title='_'");

  // Joining and leaving a same-origin interest group should not be throttled.
  EXPECT_EQ(kSuccess,
            JoinInterestGroupAndVerify(main_origin, "helmets for unicorns"));
  EXPECT_EQ(kSuccess,
            LeaveInterestGroupAndVerify(main_origin, "helmets for unicorns"));

  // The "2.b.test" cross-origin join should still be waiting for one of the
  // other cross-site joins to complete.
  EXPECT_FALSE(permissions_responses[2]->has_received_request());

  // Complete the "1.b.test" .well-known request, which should cause the
  // "2.b.test" join request to be sent to the browser, which should issue
  // another .well-known request.
  TitleWatcher title_watcher1(web_contents(), u"_1");
  permissions_responses[1]->Send(
      net::HttpStatusCode::HTTP_OK,
      /*content_type=*/"application/json",
      /*content=*/
      R"({"joinAdInterestGroup" : true, "leaveAdInterestGroup" : true})",
      /*cookies=*/{},
      /*extra_headers=*/{"Access-Control-Allow-Origin: *"});
  permissions_responses[1]->Done();
  EXPECT_EQ(u"_1", title_watcher1.WaitAndGetTitle());

  // The "2.b.test" cross-origin join should advance out of the queue and send a
  // .well-known request.
  permissions_responses[2]->WaitForRequest();
  EXPECT_TRUE(base::StartsWith(
      permissions_responses[2]->http_request()->headers.at("Host"),
      "2.b.test"));

  // A new cross-origin leave should bypass the join queue, and start
  // immediately, retrieving the previous .well-known response from the cache.
  EXPECT_EQ(kSuccess,
            LeaveInterestGroupAndVerify(
                /*owner=*/cross_origin_server.GetOrigin("1.b.test"),
                /*name=*/base::NumberToString(kMaxActiveCrossSiteJoins)));

  // Complete the "2.b.test" join's .well-known request.
  TitleWatcher title_watcher2(web_contents(), u"_12");
  permissions_responses[2]->Send(
      net::HttpStatusCode::HTTP_OK,
      /*content_type=*/"application/json",
      /*content=*/R"({"joinAdInterestGroup" : true})",
      /*cookies=*/{},
      /*extra_headers=*/{"Access-Control-Allow-Origin: *"});
  permissions_responses[2]->Done();
  EXPECT_EQ(u"_12", title_watcher2.WaitAndGetTitle());

  // Complete the "0.b.test" joins' .well-known request.
  std::u16string final_title =
      u"_12" + std::u16string(kMaxActiveCrossSiteJoins - 1, u'0');
  TitleWatcher title_watcher3(web_contents(), final_title);
  permissions_responses[0]->Send(
      net::HttpStatusCode::HTTP_OK,
      /*content_type=*/"application/json",
      /*content=*/R"({"joinAdInterestGroup" : true})",
      /*cookies=*/{},
      /*extra_headers=*/{"Access-Control-Allow-Origin: *"});
  permissions_responses[0]->Done();
  EXPECT_EQ(final_title, title_watcher3.WaitAndGetTitle());
}

// The inverse of CrossOriginJoinQueue. Unlike most leave tests, leaves interest
// groups the user isn't actually in.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, CrossOriginLeaveQueue) {
  // This matches the value in navigator_auction.cc in blink/.
  const int kMaxActiveCrossSiteLeaves = 20;

  // Since this is using another port from `cross_origin_server` below, the
  // hostname doesn't matter, but use a different one, just in case.
  GURL main_url = https_server_->GetURL("a.test", "/echo");
  url::Origin main_origin = url::Origin::Create(main_url);

  std::vector<std::unique_ptr<net::test_server::ControllableHttpResponse>>
      permissions_responses;
  net::EmbeddedTestServer cross_origin_server(
      net::test_server::EmbeddedTestServer::TYPE_HTTPS);
  cross_origin_server.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  // There should be 3 .well-known requests for the cross-origin leaves. The
  // cross-origin join should use a cached result.
  for (int i = 0; i < 3; ++i) {
    permissions_responses.emplace_back(
        std::make_unique<net::test_server::ControllableHttpResponse>(
            &cross_origin_server,
            "/.well-known/interest-group/permissions/?origin=" +
                base::EscapeQueryParamValue(main_origin.Serialize(),
                                            /*use_plus=*/false)));
  }
  ASSERT_TRUE(cross_origin_server.Start());

  // Navigate to a cross-origin URL.
  ASSERT_TRUE(NavigateToURL(shell(), main_url));

  for (int i = 0; i < kMaxActiveCrossSiteLeaves + 1; ++i) {
    const char* other_origin_host = "0.b.test";
    if (i == kMaxActiveCrossSiteLeaves - 1) {
      other_origin_host = "1.b.test";
    } else if (i == kMaxActiveCrossSiteLeaves) {
      other_origin_host = "2.b.test";
    }
    url::Origin other_origin = cross_origin_server.GetOrigin(other_origin_host);
    content_browser_client_->AddToAllowList({other_origin});

    ExecuteScriptAsync(shell(),
                       JsReplace(R"(
navigator.leaveAdInterestGroup({name: $1, owner: $2})
    .then(() => {
      // Append the first character of the owner's host to the title.
      document.title += (new URL($2)).host[0];
    });)",
                                 base::NumberToString(i), other_origin));

    // Wait for .well-known requests to be made for "0.b.test" and "1.b.test".
    // Need to wait for them immediately after the Javascript calls that should
    // trigger the requests to prevent their order from being racily reversed at
    // the network layer.
    if (i == 0) {
      permissions_responses[0]->WaitForRequest();
      EXPECT_TRUE(base::StartsWith(
          permissions_responses[0]->http_request()->headers.at("Host"),
          "0.b.test"));
    } else if (i == kMaxActiveCrossSiteLeaves - 1) {
      permissions_responses[1]->WaitForRequest();
      EXPECT_TRUE(base::StartsWith(
          permissions_responses[1]->http_request()->headers.at("Host"),
          "1.b.test"));
    }
  }

  // Clear title, as each successful leave modifies the title, so need a basic
  // title to start with. Can't set an empty title, so use "_" instead.
  ExecuteScriptAsync(shell(), "document.title='_'");

  // Joining and leaving a same-origin interest group should not be throttled.
  EXPECT_EQ(kSuccess,
            JoinInterestGroupAndVerify(main_origin, "helmets for unicorns"));
  EXPECT_EQ(kSuccess,
            LeaveInterestGroupAndVerify(main_origin, "helmets for unicorns"));

  // The "2.b.test" cross-origin leave should still be waiting for one of the
  // other cross-site leaves to complete.
  EXPECT_FALSE(permissions_responses[2]->has_received_request());

  // Complete the "1.b.test" .well-known request, which should cause the
  // "2.b.test" leave request to be sent to the browser, which should issue
  // another .well-known request.
  TitleWatcher title_watcher1(web_contents(), u"_1");
  permissions_responses[1]->Send(
      net::HttpStatusCode::HTTP_OK,
      /*content_type=*/"application/json",
      /*content=*/
      R"({"joinAdInterestGroup" : true, "leaveAdInterestGroup" : true})",
      /*cookies=*/{},
      /*extra_headers=*/{"Access-Control-Allow-Origin: *"});
  permissions_responses[1]->Done();
  EXPECT_EQ(u"_1", title_watcher1.WaitAndGetTitle());

  // The "2.b.test" cross-origin leave should advance out of the queue and send
  // a .well-known request.
  permissions_responses[2]->WaitForRequest();
  EXPECT_TRUE(base::StartsWith(
      permissions_responses[2]->http_request()->headers.at("Host"),
      "2.b.test"));

  // A new cross-origin join should bypass the leave queue, and start
  // immediately, retrieving the previous .well-known response from the cache.
  EXPECT_EQ(kSuccess,
            JoinInterestGroupAndVerify(
                /*owner=*/cross_origin_server.GetOrigin("1.b.test"),
                /*name=*/base::NumberToString(kMaxActiveCrossSiteLeaves),
                /*priority=*/0.0));

  // Complete the "2.b.test" leave's .well-known request.
  TitleWatcher title_watcher2(web_contents(), u"_12");
  permissions_responses[2]->Send(
      net::HttpStatusCode::HTTP_OK,
      /*content_type=*/"application/json",
      /*content=*/R"({"leaveAdInterestGroup" : true})",
      /*cookies=*/{},
      /*extra_headers=*/{"Access-Control-Allow-Origin: *"});
  permissions_responses[2]->Done();
  EXPECT_EQ(u"_12", title_watcher2.WaitAndGetTitle());

  // Complete the "0.b.test" leaves' .well-known request.
  std::u16string final_title =
      u"_12" + std::u16string(kMaxActiveCrossSiteLeaves - 1, u'0');
  TitleWatcher title_watcher3(web_contents(), final_title);
  permissions_responses[0]->Send(
      net::HttpStatusCode::HTTP_OK,
      /*content_type=*/"application/json",
      /*content=*/R"({"leaveAdInterestGroup" : true})",
      /*cookies=*/{},
      /*extra_headers=*/{"Access-Control-Allow-Origin: *"});
  permissions_responses[0]->Done();
  EXPECT_EQ(final_title, title_watcher3.WaitAndGetTitle());
}

// Much like CrossOriginJoinQueue, but navigates the page when the queue is
// full. Makes sure started joins complete successfully, and a join that was
// still queued when the frame was navigated away is dropped.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       CrossOriginJoinAndNavigateAway) {
  // This matches the value in navigator_auction.cc in blink/.
  const int kMaxActiveCrossSiteJoins = 20;

  // Since this is using another port from `cross_origin_server` below, the
  // hostname doesn't matter, but use a different one, just in case.
  GURL main_url = https_server_->GetURL("a.test", "/echo");
  url::Origin main_origin = url::Origin::Create(main_url);

  // URL with same origin as `main_url` to navigate to afterwards. Same origin
  // so that the renderer process will be shared.
  GURL same_origin_url = https_server_->GetURL("a.test", "/echo?2");

  net::EmbeddedTestServer cross_origin_server(
      net::test_server::EmbeddedTestServer::TYPE_HTTPS);
  cross_origin_server.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  std::vector<std::unique_ptr<net::test_server::ControllableHttpResponse>>
      permissions_responses;
  // While there should only be 2 .well-known requests in this test, create an
  // extra ControllableHttpResponse so can make sure it never sees a request.
  for (int i = 0; i < 3; ++i) {
    permissions_responses.emplace_back(
        std::make_unique<net::test_server::ControllableHttpResponse>(
            &cross_origin_server,
            "/.well-known/interest-group/permissions/?origin=" +
                base::EscapeQueryParamValue(main_origin.Serialize(),
                                            /*use_plus=*/false)));
  }
  ASSERT_TRUE(cross_origin_server.Start());

  // Navigate to a cross-origin URL.
  ASSERT_TRUE(NavigateToURL(shell(), main_url));

  for (int i = 0; i < kMaxActiveCrossSiteJoins + 1; ++i) {
    const char* other_origin_host = "0.b.test";
    if (i == kMaxActiveCrossSiteJoins - 1) {
      other_origin_host = "1.b.test";
    } else if (i == kMaxActiveCrossSiteJoins) {
      other_origin_host = "2.b.test";
    }
    url::Origin other_origin = cross_origin_server.GetOrigin(other_origin_host);
    content_browser_client_->AddToAllowList({other_origin});

    ExecuteScriptAsync(shell(),
                       JsReplace(R"(
navigator.joinAdInterestGroup(
    {name: $1, owner: $2}, /*joinDurationSec=*/ 300);)",
                                 base::NumberToString(i), other_origin));

    // Wait for .well-known requests to be made for "0.b.test" and "1.b.test".
    // Need to wait for them immediately after the Javascript calls that should
    // trigger the requests to prevent their order from being racily reversed at
    // the network layer.
    //
    // Also need to wait for them to make sure the requests reach the browser
    // process before the frame is navigated.
    if (i == 0) {
      permissions_responses[0]->WaitForRequest();
      EXPECT_TRUE(base::StartsWith(
          permissions_responses[0]->http_request()->headers.at("Host"),
          "0.b.test"));
    } else if (i == kMaxActiveCrossSiteJoins - 1) {
      permissions_responses[1]->WaitForRequest();
      EXPECT_TRUE(base::StartsWith(
          permissions_responses[1]->http_request()->headers.at("Host"),
          "1.b.test"));
    }
  }

  // Navigate the frame.
  ASSERT_TRUE(NavigateToURL(shell(), same_origin_url));

  // Complete the "1.b.test" .well-known request.
  permissions_responses[1]->Send(
      net::HttpStatusCode::HTTP_OK,
      /*content_type=*/"application/json",
      /*content=*/R"({"joinAdInterestGroup" : true})",
      /*cookies=*/{},
      /*extra_headers=*/{"Access-Control-Allow-Origin: *"});
  permissions_responses[1]->Done();

  // Wait for the "1.b.test" group to be joined successfully.
  while (GetJoinCount(cross_origin_server.GetOrigin("1.b.test"),
                      base::NumberToString(kMaxActiveCrossSiteJoins - 1)) !=
         1) {
    continue;
  }

  // Complete the "0.b.test" .well-known request.
  permissions_responses[0]->Send(
      net::HttpStatusCode::HTTP_OK,
      /*content_type=*/"application/json",
      /*content=*/R"({"joinAdInterestGroup" : true})",
      /*cookies=*/{},
      /*extra_headers=*/{"Access-Control-Allow-Origin: *"});
  permissions_responses[0]->Done();
  // Wait for two of the "0.b.test" groups to be joined successfully.
  while (GetJoinCount(cross_origin_server.GetOrigin("0.b.test"),
                      base::NumberToString(0)) != 1) {
    continue;
  }
  while (GetJoinCount(cross_origin_server.GetOrigin("0.b.test"),
                      base::NumberToString(kMaxActiveCrossSiteJoins - 2)) !=
         1) {
    continue;
  }

  // The "2.b.test" cross-origin join should never have made it to the browser
  // process, let alone to the test server.
  EXPECT_FALSE(permissions_responses[2]->has_received_request());
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       JoinInterestGroupInvalidOwner) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));
  AttachInterestGroupObserver();

  EXPECT_EQ(
      "TypeError: Failed to execute 'joinAdInterestGroup' on 'Navigator': "
      "owner 'https://invalid^&' for AuctionAdInterestGroup with name 'cars' "
      "must be a valid https origin.",
      EvalJs(shell(), R"(
(async function() {
  try {
    await navigator.joinAdInterestGroup(
        {
          name: 'cars',
          owner: 'https://invalid^&',
        },
        /*joinDurationSec=*/1);
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())"));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       JoinInterestGroupInvalidPriorityVector) {
  GURL url = https_server_->GetURL("a.test", "/echo");
  std::string origin_string = url::Origin::Create(url).Serialize();
  ASSERT_TRUE(NavigateToURL(shell(), url));
  AttachInterestGroupObserver();

  EXPECT_EQ(
      "TypeError: Failed to execute 'joinAdInterestGroup' on 'Navigator': "
      "Failed to read the 'priorityVector' property from "
      "'AuctionAdInterestGroup': The provided double value is non-finite.",
      EvalJs(shell(), JsReplace(R"(
(async function() {
  try {
    await navigator.joinAdInterestGroup(
        {
          name: 'cars',
          owner: $1,
          priorityVector: {'foo': 'invalid'},
        },
        /*joinDurationSec=*/1);
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())",
                                origin_string.c_str())));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       JoinInterestGroupInvalidPrioritySignalsOverrides) {
  GURL url = https_server_->GetURL("a.test", "/echo");
  std::string origin_string = url::Origin::Create(url).Serialize();
  ASSERT_TRUE(NavigateToURL(shell(), url));
  AttachInterestGroupObserver();

  EXPECT_EQ(
      "TypeError: Failed to execute 'joinAdInterestGroup' on 'Navigator': "
      "Failed to read the 'prioritySignalsOverrides' property from "
      "'AuctionAdInterestGroup': Only objects can be converted to record<K,V> "
      "types",
      EvalJs(shell(), JsReplace(R"(
(async function() {
  try {
    await navigator.joinAdInterestGroup(
        {
          name: 'cars',
          owner: $1,
          prioritySignalsOverrides: "Not an object",
        },
        /*joinDurationSec=*/1);
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())",
                                origin_string.c_str())));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       JoinInterestGroupSupportsDeprecatedNames) {
  GURL url = https_server_->GetURL("a.test", "/echo");
  std::string origin_string = url::Origin::Create(url).Serialize();
  ASSERT_TRUE(NavigateToURL(shell(), url));

  bool logged_interest_group_counts = false, logged_latency_stats = false,
       logged_group_by_origin = false;
  WebContentsConsoleObserver console_observer(shell()->web_contents());
  console_observer.SetFilter(base::BindLambdaForTesting(
      [&logged_interest_group_counts, &logged_latency_stats,
       &logged_group_by_origin](
          const WebContentsConsoleObserver::Message& message) {
        const std::u16string& text = message.message;

        if (text ==
            u"Enum SellerCapabilities used deprecated value "
            u"interestGroupCounts -- \"dashed-naming\" should be used instead "
            u"of \"camelCase\".") {
          logged_interest_group_counts = true;
        } else if (text ==
                   u"Enum SellerCapabilities used deprecated value "
                   u"latencyStats -- \"dashed-naming\" should be used instead "
                   u"of \"camelCase\".") {
          logged_latency_stats = true;
        } else if (text ==
                   u"Enum executionMode used deprecated value groupByOrigin "
                   u"-- \"dashed-naming\" should be used instead of "
                   u"\"camelCase\".") {
          logged_group_by_origin = true;
        }

        return logged_interest_group_counts && logged_latency_stats &&
               logged_group_by_origin;
      }));

  EXPECT_EQ("done", EvalJs(shell(), JsReplace(R"(
(async function() {
  try {
    await navigator.joinAdInterestGroup(
        {
          name: 'cars',
          owner: $1,
          sellerCapabilities: {'*': ['interestGroupCounts', 'latencyStats']},
          executionMode: 'groupByOrigin',
        },
        /*joinDurationSec=*/1000);
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())",
                                              origin_string.c_str())));
  EXPECT_TRUE(console_observer.Wait());

  WaitForInterestGroupsSatisfying(
      url::Origin::Create(url),
      base::BindLambdaForTesting([](const std::vector<StorageInterestGroup>&
                                        groups) {
        if (groups.size() != 1) {
          return false;
        }
        const auto& group = groups[0].interest_group;
        return group.all_sellers_capabilities ==
                   blink::SellerCapabilitiesType(
                       blink::SellerCapabilities::kInterestGroupCounts,
                       blink::SellerCapabilities::kLatencyStats) &&
               group.execution_mode ==
                   blink::InterestGroup::ExecutionMode::kGroupedByOriginMode;
      }));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       JoinInterestGroupInvalidEnumFieldsIgnored) {
  GURL url = https_server_->GetURL("a.test", "/echo");
  std::string origin_string = url::Origin::Create(url).Serialize();
  ASSERT_TRUE(NavigateToURL(shell(), url));

  EXPECT_EQ("done", EvalJs(shell(), JsReplace(R"(
(async function() {
  try {
    await navigator.joinAdInterestGroup(
        {
          name: 'cars',
          owner: $1,
          sellerCapabilities: {'https://example.test': ['non-valid-capability']},
          executionMode: 'non-valid-execution-mode',
        },
        /*joinDurationSec=*/1);
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())",
                                              origin_string.c_str())));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       JoinInterestGroupValidSellerCapabilities) {
  GURL url = https_server_->GetURL("a.test", "/echo");
  auto origin = url::Origin::Create(url);
  std::string origin_string = origin.Serialize();
  ASSERT_TRUE(NavigateToURL(shell(), url));

  EXPECT_EQ(kSuccess,
            JoinInterestGroupAndVerify(
                blink::TestInterestGroupBuilder(
                    /*owner=*/origin,
                    /*name=*/"cars")
                    .SetSellerCapabilities(
                        {{{url::Origin::Create(GURL("https://example.test")),
                           blink::SellerCapabilities::kInterestGroupCounts}}})
                    .SetAllSellerCapabilities(
                        blink::SellerCapabilities::kLatencyStats)
                    .Build()));

  std::vector<StorageInterestGroup> groups = GetInterestGroupsForOwner(origin);
  ASSERT_EQ(groups.size(), 1u);
  const blink::InterestGroup& group = groups[0].interest_group;
  EXPECT_EQ(group.all_sellers_capabilities,
            blink::SellerCapabilities::kLatencyStats);
  ASSERT_TRUE(group.seller_capabilities);
  ASSERT_EQ(group.seller_capabilities->size(), 1u);
  EXPECT_EQ(group.seller_capabilities->at(
                url::Origin::Create(GURL("https://example.test"))),
            blink::SellerCapabilities::kInterestGroupCounts);
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       JoinInterestGroupValidSizeFields) {
  GURL url = https_server_->GetURL("a.test", "/echo");
  auto origin = url::Origin::Create(url);
  std::string origin_string = origin.Serialize();
  ASSERT_TRUE(NavigateToURL(shell(), url));

  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          blink::TestInterestGroupBuilder(origin, "cars")
              .SetAds(
                  {{{GURL("https://example.com/render"),
                     /*metadata=*/absl::nullopt, /*size_group=*/"group_1"}}})
              .SetAdComponents(
                  {{{GURL("https://example.com/component"),
                     /*metadata=*/absl::nullopt, /*size_group=*/"group_1"}}})
              .SetAdSizes(
                  {{{"size_1",
                     blink::AdSize(150, blink::AdSize::LengthUnit::kPixels, 75,
                                   blink::AdSize::LengthUnit::kPixels)}}})
              .SetSizeGroups({{{"group_1", {"size_1"}}}})
              .Build()));

  std::vector<StorageInterestGroup> groups = GetInterestGroupsForOwner(origin);
  ASSERT_EQ(groups.size(), 1u);
  const blink::InterestGroup& group = groups[0].interest_group;
  ASSERT_TRUE(group.ads.has_value());
  ASSERT_EQ(group.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url,
            GURL("https://example.com/render"));
  ASSERT_TRUE(group.ads.value()[0].size_group.has_value());
  EXPECT_EQ(group.ads.value()[0].size_group, "group_1");
  ASSERT_EQ(group.ad_components->size(), 1u);
  EXPECT_EQ(group.ad_components.value()[0].render_url,
            GURL("https://example.com/component"));
  ASSERT_TRUE(group.ad_components.value()[0].size_group.has_value());
  EXPECT_EQ(group.ad_components.value()[0].size_group, "group_1");
  EXPECT_EQ(group.ad_sizes->size(), 1u);
  ASSERT_EQ(group.ad_sizes->at("size_1"),
            blink::AdSize(150, blink::AdSize::LengthUnit::kPixels, 75,
                          blink::AdSize::LengthUnit::kPixels));
  EXPECT_EQ(group.size_groups->size(), 1u);
  ASSERT_EQ(group.size_groups->at("group_1").size(), 1u);
  ASSERT_EQ(group.size_groups->at("group_1").at(0), "size_1");
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       JoinInterestGroupInvalidBiddingLogicUrl) {
  GURL url = https_server_->GetURL("a.test", "/echo");
  std::string origin_string = url::Origin::Create(url).Serialize();
  ASSERT_TRUE(NavigateToURL(shell(), url));
  AttachInterestGroupObserver();

  EXPECT_EQ(
      base::StringPrintf(
          "TypeError: Failed to execute 'joinAdInterestGroup' on 'Navigator': "
          "biddingLogicUrl 'https://invalid^&' for AuctionAdInterestGroup with "
          "owner '%s' and name 'cars' cannot be resolved to a valid URL.",
          origin_string.c_str()),
      EvalJs(shell(), JsReplace(R"(
(async function() {
  try {
    await navigator.joinAdInterestGroup(
        {
          name: 'cars',
          owner: $1,
          biddingLogicUrl: 'https://invalid^&',
        },
        /*joinDurationSec=*/1);
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())",
                                origin_string.c_str())));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       JoinInterestGroupInvalidBiddingWasmHelperUrl) {
  const char kScriptTemplate[] = R"(
(async function() {
  try {
    await navigator.joinAdInterestGroup(
        {
          name: 'cars',
          owner: $1,
          biddingWasmHelperUrl: 'https://invalid^&',
        },
        /*joinDurationSec=*/1);
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())";

  GURL url = https_server_->GetURL("a.test", "/echo");
  std::string origin_string = url::Origin::Create(url).Serialize();
  ASSERT_TRUE(NavigateToURL(shell(), url));

  EXPECT_EQ(
      base::StringPrintf(
          "TypeError: Failed to execute 'joinAdInterestGroup' on 'Navigator': "
          "biddingWasmHelperUrl 'https://invalid^&' for AuctionAdInterestGroup "
          "with owner '%s' and name 'cars' cannot be resolved to a valid URL.",
          origin_string.c_str()),
      EvalJs(shell(), JsReplace(kScriptTemplate, origin_string.c_str())));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       JoinInterestGroupInvalidUpdateUrl) {
  GURL url = https_server_->GetURL("a.test", "/echo");
  std::string origin_string = url::Origin::Create(url).Serialize();
  ASSERT_TRUE(NavigateToURL(shell(), url));
  AttachInterestGroupObserver();

  EXPECT_EQ(
      base::StringPrintf(
          "TypeError: Failed to execute 'joinAdInterestGroup' on 'Navigator': "
          "updateUrl 'https://invalid^&' for AuctionAdInterestGroup with "
          "owner '%s' and name 'cars' cannot be resolved to a valid URL.",
          origin_string.c_str()),
      EvalJs(shell(), JsReplace(R"(
(async function() {
  try {
    await navigator.joinAdInterestGroup(
        {
          name: 'cars',
          owner: $1,
          updateUrl: 'https://invalid^&',
        },
        /*joinDurationSec=*/1);
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())",
                                origin_string.c_str())));
  WaitForAccessObserved({});
}

// TODO(https://crbug.com/1420080): Remove one support for `dailyUpdateUrl` has
// been removed.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       JoinInterestGroupInvalidDailyUpdateUrl) {
  GURL url = https_server_->GetURL("a.test", "/echo");
  std::string origin_string = url::Origin::Create(url).Serialize();
  ASSERT_TRUE(NavigateToURL(shell(), url));
  AttachInterestGroupObserver();

  EXPECT_EQ(
      base::StringPrintf(
          "TypeError: Failed to execute 'joinAdInterestGroup' on 'Navigator': "
          "dailyUpdateUrl 'https://invalid^&' for AuctionAdInterestGroup with "
          "owner '%s' and name 'cars' cannot be resolved to a valid URL.",
          origin_string.c_str()),
      EvalJs(shell(), JsReplace(R"(
(async function() {
  try {
    await navigator.joinAdInterestGroup(
        {
          name: 'cars',
          owner: $1,
          dailyUpdateUrl: 'https://invalid^&',
        },
        /*joinDurationSec=*/1);
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())",
                                origin_string.c_str())));
  WaitForAccessObserved({});
}

// TODO(https://crbug.com/1420080): Remove one support for `dailyUpdateUrl` has
// been removed.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       JoinInterestGroupDifferentUpdateUrlAndDailyUpdateUrl) {
  GURL url = https_server_->GetURL("a.test", "/echo");
  std::string origin_string = url::Origin::Create(url).Serialize();
  ASSERT_TRUE(NavigateToURL(shell(), url));
  AttachInterestGroupObserver();

  EXPECT_EQ(
      base::StringPrintf(
          "TypeError: Failed to execute 'joinAdInterestGroup' on 'Navigator': "
          "updateUrl '%s' for AuctionAdInterestGroup with owner '%s' and name "
          "'cars' must match dailyUpdateUrl, when both are present.",
          (origin_string + "/foo").c_str(), origin_string.c_str()),
      EvalJs(shell(), JsReplace(R"(
(async function() {
  try {
    await navigator.joinAdInterestGroup(
        {
          name: 'cars',
          owner: $1,
          updateUrl: $1 + '/foo',
          dailyUpdateUrl:  $1 + '/bar',
        },
        /*joinDurationSec=*/1);
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())",
                                origin_string.c_str())));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       JoinInterestGroupInvalidTrustedBiddingSignalsUrl) {
  GURL url = https_server_->GetURL("a.test", "/echo");
  std::string origin_string = url::Origin::Create(url).Serialize();
  ASSERT_TRUE(NavigateToURL(shell(), url));
  AttachInterestGroupObserver();

  EXPECT_EQ(base::StringPrintf(
                "TypeError: Failed to execute 'joinAdInterestGroup' on "
                "'Navigator': trustedBiddingSignalsUrl 'https://invalid^&' for "
                "AuctionAdInterestGroup with owner '%s' and name 'cars' cannot "
                "be resolved to a valid URL.",
                origin_string.c_str()),
            EvalJs(shell(), JsReplace(R"(
(async function() {
  try {
    await navigator.joinAdInterestGroup(
        {
          name: 'cars',
          owner: $1,
          trustedBiddingSignalsUrl: 'https://invalid^&',
        },
        /*joinDurationSec=*/1);
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())",
                                      origin_string.c_str())));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       JoinInterestGroupInvalidUserBiddingSignals) {
  GURL url = https_server_->GetURL("a.test", "/echo");
  std::string origin_string = url::Origin::Create(url).Serialize();
  ASSERT_TRUE(NavigateToURL(shell(), url));
  AttachInterestGroupObserver();

  EXPECT_EQ(
      base::StringPrintf(
          "TypeError: Failed to execute 'joinAdInterestGroup' on 'Navigator': "
          "userBiddingSignals for AuctionAdInterestGroup with owner '%s' and "
          "name 'cars' must be a JSON-serializable object.",
          origin_string.c_str()),
      EvalJs(shell(), JsReplace(R"(
(async function() {
  try {
    await navigator.joinAdInterestGroup(
        {
          name: 'cars',
          owner: $1,
          userBiddingSignals: function() {},
        },
        /*joinDurationSec=*/1);
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())",
                                origin_string.c_str())));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       JoinInterestGroupInvalidAdUrl) {
  GURL url = https_server_->GetURL("a.test", "/echo");
  std::string origin_string = url::Origin::Create(url).Serialize();
  ASSERT_TRUE(NavigateToURL(shell(), url));
  AttachInterestGroupObserver();

  EXPECT_EQ(
      base::StringPrintf(
          "TypeError: Failed to execute 'joinAdInterestGroup' on 'Navigator': "
          "ad renderUrl 'https://invalid^&' for AuctionAdInterestGroup with "
          "owner '%s' and name 'cars' cannot be resolved to a valid URL.",
          origin_string.c_str()),
      EvalJs(shell(), JsReplace(R"(
(async function() {
  try {
    await navigator.joinAdInterestGroup(
        {
          name: 'cars',
          owner: $1,
          ads: [{renderUrl:"https://invalid^&"}],
        },
        /*joinDurationSec=*/1);
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())",
                                origin_string.c_str())));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       JoinInterestGroupInvalidAdMetadata) {
  GURL url = https_server_->GetURL("a.test", "/echo");
  std::string origin_string = url::Origin::Create(url).Serialize();
  ASSERT_TRUE(NavigateToURL(shell(), url));
  AttachInterestGroupObserver();

  EXPECT_EQ(
      base::StringPrintf(
          "TypeError: Failed to execute 'joinAdInterestGroup' on "
          "'Navigator': ad metadata for AuctionAdInterestGroup with "
          "owner '%s' and name 'cars' must be a JSON-serializable object.",
          origin_string.c_str()),
      EvalJs(shell(), JsReplace(R"(
(async function() {
  let x = {};
  let y = {};
  x.a = y;
  y.a = x;
  try {
    await navigator.joinAdInterestGroup(
        {
          name: 'cars',
          owner: $1,
          ads: [{renderUrl:"https://test.com", metadata:x}],
        },
        /*joinDurationSec=*/1);
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())",
                                origin_string.c_str())));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       LeaveInterestGroupInvalidOwner) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));
  AttachInterestGroupObserver();

  EXPECT_EQ(
      "TypeError: Failed to execute 'leaveAdInterestGroup' on 'Navigator': "
      "owner 'https://invalid^&' for AuctionAdInterestGroup with name 'cars' "
      "must be a valid https origin.",
      EvalJs(shell(), R"(
(async function() {
  try {
    await navigator.leaveAdInterestGroup(
        {
          name: 'cars',
          owner: 'https://invalid^&',
        },
        /*joinDurationSec=*/1);
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())"));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       JoinInterestGroupInvalidAdSizeGroupEmptyName) {
  GURL url = https_server_->GetURL("a.test", "/echo");
  std::string origin_string = url::Origin::Create(url).Serialize();
  ASSERT_TRUE(NavigateToURL(shell(), url));
  AttachInterestGroupObserver();

  EXPECT_EQ(
      base::StringPrintf(
          "TypeError: Failed to execute 'joinAdInterestGroup' on 'Navigator': "
          "ads[0].sizeGroup '' for AuctionAdInterestGroup with owner '%s' and "
          "name 'cars' Size group name cannot be empty.",
          origin_string.c_str()),
      EvalJs(shell(), JsReplace(R"(
(async function() {
  try {
    await navigator.joinAdInterestGroup(
        {
          name: 'cars',
          owner: $1,
          ads: [{renderUrl: "https://test.com", sizeGroup: ""}],
        },
        /*joinDurationSec=*/1);
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())",
                                origin_string.c_str())));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       JoinInterestGroupInvalidAdSizeGroupNoSizeGroups) {
  GURL url = https_server_->GetURL("a.test", "/echo");
  std::string origin_string = url::Origin::Create(url).Serialize();
  ASSERT_TRUE(NavigateToURL(shell(), url));
  AttachInterestGroupObserver();

  EXPECT_EQ(
      base::StringPrintf(
          "TypeError: Failed to execute 'joinAdInterestGroup' on 'Navigator': "
          "ads[0].sizeGroup 'nonexistent' for AuctionAdInterestGroup with "
          "owner '%s' and name 'cars' The assigned size group does not exist "
          "in sizeGroups map.",
          origin_string.c_str()),
      EvalJs(shell(), JsReplace(R"(
(async function() {
  try {
    await navigator.joinAdInterestGroup(
        {
          name: 'cars',
          owner: $1,
          ads: [{renderUrl: "https://test.com", sizeGroup: "nonexistent"}],
        },
        /*joinDurationSec=*/1);
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())",
                                origin_string.c_str())));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(
    InterestGroupBrowserTest,
    JoinInterestGroupInvalidAdSizeGroupNotContainedInSizeGroups) {
  GURL url = https_server_->GetURL("a.test", "/echo");
  std::string origin_string = url::Origin::Create(url).Serialize();
  ASSERT_TRUE(NavigateToURL(shell(), url));
  AttachInterestGroupObserver();

  EXPECT_EQ(
      base::StringPrintf(
          "TypeError: Failed to execute 'joinAdInterestGroup' on 'Navigator': "
          "ads[0].sizeGroup 'nonexistent' for AuctionAdInterestGroup with "
          "owner '%s' and name 'cars' The assigned size group does not exist "
          "in sizeGroups map.",
          origin_string.c_str()),
      EvalJs(shell(), JsReplace(R"(
(async function() {
  try {
    await navigator.joinAdInterestGroup(
        {
          name: 'cars',
          owner: $1,
          ads: [{renderUrl: "https://test.com", sizeGroup: "nonexistent"}],
          adSizes: {"size_1": {"width": "50px", "height": "50px"}},
          sizeGroups: {"group_1": ["size_1"]},
        },
        /*joinDurationSec=*/1);
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())",
                                origin_string.c_str())));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       JoinInterestGroupInvalidAdSizeGroupNoAdSize) {
  GURL url = https_server_->GetURL("a.test", "/echo");
  std::string origin_string = url::Origin::Create(url).Serialize();
  ASSERT_TRUE(NavigateToURL(shell(), url));
  AttachInterestGroupObserver();

  EXPECT_EQ(
      base::StringPrintf(
          "TypeError: Failed to execute 'joinAdInterestGroup' on 'Navigator': "
          "sizeGroups '' for AuctionAdInterestGroup with owner '%s' and name "
          "'cars' An adSizes map must exist for sizeGroups to work.",
          origin_string.c_str()),
      EvalJs(shell(), JsReplace(R"(
(async function() {
  try {
    await navigator.joinAdInterestGroup(
        {
          name: 'cars',
          owner: $1,
          ads: [{renderUrl: "https://test.com", sizeGroup: "group_1"}],
          sizeGroups: {"group_1": ["nonexistent"]},
        },
        /*joinDurationSec=*/1);
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())",
                                origin_string.c_str())));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       JoinInterestGroupInvalidAdComponentSizeGroupEmptyName) {
  GURL url = https_server_->GetURL("a.test", "/echo");
  std::string origin_string = url::Origin::Create(url).Serialize();
  ASSERT_TRUE(NavigateToURL(shell(), url));
  AttachInterestGroupObserver();

  EXPECT_EQ(
      base::StringPrintf(
          "TypeError: Failed to execute 'joinAdInterestGroup' on 'Navigator': "
          "adComponents[0].sizeGroup '' for AuctionAdInterestGroup with owner "
          "'%s' and name 'cars' Size group name cannot be empty.",
          origin_string.c_str()),
      EvalJs(shell(), JsReplace(R"(
(async function() {
  try {
    await navigator.joinAdInterestGroup(
        {
          name: 'cars',
          owner: $1,
          adComponents: [{renderUrl: "https://test.com", sizeGroup: ""}],
        },
        /*joinDurationSec=*/1);
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())",
                                origin_string.c_str())));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(
    InterestGroupBrowserTest,
    JoinInterestGroupInvalidAdComponentSizeGroupNoSizeGroups) {
  GURL url = https_server_->GetURL("a.test", "/echo");
  std::string origin_string = url::Origin::Create(url).Serialize();
  ASSERT_TRUE(NavigateToURL(shell(), url));
  AttachInterestGroupObserver();

  EXPECT_EQ(
      base::StringPrintf(
          "TypeError: Failed to execute 'joinAdInterestGroup' on 'Navigator': "
          "adComponents[0].sizeGroup 'nonexistent' for AuctionAdInterestGroup "
          "with owner '%s' and name 'cars' The assigned size group does not "
          "exist in sizeGroups map.",
          origin_string.c_str()),
      EvalJs(shell(), JsReplace(R"(
(async function() {
  try {
    await navigator.joinAdInterestGroup(
        {
          name: 'cars',
          owner: $1,
          adComponents: [{renderUrl: "https://test.com", sizeGroup: "nonexistent"}],
        },
        /*joinDurationSec=*/1);
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())",
                                origin_string.c_str())));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(
    InterestGroupBrowserTest,
    JoinInterestGroupInvalidAdComponentSizeGroupNotContainedInSizeGroups) {
  GURL url = https_server_->GetURL("a.test", "/echo");
  std::string origin_string = url::Origin::Create(url).Serialize();
  ASSERT_TRUE(NavigateToURL(shell(), url));
  AttachInterestGroupObserver();

  EXPECT_EQ(
      base::StringPrintf(
          "TypeError: Failed to execute 'joinAdInterestGroup' on 'Navigator': "
          "adComponents[0].sizeGroup 'nonexistent' for AuctionAdInterestGroup "
          "with owner '%s' and name 'cars' The assigned size group does not "
          "exist in sizeGroups map.",
          origin_string.c_str()),
      EvalJs(shell(), JsReplace(R"(
(async function() {
  try {
    await navigator.joinAdInterestGroup(
        {
          name: 'cars',
          owner: $1,
          adComponents: [{renderUrl: "https://test.com", sizeGroup: "nonexistent"}],
          adSizes: {"size_1": {"width": "50px", "height": "50px"}},
          sizeGroups: {"group_1": ["size_1"]},
        },
        /*joinDurationSec=*/1);
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())",
                                origin_string.c_str())));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       JoinInterestGroupInvalidAdComponentSizeGroupNoAdSizes) {
  GURL url = https_server_->GetURL("a.test", "/echo");
  std::string origin_string = url::Origin::Create(url).Serialize();
  ASSERT_TRUE(NavigateToURL(shell(), url));
  AttachInterestGroupObserver();

  EXPECT_EQ(
      base::StringPrintf(
          "TypeError: Failed to execute 'joinAdInterestGroup' on 'Navigator': "
          "sizeGroups '' for AuctionAdInterestGroup with owner '%s' and name "
          "'cars' An adSizes map must exist for sizeGroups to work.",
          origin_string.c_str()),
      EvalJs(shell(), JsReplace(R"(
(async function() {
  try {
    await navigator.joinAdInterestGroup(
        {
          name: 'cars',
          owner: $1,
          adComponents: [{renderUrl: "https://test.com", sizeGroup: "group_1"}],
          sizeGroups: {"group_1": ["nonexistent"]},
        },
        /*joinDurationSec=*/1);
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())",
                                origin_string.c_str())));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       JoinInterestGroupInvalidAdSize) {
  GURL url = https_server_->GetURL("a.test", "/echo");
  std::string origin_string = url::Origin::Create(url).Serialize();
  ASSERT_TRUE(NavigateToURL(shell(), url));
  AttachInterestGroupObserver();

  EXPECT_EQ(
      base::StringPrintf(
          "TypeError: Failed to execute 'joinAdInterestGroup' on 'Navigator': "
          "adSizes '0.000000 x 50.000000' for AuctionAdInterestGroup with "
          "owner '%s' and name 'cars' Ad sizes must have a valid "
          "(non-zero/non-infinite) width and height.",
          origin_string.c_str()),
      EvalJs(shell(), JsReplace(R"(
(async function() {
  try {
    await navigator.joinAdInterestGroup(
        {
          name: 'cars',
          owner: $1,
          adSizes: {"my_size": {"width": "0px", "height": "50px"}},
        },
        /*joinDurationSec=*/1);
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())",
                                origin_string.c_str())));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       JoinInterestGroupInvalidAdSizeUnits) {
  GURL url = https_server_->GetURL("a.test", "/echo");
  std::string origin_string = url::Origin::Create(url).Serialize();
  ASSERT_TRUE(NavigateToURL(shell(), url));
  AttachInterestGroupObserver();

  EXPECT_EQ(
      base::StringPrintf(
          "TypeError: Failed to execute 'joinAdInterestGroup' on 'Navigator': "
          "adSizes '' for AuctionAdInterestGroup with owner '%s' and name "
          "'cars' Ad size dimensions must be a valid number either in pixels "
          "(px) or screen width (sw).",
          origin_string.c_str()),
      EvalJs(shell(), JsReplace(R"(
(async function() {
  try {
    await navigator.joinAdInterestGroup(
        {
          name: 'cars',
          owner: $1,
          adSizes: {"my_size": {"width": "500px", "height": "400bad"}},
        },
        /*joinDurationSec=*/1);
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())",
                                origin_string.c_str())));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       JoinInterestGroupInvalidAdSizeNoNumber) {
  GURL url = https_server_->GetURL("a.test", "/echo");
  std::string origin_string = url::Origin::Create(url).Serialize();
  ASSERT_TRUE(NavigateToURL(shell(), url));
  AttachInterestGroupObserver();

  EXPECT_EQ(
      base::StringPrintf(
          "TypeError: Failed to execute 'joinAdInterestGroup' on 'Navigator': "
          "adSizes '' for AuctionAdInterestGroup with "
          "owner '%s' and name 'cars' Ad size dimensions must be a valid "
          "number either in pixels (px) or screen width (sw).",
          origin_string.c_str()),
      EvalJs(shell(), JsReplace(R"(
(async function() {
  try {
    await navigator.joinAdInterestGroup(
        {
          name: 'cars',
          owner: $1,
          adSizes: {"my_size": {"width": "500px", "height": "px"}},
        },
        /*joinDurationSec=*/1);
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())",
                                origin_string.c_str())));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       JoinInterestGroupInvalidSizeGroup) {
  GURL url = https_server_->GetURL("a.test", "/echo");
  std::string origin_string = url::Origin::Create(url).Serialize();
  ASSERT_TRUE(NavigateToURL(shell(), url));
  AttachInterestGroupObserver();

  EXPECT_EQ(
      base::StringPrintf(
          "TypeError: Failed to execute 'joinAdInterestGroup' on 'Navigator': "
          "sizeGroups '' for AuctionAdInterestGroup with owner '%s' and name "
          "'cars' Size groups cannot map from an empty group name.",
          origin_string.c_str()),
      EvalJs(shell(), JsReplace(R"(
(async function() {
  try {
    await navigator.joinAdInterestGroup(
        {
          name: 'cars',
          owner: $1,
          adSizes: {"size_1": {"width": "300 px", "height": "150 px"}},
          sizeGroups: {"": ["size_1"]},
        },
        /*joinDurationSec=*/1);
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())",
                                origin_string.c_str())));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       JoinInterestGroupInvalidSizeGroupSize) {
  GURL url = https_server_->GetURL("a.test", "/echo");
  std::string origin_string = url::Origin::Create(url).Serialize();
  ASSERT_TRUE(NavigateToURL(shell(), url));
  AttachInterestGroupObserver();

  EXPECT_EQ(
      base::StringPrintf(
          "TypeError: Failed to execute 'joinAdInterestGroup' on 'Navigator': "
          "sizeGroups 'nonexistant' for AuctionAdInterestGroup with owner '%s' "
          "and name 'cars' Size does not exist in adSizes map.",
          origin_string.c_str()),
      EvalJs(shell(), JsReplace(R"(
(async function() {
  try {
    await navigator.joinAdInterestGroup(
        {
          name: 'cars',
          owner: $1,
          adSizes: {"size_1": {"width": "300px", "height": "150px"}},
          sizeGroups: {"my_group": ["nonexistant"]},
        },
        /*joinDurationSec=*/1);
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())",
                                origin_string.c_str())));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, RunAdAuctionInvalidSeller) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));
  AttachInterestGroupObserver();

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': seller "
      "'https://invalid^&' for AuctionAdConfig must be a valid https origin.",
      RunAuctionAndWait(R"({
      seller: 'https://invalid^&',
      decisionLogicUrl: 'https://test.com/decision_logic'
  })"));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, RunAdAuctionHttpSeller) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));
  AttachInterestGroupObserver();

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': seller "
      "'http://test.com' for AuctionAdConfig must be a valid https origin.",
      RunAuctionAndWait(R"({
      seller: 'http://test.com',
      decisionLogicUrl: 'https://test.com/decision_logic'
  })"));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionInvalidDecisionLogicUrl) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));
  AttachInterestGroupObserver();

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': "
      "decisionLogicUrl 'https://invalid^&' for AuctionAdConfig with seller "
      "'https://test.com' cannot be resolved to a valid URL.",
      RunAuctionAndWait(R"({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://invalid^&'
  })"));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionInvalidTrustedScoringSignalsUrl) {
  GURL url = https_server_->GetURL("a.test", "/echo");
  url::Origin origin = url::Origin::Create(url);
  ASSERT_TRUE(NavigateToURL(shell(), url));
  AttachInterestGroupObserver();

  EXPECT_EQ(
      base::StringPrintf(
          "TypeError: Failed to execute 'runAdAuction' on 'Navigator': "
          "trustedScoringSignalsUrl 'https://invalid^&' for AuctionAdConfig "
          "with seller '%s' cannot be resolved to a valid URL.",
          origin.Serialize().c_str()),
      RunAuctionAndWait(JsReplace(R"({
      seller: $1,
      decisionLogicUrl: $2,
      trustedScoringSignalsUrl: 'https://invalid^&'
  })",
                                  origin, url)));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionDecisionLogicUrlDifferentFromSeller) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);
  AttachInterestGroupObserver();

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': "
      "decisionLogicUrl 'https://b.test/foo' for AuctionAdConfig with seller "
      "'https://a.test/' must match seller origin.",
      RunAuctionAndWait(R"({
    seller: "https://a.test/",
    decisionLogicUrl: "https://b.test/foo",
    interestGroupBuyers: ["https://c.test/"],
                        })"));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(
    InterestGroupBrowserTest,
    RunAdAuctionTrustedScoringSignalsUrlDifferentFromSeller) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);
  AttachInterestGroupObserver();

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': "
      "trustedScoringSignalsUrl 'https://b.test/foo' for AuctionAdConfig with "
      "seller 'https://a.test/' must match seller origin.",
      RunAuctionAndWait(R"({
    seller: "https://a.test/",
    decisionLogicUrl: "https://a.test/foo",
    trustedScoringSignalsUrl: "https://b.test/foo",
    interestGroupBuyers: ["https://c.test/"],
                        })"));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionInvalidInterestGroupBuyers) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));
  AttachInterestGroupObserver();

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': "
      "interestGroupBuyers buyer 'https://invalid^&' for AuctionAdConfig "
      "with seller 'https://test.com' must be a valid https origin.",
      RunAuctionAndWait(R"({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://test.com',
      interestGroupBuyers: ['https://invalid^&'],
  })"));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionInvalidInterestGroupBuyersStr) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));
  AttachInterestGroupObserver();

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': Failed to "
      "read the 'interestGroupBuyers' property from 'AuctionAdConfig': The "
      "provided value cannot be converted to a sequence.",
      RunAuctionAndWait(R"({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://test.com',
      interestGroupBuyers: 'not an array',
  })"));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionNoInterestGroupBuyers) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));
  AttachInterestGroupObserver();

  EXPECT_EQ(nullptr, RunAuctionAndWait(R"({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://test.com',
  })"));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionEmptyInterestGroupBuyers) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));
  AttachInterestGroupObserver();

  EXPECT_EQ(nullptr, RunAuctionAndWait(R"({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://test.com',
      interestGroupBuyers: [],
  })"));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionInvalidAuctionSignals) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));
  AttachInterestGroupObserver();

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': "
      "auctionSignals for AuctionAdConfig with seller 'https://test.com' must "
      "be a JSON-serializable object.",
      RunAuctionAndWait(R"({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://test.com',
      auctionSignals: alert
  })"));
  WaitForAccessObserved({});
}

// Exercise rejection path in the renderer for promise-delivered auction
// signals.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionRejectPromiseAuctionSignals) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  url::Origin test_origin = url::Origin::Create(test_url);
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  GURL ad_url = https_server_->GetURL("c.test", "/echo?render_cars");
  GURL decision_url =
      https_server_->GetURL("a.test", "/interest_group/decision_logic.js");
  // Note: at present at least one bid must be made for promise checking to
  // be guaranteed to happen; if the auction is (effectively) empty whether
  // it happens or not is timing-dependent.
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          /*owner=*/test_origin,
          /*name=*/"cars",
          /*priority=*/0.0,
          /*execution_mode=*/
          blink::InterestGroup::ExecutionMode::kCompatibilityMode,
          /*bidding_url=*/
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /*ads=*/{{{ad_url, /*metadata=*/absl::nullopt}}}));

  const char kAuctionConfigTemplate[] = R"({
      seller: $1,
      decisionLogicUrl: $2,
      auctionSignals: new Promise((resolve, reject) => { setTimeout(
          () => { reject('boo'); }, 10) }),
      interestGroupBuyers: [$1]
  })";

  EXPECT_EQ("Promise argument rejected or resolved to invalid value.",
            RunAuctionAndWait(
                JsReplace(kAuctionConfigTemplate, test_origin, decision_url)));
}

// Exercise error-handling path in the renderer for promise-delivered auction
// signals.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionResolvePromiseInvalidAuctionSignals) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  url::Origin test_origin = url::Origin::Create(test_url);
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  GURL ad_url = https_server_->GetURL("c.test", "/echo?render_cars");
  GURL decision_url =
      https_server_->GetURL("a.test", "/interest_group/decision_logic.js");
  // Note: at present at least one bid must be made for promise checking to
  // be guaranteed to happen; if the auction is (effectively) empty whether
  // it happens or not is timing-dependent.
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          /*owner=*/test_origin,
          /*name=*/"cars",
          /*priority=*/0.0,
          /*execution_mode=*/
          blink::InterestGroup::ExecutionMode::kCompatibilityMode,
          /*bidding_url=*/
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /*ads=*/{{{ad_url, /*metadata=*/absl::nullopt}}}));

  const char kAuctionConfigTemplate[] = R"({
      seller: $1,
      decisionLogicUrl: $2,
      auctionSignals: new Promise((resolve, reject) => { setTimeout(
          () => { resolve(function() {}); }, 10) }),
      interestGroupBuyers: [$1]
  })";
  WebContentsConsoleObserver console_observer(shell()->web_contents());
  console_observer.SetPattern(
      "Uncaught (in promise) TypeError: Failed to execute 'runAdAuction' on "
      "'NavigatorAuction': auctionSignals for AuctionAdConfig with seller "
      "'https://a.test:*' must be a JSON-serializable object.");

  EXPECT_EQ("Promise argument rejected or resolved to invalid value.",
            RunAuctionAndWait(
                JsReplace(kAuctionConfigTemplate, test_origin, decision_url)));
  EXPECT_TRUE(console_observer.Wait());
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionInvalidSellerSignals) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));
  AttachInterestGroupObserver();

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': "
      "sellerSignals for AuctionAdConfig with seller 'https://test.com' must "
      "be a JSON-serializable object.",
      RunAuctionAndWait(R"({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://test.com',
      sellerSignals: function() {}
  })"));
  WaitForAccessObserved({});
}

// Exercise rejection path in the renderer for promise-delivered seller signals.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionRejectPromiseSellerSignals) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  url::Origin test_origin = url::Origin::Create(test_url);
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  GURL ad_url = https_server_->GetURL("c.test", "/echo?render_cars");
  GURL decision_url =
      https_server_->GetURL("a.test", "/interest_group/decision_logic.js");
  // Note: at present at least one bid must be made for promise checking to
  // be guaranteed to happen; if the auction is (effectively) empty whether
  // it happens or not is timing-dependent.
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          /*owner=*/test_origin,
          /*name=*/"cars",
          /*priority=*/0.0,
          /*execution_mode=*/
          blink::InterestGroup::ExecutionMode::kCompatibilityMode,
          /*bidding_url=*/
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /*ads=*/{{{ad_url, /*metadata=*/absl::nullopt}}}));

  const char kAuctionConfigTemplate[] = R"({
      seller: $1,
      decisionLogicUrl: $2,
      sellerSignals: new Promise((resolve, reject) => { setTimeout(
          () => { reject('boo'); }, 10) }),
      interestGroupBuyers: [$1]
  })";

  EXPECT_EQ("Promise argument rejected or resolved to invalid value.",
            RunAuctionAndWait(
                JsReplace(kAuctionConfigTemplate, test_origin, decision_url)));
}

// Exercise error-handling path in the renderer for promise-delivered seller
// signals.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionResolvePromiseInvalidSellerSignals) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  url::Origin test_origin = url::Origin::Create(test_url);
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  GURL ad_url = https_server_->GetURL("c.test", "/echo?render_cars");
  GURL decision_url =
      https_server_->GetURL("a.test", "/interest_group/decision_logic.js");
  // Note: at present at least one bid must be made for promise checking to
  // be guaranteed to happen; if the auction is (effectively) empty whether
  // it happens or not is timing-dependent.
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          /*owner=*/test_origin,
          /*name=*/"cars",
          /*priority=*/0.0,
          /*execution_mode=*/
          blink::InterestGroup::ExecutionMode::kCompatibilityMode,
          /*bidding_url=*/
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /*ads=*/{{{ad_url, /*metadata=*/absl::nullopt}}}));

  const char kAuctionConfigTemplate[] = R"({
      seller: $1,
      decisionLogicUrl: $2,
      sellerSignals: new Promise((resolve, reject) => { setTimeout(
          () => { resolve(function() {}); }, 10) }),
      interestGroupBuyers: [$1]
  })";

  WebContentsConsoleObserver console_observer(shell()->web_contents());
  console_observer.SetPattern(
      "Uncaught (in promise) TypeError: Failed to execute 'runAdAuction' on "
      "'NavigatorAuction': sellerSignals for AuctionAdConfig with seller "
      "'https://a.test:*' must be a JSON-serializable object.");
  EXPECT_EQ("Promise argument rejected or resolved to invalid value.",
            RunAuctionAndWait(
                JsReplace(kAuctionConfigTemplate, test_origin, decision_url)));
  EXPECT_TRUE(console_observer.Wait());
}

// Test rejection path in the renderer for promise-delivered perBuyerSignals.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionRejectPromisePerBuyerSignals) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  url::Origin test_origin = url::Origin::Create(test_url);
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  GURL ad_url = https_server_->GetURL("c.test", "/echo?render_cars");
  GURL decision_url =
      https_server_->GetURL("a.test", "/interest_group/decision_logic.js");
  // Note: at present at least one bid must be made for promise checking to
  // be guaranteed to happen; if the auction is (effectively) empty whether
  // it happens or not is timing-dependent.
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          /*owner=*/test_origin,
          /*name=*/"cars",
          /*priority=*/0.0,
          /*execution_mode=*/
          blink::InterestGroup::ExecutionMode::kCompatibilityMode,
          /*bidding_url=*/
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /*ads=*/{{{ad_url, /*metadata=*/absl::nullopt}}}));

  const char kAuctionConfigTemplate[] = R"({
      seller: $1,
      decisionLogicUrl: $2,
      perBuyerSignals: new Promise((resolve, reject) => { setTimeout(
          () => { reject('boo'); }, 10) }),
      interestGroupBuyers: [$1]
  })";

  EXPECT_EQ("Promise argument rejected or resolved to invalid value.",
            RunAuctionAndWait(
                JsReplace(kAuctionConfigTemplate, test_origin, decision_url)));
}

// Exercise error-handling path in the renderer for promise-delivered
// perBuyerSignals.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionResolvePromiseInvalidPerBuyerSignals) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  url::Origin test_origin = url::Origin::Create(test_url);
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  GURL ad_url = https_server_->GetURL("c.test", "/echo?render_cars");
  GURL decision_url =
      https_server_->GetURL("a.test", "/interest_group/decision_logic.js");
  // Note: at present at least one bid must be made for promise checking to
  // be guaranteed to happen; if the auction is (effectively) empty whether
  // it happens or not is timing-dependent.
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          /*owner=*/test_origin,
          /*name=*/"cars",
          /*priority=*/0.0,
          /*execution_mode=*/
          blink::InterestGroup::ExecutionMode::kCompatibilityMode,
          /*bidding_url=*/
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /*ads=*/{{{ad_url, /*metadata=*/absl::nullopt}}}));

  const char kAuctionConfigTemplate[] = R"({
      seller: $1,
      decisionLogicUrl: $2,
      perBuyerSignals: new Promise((resolve, reject) => { setTimeout(
          () => { resolve(52); }, 10) }),
      interestGroupBuyers: [$1]
  })";

  WebContentsConsoleObserver console_observer(shell()->web_contents());
  console_observer.SetPattern(
      "Uncaught (in promise) TypeError: Failed to execute 'runAdAuction' on "
      "'NavigatorAuction': Only objects can be converted to record<K,V> types");
  EXPECT_EQ("Promise argument rejected or resolved to invalid value.",
            RunAuctionAndWait(
                JsReplace(kAuctionConfigTemplate, test_origin, decision_url)));
  EXPECT_TRUE(console_observer.Wait());
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionInvalidPerBuyerSignalsOrigin) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));
  AttachInterestGroupObserver();

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': "
      "perBuyerSignals buyer 'https://invalid^&' for AuctionAdConfig with "
      "seller 'https://test.com' must be a valid https origin.",
      RunAuctionAndWait(R"({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://test.com',
      perBuyerSignals: {'https://invalid^&': {a:1}}
  })"));
  WaitForAccessObserved({});
}

// Test rejection path in the renderer for promise-delivered perBuyerTimeouts.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionRejectPromisePerBuyerTimeouts) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  url::Origin test_origin = url::Origin::Create(test_url);
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  GURL ad_url = https_server_->GetURL("c.test", "/echo?render_cars");
  GURL decision_url =
      https_server_->GetURL("a.test", "/interest_group/decision_logic.js");
  // Note: at present at least one bid must be made for promise checking to
  // be guaranteed to happen; if the auction is (effectively) empty whether
  // it happens or not is timing-dependent.
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          /*owner=*/test_origin,
          /*name=*/"cars",
          /*priority=*/0.0,
          /*execution_mode=*/
          blink::InterestGroup::ExecutionMode::kCompatibilityMode,
          /*bidding_url=*/
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /*ads=*/{{{ad_url, /*metadata=*/absl::nullopt}}}));

  const char kAuctionConfigTemplate[] = R"({
      seller: $1,
      decisionLogicUrl: $2,
      perBuyerTimeouts: new Promise((resolve, reject) => { setTimeout(
          () => { reject('boo'); }, 10) }),
      interestGroupBuyers: [$1]
  })";

  EXPECT_EQ("Promise argument rejected or resolved to invalid value.",
            RunAuctionAndWait(
                JsReplace(kAuctionConfigTemplate, test_origin, decision_url)));
}

// Exercise error-handling path in the renderer for promise-delivered
// perBuyerTimeouts.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionResolvePromiseInvalidPerBuyerTimeouts) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  url::Origin test_origin = url::Origin::Create(test_url);
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  GURL ad_url = https_server_->GetURL("c.test", "/echo?render_cars");
  GURL decision_url =
      https_server_->GetURL("a.test", "/interest_group/decision_logic.js");
  // Note: at present at least one bid must be made for promise checking to
  // be guaranteed to happen; if the auction is (effectively) empty whether
  // it happens or not is timing-dependent.
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          /*owner=*/test_origin,
          /*name=*/"cars",
          /*priority=*/0.0,
          /*execution_mode=*/
          blink::InterestGroup::ExecutionMode::kCompatibilityMode,
          /*bidding_url=*/
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /*ads=*/{{{ad_url, /*metadata=*/absl::nullopt}}}));

  const char kAuctionConfigTemplate[] = R"({
      seller: $1,
      decisionLogicUrl: $2,
      perBuyerTimeouts: new Promise((resolve, reject) => { setTimeout(
          () => { resolve({'http://b.com': 52}); }, 10) }),
      interestGroupBuyers: [$1]
  })";

  WebContentsConsoleObserver console_observer(shell()->web_contents());
  console_observer.SetPattern(
      "Uncaught (in promise) TypeError: Failed to execute 'runAdAuction' on "
      "'NavigatorAuction': perBuyerTimeouts buyer 'http://b.com' for "
      "AuctionAdConfig with seller 'https://a.test:*' must be \"*\" (wildcard) "
      "or a valid https origin.");
  EXPECT_EQ("Promise argument rejected or resolved to invalid value.",
            RunAuctionAndWait(
                JsReplace(kAuctionConfigTemplate, test_origin, decision_url)));
  EXPECT_TRUE(console_observer.Wait());
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionInvalidPerBuyerTimeoutsOrigin) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));
  AttachInterestGroupObserver();

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': "
      "perBuyerTimeouts buyer 'https://invalid^&' for AuctionAdConfig with "
      "seller 'https://test.com' must be \"*\" (wildcard) or a valid https "
      "origin.",
      RunAuctionAndWait(R"({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://test.com',
      perBuyerTimeouts: {'https://invalid^&': 100}
  })"));
  WaitForAccessObserved({});
}

// Test rejection path in the renderer for promise-delivered
// perBuyerCumulativeTimeouts.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionRejectPromisePerBuyerCumulativeTimeouts) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  url::Origin test_origin = url::Origin::Create(test_url);
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  GURL ad_url = https_server_->GetURL("c.test", "/echo?render_cars");
  GURL decision_url =
      https_server_->GetURL("a.test", "/interest_group/decision_logic.js");
  // Note: at present at least one bid must be made for promise checking to
  // be guaranteed to happen; if the auction is (effectively) empty whether
  // it happens or not is timing-dependent.
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          /*owner=*/test_origin,
          /*name=*/"cars",
          /*priority=*/0.0,
          /*execution_mode=*/
          blink::InterestGroup::ExecutionMode::kCompatibilityMode,
          /*bidding_url=*/
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /*ads=*/{{{ad_url, /*metadata=*/absl::nullopt}}}));

  const char kAuctionConfigTemplate[] = R"({
      seller: $1,
      decisionLogicUrl: $2,
      perBuyerCumulativeTimeouts: new Promise((resolve, reject) => { setTimeout(
          () => { reject('boo'); }, 10) }),
      interestGroupBuyers: [$1]
  })";

  EXPECT_EQ("Promise argument rejected or resolved to invalid value.",
            RunAuctionAndWait(
                JsReplace(kAuctionConfigTemplate, test_origin, decision_url)));
}

// Exercise error-handling path in the renderer for promise-delivered
// perBuyerCumulativeTimeouts.
IN_PROC_BROWSER_TEST_F(
    InterestGroupBrowserTest,
    RunAdAuctionResolvePromiseInvalidPerBuyerCumulativeTimeouts) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  url::Origin test_origin = url::Origin::Create(test_url);
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  GURL ad_url = https_server_->GetURL("c.test", "/echo?render_cars");
  GURL decision_url =
      https_server_->GetURL("a.test", "/interest_group/decision_logic.js");
  // Note: at present at least one bid must be made for promise checking to
  // be guaranteed to happen; if the auction is (effectively) empty whether
  // it happens or not is timing-dependent.
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          /*owner=*/test_origin,
          /*name=*/"cars",
          /*priority=*/0.0,
          /*execution_mode=*/
          blink::InterestGroup::ExecutionMode::kCompatibilityMode,
          /*bidding_url=*/
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /*ads=*/{{{ad_url, /*metadata=*/absl::nullopt}}}));

  const char kAuctionConfigTemplate[] = R"({
      seller: $1,
      decisionLogicUrl: $2,
      perBuyerCumulativeTimeouts: new Promise((resolve, reject) => { setTimeout(
          () => { resolve({'http://b.com': 52}); }, 10) }),
      interestGroupBuyers: [$1]
  })";

  WebContentsConsoleObserver console_observer(shell()->web_contents());
  console_observer.SetPattern(
      "Uncaught (in promise) TypeError: Failed to execute 'runAdAuction' on "
      "'NavigatorAuction': perBuyerCumulativeTimeouts buyer 'http://b.com' for "
      "AuctionAdConfig with seller 'https://a.test:*' must be \"*\" (wildcard) "
      "or a valid https origin.");
  EXPECT_EQ("Promise argument rejected or resolved to invalid value.",
            RunAuctionAndWait(
                JsReplace(kAuctionConfigTemplate, test_origin, decision_url)));
  EXPECT_TRUE(console_observer.Wait());
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionInvalidPerBuyerCumulativeTimeoutsOrigin) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));
  AttachInterestGroupObserver();

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': "
      "perBuyerCumulativeTimeouts buyer 'https://invalid^&' for "
      "AuctionAdConfig "
      "with seller 'https://test.com' must be \"*\" (wildcard) or a valid "
      "https origin.",
      RunAuctionAndWait(R"({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://test.com',
      perBuyerCumulativeTimeouts: {'https://invalid^&': 100}
  })"));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionInvalidPerBuyerCurrenciesOrigin) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));
  AttachInterestGroupObserver();

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': "
      "perBuyerCurrencies buyer 'https://invalid^&' for "
      "AuctionAdConfig "
      "with seller 'https://test.com' must be \"*\" (wildcard) or a valid "
      "https origin.",
      RunAuctionAndWait(R"({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://test.com',
      perBuyerCurrencies: {'https://invalid^&': 'USD'}
  })"));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionInvalidPerBuyerCurrenciesCurrency) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));
  AttachInterestGroupObserver();

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator':"
      " perBuyerCurrencies currency 'usd' for AuctionAdConfig with seller"
      " 'https://test.com' must be a 3-letter uppercase currency code.",
      RunAuctionAndWait(R"({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://test.com',
      perBuyerCurrencies: {'*': 'usd'}
  })"));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionInvalidSellerCurrency) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));
  AttachInterestGroupObserver();

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator':"
      " sellerCurrency 'usd' for AuctionAdConfig with seller"
      " 'https://test.com' must be a 3-letter uppercase currency code.",
      RunAuctionAndWait(R"({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://test.com',
      sellerCurrency: 'usd'
  })"));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionInvalidPerBuyerGroupLimitsValue) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));
  AttachInterestGroupObserver();

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': "
      "perBuyerGroupLimits value '0' for AuctionAdConfig with "
      "seller 'https://test.com' must be greater than 0.",
      RunAuctionAndWait(R"({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://test.com',
      perBuyerGroupLimits: {'https://test.com': 0}
  })"));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionInvalidPerBuyerGroupLimitsOrigin) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));
  AttachInterestGroupObserver();

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': "
      "perBuyerGroupLimits buyer 'https://invalid^&' for AuctionAdConfig with "
      "seller 'https://test.com' must be \"*\" (wildcard) or a valid https "
      "origin.",
      RunAuctionAndWait(R"({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://test.com',
      perBuyerGroupLimits: {'https://invalid^&': 100}
  })"));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionInvalidPerBuyerPrioritySignals) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));
  AttachInterestGroupObserver();

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': Failed to "
      "read the 'perBuyerPrioritySignals' property from 'AuctionAdConfig': The "
      "provided double value is non-finite.",
      RunAuctionAndWait(R"({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://test.com',
      perBuyerPrioritySignals: {
          'https://foo.com/':{"key": "Values must be numbers"}
      }
  })"));
  WaitForAccessObserved({});

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': "
      "perBuyerPrioritySignals key 'browserSignals.thisPrefixIsReserved' for "
      "AuctionAdConfig with seller 'https://test.com' must not start with "
      "reserved \"browserSignals.\" prefix.",
      RunAuctionAndWait(R"({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://test.com',
      perBuyerPrioritySignals: {
          'https://foo.com/':{"browserSignals.thisPrefixIsReserved": 1}
      }
  })"));
  WaitForAccessObserved({});
}

// It's invalid for an auction to have both component auctions and buyers.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionInvalidComponentAuctionsAndBuyers) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': Auctions "
      "may only have one of 'interestGroupBuyers' or 'componentAuctions'.",
      RunAuctionAndWait(R"({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://test.com',
      interestGroupBuyers: ['https://test.com'],
      componentAuctions: [{
          seller: 'https://test.com',
          decisionLogicUrl: 'https://test.com',
          interestGroupBuyers: ['https://test.com']
      }]
  })"));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionInvalidComponentAuctionsArray) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': Failed to "
      "read the 'componentAuctions' property from 'AuctionAdConfig': The "
      "provided value cannot be converted to a sequence.",
      RunAuctionAndWait(R"({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://test.com',
      componentAuctions: ''
  })"));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionInvalidComponentAuctionsElementType) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': Failed to "
      "read the 'componentAuctions' property from 'AuctionAdConfig': "
      "The provided value is not of type 'AuctionAdConfig'.",
      RunAuctionAndWait(R"({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://test.com',
      componentAuctions: ['test']
  })"));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionInvalidComponentAuctionsAuctionConfig) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': seller "
      "'http://test.com' for AuctionAdConfig must be a valid https origin.",
      RunAuctionAndWait(R"({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://test.com',
      componentAuctions: [{
        seller: 'http://test.com',
        decisionLogicUrl: 'http://test.com'
      }]
  })"));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionInvalidComponentAuctionDepth) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': Auctions "
      "listed in componentAuctions may not have their own nested "
      "componentAuctions.",
      RunAuctionAndWait(R"({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://test.com',
      componentAuctions: [{
        seller: 'https://test2.com',
        decisionLogicUrl: 'https://test2.com',
        componentAuctions: [{
          seller: 'https://test3.com',
          decisionLogicUrl: 'https://test3.com',
        }]
      }]
  })"));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionInvalidPerBuyerSignals) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));
  AttachInterestGroupObserver();

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': "
      "perBuyerSignals for AuctionAdConfig with seller 'https://test.com' "
      "must be a JSON-serializable object.",
      RunAuctionAndWait(R"({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://test.com',
      perBuyerSignals: {'https://test.com': function() {}}
  })"));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionRejectPromiseDirectFromSellerSignals) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  url::Origin test_origin = url::Origin::Create(test_url);
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  GURL ad_url = https_server_->GetURL("c.test", "/echo?render_cars");
  GURL decision_url =
      https_server_->GetURL("a.test", "/interest_group/decision_logic.js");
  // Note: at present at least one bid must be made for promise checking to
  // be guaranteed to happen; if the auction is (effectively) empty whether
  // it happens or not is timing-dependent.
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          /*owner=*/test_origin,
          /*name=*/"cars",
          /*priority=*/0.0,
          /*execution_mode=*/
          blink::InterestGroup::ExecutionMode::kCompatibilityMode,
          /*bidding_url=*/
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /*ads=*/{{{ad_url, /*metadata=*/absl::nullopt}}}));

  const char kAuctionConfigTemplate[] = R"({
      seller: $1,
      decisionLogicUrl: $2,
      directFromSellerSignals: new Promise((resolve, reject) => { setTimeout(
          () => { reject('boo'); }, 10) }),
      interestGroupBuyers: [$1]
  })";

  EXPECT_EQ("Promise argument rejected or resolved to invalid value.",
            RunAuctionAndWait(
                JsReplace(kAuctionConfigTemplate, test_origin, decision_url)));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionPromiseInvalidDirectFromSellerSignals) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  url::Origin test_origin = url::Origin::Create(test_url);
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  GURL ad_url = https_server_->GetURL("c.test", "/echo?render_cars");
  GURL decision_url =
      https_server_->GetURL("a.test", "/interest_group/decision_logic.js");
  // Note: at present at least one bid must be made for promise checking to
  // be guaranteed to happen; if the auction is (effectively) empty whether
  // it happens or not is timing-dependent.
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          /*owner=*/test_origin,
          /*name=*/"cars",
          /*priority=*/0.0,
          /*execution_mode=*/
          blink::InterestGroup::ExecutionMode::kCompatibilityMode,
          /*bidding_url=*/
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /*ads=*/{{{ad_url, /*metadata=*/absl::nullopt}}}));

  const char kAuctionConfigTemplate[] = R"({
      seller: $1,
      decisionLogicUrl: $2,
      directFromSellerSignals: new Promise((resolve, reject) => { setTimeout(
          () => { resolve('http://test.com/signals'); }, 10) }),
      interestGroupBuyers: [$1]
  })";

  WebContentsConsoleObserver console_observer(shell()->web_contents());
  console_observer.SetPattern(
      "Uncaught (in promise) TypeError: Failed to execute 'runAdAuction' on "
      "'NavigatorAuction': directFromSellerSignals 'http://test.com/signals' "
      "for AuctionAdConfig with seller 'https://a.test:*' must match seller "
      "origin; only https scheme is supported.");
  EXPECT_EQ("Promise argument rejected or resolved to invalid value.",
            RunAuctionAndWait(
                JsReplace(kAuctionConfigTemplate, test_origin, decision_url)));
  EXPECT_TRUE(console_observer.Wait());
}

IN_PROC_BROWSER_TEST_F(
    InterestGroupBrowserTest,
    RunAdAuctionPromiseToStringThrowDirectFromSellerSignals) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  url::Origin test_origin = url::Origin::Create(test_url);
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  GURL ad_url = https_server_->GetURL("c.test", "/echo?render_cars");
  GURL decision_url =
      https_server_->GetURL("a.test", "/interest_group/decision_logic.js");
  // Note: at present at least one bid must be made for promise checking to
  // be guaranteed to happen; if the auction is (effectively) empty whether
  // it happens or not is timing-dependent.
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          /*owner=*/test_origin,
          /*name=*/"cars",
          /*priority=*/0.0,
          /*execution_mode=*/
          blink::InterestGroup::ExecutionMode::kCompatibilityMode,
          /*bidding_url=*/
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /*ads=*/{{{ad_url, /*metadata=*/absl::nullopt}}}));

  const char kAuctionConfigTemplate[] = R"({
      seller: $1,
      decisionLogicUrl: $2,
      directFromSellerSignals: new Promise((resolve, reject) => {
        let o = { toString: () => { throw "Don't stringify me!"; } }
        resolve(o);
      }),
      interestGroupBuyers: [$1]
  })";

  WebContentsConsoleObserver console_observer(shell()->web_contents());
  console_observer.SetPattern("Uncaught (in promise) Don't stringify me!");
  EXPECT_EQ("Promise argument rejected or resolved to invalid value.",
            RunAuctionAndWait(
                JsReplace(kAuctionConfigTemplate, test_origin, decision_url)));
  EXPECT_TRUE(console_observer.Wait());
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionInvalidDirectFromSellerSignalsInvalidURL) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));
  AttachInterestGroupObserver();

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': "
      "directFromSellerSignals 'https://invalid^&' for AuctionAdConfig with "
      "seller 'https://test.com' cannot be resolved to a valid URL.",
      RunAuctionAndWait(R"({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://test.com',
      directFromSellerSignals: 'https://invalid^&'
  })"));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionInvalidDirectFromSellerSignalsNotHttps) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));
  AttachInterestGroupObserver();

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': "
      "directFromSellerSignals 'http://test.com/signals' for AuctionAdConfig "
      "with seller 'https://test.com' must match seller origin; only https "
      "scheme is supported.",
      RunAuctionAndWait(R"({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://test.com',
      directFromSellerSignals: 'http://test.com/signals'
  })"));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionInvalidDirectFromSellerSignalsWrongOrigin) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));
  AttachInterestGroupObserver();

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': "
      "directFromSellerSignals 'https://test2.com/signals' for AuctionAdConfig "
      "with seller 'https://test.com' must match seller origin; only https "
      "scheme is supported.",
      RunAuctionAndWait(R"({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://test.com',
      directFromSellerSignals: 'https://test2.com/signals'
  })"));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(
    InterestGroupBrowserTest,
    RunAdAuctionInvalidDirectFromSellerSignalsHasQueryString) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));
  AttachInterestGroupObserver();

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': "
      "directFromSellerSignals 'https://test.com/signals?shouldntBeHere' for "
      "AuctionAdConfig with seller 'https://test.com' URL prefix must not have "
      "a query string.",
      RunAuctionAndWait(R"({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://test.com',
      directFromSellerSignals: 'https://test.com/signals?shouldntBeHere'
  })"));
  WaitForAccessObserved({});
}

// `bidder_origin` is used in per-buyer signals, but the bundle only has
// per-buyer signals for `non_bidder_origin`.
//
// No signals should be delivered.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       DirectFromSellerSignalsNotInBuyers) {
  constexpr char kBidderHost[] = "a.test";
  constexpr char kTopFrameHost[] = "c.test";
  constexpr char kSellerHost[] = "b.test";
  constexpr char kNonBidderHost[] = "d.test";
  url::Origin seller_origin =
      url::Origin::Create(https_server_->GetURL(kSellerHost, "/echo"));
  url::Origin top_frame_origin =
      url::Origin::Create(https_server_->GetURL(kTopFrameHost, "/echo"));

  GURL bidder_url = https_server_->GetURL(kBidderHost, "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), bidder_url));
  url::Origin bidder_origin = url::Origin::Create(bidder_url);
  GURL non_bidder_url = https_server_->GetURL(kNonBidderHost, "/echo");
  url::Origin non_bidder_origin = url::Origin::Create(non_bidder_url);

  ASSERT_EQ(kSuccess,
            JoinInterestGroupAndVerify(
                /*owner=*/bidder_origin, /*name=*/"cars", /*priority=*/0.0,
                blink::InterestGroup::ExecutionMode::kCompatibilityMode,
                /*bidding_url=*/
                https_server_->GetURL(
                    kBidderHost,
                    "/interest_group/"
                    "bidding_no_direct_from_seller_signals_validator.js"),
                /*ads=*/
                {{{GURL("https://example.com/render"),
                   /*metadata=*/absl::nullopt}}}));

  std::vector<NetworkResponder::SubresourceResponse> subresource_responses = {
      NetworkResponder::DirectFromSellerPerBuyerSignals(
          non_bidder_origin, /*payload=*/
          R"({"json": "for", "buyer": [1]})")};
  std::vector<NetworkResponder::SubresourceBundle> bundles = {
      NetworkResponder::SubresourceBundle(
          /*bundle_url=*/https_server_->GetURL(kSellerHost,
                                               "/generated_bundle.wbn"),
          /*subresources=*/subresource_responses)};

  network_responder_->RegisterDirectFromSellerSignalsResponse(
      /*bundles=*/bundles,
      /*allow_origin=*/top_frame_origin.Serialize());
  constexpr char kPagePath[] = "/page-with-bundles.html";
  network_responder_->RegisterHtmlWithSubresourceBundles(
      /*bundles=*/bundles,
      /*page_url=*/kPagePath);

  ASSERT_TRUE(
      NavigateToURL(shell(), https_server_->GetURL(kTopFrameHost, kPagePath)));

  TestFencedFrameURLMappingResultObserver observer;
  ConvertFencedFrameURNToURL(
      GURL(
          EvalJs(shell(),
                 JsReplace(
                     R"(
(async function() {
  return await navigator.runAdAuction({
      seller: $1,
      decisionLogicUrl: $2,
      interestGroupBuyers: [$3],
      directFromSellerSignals: $4
  });
})())",
                     seller_origin.Serialize().c_str(),
                     https_server_->GetURL(
                         kSellerHost,
                         "/interest_group/"
                         "decision_no_direct_from_seller_signals_validator.js"),
                     bidder_origin.Serialize().c_str(),
                     https_server_->GetURL(kSellerHost,
                                           "/direct_from_seller_signals")))
              .ExtractString()),
      &observer);
  EXPECT_EQ(GURL("https://example.com/render"), observer.mapped_url());
}

// The bundle is served from `seller_origin`, but the subresource is from
// `bidder_origin` -- subresource bundles don't allow subresources to be
// cross-origin with their bundle's origin.
//
// No signals should be delivered.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       DirectFromSellerSignalsBundleSubresourceOriginMismatch) {
  constexpr char kBidderHost[] = "a.test";
  constexpr char kTopFrameHost[] = "c.test";
  constexpr char kSellerHost[] = "b.test";
  url::Origin seller_origin =
      url::Origin::Create(https_server_->GetURL(kSellerHost, "/echo"));
  url::Origin top_frame_origin =
      url::Origin::Create(https_server_->GetURL(kTopFrameHost, "/echo"));

  GURL bidder_url = https_server_->GetURL(kBidderHost, "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), bidder_url));
  url::Origin bidder_origin = url::Origin::Create(bidder_url);

  ASSERT_EQ(kSuccess,
            JoinInterestGroupAndVerify(
                /*owner=*/bidder_origin, /*name=*/"cars", /*priority=*/0.0,
                blink::InterestGroup::ExecutionMode::kCompatibilityMode,
                /*bidding_url=*/
                https_server_->GetURL(
                    kBidderHost,
                    "/interest_group/"
                    "bidding_no_direct_from_seller_signals_validator.js"),
                /*ads=*/
                {{{GURL("https://example.com/render"),
                   /*metadata=*/absl::nullopt}}}));

  std::vector<NetworkResponder::SubresourceResponse> subresource_responses = {
      NetworkResponder::DirectFromSellerPerBuyerSignals(
          bidder_origin,                      /*payload=*/
          R"({"json": "for", "buyer": [1]})", /*prefix=*/
          base::StringPrintf("%s/direct_from_seller_signals",
                             bidder_origin.Serialize().c_str()))};
  std::vector<NetworkResponder::SubresourceBundle> bundles = {
      NetworkResponder::SubresourceBundle(
          /*bundle_url=*/https_server_->GetURL(kSellerHost,
                                               "/generated_bundle.wbn"),
          /*subresources=*/subresource_responses)};

  network_responder_->RegisterDirectFromSellerSignalsResponse(
      /*bundles=*/bundles,
      /*allow_origin=*/top_frame_origin.Serialize());
  constexpr char kPagePath[] = "/page-with-bundles.html";
  network_responder_->RegisterHtmlWithSubresourceBundles(
      /*bundles=*/bundles,
      /*page_url=*/kPagePath);

  ASSERT_TRUE(
      NavigateToURL(shell(), https_server_->GetURL(kTopFrameHost, kPagePath)));

  TestFencedFrameURLMappingResultObserver observer;
  ConvertFencedFrameURNToURL(
      GURL(
          EvalJs(shell(),
                 JsReplace(
                     R"(
(async function() {
  return await navigator.runAdAuction({
      seller: $1,
      decisionLogicUrl: $2,
      interestGroupBuyers: [$3],
      directFromSellerSignals: $4
  });
})())",
                     seller_origin.Serialize().c_str(),
                     https_server_->GetURL(
                         kSellerHost,
                         "/interest_group/"
                         "decision_no_direct_from_seller_signals_validator.js"),
                     bidder_origin.Serialize().c_str(),
                     https_server_->GetURL(kSellerHost,
                                           "/direct_from_seller_signals")))
              .ExtractString()),
      &observer);
  EXPECT_EQ(GURL("https://example.com/render"), observer.mapped_url());
}

// Use "bundle_doesnt_exist.wbn" as the bundle filename -- the fetch for the
// bundle will fail, and null will be passed to the worklet. Note that no
// exception is thrown; the auction does run, since the
// <script type="webbundle"> tag does declare a subresource
// "/direct_from_seller_signals?auctionSignals", but the bundle itself (which
// loads asynchronously) fails to load, causing the auction-time
// DirectFromSellerSignals load to fail.
//
// No signals should be delivered.
IN_PROC_BROWSER_TEST_F(
    InterestGroupBrowserTest,
    RunAdAuctionInvalidDirectFromSellerSignalsBundleDoesntExist) {
  constexpr char kBidderHost[] = "a.test";
  constexpr char kTopFrameHost[] = "c.test";
  constexpr char kSellerHost[] = "b.test";
  url::Origin seller_origin =
      url::Origin::Create(https_server_->GetURL(kSellerHost, "/echo"));
  url::Origin top_frame_origin =
      url::Origin::Create(https_server_->GetURL(kTopFrameHost, "/echo"));

  GURL bidder_url = https_server_->GetURL(kBidderHost, "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), bidder_url));
  url::Origin bidder_origin = url::Origin::Create(bidder_url);

  ASSERT_EQ(kSuccess,
            JoinInterestGroupAndVerify(
                /*owner=*/bidder_origin, /*name=*/"cars", /*priority=*/0.0,
                blink::InterestGroup::ExecutionMode::kCompatibilityMode,
                /*bidding_url=*/
                https_server_->GetURL(
                    kBidderHost,
                    "/interest_group/"
                    "bidding_no_direct_from_seller_signals_validator.js"),
                /*ads=*/
                {{{GURL("https://example.com/render"),
                   /*metadata=*/absl::nullopt}}}));

  std::vector<NetworkResponder::SubresourceResponse> subresource_responses = {
      NetworkResponder::SubresourceResponse(
          /*subresource_url=*/"/direct_from_seller_signals?auctionSignals",
          /*payload=*/
          // NOTE: This doesn't really matter -- it's never sent.
          R"({"json": "for", "all": ["parties"]})")};
  // Tell the page about this bundle, but don't actually serve it over the
  // network.
  std::vector<NetworkResponder::SubresourceBundle> bundles = {
      NetworkResponder::SubresourceBundle(
          /*bundle_url=*/https_server_->GetURL(kSellerHost,
                                               "/bundle_doesnt_exist.wbn"),
          /*subresources=*/subresource_responses)};

  constexpr char kPagePath[] = "/page-with-bundles.html";
  network_responder_->RegisterHtmlWithSubresourceBundles(
      /*bundles=*/bundles,
      /*page_url=*/kPagePath);

  ASSERT_TRUE(
      NavigateToURL(shell(), https_server_->GetURL(kTopFrameHost, kPagePath)));

  TestFencedFrameURLMappingResultObserver observer;
  ConvertFencedFrameURNToURL(
      GURL(
          EvalJs(shell(),
                 JsReplace(
                     R"(
(async function() {
  return await navigator.runAdAuction({
      seller: $1,
      decisionLogicUrl: $2,
      interestGroupBuyers: [$3],
      directFromSellerSignals: $4
  });
})())",
                     seller_origin.Serialize().c_str(),
                     https_server_->GetURL(
                         kSellerHost,
                         "/interest_group/"
                         "decision_no_direct_from_seller_signals_validator.js"),
                     bidder_origin.Serialize().c_str(),
                     https_server_->GetURL(kSellerHost,
                                           "/direct_from_seller_signals")))
              .ExtractString()),
      &observer);
  EXPECT_EQ(GURL("https://example.com/render"), observer.mapped_url());
}

// Create a cross-origin iframe, and run an auction in that iframe using
// DirectFromSellerSignals.
//
// The signals should be correctly loaded.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       DirectFromSellerSignalsInCrossOriginIframe) {
  constexpr char kBidderHost[] = "a.test";
  constexpr char kTopFrameHost[] = "c.test";
  constexpr char kSellerHost[] = "b.test";
  constexpr char kIframeHost[] = "d.test";
  url::Origin seller_origin =
      url::Origin::Create(https_server_->GetURL(kSellerHost, "/echo"));
  url::Origin iframe_origin =
      url::Origin::Create(https_server_->GetURL(kIframeHost, "/echo"));

  GURL bidder_url = https_server_->GetURL(kBidderHost, "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), bidder_url));
  url::Origin bidder_origin = url::Origin::Create(bidder_url);

  ASSERT_EQ(kSuccess,
            JoinInterestGroupAndVerify(
                /*owner=*/bidder_origin, /*name=*/"cars", /*priority=*/0.0,
                blink::InterestGroup::ExecutionMode::kCompatibilityMode,
                /*bidding_url=*/
                https_server_->GetURL(kBidderHost,
                                      "/interest_group/bidding_logic.js"),
                /*ads=*/
                {{{GURL("https://example.com/render"),
                   /*metadata=*/absl::nullopt}}}));

  std::vector<NetworkResponder::SubresourceResponse> subresource_responses = {
      NetworkResponder::SubresourceResponse(
          /*subresource_url=*/"/direct_from_seller_signals?sellerSignals",
          /*payload=*/
          R"({"json": "for", "the": ["seller"]})")};
  std::vector<NetworkResponder::SubresourceBundle> bundles = {
      NetworkResponder::SubresourceBundle(
          /*bundle_url=*/https_server_->GetURL(kSellerHost,
                                               "/generated_bundle.wbn"),
          /*subresources=*/subresource_responses)};

  network_responder_->RegisterDirectFromSellerSignalsResponse(
      /*bundles=*/bundles,
      /*allow_origin=*/iframe_origin.Serialize());
  constexpr char kIframePagePath[] = "/page-with-bundles.html";
  network_responder_->RegisterHtmlWithSubresourceBundles(
      /*bundles=*/bundles,
      /*page_url=*/kIframePagePath);
  GURL top_frame_url = https_server_->GetURL(
      kTopFrameHost,
      base::StringPrintf(
          "/cross_site_iframe_factory.html?%s(%s{run-ad-auction})",
          kTopFrameHost,
          https_server_->GetURL(kIframeHost, kIframePagePath).spec().c_str()));

  for (bool use_promise : {false, true}) {
    SCOPED_TRACE(use_promise);
    ASSERT_TRUE(NavigateToURL(shell(), top_frame_url));
    RenderFrameHost* const iframe_host =
        ChildFrameAt(web_contents()->GetPrimaryMainFrame(), /*index=*/0);

    TestFencedFrameURLMappingResultObserver observer;
    ConvertFencedFrameURNToURL(
        GURL(EvalJs(
                 iframe_host,
                 JsReplace(
                     std::string(use_promise ? kFeedPromise : kFeedDirect) + R"(
(async function() {
  return await navigator.runAdAuction({
      seller: $1,
      decisionLogicUrl: $2,
      interestGroupBuyers: [$3],
      directFromSellerSignals: maybePromise($4)
  });
})())",
                     seller_origin.Serialize().c_str(),
                     https_server_->GetURL(kSellerHost,
                                           "/interest_group/"
                                           "decision_simple_direct_from_"
                                           "seller_signals_validator.js"),
                     bidder_origin.Serialize().c_str(),
                     https_server_->GetURL(kSellerHost,
                                           "/direct_from_seller_signals")))
                 .ExtractString()),
        &observer);
    EXPECT_EQ(GURL("https://example.com/render"), observer.mapped_url());
  }
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionBuyersNoInterestGroup) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  AttachInterestGroupObserver();

  EXPECT_EQ(nullptr, RunAuctionAndWait(JsReplace(
                         R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
                         })",
                         url::Origin::Create(test_url),
                         https_server_->GetURL(
                             "a.test", "/interest_group/decision_logic.js"))));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionAuctionReportBuyerKeysNotBigInt) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  AttachInterestGroupObserver();

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': "
      "Failed to read the 'auctionReportBuyerKeys' property from "
      "'AuctionAdConfig': The provided value is not a BigInt.",
      RunAuctionAndWait(JsReplace(
          R"({
    seller: $1,
    decisionLogicUrl: $2,
    auctionReportBuyerKeys: [3],
                         })",
          url::Origin::Create(test_url),
          https_server_->GetURL("a.test",
                                "/interest_group/decision_logic.js"))));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionAuctionReportBuyerKeysTooLargeBigInt) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  AttachInterestGroupObserver();

  EXPECT_EQ(base::StringPrintf(
                "TypeError: Failed to execute 'runAdAuction' on 'Navigator': "
                "auctionReportBuyerKeys for AuctionAdConfig with seller '%s': "
                "Too large BigInt; Must fit in 128 bits",
                url::Origin::Create(test_url).Serialize().c_str()),
            RunAuctionAndWait(JsReplace(
                R"({
    seller: $1,
    decisionLogicUrl: $2,
    auctionReportBuyerKeys: [1n << 129n],
                         })",
                url::Origin::Create(test_url),
                https_server_->GetURL("a.test",
                                      "/interest_group/decision_logic.js"))));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionAuctionReportBuyerKeysNegativeBigInt) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  AttachInterestGroupObserver();

  EXPECT_EQ(base::StringPrintf(
                "TypeError: Failed to execute 'runAdAuction' on 'Navigator': "
                "auctionReportBuyerKeys for AuctionAdConfig with seller '%s': "
                "Negative BigInt cannot be converted to uint128",
                url::Origin::Create(test_url).Serialize().c_str()),
            RunAuctionAndWait(JsReplace(
                R"({
    seller: $1,
    decisionLogicUrl: $2,
    auctionReportBuyerKeys: [-1n],
                         })",
                url::Origin::Create(test_url),
                https_server_->GetURL("a.test",
                                      "/interest_group/decision_logic.js"))));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(
    InterestGroupBrowserTest,
    RunAdAuctionAuctionReportBuyersUnknownReportTypeIgnored) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  AttachInterestGroupObserver();

  EXPECT_EQ(nullptr, RunAuctionAndWait(JsReplace(
                         R"({
    seller: $1,
    decisionLogicUrl: $2,
    auctionReportBuyerKeys: [1n],
    auctionReportBuyers: {
      unknownReportType: { bucket: 0n, scale: 1 },
    }
                         })",
                         url::Origin::Create(test_url),
                         https_server_->GetURL(
                             "a.test", "/interest_group/decision_logic.js"))));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionAuctionReportBuyersIncompleteDictionary) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': Failed to "
      "read the 'auctionReportBuyers' property from 'AuctionAdConfig': Failed "
      "to read the 'scale' property from 'AuctionReportBuyersConfig': Required "
      "member is undefined.",
      RunAuctionAndWait(JsReplace(
          R"({
    seller: $1,
    decisionLogicUrl: $2,
    auctionReportBuyerKeys: [1n],
    auctionReportBuyers: {
      bidCount: { bucket: 0n },
    }
                         })",
          url::Origin::Create(test_url),
          https_server_->GetURL("a.test",
                                "/interest_group/decision_logic.js"))));

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': Failed to "
      "read the 'auctionReportBuyers' property from 'AuctionAdConfig': Failed "
      "to read the 'bucket' property from 'AuctionReportBuyersConfig': "
      "Required member is undefined.",
      RunAuctionAndWait(JsReplace(
          R"({
    seller: $1,
    decisionLogicUrl: $2,
    auctionReportBuyerKeys: [1n],
    auctionReportBuyers: {
      bidCount: { scale: 1 },
    }
                         })",
          url::Origin::Create(test_url),
          https_server_->GetURL("a.test",
                                "/interest_group/decision_logic.js"))));
}

IN_PROC_BROWSER_TEST_F(
    InterestGroupBrowserTest,
    RunAdAuctionAuctionInvalidRequiredSellerCapabilitiesIgnored) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  AttachInterestGroupObserver();

  EXPECT_EQ(nullptr, RunAuctionAndWait(JsReplace(
                         R"({
    seller: $1,
    decisionLogicUrl: $2,
    requiredSellerCapabilities: ['non-valid-capability'],
                         })",
                         url::Origin::Create(test_url),
                         https_server_->GetURL(
                             "a.test", "/interest_group/decision_logic.js"))));
  WaitForAccessObserved({});
}

// Run an auction with 2 interest groups. One of the interest groups will
// satisfy the `requiredSellerCapabilities` of the auction config, and one will
// not.
//
// The bid of the interest group satisfied the `requiredSellerCapabilities`
// wins the auction, even though normally the other interest group would win,
// because the other interest group was removed from the auction for failing to
// satisfy the `requiredSellerCapabilities`.
//
// Both interest groups set an update URL, so after the auction, a post auction
// interest group update occurs and succeeds for both groups. (This is so that
// bidders can choose to make the groups that don't meet the
// requiredSellerCapabilities update to then satisfy those conditions).
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RequiredSellerCapabilitiesWithPostAuctionUpdates) {
  GURL test_url = https_server_->GetURL("a.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);
  GURL ad1_url =
      https_server_->GetURL("c.test", "/echo?stop_bidding_after_win");
  GURL ad2_url = https_server_->GetURL("c.test", "/echo?render_bikes");

  constexpr char kUpdatePath[] = "/interest_group/update.json";
  constexpr char kUpdateResponse[] = R"(
{
  "sellerCapabilities": {
    "*": ["interest-group-counts", "latency-stats"]
  }
})";
  network_responder_->RegisterNetworkResponse(kUpdatePath, kUpdateResponse);
  GURL update_url = https_server_->GetURL("a.test", kUpdatePath);

  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          blink::TestInterestGroupBuilder(
              /*owner=*/test_origin,
              /*name=*/"cars")
              .SetBiddingUrl(https_server_->GetURL(
                  "a.test",
                  "/interest_group/bidding_logic_stop_bidding_after_win.js"))
              .SetAds({{{ad1_url, /*metadata=*/absl::nullopt}}})
              .SetUpdateUrl(update_url)
              .Build()));
  EXPECT_EQ(kSuccess,
            JoinInterestGroupAndVerify(
                blink::TestInterestGroupBuilder(
                    /*owner=*/test_origin,
                    /*name=*/"bikes")
                    .SetBiddingUrl(https_server_->GetURL(
                        "a.test", "/interest_group/bidding_logic.js"))
                    .SetAds({{{ad2_url, /*metadata=*/absl::nullopt}}})
                    .SetUpdateUrl(update_url)
                    .SetAllSellerCapabilities(
                        blink::SellerCapabilities::kInterestGroupCounts)
                    .Build()));

  // `ad2_url` wins, because "cars" is removed for not satisfying
  // requiredSellerCapabilities.
  RunAuctionAndWaitForURLAndNavigateIframe(
      JsReplace(
          R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
    requiredSellerCapabilities: ['interest-group-counts'],
                })",
          test_origin,
          https_server_->GetURL("a.test", "/interest_group/decision_logic.js")),
      ad2_url);

  // A post-auction update occurs.
  WaitForInterestGroupsSatisfying(
      test_origin,
      base::BindLambdaForTesting(
          [](const std::vector<StorageInterestGroup>& groups) {
            if (groups.size() != 2) {
              return false;
            }
            for (const StorageInterestGroup& group : groups) {
              if (group.interest_group.all_sellers_capabilities !=
                  blink::SellerCapabilitiesType(
                      blink::SellerCapabilities::kInterestGroupCounts,
                      blink::SellerCapabilities::kLatencyStats)) {
                return false;
              }
            }
            return true;
          }));

  // `ad1_url` now wins, because "cars" satisfies requiredSellerCapabilities.
  RunAuctionAndWaitForURLAndNavigateIframe(
      JsReplace(
          R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
    requiredSellerCapabilities: ['interest-group-counts'],
                })",
          test_origin,
          https_server_->GetURL("a.test", "/interest_group/decision_logic.js")),
      ad1_url);
}

// Like RequiredSellerCapabilitiesWithPostAuctionUpdates, but setting the
// sellerCapabilities per-buyer instead of for all buyers.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RequiredSellerCapabilitiesWithPerBuyerCapabilities) {
  GURL test_url = https_server_->GetURL("a.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);
  GURL ad1_url =
      https_server_->GetURL("c.test", "/echo?stop_bidding_after_win");
  GURL ad2_url = https_server_->GetURL("c.test", "/echo?render_bikes");

  constexpr char kUpdatePath[] = "/interest_group/update.json";
  std::string update_response =
      base::StringPrintf(R"(
{
  "sellerCapabilities": {
    "%s": ["interest-group-counts", "latency-stats"]
  }
})",
                         test_origin.Serialize().c_str());
  network_responder_->RegisterNetworkResponse(kUpdatePath, update_response);
  GURL update_url = https_server_->GetURL("a.test", kUpdatePath);

  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          blink::TestInterestGroupBuilder(
              /*owner=*/test_origin,
              /*name=*/"cars")
              .SetBiddingUrl(https_server_->GetURL(
                  "a.test",
                  "/interest_group/bidding_logic_stop_bidding_after_win.js"))
              .SetAds({{{ad1_url, /*metadata=*/absl::nullopt}}})
              .SetUpdateUrl(update_url)
              .Build()));
  EXPECT_EQ(kSuccess,
            JoinInterestGroupAndVerify(
                blink::TestInterestGroupBuilder(
                    /*owner=*/test_origin,
                    /*name=*/"bikes")
                    .SetBiddingUrl(https_server_->GetURL(
                        "a.test", "/interest_group/bidding_logic.js"))
                    .SetAds({{{ad2_url, /*metadata=*/absl::nullopt}}})
                    .SetUpdateUrl(update_url)
                    .SetSellerCapabilities(
                        {{{test_origin,
                           blink::SellerCapabilities::kInterestGroupCounts}}})
                    .Build()));

  // `ad2_url` wins, because "cars" is removed for not satisfying
  // requiredSellerCapabilities.
  RunAuctionAndWaitForURLAndNavigateIframe(
      JsReplace(
          R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
    requiredSellerCapabilities: ['interest-group-counts'],
                })",
          test_origin,
          https_server_->GetURL("a.test", "/interest_group/decision_logic.js")),
      ad2_url);

  // A post-auction update occurs.
  WaitForInterestGroupsSatisfying(
      test_origin,
      base::BindLambdaForTesting(
          [&test_origin](const std::vector<StorageInterestGroup>& groups) {
            if (groups.size() != 2) {
              return false;
            }
            for (const StorageInterestGroup& group : groups) {
              const auto& seller_capabilities =
                  group.interest_group.seller_capabilities;
              if (!seller_capabilities) {
                return false;
              }
              auto it = seller_capabilities->find(test_origin);
              if (it == seller_capabilities->end()) {
                return false;
              }
              if (it->second !=
                  blink::SellerCapabilitiesType(
                      blink::SellerCapabilities::kInterestGroupCounts,
                      blink::SellerCapabilities::kLatencyStats)) {
                return false;
              }
            }
            return true;
          }));

  // `ad1_url` now wins, because "cars" satisfies requiredSellerCapabilities.
  RunAuctionAndWaitForURLAndNavigateIframe(
      JsReplace(
          R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
    requiredSellerCapabilities: ['interest-group-counts'],
                })",
          test_origin,
          https_server_->GetURL("a.test", "/interest_group/decision_logic.js")),
      ad1_url);
}

// Like RequiredSellerCapabilitiesWithPerBuyerCapabilities, but the per-seller
// sellerCapabilities initially don't match the seller origin, so both interest
// groups are removed from the auction.
IN_PROC_BROWSER_TEST_F(
    InterestGroupBrowserTest,
    RequiredSellerCapabilitiesWithPerBuyerCapabilitiesNoMatch) {
  GURL test_url = https_server_->GetURL("a.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);
  url::Origin other_origin =
      url::Origin::Create(https_server_->GetURL("b.test", "/"));
  GURL ad1_url =
      https_server_->GetURL("c.test", "/echo?stop_bidding_after_win");
  GURL ad2_url = https_server_->GetURL("c.test", "/echo?render_bikes");

  constexpr char kUpdatePath[] = "/interest_group/update.json";
  std::string update_response =
      base::StringPrintf(R"(
{
  "sellerCapabilities": {
    "%s": ["interest-group-counts", "latency-stats"]
  }
})",
                         test_origin.Serialize().c_str());
  network_responder_->RegisterNetworkResponse(kUpdatePath, update_response);
  GURL update_url = https_server_->GetURL("a.test", kUpdatePath);

  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          blink::TestInterestGroupBuilder(
              /*owner=*/test_origin,
              /*name=*/"cars")
              .SetBiddingUrl(https_server_->GetURL(
                  "a.test",
                  "/interest_group/bidding_logic_stop_bidding_after_win.js"))
              .SetAds({{{ad1_url, /*metadata=*/absl::nullopt}}})
              .SetUpdateUrl(update_url)
              .Build()));
  EXPECT_EQ(kSuccess,
            JoinInterestGroupAndVerify(
                blink::TestInterestGroupBuilder(
                    /*owner=*/test_origin,
                    /*name=*/"bikes")
                    .SetBiddingUrl(https_server_->GetURL(
                        "a.test", "/interest_group/bidding_logic.js"))
                    .SetAds({{{ad2_url, /*metadata=*/absl::nullopt}}})
                    .SetUpdateUrl(update_url)
                    .SetSellerCapabilities(
                        {{{other_origin,
                           blink::SellerCapabilities::kInterestGroupCounts}}})
                    .Build()));

  // There is no winner, because "cars" is removed for not satisfying
  // requiredSellerCapabilities, and "bikes" is removed since it only grants
  // "interest-group-counts" to `other_origin`, not seller `test_origin`.
  EXPECT_EQ(nullptr, RunAuctionAndWait(JsReplace(
                         R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
    requiredSellerCapabilities: ['interest-group-counts'],
                })",
                         test_origin,
                         https_server_->GetURL(
                             "a.test", "/interest_group/decision_logic.js"))));

  // A post-auction update occurs.
  WaitForInterestGroupsSatisfying(
      test_origin,
      base::BindLambdaForTesting(
          [&test_origin](const std::vector<StorageInterestGroup>& groups) {
            if (groups.size() != 2) {
              return false;
            }
            for (const StorageInterestGroup& group : groups) {
              const auto& seller_capabilities =
                  group.interest_group.seller_capabilities;
              if (!seller_capabilities) {
                return false;
              }
              auto it = seller_capabilities->find(test_origin);
              if (it == seller_capabilities->end()) {
                return false;
              }
              if (it->second !=
                  blink::SellerCapabilitiesType(
                      blink::SellerCapabilities::kInterestGroupCounts,
                      blink::SellerCapabilities::kLatencyStats)) {
                return false;
              }
            }
            return true;
          }));

  // `ad1_url` now wins, because "cars" satisfies requiredSellerCapabilities.
  RunAuctionAndWaitForURLAndNavigateIframe(
      JsReplace(
          R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
    requiredSellerCapabilities: ['interest-group-counts'],
                })",
          test_origin,
          https_server_->GetURL("a.test", "/interest_group/decision_logic.js")),
      ad1_url);
}

// Only some of the requiredSellerCapabilities are present, so the interest
// group is still removed from the auction.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RequiredSellerCapabilitiesPartialMatch) {
  GURL test_url = https_server_->GetURL("a.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);
  GURL ad1_url =
      https_server_->GetURL("c.test", "/echo?stop_bidding_after_win");
  GURL ad2_url = https_server_->GetURL("c.test", "/echo?render_bikes");

  constexpr char kUpdatePath[] = "/interest_group/update.json";
  constexpr char kUpdateResponse[] = R"(
{
  "sellerCapabilities": {
    "*": ["interest-group-counts", "latency-stats"]
  }
})";
  network_responder_->RegisterNetworkResponse(kUpdatePath, kUpdateResponse);
  GURL update_url = https_server_->GetURL("a.test", kUpdatePath);

  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          blink::TestInterestGroupBuilder(
              /*owner=*/test_origin,
              /*name=*/"cars")
              .SetBiddingUrl(https_server_->GetURL(
                  "a.test",
                  "/interest_group/bidding_logic_stop_bidding_after_win.js"))
              .SetAds({{{ad1_url, /*metadata=*/absl::nullopt}}})
              .SetUpdateUrl(update_url)
              .SetAllSellerCapabilities(
                  blink::SellerCapabilities::kInterestGroupCounts)
              .Build()));
  EXPECT_EQ(kSuccess,
            JoinInterestGroupAndVerify(
                blink::TestInterestGroupBuilder(
                    /*owner=*/test_origin,
                    /*name=*/"bikes")
                    .SetBiddingUrl(https_server_->GetURL(
                        "a.test", "/interest_group/bidding_logic.js"))
                    .SetAds({{{ad2_url, /*metadata=*/absl::nullopt}}})
                    .SetUpdateUrl(update_url)
                    .SetAllSellerCapabilities(
                        {blink::SellerCapabilities::kInterestGroupCounts,
                         blink::SellerCapabilities::kLatencyStats})
                    .Build()));

  // `ad2_url` wins, because "cars" is removed for not satisfying
  // requiredSellerCapabilities.
  RunAuctionAndWaitForURLAndNavigateIframe(
      JsReplace(
          R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
    requiredSellerCapabilities: ['interest-group-counts', 'latency-stats'],
                })",
          test_origin,
          https_server_->GetURL("a.test", "/interest_group/decision_logic.js")),
      ad2_url);

  // A post-auction update occurs.
  WaitForInterestGroupsSatisfying(
      test_origin,
      base::BindLambdaForTesting(
          [](const std::vector<StorageInterestGroup>& groups) {
            if (groups.size() != 2) {
              return false;
            }
            for (const StorageInterestGroup& group : groups) {
              if (group.interest_group.all_sellers_capabilities !=
                  blink::SellerCapabilitiesType(
                      blink::SellerCapabilities::kInterestGroupCounts,
                      blink::SellerCapabilities::kLatencyStats)) {
                return false;
              }
            }
            return true;
          }));

  // `ad1_url` now wins, because "cars" satisfies requiredSellerCapabilities.
  RunAuctionAndWaitForURLAndNavigateIframe(
      JsReplace(
          R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
    requiredSellerCapabilities: ['interest-group-counts'],
                })",
          test_origin,
          https_server_->GetURL("a.test", "/interest_group/decision_logic.js")),
      ad1_url);
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionPrivacySandboxDisabled) {
  // Successful join at a.test
  GURL test_url_a = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url_a));
  url::Origin test_origin_a = url::Origin::Create(test_url_a);
  AttachInterestGroupObserver();

  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          blink::TestInterestGroupBuilder(
              /*owner=*/test_origin_a,
              /*name=*/"cars")
              .SetBiddingUrl(https_server_->GetURL(
                  "a.test", "/interest_group/bidding_logic.js"))
              .SetTrustedBiddingSignalsUrl(https_server_->GetURL(
                  "a.test", "/interest_group/trusted_bidding_signals.json"))
              .SetTrustedBiddingSignalsKeys({{"key1"}})
              .SetAds({{{GURL("https://example.com/render"),
                         R"({"ad":"metadata","here":[1,2]})"}}})
              .Build()));

  GURL test_url_d = https_server_->GetURL("d.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url_d));

  // Auction should not be run since d.test has the API disabled.
  EXPECT_EQ(
      nullptr,
      RunAuctionAndWait(JsReplace(
          R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
    auctionSignals: {x: 1},
    sellerSignals: {yet: 'more', info: 1},
    perBuyerSignals: {$3: {even: 'more', x: 4.5}}
          })",
          url::Origin::Create(test_url_d),
          https_server_->GetURL("d.test", "/interest_group/decision_logic.js"),
          test_origin_a)));

  // No requests should have been made for the interest group or auction URLs.
  base::AutoLock auto_lock(requests_lock_);
  EXPECT_FALSE(base::Contains(
      received_https_test_server_requests_,
      https_server_->GetURL("/interest_group/bidding_logic.js")));
  EXPECT_FALSE(base::Contains(
      received_https_test_server_requests_,
      https_server_->GetURL("/interest_group/trusted_bidding_signals.json")));
  EXPECT_FALSE(base::Contains(
      received_https_test_server_requests_,
      https_server_->GetURL("/interest_group/decision_logic.js")));
  WaitForAccessObserved({
      {TestInterestGroupObserver::kJoin, test_origin_a, "cars"},
  });
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionDisabledInterestGroup) {
  // Inject an interest group into the DB for a disabled site so we can
  // try to remove it.
  GURL disabled_domain = https_server_->GetURL("d.test", "/");
  url::Origin disabled_origin = url::Origin::Create(disabled_domain);
  AttachInterestGroupObserver();

  blink::InterestGroup disabled_group;
  disabled_group.expiry = base::Time::Now() + base::Seconds(300);
  disabled_group.owner = disabled_origin;
  disabled_group.name = "candy";
  disabled_group.bidding_url = https_server_->GetURL(
      disabled_domain.host(),
      "/interest_group/bidding_logic_stop_bidding_after_win.js");
  disabled_group.ads.emplace();
  disabled_group.ads->emplace_back(
      GURL("https://stop_bidding_after_win.com/render"), absl::nullopt);
  manager_->JoinInterestGroup(std::move(disabled_group), disabled_domain);
  ASSERT_EQ(1, GetJoinCount(disabled_origin, "candy"));

  GURL test_url = https_server_->GetURL("a.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);
  GURL ad_url = https_server_->GetURL("c.test", "/echo?render_cars");

  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          blink::TestInterestGroupBuilder(
              /*owner=*/test_origin,
              /*name=*/"cars")
              .SetBiddingUrl(https_server_->GetURL(
                  test_url.host(), "/interest_group/bidding_logic.js"))
              .SetTrustedBiddingSignalsUrl(https_server_->GetURL(
                  test_url.host(),
                  "/interest_group/trusted_bidding_signals.json"))
              .SetTrustedBiddingSignalsKeys({{"key1"}})
              .SetAds(
                  /*ads=*/{{{ad_url, R"({"ad":"metadata","here":[1,2]})"}}})
              .Build()));

  std::string auction_config = JsReplace(
      R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1, $3],
    auctionSignals: {x: 1},
    sellerSignals: {yet: 'more', info: 1},
    perBuyerSignals: {$1: {even: 'more', x: 4.5}}
                })",
      test_origin,
      https_server_->GetURL(test_url.host(),
                            "/interest_group/decision_logic.js"),
      disabled_origin);
  RunAuctionAndWaitForURLAndNavigateIframe(auction_config, ad_url);
  // No requests should have been made for the disabled interest group's URLs.
  base::AutoLock auto_lock(requests_lock_);
  EXPECT_FALSE(base::Contains(
      received_https_test_server_requests_,
      https_server_->GetURL(
          "/interest_group/bidding_logic_stop_bidding_after_win.js")));
  WaitForAccessObserved({
      {TestInterestGroupObserver::kJoin, disabled_origin, "candy"},
      {TestInterestGroupObserver::kJoin, test_origin, "cars"},
      {TestInterestGroupObserver::kLoaded, test_origin, "cars"},
      {TestInterestGroupObserver::kBid, test_origin, "cars"},
      {TestInterestGroupObserver::kWin, test_origin, "cars"},
  });
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, RunAdAuctionWithWinner) {
  URLLoaderMonitor url_loader_monitor;

  GURL test_url = https_server_->GetURL("a.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);
  GURL ad_url = https_server_->GetURL("c.test", "/echo?render_cars");

  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          blink::TestInterestGroupBuilder(
              /*owner=*/test_origin,
              /*name=*/"cars")
              .SetBiddingUrl(https_server_->GetURL(
                  "a.test", "/interest_group/bidding_logic.js"))
              .SetTrustedBiddingSignalsUrl(https_server_->GetURL(
                  "a.test", "/interest_group/trusted_bidding_signals.json"))
              .SetTrustedBiddingSignalsKeys({{"key1"}})
              .SetAds(/*ads=*/{{{ad_url, R"({"ad":"metadata","here":[1,2]})"}}})
              .Build()));

  std::string auction_config = JsReplace(
      R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
    auctionSignals: {x: 1},
    sellerSignals: {yet: 'more', info: 1},
    sellerTimeout: 200,
    perBuyerSignals: {$1: {even: 'more', x: 4.5}},
    perBuyerTimeouts: {$1: 100, '*': 150}
                })",
      test_origin,
      https_server_->GetURL("a.test", "/interest_group/decision_logic.js"));
  RunAuctionAndWaitForURLAndNavigateIframe(auction_config, ad_url);

  // Check ResourceRequest structs of requests issued by the worklet process.
  const struct ExpectedRequest {
    GURL url;
    const char* accept_header;
    bool expect_trusted_params;
  } kExpectedRequests[] = {
      {https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
       "application/javascript", /*expect_trusted_params=*/true},
      {https_server_->GetURL(
           "a.test",
           "/interest_group/trusted_bidding_signals.json?"
           "hostname=a.test&keys=key1&interestGroupNames=cars"),
       "application/json", /*expect_trusted_params=*/true},
      {https_server_->GetURL("a.test", "/interest_group/decision_logic.js"),
       "application/javascript", /*expect_trusted_params=*/false},
  };
  for (const auto& expected_request : kExpectedRequests) {
    SCOPED_TRACE(expected_request.url);

    absl::optional<network::ResourceRequest> request =
        url_loader_monitor.GetRequestInfo(expected_request.url);
    ASSERT_TRUE(request);
    EXPECT_EQ(network::mojom::CredentialsMode::kOmit,
              request->credentials_mode);
    EXPECT_EQ(network::mojom::RedirectMode::kError, request->redirect_mode);
    EXPECT_EQ(test_origin, request->request_initiator);

    EXPECT_EQ(1u, request->headers.GetHeaderVector().size());
    std::string accept_value;
    ASSERT_TRUE(request->headers.GetHeader(net::HttpRequestHeaders::kAccept,
                                           &accept_value));
    EXPECT_EQ(expected_request.accept_header, accept_value);

    EXPECT_EQ(expected_request.expect_trusted_params,
              request->trusted_params.has_value());
    EXPECT_EQ(network::mojom::RequestMode::kNoCors, request->mode);
    if (request->trusted_params) {
      // Requests for interest-group provided URLs are cross-origin to the
      // publisher page, and set trusted params to use the right cache shard,
      // using a trusted URLLoaderFactory.
      const net::IsolationInfo& isolation_info =
          request->trusted_params->isolation_info;
      EXPECT_EQ(net::IsolationInfo::RequestType::kOther,
                isolation_info.request_type());
      url::Origin expected_origin = url::Origin::Create(expected_request.url);
      EXPECT_EQ(expected_origin, isolation_info.top_frame_origin());
      EXPECT_EQ(expected_origin, isolation_info.frame_origin());
      EXPECT_TRUE(isolation_info.site_for_cookies().IsNull());
    }
  }

  // Check ResourceRequest structs of report requests.
  const GURL kExpectedReportUrls[] = {
      https_server_->GetURL("a.test", "/echoall?report_seller"),
      https_server_->GetURL("a.test", "/echoall?report_bidder"),
  };
  for (const auto& expected_report_url : kExpectedReportUrls) {
    SCOPED_TRACE(expected_report_url);

    // Make sure the report URL was actually fetched over the network.
    WaitForUrl(expected_report_url);

    absl::optional<network::ResourceRequest> request =
        url_loader_monitor.WaitForUrl(expected_report_url);
    ASSERT_TRUE(request);
    EXPECT_EQ(network::mojom::CredentialsMode::kOmit,
              request->credentials_mode);
    EXPECT_EQ(network::mojom::RedirectMode::kError, request->redirect_mode);
    EXPECT_EQ(test_origin, request->request_initiator);

    EXPECT_TRUE(request->headers.IsEmpty());

    ASSERT_TRUE(request->trusted_params);
    const net::IsolationInfo& isolation_info =
        request->trusted_params->isolation_info;
    EXPECT_EQ(net::IsolationInfo::RequestType::kOther,
              isolation_info.request_type());
    EXPECT_TRUE(isolation_info.network_isolation_key().IsTransient());
    EXPECT_TRUE(isolation_info.site_for_cookies().IsNull());
  }

  // The two reporting requests should use different NIKs to prevent the
  // requests from being correlated.
  EXPECT_NE(url_loader_monitor.GetRequestInfo(kExpectedReportUrls[0])
                ->trusted_params->isolation_info.network_isolation_key(),
            url_loader_monitor.GetRequestInfo(kExpectedReportUrls[1])
                ->trusted_params->isolation_info.network_isolation_key());
}

// Runs auction just like test InterestGroupBrowserTest.RunAdAuctionWithWinner,
// but runs with the ads specified with sizes info.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionWithSizeWithWinner) {
  GURL test_url = https_server_->GetURL("a.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);
  GURL ad_url = https_server_->GetURL("c.test", "/echo?render_cars");

  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          blink::TestInterestGroupBuilder(
              /*owner=*/test_origin,
              /*name=*/"cars")
              .SetBiddingUrl(https_server_->GetURL(
                  "a.test", "/interest_group/bidding_logic_with_size.js"))
              .SetAds(/*ads=*/{
                  {{ad_url, R"({"ad":"metadata","here":[1,2]})", "group_1"}}})
              .SetAdSizes(
                  {{{"size_1",
                     blink::AdSize(100, blink::AdSize::LengthUnit::kScreenWidth,
                                   50, blink::AdSize::LengthUnit::kPixels)}}})
              .SetSizeGroups({{{"group_1", {"size_1"}}}})
              .Build()));

  std::string auction_config = JsReplace(
      R"({
          seller: $1,
          decisionLogicUrl: $2,
          interestGroupBuyers: [$1]
        })",
      test_origin,
      https_server_->GetURL("a.test", "/interest_group/decision_logic.js"));
  RunAuctionAndWaitForURLAndNavigateIframe(auction_config, ad_url);
}

// Runs auction just like test InterestGroupBrowserTest.RunAdAuctionWithWinner,
// but runs with the ads specified with sizes info. The ad url contains size
// macros, which should be substituted with the size from the winning bid.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionWithSizeWithWinnerMacroSubstitution) {
  GURL test_url = https_server_->GetURL("a.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);
  GURL ad_url = https_server_->GetURL(
      "c.test", "/echo?render_cars&size={%AD_WIDTH%}x{%AD_HEIGHT%}");

  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          blink::TestInterestGroupBuilder(
              /*owner=*/test_origin,
              /*name=*/"cars")
              .SetBiddingUrl(https_server_->GetURL(
                  "a.test", "/interest_group/bidding_logic_with_size.js"))
              .SetTrustedBiddingSignalsUrl(https_server_->GetURL(
                  "a.test", "/interest_group/trusted_bidding_signals.json"))
              .SetTrustedBiddingSignalsKeys({{"key1"}})
              .SetAds(/*ads=*/{{{ad_url, /*metadata=*/absl::nullopt,
                                 /*size_group=*/"group_1"}}})
              .SetAdSizes(
                  {{{"size_1",
                     blink::AdSize(100, blink::AdSize::LengthUnit::kScreenWidth,
                                   50, blink::AdSize::LengthUnit::kPixels)}}})
              .SetSizeGroups({{{"group_1", {"size_1"}}}})
              .Build()));

  std::string auction_config = JsReplace(
      R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
    auctionSignals: {x: 1},
    sellerSignals: {yet: 'more', info: 1},
    sellerTimeout: 200,
    perBuyerSignals: {$1: {even: 'more', x: 4.5}},
    perBuyerTimeouts: {$1: 100, '*': 150}
                })",
      test_origin,
      https_server_->GetURL("a.test", "/interest_group/decision_logic.js"));
  int screen_width = static_cast<int>(display::Screen::GetScreen()
                                          ->GetPrimaryDisplay()
                                          .GetSizeInPixel()
                                          .width());
  GURL expected_url = https_server_->GetURL(
      "c.test",
      base::StringPrintf("/echo?render_cars&size=%ix50", screen_width));
  RunAuctionAndWaitForURLAndNavigateIframe(auction_config, expected_url);
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionWithWinnerReplacedURN) {
  GURL test_url = https_server_->GetURL("a.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);
  GURL ad_url =
      GURL(https_server_->GetURL("c.test", "/%%echo%%?${INTEREST_GROUP_NAME}")
               .spec());
  GURL expected_ad_url = https_server_->GetURL("c.test", "/echo?render_cars");

  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          /*owner=*/test_origin,
          /*name=*/"cars",
          /*priority=*/0.0, /*execution_mode=*/
          blink::InterestGroup::ExecutionMode::kCompatibilityMode,
          /*bidding_url=*/
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /*ads=*/{{{ad_url, R"({"ad":"metadata","here":[1,2]})"}}}));

  std::string auction_config = JsReplace(
      R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
      })",
      test_origin,
      https_server_->GetURL("a.test", "/interest_group/decision_logic.js"));
  auto result = RunAuctionAndWait(auction_config,
                                  /*execution_target=*/absl::nullopt);
  GURL urn_url = GURL(result.ExtractString());
  EXPECT_TRUE(urn_url.is_valid());
  EXPECT_EQ(url::kUrnScheme, urn_url.scheme_piece());

  {
    TestFencedFrameURLMappingResultObserver observer;
    ConvertFencedFrameURNToURL(urn_url, &observer);
    EXPECT_TRUE(observer.mapped_url()) << urn_url;
    EXPECT_EQ(ad_url, observer.mapped_url());
  }

  EXPECT_TRUE(ReplaceInURNInJS(
      urn_url,
      {{"${INTEREST_GROUP_NAME}", "render_cars"}, {"%%echo%%", "echo"}}));

  {
    TestFencedFrameURLMappingResultObserver observer;
    ConvertFencedFrameURNToURL(urn_url, &observer);
    EXPECT_EQ(expected_ad_url, observer.mapped_url());
  }
  NavigateIframeAndCheckURL(web_contents(), urn_url, expected_ad_url);
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       ReplaceURLFailsOnBadReplacementInput) {
  GURL test_url = https_server_->GetURL("a.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  GURL urn_url = GURL("urn:uuid:84a8bf15-8539-432d-bb9f-4eb20eaf400b");
  std::string error;
  EXPECT_FALSE(ReplaceInURNInJS(
      urn_url, {{"${INTEREST_GROUP_NAME}", "render_cars"}, {"%echo%%", "echo"}},
      &error));
  EXPECT_THAT(error, HasSubstr("Replacements must be of the form "));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       ReplaceURLFailsOnMalformedURN) {
  GURL test_url = https_server_->GetURL("a.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  GURL urn_url = GURL("http://test.com");
  std::string error;
  EXPECT_FALSE(ReplaceInURNInJS(
      urn_url,
      {{"${INTEREST_GROUP_NAME}", "render_cars"}, {"%%echo%%", "echo"}},
      &error));
  EXPECT_THAT(error, HasSubstr("Passed URL must be a valid URN URL."));
}

IN_PROC_BROWSER_TEST_F(
    InterestGroupBrowserTest,
    RunAdAuctionPerBuyerSignalsAndPerBuyerTimeoutsOriginNotInBuyers) {
  GURL test_url = https_server_->GetURL("a.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);
  AttachInterestGroupObserver();

  GURL ad_url = https_server_->GetURL("c.test", "/echo?render_cars");

  EXPECT_EQ(kSuccess, JoinInterestGroupAndVerify(
                          blink::TestInterestGroupBuilder(
                              /*owner=*/test_origin,
                              /*name=*/"cars")
                              .SetBiddingUrl(https_server_->GetURL(
                                  "a.test", "/interest_group/bidding_logic.js"))
                              .SetAds({{{ad_url, /*metadata=*/absl::nullopt}}})
                              .Build()));

  RunAuctionAndWaitForURLAndNavigateIframe(
      JsReplace(
          R"({
            seller: $1,
            decisionLogicUrl: $2,
            interestGroupBuyers: [$1],
            perBuyerSignals: {$1: {a:1}, 'https://not_in_buyers.com': {a:1}},
            perBuyerTimeouts: {'https://not_in_buyers.com': 100}
          })",
          test_origin,
          https_server_->GetURL("a.test", "/interest_group/decision_logic.js")),
      ad_url);
  WaitForAccessObserved(
      {{TestInterestGroupObserver::kJoin, test_origin, "cars"},
       {TestInterestGroupObserver::kLoaded, test_origin, "cars"},
       {TestInterestGroupObserver::kBid, test_origin, "cars"},
       {TestInterestGroupObserver::kWin, test_origin, "cars"}});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, RunAdAuctionCancel) {
  // Test cancelling an auction while it's still pending.
  GURL test_url = https_server_->GetURL("a.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);
  GURL ad_url = https_server_->GetURL("c.test", "/echo?render_cars");

  // This uses /hung as the script URL to avoid race with cancellation.
  EXPECT_EQ(kSuccess,
            JoinInterestGroupAndVerify(
                blink::TestInterestGroupBuilder(
                    /*owner=*/test_origin,
                    /*name=*/"cars")
                    .SetBiddingUrl(https_server_->GetURL("a.test", "/hung"))
                    .SetAds({{{ad_url, R"({"ad":"metadata","here":[1,2]})"}}})
                    .Build()));

  std::string auction_script = JsReplace(
      R"(
      let controller = new AbortController();
      const config = {
        seller: $1,
        decisionLogicUrl: $2,
        interestGroupBuyers: [$1],
        signal: controller.signal
      };

      (async function() {
        try {
          let result = navigator.runAdAuction(config);
          controller.abort('a reason');
          return await result;
        } catch (e) {
          return e.toString();
        }
      })())",
      test_origin,
      https_server_->GetURL("a.test", "/interest_group/decision_logic.js"));
  EXPECT_EQ("a reason", EvalJs(shell(), auction_script));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, RunAdAuctionCancelLate) {
  // Test cancelling an auction after it finished (which is a no-op).
  GURL test_url = https_server_->GetURL("a.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);
  GURL ad_url = https_server_->GetURL("c.test", "/echo?render_cars");

  EXPECT_EQ(kSuccess,
            JoinInterestGroupAndVerify(
                blink::TestInterestGroupBuilder(
                    /*owner=*/test_origin,
                    /*name=*/"cars")
                    .SetBiddingUrl(https_server_->GetURL(
                        "a.test", "/interest_group/bidding_logic.js"))
                    .SetAds({{{ad_url, R"({"ad":"metadata","here":[1,2]})"}}})
                    .Build()));

  std::string auction_script = JsReplace(
      R"(
      let controller = new AbortController();
      const config = {
        seller: $1,
        decisionLogicUrl: $2,
        interestGroupBuyers: [$1],
        signal: controller.signal
      };

      (async function() {
        try {
          let result = await navigator.runAdAuction(config);
          controller.abort();
          return result;
        } catch (e) {
          return e.toString();
        }
      })())",
      test_origin,
      https_server_->GetURL("a.test", "/interest_group/decision_logic.js"));

  auto result = EvalJs(shell(), auction_script);
  GURL urn_url = GURL(result.ExtractString());
  EXPECT_TRUE(urn_url.is_valid());
  EXPECT_EQ(url::kUrnScheme, urn_url.scheme_piece());

  TestFencedFrameURLMappingResultObserver observer;
  ConvertFencedFrameURNToURL(urn_url, &observer);
  EXPECT_EQ(ad_url, observer.mapped_url()->spec());
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, RunAdAuctionCancelBefore) {
  // Test cancelling an auction before runAdAuction is even called.
  GURL test_url = https_server_->GetURL("a.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);
  GURL ad_url = https_server_->GetURL("c.test", "/echo?render_cars");

  EXPECT_EQ(kSuccess,
            JoinInterestGroupAndVerify(
                blink::TestInterestGroupBuilder(
                    /*owner=*/test_origin,
                    /*name=*/"cars")
                    .SetBiddingUrl(https_server_->GetURL(
                        "a.test", "/interest_group/bidding_logic.js"))
                    .SetAds({{{ad_url, R"({"ad":"metadata","here":[1,2]})"}}})
                    .Build()));

  std::string auction_script = JsReplace(
      R"(
      let controller = new AbortController();
      const config = {
        seller: $1,
        decisionLogicUrl: $2,
        interestGroupBuyers: [$1],
        signal: controller.signal
      };

      (async function() {
        try {
          controller.abort();
          return await navigator.runAdAuction(config);
        } catch (e) {
          return e.toString();
        }
      })())",
      test_origin,
      https_server_->GetURL("a.test", "/interest_group/decision_logic.js"));

  EXPECT_EQ("AbortError: signal is aborted without reason",
            EvalJs(shell(), auction_script));
}

// Runs an auction where the bidding function uses a WASM helper.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, RunAdAuctionWithBidderWasm) {
  GURL test_url = https_server_->GetURL("a.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);
  GURL ad_url = https_server_->GetURL("c.test", "/echo?render_cars");

  EXPECT_EQ(kSuccess,
            JoinInterestGroupAndVerify(
                blink::TestInterestGroupBuilder(
                    /*owner=*/test_origin,
                    /*name=*/"cars")
                    .SetBiddingUrl(https_server_->GetURL(
                        "a.test", "/interest_group/bidding_logic_use_wasm.js"))
                    .SetBiddingWasmHelperUrl(https_server_->GetURL(
                        "a.test", "/interest_group/multiply.wasm"))
                    .SetAds({{{ad_url, R"({"ad":"metadata","here":[1,2]})"}}})
                    .Build()));
  std::string auction_config = JsReplace(
      R"({
        seller: $1,
        decisionLogicUrl: $2,
        interestGroupBuyers: [$1],
      })",
      test_origin,
      https_server_->GetURL("a.test", "/interest_group/decision_logic.js"));
  RunAuctionAndWaitForURLAndNavigateIframe(auction_config, ad_url);
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionWithDebugReporting) {
  URLLoaderMonitor url_loader_monitor;

  GURL test_url = https_server_->GetURL("a.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);
  GURL ad1_url = https_server_->GetURL("c.test", "/echo?render_winner");
  GURL ad2_url = https_server_->GetURL("c.test", "/echo?render_bikes");
  GURL ad3_url = https_server_->GetURL("c.test", "/echo?render_shoes");

  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          blink::TestInterestGroupBuilder(
              /*owner=*/test_origin,
              /*name=*/"winner")
              .SetBiddingUrl(https_server_->GetURL(
                  "a.test",
                  "/interest_group/bidding_logic_with_debugging_report.js"))
              .SetAds({{{ad1_url, /*metadata=*/absl::nullopt}}})
              .Build()));
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          blink::TestInterestGroupBuilder(
              /*owner=*/test_origin,
              /*name=*/"bikes")
              .SetBiddingUrl(https_server_->GetURL(
                  "a.test",
                  "/interest_group/bidding_logic_with_debugging_report.js"))
              .SetAds({{{ad2_url, /*metadata=*/absl::nullopt}}})
              .Build()));
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          blink::TestInterestGroupBuilder(
              /*owner=*/test_origin,
              /*name=*/"shoes")
              .SetBiddingUrl(https_server_->GetURL(
                  "a.test",
                  "/interest_group/bidding_logic_with_debugging_report.js"))
              .SetAds({{{ad3_url, /*metadata=*/absl::nullopt}}})
              .Build()));

  std::string auction_config = JsReplace(
      R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
    auctionSignals: {x: 1},
    sellerSignals: {yet: 'more', info: 1},
    perBuyerSignals: {$1: {even: 'more', x: 4.5}}
                })",
      test_origin,
      https_server_->GetURL(
          "a.test", "/interest_group/decision_logic_with_debugging_report.js"));
  RunAuctionAndWaitForURLAndNavigateIframe(auction_config, ad1_url);

  // Check ResourceRequest structs of report requests.
  const GURL kExpectedReportUrls[] = {
      // Return value from seller's ReportResult() method.
      https_server_->GetURL("a.test", "/echoall?report_seller"),
      // Return value from winning bidder's ReportWin() method.
      https_server_->GetURL("a.test", "/echoall?report_bidder/winner"),
      // Debugging report URL from seller for win report.
      https_server_->GetURL("a.test", "/echo?seller_debug_report_win/winner"),
      // Debugging report URL from winning bidder for win report.
      https_server_->GetURL("a.test", "/echo?bidder_debug_report_win/winner"),
      // Debugging report URL from seller for loss report.
      https_server_->GetURL("a.test", "/echo?seller_debug_report_loss/bikes"),
      https_server_->GetURL("a.test", "/echo?seller_debug_report_loss/shoes"),
      // Debugging report URL from losing bidders for loss report.
      https_server_->GetURL("a.test", "/echo?bidder_debug_report_loss/bikes"),
      https_server_->GetURL("a.test", "/echo?bidder_debug_report_loss/shoes")};

  for (const auto& expected_report_url : kExpectedReportUrls) {
    SCOPED_TRACE(expected_report_url);

    // Make sure the report URL was actually fetched over the network.
    WaitForUrl(expected_report_url);

    absl::optional<network::ResourceRequest> request =
        url_loader_monitor.WaitForUrl(expected_report_url);
    ASSERT_TRUE(request);
    EXPECT_EQ(network::mojom::CredentialsMode::kOmit,
              request->credentials_mode);
    EXPECT_EQ(network::mojom::RedirectMode::kError, request->redirect_mode);
    EXPECT_EQ(test_origin, request->request_initiator);

    EXPECT_TRUE(request->headers.IsEmpty());

    ASSERT_TRUE(request->trusted_params);
    const net::IsolationInfo& isolation_info =
        request->trusted_params->isolation_info;
    EXPECT_EQ(net::IsolationInfo::RequestType::kOther,
              isolation_info.request_type());
    EXPECT_TRUE(isolation_info.network_isolation_key().IsTransient());
    EXPECT_TRUE(isolation_info.site_for_cookies().IsNull());
  }

  // The reporting requests should use different NIKs to prevent the requests
  // from being correlated.
  EXPECT_NE(url_loader_monitor.GetRequestInfo(kExpectedReportUrls[0])
                ->trusted_params->isolation_info.network_isolation_key(),
            url_loader_monitor.GetRequestInfo(kExpectedReportUrls[2])
                ->trusted_params->isolation_info.network_isolation_key());
  EXPECT_NE(url_loader_monitor.GetRequestInfo(kExpectedReportUrls[2])
                ->trusted_params->isolation_info.network_isolation_key(),
            url_loader_monitor.GetRequestInfo(kExpectedReportUrls[3])
                ->trusted_params->isolation_info.network_isolation_key());
  EXPECT_NE(url_loader_monitor.GetRequestInfo(kExpectedReportUrls[2])
                ->trusted_params->isolation_info.network_isolation_key(),
            url_loader_monitor.GetRequestInfo(kExpectedReportUrls[4])
                ->trusted_params->isolation_info.network_isolation_key());
}

// All bidders' genereteBid() failed so no bid was made, thus no render url.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionWithDebugReportingNoBid) {
  GURL test_url = https_server_->GetURL("a.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);
  GURL ad1_url = https_server_->GetURL("c.test", "/echo?render_shoes");
  GURL ad2_url = https_server_->GetURL("c.test", "/echo?render_bikes");

  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          blink::TestInterestGroupBuilder(
              /*owner=*/test_origin,
              /*name=*/"shoes")
              .SetBiddingUrl(https_server_->GetURL(
                  "a.test", "/interest_group/bidding_logic_loop_forever.js"))
              .SetAds({{{ad1_url, /*metadata=*/absl::nullopt}}})
              .Build()));
  EXPECT_EQ(kSuccess,
            JoinInterestGroupAndVerify(
                blink::TestInterestGroupBuilder(
                    /*owner=*/test_origin,
                    /*name=*/"bikes")
                    .SetBiddingUrl(https_server_->GetURL(
                        "a.test", "/interest_group/bidding_logic_throws.js"))
                    .SetAds({{{ad2_url, /*metadata=*/absl::nullopt}}})
                    .Build()));

  EXPECT_EQ(
      nullptr,
      RunAuctionAndWait(JsReplace(
          R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
    auctionSignals: {x: 1},
    sellerSignals: {yet: 'more', info: 1},
                })",
          test_origin,
          https_server_->GetURL(
              "a.test",
              "/interest_group/decision_logic_with_debugging_report.js"))));

  // Debugging loss reports which are made before generateBid()'s timeout and
  // error-throwing statements should be sent. Those made after that should not
  // be sent.
  const GURL kExpectedReportUrls[] = {
      // Debugging loss report URL (before the timeout) from bidder whose
      // generateBid() timed out.
      https_server_->GetURL(
          "a.test", "/echo?bidder_debug_report_loss/shoes/before_timeout"),
      // Debugging loss report URL (before the error) from bidder whose
      // generateBid() throws an error.
      https_server_->GetURL(
          "a.test", "/echo?bidder_debug_report_loss/bikes/before_error")};

  for (const auto& expected_report_url : kExpectedReportUrls) {
    SCOPED_TRACE(expected_report_url);
    WaitForUrl(expected_report_url);
  }
}

// Test that the FLEDGE properly handles detached documents.
IN_PROC_BROWSER_TEST_F(InterestGroupFencedFrameBrowserTest,
                       DetachedDocumentDoesNotCrash) {
  const char* kTestCases[] = {
      R"(runAdAuction({
        seller: "foo", // required
        decisionLogicUrl: "foo", // required
      }, 0.0)
    )",
      R"(joinAdInterestGroup({
        owner: "foo", // required
        name: "foo", // required
      })
    )",
      "leaveAdInterestGroup()",
      "updateAdInterestGroups()",
      "adAuctionComponents(1)",
      R"(deprecatedURNToURL("foo")
    )",
      R"(deprecatedReplaceInURN("foo", {}))",
      "canLoadAdAuctionFencedFrame()"};

  GURL main_url = https_server_->GetURL("b.test", "/page_with_iframe.html");

  for (const auto* test_case : kTestCases) {
    ASSERT_TRUE(NavigateToURL(shell(), main_url));

    EvalJsResult result = EvalJs(shell(), base::StringPrintf(R"(
        try {
          let child = document.getElementById("test_iframe");
          const detachedNavigator = child.contentWindow.navigator;
          child.remove();
          detachedNavigator.%s;
        } catch(e) {
        }
        "Did not crash"
      )",
                                                             test_case));
    EXPECT_EQ("Did not crash", result) << test_case;
  }
}

// Runs auction just like test InterestGroupBrowserTest.RunAdAuctionWithWinner,
// but runs with fenced frames enabled and expects to receive a URN URL to be
// used. After the auction, loads the URL in a fenced frame, and expects the
// correct URL is loaded.
IN_PROC_BROWSER_TEST_F(InterestGroupFencedFrameBrowserTest,
                       RunAdAuctionWithWinner) {
  URLLoaderMonitor url_loader_monitor;

  GURL test_url = https_server_->GetURL("a.test", "/fenced_frames/basic.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);

  GURL ad_url = https_server_->GetURL(
      "c.test", "/set-header?Supports-Loading-Mode: fenced-frame");
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          blink::TestInterestGroupBuilder(
              /*owner=*/test_origin,
              /*name=*/"cars")
              .SetBiddingUrl(https_server_->GetURL(
                  "a.test", "/interest_group/bidding_logic.js"))
              .SetTrustedBiddingSignalsUrl(https_server_->GetURL(
                  "a.test", "/interest_group/trusted_bidding_signals.json"))
              .SetTrustedBiddingSignalsKeys({{"key1"}})
              .SetAds({{{ad_url, R"({"ad":"metadata","here":[1,2]})"}}})
              .Build()));

  ASSERT_NO_FATAL_FAILURE(RunAuctionAndNavigateFencedFrame(
      ad_url, JsReplace(
                  R"({
seller: $1,
decisionLogicUrl: $2,
interestGroupBuyers: [$1],
auctionSignals: {x: 1},
sellerSignals: {yet: 'more', info: 1},
perBuyerSignals: {$1: {even: 'more', x: 4.5}}
                  })",
                  test_origin,
                  https_server_->GetURL("a.test",
                                        "/interest_group/decision_logic.js"))));

  // Check ResourceRequest structs of requests issued by the worklet process.
  const struct ExpectedRequest {
    GURL url;
    const char* accept_header;
    bool expect_trusted_params;
  } kExpectedRequests[] = {
      {https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
       "application/javascript", /*expect_trusted_params=*/true},
      {https_server_->GetURL(
           "a.test",
           "/interest_group/trusted_bidding_signals.json"
           "?hostname=a.test&keys=key1&interestGroupNames=cars"),
       "application/json", /*expect_trusted_params=*/true},
      {https_server_->GetURL("a.test", "/interest_group/decision_logic.js"),
       "application/javascript", /*expect_trusted_params=*/false},
  };
  for (const auto& expected_request : kExpectedRequests) {
    SCOPED_TRACE(expected_request.url);

    absl::optional<network::ResourceRequest> request =
        url_loader_monitor.GetRequestInfo(expected_request.url);
    ASSERT_TRUE(request);
    EXPECT_EQ(network::mojom::CredentialsMode::kOmit,
              request->credentials_mode);
    EXPECT_EQ(network::mojom::RedirectMode::kError, request->redirect_mode);
    EXPECT_EQ(test_origin, request->request_initiator);

    EXPECT_EQ(1u, request->headers.GetHeaderVector().size());
    std::string accept_value;
    ASSERT_TRUE(request->headers.GetHeader(net::HttpRequestHeaders::kAccept,
                                           &accept_value));
    EXPECT_EQ(expected_request.accept_header, accept_value);

    EXPECT_EQ(expected_request.expect_trusted_params,
              request->trusted_params.has_value());
    EXPECT_EQ(network::mojom::RequestMode::kNoCors, request->mode);
    if (request->trusted_params) {
      // Requests for interest-group provided URLs are cross-origin to the
      // publisher page, and set trusted params to use the right cache shard,
      // using a trusted URLLoaderFactory.
      const net::IsolationInfo& isolation_info =
          request->trusted_params->isolation_info;
      EXPECT_EQ(net::IsolationInfo::RequestType::kOther,
                isolation_info.request_type());
      url::Origin expected_origin = url::Origin::Create(expected_request.url);
      EXPECT_EQ(expected_origin, isolation_info.top_frame_origin());
      EXPECT_TRUE(isolation_info.site_for_cookies().IsNull());
      EXPECT_EQ(expected_origin, isolation_info.frame_origin());
    }
  }

  // Check ResourceRequest structs of report requests.
  const GURL kExpectedReportUrls[] = {
      https_server_->GetURL("a.test", "/echoall?report_seller"),
      https_server_->GetURL("a.test", "/echoall?report_bidder"),
  };
  for (const auto& expected_report_url : kExpectedReportUrls) {
    SCOPED_TRACE(expected_report_url);

    // Make sure the report URL was actually fetched over the network.
    WaitForUrl(expected_report_url);

    absl::optional<network::ResourceRequest> request =
        url_loader_monitor.WaitForUrl(expected_report_url);
    ASSERT_TRUE(request);
    EXPECT_EQ(network::mojom::CredentialsMode::kOmit,
              request->credentials_mode);
    EXPECT_EQ(network::mojom::RedirectMode::kError, request->redirect_mode);
    EXPECT_EQ(test_origin, request->request_initiator);

    EXPECT_TRUE(request->headers.IsEmpty());

    ASSERT_TRUE(request->trusted_params);
    const net::IsolationInfo& isolation_info =
        request->trusted_params->isolation_info;
    EXPECT_EQ(net::IsolationInfo::RequestType::kOther,
              isolation_info.request_type());
    EXPECT_TRUE(isolation_info.network_isolation_key().IsTransient());
    EXPECT_TRUE(isolation_info.site_for_cookies().IsNull());
  }

  // The two reporting requests should use different NIKs to prevent the
  // requests from being correlated.
  EXPECT_NE(url_loader_monitor.GetRequestInfo(kExpectedReportUrls[0])
                ->trusted_params->isolation_info.network_isolation_key(),
            url_loader_monitor.GetRequestInfo(kExpectedReportUrls[1])
                ->trusted_params->isolation_info.network_isolation_key());
}

// Runs auction just like test
// InterestGroupBrowserTest.RunAdAuctionWithSizeWithWinner, but load the winning
// ad in a fenced frame and verify the ad size.
IN_PROC_BROWSER_TEST_F(InterestGroupFencedFrameBrowserTest,
                       RunAdAuctionWithSizeWithWinner) {
  GURL test_url = https_server_->GetURL("a.test", "/fenced_frames/basic.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);
  GURL ad_url = https_server_->GetURL(
      "c.test", "/set-header?Supports-Loading-Mode: fenced-frame");

  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          blink::TestInterestGroupBuilder(
              /*owner=*/test_origin,
              /*name=*/"cars")
              .SetBiddingUrl(https_server_->GetURL(
                  "a.test", "/interest_group/bidding_logic_with_size.js"))
              .SetAds(/*ads=*/{{{ad_url, /*metadata=*/absl::nullopt,
                                 /*size_group=*/"group_1"}}})
              .SetAdSizes(
                  {{{"size_1",
                     blink::AdSize(100, blink::AdSize::LengthUnit::kScreenWidth,
                                   50, blink::AdSize::LengthUnit::kPixels)}}})
              .SetSizeGroups({{{"group_1", {"size_1"}}}})
              .Build()));

  std::string auction_config = JsReplace(
      R"({
          seller: $1,
          decisionLogicUrl: $2,
          interestGroupBuyers: [$1]
        })",
      test_origin,
      https_server_->GetURL("a.test", "/interest_group/decision_logic.js"));
  ASSERT_NO_FATAL_FAILURE(
      RunAuctionAndNavigateFencedFrame(ad_url, auction_config));

  // Verify the ad is loaded with the size specified in the winning bid.
  int screen_width = static_cast<int>(display::Screen::GetScreen()
                                          ->GetPrimaryDisplay()
                                          .GetSizeInPixel()
                                          .width());
  RenderFrameHost* ad_frame = GetFencedFrameRenderFrameHost(shell());
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  // Wait for 2 requestAnimationFrame calls to make things deterministic.
  // Without this, the fenced frame may end up with its default size 300px *
  // 150px. (Width * Height)
  ASSERT_TRUE(WaitForFencedFrameSizeFreeze(ad_frame));
  // Force layout.
  EXPECT_TRUE(
      ExecJs(ad_frame, "getComputedStyle(document.documentElement).width;"));
  EXPECT_EQ(EvalJs(ad_frame, "innerWidth").ExtractInt(), screen_width);
  EXPECT_EQ(EvalJs(ad_frame, "innerHeight").ExtractInt(), 50);
}

// TODO(crbug.com/1439980): Fix flaky test.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_RunAdAuctionWithAdComponentWithSize \
  DISABLED_RunAdAuctionWithAdComponentWithSize
#else
#define MAYBE_RunAdAuctionWithAdComponentWithSize \
  RunAdAuctionWithAdComponentWithSize
#endif
IN_PROC_BROWSER_TEST_F(InterestGroupFencedFrameBrowserTest,
                       MAYBE_RunAdAuctionWithAdComponentWithSize) {
  GURL test_url = https_server_->GetURL("a.test", "/fenced_frames/basic.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  GURL ad_component_url = https_server_->GetURL(
      "d.test", "/set-header?Supports-Loading-Mode: fenced-frame");

  GURL ad_url = https_server_->GetURL("c.test", "/fenced_frames/basic.html");
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          blink::TestInterestGroupBuilder(
              /*owner=*/url::Origin::Create(test_url),
              /*name=*/"cars")
              .SetBiddingUrl(https_server_->GetURL(
                  "a.test", "/interest_group/bidding_logic_with_size.js"))
              .SetAds(/*ads=*/{{{ad_url, /*metadata=*/absl::nullopt,
                                 /*size_group=*/"group_1"}}})
              .SetAdComponents({{{ad_component_url, /*metadata=*/absl::nullopt,
                                  /*size_group=*/"group_2"}}})
              .SetAdSizes(
                  {{{"size_1",
                     blink::AdSize(100, blink::AdSize::LengthUnit::kScreenWidth,
                                   50, blink::AdSize::LengthUnit::kPixels)},
                    {"size_2",
                     blink::AdSize(50, blink::AdSize::LengthUnit::kPixels, 25,
                                   blink::AdSize::LengthUnit::kPixels)}}})
              .SetSizeGroups(
                  {{{"group_1", {"size_1"}}, {"group_2", {"size_2"}}}})
              .Build()));

  ASSERT_NO_FATAL_FAILURE(RunAuctionAndNavigateFencedFrame(
      ad_url, JsReplace(
                  R"({
                      seller: $1,
                      decisionLogicUrl: $2,
                      interestGroupBuyers: [$1]
                    })",
                  url::Origin::Create(test_url),
                  https_server_->GetURL("a.test",
                                        "/interest_group/decision_logic.js"))));

  RenderFrameHost* ad_frame = GetFencedFrameRenderFrameHost(shell());

  // Verify the ad is loaded with the size specified in the winning bid.
  int screen_width = static_cast<int>(display::Screen::GetScreen()
                                          ->GetPrimaryDisplay()
                                          .GetSizeInPixel()
                                          .width());
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  // Wait for 2 requestAnimationFrame calls to make things deterministic.
  // Without this, the fenced frame may end up with its default size 300px *
  // 150px. (Width * Height)
  ASSERT_TRUE(WaitForFencedFrameSizeFreeze(ad_frame));
  // Force layout.
  EXPECT_TRUE(
      ExecJs(ad_frame, "getComputedStyle(document.documentElement).width;"));
  EXPECT_EQ(EvalJs(ad_frame, "innerWidth").ExtractInt(), screen_width);
  EXPECT_EQ(EvalJs(ad_frame, "innerHeight").ExtractInt(), 50);

  // Get the first component config from the fenced frame. Load it in the
  // nested fenced frame. The load should succeed.
  TestFrameNavigationObserver observer(GetFencedFrameRenderFrameHost(ad_frame));

  EXPECT_TRUE(ExecJs(ad_frame, R"(
        const configs = window.fence.getNestedConfigs();
        document.querySelector('fencedframe').config = configs[0];
      )"));

  WaitForFencedFrameNavigation(ad_component_url, ad_frame, observer);

  // Verify the ad component is loaded with the size specified in the winning
  // bid.
  RenderFrameHost* ad_component_frame = GetFencedFrameRenderFrameHost(ad_frame);
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  // Wait for 2 requestAnimationFrame calls to make things deterministic.
  // Without this, the fenced frame may end up with its default size 300px *
  // 150px. (Width * Height)
  ASSERT_TRUE(WaitForFencedFrameSizeFreeze(ad_component_frame));
  // Force layout.
  EXPECT_TRUE(ExecJs(ad_component_frame,
                     "getComputedStyle(document.documentElement).width;"));
  EXPECT_EQ(EvalJs(ad_component_frame, "innerWidth").ExtractInt(), 50);
  EXPECT_EQ(EvalJs(ad_component_frame, "innerHeight").ExtractInt(), 25);
}

IN_PROC_BROWSER_TEST_F(InterestGroupFencedFrameBrowserTest,
                       RunAdAuctionWithWinnerReplacedURN) {
  URLLoaderMonitor url_loader_monitor;

  GURL test_url = https_server_->GetURL("a.test", "/fenced_frames/basic.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);

  GURL ad_url = https_server_->GetURL(
      "c.test", "/set-header?Supports-Loading-Mode: %%LOADING_MODE%%");
  GURL expected_ad_url = https_server_->GetURL(
      "c.test", "/set-header?Supports-Loading-Mode: fenced-frame");
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          blink::TestInterestGroupBuilder(
              /*owner=*/test_origin,
              /*name=*/"cars")
              .SetBiddingUrl(https_server_->GetURL(
                  "a.test", "/interest_group/bidding_logic.js"))
              .SetTrustedBiddingSignalsUrl(https_server_->GetURL(
                  "a.test", "/interest_group/trusted_bidding_signals.json"))
              .SetTrustedBiddingSignalsKeys({{"key1"}})
              .SetAds({{{ad_url, R"({"ad":"metadata","here":[1,2]})"}}})
              .Build()));

  content::EvalJsResult urn_url_string = RunAuctionAndWait(
      JsReplace(
          R"({
seller: $1,
decisionLogicUrl: $2,
interestGroupBuyers: [$1],
auctionSignals: {x: 1},
sellerSignals: {yet: 'more', info: 1},
perBuyerSignals: {$1: {even: 'more', x: 4.5}}
                  })",
          test_origin,
          https_server_->GetURL("a.test", "/interest_group/decision_logic.js")),
      shell());
  ASSERT_TRUE(urn_url_string.value.is_string())
      << "Expected string, but got " << urn_url_string.value;

  GURL urn_url(urn_url_string.ExtractString());
  ASSERT_TRUE(urn_url.is_valid())
      << "URL is not valid: " << urn_url_string.ExtractString();
  EXPECT_EQ(url::kUrnScheme, urn_url.scheme_piece());

  EXPECT_TRUE(
      ReplaceInURNInJS(urn_url, {{"%%LOADING_MODE%%", "fenced-frame"}}));

  NavigateFencedFrameAndWait(urn_url, expected_ad_url, shell());
}

// Runs two ad auctions with fenced frames enabled. Both auctions should
// succeed and are then loaded in separate fenced frames. Both auctions try to
// leave the interest group, but only the one whose ad matches the joining
// origin should succeed.
IN_PROC_BROWSER_TEST_F(InterestGroupFencedFrameBrowserTest,
                       RunTwoAdAuctionWithWinnerLeaveGroup) {
  URLLoaderMonitor url_loader_monitor;

  GURL test_url = https_server_->GetURL(
      "a.test",
      base::StringPrintf(
          "/cross_site_iframe_factory.html?a.test(%s,%s)",
          base::EscapeUrlEncodedData(
              https_server_->GetURL("a.test", "/fenced_frames/basic.html")
                  .spec(),
              /*use_plus=*/false)
              .c_str(),
          base::EscapeUrlEncodedData(
              https_server_->GetURL("b.test", "/fenced_frames/basic.html")
                  .spec(),
              /*use_plus=*/false)
              .c_str()));
  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  RenderFrameHost* rfh1 =
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(rfh1);
  RenderFrameHost* rfh2 =
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 1);
  ASSERT_TRUE(rfh2);
  url::Origin test_origin = url::Origin::Create(test_url);

  AttachInterestGroupObserver();
  GURL ad_url = https_server_->GetURL(
      "b.test", "/set-header?Supports-Loading-Mode: fenced-frame");
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          blink::TestInterestGroupBuilder(
              /*owner=*/test_origin,
              /*name=*/"cars")
              .SetBiddingUrl(https_server_->GetURL(
                  "a.test",
                  // Using bidding_logic_stop_bidding_after_win.js ensures the
                  // "cars" interest group wins the first auction (whose
                  // leaveAdInterestGroup call succeeds).
                  "/interest_group/bidding_logic_stop_bidding_after_win.js"))
              .SetTrustedBiddingSignalsUrl(https_server_->GetURL(
                  "a.test", "/interest_group/trusted_bidding_signals.json"))
              .SetTrustedBiddingSignalsKeys({{"key1"}})
              .SetAds({{{ad_url, R"({"ad":"metadata","here":[1,2]})"}}})
              .Build(),
          rfh1));

  GURL ad_url2 = https_server_->GetURL(
      "a.test", "/set-header?Supports-Loading-Mode: fenced-frame");
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          blink::TestInterestGroupBuilder(
              /*owner=*/test_origin,
              /*name=*/"trucks")
              .SetBiddingUrl(https_server_->GetURL(
                  "a.test", "/interest_group/bidding_logic.js"))
              .SetTrustedBiddingSignalsUrl(https_server_->GetURL(
                  "a.test", "/interest_group/trusted_bidding_signals.json"))
              .SetTrustedBiddingSignalsKeys({{"key1"}})
              .SetAds({{{ad_url2, R"({"ad":"metadata","here":[1,2]})"}}})
              .Build(),
          rfh1));

  ASSERT_NO_FATAL_FAILURE(RunAuctionAndNavigateFencedFrame(
      ad_url,
      JsReplace(
          R"({
seller: $1,
decisionLogicUrl: $2,
interestGroupBuyers: [$1],
auctionSignals: {x: 1},
sellerSignals: {yet: 'more', info: 1},
perBuyerSignals: {$1: {even: 'more', x: 4.5}}
                  })",
          test_origin,
          https_server_->GetURL("a.test", "/interest_group/decision_logic.js")),
      rfh1));

  // InterestGroupAccessObserver should see the join and auction.
  WaitForAccessObserved({
      {TestInterestGroupObserver::kJoin, test_origin, "cars"},
      {TestInterestGroupObserver::kJoin, test_origin, "trucks"},
      {TestInterestGroupObserver::kLoaded, test_origin, "trucks"},
      {TestInterestGroupObserver::kLoaded, test_origin, "cars"},
      {TestInterestGroupObserver::kBid, test_origin, "cars"},
      {TestInterestGroupObserver::kBid, test_origin, "trucks"},
      {TestInterestGroupObserver::kWin, test_origin, "cars"},
  });

  // Try to leave the winning interest group, which should fail, since the ad is
  // on b.test, but the IG owner is a.test. Do the failed leave case first so
  // that subsequent WaitForAccessObserved() calls would likely catch an
  // unexpected leave event.
  EXPECT_EQ(nullptr, EvalJs(GetFencedFrameRenderFrameHost(rfh1),
                            "navigator.leaveAdInterestGroup()"));

  ASSERT_NO_FATAL_FAILURE(RunAuctionAndNavigateFencedFrame(
      ad_url2,
      JsReplace(
          R"({
seller: $1,
decisionLogicUrl: $2,
interestGroupBuyers: [$1],
auctionSignals: {x: 1},
sellerSignals: {yet: 'more', info: 1},
perBuyerSignals: {$1: {even: 'more', x: 4.5}}
                  })",
          test_origin,
          https_server_->GetURL("a.test", "/interest_group/decision_logic.js")),
      rfh2));

  // For the second auction, InterestGroupAccessObserver should see the two
  // groups loaded, but just the truck group win. Do this before the leave
  // attempt, as updating the data is potentially racy with the navigation
  // committing, so the leave event could appear out of order.
  WaitForAccessObserved(
      {{TestInterestGroupObserver::kLoaded, test_origin, "trucks"},
       {TestInterestGroupObserver::kLoaded, test_origin, "cars"},
       {TestInterestGroupObserver::kBid, test_origin, "trucks"},
       {TestInterestGroupObserver::kWin, test_origin, "trucks"}});

  // Try to leave the winning interest group, which should succeed this time. Do
  // it by calling Javascript directly instead of loading a page that does this
  // to avoid races with logging kBin or kWin.
  EXPECT_EQ(nullptr, EvalJs(GetFencedFrameRenderFrameHost(rfh2),
                            "navigator.leaveAdInterestGroup()"));
  WaitForAccessObserved(
      {{TestInterestGroupObserver::kLeave, test_origin, "trucks"}});

  // Only the "truck" interest group should have been left.
  auto groups = GetAllInterestGroups();
  ASSERT_EQ(1u, groups.size());
  EXPECT_EQ("cars", groups[0].name);
}

// Runs ad auction with fenced frames enabled. The auction should succeed and
// be loaded in a fenced frame. The displayed ad leaves the interest group
// from a nested iframe.
//
// TODO(crbug.com/1320438): Re-enable the test.
IN_PROC_BROWSER_TEST_F(InterestGroupFencedFrameBrowserTest,
                       RunAdAuctionWithWinnerNestedLeaveGroup) {
  URLLoaderMonitor url_loader_monitor;

  GURL test_url = https_server_->GetURL("a.test", "/fenced_frames/basic.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);

  AttachInterestGroupObserver();
  GURL inner_url = https_server_->GetURL(
      "a.test", "/set-header?Supports-Loading-Mode: fenced-frame");
  GURL ad_url = https_server_->GetURL(
      "b.test", "/fenced_frames/outer_inner_frame_as_param.html");
  GURL::Replacements rep;
  std::string query = "innerFrame=" + base::EscapeUrlEncodedData(
                                          inner_url.spec(), /*use_plus=*/false);
  rep.SetQueryStr(query);
  ad_url = ad_url.ReplaceComponents(rep);

  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          blink::TestInterestGroupBuilder(
              /*owner=*/test_origin,
              /*name=*/"cars")
              .SetBiddingUrl(https_server_->GetURL(
                  "a.test", "/interest_group/bidding_logic.js"))
              .SetTrustedBiddingSignalsUrl(https_server_->GetURL(
                  "a.test", "/interest_group/trusted_bidding_signals.json"))
              .SetTrustedBiddingSignalsKeys({{"key1"}})
              .SetAds({{{ad_url, R"({"ad":"metadata","here":[1,2]})"}}})
              .Build()));

  ASSERT_NO_FATAL_FAILURE(RunAuctionAndNavigateFencedFrame(
      ad_url, JsReplace(
                  R"({
seller: $1,
decisionLogicUrl: $2,
interestGroupBuyers: [$1],
auctionSignals: {x: 1},
sellerSignals: {yet: 'more', info: 1},
perBuyerSignals: {$1: {even: 'more', x: 4.5}}
                  })",
                  test_origin,
                  https_server_->GetURL("a.test",
                                        "/interest_group/decision_logic.js"))));

  // InterestGroupAccessObserver should see the join and auction.
  WaitForAccessObserved(
      {{TestInterestGroupObserver::kJoin, test_origin, "cars"},
       {TestInterestGroupObserver::kLoaded, test_origin, "cars"},
       {TestInterestGroupObserver::kBid, test_origin, "cars"},
       {TestInterestGroupObserver::kWin, test_origin, "cars"}});

  // Leave the interest group and wait to observe the event. Do this after the
  // above WaitForAccessObserved() call, as leaving is racy with recording the
  // result of an auction.
  EXPECT_EQ(nullptr, EvalJs(GetFencedFrameRenderFrameHost(web_contents())
                                ->child_at(0)
                                ->current_frame_host(),
                            "navigator.leaveAdInterestGroup()"));
  WaitForAccessObserved(
      {{TestInterestGroupObserver::kLeave, test_origin, "cars"}});

  // The ad should have left the interest group when the page was shown.
  EXPECT_EQ(0u, GetAllInterestGroups().size());
}

// Runs ad auction with fenced frames enabled. The auction should succeed and
// be loaded in a fenced frame. Then the fenced frame content performs a
// cross-origin navigation on itself, and the new document loads an iframe that
// is same-origin to the interest group. We leave the interest group from the
// iframe, and it should succeed.
IN_PROC_BROWSER_TEST_F(
    InterestGroupFencedFrameBrowserTest,
    RunAdAuctionWithWinnerLeaveGroupAfterRendererInitiatedNavigation) {
  URLLoaderMonitor url_loader_monitor;

  GURL test_url = https_server_->GetURL("a.test", "/fenced_frames/basic.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);

  AttachInterestGroupObserver();

  // First, load an ad urn-mapped to `test_url`.
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          blink::TestInterestGroupBuilder(
              /*owner=*/test_origin,
              /*name=*/"cars")
              .SetBiddingUrl(https_server_->GetURL(
                  "a.test", "/interest_group/bidding_logic.js"))
              .SetTrustedBiddingSignalsUrl(https_server_->GetURL(
                  "a.test", "/interest_group/trusted_bidding_signals.json"))
              .SetTrustedBiddingSignalsKeys({{"key1"}})
              .SetAds({{{test_url, R"({"ad":"metadata","here":[1,2]})"}}})
              .Build()));

  ASSERT_NO_FATAL_FAILURE(RunAuctionAndNavigateFencedFrame(
      test_url, JsReplace(
                    R"({
seller: $1,
decisionLogicUrl: $2,
interestGroupBuyers: [$1],
auctionSignals: {x: 1},
sellerSignals: {yet: 'more', info: 1},
perBuyerSignals: {$1: {even: 'more', x: 4.5}}
                  })",
                    test_origin,
                    https_server_->GetURL(
                        "a.test", "/interest_group/decision_logic.js"))));

  // Now perform a fenced frame content-initiated navigation to a cross-origin
  // document that will load a same-origin (to the mapped url) iframe that will
  // leave the interest group.
  GURL inner_url = https_server_->GetURL(
      "a.test", "/fenced_frames/ad_that_leaves_interest_group.html");
  GURL ad_url = https_server_->GetURL(
      "b.test", "/fenced_frames/outer_inner_frame_as_param.html");
  GURL::Replacements rep;
  std::string query = "innerFrame=" + base::EscapeUrlEncodedData(
                                          inner_url.spec(), /*use_plus=*/false);
  rep.SetQueryStr(query);
  ad_url = ad_url.ReplaceComponents(rep);

  RenderFrameHostImpl* ad_frame = GetFencedFrameRenderFrameHost(shell());
  TestFrameNavigationObserver observer(ad_frame);
  EXPECT_TRUE(ExecJs(ad_frame, JsReplace("document.location = $1;", ad_url)));
  observer.Wait();

  // Wait for the interest group to disappear.
  WaitForInterestGroupsSatisfying(
      test_origin,
      base::BindLambdaForTesting(
          [](const std::vector<StorageInterestGroup>& groups) -> bool {
            return groups.empty();
          }));
}

// Creates a Fenced Frame and then tries to use the leaveAdInterestGroup API.
// Leaving the interest group should silently fail.
IN_PROC_BROWSER_TEST_F(InterestGroupFencedFrameBrowserTest,
                       LeaveAdInterestGroupNoAuction) {
  URLLoaderMonitor url_loader_monitor;

  GURL test_url = https_server_->GetURL("a.test", "/fenced_frames/basic.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);

  GURL ad_url = https_server_->GetURL(
      "a.test", "/fenced_frames/ad_that_leaves_interest_group.html");
  AttachInterestGroupObserver();
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          blink::TestInterestGroupBuilder(
              /*owner=*/test_origin,
              /*name=*/"cars")
              .SetBiddingUrl(https_server_->GetURL(
                  "a.test", "/interest_group/bidding_logic.js"))
              .SetTrustedBiddingSignalsUrl(https_server_->GetURL(
                  "a.test", "/interest_group/trusted_bidding_signals.json"))
              .SetTrustedBiddingSignalsKeys({{"key1"}})
              .SetAds({{{ad_url, R"({"ad":"metadata","here":[1,2]})"}}})
              .Build()));

  // Navigate fenced frame with no ad.
  ASSERT_NO_FATAL_FAILURE(NavigateFencedFrameAndWait(ad_url, ad_url, shell()));

  // InterestGroupAccessObserver should see the join.
  WaitForAccessObserved(
      {{TestInterestGroupObserver::kJoin, test_origin, "cars"}});

  // The ad should not have left the interest group when the page was shown.
  EXPECT_EQ(1u, GetAllInterestGroups().size());
}

// Use different origins for publisher, bidder, and seller, and make sure
// everything works as expected.
IN_PROC_BROWSER_TEST_F(InterestGroupFencedFrameBrowserTest, CrossOrigin) {
  const char kPublisher[] = "a.test";
  const char kBidder[] = "b.test";
  const char kSeller[] = "c.test";
  const char kAd[] = "d.test";

  AttachInterestGroupObserver();

  GURL ad_url = https_server_->GetURL(
      kAd, "/set-header?Supports-Loading-Mode: fenced-frame");

  // Navigate to bidder site, and add an interest group.
  GURL bidder_url = https_server_->GetURL(kBidder, "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), bidder_url));
  url::Origin bidder_origin = url::Origin::Create(bidder_url);
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          blink::TestInterestGroupBuilder(
              /*owner=*/bidder_origin,
              /*name=*/"cars")
              .SetBiddingUrl(https_server_->GetURL(
                  kBidder, "/interest_group/bidding_logic.js"))
              .SetTrustedBiddingSignalsUrl(https_server_->GetURL(
                  kBidder, "/interest_group/trusted_bidding_signals.json"))
              .SetTrustedBiddingSignalsKeys({{"key1"}})
              .SetAds({{{ad_url, R"({"ad":"metadata","here":[1,2]})"}}})
              .Build()));

  // Navigate to publisher.
  ASSERT_TRUE(NavigateToURL(
      shell(), https_server_->GetURL(kPublisher, "/fenced_frames/basic.html")));

  GURL seller_logic_url = https_server_->GetURL(
      kSeller, "/interest_group/decision_logic_need_signals.js");
  // Register a seller script that only bids if the `trustedScoringSignals` are
  // successfully fetched.
  network_responder_->RegisterNetworkResponse(seller_logic_url.path(), R"(
function scoreAd(
    adMetadata, bid, auctionConfig, trustedScoringSignals, browserSignals) {
  // Reject bits if trustedScoringSignals is not received.
  if (trustedScoringSignals.renderUrl[browserSignals.renderUrl] === "foo")
    return bid;
  return 0;
}

function reportResult(
  auctionConfig, browserSignals) {
  sendReportTo(auctionConfig.seller + '/echoall?report_seller');
  return {
    'success': true,
    'signalsForWinner': {'signalForWinner': 1},
    'reportUrl': auctionConfig.seller + '/report_seller',
  };
}
)",
                                              "application/javascript");

  // Register seller signals with a value for `ad_url`.
  GURL seller_signals_url =
      https_server_->GetURL(kSeller, "/trusted_scoring_signals.json");
  network_responder_->RegisterNetworkResponse(
      seller_signals_url.path(),
      base::StringPrintf(R"({"renderUrls": {"%s": "foo"}})",
                         ad_url.spec().c_str()));

  // Run an auction with the scoring script. It should succeed.
  RunAuctionAndNavigateFencedFrame(
      ad_url, JsReplace(
                  R"(
{
  seller: $1,
  decisionLogicUrl: $2,
  trustedScoringSignalsUrl: $3,
  interestGroupBuyers: [$4],
}
                  )",
                  url::Origin::Create(seller_logic_url), seller_logic_url,
                  seller_signals_url, bidder_origin));

  WaitForAccessObserved({
      {TestInterestGroupObserver::kJoin, bidder_origin, "cars"},
      {TestInterestGroupObserver::kLoaded, bidder_origin, "cars"},
      {TestInterestGroupObserver::kBid, bidder_origin, "cars"},
      {TestInterestGroupObserver::kWin, bidder_origin, "cars"},
  });

  // Reporting urls should be fetched after an auction succeeded.
  WaitForUrl(https_server_->GetURL("/echoall?report_seller"));
  WaitForUrl(https_server_->GetURL("/echoall?report_bidder"));
  // Double-check that the trusted scoring signals URL was requested as well.
  WaitForUrl(https_server_->GetURL(base::StringPrintf(
      "/trusted_scoring_signals.json"
      "?hostname=a.test"
      "&renderUrls=%s",
      base::EscapeQueryParamValue(ad_url.spec(), /*use_plus=*/false).c_str())));
}

// Test that ad_components in an iframe ad are requested.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionWinnerWithComponents) {
  GURL test_url = https_server_->GetURL("a.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);
  GURL ad_url =
      https_server_->GetURL("c.test", "/fenced_frames/ad_with_components.html");
  GURL component_url = https_server_->GetURL("c.test", "/echo?component");
  AttachInterestGroupObserver();

  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          blink::TestInterestGroupBuilder(
              /*owner=*/test_origin,
              /*name=*/"cars")
              .SetBiddingUrl(https_server_->GetURL(
                  "a.test", "/interest_group/bidding_logic.js"))
              .SetTrustedBiddingSignalsUrl(https_server_->GetURL(
                  "a.test", "/interest_group/trusted_bidding_signals.json"))
              .SetTrustedBiddingSignalsKeys({{"key1"}})
              .SetAds({{{ad_url, R"({"ad":"metadata","here":[1,2]})"}}})
              .SetAdComponents(
                  {{{component_url, R"({"ad":"component metadata"})"}}})
              .Build()));

  std::string auction_config = JsReplace(
      R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
    auctionSignals: {x: 1},
    sellerSignals: {yet: 'more', info: 1},
    perBuyerSignals: {$1: {even: 'more', x: 4.5}}
                })",
      test_origin,
      https_server_->GetURL("a.test", "/interest_group/decision_logic.js"));
  RunAuctionAndWaitForURLAndNavigateIframe(auction_config, ad_url);

  WaitForAccessObserved({
      {TestInterestGroupObserver::kJoin, test_origin, "cars"},
      {TestInterestGroupObserver::kLoaded, test_origin, "cars"},
      {TestInterestGroupObserver::kBid, test_origin, "cars"},
      {TestInterestGroupObserver::kWin, test_origin, "cars"},
  });

  // Wait for the component to load.
  WaitForUrl(component_url);
}

// Make sure correct topFrameHostname is passed in. Check auctions from top
// frames, and iframes of various depth.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, TopFrameHostname) {
  // Buyer, seller, and iframe all use the same host.
  const char kOtherHost[] = "b.test";
  // Top frame host is unique.
  const char kTopFrameHost[] = "a.test";

  // Navigate to bidder site, and add an interest group.
  GURL other_url = https_server_->GetURL(kOtherHost, "/echo");
  url::Origin other_origin = url::Origin::Create(other_url);
  ASSERT_TRUE(NavigateToURL(shell(), other_url));
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          blink::TestInterestGroupBuilder(
              /*owner=*/other_origin,
              /*name=*/"cars")
              .SetBiddingUrl(https_server_->GetURL(
                  kOtherHost,
                  "/interest_group/bidding_logic_expect_top_frame_a_test.js"))
              .SetAds({{{GURL("https://example.com/render"),
                         /*metadata=*/absl::nullopt}}})
              .Build()));

  const struct {
    int depth;
    std::string top_frame_path;
    const char* seller_path;
  } kTestCases[] = {
      {0, "/echo", "/interest_group/decision_logic_expect_top_frame_a_test.js"},
      {1,
       base::StringPrintf("/cross_site_iframe_factory.html?a.test(%s)",
                          kOtherHost),
       "/interest_group/decision_logic_expect_top_frame_a_test.js"},
      {2,
       base::StringPrintf("/cross_site_iframe_factory.html?a.test(%s(%s))",
                          kOtherHost, kOtherHost),
       "/interest_group/decision_logic_expect_top_frame_a_test.js"},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.depth);

    // Navigate to publisher, with the cross-site iframe..
    ASSERT_TRUE(NavigateToURL(
        shell(),
        https_server_->GetURL(kTopFrameHost, test_case.top_frame_path)));

    RenderFrameHost* frame = web_contents()->GetPrimaryMainFrame();
    EXPECT_EQ(https_server_->GetOrigin(kTopFrameHost),
              frame->GetLastCommittedOrigin());
    for (int i = 0; i < test_case.depth; ++i) {
      frame = ChildFrameAt(frame, 0);
      ASSERT_TRUE(frame);
      EXPECT_EQ(other_origin, frame->GetLastCommittedOrigin());
    }

    // Run auction with a seller script with an "Access-Control-Allow-Origin"
    // header. The auction should succeed.
    GURL seller_logic_url =
        https_server_->GetURL(kOtherHost, test_case.seller_path);
    ASSERT_EQ("https://example.com/render",
              RunAuctionAndWaitForUrl(JsReplace(
                                          R"(
{
  seller: $1,
  decisionLogicUrl: $2,
  interestGroupBuyers: [$3],
  auctionSignals: {x: 1},
  sellerSignals: {yet: 'more', info: 1},
  perBuyerSignals: {$3: {even: 'more', x: 4.5}}
}
                                    )",
                                          url::Origin::Create(seller_logic_url),
                                          seller_logic_url, other_origin),
                                      frame));
  }
}

// Test running auctions in cross-site iframes, and loading the winner into a
// nested fenced frame.
IN_PROC_BROWSER_TEST_F(InterestGroupFencedFrameBrowserTest, Iframe) {
  // Use different hostnames for each participant.
  const char kTopFrameHost[] = "a.test";
  const char kBidderHost[] = "b.test";
  const char kSellerHost[] = "c.test";
  const char kIframeHost[] = "d.test";
  const char kAdHost[] = "ad.d.test";
  content_browser_client_->AddToAllowList(
      {url::Origin::Create(https_server_->GetURL(kIframeHost, "/"))});

  // Navigate to bidder site, and add an interest group.
  GURL bidder_url = https_server_->GetURL(kBidderHost, "/echo");
  url::Origin bidder_origin = url::Origin::Create(bidder_url);
  ASSERT_TRUE(NavigateToURL(shell(), bidder_url));

  GURL ad_url = https_server_->GetURL(
      kAdHost, "/set-header?Supports-Loading-Mode: fenced-frame");
  EXPECT_EQ(kSuccess,
            JoinInterestGroupAndVerify(
                /*owner=*/bidder_origin,
                /*name=*/"cars",
                /*priority=*/0.0,
                /*execution_mode=*/
                blink::InterestGroup::ExecutionMode::kCompatibilityMode,
                /*bidding_url=*/
                https_server_->GetURL(kBidderHost,
                                      "/interest_group/bidding_logic.js"),
                /*ads=*/{{{ad_url, /*metadata=*/absl::nullopt}}}));

  GURL main_frame_url = https_server_->GetURL(
      kTopFrameHost,
      base::StringPrintf(
          "/cross_site_iframe_factory.html?%s(%s)", kTopFrameHost,
          https_server_->GetURL(kIframeHost, "/fenced_frames/basic.html")
              .spec()
              .c_str()));
  ASSERT_TRUE(NavigateToURL(shell(), main_frame_url));

  RenderFrameHost* iframe =
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(iframe);
  EXPECT_EQ(kIframeHost, iframe->GetLastCommittedOrigin().host());

  GURL seller_logic_url =
      https_server_->GetURL(kSellerHost, "/interest_group/decision_logic.js");
  ASSERT_NO_FATAL_FAILURE(RunAuctionAndNavigateFencedFrame(
      ad_url,
      JsReplace(
          R"(
{
  seller: $1,
  decisionLogicUrl: $2,
  interestGroupBuyers: [$3]
}
          )",
          url::Origin::Create(seller_logic_url), seller_logic_url,
          bidder_origin),
      iframe));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionWithWinnerManyInterestGroups) {
  GURL test_url = https_server_->GetURL("a.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);
  GURL ad1_url =
      https_server_->GetURL("c.test", "/echo?stop_bidding_after_win");
  GURL ad2_url = https_server_->GetURL("c.test", "/echo?render_bikes");
  GURL ad3_url = https_server_->GetURL("c.test", "/echo?render_shoes");

  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          blink::TestInterestGroupBuilder(
              /*owner=*/test_origin,
              /*name=*/"cars")
              .SetBiddingUrl(https_server_->GetURL(
                  "a.test",
                  "/interest_group/bidding_logic_stop_bidding_after_win.js"))
              .SetAds({{{ad1_url, /*metadata=*/absl::nullopt}}})
              .Build()));
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          blink::TestInterestGroupBuilder(
              /*owner=*/test_origin,
              /*name=*/"bikes")
              .SetBiddingUrl(https_server_->GetURL(
                  "a.test", "/interest_group/bidding_logic.js"))
              .SetTrustedBiddingSignalsUrl(https_server_->GetURL(
                  "a.test", "/interest_group/trusted_bidding_signals.json"))
              .SetTrustedBiddingSignalsKeys({{"key1"}})
              .SetAds({{{ad2_url, /*metadata=*/absl::nullopt}}})
              .Build()));
  EXPECT_EQ(kSuccess, JoinInterestGroupAndVerify(
                          blink::TestInterestGroupBuilder(
                              /*owner=*/test_origin,
                              /*name=*/"shoes")
                              .SetBiddingUrl(https_server_->GetURL(
                                  "a.test", "/interest_group/bidding_logic.js"))
                              .SetAds({{{ad3_url, /*metadata=*/absl::nullopt}}})
                              .Build()));
  EXPECT_EQ(kSuccess, JoinInterestGroupAndVerify(
                          blink::TestInterestGroupBuilder(
                              /*owner=*/test_origin,
                              /*name=*/"jetskis")
                              .SetBiddingUrl(https_server_->GetURL(
                                  "a.test", "/interest_group/bidding_logic.js"))
                              .Build()));

  std::string auction_config = JsReplace(
      R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1, $3],
                })",
      test_origin,
      https_server_->GetURL("a.test", "/interest_group/decision_logic.js"));
  RunAuctionAndWaitForURLAndNavigateIframe(auction_config, ad1_url);

  // Seller and winning bidder should get reports, and other bidders shouldn't
  // get reports.
  WaitForUrl(https_server_->GetURL("/echoall?report_seller"));
  WaitForUrl(https_server_->GetURL(
      "/echoall?report_bidder_stop_bidding_after_win&cars"));
  base::AutoLock auto_lock(requests_lock_);
  EXPECT_FALSE(base::Contains(received_https_test_server_requests_,
                              https_server_->GetURL("/echoall?report_bidder")));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, RunAdAuctionAllGroupsLimited) {
  GURL test_url = https_server_->GetURL("a.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);
  GURL ad1_url = https_server_->GetURL("c.test", "/echo?render_cars");
  GURL ad2_url = https_server_->GetURL("c.test", "/echo?render_bikes");
  GURL ad3_url = https_server_->GetURL("c.test", "/echo?render_shoes");
  AttachInterestGroupObserver();

  EXPECT_EQ(kSuccess, JoinInterestGroupAndVerify(
                          blink::TestInterestGroupBuilder(
                              /*owner=*/test_origin,
                              /*name=*/"cars")
                              .SetPriority(2.3)
                              .SetBiddingUrl(https_server_->GetURL(
                                  "a.test", "/interest_group/bidding_logic.js"))
                              .SetAds({{{ad1_url, /*metadata=*/absl::nullopt}}})
                              .Build()));
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          blink::TestInterestGroupBuilder(
              /*owner=*/test_origin,
              /*name=*/"bikes")
              .SetPriority(2.2)
              .SetBiddingUrl(https_server_->GetURL(
                  "a.test", "/interest_group/bidding_logic.js"))
              .SetTrustedBiddingSignalsUrl(https_server_->GetURL(
                  "a.test", "/interest_group/trusted_bidding_signals.json"))
              .SetTrustedBiddingSignalsKeys({{"key1"}})
              .SetAds({{{ad2_url, /*metadata=*/absl::nullopt}}})
              .Build()));
  EXPECT_EQ(kSuccess, JoinInterestGroupAndVerify(
                          blink::TestInterestGroupBuilder(
                              /*owner=*/test_origin,
                              /*name=*/"shoes")
                              .SetPriority(2.1)
                              .SetBiddingUrl(https_server_->GetURL(
                                  "a.test", "/interest_group/bidding_logic.js"))
                              .SetAds({{{ad3_url, /*metadata=*/absl::nullopt}}})
                              .Build()));

  std::string auction_config = JsReplace(
      R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
    perBuyerGroupLimits: {'*': 1},
                })",
      test_origin,
      https_server_->GetURL("a.test", "/interest_group/decision_logic.js"));
  RunAuctionAndWaitForURLAndNavigateIframe(auction_config, ad1_url);

  WaitForAccessObserved({
      {TestInterestGroupObserver::kJoin, test_origin, "cars"},
      {TestInterestGroupObserver::kJoin, test_origin, "bikes"},
      {TestInterestGroupObserver::kJoin, test_origin, "shoes"},
      {TestInterestGroupObserver::kLoaded, test_origin, "shoes"},
      {TestInterestGroupObserver::kLoaded, test_origin, "bikes"},
      {TestInterestGroupObserver::kLoaded, test_origin, "cars"},
      {TestInterestGroupObserver::kBid, test_origin, "cars"},
      {TestInterestGroupObserver::kWin, test_origin, "cars"},
  });
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, RunAdAuctionOneGroupLimited) {
  GURL test_url = https_server_->GetURL("a.test", "/page_with_iframe.html");
  GURL test_url2 = https_server_->GetURL("b.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);
  url::Origin test_origin2 = url::Origin::Create(test_url2);
  GURL ad1_url = https_server_->GetURL("c.test", "/echo?render_cars");
  GURL ad2_url = https_server_->GetURL("c.test", "/echo?render_bikes");
  GURL ad3_url = https_server_->GetURL("c.test", "/echo?render_shoes");
  AttachInterestGroupObserver();

  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          blink::TestInterestGroupBuilder(
              /*owner=*/test_origin,
              /*name=*/"cars")
              .SetPriority(3)
              .SetBiddingUrl(https_server_->GetURL(
                  "a.test",
                  "/interest_group/bidding_logic_stop_bidding_after_win.js"))
              .SetAds({{{ad1_url, /*metadata=*/absl::nullopt}}})
              .Build()));
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          blink::TestInterestGroupBuilder(
              /*owner=*/test_origin,
              /*name=*/"bikes")
              .SetPriority(2)
              .SetBiddingUrl(https_server_->GetURL(
                  "a.test", "/interest_group/bidding_logic.js"))
              .SetTrustedBiddingSignalsUrl(https_server_->GetURL(
                  "a.test", "/interest_group/trusted_bidding_signals.json"))
              .SetTrustedBiddingSignalsKeys({{"key1"}})
              .SetAds({{{ad2_url, /*metadata=*/absl::nullopt}}})
              .Build()));
  EXPECT_EQ(kSuccess, JoinInterestGroupAndVerify(
                          blink::TestInterestGroupBuilder(
                              /*owner=*/test_origin,
                              /*name=*/"shoes")
                              .SetPriority(1)
                              .SetBiddingUrl(https_server_->GetURL(
                                  "a.test", "/interest_group/bidding_logic.js"))
                              .SetAds({{{ad3_url, /*metadata=*/absl::nullopt}}})
                              .Build()));

  ASSERT_TRUE(NavigateToURL(shell(), test_url2));
  EXPECT_EQ(kSuccess, JoinInterestGroupAndVerify(
                          blink::TestInterestGroupBuilder(
                              /*owner=*/test_origin2,
                              /*name=*/"cars")
                              .SetPriority(3)
                              .SetBiddingUrl(https_server_->GetURL(
                                  "b.test", "/interest_group/bidding_logic.js"))
                              .SetAds({{{ad1_url, /*metadata=*/absl::nullopt}}})
                              .Build()));
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          blink::TestInterestGroupBuilder(
              /*owner=*/test_origin2,
              /*name=*/"bikes")
              .SetPriority(2)
              .SetBiddingUrl(https_server_->GetURL(
                  "b.test", "/interest_group/bidding_logic.js"))
              .SetTrustedBiddingSignalsUrl(https_server_->GetURL(
                  "b.test", "/interest_group/trusted_bidding_signals.json"))
              .SetTrustedBiddingSignalsKeys({{"key1"}})
              .SetAds({{{ad2_url, /*metadata=*/absl::nullopt}}})
              .Build()));
  EXPECT_EQ(kSuccess, JoinInterestGroupAndVerify(
                          blink::TestInterestGroupBuilder(
                              /*owner=*/test_origin2,
                              /*name=*/"shoes")
                              .SetPriority(1)
                              .SetBiddingUrl(https_server_->GetURL(
                                  "b.test", "/interest_group/bidding_logic.js"))
                              .SetAds({{{ad3_url, /*metadata=*/absl::nullopt}}})
                              .Build()));
  std::string auction_config = JsReplace(
      R"({
    seller: $3,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1, $3],
    perBuyerGroupLimits: {$1: 1, '*': 2},
                })",
      test_origin,
      https_server_->GetURL("b.test", "/interest_group/decision_logic.js"),
      test_origin2);
  RunAuctionAndWaitForURLAndNavigateIframe(auction_config, ad1_url);

  WaitForAccessObserved({
      {TestInterestGroupObserver::kJoin, test_origin, "cars"},
      {TestInterestGroupObserver::kJoin, test_origin, "bikes"},
      {TestInterestGroupObserver::kJoin, test_origin, "shoes"},
      {TestInterestGroupObserver::kJoin, test_origin2, "cars"},
      {TestInterestGroupObserver::kJoin, test_origin2, "bikes"},
      {TestInterestGroupObserver::kJoin, test_origin2, "shoes"},
      {TestInterestGroupObserver::kLoaded, test_origin, "shoes"},
      {TestInterestGroupObserver::kLoaded, test_origin, "bikes"},
      {TestInterestGroupObserver::kLoaded, test_origin, "cars"},
      {TestInterestGroupObserver::kLoaded, test_origin2, "shoes"},
      {TestInterestGroupObserver::kLoaded, test_origin2, "bikes"},
      {TestInterestGroupObserver::kLoaded, test_origin2, "cars"},
      {TestInterestGroupObserver::kBid, test_origin, "cars"},
      {TestInterestGroupObserver::kBid, test_origin2, "bikes"},
      {TestInterestGroupObserver::kBid, test_origin2, "cars"},
      {TestInterestGroupObserver::kWin, test_origin, "cars"},
  });
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionOneGroupHighLimit) {
  GURL test_url = https_server_->GetURL("a.test", "/page_with_iframe.html");
  GURL test_url2 = https_server_->GetURL("b.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);
  url::Origin test_origin2 = url::Origin::Create(test_url2);
  GURL ad1_url = https_server_->GetURL("c.test", "/echo?render_cars");
  GURL ad2_url = https_server_->GetURL("c.test", "/echo?render_bikes");
  GURL ad3_url = https_server_->GetURL("c.test", "/echo?render_shoes");
  AttachInterestGroupObserver();

  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          blink::TestInterestGroupBuilder(
              /*owner=*/test_origin,
              /*name=*/"cars")
              .SetPriority(3)
              .SetBiddingUrl(https_server_->GetURL(
                  "a.test",
                  "/interest_group/bidding_logic_stop_bidding_after_win.js"))
              .SetAds({{{ad1_url, /*metadata=*/absl::nullopt}}})
              .Build()));
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          blink::TestInterestGroupBuilder(
              /*owner=*/test_origin,
              /*name=*/"bikes")
              .SetPriority(2)
              .SetBiddingUrl(https_server_->GetURL(
                  "a.test", "/interest_group/bidding_logic.js"))
              .SetTrustedBiddingSignalsUrl(https_server_->GetURL(
                  "a.test", "/interest_group/trusted_bidding_signals.json"))
              .SetTrustedBiddingSignalsKeys({{"key1"}})
              .SetAds({{{ad2_url, /*metadata=*/absl::nullopt}}})
              .Build()));
  EXPECT_EQ(kSuccess, JoinInterestGroupAndVerify(
                          blink::TestInterestGroupBuilder(
                              /*owner=*/test_origin,
                              /*name=*/"shoes")
                              .SetPriority(1)
                              .SetBiddingUrl(https_server_->GetURL(
                                  "a.test", "/interest_group/bidding_logic.js"))
                              .SetAds({{{ad3_url, /*metadata=*/absl::nullopt}}})
                              .Build()));

  ASSERT_TRUE(NavigateToURL(shell(), test_url2));
  EXPECT_EQ(kSuccess, JoinInterestGroupAndVerify(
                          blink::TestInterestGroupBuilder(
                              /*owner=*/test_origin2,
                              /*name=*/"cars")
                              .SetPriority(3)
                              .SetBiddingUrl(https_server_->GetURL(
                                  "b.test", "/interest_group/bidding_logic.js"))
                              .SetAds({{{ad1_url, /*metadata=*/absl::nullopt}}})
                              .Build()));
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          blink::TestInterestGroupBuilder(
              /*owner=*/test_origin2,
              /*name=*/"bikes")
              .SetPriority(2)
              .SetBiddingUrl(https_server_->GetURL(
                  "b.test", "/interest_group/bidding_logic.js"))
              .SetTrustedBiddingSignalsUrl(https_server_->GetURL(
                  "b.test", "/interest_group/trusted_bidding_signals.json"))
              .SetTrustedBiddingSignalsKeys({{"key1"}})
              .SetAds({{{ad2_url, /*metadata=*/absl::nullopt}}})
              .Build()));
  EXPECT_EQ(kSuccess, JoinInterestGroupAndVerify(
                          blink::TestInterestGroupBuilder(
                              /*owner=*/test_origin2,
                              /*name=*/"shoes")
                              .SetPriority(1)
                              .SetBiddingUrl(https_server_->GetURL(
                                  "b.test", "/interest_group/bidding_logic.js"))
                              .SetAds({{{ad3_url, /*metadata=*/absl::nullopt}}})
                              .Build()));
  std::string auction_config = JsReplace(
      R"({
    seller: $3,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1, $3],
    perBuyerGroupLimits: {$3: 3, '*': 1},
                })",
      test_origin,
      https_server_->GetURL("b.test", "/interest_group/decision_logic.js"),
      test_origin2);
  RunAuctionAndWaitForURLAndNavigateIframe(auction_config, ad1_url);

  WaitForAccessObserved({
      {TestInterestGroupObserver::kJoin, test_origin, "cars"},
      {TestInterestGroupObserver::kJoin, test_origin, "bikes"},
      {TestInterestGroupObserver::kJoin, test_origin, "shoes"},
      {TestInterestGroupObserver::kJoin, test_origin2, "cars"},
      {TestInterestGroupObserver::kJoin, test_origin2, "bikes"},
      {TestInterestGroupObserver::kJoin, test_origin2, "shoes"},
      {TestInterestGroupObserver::kLoaded, test_origin, "shoes"},
      {TestInterestGroupObserver::kLoaded, test_origin, "bikes"},
      {TestInterestGroupObserver::kLoaded, test_origin, "cars"},
      {TestInterestGroupObserver::kLoaded, test_origin2, "shoes"},
      {TestInterestGroupObserver::kLoaded, test_origin2, "bikes"},
      {TestInterestGroupObserver::kLoaded, test_origin2, "cars"},
      {TestInterestGroupObserver::kBid, test_origin, "cars"},
      {TestInterestGroupObserver::kBid, test_origin2, "bikes"},
      {TestInterestGroupObserver::kBid, test_origin2, "cars"},
      {TestInterestGroupObserver::kBid, test_origin2, "shoes"},
      {TestInterestGroupObserver::kWin, test_origin, "cars"},
  });
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionGroupLimitRandomized) {
  GURL test_url = https_server_->GetURL("a.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);
  GURL ad_url = https_server_->GetURL("c.test", "/echo?render_ad");

  std::vector<std::pair<std::string, double>> interest_groups = {
      {"cars", 3},
      {"motorcycles", 2},
      {"bikes", 2},
      {"shoes", 1},
      {"scooters", 2}};
  for (const auto& g : interest_groups) {
    EXPECT_EQ(
        kSuccess,
        JoinInterestGroupAndVerify(
            blink::TestInterestGroupBuilder(
                /*owner=*/test_origin,
                /*name=*/g.first)
                .SetPriority(g.second)
                .SetBiddingUrl(https_server_->GetURL(
                    "a.test",
                    "/interest_group/bidding_logic_stop_bidding_after_win.js"))
                .SetAds({{{ad_url, /*metadata=*/absl::nullopt}}})
                .Build()));
  }
  std::string auction_config = JsReplace(
      R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
    perBuyerGroupLimits: {'*': 3},
                })",
      test_origin,
      https_server_->GetURL("a.test", "/interest_group/decision_logic.js"));

  std::vector<GURL> expected_urls = {
      https_server_->GetURL(
          "a.test", "/echoall?report_bidder_stop_bidding_after_win&cars"),
      https_server_->GetURL(
          "a.test",
          "/echoall?report_bidder_stop_bidding_after_win&motorcycles"),
      https_server_->GetURL(
          "a.test", "/echoall?report_bidder_stop_bidding_after_win&bikes"),
      https_server_->GetURL(
          "a.test", "/echoall?report_bidder_stop_bidding_after_win&scooters"),
  };

  while (!HasServerSeenUrls(expected_urls)) {
    EvalJsResult result = RunAuctionAndWait(auction_config);

    // Some auctions will have no winner, depending on which interest groups
    // were chosen to participate. No need to do anything more for those.
    if (result.value.is_none())
      continue;

    // For other auctions, navigate iframe to winning URN to trigger reports.
    // This should happen exactly 4 times, so shouldn't slow the test down too
    // much.
    NavigateIframeAndCheckURL(web_contents(), GURL(result.ExtractString()),
                              ad_url);
  }
  EXPECT_FALSE(HasServerSeenUrl(https_server_->GetURL(
      "a.test", "/echoall?report_bidder_stop_bidding_after_win&shoes")));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, RunAdAuctionMultipleAuctions) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  const url::Origin origin = url::Origin::Create(test_url);

  GURL ad1_url =
      https_server_->GetURL("c.test", "/echo?stop_bidding_after_win");
  GURL ad2_url = https_server_->GetURL("c.test", "/echo?render_shoes");

  // This group will win if it has never won an auction.
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          blink::TestInterestGroupBuilder(
              /*owner=*/origin,
              /*name=*/"cars")
              .SetBiddingUrl(https_server_->GetURL(
                  "a.test",
                  "/interest_group/bidding_logic_stop_bidding_after_win.js"))
              .SetAds({{{ad1_url, R"({"ad":"metadata","here":[1,2]})"}}})
              .Build()));

  GURL test_url2 = https_server_->GetURL("b.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url2));
  const url::Origin origin2 = url::Origin::Create(test_url2);
  // This group will win if the other interest group has won an auction.
  EXPECT_EQ(kSuccess, JoinInterestGroupAndVerify(
                          blink::TestInterestGroupBuilder(
                              /*owner=*/origin2,
                              /*name=*/"shoes")
                              .SetBiddingUrl(https_server_->GetURL(
                                  "b.test", "/interest_group/bidding_logic.js"))
                              .SetAds({{{ad2_url, /*metadata=*/absl::nullopt}}})
                              .Build()));

  // Both owners have one interest group in storage, and both interest groups
  // have no `prev_wins`.
  std::vector<StorageInterestGroup> storage_interest_groups =
      GetInterestGroupsForOwner(origin);
  EXPECT_EQ(storage_interest_groups.size(), 1u);
  EXPECT_EQ(
      storage_interest_groups.front().bidding_browser_signals->prev_wins.size(),
      0u);
  EXPECT_EQ(storage_interest_groups.front().bidding_browser_signals->bid_count,
            0);
  std::vector<StorageInterestGroup> storage_interest_groups2 =
      GetInterestGroupsForOwner(origin2);
  EXPECT_EQ(storage_interest_groups2.size(), 1u);
  EXPECT_EQ(storage_interest_groups2.front()
                .bidding_browser_signals->prev_wins.size(),
            0u);
  EXPECT_EQ(storage_interest_groups2.front().bidding_browser_signals->bid_count,
            0);

  // Start observer after joins.
  AttachInterestGroupObserver();

  std::string auction_config = JsReplace(
      R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1, $3],
  })",
      origin2,
      https_server_->GetURL("b.test", "/interest_group/decision_logic.js"),
      origin);
  // Run an ad auction. Interest group cars of owner `test_url` wins.
  RunAuctionAndWaitForURLAndNavigateIframe(auction_config, ad1_url);
  // Wait for interest groups to be updated. Interest groups are updated
  // during/after commit, so this test is potentially racy without this.
  WaitForAccessObserved({{TestInterestGroupObserver::kLoaded, origin2, "shoes"},
                         {TestInterestGroupObserver::kLoaded, origin, "cars"},
                         {TestInterestGroupObserver::kBid, origin, "cars"},
                         {TestInterestGroupObserver::kBid, origin2, "shoes"},
                         {TestInterestGroupObserver::kWin, origin, "cars"}});

  // `prev_wins` of `test_url`'s interest group cars is updated in storage.
  storage_interest_groups = GetInterestGroupsForOwner(origin);
  storage_interest_groups2 = GetInterestGroupsForOwner(origin2);
  // Remove the above two loads from the observer.
  WaitForAccessObserved(
      {{TestInterestGroupObserver::kLoaded, origin, "cars"},
       {TestInterestGroupObserver::kLoaded, origin2, "shoes"}});
  EXPECT_EQ(
      storage_interest_groups.front().bidding_browser_signals->prev_wins.size(),
      1u);
  EXPECT_EQ(storage_interest_groups2.front()
                .bidding_browser_signals->prev_wins.size(),
            0u);
  EXPECT_EQ(
      storage_interest_groups.front()
          .bidding_browser_signals->prev_wins.front()
          ->ad_json,
      JsReplace(
          R"({"render_url":$1,"metadata":{"ad":"metadata","here":[1,2]}})",
          ad1_url));
  EXPECT_EQ(storage_interest_groups.front().bidding_browser_signals->bid_count,
            1);
  EXPECT_EQ(storage_interest_groups2.front().bidding_browser_signals->bid_count,
            1);

  // Run auction again. Interest group shoes of owner `test_url2` wins.
  RunAuctionAndWaitForURLAndNavigateIframe(auction_config, ad2_url);
  // Need to wait again.
  WaitForAccessObserved({{TestInterestGroupObserver::kLoaded, origin2, "shoes"},
                         {TestInterestGroupObserver::kLoaded, origin, "cars"},
                         {TestInterestGroupObserver::kBid, origin2, "shoes"},
                         {TestInterestGroupObserver::kWin, origin2, "shoes"}});

  // `test_url2`'s interest group shoes has one `prev_wins` in storage.
  storage_interest_groups = GetInterestGroupsForOwner(origin);
  storage_interest_groups2 = GetInterestGroupsForOwner(origin2);
  // Remove the above two loads from the observer.
  WaitForAccessObserved(
      {{TestInterestGroupObserver::kLoaded, origin, "cars"},
       {TestInterestGroupObserver::kLoaded, origin2, "shoes"}});
  EXPECT_EQ(
      storage_interest_groups.front().bidding_browser_signals->prev_wins.size(),
      1u);
  EXPECT_EQ(storage_interest_groups2.front()
                .bidding_browser_signals->prev_wins.size(),
            1u);
  EXPECT_EQ(storage_interest_groups2.front()
                .bidding_browser_signals->prev_wins.front()
                ->ad_json,
            JsReplace(R"({"render_url":$1})", ad2_url));
  // First interest group didn't bid this time.
  EXPECT_EQ(storage_interest_groups.front().bidding_browser_signals->bid_count,
            1);
  EXPECT_EQ(storage_interest_groups2.front().bidding_browser_signals->bid_count,
            2);

  // Run auction third time, and only interest group "shoes" bids this time.
  auction_config = JsReplace(
      R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
                })",
      origin2,
      https_server_->GetURL("b.test", "/interest_group/decision_logic.js"));
  RunAuctionAndWaitForURLAndNavigateIframe(auction_config, ad2_url);
  // Need to wait again.
  WaitForAccessObserved({
      {TestInterestGroupObserver::kLoaded, origin2, "shoes"},
      {TestInterestGroupObserver::kBid, origin2, "shoes"},
      {TestInterestGroupObserver::kWin, origin2, "shoes"},
  });

  // `test_url2`'s interest group shoes has two `prev_wins` in storage.
  storage_interest_groups = GetInterestGroupsForOwner(origin);
  storage_interest_groups2 = GetInterestGroupsForOwner(origin2);
  EXPECT_EQ(
      storage_interest_groups.front().bidding_browser_signals->prev_wins.size(),
      1u);
  EXPECT_EQ(storage_interest_groups2.front()
                .bidding_browser_signals->prev_wins.size(),
            2u);
  EXPECT_EQ(storage_interest_groups2.front()
                .bidding_browser_signals->prev_wins.back()
                ->ad_json,
            JsReplace(R"({"render_url":$1})", ad2_url));
  // First interest group didn't bid this time.
  EXPECT_EQ(storage_interest_groups.front().bidding_browser_signals->bid_count,
            1);
  EXPECT_EQ(storage_interest_groups2.front().bidding_browser_signals->bid_count,
            3);
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, ReportingMultipleAuctions) {
  URLLoaderMonitor url_loader_monitor;

  GURL test_url_a = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url_a));
  const url::Origin origin_a = url::Origin::Create(test_url_a);

  GURL ad1_url =
      https_server_->GetURL("c.test", "/echo?stop_bidding_after_win");
  GURL ad2_url = https_server_->GetURL("c.test", "/echo?render_shoes");

  // This group will win if it has never won an auction.
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          blink::TestInterestGroupBuilder(
              /*owner=*/origin_a,
              /*name=*/"cars")
              .SetBiddingUrl(https_server_->GetURL(
                  "a.test",
                  "/interest_group/bidding_logic_stop_bidding_after_win.js"))
              .SetAds({{{ad1_url, R"({"ad":"metadata","here":[1,2]})"}}})
              .Build()));

  GURL test_url_b = https_server_->GetURL("b.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url_b));
  const url::Origin origin_b = url::Origin::Create(test_url_b);
  // This group will win if the other interest group has won an auction.
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          blink::TestInterestGroupBuilder(
              /*owner=*/origin_b,
              /*name=*/"shoes")
              .SetBiddingUrl(https_server_->GetURL(
                  "b.test",
                  "/interest_group/bidding_logic_with_debugging_report.js"))
              .SetAds({{{ad2_url, /*metadata=*/absl::nullopt}}})
              .Build()));

  std::string auction_config = JsReplace(
      R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1, $3],
  })",
      origin_b,
      https_server_->GetURL("b.test", "/interest_group/decision_logic.js"),
      origin_a);
  // Setting a small reporting interval to run the test faster.
  manager_->set_reporting_interval_for_testing(base::Milliseconds(1));

  // Run an ad auction. Interest group cars of owner `test_url_a` wins.
  RunAuctionAndWaitForURLAndNavigateIframe(auction_config, ad1_url);

  // Wait for database to be updated with the win, which may happen after the
  // auction completes.
  WaitForInterestGroupsSatisfying(
      origin_a,
      base::BindLambdaForTesting(
          [](const std::vector<StorageInterestGroup>& groups) -> bool {
            EXPECT_EQ(1u, groups.size());
            return groups[0].bidding_browser_signals->prev_wins.size() == 1u;
          }));

  // Run auction again on the same page. Interest group shoes of owner
  // `test_url2` wins.
  auction_config = JsReplace(
      R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1, $3],
  })",
      origin_b,
      https_server_->GetURL("b.test", "/interest_group/decision_logic.js"),
      origin_a);
  RunAuctionAndWaitForURLAndNavigateIframe(auction_config, ad2_url);

  // Wait for database to be updated with the win, which may happen after the
  // auction completes.
  WaitForInterestGroupsSatisfying(
      origin_b,
      base::BindLambdaForTesting(
          [](const std::vector<StorageInterestGroup>& groups) -> bool {
            EXPECT_EQ(1u, groups.size());
            return groups[0].bidding_browser_signals->prev_wins.size() == 1u;
          }));

  // Run the third auction on another page c.test, and only interest group
  // "shoes" of c.test bids this time.
  GURL test_url_c = https_server_->GetURL("c.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url_c));
  const url::Origin origin_c = url::Origin::Create(test_url_c);

  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          blink::TestInterestGroupBuilder(
              /*owner=*/origin_c,
              /*name=*/"cars")
              .SetBiddingUrl(https_server_->GetURL(
                  "c.test",
                  "/interest_group/bidding_logic_with_debugging_report.js"))
              .SetAds({{{ad2_url, /*metadata=*/absl::nullopt}}})
              .Build()));

  auction_config = JsReplace(
      R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
    sellerSignals: {reportTo: $3},
                })",
      origin_c,
      https_server_->GetURL(
          "c.test",
          "/interest_group/decision_logic_report_to_seller_signals.js"),
      https_server_->GetURL("c.test", "/echoall?report_seller/cars"));
  RunAuctionAndWaitForURLAndNavigateIframe(auction_config, ad2_url);

  // Check ResourceRequest structs of report requests.
  // The URLs must not have the same path with different hostnames, because
  // WaitForUrl() always replaces hostnames with "127.0.0.1", thus only waits
  // for the first URL among URLs with the same path.
  const struct ExpectedReportRequest {
    GURL url;
    url::Origin request_initiator;
  } kExpectedReportRequests[] = {
      // First auction's seller's ReportResult() URL.
      {https_server_->GetURL("b.test", "/echoall?report_seller"), origin_b},
      // First auction's winning bidder's ReportWin() URL.
      {https_server_->GetURL(
           "a.test", "/echoall?report_bidder_stop_bidding_after_win&cars"),
       origin_b},
      // First auction's debugging loss report URL from bidder.
      {https_server_->GetURL("b.test", "/echo?bidder_debug_report_loss/shoes"),
       origin_b},

      // Second auction's seller's ReportResult() URL. Although this URL is the
      // second time requesting this URL, this test does not confirm that we
      // requested the URL twice unfortunately.
      // TODO(qingxinwu): Update the test fixture's use of RequestMonitor
      // instead of URLLoaderMonitor to handle duplicate URLs.
      {https_server_->GetURL("b.test", "/echoall?report_seller"), origin_b},
      // Second auction's winning bidder's ReportWin() URL.
      {https_server_->GetURL("b.test", "/echoall?report_bidder/shoes"),
       origin_b},
      // Second auction's debugging win report URL from bidder.
      {https_server_->GetURL("b.test", "/echo?bidder_debug_report_win/shoes"),
       origin_b},

      // Third auction's seller's ReportResult() URL.
      {https_server_->GetURL("c.test", "/echoall?report_seller/cars"),
       origin_c},
      // Third auction's winning bidder's ReportWin() URL.
      {https_server_->GetURL("c.test", "/echoall?report_bidder/cars"),
       origin_c},
      // Third auction's debugging win report URL from seller.
      {https_server_->GetURL("c.test",
                             "/echoall?report_seller/cars_debug_win_report"),
       origin_c},
      // Third auction's debugging win report URL from bidder.
      {https_server_->GetURL("c.test", "/echo?bidder_debug_report_win/cars"),
       origin_c}};

  for (const auto& expected_report_request : kExpectedReportRequests) {
    SCOPED_TRACE(expected_report_request.url);

    // Make sure the report URL was actually fetched over the network.
    WaitForUrl(expected_report_request.url);

    absl::optional<network::ResourceRequest> request =
        url_loader_monitor.WaitForUrl(expected_report_request.url);
    ASSERT_TRUE(request);
    EXPECT_EQ(network::mojom::CredentialsMode::kOmit,
              request->credentials_mode);
    EXPECT_EQ(network::mojom::RedirectMode::kError, request->redirect_mode);
    EXPECT_EQ(expected_report_request.request_initiator,
              request->request_initiator);

    EXPECT_TRUE(request->headers.IsEmpty());

    ASSERT_TRUE(request->trusted_params);
    const net::IsolationInfo& isolation_info =
        request->trusted_params->isolation_info;
    EXPECT_EQ(net::IsolationInfo::RequestType::kOther,
              isolation_info.request_type());
    EXPECT_TRUE(isolation_info.network_isolation_key().IsTransient());
    EXPECT_TRUE(isolation_info.site_for_cookies().IsNull());
  }
}

// Adding an interest group and then immediately running the ad auction, without
// waiting in between, should always work because although adding the interest
// group is async (and intentionally without completion notification), it should
// complete before the auction runs.
//
// On regression, this test will likely only fail with very low frequency.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       AddInterestGroupRunAuctionWithWinnerWithoutWaiting) {
  GURL test_url = https_server_->GetURL("a.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);
  GURL ad_url = https_server_->GetURL("c.test", "/echo?render_cars");
  const char kName[] = "cars";

  // All joinAdInterestGroup wrapper calls wait for the returned promise to
  // complete. Inline the call to avoid waiting.
  EXPECT_EQ(
      "done",
      EvalJs(shell(),
             JsReplace(
                 R"(
(function() {
  navigator.joinAdInterestGroup(
    {
      name: $1,
      owner: $2,
      biddingLogicUrl: $3,
      ads: $4
    },
    /*joinDurationSec=*/ 300);
  return 'done';
})())",
                 kName, test_origin,
                 https_server_->GetURL("a.test",
                                       "/interest_group/bidding_logic.js"),
                 MakeAdsValue(
                     {{{ad_url, R"({"ad":"metadata","here":[1,2]})"}}}))));

  std::string auction_config = JsReplace(
      R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
    auctionSignals: {x: 1},
    sellerSignals: {yet: 'more', info: 1},
    perBuyerSignals: {$1: {even: 'more', x: 4.5}}
                })",
      test_origin,
      https_server_->GetURL("a.test", "/interest_group/decision_logic.js"));

  // All leaveAdInterestGroup wrapper calls wait for the returned promise to
  // complete. Inline the call to avoid waiting.
  EXPECT_EQ("done", EvalJs(shell(), JsReplace(R"(
(function() {
  navigator.leaveAdInterestGroup({name: $1, owner: $2});
  return 'done';
})())",
                                              kName, test_origin)));

  EXPECT_EQ(nullptr, RunAuctionAndWait(JsReplace(
                         R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
    auctionSignals: {x: 1},
    sellerSignals: {yet: 'more', info: 1},
    perBuyerSignals: {$1: {even: 'more', x: 4.5}}
                         })",
                         test_origin,
                         https_server_->GetURL(
                             "a.test", "/interest_group/decision_logic.js"))));
}

// The winning ad's render url is invalid (invalid url or has http scheme).
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, RunAdAuctionWithInvalidAdUrl) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);

  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          blink::TestInterestGroupBuilder(
              /*owner=*/test_origin,
              /*name=*/"cars")
              .SetBiddingUrl(https_server_->GetURL(
                  "a.test", "/interest_group/bidding_logic_invalid_ad_url.js"))
              .SetAds({{{GURL("https://shoes.com/render"),
                         R"({"ad":"metadata","here":[1,2]})"}}})
              .Build()));

  EXPECT_EQ(nullptr, RunAuctionAndWait(JsReplace(
                         R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
                         })",
                         test_origin,
                         https_server_->GetURL(
                             "a.test", "/interest_group/decision_logic.js"))));
}

// Test that when there are no ad components, an array of ad components is still
// available, and they're all mapped to about:blank.
IN_PROC_BROWSER_TEST_F(InterestGroupFencedFrameBrowserTest, NoAdComponents) {
  GURL test_url = https_server_->GetURL("a.test", "/fenced_frames/basic.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  // Trying to retrieve the adAuctionComponents of the main frame should throw
  // an exception.
  EXPECT_FALSE(GetAdAuctionComponentsInJS(shell(), 1));

  GURL ad_url = https_server_->GetURL("c.test", "/fenced_frames/basic.html");
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          /*owner=*/url::Origin::Create(test_url),
          /*name=*/"cars",
          /*priority=*/0.0,
          /*execution_mode=*/
          blink::InterestGroup::ExecutionMode::kCompatibilityMode,
          /*bidding_url=*/
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /*ads=*/{{{ad_url, /*metadata=*/absl::nullopt}}},
          /*ad_components=*/absl::nullopt));

  ASSERT_NO_FATAL_FAILURE(RunAuctionAndNavigateFencedFrame(
      ad_url, JsReplace(
                  R"(
{
  seller: $1,
  decisionLogicUrl: $2,
  interestGroupBuyers: [$1]
}
                  )",
                  url::Origin::Create(test_url),
                  https_server_->GetURL("a.test",
                                        "/interest_group/decision_logic.js"))));

  // Check that adAuctionComponents() returns an array of URNs that all map to
  // about:blank.
  RenderFrameHostImpl* ad_frame = GetFencedFrameRenderFrameHost(shell());
  CheckAdComponents(/*expected_ad_component_urls=*/std::vector<GURL>{},
                    ad_frame);

  absl::optional<std::vector<GURL>> all_component_urls =
      GetAdAuctionComponentsInJS(ad_frame, blink::kMaxAdAuctionAdComponents);
  ASSERT_TRUE(all_component_urls);
  NavigateFencedFrameAndWait((*all_component_urls)[0],
                             GURL(url::kAboutBlankURL),
                             GetFencedFrameRenderFrameHost(shell()));
  NavigateFencedFrameAndWait(
      (*all_component_urls)[blink::kMaxAdAuctionAdComponents - 1],
      GURL(url::kAboutBlankURL), GetFencedFrameRenderFrameHost(shell()));
}

// Test with an ad component. Run an auction with an ad component, load the ad
// in a fenced frame, and the ad component in a nested fenced frame. Fully
// exercise navigator.adAuctionComponents() on the main ad's fenced frame.
IN_PROC_BROWSER_TEST_F(InterestGroupFencedFrameBrowserTest, AdComponents) {
  GURL ad_component_url = https_server_->GetURL(
      "d.test", "/set-header?Supports-Loading-Mode: fenced-frame");
  ASSERT_NO_FATAL_FAILURE(RunBasicAuctionWithAdComponents(ad_component_url));

  // Trying to retrieve the adAuctionComponents of the main frame should throw
  // an exception.
  EXPECT_FALSE(GetAdAuctionComponentsInJS(shell(), 1));

  // Check that adAuctionComponents() returns an array of URNs, the first of
  // which maps to `ad_component_url`, and the rest of which map to about:blank.
  RenderFrameHostImpl* ad_frame = GetFencedFrameRenderFrameHost(shell());
  CheckAdComponents(
      /*expected_ad_component_urls=*/std::vector<GURL>{ad_component_url},
      ad_frame);

  absl::optional<std::vector<GURL>> all_component_urls =
      GetAdAuctionComponentsInJS(ad_frame, blink::kMaxAdAuctionAdComponents);
  ASSERT_TRUE(all_component_urls);
  NavigateFencedFrameAndWait((*all_component_urls)[1],
                             GURL(url::kAboutBlankURL),
                             GetFencedFrameRenderFrameHost(shell()));
  NavigateFencedFrameAndWait(
      (*all_component_urls)[blink::kMaxAdAuctionAdComponents - 1],
      GURL(url::kAboutBlankURL), GetFencedFrameRenderFrameHost(shell()));
}

// Checked that navigator.adAuctionComponents() from an ad auction with
// components aren't leaked to other frames. In particular, check that they
// aren't provided to:
// * The main frame. It will throw an exception.
// * The fenced frame the ad component is loaded in, though it will have a list
//   of URNs that map to about:blank.
// * The ad fenced frame itself, after a renderer-initiated navigation.
IN_PROC_BROWSER_TEST_F(InterestGroupFencedFrameBrowserTest,
                       AdComponentsNotLeaked) {
  GURL ad_component_url =
      https_server_->GetURL("d.test", "/fenced_frames/basic.html");
  ASSERT_NO_FATAL_FAILURE(RunBasicAuctionWithAdComponents(ad_component_url));

  // The top frame should have no ad components.
  EXPECT_FALSE(GetAdAuctionComponentsInJS(shell(), 1));

  // Check that adAuctionComponents(), when invoked in the ad component's frame,
  // returns an array of URNs that all map to about:blank.
  RenderFrameHostImpl* ad_frame = GetFencedFrameRenderFrameHost(shell());
  RenderFrameHostImpl* ad_component_frame =
      GetFencedFrameRenderFrameHost(ad_frame);
  CheckAdComponents(/*expected_ad_component_urls=*/std::vector<GURL>{},
                    ad_component_frame);

  // Navigate the ad component's nested fenced frame (3 fenced frames deep) to
  // some of the URNs, which should navigate it to about:blank.
  absl::optional<std::vector<GURL>> all_component_urls =
      GetAdAuctionComponentsInJS(ad_component_frame,
                                 blink::kMaxAdAuctionAdComponents);
  ASSERT_TRUE(all_component_urls);
  NavigateFencedFrameAndWait((*all_component_urls)[0],
                             GURL(url::kAboutBlankURL), ad_component_frame);
  NavigateFencedFrameAndWait(
      (*all_component_urls)[blink::kMaxAdAuctionAdComponents - 1],
      GURL(url::kAboutBlankURL), ad_component_frame);

  // Load a new URL in the top-level fenced frame, which should cause future
  // navigator.adComponents() calls to fail. Use a new URL, so can wait for the
  // server to see it. Same origin navigation so that the RenderFrameHost will
  // be reused.
  GURL new_url = https_server_->GetURL(
      ad_frame->GetLastCommittedOrigin().host(), "/echoall");

  // Used to wait for navigation completion in the ShadowDOM case only.
  // Harmlessly created but not used in the MPArch case.
  TestFrameNavigationObserver observer(ad_frame);

  EXPECT_TRUE(ExecJs(ad_frame, JsReplace("document.location = $1;", new_url)));

  // Wait for the URL to be requested, to make sure the fenced frame actually
  // made the request and, in the MPArch case, to make sure the load actually
  // started.
  WaitForUrl(new_url);

  // Wait for the load to complete.
  observer.Wait();

  // Navigating the ad fenced frame may result in it using a new
  // RenderFrameHost, invalidating the old `ad_frame`.
  ad_frame = GetFencedFrameRenderFrameHost(shell());

  // Make sure the expected page has loaded in the ad frame.
  EXPECT_EQ(new_url, ad_frame->GetLastCommittedURL());

  // Calling navigator.adAuctionComponents on the new frame should fail.
  EXPECT_FALSE(GetAdAuctionComponentsInJS(ad_frame, 1));
}

// Test with an ad component that tries to leave the group. Verify that leaving
// the group from within an ad component has no effect
IN_PROC_BROWSER_TEST_F(InterestGroupFencedFrameBrowserTest, AdComponentsLeave) {
  url::Origin test_origin =
      url::Origin::Create(https_server_->GetURL("a.test", "/"));
  GURL ad_component_url = https_server_->GetURL(
      "d.test", "/fenced_frames/ad_that_leaves_interest_group.html");
  AttachInterestGroupObserver();

  ASSERT_NO_FATAL_FAILURE(RunBasicAuctionWithAdComponents(ad_component_url));

  // InterestGroupAccessObserver should see the join and auction, but not the
  // implicit leave since it was blocked.
  WaitForAccessObserved(
      {{TestInterestGroupObserver::kJoin, test_origin, "cars"},
       {TestInterestGroupObserver::kLoaded, test_origin, "cars"},
       {TestInterestGroupObserver::kBid, test_origin, "cars"},
       {TestInterestGroupObserver::kWin, test_origin, "cars"}});

  // The ad shouldn't have left the interest group when the component ad was
  // shown.
  EXPECT_EQ(1u, GetAllInterestGroups().size());
}

// Test navigating multiple fenced frames to the same render URL from a single
// auction, when the winning bid included ad components. All fenced frames
// navigated to the URL should get ad component URLs from the winning bid.
IN_PROC_BROWSER_TEST_F(InterestGroupFencedFrameBrowserTest,
                       AdComponentsMainAdLoadedInMultipleFrames) {
  GURL ad_component_url = https_server_->GetURL(
      "d.test", "/set-header?Supports-Loading-Mode: fenced-frame");
  GURL test_url = https_server_->GetURL("a.test", "/fenced_frames/basic.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  GURL ad_url = https_server_->GetURL("c.test", "/fenced_frames/basic.html");
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          /*owner=*/url::Origin::Create(test_url),
          /*name=*/"cars",
          /*priority=*/0.0,
          /*execution_mode=*/
          blink::InterestGroup::ExecutionMode::kCompatibilityMode,
          /*bidding_url=*/
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /*ads=*/{{{ad_url, /*metadata=*/absl::nullopt}}},
          /*ad_components=*/
          {{{ad_component_url, /*metadata=*/absl::nullopt}}}));

  content::EvalJsResult urn_url_string = RunAuctionAndWait(JsReplace(
      R"(
{
  seller: $1,
  decisionLogicUrl: $2,
  interestGroupBuyers: [$1]
}
      )",
      url::Origin::Create(test_url),
      https_server_->GetURL("a.test", "/interest_group/decision_logic.js")));
  ASSERT_TRUE(urn_url_string.value.is_string())
      << "Expected string, but got " << urn_url_string.value;

  GURL urn_url(urn_url_string.ExtractString());
  ASSERT_TRUE(urn_url.is_valid())
      << "URL is not valid: " << urn_url_string.ExtractString();
  EXPECT_EQ(url::kUrnScheme, urn_url.scheme_piece());

  // Repeatedly load the URN in fenced frames.  The first two iterations use the
  // original fenced frame, the next two use a new one that replaces the first.
  for (int i = 0; i < 4; ++i) {
    if (i == 2) {
      EXPECT_TRUE(ExecJs(shell(),
                         "document.querySelector('fencedframe').remove();"
                         "const ff = document.createElement('fencedframe');"
                         "document.body.appendChild(ff);"));
    }
    ClearReceivedRequests();
    NavigateFencedFrameAndWait(urn_url, ad_url, shell());

    RenderFrameHost* ad_frame = GetFencedFrameRenderFrameHost(shell());
    absl::optional<std::vector<GURL>> components =
        GetAdAuctionComponentsInJS(ad_frame, 1);
    ASSERT_TRUE(components);
    ASSERT_EQ(1u, components->size());
    EXPECT_EQ(url::kUrnScheme, (*components)[0].scheme_piece());
    NavigateFencedFrameAndWait((*components)[0], ad_component_url, ad_frame);
  }
}

// Test with multiple ad components. Also checks that ad component metadata is
// passed in correctly.
IN_PROC_BROWSER_TEST_F(InterestGroupFencedFrameBrowserTest,
                       MultipleAdComponents) {
  // Note that the extra "&1" and the like are added to make the URLs unique.
  // They have no impact on the returned result, since they aren't a
  // header/value pair.
  std::vector<blink::InterestGroup::Ad> ad_components{
      {https_server_->GetURL(
           "d.test", "/set-header?Supports-Loading-Mode: fenced-frame&1"),
       absl::nullopt},
      {https_server_->GetURL(
           "d.test", "/set-header?Supports-Loading-Mode: fenced-frame&2"),
       "2"},
      {https_server_->GetURL(
           "d.test", "/set-header?Supports-Loading-Mode: fenced-frame&3"),
       R"(["3",{"4":"five"}])"},
  };

  GURL test_url = https_server_->GetURL("a.test", "/fenced_frames/basic.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  // Register bidding script that validates interestGroup.adComponents and
  // returns the first and third components in the offered bid.
  std::string bidding_script = R"(
let adComponents = interestGroup.adComponents;
if (adComponents.length !== 3)
  throw 'Incorrect length';
if (adComponents[0].metadata !== undefined)
  throw 'adComponents[0] has incorrect metadata: ' + adComponents[0].metadata;
if (adComponents[1].metadata !== 2)
  throw 'adComponents[1] has incorrect metadata: ' + adComponents[1].metadata;
if (JSON.stringify(adComponents[2].metadata) !== '["3",{"4":"five"}]') {
  throw 'adComponents[2] has incorrect metadata: ' + adComponents[2].metadata;
}

return {
  ad: 'ad',
  bid: 1,
  render: interestGroup.ads[0].renderUrl,
  adComponents: [interestGroup.adComponents[0].renderUrl,
                 interestGroup.adComponents[2].renderUrl]
};
  )";
  GURL bidding_url =
      https_server_->GetURL("a.test", "/generated_bidding_logic.js");
  network_responder_->RegisterBidderScript(bidding_url.path(), bidding_script);

  GURL ad_url = https_server_->GetURL("c.test", "/fenced_frames/basic.html");
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          /*owner=*/url::Origin::Create(test_url),
          /*name=*/"cars", /*priority=*/0.0, /*execution_mode=*/
          blink::InterestGroup::ExecutionMode::kCompatibilityMode, bidding_url,
          /*ads=*/{{{ad_url, /*metadata=*/absl::nullopt}}}, ad_components));

  ASSERT_NO_FATAL_FAILURE(RunAuctionAndNavigateFencedFrame(
      ad_url, JsReplace(
                  R"({
seller: $1,
decisionLogicUrl: $2,
interestGroupBuyers: [$1]
                  })",
                  url::Origin::Create(test_url),
                  https_server_->GetURL("a.test",
                                        "/interest_group/decision_logic.js"))));

  // Validate ad components array. The bidder script should return only the
  // first and last ad component URLs, skpping the second.
  RenderFrameHostImpl* ad_frame = GetFencedFrameRenderFrameHost(shell());
  CheckAdComponents(
      /*expected_ad_component_urls=*/{ad_components[0].render_url,
                                      ad_components[2].render_url},
      ad_frame);

  // Get first three URLs from the fenced frame.
  absl::optional<std::vector<GURL>> components =
      GetAdAuctionComponentsInJS(ad_frame, 3);
  ASSERT_TRUE(components);
  ASSERT_EQ(3u, components->size());

  // Load each of the ad components in the nested fenced frame, validating the
  // URLs they're mapped to.
  NavigateFencedFrameAndWait((*components)[0], ad_components[0].render_url,
                             ad_frame);
  NavigateFencedFrameAndWait((*components)[1], ad_components[2].render_url,
                             ad_frame);
  NavigateFencedFrameAndWait((*components)[2], GURL(url::kAboutBlankURL),
                             ad_frame);
}

// These end-to-end tests validate that information from navigator-exposed APIs
// is correctly passed to worklets.

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       BuyerWorkletThrowsFailsAuction) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);

  ASSERT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          blink::TestInterestGroupBuilder(
              /*owner=*/test_origin,
              /*name=*/"cars")
              .SetBiddingUrl(https_server_->GetURL(
                  "a.test", "/interest_group/bidding_logic_throws.js"))
              .SetTrustedBiddingSignalsUrl(https_server_->GetURL(
                  "a.test", "/interest_group/trusted_bidding_signals.json"))
              .SetTrustedBiddingSignalsKeys({{"key1"}})
              .SetAds({{{GURL("https://example.com/render"),
                         R"({"ad":"metadata","here":[1,2,3]})"}}})
              .Build()));

  EXPECT_EQ(
      nullptr,
      EvalJs(shell(), JsReplace(
                          R"(
(async function() {
  return await navigator.runAdAuction({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
  });
})())",
                          test_origin,
                          https_server_->GetURL(
                              "a.test", "/interest_group/decision_logic.js"))));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, ComponentAuction) {
  GURL test_url = https_server_->GetURL("a.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);
  GURL ad_url = https_server_->GetURL("c.test", "/echo?render_cars");

  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          /*owner=*/test_origin,
          /*name=*/"cars",
          /*priority=*/0.0,
          /*execution_mode=*/
          blink::InterestGroup::ExecutionMode::kCompatibilityMode,
          /*bidding_url=*/
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /*ads=*/{{{ad_url, /*metadata=*/absl::nullopt}}}));

  std::string auction_config = JsReplace(
      R"({
        seller: $1,
        decisionLogicUrl: $2,
        // Signal to the top-level seller to allow participation in a component
        // auction.
        auctionSignals: "sellerAllowsComponentAuction",
        componentAuctions: [{
          seller: $1,
          decisionLogicUrl: $2,
          interestGroupBuyers: [$1],
          // Signal to the bidder and component seller to allow participation in
          // a component auction.
          auctionSignals: "bidderAllowsComponentAuction,"+
                          "sellerAllowsComponentAuction"
        }]
      })",
      test_origin,
      https_server_->GetURL("a.test", "/interest_group/decision_logic.js"));
  RunAuctionAndWaitForURLAndNavigateIframe(auction_config, ad_url);
}

// Test the case of a component argument in the case a bidder refuses to
// participate in component auctions.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       ComponentAuctionBidderRefuses) {
  GURL test_url = https_server_->GetURL("a.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);
  GURL ad_url = https_server_->GetURL("c.test", "/echo?render_cars");

  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          /*owner=*/test_origin,
          /*name=*/"cars",
          /*priority=*/0.0,
          /*execution_mode=*/
          blink::InterestGroup::ExecutionMode::kCompatibilityMode,
          /*bidding_url=*/
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /*ads=*/{{{ad_url, /*metadata=*/absl::nullopt}}}));

  std::string auction_config = JsReplace(
      R"({
        seller: $1,
        decisionLogicUrl: $2,
        // Signal to the top-level seller to allow participation in a component
        // auction.
        auctionSignals: "sellerAllowsComponentAuction",
        componentAuctions: [{
          seller: $1,
          decisionLogicUrl: $2,
          interestGroupBuyers: [$1],
          // Signal to the component seller to allow participation in a
          // component auction.
          auctionSignals: "sellerAllowsComponentAuction"
        }]
      })",
      test_origin,
      https_server_->GetURL("a.test", "/interest_group/decision_logic.js"));
  EXPECT_EQ(nullptr, RunAuctionAndWait(auction_config));
}

// Test the case of a component argument in the case the top-level seller
// refuses to participate in component auctions.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       ComponentAuctionTopLevelSellerRefuses) {
  GURL test_url = https_server_->GetURL("a.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);
  GURL ad_url = https_server_->GetURL("c.test", "/echo?render_cars");

  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          /*owner=*/test_origin,
          /*name=*/"cars",
          /*priority=*/0.0,
          /*execution_mode=*/
          blink::InterestGroup::ExecutionMode::kCompatibilityMode,
          /*bidding_url=*/
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /*ads=*/{{{ad_url, /*metadata=*/absl::nullopt}}}));

  std::string auction_config = JsReplace(
      R"({
        seller: $1,
        decisionLogicUrl: $2,
        componentAuctions: [{
          seller: $1,
          decisionLogicUrl: $2,
          interestGroupBuyers: [$1],
          // Signal to the bidder and component seller to allow participation in
          // a component auction.
          auctionSignals: "bidderAllowsComponentAuction,"+
                          "sellerAllowsComponentAuction"
        }]
      })",
      test_origin,
      https_server_->GetURL("a.test", "/interest_group/decision_logic.js"));
  EXPECT_EQ(nullptr, RunAuctionAndWait(auction_config));
}

// Test the case of a component argument in the case a component seller refuses
// to participate in component auctions.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       ComponentAuctionComponentSellerRefuses) {
  GURL test_url = https_server_->GetURL("a.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);
  GURL ad_url = https_server_->GetURL("c.test", "/echo?render_cars");

  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          /*owner=*/test_origin,
          /*name=*/"cars",
          /*priority=*/0.0,
          /*execution_mode=*/
          blink::InterestGroup::ExecutionMode::kCompatibilityMode,
          /*bidding_url=*/
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /*ads=*/{{{ad_url, /*metadata=*/absl::nullopt}}}));

  std::string auction_config = JsReplace(
      R"({
        seller: $1,
        decisionLogicUrl: $2,
        // Signal to the top-level seller to allow participation in a component
        // auction.
        auctionSignals: "sellerAllowsComponentAuction",
        componentAuctions: [{
          seller: $1,
          decisionLogicUrl: $2,
          interestGroupBuyers: [$1],
          // Signal to the bidder to allow participation in a component auction.
          auctionSignals: "bidderAllowsComponentAuction"
        }]
      })",
      test_origin,
      https_server_->GetURL("a.test", "/interest_group/decision_logic.js"));
  EXPECT_EQ(nullptr, RunAuctionAndWait(auction_config));
}

// Use bidder and seller worklet files that validate their arguments all have
// the expected values.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, ValidateWorkletParameters) {
  // Use different hostnames for each participant, since
  // `trusted_bidding_signals` only checks the hostname of certain parameters.
  constexpr char kBidderHost[] = "a.test";
  constexpr char kSellerHost[] = "b.test";
  constexpr char kTopFrameHost[] = "c.test";
  constexpr char kSecondBidderHost[] = "d.test";
  content_browser_client_->AddToAllowList(
      {url::Origin::Create(https_server_->GetURL(kSecondBidderHost, "/"))});
  const url::Origin top_frame_origin =
      url::Origin::Create(https_server_->GetURL(kTopFrameHost, "/echo"));

  // Start by adding a placeholder bidder in domain d.test, used for
  // perBuyerSignals validation.
  GURL second_bidder_url = https_server_->GetURL(kSecondBidderHost, "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), second_bidder_url));
  url::Origin second_bidder_origin = url::Origin::Create(second_bidder_url);

  ASSERT_EQ(kSuccess,
            JoinInterestGroupAndVerify(
                /*owner=*/second_bidder_origin,
                /*name=*/"boats",
                /*priority=*/0.0,
                /*execution_mode=*/
                blink::InterestGroup::ExecutionMode::kCompatibilityMode,
                /*bidding_url=*/
                https_server_->GetURL(kSecondBidderHost,
                                      "/interest_group/bidding_logic.js"),
                /*ads=*/
                {{{GURL("https://should-not-be-returned/"),
                   /*metadata=*/absl::nullopt}}}));

  GURL bidder_url = https_server_->GetURL(kBidderHost, "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), bidder_url));
  url::Origin bidder_origin = url::Origin::Create(bidder_url);

  ASSERT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/bidder_origin,
          /*name=*/"cars",
          /*priority=*/0.0, /*enable_bidding_signals_prioritization=*/true,
          /*priority_vector=*/{{{"foo", 2}, {"bar", -11}}},
          /*priority_signals_overrides=*/{{{"foo", 1}}},
          /*seller_capabilities=*/absl::nullopt,
          /*all_sellers_capabilities=*/
          {}, /*execution_mode=*/
          blink::InterestGroup::ExecutionMode::kCompatibilityMode,
          /*bidding_url=*/
          https_server_->GetURL(
              kBidderHost, "/interest_group/bidding_argument_validator.js"),
          /*bidding_wasm_helper_url=*/absl::nullopt,
          /*update_url=*/
          https_server_->GetURL(kBidderHost, "/not_found_update_url.json"),
          /*trusted_bidding_signals_url=*/
          https_server_->GetURL(kBidderHost,
                                "/interest_group/trusted_bidding_signals.json"),
          /*trusted_bidding_signals_keys=*/{{"key1"}},
          /*user_bidding_signals=*/R"({"some":"json","stuff":{"here":[1,2]}})",
          /*ads=*/
          {{{GURL("https://example.com/render"),
             R"({"ad":"metadata","here":[1,2,3]})"}}},
          /*ad_components=*/
          {{{GURL("https://example.com/render-component"),
             /*metadata=*/absl::nullopt}}},
          /*ad_sizes=*/{},
          /*size_groups=*/{})));

  // For `directFromSellerSignals` to work, we need to navigate to a page that
  // declares the subresource bundle resources we pass to those fields.
  GURL seller_script_url = https_server_->GetURL(
      kSellerHost, "/interest_group/decision_argument_validator.js");
  url::Origin seller_origin = url::Origin::Create(seller_script_url);
  std::vector<NetworkResponder::SubresourceResponse> subresource_responses = {
      NetworkResponder::SubresourceResponse(
          /*subresource_url=*/
          "/direct_from_seller_signals?sellerSignals",
          /*payload=*/
          R"({"json": "for", "the": ["seller"]})"),
      NetworkResponder::SubresourceResponse(
          /*subresource_url=*/"/direct_from_seller_signals?auctionSignals",
          /*payload=*/
          R"({"json": "for", "all": ["parties"]})"),
      NetworkResponder::DirectFromSellerPerBuyerSignals(
          bidder_origin, /*payload=*/
          R"({"json": "for", "buyer": [1]})"),
      NetworkResponder::DirectFromSellerPerBuyerSignals(
          second_bidder_origin, /*payload=*/
          R"({"json": "for", "buyer": [2]})"),
  };
  std::vector<NetworkResponder::SubresourceBundle> bundles = {
      NetworkResponder::SubresourceBundle(
          /*bundle_url=*/https_server_->GetURL(kSellerHost,
                                               "/generated_bundle.wbn"),
          /*subresources=*/subresource_responses)};

  network_responder_->RegisterDirectFromSellerSignalsResponse(
      /*bundles=*/bundles,
      /*allow_origin=*/top_frame_origin.Serialize());
  constexpr char kPagePath[] = "/page-with-bundles.html";
  network_responder_->RegisterHtmlWithSubresourceBundles(
      /*bundles=*/bundles,
      /*page_url=*/kPagePath);

  ASSERT_TRUE(
      NavigateToURL(shell(), https_server_->GetURL(kTopFrameHost, kPagePath)));
  TestFencedFrameURLMappingResultObserver observer;
  ConvertFencedFrameURNToURL(
      GURL(EvalJs(shell(),
                  JsReplace(
                      R"(
(async function() {
  return await navigator.runAdAuction({
    seller: $1,
    decisionLogicUrl: $2,
    trustedScoringSignalsUrl: $3,
    interestGroupBuyers: [$4, $5],
    auctionSignals: {so: 'I', hear: ['you', 'like', 'json']},
    sellerSignals: {signals: 'from', the: ['seller']},
    directFromSellerSignals: $6,
    sellerTimeout: 200,
    perBuyerSignals: {$4: {signalsForBuyer: 1}, $5: {signalsForBuyer: 2}},
    perBuyerTimeouts: {$4: 110, $5: 120, '*': 150},
    perBuyerCumulativeTimeouts: {$4: 13000, $5: 14000, '*': 16000},
    perBuyerPrioritySignals: {$4: {foo: 1}, '*': {BaR: -2}},
    perBuyerCurrencies: {$4: 'USD', $5: 'CAD', '*': 'EUR'},
    sellerCurrency: 'EUR'
  });
})())",
                      seller_origin, seller_script_url,
                      https_server_->GetURL(
                          kSellerHost,
                          "/interest_group/trusted_scoring_signals.json"),
                      bidder_origin, second_bidder_origin,
                      https_server_->GetURL(kSellerHost,
                                            "/direct_from_seller_signals")))
               .ExtractString()),
      &observer);
  EXPECT_EQ(GURL("https://example.com/render"), observer.mapped_url());

  // Run URN to URL mapping callback manually to trigger sending reports, and
  // validate the right URLs are requested. Do this instead of navigating to
  // the URN because the validation logic checks a fixed URL for this test,
  // and don't want to send a random request to port 80 on localhost, which is
  // what example.com is mapped to.
  observer.on_navigate_callback().Run();
  WaitForUrl(https_server_->GetURL(kSellerHost, "/echo?report_seller"));
  WaitForUrl(https_server_->GetURL(kSellerHost, "/echo?report_bidder"));
}

// Same as above test, but leaves out the extra bidder and uses the older
// version 1 bidding signals format.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       ValidateWorkletParametersWithBiddingSignalsV1) {
  // Use different hostnames for each participant, since
  // `trusted_bidding_signals` only checks the hostname of certain parameters.
  constexpr char kBidderHost[] = "a.test";
  constexpr char kSellerHost[] = "b.test";
  constexpr char kTopFrameHost[] = "c.test";
  const url::Origin top_frame_origin =
      url::Origin::Create(https_server_->GetURL(kTopFrameHost, "/echo"));

  // This is the primary interest group that wins the auction because
  // bidding_argument_validator.js bids 2, whereas bidding_logic.js bids 1, and
  // decision_logic.js just returns the bid as the rank -- highest rank wins.
  GURL bidder_url = https_server_->GetURL(kBidderHost, "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), bidder_url));
  url::Origin bidder_origin = url::Origin::Create(bidder_url);

  ASSERT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/bidder_origin,
          /*name=*/"cars",
          /*priority=*/0.0, /*enable_bidding_signals_prioritization=*/true,
          /*priority_vector=*/{{{"foo", 2}, {"bar", -11}}},
          /*priority_signals_overrides=*/{{{"foo", 1}}},
          /*seller_capabilities=*/absl::nullopt,
          /*all_sellers_capabilities=*/
          {}, /*execution_mode=*/
          blink::InterestGroup::ExecutionMode::kCompatibilityMode,
          /*bidding_url=*/
          https_server_->GetURL(
              kBidderHost, "/interest_group/bidding_argument_validator.js"),
          /*bidding_wasm_helper_url=*/absl::nullopt,
          /*update_url=*/
          https_server_->GetURL(kBidderHost, "/not_found_update_url.json"),
          /*trusted_bidding_signals_url=*/
          https_server_->GetURL(
              kBidderHost, "/interest_group/trusted_bidding_signals_v1.json"),
          /*trusted_bidding_signals_keys=*/{{"key1"}},
          /*user_bidding_signals=*/R"({"some":"json","stuff":{"here":[1,2]}})",
          /*ads=*/
          {{{GURL("https://example.com/render"),
             R"({"ad":"metadata","here":[1,2,3]})"}}},
          /*ad_components=*/
          {{{GURL("https://example.com/render-component"),
             /*metadata=*/absl::nullopt}}},
          /*ad_sizes=*/{},
          /*size_groups=*/{})));

  // For `directFromSellerSignals` to work, we need to navigate to a page that
  // declares the subresource bundle resources we pass to those fields.
  GURL seller_script_url = https_server_->GetURL(
      kSellerHost, "/interest_group/decision_argument_validator.js");
  url::Origin seller_origin = url::Origin::Create(seller_script_url);
  std::vector<NetworkResponder::SubresourceResponse> subresource_responses = {
      NetworkResponder::SubresourceResponse(
          /*subresource_url=*/
          "/direct_from_seller_signals?sellerSignals",
          /*payload=*/
          R"({"json": "for", "the": ["seller"]})"),
      NetworkResponder::SubresourceResponse(
          /*subresource_url=*/"/direct_from_seller_signals?auctionSignals",
          /*payload=*/
          R"({"json": "for", "all": ["parties"]})"),
      NetworkResponder::DirectFromSellerPerBuyerSignals(
          bidder_origin, /*payload=*/
          R"({"json": "for", "buyer": [1]})"),
  };
  std::vector<NetworkResponder::SubresourceBundle> bundles = {
      NetworkResponder::SubresourceBundle(
          /*bundle_url=*/https_server_->GetURL(kSellerHost,
                                               "/generated_bundle.wbn"),
          /*subresources=*/subresource_responses)};

  network_responder_->RegisterDirectFromSellerSignalsResponse(
      /*bundles=*/bundles,
      /*allow_origin=*/top_frame_origin.Serialize());
  constexpr char kPagePath[] = "/page-with-bundles.html";
  network_responder_->RegisterHtmlWithSubresourceBundles(
      /*bundles=*/bundles,
      /*page_url=*/kPagePath);

  ASSERT_TRUE(
      NavigateToURL(shell(), https_server_->GetURL(kTopFrameHost, kPagePath)));

  TestFencedFrameURLMappingResultObserver observer;
  ConvertFencedFrameURNToURL(
      GURL(EvalJs(shell(),
                  JsReplace(
                      R"(
(async function() {
  return await navigator.runAdAuction({
    seller: $1,
    decisionLogicUrl: $2,
    trustedScoringSignalsUrl: $3,
    interestGroupBuyers: [$4, $5],
    auctionSignals: {so: 'I', hear: ['you', 'like', 'json']},
    sellerSignals: {signals: 'from', the: ['seller']},
    directFromSellerSignals: $6,
    sellerTimeout: 200,
    perBuyerSignals: {$4: {signalsForBuyer: 1}, $5: {signalsForBuyer: 2}},
    perBuyerTimeouts: {$4: 110, $5: 120, '*': 150},
    perBuyerCumulativeTimeouts: {$4: 13000, $5: 14000, '*': 16000},
    perBuyerPrioritySignals: {$4: {foo: 1}, '*': {BaR: -2}},
    perBuyerCurrencies: {$4: 'USD', $5: 'CAD', '*': 'EUR'},
    sellerCurrency: 'EUR'
  });
})())",
                      seller_origin, seller_script_url,
                      https_server_->GetURL(
                          kSellerHost,
                          "/interest_group/trusted_scoring_signals.json"),
                      bidder_origin,
                      // Validation scripts expect https://d.test to also be
                      // listed as a bidder.
                      https_server_->GetOrigin("d.test"),
                      https_server_->GetURL(kSellerHost,
                                            "/direct_from_seller_signals")))
               .ExtractString()),
      &observer);
  EXPECT_EQ(GURL("https://example.com/render"), observer.mapped_url());

  // Run URN to URL mapping callback manually to trigger sending reports, and
  // validate the right URLs are requested. Do this instead of navigating to
  // the URN because the validation logic checks a fixed URL for this test,
  // and don't want to send a random request to port 80 on localhost, which is
  // what example.com is mapped to.
  observer.on_navigate_callback().Run();
  WaitForUrl(https_server_->GetURL(kSellerHost, "/echo?report_seller"));
  WaitForUrl(https_server_->GetURL(kSellerHost, "/echo?report_bidder"));
}

// Use bidder and seller worklet files that validate their arguments all have
// the expected values, in the case of an auction with one component auction.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       ComponentAuctionValidateWorkletParameters) {
  // Use different hostnames for each participant.
  //
  // Match assignments in above test as closely as possible, to make scripts
  // similar.
  constexpr char kBidderHost[] = "a.test";
  constexpr char kTopLevelSellerHost[] = "b.test";
  constexpr char kTopFrameHost[] = "c.test";
  constexpr char kComponentSellerHost[] = "d.test";

  content_browser_client_->AddToAllowList(
      {url::Origin::Create(https_server_->GetURL(kComponentSellerHost, "/"))});

  for (bool use_promise : {false, true}) {
    SCOPED_TRACE(use_promise);

    const url::Origin top_frame_origin =
        url::Origin::Create(https_server_->GetURL(kTopFrameHost, "/echo"));

    GURL bidder_url = https_server_->GetURL(kBidderHost, "/echo");
    ASSERT_TRUE(NavigateToURL(shell(), bidder_url));
    url::Origin bidder_origin = url::Origin::Create(bidder_url);

    if (use_promise) {
      // Cleanup between runs, to reset the stats to expected values.
      EXPECT_EQ(kSuccess, LeaveInterestGroupAndVerify(/*owner=*/bidder_origin,
                                                      /*name=*/"cars"));
    }

    ASSERT_EQ(
        kSuccess,
        JoinInterestGroupAndVerify(blink::InterestGroup(
            /*expiry=*/base::Time(),
            /*owner=*/bidder_origin,
            /*name=*/"cars",
            /*priority=*/0.0, /*enable_bidding_signals_prioritization=*/false,
            /*priority_vector=*/{{{"FOO", 2}}},
            /*priority_signals_overrides=*/{{{"FOO", 1}}},
            /*seller_capabilities=*/absl::nullopt,
            /*all_sellers_capabilities=*/
            {}, /*execution_mode=*/
            blink::InterestGroup::ExecutionMode::kCompatibilityMode,
            /*bidding_url=*/
            https_server_->GetURL(
                kBidderHost,
                "/interest_group/"
                "component_auction_bidding_argument_validator.js"),
            /*bidding_wasm_helper_url=*/absl::nullopt,
            /*update_url=*/
            https_server_->GetURL(kBidderHost, "/not_found_update_url.json"),
            /*trusted_bidding_signals_url=*/
            https_server_->GetURL(
                kBidderHost, "/interest_group/trusted_bidding_signals.json"),
            /*trusted_bidding_signals_keys=*/{{"key1"}},
            /*user_bidding_signals=*/
            R"({"some":"json","stuff":{"here":[1,2]}})",
            /*ads=*/
            {{{GURL("https://example.com/render"),
               R"({"ad":"metadata","here":[1,2,3]})"}}},
            /*ad_components=*/
            {{{GURL("https://example.com/render-component"),
               /*metadata=*/absl::nullopt}}},
            /*ad_sizes=*/{},
            /*size_groups=*/{})));

    // For `directFromSellerSignals` to work, we need to navigate to a page that
    // declares the subresource bundle resources we pass to those fields.
    GURL top_level_seller_script_url = https_server_->GetURL(
        kTopLevelSellerHost,
        "/interest_group/"
        "component_auction_top_level_decision_argument_validator.js");
    GURL component_seller_script_url = https_server_->GetURL(
        kComponentSellerHost,
        "/interest_group/"
        "component_auction_component_decision_argument_validator.js");
    const url::Origin top_level_seller_origin =
        url::Origin::Create(top_level_seller_script_url);
    const url::Origin component_seller_origin =
        url::Origin::Create(component_seller_script_url);
    std::vector<NetworkResponder::SubresourceResponse>
        top_level_subresource_responses = {
            NetworkResponder::SubresourceResponse(
                /*subresource_url=*/
                "/direct_from_seller_signals?sellerSignals",
                /*payload=*/
                R"({"json": "for", "the": ["seller"]})"),
            NetworkResponder::SubresourceResponse(
                /*subresource_url=*/"/direct_from_seller_signals?"
                                    "auctionSignals",
                /*payload=*/
                R"({"json": "for", "all": ["parties"]})"),
        };
    std::vector<NetworkResponder::SubresourceResponse>
        component_subresource_responses = {
            NetworkResponder::SubresourceResponse(
                /*subresource_url=*/
                "/direct_from_seller_signals?sellerSignals",
                /*payload=*/
                R"({"from": "component", "json": "for", "the": ["seller"]})"),
            NetworkResponder::SubresourceResponse(
                /*subresource_url=*/"/direct_from_seller_signals?"
                                    "auctionSignals",
                /*payload=*/
                R"({"from": "component", "json": "for", "all": ["parties"]})"),
            NetworkResponder::DirectFromSellerPerBuyerSignals(
                bidder_origin, /*payload=*/
                R"({"from": "component", "json": "for", "buyer": [1]})"),
        };
    std::vector<NetworkResponder::SubresourceBundle> bundles = {
        NetworkResponder::SubresourceBundle(
            /*bundle_url=*/https_server_->GetURL(kTopLevelSellerHost,
                                                 "/0generated_bundle.wbn"),
            /*subresources=*/top_level_subresource_responses),
        {NetworkResponder::SubresourceBundle(
            /*bundle_url=*/https_server_->GetURL(kComponentSellerHost,
                                                 "/1generated_bundle.wbn"),
            /*subresources=*/component_subresource_responses)}};

    network_responder_->RegisterDirectFromSellerSignalsResponse(
        /*bundles=*/bundles,
        /*allow_origin=*/top_frame_origin.Serialize());
    constexpr char kPagePath[] = "/page-with-bundles.html";
    network_responder_->RegisterHtmlWithSubresourceBundles(
        /*bundles=*/bundles,
        /*page_url=*/kPagePath);

    ASSERT_TRUE(NavigateToURL(shell(),
                              https_server_->GetURL(kTopFrameHost, kPagePath)));

    TestFencedFrameURLMappingResultObserver observer;
    ConvertFencedFrameURNToURL(
        GURL(EvalJs(
                 shell(),
                 JsReplace(
                     std::string(use_promise ? kFeedPromise : kFeedDirect) + R"(
(async function() {
  return await navigator.runAdAuction({
    seller: $1,
    decisionLogicUrl: $2,
    trustedScoringSignalsUrl: $3,
    auctionSignals: maybePromise(["top-level auction signals"]),
    sellerSignals: maybePromise(["top-level seller signals"]),
    directFromSellerSignals: maybePromise($4),
    sellerTimeout: 300,
    perBuyerSignals: maybePromise({$8: ["top-level buyer signals"]}),
    perBuyerTimeouts: maybePromise({$8: 110, '*': 150}),
    perBuyerCumulativeTimeouts: maybePromise({$8: 11100, '*': 15100}),
    perBuyerPrioritySignals: {'*': {foo: 3}},
    perBuyerCurrencies: {'*': 'MXN', $5: 'CAD'},
    componentAuctions: [{
      seller: $5,
      decisionLogicUrl: $6,
      trustedScoringSignalsUrl: $7,
      interestGroupBuyers: [$8],
      auctionSignals: maybePromise(["component auction signals"]),
      sellerSignals: maybePromise(["component seller signals"]),
      directFromSellerSignals: maybePromise($9),
      sellerTimeout: 200,
      perBuyerSignals: maybePromise({$8: ["component buyer signals"]}),
      perBuyerTimeouts: maybePromise({$8: 200}),
      perBuyerCumulativeTimeouts: maybePromise({$8: 20100}),
      perBuyerPrioritySignals: {$8: {bar: 1}, '*': {BaZ: -2}},
      perBuyerCurrencies: maybePromise({$8: 'USD'}),
      sellerCurrency: 'CAD',
    }],
  });
})())",
                     top_level_seller_origin, top_level_seller_script_url,
                     https_server_->GetURL(
                         kTopLevelSellerHost,
                         "/interest_group/trusted_scoring_signals.json"),
                     https_server_->GetURL(kTopLevelSellerHost,
                                           "/direct_from_seller_signals"),
                     url::Origin::Create(component_seller_script_url),
                     component_seller_script_url,
                     https_server_->GetURL(
                         kComponentSellerHost,
                         "/interest_group/trusted_scoring_signals2.json"),
                     bidder_origin,
                     https_server_->GetURL(kComponentSellerHost,
                                           "/direct_from_seller_signals")))
                 .ExtractString()),
        &observer);
    EXPECT_EQ(GURL("https://example.com/render"), observer.mapped_url());

    // Run URN to URL mapping callback manually to trigger sending reports, and
    // validate the right URLs are requested. Do this instead of navigating to
    // the URN because the validation logic checks a fixed URL for this test,
    // and don't want to send a random request to port 80 on localhost, which is
    // what example.com is mapped to.
    observer.on_navigate_callback().Run();
    WaitForUrl(https_server_->GetURL(kTopLevelSellerHost,
                                     "/echo?report_top_level_seller"));
    WaitForUrl(https_server_->GetURL(kComponentSellerHost,
                                     "/echo?report_component_seller"));
    WaitForUrl(https_server_->GetURL(kBidderHost, "/echo?report_bidder"));
  }
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       SellerWorkletThrowsFailsAuction) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);

  ASSERT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          blink::TestInterestGroupBuilder(
              /*owner=*/test_origin,
              /*name=*/"cars")
              .SetBiddingUrl(https_server_->GetURL(
                  "a.test", "/interest_group/bidding_logic.js"))
              .SetTrustedBiddingSignalsUrl(https_server_->GetURL(
                  "a.test", "/interest_group/trusted_bidding_signals.json"))
              .SetTrustedBiddingSignalsKeys({{"key1"}})
              .SetAds({{{GURL("https://example.com/render"),
                         R"({"ad":"metadata","here":[1,2,3]})"}}})
              .Build()));

  EXPECT_EQ(
      nullptr,
      EvalJs(shell(),
             JsReplace(
                 R"(
(async function() {
  return await navigator.runAdAuction({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
  });
})())",
                 test_origin,
                 https_server_->GetURL(
                     "a.test", "/interest_group/decision_logic_throws.js"))));
}

// JSON fields of joinAdInterestGroup() and runAdAuction() should support
// non-object types, like numbers.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       JoinInterestGroupNonObjectJSONFields) {
  // These scripts are generated by this test.
  constexpr char kBiddingLogicPath[] =
      "/interest_group/non_object_bidding_argument_validator.js";
  constexpr char kDecisionLogicPath[] =
      "/interest_group/non_object_decision_argument_validator.js";
  constexpr char kTrustedBiddingSignalsPath[] =
      "/interest_group/non_object_bidding_signals.json";
  const GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);

  // In the below JavaScript, if fields are incorrectly passed in as a string
  // ("2") instead of a number (2), JSON.stringify() will wrap it in another
  // layer of quotes, causing the test to fail. The order of properties produced
  // by stringify() isn't guaranteed by the ECMAScript standard, but some sites
  // depend on the V8 behavior of serializing in declaration order.

  constexpr char kBiddingLogicScript[] = R"(
function generateBid(
    interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
    unusedBrowserSignals) {
  validateInterestGroup(interestGroup);
  validateAuctionSignals(auctionSignals);
  validatePerBuyerSignals(perBuyerSignals);
  validateTrustedBiddingSignals(trustedBiddingSignals);
  const ad = interestGroup.ads[0];
  return {'ad': ad, 'bid': 1, 'render': ad.renderUrl};
}

function validateInterestGroup(interestGroup) {
  const userBiddingSignalsJSON =
      JSON.stringify(interestGroup.userBiddingSignals);
  if (userBiddingSignalsJSON !== '1')
    throw 'Wrong userBiddingSignals ' + userBiddingSignalsJSON;
  if (interestGroup.ads.length !== 1)
    throw 'Wrong ads.length ' + ads.length;
  const adMetadataJSON = JSON.stringify(interestGroup.ads[0].metadata);
  if (adMetadataJSON !== '2')
    throw 'Wrong ads[0].metadata ' + adMetadataJSON;
}

function validateAuctionSignals(auctionSignals) {
  const auctionSignalsJSON = JSON.stringify(auctionSignals);
  if (auctionSignalsJSON !== '3')
    throw 'Wrong auctionSignals ' + auctionSignalsJSON;
}

function validatePerBuyerSignals(perBuyerSignals) {
  const perBuyerSignalsJson = JSON.stringify(perBuyerSignals);
  if (perBuyerSignalsJson !== '5')
    throw 'Wrong perBuyerSignals ' + perBuyerSignalsJson;
}

function validateTrustedBiddingSignals(trustedBiddingSignals) {
  const trustedBiddingSignalsJSON = JSON.stringify(trustedBiddingSignals);
  if (trustedBiddingSignalsJSON !== '{"key1":0}')
    throw 'Wrong trustedBiddingSignals ' + trustedBiddingSignalsJSON;
}
)";

  constexpr char kDecisionLogicScript[] = R"(
function scoreAd(
    adMetadata, bid, auctionConfig, unusedTrustedScoringSignals,
    unusedBrowserSignals) {
  validateAdMetadata(adMetadata);
  validateAuctionConfig(auctionConfig);
  return bid;
}

function validateAdMetadata(adMetadata) {
  const adMetadataJSON = JSON.stringify(adMetadata);
  if (adMetadataJSON !==
      '{"renderUrl":"https://example.com/render","metadata":2}')
    throw 'Wrong adMetadata ' + adMetadataJSON;
}

function validateAuctionConfig(auctionConfig) {
  const auctionSignalsJSON = JSON.stringify(auctionConfig.auctionSignals);
  if (auctionSignalsJSON !== '3')
    throw 'Wrong auctionSignals ' + auctionSignalsJSON;
  const sellerSignalsJSON = JSON.stringify(auctionConfig.sellerSignals);
  if (sellerSignalsJSON !== '4')
    throw 'Wrong sellerSignals ' + sellerSignalsJSON;
  const perBuyerSignalsJson = JSON.stringify(auctionConfig.perBuyerSignals);
  if (!perBuyerSignalsJson.includes('a.test') ||
      !perBuyerSignalsJson.includes('5')) {
    throw 'Wrong perBuyerSignals ' + perBuyerSignalsJson;
  }
}
)";

  network_responder_->RegisterNetworkResponse(
      kBiddingLogicPath, kBiddingLogicScript, "application/javascript");
  network_responder_->RegisterNetworkResponse(
      kDecisionLogicPath, kDecisionLogicScript, "application/javascript");
  network_responder_->RegisterNetworkResponse(
      kTrustedBiddingSignalsPath, R"({"key1":0})", "application/json");

  EXPECT_EQ(
      "done",
      EvalJs(shell(),
             JsReplace(
                 R"(
(async function() {
  try {
    await navigator.joinAdInterestGroup(
        {
          name: 'cars',
          owner: $1,
          trustedBiddingSignalsUrl: $2,
          trustedBiddingSignalsKeys: ['key1'],
          biddingLogicUrl: $3,
          userBiddingSignals: 1,
          ads: [{renderUrl:"https://example.com/render", metadata:2}],
        },
        /*joinDurationSec=*/1);
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())",
                 test_origin,
                 https_server_->GetURL("a.test", kTrustedBiddingSignalsPath),
                 https_server_->GetURL("a.test", kBiddingLogicPath))));

  TestFencedFrameURLMappingResultObserver observer;
  ConvertFencedFrameURNToURL(
      GURL(EvalJs(shell(),
                  JsReplace(
                      R"(
(async function() {
  return await navigator.runAdAuction({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
    auctionSignals: 3,
    sellerSignals: 4,
    perBuyerSignals: {$1: 5}
  });
})())",
                      test_origin,
                      https_server_->GetURL("a.test", kDecisionLogicPath)))
               .ExtractString()),
      &observer);

  EXPECT_EQ(GURL("https://example.com/render"), observer.mapped_url());
}

// Test for auctionSignals, perBuyerSignals, and sellerSignals being passed to
// runAdAuction as promises.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, PromiseSignals) {
  // These scripts are generated by this test.
  constexpr char kBiddingLogicPath[] =
      "/interest_group/test_generated_bidding_argument_validator.js";
  constexpr char kDecisionLogicPath[] =
      "/interest_group/test_generated_decision_argument_validator.js";
  const GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);

  // In the below JavaScript, if fields are incorrectly passed in as a string
  // ("2") instead of a number (2), JSON.stringify() will wrap it in another
  // layer of quotes, causing the test to fail.

  constexpr char kBiddingLogicScript[] = R"(
function generateBid(
    interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
    unusedBrowserSignals) {
  validateAuctionSignals(auctionSignals);
  if (perBuyerSignals !== 5)
    throw 'Wrong perBuyerSignals ' + JSON.stringify(perBuyerSignals);
  const ad = interestGroup.ads[0];
  return {'ad': ad, 'bid': 1, 'render': ad.renderUrl};
}

function validateAuctionSignals(auctionSignals) {
  const auctionSignalsJSON = JSON.stringify(auctionSignals);
  if (auctionSignalsJSON !== '3')
    throw 'Wrong auctionSignals ' + auctionSignalsJSON;
}

)";

  constexpr char kDecisionLogicScript[] = R"(
function scoreAd(
    adMetadata, bid, auctionConfig, unusedTrustedScoringSignals,
    unusedBrowserSignals) {
  validateAuctionConfig(auctionConfig);
  return bid;
}

function validateAuctionConfig(auctionConfig) {
  const auctionSignalsJSON = JSON.stringify(auctionConfig.auctionSignals);
  if (auctionSignalsJSON !== '3')
    throw 'Wrong auctionSignals ' + auctionSignalsJSON;
  const sellerSignalsJSON = JSON.stringify(auctionConfig.sellerSignals);
  if (sellerSignalsJSON !== '4')
    throw 'Wrong sellerSignals ' + sellerSignalsJSON;
  let ok = false;
  const perBuyerSignalsJson = JSON.stringify(auctionConfig.perBuyerSignals);
  for (key in auctionConfig.perBuyerSignals) {
    if (key.startsWith("https://a.test")) {
      ok = (auctionConfig.perBuyerSignals[key] === 5);
    } else {
      throw 'Wrong key in perBuyerSignals ' + perBuyerSignalsJson;
    }
  }
  if (!ok) {
    throw 'Wrong perBuyerSignals ' + perBuyerSignalsJson;
  }
}
)";

  network_responder_->RegisterNetworkResponse(
      kBiddingLogicPath, kBiddingLogicScript, "application/javascript");
  network_responder_->RegisterNetworkResponse(
      kDecisionLogicPath, kDecisionLogicScript, "application/javascript");

  EXPECT_EQ(
      "done",
      EvalJs(shell(), JsReplace(
                          R"(
(async function() {
  try {
    await navigator.joinAdInterestGroup(
        {
          name: 'cars',
          owner: $1,
          biddingLogicUrl: $2,
          ads: [{renderUrl:"https://example.com/render", metadata:2}],
        },
        /*joinDurationSec=*/100);
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())",
                          test_origin,
                          https_server_->GetURL("a.test", kBiddingLogicPath))));

  TestFencedFrameURLMappingResultObserver observer;
  ConvertFencedFrameURNToURL(
      GURL(EvalJs(shell(),
                  JsReplace(
                      R"(
(async function() {
  return await navigator.runAdAuction({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
    auctionSignals: new Promise((resolve, reject) => {
      setTimeout(
          () => { resolve(3); }, 1)
    }),
    sellerSignals: new Promise((resolve, reject) => {
      setTimeout(
          () => { resolve(4); }, 1)
    }),
    perBuyerSignals: new Promise((resolve, reject) => {
      setTimeout(
          () => { resolve({$1: 5}); }, 1)
    })
  });
})())",
                      test_origin,
                      https_server_->GetURL("a.test", kDecisionLogicPath)))
               .ExtractString()),
      &observer);

  EXPECT_EQ(GURL("https://example.com/render"), observer.mapped_url());
}

// Test for abort before promises resolved.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, PromiseAborted) {
  // These scripts are generated by this test.
  constexpr char kBiddingLogicPath[] =
      "/interest_group/test_generated_bidding_argument_validator.js";
  constexpr char kDecisionLogicPath[] =
      "/interest_group/test_generated_decision_argument_validator.js";
  const GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);

  constexpr char kBiddingLogicScript[] = R"(
function generateBid(
    interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
    unusedBrowserSignals) {
  validateAuctionSignals(auctionSignals);
  const ad = interestGroup.ads[0];
  return {'ad': ad, 'bid': 1, 'render': ad.renderUrl};
}

function validateAuctionSignals(auctionSignals) {
  const auctionSignalsJSON = JSON.stringify(auctionSignals);
  if (auctionSignalsJSON !== '3')
    throw 'Wrong auctionSignals ' + auctionSignalsJSON;
}

)";

  constexpr char kDecisionLogicScript[] = R"(
function scoreAd(
    adMetadata, bid, auctionConfig, unusedTrustedScoringSignals,
    unusedBrowserSignals) {
  validateAuctionConfig(auctionConfig);
  return bid;
}

function validateAuctionConfig(auctionConfig) {
  const auctionSignalsJSON = JSON.stringify(auctionConfig.auctionSignals);
  if (auctionSignalsJSON !== '3')
    throw 'Wrong auctionSignals ' + auctionSignalsJSON;
  const sellerSignalsJSON = JSON.stringify(auctionConfig.sellerSignals);
  if (sellerSignalsJSON !== '4')
    throw 'Wrong sellerSignals ' + sellerSignalsJSON;
}
)";

  network_responder_->RegisterNetworkResponse(
      kBiddingLogicPath, kBiddingLogicScript, "application/javascript");
  network_responder_->RegisterNetworkResponse(
      kDecisionLogicPath, kDecisionLogicScript, "application/javascript");

  EXPECT_EQ(
      "done",
      EvalJs(shell(), JsReplace(
                          R"(
(async function() {
  try {
    await navigator.joinAdInterestGroup(
        {
          name: 'cars',
          owner: $1,
          biddingLogicUrl: $2,
          ads: [{renderUrl:"https://example.com/render", metadata:2}],
        },
        /*joinDurationSec=*/100);
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())",
                          test_origin,
                          https_server_->GetURL("a.test", kBiddingLogicPath))));

  std::string script = JsReplace(
      R"(
        (async function() {
          let controller = new AbortController();
          let adPromise =  navigator.runAdAuction({
            seller: $1,
            decisionLogicUrl: $2,
            interestGroupBuyers: [$1],
            auctionSignals: new Promise((resolve, reject) => {
              setTimeout(
                  () => { resolve(3); }, 1)
            }),
            sellerSignals: new Promise((resolve, reject) => {
              setTimeout(
                  () => { resolve(4); }, 1)
            }),
            perBuyerSignals: {$1: 5},
            signal: controller.signal
          });
          controller.abort('manual cancel');
          return await adPromise;
        })())",
      test_origin, https_server_->GetURL("a.test", kDecisionLogicPath));
  EXPECT_EQ("a JavaScript error: \"manual cancel\"\n",
            EvalJs(shell(), script).error);
}

// Test for auctionSignals, perBuyerSignals, directFromSellerSignals, and
// sellerSignals being passed to runAdAuction as promises... which resolve to
// nothing.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, PromiseSignalsNothing) {
  // These scripts are generated by this test.
  constexpr char kBiddingLogicPath[] =
      "/interest_group/test_generated_bidding_argument_validator.js";
  constexpr char kDecisionLogicPath[] =
      "/interest_group/test_generated_decision_argument_validator.js";
  const GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);

  constexpr char kBiddingLogicScript[] = R"(
function generateBid(
    interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
    unusedBrowserSignals, directFromSellerSignals) {
  validateAuctionSignals(auctionSignals);
  validateDirectFromSellerSignals(directFromSellerSignals);
  const ad = interestGroup.ads[0];
  if (perBuyerSignals !== null)
    throw 'perBuyerSignals in generateBid not null!';
  return {'ad': ad, 'bid': 1, 'render': ad.renderUrl};
}

function validateAuctionSignals(auctionSignals) {
  if (auctionSignals !== null)
    throw 'auctionSignals in generateBid not null!';
}

function validateDirectFromSellerSignals(directFromSellerSignals) {
  const perBuyerSignalsJSON =
      JSON.stringify(directFromSellerSignals.perBuyerSignals);
  if (perBuyerSignalsJSON !== 'null') {
    throw 'Wrong directFromSellerSignals.perBuyerSignals ' +
        perBuyerSignalsJSON;
  }
  const auctionSignalsJSON =
      JSON.stringify(directFromSellerSignals.auctionSignals);
  if (auctionSignalsJSON !== 'null') {
    throw 'Wrong directFromSellerSignals.auctionSignals ' +
        auctionSignalsJSON;
  }
}

)";

  constexpr char kDecisionLogicScript[] = R"(
function scoreAd(
    adMetadata, bid, auctionConfig, unusedTrustedScoringSignals,
    unusedBrowserSignals, directFromSellerSignals) {
  validateAuctionConfig(auctionConfig);
  validateDirectFromSellerSignals(directFromSellerSignals);
  return bid;
}

function validateAuctionConfig(auctionConfig) {
  if ('auctionSignals' in auctionConfig)
    throw 'Have auctionSignals in scoreAd auctionConfig!';
  if ('sellerSignals' in auctionConfig)
    throw 'Have sellerSignals in scoreAd auctionConfig!';
  if ('perBuyerSignals' in auctionConfig)
    throw 'Have perBuyerSignals in scoreAd auctionConfig!';
  if ('directFromSellerSignals' in auctionConfig)
    throw 'Have directFromSellerSignals in scoreAd auctionConfig!';
}

function validateDirectFromSellerSignals(directFromSellerSignals) {
  const sellerSignalsJSON =
      JSON.stringify(directFromSellerSignals.sellerSignals);
  if (sellerSignalsJSON !== 'null') {
    throw 'Wrong directFromSellerSignals.sellerSignals ' +
        sellerSignalsJSON;
  }
  const auctionSignalsJSON =
      JSON.stringify(directFromSellerSignals.auctionSignals);
  if (auctionSignalsJSON !== 'null') {
    throw 'Wrong directFromSellerSignals.auctionSignals ' +
        auctionSignalsJSON;
  }
}
)";

  network_responder_->RegisterNetworkResponse(
      kBiddingLogicPath, kBiddingLogicScript, "application/javascript");
  network_responder_->RegisterNetworkResponse(
      kDecisionLogicPath, kDecisionLogicScript, "application/javascript");

  EXPECT_EQ(
      "done",
      EvalJs(shell(), JsReplace(
                          R"(
(async function() {
  try {
    await navigator.joinAdInterestGroup(
        {
          name: 'cars',
          owner: $1,
          biddingLogicUrl: $2,
          ads: [{renderUrl:"https://example.com/render", metadata:2}],
        },
        /*joinDurationSec=*/100);
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())",
                          test_origin,
                          https_server_->GetURL("a.test", kBiddingLogicPath))));

  TestFencedFrameURLMappingResultObserver observer;
  ConvertFencedFrameURNToURL(
      GURL(EvalJs(shell(),
                  JsReplace(
                      R"(
(async function() {
  return await navigator.runAdAuction({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
    auctionSignals: new Promise((resolve, reject) => {
      setTimeout(
          () => { resolve(); }, 1)
    }),
    sellerSignals: new Promise((resolve, reject) => {
      setTimeout(
          () => { resolve(undefined); }, 1)
    }),
    perBuyerSignals: new Promise((resolve, reject) => {
      setTimeout(
          () => { resolve(undefined); }, 1)
    }),
    directFromSellerSignals: new Promise((resolve, reject) => {
      setTimeout(
          () => { resolve("null"); }, 1)
    }),
  });
})())",
                      test_origin,
                      https_server_->GetURL("a.test", kDecisionLogicPath)))
               .ExtractString()),
      &observer);

  EXPECT_EQ(GURL("https://example.com/render"), observer.mapped_url());
}

// Test for perBuyerTimeouts and perBuyerCumulativeTimeouts being passed to
// runAdAuction as promises.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       PromiseBuyerTimeoutsAndCumulativeBuyerTimeouts) {
  // These scripts are generated by this test.
  constexpr char kBiddingLogicPath[] =
      "/interest_group/test_generated_bidding_argument_validator.js";
  constexpr char kDecisionLogicPath[] =
      "/interest_group/test_generated_decision_argument_validator.js";
  const GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);

  // In the below JavaScript, if fields are incorrectly passed in as a string
  // ("2") instead of a number (2), JSON.stringify() will wrap it in another
  // layer of quotes, causing the test to fail.

  constexpr char kBiddingLogicScript[] = R"(
function generateBid(
    interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
    unusedBrowserSignals) {
  const ad = interestGroup.ads[0];
  return {'ad': ad, 'bid': 1, 'render': ad.renderUrl};
}

)";

  constexpr char kDecisionLogicScript[] = R"(
function scoreAd(
    adMetadata, bid, auctionConfig, unusedTrustedScoringSignals,
    unusedBrowserSignals) {
  validatePerBuyerTimeouts(auctionConfig.perBuyerTimeouts);
  validatePerBuyerCumulativeTimeouts(auctionConfig.perBuyerCumulativeTimeouts);
  return bid;
}

function validatePerBuyerTimeouts(perBuyerTimeouts) {
  const perBuyerTimeoutsJSON = JSON.stringify(perBuyerTimeouts);
  let ok = 0;
  for (key in perBuyerTimeouts) {
    if (key.startsWith("https://a.test") && perBuyerTimeouts[key] === 50) {
      ++ok;
    } else if (key === "https://b.test" && perBuyerTimeouts[key] === 60) {
      ++ok;
    } else if (key === '*' &&
        perBuyerTimeouts[key] === 56) {
      ++ok;
    } else {
      throw 'Wrong key in perBuyerTimeouts ' + perBuyerTimeoutsJSON;
    }
  }
  if (ok !== 3) {
    throw 'Wrong perBuyerTimeouts ' + perBuyerTimeoutsJSON;
  }
}

function validatePerBuyerCumulativeTimeouts(perBuyerCumulativeTimeouts) {
  const perBuyerCumulativeTimeoutsJSON =
      JSON.stringify(perBuyerCumulativeTimeouts);
  let ok = 0;
  for (key in perBuyerCumulativeTimeouts) {
    if (key.startsWith("https://a.test") &&
        perBuyerCumulativeTimeouts[key] === 7000) {
      ++ok;
    } else if (key === "https://c.test" &&
        perBuyerCumulativeTimeouts[key] === 8000) {
      ++ok;
    } else if (key === '*' &&
        perBuyerCumulativeTimeouts[key] === 7600) {
      ++ok;
    } else {
      throw 'Wrong key in perCumulativeBuyerTimeouts ' +
          perBuyerCumulativeTimeoutsJSON;
    }
  }
  if (ok !== 3) {
    throw 'Wrong perCumulativeBuyerTimeouts ' + perBuyerCumulativeTimeoutsJSON;
  }
}
)";

  network_responder_->RegisterNetworkResponse(
      kBiddingLogicPath, kBiddingLogicScript, "application/javascript");
  network_responder_->RegisterNetworkResponse(
      kDecisionLogicPath, kDecisionLogicScript, "application/javascript");

  EXPECT_EQ(
      "done",
      EvalJs(shell(), JsReplace(
                          R"(
(async function() {
  try {
    await navigator.joinAdInterestGroup(
        {
          name: 'cars',
          owner: $1,
          biddingLogicUrl: $2,
          ads: [{renderUrl:"https://example.com/render", metadata:2}],
        },
        /*joinDurationSec=*/100);
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())",
                          test_origin,
                          https_server_->GetURL("a.test", kBiddingLogicPath))));

  TestFencedFrameURLMappingResultObserver observer;
  ConvertFencedFrameURNToURL(
      GURL(EvalJs(shell(),
                  JsReplace(
                      R"(
(async function() {
  return await navigator.runAdAuction({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
    perBuyerTimeouts: new Promise((resolve, reject) => {
      setTimeout(
          () => { resolve({$1: 50, 'https://b.test': 60, '*': 56}); }, 1)
    }),
    perBuyerCumulativeTimeouts: new Promise((resolve, reject) => {
      setTimeout(
          () => { resolve({$1: 7000, 'https://c.test': 8000, '*': 7600}); }, 1)
    })
  });
})())",
                      test_origin,
                      https_server_->GetURL("a.test", kDecisionLogicPath)))
               .ExtractString()),
      &observer);

  EXPECT_EQ(GURL("https://example.com/render"), observer.mapped_url());
}

// Make sure that qutting with a live auction doesn't crash.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, QuitWithRunningAuction) {
  URLLoaderMonitor url_loader_monitor;

  GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  GURL hanging_url = https_server_->GetURL("a.test", "/hung");
  url::Origin hanging_origin = url::Origin::Create(hanging_url);

  EXPECT_EQ(kSuccess, JoinInterestGroupAndVerify(
                          blink::TestInterestGroupBuilder(
                              /*owner=*/hanging_origin,
                              /*name=*/"cars")
                              .SetBiddingUrl(hanging_url)
                              .SetAds({{{GURL("https://example.com/render"),
                                         R"({"ad":"metadata","here":[1,2]})"}}})
                              .Build()));

  ExecuteScriptAsync(shell(), JsReplace(R"(
navigator.runAdAuction({
  seller: $1,
  decisionLogicUrl: $2,
  interestGroupBuyers: [$1]
});
                                        )",
                                        hanging_origin, hanging_url));

  WaitForUrl(https_server_->GetURL("/hung"));
}

// These tests validate the `updateUrl` and navigator.updateAdInterestGroups()
// functionality.

// The server JSON updates a number of updatable fields.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, Update) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  url::Origin test_origin = url::Origin::Create(test_url);
  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  // The server JSON updates all fields that can be updated.
  constexpr char kUpdateUrlPath[] = "/interest_group/update_partial.json";
  network_responder_->RegisterNetworkResponse(
      kUpdateUrlPath, base::StringPrintf(R"({
"biddingLogicUrl": "%s/interest_group/new_bidding_logic.js",
"trustedBiddingSignalsUrl":
  "%s/interest_group/new_trusted_bidding_signals_url.json",
"trustedBiddingSignalsKeys": ["new_key"],
"executionMode": "group-by-origin",
"ads": [{"renderUrl": "%s/new_ad_render_url",
         "metadata": {"new_a": "b"}
        }]
})",
                                         test_origin.Serialize().c_str(),
                                         test_origin.Serialize().c_str(),
                                         test_origin.Serialize().c_str()));

  ASSERT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/test_origin,
          /*name=*/"cars",
          /*priority=*/0.0, /*enable_bidding_signals_prioritization=*/false,
          /*priority_vector=*/{{{"one", 1}}},
          /*priority_signals_overrides=*/{{{"two", 2}}},
          /*seller_capabilities=*/absl::nullopt,
          /*all_sellers_capabilities=*/
          {}, /*execution_mode=*/
          blink::InterestGroup::ExecutionMode::kCompatibilityMode,
          /*bidding_url=*/
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /*bidding_wasm_helper_url=*/absl::nullopt,
          /*update_url=*/
          https_server_->GetURL("a.test", kUpdateUrlPath),
          /*trusted_bidding_signals_url=*/
          https_server_->GetURL("a.test",
                                "/interest_group/trusted_bidding_signals.json"),
          /*trusted_bidding_signals_keys=*/{{"key1"}},
          /*user_bidding_signals=*/R"({"some":"json","stuff":{"here":[1,2]}})",
          /*ads=*/
          {{{GURL("https://example.com/render"),
             R"({"ad":"metadata","here":[1,2,3]})"}}},
          /*ad_components=*/absl::nullopt,
          /*ad_sizes=*/{},
          /*size_groups=*/{})));

  EXPECT_EQ("done", UpdateInterestGroupsInJS());

  WaitForInterestGroupsSatisfying(
      test_origin,
      base::BindLambdaForTesting([](const std::vector<StorageInterestGroup>&
                                        groups) {
        if (groups.size() != 1) {
          return false;
        }
        const auto& group = groups[0].interest_group;
        return group.name == "cars" && group.priority == 0.0 &&
               group.execution_mode ==
                   blink::InterestGroup::ExecutionMode::kGroupedByOriginMode &&
               group.bidding_url.has_value() &&
               group.bidding_url->path() ==
                   "/interest_group/new_bidding_logic.js" &&
               group.trusted_bidding_signals_url.has_value() &&
               group.trusted_bidding_signals_url->path() ==
                   "/interest_group/new_trusted_bidding_signals_url.json" &&
               group.trusted_bidding_signals_keys.has_value() &&
               group.trusted_bidding_signals_keys->size() == 1 &&
               group.trusted_bidding_signals_keys.value()[0] == "new_key" &&
               group.ads.has_value() && group.ads->size() == 1 &&
               group.ads.value()[0].render_url.path() == "/new_ad_render_url" &&
               group.ads.value()[0].metadata == "{\"new_a\":\"b\"}";
      }));
}

// The server JSON updates a number of updatable fields, using the deprecated
// `dailyUpdateUrl` field.
//
// TODO(https://crbug.com/1420080): Remove once support for `dailyUpdateUrl` is
// removed.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, DeprecatedDailyUpdateUrl) {
  set_daily_update_url_ = true;
  set_update_url_ = false;

  GURL test_url = https_server_->GetURL("a.test", "/echo");
  url::Origin test_origin = url::Origin::Create(test_url);
  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  // The server JSON updates all fields that can be updated.
  constexpr char kUpdateUrlPath[] = "/interest_group/update_partial.json";
  network_responder_->RegisterNetworkResponse(
      kUpdateUrlPath, base::StringPrintf(R"({
"biddingLogicUrl": "%s/interest_group/new_bidding_logic.js",
"trustedBiddingSignalsUrl":
  "%s/interest_group/new_trusted_bidding_signals_url.json",
"trustedBiddingSignalsKeys": ["new_key"],
"executionMode": "groupByOrigin",
"ads": [{"renderUrl": "%s/new_ad_render_url",
         "metadata": {"new_a": "b"}
        }]
})",
                                         test_origin.Serialize().c_str(),
                                         test_origin.Serialize().c_str(),
                                         test_origin.Serialize().c_str()));

  ASSERT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/test_origin,
          /*name=*/"cars",
          /*priority=*/0.0, /*enable_bidding_signals_prioritization=*/false,
          /*priority_vector=*/{{{"one", 1}}},
          /*priority_signals_overrides=*/{{{"two", 2}}},
          /*seller_capabilities=*/absl::nullopt,
          /*all_sellers_capabilities=*/
          {}, /*execution_mode=*/
          blink::InterestGroup::ExecutionMode::kCompatibilityMode,
          /*bidding_url=*/
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /*bidding_wasm_helper_url=*/absl::nullopt,
          /*update_url=*/
          https_server_->GetURL("a.test", kUpdateUrlPath),
          /*trusted_bidding_signals_url=*/
          https_server_->GetURL("a.test",
                                "/interest_group/trusted_bidding_signals.json"),
          /*trusted_bidding_signals_keys=*/{{"key1"}},
          /*user_bidding_signals=*/R"({"some":"json","stuff":{"here":[1,2]}})",
          /*ads=*/
          {{{GURL("https://example.com/render"),
             R"({"ad":"metadata","here":[1,2,3]})"}}},
          /*ad_components=*/absl::nullopt,
          /*ad_sizes=*/{},
          /*size_groups=*/{})));

  EXPECT_EQ("done", UpdateInterestGroupsInJS());

  WaitForInterestGroupsSatisfying(
      test_origin,
      base::BindLambdaForTesting([](const std::vector<StorageInterestGroup>&
                                        groups) {
        if (groups.size() != 1)
          return false;
        const auto& group = groups[0].interest_group;
        return group.name == "cars" && group.priority == 0.0 &&
               group.execution_mode ==
                   blink::InterestGroup::ExecutionMode::kGroupedByOriginMode &&
               group.bidding_url.has_value() &&
               group.bidding_url->path() ==
                   "/interest_group/new_bidding_logic.js" &&
               group.trusted_bidding_signals_url.has_value() &&
               group.trusted_bidding_signals_url->path() ==
                   "/interest_group/new_trusted_bidding_signals_url.json" &&
               group.trusted_bidding_signals_keys.has_value() &&
               group.trusted_bidding_signals_keys->size() == 1 &&
               group.trusted_bidding_signals_keys.value()[0] == "new_key" &&
               group.ads.has_value() && group.ads->size() == 1 &&
               group.ads.value()[0].render_url.path() == "/new_ad_render_url" &&
               group.ads.value()[0].metadata == "{\"new_a\":\"b\"}";
      }));
}

// The server JSON updates a number of updatable fields, using the deprecated
// `dailyUpdateUrl` field, and the `updateUrl` field. Both have the same value.
// This is what consumers are expected to due during migration from one name to
// the other, so best to make sure it works.
//
// TODO(https://crbug.com/1420080): Remove once support for `dailyUpdateUrl` is
// removed.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       UpdateUrlAndDeprecatedDailyUpdateUrl) {
  set_daily_update_url_ = true;
  set_update_url_ = true;

  GURL test_url = https_server_->GetURL("a.test", "/echo");
  url::Origin test_origin = url::Origin::Create(test_url);
  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  // The server JSON updates all fields that can be updated.
  constexpr char kUpdateUrlPath[] = "/interest_group/update_partial.json";
  network_responder_->RegisterNetworkResponse(
      kUpdateUrlPath, base::StringPrintf(R"({
"biddingLogicUrl": "%s/interest_group/new_bidding_logic.js",
"trustedBiddingSignalsUrl":
  "%s/interest_group/new_trusted_bidding_signals_url.json",
"trustedBiddingSignalsKeys": ["new_key"],
"executionMode": "groupByOrigin",
"ads": [{"renderUrl": "%s/new_ad_render_url",
         "metadata": {"new_a": "b"}
        }]
})",
                                         test_origin.Serialize().c_str(),
                                         test_origin.Serialize().c_str(),
                                         test_origin.Serialize().c_str()));

  ASSERT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/test_origin,
          /*name=*/"cars",
          /*priority=*/0.0, /*enable_bidding_signals_prioritization=*/false,
          /*priority_vector=*/{{{"one", 1}}},
          /*priority_signals_overrides=*/{{{"two", 2}}},
          /*seller_capabilities=*/absl::nullopt,
          /*all_sellers_capabilities=*/
          {}, /*execution_mode=*/
          blink::InterestGroup::ExecutionMode::kCompatibilityMode,
          /*bidding_url=*/
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /*bidding_wasm_helper_url=*/absl::nullopt,
          /*update_url=*/
          https_server_->GetURL("a.test", kUpdateUrlPath),
          /*trusted_bidding_signals_url=*/
          https_server_->GetURL("a.test",
                                "/interest_group/trusted_bidding_signals.json"),
          /*trusted_bidding_signals_keys=*/{{"key1"}},
          /*user_bidding_signals=*/R"({"some":"json","stuff":{"here":[1,2]}})",
          /*ads=*/
          {{{GURL("https://example.com/render"),
             R"({"ad":"metadata","here":[1,2,3]})"}}},
          /*ad_components=*/absl::nullopt,
          /*ad_sizes=*/{},
          /*size_groups=*/{})));

  EXPECT_EQ("done", UpdateInterestGroupsInJS());

  WaitForInterestGroupsSatisfying(
      test_origin,
      base::BindLambdaForTesting([](const std::vector<StorageInterestGroup>&
                                        groups) {
        if (groups.size() != 1) {
          return false;
        }
        const auto& group = groups[0].interest_group;
        return group.name == "cars" && group.priority == 0.0 &&
               group.execution_mode ==
                   blink::InterestGroup::ExecutionMode::kGroupedByOriginMode &&
               group.bidding_url.has_value() &&
               group.bidding_url->path() ==
                   "/interest_group/new_bidding_logic.js" &&
               group.trusted_bidding_signals_url.has_value() &&
               group.trusted_bidding_signals_url->path() ==
                   "/interest_group/new_trusted_bidding_signals_url.json" &&
               group.trusted_bidding_signals_keys.has_value() &&
               group.trusted_bidding_signals_keys->size() == 1 &&
               group.trusted_bidding_signals_keys.value()[0] == "new_key" &&
               group.ads.has_value() && group.ads->size() == 1 &&
               group.ads.value()[0].render_url.path() == "/new_ad_render_url" &&
               group.ads.value()[0].metadata == "{\"new_a\":\"b\"}";
      }));
}

// Updates can proceed even if the page that started the update isn't running
// anymore.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       UpdateAndNavigateAwayStillCompletes) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  url::Origin test_origin = url::Origin::Create(test_url);
  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  // Start an update, then navigate to a different page. The update completes
  // even though the page that started the update is gone.
  constexpr char kUpdateUrlPath[] = "/interest_group/update_partial.json";
  network_responder_->RegisterNetworkResponse(
      kUpdateUrlPath, base::StringPrintf(R"({
"ads": [{"renderUrl": "%s/new_ad_render_url",
         "metadata": {"new_a": "b"}
        }]
})",
                                         test_origin.Serialize().c_str()));

  ASSERT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/test_origin,
          /*name=*/"cars",
          /*priority=*/0.0, /*enable_bidding_signals_prioritization=*/false,
          /*priority_vector=*/absl::nullopt,
          /*priority_signals_overrides=*/absl::nullopt,
          /*seller_capabilities=*/absl::nullopt,
          /*all_sellers_capabilities=*/
          {}, /*execution_mode=*/
          blink::InterestGroup::ExecutionMode::kCompatibilityMode,
          /*bidding_url=*/
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /*bidding_wasm_helper_url=*/absl::nullopt,
          /*update_url=*/
          https_server_->GetURL("a.test", kUpdateUrlPath),
          /*trusted_bidding_signals_url=*/
          https_server_->GetURL("a.test",
                                "/interest_group/trusted_bidding_signals.json"),
          /*trusted_bidding_signals_keys=*/{{"key1"}},
          /*user_bidding_signals=*/R"({"some":"json","stuff":{"here":[1,2]}})",
          /*ads=*/
          {{{GURL("https://example.com/render"),
             R"({"ad":"metadata","here":[1,2,3]})"}}},
          /*ad_components=*/absl::nullopt,
          /*ad_sizes=*/{},
          /*size_groups=*/{})));

  EXPECT_EQ("done", UpdateInterestGroupsInJS());

  // Navigate away -- the update should still continue.
  GURL test_url_b = https_server_->GetURL("b.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url_b));

  WaitForInterestGroupsSatisfying(
      test_origin,
      base::BindLambdaForTesting(
          [](const std::vector<StorageInterestGroup>& groups) {
            if (groups.size() != 1)
              return false;
            const auto& group = groups[0].interest_group;
            return group.name == "cars" && group.bidding_url.has_value() &&
                   group.bidding_url->path() ==
                       "/interest_group/bidding_logic.js" &&
                   group.update_url.has_value() &&
                   group.update_url->path() ==
                       "/interest_group/update_partial.json" &&
                   group.trusted_bidding_signals_url.has_value() &&
                   group.trusted_bidding_signals_url->path() ==
                       "/interest_group/trusted_bidding_signals.json" &&
                   group.trusted_bidding_signals_keys.has_value() &&
                   group.trusted_bidding_signals_keys->size() == 1 &&
                   group.trusted_bidding_signals_keys.value()[0] == "key1" &&
                   group.ads.has_value() && group.ads->size() == 1 &&
                   group.ads.value()[0].render_url.path() ==
                       "/new_ad_render_url" &&
                   group.ads.value()[0].metadata == "{\"new_a\":\"b\"}";
          }));
}

// Bidders' generateBid() scripts that run forever should timeout. They will not
// affect other bidders or fail the auction.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionWithCustomPerBuyerTimeouts) {
  const char kHostA[] = "a.test";
  const char kHostB[] = "b.test";
  // Navigate to other bidder site, and add an interest group.
  GURL bidder_b_url = https_server_->GetURL(kHostB, "/echo");
  url::Origin bidder_b_origin = url::Origin::Create(bidder_b_url);
  ASSERT_TRUE(NavigateToURL(shell(), bidder_b_url));

  GURL ad_url_b = https_server_->GetURL(kHostB, "/echo?render_shoes");
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          /*owner=*/bidder_b_origin,
          /*name=*/"shoes",
          /*priority=*/0.0,
          /*execution_mode=*/
          blink::InterestGroup::ExecutionMode::kCompatibilityMode,
          /*bidding_url=*/
          https_server_->GetURL(kHostB, "/interest_group/bidding_logic.js"),
          /*ads=*/{{{ad_url_b, /*metadata=*/absl::nullopt}}}));

  GURL bidder_a_url = https_server_->GetURL(kHostA, "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), bidder_a_url));
  url::Origin bidder_a_origin = url::Origin::Create(bidder_a_url);
  GURL ad1_url_a = https_server_->GetURL(kHostA, "/echo?render_cars");
  GURL ad2_url_a = https_server_->GetURL(kHostA, "/echo?render_bikes");

  EXPECT_EQ(kSuccess,
            JoinInterestGroupAndVerify(
                /*owner=*/bidder_a_origin,
                /*name=*/"cars",
                /*priority=*/0.0,
                /*execution_mode=*/
                blink::InterestGroup::ExecutionMode::kCompatibilityMode,
                /*bidding_url=*/
                https_server_->GetURL(
                    kHostA, "/interest_group/bidding_logic_loop_forever.js"),
                /*ads=*/{{{ad1_url_a, /*metadata=*/absl::nullopt}}}));
  EXPECT_EQ(kSuccess,
            JoinInterestGroupAndVerify(
                /*owner=*/bidder_a_origin,
                /*name=*/"bikes",
                /*priority=*/0.0, /*execution_mode=*/
                blink::InterestGroup::ExecutionMode::kCompatibilityMode,
                /*bidding_url=*/
                https_server_->GetURL(
                    kHostA, "/interest_group/bidding_logic_loop_forever.js"),
                /*ads=*/{{{ad2_url_a, /*metadata=*/absl::nullopt}}}));

  // Set per buyer timeout of bidder a to 1 ms, so that its generateBid()
  // scripts which has an endless loop times out fast.
  const std::string kTestPerBuyerTimeouts[] = {
      JsReplace("{$1: 1}", bidder_a_origin),
      JsReplace("{$1: 1, '*': 100}", bidder_a_origin),
      JsReplace("{$1: 100, '*': 1}", bidder_b_origin),
  };

  for (const auto& test_per_buyer_timeouts : kTestPerBuyerTimeouts) {
    std::string auction_config = JsReplace(
        R"({
      seller: $1,
      decisionLogicUrl: $2,
      interestGroupBuyers: [$1, $3],
                  )",
        bidder_a_origin,
        https_server_->GetURL("a.test", "/interest_group/decision_logic.js"),
        bidder_b_origin);
    // Since test_per_buyer_timeout is JSON, it shouldn't be wrapped in quotes,
    // so can't use JsReplace.
    auction_config += base::StringPrintf("perBuyerTimeouts: %s}",
                                         test_per_buyer_timeouts.c_str());
    // Bidder b won the auction.
    RunAuctionAndWaitForURLAndNavigateIframe(auction_config, ad_url_b);
  }
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionWithCustomSellerTimeout) {
  const char kHostA[] = "a.test";
  GURL test_url = https_server_->GetURL(kHostA, "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);
  GURL ad_url = https_server_->GetURL(kHostA, "/echo?render_cars");

  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          /*owner=*/test_origin,
          /*name=*/"cars",
          /*priority=*/0.0,
          /*execution_mode=*/
          blink::InterestGroup::ExecutionMode::kCompatibilityMode,
          /*bidding_url=*/
          https_server_->GetURL(kHostA, "/interest_group/bidding_logic.js"),
          /*ads=*/{{{ad_url, /*metadata=*/absl::nullopt}}}));

  // The auction fails, since seller's scoreAd() script times out after 1 ms.
  EXPECT_EQ(
      nullptr,
      RunAuctionAndWait(JsReplace(
          R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
    sellerTimeout: 1,
                })",
          test_origin,
          https_server_->GetURL(
              "a.test", "/interest_group/decision_logic_loop_forever.js"))));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionWithExperimentGroupId) {
  const char kPublisher[] = "a.test";
  const char kBidder[] = "b.test";
  const char kSeller[] = "c.test";

  // Navigate to bidder site, and add an interest group.
  GURL bidder_url = https_server_->GetURL(kBidder, "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), bidder_url));
  url::Origin bidder_origin = url::Origin::Create(bidder_url);
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          blink::TestInterestGroupBuilder(
              /*owner=*/bidder_origin,
              /*name=*/"cars")
              .SetBiddingUrl(https_server_->GetURL(
                  kBidder, "/interest_group/bidding_logic.js"))
              .SetTrustedBiddingSignalsUrl(https_server_->GetURL(
                  kBidder, "/interest_group/trusted_bidding_signals.json"))
              .SetTrustedBiddingSignalsKeys({{"key1"}})
              .SetAds({{{GURL("https://example.com/render"),
                         R"({"ad":"metadata","here":[1,2]})"}}})
              .Build()));

  // Navigate to publisher.
  ASSERT_TRUE(
      NavigateToURL(shell(), https_server_->GetURL(kPublisher, "/echo")));
  GURL seller_logic_url =
      https_server_->GetURL(kSeller, "/interest_group/decision_logic.js");

  const char kAuctionConfigTemplate[] = R"({
    seller: $1,
    decisionLogicUrl: $2,
    trustedScoringSignalsUrl: $3,
    interestGroupBuyers: [$4],
    sellerExperimentGroupId: 8349,
    perBuyerExperimentGroupIds: {'*': 3498},
  })";

  EXPECT_EQ("https://example.com/render",
            RunAuctionAndWaitForUrl(JsReplace(
                kAuctionConfigTemplate, url::Origin::Create(seller_logic_url),
                seller_logic_url,
                https_server_->GetURL(
                    kSeller, "/interest_group/trusted_scoring_signals.json"),
                bidder_origin)));

  // Make sure that the right trusted signals URLs got fetched, incorporating
  // the experiment group ID.
  WaitForUrl(https_server_->GetURL(
      "/interest_group/trusted_bidding_signals.json?hostname=a.test&keys=key1"
      "&interestGroupNames=cars&experimentGroupId=3498"));
  WaitForUrl(https_server_->GetURL(
      "/interest_group/trusted_scoring_signals.json?hostname=a.test"
      "&renderUrls=https%3A%2F%2Fexample.com%2Frender&experimentGroupId=8349"));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionWithPerBuyerExperimentGroupId) {
  const char kPublisher[] = "a.test";
  const char kBidder[] = "b.test";
  const char kBidder2[] = "d.test";
  const char kSeller[] = "c.test";

  // Navigate to bidder site, and add an interest group, then same for bidder 2.
  GURL bidder_url = https_server_->GetURL(kBidder, "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), bidder_url));
  url::Origin bidder_origin = url::Origin::Create(bidder_url);
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          blink::TestInterestGroupBuilder(
              /*owner=*/bidder_origin,
              /*name=*/"cars")
              .SetBiddingUrl(https_server_->GetURL(
                  kBidder, "/interest_group/bidding_logic.js"))
              .SetTrustedBiddingSignalsUrl(https_server_->GetURL(
                  kBidder, "/interest_group/trusted_bidding_signals.json"))
              .SetTrustedBiddingSignalsKeys({{"key1"}})
              .SetAds({{{GURL("https://example.com/render"),
                         R"({"ad":"metadata","here":[1,2]})"}}})
              .Build()));

  GURL bidder2_url = https_server_->GetURL(kBidder2, "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), bidder2_url));
  url::Origin bidder2_origin = url::Origin::Create(bidder2_url);
  content_browser_client_->AddToAllowList({bidder2_origin});
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          blink::TestInterestGroupBuilder(
              /*owner=*/bidder2_origin,
              /*name=*/"cars_and_trucks")
              .SetBiddingUrl(https_server_->GetURL(
                  kBidder2, "/interest_group/bidding_logic.js"))
              .SetTrustedBiddingSignalsUrl(https_server_->GetURL(
                  kBidder2, "/interest_group/trusted_bidding_signals.json"))
              .SetTrustedBiddingSignalsKeys({{"key2"}})
              .SetAds({{{GURL("https://example.com/render"),
                         R"({"ad":"metadata","here":[1,2]})"}}})
              .Build()));

  // Navigate to publisher.
  ASSERT_TRUE(
      NavigateToURL(shell(), https_server_->GetURL(kPublisher, "/echo")));
  GURL seller_logic_url =
      https_server_->GetURL(kSeller, "/interest_group/decision_logic.js");

  const char kAuctionConfigTemplate[] = R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$3, $4],
    perBuyerExperimentGroupIds: {'*': 3498,
                                 $4: 1203},
  })";

  EXPECT_EQ("https://example.com/render",
            RunAuctionAndWaitForUrl(JsReplace(
                kAuctionConfigTemplate, url::Origin::Create(seller_logic_url),
                seller_logic_url, bidder_origin, bidder2_origin)));

  // Make sure that the right trusted signals URLs got fetched, incorporating
  // the experiment group IDs.
  WaitForUrl(https_server_->GetURL(
      "/interest_group/trusted_bidding_signals.json?hostname=a.test&keys=key1"
      "&interestGroupNames=cars&experimentGroupId=3498"));
  WaitForUrl(https_server_->GetURL(
      "/interest_group/trusted_bidding_signals.json?hostname=a.test&keys=key2"
      "&interestGroupNames=cars_and_trucks&experimentGroupId=1203"));
}

// Validate that createAdRequest is available and be successfully called as part
// of PARAKEET.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, CreateAdRequestWorks) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  EXPECT_EQ("NotSupportedError: createAdRequest API not yet implemented",
            CreateAdRequestAndWait());
}

// Validate that finalizeAd is available and be successfully called as part of
// PARAKEET.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, FinalizeAdWorks) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  // The finalize API relies on createAdRequest, until it is fully implemented
  // we expect a createAdRequest failure initially.
  EXPECT_EQ("NotSupportedError: createAdRequest API not yet implemented",
            FinalizeAdAndWait());
}

// The bidder worklet is served from a private network, everything else from a
// public network. The auction should fail.
IN_PROC_BROWSER_TEST_F(InterestGroupLocalNetworkBrowserTest,
                       BidderOnLocalNetwork) {
  URLLoaderMonitor url_loader_monitor;

  // Learn the bidder IG, served from the local server.
  GURL bidder_url =
      https_server_->GetURL("b.test", "/interest_group/bidding_logic.js");
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("b.test", "/echo")));
  url::Origin bidder_origin = url::Origin::Create(bidder_url);
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          /*owner=*/bidder_origin,
          /*name=*/"Cthulhu", /*priority=*/0.0, /*execution_mode=*/
          blink::InterestGroup::ExecutionMode::kCompatibilityMode, bidder_url,
          /*ads=*/
          {{{GURL("https://example.com/render"),
             /*metadata=*/absl::nullopt}}}));

  // Use `remote_test_server_` for all other URLs.
  GURL test_url = remote_test_server_.GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  EXPECT_EQ(nullptr, RunAuctionAndWait(JsReplace(
                         R"(
{
  seller: $1,
  decisionLogicUrl: $2,
  interestGroupBuyers: [$3]
}
                         )",
                         url::Origin::Create(test_url),
                         remote_test_server_.GetURL(
                             "a.test", "/interest_group/decision_logic.js"),
                         bidder_origin)));

  // The URLLoaderMonitor should have seen a request for the bidder URL, which
  // should have been made from a public address space.
  absl::optional<network::ResourceRequest> bidder_request =
      url_loader_monitor.GetRequestInfo(bidder_url);
  ASSERT_TRUE(bidder_request);
  EXPECT_EQ(
      network::mojom::IPAddressSpace::kPublic,
      bidder_request->trusted_params->client_security_state->ip_address_space);

  const network::URLLoaderCompletionStatus& bidder_status =
      url_loader_monitor.WaitForRequestCompletion(bidder_url);
  EXPECT_EQ(net::ERR_FAILED, bidder_status.error_code);
  EXPECT_THAT(bidder_status.cors_error_status,
              Optional(network::CorsErrorStatus(
                  network::mojom::CorsError::kPreflightMissingAllowOriginHeader,
                  network::mojom::IPAddressSpace::kLoopback,
                  network::mojom::IPAddressSpace::kUnknown)));
}

IN_PROC_BROWSER_TEST_F(InterestGroupLocalNetworkBrowserTest,
                       SellerOnLocalNetwork) {
  GURL seller_url =
      https_server_->GetURL("b.test", "/interest_group/decision_logic.js");

  // Use `remote_test_server_` for all URLs except the seller worklet.
  GURL test_url = remote_test_server_.GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);

  // Need to set this up before the join, since joining instantiates the
  // AdAuctionServiceImpl's URLLoaderFactory.
  URLLoaderMonitor url_loader_monitor;

  EXPECT_EQ(kSuccess,
            JoinInterestGroupAndVerify(
                /*owner=*/test_origin,
                /*name=*/"Cthulhu",
                /*priority=*/0.0, /*execution_mode=*/
                blink::InterestGroup::ExecutionMode::kCompatibilityMode,
                /*bidding_url=*/
                remote_test_server_.GetURL("a.test",
                                           "/interest_group/bidding_logic.js"),
                /*ads=*/
                {{{GURL("https://example.com/render"),
                   /*metadata=*/absl::nullopt}}}));

  EXPECT_EQ(nullptr,
            RunAuctionAndWait(JsReplace(
                R"(
{
  seller: $1,
  decisionLogicUrl: $2,
  interestGroupBuyers: [$3]
}
                )",
                url::Origin::Create(seller_url), seller_url, test_origin)));

  // The URLLoaderMonitor should have seen a request for the seller URL. The
  // request should have gone through the renderer's URLLoader, and inherited
  // its IPAddressSpace, instead of passing its own.
  absl::optional<network::ResourceRequest> seller_request =
      url_loader_monitor.GetRequestInfo(seller_url);
  ASSERT_TRUE(seller_request);
  EXPECT_FALSE(seller_request->trusted_params);

  const network::URLLoaderCompletionStatus& seller_status =
      url_loader_monitor.WaitForRequestCompletion(seller_url);
  EXPECT_EQ(net::ERR_FAILED, seller_status.error_code);
  EXPECT_THAT(seller_status.cors_error_status,
              Optional(network::CorsErrorStatus(
                  network::mojom::CorsError::kPreflightMissingAllowOriginHeader,
                  network::mojom::IPAddressSpace::kLoopback,
                  network::mojom::IPAddressSpace::kUnknown)));
}

// Have the auction and worklets server from public IPs, but send reports to a
// local network. The reports should be blocked.
IN_PROC_BROWSER_TEST_F(InterestGroupLocalNetworkBrowserTest,
                       ReportToLocalNetwork) {
  // Use `remote_test_server_` exclusively with hostname "a.test" for root page
  // and script URLs.
  GURL test_url =
      remote_test_server_.GetURL("a.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);

  GURL ad_url = remote_test_server_.GetURL("c.test", "/echo");

  // Use `https_server_` exclusively with hostname "b.test" for reports.
  GURL bidder_report_to_url = https_server_->GetURL("b.test", "/bidder_report");
  GURL seller_report_to_url = https_server_->GetURL("b.test", "/seller_report");
  GURL bidder_debug_win_report_url =
      https_server_->GetURL("b.test", "/bidder_report_debug_win_report");
  GURL seller_debug_win_report_url =
      https_server_->GetURL("b.test", "/seller_report_debug_win_report");
  URLLoaderMonitor url_loader_monitor;

  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          /*owner=*/test_origin,
          /*name=*/bidder_report_to_url.spec(),
          /*priority=*/0.0, /*execution_mode=*/
          blink::InterestGroup::ExecutionMode::kCompatibilityMode,
          /*bidding_url=*/
          remote_test_server_.GetURL(
              "a.test", "/interest_group/bidding_logic_report_to_name.js"),
          /*ads=*/
          {{{ad_url,
             /*metadata=*/absl::nullopt}}}));

  RunAuctionAndWaitForURLAndNavigateIframe(
      JsReplace(
          R"(
{
  seller: $1,
  decisionLogicUrl: $2,
  interestGroupBuyers: [$1],
  sellerSignals: {reportTo: $3},
}
          )",
          test_origin,
          remote_test_server_.GetURL(
              "a.test",
              "/interest_group/decision_logic_report_to_seller_signals.js"),
          seller_report_to_url),
      ad_url);

  // Wait for both requests to be completed, and check their IPAddressSpace and
  // make sure that they failed.
  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            url_loader_monitor.WaitForUrl(bidder_report_to_url)
                .trusted_params->client_security_state->ip_address_space);
  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            url_loader_monitor.WaitForUrl(seller_report_to_url)
                .trusted_params->client_security_state->ip_address_space);

  for (const GURL& report_url :
       {bidder_report_to_url, seller_report_to_url, bidder_debug_win_report_url,
        seller_debug_win_report_url}) {
    SCOPED_TRACE(report_url.spec());
    const network::URLLoaderCompletionStatus& report_status =
        url_loader_monitor.WaitForRequestCompletion(report_url);
    EXPECT_EQ(net::ERR_FAILED, report_status.error_code);
    EXPECT_THAT(
        report_status.cors_error_status,
        Optional(network::CorsErrorStatus(
            network::mojom::CorsError::kPreflightMissingAllowOriginHeader,
            network::mojom::IPAddressSpace::kLoopback,
            network::mojom::IPAddressSpace::kUnknown)));
  }
}

// Have all requests for an auction served from a public network, and all
// reports send there as well. The auction should succeed, and all reports
// should be sent.
IN_PROC_BROWSER_TEST_F(InterestGroupLocalNetworkBrowserTest,
                       ReportToPublicNetwork) {
  // Use `remote_test_server_` exclusively with hostname "a.test" for root page
  // and script URLs.
  GURL test_url =
      remote_test_server_.GetURL("a.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);

  GURL bidder_url = remote_test_server_.GetURL(
      "a.test", "/interest_group/bidding_logic_report_to_name.js");
  GURL trusted_bidding_signals_url = remote_test_server_.GetURL(
      "a.test", "/interest_group/trusted_bidding_signals.json");

  GURL seller_url = remote_test_server_.GetURL(
      "a.test", "/interest_group/decision_logic_report_to_seller_signals.js");
  GURL ad_url = https_server_->GetURL("c.test", "/echo");

  // While reports should be made to these URLs in this test, their results
  // don't matter, so there's no need for a test server to respond to these URLs
  // with anything other than errors.
  GURL bidder_report_to_url =
      remote_test_server_.GetURL("a.test", "/bidder_report");
  GURL seller_report_to_url =
      remote_test_server_.GetURL("a.test", "/seller_report");
  GURL bidder_debug_win_report_url =
      remote_test_server_.GetURL("a.test", "/bidder_report_debug_win_report");
  GURL seller_debug_win_report_url =
      remote_test_server_.GetURL("a.test", "/seller_report_debug_win_report");
  URLLoaderMonitor url_loader_monitor;

  GURL trusted_bidding_signals_url_with_query = remote_test_server_.GetURL(
      "a.test",
      base::StringPrintf("/interest_group/trusted_bidding_signals.json"
                         "?hostname=a.test&keys=key1&interestGroupNames=%s",
                         base::EscapeQueryParamValue(
                             bidder_report_to_url.spec(), /*use_plus=*/true)
                             .c_str()));

  ASSERT_EQ(kSuccess,
            JoinInterestGroupAndVerify(
                blink::TestInterestGroupBuilder(
                    /*owner=*/test_origin,
                    /*name=*/bidder_report_to_url.spec())
                    .SetBiddingUrl(bidder_url)
                    .SetTrustedBiddingSignalsUrl(trusted_bidding_signals_url)
                    .SetTrustedBiddingSignalsKeys({{"key1"}})
                    .SetAds({{{ad_url, /*metadata=*/absl::nullopt}}})
                    .Build()));

  std::string auction_config = JsReplace(
      R"(
{
  seller: $1,
  decisionLogicUrl: $2,
  interestGroupBuyers: [$1],
  sellerSignals: {reportTo: $3},
}
          )",
      test_origin,
      remote_test_server_.GetURL(
          "a.test",
          "/interest_group/decision_logic_report_to_seller_signals.js"),
      seller_report_to_url);
  RunAuctionAndWaitForURLAndNavigateIframe(auction_config, ad_url);

  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            url_loader_monitor.WaitForUrl(bidder_url)
                .trusted_params->client_security_state->ip_address_space);
  EXPECT_EQ(
      network::mojom::IPAddressSpace::kPublic,
      url_loader_monitor.WaitForUrl(trusted_bidding_signals_url_with_query)
          .trusted_params->client_security_state->ip_address_space);
  // Unlike the others, the request for the seller URL has an empty
  // `trusted_params`, since it uses the renderer's untrusted URLLoader.
  EXPECT_FALSE(url_loader_monitor.WaitForUrl(seller_url).trusted_params);
  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            url_loader_monitor.WaitForUrl(seller_report_to_url)
                .trusted_params->client_security_state->ip_address_space);
  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            url_loader_monitor.WaitForUrl(seller_debug_win_report_url)
                .trusted_params->client_security_state->ip_address_space);

  for (const GURL& report_url :
       {bidder_report_to_url, seller_report_to_url, bidder_debug_win_report_url,
        seller_debug_win_report_url}) {
    SCOPED_TRACE(report_url.spec());
    EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
              url_loader_monitor.WaitForUrl(report_url)
                  .trusted_params->client_security_state->ip_address_space);
  }

  // Check that all reports reached the server.
  WaitForUrl(bidder_report_to_url);
  WaitForUrl(seller_report_to_url);
  WaitForUrl(bidder_debug_win_report_url);
  WaitForUrl(seller_debug_win_report_url);
}

// Make sure that the IPAddressSpace of the frame that triggers the update is
// respected for the update request. Does this by adding an interest group,
// trying to update it from a public page, and expecting the request to be
// blocked, and then adding another interest group and updating it from a
// local page, which should succeed. Have to use two interest groups to avoid
// the delay between updates.
IN_PROC_BROWSER_TEST_F(InterestGroupLocalNetworkBrowserTest,
                       UpdatePublicVsLocalNetwork) {
  const char kPubliclyUpdateGroupName[] = "Publicly updated group";
  const char kLocallyUpdateGroupName[] = "Locally updated group";

  GURL update_url =
      https_server_->GetURL("a.test", "/interest_group/update_partial.json");
  GURL initial_bidding_url = https_server_->GetURL(
      "a.test", "/interest_group/initial_bidding_logic.js");
  GURL new_bidding_url =
      https_server_->GetURL("a.test", "/interest_group/new_bidding_logic.js");

  // The server JSON updates biddingLogicUrl only.
  network_responder_->RegisterNetworkResponse(update_url.path(),
                                              JsReplace(R"(
{
  "biddingLogicUrl": $1
}
                                                        )",
                                                        new_bidding_url));

  URLLoaderMonitor url_loader_monitor;
  for (bool public_address_space : {true, false}) {
    SCOPED_TRACE(public_address_space);

    GURL test_url;
    std::string group_name;
    if (public_address_space) {
      // This header treats a response from a server on a local IP as if the
      // server were on public address space.
      test_url = https_server_->GetURL(
          "a.test",
          "/set-header?Content-Security-Policy: treat-as-public-address");
      group_name = kPubliclyUpdateGroupName;
    } else {
      test_url = https_server_->GetURL("a.test", "/echo");
      group_name = kLocallyUpdateGroupName;
    }
    ASSERT_TRUE(NavigateToURL(shell(), test_url));

    ASSERT_EQ(kSuccess,
              JoinInterestGroupAndVerify(
                  blink::TestInterestGroupBuilder(
                      /*owner=*/url::Origin::Create(test_url), group_name)
                      .SetBiddingUrl(initial_bidding_url)
                      .SetUpdateUrl(update_url)
                      .SetAds({{{GURL("https://example.com/render"),
                                 /*metadata=*/absl::nullopt}}})
                      .Build()));

    EXPECT_EQ("done", UpdateInterestGroupsInJS());

    // Wait for the update request to be made, and check its IPAddressSpace.
    url_loader_monitor.WaitForUrls();
    const network::ResourceRequest& request =
        url_loader_monitor.WaitForUrl(update_url);
    ASSERT_TRUE(request.trusted_params->client_security_state);
    if (public_address_space) {
      EXPECT_EQ(
          network::mojom::IPAddressSpace::kPublic,
          request.trusted_params->client_security_state->ip_address_space);
    } else {
      EXPECT_EQ(
          network::mojom::IPAddressSpace::kLoopback,
          request.trusted_params->client_security_state->ip_address_space);
    }
    // Not the main purpose of this test, but it should be using a transient
    // NetworkIsolationKey as well.
    ASSERT_TRUE(request.trusted_params->isolation_info.network_isolation_key()
                    .IsTransient());

    // The request should be blocked in the public address space case.
    if (public_address_space) {
      EXPECT_EQ(
          net::ERR_FAILED,
          url_loader_monitor.WaitForRequestCompletion(update_url).error_code);
    } else {
      EXPECT_EQ(
          net::OK,
          url_loader_monitor.WaitForRequestCompletion(update_url).error_code);
    }

    url_loader_monitor.ClearRequests();
  }

  // Wait for the kLocallyUpdateGroupName interest group to have an updated
  // bidding URL, while expecting the kPubliclyUpdateGroupName to continue to
  // have the original bidding URL. Have to wait because just because
  // URLLoaderMonitor has seen the request completed successfully doesn't mean
  // that the InterestGroup has been updated yet.
  WaitForInterestGroupsSatisfying(
      url::Origin::Create(initial_bidding_url),
      base::BindLambdaForTesting(
          [&](const std::vector<StorageInterestGroup>& storage_groups) {
            bool found_updated_group = false;
            for (const auto& storage_group : storage_groups) {
              const blink::InterestGroup& group = storage_group.interest_group;
              if (group.name == kPubliclyUpdateGroupName) {
                EXPECT_EQ(initial_bidding_url, group.bidding_url);
              } else {
                EXPECT_EQ(group.name, kLocallyUpdateGroupName);
                found_updated_group = (new_bidding_url == group.bidding_url);
              }
            }
            return found_updated_group;
          }));
}

// Create three interest groups, each belonging to different origins. Update one
// on a local network, but delay its server response. Update the second on a
// public network (thus expecting the request to be blocked). Update the final
// interest group on a local interest group -- it should be updated after the
// first two. After the server responds to the first update request, all updates
// should proceed -- the first should succeed, and the second should be blocked
// since the page is on a public network, and the third should succeed.
IN_PROC_BROWSER_TEST_F(InterestGroupLocalNetworkBrowserTest,
                       LocalNetProtectionsApplyToSubsequentUpdates) {
  constexpr char kLocallyUpdateGroupName[] = "Locally updated group";
  constexpr char kPubliclyUpdateGroupName[] = "Publicly updated group";

  // The update for a.test happens locally and gets deferred, whereas the update
  // for b.test and c.test are allowed to proceed immediately.
  const GURL update_url_a =
      https_server_->GetURL("a.test", kDeferredUpdateResponsePath);
  const GURL update_url_b =
      https_server_->GetURL("b.test", "/interest_group/update_partial_b.json");
  const GURL update_url_c =
      https_server_->GetURL("c.test", "/interest_group/update_partial_c.json");

  constexpr char kInitialBiddingPath[] =
      "/interest_group/initial_bidding_logic.js";
  const GURL initial_bidding_url_a =
      https_server_->GetURL("a.test", kInitialBiddingPath);
  const GURL initial_bidding_url_b =
      https_server_->GetURL("b.test", kInitialBiddingPath);
  const GURL initial_bidding_url_c =
      https_server_->GetURL("c.test", kInitialBiddingPath);

  constexpr char kNewBiddingPath[] = "/interest_group/new_bidding_logic.js";
  const GURL new_bidding_url_a =
      https_server_->GetURL("a.test", kNewBiddingPath);
  const GURL new_bidding_url_b =
      https_server_->GetURL("b.test", kNewBiddingPath);
  const GURL new_bidding_url_c =
      https_server_->GetURL("c.test", kNewBiddingPath);

  // The server JSON updates biddingLogicUrl only.
  constexpr char kUpdateContentTemplate[] = R"(
{
  "biddingLogicUrl": $1
}
)";
  // a.test's response is delayed until later.
  network_responder_->RegisterNetworkResponse(
      update_url_b.path(),
      JsReplace(kUpdateContentTemplate, new_bidding_url_b));
  network_responder_->RegisterNetworkResponse(
      update_url_c.path(),
      JsReplace(kUpdateContentTemplate, new_bidding_url_c));

  // First, create an interest group in a.test and start updating it from a
  // local site. The update doesn't finish yet because the network response
  // is delayed.
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));

  ASSERT_EQ(kSuccess,
            JoinInterestGroupAndVerify(
                blink::TestInterestGroupBuilder(
                    /*owner=*/url::Origin::Create(initial_bidding_url_a),
                    kLocallyUpdateGroupName)
                    .SetBiddingUrl(initial_bidding_url_a)
                    .SetUpdateUrl(update_url_a)
                    .SetAds({{{GURL("https://example.com/render"),
                               /*metadata=*/absl::nullopt}}})
                    .Build()));

  EXPECT_EQ("done", UpdateInterestGroupsInJS());

  // Now, create an interest group in b.test and start updating it from a
  // public site. The update will be delayed because the first interest group
  // hasn't finished updating, and it should get blocked because we are on a
  // public page.
  ASSERT_TRUE(NavigateToURL(
      shell(),
      https_server_->GetURL(
          "b.test",
          "/set-header?Content-Security-Policy: treat-as-public-address")));

  ASSERT_EQ(kSuccess,
            JoinInterestGroupAndVerify(
                blink::TestInterestGroupBuilder(
                    /*owner=*/url::Origin::Create(initial_bidding_url_b),
                    kPubliclyUpdateGroupName)
                    .SetBiddingUrl(initial_bidding_url_b)
                    .SetUpdateUrl(update_url_b)
                    .SetAds({{{GURL("https://example.com/render"),
                               /*metadata=*/absl::nullopt}}})
                    .Build()));

  EXPECT_EQ("done", UpdateInterestGroupsInJS());

  // Finally, create and update the last interest group on a local network --
  // this update shouldn't be blocked.
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("c.test", "/echo")));

  ASSERT_EQ(kSuccess,
            JoinInterestGroupAndVerify(
                blink::TestInterestGroupBuilder(
                    /*owner=*/url::Origin::Create(initial_bidding_url_c),
                    kLocallyUpdateGroupName)
                    .SetBiddingUrl(initial_bidding_url_c)
                    .SetUpdateUrl(update_url_c)
                    .SetAds({{{GURL("https://example.com/render"),
                               /*metadata=*/absl::nullopt}}})
                    .Build()));

  EXPECT_EQ("done", UpdateInterestGroupsInJS());

  // Now, finish the first interest group update by responding to its update
  // network request. All interest groups should be able to update now.
  network_responder_->DoDeferredUpdateResponse(
      JsReplace(kUpdateContentTemplate, new_bidding_url_a));

  // Wait for the c.test to update -- after it updates, all the other interest
  // groups should have updated too.
  WaitForInterestGroupsSatisfying(
      url::Origin::Create(initial_bidding_url_c),
      base::BindLambdaForTesting(
          [&](const std::vector<StorageInterestGroup>& storage_groups) {
            return storage_groups.size() == 1 &&
                   storage_groups[0].interest_group.bidding_url ==
                       new_bidding_url_c;
          }));

  // By this point, all the interest group updates should have completed.
  std::vector<StorageInterestGroup> a_groups =
      GetInterestGroupsForOwner(url::Origin::Create(initial_bidding_url_a));
  ASSERT_EQ(a_groups.size(), 1u);
  EXPECT_EQ(a_groups[0].interest_group.bidding_url, new_bidding_url_a);

  std::vector<StorageInterestGroup> b_groups =
      GetInterestGroupsForOwner(url::Origin::Create(initial_bidding_url_b));
  ASSERT_EQ(b_groups.size(), 1u);

  // Because it was updated on a public address, the update for b.test didn't
  // happen.
  EXPECT_EQ(b_groups[0].interest_group.bidding_url, initial_bidding_url_b);
}

// Join interest groups with local update URLs, and run auctions from
// both a a main frame loaded with public address space, and with a local
// address space. The auctions trigger updates the interest groups, but only the
// frame using a local address space successfully updates the IG, since frames
// from public address spaces are blocked from making requests to servers with
// local addresses.
//
// Different interest groups (with different origins) are used for the public
// and local auction, to avoid running into update rate limits.
IN_PROC_BROWSER_TEST_F(InterestGroupLocalNetworkBrowserTest,
                       LocalNetProtectionsApplyToPostAuctionUpdates) {
  // Fetches for the interest group-related scripts and updates are always
  // local, it's where they're updated from that matters. Interest group A will
  // be updated from an auction on a public origin, and B from a local one.
  // Only the second update will succeed.
  //
  // It's important to do the successful update last, so that the first update
  // would have most likely succeeded by that point in time, if it were going
  // to, since there's no exposed API to wait for an interest group update to
  // fail, though the test does wait for the update network request itself to
  // succeed / fail. As of this writing, the current updating queuing should
  // guarantee updates for one origin start only after previously requested
  // updates for another origin have completed (successfully or unsuccessfully),
  // but this test should be robust against changes in that logic.
  const url::Origin interest_group_a_origin =
      https_server_->GetOrigin("a.test");
  const url::Origin interest_group_b_origin =
      https_server_->GetOrigin("b.test");

  constexpr char kUpdatePath[] = "/interest_group/update_partial_a.json";
  constexpr char kUpdateResponse[] = R"(
{
"ads": [{"renderUrl": "https://example.com/render2"
        }]
})";
  // The server JSON updates the ads only. Both update URLs use the same path,
  // so only need to add a response once
  network_responder_->RegisterNetworkResponse(kUpdatePath, kUpdateResponse);

  // The origin of the seller script URL doesn't matter for these tests. Use
  // an origin other than the interest groups just to make clear there are no
  // dependencies on a shared origin anywhere.
  const GURL decision_logic_url =
      https_server_->GetURL("c.test", "/interest_group/decision_logic.js");

  const struct {
    // All interest group URLs are derived from this.
    url::Origin interest_group_origin;
    bool run_auction_from_public_address_space;
    GURL auction_url;
  } kTestCases[] = {
      {interest_group_a_origin,
       /*run_auction_from_public_address_space=*/true,
       // This header treats a response from a server on a local IP as if the
       // server were on public address space.
       https_server_->GetURL(
           "c.test",
           "/set-header?Content-Security-Policy: treat-as-public-address")},
      {interest_group_b_origin,
       /*run_auction_from_public_address_space=*/false,
       https_server_->GetURL("c.test", "/echo")},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.run_auction_from_public_address_space);

    URLLoaderMonitor url_loader_monitor;

    std::string interest_group_host = test_case.interest_group_origin.host();
    GURL join_url = https_server_->GetURL(interest_group_host, "/echo");
    GURL update_url = https_server_->GetURL(interest_group_host, kUpdatePath);
    GURL bidding_url = https_server_->GetURL(
        interest_group_host, "/interest_group/bidding_logic.js");

    ASSERT_TRUE(NavigateToURL(shell(), join_url));
    EXPECT_EQ(kSuccess,
              JoinInterestGroupAndVerify(
                  blink::TestInterestGroupBuilder(
                      /*owner=*/test_case.interest_group_origin, "name")
                      .SetBiddingUrl(bidding_url)
                      .SetUpdateUrl(update_url)
                      .SetAds({{{GURL("https://example.com/render"),
                                 /*metadata=*/absl::nullopt}}})
                      .Build()));

    ASSERT_TRUE(NavigateToURL(shell(), test_case.auction_url));
    EvalJsResult auction_result = EvalJs(
        shell(), JsReplace(
                     R"(
(async function() {
  return await navigator.runAdAuction({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$3],
  });
})())",
                     url::Origin::Create(decision_logic_url),
                     decision_logic_url, test_case.interest_group_origin));
    if (test_case.run_auction_from_public_address_space) {
      // The auction fails because the scripts get blocked; the update request
      // should still happen, though it will also ultimately be blocked.
      EXPECT_EQ(nullptr, auction_result);
    } else {
      TestFencedFrameURLMappingResultObserver observer;
      ConvertFencedFrameURNToURL(GURL(auction_result.ExtractString()),
                                 &observer);
      EXPECT_EQ(GURL("https://example.com/render"), observer.mapped_url());
    }

    // Wait for the update request to be made, and check its IPAddressSpace.
    url_loader_monitor.WaitForUrls();
    const network::ResourceRequest& request =
        url_loader_monitor.WaitForUrl(update_url);
    ASSERT_TRUE(request.trusted_params->client_security_state);
    if (test_case.run_auction_from_public_address_space) {
      EXPECT_EQ(
          network::mojom::IPAddressSpace::kPublic,
          request.trusted_params->client_security_state->ip_address_space);
      // The request should be blocked in the public address space case.
      EXPECT_EQ(
          net::ERR_FAILED,
          url_loader_monitor.WaitForRequestCompletion(update_url).error_code);
    } else {
      EXPECT_EQ(
          network::mojom::IPAddressSpace::kLoopback,
          request.trusted_params->client_security_state->ip_address_space);
      EXPECT_EQ(
          net::OK,
          url_loader_monitor.WaitForRequestCompletion(update_url).error_code);
    }

    // Not the main purpose of this test, but it should be using a transient
    // NetworkIsolationKey as well.
    ASSERT_TRUE(request.trusted_params->isolation_info.network_isolation_key()
                    .IsTransient());
  }

  // Wait for interest group B's ad URL to be successfully updated.

  const GURL initial_ad_url = GURL("https://example.com/render");
  const GURL new_ad_url = GURL("https://example.com/render2");

  auto check_for_new_ad_url = base::BindLambdaForTesting(
      [&](const std::vector<StorageInterestGroup>& storage_groups) {
        EXPECT_EQ(storage_groups.size(), 1u);
        const blink::InterestGroup& group = storage_groups[0].interest_group;
        EXPECT_TRUE(group.ads.has_value());
        EXPECT_EQ(group.ads->size(), 1u);
        if (group.ads.value()[0].render_url == new_ad_url) {
          return true;
        }
        EXPECT_EQ(initial_ad_url, group.ads.value()[0].render_url);
        return false;
      });

  WaitForInterestGroupsSatisfying(interest_group_b_origin,
                                  check_for_new_ad_url);

  // Check that interest group A's ad URL was not updated.
  auto storage_groups = GetInterestGroupsForOwner(interest_group_a_origin);
  ASSERT_EQ(storage_groups.size(), 1u);
  const blink::InterestGroup& group = storage_groups[0].interest_group;
  ASSERT_TRUE(group.ads.has_value());
  ASSERT_EQ(group.ads->size(), 1u);
  EXPECT_EQ(initial_ad_url, group.ads.value()[0].render_url);
}

// Interest group APIs succeeded (i.e., feature join-ad-interest-group is
// enabled by Permissions Policy), and runAdAuction succeeded (i.e., feature
// run-ad-auction is enabled by Permissions Policy) in all contexts, because
// the kAdInterestGroupAPIRestrictedPolicyByDefault runtime flag is disabled by
// default and in that case the default value for those features are
// EnableForAll.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       FeaturesEnabledForAllByPermissionsPolicy) {
  // clang-format off
  GURL test_url = https_server_->GetURL(
      "a.test",
      "/cross_site_iframe_factory.html?a.test("
          "a.test,"
          "b.test("
              "c.test{allow-join-ad-interest-group;run-ad-auction},"
              "a.test{allow-join-ad-interest-group;run-ad-auction},"
              "a.test{allow-join-ad-interest-group;run-ad-auction}"
          ")"
       ")");
  // clang-format on
  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  RenderFrameHost* main_frame = web_contents()->GetPrimaryMainFrame();
  RenderFrameHost* same_origin_iframe = ChildFrameAt(main_frame, 0);
  RenderFrameHost* cross_origin_iframe = ChildFrameAt(main_frame, 1);
  RenderFrameHost* inner_cross_origin_iframe =
      ChildFrameAt(cross_origin_iframe, 0);
  RenderFrameHost* same_origin_iframe_in_cross_origin_iframe =
      ChildFrameAt(cross_origin_iframe, 1);
  RenderFrameHost* same_origin_iframe_in_cross_origin_iframe2 =
      ChildFrameAt(cross_origin_iframe, 2);

  // The server JSON updates all fields that can be updated.
  constexpr char kUpdateUrlPath[] = "/interest_group/update_partial.json";
  network_responder_->RegisterNetworkResponse(kUpdateUrlPath,
                                              base::StringPrintf(
                                                  R"(
  {
    "trustedBiddingSignalsKeys": ["new_key"],
  }
                                                  )"));

  GURL url;
  url::Origin origin;
  std::string host;
  RenderFrameHost* execution_targets[] = {
      main_frame,
      same_origin_iframe,
      cross_origin_iframe,
      inner_cross_origin_iframe,
      same_origin_iframe_in_cross_origin_iframe,
      same_origin_iframe_in_cross_origin_iframe2};

  for (auto* execution_target : execution_targets) {
    url = execution_target->GetLastCommittedURL();
    origin = url::Origin::Create(url);
    host = url.host();
    WebContentsConsoleObserver console_observer(shell()->web_contents());
    console_observer.SetPattern(WarningPermissionsPolicy("*", "*"));

    EXPECT_EQ(kSuccess,
              JoinInterestGroupAndVerify(
                  blink::TestInterestGroupBuilder(
                      /*owner=*/origin,
                      /*name=*/"cars")
                      .SetBiddingUrl(https_server_->GetURL(
                          host, "/interest_group/bidding_logic.js"))
                      .SetUpdateUrl(https_server_->GetURL(
                          host, "/interest_group/update_partial.json"))
                      .SetAds({{{GURL("https://example.com/render"),
                                 /*metadata=*/absl::nullopt}}})
                      .Build(),
                  execution_target));

    EXPECT_EQ("https://example.com/render",
              RunAuctionAndWaitForUrl(
                  JsReplace(
                      R"(
{
  seller: $1,
  decisionLogicUrl: $2,
  interestGroupBuyers: [$1],
}
                              )",
                      origin,
                      https_server_->GetURL(
                          host, "/interest_group/decision_logic.js")),
                  execution_target));

    EXPECT_EQ("done", UpdateInterestGroupsInJS(execution_target));
    // The second UpdateInterestGroupsInJS will not add a warning message, since
    // the same message has already been added and redundant messages will be
    // discarded.
    EXPECT_EQ("done", UpdateInterestGroupsInJS(execution_target));
    EXPECT_EQ(kSuccess, LeaveInterestGroup(origin, "cars", execution_target));

    // It seems discard_duplicates of AddConsoleMessage works differently on
    // Android and other platforms. On Android, a message will be discarded if
    // it's not unique across all frames in a page. On other platforms, a
    // message will be discarded if it's not unique per origin (e.g., iframe
    // a.test and iframe b.test can have the same message, while the same
    // message from another a.test will be discarded).

#if BUILDFLAG(IS_ANDROID)
    RenderFrameHost* execution_targets_with_message[] = {cross_origin_iframe};
#else
    RenderFrameHost* execution_targets_with_message[] = {
        cross_origin_iframe, inner_cross_origin_iframe,
        same_origin_iframe_in_cross_origin_iframe};
#endif  // BUILDFLAG(IS_ANDROID)

    if (base::Contains(execution_targets_with_message, execution_target)) {
      EXPECT_EQ(WarningPermissionsPolicy("join-ad-interest-group",
                                         "joinAdInterestGroup"),
                console_observer.GetMessageAt(0));
      EXPECT_EQ(WarningPermissionsPolicy("run-ad-auction", "runAdAuction"),
                console_observer.GetMessageAt(1));
      EXPECT_EQ(WarningPermissionsPolicy("join-ad-interest-group",
                                         "updateAdInterestGroups"),
                console_observer.GetMessageAt(2));
      EXPECT_EQ(WarningPermissionsPolicy("join-ad-interest-group",
                                         "leaveAdInterestGroup"),
                console_observer.GetMessageAt(3));
    } else {
      EXPECT_TRUE(console_observer.messages().empty());
    }
  }
}

// Features join-ad-interest-group and run-ad-auction can be disabled by HTTP
// headers, and they cannot be enabled again by container policy in that case.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, FeaturesDisabledByHttpHeader) {
  GURL test_url = https_server_->GetURL(
      "a.test",
      "/interest_group/page-with-fledge-permissions-policy-disabled.html");
  url::Origin origin = url::Origin::Create(test_url);
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  RenderFrameHost* main_frame = web_contents()->GetPrimaryMainFrame();
  RenderFrameHost* iframe = ChildFrameAt(main_frame, 0);

  for (auto* execution_target : {main_frame, iframe}) {
    ExpectNotAllowedToJoinOrUpdateInterestGroup(origin, execution_target);
    ExpectNotAllowedToRunAdAuction(
        origin,
        https_server_->GetURL("a.test", "/interest_group/decision_logic.js"),
        execution_target);
    ExpectNotAllowedToLeaveInterestGroup(origin, "cars", execution_target);
  }
}

// Features join-ad-interest-group and run-ad-auction can be disabled by
// container policy.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       FeaturesDisabledByContainerPolicy) {
  GURL test_url = https_server_->GetURL(
      "a.test",
      "/interest_group/"
      "page-with-fledge-permissions-policy-disabled-in-iframe.html");
  url::Origin origin = url::Origin::Create(test_url);
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  RenderFrameHost* same_origin_iframe =
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  ExpectNotAllowedToJoinOrUpdateInterestGroup(origin, same_origin_iframe);
  ExpectNotAllowedToRunAdAuction(
      origin,
      https_server_->GetURL("a.test", "/interest_group/decision_logic.js"),
      same_origin_iframe);
  ExpectNotAllowedToLeaveInterestGroup(origin, "cars", same_origin_iframe);
}

// Interest group APIs succeeded (i.e., feature join-ad-interest-group is
// enabled by Permissions Policy), and runAdAuction succeeded (i.e., feature
// run-ad-auction is enabled by Permissions Policy) in
// (1) same-origin frames by default,
// (2) cross-origin iframes that enable those features in container policy
//     (iframe "allow" attribute).
// (3) cross-origin iframes that enables those features inside parent
//     cross-origin iframes that also enables those features.
IN_PROC_BROWSER_TEST_F(InterestGroupRestrictedPermissionsPolicyBrowserTest,
                       EnabledByPermissionsPolicy) {
  // clang-format off
  GURL test_url = https_server_->GetURL(
      "a.test",
      "/cross_site_iframe_factory.html?a.test("
          "a.test,"
          "b.test{allow-join-ad-interest-group;run-ad-auction}("
              "c.test{allow-join-ad-interest-group;run-ad-auction}"
          ")"
       ")");
  // clang-format on
  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  RenderFrameHost* main_frame = web_contents()->GetPrimaryMainFrame();
  RenderFrameHost* same_origin_iframe = ChildFrameAt(main_frame, 0);
  RenderFrameHost* cross_origin_iframe = ChildFrameAt(main_frame, 1);
  RenderFrameHost* inner_cross_origin_iframe =
      ChildFrameAt(cross_origin_iframe, 0);

  // The server JSON updates all fields that can be updated.
  constexpr char kUpdateUrlPath[] = "/interest_group/update_partial.json";
  network_responder_->RegisterNetworkResponse(kUpdateUrlPath,
                                              base::StringPrintf(
                                                  R"(
  {
    "trustedBiddingSignalsKeys": ["new_key"],
  }
                                                  )"));

  GURL url;
  url::Origin origin;
  std::string host;
  RenderFrameHost* execution_targets[] = {main_frame, same_origin_iframe,
                                          cross_origin_iframe,
                                          inner_cross_origin_iframe};

  for (auto* execution_target : execution_targets) {
    url = execution_target->GetLastCommittedURL();
    origin = url::Origin::Create(url);
    host = url.host();
    WebContentsConsoleObserver console_observer(shell()->web_contents());
    console_observer.SetPattern(WarningPermissionsPolicy("*", "*"));

    EXPECT_EQ(kSuccess,
              JoinInterestGroupAndVerify(
                  blink::TestInterestGroupBuilder(
                      /*owner=*/origin,
                      /*name=*/"cars")
                      .SetBiddingUrl(https_server_->GetURL(
                          host, "/interest_group/bidding_logic.js"))
                      .SetUpdateUrl(https_server_->GetURL(
                          host, "/interest_group/update_partial.json"))
                      .SetAds({{{GURL("https://example.com/render"),
                                 /*metadata=*/absl::nullopt}}})
                      .Build(),
                  execution_target));

    EXPECT_EQ("https://example.com/render",
              RunAuctionAndWaitForUrl(
                  JsReplace(
                      R"(
{
  seller: $1,
  decisionLogicUrl: $2,
  interestGroupBuyers: [$1],
}
                              )",
                      origin,
                      https_server_->GetURL(
                          host, "/interest_group/decision_logic.js")),
                  execution_target));

    EXPECT_EQ("done", UpdateInterestGroupsInJS(execution_target));
    EXPECT_EQ(kSuccess, LeaveInterestGroup(origin, "cars", execution_target));
    EXPECT_TRUE(console_observer.messages().empty());
  }
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       LotsOfInterestGroupsEpsilonTimeout) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  url::Origin test_origin = url::Origin::Create(test_url);
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  GURL ad_url = https_server_->GetURL("c.test", "/echo?render_cars");
  GURL decision_url =
      https_server_->GetURL("a.test", "/interest_group/decision_logic.js");

  // Need lots of groups to exercise them being handled in chunks.
  // Use /hung as script since we don't actually want to finish.
  for (int group = 0; group < 100; ++group) {
    EXPECT_EQ(kSuccess,
              JoinInterestGroupAndVerify(
                  /*owner=*/test_origin,
                  /*name=*/base::StringPrintf("cars%d", group),
                  /*priority=*/0.0,
                  /*execution_mode=*/
                  blink::InterestGroup::ExecutionMode::kCompatibilityMode,
                  /*bidding_url=*/
                  https_server_->GetURL("a.test", "/hung"),
                  /*ads=*/{{{ad_url, /*metadata=*/absl::nullopt}}}));
  }

  // Also add an IG on a different host also with hung script so that we don't
  // terminate immediately.
  GURL other_url = https_server_->GetURL("allow-join.b.test", "/hung");
  url::Origin other_origin = url::Origin::Create(other_url);
  content_browser_client_->AddToAllowList({other_origin});
  EXPECT_EQ(kSuccess,
            JoinInterestGroupAndVerify(
                /*owner=*/other_origin,
                /*name=*/"bicycles",
                /*priority=*/0.0,
                /*execution_mode=*/
                blink::InterestGroup::ExecutionMode::kCompatibilityMode,
                /*bidding_url=*/other_url,
                /*ads=*/{{{ad_url, /*metadata=*/absl::nullopt}}}));

  const char kAuctionConfigTemplate[] = R"({
      seller: $1,
      decisionLogicUrl: $2,
      perBuyerCumulativeTimeouts: {$1: 1, $3: 50},
      interestGroupBuyers: [$1, $3]
  })";

  WebContentsConsoleObserver console_observer(shell()->web_contents());
  console_observer.SetPattern(
      "Worklet error: https://a.test:*/hung perBuyerCumulativeTimeout "
      "exceeded during bid generation.");
  EXPECT_EQ(nullptr,
            RunAuctionAndWait(JsReplace(kAuctionConfigTemplate, test_origin,
                                        decision_url, other_origin)));
  EXPECT_TRUE(console_observer.Wait());
}

// Interest group APIs throw NotAllowedError (i.e., feature
// join-ad-interest-group is disabled by Permissions Policy), and runAdAuction
// throws NotAllowedError (i.e, feature run-ad-auction is disabled by
// Permissions Policy) in
// (1) same-origin iframes that disabled the features using allow attribute,
// (2) cross-origin iframes that don't enable those features in container policy
//     (iframe "allow" attribute).
// (3) iframes that enables those features inside parent cross-origin iframes
//     that don't enable those features.
IN_PROC_BROWSER_TEST_F(InterestGroupRestrictedPermissionsPolicyBrowserTest,
                       DisabledByContainerPolicy) {
  GURL other_url = https_server_->GetURL("b.test", "/echo");
  url::Origin other_origin = url::Origin::Create(other_url);
  // clang-format off
  GURL test_url = https_server_->GetURL(
      "a.test",
      "/cross_site_iframe_factory.html?a.test("
          "b.test("
              "b.test{allow-join-ad-interest-group;run-ad-auction}"
          ")"
       ")");
  // clang-format on
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  RenderFrameHost* outter_iframe =
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  RenderFrameHost* inner_iframe = ChildFrameAt(outter_iframe, 0);

  for (auto* execution_target : {outter_iframe, inner_iframe}) {
    ExpectNotAllowedToJoinOrUpdateInterestGroup(other_origin, execution_target);
    ExpectNotAllowedToRunAdAuction(
        other_origin,
        https_server_->GetURL("b.test", "/interest_group/decision_logic.js"),
        execution_target);
    ExpectNotAllowedToLeaveInterestGroup(other_origin, "cars",
                                         execution_target);
  }

  test_url = https_server_->GetURL(
      "a.test",
      "/interest_group/"
      "page-with-fledge-permissions-policy-disabled-in-iframe.html");
  url::Origin origin = url::Origin::Create(test_url);
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  RenderFrameHost* same_origin_iframe =
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  ExpectNotAllowedToJoinOrUpdateInterestGroup(origin, same_origin_iframe);
  ExpectNotAllowedToRunAdAuction(
      origin,
      https_server_->GetURL("a.test", "/interest_group/decision_logic.js"),
      same_origin_iframe);
  ExpectNotAllowedToLeaveInterestGroup(origin, "cars", same_origin_iframe);
}

// Features join-ad-interest-group and run-ad-auction can be enabled/disabled
// separately.
IN_PROC_BROWSER_TEST_F(InterestGroupRestrictedPermissionsPolicyBrowserTest,
                       EnableOneOfInterestGroupAPIsAndAuctionAPIForIframe) {
  GURL other_url = https_server_->GetURL("b.test", "/echo");
  url::Origin other_origin = url::Origin::Create(other_url);
  // clang-format off
  GURL test_url = https_server_->GetURL(
      "a.test",
      "/cross_site_iframe_factory.html?a.test("
          "b.test{allow-join-ad-interest-group},"
          "b.test{allow-run-ad-auction})");
  // clang-format on
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  RenderFrameHost* iframe_interest_group =
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  RenderFrameHost* iframe_ad_auction =
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 1);

  // Interest group APIs succeed and run ad auction fails for
  // iframe_interest_group.
  EXPECT_EQ(kSuccess,
            JoinInterestGroupAndVerify(
                blink::TestInterestGroupBuilder(
                    /*owner=*/other_origin,
                    /*name=*/"cars")
                    .SetBiddingUrl(https_server_->GetURL(
                        "b.test", "/interest_group/bidding_logic.js"))
                    .SetUpdateUrl(https_server_->GetURL(
                        "b.test", "/interest_group/update_partial.json"))
                    .SetAds({{{GURL("https://example.com/render"),
                               /*metadata=*/absl::nullopt}}})
                    .Build(),
                iframe_interest_group));

  EXPECT_EQ("done", UpdateInterestGroupsInJS(iframe_interest_group));
  ExpectNotAllowedToRunAdAuction(
      other_origin,
      https_server_->GetURL("b.test", "/interest_group/decision_logic.js"),
      iframe_interest_group);

  // Interest group APIs fail and run ad auction succeeds for iframe_ad_auction.
  ExpectNotAllowedToJoinOrUpdateInterestGroup(other_origin, iframe_ad_auction);
  EXPECT_EQ("https://example.com/render",
            RunAuctionAndWaitForUrl(
                JsReplace(
                    R"(
{
  seller: $1,
  decisionLogicUrl: $2,
  interestGroupBuyers: [$1],
}
                            )",
                    other_origin,
                    https_server_->GetURL("b.test",
                                          "/interest_group/decision_logic.js")),
                iframe_ad_auction));
  ExpectNotAllowedToLeaveInterestGroup(other_origin, "cars", iframe_ad_auction);

  EXPECT_EQ(kSuccess,
            LeaveInterestGroup(other_origin, "cars", iframe_interest_group));
}

// Features join-ad-interest-group and run-ad-auction can be disabled by HTTP
// headers, and they cannot be enabled again by container policy in that case.
IN_PROC_BROWSER_TEST_F(InterestGroupRestrictedPermissionsPolicyBrowserTest,
                       DisabledByHttpHeader) {
  GURL test_url = https_server_->GetURL(
      "a.test",
      "/interest_group/page-with-fledge-permissions-policy-disabled.html");
  url::Origin origin = url::Origin::Create(test_url);
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  RenderFrameHost* main_frame = web_contents()->GetPrimaryMainFrame();
  RenderFrameHost* iframe = ChildFrameAt(main_frame, 0);

  for (auto* execution_target : {main_frame, iframe}) {
    ExpectNotAllowedToJoinOrUpdateInterestGroup(origin, execution_target);
    ExpectNotAllowedToRunAdAuction(
        origin,
        https_server_->GetURL("a.test", "/interest_group/decision_logic.js"),
        execution_target);
    ExpectNotAllowedToLeaveInterestGroup(origin, "cars", execution_target);
  }
}

// navigator.deprecatedURNToURL returns null for an invalid URN.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, DeprecatedURNToURLInvalidURN) {
  GURL invalid_urn("urn:uuid:c36973b5-e5d9-de59-e4c4-364f137b3c7a");
  EXPECT_EQ(absl::nullopt, ConvertFencedFrameURNToURLInJS(invalid_urn));
}

// Tests navigator.deprecatedURNToURL for a valid URN. Covers both the cases
// where sendReports is false and true. Both are done in the same test because
// there's no way to wait until reports aren't sent, so first run a case that
// doesn't send reports, then run a case that does, and finally make sure that
// reports were only sent for the first case.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, DeprecatedURNToURLValidURN) {
  const struct {
    bool send_reports;
    // Host for buyer, seller, and publisher. Use a different hostname for each
    // loop iteration so they can use different interest groups.
    const char* host;
    // Path for reports. Have to be different so can make reports are only send
    // when `send_reports` is true.
    GURL report_url;
  } kTestCases[] = {
      {
          /*send_reports=*/false,
          /*host=*/"a.test",
          /*report_path=*/https_server_->GetURL("c.test", "/report_for_a"),
      },
      {
          /*send_reports=*/true,
          /*host=*/"b.test",
          /*report_path=*/https_server_->GetURL("c.test", "/report_for_b"),
      }};

  for (const auto& test_case : kTestCases) {
    GURL test_url = https_server_->GetURL(test_case.host, "/echo");
    ASSERT_TRUE(NavigateToURL(shell(), test_url));
    url::Origin test_origin = url::Origin::Create(test_url);
    GURL ad_url = https_server_->GetURL("c.test", "/echo?render_cars");

    EXPECT_EQ(kSuccess,
              JoinInterestGroupAndVerify(
                  /*owner=*/test_origin,
                  // This test uses a script that sends reports to name of the
                  // interest group, so use the report URL as the name.
                  /*name=*/test_case.report_url.spec(),
                  /*priority=*/0.0, /*execution_mode=*/
                  blink::InterestGroup::ExecutionMode::kCompatibilityMode,
                  /*bidding_url=*/
                  https_server_->GetURL(
                      test_case.host,
                      "/interest_group/bidding_logic_report_to_name.js"),
                  /*ads=*/{{{ad_url, /*metadata=*/absl::nullopt}}}));

    std::string auction_config = JsReplace(
        R"({
          seller: $1,
          decisionLogicUrl: $2,
          interestGroupBuyers: [$1]
        })",
        test_origin,
        https_server_->GetURL(test_case.host,
                              "/interest_group/decision_logic.js"));
    auto result = RunAuctionAndWait(auction_config);
    GURL urn_url = GURL(result.ExtractString());
    EXPECT_TRUE(urn_url.is_valid());
    EXPECT_EQ(url::kUrnScheme, urn_url.scheme_piece());
    EXPECT_EQ(ad_url,
              ConvertFencedFrameURNToURLInJS(urn_url, test_case.send_reports));
  }

  // Only the `send_reports` == true case should have sent a report. Wait for
  // it, and then check that the report URL for the first case was not seen.
  WaitForUrl(kTestCases[1].report_url);
  EXPECT_FALSE(HasServerSeenUrl(kTestCases[0].report_url));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, ExecutionModeGroupByOrigin) {
  const char kScript[] = R"(
    if (!('count' in globalThis))
      globalThis.count = 0;
    function generateBid() {
      ++count;
      return {ad: ["ad"], bid:count, render:$1 + count};
    }
    function reportWin(auctionSignals, perBuyerSignals, sellerSignals,
                       browserSignals) {
    }
  )";

  const int kNumGroups = 10;  // as many ads in each group, too.
  GURL test_url = https_server_->GetURL("a.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);

  std::vector<GURL> ad_urls;
  for (int i = 0; i < kNumGroups; ++i) {
    ad_urls.push_back(https_server_->GetURL(
        "c.test", "/echo?" + base::NumberToString(i + 1)));
  }

  network_responder_->RegisterNetworkResponse(
      "/interest_group/bidding_logic.js",
      JsReplace(kScript, https_server_->GetURL("c.test", "/echo?")),
      "application/javascript");

  std::vector<blink::InterestGroup::Ad> ads;
  for (const GURL& ad_url : ad_urls) {
    ads.emplace_back(ad_url, /*metadata=*/absl::nullopt);
  }

  for (auto execution_mode :
       {blink::InterestGroup::ExecutionMode::kCompatibilityMode,
        blink::InterestGroup::ExecutionMode::kGroupedByOriginMode}) {
    for (int i = 0; i < kNumGroups; ++i) {
      EXPECT_EQ(
          kSuccess,
          JoinInterestGroupAndVerify(
              blink::TestInterestGroupBuilder(
                  /*owner=*/test_origin,
                  /*name=*/"cars" + base::NumberToString(i))
                  .SetExecutionMode(execution_mode)
                  .SetBiddingUrl(https_server_->GetURL(
                      test_url.host(), "/interest_group/bidding_logic.js"))
                  .SetAds(ads)
                  .Build()));
    }

    EXPECT_EQ(
        https_server_->GetURL(
            "c.test",
            execution_mode ==
                    blink::InterestGroup::ExecutionMode::kCompatibilityMode
                ? "/echo?1"
                : "/echo?10"),
        RunAuctionAndWaitForUrl(JsReplace(
            R"({
                    seller: $1,
                    decisionLogicUrl: $2,
                    interestGroupBuyers: [$1],
                  })",
            test_origin,
            https_server_->GetURL("a.test",
                                  "/interest_group/decision_logic.js"))));
  }
}

// Runs auction like Just like
// InterestGroupFencedFrameBrowserTest.RunAdAuctionWithWinner but also registers
// an ad beacon that is sent by the render URL.
IN_PROC_BROWSER_TEST_F(InterestGroupFencedFrameBrowserTest,
                       RunAdAuctionWithWinnerRegisterAdBeaconBuyer) {
  URLLoaderMonitor url_loader_monitor;

  GURL test_url = https_server_->GetURL("a.test", "/fenced_frames/basic.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);

  GURL ad_url = https_server_->GetURL(
      "c.test", "/fenced_frames/ad_with_fenced_frame_reporting.html");
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          blink::TestInterestGroupBuilder(
              /*owner=*/test_origin,
              /*name=*/"cars")
              .SetBiddingUrl(https_server_->GetURL(
                  "a.test", "/interest_group/bidding_logic.js"))
              .SetTrustedBiddingSignalsUrl(https_server_->GetURL(
                  "a.test", "/interest_group/trusted_bidding_signals.json"))
              .SetTrustedBiddingSignalsKeys({{"key1"}})
              .SetAds({{{ad_url, R"({"ad":"metadata","here":[1,2]})"}}})
              .Build()));

  ASSERT_NO_FATAL_FAILURE(RunAuctionAndNavigateFencedFrame(
      ad_url, JsReplace(
                  R"({
seller: $1,
decisionLogicUrl: $2,
interestGroupBuyers: [$1],
                  })",
                  test_origin,
                  https_server_->GetURL("a.test",
                                        "/interest_group/decision_logic.js"))));

  absl::optional<network::ResourceRequest> request =
      url_loader_monitor.WaitForUrl(
          https_server_->GetURL("d.test", "/echoall?report_win_beacon"));
  ASSERT_TRUE(request);
  EXPECT_EQ(net::HttpRequestHeaders::kPostMethod, request->method);
}

// Runs an auction similar to
// InterestGroupFencedFrameBrowserTest.RunAdAuctionWithWinner, but also triggers
// sending a *private aggregation* event using `window.fence.reportEvent`.
IN_PROC_BROWSER_TEST_F(InterestGroupFencedFrameBrowserTest,
                       RunAdAuctionWithWinnerRegisterPrivateAggregationBuyer) {
  URLLoaderMonitor url_loader_monitor;

  GURL test_url = https_server_->GetURL("a.test", "/fenced_frames/basic.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);

  base::RunLoop run_loop;

  class TestPrivateAggregationManagerImpl
      : public PrivateAggregationManagerImpl {
   public:
    TestPrivateAggregationManagerImpl(
        std::unique_ptr<PrivateAggregationBudgeter> budgeter,
        std::unique_ptr<PrivateAggregationHost> host)
        : PrivateAggregationManagerImpl(std::move(budgeter),
                                        std::move(host),
                                        /*storage_partition=*/nullptr) {}
  };

  base::MockRepeatingCallback<void(AggregatableReportRequest,
                                   PrivateAggregationBudgetKey)>
      mock_callback;

  auto* storage_partition_impl =
      static_cast<StoragePartitionImpl*>(shell()
                                             ->web_contents()
                                             ->GetBrowserContext()
                                             ->GetDefaultStoragePartition());
  storage_partition_impl->OverridePrivateAggregationManagerForTesting(
      std::make_unique<TestPrivateAggregationManagerImpl>(
          std::make_unique<MockPrivateAggregationBudgeter>(),
          std::make_unique<PrivateAggregationHost>(
              /*on_report_request_received=*/mock_callback.Get(),
              /*browser_context=*/storage_partition_impl->browser_context())));

  // We only need to test that a request was made in PrivateAggregationHost, so
  // we mock out the callback and check that it was called. The callback will
  // only run *after* the ad auction finishes, but we register it beforehand
  // so that it is guaranteed to detect when the private aggregation event is
  // sent.
  EXPECT_CALL(mock_callback, Run)
      .WillRepeatedly(
          testing::Invoke([&](AggregatableReportRequest request,
                              PrivateAggregationBudgetKey budget_key) {
            ASSERT_EQ(request.payload_contents().contributions.size(), 1u);
            EXPECT_EQ(request.payload_contents().contributions[0].bucket, 3);
            EXPECT_EQ(request.payload_contents().contributions[0].value, 5);
            EXPECT_EQ(request.shared_info().reporting_origin, test_origin);
            EXPECT_EQ(budget_key.api(),
                      PrivateAggregationBudgetKey::Api::kFledge);
            EXPECT_EQ(budget_key.origin(), test_origin);
            run_loop.Quit();
          }));

  GURL ad_url = https_server_->GetURL(
      "c.test",
      "/fenced_frames/ad_with_fenced_frame_private_aggregation_reporting.html");
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          blink::TestInterestGroupBuilder(
              /*owner=*/test_origin,
              /*name=*/"cars")
              .SetBiddingUrl(https_server_->GetURL(
                  "a.test", "/interest_group/bidding_logic.js"))
              .SetTrustedBiddingSignalsUrl(https_server_->GetURL(
                  "a.test", "/interest_group/trusted_bidding_signals.json"))
              .SetTrustedBiddingSignalsKeys({{"key1"}})
              .SetAds({{{ad_url, R"({"ad":"metadata","here":[1,2]})"}}})
              .Build()));

  ASSERT_NO_FATAL_FAILURE(RunAuctionAndNavigateFencedFrame(
      ad_url, JsReplace(
                  R"({
                      seller: $1,
                      decisionLogicUrl: $2,
                      interestGroupBuyers: [$1],
                  })",
                  test_origin,
                  https_server_->GetURL("a.test",
                                        "/interest_group/decision_logic.js"))));

  run_loop.Run();
}

class InterestGroupAuctionLimitBrowserTest : public InterestGroupBrowserTest {
 public:
  InterestGroupAuctionLimitBrowserTest() {
    // Only 2 auctions are allowed per-page.
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{features::kFledgeLimitNumAuctions, {{"max_auctions_per_page", "2"}}},
         {blink::features::kAdInterestGroupAPIRestrictedPolicyByDefault, {}}},
        /*disabled_features=*/{});
    // TODO(crbug.com/1186444): When
    // kAdInterestGroupAPIRestrictedPolicyByDefault is the default, we won't
    // need to set it here.
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

// TODO(crbug.com/1289207): Investigate why this is failing on
// android-pie-x86-rel.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_NavigatingWithBfcachePreservesAuctionLimits \
  DISABLED_NavigatingWithBfcachePreservesAuctionLimits
#else
#define MAYBE_NavigatingWithBfcachePreservesAuctionLimits \
  NavigatingWithBfcachePreservesAuctionLimits
#endif  // BUILDFLAG(IS_ANDROID)

// Perform an auction, navigate the top-level frame, then navigate it back.
// Perform 2 more auctions. The second of those two should fail, because 2
// auctions have already been performed on the page -- one before the top level
// bfcached navigations, and one after.
//
// That is, the auction limit count is preserved due to bfcache.
IN_PROC_BROWSER_TEST_F(InterestGroupAuctionLimitBrowserTest,
                       MAYBE_NavigatingWithBfcachePreservesAuctionLimits) {
  const GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  const url::Origin test_origin = url::Origin::Create(test_url);

  EXPECT_EQ(kSuccess, JoinInterestGroupAndVerify(
                          blink::TestInterestGroupBuilder(
                              /*owner=*/test_origin,
                              /*name=*/"cars")
                              .SetBiddingUrl(https_server_->GetURL(
                                  "a.test", "/interest_group/bidding_logic.js"))
                              .SetAds({{{GURL("https://example.com/render"),
                                         R"({"ad":"metadata","here":[1,2]})"}}})
                              .Build()));

  // 1st auction -- before navigations
  EXPECT_EQ("https://example.com/render",
            RunAuctionAndWaitForUrl(JsReplace(
                R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
                })",
                test_origin,
                https_server_->GetURL("a.test",
                                      "/interest_group/decision_logic.js"))));

  // Navigate, then navigate back. The auction limits shouldn't be reset since
  // the original page goes into the bfcache.
  const GURL test_url_b = https_server_->GetURL("b.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url_b));
  TestNavigationObserver back_load_observer(shell()->web_contents());
  shell()->web_contents()->GetController().GoBack();
  back_load_observer.Wait();

  // 2nd auction -- after navigations
  EXPECT_EQ("https://example.com/render",
            RunAuctionAndWaitForUrl(JsReplace(
                R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
                })",
                test_origin,
                https_server_->GetURL("a.test",
                                      "/interest_group/decision_logic.js"))));

  // 3rd auction -- after navigations; should fail due to hitting the auction
  // limit.
  EXPECT_EQ(nullptr, RunAuctionAndWait(JsReplace(
                         R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
                })",
                         test_origin,
                         https_server_->GetURL(
                             "a.test", "/interest_group/decision_logic.js"))));
}

// Create a page with a cross-origin iframe. Run an auction in the main frame,
// then run 2 auctions in the cross-origin iframe. The last auction should fail
// due to encontering the auction limit, since the limit is stored per-page (top
// level frame), not per frame.
IN_PROC_BROWSER_TEST_F(InterestGroupAuctionLimitBrowserTest,
                       AuctionLimitSharedWithCrossOriginFrameOnPage) {
  // Give the cross-origin iframe permission to run auctions.
  const GURL test_url =
      https_server_->GetURL("a.test",
                            "/cross_site_iframe_factory.html?a.test(b.test{"
                            "allow-run-ad-auction})");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  const url::Origin test_origin = url::Origin::Create(test_url);
  RenderFrameHost* const b_iframe =
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);

  EXPECT_EQ(kSuccess, JoinInterestGroupAndVerify(
                          blink::TestInterestGroupBuilder(
                              /*owner=*/test_origin,
                              /*name=*/"cars")
                              .SetBiddingUrl(https_server_->GetURL(
                                  "a.test", "/interest_group/bidding_logic.js"))
                              .SetAds({{{GURL("https://example.com/render"),
                                         R"({"ad":"metadata","here":[1,2]})"}}})
                              .Build()));

  // 1st auction -- in main frame
  EXPECT_EQ("https://example.com/render",
            RunAuctionAndWaitForUrl(JsReplace(
                R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
                })",
                test_origin,
                https_server_->GetURL("a.test",
                                      "/interest_group/decision_logic.js"))));

  // 2nd auction -- in cross-origin iframe
  EXPECT_EQ("https://example.com/render",
            RunAuctionAndWaitForUrl(
                JsReplace(
                    R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
                })",
                    test_origin,
                    https_server_->GetURL("a.test",
                                          "/interest_group/decision_logic.js")),
                b_iframe));

  // 3rd auction -- in cross-origin iframe; should fail due to hitting the
  // auction limit.
  EXPECT_EQ(
      nullptr,
      RunAuctionAndWait(JsReplace(
                            R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
                })",
                            test_origin,
                            https_server_->GetURL(
                                "a.test", "/interest_group/decision_logic.js")),
                        b_iframe));
}

// forDebuggingOnly.reportAdAuctionLoss() and
// forDebuggingOnly.reportAdAuctionWin() APIs will be disabled (available but do
// nothing) when feature kBiddingAndScoringDebugReportingAPI is disabled.
class InterestGroupBiddingAndScoringDebugReportingAPIDisabledBrowserTest
    : public InterestGroupBrowserTest {
 public:
  InterestGroupBiddingAndScoringDebugReportingAPIDisabledBrowserTest() {
    feature_list_.InitAndDisableFeature(
        blink::features::kBiddingAndScoringDebugReportingAPI);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    InterestGroupBiddingAndScoringDebugReportingAPIDisabledBrowserTest,
    RunAdAuctionWithDebugReporting) {
  GURL test_url = https_server_->GetURL("a.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);
  GURL ad_url = https_server_->GetURL("c.test", "/echo?render_winner");
  GURL ad2_url = https_server_->GetURL("c.test", "/echo?render_bikes");

  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          blink::TestInterestGroupBuilder(
              /*owner=*/test_origin,
              /*name=*/"winner")
              .SetBiddingUrl(https_server_->GetURL(
                  "a.test",
                  "/interest_group/bidding_logic_with_debugging_report.js"))
              .SetAds({{{ad_url, R"({"ad":"metadata","here":[1,2]})"}}})
              .Build()));
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          blink::TestInterestGroupBuilder(
              /*owner=*/test_origin,
              /*name=*/"bikes")
              .SetBiddingUrl(https_server_->GetURL(
                  "a.test",
                  "/interest_group/bidding_logic_with_debugging_report.js"))
              .SetAds({{{ad2_url, /*metadata=*/absl::nullopt}}})
              .Build()));

  std::string auction_config = JsReplace(
      R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
    auctionSignals: {x: 1},
    sellerSignals: {yet: 'more', info: 1},
    perBuyerSignals: {$1: {even: 'more', x: 4.5}}
                })",
      test_origin,
      https_server_->GetURL(
          "a.test", "/interest_group/decision_logic_with_debugging_report.js"));
  RunAuctionAndWaitForURLAndNavigateIframe(auction_config, ad_url);

  // Check ResourceRequest structs of report requests.
  const GURL kExpectedReportUrls[] = {
      https_server_->GetURL("a.test", "/echoall?report_seller"),
      https_server_->GetURL("a.test", "/echoall?report_bidder/winner")};

  for (const auto& expected_report_url : kExpectedReportUrls) {
    SCOPED_TRACE(expected_report_url);
    WaitForUrl(expected_report_url);
  }

  // No requests should be sent to forDebuggingOnly reporting URLs when
  // feature kBiddingAndScoringDebugReportingAPI is disabled.
  const GURL kDebuggingReportUrls[] = {
      // Debugging report URL from winner for win report.
      https_server_->GetURL("a.test", "/echo?bidder_debug_report_win/winner"),
      // Debugging report URL from losing bidder for loss report.
      https_server_->GetURL("a.test", "/echo?bidder_debug_report_loss/bikes"),
      // Debugging report URL from seller for loss report.
      https_server_->GetURL("a.test", "/echo?seller_debug_report_loss/bikes"),
      // Debugging report URL from seller for win report.
      https_server_->GetURL("a.test", "/echo?seller_debug_report_win/winner")};
  for (const auto& debugging_report_url : kDebuggingReportUrls) {
    EXPECT_FALSE(HasServerSeenUrl(debugging_report_url));
  }
}

// Test event-level reporting of ad component fenced frame.
class InterestGroupFencedFrameAdComponentAutomaticBeaconBrowserTest
    : public InterestGroupFencedFrameBrowserTest {
 public:
  std::unique_ptr<NetworkResponder> CreateNetworkResponder() override {
    // Fenced frame window.fence.reportEvent API requires a responder that
    // handles beacons sent to the reporting url.
    return std::make_unique<NetworkResponder>(*https_server_,
                                              "/report_event.html");
  }

  void RunAdAuctionAndLoadAdComponent(GURL ad_component_url) {
    // Run ad auction with ad components and register ad beacons.
    ASSERT_NO_FATAL_FAILURE(RunBasicAuctionWithAdComponents(
        ad_component_url, /*component_ad_urn=*/nullptr,
        "bidding_logic_register_ad_beacon.js",
        "decision_logic_register_ad_beacon.js"));

    RenderFrameHostImpl* ad_frame = GetFencedFrameRenderFrameHost(shell());

    // Validate the ad components.
    CheckAdComponents(
        /*expected_ad_component_urls=*/std::vector<GURL>{ad_component_url},
        ad_frame);

    // Navigate the existing nested fenced frame to the ad component urn.
    absl::optional<std::vector<GURL>> all_component_urls =
        GetAdAuctionComponentsInJS(ad_frame, blink::kMaxAdAuctionAdComponents);
    ASSERT_TRUE(all_component_urls);
    NavigateFencedFrameAndWait((*all_component_urls)[0], ad_component_url,
                               ad_frame);
  }

  // Send a request that has the content "Basic request data" to the reporting
  // destination. This function is used in negative test cases where a reporting
  // beacon is expected not to be sent. For example:
  // 1. A click without user gesture happened on ad component fenced frame, the
  // reporting beacon should not be sent.
  // 2. Call SendBasicRequest().
  // 3. Verify the received request's content is from SendBasicRequest().
  // This works because `ControllableHttpResponse` only handles one request.
  void SendBasicRequest(GURL url) {
    // Construct the resource request.
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
        web_contents()
            ->GetPrimaryMainFrame()
            ->GetStoragePartition()
            ->GetURLLoaderFactoryForBrowserProcess();

    auto request = std::make_unique<network::ResourceRequest>();

    request->url = url;
    request->credentials_mode = network::mojom::CredentialsMode::kOmit;
    request->method = net::HttpRequestHeaders::kPostMethod;
    request->trusted_params = network::ResourceRequest::TrustedParams();
    request->trusted_params->isolation_info =
        net::IsolationInfo::CreateTransient();

    std::unique_ptr<network::SimpleURLLoader> simple_url_loader =
        network::SimpleURLLoader::Create(std::move(request),
                                         TRAFFIC_ANNOTATION_FOR_TESTS);

    simple_url_loader->AttachStringForUpload(
        "Basic request data",
        /*upload_content_type=*/"text/plain;charset=UTF-8");
    network::SimpleURLLoader* simple_url_loader_ptr = simple_url_loader.get();

    // Send out the beacon.
    simple_url_loader_ptr->DownloadHeadersOnly(
        url_loader_factory.get(),
        base::DoNothingWithBoundArgs(std::move(simple_url_loader)));
  }
};

// Test window.fence.reportEvent from an ad component fenced frame is
// disallowed:
// 1. Run an auction with an ad component.
// 2. Load the ad in a fenced frame.
// 3. Load the ad component in the nested fenced frame.
// 4. Invoke window.fence.reportEvent from the nested fenced frame.
// 5. Expect reportEvent to fail because it is not allowed from an ad component.
// For an ad component, only reserved.top_navigation beacon is allowed.
IN_PROC_BROWSER_TEST_F(
    InterestGroupFencedFrameAdComponentAutomaticBeaconBrowserTest,
    AdComponentFencedFrameReportEventNotAllowed) {
  GURL ad_component_url = https_server_->GetURL(
      "a.test", "/set-header?Supports-Loading-Mode: fenced-frame");

  ASSERT_NO_FATAL_FAILURE(RunAdAuctionAndLoadAdComponent(ad_component_url));

  RenderFrameHostImpl* ad_frame = GetFencedFrameRenderFrameHost(shell());
  RenderFrameHostImpl* ad_component_frame =
      GetFencedFrameRenderFrameHost(ad_frame);

  // Monitor the console errors.
  WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetFilter(base::BindRepeating(IsErrorMessage));
  console_observer.SetPattern(
      "This frame is an ad component. It is not allowed to call "
      "fence.reportEvent.");

  // Invoke window.fence.reportEvent from the ad component fenced frame. This
  // should fail because only reserved.top_navigation event beacon is allowed
  // from an ad component.
  EXPECT_TRUE(ExecJs(ad_component_frame, R"(
                                            window.fence.reportEvent(
                                              {
                                                eventType: 'click',
                                                eventData: 'some data',
                                                destination: ['seller']
                                              }
                                            );
                                          )"));

  // Verify the expected error is logged to the console.
  ASSERT_TRUE(console_observer.Wait());
  EXPECT_EQ(console_observer.messages().size(), 1u);
  EXPECT_EQ(base::UTF16ToUTF8(console_observer.messages()[0].message),
            "This frame is an ad component. It is not allowed to call "
            "fence.reportEvent.");

  // Send a basic request to the reporting destination.
  GURL reporting_url = https_server_->GetURL("a.test", "/report_event.html");
  SendBasicRequest(reporting_url);

  // Verify the request received is the basic request, which implies the
  // reportEvent beacon was not sent as expected.
  EXPECT_EQ(network_responder_->GetRequest()->content, "Basic request data");
  EXPECT_TRUE(network_responder_->HasReceivedRequest());
}

// Test `reserved.top_navigation` beacon from an ad component:
// 1. Run an auction with an ad component.
// 2. Load the ad in a fenced frame.
// 3. Load the ad component in the nested fenced frame.
// 4. Register the automatic beacon data.
// 5. Navigate to a same-origin url.
// 6. The automatic beacon from ad component is sent successfully.
IN_PROC_BROWSER_TEST_F(
    InterestGroupFencedFrameAdComponentAutomaticBeaconBrowserTest,
    AdComponentFencedFrameSameOriginNavigation) {
  GURL ad_component_url = https_server_->GetURL(
      "a.test", "/set-header?Supports-Loading-Mode: fenced-frame");

  ASSERT_NO_FATAL_FAILURE(RunAdAuctionAndLoadAdComponent(ad_component_url));

  RenderFrameHostImpl* ad_frame = GetFencedFrameRenderFrameHost(shell());
  RenderFrameHostImpl* ad_component_frame =
      GetFencedFrameRenderFrameHost(ad_frame);

  // Set automatic beacon data for ad component.
  EXPECT_TRUE(ExecJs(ad_component_frame, (R"(
                        window.fence.setReportEventDataForAutomaticBeacons(
                          {
                            eventType: 'reserved.top_navigation',
                            eventData: 'should be igonred',
                            destination: ['seller']
                          }
                        );
                      )")));

  // Perform a same-origin `_unfencedTop` navigation.
  GURL navigation_url = https_server_->GetURL(
      "a.test", "/set-header?Supports-Loading-Mode: fenced-frame");
  EXPECT_TRUE(
      ExecJs(ad_component_frame,
             JsReplace("window.open($1, '_unfencedTop');", navigation_url)));

  // Expect automatic beacon being sent successfully with empty event data.
  EXPECT_TRUE(network_responder_->GetRequest()->content.empty());
  EXPECT_TRUE(network_responder_->HasReceivedRequest());
}

// Test `reserved.top_navigation` beacon from an ad component:
// 1. Run an auction with an ad component.
// 2. Load the ad in a fenced frame.
// 3. Load the ad component in the nested fenced frame.
// 4. Register the automatic beacon data.
// 5. Navigate to a cross-origin url.
// 6. The automatic beacon from ad component is sent successfully.
IN_PROC_BROWSER_TEST_F(
    InterestGroupFencedFrameAdComponentAutomaticBeaconBrowserTest,
    AdComponentFencedFrameCrossOriginNavigation) {
  GURL ad_component_url = https_server_->GetURL(
      "a.test", "/set-header?Supports-Loading-Mode: fenced-frame");

  ASSERT_NO_FATAL_FAILURE(RunAdAuctionAndLoadAdComponent(ad_component_url));

  RenderFrameHostImpl* ad_frame = GetFencedFrameRenderFrameHost(shell());
  RenderFrameHostImpl* ad_component_frame =
      GetFencedFrameRenderFrameHost(ad_frame);

  // Set automatic beacon data for ad component.
  EXPECT_TRUE(ExecJs(ad_component_frame, (R"(
                        window.fence.setReportEventDataForAutomaticBeacons(
                          {
                            eventType: 'reserved.top_navigation',
                            eventData: 'should be igonred',
                            destination: ['seller']
                          }
                        );
                      )")));

  // Perform a cross-origin `_unfencedTop` navigation.
  GURL navigation_url = https_server_->GetURL(
      "b.test", "/set-header?Supports-Loading-Mode: fenced-frame");
  EXPECT_TRUE(
      ExecJs(ad_component_frame,
             JsReplace("window.open($1, '_unfencedTop');", navigation_url)));

  // Expect automatic beacon being sent successfully with empty event data.
  EXPECT_TRUE(network_responder_->GetRequest()->content.empty());
  EXPECT_TRUE(network_responder_->HasReceivedRequest());
}

// Just like `AdComponentFencedFrameCrossOriginNavigation`, but with
// BackForwardCache disabled.
IN_PROC_BROWSER_TEST_F(
    InterestGroupFencedFrameAdComponentAutomaticBeaconBrowserTest,
    AdComponentFencedFrameBFCacheDisabled) {
  DisableBackForwardCacheForTesting(shell()->web_contents(),
                                    BackForwardCache::TEST_REQUIRES_NO_CACHING);
  GURL ad_component_url = https_server_->GetURL(
      "a.test", "/set-header?Supports-Loading-Mode: fenced-frame");

  ASSERT_NO_FATAL_FAILURE(RunAdAuctionAndLoadAdComponent(ad_component_url));

  RenderFrameHostImpl* ad_frame = GetFencedFrameRenderFrameHost(shell());
  RenderFrameHostImpl* ad_component_frame =
      GetFencedFrameRenderFrameHost(ad_frame);

  // Set automatic beacon data for ad component.
  EXPECT_TRUE(ExecJs(ad_component_frame, (R"(
                        window.fence.setReportEventDataForAutomaticBeacons(
                          {
                            eventType: 'reserved.top_navigation',
                            eventData: 'should be igonred',
                            destination: ['seller']
                          }
                        );
                      )")));

  // Perform a cross-origin `_unfencedTop` navigation.
  GURL navigation_url = https_server_->GetURL(
      "b.test", "/set-header?Supports-Loading-Mode: fenced-frame");
  EXPECT_TRUE(
      ExecJs(ad_component_frame,
             JsReplace("window.open($1, '_unfencedTop');", navigation_url)));

  // Expect automatic beacon being sent successfully with empty event data.
  EXPECT_TRUE(network_responder_->GetRequest()->content.empty());
  EXPECT_TRUE(network_responder_->HasReceivedRequest());
}

// No beacon is sent if `setReportEventDataForAutomaticBeacons` is not invoked
// to register automatic beacon data in ad component.
IN_PROC_BROWSER_TEST_F(
    InterestGroupFencedFrameAdComponentAutomaticBeaconBrowserTest,
    AdComponentFencedFrameNoBeaconDataRegistered) {
  GURL ad_component_url = https_server_->GetURL(
      "a.test", "/set-header?Supports-Loading-Mode: fenced-frame");

  ASSERT_NO_FATAL_FAILURE(RunAdAuctionAndLoadAdComponent(ad_component_url));

  RenderFrameHostImpl* ad_frame = GetFencedFrameRenderFrameHost(shell());
  RenderFrameHostImpl* ad_component_frame =
      GetFencedFrameRenderFrameHost(ad_frame);

  // Perform a cross-origin `_unfencedTop` navigation, without registering any
  // automatic beacon data.
  GURL navigation_url = https_server_->GetURL(
      "b.test", "/set-header?Supports-Loading-Mode: fenced-frame");
  EXPECT_TRUE(
      ExecJs(ad_component_frame,
             JsReplace("window.open($1, '_unfencedTop');", navigation_url)));

  // Send a basic request to the reporting destination.
  GURL reporting_url = https_server_->GetURL("a.test", "/report_event.html");
  SendBasicRequest(reporting_url);

  // Verify the request received is the basic request, which implies the
  // automatic beacon was not sent as expected.
  EXPECT_EQ(network_responder_->GetRequest()->content, "Basic request data");
  EXPECT_TRUE(network_responder_->HasReceivedRequest());
}

// Set the event data to an empty string. The beacon should be sent.
IN_PROC_BROWSER_TEST_F(
    InterestGroupFencedFrameAdComponentAutomaticBeaconBrowserTest,
    AdComponentFencedFrameEmptyEventData) {
  GURL ad_component_url = https_server_->GetURL(
      "a.test", "/set-header?Supports-Loading-Mode: fenced-frame");

  ASSERT_NO_FATAL_FAILURE(RunAdAuctionAndLoadAdComponent(ad_component_url));

  RenderFrameHostImpl* ad_frame = GetFencedFrameRenderFrameHost(shell());
  RenderFrameHostImpl* ad_component_frame =
      GetFencedFrameRenderFrameHost(ad_frame);

  // Set automatic beacon data for ad component, with the eventData field being
  // an empty string.
  // TODO(xiaochenzh): The web IDL for `FenceEvent` should be updated to make
  // eventData field optional. This test should be updated when it is done.
  EXPECT_TRUE(ExecJs(ad_component_frame, (R"(
                        window.fence.setReportEventDataForAutomaticBeacons(
                          {
                            eventType: 'reserved.top_navigation',
                            eventData: '',
                            destination: ['seller']
                          }
                        );
                      )")));

  // Perform a cross-origin `_unfencedTop` navigation.
  GURL navigation_url = https_server_->GetURL(
      "b.test", "/set-header?Supports-Loading-Mode: fenced-frame");
  EXPECT_TRUE(
      ExecJs(ad_component_frame,
             JsReplace("window.open($1, '_unfencedTop');", navigation_url)));

  // Expect automatic beacon being sent successfully with empty event data.
  EXPECT_TRUE(network_responder_->GetRequest()->content.empty());
  EXPECT_TRUE(network_responder_->HasReceivedRequest());
}

// No beacon is sent if the navigation is without user activation.
IN_PROC_BROWSER_TEST_F(
    InterestGroupFencedFrameAdComponentAutomaticBeaconBrowserTest,
    AdComponentFencedFrameNoUserActivation) {
  GURL ad_component_url = https_server_->GetURL(
      "a.test", "/set-header?Supports-Loading-Mode: fenced-frame");

  ASSERT_NO_FATAL_FAILURE(RunAdAuctionAndLoadAdComponent(ad_component_url));

  RenderFrameHostImpl* ad_frame = GetFencedFrameRenderFrameHost(shell());
  RenderFrameHostImpl* ad_component_frame =
      GetFencedFrameRenderFrameHost(ad_frame);

  // Set automatic beacon data for ad component, this step must be done without
  // user gesture. Otherwise later the '_unfencedTop' navigation will find there
  // exists an user gesture.
  EXPECT_TRUE(ExecJs(ad_component_frame, (R"(
                        window.fence.setReportEventDataForAutomaticBeacons(
                          {
                            eventType: 'reserved.top_navigation',
                            eventData: 'should be igonred',
                            destination: ['seller']
                          }
                        );
                      )"),
                     EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Perform a cross-origin `_unfencedTop` navigation without user activation.
  GURL navigation_url = https_server_->GetURL(
      "b.test", "/set-header?Supports-Loading-Mode: fenced-frame");
  EXPECT_TRUE(
      ExecJs(ad_component_frame,
             JsReplace("window.open($1, '_unfencedTop');", navigation_url),
             EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Send a basic request to the reporting destination.
  GURL reporting_url = https_server_->GetURL("a.test", "/report_event.html");
  SendBasicRequest(reporting_url);

  // Verify the request received is the basic request, which implies the
  // automatic beacon was not sent as expected.
  EXPECT_EQ(network_responder_->GetRequest()->content, "Basic request data");
  EXPECT_TRUE(network_responder_->HasReceivedRequest());
}

// Call `setReportEventDataForAutomaticBeacons` without `eventData` field. The
// beacon should be sent with empty event data.
IN_PROC_BROWSER_TEST_F(
    InterestGroupFencedFrameAdComponentAutomaticBeaconBrowserTest,
    AdComponentFencedFrameNoEventData) {
  GURL ad_component_url = https_server_->GetURL(
      "a.test", "/set-header?Supports-Loading-Mode: fenced-frame");

  ASSERT_NO_FATAL_FAILURE(RunAdAuctionAndLoadAdComponent(ad_component_url));

  RenderFrameHostImpl* ad_frame = GetFencedFrameRenderFrameHost(shell());
  RenderFrameHostImpl* ad_component_frame =
      GetFencedFrameRenderFrameHost(ad_frame);

  // Set automatic beacon data for ad component without the eventData field.
  EXPECT_TRUE(ExecJs(ad_component_frame, (R"(
                        window.fence.setReportEventDataForAutomaticBeacons(
                          {
                            eventType: 'reserved.top_navigation',
                            destination: ['seller']
                          }
                        );
                      )")));

  // Perform a cross-origin `_unfencedTop` navigation.
  GURL navigation_url = https_server_->GetURL(
      "b.test", "/set-header?Supports-Loading-Mode: fenced-frame");
  EXPECT_TRUE(
      ExecJs(ad_component_frame,
             JsReplace("window.open($1, '_unfencedTop');", navigation_url)));

  // Expect automatic beacon being sent successfully with empty event data.
  EXPECT_TRUE(network_responder_->GetRequest()->content.empty());
  EXPECT_TRUE(network_responder_->HasReceivedRequest());
}

}  // namespace

}  // namespace content
