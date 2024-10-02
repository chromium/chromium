// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Provides a browsertest-based tool for running simulated auctions with
// custom parameters. The tool is useful for profiling hypothetical scenarios
// locally.

#include <cstddef>
#include <fstream>
#include <string_view>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gtest_util.h"
#include "base/test/launcher/test_launcher.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_suite.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/browser/interest_group/ad_auction_service_impl.h"
#include "content/browser/interest_group/interest_group_manager_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_shell_main_delegate.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/content_test_suite_base.h"
#include "content/public/test/test_launcher.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "content/shell/common/shell_switches.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/interest_group/test_interest_group_builder.h"

namespace content {

namespace {

constexpr char kScoringLogicRelativeURL[] =
    "/interest_group/generated_decision_logic.js";

constexpr char kBiddingLogicRelativeURL[] =
    "/interest_group/generated_bidding_logic.js";

constexpr char kBiddingSignalsRelativeURL[] =
    "/interest_group/generated_trusted_bidding_signals.json";

constexpr char kScoringSignalsRelativeURL[] =
    "/interest_group/generated_trusted_scoring_signals.json";

constexpr char kFledgeScriptHeaders[] =
    "HTTP/1.1 200 OK\n"
    "Ad-Auction-Allowed: true\n";

// This function is a delay mechanism for bidding and scoring scripts. This
// function is used because Date() is disabled in worklets.
constexpr char kStallingJavascriptFunction[] = R"(
  function runNAdditions(script_delay){
    var i = 0;
    for (var index = 0; index < script_delay; ++index) {
      i = 67 + 239;
    }
  })";

class AllowAllContentBrowserClient
    : public ContentBrowserTestContentBrowserClient {
 public:
  explicit AllowAllContentBrowserClient() = default;

  AllowAllContentBrowserClient(const AllowAllContentBrowserClient&) = delete;
  AllowAllContentBrowserClient& operator=(const AllowAllContentBrowserClient&) =
      delete;

  // ContentBrowserClient overrides:
  bool IsInterestGroupAPIAllowed(
      content::RenderFrameHost* render_frame_host,
      ContentBrowserClient::InterestGroupApiOperation operation,
      const url::Origin& top_frame_origin,
      const url::Origin& api_origin) override {
    return true;
  }

  bool IsPrivacySandboxReportingDestinationAttested(
      content::BrowserContext* browser_context,
      const url::Origin& destination_origin,
      content::PrivacySandboxInvokingAPI invoking_api) override {
    return true;
  }
};

class NetworkResponder {
 public:
  using ResponseHeaders = std::vector<std::pair<std::string, std::string>>;

  NetworkResponder() = default;

  NetworkResponder(const NetworkResponder&) = delete;
  NetworkResponder& operator=(const NetworkResponder&) = delete;

  void RegisterScoringScript(const std::string& url_path,
                             int script_delay,
                             int network_delay) {
    std::string script =
        kStallingJavascriptFunction + base::StringPrintf(R"(
    function scoreAd(adMetadata, bid, auctionConfig, trustedScoringSignals,
                 browserSignals) {
      runNAdditions(%d);
      return {desirability: bid, allowComponentAuction: true};
})",
                                                         script_delay);
    RegisterNetworkResponse(url_path, kStallingJavascriptFunction + script,
                            "application/javascript", network_delay);
  }

  void RegisterBidderScript(const std::string& url_path,
                            int script_delay,
                            int network_delay) {
    std::string script =
        kStallingJavascriptFunction + base::StringPrintf(R"(
function generateBid(
    interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
    unusedBrowserSignals) {
    const ad = interestGroup.ads[0];
    let result = {'ad': ad, 'bid': 1, 'render': ad.renderURL,
                  'allowComponentAuction': true};
    runNAdditions(%d);
    return result;
})",
                                                         script_delay);
    RegisterNetworkResponse(url_path, script, "application/javascript",
                            network_delay);
  }

  void RegisterTrustedBiddingSignals(const std::string& url_path,
                                     int network_delay) {
    std::string json_response = R"({ "keys": { "key1": "1" } })";
    RegisterNetworkResponse(url_path, json_response, "application/json",
                            network_delay,
                            "Ad-Auction-Bidding-Signals-Format-Version: 2\n");
  }

  void RegisterTrustedScoringSignals(const std::string& url_path,
                                     int network_delay) {
    std::string json_response = R"({ "keys": { "key1": "1" } })";
    RegisterNetworkResponse(url_path, json_response, "application/json",
                            network_delay);
  }

 private:
  struct Response {
    std::string body;
    std::string extra_headers;
    std::string content_type;
    base::TimeDelta network_delay;
  };

  void RegisterNetworkResponse(const std::string& url_path,
                               const std::string& body,
                               const std::string& content_type,
                               int network_delay,
                               std::string extra_headers = "") {
    Response response;
    response.body = body;
    response.content_type = content_type;
    response.extra_headers = extra_headers;
    response.network_delay = base::Milliseconds(network_delay);
    base::AutoLock auto_lock(response_map_lock_);
    response_map_[url_path] = response;
  }

  bool RequestHandler(URLLoaderInterceptor::RequestParams* params) {
    base::AutoLock auto_lock(response_map_lock_);
    const auto it = response_map_.find(params->url_request.url.path());
    if (it == response_map_.end()) {
      return false;
    }
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&NetworkResponder::DoDelayedResponse, it->second.body,
                       it->second.content_type, it->second.extra_headers,
                       std::move(params->client)),
        it->second.network_delay);
    return true;
  }

  static void DoDelayedResponse(
      std::string body,
      std::string content_type,
      std::string extra_headers,
      const mojo::Remote<network::mojom::URLLoaderClient> client) {
    URLLoaderInterceptor::WriteResponse(
        base::StrCat({kFledgeScriptHeaders, extra_headers,
                      "Content-type: ", content_type, "\n"}),
        body, client.get());
  }

  base::Lock response_map_lock_;

  // For each HTTPS request, we see if any path in the map matches the request
  // path. If so, the server returns the mapped value string as the response.
  base::flat_map<std::string, Response> response_map_
      GUARDED_BY(response_map_lock_);

  // Handles all network requests.
  URLLoaderInterceptor network_interceptor_{
      base::BindRepeating(&NetworkResponder::RequestHandler,
                          base::Unretained(this))};
};

class AdjustableAuction : public ContentBrowserTest {
 public:
  inline static std::string filename_to_save_histogram = "histograms.txt";

  // Wait between joining interest groups and the first auction, measured in
  // seconds.
  inline static size_t kPreauctionLogicDelay = 5;

  inline static size_t kAdsPerInterestGroup = 30;
  inline static size_t kInterestGroupsPerOwner = 100;
  inline static size_t kSellers = 1;
  inline static size_t kOwners = 2;

  // Script delays measured in number of operations.
  inline static size_t kBiddingLogicDelay = 20000000;
  inline static size_t kScoringLogicDelay = 10000000;

  // Network delays measured in milliseconds.
  inline static size_t kBiddingNetworkDelay = 100;
  inline static size_t kScoringNetworkDelay = 80;
  inline static size_t kTrustedBiddingSignalsDelay = 200;
  inline static size_t kTrustedScoringSignalsDelay = 100;

  inline static size_t kAuctions = 1;
  inline static bool kAuctionsAreParallel = false;

  AdjustableAuction() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {blink::features::kInterestGroupStorage,
         blink::features::kAdInterestGroupAPI, blink::features::kFledge,
         blink::features::kAllowURNsInIframes,
         blink::features::kFledgeDirectFromSellerSignalsHeaderAdSlot},
        /*disabled_features=*/
        {blink::features::kFledgeEnforceKAnonymity,
         features::kCookieDeprecationFacilitatedTesting});
  }

  ~AdjustableAuction() override { content_browser_client_.reset(); }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::test_server::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_->AddDefaultHandlers(GetTestDataFilePath());
    network_responder_ = std::make_unique<NetworkResponder>();
    ASSERT_TRUE(https_server_->Start());
    manager_ = static_cast<InterestGroupManagerImpl*>(
        shell()
            ->web_contents()
            ->GetBrowserContext()
            ->GetDefaultStoragePartition()
            ->GetInterestGroupManager());
    content_browser_client_ = std::make_unique<AllowAllContentBrowserClient>();
  }

  void TearDownOnMainThread() override {
    manager_ = nullptr;  // don't dangle once StoragePartition cleans it up.
    network_responder_.reset();
    ContentBrowserTest::TearDownOnMainThread();
  }

  content::EvalJsResult RunAuctionsAndWait(
      const std::string& auction_config_json,
      int n_auctions) {
    return EvalJs(shell(), base::StringPrintf(
                               R"(
(async function() {
  let auctionConfig = %s;
  let nAuctions = %d;
  try {
    for (var i = 0; i < nAuctions; ++i){
      await navigator.runAdAuction(auctionConfig);
    }
  } catch (e) {
    return e.toString();
  }
})())",
                               auction_config_json.c_str(), n_auctions));
  }

  content::EvalJsResult RunParallelAuctionsAndWait(
      const std::string& auction_config_json,
      int n_auctions) {
    return EvalJs(shell(), base::StringPrintf(
                               R"(
(async function() {
  let auctionConfig = %s;
  let nAuctions = %d;
  try {
    const auction_configs = Array(nAuctions).fill(auctionConfig);
    return await Promise.allSettled(auction_configs.map(async (t) =>
     {await navigator.runAdAuction(t)}));
  } catch (e) {
    return e.toString();
  }
})())",
                               auction_config_json.c_str(), n_auctions));
  }

 protected:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<AllowAllContentBrowserClient> content_browser_client_;
  raw_ptr<InterestGroupManagerImpl> manager_;

  std::unique_ptr<NetworkResponder> network_responder_;
};

IN_PROC_BROWSER_TEST_F(AdjustableAuction, RunAdjustableAuction) {
  GURL test_url = https_server_->GetURL("a.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  // Set up network responses.
  network_responder_->RegisterScoringScript(
      kScoringLogicRelativeURL, kScoringLogicDelay, kScoringNetworkDelay);
  network_responder_->RegisterBidderScript(
      kBiddingLogicRelativeURL, kBiddingLogicDelay, kBiddingNetworkDelay);
  network_responder_->RegisterTrustedBiddingSignals(
      kBiddingSignalsRelativeURL, kTrustedBiddingSignalsDelay);
  network_responder_->RegisterTrustedScoringSignals(
      kScoringSignalsRelativeURL, kTrustedScoringSignalsDelay);

  // Join interest groups before running any auctions.
  // Keep the buyers in a Value so that we can put this directly into the
  // auction config.
  base::Value::List buyers_for_auction;
  for (size_t owner_i = 0; owner_i < kOwners; ++owner_i) {
    std::string owner_str =
        base::StrCat({"origin", base::NumberToString(owner_i), ".b.test"});
    auto bidding_url =
        https_server_->GetURL(owner_str, kBiddingLogicRelativeURL);
    url::Origin owner_origin = url::Origin::Create(bidding_url);

    buyers_for_auction.Append(owner_origin.GetURL().spec());
    GURL bidding_signals_url =
        https_server_->GetURL(owner_str, kBiddingSignalsRelativeURL);

    for (size_t ig_i = 0; ig_i < kInterestGroupsPerOwner; ++ig_i) {
      std::vector<blink::InterestGroup::Ad> ads;
      for (size_t ad_i = 0; ad_i < kAdsPerInterestGroup; ++ad_i) {
        GURL ad_url = https_server_->GetURL(
            "c.test", base::StrCat({"/test?test_render_url_here_and_some_more_"
                                    "words_for_extra_length",
                                    base::NumberToString(ad_i)}));

        ads.emplace_back(ad_url, R"({"ad":"metadata","here":[1,2,3,4,5]})");
      }
      manager_->JoinInterestGroup(
          blink::TestInterestGroupBuilder(
              /*owner=*/owner_origin,
              /*name=*/base::NumberToString(ig_i))
              .SetBiddingUrl(bidding_url)
              .SetTrustedBiddingSignalsUrl(bidding_signals_url)
              .SetTrustedBiddingSignalsKeys({{"key1"}})
              .SetAds(ads)
              .Build(),
          owner_origin.GetURL());
    }
  }

  // Sleeping here helps ensure all interest groups are joined before the
  // auction starts. If we're profiling these auctions with perf, we can also
  // set a matching delay there to skip profiling the set up of the test and
  // auction.
  base::PlatformThread::Sleep(base::Seconds(kPreauctionLogicDelay));

  // Set up the auction config.
  auto scoring_url = https_server_->GetURL("a.test", kScoringLogicRelativeURL);
  auto scoring_signals_url =
      https_server_->GetURL("a.test", kScoringSignalsRelativeURL);
  // The resulting config will combine `config_template`, repeated `component`s,
  // and `config_template_end`.
  std::string config_template = R"({
        seller: $1,
        decisionLogicURL: $2,
        componentAuctions: [)";
  std::string component = R"({
    seller: $1,
    decisionLogicURL: $2,
    trustedScoringSignalsURL: $3,
    interestGroupBuyers: $4,
    auctionSignals: {x: 1},
    sellerSignals: {yet: 'more', info: 1},
    sellerTimeout: 200,
    perBuyerSignals: {$1: {even: 'more', x: 4.5}},
    perBuyerTimeouts: {$1: 100, '*': 150}
    })";
  std::string config_template_end = R"(]
      })";
  for (size_t i = 0; i < kSellers; ++i) {
    std::string end = i < kSellers - 1 ? "," : config_template_end;
    config_template = base::StrCat({config_template, component, end});
  }
  std::string auction_config =
      JsReplace(config_template, url::Origin::Create(test_url), scoring_url,
                scoring_signals_url, std::move(buyers_for_auction));

  // Run the auction(s).
  base::HistogramTester histogram_tester;
  if (kAuctionsAreParallel) {
    RunParallelAuctionsAndWait(auction_config, kAuctions);
  } else {
    RunAuctionsAndWait(auction_config, kAuctions);
  }

  // Verify and save results.
  histogram_tester.ExpectTotalCount("Ads.InterestGroup.Auction.LoadGroupsTime",
                                    kAuctions * (kSellers + 1));
  histogram_tester.ExpectBucketCount(
      "Ads.InterestGroup.Auction.NumInterestGroups",
      kInterestGroupsPerOwner * kOwners * kSellers, kAuctions);
  histogram_tester.ExpectTotalCount("Ads.InterestGroup.Auction.Result",
                                    kAuctions);
  histogram_tester.ExpectTotalCount(
      "Ads.InterestGroup.Auction.AuctionWithWinnerTime", kAuctions);

  content::FetchHistogramsFromChildProcesses();
  std::ofstream histogram_file(filename_to_save_histogram);
  histogram_file << histogram_tester.GetAllHistogramsRecorded();
  histogram_file.close();
  ASSERT_TRUE(histogram_file.good());
}

}  // namespace

// Creating our own ContentTestSuiteBase and TestLauncherDelegate allows us to
// specify our own main function while still running the test above.
class AdjustableAuctionTestSuite : public ContentTestSuiteBase {
 public:
  AdjustableAuctionTestSuite(int argc, char** argv)
      : ContentTestSuiteBase(argc, argv) {}

  AdjustableAuctionTestSuite(const AdjustableAuctionTestSuite&) = delete;
  AdjustableAuctionTestSuite& operator=(const AdjustableAuctionTestSuite&) =
      delete;

  ~AdjustableAuctionTestSuite() override = default;

 protected:
  void Initialize() override {
    // Browser tests are expected not to tear-down various globals.
    base::TestSuite::DisableCheckForLeakedGlobals();

    ContentTestSuiteBase::Initialize();

#if BUILDFLAG(IS_ANDROID)
    RegisterInProcessThreads();
#endif
  }
};

class AdjustableAuctionTestLauncherDelegate : public TestLauncherDelegate {
 public:
  AdjustableAuctionTestLauncherDelegate() = default;

  AdjustableAuctionTestLauncherDelegate(
      const AdjustableAuctionTestLauncherDelegate&) = delete;
  AdjustableAuctionTestLauncherDelegate& operator=(
      const AdjustableAuctionTestLauncherDelegate&) = delete;

  ~AdjustableAuctionTestLauncherDelegate() override = default;

  int RunTestSuite(int argc, char** argv) override {
    return AdjustableAuctionTestSuite(argc, argv).Run();
  }

  std::string GetUserDataDirectoryCommandLineSwitch() override {
    return switches::kContentShellUserDataDir;
    ;
  }

 protected:
#if !BUILDFLAG(IS_ANDROID)
  ContentMainDelegate* CreateContentMainDelegate() override {
    return new ContentBrowserTestShellMainDelegate();
  }
#endif
};

}  // namespace content

void SetUpSizeTCommandlineArg(std::string switch_name, size_t* output) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switch_name)) {
    std::string param =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switch_name);
    base::StringToSizeT(param, output);
  }
}

void SetUpCommandLineArgs() {
  // Filename to save UMA histograms from the auction.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch("hist-filename")) {
    content::AdjustableAuction::filename_to_save_histogram =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            "hist-filename");
  }
  // Whether to run auctions in parallel (or series) if more than one auction
  // is specified.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch("auctions-parallel")) {
    content::AdjustableAuction::kAuctionsAreParallel = true;
  }

  // The number of owners in the auction.
  SetUpSizeTCommandlineArg("owners", &content::AdjustableAuction::kOwners);
  // The number of ads per interest group.
  SetUpSizeTCommandlineArg("ads-per-ig",
                           &content::AdjustableAuction::kAdsPerInterestGroup);
  // The number of interest groups per owner.
  SetUpSizeTCommandlineArg(
      "ig-per-owner", &content::AdjustableAuction::kInterestGroupsPerOwner);
  // The number of sellers participating in the auction.
  SetUpSizeTCommandlineArg("sellers", &content::AdjustableAuction::kSellers);
  // The number of top level auctions to run.
  SetUpSizeTCommandlineArg("n-auctions",
                           &content::AdjustableAuction::kAuctions);

  // The number of seconds to wait between joining interest groups and starting
  // the auction. This delay is necessary to ensure that the database is not
  // still busy joining interest groups when the auction starts.
  SetUpSizeTCommandlineArg("preauction-delay",
                           &content::AdjustableAuction::kPreauctionLogicDelay);
  // The number of additional operations to run in the bidding script to
  // simulate various bidding script durations.
  SetUpSizeTCommandlineArg("bidding-delay",
                           &content::AdjustableAuction::kBiddingLogicDelay);
  // The number of additional operations to run in the scoring script to
  // simulate various scoring script durations.
  SetUpSizeTCommandlineArg("scoring-delay",
                           &content::AdjustableAuction::kScoringLogicDelay);
  // A simulated "network" delay in downloading a bidding script, measured in
  // milliseconds.
  SetUpSizeTCommandlineArg("bidding-network-delay",
                           &content::AdjustableAuction::kBiddingNetworkDelay);
  // A simulated "network" delay in downloading a scoring script, measured in
  // milliseconds.
  SetUpSizeTCommandlineArg("scoring-network-delay",
                           &content::AdjustableAuction::kScoringNetworkDelay);
  // A simulated "network" delay in downloading trusted bidding signals,
  // measured in milliseconds.
  SetUpSizeTCommandlineArg(
      "trusted-bidding-delay",
      &content::AdjustableAuction::kTrustedBiddingSignalsDelay);
  // A simulated "network" delay in downloading trusted scoring signals,
  // measured in milliseconds.
  SetUpSizeTCommandlineArg(
      "trusted-scoring-delay",
      &content::AdjustableAuction::kTrustedScoringSignalsDelay);
}

int main(int argc, char* argv[]) {
  base::CommandLine::Init(argc, argv);
  SetUpCommandLineArgs();

  size_t parallel_jobs = base::NumParallelJobs(/*cores_per_job=*/2);
  if (parallel_jobs == 0U) {
    return 1;
  }

#if BUILDFLAG(IS_WIN)
  // Load and pin user32.dll to avoid having to load it once tests start while
  // on the main thread loop where blocking calls are disallowed.
  base::win::PinUser32();
#endif  // BUILDFLAG(IS_WIN)
  content::AdjustableAuctionTestLauncherDelegate launcher_delegate;
  return LaunchTests(&launcher_delegate, parallel_jobs, argc, argv);
}
