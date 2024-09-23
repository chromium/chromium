// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/federated_auth_disconnect_request.h"

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/webid/fedcm_metrics.h"
#include "content/browser/webid/test/mock_api_permission_delegate.h"
#include "content/browser/webid/test/mock_idp_network_request_manager.h"
#include "content/browser/webid/test/mock_permission_delegate.h"
#include "content/browser/webid/webid_utils.h"
#include "content/public/test/navigation_simulator.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "net/http/http_status_code.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

using PermissionStatus =
    content::FederatedIdentityApiPermissionContextDelegate::PermissionStatus;
using FetchStatus = content::IdpNetworkRequestManager::FetchStatus;
using ParseStatus = content::IdpNetworkRequestManager::ParseStatus;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

using DisconnectResponse =
    content::IdpNetworkRequestManager::DisconnectResponse;
using DisconnectStatusForMetrics = content::FedCmDisconnectStatus;
using LoginState = content::IdentityRequestAccount::LoginState;
using blink::mojom::DisconnectStatus;

namespace content {
namespace {

constexpr char kRpUrl[] = "https://rp.example";
constexpr char kProviderUrl[] = "https://idp.example/fedcm.json";
constexpr char kAccountsEndpoint[] = "https://idp.example/accounts";
constexpr char kDisconnectEndpoint[] = "https://idp.example/disconnect";
constexpr char kTokenEndpoint[] = "https://idp.example/token";
constexpr char kLoginUrl[] = "https://idp.example/login";
constexpr char kClientId[] = "client_id_123";

// Not used?
// constexpr char kIdpDisconnectUrl[] = "https://idp.example/disconnect";

struct AccountConfig {
  std::string id;
  std::optional<content::IdentityRequestAccount::LoginState> login_state;
  bool was_granted_sharing_permission;
};

struct Config {
  std::vector<AccountConfig> accounts;
  FetchStatus config_fetch_status;
  FetchStatus disconnect_fetch_status;
  std::string config_url;
};

Config kValidConfig = {
    /*accounts=*/
    {{"account1", /*login_state=*/std::nullopt,
      /*was_granted_sharing_permission=*/true}},
    /*config_fetch_status=*/{ParseStatus::kSuccess, net::HTTP_OK},
    /*disconnect_fetch_status=*/{ParseStatus::kSuccess, net::HTTP_OK},
    kProviderUrl};

url::Origin OriginFromString(const std::string& url_string) {
  return url::Origin::Create(GURL(url_string));
}

// Helper class for receiving the Disconnect method callback.
class DisconnectRequestCallbackHelper {
 public:
  DisconnectRequestCallbackHelper() = default;
  ~DisconnectRequestCallbackHelper() = default;

  DisconnectRequestCallbackHelper(const DisconnectRequestCallbackHelper&) =
      delete;
  DisconnectRequestCallbackHelper& operator=(
      const DisconnectRequestCallbackHelper&) = delete;

  DisconnectStatus status() const { return status_; }

  // This can only be called once per lifetime of this object.
  base::OnceCallback<void(DisconnectStatus)> callback() {
    return base::BindOnce(&DisconnectRequestCallbackHelper::ReceiverMethod,
                          base::Unretained(this));
  }

  // Returns when callback() is called, which can be immediately if it has
  // already been called.
  void WaitForCallback() {
    if (was_called_) {
      return;
    }
    wait_for_callback_loop_.Run();
  }

 private:
  void ReceiverMethod(DisconnectStatus status) {
    status_ = status;
    was_called_ = true;
    wait_for_callback_loop_.Quit();
  }

  bool was_called_ = false;
  base::RunLoop wait_for_callback_loop_;
  DisconnectStatus status_;
};

class TestIdpNetworkRequestManager : public MockIdpNetworkRequestManager {
 public:
  explicit TestIdpNetworkRequestManager(const Config& config)
      : config_(config) {}
  ~TestIdpNetworkRequestManager() override = default;

  void FetchWellKnown(const GURL& provider,
                      FetchWellKnownCallback callback) override {
    has_fetched_well_known_ = true;
    FetchStatus fetch_status = {ParseStatus::kSuccess, net::HTTP_OK};
    IdpNetworkRequestManager::WellKnown well_known;
    std::set<GURL> well_known_urls = {GURL(config_.config_url)};
    well_known.provider_urls = std::move(well_known_urls);
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), fetch_status, well_known));
  }

  void FetchConfig(const GURL& provider,
                   blink::mojom::RpMode rp_mode,
                   int idp_brand_icon_ideal_size,
                   int idp_brand_icon_minimum_size,
                   FetchConfigCallback callback) override {
    has_fetched_config_ = true;

    IdpNetworkRequestManager::Endpoints endpoints;
    endpoints.accounts = GURL(kAccountsEndpoint);
    endpoints.token = GURL(kTokenEndpoint);
    endpoints.disconnect = GURL(kDisconnectEndpoint);

    IdentityProviderMetadata idp_metadata;
    idp_metadata.config_url = GURL(config_.config_url);
    idp_metadata.idp_login_url = GURL(kLoginUrl);
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), config_.config_fetch_status,
                       endpoints, idp_metadata));
  }

  void SendDisconnectRequest(const GURL& disconnect_url,
                             const std::string& account_hint,
                             const std::string& client_id,
                             DisconnectCallback callback) override {
    has_fetched_disconnect_ = true;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), config_.disconnect_fetch_status,
                       account_hint));
  }

  bool has_fetched_well_known_{false};
  bool has_fetched_config_{false};
  bool has_fetched_disconnect_{false};

 private:
  const Config config_;
};

class TestPermissionDelegate : public MockPermissionDelegate {
 public:
  std::optional<bool> GetIdpSigninStatus(
      const url::Origin& idp_origin) override {
    return true;
  }
};

}  // namespace

class FederatedAuthDisconnectRequestTest
    : public RenderViewHostImplTestHarness {
 public:
  FederatedAuthDisconnectRequestTest() {
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }
  ~FederatedAuthDisconnectRequestTest() override = default;

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();

    api_permission_delegate_ = std::make_unique<MockApiPermissionDelegate>();
    permission_delegate_ = std::make_unique<TestPermissionDelegate>();

    static_cast<TestWebContents*>(web_contents())
        ->NavigateAndCommit(GURL(kRpUrl), ui::PAGE_TRANSITION_LINK);
  }

  void TearDown() override {
    network_manager_ = nullptr;
    RenderViewHostImplTestHarness::TearDown();
  }

  void RunDisconnectTest(const Config& config,
                         DisconnectStatus expected_disconnect_status,
                         RenderFrameHost* rfh = nullptr) {
    auto network_manager =
        std::make_unique<TestIdpNetworkRequestManager>(config);
    network_manager_ = network_manager.get();

    if (!rfh) {
      rfh = static_cast<RenderFrameHost*>(main_test_rfh());
    }
    if (expected_disconnect_status != DisconnectStatus::kSuccess) {
      EXPECT_CALL(*permission_delegate_, RevokeSharingPermission(_, _, _, _))
          .Times(0);
    }

    metrics_ = std::make_unique<FedCmMetrics>(rfh->GetPageUkmSourceId());

    blink::mojom::IdentityCredentialDisconnectOptionsPtr options =
        blink::mojom::IdentityCredentialDisconnectOptions::New();
    options->config = blink::mojom::IdentityProviderConfig::New();
    options->config->config_url = GURL(config.config_url);
    options->config->client_id = kClientId;
    options->account_hint = "accountHint";

    DisconnectRequestCallbackHelper callback_helper;
    request_ = FederatedAuthDisconnectRequest::Create(
        std::move(network_manager), permission_delegate_.get(), rfh,
        metrics_.get(), std::move(options));
    request_->SetCallbackAndStart(callback_helper.callback(),
                                  api_permission_delegate_.get());
    callback_helper.WaitForCallback();

    EXPECT_EQ(expected_disconnect_status, callback_helper.status());
  }

  void ExpectDisconnectMetricsAndConsoleError(
      DisconnectStatusForMetrics status,
      FedCmRequesterFrameType requester_frame_type,
      bool should_record_duration) {
    histogram_tester_.ExpectUniqueSample("Blink.FedCm.Status.Disconnect",
                                         status, 1);
    histogram_tester_.ExpectUniqueSample("Blink.FedCm.Disconnect.FrameType",
                                         requester_frame_type, 1);
    histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.Disconnect",
                                       should_record_duration ? 1 : 0);
    ExpectDisconnectUKM(ukm::builders::Blink_FedCm::kEntryName, status,
                        requester_frame_type, should_record_duration);
    ExpectDisconnectUKM(ukm::builders::Blink_FedCmIdp::kEntryName, status,
                        requester_frame_type, should_record_duration);

    std::vector<std::string> messages =
        RenderFrameHostTester::For(main_rfh())->GetConsoleMessages();
    if (status == DisconnectStatusForMetrics::kSuccess) {
      EXPECT_TRUE(messages.empty());
    } else {
      ASSERT_EQ(messages.size(), 1u);
      EXPECT_EQ(messages[0], webid::GetDisconnectConsoleErrorMessage(status));
    }
  }

  void ExpectDisconnectUKM(const char* entry_name,
                           DisconnectStatusForMetrics status,
                           FedCmRequesterFrameType requester_frame_type,
                           bool should_record_duration) {
    auto entries = ukm_recorder()->GetEntriesByName(entry_name);

    ASSERT_FALSE(entries.empty())
        << "No " << entry_name << " entry was recorded";

    // There are multiple types of metrics under the same FedCM UKM. We need to
    // make sure that the metric only includes the expected one.
    bool metric_found = false;
    for (const ukm::mojom::UkmEntry* const entry : entries) {
      if (!should_record_duration) {
        EXPECT_FALSE(ukm_recorder()->GetEntryMetric(entry, "Timing.Disconnect"))
            << "Timing.Disconnect must not be present";
      }
      const int64_t* metric =
          ukm_recorder()->GetEntryMetric(entry, "Status.Disconnect");
      if (!metric) {
        EXPECT_FALSE(ukm_recorder()->GetEntryMetric(entry, "Timing.Disconnect"))
            << "Timing.Disconnect must not be present when Status is not "
               "present";
        EXPECT_FALSE(
            ukm_recorder()->GetEntryMetric(entry, "Disconnect.FrameType"))
            << "Disconnect.FrameType must not be present when Status is not "
               "present";
        continue;
      }
      EXPECT_FALSE(metric_found)
          << "Found more than one entry with Status.Disconnect in "
          << entry_name;
      metric_found = true;
      EXPECT_EQ(static_cast<int>(status), *metric)
          << "Unexpected status recorded in " << entry_name;

      metric = ukm_recorder()->GetEntryMetric(entry, "Disconnect.FrameType");
      ASSERT_TRUE(metric)
          << "Disconnect.FrameType must be present when Status is present";
      EXPECT_EQ(static_cast<int>(requester_frame_type), *metric)
          << "Unexpected frame type recorded in " << entry_name;

      if (should_record_duration) {
        EXPECT_TRUE(ukm_recorder()->GetEntryMetric(entry, "Timing.Disconnect"))
            << "Timing.Disconnect must be present in the same entry as "
               "Status.Disconnect";
      }
    }
    EXPECT_TRUE(metric_found)
        << "No Status.Disconnect entry was found in " << entry_name;
  }

  bool DidFetchAnyEndpoint() {
    return network_manager_->has_fetched_well_known_ ||
           network_manager_->has_fetched_config_ ||
           network_manager_->has_fetched_disconnect_;
  }

  bool DidFetchAllEndpoints() {
    return network_manager_->has_fetched_well_known_ &&
           network_manager_->has_fetched_config_ &&
           network_manager_->has_fetched_disconnect_;
  }

  ukm::TestAutoSetUkmRecorder* ukm_recorder() { return ukm_recorder_.get(); }

 protected:
  raw_ptr<TestIdpNetworkRequestManager> network_manager_;
  std::unique_ptr<MockApiPermissionDelegate> api_permission_delegate_;
  std::unique_ptr<TestPermissionDelegate> permission_delegate_;
  std::unique_ptr<FedCmMetrics> metrics_;
  std::unique_ptr<FederatedAuthDisconnectRequest> request_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
};

TEST_F(FederatedAuthDisconnectRequestTest, Success) {
  Config config = kValidConfig;
  EXPECT_CALL(
      *permission_delegate_,
      HasSharingPermission(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrl)))
      .WillOnce(Return(true));
  EXPECT_CALL(*permission_delegate_,
              RevokeSharingPermission(OriginFromString(kRpUrl),
                                      OriginFromString(kRpUrl),
                                      OriginFromString(kProviderUrl), _));
  EXPECT_CALL(*api_permission_delegate_,
              GetApiPermissionStatus(OriginFromString(kRpUrl)))
      .WillOnce(Return(PermissionStatus::GRANTED));

  RunDisconnectTest(config, DisconnectStatus::kSuccess);
  EXPECT_TRUE(DidFetchAllEndpoints());

  ExpectDisconnectMetricsAndConsoleError(DisconnectStatusForMetrics::kSuccess,
                                         FedCmRequesterFrameType::kMainFrame,
                                         /*should_record_duration=*/true);
}

TEST_F(FederatedAuthDisconnectRequestTest, NotTrustworthyIdP) {
  Config config = kValidConfig;
  config.config_url = "http://idp.example/fedcm.json";
  RunDisconnectTest(config, DisconnectStatus::kError);
  EXPECT_FALSE(DidFetchAnyEndpoint());

  ExpectDisconnectMetricsAndConsoleError(
      DisconnectStatusForMetrics::kIdpNotPotentiallyTrustworthy,
      FedCmRequesterFrameType::kMainFrame,
      /*should_record_duration=*/false);
}

TEST_F(FederatedAuthDisconnectRequestTest,
       NoSharingPermissionButIdpHasThirdPartyCookiesAccessAndClaimsSignin) {
  const char kAccountId[] = "account";

  Config config = kValidConfig;
  config.accounts = {{kAccountId, /*login_state=*/LoginState::kSignIn,
                      /*was_granted_sharing_permission=*/false}};

  // Pretend the IdP was given third-party cookies access.
  EXPECT_CALL(*api_permission_delegate_,
              HasThirdPartyCookiesAccess(_, GURL(kProviderUrl),
                                         url::Origin::Create(GURL(kRpUrl))))
      .WillOnce(Return(true));
  EXPECT_CALL(*api_permission_delegate_,
              GetApiPermissionStatus(OriginFromString(kRpUrl)))
      .WillOnce(Return(PermissionStatus::GRANTED));

  EXPECT_CALL(*permission_delegate_,
              RevokeSharingPermission(OriginFromString(kRpUrl),
                                      OriginFromString(kRpUrl),
                                      OriginFromString(kProviderUrl), _));
  RunDisconnectTest(config, DisconnectStatus::kSuccess);
  EXPECT_TRUE(DidFetchAllEndpoints());

  ExpectDisconnectMetricsAndConsoleError(DisconnectStatusForMetrics::kSuccess,
                                         FedCmRequesterFrameType::kMainFrame,
                                         /*should_record_duration=*/true);
}

TEST_F(FederatedAuthDisconnectRequestTest, SameSiteIframe) {
  const char kSameSiteIframeUrl[] = "https://rp.example/iframe.html";
  RenderFrameHost* same_site_iframe =
      NavigationSimulator::NavigateAndCommitFromDocument(
          GURL(kSameSiteIframeUrl),
          RenderFrameHostTester::For(web_contents()->GetPrimaryMainFrame())
              ->AppendChild("same_site_iframe"));
  Config config = kValidConfig;
  EXPECT_CALL(*api_permission_delegate_,
              GetApiPermissionStatus(OriginFromString(kRpUrl)))
      .WillOnce(Return(PermissionStatus::GRANTED));
  EXPECT_CALL(*permission_delegate_,
              HasSharingPermission(OriginFromString(kSameSiteIframeUrl),
                                   OriginFromString(kRpUrl),
                                   OriginFromString(kProviderUrl)))
      .WillOnce(Return(true));

  EXPECT_CALL(*permission_delegate_,
              RevokeSharingPermission(OriginFromString(kSameSiteIframeUrl),
                                      OriginFromString(kRpUrl),
                                      OriginFromString(kProviderUrl), _));
  RunDisconnectTest(config, DisconnectStatus::kSuccess, same_site_iframe);
  EXPECT_TRUE(DidFetchAllEndpoints());

  ExpectDisconnectMetricsAndConsoleError(
      DisconnectStatusForMetrics::kSuccess,
      FedCmRequesterFrameType::kSameSiteIframe,
      /*should_record_duration=*/true);
}

TEST_F(FederatedAuthDisconnectRequestTest, CrossSiteIframe) {
  // FedCM works due to third party cookies since sharing permission is not set
  // for the cross-site RP.
  const char kCrossSiteIframeUrl[] = "https://otherrp.com";
  RenderFrameHost* cross_site_iframe =
      NavigationSimulator::NavigateAndCommitFromDocument(
          GURL(kCrossSiteIframeUrl),
          RenderFrameHostTester::For(web_contents()->GetPrimaryMainFrame())
              ->AppendChild("cross_site_iframe"));
  EXPECT_CALL(*api_permission_delegate_,
              GetApiPermissionStatus(OriginFromString(kRpUrl)))
      .WillOnce(Return(PermissionStatus::GRANTED));
  EXPECT_CALL(*permission_delegate_,
              HasSharingPermission(OriginFromString(kCrossSiteIframeUrl),
                                   OriginFromString(kRpUrl),
                                   OriginFromString(kProviderUrl)))
      .WillOnce(Return(true));
  Config config = kValidConfig;

  EXPECT_CALL(*permission_delegate_,
              RevokeSharingPermission(OriginFromString(kCrossSiteIframeUrl),
                                      OriginFromString(kRpUrl),
                                      OriginFromString(kProviderUrl), _));
  RunDisconnectTest(config, DisconnectStatus::kSuccess, cross_site_iframe);
  EXPECT_TRUE(DidFetchAllEndpoints());

  ExpectDisconnectMetricsAndConsoleError(
      DisconnectStatusForMetrics::kSuccess,
      FedCmRequesterFrameType::kCrossSiteIframe,
      /*should_record_duration=*/true);
}

TEST_F(FederatedAuthDisconnectRequestTest, NoAccountToDisconnect) {
  Config config = kValidConfig;
  EXPECT_CALL(*api_permission_delegate_,
              GetApiPermissionStatus(OriginFromString(kRpUrl)))
      .WillOnce(Return(PermissionStatus::GRANTED));
  EXPECT_CALL(
      *permission_delegate_,
      HasSharingPermission(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrl)))
      .WillOnce(Return(false));

  RunDisconnectTest(config, DisconnectStatus::kError);
  EXPECT_FALSE(DidFetchAnyEndpoint());

  ExpectDisconnectMetricsAndConsoleError(
      DisconnectStatusForMetrics::kNoAccountToDisconnect,
      FedCmRequesterFrameType::kMainFrame,
      /*should_record_duration=*/false);
}

TEST_F(FederatedAuthDisconnectRequestTest, DisabledInSettings) {
  Config config = kValidConfig;
  EXPECT_CALL(*api_permission_delegate_,
              GetApiPermissionStatus(OriginFromString(kRpUrl)))
      .WillOnce(Return(PermissionStatus::BLOCKED_SETTINGS));

  RunDisconnectTest(config, DisconnectStatus::kError);
  EXPECT_FALSE(DidFetchAnyEndpoint());

  ExpectDisconnectMetricsAndConsoleError(
      DisconnectStatusForMetrics::kDisabledInSettings,
      FedCmRequesterFrameType::kMainFrame,
      /*should_record_duration=*/false);
}

TEST_F(FederatedAuthDisconnectRequestTest, DisabledInFlags) {
  Config config = kValidConfig;
  EXPECT_CALL(*api_permission_delegate_,
              GetApiPermissionStatus(OriginFromString(kRpUrl)))
      .WillOnce(Return(PermissionStatus::BLOCKED_VARIATIONS));

  RunDisconnectTest(config, DisconnectStatus::kError);
  EXPECT_FALSE(DidFetchAnyEndpoint());

  ExpectDisconnectMetricsAndConsoleError(
      DisconnectStatusForMetrics::kDisabledInFlags,
      FedCmRequesterFrameType::kMainFrame,
      /*should_record_duration=*/false);
}

// Tests that disconnect() succeeds even if FedCM is under embargo (e.g.
// cooldown).
TEST_F(FederatedAuthDisconnectRequestTest, SuccessDespiteEmbargo) {
  Config config = kValidConfig;
  EXPECT_CALL(*api_permission_delegate_,
              GetApiPermissionStatus(OriginFromString(kRpUrl)))
      .WillOnce(Return(PermissionStatus::BLOCKED_EMBARGO));
  EXPECT_CALL(
      *permission_delegate_,
      HasSharingPermission(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrl)))
      .WillOnce(Return(true));
  EXPECT_CALL(*permission_delegate_,
              RevokeSharingPermission(OriginFromString(kRpUrl),
                                      OriginFromString(kRpUrl),
                                      OriginFromString(kProviderUrl), _));

  RunDisconnectTest(config, DisconnectStatus::kSuccess);
  EXPECT_TRUE(DidFetchAllEndpoints());

  ExpectDisconnectMetricsAndConsoleError(DisconnectStatusForMetrics::kSuccess,
                                         FedCmRequesterFrameType::kMainFrame,
                                         /*should_record_duration=*/true);
}

}  // namespace content
