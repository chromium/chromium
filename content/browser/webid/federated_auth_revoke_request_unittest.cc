// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/federated_auth_revoke_request.h"

#include <memory>
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
#include "content/public/test/navigation_simulator.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "net/http/http_status_code.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

using ApiPermissionStatus =
    content::FederatedIdentityApiPermissionContextDelegate::PermissionStatus;
using FetchStatus = content::IdpNetworkRequestManager::FetchStatus;
using ParseStatus = content::IdpNetworkRequestManager::ParseStatus;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

using FedCmEntry = ukm::builders::Blink_FedCm;
using RevokeResponse = content::IdpNetworkRequestManager::RevokeResponse;
using RevokeStatusForMetrics = content::FedCmRevokeStatus;
using blink::mojom::RevokeStatus;

namespace content {
namespace {

constexpr char kRpUrl[] = "https://rp.example";
constexpr char kProviderUrl[] = "https://idp.example/fedcm.json";
constexpr char kAccountsEndpoint[] = "https://idp.example/accounts";
constexpr char kRevokeEndpoint[] = "https://idp.example/revoke";
constexpr char kTokenEndpoint[] = "https://idp.example/token";
constexpr char kLoginUrl[] = "https://idp.example/login";
constexpr char kClientId[] = "client_id_123";

// Not used?
// constexpr char kIdpRevokeUrl[] = "https://idp.example/revoke";

struct AccountConfig {
  std::string id;
  absl::optional<content::IdentityRequestAccount::LoginState> login_state;
  bool was_granted_sharing_permission;
};

struct Config {
  std::vector<AccountConfig> accounts;
  FetchStatus config_fetch_status;
  FetchStatus accounts_fetch_status;
  std::string config_url;
};

Config kValidConfig = {
    /*accounts=*/
    {{"account1", /*login_state=*/absl::nullopt,
      /*was_granted_sharing_permission=*/true}},
    /*config_fetch_status=*/{ParseStatus::kSuccess, net::HTTP_OK},
    /*accounts_fetch_status=*/{ParseStatus::kSuccess, net::HTTP_OK},
    kProviderUrl};

// Helper class for receiving the Revoke method callback.
class RevokeRequestCallbackHelper {
 public:
  RevokeRequestCallbackHelper() = default;
  ~RevokeRequestCallbackHelper() = default;

  RevokeRequestCallbackHelper(const RevokeRequestCallbackHelper&) = delete;
  RevokeRequestCallbackHelper& operator=(const RevokeRequestCallbackHelper&) =
      delete;

  RevokeStatus status() const { return status_; }

  // This can only be called once per lifetime of this object.
  base::OnceCallback<void(RevokeStatus)> callback() {
    return base::BindOnce(&RevokeRequestCallbackHelper::ReceiverMethod,
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
  void ReceiverMethod(RevokeStatus status) {
    status_ = status;
    was_called_ = true;
    wait_for_callback_loop_.Quit();
  }

  bool was_called_ = false;
  base::RunLoop wait_for_callback_loop_;
  RevokeStatus status_;
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
                   int idp_brand_icon_ideal_size,
                   int idp_brand_icon_minimum_size,
                   FetchConfigCallback callback) override {
    has_fetched_config_ = true;

    IdpNetworkRequestManager::Endpoints endpoints;
    endpoints.accounts = GURL(kAccountsEndpoint);
    endpoints.token = GURL(kTokenEndpoint);
    endpoints.revoke = GURL(kRevokeEndpoint);

    IdentityProviderMetadata idp_metadata;
    idp_metadata.config_url = GURL(config_.config_url);
    idp_metadata.idp_login_url = GURL(kLoginUrl);
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), config_.config_fetch_status,
                       endpoints, idp_metadata));
  }

  bool DidFetchAnyEndpoint() {
    return has_fetched_well_known_ || has_fetched_config_;
  }

 protected:
  bool has_fetched_well_known_{false};
  bool has_fetched_config_{false};

 private:
  const Config config_;
};

class TestApiPermissionDelegate : public MockApiPermissionDelegate {
 public:
  ApiPermissionStatus GetApiPermissionStatus(
      const url::Origin& origin) override {
    return ApiPermissionStatus::GRANTED;
  }
};

class TestPermissionDelegate : public MockPermissionDelegate {
 public:
  bool HasSharingPermission(
      const url::Origin& relying_party_requester,
      const url::Origin& relying_party_embedder,
      const url::Origin& identity_provider,
      const absl::optional<std::string>& account_id) override {
    return true;
  }

  absl::optional<bool> GetIdpSigninStatus(
      const url::Origin& idp_origin) override {
    return true;
  }
};

}  // namespace

class FederatedAuthRevokeRequestTest : public RenderViewHostImplTestHarness {
 public:
  FederatedAuthRevokeRequestTest() {
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }
  ~FederatedAuthRevokeRequestTest() override = default;

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    scoped_feature_list_.InitAndEnableFeature(features::kFedCmRevoke);

    api_permission_delegate_ = std::make_unique<TestApiPermissionDelegate>();
    permission_delegate_ = std::make_unique<TestPermissionDelegate>();

    static_cast<TestWebContents*>(web_contents())
        ->NavigateAndCommit(GURL(kRpUrl), ui::PAGE_TRANSITION_LINK);
  }

  void RunRevokeTest(const Config& config,
                     RevokeStatus expected_revoke_status) {
    auto network_manager =
        std::make_unique<TestIdpNetworkRequestManager>(config);
    network_manager_ = network_manager.get();

    metrics_ = std::make_unique<FedCmMetrics>(
        GURL(config.config_url), main_test_rfh()->GetPageUkmSourceId(),
        /*session_id=*/1, /*is_disabled=*/false);

    blink::mojom::IdentityCredentialRevokeOptionsPtr options =
        blink::mojom::IdentityCredentialRevokeOptions::New();
    options->config = blink::mojom::IdentityProviderConfig::New();
    options->config->config_url = GURL(config.config_url);
    options->config->client_id = kClientId;
    options->account_hint = "accountHint";

    RevokeRequestCallbackHelper callback_helper;
    request_ = FederatedAuthRevokeRequest::Create(
        std::move(network_manager), permission_delegate_.get(), main_rfh(),
        metrics_.get(), std::move(options),
        /*should_complete_request_immediately=*/true);
    request_->SetCallbackAndStart(callback_helper.callback(),
                                  api_permission_delegate_.get());
    callback_helper.WaitForCallback();

    EXPECT_EQ(expected_revoke_status, callback_helper.status());
  }

  bool DidFetchAnyEndpoint() { return network_manager_->DidFetchAnyEndpoint(); }

  void ExpectRevokeStatusUKM(RevokeStatusForMetrics status,
                             const char* entry_name) {
    auto entries = ukm_recorder()->GetEntriesByName(entry_name);

    ASSERT_FALSE(entries.empty())
        << "No " << entry_name << " entry was recorded";

    // There are multiple types of metrics under the same FedCM UKM. We need to
    // make sure that the metric only includes the expected one.
    bool metric_found = false;
    for (const auto* const entry : entries) {
      const int64_t* metric =
          ukm_recorder()->GetEntryMetric(entry, "Status.Revoke2");
      if (!metric) {
        continue;
      }
      EXPECT_FALSE(metric_found)
          << "Found more than one entry with Status.Revoke2 in " << entry_name;
      metric_found = true;
      EXPECT_EQ(static_cast<int>(status), *metric)
          << "Unexpected status recorded in " << entry_name;
    }
    EXPECT_TRUE(metric_found)
        << "No Status.Revoke2 entry was found in " << entry_name;
  }

  ukm::TestAutoSetUkmRecorder* ukm_recorder() { return ukm_recorder_.get(); }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<TestIdpNetworkRequestManager, DanglingUntriaged> network_manager_;
  std::unique_ptr<TestApiPermissionDelegate> api_permission_delegate_;
  std::unique_ptr<TestPermissionDelegate> permission_delegate_;
  std::unique_ptr<FedCmMetrics> metrics_;
  std::unique_ptr<FederatedAuthRevokeRequest> request_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
};

TEST_F(FederatedAuthRevokeRequestTest, Success) {
  Config config = kValidConfig;
  RunRevokeTest(config, RevokeStatus::kSuccess);

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.Status.Revoke2",
                                       RevokeStatusForMetrics::kSuccess, 1);
  ExpectRevokeStatusUKM(RevokeStatusForMetrics::kSuccess,
                        FedCmEntry::kEntryName);
}

TEST_F(FederatedAuthRevokeRequestTest, NotTrustworthyIdP) {
  Config config = kValidConfig;
  config.config_url = "http://idp.example/fedcm.json";
  RunRevokeTest(config, RevokeStatus::kError);

  histogram_tester_.ExpectUniqueSample(
      "Blink.FedCm.Status.Revoke2",
      RevokeStatusForMetrics::kIdpNotPotentiallyTrustworthy, 1);
  ExpectRevokeStatusUKM(RevokeStatusForMetrics::kIdpNotPotentiallyTrustworthy,
                        FedCmEntry::kEntryName);
}

}  // namespace content
