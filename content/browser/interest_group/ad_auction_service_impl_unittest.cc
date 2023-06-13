// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/ad_auction_service_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "base/barrier_closure.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "components/services/storage/shared_storage/shared_storage_manager.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/fenced_frame/fenced_frame_url_mapping.h"
#include "content/browser/interest_group/auction_process_manager.h"
#include "content/browser/interest_group/interest_group_manager_impl.h"
#include "content/browser/interest_group/interest_group_storage.h"
#include "content/browser/private_aggregation/private_aggregation_manager_impl.h"
#include "content/browser/private_aggregation/private_aggregation_test_utils.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/auction_worklet_service_impl.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "content/test/fenced_frame_test_utils.h"
#include "content/test/test_content_browser_client.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy_features.h"
#include "third_party/blink/public/mojom/interest_group/ad_auction_service.mojom.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "third_party/blink/public/mojom/parakeet/ad_request.mojom.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

constexpr char kInterestGroupName[] = "interest-group-name";
constexpr char kOriginStringA[] = "https://a.test";
constexpr char kOriginStringB[] = "https://b.test";
constexpr char kOriginStringC[] = "https://c.test";
constexpr char kOriginStringNoUpdate[] = "https://no.update.test";
constexpr char kBiddingUrlPath[] = "/interest_group/bidding_logic.js";
constexpr char kNewBiddingUrlPath[] = "/interest_group/new_bidding_logic.js";
constexpr char kDecisionUrlPath[] = "/interest_group/decision_logic.js";
constexpr char kTrustedBiddingSignalsUrlPath[] =
    "/interest_group/trusted_bidding_signals.json";
constexpr char kUpdateUrlPath[] = "/interest_group/daily_update_partial.json";
constexpr char kUpdateUrlPath2[] =
    "/interest_group/daily_update_partial_2.json";
constexpr char kUpdateUrlPath3[] =
    "/interest_group/daily_update_partial_3.json";
constexpr char kUpdateUrlPath4[] =
    "/interest_group/daily_update_partial_4.json";
constexpr char kUpdateUrlPathB[] =
    "/interest_group/daily_update_partial_b.json";
constexpr char kUpdateUrlPathC[] =
    "/interest_group/daily_update_partial_c.json";

// Returns a basic bidder script that sends reports to
// kOriginStringA/report_bidder.
std::string BasicBiddingReportScript() {
  return base::StringPrintf(R"(
function generateBid(
    interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
    browserSignals) {
  return {'ad': 'example', 'bid': 1, 'render': 'https://example.com/render'};
}
function reportWin(
    auctionSignals, perBuyerSignals, sellerSignals, browserSignals) {
  sendReportTo('%s/report_bidder');
}
                            )",
                            kOriginStringA);
}

// Returns a basic seller script that sends reports to
// kOriginStringA/report_seller.
std::string BasicSellerReportScript() {
  return base::StringPrintf(R"(
function scoreAd(
    adMetadata, bid, auctionConfig, trustedScoringSignals, browserSignals) {
  return bid;
}
function reportResult(auctionConfig, browserSignals) {
  sendReportTo('%s/report_seller');
  return {
    'success': true,
    'signalsForWinner': {'signalForWinner': 1},
    'reportUrl': '%s/report_seller',
  };
}
                            )",
                            kOriginStringA, kOriginStringA);
}

class AllowInterestGroupContentBrowserClient : public TestContentBrowserClient {
 public:
  explicit AllowInterestGroupContentBrowserClient() = default;
  ~AllowInterestGroupContentBrowserClient() override = default;

  AllowInterestGroupContentBrowserClient(
      const AllowInterestGroupContentBrowserClient&) = delete;
  AllowInterestGroupContentBrowserClient& operator=(
      const AllowInterestGroupContentBrowserClient&) = delete;

  // ContentBrowserClient overrides:
  bool IsInterestGroupAPIAllowed(content::RenderFrameHost* render_frame_host,
                                 InterestGroupApiOperation operation,
                                 const url::Origin& top_frame_origin,
                                 const url::Origin& api_origin) override {
    // No updating allowed on no.update.test.
    if (operation == ContentBrowserClient::InterestGroupApiOperation::kUpdate &&
        api_origin.host() == "no.update.test") {
      return false;
    }

    // Can join A interest groups on A top frames, B interest groups on B top
    // frames, C interest groups on C top frames, C interest groups on A top
    // frames, and no.update.test interest groups on no.update.test top frames.
    return (top_frame_origin.host() == "a.test" &&
            api_origin.host() == "a.test") ||
           (top_frame_origin.host() == "b.test" &&
            api_origin.host() == "b.test") ||
           (top_frame_origin.host() == "c.test" &&
            api_origin.host() == "c.test") ||
           (top_frame_origin.host() == "a.test" &&
            api_origin.host() == "c.test") ||
           (top_frame_origin.host() == "no.update.test" &&
            api_origin.host() == "no.update.test");
  }
};

constexpr char kFledgeUpdateHeaders[] =
    "HTTP/1.1 200 OK\n"
    "Content-type: Application/JSON\n"
    "X-Allow-FLEDGE: true\n";

constexpr char kFledgeScriptHeaders[] =
    "HTTP/1.1 200 OK\n"
    "Content-type: Application/Javascript\n"
    "X-Allow-FLEDGE: true\n";

constexpr char kFledgeReportHeaders[] =
    "HTTP/1.1 200 OK\n"
    "X-Allow-FLEDGE: true\n";

// Allows registering network responses to update and scoring / bidding script
// requests; *must* be destroyed before the task environment is shutdown (which
// happens in RenderViewHostTestHarness::TearDown()).
//
// Updates and script serving have different requirements, but unfortunately
// it's not possible to simultaneously instantiate 2 classes that both use their
// own URLLoaderInterceptor...so these are combined in this same class.
class NetworkResponder {
 public:
  // Register interest group update `response` to be served with JSON content
  // type when a request to `url_path` is made.
  void RegisterUpdateResponse(const std::string& url_path,
                              const std::string& response) {
    base::AutoLock auto_lock(lock_);
    json_update_map_[url_path] = response;
  }

  // Register script `response` to be served with Javascript content type when a
  // request to `url_path` is made.
  void RegisterScriptResponse(const std::string& url_path,
                              const std::string& response) {
    base::AutoLock auto_lock(lock_);
    script_map_[url_path] = response;
  }

  // Register ad auction reporting `response` to be served when a request to
  // `url_path` is made.
  void RegisterReportResponse(const std::string& url_path,
                              const std::string& response) {
    base::AutoLock auto_lock(lock_);
    report_map_[url_path] = response;
  }

  // Registers a URL to use a "deferred" script response. For a deferred
  // response, the request handler returns true without a write, and writes are
  // performed later in DoDeferred[Script|Update]Write() using a "stolen" Mojo
  // pipe to the URLLoaderClient.
  //
  // It is valid to have a "deferred" response that never completes before the
  // test exits.
  void RegisterDeferredScriptResponse(const std::string& url_path) {
    RegisterDeferredResponse(url_path, /*is_update=*/false);
  }

  // Just like RegisterDeferredResponse(), but for deferred update responses.
  void RegisterDeferredUpdateResponse(const std::string& url_path) {
    RegisterDeferredResponse(url_path, /*is_update=*/true);
  }

  // Checks if there's a deferred response (of any type) for `url_path`.
  bool HasPendingResponse(const std::string& url_path) const {
    base::AutoLock auto_lock(lock_);
    auto it = deferred_responses_map_.find(url_path);
    CHECK(it != deferred_responses_map_.end());
    return it->second.url_loader_client.is_bound() &&
           it->second.url_loader_client.is_connected();
  }

  // Perform the deferred response for `url_path`, using response headers
  // associated with scripts -- the test fails if the client isn't waiting on
  // `url_path` registered with RegisterDeferredResponse().
  void DoDeferredScriptResponse(const std::string& url_path,
                                const std::string& response) {
    DoDeferredResponse(url_path, response, kFledgeScriptHeaders,
                       /*expected_is_update=*/false);
  }

  // Perform the deferred response for `url_path`, using response headers
  // associated with updates -- the test fails if the client isn't waiting on
  // `url_path` registered with RegisterDeferredResponse().
  void DoDeferredUpdateResponse(const std::string& url_path,
                                const std::string& response) {
    DoDeferredResponse(url_path, response, kFledgeScriptHeaders,
                       /*expected_is_update=*/true);
  }

  // Registers a URL that, when seen, will have its URLLoaderClient stored in
  // `stored_url_loader_client_` without sending a response.
  //
  // Only one request can be handled with this method at a time.
  void RegisterStoreUrlLoaderClient(const std::string& url_path) {
    base::AutoLock auto_lock(lock_);
    store_url_loader_client_url_path_ = url_path;
  }

  // Make the next request fail with `error` -- subsequent requests will succeed
  // again unless another FailNextUpdateRequestWithError() call is made.
  //
  // TODO(crbug.com/1298593): Replace this with FailUpdateRequestWithError().
  void FailNextUpdateRequestWithError(net::Error error) {
    base::AutoLock auto_lock(lock_);
    update_next_error_ = error;
  }

  // Like FailNextUpdateRequestWithError(), but for a specific path.
  void FailUpdateRequestWithError(const std::string& path, net::Error error) {
    base::AutoLock auto_lock(lock_);
    update_error_ = error;
    update_error_path_ = path;
  }

  // Like FailUpdateRequestWithError(), but doesn't alter the update count or
  // expect transient NIKs.
  void FailRequestWithError(const std::string& path, net::Error error) {
    base::AutoLock auto_lock(lock_);
    non_update_error_ = error;
    non_update_error_path_ = path;
  }

  // Returns the number of updates that occurred -- does not include other
  // network requests.
  size_t UpdateCount() const {
    base::AutoLock auto_lock(lock_);
    return update_count_;
  }

  // Returns the number of reports that occurred -- does not include other
  // network requests.
  size_t ReportCount() const {
    base::AutoLock auto_lock(lock_);
    return report_count_;
  }

  // Returns true if the network request for path received a response.
  bool ReportSent(const std::string& path) const {
    base::AutoLock auto_lock(lock_);
    return base::Contains(sent_reports_, path);
  }

  // Indicates whether `stored_url_loader_client_` is connected to a receiver.
  bool RemoteIsConnected() {
    base::AutoLock auto_lock(lock_);
    return stored_url_loader_client_.is_connected();
  }

  void WaitForNumReports(size_t num_reports) {
    base::RunLoop run_loop;
    {
      base::AutoLock auto_lock(lock_);
      DCHECK(!quit_report_wait_loop_callback_);
      EXPECT_LE(report_count_, num_reports);
      if (report_count_ >= num_reports)
        return;
      waiting_for_report_count_ = num_reports;
      quit_report_wait_loop_callback_ = run_loop.QuitClosure();
    }

    run_loop.Run();

    {
      base::AutoLock auto_lock(lock_);
      waiting_for_report_count_ = 0;
      EXPECT_EQ(report_count_, num_reports);
    }
  }

 private:
  bool RequestHandler(URLLoaderInterceptor::RequestParams* params) {
    base::AutoLock auto_lock(lock_);

    // Check deferred responses map first.
    const auto deferred_it =
        deferred_responses_map_.find(params->url_request.url.path());
    if (deferred_it != deferred_responses_map_.end()) {
      CHECK(!deferred_it->second.url_loader_client);
      deferred_it->second.url_loader_client = std::move(params->client);
      if (deferred_it->second.is_update) {
        OnUpdateRequestReceived(params);
      }
      return true;
    }

    // Cross-origin iframe handling is covered by integration tests, for cases
    // that request .well-known URLs.
    if (params->url_request.url.path_piece() ==
        "/.well-known/interest-group/permissions/") {
      CHECK(false);
      return false;
    }

    // Check if this is a non-update error.
    if (params->url_request.url.path() == non_update_error_path_) {
      CHECK(non_update_error_ != net::OK);
      params->client->OnComplete(
          network::URLLoaderCompletionStatus(non_update_error_));
      return true;
    }

    // Not a non-update error, check if this is a script request.
    const auto script_it = script_map_.find(params->url_request.url.path());
    if (script_it != script_map_.end()) {
      URLLoaderInterceptor::WriteResponse(
          kFledgeScriptHeaders, script_it->second, params->client.get());
      return true;
    }

    // Not a non-update error or script request, check if it's a reporting
    // request.
    const auto report_it = report_map_.find(params->url_request.url.path());
    if (report_it != report_map_.end()) {
      URLLoaderInterceptor::WriteResponse(
          kFledgeReportHeaders, report_it->second, params->client.get());
      sent_reports_.push_back(params->url_request.url.path());
      OnReportSent();
      return true;
    }

    if ((params->url_request.url.path() == store_url_loader_client_url_path_)) {
      CHECK(!stored_url_loader_client_);
      stored_url_loader_client_ = std::move(params->client);
      OnReportSent();
      return true;
    }

    // Not a non-update error, script request, or report request, so consider
    // this an update request.
    OnUpdateRequestReceived(params);
    const auto update_it =
        json_update_map_.find(params->url_request.url.path());
    if (update_it != json_update_map_.end()) {
      URLLoaderInterceptor::WriteResponse(
          kFledgeUpdateHeaders, update_it->second, params->client.get());
      return true;
    }

    if (params->url_request.url.path() == update_error_path_) {
      CHECK(update_error_ != net::OK);
      params->client->OnComplete(
          network::URLLoaderCompletionStatus(update_error_));
      return true;
    }

    if (update_next_error_ != net::OK) {
      params->client->OnComplete(
          network::URLLoaderCompletionStatus(update_next_error_));
      update_next_error_ = net::OK;
      return true;
    }

    return false;
  }

  void OnReportSent() EXCLUSIVE_LOCKS_REQUIRED(lock_) {
    ++report_count_;
    if (waiting_for_report_count_ == report_count_)
      std::move(quit_report_wait_loop_callback_).Run();
  }

  void OnUpdateRequestReceived(URLLoaderInterceptor::RequestParams* params)
      EXCLUSIVE_LOCKS_REQUIRED(lock_) {
    update_count_++;
    EXPECT_TRUE(params->url_request.trusted_params);
    EXPECT_TRUE(params->url_request.trusted_params->isolation_info
                    .network_isolation_key()
                    .IsTransient());
  }

  void RegisterDeferredResponse(const std::string& url_path, bool is_update) {
    base::AutoLock auto_lock(lock_);
    CHECK(deferred_responses_map_
              .emplace(url_path,
                       DeferredResponseInfo{
                           mojo::Remote<network::mojom::URLLoaderClient>(),
                           is_update})
              .second);
  }

  void DoDeferredResponse(const std::string& url_path,
                          const std::string& response,
                          const std::string headers,
                          bool expected_is_update) {
    base::AutoLock auto_lock(lock_);
    auto it = deferred_responses_map_.find(url_path);
    CHECK(it != deferred_responses_map_.end());
    EXPECT_EQ(expected_is_update, it->second.is_update);
    mojo::Remote<network::mojom::URLLoaderClient>& url_loader_client =
        it->second.url_loader_client;
    CHECK(url_loader_client.is_bound());
    URLLoaderInterceptor::WriteResponse(headers, response,
                                        url_loader_client.get());
    deferred_responses_map_.erase(it);
  }

  // Handles network requests for interest group updates and scripts.
  URLLoaderInterceptor network_interceptor_{
      base::BindRepeating(&NetworkResponder::RequestHandler,
                          base::Unretained(this))};

  mutable base::Lock lock_;

  // For each HTTPS request, we see if any path in the map matches the request
  // path. If so, the server returns the mapped value string as the response,
  // with JSON MIME type.
  base::flat_map<std::string, std::string> json_update_map_ GUARDED_BY(lock_);

  // Like `json_update_map_`, but for serving bidding / scoring scripts, with
  // the Javascript MIME type.
  base::flat_map<std::string, std::string> script_map_ GUARDED_BY(lock_);

  // Like `json_update_map_`, but for reporting requests.
  base::flat_map<std::string, std::string> report_map_ GUARDED_BY(lock_);

  // Only saves reporting requests that auctually received responses.
  std::vector<std::string> sent_reports_ GUARDED_BY(lock_);

  struct DeferredResponseInfo {
    mojo::Remote<network::mojom::URLLoaderClient> url_loader_client;
    bool is_update = false;
  };

  // Stores the set of URL paths that will receive a deferred response.
  //
  // First, a URL path is registered to defer the response, but the mapped value
  // will not be bound.
  //
  // Next, once a request is made for that URL path, the
  // URLLoaderClient used for the request is stored as the value for that URL
  // path.
  //
  // Finally, after the deferred response is made, the key-value pair for that
  // response is removed from the map.
  //
  // It is valid to have a "deferred" response that never completes before the
  // test exits.
  base::flat_map<std::string, DeferredResponseInfo> deferred_responses_map_
      GUARDED_BY(lock_);

  // Stores the last URL path that was registered with
  // RegisterStoreUrlLoaderClient().
  std::string store_url_loader_client_url_path_ GUARDED_BY(lock_);

  // Stores the Mojo URLLoaderClient remote "stolen" from
  // RequestHandlerForUpdates() for use with no responses -- unbound if no
  // remote has been "stolen" yet, or if the last no response request timed out.
  mojo::Remote<network::mojom::URLLoaderClient> stored_url_loader_client_
      GUARDED_BY(lock_);

  // For updates, fail the next request with `update_next_error_` if
  // `update_next_error_` is not net::OK.
  net::Error update_next_error_ GUARDED_BY(lock_) = net::OK;

  // For updates, the error to return if `update_error_path_` matches the path
  // of the current request.
  net::Error update_error_ GUARDED_BY(lock_) = net::OK;

  // For updates, if the current request's path matches `update_error_path_`,
  // fail the request with `update_error_`.
  std::string update_error_path_ GUARDED_BY(lock_);

  // The non-update variant doesn't alter the update attempt counter or check
  // for transient NIKs.

  // For non-updates, the error to return if `update_error_path_` matches the
  // path of the current request.
  net::Error non_update_error_ GUARDED_BY(lock_) = net::OK;

  // For non-updates, if the current request's path matches
  // `update_error_path_`, fail the request with `update_error_`.
  std::string non_update_error_path_ GUARDED_BY(lock_);

  size_t update_count_ GUARDED_BY(lock_) = 0;

  size_t report_count_ GUARDED_BY(lock_) = 0;

  // Used to wait for a specific number of reports.
  size_t waiting_for_report_count_ GUARDED_BY(lock_) = 0;
  base::OnceClosure quit_report_wait_loop_callback_ GUARDED_BY(lock_);
};

// AuctionProcessManager that allows running auctions in-proc.
class SameProcessAuctionProcessManager : public AuctionProcessManager {
 public:
  SameProcessAuctionProcessManager() = default;
  SameProcessAuctionProcessManager(const SameProcessAuctionProcessManager&) =
      delete;
  SameProcessAuctionProcessManager& operator=(
      const SameProcessAuctionProcessManager&) = delete;
  ~SameProcessAuctionProcessManager() override = default;

 private:
  RenderProcessHost* LaunchProcess(
      mojo::PendingReceiver<auction_worklet::mojom::AuctionWorkletService>
          auction_worklet_service_receiver,
      const ProcessHandle* handle,
      const std::string& display_name) override {
    // Create one AuctionWorkletServiceImpl per Mojo pipe, just like in
    // production code. Don't bother to delete the service on pipe close,
    // though; just keep it in a vector instead.
    auction_worklet_services_.push_back(
        auction_worklet::AuctionWorkletServiceImpl::CreateForService(
            std::move(auction_worklet_service_receiver)));
    return nullptr;
  }

  scoped_refptr<SiteInstance> MaybeComputeSiteInstance(
      SiteInstance* frame_site_instance,
      const url::Origin& worklet_origin) override {
    return nullptr;
  }

  bool TryUseSharedProcess(ProcessHandle* process_handle) override {
    return false;
  }

  std::vector<std::unique_ptr<auction_worklet::AuctionWorkletServiceImpl>>
      auction_worklet_services_;
};

class TestKAnonymityServiceDelegate : public KAnonymityServiceDelegate {
 public:
  TestKAnonymityServiceDelegate() = default;

  void JoinSet(std::string id,
               base::OnceCallback<void(bool)> callback) override {
    join_ids_.push_back(id);
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), true));
  }

  void QuerySets(
      std::vector<std::string> ids,
      base::OnceCallback<void(std::vector<bool>)> callback) override {
    // Return that nothing is k-anonymous.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  std::vector<bool>(ids.size(), false)));
  }

  base::TimeDelta GetJoinInterval() override { return base::Seconds(1); }

  base::TimeDelta GetQueryInterval() override { return base::Seconds(1); }

  const std::vector<std::string>& join_ids() const { return join_ids_; }

 private:
  std::vector<std::string> join_ids_;
};

}  // namespace

// Tests the interest group management functionality of AdAuctionServiceImpl --
// this particular functionality used to be in a separate interface called
// RestrictedInterestStore. The interfaces were combined so so that they'd share
// a Mojo pipe (for message ordering consistency).
class AdAuctionServiceImplTest : public RenderViewHostTestHarness {
 public:
  AdAuctionServiceImplTest()
      : RenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{blink::features::kInterestGroupStorage,
                              blink::features::kAdInterestGroupAPI,
                              blink::features::kFledge},
        /*disabled_features=*/{});
    fenced_frame_feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kFencedFrames, {{"implementation_type", "mparch"}});
    old_content_browser_client_ =
        SetBrowserClientForTesting(&content_browser_client_);
  }

  ~AdAuctionServiceImplTest() override {
    SetBrowserClientForTesting(old_content_browser_client_);
  }

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    NavigateAndCommit(kUrlA);

    manager_ = static_cast<InterestGroupManagerImpl*>(
        browser_context()
            ->GetDefaultStoragePartition()
            ->GetInterestGroupManager());
    // Process creation crashes in the Chrome zygote init in unit tests, so run
    // the auction "processes" in-process instead.
    manager_->set_auction_process_manager_for_testing(
        std::make_unique<SameProcessAuctionProcessManager>());
    manager_->set_k_anonymity_manager_for_testing(
        std::make_unique<InterestGroupKAnonymityManager>(manager_.get(),
                                                         &k_anon_delegate_));
  }

  void TearDown() override {
    // `network_responder_` must be destructed while the task environment,
    // which gets destroyed by RenderViewHostTestHarness::TearDown(), is still
    // active.
    network_responder_.reset();
    RenderViewHostTestHarness::TearDown();
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

  // Returns the specified interest group, if it exists.
  absl::optional<StorageInterestGroup> GetInterestGroup(
      const url::Origin& owner,
      const std::string& name) {
    for (auto& interest_group : GetInterestGroupsForOwner(owner)) {
      if (interest_group.interest_group.name == name) {
        return std::move(interest_group);
      }
    }
    return absl::nullopt;
  }

  int GetJoinCount(const url::Origin& owner, const std::string& name) {
    auto interest_group = GetInterestGroup(owner, name);
    if (!interest_group)
      return 0;
    return interest_group->bidding_browser_signals->join_count;
  }

  double GetPriority(const url::Origin& owner, const std::string& name) {
    auto interest_group = GetInterestGroup(owner, name);
    if (!interest_group)
      return 0;
    return interest_group->interest_group.priority;
  }

  // Retrieves the FencedFrameProperties for the specified URN from the main
  // frame. Returns nullopt if no such URN exists.
  absl::optional<FencedFrameProperties> GetFencedFramePropertiesForURN(
      const GURL& urn_url) {
    TestFencedFrameURLMappingResultObserver observer;
    FencedFrameURLMapping& fenced_frame_urls_map =
        static_cast<RenderFrameHostImpl*>(main_rfh())
            ->GetPage()
            .fenced_frame_urls_map();
    fenced_frame_urls_map.ConvertFencedFrameURNToURL(urn_url, &observer);
    return observer.fenced_frame_properties();
  }

  absl::optional<GURL> ConvertFencedFrameURNToURL(const GURL& urn_url) {
    auto properties = GetFencedFramePropertiesForURN(urn_url);
    if (properties && properties->mapped_url_.has_value()) {
      return properties->mapped_url_->GetValueIgnoringVisibility();
    }
    return absl::nullopt;
  }

  // Invokes the callback for the provided URN in the scope of the main frame,
  // as would happen if it were navigated to. Doesn't use NavigationSimulator
  // because it doesn't seem capable of triggering URN swaps. Integration tests
  // cover actual swap cases.
  void InvokeCallbackForURN(const GURL& urn_url) {
    auto properties = GetFencedFramePropertiesForURN(urn_url);
    ASSERT_TRUE(properties);
    properties->on_navigate_callback_.Run();
  }

  // Creates a new AdAuctionServiceImpl and use it to try and join
  // `interest_group`. Waits for the operation to signal completion.
  //
  // Creates a new AdAuctionServiceImpl with each call so the RFH
  // can be navigated between different sites. And
  // AdAuctionServiceImpl only handles one site (cross site navs use
  // different AdAuctionServices, and generally use different
  // RFHs as well).
  //
  // If `rfh` is nullptr, uses the main frame.
  void JoinInterestGroupAndFlush(const blink::InterestGroup& interest_group,
                                 RenderFrameHost* rfh = nullptr) {
    mojo::Remote<blink::mojom::AdAuctionService> interest_service;
    AdAuctionServiceImpl::CreateMojoService(
        rfh ? rfh : main_rfh(), interest_service.BindNewPipeAndPassReceiver());

    base::RunLoop run_loop;
    interest_service->JoinInterestGroup(
        interest_group,
        base::BindLambdaForTesting(
            [&](bool failed_well_known_check) { run_loop.Quit(); }));
    run_loop.Run();

    // Pipe should not have been closed - if it is expected to be closed, use
    // JoinInterestGroupAndExpectPipeClosed().
    EXPECT_TRUE(interest_service.is_bound());
    EXPECT_TRUE(interest_service.is_connected());
  }

  // Attempt to join an interest group and expect the Mojo pipe to be closed.
  // This happens when an operation should have been rejected in the renderer,
  // so should only happen if the renderer has been compromised.
  //
  // If `rfh` is nullptr, uses the main frame.
  void JoinInterestGroupAndExpectPipeClosed(
      const blink::InterestGroup& interest_group,
      RenderFrameHost* rfh = nullptr) {
    mojo::Remote<blink::mojom::AdAuctionService> interest_service;
    AdAuctionServiceImpl::CreateMojoService(
        rfh ? rfh : main_rfh(), interest_service.BindNewPipeAndPassReceiver());

    base::RunLoop run_loop;
    interest_service.set_disconnect_handler(run_loop.QuitClosure());
    interest_service->JoinInterestGroup(
        interest_group, base::BindOnce([](bool failed_well_known_check) {
          ADD_FAILURE() << "This callback should not be invoked.";
        }));
    run_loop.Run();
  }

  // Analogous to JoinInterestGroupAndFlush(), but leaves an interest
  // group instead of joining one.
  void LeaveInterestGroupAndFlush(const url::Origin& owner,
                                  const std::string& name,
                                  RenderFrameHost* rfh = nullptr) {
    mojo::Remote<blink::mojom::AdAuctionService> interest_service;
    AdAuctionServiceImpl::CreateMojoService(
        rfh ? rfh : main_rfh(), interest_service.BindNewPipeAndPassReceiver());

    base::RunLoop run_loop;
    interest_service->LeaveInterestGroup(
        owner, name,
        base::BindLambdaForTesting(
            [&](bool failed_well_known_check) { run_loop.Quit(); }));
    run_loop.Run();

    // Pipe should not have been closed - if it is expected to be closed, use
    // LeaveInterestGroupAndExpectPipeClosed().
    EXPECT_TRUE(interest_service.is_bound());
    EXPECT_TRUE(interest_service.is_connected());
  }

  // Analogous to JoinInterestGroupAndExpectPipeClosed(), but leaves an interest
  // group instead of joining one.
  void LeaveInterestGroupAndExpectPipeClosed(const url::Origin& owner,
                                             const std::string& name,
                                             RenderFrameHost* rfh = nullptr) {
    mojo::Remote<blink::mojom::AdAuctionService> interest_service;
    AdAuctionServiceImpl::CreateMojoService(
        rfh ? rfh : main_rfh(), interest_service.BindNewPipeAndPassReceiver());

    base::RunLoop run_loop;
    interest_service.set_disconnect_handler(run_loop.QuitClosure());
    interest_service->LeaveInterestGroup(
        owner, name, base::BindOnce([](bool failed_well_known_check) {
          ADD_FAILURE() << "This callback should not be invoked.";
        }));
    run_loop.Run();
  }

  // Updates registered interest groups according to their registered update
  // URL. Doesn't flush since the update operation requires a sequence of
  // asynchronous operations.
  void UpdateInterestGroupNoFlushForFrame(RenderFrameHost* rfh) {
    mojo::Remote<blink::mojom::AdAuctionService> interest_service;
    AdAuctionServiceImpl::CreateMojoService(
        rfh, interest_service.BindNewPipeAndPassReceiver());

    interest_service->UpdateAdInterestGroups();
  }

  // Runs an ad auction using the config specified in `auction_config` in the
  // frame `rfh`. Returns the result of the auction, which is either a URL to
  // the winning ad, or absl::nullopt if no ad won the auction.
  absl::optional<GURL> RunAdAuctionAndFlushForFrame(
      const blink::AuctionConfig& auction_config,
      RenderFrameHost* rfh) {
    // Use a new service for each call. Keep the service alive as some calls
    // (e.g., sending reports via the URN callback) require it not be deleted.
    ad_auction_service_.reset();
    AdAuctionServiceImpl::CreateMojoService(
        rfh, ad_auction_service_.BindNewPipeAndPassReceiver());

    base::RunLoop run_loop;
    absl::optional<blink::FencedFrame::RedactedFencedFrameConfig> maybe_config;
    ad_auction_service_->RunAdAuction(
        auction_config, mojo::NullReceiver(),
        base::BindLambdaForTesting(
            [&run_loop, &maybe_config](
                bool manually_aborted,
                const absl::optional<
                    blink::FencedFrame::RedactedFencedFrameConfig>& config) {
              EXPECT_FALSE(manually_aborted);
              maybe_config = config;
              run_loop.Quit();
            }));
    ad_auction_service_.FlushForTesting();
    run_loop.Run();
    if (!maybe_config) {
      return absl::nullopt;
    }
    CHECK(maybe_config->urn_uuid().has_value());
    return maybe_config->urn_uuid();
  }

  // Like RunAdAuctionAndFlushForFrame(), but uses the RenderFrameHost of the
  // main frame.
  absl::optional<GURL> RunAdAuctionAndFlush(
      const blink::AuctionConfig& auction_config) {
    return RunAdAuctionAndFlushForFrame(auction_config, main_rfh());
  }

  // Like UpdateInterestGroupNoFlushForFrame, but uses the RenderFrameHost of
  // the main frame.
  void UpdateInterestGroupNoFlush() {
    UpdateInterestGroupNoFlushForFrame(main_rfh());
  }

  // Helper to create a valid interest group with only an origin and name. All
  // URLs are nullopt.
  blink::InterestGroup CreateInterestGroup() {
    blink::InterestGroup interest_group;
    interest_group.expiry = base::Time::Now() + base::Seconds(300);
    interest_group.name = kInterestGroupName;
    interest_group.owner = kOriginA;
    return interest_group;
  }

  void CreateAdRequest(blink::mojom::AdRequestConfigPtr config,
                       AdAuctionServiceImpl::CreateAdRequestCallback callback) {
    mojo::Remote<blink::mojom::AdAuctionService> interest_service;
    AdAuctionServiceImpl::CreateMojoService(
        main_rfh(), interest_service.BindNewPipeAndPassReceiver());

    interest_service->CreateAdRequest(std::move(config), std::move(callback));
    interest_service.FlushForTesting();
  }

  // Finalizes an ad and expects the Mojo pipe to be closed without invoking the
  // callback, as should be done in the case of a bad Mojo message.
  void FinalizeAdAndExpectPipeClosed(const std::string& guid,
                                     const blink::AuctionConfig& config) {
    mojo::Remote<blink::mojom::AdAuctionService> interest_service;
    AdAuctionServiceImpl::CreateMojoService(
        main_rfh(), interest_service.BindNewPipeAndPassReceiver());

    base::RunLoop run_loop;
    interest_service.set_disconnect_handler(run_loop.QuitClosure());
    interest_service->FinalizeAd(
        guid, config,
        base::BindLambdaForTesting(
            [&](const absl::optional<GURL>& creative_url) {
              ADD_FAILURE() << "Callback unexpectedly invoked.";
            }));
    run_loop.Run();
  }

  // Destroy the AdAuctionService, if there is one.
  void DestroyAdAuctionService() { ad_auction_service_.reset(); }

  const std::vector<std::string>& GetKAnonJoinedIds() const {
    return k_anon_delegate_.join_ids();
  }

 protected:
  const GURL kUrlA = GURL(kOriginStringA);
  const url::Origin kOriginA = url::Origin::Create(kUrlA);
  const GURL kUrlB = GURL(kOriginStringB);
  const url::Origin kOriginB = url::Origin::Create(kUrlB);
  const GURL kUrlC = GURL(kOriginStringC);
  const url::Origin kOriginC = url::Origin::Create(kUrlC);
  const GURL kUrlNoUpdate = GURL(kOriginStringNoUpdate);
  const url::Origin kOriginNoUpdate = url::Origin::Create(kUrlNoUpdate);
  const GURL kBiddingLogicUrlA = kUrlA.Resolve(kBiddingUrlPath);
  const GURL kNewBiddingLogicUrlA = kUrlA.Resolve(kNewBiddingUrlPath);
  const GURL kTrustedBiddingSignalsUrlA =
      kUrlA.Resolve(kTrustedBiddingSignalsUrlPath);
  const GURL kUpdateUrlA = kUrlA.Resolve(kUpdateUrlPath);
  const GURL kUpdateUrlA2 = kUrlA.Resolve(kUpdateUrlPath2);
  const GURL kUpdateUrlA3 = kUrlA.Resolve(kUpdateUrlPath3);
  const GURL kUpdateUrlA4 = kUrlA.Resolve(kUpdateUrlPath4);
  const GURL kUpdateUrlB = kUrlB.Resolve(kUpdateUrlPathB);
  const GURL kUpdateUrlC = kUrlC.Resolve(kUpdateUrlPathC);
  const GURL kUpdateUrlNoUpdate = kUrlNoUpdate.Resolve(kUpdateUrlPath);

  base::test::ScopedFeatureList feature_list_;
  base::test::ScopedFeatureList fenced_frame_feature_list_;

  AllowInterestGroupContentBrowserClient content_browser_client_;
  TestKAnonymityServiceDelegate k_anon_delegate_;
  raw_ptr<ContentBrowserClient> old_content_browser_client_ = nullptr;
  raw_ptr<InterestGroupManagerImpl> manager_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;

  // Must be destroyed before RenderViewHostTestHarness::TearDown().
  std::unique_ptr<NetworkResponder> network_responder_{
      std::make_unique<NetworkResponder>()};

  mojo::Remote<blink::mojom::AdAuctionService> ad_auction_service_;
};

// Check basic success case.
TEST_F(AdAuctionServiceImplTest, JoinInterestGroupBasic) {
  blink::InterestGroup interest_group = CreateInterestGroup();
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  // Several tests assume interest group API are also allowed on kOriginB, so
  // make sure that's enabled correctly.
  NavigateAndCommit(kUrlB);
  interest_group.owner = kOriginB;
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginB, kInterestGroupName));
}

// Non-HTTPS frames should not be able to join interest groups.
TEST_F(AdAuctionServiceImplTest, JoinInterestGroupFrameNotHttps) {
  // Note that the ContentBrowserClient allows URLs based on hosts, not origins,
  // so it should not block this URL. Instead, it should run into the HTTPS
  // check.
  const GURL kHttpUrlA = GURL("http://a.test/");
  const url::Origin kHttpOriginA = url::Origin::Create(kHttpUrlA);
  NavigateAndCommit(kHttpUrlA);

  // Try to join an HTTPS interest group.
  blink::InterestGroup interest_group = CreateInterestGroup();
  JoinInterestGroupAndExpectPipeClosed(interest_group);
  EXPECT_EQ(0, GetJoinCount(interest_group.owner, kInterestGroupName));

  // Try to join a same-origin HTTP interest group.
  interest_group.owner = kHttpOriginA;
  JoinInterestGroupAndExpectPipeClosed(interest_group);
  EXPECT_EQ(0, GetJoinCount(kHttpOriginA, kInterestGroupName));
}

// Try to join a non-HTTPS interest group.
TEST_F(AdAuctionServiceImplTest, JoinInterestGroupOwnerNotHttps) {
  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.owner = url::Origin::Create(GURL("http://a.test/"));
  JoinInterestGroupAndExpectPipeClosed(interest_group);
  EXPECT_EQ(0, GetJoinCount(interest_group.owner, kInterestGroupName));

  // Secure, but not HTTPS.
  interest_group.owner = url::Origin::Create(GURL("wss://a.test/"));
  JoinInterestGroupAndExpectPipeClosed(interest_group);
  EXPECT_EQ(0, GetJoinCount(interest_group.owner, kInterestGroupName));
}

// Test joining an interest group with a disallowed URL. Doesn't
// exhaustively test all cases, as the validation function has its own unit
// tests. This is just to make sure those are hooked up.
TEST_F(AdAuctionServiceImplTest, JoinInterestGroupDisallowedUrls) {
  const GURL kBadUrl = GURL("https://user:pass@a.test/");

  // Test `bidding_url`.
  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.bidding_url = kBadUrl;
  JoinInterestGroupAndExpectPipeClosed(interest_group);
  EXPECT_EQ(0, GetJoinCount(kOriginA, kInterestGroupName));

  // Test `update_url`.
  interest_group = CreateInterestGroup();
  interest_group.update_url = kBadUrl;
  JoinInterestGroupAndExpectPipeClosed(interest_group);
  EXPECT_EQ(0, GetJoinCount(kOriginA, kInterestGroupName));

  // Test `trusted_bidding_signals_url`.
  interest_group = CreateInterestGroup();
  interest_group.trusted_bidding_signals_url = kBadUrl;
  JoinInterestGroupAndExpectPipeClosed(interest_group);
  EXPECT_EQ(0, GetJoinCount(kOriginA, kInterestGroupName));
}

// Attempt to join an interest group whose size is very large. No join should
// happen -- it should fail and close the pipe.
TEST_F(AdAuctionServiceImplTest, JoinMassiveInterestGroupFails) {
  blink::InterestGroup interest_group = CreateInterestGroup();
  // 1 MiB of '5' characters is over the size limit.
  interest_group.user_bidding_signals = std::string(1024 * 1024, '5');
  JoinInterestGroupAndExpectPipeClosed(interest_group);

  EXPECT_EQ(0, GetJoinCount(kOriginA, kInterestGroupName));
  std::vector<StorageInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 0u);
}

// Non-HTTPS interest groups should be rejected, and result in the pipe being
// closed.
TEST_F(AdAuctionServiceImplTest, LeaveInterestGroupOriginNotHttps) {
  const GURL kHttpUrl = GURL("http://a.test/");
  const url::Origin kHttpOrigin = url::Origin::Create(kHttpUrl);

  NavigateAndCommit(kUrlA);
  JoinInterestGroupAndFlush(CreateInterestGroup());
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  // Navigate to an HTTP origin and try to leave a group with an HTTPS owner.
  // The request should be rejected.
  NavigateAndCommit(kHttpUrl);
  LeaveInterestGroupAndExpectPipeClosed(kOriginA, kInterestGroupName);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));
}

TEST_F(AdAuctionServiceImplTest, FixExpiryOnJoin) {
  const base::TimeDelta kMaxExpiry = base::Days(30);
  blink::InterestGroup interest_group = CreateInterestGroup();

  // Join an interest group with an expiry that's exactly the maximum allowed.
  // The expiry should be stored without modification.
  interest_group.expiry = base::Time::Now() + kMaxExpiry;
  JoinInterestGroupAndFlush(interest_group);
  auto storage_interest_group = GetInterestGroup(kOriginA, kInterestGroupName);
  ASSERT_TRUE(storage_interest_group);
  EXPECT_EQ(1, storage_interest_group->bidding_browser_signals->join_count);
  EXPECT_EQ(interest_group.expiry,
            storage_interest_group->interest_group.expiry);

  // Rejoin the interest group with a short expiry. The expiry should also be
  // stored without modification.
  interest_group.expiry = base::Time::Now() + base::Days(1);
  JoinInterestGroupAndFlush(interest_group);
  storage_interest_group = GetInterestGroup(kOriginA, kInterestGroupName);
  ASSERT_TRUE(storage_interest_group);
  EXPECT_EQ(2, storage_interest_group->bidding_browser_signals->join_count);
  EXPECT_EQ(interest_group.expiry,
            storage_interest_group->interest_group.expiry);

  // Rejoin the interest group with an expiry that exceeds the maximum allowed.
  // The expiry should be set to kMaxExpiry days from now.
  interest_group.expiry = base::Time::Now() + base::Days(300);
  JoinInterestGroupAndFlush(interest_group);
  storage_interest_group = GetInterestGroup(kOriginA, kInterestGroupName);
  ASSERT_TRUE(storage_interest_group);
  EXPECT_EQ(3, storage_interest_group->bidding_browser_signals->join_count);
  base::Time actual_expiry = storage_interest_group->interest_group.expiry;
  EXPECT_EQ(base::Time::Now() + kMaxExpiry, actual_expiry);
  EXPECT_NE(interest_group.expiry, actual_expiry);
}

// These tests validate the `updateUrl` and navigator.updateAdInterestGroups()
// functionality.

// The server JSON updates all fields that can be updated.
TEST_F(AdAuctionServiceImplTest, UpdateAllUpdatableFields) {
  // TODO(caraitto): Remove camelCase sellerCapabilities fields when no longer
  // supported.
  network_responder_->RegisterUpdateResponse(
      kUpdateUrlPath,
      base::StringPrintf(R"({
"priority": 1.59,
"enableBiddingSignalsPrioritization": true,
"priorityVector": {"old1": 2, "new1": 1.1},
"prioritySignalsOverrides": {"old2": 1, "new1": 1.1,
                             "browserSignals.reserved":-1},
"sellerCapabilities": {"%s": ["latency-stats"],
                       "*": ["interest-group-counts", "latencyStats"]},
"biddingLogicUrl": "%s/interest_group/new_bidding_logic.js",
"biddingWasmHelperUrl":"%s/interest_group/new_bidding_wasm_helper_url.wasm",
"trustedBiddingSignalsUrl":
  "%s/interest_group/new_trusted_bidding_signals_url.json",
"trustedBiddingSignalsKeys": ["new_key"],
"ads": [{"renderURL": "%s/new_ad_render_url",
         "sizeGroup": "group_new",
         "metadata": {"new_a": "b"},
         "buyerReportingId": "new_brid",
         "buyerAndSellerReportingId": "new_shrid",
         "adRenderId": "123abc"
        }],
"adComponents": [{"renderURL": "https://example.com/component_url",
                  "sizeGroup": "group_new",
                  "metadata": {"new_c": "d"},
                  "adRenderId": "456def"
                 }],
"adSizes": {"size_new": {"width": "300px", "height": "150px"}},
"sizeGroups": {"group_new": ["size_new"]}
})",
                         kOriginStringA, kOriginStringA, kOriginStringA,
                         kOriginStringA, kOriginStringA));

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.priority = 2.0;
  interest_group.enable_bidding_signals_prioritization = false;
  interest_group.priority_vector = {{{"old1", 1}, {"old2", 2}}};
  interest_group.priority_signals_overrides = {{{"old1", 1}, {"old2", 2}}};
  interest_group.seller_capabilities.emplace();
  interest_group.seller_capabilities->insert(std::make_pair(
      kOriginA, blink::SellerCapabilitiesType(
                    {blink::SellerCapabilities::kInterestGroupCounts})));
  interest_group.all_sellers_capabilities = {
      blink::SellerCapabilities::kLatencyStats};
  interest_group.update_url = kUpdateUrlA;
  interest_group.bidding_url = kBiddingLogicUrlA;
  interest_group.trusted_bidding_signals_url = kTrustedBiddingSignalsUrlA;
  interest_group.trusted_bidding_signals_keys.emplace();
  interest_group.trusted_bidding_signals_keys->push_back("key1");
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/"{\"ad\":\"metadata\",\"here\":[1,2,3]}",
      /*size_group=*/"group_old",
      /*buyer_reporting_id=*/"old_brid",
      /*buyer_and_seller_reporting_id=*/"old_shrid",
      /*ad_render_id=*/"123abc");
  interest_group.ads->emplace_back(std::move(ad));
  interest_group.ad_components.emplace();
  blink::InterestGroup::Ad ad_component(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/"{\"ad\":\"metadata\",\"here\":[1,2,3]}",
      /*size_group=*/"group_old",
      /*ad_render_id=*/"456def");
  interest_group.ad_components->emplace_back(std::move(ad_component));
  interest_group.ad_sizes.emplace();
  interest_group.ad_sizes->emplace(
      "size_old", blink::AdSize(640, blink::AdSize::LengthUnit::kPixels, 480,
                                blink::AdSize::LengthUnit::kPixels));
  interest_group.size_groups.emplace();
  std::vector<std::string> size_list = {"size_old"};
  interest_group.size_groups->emplace("group_old", size_list);
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  std::vector<StorageInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);
  const auto& group = groups[0].interest_group;
  EXPECT_EQ(group.name, kInterestGroupName);
  EXPECT_EQ(group.priority, 1.59);
  EXPECT_EQ(group.enable_bidding_signals_prioritization, true);

  // The new value for `priority_vector` should completely replace the old one.
  base::flat_map<std::string, double> expected_priority_vector{{"old1", 2},
                                                               {"new1", 1.1}};
  EXPECT_EQ(group.priority_vector, expected_priority_vector);

  // The new value for `priority_signals_overrides` should be merged with the
  // old one. Interest groups can use the "browserSignals." prefix, though it's
  // not allowed in auctionConfig.prioritySignals fields.
  base::flat_map<std::string, double> expected_priority_signals_overrides{
      {"old1", 1}, {"old2", 1}, {"new1", 1.1}, {"browserSignals.reserved", -1}};
  EXPECT_EQ(group.priority_signals_overrides,
            expected_priority_signals_overrides);

  EXPECT_EQ(group.all_sellers_capabilities,
            blink::SellerCapabilitiesType(
                {blink::SellerCapabilities::kInterestGroupCounts,
                 blink::SellerCapabilities::kLatencyStats}));
  ASSERT_TRUE(group.seller_capabilities);
  ASSERT_EQ(group.seller_capabilities->size(), 1u);
  EXPECT_EQ(group.seller_capabilities->at(kOriginA),
            blink::SellerCapabilitiesType(
                {blink::SellerCapabilities::kLatencyStats}));
  ASSERT_TRUE(group.bidding_url.has_value());
  EXPECT_EQ(group.bidding_url->spec(),
            base::StringPrintf("%s/interest_group/new_bidding_logic.js",
                               kOriginStringA));
  ASSERT_TRUE(group.bidding_wasm_helper_url.has_value());
  EXPECT_EQ(
      group.bidding_wasm_helper_url->spec(),
      base::StringPrintf("%s/interest_group/new_bidding_wasm_helper_url.wasm",
                         kOriginStringA));
  ASSERT_TRUE(group.trusted_bidding_signals_url.has_value());
  EXPECT_EQ(group.trusted_bidding_signals_url->spec(),
            base::StringPrintf(
                "%s/interest_group/new_trusted_bidding_signals_url.json",
                kOriginStringA));
  ASSERT_TRUE(group.trusted_bidding_signals_keys.has_value());
  EXPECT_EQ(group.trusted_bidding_signals_keys->size(), 1u);
  EXPECT_EQ(group.trusted_bidding_signals_keys.value()[0], "new_key");
  ASSERT_TRUE(group.ads.has_value());
  ASSERT_EQ(group.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            base::StringPrintf("%s/new_ad_render_url", kOriginStringA));
  EXPECT_EQ(group.ads.value()[0].size_group, "group_new");
  EXPECT_EQ(group.ads.value()[0].metadata, "{\"new_a\":\"b\"}");
  ASSERT_TRUE(group.ads.value()[0].buyer_reporting_id.has_value());
  EXPECT_EQ(*group.ads.value()[0].buyer_reporting_id, "new_brid");
  ASSERT_TRUE(group.ads.value()[0].buyer_and_seller_reporting_id.has_value());
  EXPECT_EQ(*group.ads.value()[0].buyer_and_seller_reporting_id, "new_shrid");
  ASSERT_TRUE(group.ad_components.has_value());
  ASSERT_EQ(group.ad_components->size(), 1u);
  EXPECT_EQ(group.ad_components.value()[0].render_url.spec(),
            "https://example.com/component_url");
  EXPECT_EQ(group.ad_components.value()[0].size_group, "group_new");
  EXPECT_EQ(group.ad_components.value()[0].metadata, "{\"new_c\":\"d\"}");
  ASSERT_TRUE(group.ad_sizes.has_value());
  ASSERT_EQ(group.ad_sizes->size(), 1u);
  EXPECT_EQ(group.ad_sizes->at("size_new"),
            blink::AdSize(300, blink::AdSize::LengthUnit::kPixels, 150,
                          blink::AdSize::LengthUnit::kPixels));
  ASSERT_TRUE(group.size_groups.has_value());
  ASSERT_EQ(group.size_groups->size(), 1u);
  EXPECT_EQ(group.size_groups->at("group_new")[0], "size_new");
}

// Only set the ads field -- the other fields shouldn't be changed.
TEST_F(AdAuctionServiceImplTest, UpdatePartialPerformsMerge) {
  network_responder_->RegisterUpdateResponse(
      kUpdateUrlPath, base::StringPrintf(R"({
"ads": [{"renderURL": "%s/new_ad_render_url",
         "metadata": {"new_a": "b"}
        }]
})",
                                         kOriginStringA));

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.priority = 2.0;
  interest_group.update_url = kUpdateUrlA;
  interest_group.bidding_url = kBiddingLogicUrlA;
  interest_group.trusted_bidding_signals_url = kTrustedBiddingSignalsUrlA;
  interest_group.trusted_bidding_signals_keys.emplace();
  interest_group.trusted_bidding_signals_keys->push_back("key1");
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/"{\"ad\":\"metadata\",\"here\":[1,2,3]}");
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  std::vector<StorageInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);
  const auto& group = groups[0].interest_group;
  EXPECT_EQ(group.name, kInterestGroupName);
  EXPECT_EQ(group.priority, 2.0);
  ASSERT_TRUE(group.bidding_url.has_value());
  EXPECT_EQ(
      group.bidding_url->spec(),
      base::StringPrintf("%s/interest_group/bidding_logic.js", kOriginStringA));
  ASSERT_TRUE(group.update_url.has_value());
  EXPECT_EQ(group.update_url->spec(),
            base::StringPrintf("%s/interest_group/daily_update_partial.json",
                               kOriginStringA));
  ASSERT_TRUE(group.trusted_bidding_signals_url.has_value());
  EXPECT_EQ(group.trusted_bidding_signals_url->spec(),
            base::StringPrintf("%s/interest_group/trusted_bidding_signals.json",
                               kOriginStringA));
  ASSERT_TRUE(group.trusted_bidding_signals_keys.has_value());
  EXPECT_EQ(group.trusted_bidding_signals_keys->size(), 1u);
  EXPECT_EQ(group.trusted_bidding_signals_keys.value()[0], "key1");
  ASSERT_TRUE(group.ads.has_value());
  ASSERT_EQ(group.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            base::StringPrintf("%s/new_ad_render_url", kOriginStringA));
  EXPECT_EQ(group.ads.value()[0].metadata, "{\"new_a\":\"b\"}");
}

// The update shouldn't change the expiration time of the interest group.
TEST_F(AdAuctionServiceImplTest, UpdateDoesntChangeExpiration) {
  network_responder_->RegisterUpdateResponse(kUpdateUrlPath, R"({
"ads": [{"renderURL": "https://example.com/new_render"
        }]
})");

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.update_url = kUpdateUrlA;
  interest_group.bidding_url = kBiddingLogicUrlA;
  interest_group.trusted_bidding_signals_url = kTrustedBiddingSignalsUrlA;
  interest_group.trusted_bidding_signals_keys.emplace();
  interest_group.trusted_bidding_signals_keys->push_back("key1");
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  // Lookup expiry from the database before updating.
  const auto groups_before_update = GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups_before_update.size(), 1u);
  const base::Time kExpirationTime =
      groups_before_update[0].interest_group.expiry;

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // The expiration time shouldn't change.
  std::vector<StorageInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);
  const auto& group = groups[0].interest_group;
  EXPECT_EQ(group.name, kInterestGroupName);
  EXPECT_EQ(group.expiry, kExpirationTime);
  ASSERT_TRUE(group.ads.has_value());
  ASSERT_EQ(group.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            "https://example.com/new_render");
}

// Updates should succeed even when updating interest groups with no ads.
TEST_F(AdAuctionServiceImplTest, UpdateGroupWithNoAds) {
  network_responder_->RegisterUpdateResponse(
      kUpdateUrlPath, base::StringPrintf(R"({
"trustedBiddingSignalsUrl":
  "%s/interest_group/new_trusted_bidding_signals_url.json",
"trustedBiddingSignalsKeys": ["new_key"]
})",
                                         kOriginStringA));

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.update_url = kUpdateUrlA;
  interest_group.bidding_url = kBiddingLogicUrlA;
  interest_group.trusted_bidding_signals_url = kTrustedBiddingSignalsUrlA;
  interest_group.trusted_bidding_signals_keys.emplace();
  interest_group.trusted_bidding_signals_keys->push_back("key1");
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  std::vector<StorageInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);
  const auto& group = groups[0].interest_group;
  EXPECT_EQ(group.name, kInterestGroupName);
  ASSERT_TRUE(group.bidding_url.has_value());
  EXPECT_EQ(
      group.bidding_url->spec(),
      base::StringPrintf("%s/interest_group/bidding_logic.js", kOriginStringA));
  ASSERT_TRUE(group.update_url.has_value());
  EXPECT_EQ(group.update_url->spec(),
            base::StringPrintf("%s/interest_group/daily_update_partial.json",
                               kOriginStringA));
  ASSERT_TRUE(group.trusted_bidding_signals_url.has_value());
  EXPECT_EQ(group.trusted_bidding_signals_url->spec(),
            base::StringPrintf(
                "%s/interest_group/new_trusted_bidding_signals_url.json",
                kOriginStringA));
  ASSERT_TRUE(group.trusted_bidding_signals_keys.has_value());
  EXPECT_EQ(group.trusted_bidding_signals_keys->size(), 1u);
  EXPECT_EQ(group.trusted_bidding_signals_keys.value()[0], "new_key");
  EXPECT_FALSE(group.ads.has_value());
}

// Only set the ads field -- the other fields shouldn't be changed.
TEST_F(AdAuctionServiceImplTest, UpdateSucceedsIfOptionalNameOwnerMatch) {
  network_responder_->RegisterUpdateResponse(
      kUpdateUrlPath,
      base::StringPrintf(R"({
"name": "%s",
"owner": "%s",
"ads": [{"renderURL": "%s/new_ad_render_url"
        }]
})",
                         kInterestGroupName, kOriginStringA, kOriginStringA));

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.update_url = kUpdateUrlA;
  interest_group.bidding_url = kBiddingLogicUrlA;
  interest_group.trusted_bidding_signals_url = kTrustedBiddingSignalsUrlA;
  interest_group.trusted_bidding_signals_keys.emplace();
  interest_group.trusted_bidding_signals_keys->push_back("key1");
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  std::vector<StorageInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);
  const auto& group = groups[0].interest_group;
  EXPECT_EQ(group.name, kInterestGroupName);
  ASSERT_TRUE(group.bidding_url.has_value());
  EXPECT_EQ(
      group.bidding_url->spec(),
      base::StringPrintf("%s/interest_group/bidding_logic.js", kOriginStringA));
  ASSERT_TRUE(group.update_url.has_value());
  EXPECT_EQ(group.update_url->spec(),
            base::StringPrintf("%s/interest_group/daily_update_partial.json",
                               kOriginStringA));
  ASSERT_TRUE(group.trusted_bidding_signals_url.has_value());
  EXPECT_EQ(group.trusted_bidding_signals_url->spec(),
            base::StringPrintf("%s/interest_group/trusted_bidding_signals.json",
                               kOriginStringA));
  ASSERT_TRUE(group.trusted_bidding_signals_keys.has_value());
  EXPECT_EQ(group.trusted_bidding_signals_keys->size(), 1u);
  EXPECT_EQ(group.trusted_bidding_signals_keys.value()[0], "key1");
  ASSERT_TRUE(group.ads.has_value());
  ASSERT_EQ(group.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            base::StringPrintf("%s/new_ad_render_url", kOriginStringA));
}

// For forward compatibility we should silently ignore fields that we don't
// know about.
TEST_F(AdAuctionServiceImplTest, UpdateIgnoresUnknownFields) {
  network_responder_->RegisterUpdateResponse(kUpdateUrlPath, R"({
"unsupportedField": "InInterestGroup",
"ads": [{
  "renderURL": "https://example.com/new_render",
  "unsupportedField": "InAd"
        }],
"adComponents": [{
  "renderURL": "https://example.com/new_component",
  "unsupportedField": "InAdComponent"
        }]
})");

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.update_url = kUpdateUrlA;
  interest_group.bidding_url = kBiddingLogicUrlA;
  interest_group.trusted_bidding_signals_url = kTrustedBiddingSignalsUrlA;
  interest_group.trusted_bidding_signals_keys.emplace();
  interest_group.trusted_bidding_signals_keys->push_back("key1");
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // Check that the ad changed.
  std::vector<StorageInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);
  const auto& group = groups[0].interest_group;
  ASSERT_TRUE(group.ads.has_value());
  ASSERT_EQ(group.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            "https://example.com/new_render");
  ASSERT_EQ(group.ad_components->size(), 1u);
  EXPECT_EQ(group.ad_components.value()[0].render_url.spec(),
            "https://example.com/new_component");
}

// Try to set the name -- for security, name and owner shouldn't be
// allowed to change. If they don't match the interest group (update URLs are
// registered per interest group), fail the update and don't update anything.
TEST_F(AdAuctionServiceImplTest, NoUpdateIfOptionalNameDoesntMatch) {
  network_responder_->RegisterUpdateResponse(kUpdateUrlPath, R"({
"name": "boats",
"ads": [{"renderURL": "https://example.com/new_render"
        }]
})");

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.update_url = kUpdateUrlA;
  interest_group.bidding_url = kBiddingLogicUrlA;
  interest_group.trusted_bidding_signals_url = kTrustedBiddingSignalsUrlA;
  interest_group.trusted_bidding_signals_keys.emplace();
  interest_group.trusted_bidding_signals_keys->push_back("key1");
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // Check that the ads didn't change.
  std::vector<StorageInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);
  const auto& group = groups[0].interest_group;
  ASSERT_TRUE(group.ads.has_value());
  ASSERT_EQ(group.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            "https://example.com/render");
}

// Try to set the owner -- for security, name and owner shouldn't be
// allowed to change. If they don't match the interest group (update URLs are
// registered per interest group), fail the update and don't update anything.
TEST_F(AdAuctionServiceImplTest, NoUpdateIfOptionalOwnerDoesntMatch) {
  network_responder_->RegisterUpdateResponse(
      kUpdateUrlPath, base::StringPrintf(R"({
"owner": "%s",
"ads": [{"renderURL": "%s/new_ad_render_url"
        }]
})",
                                         kOriginStringB, kOriginStringA));

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.update_url = kUpdateUrlA;
  interest_group.bidding_url = kBiddingLogicUrlA;
  interest_group.trusted_bidding_signals_url = kTrustedBiddingSignalsUrlA;
  interest_group.trusted_bidding_signals_keys.emplace();
  interest_group.trusted_bidding_signals_keys->push_back("key1");
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // Check that the ads didn't change.
  std::vector<StorageInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);
  const auto& group = groups[0].interest_group;
  ASSERT_TRUE(group.ads.has_value());
  ASSERT_EQ(group.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            "https://example.com/render");
}

TEST_F(AdAuctionServiceImplTest, UpdatePriorityVector) {
  // These are all set in sequence, on top of each other, so if one update
  // should fail to parse, the previous value should be unmodified.
  const struct {
    const char* priority_vector_value;
    base::flat_map<std::string, double> expected_priority_vector;
  } kTestCases[] = {
      // Set one value.
      {R"({"key1":1})", {{"key1", 1}}},
      // Overwrite it.
      {R"({"key1":2})", {{"key1", 2}}},

      // Trying to set a value that's not a double should fail.
      {R"({"key1":null})", {{"key1", 2}}},
      {R"({"key1":"42"})", {{"key1", 2}}},
      {R"({"key1":[42]})", {{"key1", 2}}},

      // Setting the entire vector to something that isn't a dict should fail.
      {R"(null)", {{"key1", 2}}},
      {R"([])", {{"key1", 2}}},
      {R"(5)", {{"key1", 2}}},

      // Old values should not be preserved when setting new values, even when
      // not explicitly overwriting the old key.
      {R"({"key2":-2,"key3":0})", {{"key2", -2}, {"key3", 0}}},

      // Empty value is valid.
      {R"({})", {}},
  };

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.update_url = kUpdateUrlA;
  interest_group.expiry = base::Time::Now() + base::Days(30);
  JoinInterestGroupAndFlush(interest_group);

  std::vector<StorageInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);
  EXPECT_EQ(groups[0].interest_group.priority_vector, absl::nullopt);

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.priority_vector_value);

    // Pass enough time so that update rate limits don't cause an update to
    // fail.
    task_environment()->FastForwardBy(
        InterestGroupStorage::kUpdateSucceededBackoffPeriod);

    // Set new update response, and update.
    network_responder_->RegisterUpdateResponse(
        kUpdateUrlPath, base::StringPrintf(R"({"priorityVector": %s})",
                                           test_case.priority_vector_value));
    UpdateInterestGroupNoFlush();
    task_environment()->RunUntilIdle();

    groups = GetInterestGroupsForOwner(kOriginA);
    ASSERT_EQ(groups.size(), 1u);
    EXPECT_EQ(groups[0].interest_group.priority_vector,
              test_case.expected_priority_vector);
  }
}

TEST_F(AdAuctionServiceImplTest, UpdatePrioritySignalsOverrides) {
  // These are all set in sequence, on top of each other, so if one update
  // should fail to parse, the previous value should be unmodified.
  const struct {
    const char* priority_signals_overrides_value;
    base::flat_map<std::string, double> expected_priority_signals_overrides;
  } kTestCases[] = {
      // Set one value.
      {R"({"key1":1})", {{"key1", 1}}},
      // Overwrite it.
      {R"({"key1":2})", {{"key1", 2}}},

      // Trying to set a value that's not a double or null should fail.
      {R"({"key1":"42"})", {{"key1", 2}}},
      {R"({"key1":[42]})", {{"key1", 2}}},

      // Setting the entire vector to something that isn't a dict should fail.
      {R"(null)", {{"key1", 2}}},
      {R"([])", {{"key1", 2}}},
      {R"(5)", {{"key1", 2}}},

      // New values should be merged with old values.
      {R"({"key2":-2,"key3":0})", {{"key1", 2}, {"key2", -2}, {"key3", 0}}},

      // Setting a value to null should delete it.
      {R"({"key2":null})", {{"key1", 2}, {"key3", 0}}},

      // Empty value is valid, but has no effect.
      {R"({})", {{"key1", 2}, {"key3", 0}}},
  };

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.update_url = kUpdateUrlA;
  interest_group.expiry = base::Time::Now() + base::Days(30);
  JoinInterestGroupAndFlush(interest_group);

  std::vector<StorageInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);
  EXPECT_EQ(groups[0].interest_group.priority_signals_overrides, absl::nullopt);

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.priority_signals_overrides_value);

    // Pass enough time so that update rate limits don't cause an update to
    // fail.
    task_environment()->FastForwardBy(
        InterestGroupStorage::kUpdateSucceededBackoffPeriod);

    // Set new update response, and update.
    network_responder_->RegisterUpdateResponse(
        kUpdateUrlPath,
        base::StringPrintf(R"({"prioritySignalsOverrides": %s})",
                           test_case.priority_signals_overrides_value));
    UpdateInterestGroupNoFlush();
    task_environment()->RunUntilIdle();

    groups = GetInterestGroupsForOwner(kOriginA);
    ASSERT_EQ(groups.size(), 1u);
    EXPECT_EQ(groups[0].interest_group.priority_signals_overrides,
              test_case.expected_priority_signals_overrides);
  }
}

// Join 2 interest groups, each with the same owner, but with different update
// URLs. Both interest groups should be updated correctly.
TEST_F(AdAuctionServiceImplTest, UpdateMultipleInterestGroups) {
  constexpr char kGroupName1[] = "group1";
  constexpr char kGroupName2[] = "group2";
  network_responder_->RegisterUpdateResponse(kUpdateUrlPath, R"({
"ads": [{"renderURL": "https://example.com/new_render1"}]
})");
  network_responder_->RegisterUpdateResponse(kUpdateUrlPath2, R"({
"ads": [{"renderURL": "https://example.com/new_render2"}]
})");

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.name = kGroupName1;
  interest_group.update_url = kUrlA.Resolve(kUpdateUrlPath);
  interest_group.bidding_url = kBiddingLogicUrlA;
  interest_group.trusted_bidding_signals_url = kTrustedBiddingSignalsUrlA;
  interest_group.trusted_bidding_signals_keys.emplace();
  interest_group.trusted_bidding_signals_keys->push_back("key1");
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kGroupName1));

  // Now, join the second interest group, also belonging to `kOriginA`.
  blink::InterestGroup interest_group_2 = CreateInterestGroup();
  interest_group_2.name = kGroupName2;
  interest_group_2.update_url = kUrlA.Resolve(kUpdateUrlPath2);
  interest_group_2.bidding_url = kBiddingLogicUrlA;
  interest_group_2.trusted_bidding_signals_url = kTrustedBiddingSignalsUrlA;
  interest_group_2.trusted_bidding_signals_keys.emplace();
  interest_group_2.trusted_bidding_signals_keys->push_back("key1");
  interest_group_2.ads.emplace();
  ad = blink::InterestGroup::Ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group_2.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group_2);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kGroupName2));

  // Now, run the update. Both interest groups should update.
  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // Both interest groups should update.
  std::vector<StorageInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 2u);
  const auto& first_group = groups[0].interest_group.name == kGroupName1
                                ? groups[0].interest_group
                                : groups[1].interest_group;
  const auto& second_group = groups[0].interest_group.name == kGroupName2
                                 ? groups[0].interest_group
                                 : groups[1].interest_group;

  EXPECT_EQ(first_group.name, kGroupName1);
  ASSERT_TRUE(first_group.ads.has_value());
  ASSERT_EQ(first_group.ads->size(), 1u);
  EXPECT_EQ(first_group.ads.value()[0].render_url.spec(),
            "https://example.com/new_render1");

  EXPECT_EQ(second_group.name, kGroupName2);
  ASSERT_TRUE(second_group.ads.has_value());
  ASSERT_EQ(second_group.ads->size(), 1u);
  EXPECT_EQ(second_group.ads.value()[0].render_url.spec(),
            "https://example.com/new_render2");
}

// Join 2 interest groups, each with a different owner. When updating interest
// groups, only the 1 interest group owned by the origin of the frame that
// called navigator.updateAdInterestGroups() gets updated.
TEST_F(AdAuctionServiceImplTest, UpdateOnlyOwnOrigin) {
  // Both interest groups can share the same update logic and path (they just
  // use different origins).
  network_responder_->RegisterUpdateResponse(kUpdateUrlPath, R"({
"ads": [{"renderURL": "https://example.com/new_render"
        }]
})");

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.update_url = kUpdateUrlA;
  interest_group.bidding_url = kBiddingLogicUrlA;
  interest_group.trusted_bidding_signals_url = kTrustedBiddingSignalsUrlA;
  interest_group.trusted_bidding_signals_keys.emplace();
  interest_group.trusted_bidding_signals_keys->push_back("key1");
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  // Now, join the second interest group, belonging to `kOriginB`.
  NavigateAndCommit(kUrlB);
  blink::InterestGroup interest_group_b = CreateInterestGroup();
  interest_group_b.owner = kOriginB;
  interest_group_b.update_url = kUrlB.Resolve(kUpdateUrlPath);
  interest_group_b.bidding_url = kUrlB.Resolve(kBiddingUrlPath);
  interest_group_b.trusted_bidding_signals_url =
      kUrlB.Resolve(kTrustedBiddingSignalsUrlPath);
  interest_group_b.trusted_bidding_signals_keys.emplace();
  interest_group_b.trusted_bidding_signals_keys->push_back("key1");
  interest_group_b.ads.emplace();
  ad = blink::InterestGroup::Ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group_b.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group_b);
  EXPECT_EQ(1, GetJoinCount(kOriginB, kInterestGroupName));

  // Now, run the update. Only the `kOriginB` group should get updated.
  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // The `kOriginB` interest group should update...
  std::vector<StorageInterestGroup> origin_b_groups =
      GetInterestGroupsForOwner(kOriginB);
  ASSERT_EQ(origin_b_groups.size(), 1u);
  const auto& origin_b_group = origin_b_groups[0].interest_group;
  EXPECT_EQ(origin_b_group.name, kInterestGroupName);
  ASSERT_TRUE(origin_b_group.ads.has_value());
  ASSERT_EQ(origin_b_group.ads->size(), 1u);
  EXPECT_EQ(origin_b_group.ads.value()[0].render_url.spec(),
            "https://example.com/new_render");

  // ...but the `kOriginA` interest group shouldn't change.
  std::vector<StorageInterestGroup> origin_a_groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(origin_a_groups.size(), 1u);
  const auto& origin_a_group = origin_a_groups[0].interest_group;
  ASSERT_TRUE(origin_a_group.ads.has_value());
  ASSERT_EQ(origin_a_group.ads->size(), 1u);
  EXPECT_EQ(origin_a_group.ads.value()[0].render_url.spec(),
            "https://example.com/render");
}

// Test updating an interest group with a cross-site owner.
TEST_F(AdAuctionServiceImplTest, UpdateFromCrossSiteIFrame) {
  // All interest groups can share the same update logic and path (they just
  // use different origins).
  network_responder_->RegisterUpdateResponse(kUpdateUrlPath, R"({
"ads": [{"renderURL": "https://example.com/new_render"
        }]
})");

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.update_url = kUpdateUrlA;
  interest_group.bidding_url = kBiddingLogicUrlA;
  interest_group.trusted_bidding_signals_url = kTrustedBiddingSignalsUrlA;
  interest_group.trusted_bidding_signals_keys.emplace();
  interest_group.trusted_bidding_signals_keys->push_back("key1");
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  // Now, join the second interest group, belonging to `kOriginB`.
  NavigateAndCommit(kUrlB);
  blink::InterestGroup interest_group_b = CreateInterestGroup();
  interest_group_b.owner = kOriginB;
  interest_group_b.update_url = kUrlB.Resolve(kUpdateUrlPath);
  interest_group_b.bidding_url = kUrlB.Resolve(kBiddingUrlPath);
  interest_group_b.trusted_bidding_signals_url =
      kUrlB.Resolve(kTrustedBiddingSignalsUrlPath);
  interest_group_b.trusted_bidding_signals_keys.emplace();
  interest_group_b.trusted_bidding_signals_keys->push_back("key1");
  interest_group_b.ads.emplace();
  ad = blink::InterestGroup::Ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group_b.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group_b);
  EXPECT_EQ(1, GetJoinCount(kOriginB, kInterestGroupName));

  // Now, join the third interest group, belonging to `kOriginC`.
  NavigateAndCommit(kUrlC);
  blink::InterestGroup interest_group_c = CreateInterestGroup();
  interest_group_c.owner = kOriginC;
  interest_group_c.update_url = kUrlC.Resolve(kUpdateUrlPath);
  interest_group_c.bidding_url = kUrlC.Resolve(kBiddingUrlPath);
  interest_group_c.trusted_bidding_signals_url =
      kUrlC.Resolve(kTrustedBiddingSignalsUrlPath);
  interest_group_c.trusted_bidding_signals_keys.emplace();
  interest_group_c.trusted_bidding_signals_keys->push_back("key1");
  interest_group_c.ads.emplace();
  ad = blink::InterestGroup::Ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group_c.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group_c);
  EXPECT_EQ(1, GetJoinCount(kOriginC, kInterestGroupName));

  NavigateAndCommit(kUrlA);

  // Create a subframe and use it to send the join request.
  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  content::RenderFrameHost* subframe = rfh_tester->AppendChild("subframe");
  subframe =
      NavigationSimulator::NavigateAndCommitFromDocument(kUrlC, subframe);

  UpdateInterestGroupNoFlushForFrame(subframe);
  task_environment()->RunUntilIdle();

  // Subframes from origin C with a top frame of A should update groups
  // with C as the owner, but the subframe from C should not be able to update
  // groups for A.
  // The `kOriginC` interest group should update...
  std::vector<StorageInterestGroup> origin_c_groups =
      GetInterestGroupsForOwner(kOriginC);
  ASSERT_EQ(origin_c_groups.size(), 1u);
  const auto& origin_c_group = origin_c_groups[0].interest_group;
  EXPECT_EQ(origin_c_group.name, kInterestGroupName);
  ASSERT_TRUE(origin_c_group.ads.has_value());
  ASSERT_EQ(origin_c_group.ads->size(), 1u);
  EXPECT_EQ(origin_c_group.ads.value()[0].render_url.spec(),
            "https://example.com/new_render");

  // ...but the `kOriginA` interest group shouldn't change.
  std::vector<StorageInterestGroup> origin_a_groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(origin_a_groups.size(), 1u);
  const auto& origin_a_group = origin_a_groups[0].interest_group;
  ASSERT_TRUE(origin_a_group.ads.has_value());
  ASSERT_EQ(origin_a_group.ads->size(), 1u);
  EXPECT_EQ(origin_a_group.ads.value()[0].render_url.spec(),
            "https://example.com/render");

  // Now try on disallowed subframe from originB.
  subframe =
      NavigationSimulator::NavigateAndCommitFromDocument(kUrlB, subframe);
  interest_group = CreateInterestGroup();
  interest_group.owner = kOriginB;
  UpdateInterestGroupNoFlushForFrame(subframe);
  task_environment()->RunUntilIdle();

  // Subframes from origin B with a top frame of A should not (by policy) be
  // allowed to update groups with B as the owner.
  std::vector<StorageInterestGroup> origin_b_groups =
      GetInterestGroupsForOwner(kOriginB);
  ASSERT_EQ(origin_b_groups.size(), 1u);
  const auto& origin_b_group = origin_b_groups[0].interest_group;
  ASSERT_TRUE(origin_b_group.ads.has_value());
  ASSERT_EQ(origin_b_group.ads->size(), 1u);
  EXPECT_EQ(origin_b_group.ads.value()[0].render_url.spec(),
            "https://example.com/render");
}

// The `ads` field is valid, but the ad `renderURL` field is an invalid
// URL. The entire update should get cancelled, since updates are atomic.
TEST_F(AdAuctionServiceImplTest, UpdateInvalidFieldCancelsAllUpdates) {
  network_responder_->RegisterUpdateResponse(
      kUpdateUrlPath, base::StringPrintf(R"({
"biddingLogicUrl": "%s/interest_group/new_bidding_logic.js",
"ads": [{"renderURL": "https://invalid^&",
         "metadata": {"new_a": "b"}
        }]
})",
                                         kOriginStringA));

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.update_url = kUpdateUrlA;
  interest_group.bidding_url = kBiddingLogicUrlA;
  interest_group.trusted_bidding_signals_url = kTrustedBiddingSignalsUrlA;
  interest_group.trusted_bidding_signals_keys.emplace();
  interest_group.trusted_bidding_signals_keys->push_back("key1");
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/"{\"ad\":\"metadata\",\"here\":[1,2,3]}");
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // Check that the ads and bidding logic URL didn't change.
  std::vector<StorageInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);
  const auto& group = groups[0].interest_group;
  ASSERT_TRUE(group.ads.has_value());
  ASSERT_EQ(group.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            "https://example.com/render");
  EXPECT_EQ(group.ads.value()[0].metadata,
            "{\"ad\":\"metadata\",\"here\":[1,2,3]}");
  EXPECT_EQ(group.bidding_url, kBiddingLogicUrlA);
}

// The `priority` field is not a valid number. The entire update should get
// cancelled, since updates are atomic.
TEST_F(AdAuctionServiceImplTest, UpdateInvalidPriorityCancelsAllUpdates) {
  network_responder_->RegisterUpdateResponse(
      kUpdateUrlPath, base::StringPrintf(R"({
"priority": "high",
"biddingLogicUrl": "%s/interest_group/new_bidding_logic.js"
})",
                                         kOriginStringA));

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.priority = 2.0;
  interest_group.update_url = kUpdateUrlA;
  interest_group.bidding_url = kBiddingLogicUrlA;
  interest_group.trusted_bidding_signals_url = kTrustedBiddingSignalsUrlA;
  interest_group.trusted_bidding_signals_keys.emplace();
  interest_group.trusted_bidding_signals_keys->push_back("key1");
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/"{\"ad\":\"metadata\",\"here\":[1,2,3]}");
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // Check that the priority and bidding logic URL didn't change.
  std::vector<StorageInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);
  const auto& group = groups[0].interest_group;
  EXPECT_EQ(group.priority, 2.0);
  EXPECT_EQ(group.bidding_url, kBiddingLogicUrlA);
}

// The `sellerCapabilities` field has an invalid capability. The invalid
// capability gets skipped, but the rest of the update proceeds.
TEST_F(AdAuctionServiceImplTest, UpdateInvalidSellerCapabilitiesIgnored) {
  // TODO(caraitto): Convert interestGroupCounts to interest-group-counts when
  // support for the camelCase version is dropped.
  network_responder_->RegisterUpdateResponse(
      kUpdateUrlPath, base::StringPrintf(R"({
"sellerCapabilities": {"%s": ["latency-stats"], "*": ["interestGroupCounts",
                                                     "invalid-capability"]}
})",
                                         kOriginStringA));

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.update_url = kUpdateUrlA;
  interest_group.bidding_url = kBiddingLogicUrlA;
  interest_group.trusted_bidding_signals_url = kTrustedBiddingSignalsUrlA;
  interest_group.trusted_bidding_signals_keys.emplace();
  interest_group.trusted_bidding_signals_keys->push_back("key1");
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // Check that the seller capabilities was updated, with the invalid capability
  // ignored.
  std::vector<StorageInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);
  const auto& group = groups[0].interest_group;
  EXPECT_EQ(group.all_sellers_capabilities,
            blink::SellerCapabilitiesType(
                {blink::SellerCapabilities::kInterestGroupCounts}));
  ASSERT_TRUE(group.seller_capabilities);
  ASSERT_EQ(group.seller_capabilities->size(), 1u);
  EXPECT_EQ(group.seller_capabilities->at(kOriginA),
            blink::SellerCapabilitiesType(
                {blink::SellerCapabilities::kLatencyStats}));
}

// The server response can't be parsed as valid JSON. The update is cancelled.
TEST_F(AdAuctionServiceImplTest, UpdateInvalidJSONIgnored) {
  network_responder_->RegisterUpdateResponse(kUpdateUrlPath,
                                             "This isn't JSON.");

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.update_url = kUpdateUrlA;
  interest_group.bidding_url = kBiddingLogicUrlA;
  interest_group.trusted_bidding_signals_url = kTrustedBiddingSignalsUrlA;
  interest_group.trusted_bidding_signals_keys.emplace();
  interest_group.trusted_bidding_signals_keys->push_back("key1");
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // Check that the ads didn't change.
  std::vector<StorageInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);
  const auto& group = groups[0].interest_group;
  ASSERT_TRUE(group.ads.has_value());
  ASSERT_EQ(group.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            "https://example.com/render");
}

// UpdateJSONParserCrash fails on Android because Android doesn't use a separate
// process to parse JSON -- instead, it validates JSON in-process in Java, then,
// if validation succeeded, uses the C++ JSON parser, also in-proc. On other
// platforms, the C++ parser runs out-of-proc for safety.
#if !BUILDFLAG(IS_ANDROID)

// The server response is valid, but we simulate the JSON parser (which may
// run in a separate process) crashing, so the update doesn't happen.
TEST_F(AdAuctionServiceImplTest, UpdateJSONParserCrash) {
  network_responder_->RegisterUpdateResponse(kUpdateUrlPath, R"({
"ads": [{"renderURL": "https://example.com/new_render"
        }]
})");

  blink::InterestGroup interest_group = CreateInterestGroup();
  // Set a long expiration delta so that we can advance to the next rate limit
  // period without the interest group expiring.
  interest_group.expiry = base::Time::Now() + base::Days(30);
  interest_group.update_url = kUpdateUrlA;
  interest_group.bidding_url = kBiddingLogicUrlA;
  interest_group.trusted_bidding_signals_url = kTrustedBiddingSignalsUrlA;
  interest_group.trusted_bidding_signals_keys.emplace();
  interest_group.trusted_bidding_signals_keys->push_back("key1");
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  // Simulate the JSON service crashing instead of returning a result.
  data_decoder::test::InProcessDataDecoder in_process_data_decoder;
  in_process_data_decoder.service().SimulateJsonParserCrashForTesting(
      /*drop=*/true);

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // Check that the ads didn't change.
  std::vector<StorageInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);
  auto group = groups[0].interest_group;
  ASSERT_TRUE(group.ads.has_value());
  ASSERT_EQ(group.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            "https://example.com/render");

  // Try another IG update, this time with no crash. It should succceed.
  // (We need to advance time since this next attempt is rate-limited).
  in_process_data_decoder.service().SimulateJsonParserCrashForTesting(
      /*drop=*/false);
  task_environment()->FastForwardBy(
      InterestGroupStorage::kUpdateSucceededBackoffPeriod);
  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // Check that the ads *did* change this time.
  groups = GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);
  group = groups[0].interest_group;
  ASSERT_TRUE(group.ads.has_value());
  ASSERT_EQ(group.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            "https://example.com/new_render");
}

#endif  // !BUILDFLAG(IS_ANDROID)

// Trigger an update, but block it via ContentBrowserClient policy.
// The update shouldn't happen.
TEST_F(AdAuctionServiceImplTest, UpdateBlockedByContentBrowserClient) {
  NavigateAndCommit(kUrlNoUpdate);
  network_responder_->RegisterUpdateResponse(kUpdateUrlPath, R"({
"ads": [{"renderURL": "https://example.com/new_render"
        }]
})");

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.owner = kOriginNoUpdate;
  interest_group.update_url = kUpdateUrlNoUpdate;
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginNoUpdate, kInterestGroupName));

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  std::vector<StorageInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginNoUpdate);
  ASSERT_EQ(groups.size(), 1u);
  const auto& group = groups[0].interest_group;
  ASSERT_TRUE(group.ads.has_value());
  ASSERT_EQ(group.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            "https://example.com/render");

  // There shouldn't have even been an attempt to update.
  EXPECT_EQ(network_responder_->UpdateCount(), 0u);
}

// The network request fails (not implemented), so the update is cancelled.
TEST_F(AdAuctionServiceImplTest, UpdateNetworkFailure) {
  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.update_url = kUrlA.Resolve("no_handler.json");
  interest_group.bidding_url = kBiddingLogicUrlA;
  interest_group.trusted_bidding_signals_url = kTrustedBiddingSignalsUrlA;
  interest_group.trusted_bidding_signals_keys.emplace();
  interest_group.trusted_bidding_signals_keys->push_back("key1");
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // Check that the ads didn't change.
  std::vector<StorageInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);
  const auto& group = groups[0].interest_group;
  ASSERT_TRUE(group.ads.has_value());
  ASSERT_EQ(group.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            "https://example.com/render");
}

// The network request for updating interest groups times out, so the update
// fails.
TEST_F(AdAuctionServiceImplTest, UpdateTimeout) {
  network_responder_->RegisterDeferredUpdateResponse(kUpdateUrlPath);
  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.update_url = kUpdateUrlA;
  interest_group.bidding_url = kBiddingLogicUrlA;
  interest_group.trusted_bidding_signals_url = kTrustedBiddingSignalsUrlA;
  interest_group.trusted_bidding_signals_keys.emplace();
  interest_group.trusted_bidding_signals_keys->push_back("key1");
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  UpdateInterestGroupNoFlush();
  task_environment()->FastForwardBy(base::Seconds(30) + base::Seconds(1));
  task_environment()->RunUntilIdle();

  // The request times out (ERR_TIMED_OUT), so the ads should not change.
  std::vector<StorageInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);
  const auto& group = groups[0].interest_group;
  ASSERT_TRUE(group.ads.has_value());
  ASSERT_EQ(group.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            "https://example.com/render");
}

// Start an update, and delay the server response so that the interest group
// expires before the interest group updates. Don't advance time enough for DB
// maintenance tasks to run -- that is the interest group will only exist on
// disk in an expired state, and not appear in queries.
TEST_F(AdAuctionServiceImplTest,
       UpdateDuringInterestGroupExpirationNoDbMaintenence) {
  constexpr char kServerResponse[] = R"({
"ads": [{"renderURL": "https://example.com/new_render"}]
})";
  network_responder_->RegisterDeferredUpdateResponse(kUpdateUrlPath);

  // Make the interest group expire before the DB maintenance task should be
  // run, with a gap second where expiration has happened, but DB maintenance
  // has not. Time order:
  // (*NOW*, group expiration, db maintenance).
  const base::TimeDelta kExpiryDelta =
      InterestGroupStorage::kIdlePeriod - base::Seconds(2);
  ASSERT_GT(kExpiryDelta, base::Seconds(0));
  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.expiry = base::Time::Now() + kExpiryDelta;
  interest_group.update_url = kUpdateUrlA;
  interest_group.bidding_url = kBiddingLogicUrlA;
  interest_group.trusted_bidding_signals_url = kTrustedBiddingSignalsUrlA;
  interest_group.trusted_bidding_signals_keys.emplace();
  interest_group.trusted_bidding_signals_keys->push_back("key1");
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  // Start an interest group update and then advance time to ensure the interest
  // group expires before a response is returned.
  UpdateInterestGroupNoFlush();
  task_environment()->FastForwardBy(kExpiryDelta + base::Seconds(1));
  task_environment()->RunUntilIdle();
  EXPECT_EQ(0, GetJoinCount(kOriginA, kInterestGroupName));
  EXPECT_EQ(0u, GetInterestGroupsForOwner(kOriginA).size());

  // Due to FastForwardBy(), we're at this time order:
  // (group expiration, *NOW*, db maintenance).
  // So, DB maintenance should not have been run.
  base::RunLoop run_loop;
  manager_->GetLastMaintenanceTimeForTesting(
      base::BindLambdaForTesting([&run_loop](base::Time time) {
        EXPECT_EQ(time, base::Time::Min());
        run_loop.Quit();
      }));
  run_loop.Run();

  // Now return the server response. The interest group shouldn't change as it's
  // expired.
  network_responder_->DoDeferredUpdateResponse(kUpdateUrlPath, kServerResponse);
  task_environment()->RunUntilIdle();
  EXPECT_EQ(0, GetJoinCount(kOriginA, kInterestGroupName));
  EXPECT_EQ(0u, GetInterestGroupsForOwner(kOriginA).size());

  // Updating again when the interest group has been deleted shouldn't somehow
  // bring it back -- also, advance past the rate limit window to ensure the
  // update actually happens.
  task_environment()->FastForwardBy(
      InterestGroupStorage::kUpdateSucceededBackoffPeriod + base::Seconds(1));
  network_responder_->RegisterUpdateResponse(kUpdateUrlPath, kServerResponse);
  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();
  EXPECT_EQ(0, GetJoinCount(kOriginA, kInterestGroupName));
  EXPECT_EQ(0u, GetInterestGroupsForOwner(kOriginA).size());

  // DB maintenance never occurs since we never FastForward() past db
  // maintenance. We still are at time order:
  // (group expiration, *NOW*, db maintenance).
}

// Start an update, and delay the server response so that the interest group
// expires before the interest group updates. Advance time enough for DB
// maintenance tasks to run -- that is the interest group will be deleted from
// the database.
TEST_F(AdAuctionServiceImplTest,
       UpdateDuringInterestGroupExpirationWithDbMaintenence) {
  constexpr char kServerResponse[] = R"({
"ads": [{"renderURL": "https://example.com/new_render"}]
})";
  network_responder_->RegisterDeferredUpdateResponse(kUpdateUrlPath);

  // Make the interest group expire just before the DB maintenance task should
  // be run. Time order:
  // (*NOW*, group expiration, db maintenance).
  const base::Time now = base::Time::Now();
  const base::TimeDelta kExpiryDelta =
      InterestGroupStorage::kIdlePeriod - base::Seconds(1);
  ASSERT_GT(kExpiryDelta, base::Seconds(0));
  const base::Time next_maintenance_time =
      now + InterestGroupStorage::kIdlePeriod;
  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.expiry = now + kExpiryDelta;
  interest_group.update_url = kUpdateUrlA;
  interest_group.bidding_url = kBiddingLogicUrlA;
  interest_group.trusted_bidding_signals_url = kTrustedBiddingSignalsUrlA;
  interest_group.trusted_bidding_signals_keys.emplace();
  interest_group.trusted_bidding_signals_keys->push_back("key1");
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  // Start an interest group update and then advance time to ensure the interest
  // group expires and then DB maintenance is performed, both before a response
  // is returned.
  UpdateInterestGroupNoFlush();
  task_environment()->FastForwardBy(InterestGroupStorage::kIdlePeriod +
                                    base::Seconds(1));
  task_environment()->RunUntilIdle();
  EXPECT_EQ(0, GetJoinCount(kOriginA, kInterestGroupName));
  EXPECT_EQ(0u, GetInterestGroupsForOwner(kOriginA).size());

  // Due to FastForwardBy(), we're at this time order:
  // (group expiration, db maintenance, *NOW*).
  // So, DB maintenance should have been run.
  base::RunLoop run_loop;
  manager_->GetLastMaintenanceTimeForTesting(base::BindLambdaForTesting(
      [&run_loop, next_maintenance_time](base::Time time) {
        EXPECT_EQ(time, next_maintenance_time);
        run_loop.Quit();
      }));
  run_loop.Run();

  // Now return the server response. The interest group shouldn't change as it's
  // expired.
  network_responder_->DoDeferredUpdateResponse(kUpdateUrlPath, kServerResponse);
  task_environment()->RunUntilIdle();
  EXPECT_EQ(0, GetJoinCount(kOriginA, kInterestGroupName));
  EXPECT_EQ(0u, GetInterestGroupsForOwner(kOriginA).size());

  // Updating again when the interest group has been deleted shouldn't somehow
  // bring it back -- also, advance past the rate limit window to ensure the
  // update actually happens.
  task_environment()->FastForwardBy(
      InterestGroupStorage::kUpdateSucceededBackoffPeriod + base::Seconds(1));
  network_responder_->RegisterUpdateResponse(kUpdateUrlPath, kServerResponse);
  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();
  EXPECT_EQ(0, GetJoinCount(kOriginA, kInterestGroupName));
  EXPECT_EQ(0u, GetInterestGroupsForOwner(kOriginA).size());
}

// Start an update, and delay the server response so that the test ends before
// the interest group finishes updating. Nothing should crash.
TEST_F(AdAuctionServiceImplTest, UpdateNeverFinishesBeforeDestruction) {
  // We never respond to this request.
  network_responder_->RegisterDeferredUpdateResponse(kUpdateUrlPath);

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.expiry = base::Time::Now() + base::Days(30);
  interest_group.update_url = kUpdateUrlA;
  interest_group.bidding_url = kBiddingLogicUrlA;
  interest_group.trusted_bidding_signals_url = kTrustedBiddingSignalsUrlA;
  interest_group.trusted_bidding_signals_keys.emplace();
  interest_group.trusted_bidding_signals_keys->push_back("key1");
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  // Start an interest group update, but never respond to network requests. The
  // update shouldn't happen.
  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // No updates have happened yet, nor will they before the test ends.
  std::vector<StorageInterestGroup> a_groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(a_groups.size(), 1u);
  auto a_group = a_groups[0].interest_group;
  ASSERT_TRUE(a_group.ads.has_value());
  ASSERT_EQ(a_group.ads->size(), 1u);
  EXPECT_EQ(a_group.ads.value()[0].render_url.spec(),
            "https://example.com/render");

  // The test ends while the update is in progress. Nothing should crash as we
  // run destructors.
}

// The update doesn't happen because the update URL isn't specified at
// Join() time.
TEST_F(AdAuctionServiceImplTest, DoesntChangeGroupsWithNoUpdateUrl) {
  network_responder_->RegisterUpdateResponse(kUpdateUrlPath, R"({
"ads": [{"renderURL": "https://example.com/new_render"
        }]
})");

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.bidding_url = kBiddingLogicUrlA;
  interest_group.trusted_bidding_signals_url = kTrustedBiddingSignalsUrlA;
  interest_group.trusted_bidding_signals_keys.emplace();
  interest_group.trusted_bidding_signals_keys->push_back("key1");
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // Check that the ads didn't change.
  std::vector<StorageInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);
  const auto& group = groups[0].interest_group;
  ASSERT_TRUE(group.ads.has_value());
  ASSERT_EQ(group.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            "https://example.com/render");
}

// Register a bid and a win, then perform a successful update. The bid and win
// stats shouldn't change.
TEST_F(AdAuctionServiceImplTest, UpdateDoesntChangeBrowserSignals) {
  network_responder_->RegisterUpdateResponse(kUpdateUrlPath, R"({
"ads": [{"renderURL": "https://example.com/new_render"
        }]
})");
  blink::InterestGroupKey originA_group_key(kOriginA, kInterestGroupName);

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.update_url = kUpdateUrlA;
  interest_group.bidding_url = kBiddingLogicUrlA;
  interest_group.trusted_bidding_signals_url = kTrustedBiddingSignalsUrlA;
  interest_group.trusted_bidding_signals_keys.emplace();
  interest_group.trusted_bidding_signals_keys->push_back("key1");
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  // Register 2 bids and a win.
  manager_->RecordInterestGroupBids(blink::InterestGroupSet{originA_group_key});
  manager_->RecordInterestGroupBids(blink::InterestGroupSet{originA_group_key});
  manager_->RecordInterestGroupWin(originA_group_key, "{}");

  std::vector<StorageInterestGroup> prev_groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(prev_groups.size(), 1u);
  const auto& prev_signals = prev_groups[0].bidding_browser_signals;
  EXPECT_EQ(prev_signals->join_count, 1);
  EXPECT_EQ(prev_signals->bid_count, 2);
  EXPECT_EQ(prev_signals->prev_wins.size(), 1u);

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // The group updates, but the signals don't.
  std::vector<StorageInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);
  const auto& group = groups[0].interest_group;
  const auto& signals = groups[0].bidding_browser_signals;

  EXPECT_EQ(signals->join_count, 1);
  EXPECT_EQ(signals->bid_count, 2);
  EXPECT_EQ(signals->prev_wins.size(), 1u);

  EXPECT_EQ(group.name, kInterestGroupName);
  ASSERT_TRUE(group.ads.has_value());
  ASSERT_EQ(group.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            "https://example.com/new_render");
}

// Join an interest group.
// Update interest group successfully.
// Change update response to different value.
// Update attempt does nothing (rate limited).
// Advance to just before time limit drops, update does nothing (rate limited).
// Advance after time limit. Update should work.
TEST_F(AdAuctionServiceImplTest, UpdateRateLimitedAfterSuccessfulUpdate) {
  network_responder_->RegisterUpdateResponse(kUpdateUrlPath, R"({
"ads": [{"renderURL": "https://example.com/new_render"
        }]
})");

  blink::InterestGroup interest_group = CreateInterestGroup();
  // Set a long expiration delta so that we can advance to the next rate limit
  // period without the interest group expiring.
  interest_group.expiry = base::Time::Now() + base::Days(30);
  interest_group.update_url = kUpdateUrlA;
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // The first update completes successfully.
  std::vector<StorageInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);
  const auto& group = groups[0].interest_group;
  ASSERT_TRUE(group.ads.has_value());
  ASSERT_EQ(group.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            "https://example.com/new_render");

  // Change the update response and try updating again.
  network_responder_->RegisterUpdateResponse(kUpdateUrlPath, R"({
"ads": [{"renderURL": "https://example.com/new_render"
        }]
})");
  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // The update does nothing due to rate limiting, nothing changes.
  std::vector<StorageInterestGroup> groups2 =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups2.size(), 1u);
  const auto& group2 = groups2[0].interest_group;
  ASSERT_TRUE(group2.ads.has_value());
  ASSERT_EQ(group2.ads->size(), 1u);
  EXPECT_EQ(group2.ads.value()[0].render_url.spec(),
            "https://example.com/new_render");

  // Advance time to just before end of rate limit period. Update should still
  // do nothing due to rate limiting.
  task_environment()->FastForwardBy(
      InterestGroupStorage::kUpdateSucceededBackoffPeriod - base::Seconds(1));

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // The update does nothing due to rate limiting, nothing changes.
  std::vector<StorageInterestGroup> groups3 =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups3.size(), 1u);
  const auto& group3 = groups3[0].interest_group;
  ASSERT_TRUE(group3.ads.has_value());
  ASSERT_EQ(group3.ads->size(), 1u);
  EXPECT_EQ(group3.ads.value()[0].render_url.spec(),
            "https://example.com/new_render");

  // Advance time to just after end of rate limit period. Update should now
  // succeed.
  task_environment()->FastForwardBy(base::Seconds(2));

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // The update changes the database contents.
  std::vector<StorageInterestGroup> groups4 =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups4.size(), 1u);
  const auto& group4 = groups4[0].interest_group;
  ASSERT_TRUE(group4.ads.has_value());
  ASSERT_EQ(group4.ads->size(), 1u);
  EXPECT_EQ(group4.ads.value()[0].render_url.spec(),
            "https://example.com/new_render");
}

// Join an interest group.
// Set up update to fail (return invalid server response).
// Update interest group fails.
// Change update response to different value that will succeed.
// Update does nothing (rate limited).
// Advance to just before rate limit drops (which for bad response is the longer
// "successful" duration), update does nothing (rate limited).
// Advance after time limit. Update should work.
TEST_F(AdAuctionServiceImplTest, UpdateRateLimitedAfterBadUpdateResponse) {
  network_responder_->RegisterUpdateResponse(kUpdateUrlPath,
                                             "This isn't JSON.");

  blink::InterestGroup interest_group = CreateInterestGroup();
  // Set a long expiration delta so that we can advance to the next rate limit
  // period without the interest group expiring.
  interest_group.expiry = base::Time::Now() + base::Days(30);
  interest_group.update_url = kUpdateUrlA;
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // The first update fails, nothing changes.
  std::vector<StorageInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);
  const auto& group = groups[0].interest_group;
  ASSERT_TRUE(group.ads.has_value());
  ASSERT_EQ(group.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            "https://example.com/render");

  // Change the update response and try updating again.
  network_responder_->RegisterUpdateResponse(kUpdateUrlPath, R"({
"ads": [{"renderURL": "https://example.com/new_render"
        }]
})");
  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // The update does nothing due to rate limiting, nothing changes.
  std::vector<StorageInterestGroup> groups2 =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups2.size(), 1u);
  const auto& group2 = groups2[0].interest_group;
  ASSERT_TRUE(group2.ads.has_value());
  ASSERT_EQ(group2.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            "https://example.com/render");

  // Advance time to just before end of rate limit period. Update should still
  // do nothing due to rate limiting. Invalid responses use the longer
  // "successful" backoff period.
  task_environment()->FastForwardBy(
      InterestGroupStorage::kUpdateSucceededBackoffPeriod - base::Seconds(1));

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // The update does nothing due to rate limiting, nothing changes.
  std::vector<StorageInterestGroup> groups3 =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups3.size(), 1u);
  const auto& group3 = groups3[0].interest_group;
  ASSERT_TRUE(group3.ads.has_value());
  ASSERT_EQ(group3.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            "https://example.com/render");

  // Advance time to just after end of rate limit period. Update should now
  // succeed.
  task_environment()->FastForwardBy(base::Seconds(2));

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // The update changes the database contents.
  std::vector<StorageInterestGroup> groups4 =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups4.size(), 1u);
  const auto& group4 = groups4[0].interest_group;
  ASSERT_TRUE(group4.ads.has_value());
  ASSERT_EQ(group4.ads->size(), 1u);
  EXPECT_EQ(group4.ads.value()[0].render_url.spec(),
            "https://example.com/new_render");
}

// Join an interest group.
// Make interest group update fail with net::ERR_CONNECTION_RESET.
// Update interest group fails.
// Change update response to succeed.
// Update does nothing (rate limited).
// Advance to just before rate limit drops, update does nothing (rate limited).
// Advance after time limit. Update should work.
TEST_F(AdAuctionServiceImplTest, UpdateRateLimitedAfterFailedUpdate) {
  network_responder_->FailNextUpdateRequestWithError(net::ERR_CONNECTION_RESET);

  blink::InterestGroup interest_group = CreateInterestGroup();
  // Set a long expiration delta so that we can advance to the next rate limit
  // period without the interest group expiring.
  interest_group.expiry = base::Time::Now() + base::Days(30);
  interest_group.update_url = kUpdateUrlA;
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // The first update fails, nothing changes.
  std::vector<StorageInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);
  const auto& group = groups[0].interest_group;
  ASSERT_TRUE(group.ads.has_value());
  ASSERT_EQ(group.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            "https://example.com/render");

  // Change the update response and try updating again.
  network_responder_->RegisterUpdateResponse(kUpdateUrlPath, R"({
"ads": [{"renderURL": "https://example.com/new_render"
        }]
})");
  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // The update does nothing due to rate limiting, nothing changes.
  std::vector<StorageInterestGroup> groups2 =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups2.size(), 1u);
  const auto& group2 = groups2[0].interest_group;
  ASSERT_TRUE(group2.ads.has_value());
  ASSERT_EQ(group2.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            "https://example.com/render");

  // Advance time to just before end of rate limit period. Update should still
  // do nothing due to rate limiting.
  task_environment()->FastForwardBy(
      InterestGroupStorage::kUpdateFailedBackoffPeriod - base::Seconds(1));

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // The update does nothing due to rate limiting, nothing changes.
  std::vector<StorageInterestGroup> groups3 =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups3.size(), 1u);
  const auto& group3 = groups3[0].interest_group;
  ASSERT_TRUE(group3.ads.has_value());
  ASSERT_EQ(group3.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            "https://example.com/render");

  // Advance time to just after end of rate limit period. Update should now
  // succeed.
  task_environment()->FastForwardBy(base::Seconds(2));

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // The update changes the database contents.
  std::vector<StorageInterestGroup> groups4 =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups4.size(), 1u);
  const auto& group4 = groups4[0].interest_group;
  ASSERT_TRUE(group4.ads.has_value());
  ASSERT_EQ(group4.ads->size(), 1u);
  EXPECT_EQ(group4.ads.value()[0].render_url.spec(),
            "https://example.com/new_render");
}

// net::ERR_INTERNET_DISCONNECTED skips rate limiting, unlike other errors.
//
// Join an interest group.
// Make interest group update fail with net::ERR_INTERNET_DISCONNECTED.
// Update interest group fails.
// Change update response to different value that will succeed.
// Update succeeds (not rate limited).
TEST_F(AdAuctionServiceImplTest, UpdateNotRateLimitedIfDisconnected) {
  network_responder_->FailNextUpdateRequestWithError(
      net::ERR_INTERNET_DISCONNECTED);

  blink::InterestGroup interest_group = CreateInterestGroup();
  // Set a long expiration delta so that we can advance to the next rate limit
  // period without the interest group expiring.
  interest_group.expiry = base::Time::Now() + base::Days(30);
  interest_group.update_url = kUpdateUrlA;
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // The first update fails (internet disconnected), nothing changes.
  std::vector<StorageInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);
  const auto& group = groups[0].interest_group;
  ASSERT_TRUE(group.ads.has_value());
  ASSERT_EQ(group.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            "https://example.com/render");

  // Change the update response and try updating again.
  network_responder_->RegisterUpdateResponse(kUpdateUrlPath, R"({
"ads": [{"renderURL": "https://example.com/new_render"
        }]
})");
  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // The update changes the database contents -- no rate limiting occurs.
  std::vector<StorageInterestGroup> groups2 =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups2.size(), 1u);
  const auto& group2 = groups2[0].interest_group;
  ASSERT_TRUE(group2.ads.has_value());
  ASSERT_EQ(group2.ads->size(), 1u);
  EXPECT_EQ(group2.ads.value()[0].render_url.spec(),
            "https://example.com/new_render");
}

// Nothing crashes if we have a disconnect and a successful update in-flight at
// the same time.
//
// Join 2 interest groups that have the same owner.
//
// Update both interest groups; the first has a delayed response, and the second
// fails with net::ERR_INTERNET_DISCONNECTED. After that, the first update
// response arrives.
//
// Check that the second interest group is not updated. Intentionally don't
// whether the first interest group updates or not.
//
// Nothing should crash.
//
// Afterwards, updating should successfully update both interest groups, without
// rate limiting.
TEST_F(AdAuctionServiceImplTest, DisconnectedAndSuccessInFlightTogether) {
  // Create 2 interest groups belonging to the same owner.
  const std::string kServerResponse1 = R"({
"ads": [{"renderURL": "https://example.com/new_render"}]
})";
  network_responder_->RegisterDeferredUpdateResponse(kUpdateUrlPath);

  blink::InterestGroup interest_group_1 = CreateInterestGroup();
  interest_group_1.expiry = base::Time::Now() + base::Days(30);
  interest_group_1.update_url = kUpdateUrlA;
  interest_group_1.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group_1.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group_1);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  network_responder_->FailUpdateRequestWithError(
      kUpdateUrlPath2, net::ERR_INTERNET_DISCONNECTED);

  constexpr char kInterestGroupName2[] = "group2";
  blink::InterestGroup interest_group_2 = CreateInterestGroup();
  interest_group_2.name = kInterestGroupName2;
  interest_group_2.expiry = base::Time::Now() + base::Days(30);
  interest_group_2.update_url = kUpdateUrlA2;
  interest_group_2.ads.emplace();
  ad = blink::InterestGroup::Ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group_2.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group_2);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName2));

  // Start the update. The second group update will fail with
  // ERR_INTERNET_DISCONNECTED.
  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // Now, let the first group's update response be sent.
  network_responder_->DoDeferredUpdateResponse(kUpdateUrlPath,
                                               kServerResponse1);
  task_environment()->RunUntilIdle();

  // The second update fails (internet disconnected), so that interest group
  // doesn't update. We don't have any particular requirement what happens to
  // the "successful" update that happened at the same time.
  std::vector<StorageInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 2u);
  auto group_2 = groups[0].interest_group.name == kInterestGroupName2
                     ? groups[0].interest_group
                     : groups[1].interest_group;
  ASSERT_TRUE(group_2.ads.has_value());
  ASSERT_EQ(group_2.ads->size(), 1u);
  EXPECT_EQ(group_2.ads.value()[0].render_url.spec(),
            "https://example.com/render");

  // Now, try to update both interest groups. Both should now succeed.
  const std::string kServerResponse2 = R"({
"ads": [{"renderURL": "https://example.com/new_render2"}]
})";
  network_responder_->RegisterUpdateResponse(kUpdateUrlPath, kServerResponse1);
  network_responder_->RegisterUpdateResponse(kUpdateUrlPath2, kServerResponse2);

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // Check that both groups updated.
  groups = GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 2u);
  auto group_1 = groups[0].interest_group.name == kInterestGroupName
                     ? groups[0].interest_group
                     : groups[1].interest_group;
  group_2 = groups[0].interest_group.name == kInterestGroupName2
                ? groups[0].interest_group
                : groups[1].interest_group;

  ASSERT_TRUE(group_1.ads.has_value());
  ASSERT_EQ(group_1.ads->size(), 1u);
  EXPECT_EQ(group_1.ads.value()[0].render_url.spec(),
            "https://example.com/new_render");

  ASSERT_TRUE(group_2.ads.has_value());
  ASSERT_EQ(group_2.ads->size(), 1u);
  EXPECT_EQ(group_2.ads.value()[0].render_url.spec(),
            "https://example.com/new_render2");
}

// Fire off many updates rapidly in a loop. Only one update should happen.
TEST_F(AdAuctionServiceImplTest, UpdateRateLimitedTightLoop) {
  network_responder_->RegisterUpdateResponse(kUpdateUrlPath, R"({
"ads": [{"renderURL": "https://example.com/new_render"
        }]
})");

  blink::InterestGroup interest_group = CreateInterestGroup();
  // Set a long expiration delta so that we can advance to the next rate limit
  // period without the interest group expiring.
  interest_group.expiry = base::Time::Now() + base::Days(30);
  interest_group.update_url = kUpdateUrlA;
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  EXPECT_EQ(network_responder_->UpdateCount(), 0u);

  for (size_t i = 0; i < 1000u; i++) {
    UpdateInterestGroupNoFlush();
  }
  task_environment()->RunUntilIdle();

  EXPECT_EQ(network_responder_->UpdateCount(), 1u);

  // One of the updates completes successfully.
  std::vector<StorageInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);
  const auto& group = groups[0].interest_group;
  ASSERT_TRUE(group.ads.has_value());
  ASSERT_EQ(group.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            "https://example.com/new_render");
}

// Test that creates 3 interest groups for different origins, then runs update
// for each origin, with the first update delayed.
//
// The second and third IGs shouldn't get updated until the first is allowed to
// proceed.
TEST_F(AdAuctionServiceImplTest, OnlyOneOriginUpdatesAtATime) {
  // kOriginA's update will be deferred, whereas kOriginB's and kOriginC's
  // updates will be allowed to proceed immediately.
  constexpr char kServerResponseA[] = R"({
"ads": [{"renderURL": "https://example.com/new_render"}]
})";
  network_responder_->RegisterDeferredUpdateResponse(kUpdateUrlPath);

  network_responder_->RegisterUpdateResponse(kUpdateUrlPathB, R"({
"ads": [{"renderURL": "https://example.com/new_render"
        }]
})");
  network_responder_->RegisterUpdateResponse(kUpdateUrlPathC, R"({
"ads": [{"renderURL": "https://example.com/new_render"
        }]
})");

  // Create interest group for kOriginA.
  blink::InterestGroup interest_group = CreateInterestGroup();
  // Set a long expiration delta so that we can advance to the next rate limit
  // period without the interest group expiring.
  interest_group.expiry = base::Time::Now() + base::Days(30);
  interest_group.update_url = kUpdateUrlA;
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  // Create interest group for kOriginB.
  NavigateAndCommit(kUrlB);
  interest_group = CreateInterestGroup();
  // Set a long expiration delta so that we can advance to the next rate limit
  // period without the interest group expiring.
  interest_group.owner = kOriginB;
  interest_group.expiry = base::Time::Now() + base::Days(30);
  interest_group.update_url = kUpdateUrlB;
  interest_group.ads.emplace();
  ad = blink::InterestGroup::Ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginB, kInterestGroupName));

  // Create interest group for kOriginC.
  NavigateAndCommit(kUrlC);
  interest_group = CreateInterestGroup();
  // Set a long expiration delta so that we can advance to the next rate limit
  // period without the interest group expiring.
  interest_group.owner = kOriginC;
  interest_group.expiry = base::Time::Now() + base::Days(30);
  interest_group.update_url = kUpdateUrlC;
  interest_group.ads.emplace();
  ad = blink::InterestGroup::Ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginC, kInterestGroupName));

  EXPECT_EQ(network_responder_->UpdateCount(), 0u);

  // Attempt to update kOriginA's interest groups. The update doesn't happen
  // yet, because the server delays its response.
  NavigateAndCommit(kUrlA);
  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();
  std::vector<StorageInterestGroup> a_groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(a_groups.size(), 1u);
  auto a_group = a_groups[0].interest_group;
  ASSERT_TRUE(a_group.ads.has_value());
  ASSERT_EQ(a_group.ads->size(), 1u);
  EXPECT_EQ(a_group.ads.value()[0].render_url.spec(),
            "https://example.com/render");

  // Now, try to update kOriginB's interest groups. The update shouldn't happen
  // yet, because we're still updating kOriginA's interest groups.
  NavigateAndCommit(kUrlB);
  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();
  std::vector<StorageInterestGroup> b_groups =
      GetInterestGroupsForOwner(kOriginB);
  ASSERT_EQ(b_groups.size(), 1u);
  auto b_group = b_groups[0].interest_group;
  ASSERT_TRUE(b_group.ads.has_value());
  ASSERT_EQ(b_group.ads->size(), 1u);
  EXPECT_EQ(b_group.ads.value()[0].render_url.spec(),
            "https://example.com/render");

  // Now, try to update kOriginC's interest groups. The update shouldn't happen
  // yet, because we're still updating kOriginA's interest groups.
  NavigateAndCommit(kUrlC);
  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();
  std::vector<StorageInterestGroup> c_groups =
      GetInterestGroupsForOwner(kOriginC);
  ASSERT_EQ(c_groups.size(), 1u);
  auto c_group = c_groups[0].interest_group;
  ASSERT_TRUE(c_group.ads.has_value());
  ASSERT_EQ(c_group.ads->size(), 1u);
  EXPECT_EQ(c_group.ads.value()[0].render_url.spec(),
            "https://example.com/render");

  // Only one network request should have been made (for the kOriginA update).
  EXPECT_EQ(network_responder_->UpdateCount(), 1u);

  // Now, the server finishes sending the kOriginA response. Both interest
  // groups should now update, since kOriginA's update completion unblocks
  // kOriginB's update.
  network_responder_->DoDeferredUpdateResponse(kUpdateUrlPath,
                                               kServerResponseA);
  task_environment()->RunUntilIdle();
  EXPECT_EQ(network_responder_->UpdateCount(), 3u);

  // kOriginA's groups have updated.
  a_groups = GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(a_groups.size(), 1u);
  a_group = a_groups[0].interest_group;
  ASSERT_TRUE(a_group.ads.has_value());
  ASSERT_EQ(a_group.ads->size(), 1u);
  EXPECT_EQ(a_group.ads.value()[0].render_url.spec(),
            "https://example.com/new_render");

  // kOriginB's groups have updated.
  b_groups = GetInterestGroupsForOwner(kOriginB);
  ASSERT_EQ(b_groups.size(), 1u);
  b_group = b_groups[0].interest_group;
  ASSERT_TRUE(b_group.ads.has_value());
  ASSERT_EQ(b_group.ads->size(), 1u);
  EXPECT_EQ(b_group.ads.value()[0].render_url.spec(),
            "https://example.com/new_render");

  // kOriginC's groups have updated.
  b_groups = GetInterestGroupsForOwner(kOriginC);
  ASSERT_EQ(b_groups.size(), 1u);
  b_group = b_groups[0].interest_group;
  ASSERT_TRUE(b_group.ads.has_value());
  ASSERT_EQ(b_group.ads->size(), 1u);
  EXPECT_EQ(b_group.ads.value()[0].render_url.spec(),
            "https://example.com/new_render");
}

// Set the maximum number of parallel updates to 2. Create three interest
// groups, each in origin A, and update origin A's interest groups.
//
// Check that all the interest groups updated.
TEST_F(AdAuctionServiceImplTest, UpdatesInBatches) {
  manager_->set_max_parallel_updates_for_testing(2);

  network_responder_->RegisterUpdateResponse(kUpdateUrlPath, R"({
"ads": [{"renderURL": "https://example.com/new_render"
        }]
})");

  // Create 3 interest groups for kOriginA.
  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.expiry = base::Time::Now() + base::Days(30);
  interest_group.update_url = kUpdateUrlA;
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  constexpr char kInterestGroupName2[] = "group2";
  interest_group = CreateInterestGroup();
  interest_group.name = kInterestGroupName2;
  interest_group.expiry = base::Time::Now() + base::Days(30);
  interest_group.update_url = kUpdateUrlA;
  interest_group.ads.emplace();
  ad = blink::InterestGroup::Ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName2));

  constexpr char kInterestGroupName3[] = "group3";
  interest_group = CreateInterestGroup();
  interest_group.name = kInterestGroupName3;
  interest_group.expiry = base::Time::Now() + base::Days(30);
  interest_group.update_url = kUpdateUrlA;
  interest_group.ads.emplace();
  ad = blink::InterestGroup::Ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName3));

  EXPECT_EQ(network_responder_->UpdateCount(), 0u);

  // Update all interest groups.
  NavigateAndCommit(kUrlA);
  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  EXPECT_EQ(network_responder_->UpdateCount(), 3u);

  std::vector<StorageInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 3u);

  for (size_t i = 0; i < groups.size(); i++) {
    SCOPED_TRACE(i);
    const auto& group = groups[i].interest_group;
    ASSERT_TRUE(group.ads.has_value());
    ASSERT_EQ(group.ads->size(), 1u);
    EXPECT_EQ(group.ads.value()[0].render_url.spec(),
              "https://example.com/new_render");
  }
}

// Set the maximum number of parallel updates to 2. Create three interest
// groups, each in origin A, and update origin A's interest groups. Make one
// fail, and one timeout.
//
// Check that the interest group that didn't fail or timeout updates
// successfully.
TEST_F(AdAuctionServiceImplTest, UpdatesInBatchesWithFailuresAndTimeouts) {
  manager_->set_max_parallel_updates_for_testing(2);

  network_responder_->RegisterUpdateResponse(kUpdateUrlPath, R"({
"ads": [{"renderURL": "https://example.com/new_render"
        }]
})");
  network_responder_->FailUpdateRequestWithError(kUpdateUrlPath2,
                                                 net::ERR_CONNECTION_RESET);
  // We never respond to this -- just let it timeout.
  network_responder_->RegisterDeferredUpdateResponse(kUpdateUrlPath3);

  // Create 3 interest groups for kOriginA -- give them different update URLs to
  // so that some timeout and some fail.
  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.expiry = base::Time::Now() + base::Days(30);
  interest_group.update_url = kUpdateUrlA;
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  constexpr char kInterestGroupName2[] = "group2";
  interest_group = CreateInterestGroup();
  interest_group.name = kInterestGroupName2;
  interest_group.expiry = base::Time::Now() + base::Days(30);
  interest_group.update_url = kUpdateUrlA2;
  interest_group.ads.emplace();
  ad = blink::InterestGroup::Ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName2));

  constexpr char kInterestGroupName3[] = "group3";
  interest_group = CreateInterestGroup();
  interest_group.name = kInterestGroupName3;
  interest_group.expiry = base::Time::Now() + base::Days(30);
  interest_group.update_url = kUpdateUrlA3;
  interest_group.ads.emplace();
  ad = blink::InterestGroup::Ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName3));

  EXPECT_EQ(network_responder_->UpdateCount(), 0u);

  // Update all interest groups.
  NavigateAndCommit(kUrlA);
  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // Requests are issued in random order. If the first or second request is the
  // delayed request, the third request won't be issued, since the first 2
  // aren't complete. On the other hand, if the delayed request is the third
  // request, all three update requests would have been issued by now.
  EXPECT_GE(network_responder_->UpdateCount(), 2u);
  EXPECT_LE(network_responder_->UpdateCount(), 3u);

  // Now, fast forward so that the hanging request times out. After this, all
  // updates should be completed.
  task_environment()->FastForwardBy(base::Seconds(31));
  task_environment()->RunUntilIdle();
  EXPECT_EQ(network_responder_->UpdateCount(), 3u);

  std::vector<StorageInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 3u);

  for (size_t i = 0; i < groups.size(); i++) {
    SCOPED_TRACE(i);
    const auto& group = groups[i].interest_group;
    ASSERT_TRUE(group.ads.has_value());
    ASSERT_EQ(group.ads->size(), 1u);

    if (group.update_url == kUpdateUrlA) {
      EXPECT_EQ(group.ads.value()[0].render_url.spec(),
                "https://example.com/new_render");
    } else {
      EXPECT_EQ(group.ads.value()[0].render_url.spec(),
                "https://example.com/render");
    }
  }
}

// Create an interest group in a.test, and in b.test. Defer the update response
// for a.test, and update a.test and b.test.
//
// Wait the max update round duration, then respond to the a.test update
// request. The a.test interest group should update, but the b.test update
// should be cancelled.
//
// Then, try updating b.test normally, without deferral. The update should
// complete successfully.
TEST_F(AdAuctionServiceImplTest, CancelsLongstandingUpdates) {
  // Lower the max update round duration so that it is smaller than the network
  // timeout.
  //
  // The production value is much longer than the interest group
  // network timeout, so to exceed the production max update round duration,
  // we'd need to do delayed updates for a large number of interest groups. The
  // test override avoids this awkwardness while still exercising the same
  // scenario.
  constexpr base::TimeDelta kMaxUpdateRoundDuration = base::Seconds(5);
  manager_->set_max_update_round_duration_for_testing(kMaxUpdateRoundDuration);

  // kOriginA's update will be deferred, whereas kOriginB's
  // update will be allowed to proceed immediately.
  constexpr char kServerResponseA[] = R"({
"ads": [{"renderURL": "https://example.com/new_render"}]
})";
  network_responder_->RegisterDeferredUpdateResponse(kUpdateUrlPath);

  network_responder_->RegisterUpdateResponse(kUpdateUrlPathB, R"({
"ads": [{"renderURL": "https://example.com/new_render"
        }]
})");

  // Create interest group for kOriginA.
  blink::InterestGroup interest_group = CreateInterestGroup();
  // Set a long expiration delta so that we can advance to update cancellation
  // without the interest group expiring.
  interest_group.expiry = base::Time::Now() + base::Days(30);
  interest_group.update_url = kUpdateUrlA;
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  // Create interest group for kOriginB.
  NavigateAndCommit(kUrlB);
  interest_group = CreateInterestGroup();
  // Set a long expiration delta so that we can advance to update cancellation
  // without the interest group expiring.
  interest_group.owner = kOriginB;
  interest_group.expiry = base::Time::Now() + base::Days(30);
  interest_group.update_url = kUpdateUrlB;
  interest_group.ads.emplace();
  ad = blink::InterestGroup::Ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginB, kInterestGroupName));

  EXPECT_EQ(network_responder_->UpdateCount(), 0u);

  // Attempt to update kOriginA's interest groups. The update doesn't happen
  // yet, because the server delays its response.
  NavigateAndCommit(kUrlA);
  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();
  std::vector<StorageInterestGroup> a_groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(a_groups.size(), 1u);
  auto a_group = a_groups[0].interest_group;
  ASSERT_TRUE(a_group.ads.has_value());
  ASSERT_EQ(a_group.ads->size(), 1u);
  EXPECT_EQ(a_group.ads.value()[0].render_url.spec(),
            "https://example.com/render");

  // Now, try to update kOriginB's interest groups. The update shouldn't happen
  // yet, because we're still updating kOriginA's interest groups.
  NavigateAndCommit(kUrlB);
  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();
  std::vector<StorageInterestGroup> b_groups =
      GetInterestGroupsForOwner(kOriginB);
  ASSERT_EQ(b_groups.size(), 1u);
  auto b_group = b_groups[0].interest_group;
  ASSERT_TRUE(b_group.ads.has_value());
  ASSERT_EQ(b_group.ads->size(), 1u);
  EXPECT_EQ(b_group.ads.value()[0].render_url.spec(),
            "https://example.com/render");

  // Only one network request should have been made (for the kOriginA update).
  EXPECT_EQ(network_responder_->UpdateCount(), 1u);

  // Advance time beyond the max update round duration. This will result in
  // kOriginB's update getting cancelled, but kOriginA's update will still be
  // able to proceed because it's in-progress.
  task_environment()->FastForwardBy(kMaxUpdateRoundDuration + base::Seconds(1));

  // Now, the server finishes sending the kOriginA response. Both interest
  // groups should now update, since kOriginA's update completion unblocks
  // kOriginB's update. However, kOriginB's update never happens, because it
  // gets cancelled.
  network_responder_->DoDeferredUpdateResponse(kUpdateUrlPath,
                                               kServerResponseA);
  task_environment()->RunUntilIdle();
  EXPECT_EQ(network_responder_->UpdateCount(), 1u);

  // kOriginA's groups have updated.
  a_groups = GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(a_groups.size(), 1u);
  a_group = a_groups[0].interest_group;
  ASSERT_TRUE(a_group.ads.has_value());
  ASSERT_EQ(a_group.ads->size(), 1u);
  EXPECT_EQ(a_group.ads.value()[0].render_url.spec(),
            "https://example.com/new_render");

  // But kOriginB's groups have not updated, because they got cancelled.
  b_groups = GetInterestGroupsForOwner(kOriginB);
  ASSERT_EQ(b_groups.size(), 1u);
  b_group = b_groups[0].interest_group;
  ASSERT_TRUE(b_group.ads.has_value());
  ASSERT_EQ(b_group.ads->size(), 1u);
  EXPECT_EQ(b_group.ads.value()[0].render_url.spec(),
            "https://example.com/render");

  // Now, try updating kOriginB. The update should complete successfully.
  network_responder_->RegisterUpdateResponse(kUpdateUrlPathB, R"({
"ads": [{"renderURL": "https://example.com/newer_render"
        }]
})");

  NavigateAndCommit(kUrlB);
  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // kOriginB's groups have updated.
  b_groups = GetInterestGroupsForOwner(kOriginB);
  ASSERT_EQ(b_groups.size(), 1u);
  b_group = b_groups[0].interest_group;
  ASSERT_TRUE(b_group.ads.has_value());
  ASSERT_EQ(b_group.ads->size(), 1u);
  EXPECT_EQ(b_group.ads.value()[0].render_url.spec(),
            "https://example.com/newer_render");
}

// Like CancelsLongstandingUpdates, but after the cancellation, tries to update
// a different origin, c.test, that succeeds.
//
// NOTE that a.test won't qualify for update until the next day due to rate
// limiting, since it successfully updated.
TEST_F(AdAuctionServiceImplTest, CancelsLongstandingUpdates2) {
  // Lower the max update round duration so that it is smaller than the network
  // timeout.
  //
  // The production value is much longer than the interest group
  // network timeout, so to exceed the production max update round duration,
  // we'd need to do delayed updates for a large number of interest groups. The
  // test override avoids this awkwardness while still exercising the same
  // scenario.
  constexpr base::TimeDelta kMaxUpdateRoundDuration = base::Seconds(5);
  manager_->set_max_update_round_duration_for_testing(kMaxUpdateRoundDuration);

  // kOriginA's update will be deferred, whereas kOriginB's
  // update will be allowed to proceed immediately.
  constexpr char kServerResponseA[] = R"({
"ads": [{"renderURL": "https://example.com/new_render"}]
})";
  network_responder_->RegisterDeferredUpdateResponse(kUpdateUrlPath);

  network_responder_->RegisterUpdateResponse(kUpdateUrlPathB, R"({
"ads": [{"renderURL": "https://example.com/new_render"
        }]
})");

  // Create interest group for kOriginA.
  blink::InterestGroup interest_group = CreateInterestGroup();
  // Set a long expiration delta so that we can advance to update cancellation
  // without the interest group expiring.
  interest_group.expiry = base::Time::Now() + base::Days(30);
  interest_group.update_url = kUpdateUrlA;
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  // Create interest group for kOriginB.
  NavigateAndCommit(kUrlB);
  interest_group = CreateInterestGroup();
  // Set a long expiration delta so that we can advance to update cancellation
  // without the interest group expiring.
  interest_group.owner = kOriginB;
  interest_group.expiry = base::Time::Now() + base::Days(30);
  interest_group.update_url = kUpdateUrlB;
  interest_group.ads.emplace();
  ad = blink::InterestGroup::Ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginB, kInterestGroupName));

  EXPECT_EQ(network_responder_->UpdateCount(), 0u);

  // Attempt to update kOriginA's interest groups. The update doesn't happen
  // yet, because the server delays its response.
  NavigateAndCommit(kUrlA);
  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();
  std::vector<StorageInterestGroup> a_groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(a_groups.size(), 1u);
  auto a_group = a_groups[0].interest_group;
  ASSERT_TRUE(a_group.ads.has_value());
  ASSERT_EQ(a_group.ads->size(), 1u);
  EXPECT_EQ(a_group.ads.value()[0].render_url.spec(),
            "https://example.com/render");

  // Now, try to update kOriginB's interest groups. The update shouldn't happen
  // yet, because we're still updating kOriginA's interest groups.
  NavigateAndCommit(kUrlB);
  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();
  std::vector<StorageInterestGroup> b_groups =
      GetInterestGroupsForOwner(kOriginB);
  ASSERT_EQ(b_groups.size(), 1u);
  auto b_group = b_groups[0].interest_group;
  ASSERT_TRUE(b_group.ads.has_value());
  ASSERT_EQ(b_group.ads->size(), 1u);
  EXPECT_EQ(b_group.ads.value()[0].render_url.spec(),
            "https://example.com/render");

  // Only one network request should have been made (for the kOriginA update).
  EXPECT_EQ(network_responder_->UpdateCount(), 1u);

  // Advance time beyond the max update round duration. This will result in
  // kOriginB's update getting cancelled, but kOriginA's update will still be
  // able to proceed because it's in-progress.
  task_environment()->FastForwardBy(kMaxUpdateRoundDuration + base::Seconds(1));

  // Now, the server finishes sending the kOriginA response. Both interest
  // groups should now update, since kOriginA's update completion unblocks
  // kOriginB's update. However, kOriginB's update never happens, because it
  // gets cancelled.
  network_responder_->DoDeferredUpdateResponse(kUpdateUrlPath,
                                               kServerResponseA);
  task_environment()->RunUntilIdle();
  EXPECT_EQ(network_responder_->UpdateCount(), 1u);

  // kOriginA's groups have updated.
  a_groups = GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(a_groups.size(), 1u);
  a_group = a_groups[0].interest_group;
  ASSERT_TRUE(a_group.ads.has_value());
  ASSERT_EQ(a_group.ads->size(), 1u);
  EXPECT_EQ(a_group.ads.value()[0].render_url.spec(),
            "https://example.com/new_render");

  // But kOriginB's groups have not updated, because they got cancelled.
  b_groups = GetInterestGroupsForOwner(kOriginB);
  ASSERT_EQ(b_groups.size(), 1u);
  b_group = b_groups[0].interest_group;
  ASSERT_TRUE(b_group.ads.has_value());
  ASSERT_EQ(b_group.ads->size(), 1u);
  EXPECT_EQ(b_group.ads.value()[0].render_url.spec(),
            "https://example.com/render");

  // Now, try updating a new origin, kOriginC. The update should complete
  // successfully.

  // Create interest group for kOriginC.
  NavigateAndCommit(kUrlC);
  interest_group = CreateInterestGroup();
  interest_group.owner = kOriginC;
  interest_group.expiry = base::Time::Now() + base::Days(30);
  interest_group.update_url = kUpdateUrlC;
  interest_group.ads.emplace();
  ad = blink::InterestGroup::Ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  ASSERT_TRUE(interest_group.IsValid());
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginC, kInterestGroupName));

  network_responder_->RegisterUpdateResponse(kUpdateUrlPathC, R"({
"ads": [{"renderURL": "https://example.com/newer_render"
        }]
})");
  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // kOriginC's groups have updated.
  auto c_groups = GetInterestGroupsForOwner(kOriginC);
  ASSERT_EQ(c_groups.size(), 1u);
  auto c_group = c_groups[0].interest_group;
  ASSERT_TRUE(c_group.ads.has_value());
  ASSERT_EQ(c_group.ads->size(), 1u);
  EXPECT_EQ(c_group.ads.value()[0].render_url.spec(),
            "https://example.com/newer_render");

  // But kOriginB's groups have not updated.
  b_groups = GetInterestGroupsForOwner(kOriginB);
  ASSERT_EQ(b_groups.size(), 1u);
  b_group = b_groups[0].interest_group;
  ASSERT_TRUE(b_group.ads.has_value());
  ASSERT_EQ(b_group.ads->size(), 1u);
  EXPECT_EQ(b_group.ads.value()[0].render_url.spec(),
            "https://example.com/render");
}

// After a round of updates completes, the round cancellation timer should reset
// so that future updates can proceed.
//
// Create 2 interest groups in different origins. Update the first, then wait
// for more than the max update round duration, then update the second.
//
// Both interest groups should update correctly.
TEST_F(AdAuctionServiceImplTest, UpdateCancellationTimerClearedOnCompletion) {
  // Set the max update duration to a known value.
  constexpr base::TimeDelta kMaxUpdateRoundDuration = base::Seconds(5);
  manager_->set_max_update_round_duration_for_testing(kMaxUpdateRoundDuration);

  network_responder_->RegisterUpdateResponse(kUpdateUrlPath, R"({
"ads": [{"renderURL": "https://example.com/new_render"
        }]
})");
  network_responder_->RegisterUpdateResponse(kUpdateUrlPathB, R"({
"ads": [{"renderURL": "https://example.com/new_render"
        }]
})");

  // Create interest group for kOriginA.
  blink::InterestGroup interest_group = CreateInterestGroup();
  // Set a long expiration delta so that we can advance to update cancellation
  // without the interest group expiring.
  interest_group.expiry = base::Time::Now() + base::Days(30);
  interest_group.update_url = kUpdateUrlA;
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  // Create interest group for kOriginB.
  NavigateAndCommit(kUrlB);
  interest_group = CreateInterestGroup();
  // Set a long expiration delta so that we can advance to update cancellation
  // without the interest group expiring.
  interest_group.owner = kOriginB;
  interest_group.expiry = base::Time::Now() + base::Days(30);
  interest_group.update_url = kUpdateUrlB;
  interest_group.ads.emplace();
  ad = blink::InterestGroup::Ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginB, kInterestGroupName));

  EXPECT_EQ(network_responder_->UpdateCount(), 0u);

  // Attempt to update kOriginA's interest groups. The update completes
  // successfully.
  NavigateAndCommit(kUrlA);
  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  std::vector<StorageInterestGroup> a_groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(a_groups.size(), 1u);
  auto a_group = a_groups[0].interest_group;
  ASSERT_TRUE(a_group.ads.has_value());
  ASSERT_EQ(a_group.ads->size(), 1u);
  EXPECT_EQ(a_group.ads.value()[0].render_url.spec(),
            "https://example.com/new_render");

  // Only one network request should have been made (for the kOriginA update).
  EXPECT_EQ(network_responder_->UpdateCount(), 1u);

  // Advance time beyond the max update round duration.
  task_environment()->FastForwardBy(kMaxUpdateRoundDuration + base::Seconds(1));

  // Now, try to update kOriginB's interest groups. The update completes
  // successfully.
  NavigateAndCommit(kUrlB);
  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  std::vector<StorageInterestGroup> b_groups =
      GetInterestGroupsForOwner(kOriginB);
  ASSERT_EQ(b_groups.size(), 1u);
  auto b_group = b_groups[0].interest_group;
  ASSERT_TRUE(b_group.ads.has_value());
  ASSERT_EQ(b_group.ads->size(), 1u);
  EXPECT_EQ(b_group.ads.value()[0].render_url.spec(),
            "https://example.com/new_render");

  // Two network requests should have been made (for the kOriginA and kOriginB
  // updates).
  EXPECT_EQ(network_responder_->UpdateCount(), 2u);
}

// Create 4 interest groups in a.test, and one in b.test.
//
// For the a.test groups, have one succeed immediately, one fail immediately
// (invalid JSON), one be delayed a duration shorter than the max update round
// duration (and succeed), and one delayed a duration more than the max update
// round duration (and succeed).
//
// For the b.test group, let it succeed immediately.
//
// Update all groups, advancing time twice to issue the 2 a.test delayed
// responses.
//
// All a.test updates except the failed update should succeed. The b.test update
// should be cancelled.
//
// Then, try updating b.test normally, without deferral. The update should
// complete successfully.
TEST_F(AdAuctionServiceImplTest, CancelsLongstandingUpdatesComplex) {
  // Lower the max update round duration so that it is smaller than the network
  // timeout.
  //
  // The production value is much longer than the interest group
  // network timeout, so to exceed the production max update round duration,
  // we'd need to do delayed updates for a large number of interest groups. The
  // test override avoids this awkwardness while still exercising the same
  // scenario.
  constexpr base::TimeDelta kMaxUpdateRoundDuration = base::Seconds(5);
  manager_->set_max_update_round_duration_for_testing(kMaxUpdateRoundDuration);

  // 2 of kOriginA's updates will be deferred (each by different amounts of
  // time) and one will be allowed to proceed immediately, whereas kOriginB's 1
  // update will be allowed to proceed immediately. The last group's update will
  // fail.
  constexpr char kServerResponse[] = R"({
"ads": [{"renderURL": "https://example.com/render2"}]
})";
  network_responder_->RegisterUpdateResponse(kUpdateUrlPath, kServerResponse);
  network_responder_->FailUpdateRequestWithError(kUpdateUrlPath2,
                                                 net::ERR_CONNECTION_RESET);
  network_responder_->RegisterDeferredUpdateResponse(kUpdateUrlPath3);
  network_responder_->RegisterDeferredUpdateResponse(kUpdateUrlPath4);

  network_responder_->RegisterUpdateResponse(kUpdateUrlPathB, kServerResponse);

  // Create interest groups for kOriginA.
  for (const GURL& update_url :
       {kUpdateUrlA, kUpdateUrlA2, kUpdateUrlA3, kUpdateUrlA4}) {
    blink::InterestGroup interest_group = CreateInterestGroup();
    // Set a long expiration delta so that we can advance to update cancellation
    // without the interest group expiring.
    interest_group.expiry = base::Time::Now() + base::Days(30);
    interest_group.name = update_url.path();
    interest_group.update_url = update_url;
    interest_group.ads.emplace();
    blink::InterestGroup::Ad ad(
        /*render_url=*/GURL("https://example.com/render"),
        /*metadata=*/absl::nullopt);
    interest_group.ads->emplace_back(std::move(ad));
    JoinInterestGroupAndFlush(interest_group);
    EXPECT_EQ(1, GetJoinCount(kOriginA, /*name=*/update_url.path()));
  }

  // Create interest group for kOriginB.
  NavigateAndCommit(kUrlB);
  blink::InterestGroup interest_group = CreateInterestGroup();
  // Set a long expiration delta so that we can advance to update cancellation
  // without the interest group expiring.
  interest_group.owner = kOriginB;
  interest_group.expiry = base::Time::Now() + base::Days(30);
  interest_group.update_url = kUpdateUrlB;
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginB, kInterestGroupName));

  EXPECT_EQ(network_responder_->UpdateCount(), 0u);

  // Attempt to update kOriginA's interest groups. The first 2 interest group
  // updates complete (success and failure). The remaining updates don't
  // happen yet, because the server delays its response.
  NavigateAndCommit(kUrlA);
  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();
  std::vector<StorageInterestGroup> a_groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(a_groups.size(), 4u);
  bool seen_succeeded = false, seen_failed = false;
  for (const auto& a_group : a_groups) {
    const blink::InterestGroup& group = a_group.interest_group;
    ASSERT_TRUE(group.ads.has_value());
    ASSERT_EQ(group.ads->size(), 1u);
    if (group.name == kUpdateUrlA.path()) {
      EXPECT_EQ(group.ads.value()[0].render_url.spec(),
                "https://example.com/render2");
      seen_succeeded = true;
      continue;
    } else if (group.name == kUpdateUrlA2.path()) {
      seen_failed = true;
    }
    // Failed and deferred interest groups shouldn't have updated.
    EXPECT_EQ(group.ads.value()[0].render_url.spec(),
              "https://example.com/render");
  }
  EXPECT_TRUE(seen_succeeded);
  EXPECT_TRUE(seen_failed);

  // Now, try to update kOriginB's interest groups. The update shouldn't happen
  // yet, because we're still updating kOriginA's interest groups.
  NavigateAndCommit(kUrlB);
  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();
  std::vector<StorageInterestGroup> b_groups =
      GetInterestGroupsForOwner(kOriginB);
  ASSERT_EQ(b_groups.size(), 1u);
  auto b_group = b_groups[0].interest_group;
  ASSERT_TRUE(b_group.ads.has_value());
  ASSERT_EQ(b_group.ads->size(), 1u);
  EXPECT_EQ(b_group.ads.value()[0].render_url.spec(),
            "https://example.com/render");

  // Only 4 network requests should have been made (for the kOriginA updates).
  EXPECT_EQ(network_responder_->UpdateCount(), 4u);

  // Advance time to just before the max update round duration, then issue the
  // server response for one of the interest group updates. It should update
  // immediately.
  task_environment()->FastForwardBy(kMaxUpdateRoundDuration - base::Seconds(1));
  network_responder_->DoDeferredUpdateResponse(kUpdateUrlPath3,
                                               kServerResponse);
  task_environment()->RunUntilIdle();
  a_groups = GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(a_groups.size(), 4u);
  for (const auto& a_group : a_groups) {
    const blink::InterestGroup& group = a_group.interest_group;
    ASSERT_TRUE(group.ads.has_value());
    ASSERT_EQ(group.ads->size(), 1u);
    if (group.name == kUpdateUrlA3.path()) {
      EXPECT_EQ(group.ads.value()[0].render_url.spec(),
                "https://example.com/render2");
      break;
    }
  }
  EXPECT_EQ(network_responder_->UpdateCount(), 4u);

  // Advance time beyond the max update round duration. This will result in
  // kOriginB's update getting cancelled, but kOriginA's last update will still
  // be able to proceed because it's in-progress.
  task_environment()->FastForwardBy(base::Seconds(2));

  // Now, the server finishes sending the last kOriginA response. Both it and
  // kOriginB's interest groups should now update, since the completion of
  // kOriginA's last update unblocks kOriginB's update. However, kOriginB's
  // update never happens, because it gets cancelled.
  network_responder_->DoDeferredUpdateResponse(kUpdateUrlPath4,
                                               kServerResponse);
  task_environment()->RunUntilIdle();
  a_groups = GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(a_groups.size(), 4u);
  for (const auto& a_group : a_groups) {
    const blink::InterestGroup& group = a_group.interest_group;
    ASSERT_TRUE(group.ads.has_value());
    ASSERT_EQ(group.ads->size(), 1u);
    if (group.name == kUpdateUrlA4.path()) {
      EXPECT_EQ(group.ads.value()[0].render_url.spec(),
                "https://example.com/render2");
      break;
    }
  }
  EXPECT_EQ(network_responder_->UpdateCount(), 4u);

  // kOriginB's group hasn't been updated, because the update got cancelled.
  b_groups = GetInterestGroupsForOwner(kOriginB);
  ASSERT_EQ(b_groups.size(), 1u);
  b_group = b_groups[0].interest_group;
  ASSERT_TRUE(b_group.ads.has_value());
  ASSERT_EQ(b_group.ads->size(), 1u);
  EXPECT_EQ(b_group.ads.value()[0].render_url.spec(),
            "https://example.com/render");

  // Now, try updating kOriginB. The update should complete successfully.
  network_responder_->RegisterUpdateResponse(kUpdateUrlPathB, R"({
"ads": [{"renderURL": "https://example.com/render3"
        }]
})");

  NavigateAndCommit(kUrlB);
  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // kOriginB's groups have updated.
  b_groups = GetInterestGroupsForOwner(kOriginB);
  ASSERT_EQ(b_groups.size(), 1u);
  b_group = b_groups[0].interest_group;
  ASSERT_TRUE(b_group.ads.has_value());
  ASSERT_EQ(b_group.ads->size(), 1u);
  EXPECT_EQ(b_group.ads.value()[0].render_url.spec(),
            "https://example.com/render3");
}

// Add an interest group, and run an ad auction.
TEST_F(AdAuctionServiceImplTest, RunAdAuction) {
  constexpr char kBiddingScript[] = R"(
function generateBid(
    interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
    browserSignals) {
  return {'ad': 'example', 'bid': 1, 'render': 'https://example.com/render'};
}
)";

  constexpr char kDecisionScript[] = R"(
function scoreAd(
    adMetadata, bid, auctionConfig, trustedScoringSignals, browserSignals) {
  return bid;
}
)";

  network_responder_->RegisterScriptResponse(kBiddingUrlPath, kBiddingScript);
  network_responder_->RegisterScriptResponse(kDecisionUrlPath, kDecisionScript);

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.bidding_url = kUrlA.Resolve(kBiddingUrlPath);
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  blink::AuctionConfig auction_config;
  auction_config.seller = kOriginA;
  auction_config.decision_logic_url = kUrlA.Resolve(kDecisionUrlPath);
  auction_config.non_shared_params.interest_group_buyers = {kOriginA};
  absl::optional<GURL> auction_result = RunAdAuctionAndFlush(auction_config);
  ASSERT_NE(auction_result, absl::nullopt);
  EXPECT_EQ(ConvertFencedFrameURNToURL(*auction_result),
            GURL("https://example.com/render"));

  // Running the auction alone should not result in updating the interest
  // group's bid count or previous win list, no matter how much time passes.
  task_environment()->RunUntilIdle();
  auto storage_interest_group =
      GetInterestGroup(interest_group.owner, interest_group.name);
  ASSERT_TRUE(storage_interest_group);
  EXPECT_EQ(0, storage_interest_group->bidding_browser_signals->bid_count);
  EXPECT_EQ(0u,
            storage_interest_group->bidding_browser_signals->prev_wins.size());

  // Invoking the URN callback (which is done when the result is loaded in a
  // frame) updates those fields.
  InvokeCallbackForURN(*auction_result);
  storage_interest_group =
      GetInterestGroup(interest_group.owner, interest_group.name);
  ASSERT_TRUE(storage_interest_group);
  EXPECT_EQ(1, storage_interest_group->bidding_browser_signals->bid_count);
  ASSERT_EQ(1u,
            storage_interest_group->bidding_browser_signals->prev_wins.size());
  ASSERT_EQ(
      R"({"render_url":"https://example.com/render"})",
      storage_interest_group->bidding_browser_signals->prev_wins[0]->ad_json);

  // The auction should also trigger a k-anon "join" for the winning ad.
  EXPECT_THAT(GetKAnonJoinedIds(),
              ::testing::UnorderedElementsAre(
                  KAnonKeyForAdBid(interest_group,
                                   interest_group.ads.value()[0].render_url),
                  KAnonKeyForAdNameReporting(interest_group,
                                             interest_group.ads.value()[0])));
}

// Add an interest group, and run an ad auction. Seller rejects the bid. Bid
// count should be updated.
TEST_F(AdAuctionServiceImplTest, RunAdAuctionSellerRejectsBid) {
  constexpr char kBiddingScript[] = R"(
function generateBid(
    interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
    browserSignals) {
  return {'ad': 'example', 'bid': 1, 'render': 'https://example.com/render'};
}
)";

  constexpr char kDecisionScript[] = R"(
function scoreAd(
    adMetadata, bid, auctionConfig, trustedScoringSignals, browserSignals) {
  return -1;
}
)";

  network_responder_->RegisterScriptResponse(kBiddingUrlPath, kBiddingScript);
  network_responder_->RegisterScriptResponse(kDecisionUrlPath, kDecisionScript);

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.bidding_url = kUrlA.Resolve(kBiddingUrlPath);
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  blink::AuctionConfig auction_config;
  auction_config.seller = kOriginA;
  auction_config.decision_logic_url = kUrlA.Resolve(kDecisionUrlPath);
  auction_config.non_shared_params.interest_group_buyers = {kOriginA};
  absl::optional<GURL> auction_result = RunAdAuctionAndFlush(auction_config);
  EXPECT_EQ(auction_result, absl::nullopt);

  // The bid count should be updated immediately, since theere's no URN to wait
  // to be loaded in a frame.
  auto storage_interest_group =
      GetInterestGroup(interest_group.owner, interest_group.name);
  ASSERT_TRUE(storage_interest_group);
  EXPECT_EQ(1, storage_interest_group->bidding_browser_signals->bid_count);
  EXPECT_EQ(0u,
            storage_interest_group->bidding_browser_signals->prev_wins.size());

  // The auction should not trigger any k-anon "joins".
  EXPECT_THAT(GetKAnonJoinedIds(), ::testing::UnorderedElementsAre());
}

// Run ad auction when number of urn mappings has reached limit, the action
// should fail.
TEST_F(AdAuctionServiceImplTest,
       RunAdAuctionExceedNumOfUrnMappingsLimitFailsAuction) {
  constexpr char kBiddingScript[] = R"(
function generateBid(
    interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
    browserSignals) {
  return {'ad': 'example', 'bid': 1, 'render': 'https://example.com/render'};
}
)";

  constexpr char kDecisionScript[] = R"(
function scoreAd(
    adMetadata, bid, auctionConfig, trustedScoringSignals, browserSignals) {
  return bid;
}
)";

  network_responder_->RegisterScriptResponse(kBiddingUrlPath, kBiddingScript);
  network_responder_->RegisterScriptResponse(kDecisionUrlPath, kDecisionScript);

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.bidding_url = kUrlA.Resolve(kBiddingUrlPath);
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  blink::AuctionConfig auction_config;
  auction_config.seller = kOriginA;
  auction_config.decision_logic_url = kUrlA.Resolve(kDecisionUrlPath);
  auction_config.non_shared_params.interest_group_buyers = {kOriginA};

  FencedFrameURLMapping& fenced_frame_urls_map =
      static_cast<RenderFrameHostImpl*>(main_rfh())
          ->GetPage()
          .fenced_frame_urls_map();
  FencedFrameURLMappingTestPeer fenced_frame_url_mapping_test_peer(
      &fenced_frame_urls_map);

  // Fill the map until its size reaches the limit.
  GURL url("https://a.test");
  fenced_frame_url_mapping_test_peer.FillMap(url);

  absl::optional<GURL> auction_result = RunAdAuctionAndFlush(auction_config);
  // Auction failed because the number of urn mappings has reached limit.
  ASSERT_EQ(auction_result, absl::nullopt);
}

// Runs an auction, and expects that the interest group that participated in
// the auction gets updated after the auction completes.
//
// Create an interest group. Run an auction with that interest group.
//
// The interest group should be updated after the auction completes.
TEST_F(AdAuctionServiceImplTest, UpdatesInterestGroupsAfterSuccessfulAuction) {
  constexpr char kBiddingScript[] = R"(
function generateBid(
    interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
    browserSignals) {
  return {'ad': 'example', 'bid': 1, 'render': 'https://example.com/render'};
}
)";

  constexpr char kDecisionScript[] = R"(
function scoreAd(
    adMetadata, bid, auctionConfig, trustedScoringSignals, browserSignals) {
  return bid;
}
)";

  network_responder_->RegisterUpdateResponse(kUpdateUrlPath, R"({
"ads": [{"renderURL": "https://example.com/new_render"
        }]
})");

  network_responder_->RegisterScriptResponse(kBiddingUrlPath, kBiddingScript);
  network_responder_->RegisterScriptResponse(kDecisionUrlPath, kDecisionScript);

  blink::InterestGroup interest_group_a = CreateInterestGroup();
  interest_group_a.update_url = kUpdateUrlA;
  interest_group_a.bidding_url = kUrlA.Resolve(kBiddingUrlPath);
  interest_group_a.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group_a.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group_a);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  blink::AuctionConfig auction_config;
  auction_config.seller = kOriginA;
  auction_config.decision_logic_url = kUrlA.Resolve(kDecisionUrlPath);
  auction_config.non_shared_params.interest_group_buyers = {kOriginA};
  absl::optional<GURL> auction_result = RunAdAuctionAndFlush(auction_config);
  ASSERT_NE(auction_result, absl::nullopt);
  EXPECT_EQ(ConvertFencedFrameURNToURL(*auction_result),
            GURL("https://example.com/render"));

  // Now that the auction has completed, check that the interest group updated.
  task_environment()->RunUntilIdle();

  auto a_groups = GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(a_groups.size(), 1u);
  auto a_group = a_groups[0].interest_group;
  ASSERT_TRUE(a_group.ads.has_value());
  ASSERT_EQ(a_group.ads->size(), 1u);
  EXPECT_EQ(a_group.ads.value()[0].render_url.spec(),
            "https://example.com/new_render");
}

// Like UpdatesInterestGroupsAfterSuccessfulAuction, but the auction fails
// because the scoring script always returns 0. The interest group should still
// update.
TEST_F(AdAuctionServiceImplTest, UpdatesInterestGroupsAfterFailedAuction) {
  constexpr char kBiddingScript[] = R"(
function generateBid(
    interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
    browserSignals) {
  return {'ad': 'example', 'bid': 1, 'render': 'https://example.com/render'};
}
)";

  constexpr char kDecisionScript[] = R"(
function scoreAd(
    adMetadata, bid, auctionConfig, trustedScoringSignals, browserSignals) {
  return 0;
}
)";

  network_responder_->RegisterUpdateResponse(kUpdateUrlPath, R"({
"ads": [{"renderURL": "https://example.com/new_render"
        }]
})");

  network_responder_->RegisterScriptResponse(kBiddingUrlPath, kBiddingScript);
  network_responder_->RegisterScriptResponse(kDecisionUrlPath, kDecisionScript);

  blink::InterestGroup interest_group_a = CreateInterestGroup();
  interest_group_a.update_url = kUpdateUrlA;
  interest_group_a.bidding_url = kUrlA.Resolve(kBiddingUrlPath);
  interest_group_a.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group_a.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group_a);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  blink::AuctionConfig auction_config;
  auction_config.seller = kOriginA;
  auction_config.decision_logic_url = kUrlA.Resolve(kDecisionUrlPath);
  auction_config.non_shared_params.interest_group_buyers = {kOriginA};
  absl::optional<GURL> auction_result = RunAdAuctionAndFlush(auction_config);
  EXPECT_EQ(auction_result, absl::nullopt);

  // Now that the auction has completed, check that the interest group updated.
  task_environment()->RunUntilIdle();

  auto a_groups = GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(a_groups.size(), 1u);
  auto a_group = a_groups[0].interest_group;
  ASSERT_TRUE(a_group.ads.has_value());
  ASSERT_EQ(a_group.ads->size(), 1u);
  EXPECT_EQ(a_group.ads.value()[0].render_url.spec(),
            "https://example.com/new_render");
}

// Like UpdatesInterestGroupsAfterFailedAuction, but the auction fails because
// the decision script can't be loaded. The interest group still updates.
TEST_F(AdAuctionServiceImplTest,
       UpdatesInterestGroupsAfterFailedAuctionMissingScript) {
  constexpr char kMissingScriptPath[] = "/script-not-found.js";
  constexpr char kBiddingScript[] = R"(
function generateBid(
    interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
    browserSignals) {
  return {'ad': 'example', 'bid': 1, 'render': 'https://example.com/render'};
}
)";

  network_responder_->RegisterUpdateResponse(kUpdateUrlPath, R"({
"ads": [{"renderURL": "https://example.com/new_render"
        }]
})");

  network_responder_->RegisterScriptResponse(kBiddingUrlPath, kBiddingScript);
  network_responder_->FailRequestWithError(kMissingScriptPath,
                                           net::ERR_FILE_NOT_FOUND);

  blink::InterestGroup interest_group_a = CreateInterestGroup();
  interest_group_a.update_url = kUpdateUrlA;
  interest_group_a.bidding_url = kUrlA.Resolve(kBiddingUrlPath);
  interest_group_a.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group_a.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group_a);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  blink::AuctionConfig auction_config;
  auction_config.seller = kOriginA;
  auction_config.decision_logic_url = kUrlA.Resolve(kMissingScriptPath);
  auction_config.non_shared_params.interest_group_buyers = {kOriginA};
  absl::optional<GURL> auction_result = RunAdAuctionAndFlush(auction_config);
  EXPECT_EQ(auction_result, absl::nullopt);

  // Now that the auction has completed, check that the interest group updated.
  task_environment()->RunUntilIdle();

  auto a_groups = GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(a_groups.size(), 1u);
  auto a_group = a_groups[0].interest_group;
  ASSERT_TRUE(a_group.ads.has_value());
  ASSERT_EQ(a_group.ads->size(), 1u);
  EXPECT_EQ(a_group.ads.value()[0].render_url.spec(),
            "https://example.com/new_render");
}

// Trigger a post auction update, but block it via ContentBrowserClient policy.
// The update shouldn't happen.
TEST_F(AdAuctionServiceImplTest,
       UpdatesInterestGroupsAfterAuctionBlockedByContentBrowserClient) {
  NavigateAndCommit(kUrlNoUpdate);
  constexpr char kBiddingScript[] = R"(
function generateBid(
    interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
    browserSignals) {
  return {'ad': 'example', 'bid': 1, 'render': 'https://example.com/render'};
}
)";

  constexpr char kDecisionScript[] = R"(
function scoreAd(
    adMetadata, bid, auctionConfig, trustedScoringSignals, browserSignals) {
  return bid;
}
)";

  network_responder_->RegisterUpdateResponse(kUpdateUrlPath, R"({
"ads": [{"renderURL": "https://example.com/new_render"
        }]
})");

  network_responder_->RegisterScriptResponse(kBiddingUrlPath, kBiddingScript);
  network_responder_->RegisterScriptResponse(kDecisionUrlPath, kDecisionScript);

  blink::InterestGroup interest_group_no_update = CreateInterestGroup();
  interest_group_no_update.owner = kOriginNoUpdate;
  interest_group_no_update.update_url = kUpdateUrlNoUpdate;
  interest_group_no_update.bidding_url = kUrlNoUpdate.Resolve(kBiddingUrlPath);
  interest_group_no_update.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group_no_update.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group_no_update);
  EXPECT_EQ(1, GetJoinCount(kOriginNoUpdate, kInterestGroupName));

  blink::AuctionConfig auction_config;
  auction_config.seller = kOriginNoUpdate;
  auction_config.decision_logic_url = kUrlNoUpdate.Resolve(kDecisionUrlPath);
  auction_config.non_shared_params.interest_group_buyers = {kOriginNoUpdate};
  absl::optional<GURL> auction_result = RunAdAuctionAndFlush(auction_config);
  ASSERT_NE(auction_result, absl::nullopt);
  EXPECT_EQ(ConvertFencedFrameURNToURL(*auction_result),
            GURL("https://example.com/render"));

  // Now that the auction has completed, check that the interest group didn't
  // update.
  task_environment()->RunUntilIdle();

  auto no_update_groups = GetInterestGroupsForOwner(kOriginNoUpdate);
  ASSERT_EQ(no_update_groups.size(), 1u);
  auto no_update_group = no_update_groups[0].interest_group;
  ASSERT_TRUE(no_update_group.ads.has_value());
  ASSERT_EQ(no_update_group.ads->size(), 1u);
  EXPECT_EQ(no_update_group.ads.value()[0].render_url.spec(),
            "https://example.com/render");

  // There shouldn't have even been an attempt to update.
  EXPECT_EQ(network_responder_->UpdateCount(), 0u);
}

// Like UpdatesInterestGroupsAfterAuction, but with a component auction.
//
// Create 2 interest groups, each in different origins, A and C (we can't use B
// because AllowInterestGroupContentBrowserClient doesn't allow B interest
// groups to participate in A auctions). Run a component auction where A is a
// buyer in one component auction, and C is a buyer in another component
// auction. A wins.
//
// Both interest groups should be updated after the auction completes.
TEST_F(AdAuctionServiceImplTest,
       UpdatesInterestGroupsAfterComponentAuctionWithWinner) {
  constexpr char kBiddingScript1[] = R"(
function generateBid(
    interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
    browserSignals) {
  return {'ad': 'example', 'bid': 1, 'render': 'https://example.com/render1',
          'allowComponentAuction': true};
}
)";
  constexpr char kBiddingScript2[] = R"(
function generateBid(
    interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
    browserSignals) {
  return {'ad': 'example', 'bid': 2, 'render': 'https://example.com/render2',
          'allowComponentAuction': true};
}
)";

  constexpr char kDecisionScript[] = R"(
function scoreAd(
    adMetadata, bid, auctionConfig, trustedScoringSignals, browserSignals) {
  return {desirability: bid, allowComponentAuction: true};
}
)";

  network_responder_->RegisterUpdateResponse(kUpdateUrlPath, R"({
"ads": [{"renderURL": "https://example.com/new_render"
        }]
})");

  network_responder_->RegisterUpdateResponse(kUpdateUrlPathC, R"({
"ads": [{"renderURL": "https://example.com/new_render"
        }]
})");

  network_responder_->RegisterScriptResponse(kBiddingUrlPath, kBiddingScript1);
  network_responder_->RegisterScriptResponse(kNewBiddingUrlPath,
                                             kBiddingScript2);
  network_responder_->RegisterScriptResponse(kDecisionUrlPath, kDecisionScript);

  blink::InterestGroup interest_group_a = CreateInterestGroup();
  interest_group_a.update_url = kUpdateUrlA;
  interest_group_a.bidding_url = kUrlA.Resolve(kBiddingUrlPath);
  interest_group_a.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render1"),
      /*metadata=*/absl::nullopt);
  interest_group_a.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group_a);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  NavigateAndCommit(kUrlC);
  blink::InterestGroup interest_group_b = CreateInterestGroup();
  interest_group_b.owner = kOriginC;
  interest_group_b.update_url = kUpdateUrlC;
  interest_group_b.bidding_url = kUrlC.Resolve(kNewBiddingUrlPath);
  interest_group_b.ads.emplace();
  ad = blink::InterestGroup::Ad(
      /*render_url=*/GURL("https://example.com/render2"),
      /*metadata=*/absl::nullopt);
  interest_group_b.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group_b);
  EXPECT_EQ(1, GetJoinCount(kOriginC, kInterestGroupName));

  NavigateAndCommit(kUrlA);

  blink::AuctionConfig auction_config;
  auction_config.seller = kOriginA;
  auction_config.decision_logic_url = kUrlA.Resolve(kDecisionUrlPath);

  blink::AuctionConfig component_auction1;
  component_auction1.seller = kOriginA;
  component_auction1.decision_logic_url = kUrlA.Resolve(kDecisionUrlPath);
  component_auction1.non_shared_params.interest_group_buyers = {kOriginA};
  auction_config.non_shared_params.component_auctions.emplace_back(
      std::move(component_auction1));

  blink::AuctionConfig component_auction2;
  component_auction2.seller = kOriginA;
  component_auction2.decision_logic_url = kUrlA.Resolve(kDecisionUrlPath);
  component_auction2.non_shared_params.interest_group_buyers = {kOriginC};
  auction_config.non_shared_params.component_auctions.emplace_back(
      std::move(component_auction2));

  absl::optional<GURL> auction_result = RunAdAuctionAndFlush(auction_config);
  ASSERT_NE(auction_result, absl::nullopt);
  EXPECT_EQ(ConvertFencedFrameURNToURL(*auction_result),
            GURL("https://example.com/render2"));

  // Now that the auction has completed, check that the interest groups updated.
  task_environment()->RunUntilIdle();

  auto a_groups = GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(a_groups.size(), 1u);
  auto a_group = a_groups[0].interest_group;
  ASSERT_TRUE(a_group.ads.has_value());
  ASSERT_EQ(a_group.ads->size(), 1u);
  EXPECT_EQ(a_group.ads.value()[0].render_url.spec(),
            "https://example.com/new_render");

  auto c_groups = GetInterestGroupsForOwner(kOriginC);
  ASSERT_EQ(c_groups.size(), 1u);
  auto c_group = c_groups[0].interest_group;
  ASSERT_TRUE(c_group.ads.has_value());
  ASSERT_EQ(c_group.ads->size(), 1u);
  EXPECT_EQ(c_group.ads.value()[0].render_url.spec(),
            "https://example.com/new_render");
}

// Like UpdatesInterestGroupsAfterComponentAuctionWithWinner, but there's no
// winner, since the decision script scores every bid as 0.
//
// All participating interest groups should still be updated.
TEST_F(AdAuctionServiceImplTest,
       UpdatesInterestGroupsAfterComponentAuctionWithNoWinner) {
  constexpr char kBiddingScript1[] = R"(
function generateBid(
    interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
    browserSignals) {
  return {'ad': 'example', 'bid': 2, 'render': 'https://example.com/render1',
          'allowComponentAuction': true};
}
)";
  constexpr char kBiddingScript2[] = R"(
function generateBid(
    interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
    browserSignals) {
  return {'ad': 'example', 'bid': 1, 'render': 'https://example.com/render2',
          'allowComponentAuction': true};
}
)";

  constexpr char kDecisionScript[] = R"(
function scoreAd(
    adMetadata, bid, auctionConfig, trustedScoringSignals, browserSignals) {
  return {desirability: 0, allowComponentAuction: true};
}
)";

  network_responder_->RegisterUpdateResponse(kUpdateUrlPath, R"({
"ads": [{"renderURL": "https://example.com/new_render"
        }]
})");

  network_responder_->RegisterUpdateResponse(kUpdateUrlPathC, R"({
"ads": [{"renderURL": "https://example.com/new_render"
        }]
})");

  network_responder_->RegisterScriptResponse(kBiddingUrlPath, kBiddingScript1);
  network_responder_->RegisterScriptResponse(kNewBiddingUrlPath,
                                             kBiddingScript2);
  network_responder_->RegisterScriptResponse(kDecisionUrlPath, kDecisionScript);

  blink::InterestGroup interest_group_a = CreateInterestGroup();
  interest_group_a.update_url = kUpdateUrlA;
  interest_group_a.bidding_url = kUrlA.Resolve(kBiddingUrlPath);
  interest_group_a.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render1"),
      /*metadata=*/absl::nullopt);
  interest_group_a.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group_a);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  NavigateAndCommit(kUrlC);
  blink::InterestGroup interest_group_b = CreateInterestGroup();
  interest_group_b.owner = kOriginC;
  interest_group_b.update_url = kUpdateUrlC;
  interest_group_b.bidding_url = kUrlC.Resolve(kNewBiddingUrlPath);
  interest_group_b.ads.emplace();
  ad = blink::InterestGroup::Ad(
      /*render_url=*/GURL("https://example.com/render2"),
      /*metadata=*/absl::nullopt);
  interest_group_b.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group_b);
  EXPECT_EQ(1, GetJoinCount(kOriginC, kInterestGroupName));

  NavigateAndCommit(kUrlA);

  blink::AuctionConfig auction_config;
  auction_config.seller = kOriginA;
  auction_config.decision_logic_url = kUrlA.Resolve(kDecisionUrlPath);

  blink::AuctionConfig component_auction1;
  component_auction1.seller = kOriginA;
  component_auction1.decision_logic_url = kUrlA.Resolve(kDecisionUrlPath);
  component_auction1.non_shared_params.interest_group_buyers = {kOriginA};
  auction_config.non_shared_params.component_auctions.emplace_back(
      std::move(component_auction1));

  blink::AuctionConfig component_auction2;
  component_auction2.seller = kOriginA;
  component_auction2.decision_logic_url = kUrlA.Resolve(kDecisionUrlPath);
  component_auction2.non_shared_params.interest_group_buyers = {kOriginC};
  auction_config.non_shared_params.component_auctions.emplace_back(
      std::move(component_auction2));

  absl::optional<GURL> auction_result = RunAdAuctionAndFlush(auction_config);
  EXPECT_EQ(auction_result, absl::nullopt);

  // Now that the auction has completed, check that the interest groups updated.
  task_environment()->RunUntilIdle();

  auto a_groups = GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(a_groups.size(), 1u);
  auto a_group = a_groups[0].interest_group;
  ASSERT_TRUE(a_group.ads.has_value());
  ASSERT_EQ(a_group.ads->size(), 1u);
  EXPECT_EQ(a_group.ads.value()[0].render_url.spec(),
            "https://example.com/new_render");

  auto c_groups = GetInterestGroupsForOwner(kOriginC);
  ASSERT_EQ(c_groups.size(), 1u);
  auto c_group = c_groups[0].interest_group;
  ASSERT_TRUE(c_group.ads.has_value());
  ASSERT_EQ(c_group.ads->size(), 1u);
  EXPECT_EQ(c_group.ads.value()[0].render_url.spec(),
            "https://example.com/new_render");
}

// Like UpdatesInterestGroupsAfterSuccessfulAuction, but neither the interest
// group nor the update have any ads.
TEST_F(AdAuctionServiceImplTest, UpdatesInterestGroupsAfterAuctionNoAds) {
  network_responder_->RegisterUpdateResponse(
      kUpdateUrlPath, base::StringPrintf(R"({
"trustedBiddingSignalsUrl":
  "%s/interest_group/new_trusted_bidding_signals_url.json",
"trustedBiddingSignalsKeys": ["new_key"]
})",
                                         kOriginStringA));

  blink::InterestGroup interest_group_a = CreateInterestGroup();
  interest_group_a.update_url = kUpdateUrlA;
  interest_group_a.bidding_url = kUrlA.Resolve(kBiddingUrlPath);
  JoinInterestGroupAndFlush(interest_group_a);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  blink::AuctionConfig auction_config;
  auction_config.seller = kOriginA;
  auction_config.decision_logic_url = kUrlA.Resolve(kDecisionUrlPath);
  auction_config.non_shared_params.interest_group_buyers = {kOriginA};
  absl::optional<GURL> auction_result = RunAdAuctionAndFlush(auction_config);
  EXPECT_EQ(auction_result, absl::nullopt);

  // Now that the auction has completed, check that the interest group updated.
  task_environment()->RunUntilIdle();

  auto a_groups = GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(a_groups.size(), 1u);
  auto a_group = a_groups[0].interest_group;
  ASSERT_TRUE(a_group.trusted_bidding_signals_url.has_value());
  EXPECT_EQ(a_group.trusted_bidding_signals_url->spec(),
            base::StringPrintf(
                "%s/interest_group/new_trusted_bidding_signals_url.json",
                kOriginStringA));
  ASSERT_TRUE(a_group.trusted_bidding_signals_keys.has_value());
  EXPECT_EQ(a_group.trusted_bidding_signals_keys->size(), 1u);
  EXPECT_EQ(a_group.trusted_bidding_signals_keys.value()[0], "new_key");
  EXPECT_FALSE(a_group.ads.has_value());
}

TEST_F(AdAuctionServiceImplTest, UpdateSupportsDeprecatedNames) {
  network_responder_->RegisterUpdateResponse(kUpdateUrlPath, R"({
    "ads": [{
        "renderURL": "https://example.com/new_render"
    }],
    "sellerCapabilities": {
        "*": ["interestGroupCounts", "latencyStats"]
    },
    "executionMode": "groupByOrigin"
})");

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.expiry = base::Time::Now() + base::Days(30);
  interest_group.update_url = kUpdateUrlA;
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  std::vector<StorageInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);

  const auto& group = groups[0].interest_group;
  ASSERT_TRUE(group.ads.has_value());
  ASSERT_EQ(group.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            "https://example.com/new_render");
  EXPECT_EQ(group.all_sellers_capabilities,
            blink::SellerCapabilitiesType(
                {blink::SellerCapabilities::kInterestGroupCounts,
                 blink::SellerCapabilities::kLatencyStats}));
  EXPECT_EQ(group.execution_mode,
            blink::InterestGroup::ExecutionMode::kGroupedByOriginMode);
}

TEST_F(AdAuctionServiceImplTest, UpdateIgnoresUnknownEnumFields) {
  network_responder_->RegisterUpdateResponse(kUpdateUrlPath, R"({
    "ads": [{
        "renderURL": "https://example.com/new_render"
    }],
    "sellerCapabilities": {
        "https://example.test": ["non-valid-capability"]
    },
    "executionMode": "non-valid-execution-mode"
})");

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.expiry = base::Time::Now() + base::Days(30);
  interest_group.update_url = kUpdateUrlA;
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  std::vector<StorageInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);

  // The unknown enum values are ignored, and renderURL is updated.
  const auto& group = groups[0].interest_group;
  ASSERT_TRUE(group.ads.has_value());
  ASSERT_EQ(group.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            "https://example.com/new_render");
}

TEST_F(AdAuctionServiceImplTest, UpdateRenamedFields) {
  blink::InterestGroup initial_interest_group = CreateInterestGroup();
  initial_interest_group.update_url = kUpdateUrlA;
  JoinInterestGroupAndFlush(initial_interest_group);

  // Update priority (which is *not* being renamed) in addition to target field
  // to update -- this way, if the update fails, the test can observe that
  // priority wasn't updated.

  blink::InterestGroup updated_interest_group_ads = CreateInterestGroup();
  updated_interest_group_ads.update_url = kUpdateUrlA;
  updated_interest_group_ads.priority = 1.0;
  updated_interest_group_ads.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  updated_interest_group_ads.ads->emplace_back(std::move(ad));

  blink::InterestGroup updated_interest_group_ad_components =
      CreateInterestGroup();
  updated_interest_group_ad_components.update_url = kUpdateUrlA;
  updated_interest_group_ad_components.priority = 1.0;
  updated_interest_group_ad_components.ad_components.emplace();
  blink::InterestGroup::Ad ad_component(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  updated_interest_group_ad_components.ad_components->emplace_back(
      std::move(ad_component));

  blink::InterestGroup updated_interest_group_bidding_logic_url =
      CreateInterestGroup();
  updated_interest_group_bidding_logic_url.update_url = kUpdateUrlA;
  updated_interest_group_bidding_logic_url.priority = 1.0;
  updated_interest_group_bidding_logic_url.bidding_url =
      GURL(base::StringPrintf("%s/bidding.js", kOriginStringA));

  blink::InterestGroup updated_interest_group_bidding_wasm_helper_url =
      CreateInterestGroup();
  updated_interest_group_bidding_wasm_helper_url.update_url = kUpdateUrlA;
  updated_interest_group_bidding_wasm_helper_url.priority = 1.0;
  updated_interest_group_bidding_wasm_helper_url.bidding_wasm_helper_url =
      GURL(base::StringPrintf("%s/bidding.wasm", kOriginStringA));

  blink::InterestGroup updated_interest_group_trusted_bidding_signals_url =
      CreateInterestGroup();
  updated_interest_group_trusted_bidding_signals_url.update_url = kUpdateUrlA;
  updated_interest_group_trusted_bidding_signals_url.priority = 1.0;
  updated_interest_group_trusted_bidding_signals_url
      .trusted_bidding_signals_url =
      GURL(base::StringPrintf("%s/signals.json", kOriginStringA));

  struct TestCase {
    const std::string update_contents;
    const blink::InterestGroup& expected_group;
  } kTestCases[] = {
      // ***
      // ads renderURL
      // ***
      {R"("ads": [{"renderUrl": "https://example.com/render"}])",
       updated_interest_group_ads},
      {R"("ads": [{"renderUrl": "https://example.com/render",
                 "renderURL": "https://example.com/render"}])",
       updated_interest_group_ads},
      {R"("ads": [{"renderUrl": "https://example.com/render",
                 "renderURL": "https://example.com/render2"}])",
       initial_interest_group},
      {R"("ads": [{}])", initial_interest_group},
      // ***
      // adComponents renderURL
      // ***
      {R"("adComponents": [{"renderUrl": "https://example.com/render"}])",
       updated_interest_group_ad_components},
      {R"("adComponents": [{"renderUrl": "https://example.com/render",
                 "renderURL": "https://example.com/render"}])",
       updated_interest_group_ad_components},
      {R"("adComponents": [{"renderUrl": "https://example.com/render",
                 "renderURL": "https://example.com/render2"}])",
       initial_interest_group},
      {R"("adComponents": [{}])", initial_interest_group},
      // ***
      // biddingLogicURL
      // ***
      {base::StringPrintf(R"("biddingLogicUrl": "%s/bidding.js")",
                          kOriginStringA),
       updated_interest_group_bidding_logic_url},
      {base::StringPrintf(R"("biddingLogicURL": "%s/bidding.js")",
                          kOriginStringA),
       updated_interest_group_bidding_logic_url},
      {base::StringPrintf(R"("biddingLogicUrl": "%s/bidding.js",)"
                          R"("biddingLogicURL": "%s/bidding.js")",
                          kOriginStringA, kOriginStringA),
       updated_interest_group_bidding_logic_url},
      {base::StringPrintf(R"("biddingLogicUrl": "%s/bidding.js",)"
                          R"("biddingLogicURL": "%s/bidding2.js")",
                          kOriginStringA, kOriginStringA),
       initial_interest_group},
      // ***
      // biddingWasmHelperURL
      // ***
      {base::StringPrintf(R"("biddingWasmHelperUrl": "%s/bidding.wasm")",
                          kOriginStringA),
       updated_interest_group_bidding_wasm_helper_url},
      {base::StringPrintf(R"("biddingWasmHelperURL": "%s/bidding.wasm")",
                          kOriginStringA),
       updated_interest_group_bidding_wasm_helper_url},
      {base::StringPrintf(R"("biddingWasmHelperUrl": "%s/bidding.wasm",)"
                          R"("biddingWasmHelperURL": "%s/bidding.wasm")",
                          kOriginStringA, kOriginStringA),
       updated_interest_group_bidding_wasm_helper_url},
      {base::StringPrintf(R"("biddingWasmHelperUrl": "%s/bidding.wasm",)"
                          R"("biddingWasmHelperURL": "%s/bidding2.wasm")",
                          kOriginStringA, kOriginStringA),
       initial_interest_group},
      // ***
      // trustedBiddingSignalsURL
      // ***
      {base::StringPrintf(R"("trustedBiddingSignalsUrl": "%s/signals.json")",
                          kOriginStringA),
       updated_interest_group_trusted_bidding_signals_url},
      {base::StringPrintf(R"("trustedBiddingSignalsURL": "%s/signals.json")",
                          kOriginStringA),
       updated_interest_group_trusted_bidding_signals_url},
      {base::StringPrintf(R"("trustedBiddingSignalsUrl": "%s/signals.json",)"
                          R"("trustedBiddingSignalsURL": "%s/signals.json")",
                          kOriginStringA, kOriginStringA),
       updated_interest_group_trusted_bidding_signals_url},
      {base::StringPrintf(R"("trustedBiddingSignalsUrl": "%s/signals.json",)"
                          R"("trustedBiddingSignalsURL": "%s/signals2.json")",
                          kOriginStringA, kOriginStringA),
       initial_interest_group},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.update_contents);

    network_responder_->RegisterUpdateResponse(
        kUpdateUrlPath, base::StringPrintf(R"({
    "priority": 1.0,
    %s
})",
                                           test_case.update_contents.c_str()));
    UpdateInterestGroupNoFlush();
    task_environment()->RunUntilIdle();

    std::vector<StorageInterestGroup> groups =
        GetInterestGroupsForOwner(kOriginA);
    ASSERT_EQ(groups.size(), 1u);
    EXPECT_TRUE(
        groups[0].interest_group.IsEqualForTesting(test_case.expected_group));

    // Reset for the next iteration.
    JoinInterestGroupAndFlush(initial_interest_group);
  }
}

// When sending reports, the next report request is feteched after the previous
// report request completed (`max_active_report_requests_` is set to 1 in this
// test). Reporting should continue even after the page navigated away. Timeout
// works for report requests.
TEST_F(AdAuctionServiceImplTest, SendReports) {
  manager_->set_reporting_interval_for_testing(base::Seconds(5));
  manager_->set_max_active_report_requests_for_testing(1);
  network_responder_->RegisterScriptResponse(kBiddingUrlPath,
                                             BasicBiddingReportScript());
  network_responder_->RegisterScriptResponse(kDecisionUrlPath,
                                             BasicSellerReportScript());
  network_responder_->RegisterReportResponse("/report_bidder", /*response=*/"");
  network_responder_->RegisterStoreUrlLoaderClient("/report_seller");

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.bidding_url = kUrlA.Resolve(kBiddingUrlPath);
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  blink::AuctionConfig auction_config;
  auction_config.seller = kOriginA;
  auction_config.decision_logic_url = kUrlA.Resolve(kDecisionUrlPath);
  auction_config.non_shared_params.interest_group_buyers = {kOriginA};
  absl::optional<GURL> auction_result = RunAdAuctionAndFlush(auction_config);
  ASSERT_NE(auction_result, absl::nullopt);
  InvokeCallbackForURN(*auction_result);

  task_environment()->FastForwardBy(base::Seconds(30) - base::Seconds(1));
  // There should only be the seller report, and the bidder report request is
  // not fetched because the seller report request hangs and didn't finish yet.
  EXPECT_EQ(network_responder_->ReportCount(), 1u);
  // The request to seller's report url should hang before 30s.
  EXPECT_TRUE(network_responder_->RemoteIsConnected());
  task_environment()->FastForwardBy(base::Seconds(2));
  // The request to seller report url should be disconnected after 30s due to
  // timeout.
  EXPECT_FALSE(network_responder_->RemoteIsConnected());
  // Reporting should continue even after the page navigated away.
  NavigateAndCommit(kUrlB);
  // Navigating away normally deletes the AdAuctionServiceImpl. With this test
  // fixture, however, the frame doesn't own the service so have to delete it
  // manually.
  DestroyAdAuctionService();

  task_environment()->FastForwardBy(base::Seconds(1));
  // The next request will not be sent when it's less than 5 seconds after the
  // previous request completed.
  EXPECT_EQ(network_responder_->ReportCount(), 1u);
  task_environment()->FastForwardBy(base::Seconds(4));
  // There should be two reports in total now, since the seller's report request
  // completed (timed out) and then the bidder's report request was also fetched
  // after 5 seconds since then.
  EXPECT_EQ(network_responder_->ReportCount(), 2u);
}

// Check that reports aren't sent until the URN to URL callback is invoked.
TEST_F(AdAuctionServiceImplTest, SendReportsWaitsForCallback) {
  network_responder_->RegisterScriptResponse(kBiddingUrlPath,
                                             BasicBiddingReportScript());
  network_responder_->RegisterScriptResponse(kDecisionUrlPath,
                                             BasicSellerReportScript());
  network_responder_->RegisterReportResponse("/report_bidder", /*response=*/"");
  network_responder_->RegisterReportResponse("/report_seller", /*response=*/"");

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.bidding_url = kUrlA.Resolve(kBiddingUrlPath);
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  blink::AuctionConfig auction_config;
  auction_config.seller = kOriginA;
  auction_config.decision_logic_url = kUrlA.Resolve(kDecisionUrlPath);
  auction_config.non_shared_params.interest_group_buyers = {kOriginA};
  absl::optional<GURL> auction_result = RunAdAuctionAndFlush(auction_config);
  ASSERT_NE(auction_result, absl::nullopt);

  // Nothing should happen until the URN's callback is invoked.
  task_environment()->FastForwardBy(base::Days(1));
  EXPECT_EQ(network_responder_->ReportCount(), 0u);

  InvokeCallbackForURN(*auction_result);
  network_responder_->WaitForNumReports(2);
}

// Make sure the report-sending logic can handle two auctions with a delay
// between them. This is regression test for https://crbug.com/1379234.
TEST_F(AdAuctionServiceImplTest, SendReportsTwoAuctionsWithDelay) {
  network_responder_->RegisterScriptResponse(kBiddingUrlPath,
                                             BasicBiddingReportScript());
  network_responder_->RegisterScriptResponse(kDecisionUrlPath,
                                             BasicSellerReportScript());
  network_responder_->RegisterReportResponse("/report_bidder", /*response=*/"");
  network_responder_->RegisterReportResponse("/report_seller", /*response=*/"");

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.expiry = base::Time::Now() + base::Days(2);
  interest_group.bidding_url = kUrlA.Resolve(kBiddingUrlPath);
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  blink::AuctionConfig auction_config;
  auction_config.seller = kOriginA;
  auction_config.decision_logic_url = kUrlA.Resolve(kDecisionUrlPath);
  auction_config.non_shared_params.interest_group_buyers = {kOriginA};
  absl::optional<GURL> auction_result = RunAdAuctionAndFlush(auction_config);
  ASSERT_NE(auction_result, absl::nullopt);
  InvokeCallbackForURN(*auction_result);
  network_responder_->WaitForNumReports(2u);

  // Chrome runs for a day.
  task_environment()->FastForwardBy(base::Days(1));
  // Should have been no other pending reports.
  EXPECT_EQ(network_responder_->ReportCount(), 2u);

  // Re-running the auction should result in 2 more reports.
  auction_result = RunAdAuctionAndFlush(auction_config);
  ASSERT_NE(auction_result, absl::nullopt);
  InvokeCallbackForURN(*auction_result);
  network_responder_->WaitForNumReports(4u);
}

// Test that if one auction completes after another's reports have been sent,
// but before the report interval has elapsed, its requests still respect the
// report interval.
TEST_F(AdAuctionServiceImplTest, SendReportsTwoAuctionsRespectsReportInterval) {
  manager_->set_reporting_interval_for_testing(base::Seconds(5));
  manager_->set_max_active_report_requests_for_testing(1);
  network_responder_->RegisterScriptResponse(kBiddingUrlPath,
                                             BasicBiddingReportScript());
  network_responder_->RegisterScriptResponse(kDecisionUrlPath,
                                             BasicSellerReportScript());
  network_responder_->RegisterReportResponse("/report_bidder", /*response=*/"");
  network_responder_->RegisterReportResponse("/report_seller", /*response=*/"");

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.expiry = base::Time::Now() + base::Days(2);
  interest_group.bidding_url = kUrlA.Resolve(kBiddingUrlPath);
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  base::Time start_time = base::Time::Now();

  blink::AuctionConfig auction_config;
  auction_config.seller = kOriginA;
  auction_config.decision_logic_url = kUrlA.Resolve(kDecisionUrlPath);
  auction_config.non_shared_params.interest_group_buyers = {kOriginA};
  absl::optional<GURL> auction_result = RunAdAuctionAndFlush(auction_config);
  EXPECT_NE(auction_result, absl::nullopt);
  InvokeCallbackForURN(*auction_result);

  // First report should be sent immediately.
  network_responder_->WaitForNumReports(1u);
  EXPECT_EQ(base::TimeDelta(), base::Time::Now() - start_time);
  // Second report should be sent after the reporting interval.
  network_responder_->WaitForNumReports(2u);
  EXPECT_EQ(base::Seconds(5), base::Time::Now() - start_time);

  // Time passes that's less than the reporting interval. This shouldn't have
  // any effect on when the next report is sent. That is, the reporting interval
  // for the next report starts when the previous report was sent, not when the
  // next report is queued.
  task_environment()->FastForwardBy(base::Seconds(2));

  // Re-running the auction should result in 2 more reports.
  auction_result = RunAdAuctionAndFlush(auction_config);
  EXPECT_NE(auction_result, absl::nullopt);
  InvokeCallbackForURN(*auction_result);

  // The third report (first report for the second auction) should be only be
  // sent after the reporting interval has elapsed again, starting from when the
  // second report was sent.
  network_responder_->WaitForNumReports(3u);
  EXPECT_EQ(base::Seconds(10), base::Time::Now() - start_time);
  // Second report of the second auction should wait for yet another reporting
  // interval.
  network_responder_->WaitForNumReports(4u);
  EXPECT_EQ(base::Seconds(15), base::Time::Now() - start_time);
}

// Similar to SendReports() above, but with one report request failed instead of
// timed out. Following report requests should still be send after previous ones
// failed.
TEST_F(AdAuctionServiceImplTest, SendReportsOneReportFailed) {
  manager_->set_reporting_interval_for_testing(base::Seconds(5));
  manager_->set_max_active_report_requests_for_testing(1);
  network_responder_->RegisterScriptResponse(kBiddingUrlPath,
                                             BasicBiddingReportScript());
  network_responder_->RegisterScriptResponse(kDecisionUrlPath,
                                             BasicSellerReportScript());
  network_responder_->RegisterReportResponse("/report_bidder", /*response=*/"");
  network_responder_->FailRequestWithError("/report_seller",
                                           net::ERR_CONNECTION_FAILED);

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.bidding_url = kUrlA.Resolve(kBiddingUrlPath);
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  blink::AuctionConfig auction_config;
  auction_config.seller = kOriginA;
  auction_config.decision_logic_url = kUrlA.Resolve(kDecisionUrlPath);
  auction_config.non_shared_params.interest_group_buyers = {kOriginA};
  absl::optional<GURL> auction_result = RunAdAuctionAndFlush(auction_config);
  ASSERT_NE(auction_result, absl::nullopt);
  InvokeCallbackForURN(*auction_result);

  // There should be no report since the seller report failed, and the bidder
  // report request is not fetched yet.
  EXPECT_EQ(network_responder_->ReportCount(), 0u);

  task_environment()->FastForwardBy(base::Seconds(2));
  // The next request will not be sent when it's less than 5 seconds after the
  // previous request completed.
  EXPECT_EQ(network_responder_->ReportCount(), 0u);
  task_environment()->FastForwardBy(base::Seconds(4));
  // The bidder's report request was fetched after 5 seconds since the previous
  // request completed.
  EXPECT_EQ(network_responder_->ReportCount(), 1u);
}

// Checks that all reporting in the pending queue gets canceled if the reporting
// queue max length is exceeded at the time of enqueuing a new set of reports.
TEST_F(AdAuctionServiceImplTest, ReportQueueMaxLength) {
  // Use interest group name as bid value.
  const std::string kBiddingScript = base::StringPrintf(R"(
function generateBid(
    interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
    browserSignals) {
  return {
    'ad': 'example',
    'bid': parseInt(interestGroup.name),
    'render': 'https://example.com/render'
  };
}
function reportWin(
    auctionSignals, perBuyerSignals, sellerSignals, browserSignals) {
  sendReportTo('%s/report_bidder_' + browserSignals.bid);
}
  )",
                                                        kOriginStringA);

  const std::string kDecisionScript =
      base::StringPrintf(R"(
function scoreAd(
    adMetadata, bid, auctionConfig, trustedScoringSignals, browserSignals) {
  return bid;
}
function reportResult(auctionConfig, browserSignals) {
  sendReportTo('%s/report_seller_' + browserSignals.bid);
  return {
    'success': true,
    'signalsForWinner': {'signalForWinner': 1},
    'reportUrl': '%s/report_seller_' + browserSignals.bid,
  };
}
)",
                         kOriginStringA, kOriginStringA);

  // Set the global maximum queue length to the number of reports each auction
  // run below tries to make (which is 2), so the second report made by each
  // auction won't evict the first request.
  //
  // Use 3 instead of 2 because the queue is truncated when the "max" is hit by
  // appending a report, as opposed to when the "max" is exceeded.
  manager_->set_max_report_queue_length_for_testing(3);
  manager_->set_max_active_report_requests_for_testing(1);
  manager_->set_reporting_interval_for_testing(base::Seconds(5));
  network_responder_->RegisterScriptResponse(kBiddingUrlPath, kBiddingScript);
  network_responder_->RegisterScriptResponse(kDecisionUrlPath, kDecisionScript);

  // Run three auctions, each time with a new interest group which bids i wins
  // the auction.
  for (int i = 1; i < 4; i++) {
    const std::string name = base::NumberToString(i);
    network_responder_->RegisterReportResponse(
        base::StringPrintf("/report_bidder_%s", name.c_str()), "");
    network_responder_->RegisterReportResponse(
        base::StringPrintf("/report_seller_%s", name.c_str()), "");
    blink::InterestGroup interest_group = CreateInterestGroup();
    interest_group.bidding_url = kUrlA.Resolve(kBiddingUrlPath);
    interest_group.name = name;
    interest_group.ads.emplace();
    blink::InterestGroup::Ad ad(
        /*render_url=*/GURL("https://example.com/render"),
        /*metadata=*/absl::nullopt);
    interest_group.ads->emplace_back(std::move(ad));
    JoinInterestGroupAndFlush(interest_group);
    EXPECT_EQ(1, GetJoinCount(kOriginA, name));

    blink::AuctionConfig auction_config;
    auction_config.seller = kOriginA;

    auction_config.decision_logic_url = kUrlA.Resolve(kDecisionUrlPath);
    auction_config.non_shared_params.interest_group_buyers = {kOriginA};
    absl::optional<GURL> auction_result = RunAdAuctionAndFlush(auction_config);
    ASSERT_NE(auction_result, absl::nullopt);
    InvokeCallbackForURN(*auction_result);
    // Wait for the reporting scripts to complete and reporting URLs to be
    // requested. Need to do this for each auction to make sure reporting
    // scripts complete and requests are made in a consistent order.
    task_environment()->RunUntilIdle();
  }

  // There should be one report sent already, since there's no delay for the
  // first report.
  EXPECT_EQ(network_responder_->ReportCount(), 1u);
  EXPECT_TRUE(network_responder_->ReportSent("/report_seller_1"));
  // Fastforward enough time for all expected reports to be sent.
  task_environment()->FastForwardBy(base::Seconds(60));
  // Two more reports were sent.
  EXPECT_EQ(network_responder_->ReportCount(), 3u);

  // The last (third) auction's reports should be sent successfully.
  EXPECT_TRUE(network_responder_->ReportSent("/report_bidder_3"));
  EXPECT_TRUE(network_responder_->ReportSent("/report_seller_3"));
  // The first auction's second report and the second auction's reports should
  // be dropped and not be sent.
  EXPECT_FALSE(network_responder_->ReportSent("/report_bidder_1"));
  EXPECT_FALSE(network_responder_->ReportSent("/report_bidder_2"));
  EXPECT_FALSE(network_responder_->ReportSent("/report_seller_2"));
}

TEST_F(AdAuctionServiceImplTest, SendReportsMaxReportRoundDuration) {
  // `max_reporting_round_duration_` is set lower than `reporting_interval_` so
  // that we can exceed the max round duration with pending unsent reports.
  manager_->set_reporting_interval_for_testing(base::Seconds(5));
  manager_->set_max_reporting_round_duration_for_testing(base::Seconds(1));
  manager_->set_max_active_report_requests_for_testing(1);

  network_responder_->RegisterScriptResponse(kBiddingUrlPath,
                                             BasicBiddingReportScript());
  network_responder_->RegisterScriptResponse(kDecisionUrlPath,
                                             BasicSellerReportScript());
  network_responder_->RegisterReportResponse("/report_bidder", /*response=*/"");
  network_responder_->RegisterReportResponse("/report_seller", /*response=*/"");

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.bidding_url = kUrlA.Resolve(kBiddingUrlPath);
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  blink::AuctionConfig auction_config;
  auction_config.seller = kOriginA;
  auction_config.decision_logic_url = kUrlA.Resolve(kDecisionUrlPath);
  auction_config.non_shared_params.interest_group_buyers = {kOriginA};
  absl::optional<GURL> auction_result = RunAdAuctionAndFlush(auction_config);
  ASSERT_NE(auction_result, absl::nullopt);
  InvokeCallbackForURN(*auction_result);
  // Wait for the reporting scripts to complete and all reporting URLs to be
  // requested.
  task_environment()->RunUntilIdle();

  // Wait for the seller report to be sent.
  network_responder_->WaitForNumReports(1);
  // The bidder report request should still be waiting in the report queue.
  EXPECT_EQ(manager_->report_queue_length_for_testing(), 1u);

  // Run a second auction while the first auction's reporting is in progress.
  blink::AuctionConfig auction_config2;
  auction_config2.seller = kOriginA;
  auction_config2.decision_logic_url = kUrlA.Resolve(kDecisionUrlPath);
  auction_config2.non_shared_params.interest_group_buyers = {kOriginA};
  auction_result = RunAdAuctionAndFlush(auction_config2);
  ASSERT_NE(auction_result, absl::nullopt);
  InvokeCallbackForURN(*auction_result);
  // Wait for the reporting scripts to complete and all reporting URLs to be
  // requested.
  task_environment()->RunUntilIdle();
  // Two more reports are enqueued.
  EXPECT_EQ(manager_->report_queue_length_for_testing(), 3u);

  task_environment()->FastForwardBy(base::Seconds(20));
  // Should still only have 1 report sent because the report queue is cleared
  // after `max_reporting_round_duration` passed, before popping the second
  // report from the queue and send it.
  EXPECT_EQ(network_responder_->ReportCount(), 1u);
  EXPECT_EQ(manager_->report_queue_length_for_testing(), 0u);

  // Set `max_reporting_round_duration_` high enough so that the auction's two
  // reports can be sent successfully.
  manager_->set_max_reporting_round_duration_for_testing(base::Seconds(20));

  // Run a third auction after report queue is cleared, to make sure further
  // auction's reports can be normally enqueued and sent again.
  blink::AuctionConfig auction_config3;
  auction_config3.seller = kOriginA;
  auction_config3.decision_logic_url = kUrlA.Resolve(kDecisionUrlPath);
  auction_config3.non_shared_params.interest_group_buyers = {kOriginA};
  auction_result = RunAdAuctionAndFlush(auction_config3);
  ASSERT_NE(auction_result, absl::nullopt);
  InvokeCallbackForURN(*auction_result);
  // Wait for the reporting scripts to complete and all reporting URLs to be
  // requested.
  task_environment()->RunUntilIdle();

  task_environment()->FastForwardBy(base::Seconds(20));
  // Two more reports from the third auction are sent.
  EXPECT_EQ(network_responder_->ReportCount(), 3u);
}

// Check that running reporting worklets doesn't block auction completion. To do
// this, the bidding script is set to be deferred. The auction is started, and
// the bid script is supplied. Then the auction completes. This should trigger
// reloading the bidding script to call reportWin(). The second time, a bidding
// script is not supplied. The fact that the auction completes despite the
// second stalled load verifies that running reporting scripts does not block
// completion of an auction. The AdAuctionServiceImpl is destroyed before
// the bidding URL is downloaded the second time, which provides some test
// coverage of that as well.
TEST_F(AdAuctionServiceImplTest, ReportingWorkletsDoNotBlockCompletion) {
  network_responder_->RegisterDeferredScriptResponse(kBiddingUrlPath);
  network_responder_->RegisterScriptResponse(kDecisionUrlPath,
                                             BasicSellerReportScript());
  network_responder_->RegisterReportResponse("/report_bidder", /*response=*/"");
  network_responder_->RegisterReportResponse("/report_seller", /*response=*/"");

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.bidding_url = kUrlA.Resolve(kBiddingUrlPath);
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));

  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  blink::AuctionConfig auction_config;
  auction_config.seller = kOriginA;
  auction_config.decision_logic_url = kUrlA.Resolve(kDecisionUrlPath);
  auction_config.non_shared_params.interest_group_buyers = {kOriginA};

  AdAuctionServiceImpl::CreateMojoService(
      main_rfh(), ad_auction_service_.BindNewPipeAndPassReceiver());

  // Start the auction.
  base::RunLoop run_loop;
  absl::optional<blink::FencedFrame::RedactedFencedFrameConfig> maybe_config;
  ad_auction_service_->RunAdAuction(
      auction_config, mojo::NullReceiver(),
      base::BindLambdaForTesting(
          [&run_loop, &maybe_config](
              bool manually_aborted,
              const absl::optional<
                  blink::FencedFrame::RedactedFencedFrameConfig>& config) {
            EXPECT_FALSE(manually_aborted);
            maybe_config = config;
            run_loop.Quit();
          }));

  // Wait for the NetworkResponder to see the request for the bidding URL, and
  // response with the bidding script.
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(run_loop.AnyQuitCalled());
  network_responder_->DoDeferredScriptResponse(kBiddingUrlPath,
                                               BasicBiddingReportScript());
  // Register another deferred response for when the bidding URL is requested
  // again to run the reporting script.
  network_responder_->RegisterDeferredScriptResponse(kBiddingUrlPath);

  // Complete the auction. It should have a winning ad.
  run_loop.Run();
  ASSERT_TRUE(maybe_config);
  EXPECT_TRUE(maybe_config->urn_uuid().has_value());

  // Running until idle should result in the NetworkResponder receiving a
  // request for the bidding URL, to run the reporting script.
  task_environment()->RunUntilIdle();
  EXPECT_TRUE(network_responder_->HasPendingResponse(kBiddingUrlPath));

  // Destroy the auction service Mojo pipe, and wait for the underlying service
  // to be destroyed, which should not cause a crash.
  DestroyAdAuctionService();
  base::RunLoop().RunUntilIdle();
}

// Run several auctions, some of which have a winner, and some of which do
// not. Verify that the auction result UMA is recorded correctly.
TEST_F(AdAuctionServiceImplTest,
       AddInterestGroupRunAuctionVerifyResultMetrics) {
  base::HistogramTester histogram_tester;
  constexpr char kDecisionFailAllUrlPath[] =
      "/interest_group/decision_logic_fail_all.js";

  constexpr char kBiddingScript[] = R"(
function generateBid(
    interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
    browserSignals) {
  return {'ad': 'example', 'bid': 1, 'render': 'https://example.com/render'};
}
function reportWin() {}
)";

  constexpr char kDecisionScript[] = R"(
function scoreAd(
    adMetadata, bid, auctionConfig, trustedScoringSignals, browserSignals) {
  return bid;
}
function reportResult() {}
)";

  constexpr char kDecisionScriptFailAll[] = R"(
function scoreAd(
    adMetadata, bid, auctionConfig, trustedScoringSignals, browserSignals) {
  return 0;
}
function reportResult() {}
)";

  network_responder_->RegisterScriptResponse(kBiddingUrlPath, kBiddingScript);
  network_responder_->RegisterScriptResponse(kDecisionUrlPath, kDecisionScript);
  network_responder_->RegisterScriptResponse(kDecisionFailAllUrlPath,
                                             kDecisionScriptFailAll);

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.expiry = base::Time::Now() + base::Days(10);
  interest_group.bidding_url = kUrlA.Resolve(kBiddingUrlPath);
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  // Run 7 auctions, with delays:
  //
  // succeed, (1s), fail, (3s), succeed, (1m), succeed, (10m) succeed, (30m)
  // fail, (1h), fail, which in bits (with an extra leading 1) is 0b1101110 --
  // the last failure isn't recorded in the bitfield, since only the first 6
  // auctions get recorded in the bitfield.

  // Expect*TimeSample() doesn't accept base::TimeDelta::Max(), but the max time
  // bucket size is 1 hour, so specifying kMaxTime will select the max bucket.
  constexpr base::TimeDelta kMaxTime{base::Days(1)};

  blink::AuctionConfig succeed_auction_config;
  succeed_auction_config.seller = kOriginA;
  succeed_auction_config.decision_logic_url = kUrlA.Resolve(kDecisionUrlPath);
  succeed_auction_config.non_shared_params.interest_group_buyers = {kOriginA};

  blink::AuctionConfig fail_auction_config;
  fail_auction_config.seller = kOriginA;
  fail_auction_config.decision_logic_url =
      kUrlA.Resolve(kDecisionFailAllUrlPath);
  fail_auction_config.non_shared_params.interest_group_buyers = {kOriginA};

  // 1st auction
  EXPECT_NE(RunAdAuctionAndFlush(succeed_auction_config), absl::nullopt);
  // Time metrics are published every auction.
  histogram_tester.ExpectUniqueTimeSample(
      "Ads.InterestGroup.Auction.TimeSinceLastAuctionPerPage", kMaxTime, 1);

  // 2nd auction
  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_EQ(RunAdAuctionAndFlush(fail_auction_config), absl::nullopt);
  histogram_tester.ExpectTimeBucketCount(
      "Ads.InterestGroup.Auction.TimeSinceLastAuctionPerPage", base::Seconds(1),
      1);

  // 3rd auction
  task_environment()->FastForwardBy(base::Seconds(3));
  EXPECT_NE(RunAdAuctionAndFlush(succeed_auction_config), absl::nullopt);
  histogram_tester.ExpectTimeBucketCount(
      "Ads.InterestGroup.Auction.TimeSinceLastAuctionPerPage", base::Seconds(3),
      1);

  // 4th auction
  task_environment()->FastForwardBy(base::Minutes(1));
  EXPECT_NE(RunAdAuctionAndFlush(succeed_auction_config), absl::nullopt);
  histogram_tester.ExpectTimeBucketCount(
      "Ads.InterestGroup.Auction.TimeSinceLastAuctionPerPage", base::Minutes(1),
      1);

  // 5th auction
  task_environment()->FastForwardBy(base::Minutes(10));
  EXPECT_NE(RunAdAuctionAndFlush(succeed_auction_config), absl::nullopt);
  histogram_tester.ExpectTimeBucketCount(
      "Ads.InterestGroup.Auction.TimeSinceLastAuctionPerPage",
      base::Minutes(10), 1);

  // 6th auction
  task_environment()->FastForwardBy(base::Minutes(30));
  EXPECT_EQ(RunAdAuctionAndFlush(fail_auction_config), absl::nullopt);
  histogram_tester.ExpectTimeBucketCount(
      "Ads.InterestGroup.Auction.TimeSinceLastAuctionPerPage",
      base::Minutes(30), 1);

  // 7th auction
  task_environment()->FastForwardBy(base::Hours(1));
  EXPECT_EQ(RunAdAuctionAndFlush(fail_auction_config), absl::nullopt);
  // Since the 1st auction has no prior auction -- it gets put in the same
  // bucket with the 7th auction -- there are 2 auctions now in this bucket.
  histogram_tester.ExpectTimeBucketCount(
      "Ads.InterestGroup.Auction.TimeSinceLastAuctionPerPage", kMaxTime, 2);

  // Some metrics only get reported until after navigation.
  EXPECT_EQ(histogram_tester
                .GetAllSamples("Ads.InterestGroup.Auction.NumAuctionsPerPage")
                .size(),
            0u);
  EXPECT_EQ(
      histogram_tester
          .GetAllSamples(
              "Ads.InterestGroup.Auction.PercentAuctionsSuccessfulPerPage")
          .size(),
      0u);
  EXPECT_EQ(
      histogram_tester
          .GetAllSamples("Ads.InterestGroup.Auction.First6AuctionsBitsPerPage")
          .size(),
      0u);
  EXPECT_EQ(
      histogram_tester
          .GetAllSamples(
              "Ads.InterestGroup.Auction.NumAuctionsSkippedDueToAuctionLimit")
          .size(),
      0u);

  // DeleteContents() to force-populate remaining metrics.
  DeleteContents();

  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.Auction.NumAuctionsPerPage", 7, 1);
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.Auction.PercentAuctionsSuccessfulPerPage", 4 * 100 / 7,
      1);
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.Auction.First6AuctionsBitsPerPage", 0b1101110, 1);
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.Auction.NumAuctionsSkippedDueToAuctionLimit", 0, 1);
}

// Like AddInterestGroupRunAuctionVerifyResultMetrics, but with a smaller number
// of auctions -- this verifies that metrics (especially the bit metrics) are
// reported correctly in this scenario.
TEST_F(AdAuctionServiceImplTest,
       AddInterestGroupRunAuctionVerifyResultMetricsFewAuctions) {
  base::HistogramTester histogram_tester;
  constexpr char kDecisionFailAllUrlPath[] =
      "/interest_group/decision_logic_fail_all.js";

  constexpr char kBiddingScript[] = R"(
function generateBid(
    interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
    browserSignals) {
  return {'ad': 'example', 'bid': 1, 'render': 'https://example.com/render'};
}
function reportWin() {}
)";

  constexpr char kDecisionScript[] = R"(
function scoreAd(
    adMetadata, bid, auctionConfig, trustedScoringSignals, browserSignals) {
  return bid;
}
function reportResult() {}
)";

  constexpr char kDecisionScriptFailAll[] = R"(
function scoreAd(
    adMetadata, bid, auctionConfig, trustedScoringSignals, browserSignals) {
  return 0;
}
function reportResult() {}
)";

  network_responder_->RegisterScriptResponse(kBiddingUrlPath, kBiddingScript);
  network_responder_->RegisterScriptResponse(kDecisionUrlPath, kDecisionScript);
  network_responder_->RegisterScriptResponse(kDecisionFailAllUrlPath,
                                             kDecisionScriptFailAll);

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.expiry = base::Time::Now() + base::Days(10);
  interest_group.bidding_url = kUrlA.Resolve(kBiddingUrlPath);
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  // Run 2 auctions, with delays:
  //
  // succeed, (1s), fail, which in bits (with an extra leading 1) is 0b110.

  // Expect*TimeSample() doesn't accept base::TimeDelta::Max(), but the max time
  // bucket size is 1 hour, so specifying kMaxTime will select the max bucket.
  constexpr base::TimeDelta kMaxTime{base::Days(1)};

  blink::AuctionConfig succeed_auction_config;
  succeed_auction_config.seller = kOriginA;
  succeed_auction_config.decision_logic_url = kUrlA.Resolve(kDecisionUrlPath);
  succeed_auction_config.non_shared_params.interest_group_buyers = {kOriginA};

  blink::AuctionConfig fail_auction_config;
  fail_auction_config.seller = kOriginA;
  fail_auction_config.decision_logic_url =
      kUrlA.Resolve(kDecisionFailAllUrlPath);
  fail_auction_config.non_shared_params.interest_group_buyers = {kOriginA};

  // 1st auction
  EXPECT_NE(RunAdAuctionAndFlush(succeed_auction_config), absl::nullopt);
  // Time metrics are published every auction.
  histogram_tester.ExpectUniqueTimeSample(
      "Ads.InterestGroup.Auction.TimeSinceLastAuctionPerPage", kMaxTime, 1);

  // 2nd auction
  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_EQ(RunAdAuctionAndFlush(fail_auction_config), absl::nullopt);
  histogram_tester.ExpectTimeBucketCount(
      "Ads.InterestGroup.Auction.TimeSinceLastAuctionPerPage", base::Seconds(1),
      1);

  // Some metrics only get reported until after navigation.
  EXPECT_EQ(histogram_tester
                .GetAllSamples("Ads.InterestGroup.Auction.NumAuctionsPerPage")
                .size(),
            0u);
  EXPECT_EQ(
      histogram_tester
          .GetAllSamples(
              "Ads.InterestGroup.Auction.PercentAuctionsSuccessfulPerPage")
          .size(),
      0u);
  EXPECT_EQ(
      histogram_tester
          .GetAllSamples("Ads.InterestGroup.Auction.First6AuctionsBitsPerPage")
          .size(),
      0u);
  EXPECT_EQ(
      histogram_tester
          .GetAllSamples(
              "Ads.InterestGroup.Auction.NumAuctionsSkippedDueToAuctionLimit")
          .size(),
      0u);

  // DeleteContents() to force-populate remaining metrics.
  DeleteContents();

  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.Auction.NumAuctionsPerPage", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.Auction.PercentAuctionsSuccessfulPerPage", 1 * 100 / 2,
      1);
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.Auction.First6AuctionsBitsPerPage", 0b110, 1);
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.Auction.NumAuctionsSkippedDueToAuctionLimit", 0, 1);
}

// Like AddInterestGroupRunAuctionVerifyResultMetricsFewAuctions, but with no
// auctions.
TEST_F(AdAuctionServiceImplTest,
       AddInterestGroupRunAuctionVerifyResultMetricsNoAuctions) {
  base::HistogramTester histogram_tester;

  // Don't run any auctions.

  // Navigate to "populate" remaining metrics.
  DeleteContents();

  // Nothing gets reported since there were no auctions.
  EXPECT_EQ(histogram_tester
                .GetAllSamples("Ads.InterestGroup.Auction.NumAuctionsPerPage")
                .size(),
            0u);
  EXPECT_EQ(
      histogram_tester
          .GetAllSamples(
              "Ads.InterestGroup.Auction.PercentAuctionsSuccessfulPerPage")
          .size(),
      0u);
  EXPECT_EQ(
      histogram_tester
          .GetAllSamples("Ads.InterestGroup.Auction.First6AuctionsBitsPerPage")
          .size(),
      0u);
  EXPECT_EQ(histogram_tester
                .GetAllSamples(
                    "Ads.InterestGroup.Auction.TimeSinceLastAuctionPerPage")
                .size(),
            0u);
  EXPECT_EQ(
      histogram_tester
          .GetAllSamples(
              "Ads.InterestGroup.Auction.NumAuctionsSkippedDueToAuctionLimit")
          .size(),
      0u);
}

// The feature parameter that controls the interest group limit should default
// to off. We both check the parameter is off, and we run a number of auctions
// and make sure they all succeed.
TEST_F(AdAuctionServiceImplTest, NoInterestLimitByDefault) {
  EXPECT_FALSE(base::FeatureList::IsEnabled(features::kFledgeLimitNumAuctions));
  base::HistogramTester histogram_tester;
  constexpr char kDecisionFailAllUrlPath[] =
      "/interest_group/decision_logic_fail_all.js";

  constexpr char kBiddingScript[] = R"(
function generateBid(
    interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
    browserSignals) {
  return {'ad': 'example', 'bid': 1, 'render': 'https://example.com/render'};
}
function reportWin() {}
)";

  constexpr char kDecisionScript[] = R"(
function scoreAd(
    adMetadata, bid, auctionConfig, trustedScoringSignals, browserSignals) {
  return bid;
}
function reportResult() {}
)";

  constexpr char kDecisionScriptFailAll[] = R"(
function scoreAd(
    adMetadata, bid, auctionConfig, trustedScoringSignals, browserSignals) {
  return 0;
}
function reportResult() {}
)";

  network_responder_->RegisterScriptResponse(kBiddingUrlPath, kBiddingScript);
  network_responder_->RegisterScriptResponse(kDecisionUrlPath, kDecisionScript);
  network_responder_->RegisterScriptResponse(kDecisionFailAllUrlPath,
                                             kDecisionScriptFailAll);

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.expiry = base::Time::Now() + base::Days(10);
  interest_group.bidding_url = kUrlA.Resolve(kBiddingUrlPath);
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  constexpr int kNumAuctions = 10;
  // Run kNumAuctions auctions, all should succeed since there's no limit:
  blink::AuctionConfig succeed_auction_config;
  succeed_auction_config.seller = kOriginA;
  succeed_auction_config.decision_logic_url = kUrlA.Resolve(kDecisionUrlPath);
  succeed_auction_config.non_shared_params.interest_group_buyers = {kOriginA};

  for (int i = 0; i < kNumAuctions; i++) {
    EXPECT_NE(RunAdAuctionAndFlush(succeed_auction_config), absl::nullopt);
  }

  // Some metrics only get reported until after navigation.
  EXPECT_EQ(histogram_tester
                .GetAllSamples("Ads.InterestGroup.Auction.NumAuctionsPerPage")
                .size(),
            0u);
  EXPECT_EQ(
      histogram_tester
          .GetAllSamples(
              "Ads.InterestGroup.Auction.PercentAuctionsSuccessfulPerPage")
          .size(),
      0u);
  EXPECT_EQ(
      histogram_tester
          .GetAllSamples("Ads.InterestGroup.Auction.First6AuctionsBitsPerPage")
          .size(),
      0u);
  EXPECT_EQ(
      histogram_tester
          .GetAllSamples(
              "Ads.InterestGroup.Auction.NumAuctionsSkippedDueToAuctionLimit")
          .size(),
      0u);

  // DeleteContents() to force-populate remaining metrics.
  DeleteContents();

  // Every auction succeeds, none are skipped.
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.Auction.NumAuctionsPerPage", kNumAuctions, 1);
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.Auction.PercentAuctionsSuccessfulPerPage", 100, 1);
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.Auction.First6AuctionsBitsPerPage", 0b1111111, 1);
  // However, we do record that the auction was skipped.
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.Auction.NumAuctionsSkippedDueToAuctionLimit", 0, 1);
}

// CreateAdRequest should reject if we have an empty config.
TEST_F(AdAuctionServiceImplTest, CreateAdRequestRejectsEmptyConfigRequest) {
  auto mojo_config = blink::mojom::AdRequestConfig::New();
  bool callback_fired = false;
  CreateAdRequest(std::move(mojo_config),
                  base::BindLambdaForTesting(
                      [&](const absl::optional<std::string>& ads_guid) {
                        ASSERT_FALSE(ads_guid.has_value());
                        callback_fired = true;
                      }));
  ASSERT_TRUE(callback_fired);
}

// CreateAdRequest should reject if we have an otherwise okay request but our
// request URL is not using HTTPS.
TEST_F(AdAuctionServiceImplTest, CreateAdRequestRejectsHttpUrls) {
  auto mojo_config = blink::mojom::AdRequestConfig::New();
  mojo_config->ad_request_url = GURL("http://site.test/");
  auto mojo_ad_properties = blink::mojom::AdProperties::New();
  mojo_ad_properties->width = "48";
  mojo_ad_properties->height = "64";
  mojo_ad_properties->slot = "123";
  mojo_ad_properties->lang = "en";
  mojo_ad_properties->ad_type = "test";
  mojo_ad_properties->bid_floor = 1.0;
  mojo_config->ad_properties.push_back(std::move(mojo_ad_properties));

  bool callback_fired = false;
  CreateAdRequest(std::move(mojo_config),
                  base::BindLambdaForTesting(
                      [&](const absl::optional<std::string>& ads_guid) {
                        ASSERT_FALSE(ads_guid.has_value());
                        callback_fired = true;
                      }));
  ASSERT_TRUE(callback_fired);
}

// CreateAdRequest should reject if we have an otherwise okay request but no ad
// properties.
TEST_F(AdAuctionServiceImplTest, CreateAdRequestRejectsMissingAds) {
  auto mojo_config = blink::mojom::AdRequestConfig::New();
  mojo_config->ad_request_url = GURL("https://site.test/");

  bool callback_fired = false;
  CreateAdRequest(std::move(mojo_config),
                  base::BindLambdaForTesting(
                      [&](const absl::optional<std::string>& ads_guid) {
                        ASSERT_FALSE(ads_guid.has_value());
                        callback_fired = true;
                      }));
  ASSERT_TRUE(callback_fired);
}

// CreateAdRequest should reject if we have an otherwise okay request but
// include an HTTP fallback URL.
TEST_F(AdAuctionServiceImplTest, CreateAdRequestRejectsHttpFallback) {
  auto mojo_config = blink::mojom::AdRequestConfig::New();
  mojo_config->ad_request_url = GURL("https://site.test/");
  auto mojo_ad_properties = blink::mojom::AdProperties::New();
  mojo_ad_properties->width = "48";
  mojo_ad_properties->height = "64";
  mojo_ad_properties->slot = "123";
  mojo_ad_properties->lang = "en";
  mojo_ad_properties->ad_type = "test";
  mojo_ad_properties->bid_floor = 1.0;
  mojo_config->ad_properties.push_back(std::move(mojo_ad_properties));

  mojo_config->fallback_source = GURL("http://fallback_site.test/");

  bool callback_fired = false;
  CreateAdRequest(std::move(mojo_config),
                  base::BindLambdaForTesting(
                      [&](const absl::optional<std::string>& ads_guid) {
                        ASSERT_FALSE(ads_guid.has_value());
                        callback_fired = true;
                      }));
  ASSERT_TRUE(callback_fired);
}

// An empty config should be treated as a bad message.
TEST_F(AdAuctionServiceImplTest, FinalizeAdRejectsEmptyConfig) {
  blink::AuctionConfig config;
  FinalizeAdAndExpectPipeClosed(
      /*guid=*/std::string("1234"), config);
}

// An HTTP decision logic URL should be treated as a bad message.
TEST_F(AdAuctionServiceImplTest, FinalizeAdRejectsHTTPDecisionUrl) {
  blink::AuctionConfig config;
  config.seller = url::Origin::Create(GURL("https://site.test"));
  config.decision_logic_url = GURL("http://site.test/");

  FinalizeAdAndExpectPipeClosed(
      /*guid=*/"1234", config);
}

// An empty GUID should be treated as a bad message.
TEST_F(AdAuctionServiceImplTest, FinalizeAdRejectsMissingGuid) {
  blink::AuctionConfig config;
  config.seller = url::Origin::Create(GURL("https://site.test"));
  config.decision_logic_url = GURL("https://site.test/");

  FinalizeAdAndExpectPipeClosed(
      /*guid=*/std::string(), config);
}

TEST_F(AdAuctionServiceImplTest, SetPriorityAdjustsPriority) {
  constexpr char kBiddingScript[] = R"(
function generateBid(
    interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
    browserSignals) {
  if (interestGroup.priority !== undefined)
    throw new Error("Priority should not be in worklet");
  setPriority(99);
  return {'ad': 'example', 'bid': 1, 'render': 'https://example.com/render'};
}
)";

  constexpr char kDecisionScript[] = R"(
function scoreAd(
    adMetadata, bid, auctionConfig, trustedScoringSignals, browserSignals) {
  return bid;
}
)";

  network_responder_->RegisterScriptResponse(kBiddingUrlPath, kBiddingScript);
  network_responder_->RegisterScriptResponse(kDecisionUrlPath, kDecisionScript);

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.bidding_url = kUrlA.Resolve(kBiddingUrlPath);
  interest_group.priority = 2;
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(2, GetPriority(kOriginA, kInterestGroupName));

  blink::AuctionConfig auction_config;
  auction_config.seller = kOriginA;
  auction_config.decision_logic_url = kUrlA.Resolve(kDecisionUrlPath);
  auction_config.non_shared_params.interest_group_buyers = {kOriginA};
  absl::optional<GURL> auction_result = RunAdAuctionAndFlush(auction_config);
  ASSERT_NE(auction_result, absl::nullopt);
  EXPECT_EQ(ConvertFencedFrameURNToURL(*auction_result),
            GURL("https://example.com/render"));
  EXPECT_EQ(99, GetPriority(kOriginA, kInterestGroupName));
}

class AdAuctionServiceImplNumAuctionLimitTest
    : public AdAuctionServiceImplTest {
 public:
  AdAuctionServiceImplNumAuctionLimitTest() {
    // Only 2 auctions are allowed per-page.
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kFledgeLimitNumAuctions, {{"max_auctions_per_page", "2"}});
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

// Like AddInterestGroupRunAuctionVerifyResultMetrics, but with enforcement
// limiting the number of auctions.
TEST_F(AdAuctionServiceImplNumAuctionLimitTest,
       AddInterestGroupRunAuctionWithNumAuctionLimits) {
  base::HistogramTester histogram_tester;
  constexpr char kDecisionFailAllUrlPath[] =
      "/interest_group/decision_logic_fail_all.js";

  constexpr char kBiddingScript[] = R"(
function generateBid(
    interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
    browserSignals) {
  return {'ad': 'example', 'bid': 1, 'render': 'https://example.com/render'};
}
function reportWin() {}
)";

  constexpr char kDecisionScript[] = R"(
function scoreAd(
    adMetadata, bid, auctionConfig, trustedScoringSignals, browserSignals) {
  return bid;
}
function reportResult() {}
)";

  constexpr char kDecisionScriptFailAll[] = R"(
function scoreAd(
    adMetadata, bid, auctionConfig, trustedScoringSignals, browserSignals) {
  return 0;
}
function reportResult() {}
)";

  network_responder_->RegisterScriptResponse(kBiddingUrlPath, kBiddingScript);
  network_responder_->RegisterScriptResponse(kDecisionUrlPath, kDecisionScript);
  network_responder_->RegisterScriptResponse(kDecisionFailAllUrlPath,
                                             kDecisionScriptFailAll);

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.expiry = base::Time::Now() + base::Days(10);
  interest_group.bidding_url = kUrlA.Resolve(kBiddingUrlPath);
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  // Run 3 auctions, with delays:
  //
  // succeed, (1s), fail, (3s), succeed which in bits (with an extra leading 1)
  // is 0b110 -- the last success isn't recorded since the auction limit is
  // enforced.

  // Expect*TimeSample() doesn't accept base::TimeDelta::Max(), but the max time
  // bucket size is 1 hour, so specifying kMaxTime will select the max bucket.
  constexpr base::TimeDelta kMaxTime{base::Days(1)};

  blink::AuctionConfig succeed_auction_config;
  succeed_auction_config.seller = kOriginA;
  succeed_auction_config.decision_logic_url = kUrlA.Resolve(kDecisionUrlPath);
  succeed_auction_config.non_shared_params.interest_group_buyers = {kOriginA};

  blink::AuctionConfig fail_auction_config;
  fail_auction_config.seller = kOriginA;
  fail_auction_config.decision_logic_url =
      kUrlA.Resolve(kDecisionFailAllUrlPath);
  fail_auction_config.non_shared_params.interest_group_buyers = {kOriginA};

  // 1st auction
  EXPECT_NE(RunAdAuctionAndFlush(succeed_auction_config), absl::nullopt);
  // Time metrics are published every auction.
  histogram_tester.ExpectUniqueTimeSample(
      "Ads.InterestGroup.Auction.TimeSinceLastAuctionPerPage", kMaxTime, 1);

  // 2nd auction
  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_EQ(RunAdAuctionAndFlush(fail_auction_config), absl::nullopt);
  histogram_tester.ExpectTimeBucketCount(
      "Ads.InterestGroup.Auction.TimeSinceLastAuctionPerPage", base::Seconds(1),
      1);

  // 3rd auction -- fails even though decision_logic.js is used because the
  // auction limit is encountered.
  task_environment()->FastForwardBy(base::Seconds(3));
  EXPECT_EQ(RunAdAuctionAndFlush(succeed_auction_config), absl::nullopt);
  // The time metrics shouldn't get updated.
  histogram_tester.ExpectTimeBucketCount(
      "Ads.InterestGroup.Auction.TimeSinceLastAuctionPerPage", base::Seconds(3),
      0);

  // Some metrics only get reported until after navigation.
  EXPECT_EQ(histogram_tester
                .GetAllSamples("Ads.InterestGroup.Auction.NumAuctionsPerPage")
                .size(),
            0u);
  EXPECT_EQ(
      histogram_tester
          .GetAllSamples(
              "Ads.InterestGroup.Auction.PercentAuctionsSuccessfulPerPage")
          .size(),
      0u);
  EXPECT_EQ(
      histogram_tester
          .GetAllSamples("Ads.InterestGroup.Auction.First6AuctionsBitsPerPage")
          .size(),
      0u);
  EXPECT_EQ(
      histogram_tester
          .GetAllSamples(
              "Ads.InterestGroup.Auction.NumAuctionsSkippedDueToAuctionLimit")
          .size(),
      0u);

  // DeleteContents() to force-populate remaining metrics.
  DeleteContents();

  // The last auction doesn't count towards these metrics since the auction
  // limit is enforced -- this is because that auction doesn't contribute any
  // knowledge about stored interest groups to the page.
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.Auction.NumAuctionsPerPage", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.Auction.PercentAuctionsSuccessfulPerPage", 1 * 100 / 2,
      1);
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.Auction.First6AuctionsBitsPerPage", 0b110, 1);
  // However, we do record that the auction was skipped.
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.Auction.NumAuctionsSkippedDueToAuctionLimit", 1, 1);
}

TEST_F(AdAuctionServiceImplNumAuctionLimitTest,
       AddInterestGroupRunAuctionStartManyAuctionsInParallel) {
  base::HistogramTester histogram_tester;

  constexpr char kBiddingScript[] = R"(
function generateBid(
    interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
    browserSignals) {
  return {'ad': 'example', 'bid': 1, 'render': 'https://example.com/render'};
}
function reportWin() {}
)";

  constexpr char kDecisionScript[] = R"(
function scoreAd(
    adMetadata, bid, auctionConfig, trustedScoringSignals, browserSignals) {
  return bid;
}
function reportResult() {}
)";

  network_responder_->RegisterScriptResponse(kBiddingUrlPath, kBiddingScript);
  network_responder_->RegisterScriptResponse(kDecisionUrlPath, kDecisionScript);

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.expiry = base::Time::Now() + base::Days(10);
  interest_group.bidding_url = kUrlA.Resolve(kBiddingUrlPath);
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  blink::AuctionConfig succeed_auction_config;
  succeed_auction_config.seller = kOriginA;
  succeed_auction_config.decision_logic_url = kUrlA.Resolve(kDecisionUrlPath);
  succeed_auction_config.non_shared_params.interest_group_buyers = {kOriginA};

  // Pick some large number, larger than the auction limit.
  constexpr int kNumAuctions = 10;
  base::RunLoop run_loop;
  mojo::Remote<blink::mojom::AdAuctionService> interest_service;
  AdAuctionServiceImpl::CreateMojoService(
      main_rfh(), interest_service.BindNewPipeAndPassReceiver());
  base::RepeatingClosure one_auction_complete =
      base::BarrierClosure(kNumAuctions, run_loop.QuitClosure());

  for (int i = 0; i < kNumAuctions; i++) {
    interest_service->RunAdAuction(
        succeed_auction_config, mojo::NullReceiver(),
        base::BindLambdaForTesting(
            [&one_auction_complete](
                bool manually_aborted,
                const absl::optional<
                    blink::FencedFrame::RedactedFencedFrameConfig>&
                    ignored_config) { one_auction_complete.Run(); }));
  }
  run_loop.Run();

  // DeleteContents() to force-populate remaining metrics.
  DeleteContents();

  // Only the first 2 auctions should have succeeded -- the others should fail.
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.Auction.NumAuctionsPerPage", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.Auction.PercentAuctionsSuccessfulPerPage", 2 * 100 / 2,
      1);
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.Auction.First6AuctionsBitsPerPage", 0b111, 1);
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.Auction.NumAuctionsSkippedDueToAuctionLimit",
      kNumAuctions - 2, 1);
}

class AdAuctionServiceImplRestrictedPermissionsPolicyTest
    : public AdAuctionServiceImplTest {
 public:
  AdAuctionServiceImplRestrictedPermissionsPolicyTest() {
    feature_list_.InitAndEnableFeature(
        blink::features::kAdInterestGroupAPIRestrictedPolicyByDefault);
    blink::UpdatePermissionsPolicyFeatureListForTesting();
    old_content_browser_client_ =
        SetBrowserClientForTesting(&content_browser_client_);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

// Permissions policy feature join-ad-interest-group is enabled by default for
// top level frames under restricted permissions policy, so interest group
// APIs should succeed.
TEST_F(AdAuctionServiceImplRestrictedPermissionsPolicyTest,
       APICallsFromTopFrame) {
  network_responder_->RegisterUpdateResponse(
      kUpdateUrlPath, base::StringPrintf(R"({"biddingLogicUrl": "%s%s"})",
                                         kOriginStringA, kNewBiddingUrlPath));
  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.update_url = kUpdateUrlA;
  interest_group.bidding_url = kBiddingLogicUrlA;
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  std::vector<StorageInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);
  const auto& group = groups[0].interest_group;
  EXPECT_EQ(group.name, kInterestGroupName);
  ASSERT_TRUE(group.bidding_url.has_value());
  EXPECT_EQ(group.bidding_url->spec(),
            base::StringPrintf("%s%s", kOriginStringA, kNewBiddingUrlPath));

  LeaveInterestGroupAndFlush(kOriginA, kInterestGroupName);
  EXPECT_EQ(0, GetJoinCount(kOriginA, kInterestGroupName));
}

// Like APICallsFromTopFrame, but API calls happens in a same site iframe
// instead of a top frame.
TEST_F(AdAuctionServiceImplRestrictedPermissionsPolicyTest,
       APICallsFromSameSiteIframe) {
  network_responder_->RegisterUpdateResponse(
      kUpdateUrlPath, base::StringPrintf(R"({"biddingLogicUrl": "%s%s"})",
                                         kOriginStringA, kNewBiddingUrlPath));
  // Create a same site subframe and use it to send the interest group requests.
  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  content::RenderFrameHost* subframe = rfh_tester->AppendChild("subframe");
  subframe =
      NavigationSimulator::NavigateAndCommitFromDocument(kUrlA, subframe);

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.update_url = kUpdateUrlA;
  interest_group.bidding_url = kBiddingLogicUrlA;
  JoinInterestGroupAndFlush(std::move(interest_group), subframe);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  UpdateInterestGroupNoFlushForFrame(subframe);
  task_environment()->RunUntilIdle();

  std::vector<StorageInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);
  const auto& group = groups[0].interest_group;
  EXPECT_EQ(group.name, kInterestGroupName);
  ASSERT_TRUE(group.bidding_url.has_value());
  EXPECT_EQ(group.bidding_url->spec(),
            base::StringPrintf("%s%s", kOriginStringA, kNewBiddingUrlPath));

  LeaveInterestGroupAndFlush(kOriginA, kInterestGroupName, subframe);
  EXPECT_EQ(0, GetJoinCount(kOriginA, kInterestGroupName));
}

// Permissions policy feature join-ad-interest-group is disabled by default for
// cross site iframes under restricted permissions policy, so interest group
// APIs should not work, and result in the pipe being closed.
TEST_F(AdAuctionServiceImplRestrictedPermissionsPolicyTest,
       APICallsFromCrossSiteIFrame) {
  network_responder_->RegisterUpdateResponse(
      kUpdateUrlPath, base::StringPrintf(R"({"biddingLogicUrl": "%s%s"})",
                                         kOriginStringC, kNewBiddingUrlPath));

  NavigateAndCommit(kUrlC);

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.owner = kOriginC;
  interest_group.bidding_url = kUrlC.Resolve(kBiddingUrlPath);
  JoinInterestGroupAndFlush(interest_group);

  NavigateAndCommit(kUrlA);
  EXPECT_EQ(1, GetJoinCount(kOriginC, kInterestGroupName));

  // Create a cross site subframe and use it to send interest group requests.
  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  content::RenderFrameHost* subframe = rfh_tester->AppendChild("subframe");
  subframe =
      NavigationSimulator::NavigateAndCommitFromDocument(kUrlC, subframe);
  blink::InterestGroup interest_group_2 = CreateInterestGroup();
  constexpr char kInterestGroupName2[] = "group2";
  interest_group.owner = kOriginC;
  interest_group.name = kInterestGroupName2;
  JoinInterestGroupAndExpectPipeClosed(std::move(interest_group_2), subframe);
  EXPECT_EQ(0, GetJoinCount(kOriginC, kInterestGroupName2));

  UpdateInterestGroupNoFlushForFrame(subframe);
  task_environment()->RunUntilIdle();

  // `bidding_url` should not change.
  std::vector<StorageInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginC);
  ASSERT_EQ(groups.size(), 1u);
  const auto& group = groups[0].interest_group;
  EXPECT_EQ(group.name, kInterestGroupName);
  ASSERT_TRUE(group.bidding_url.has_value());
  EXPECT_EQ(group.bidding_url->spec(),
            base::StringPrintf("%s%s", kOriginStringC, kBiddingUrlPath));

  LeaveInterestGroupAndExpectPipeClosed(kOriginC, kInterestGroupName, subframe);
  EXPECT_EQ(1, GetJoinCount(kOriginC, kInterestGroupName));
}

class AdAuctionServiceImplBiddingAndScoringDebugReportingAPIEnabledTest
    : public AdAuctionServiceImplTest {
 public:
  AdAuctionServiceImplBiddingAndScoringDebugReportingAPIEnabledTest() {
    feature_list_.InitAndEnableFeature(
        blink::features::kBiddingAndScoringDebugReportingAPI);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

// Allowing sending multiple reports in parallel, instead of only allowing
// sending one at a time.
TEST_F(AdAuctionServiceImplBiddingAndScoringDebugReportingAPIEnabledTest,
       SendReportsMaximumActive) {
  // Use interest group name as bid value.
  const std::string kBiddingScript =
      base::StringPrintf(R"(
function generateBid(
    interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
    browserSignals) {
  forDebuggingOnly.reportAdAuctionWin(
      `%s/bidder_debug_win_` + interestGroup.name);
  return {
    'ad': 'example',
    'bid': parseInt(interestGroup.name),
    'render': 'https://example.com/render'
  };
}
function reportWin(
    auctionSignals, perBuyerSignals, sellerSignals, browserSignals) {
  sendReportTo('%s/report_bidder_' + browserSignals.bid);
}
  )",
                         kOriginStringA, kOriginStringA);

  const std::string kDecisionScript =
      base::StringPrintf(R"(
function scoreAd(
    adMetadata, bid, auctionConfig, trustedScoringSignals, browserSignals) {
  forDebuggingOnly.reportAdAuctionWin(`%s/seller_debug_win_` + bid);
  return bid;
}
function reportResult(auctionConfig, browserSignals) {
  const reportUrl = '%s/report_seller_' + browserSignals.bid;
  sendReportTo(reportUrl);
  return {
    'success': true,
    'signalsForWinner': {'signalForWinner': 1},
    'reportUrl': reportUrl,
  };
}
)",
                         kOriginStringA, kOriginStringA);

  manager_->set_max_report_queue_length_for_testing(50);
  manager_->set_max_active_report_requests_for_testing(3);
  manager_->set_reporting_interval_for_testing(base::Seconds(5));
  network_responder_->RegisterScriptResponse(kBiddingUrlPath, kBiddingScript);
  network_responder_->RegisterScriptResponse(kDecisionUrlPath, kDecisionScript);

  // Run two auctions, each time with a new interest group which bids i wins
  // the auction.
  for (int i = 1; i < 3; i++) {
    const std::string name = base::NumberToString(i);
    network_responder_->RegisterReportResponse(
        base::StringPrintf("/report_bidder_%s", name.c_str()), /*response=*/"");
    network_responder_->RegisterReportResponse(
        base::StringPrintf("/report_seller_%s", name.c_str()), /*response=*/"");
    network_responder_->RegisterReportResponse(
        base::StringPrintf("/seller_debug_win_%s", name.c_str()),
        /*response=*/"");
    network_responder_->RegisterReportResponse(
        base::StringPrintf("/bidder_debug_win_%s", name.c_str()),
        /*response=*/"");
    blink::InterestGroup interest_group = CreateInterestGroup();
    interest_group.bidding_url = kUrlA.Resolve(kBiddingUrlPath);
    interest_group.name = name;
    interest_group.ads.emplace();
    blink::InterestGroup::Ad ad(
        /*render_url=*/GURL("https://example.com/render"),
        /*metadata=*/absl::nullopt);
    interest_group.ads->emplace_back(std::move(ad));
    JoinInterestGroupAndFlush(interest_group);
    EXPECT_EQ(1, GetJoinCount(kOriginA, name));

    blink::AuctionConfig auction_config;
    auction_config.seller = kOriginA;

    auction_config.decision_logic_url = kUrlA.Resolve(kDecisionUrlPath);
    auction_config.non_shared_params.interest_group_buyers = {kOriginA};
    absl::optional<GURL> auction_result = RunAdAuctionAndFlush(auction_config);
    ASSERT_NE(auction_result, absl::nullopt);
    // Wait for the reporting scripts to complete. Doing this before invoking
    // the navigation callback results in reports being sent in the order:
    // * Seller reportResult() report.
    // * Bidder reportWin() report.
    // * Win reports.
    //
    // Not doing this results in more confusing orders, with the seller report
    // potentially being sent either before or after the debug reports, which
    // are always sent before the bidder report.
    task_environment()->RunUntilIdle();
    // This will cause the reports to be queued.
    InvokeCallbackForURN(*auction_result);
  }

  task_environment()->FastForwardBy(base::Seconds(3));
  // Three reports sent already. Reporting interval is set to 5s, and only 3s
  // passed, so no next report was sent after one report was sent, i.e., all
  // sent reports were sent in the same round in parallel.
  EXPECT_EQ(network_responder_->ReportCount(), 3u);
  EXPECT_TRUE(network_responder_->ReportSent("/report_seller_1"));
  EXPECT_TRUE(network_responder_->ReportSent("/report_bidder_1"));
  EXPECT_TRUE(network_responder_->ReportSent("/bidder_debug_win_1"));
  // Fastforward to pass reporting interval (but less than two reporting
  // intervals) so that the second round of reports are sent but the third
  // round hasn't started.
  task_environment()->FastForwardBy(base::Seconds(5));
  // Three more reports were sent.
  EXPECT_EQ(network_responder_->ReportCount(), 6u);
  EXPECT_TRUE(network_responder_->ReportSent("/seller_debug_win_1"));
  EXPECT_TRUE(network_responder_->ReportSent("/report_seller_2"));
  EXPECT_TRUE(network_responder_->ReportSent("/report_bidder_2"));
  // Fastforward enough time for all reports to be sent.
  task_environment()->FastForwardBy(base::Seconds(6));
  EXPECT_EQ(network_responder_->ReportCount(), 8u);
  EXPECT_TRUE(network_responder_->ReportSent("/bidder_debug_win_2"));
  EXPECT_TRUE(network_responder_->ReportSent("/seller_debug_win_2"));
}

// TODO(crbug.com/1422301): The auction must operate on the same fenced frame
// mapping that was used at the beginning of the auction. If not, we fail the
// auction and dump without crashing the browser. Once the root cause is known
// and the issue fixed, remove this test.
TEST_F(AdAuctionServiceImplTest, FencedFrameUrlMappingChangedDuringAuction) {
  network_responder_->RegisterDeferredScriptResponse(kBiddingUrlPath);
  network_responder_->RegisterScriptResponse(kDecisionUrlPath,
                                             BasicSellerReportScript());

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.bidding_url = kUrlA.Resolve(kBiddingUrlPath);
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));

  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  blink::AuctionConfig auction_config;
  auction_config.seller = kOriginA;
  auction_config.decision_logic_url = kUrlA.Resolve(kDecisionUrlPath);
  auction_config.non_shared_params.interest_group_buyers = {kOriginA};

  AdAuctionServiceImpl::CreateMojoService(
      main_rfh(), ad_auction_service_.BindNewPipeAndPassReceiver());

  // Start the auction.
  base::RunLoop run_loop;
  absl::optional<blink::FencedFrame::RedactedFencedFrameConfig> maybe_config;
  ad_auction_service_->RunAdAuction(
      auction_config, mojo::NullReceiver(),
      base::BindLambdaForTesting(
          [&run_loop, &maybe_config](
              bool manually_aborted,
              const absl::optional<
                  blink::FencedFrame::RedactedFencedFrameConfig>& config) {
            EXPECT_FALSE(manually_aborted);
            maybe_config = config;
            run_loop.Quit();
          }));

  // Wait for the NetworkResponder to see the request for the bidding URL, then
  // respond.
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(run_loop.AnyQuitCalled());
  network_responder_->DoDeferredScriptResponse(kBiddingUrlPath,
                                               BasicBiddingReportScript());

  FencedFrameURLMapping& fenced_frame_urls_map =
      static_cast<RenderFrameHostImpl*>(main_rfh())
          ->GetPage()
          .fenced_frame_urls_map();

  FencedFrameURLMappingTestPeer test_peer(&fenced_frame_urls_map);

  // Change the id of the fenced frame url mapping used by the current auction
  // to simulate the scenario we've seen in crbug.com/1422301: the fenced frame
  // url mapping used at the beginning of the auction is not the same as the one
  // at the end of the auction.
  test_peer.SetId(test_peer.GetNextId());

  // Complete the auction. It should fail due to the fenced frame url mapping
  // mismatch.
  run_loop.Run();
  EXPECT_FALSE(maybe_config.has_value());
}

class AdAuctionServiceImplSharedStorageEnabledTest
    : public AdAuctionServiceImplTest {
 public:
  AdAuctionServiceImplSharedStorageEnabledTest() {
    feature_list_.InitAndEnableFeature(blink::features::kSharedStorageAPI);
  }

  std::u16string SharedStorageGet(const url::Origin& context_origin,
                                  const std::u16string& key) {
    storage::SharedStorageManager* shared_storage_manager =
        static_cast<StoragePartitionImpl*>(
            browser_context()->GetDefaultStoragePartition())
            ->GetSharedStorageManager();
    DCHECK(shared_storage_manager);

    base::test::TestFuture<storage::SharedStorageManager::GetResult> future;
    shared_storage_manager->Get(context_origin, key, future.GetCallback());
    storage::SharedStorageManager::GetResult result = future.Take();

    return result.data;
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(AdAuctionServiceImplSharedStorageEnabledTest, SharedStorageWrite) {
  constexpr char kBiddingScript[] = R"(
function generateBid(
    interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
    browserSignals) {
  sharedStorage.set('key0', 'value0');
  return {'ad': 'example', 'bid': 1, 'render': 'https://example.com/render'};
}
function reportWin(
    auctionSignals, perBuyerSignals, sellerSignals, browserSignals) {
  sharedStorage.append('key0', 'value1');
  sharedStorage.set('key1', 'value1');
  sharedStorage.set('key4', 'value4');
}
)";

  constexpr char kDecisionScript[] = R"(
function scoreAd(
    adMetadata, bid, auctionConfig, trustedScoringSignals, browserSignals) {
  sharedStorage.set('key2', 'value2');
  return bid;
}
function reportResult() {
  sharedStorage.append('key2', 'value3');
  sharedStorage.set('key3', 'value3');
  sharedStorage.set('key4', 'value4');
}
)";

  network_responder_->RegisterScriptResponse(kBiddingUrlPath, kBiddingScript);
  network_responder_->RegisterScriptResponse(kDecisionUrlPath, kDecisionScript);

  NavigateAndCommit(kUrlC);
  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.owner = kOriginC;
  interest_group.bidding_url = kUrlC.Resolve(kBiddingUrlPath);
  interest_group.priority = 2;
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);

  NavigateAndCommit(kUrlA);
  blink::AuctionConfig auction_config;
  auction_config.seller = kOriginA;
  auction_config.decision_logic_url = kUrlA.Resolve(kDecisionUrlPath);
  auction_config.non_shared_params.interest_group_buyers = {kOriginC};

  absl::optional<GURL> auction_result = RunAdAuctionAndFlush(auction_config);
  EXPECT_NE(auction_result, absl::nullopt);

  // Make sure the shared storage mojom methods are invoked as they use a
  // dedicated pipe.
  task_environment()->RunUntilIdle();

  EXPECT_EQ(SharedStorageGet(kOriginC, u"key0"), u"value0value1");
  EXPECT_EQ(SharedStorageGet(kOriginC, u"key1"), u"value1");
  EXPECT_EQ(SharedStorageGet(kOriginC, u"key4"), u"value4");
  EXPECT_EQ(SharedStorageGet(kOriginA, u"key2"), u"value2value3");
  EXPECT_EQ(SharedStorageGet(kOriginA, u"key3"), u"value3");
  EXPECT_EQ(SharedStorageGet(kOriginA, u"key4"), u"value4");
}

TEST_F(AdAuctionServiceImplSharedStorageEnabledTest,
       ScriptErrorAfterSharedStorageWrite) {
  constexpr char kBiddingScript[] = R"(
function generateBid(
    interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
    browserSignals) {
  sharedStorage.set('key0', 'value0');
  return {'ad': 'example', 'bid': 1, 'render': 'https://example.com/render'};
}
function reportWin(
    auctionSignals, perBuyerSignals, sellerSignals, browserSignals) {
  sharedStorage.set('key1', 'value1');
}
)";

  constexpr char kDecisionScript[] = R"(
function scoreAd(
    adMetadata, bid, auctionConfig, trustedScoringSignals, browserSignals) {
  sharedStorage.set('key2', 'value2');

  triggerReferenceError

  return bid;
}
function reportResult() {
  sharedStorage.set('key3', 'value3');
}
)";

  network_responder_->RegisterScriptResponse(kBiddingUrlPath, kBiddingScript);
  network_responder_->RegisterScriptResponse(kDecisionUrlPath, kDecisionScript);

  NavigateAndCommit(kUrlC);
  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.owner = kOriginC;
  interest_group.bidding_url = kUrlC.Resolve(kBiddingUrlPath);
  interest_group.priority = 2;
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);

  NavigateAndCommit(kUrlA);
  blink::AuctionConfig auction_config;
  auction_config.seller = kOriginA;
  auction_config.decision_logic_url = kUrlA.Resolve(kDecisionUrlPath);
  auction_config.non_shared_params.interest_group_buyers = {kOriginC};

  absl::optional<GURL> auction_result = RunAdAuctionAndFlush(auction_config);
  EXPECT_EQ(auction_result, absl::nullopt);

  // Make sure the shared storage mojom methods are invoked as they use a
  // dedicated pipe.
  task_environment()->RunUntilIdle();

  // When scoreAd() throws an exception after a
  // sharedStorage.set('key2', 'value2'), the write operation should still be
  // handled.
  EXPECT_EQ(SharedStorageGet(kOriginC, u"key0"), u"value0");
  EXPECT_EQ(SharedStorageGet(kOriginC, u"key1"), u"");
  EXPECT_EQ(SharedStorageGet(kOriginA, u"key2"), u"value2");
  EXPECT_EQ(SharedStorageGet(kOriginA, u"key3"), u"");
}

TEST_F(AdAuctionServiceImplSharedStorageEnabledTest,
       SharedStoragePermissionsPolicyDisallowsSellerOrigin) {
  constexpr char kBiddingScript[] = R"(
function generateBid(
    interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
    browserSignals) {
  sharedStorage.set('key0', 'value0');
  return {'ad': 'example', 'bid': 1, 'render': 'https://example.com/render'};
}
function reportWin(
    auctionSignals, perBuyerSignals, sellerSignals, browserSignals) {
  sharedStorage.set('key1', 'value1');
}
)";

  constexpr char kDecisionScript[] = R"(
function scoreAd(
    adMetadata, bid, auctionConfig, trustedScoringSignals, browserSignals) {
  sharedStorage.set('key2', 'value2');
  return bid;
}
function reportResult() {
  sharedStorage.set('key3', 'value3');
}
)";

  network_responder_->RegisterScriptResponse(kBiddingUrlPath, kBiddingScript);
  network_responder_->RegisterScriptResponse(kDecisionUrlPath, kDecisionScript);

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.bidding_url = kUrlA.Resolve(kBiddingUrlPath);
  interest_group.priority = 2;
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);

  // Allow only (bidder) origin A in the permissions policy. The auction with
  // seller origin C should fail.
  auto simulator =
      NavigationSimulator::CreateBrowserInitiated(kUrlA, web_contents());
  blink::ParsedPermissionsPolicy policy;
  policy.emplace_back(
      blink::mojom::PermissionsPolicyFeature::kSharedStorage,
      /*allowed_origins=*/
      std::vector<blink::OriginWithPossibleWildcards>{
          blink::OriginWithPossibleWildcards(kOriginA,
                                             /*has_subdomain_wildcard=*/false)},
      /*self_if_matches=*/absl::nullopt,
      /*matches_all_origins=*/false,
      /*matches_opaque_src=*/false);
  simulator->SetPermissionsPolicyHeader(std::move(policy));
  simulator->Commit();

  blink::AuctionConfig auction_config;
  auction_config.seller = kOriginC;
  auction_config.decision_logic_url = kUrlC.Resolve(kDecisionUrlPath);
  auction_config.non_shared_params.interest_group_buyers = {kOriginA};

  absl::optional<GURL> auction_result = RunAdAuctionAndFlush(auction_config);
  EXPECT_EQ(auction_result, absl::nullopt);

  // Make sure the shared storage mojom methods are invoked as they use a
  // dedicated pipe.
  task_environment()->RunUntilIdle();

  // Only the sharedStorage.set() from generateBid() was successful.
  EXPECT_EQ(SharedStorageGet(kOriginA, u"key0"), u"value0");
  EXPECT_EQ(SharedStorageGet(kOriginA, u"key1"), u"");
  EXPECT_EQ(SharedStorageGet(kOriginC, u"key2"), u"");
  EXPECT_EQ(SharedStorageGet(kOriginC, u"key3"), u"");
}

class AdAuctionServiceImplSharedStorageDisabledTest
    : public AdAuctionServiceImplTest {
 public:
  AdAuctionServiceImplSharedStorageDisabledTest() {
    feature_list_.InitAndDisableFeature(blink::features::kSharedStorageAPI);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

// TODO(crbug.com/1408576): Flaky.
TEST_F(AdAuctionServiceImplSharedStorageDisabledTest,
       DISABLED_SharedStorageNotDefined) {
  constexpr char kBiddingScript[] = R"(
function generateBid(
    interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
    browserSignals) {
  sharedStorage.set('key0', 'value0');
  return {'ad': 'example', 'bid': 1, 'render': 'https://example.com/render'};
}
)";

  constexpr char kDecisionScript[] = R"(
function scoreAd(
    adMetadata, bid, auctionConfig, trustedScoringSignals, browserSignals) {
  return bid;
}
)";

  network_responder_->RegisterScriptResponse(kBiddingUrlPath, kBiddingScript);
  network_responder_->RegisterScriptResponse(kDecisionUrlPath, kDecisionScript);

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.owner = kOriginA;
  interest_group.bidding_url = kUrlA.Resolve(kBiddingUrlPath);
  interest_group.priority = 2;
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);

  blink::AuctionConfig auction_config;
  auction_config.seller = kOriginA;
  auction_config.decision_logic_url = kUrlA.Resolve(kDecisionUrlPath);
  auction_config.non_shared_params.interest_group_buyers = {kOriginA};

  absl::optional<GURL> auction_result = RunAdAuctionAndFlush(auction_config);
  EXPECT_EQ(auction_result, absl::nullopt);
}

class AdAuctionServiceImplPrivateAggregationEnabledTest
    : public AdAuctionServiceImplTest {
 public:
  AdAuctionServiceImplPrivateAggregationEnabledTest() {
    feature_list_.InitAndEnableFeature(blink::features::kPrivateAggregationApi);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(AdAuctionServiceImplPrivateAggregationEnabledTest,
       PrivateAggregationReportsForwarded) {
  constexpr char kBiddingScript[] = R"(
function generateBid(
    interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
    browserSignals) {
  privateAggregation.contributeToHistogram({bucket: 1n, value: 2});
  privateAggregation.contributeToHistogram({bucket: 3n, value: 4});
  return {'ad': 'example', 'bid': 1, 'render': 'https://example.com/render'};
}
)";

  constexpr char kDecisionScript[] = R"(
function scoreAd(
    adMetadata, bid, auctionConfig, trustedScoringSignals, browserSignals) {
  return bid;
}
)";

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

  auto* storage_partition_impl = static_cast<StoragePartitionImpl*>(
      browser_context()->GetDefaultStoragePartition());
  storage_partition_impl->OverridePrivateAggregationManagerForTesting(
      std::make_unique<TestPrivateAggregationManagerImpl>(
          std::make_unique<MockPrivateAggregationBudgeter>(),
          std::make_unique<PrivateAggregationHost>(
              /*on_report_request_received=*/mock_callback.Get(),
              /*browser_context=*/storage_partition_impl->browser_context())));

  network_responder_->RegisterScriptResponse(kBiddingUrlPath, kBiddingScript);
  network_responder_->RegisterScriptResponse(kDecisionUrlPath, kDecisionScript);

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.bidding_url = kUrlA.Resolve(kBiddingUrlPath);
  interest_group.priority = 2;
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);

  blink::AuctionConfig auction_config;
  auction_config.seller = kOriginA;
  auction_config.decision_logic_url = kUrlA.Resolve(kDecisionUrlPath);
  auction_config.non_shared_params.interest_group_buyers = {kOriginA};

  base::RunLoop run_loop;
  EXPECT_CALL(mock_callback, Run)
      .WillOnce(testing::Invoke([&](AggregatableReportRequest request,
                                    PrivateAggregationBudgetKey budget_key) {
        ASSERT_EQ(request.payload_contents().contributions.size(), 2u);
        EXPECT_EQ(request.payload_contents().contributions[0].bucket, 1);
        EXPECT_EQ(request.payload_contents().contributions[0].value, 2);
        EXPECT_EQ(request.payload_contents().contributions[1].bucket, 3);
        EXPECT_EQ(request.payload_contents().contributions[1].value, 4);
        EXPECT_EQ(request.shared_info().reporting_origin, kOriginA);
        EXPECT_EQ(budget_key.api(),
                  PrivateAggregationBudgetKey::Api::kProtectedAudience);
        EXPECT_EQ(budget_key.origin(), kOriginA);
        run_loop.Quit();
      }));

  absl::optional<GURL> auction_result = RunAdAuctionAndFlush(auction_config);
  EXPECT_NE(auction_result, absl::nullopt);
  InvokeCallbackForURN(*auction_result);
  run_loop.Run();
}

TEST_F(AdAuctionServiceImplPrivateAggregationEnabledTest,
       PrivateAggregationPermissionsPolicyDisallowsSellerOrigin) {
  constexpr char kBiddingScript[] = R"(
function generateBid(
    interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
    browserSignals) {
  return {'ad': 'example', 'bid': 1, 'render': 'https://example.com/render'};
}
)";

  constexpr char kDecisionScript[] = R"(
function scoreAd(
    adMetadata, bid, auctionConfig, trustedScoringSignals, browserSignals) {
  privateAggregation.contributeToHistogram({bucket: 1n, value: 2});
  return bid;
}
)";

  network_responder_->RegisterScriptResponse(kBiddingUrlPath, kBiddingScript);
  network_responder_->RegisterScriptResponse(kDecisionUrlPath, kDecisionScript);

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.bidding_url = kUrlA.Resolve(kBiddingUrlPath);
  interest_group.priority = 2;
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);

  // Allow only (bidder) origin A in the permissions policy. The auction with
  // seller origin C should fail.
  {
    auto simulator =
        NavigationSimulator::CreateBrowserInitiated(kUrlA, web_contents());
    blink::ParsedPermissionsPolicy policy;
    policy.emplace_back(
        blink::mojom::PermissionsPolicyFeature::kPrivateAggregation,
        /*allowed_origins=*/
        std::vector<blink::OriginWithPossibleWildcards>{
            blink::OriginWithPossibleWildcards(
                kOriginA,
                /*has_subdomain_wildcard=*/false)},
        /*self_if_matches=*/absl::nullopt,
        /*matches_all_origins=*/false,
        /*matches_opaque_src=*/false);
    simulator->SetPermissionsPolicyHeader(std::move(policy));
    simulator->Commit();

    blink::AuctionConfig auction_config;
    auction_config.seller = kOriginC;
    auction_config.decision_logic_url = kUrlC.Resolve(kDecisionUrlPath);
    auction_config.non_shared_params.interest_group_buyers = {kOriginA};

    absl::optional<GURL> auction_result = RunAdAuctionAndFlush(auction_config);
    EXPECT_EQ(auction_result, absl::nullopt);
  }

  // In contrast to the case above, additionally allow origin C in the
  // permissions policy. The auction with seller origin C should succeed.
  {
    auto simulator =
        NavigationSimulator::CreateBrowserInitiated(kUrlA, web_contents());
    blink::ParsedPermissionsPolicy policy;
    policy.emplace_back(
        blink::mojom::PermissionsPolicyFeature::kPrivateAggregation,
        /*allowed_origins=*/
        std::vector<blink::OriginWithPossibleWildcards>{
            blink::OriginWithPossibleWildcards(
                kOriginA,
                /*has_subdomain_wildcard=*/false),
            blink::OriginWithPossibleWildcards(
                kOriginC,
                /*has_subdomain_wildcard=*/false)},
        /*self_if_matches=*/absl::nullopt,
        /*matches_all_origins=*/false,
        /*matches_opaque_src=*/false);
    simulator->SetPermissionsPolicyHeader(std::move(policy));
    simulator->Commit();

    blink::AuctionConfig auction_config;
    auction_config.seller = kOriginC;
    auction_config.decision_logic_url = kUrlC.Resolve(kDecisionUrlPath);
    auction_config.non_shared_params.interest_group_buyers = {kOriginA};

    absl::optional<GURL> auction_result = RunAdAuctionAndFlush(auction_config);
    EXPECT_NE(auction_result, absl::nullopt);
  }
}

TEST_F(AdAuctionServiceImplPrivateAggregationEnabledTest,
       PrivateAggregationPermissionsPolicyDisallowsBidderOrigin) {
  constexpr char kBiddingScript[] = R"(
function generateBid(
    interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
    browserSignals) {
  privateAggregation.contributeToHistogram({bucket: 1n, value: 2});
  return {'ad': 'example', 'bid': 1, 'render': 'https://example.com/render'};
}
)";

  constexpr char kDecisionScript[] = R"(
function scoreAd(
    adMetadata, bid, auctionConfig, trustedScoringSignals, browserSignals) {
  return bid;
}
)";

  network_responder_->RegisterScriptResponse(kBiddingUrlPath, kBiddingScript);
  network_responder_->RegisterScriptResponse(kDecisionUrlPath, kDecisionScript);

  NavigateAndCommit(kUrlC);
  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.owner = kOriginC;
  interest_group.bidding_url = kUrlC.Resolve(kBiddingUrlPath);
  interest_group.priority = 2;
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);

  // Allow only (seller) origin A in the permissions policy. The auction with
  // bidder origin C should fail.
  {
    auto simulator =
        NavigationSimulator::CreateBrowserInitiated(kUrlA, web_contents());
    blink::ParsedPermissionsPolicy policy;
    policy.emplace_back(
        blink::mojom::PermissionsPolicyFeature::kPrivateAggregation,
        /*allowed_origins=*/
        std::vector<blink::OriginWithPossibleWildcards>{
            blink::OriginWithPossibleWildcards(
                kOriginA,
                /*has_subdomain_wildcard=*/false)},
        /*self_if_matches=*/absl::nullopt,
        /*matches_all_origins=*/false,
        /*matches_opaque_src=*/false);
    simulator->SetPermissionsPolicyHeader(std::move(policy));
    simulator->Commit();

    blink::AuctionConfig auction_config;
    auction_config.seller = kOriginA;
    auction_config.decision_logic_url = kUrlA.Resolve(kDecisionUrlPath);
    auction_config.non_shared_params.interest_group_buyers = {kOriginC};

    absl::optional<GURL> auction_result = RunAdAuctionAndFlush(auction_config);
    EXPECT_EQ(auction_result, absl::nullopt);
  }

  // In contrast to the case above, additionally allow origin C in the
  // permissions policy. The auction with bidder origin C should succeed.
  {
    auto simulator =
        NavigationSimulator::CreateBrowserInitiated(kUrlA, web_contents());
    blink::ParsedPermissionsPolicy policy;
    policy.emplace_back(
        blink::mojom::PermissionsPolicyFeature::kPrivateAggregation,
        /*allowed_origins=*/
        std::vector<blink::OriginWithPossibleWildcards>{
            blink::OriginWithPossibleWildcards(
                kOriginA,
                /*has_subdomain_wildcard=*/false),
            blink::OriginWithPossibleWildcards(
                kOriginC,
                /*has_subdomain_wildcard=*/false)},
        /*self_if_matches=*/absl::nullopt,
        /*matches_all_origins=*/false,
        /*matches_opaque_src=*/false);
    simulator->SetPermissionsPolicyHeader(std::move(policy));
    simulator->Commit();

    blink::AuctionConfig auction_config;
    auction_config.seller = kOriginA;
    auction_config.decision_logic_url = kUrlA.Resolve(kDecisionUrlPath);
    auction_config.non_shared_params.interest_group_buyers = {kOriginC};

    absl::optional<GURL> auction_result = RunAdAuctionAndFlush(auction_config);
    EXPECT_NE(auction_result, absl::nullopt);
  }
}

class PrivateAggregationUseCounterContentBrowserClient
    : public AllowInterestGroupContentBrowserClient {
 public:
  PrivateAggregationUseCounterContentBrowserClient() = default;
  ~PrivateAggregationUseCounterContentBrowserClient() override = default;

  // ContentBrowserClient:
  MOCK_METHOD(void,
              LogWebFeatureForCurrentPage,
              (content::RenderFrameHost*, blink::mojom::WebFeature),
              (override));
};

TEST_F(AdAuctionServiceImplPrivateAggregationEnabledTest,
       PrivateAggregationUseCountersLogged) {
  constexpr char kBiddingScript[] = R"(
function generateBid(
    interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
    browserSignals) {
  privateAggregation.contributeToHistogramOnEvent("reserved.win",
                                                  {bucket: 1n, value: 2});
  return {'ad': 'example', 'bid': 1, 'render': 'https://example.com/render'};
}
)";

  constexpr char kDecisionScript[] = R"(
function scoreAd(
    adMetadata, bid, auctionConfig, trustedScoringSignals, browserSignals) {
  return bid;
}
)";

  PrivateAggregationUseCounterContentBrowserClient browser_client;
  ScopedContentBrowserClientSetting setting(&browser_client);

  network_responder_->RegisterScriptResponse(kBiddingUrlPath, kBiddingScript);
  network_responder_->RegisterScriptResponse(kDecisionUrlPath, kDecisionScript);

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.bidding_url = kUrlA.Resolve(kBiddingUrlPath);
  interest_group.priority = 2;
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);

  blink::AuctionConfig auction_config;
  auction_config.seller = kOriginA;
  auction_config.decision_logic_url = kUrlA.Resolve(kDecisionUrlPath);
  auction_config.non_shared_params.interest_group_buyers = {kOriginA};

  EXPECT_CALL(
      browser_client,
      LogWebFeatureForCurrentPage(
          main_rfh(), blink::mojom::WebFeature::kPrivateAggregationApiAll));
  EXPECT_CALL(
      browser_client,
      LogWebFeatureForCurrentPage(
          main_rfh(), blink::mojom::WebFeature::kPrivateAggregationApiFledge));
  EXPECT_CALL(
      browser_client,
      LogWebFeatureForCurrentPage(
          main_rfh(),
          blink::mojom::WebFeature::kPrivateAggregationApiFledgeExtensions));

  absl::optional<GURL> auction_result = RunAdAuctionAndFlush(auction_config);
  ASSERT_NE(auction_result, absl::nullopt);
  InvokeCallbackForURN(*auction_result);
}

TEST_F(AdAuctionServiceImplPrivateAggregationEnabledTest,
       PrivateAggregationExtensionsUseCounterNotLoggedOnContributeToHistogram) {
  constexpr char kBiddingScript[] = R"(
function generateBid(
    interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
    browserSignals) {
  privateAggregation.contributeToHistogram({bucket: 1n, value: 2});
  return {'ad': 'example', 'bid': 1, 'render': 'https://example.com/render'};
}
)";

  constexpr char kDecisionScript[] = R"(
function scoreAd(
    adMetadata, bid, auctionConfig, trustedScoringSignals, browserSignals) {
  return bid;
}
)";

  PrivateAggregationUseCounterContentBrowserClient browser_client;
  ScopedContentBrowserClientSetting setting(&browser_client);

  network_responder_->RegisterScriptResponse(kBiddingUrlPath, kBiddingScript);
  network_responder_->RegisterScriptResponse(kDecisionUrlPath, kDecisionScript);

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.bidding_url = kUrlA.Resolve(kBiddingUrlPath);
  interest_group.priority = 2;
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);

  blink::AuctionConfig auction_config;
  auction_config.seller = kOriginA;
  auction_config.decision_logic_url = kUrlA.Resolve(kDecisionUrlPath);
  auction_config.non_shared_params.interest_group_buyers = {kOriginA};

  EXPECT_CALL(
      browser_client,
      LogWebFeatureForCurrentPage(
          main_rfh(), blink::mojom::WebFeature::kPrivateAggregationApiAll));
  EXPECT_CALL(
      browser_client,
      LogWebFeatureForCurrentPage(
          main_rfh(), blink::mojom::WebFeature::kPrivateAggregationApiFledge));
  EXPECT_CALL(
      browser_client,
      LogWebFeatureForCurrentPage(
          main_rfh(),
          blink::mojom::WebFeature::kPrivateAggregationApiFledgeExtensions))
      .Times(0);

  absl::optional<GURL> auction_result = RunAdAuctionAndFlush(auction_config);
  ASSERT_NE(auction_result, absl::nullopt);
  InvokeCallbackForURN(*auction_result);
}

// TODO(crbug.com/1356654): Update when use counter coverage is improved.
// TODO(crbug.com/1408576): Flaky.
TEST_F(AdAuctionServiceImplPrivateAggregationEnabledTest,
       DISABLED_PrivateAggregationUseCountersNotLoggedOnFailedInvocation) {
  constexpr char kBiddingScript[] = R"(
function generateBid(
    interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
    browserSignals) {
  privateAggregation.contributeToHistogram({});
  return {'ad': 'example', 'bid': 1, 'render': 'https://example.com/render'};
}
)";

  constexpr char kDecisionScript[] = R"(
function scoreAd(
    adMetadata, bid, auctionConfig, trustedScoringSignals, browserSignals) {
  return bid;
}
)";

  PrivateAggregationUseCounterContentBrowserClient browser_client;
  ScopedContentBrowserClientSetting setting(&browser_client);

  network_responder_->RegisterScriptResponse(kBiddingUrlPath, kBiddingScript);
  network_responder_->RegisterScriptResponse(kDecisionUrlPath, kDecisionScript);

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.bidding_url = kUrlA.Resolve(kBiddingUrlPath);
  interest_group.priority = 2;
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);

  blink::AuctionConfig auction_config;
  auction_config.seller = kOriginA;
  auction_config.decision_logic_url = kUrlA.Resolve(kDecisionUrlPath);
  auction_config.non_shared_params.interest_group_buyers = {kOriginA};

  EXPECT_CALL(
      browser_client,
      LogWebFeatureForCurrentPage(
          main_rfh(), blink::mojom::WebFeature::kPrivateAggregationApiAll))
      .Times(0);
  EXPECT_CALL(
      browser_client,
      LogWebFeatureForCurrentPage(
          main_rfh(), blink::mojom::WebFeature::kPrivateAggregationApiFledge))
      .Times(0);
  EXPECT_CALL(
      browser_client,
      LogWebFeatureForCurrentPage(
          main_rfh(),
          blink::mojom::WebFeature::kPrivateAggregationApiFledgeExtensions))
      .Times(0);
  absl::optional<GURL> auction_result = RunAdAuctionAndFlush(auction_config);

  // There should've been a contributeToHistogram() error.
  EXPECT_EQ(auction_result, absl::nullopt);
}

// Tests that the use counters are logged only once, even when the API is used
// multiple times (and different functions are used).
TEST_F(AdAuctionServiceImplPrivateAggregationEnabledTest,
       PrivateAggregationUseCountersLoggedOnlyOnce) {
  constexpr char kBiddingScript[] = R"(
function generateBid(
    interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
    browserSignals) {
  privateAggregation.contributeToHistogramOnEvent("reserved.win",
                                                  {bucket: 1n, value: 2});
  privateAggregation.contributeToHistogram({bucket: 3n, value: 4});
  return {'ad': 'example', 'bid': 1, 'render': 'https://example.com/render'};
}
)";

  constexpr char kDecisionScript[] = R"(
function scoreAd(
    adMetadata, bid, auctionConfig, trustedScoringSignals, browserSignals) {
  privateAggregation.contributeToHistogram({bucket: 5n, value: 6});
  privateAggregation.contributeToHistogramOnEvent("reserved.win",
                                                  {bucket: 7n, value: 8});
  return bid;
}
)";

  PrivateAggregationUseCounterContentBrowserClient browser_client;
  ScopedContentBrowserClientSetting setting(&browser_client);

  network_responder_->RegisterScriptResponse(kBiddingUrlPath, kBiddingScript);
  network_responder_->RegisterScriptResponse(kDecisionUrlPath, kDecisionScript);

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.bidding_url = kUrlA.Resolve(kBiddingUrlPath);
  interest_group.priority = 2;
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);

  blink::AuctionConfig auction_config;
  auction_config.seller = kOriginA;
  auction_config.decision_logic_url = kUrlA.Resolve(kDecisionUrlPath);
  auction_config.non_shared_params.interest_group_buyers = {kOriginA};

  EXPECT_CALL(
      browser_client,
      LogWebFeatureForCurrentPage(
          main_rfh(), blink::mojom::WebFeature::kPrivateAggregationApiAll));
  EXPECT_CALL(
      browser_client,
      LogWebFeatureForCurrentPage(
          main_rfh(), blink::mojom::WebFeature::kPrivateAggregationApiFledge));
  EXPECT_CALL(
      browser_client,
      LogWebFeatureForCurrentPage(
          main_rfh(),
          blink::mojom::WebFeature::kPrivateAggregationApiFledgeExtensions));

  absl::optional<GURL> auction_result = RunAdAuctionAndFlush(auction_config);
  ASSERT_NE(auction_result, absl::nullopt);
  InvokeCallbackForURN(*auction_result);
}

class AdAuctionServiceImplPrivateAggregationDisabledTest
    : public AdAuctionServiceImplTest {
 public:
  AdAuctionServiceImplPrivateAggregationDisabledTest() {
    feature_list_.InitAndDisableFeature(
        blink::features::kPrivateAggregationApi);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

// TODO(crbug.com/1408576): Flaky.
TEST_F(AdAuctionServiceImplPrivateAggregationDisabledTest,
       DISABLED_PrivateAggregationNotExposed) {
  constexpr char kBiddingScript[] = R"(
function generateBid(
    interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
    browserSignals) {
  privateAggregation.contributeToHistogram({bucket: 1n, value: 2});
  return {'ad': 'example', 'bid': 1, 'render': 'https://example.com/render'};
}
)";

  constexpr char kDecisionScript[] = R"(
function scoreAd(
    adMetadata, bid, auctionConfig, trustedScoringSignals, browserSignals) {
  return bid;
}
)";

  network_responder_->RegisterScriptResponse(kBiddingUrlPath, kBiddingScript);
  network_responder_->RegisterScriptResponse(kDecisionUrlPath, kDecisionScript);

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.bidding_url = kUrlA.Resolve(kBiddingUrlPath);
  interest_group.priority = 2;
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);

  blink::AuctionConfig auction_config;
  auction_config.seller = kOriginA;
  auction_config.decision_logic_url = kUrlA.Resolve(kDecisionUrlPath);
  auction_config.non_shared_params.interest_group_buyers = {kOriginA};

  absl::optional<GURL> auction_result = RunAdAuctionAndFlush(auction_config);

  // privateAggregation should cause a ReferenceError.
  EXPECT_EQ(auction_result, absl::nullopt);
}

// TODO(crbug.com/1408576): Flaky.
TEST_F(AdAuctionServiceImplPrivateAggregationDisabledTest,
       DISABLED_PrivateAggregationUseCounterNotLogged) {
  constexpr char kBiddingScript[] = R"(
function generateBid(
    interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
    browserSignals) {
  privateAggregation.contributeToHistogram({bucket: 1n, value: 2});
  return {'ad': 'example', 'bid': 1, 'render': 'https://example.com/render'};
}
)";

  constexpr char kDecisionScript[] = R"(
function scoreAd(
    adMetadata, bid, auctionConfig, trustedScoringSignals, browserSignals) {
  return bid;
}
)";

  PrivateAggregationUseCounterContentBrowserClient browser_client;
  ScopedContentBrowserClientSetting setting(&browser_client);

  network_responder_->RegisterScriptResponse(kBiddingUrlPath, kBiddingScript);
  network_responder_->RegisterScriptResponse(kDecisionUrlPath, kDecisionScript);

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.bidding_url = kUrlA.Resolve(kBiddingUrlPath);
  interest_group.priority = 2;
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);

  blink::AuctionConfig auction_config;
  auction_config.seller = kOriginA;
  auction_config.decision_logic_url = kUrlA.Resolve(kDecisionUrlPath);
  auction_config.non_shared_params.interest_group_buyers = {kOriginA};

  EXPECT_CALL(
      browser_client,
      LogWebFeatureForCurrentPage(
          testing::_, blink::mojom::WebFeature::kPrivateAggregationApiAll))
      .Times(0);
  EXPECT_CALL(
      browser_client,
      LogWebFeatureForCurrentPage(
          testing::_, blink::mojom::WebFeature::kPrivateAggregationApiFledge))
      .Times(0);

  absl::optional<GURL> auction_result = RunAdAuctionAndFlush(auction_config);

  // privateAggregation should cause a ReferenceError.
  EXPECT_EQ(auction_result, absl::nullopt);
}

class AdAuctionServiceImplKAnonTest
    : public AdAuctionServiceImplTest,
      public ::testing::WithParamInterface<
          auction_worklet::mojom::KAnonymityBidMode> {
 public:
  AdAuctionServiceImplKAnonTest() {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    switch (kanon_mode()) {
      case auction_worklet::mojom::KAnonymityBidMode::kEnforce:
        enabled_features.push_back(blink::features::kFledgeConsiderKAnonymity);
        enabled_features.push_back(blink::features::kFledgeEnforceKAnonymity);
        break;
      case auction_worklet::mojom::KAnonymityBidMode::kSimulate:
        enabled_features.push_back(blink::features::kFledgeConsiderKAnonymity);
        disabled_features.push_back(blink::features::kFledgeEnforceKAnonymity);
        break;
      case auction_worklet::mojom::KAnonymityBidMode::kNone:
        disabled_features.push_back(blink::features::kFledgeConsiderKAnonymity);
        disabled_features.push_back(blink::features::kFledgeEnforceKAnonymity);
        break;
    }

    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  auction_worklet::mojom::KAnonymityBidMode kanon_mode() { return GetParam(); }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Add an interest group with a non-k-anonymous ad and run an ad auction.
TEST_P(AdAuctionServiceImplKAnonTest, RunAdAuctionNotKAnon) {
  constexpr char kBiddingScript[] = R"(
function generateBid(
    interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
    browserSignals) {
  return {'ad': 'example', 'bid': 1, 'render': 'https://example.com/render'};
}
)";

  constexpr char kDecisionScript[] = R"(
function scoreAd(
    adMetadata, bid, auctionConfig, trustedScoringSignals, browserSignals) {
  return bid;
}
)";

  network_responder_->RegisterScriptResponse(kBiddingUrlPath, kBiddingScript);
  network_responder_->RegisterScriptResponse(kDecisionUrlPath, kDecisionScript);

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.bidding_url = kUrlA.Resolve(kBiddingUrlPath);
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad(
      /*render_url=*/GURL("https://example.com/render"),
      /*metadata=*/absl::nullopt);
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  blink::AuctionConfig auction_config;
  auction_config.seller = kOriginA;
  auction_config.decision_logic_url = kUrlA.Resolve(kDecisionUrlPath);
  auction_config.non_shared_params.interest_group_buyers = {kOriginA};
  absl::optional<GURL> auction_result = RunAdAuctionAndFlush(auction_config);
  switch (kanon_mode()) {
    case auction_worklet::mojom::KAnonymityBidMode::kNone:
    case auction_worklet::mojom::KAnonymityBidMode::kSimulate: {
      ASSERT_NE(auction_result, absl::nullopt);
      EXPECT_EQ(ConvertFencedFrameURNToURL(*auction_result),
                GURL("https://example.com/render"));

      // Running the auction alone should not result in updating the interest
      // group's bid count, previous win list or trigger k-anon joins, no matter
      // how much time passes.
      task_environment()->RunUntilIdle();
      auto storage_interest_group =
          GetInterestGroup(interest_group.owner, interest_group.name);
      ASSERT_TRUE(storage_interest_group);
      EXPECT_EQ(0, storage_interest_group->bidding_browser_signals->bid_count);
      EXPECT_EQ(
          0u,
          storage_interest_group->bidding_browser_signals->prev_wins.size());
      EXPECT_THAT(GetKAnonJoinedIds(), ::testing::UnorderedElementsAre());

      // Invoking the URN callback (which is done when the result is loaded in a
      // frame) updates those fields.
      InvokeCallbackForURN(*auction_result);
      storage_interest_group =
          GetInterestGroup(interest_group.owner, interest_group.name);
      ASSERT_TRUE(storage_interest_group);
      EXPECT_EQ(1, storage_interest_group->bidding_browser_signals->bid_count);
      ASSERT_EQ(
          1u,
          storage_interest_group->bidding_browser_signals->prev_wins.size());
      ASSERT_EQ(R"({"render_url":"https://example.com/render"})",
                storage_interest_group->bidding_browser_signals->prev_wins[0]
                    ->ad_json);
      EXPECT_THAT(
          GetKAnonJoinedIds(),
          ::testing::UnorderedElementsAre(
              KAnonKeyForAdBid(interest_group,
                               interest_group.ads.value()[0].render_url),
              KAnonKeyForAdNameReporting(interest_group,
                                         interest_group.ads.value()[0])));
      break;
    }
    case auction_worklet::mojom::KAnonymityBidMode::kEnforce: {
      // The auction should fail because there were no k-anonymous bids.
      // Since the auction failed, everything should update immediately.
      EXPECT_FALSE(auction_result);
      task_environment()->RunUntilIdle();
      EXPECT_THAT(
          GetKAnonJoinedIds(),
          ::testing::UnorderedElementsAre(
              KAnonKeyForAdBid(interest_group,
                               interest_group.ads.value()[0].render_url),
              KAnonKeyForAdNameReporting(interest_group,
                                         interest_group.ads.value()[0])));
      break;
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    /* no label */,
    AdAuctionServiceImplKAnonTest,
    ::testing::Values(auction_worklet::mojom::KAnonymityBidMode::kNone,
                      auction_worklet::mojom::KAnonymityBidMode::kSimulate,
                      auction_worklet::mojom::KAnonymityBidMode::kEnforce));

}  // namespace content
