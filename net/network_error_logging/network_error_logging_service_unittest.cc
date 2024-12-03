// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/base/features.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/schemeful_site.h"
#include "net/network_error_logging/mock_persistent_nel_store.h"
#include "net/network_error_logging/network_error_logging_service.h"
#include "net/reporting/reporting_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {
namespace {

// The tests are parametrized on a boolean value which represents whether or not
// to use a MockPersistentNelStore.
// If a MockPersistentNelStore is used, then calls to
// NetworkErrorLoggingService::OnHeader(), OnRequest(),
// QueueSignedExchangeReport(), RemoveBrowsingData(), and
// RemoveAllBrowsingData() will block until the store finishes loading.
// Therefore, for tests that should run synchronously (i.e. tests that don't
// specifically test the asynchronous/deferred task behavior), FinishLoading()
// must be called after the first call to one of the above methods.
class NetworkErrorLoggingServiceTest : public ::testing::TestWithParam<bool> {
 protected:
  using NelPolicyKey = NetworkErrorLoggingService::NelPolicyKey;

  NetworkErrorLoggingServiceTest() {
    feature_list_.InitAndEnableFeature(
        features::kPartitionConnectionsByNetworkIsolationKey);

    if (GetParam()) {
      store_ = std::make_unique<MockPersistentNelStore>();
    } else {
      store_.reset(nullptr);
    }
    service_ = NetworkErrorLoggingService::Create(store_.get());
    CreateReportingService();
  }

  void CreateReportingService() {
    DCHECK(!reporting_service_);

    reporting_service_ = std::make_unique<TestReportingService>();
    service_->SetReportingService(reporting_service_.get());
  }

  NetworkErrorLoggingService::RequestDetails MakeRequestDetails(
      const NetworkAnonymizationKey& network_anonymization_key,
      const GURL& url,
      Error error_type,
      std::string method = "GET",
      int status_code = 0,
      IPAddress server_ip = IPAddress()) {
    NetworkErrorLoggingService::RequestDetails details;

    details.network_anonymization_key = network_anonymization_key;
    details.uri = url;
    details.referrer = kReferrer_;
    details.user_agent = kUserAgent_;
    details.server_ip = server_ip.IsValid() ? server_ip : kServerIP_;
    details.method = std::move(method);
    details.status_code = status_code;
    details.elapsed_time = base::Seconds(1);
    details.type = error_type;
    details.reporting_upload_depth = 0;

    return details;
  }

  NetworkErrorLoggingService::SignedExchangeReportDetails
  MakeSignedExchangeReportDetails(
      const NetworkAnonymizationKey& network_anonymization_key,
      bool success,
      const std::string& type,
      const GURL& outer_url,
      const GURL& inner_url,
      const GURL& cert_url,
      const IPAddress& server_ip_address) {
    NetworkErrorLoggingService::SignedExchangeReportDetails details;
    details.network_anonymization_key = network_anonymization_key;
    details.success = success;
    details.type = type;
    details.outer_url = outer_url;
    details.inner_url = inner_url;
    details.cert_url = cert_url;
    details.referrer = kReferrer_.spec();
    details.server_ip_address = server_ip_address;
    details.protocol = "http/1.1";
    details.method = "GET";
    details.status_code = 200;
    details.elapsed_time = base::Milliseconds(1234);
    details.user_agent = kUserAgent_;
    return details;
  }
  NetworkErrorLoggingService* service() { return service_.get(); }
  MockPersistentNelStore* store() { return store_.get(); }
  const std::vector<TestReportingService::Report>& reports() {
    return reporting_service_->reports();
  }

  // These methods are design so that using them together will create unique
  // Origin, NetworkAnonymizationKey pairs, but they do return repeated values
  // when called separately, so they can be used to ensure that reports are
  // keyed on both NAK and Origin.
  url::Origin MakeOrigin(size_t index) {
    GURL url(base::StringPrintf("https://example%zd.com/", index / 2));
    return url::Origin::Create(url);
  }
  NetworkAnonymizationKey MakeNetworkAnonymizationKey(size_t index) {
    SchemefulSite site(
        GURL(base::StringPrintf("https://example%zd.com/", (index + 1) / 2)));
    return NetworkAnonymizationKey::CreateSameSite(site);
  }

  NetworkErrorLoggingService::NelPolicy MakePolicy(
      const NetworkAnonymizationKey& network_anonymization_key,
      const url::Origin& origin,
      base::Time expires = base::Time(),
      base::Time last_used = base::Time()) {
    NetworkErrorLoggingService::NelPolicy policy;
    policy.key = NelPolicyKey(network_anonymization_key, origin);
    policy.expires = expires;
    policy.last_used = last_used;

    return policy;
  }

  // Returns whether the NetworkErrorLoggingService has a policy corresponding
  // to |network_anonymization_key| and |origin|. Returns true if so, even if
  // the policy is expired.
  bool HasPolicy(const NetworkAnonymizationKey& network_anonymization_key,
                 const url::Origin& origin) {
    std::set<NelPolicyKey> all_policy_keys =
        service_->GetPolicyKeysForTesting();
    return all_policy_keys.find(NelPolicyKey(network_anonymization_key,
                                             origin)) != all_policy_keys.end();
  }

  size_t PolicyCount() { return service_->GetPolicyKeysForTesting().size(); }

  // Makes the rest of the test run synchronously.
  void FinishLoading(bool load_success) {
    if (store())
      store()->FinishLoading(load_success);
  }

  base::test::ScopedFeatureList feature_list_;

  const GURL kUrl_ = GURL("https://example.com/path");
  const GURL kUrlDifferentPort_ = GURL("https://example.com:4433/path");
  const GURL kUrlSubdomain_ = GURL("https://subdomain.example.com/path");
  const GURL kUrlDifferentHost_ = GURL("https://somewhere-else.com/path");
  const GURL kUrlEtld_ = GURL("https://co.uk/foo.html");

  const GURL kInnerUrl_ = GURL("https://example.net/path");
  const GURL kCertUrl_ = GURL("https://example.com/cert_path");

  const IPAddress kServerIP_ = IPAddress(192, 168, 0, 1);
  const IPAddress kOtherServerIP_ = IPAddress(192, 168, 0, 2);
  const url::Origin kOrigin_ = url::Origin::Create(kUrl_);
  const url::Origin kOriginDifferentPort_ =
      url::Origin::Create(kUrlDifferentPort_);
  const url::Origin kOriginSubdomain_ = url::Origin::Create(kUrlSubdomain_);
  const url::Origin kOriginDifferentHost_ =
      url::Origin::Create(kUrlDifferentHost_);
  const url::Origin kOriginEtld_ = url::Origin::Create(kUrlEtld_);
  const NetworkAnonymizationKey kNak_ =
      NetworkAnonymizationKey::CreateSameSite(SchemefulSite(kOrigin_));
  const NetworkAnonymizationKey kOtherNak_ =
      NetworkAnonymizationKey::CreateSameSite(
          SchemefulSite(kOriginDifferentHost_));

  const std::string kHeader_ = "{\"report_to\":\"group\",\"max_age\":86400}";
  const std::string kHeaderSuccessFraction0_ =
      "{\"report_to\":\"group\",\"max_age\":86400,\"success_fraction\":0.0}";
  const std::string kHeaderSuccessFraction1_ =
      "{\"report_to\":\"group\",\"max_age\":86400,\"success_fraction\":1.0}";
  const std::string kHeaderIncludeSubdomains_ =
      "{\"report_to\":\"group\",\"max_age\":86400,\"include_subdomains\":true}";
  const std::string kHeaderIncludeSubdomainsAndSuccess_ =
      "{\"report_to\":\"group\",\"max_age\":86400,\"include_subdomains\":true,"
      "\"success_fraction\":1.0}";
  const std::string kHeaderMaxAge0_ = "{\"max_age\":0}";
  const std::string kHeaderTooLong_ =
      "{\"report_to\":\"group\",\"max_age\":86400,\"junk\":\"" +
      std::string(32 * 1024, 'a') + "\"}";
  const std::string kHeaderTooDeep_ =
      "{\"report_to\":\"group\",\"max_age\":86400,\"junk\":[[[[[[[[[[]]]]]]]]]]"
      "}";

  const std::string kUserAgent_ = "Mozilla/1.0";
  const std::string kGroup_ = "group";

  const std::string kType_ = NetworkErrorLoggingService::kReportType;

  const GURL kReferrer_ = GURL("https://referrer.com/");

  // `store_` and `reporting_service_` need to outlive `service_`.
  std::unique_ptr<MockPersistentNelStore> store_;
  std::unique_ptr<TestReportingService> reporting_service_;
  std::unique_ptr<NetworkErrorLoggingService> service_;
};

void ExpectDictDoubleValue(double expected_value,
                           const base::Value::Dict& value,
                           const std::string& key) {
  std::optional<double> double_value = value.FindDouble(key);
  ASSERT_TRUE(double_value) << key;
  EXPECT_DOUBLE_EQ(expected_value, *double_value) << key;
}

TEST_P(NetworkErrorLoggingServiceTest, CreateService) {
  // Service is created by default in the test fixture..
  EXPECT_TRUE(service());
}

TEST_P(NetworkErrorLoggingServiceTest, NoReportingService) {
  service_ = NetworkErrorLoggingService::Create(store_.get());

  service()->OnHeader(kNak_, kOrigin_, kServerIP_, kHeader_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  // Should not crash.
  service()->OnRequest(
      MakeRequestDetails(kNak_, kUrl_, ERR_CONNECTION_REFUSED));
}

TEST_P(NetworkErrorLoggingServiceTest, NoPolicy) {
  service()->OnRequest(
      MakeRequestDetails(kNak_, kUrl_, ERR_CONNECTION_REFUSED));

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  EXPECT_TRUE(reports().empty());
}

TEST_P(NetworkErrorLoggingServiceTest, PolicyKeyMatchesNakAndOrigin) {
  service()->OnHeader(kNak_, kOrigin_, kServerIP_, kHeader_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  // Wrong NAK and origin.
  service()->OnRequest(MakeRequestDetails(kOtherNak_, kUrlDifferentHost_,
                                          ERR_CONNECTION_REFUSED));
  EXPECT_TRUE(reports().empty());

  // Wrong NAK.
  service()->OnRequest(
      MakeRequestDetails(kOtherNak_, kUrl_, ERR_CONNECTION_REFUSED));
  EXPECT_TRUE(reports().empty());

  // Wrong origin.
  service()->OnRequest(
      MakeRequestDetails(kNak_, kUrlDifferentHost_, ERR_CONNECTION_REFUSED));
  EXPECT_TRUE(reports().empty());

  // Correct key.
  service()->OnRequest(
      MakeRequestDetails(kNak_, kUrl_, ERR_CONNECTION_REFUSED));
  EXPECT_EQ(1u, reports().size());
  EXPECT_EQ(kUrl_, reports()[0].url);
  EXPECT_EQ(kNak_, reports()[0].network_anonymization_key);
  EXPECT_EQ(kUserAgent_, reports()[0].user_agent);
  EXPECT_EQ(kGroup_, reports()[0].group);
  EXPECT_EQ(kType_, reports()[0].type);
}

TEST_P(NetworkErrorLoggingServiceTest,
       PolicyKeyMatchesNakAndOriginIncludeSubdomains) {
  service()->OnHeader(kNak_, kOrigin_, kServerIP_, kHeaderIncludeSubdomains_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  // Wrong NAK and origin.
  service()->OnRequest(MakeRequestDetails(kOtherNak_, kUrlDifferentHost_,
                                          ERR_CONNECTION_REFUSED));
  EXPECT_TRUE(reports().empty());

  // Wrong NAK (same origin).
  service()->OnRequest(
      MakeRequestDetails(kOtherNak_, kUrl_, ERR_CONNECTION_REFUSED));
  EXPECT_TRUE(reports().empty());

  // Wrong NAK (subdomain).
  service()->OnRequest(
      MakeRequestDetails(kOtherNak_, kUrlSubdomain_, ERR_CONNECTION_REFUSED));
  EXPECT_TRUE(reports().empty());

  // Wrong origin.
  service()->OnRequest(
      MakeRequestDetails(kNak_, kUrlDifferentHost_, ERR_CONNECTION_REFUSED));
  EXPECT_TRUE(reports().empty());

  // Correct key, successful request (same origin).
  service()->OnRequest(MakeRequestDetails(kNak_, kUrl_, OK));
  EXPECT_TRUE(reports().empty());

  // Correct key, non-DNS error (same origin).
  service()->OnRequest(
      MakeRequestDetails(kNak_, kUrl_, ERR_CONNECTION_REFUSED));
  EXPECT_EQ(1u, reports().size());
  EXPECT_EQ(kUrl_, reports()[0].url);
  EXPECT_EQ(kNak_, reports()[0].network_anonymization_key);
  EXPECT_EQ(kUserAgent_, reports()[0].user_agent);
  EXPECT_EQ(kGroup_, reports()[0].group);
  EXPECT_EQ(kType_, reports()[0].type);

  // Correct key, successful request (subdomain).
  service()->OnRequest(MakeRequestDetails(kNak_, kUrlSubdomain_, OK));
  EXPECT_EQ(1u, reports().size());

  // Correct key, non-DNS error (subdomain).
  service()->OnRequest(
      MakeRequestDetails(kNak_, kUrlSubdomain_, ERR_CONNECTION_REFUSED));
  EXPECT_EQ(1u, reports().size());

  // Correct key, DNS error (subdomain).
  service()->OnRequest(
      MakeRequestDetails(kNak_, kUrlSubdomain_, ERR_NAME_NOT_RESOLVED));
  EXPECT_EQ(2u, reports().size());
  EXPECT_EQ(kUrlSubdomain_, reports()[1].url);
  EXPECT_EQ(kNak_, reports()[1].network_anonymization_key);
  EXPECT_EQ(kUserAgent_, reports()[1].user_agent);
  EXPECT_EQ(kGroup_, reports()[1].group);
  EXPECT_EQ(kType_, reports()[1].type);
}

TEST_P(NetworkErrorLoggingServiceTest,
       PolicyKeyMatchesNakAndOriginIncludeSubdomainsAndSuccess) {
  service()->OnHeader(kNak_, kOrigin_, kServerIP_,
                      kHeaderIncludeSubdomainsAndSuccess_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  // Wrong NAK and origin.
  service()->OnRequest(MakeRequestDetails(kOtherNak_, kUrlDifferentHost_,
                                          ERR_CONNECTION_REFUSED));
  EXPECT_TRUE(reports().empty());

  // Wrong NAK (same origin).
  service()->OnRequest(
      MakeRequestDetails(kOtherNak_, kUrl_, ERR_CONNECTION_REFUSED));
  EXPECT_TRUE(reports().empty());

  // Wrong NAK (subdomain).
  service()->OnRequest(
      MakeRequestDetails(kOtherNak_, kUrlSubdomain_, ERR_CONNECTION_REFUSED));
  EXPECT_TRUE(reports().empty());

  // Wrong origin.
  service()->OnRequest(
      MakeRequestDetails(kNak_, kUrlDifferentHost_, ERR_CONNECTION_REFUSED));
  EXPECT_TRUE(reports().empty());

  // Correct key, successful request (same origin).
  service()->OnRequest(MakeRequestDetails(kNak_, kUrl_, OK));
  EXPECT_EQ(1u, reports().size());
  EXPECT_EQ(kUrl_, reports()[0].url);
  EXPECT_EQ(kNak_, reports()[0].network_anonymization_key);
  EXPECT_EQ(kUserAgent_, reports()[0].user_agent);
  EXPECT_EQ(kGroup_, reports()[0].group);
  EXPECT_EQ(kType_, reports()[0].type);

  // Correct key, non-DNS error (same origin).
  service()->OnRequest(
      MakeRequestDetails(kNak_, kUrl_, ERR_CONNECTION_REFUSED));
  EXPECT_EQ(2u, reports().size());
  EXPECT_EQ(kUrl_, reports()[1].url);
  EXPECT_EQ(kNak_, reports()[1].network_anonymization_key);
  EXPECT_EQ(kUserAgent_, reports()[1].user_agent);
  EXPECT_EQ(kGroup_, reports()[1].group);
  EXPECT_EQ(kType_, reports()[1].type);

  // Correct key (subdomain).
  service()->OnRequest(
      MakeRequestDetails(kNak_, kUrlSubdomain_, ERR_NAME_NOT_RESOLVED));
  EXPECT_EQ(3u, reports().size());
  EXPECT_EQ(kUrlSubdomain_, reports()[2].url);
  EXPECT_EQ(kNak_, reports()[2].network_anonymization_key);
  EXPECT_EQ(kUserAgent_, reports()[2].user_agent);
  EXPECT_EQ(kGroup_, reports()[2].group);
  EXPECT_EQ(kType_, reports()[2].type);

  // Correct key, successful request (subdomain).
  service()->OnRequest(MakeRequestDetails(kNak_, kUrlSubdomain_, OK));
  EXPECT_EQ(3u, reports().size());

  // Correct key, successful request on mismatched IP (subdomain).
  service()->OnRequest(MakeRequestDetails(kNak_, kUrlSubdomain_, OK, "GET", 200,
                                          kOtherServerIP_));
  ASSERT_EQ(3u, reports().size());
}

TEST_P(NetworkErrorLoggingServiceTest, NetworkAnonymizationKeyDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kPartitionConnectionsByNetworkIsolationKey);

  // Need to re-create the service, since it caches the feature value on
  // creation.
  service_ = NetworkErrorLoggingService::Create(store_.get());
  reporting_service_ = std::make_unique<TestReportingService>();
  service_->SetReportingService(reporting_service_.get());

  service()->OnHeader(kNak_, kOrigin_, kServerIP_, kHeader_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  // Wrong NAK, but a report should be generated anyways.
  service()->OnRequest(
      MakeRequestDetails(kOtherNak_, kUrl_, ERR_CONNECTION_REFUSED));
  EXPECT_EQ(1u, reports().size());
  EXPECT_EQ(kUrl_, reports()[0].url);
  EXPECT_EQ(NetworkAnonymizationKey(), reports()[0].network_anonymization_key);
  EXPECT_EQ(kUserAgent_, reports()[0].user_agent);
  EXPECT_EQ(kGroup_, reports()[0].group);
  EXPECT_EQ(kType_, reports()[0].type);
}

TEST_P(NetworkErrorLoggingServiceTest, JsonTooLong) {
  service()->OnHeader(kNak_, kOrigin_, kServerIP_, kHeaderTooLong_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  service()->OnRequest(
      MakeRequestDetails(kNak_, kUrl_, ERR_CONNECTION_REFUSED));

  EXPECT_TRUE(reports().empty());
}

TEST_P(NetworkErrorLoggingServiceTest, JsonTooDeep) {
  service()->OnHeader(kNak_, kOrigin_, kServerIP_, kHeaderTooDeep_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  service()->OnRequest(
      MakeRequestDetails(kNak_, kUrl_, ERR_CONNECTION_REFUSED));

  EXPECT_TRUE(reports().empty());
}

TEST_P(NetworkErrorLoggingServiceTest, IncludeSubdomainsEtldRejected) {
  service()->OnHeader(kNak_, kOriginEtld_, kServerIP_,
                      kHeaderIncludeSubdomains_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  EXPECT_EQ(0u, PolicyCount());

  service()->OnRequest(
      MakeRequestDetails(kNak_, kUrlEtld_, ERR_CONNECTION_REFUSED));

  EXPECT_TRUE(reports().empty());
}

TEST_P(NetworkErrorLoggingServiceTest, NonIncludeSubdomainsEtldAccepted) {
  service()->OnHeader(kNak_, kOriginEtld_, kServerIP_, kHeader_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  EXPECT_EQ(1u, PolicyCount());

  service()->OnRequest(
      MakeRequestDetails(kNak_, kUrlEtld_, ERR_CONNECTION_REFUSED));

  EXPECT_EQ(1u, reports().size());
  EXPECT_EQ(kUrlEtld_, reports()[0].url);
}

TEST_P(NetworkErrorLoggingServiceTest, SuccessReportQueued) {
  service()->OnHeader(kNak_, kOrigin_, kServerIP_, kHeaderSuccessFraction1_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  service()->OnRequest(MakeRequestDetails(kNak_, kUrl_, OK));

  ASSERT_EQ(1u, reports().size());
  EXPECT_EQ(kUrl_, reports()[0].url);
  EXPECT_EQ(kNak_, reports()[0].network_anonymization_key);
  EXPECT_EQ(kUserAgent_, reports()[0].user_agent);
  EXPECT_EQ(kGroup_, reports()[0].group);
  EXPECT_EQ(kType_, reports()[0].type);
  EXPECT_EQ(0, reports()[0].depth);

  const base::Value* body = reports()[0].body.get();
  ASSERT_TRUE(body);
  ASSERT_TRUE(body->is_dict());
  const base::Value::Dict& body_dict = body->GetDict();

  base::ExpectDictStringValue(kReferrer_.spec(), body_dict,
                              NetworkErrorLoggingService::kReferrerKey);
  // TODO(juliatuttle): Extract these constants.
  ExpectDictDoubleValue(1.0, body_dict,
                        NetworkErrorLoggingService::kSamplingFractionKey);
  base::ExpectDictStringValue(kServerIP_.ToString(), body_dict,
                              NetworkErrorLoggingService::kServerIpKey);
  base::ExpectDictStringValue("", body_dict,
                              NetworkErrorLoggingService::kProtocolKey);
  base::ExpectDictStringValue("GET", body_dict,
                              NetworkErrorLoggingService::kMethodKey);
  base::ExpectDictIntegerValue(0, body_dict,
                               NetworkErrorLoggingService::kStatusCodeKey);
  base::ExpectDictIntegerValue(1000, body_dict,
                               NetworkErrorLoggingService::kElapsedTimeKey);
  base::ExpectDictStringValue("application", body_dict,
                              NetworkErrorLoggingService::kPhaseKey);
  base::ExpectDictStringValue("ok", body_dict,
                              NetworkErrorLoggingService::kTypeKey);
}

TEST_P(NetworkErrorLoggingServiceTest, FailureReportQueued) {
  static const std::string kHeaderFailureFraction1 =
      "{\"report_to\":\"group\",\"max_age\":86400,\"failure_fraction\":1.0}";
  service()->OnHeader(kNak_, kOrigin_, kServerIP_, kHeaderFailureFraction1);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  service()->OnRequest(
      MakeRequestDetails(kNak_, kUrl_, ERR_CONNECTION_REFUSED));

  ASSERT_EQ(1u, reports().size());
  EXPECT_EQ(kUrl_, reports()[0].url);
  EXPECT_EQ(kNak_, reports()[0].network_anonymization_key);
  EXPECT_EQ(kUserAgent_, reports()[0].user_agent);
  EXPECT_EQ(kGroup_, reports()[0].group);
  EXPECT_EQ(kType_, reports()[0].type);
  EXPECT_EQ(0, reports()[0].depth);

  const base::Value* body = reports()[0].body.get();
  ASSERT_TRUE(body);
  ASSERT_TRUE(body->is_dict());
  const base::Value::Dict& body_dict = body->GetDict();

  base::ExpectDictStringValue(kReferrer_.spec(), body_dict,
                              NetworkErrorLoggingService::kReferrerKey);
  // TODO(juliatuttle): Extract these constants.
  ExpectDictDoubleValue(1.0, body_dict,
                        NetworkErrorLoggingService::kSamplingFractionKey);
  base::ExpectDictStringValue(kServerIP_.ToString(), body_dict,
                              NetworkErrorLoggingService::kServerIpKey);
  base::ExpectDictStringValue("", body_dict,
                              NetworkErrorLoggingService::kProtocolKey);
  base::ExpectDictStringValue("GET", body_dict,
                              NetworkErrorLoggingService::kMethodKey);
  base::ExpectDictIntegerValue(0, body_dict,
                               NetworkErrorLoggingService::kStatusCodeKey);
  base::ExpectDictIntegerValue(1000, body_dict,
                               NetworkErrorLoggingService::kElapsedTimeKey);
  base::ExpectDictStringValue("connection", body_dict,
                              NetworkErrorLoggingService::kPhaseKey);
  base::ExpectDictStringValue("tcp.refused", body_dict,
                              NetworkErrorLoggingService::kTypeKey);
}

TEST_P(NetworkErrorLoggingServiceTest, UnknownFailureReportQueued) {
  static const std::string kHeaderFailureFraction1 =
      "{\"report_to\":\"group\",\"max_age\":86400,\"failure_fraction\":1.0}";
  service()->OnHeader(kNak_, kOrigin_, kServerIP_, kHeaderFailureFraction1);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  // This error code happens to not be mapped to a NEL report `type` field
  // value.
  service()->OnRequest(MakeRequestDetails(kNak_, kUrl_, ERR_FILE_NO_SPACE));

  ASSERT_EQ(1u, reports().size());
  const base::Value* body = reports()[0].body.get();
  ASSERT_TRUE(body);
  ASSERT_TRUE(body->is_dict());
  const base::Value::Dict& body_dict = body->GetDict();
  base::ExpectDictStringValue("application", body_dict,
                              NetworkErrorLoggingService::kPhaseKey);
  base::ExpectDictStringValue("unknown", body_dict,
                              NetworkErrorLoggingService::kTypeKey);
}

TEST_P(NetworkErrorLoggingServiceTest, UnknownCertFailureReportQueued) {
  static const std::string kHeaderFailureFraction1 =
      "{\"report_to\":\"group\",\"max_age\":86400,\"failure_fraction\":1.0}";
  service()->OnHeader(kNak_, kOrigin_, kServerIP_, kHeaderFailureFraction1);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  // This error code happens to not be mapped to a NEL report `type` field
  // value.  Because it's a certificate error, we'll set the `phase` to be
  // `connection`.
  service()->OnRequest(
      MakeRequestDetails(kNak_, kUrl_, ERR_CERT_NON_UNIQUE_NAME));

  ASSERT_EQ(1u, reports().size());
  const base::Value* body = reports()[0].body.get();
  ASSERT_TRUE(body);
  ASSERT_TRUE(body->is_dict());
  const base::Value::Dict& body_dict = body->GetDict();
  base::ExpectDictStringValue("connection", body_dict,
                              NetworkErrorLoggingService::kPhaseKey);
  base::ExpectDictStringValue("unknown", body_dict,
                              NetworkErrorLoggingService::kTypeKey);
}

TEST_P(NetworkErrorLoggingServiceTest, HttpErrorReportQueued) {
  static const std::string kHeaderFailureFraction1 =
      "{\"report_to\":\"group\",\"max_age\":86400,\"failure_fraction\":1.0}";
  service()->OnHeader(kNak_, kOrigin_, kServerIP_, kHeaderFailureFraction1);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  service()->OnRequest(MakeRequestDetails(kNak_, kUrl_, OK, "GET", 504));

  ASSERT_EQ(1u, reports().size());
  EXPECT_EQ(kUrl_, reports()[0].url);
  EXPECT_EQ(kNak_, reports()[0].network_anonymization_key);
  EXPECT_EQ(kUserAgent_, reports()[0].user_agent);
  EXPECT_EQ(kGroup_, reports()[0].group);
  EXPECT_EQ(kType_, reports()[0].type);
  EXPECT_EQ(0, reports()[0].depth);

  const base::Value* body = reports()[0].body.get();
  ASSERT_TRUE(body);
  ASSERT_TRUE(body->is_dict());
  const base::Value::Dict& body_dict = body->GetDict();

  base::ExpectDictStringValue(kReferrer_.spec(), body_dict,
                              NetworkErrorLoggingService::kReferrerKey);
  // TODO(juliatuttle): Extract these constants.
  ExpectDictDoubleValue(1.0, body_dict,
                        NetworkErrorLoggingService::kSamplingFractionKey);
  base::ExpectDictStringValue(kServerIP_.ToString(), body_dict,
                              NetworkErrorLoggingService::kServerIpKey);
  base::ExpectDictStringValue("", body_dict,
                              NetworkErrorLoggingService::kProtocolKey);
  base::ExpectDictStringValue("GET", body_dict,
                              NetworkErrorLoggingService::kMethodKey);
  base::ExpectDictIntegerValue(504, body_dict,
                               NetworkErrorLoggingService::kStatusCodeKey);
  base::ExpectDictIntegerValue(1000, body_dict,
                               NetworkErrorLoggingService::kElapsedTimeKey);
  base::ExpectDictStringValue("application", body_dict,
                              NetworkErrorLoggingService::kPhaseKey);
  base::ExpectDictStringValue("http.error", body_dict,
                              NetworkErrorLoggingService::kTypeKey);
}

TEST_P(NetworkErrorLoggingServiceTest, SuccessReportDowngraded) {
  service()->OnHeader(kNak_, kOrigin_, kServerIP_, kHeaderSuccessFraction1_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  service()->OnRequest(
      MakeRequestDetails(kNak_, kUrl_, OK, "GET", 200, kOtherServerIP_));

  ASSERT_EQ(1u, reports().size());
  EXPECT_EQ(kUrl_, reports()[0].url);
  EXPECT_EQ(kNak_, reports()[0].network_anonymization_key);
  EXPECT_EQ(kGroup_, reports()[0].group);
  EXPECT_EQ(kType_, reports()[0].type);
  EXPECT_EQ(0, reports()[0].depth);

  const base::Value* body = reports()[0].body.get();
  ASSERT_TRUE(body);
  ASSERT_TRUE(body->is_dict());
  const base::Value::Dict& body_dict = body->GetDict();

  base::ExpectDictStringValue(kReferrer_.spec(), body_dict,
                              NetworkErrorLoggingService::kReferrerKey);
  ExpectDictDoubleValue(1.0, body_dict,
                        NetworkErrorLoggingService::kSamplingFractionKey);
  base::ExpectDictStringValue(kOtherServerIP_.ToString(), body_dict,
                              NetworkErrorLoggingService::kServerIpKey);
  base::ExpectDictStringValue("", body_dict,
                              NetworkErrorLoggingService::kProtocolKey);
  base::ExpectDictStringValue("GET", body_dict,
                              NetworkErrorLoggingService::kMethodKey);
  base::ExpectDictIntegerValue(0, body_dict,
                               NetworkErrorLoggingService::kStatusCodeKey);
  base::ExpectDictIntegerValue(0, body_dict,
                               NetworkErrorLoggingService::kElapsedTimeKey);
  base::ExpectDictStringValue("dns", body_dict,
                              NetworkErrorLoggingService::kPhaseKey);
  base::ExpectDictStringValue("dns.address_changed", body_dict,
                              NetworkErrorLoggingService::kTypeKey);
}

TEST_P(NetworkErrorLoggingServiceTest, FailureReportDowngraded) {
  service()->OnHeader(kNak_, kOrigin_, kServerIP_, kHeaderSuccessFraction1_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  service()->OnRequest(MakeRequestDetails(kNak_, kUrl_, ERR_CONNECTION_REFUSED,
                                          "GET", 200, kOtherServerIP_));

  ASSERT_EQ(1u, reports().size());
  EXPECT_EQ(kUrl_, reports()[0].url);
  EXPECT_EQ(kNak_, reports()[0].network_anonymization_key);
  EXPECT_EQ(kGroup_, reports()[0].group);
  EXPECT_EQ(kType_, reports()[0].type);
  EXPECT_EQ(0, reports()[0].depth);

  const base::Value* body = reports()[0].body.get();
  ASSERT_TRUE(body);
  ASSERT_TRUE(body->is_dict());
  const base::Value::Dict& body_dict = body->GetDict();

  base::ExpectDictStringValue(kReferrer_.spec(), body_dict,
                              NetworkErrorLoggingService::kReferrerKey);
  ExpectDictDoubleValue(1.0, body_dict,
                        NetworkErrorLoggingService::kSamplingFractionKey);
  base::ExpectDictStringValue(kOtherServerIP_.ToString(), body_dict,
                              NetworkErrorLoggingService::kServerIpKey);
  base::ExpectDictStringValue("", body_dict,
                              NetworkErrorLoggingService::kProtocolKey);
  base::ExpectDictStringValue("GET", body_dict,
                              NetworkErrorLoggingService::kMethodKey);
  base::ExpectDictIntegerValue(0, body_dict,
                               NetworkErrorLoggingService::kStatusCodeKey);
  base::ExpectDictIntegerValue(0, body_dict,
                               NetworkErrorLoggingService::kElapsedTimeKey);
  base::ExpectDictStringValue("dns", body_dict,
                              NetworkErrorLoggingService::kPhaseKey);
  base::ExpectDictStringValue("dns.address_changed", body_dict,
                              NetworkErrorLoggingService::kTypeKey);
}

TEST_P(NetworkErrorLoggingServiceTest, HttpErrorReportDowngraded) {
  service()->OnHeader(kNak_, kOrigin_, kServerIP_, kHeaderSuccessFraction1_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  service()->OnRequest(
      MakeRequestDetails(kNak_, kUrl_, OK, "GET", 504, kOtherServerIP_));

  ASSERT_EQ(1u, reports().size());
  EXPECT_EQ(kUrl_, reports()[0].url);
  EXPECT_EQ(kNak_, reports()[0].network_anonymization_key);
  EXPECT_EQ(kGroup_, reports()[0].group);
  EXPECT_EQ(kType_, reports()[0].type);
  EXPECT_EQ(0, reports()[0].depth);

  const base::Value* body = reports()[0].body.get();
  ASSERT_TRUE(body);
  ASSERT_TRUE(body->is_dict());
  const base::Value::Dict& body_dict = body->GetDict();

  base::ExpectDictStringValue(kReferrer_.spec(), body_dict,
                              NetworkErrorLoggingService::kReferrerKey);
  ExpectDictDoubleValue(1.0, body_dict,
                        NetworkErrorLoggingService::kSamplingFractionKey);
  base::ExpectDictStringValue(kOtherServerIP_.ToString(), body_dict,
                              NetworkErrorLoggingService::kServerIpKey);
  base::ExpectDictStringValue("", body_dict,
                              NetworkErrorLoggingService::kProtocolKey);
  base::ExpectDictStringValue("GET", body_dict,
                              NetworkErrorLoggingService::kMethodKey);
  base::ExpectDictIntegerValue(0, body_dict,
                               NetworkErrorLoggingService::kStatusCodeKey);
  base::ExpectDictIntegerValue(0, body_dict,
                               NetworkErrorLoggingService::kElapsedTimeKey);
  base::ExpectDictStringValue("dns", body_dict,
                              NetworkErrorLoggingService::kPhaseKey);
  base::ExpectDictStringValue("dns.address_changed", body_dict,
                              NetworkErrorLoggingService::kTypeKey);
}

TEST_P(NetworkErrorLoggingServiceTest, DNSFailureReportNotDowngraded) {
  service()->OnHeader(kNak_, kOrigin_, kServerIP_, kHeaderSuccessFraction1_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  service()->OnRequest(MakeRequestDetails(kNak_, kUrl_, ERR_NAME_NOT_RESOLVED,
                                          "GET", 0, kOtherServerIP_));

  ASSERT_EQ(1u, reports().size());
  EXPECT_EQ(kUrl_, reports()[0].url);
  EXPECT_EQ(kNak_, reports()[0].network_anonymization_key);
  EXPECT_EQ(kGroup_, reports()[0].group);
  EXPECT_EQ(kType_, reports()[0].type);
  EXPECT_EQ(0, reports()[0].depth);

  const base::Value* body = reports()[0].body.get();
  ASSERT_TRUE(body);
  ASSERT_TRUE(body->is_dict());
  const base::Value::Dict& body_dict = body->GetDict();

  base::ExpectDictStringValue(kReferrer_.spec(), body_dict,
                              NetworkErrorLoggingService::kReferrerKey);
  ExpectDictDoubleValue(1.0, body_dict,
                        NetworkErrorLoggingService::kSamplingFractionKey);
  base::ExpectDictStringValue(kOtherServerIP_.ToString(), body_dict,
                              NetworkErrorLoggingService::kServerIpKey);
  base::ExpectDictStringValue("", body_dict,
                              NetworkErrorLoggingService::kProtocolKey);
  base::ExpectDictStringValue("GET", body_dict,
                              NetworkErrorLoggingService::kMethodKey);
  base::ExpectDictIntegerValue(0, body_dict,
                               NetworkErrorLoggingService::kStatusCodeKey);
  base::ExpectDictIntegerValue(1000, body_dict,
                               NetworkErrorLoggingService::kElapsedTimeKey);
  base::ExpectDictStringValue("dns", body_dict,
                              NetworkErrorLoggingService::kPhaseKey);
  base::ExpectDictStringValue("dns.name_not_resolved", body_dict,
                              NetworkErrorLoggingService::kTypeKey);
}

TEST_P(NetworkErrorLoggingServiceTest, SuccessPOSTReportQueued) {
  service()->OnHeader(kNak_, kOrigin_, kServerIP_, kHeaderSuccessFraction1_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  service()->OnRequest(MakeRequestDetails(kNak_, kUrl_, OK, "POST"));

  ASSERT_EQ(1u, reports().size());
  EXPECT_EQ(kUrl_, reports()[0].url);
  EXPECT_EQ(kNak_, reports()[0].network_anonymization_key);
  EXPECT_EQ(kGroup_, reports()[0].group);
  EXPECT_EQ(kType_, reports()[0].type);
  EXPECT_EQ(0, reports()[0].depth);

  const base::Value* body = reports()[0].body.get();
  ASSERT_TRUE(body);
  ASSERT_TRUE(body->is_dict());
  const base::Value::Dict& body_dict = body->GetDict();

  base::ExpectDictStringValue(kReferrer_.spec(), body_dict,
                              NetworkErrorLoggingService::kReferrerKey);
  ExpectDictDoubleValue(1.0, body_dict,
                        NetworkErrorLoggingService::kSamplingFractionKey);
  base::ExpectDictStringValue(kServerIP_.ToString(), body_dict,
                              NetworkErrorLoggingService::kServerIpKey);
  base::ExpectDictStringValue("", body_dict,
                              NetworkErrorLoggingService::kProtocolKey);
  base::ExpectDictStringValue("POST", body_dict,
                              NetworkErrorLoggingService::kMethodKey);
  base::ExpectDictStringValue("application", body_dict,
                              NetworkErrorLoggingService::kPhaseKey);
  base::ExpectDictStringValue("ok", body_dict,
                              NetworkErrorLoggingService::kTypeKey);
}

TEST_P(NetworkErrorLoggingServiceTest, MaxAge0) {
  service()->OnHeader(kNak_, kOrigin_, kServerIP_, kHeader_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  EXPECT_EQ(1u, PolicyCount());

  // Max_age of 0 removes the policy.
  service()->OnHeader(kNak_, kOrigin_, kServerIP_, kHeaderMaxAge0_);
  EXPECT_EQ(0u, PolicyCount());

  service()->OnRequest(
      MakeRequestDetails(kNak_, kUrl_, ERR_CONNECTION_REFUSED));

  EXPECT_TRUE(reports().empty());
}

TEST_P(NetworkErrorLoggingServiceTest, SuccessFraction0) {
  service()->OnHeader(kNak_, kOrigin_, kServerIP_, kHeaderSuccessFraction0_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  // Each network error has a 0% chance of being reported.  Fire off several and
  // verify that no reports are produced.
  constexpr size_t kReportCount = 100;
  for (size_t i = 0; i < kReportCount; ++i)
    service()->OnRequest(MakeRequestDetails(kNak_, kUrl_, OK));

  EXPECT_TRUE(reports().empty());
}

TEST_P(NetworkErrorLoggingServiceTest, SuccessFractionHalf) {
  // Include a different value for failure_fraction to ensure that we copy the
  // right value into sampling_fraction.
  static const std::string kHeaderSuccessFractionHalf =
      "{\"report_to\":\"group\",\"max_age\":86400,\"success_fraction\":0.5,"
      "\"failure_fraction\":0.25}";
  service()->OnHeader(kNak_, kOrigin_, kServerIP_, kHeaderSuccessFractionHalf);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  // Each network error has a 50% chance of being reported.  Fire off several
  // and verify that some requests were reported and some weren't.  (We can't
  // verify exact counts because each decision is made randomly.)
  constexpr size_t kReportCount = 100;
  for (size_t i = 0; i < kReportCount; ++i)
    service()->OnRequest(MakeRequestDetails(kNak_, kUrl_, OK));

  // If our random selection logic is correct, there is a 2^-100 chance that
  // every single report above was skipped.  If this check fails, it's much more
  // likely that our code is wrong.
  EXPECT_FALSE(reports().empty());

  // There's also a 2^-100 chance that every single report was logged.  Same as
  // above, that's much more likely to be a code error.
  EXPECT_GT(kReportCount, reports().size());

  for (const auto& report : reports()) {
    const base::Value::Dict* body_dict = report.body->GetIfDict();
    ASSERT_TRUE(body_dict);
    // Our header includes a different value for failure_fraction, so that this
    // check verifies that we copy the correct fraction into sampling_fraction.
    ExpectDictDoubleValue(0.5, *body_dict,
                          NetworkErrorLoggingService::kSamplingFractionKey);
  }
}

TEST_P(NetworkErrorLoggingServiceTest, FailureFraction0) {
  static const std::string kHeaderFailureFraction0 =
      "{\"report_to\":\"group\",\"max_age\":86400,\"failure_fraction\":0.0}";
  service()->OnHeader(kNak_, kOrigin_, kServerIP_, kHeaderFailureFraction0);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  // Each network error has a 0% chance of being reported.  Fire off several and
  // verify that no reports are produced.
  constexpr size_t kReportCount = 100;
  for (size_t i = 0; i < kReportCount; ++i)
    service()->OnRequest(
        MakeRequestDetails(kNak_, kUrl_, ERR_CONNECTION_REFUSED));

  EXPECT_TRUE(reports().empty());
}

TEST_P(NetworkErrorLoggingServiceTest, FailureFractionHalf) {
  // Include a different value for success_fraction to ensure that we copy the
  // right value into sampling_fraction.
  static const std::string kHeaderFailureFractionHalf =
      "{\"report_to\":\"group\",\"max_age\":86400,\"failure_fraction\":0.5,"
      "\"success_fraction\":0.25}";
  service()->OnHeader(kNak_, kOrigin_, kServerIP_, kHeaderFailureFractionHalf);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  // Each network error has a 50% chance of being reported.  Fire off several
  // and verify that some requests were reported and some weren't.  (We can't
  // verify exact counts because each decision is made randomly.)
  constexpr size_t kReportCount = 100;
  for (size_t i = 0; i < kReportCount; ++i)
    service()->OnRequest(
        MakeRequestDetails(kNak_, kUrl_, ERR_CONNECTION_REFUSED));

  // If our random selection logic is correct, there is a 2^-100 chance that
  // every single report above was skipped.  If this check fails, it's much more
  // likely that our code is wrong.
  EXPECT_FALSE(reports().empty());

  // There's also a 2^-100 chance that every single report was logged.  Same as
  // above, that's much more likely to be a code error.
  EXPECT_GT(kReportCount, reports().size());

  for (const auto& report : reports()) {
    const base::Value::Dict* body_dict = report.body->GetIfDict();
    ASSERT_TRUE(body_dict);
    ExpectDictDoubleValue(0.5, *body_dict,
                          NetworkErrorLoggingService::kSamplingFractionKey);
  }
}

TEST_P(NetworkErrorLoggingServiceTest,
       ExcludeSubdomainsDoesntMatchDifferentPort) {
  service()->OnHeader(kNak_, kOrigin_, kServerIP_, kHeader_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  service()->OnRequest(
      MakeRequestDetails(kNak_, kUrlDifferentPort_, ERR_CONNECTION_REFUSED));

  EXPECT_TRUE(reports().empty());
}

TEST_P(NetworkErrorLoggingServiceTest, ExcludeSubdomainsDoesntMatchSubdomain) {
  service()->OnHeader(kNak_, kOrigin_, kServerIP_, kHeader_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  service()->OnRequest(
      MakeRequestDetails(kNak_, kUrlSubdomain_, ERR_CONNECTION_REFUSED));

  EXPECT_TRUE(reports().empty());
}

TEST_P(NetworkErrorLoggingServiceTest, IncludeSubdomainsMatchesDifferentPort) {
  service()->OnHeader(kNak_, kOrigin_, kServerIP_, kHeaderIncludeSubdomains_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  service()->OnRequest(
      MakeRequestDetails(kNak_, kUrlDifferentPort_, ERR_NAME_NOT_RESOLVED));

  ASSERT_EQ(1u, reports().size());
  EXPECT_EQ(kUrlDifferentPort_, reports()[0].url);
}

TEST_P(NetworkErrorLoggingServiceTest, IncludeSubdomainsMatchesSubdomain) {
  service()->OnHeader(kNak_, kOrigin_, kServerIP_, kHeaderIncludeSubdomains_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  service()->OnRequest(
      MakeRequestDetails(kNak_, kUrlSubdomain_, ERR_NAME_NOT_RESOLVED));

  ASSERT_EQ(1u, reports().size());
}

TEST_P(NetworkErrorLoggingServiceTest,
       IncludeSubdomainsDoesntMatchSuperdomain) {
  service()->OnHeader(kNak_, kOriginSubdomain_, kServerIP_,
                      kHeaderIncludeSubdomains_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  service()->OnRequest(MakeRequestDetails(kNak_, kUrl_, ERR_NAME_NOT_RESOLVED));

  EXPECT_TRUE(reports().empty());
}

TEST_P(NetworkErrorLoggingServiceTest,
       IncludeSubdomainsDoesntReportConnectionError) {
  service()->OnHeader(kNak_, kOrigin_, kServerIP_, kHeaderIncludeSubdomains_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  service()->OnRequest(
      MakeRequestDetails(kNak_, kUrlSubdomain_, ERR_CONNECTION_REFUSED));

  EXPECT_TRUE(reports().empty());
}

TEST_P(NetworkErrorLoggingServiceTest,
       IncludeSubdomainsDoesntReportApplicationError) {
  service()->OnHeader(kNak_, kOrigin_, kServerIP_, kHeaderIncludeSubdomains_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  service()->OnRequest(
      MakeRequestDetails(kNak_, kUrlSubdomain_, ERR_INVALID_HTTP_RESPONSE));

  EXPECT_TRUE(reports().empty());
}

TEST_P(NetworkErrorLoggingServiceTest, IncludeSubdomainsDoesntReportSuccess) {
  service()->OnHeader(kNak_, kOrigin_, kServerIP_, kHeaderIncludeSubdomains_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  service()->OnRequest(MakeRequestDetails(kNak_, kUrlSubdomain_, OK));

  EXPECT_TRUE(reports().empty());
}

TEST_P(NetworkErrorLoggingServiceTest,
       IncludeSubdomainsReportsSameOriginSuccess) {
  static const std::string kHeaderIncludeSubdomainsSuccess1 =
      "{\"report_to\":\"group\",\"max_age\":86400,"
      "\"include_subdomains\":true,\"success_fraction\":1.0}";
  service()->OnHeader(kNak_, kOrigin_, kServerIP_,
                      kHeaderIncludeSubdomainsSuccess1);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  service()->OnRequest(MakeRequestDetails(kNak_, kUrl_, OK));

  ASSERT_EQ(1u, reports().size());
  EXPECT_EQ(kUrl_, reports()[0].url);
}

TEST_P(NetworkErrorLoggingServiceTest, RemoveAllBrowsingData) {
  service()->OnHeader(kNak_, kOrigin_, kServerIP_, kHeader_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  EXPECT_EQ(1u, PolicyCount());
  EXPECT_TRUE(HasPolicy(kNak_, kOrigin_));

  service()->RemoveAllBrowsingData();

  service()->OnRequest(
      MakeRequestDetails(kNak_, kUrl_, ERR_CONNECTION_REFUSED));

  EXPECT_EQ(0u, PolicyCount());
  EXPECT_FALSE(HasPolicy(kNak_, kOrigin_));
  EXPECT_TRUE(reports().empty());
}

TEST_P(NetworkErrorLoggingServiceTest, RemoveSomeBrowsingData) {
  service()->OnHeader(kNak_, kOrigin_, kServerIP_, kHeader_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  service()->OnHeader(kNak_, kOriginDifferentHost_, kServerIP_, kHeader_);
  EXPECT_EQ(2u, PolicyCount());

  // Remove policy for kOrigin_ but not kOriginDifferentHost_
  service()->RemoveBrowsingData(
      base::BindRepeating([](const url::Origin& origin) -> bool {
        return origin.host() == "example.com";
      }));
  EXPECT_EQ(1u, PolicyCount());
  EXPECT_TRUE(HasPolicy(kNak_, kOriginDifferentHost_));
  EXPECT_FALSE(HasPolicy(kNak_, kOrigin_));

  service()->OnRequest(
      MakeRequestDetails(kNak_, kUrl_, ERR_CONNECTION_REFUSED));

  EXPECT_TRUE(reports().empty());

  service()->OnRequest(
      MakeRequestDetails(kNak_, kUrlDifferentHost_, ERR_CONNECTION_REFUSED));

  ASSERT_EQ(1u, reports().size());
}

TEST_P(NetworkErrorLoggingServiceTest, Nested) {
  service()->OnHeader(kNak_, kOrigin_, kServerIP_, kHeader_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  NetworkErrorLoggingService::RequestDetails details =
      MakeRequestDetails(kNak_, kUrl_, ERR_CONNECTION_REFUSED);
  details.reporting_upload_depth =
      NetworkErrorLoggingService::kMaxNestedReportDepth;
  service()->OnRequest(details);

  ASSERT_EQ(1u, reports().size());
  EXPECT_EQ(NetworkErrorLoggingService::kMaxNestedReportDepth,
            reports()[0].depth);
}

TEST_P(NetworkErrorLoggingServiceTest, NestedTooDeep) {
  service()->OnHeader(kNak_, kOrigin_, kServerIP_, kHeader_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  NetworkErrorLoggingService::RequestDetails details =
      MakeRequestDetails(kNak_, kUrl_, ERR_CONNECTION_REFUSED);
  details.reporting_upload_depth =
      NetworkErrorLoggingService::kMaxNestedReportDepth + 1;
  service()->OnRequest(details);

  EXPECT_TRUE(reports().empty());
}

TEST_P(NetworkErrorLoggingServiceTest, StatusAsValue) {
  // The expiration times will be bogus, but we need a reproducible value for
  // this test.
  base::SimpleTestClock clock;
  service()->SetClockForTesting(&clock);
  // The clock is initialized to the "zero" or origin point of the Time class.
  // This sets the clock's Time to the equivalent of the "zero" or origin point
  // of the TimeTicks class, so that the serialized value produced by
  // NetLog::TimeToString is consistent across restarts.
  base::TimeDelta delta_from_origin =
      base::Time::UnixEpoch().since_origin() -
      base::TimeTicks::UnixEpoch().since_origin();
  clock.Advance(delta_from_origin);

  service()->OnHeader(kNak_, kOrigin_, kServerIP_, kHeaderSuccessFraction1_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  service()->OnHeader(kNak_, kOriginDifferentHost_, kServerIP_, kHeader_);
  service()->OnHeader(kOtherNak_, kOriginSubdomain_, kServerIP_,
                      kHeaderIncludeSubdomains_);
  const std::string kHeaderWrongTypes =
      ("{\"report_to\":\"group\","
       "\"max_age\":86400,"
       // We'll ignore each of these fields because they're the wrong type.
       // We'll use a default value instead.
       "\"include_subdomains\":\"true\","
       "\"success_fraction\": \"1.0\","
       "\"failure_fraction\": \"0.0\"}");
  service()->OnHeader(
      kNak_, url::Origin::Create(GURL("https://invalid-types.example.com")),
      kServerIP_, kHeaderWrongTypes);

  base::Value actual = service()->StatusAsValue();
  base::Value expected = base::test::ParseJson(R"json(
      {
        "originPolicies": [
          {
            "NetworkAnonymizationKey": "https://example.com same_site",
            "origin": "https://example.com",
            "includeSubdomains": false,
            "expires": "86400000",
            "reportTo": "group",
            "successFraction": 1.0,
            "failureFraction": 1.0,
          },
          {
            "NetworkAnonymizationKey": "https://example.com same_site",
            "origin": "https://invalid-types.example.com",
            "includeSubdomains": false,
            "expires": "86400000",
            "reportTo": "group",
            "successFraction": 0.0,
            "failureFraction": 1.0,
          },
          {
            "NetworkAnonymizationKey": "https://example.com same_site",
            "origin": "https://somewhere-else.com",
            "includeSubdomains": false,
            "expires": "86400000",
            "reportTo": "group",
            "successFraction": 0.0,
            "failureFraction": 1.0,
          },
          {
            "NetworkAnonymizationKey": "https://somewhere-else.com same_site",
            "origin": "https://subdomain.example.com",
            "includeSubdomains": true,
            "expires": "86400000",
            "reportTo": "group",
            "successFraction": 0.0,
            "failureFraction": 1.0,
          },
        ]
      }
      )json");
  EXPECT_EQ(expected, actual);
}

TEST_P(NetworkErrorLoggingServiceTest, InvalidHeaderData) {
  service()->OnHeader(kNak_, kOrigin_, kServerIP_, "0");
}

TEST_P(NetworkErrorLoggingServiceTest, NoReportingService_SignedExchange) {
  service_ = NetworkErrorLoggingService::Create(store_.get());

  service()->OnHeader(kNak_, kOrigin_, kServerIP_, kHeader_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  // Should not crash
  service()->QueueSignedExchangeReport(MakeSignedExchangeReportDetails(
      kNak_, false, "sxg.failed", kUrl_, kInnerUrl_, kCertUrl_, kServerIP_));
}

TEST_P(NetworkErrorLoggingServiceTest, NoPolicyForOrigin_SignedExchange) {
  service()->QueueSignedExchangeReport(MakeSignedExchangeReportDetails(
      kNak_, false, "sxg.failed", kUrl_, kInnerUrl_, kCertUrl_, kServerIP_));

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  EXPECT_TRUE(reports().empty());
}

TEST_P(NetworkErrorLoggingServiceTest, SuccessFraction0_SignedExchange) {
  service()->OnHeader(kNak_, kOrigin_, kServerIP_, kHeaderSuccessFraction0_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  // Each network error has a 0% chance of being reported.  Fire off several and
  // verify that no reports are produced.
  constexpr size_t kReportCount = 100;
  for (size_t i = 0; i < kReportCount; ++i) {
    service()->QueueSignedExchangeReport(MakeSignedExchangeReportDetails(
        kNak_, true, "ok", kUrl_, kInnerUrl_, kCertUrl_, kServerIP_));
  }

  EXPECT_TRUE(reports().empty());
}

TEST_P(NetworkErrorLoggingServiceTest, SuccessReportQueued_SignedExchange) {
  service()->OnHeader(kNak_, kOrigin_, kServerIP_, kHeaderSuccessFraction1_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  service()->QueueSignedExchangeReport(MakeSignedExchangeReportDetails(
      kNak_, true, "ok", kUrl_, kInnerUrl_, kCertUrl_, kServerIP_));
  ASSERT_EQ(1u, reports().size());
  EXPECT_EQ(kUrl_, reports()[0].url);
  EXPECT_EQ(kNak_, reports()[0].network_anonymization_key);
  EXPECT_EQ(kUserAgent_, reports()[0].user_agent);
  EXPECT_EQ(kGroup_, reports()[0].group);
  EXPECT_EQ(kType_, reports()[0].type);
  EXPECT_EQ(0, reports()[0].depth);

  const base::Value* body = reports()[0].body.get();
  ASSERT_TRUE(body);
  ASSERT_TRUE(body->is_dict());
  const base::Value::Dict& body_dict = body->GetDict();

  base::ExpectDictStringValue(kReferrer_.spec(), body_dict,
                              NetworkErrorLoggingService::kReferrerKey);
  ExpectDictDoubleValue(1.0, body_dict,
                        NetworkErrorLoggingService::kSamplingFractionKey);
  base::ExpectDictStringValue(kServerIP_.ToString(), body_dict,
                              NetworkErrorLoggingService::kServerIpKey);
  base::ExpectDictStringValue("http/1.1", body_dict,
                              NetworkErrorLoggingService::kProtocolKey);
  base::ExpectDictStringValue("GET", body_dict,
                              NetworkErrorLoggingService::kMethodKey);
  base::ExpectDictIntegerValue(200, body_dict,
                               NetworkErrorLoggingService::kStatusCodeKey);
  base::ExpectDictIntegerValue(1234, body_dict,
                               NetworkErrorLoggingService::kElapsedTimeKey);
  base::ExpectDictStringValue(
      NetworkErrorLoggingService::kSignedExchangePhaseValue, body_dict,
      NetworkErrorLoggingService::kPhaseKey);
  base::ExpectDictStringValue("ok", body_dict,
                              NetworkErrorLoggingService::kTypeKey);

  const base::Value::Dict* sxg_body =
      body_dict.FindDict(NetworkErrorLoggingService::kSignedExchangeBodyKey);
  ASSERT_TRUE(sxg_body);

  base::ExpectDictStringValue(kUrl_.spec(), *sxg_body,
                              NetworkErrorLoggingService::kOuterUrlKey);
  base::ExpectDictStringValue(kInnerUrl_.spec(), *sxg_body,
                              NetworkErrorLoggingService::kInnerUrlKey);
  base::ExpectStringValue(
      kCertUrl_.spec(),
      sxg_body->Find(NetworkErrorLoggingService::kCertUrlKey)->GetList()[0]);
}

TEST_P(NetworkErrorLoggingServiceTest, FailureReportQueued_SignedExchange) {
  service()->OnHeader(kNak_, kOrigin_, kServerIP_, kHeader_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  service()->QueueSignedExchangeReport(MakeSignedExchangeReportDetails(
      kNak_, false, "sxg.failed", kUrl_, kInnerUrl_, kCertUrl_, kServerIP_));
  ASSERT_EQ(1u, reports().size());
  EXPECT_EQ(kUrl_, reports()[0].url);
  EXPECT_EQ(kNak_, reports()[0].network_anonymization_key);
  EXPECT_EQ(kUserAgent_, reports()[0].user_agent);
  EXPECT_EQ(kGroup_, reports()[0].group);
  EXPECT_EQ(kType_, reports()[0].type);
  EXPECT_EQ(0, reports()[0].depth);

  const base::Value* body = reports()[0].body.get();
  ASSERT_TRUE(body);
  ASSERT_TRUE(body->is_dict());
  const base::Value::Dict& body_dict = body->GetDict();

  base::ExpectDictStringValue(kReferrer_.spec(), body_dict,
                              NetworkErrorLoggingService::kReferrerKey);
  ExpectDictDoubleValue(1.0, body_dict,
                        NetworkErrorLoggingService::kSamplingFractionKey);
  base::ExpectDictStringValue(kServerIP_.ToString(), body_dict,
                              NetworkErrorLoggingService::kServerIpKey);
  base::ExpectDictStringValue("http/1.1", body_dict,
                              NetworkErrorLoggingService::kProtocolKey);
  base::ExpectDictStringValue("GET", body_dict,
                              NetworkErrorLoggingService::kMethodKey);
  base::ExpectDictIntegerValue(200, body_dict,
                               NetworkErrorLoggingService::kStatusCodeKey);
  base::ExpectDictIntegerValue(1234, body_dict,
                               NetworkErrorLoggingService::kElapsedTimeKey);
  base::ExpectDictStringValue(
      NetworkErrorLoggingService::kSignedExchangePhaseValue, body_dict,
      NetworkErrorLoggingService::kPhaseKey);
  base::ExpectDictStringValue("sxg.failed", body_dict,
                              NetworkErrorLoggingService::kTypeKey);

  const base::Value::Dict* sxg_body =
      body_dict.FindDict(NetworkErrorLoggingService::kSignedExchangeBodyKey);
  ASSERT_TRUE(sxg_body);

  base::ExpectDictStringValue(kUrl_.spec(), *sxg_body,
                              NetworkErrorLoggingService::kOuterUrlKey);
  base::ExpectDictStringValue(kInnerUrl_.spec(), *sxg_body,
                              NetworkErrorLoggingService::kInnerUrlKey);
  base::ExpectStringValue(
      kCertUrl_.spec(),
      sxg_body->Find(NetworkErrorLoggingService::kCertUrlKey)->GetList()[0]);
}

TEST_P(NetworkErrorLoggingServiceTest, MismatchingSubdomain_SignedExchange) {
  service()->OnHeader(kNak_, kOrigin_, kServerIP_, kHeaderIncludeSubdomains_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  service()->QueueSignedExchangeReport(MakeSignedExchangeReportDetails(
      kNak_, false, "sxg.failed", kUrlSubdomain_, kInnerUrl_, kCertUrl_,
      kServerIP_));
  EXPECT_TRUE(reports().empty());
}

TEST_P(NetworkErrorLoggingServiceTest, MismatchingIPAddress_SignedExchange) {
  service()->OnHeader(kNak_, kOrigin_, kServerIP_, kHeader_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  service()->QueueSignedExchangeReport(
      MakeSignedExchangeReportDetails(kNak_, false, "sxg.failed", kUrl_,
                                      kInnerUrl_, kCertUrl_, kOtherServerIP_));
  EXPECT_TRUE(reports().empty());
}

TEST_P(NetworkErrorLoggingServiceTest,
       SignedExchangeNetworkAnonymizationKeyDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kPartitionConnectionsByNetworkIsolationKey);

  // Need to re-create the service, since it caches the feature value on
  // creation.
  service_ = NetworkErrorLoggingService::Create(store_.get());
  reporting_service_ = std::make_unique<TestReportingService>();
  service_->SetReportingService(reporting_service_.get());

  service()->OnHeader(kNak_, kOrigin_, kServerIP_, kHeaderSuccessFraction1_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  // Wrong NAK, but a report should be generated anyways.
  service()->QueueSignedExchangeReport(MakeSignedExchangeReportDetails(
      kOtherNak_, true, "ok", kUrl_, kInnerUrl_, kCertUrl_, kServerIP_));

  ASSERT_EQ(1u, reports().size());
  EXPECT_EQ(kUrl_, reports()[0].url);
  EXPECT_EQ(NetworkAnonymizationKey(), reports()[0].network_anonymization_key);
  EXPECT_EQ(kUserAgent_, reports()[0].user_agent);
  EXPECT_EQ(kGroup_, reports()[0].group);
  EXPECT_EQ(kType_, reports()[0].type);
  EXPECT_EQ(0, reports()[0].depth);
}

// When the max number of policies is exceeded, first try to remove expired
// policies before evicting the least recently used unexpired policy.
TEST_P(NetworkErrorLoggingServiceTest, EvictAllExpiredPoliciesFirst) {
  base::SimpleTestClock clock;
  service()->SetClockForTesting(&clock);

  // Add 100 policies then make them expired.
  for (size_t i = 0; i < 100; ++i) {
    service()->OnHeader(MakeNetworkAnonymizationKey(i), MakeOrigin(i),
                        kServerIP_, kHeader_);
  }
  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  EXPECT_EQ(100u, PolicyCount());
  clock.Advance(base::Seconds(86401));  // max_age is 86400 sec
  // Expired policies are allowed to linger before hitting the policy limit.
  EXPECT_EQ(100u, PolicyCount());

  // Reach the max policy limit.
  for (size_t i = 100; i < NetworkErrorLoggingService::kMaxPolicies; ++i) {
    service()->OnHeader(MakeNetworkAnonymizationKey(i), MakeOrigin(i),
                        kServerIP_, kHeader_);
  }
  EXPECT_EQ(NetworkErrorLoggingService::kMaxPolicies, PolicyCount());

  // Add one more policy to trigger eviction of only the expired policies.
  service()->OnHeader(kNak_, kOrigin_, kServerIP_, kHeader_);
  EXPECT_EQ(NetworkErrorLoggingService::kMaxPolicies - 100 + 1, PolicyCount());
}

TEST_P(NetworkErrorLoggingServiceTest, EvictLeastRecentlyUsedPolicy) {
  base::SimpleTestClock clock;
  service()->SetClockForTesting(&clock);

  // A policy's |last_used| is updated when it is added
  for (size_t i = 0; i < NetworkErrorLoggingService::kMaxPolicies; ++i) {
    service()->OnHeader(MakeNetworkAnonymizationKey(i), MakeOrigin(i),
                        kServerIP_, kHeader_);
    clock.Advance(base::Seconds(1));
  }
  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  EXPECT_EQ(PolicyCount(), NetworkErrorLoggingService::kMaxPolicies);

  // Set another policy which triggers eviction. None of the policies have
  // expired, so the least recently used (i.e. least recently added) policy
  // should be evicted.
  service()->OnHeader(kNak_, kOrigin_, kServerIP_, kHeader_);
  clock.Advance(base::Seconds(1));
  EXPECT_EQ(PolicyCount(), NetworkErrorLoggingService::kMaxPolicies);

  EXPECT_FALSE(
      HasPolicy(MakeNetworkAnonymizationKey(0), MakeOrigin(0)));  // evicted
  std::set<NelPolicyKey> all_policy_keys = service()->GetPolicyKeysForTesting();
  for (size_t i = 1; i < NetworkErrorLoggingService::kMaxPolicies; ++i) {
    // Avoid n calls to HasPolicy(), which would be O(n^2).
    NelPolicyKey key(MakeNetworkAnonymizationKey(i), MakeOrigin(i));
    EXPECT_EQ(1u, all_policy_keys.count(key));
  }
  EXPECT_TRUE(HasPolicy(kNak_, kOrigin_));

  // Now use the policies in reverse order starting with kOrigin_, then add
  // another policy to trigger eviction, to check that the stalest policy is
  // identified correctly.
  service()->OnRequest(
      MakeRequestDetails(kNak_, kOrigin_.GetURL(), ERR_CONNECTION_REFUSED));
  clock.Advance(base::Seconds(1));
  for (size_t i = NetworkErrorLoggingService::kMaxPolicies - 1; i >= 1; --i) {
    service()->OnRequest(MakeRequestDetails(MakeNetworkAnonymizationKey(i),
                                            MakeOrigin(i).GetURL(),
                                            ERR_CONNECTION_REFUSED));
    clock.Advance(base::Seconds(1));
  }
  service()->OnHeader(kNak_, kOriginSubdomain_, kServerIP_, kHeader_);
  EXPECT_EQ(PolicyCount(), NetworkErrorLoggingService::kMaxPolicies);

  EXPECT_FALSE(HasPolicy(kNak_, kOrigin_));  // evicted
  all_policy_keys = service()->GetPolicyKeysForTesting();
  for (size_t i = NetworkErrorLoggingService::kMaxPolicies - 1; i >= 1; --i) {
    // Avoid n calls to HasPolicy(), which would be O(n^2).
    NelPolicyKey key(MakeNetworkAnonymizationKey(i), MakeOrigin(i));
    EXPECT_EQ(1u, all_policy_keys.count(key));
  }
  EXPECT_TRUE(HasPolicy(kNak_, kOriginSubdomain_));  // most recently added

  // Note: This test advances the clock by ~2000 seconds, which is below the
  // specified max_age of 86400 seconds, so none of the policies expire during
  // this test.
}

TEST_P(NetworkErrorLoggingServiceTest, SendsCommandsToStoreSynchronous) {
  if (!store())
    return;

  MockPersistentNelStore::CommandList expected_commands;
  NetworkErrorLoggingService::NelPolicy policy1 = MakePolicy(kNak_, kOrigin_);
  NetworkErrorLoggingService::NelPolicy policy2 =
      MakePolicy(kNak_, kOriginDifferentHost_);
  std::vector<NetworkErrorLoggingService::NelPolicy> prestored_policies = {
      policy1, policy2};
  store()->SetPrestoredPolicies(std::move(prestored_policies));

  // The first call to any of the public methods triggers a load.
  service()->OnHeader(kNak_, kOrigin_, kServerIP_, kHeader_);
  expected_commands.emplace_back(
      MockPersistentNelStore::Command::Type::LOAD_NEL_POLICIES);
  EXPECT_TRUE(store()->VerifyCommands(expected_commands));

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);
  // DoOnHeader() should now execute.
  expected_commands.emplace_back(
      MockPersistentNelStore::Command::Type::DELETE_NEL_POLICY, policy1);
  expected_commands.emplace_back(
      MockPersistentNelStore::Command::Type::ADD_NEL_POLICY, policy1);
  EXPECT_TRUE(store()->VerifyCommands(expected_commands));

  service()->OnRequest(
      MakeRequestDetails(kNak_, kOrigin_.GetURL(), ERR_CONNECTION_REFUSED));
  expected_commands.emplace_back(
      MockPersistentNelStore::Command::Type::UPDATE_NEL_POLICY, policy1);
  EXPECT_TRUE(store()->VerifyCommands(expected_commands));

  service()->QueueSignedExchangeReport(MakeSignedExchangeReportDetails(
      kNak_, false, "sxg.failed", kUrl_, kInnerUrl_, kCertUrl_, kServerIP_));
  expected_commands.emplace_back(
      MockPersistentNelStore::Command::Type::UPDATE_NEL_POLICY, policy1);
  EXPECT_TRUE(store()->VerifyCommands(expected_commands));

  // Removes policy1 but not policy2.
  EXPECT_EQ(2, store()->StoredPoliciesCount());
  service()->RemoveBrowsingData(
      base::BindRepeating([](const url::Origin& origin) -> bool {
        return origin.host() == "example.com";
      }));
  expected_commands.emplace_back(
      MockPersistentNelStore::Command::Type::DELETE_NEL_POLICY, policy1);
  expected_commands.emplace_back(MockPersistentNelStore::Command::Type::FLUSH);
  EXPECT_EQ(1, store()->StoredPoliciesCount());
  EXPECT_TRUE(store()->VerifyCommands(expected_commands));

  service()->RemoveAllBrowsingData();
  expected_commands.emplace_back(
      MockPersistentNelStore::Command::Type::DELETE_NEL_POLICY, policy2);
  expected_commands.emplace_back(MockPersistentNelStore::Command::Type::FLUSH);
  EXPECT_EQ(0, store()->StoredPoliciesCount());
  EXPECT_TRUE(store()->VerifyCommands(expected_commands));
}

TEST_P(NetworkErrorLoggingServiceTest, DuplicateEntriesInStore) {
  if (!store())
    return;

  NetworkErrorLoggingService::NelPolicy policy1 = MakePolicy(kNak_, kOrigin_);
  NetworkErrorLoggingService::NelPolicy policy2 = policy1;
  std::vector<NetworkErrorLoggingService::NelPolicy> prestored_policies = {
      policy1, policy2};
  store()->SetPrestoredPolicies(std::move(prestored_policies));

  // The first call to any of the public methods triggers a load.
  service()->OnHeader(kNak_, kOrigin_, kServerIP_, kHeader_);
  EXPECT_TRUE(store()->VerifyCommands({MockPersistentNelStore::Command(
      MockPersistentNelStore::Command::Type::LOAD_NEL_POLICIES)}));
  FinishLoading(/*load_success=*/true);

  EXPECT_EQ(service()->GetPolicyKeysForTesting().size(), 1u);
}

// Same as the above test, except that all the tasks are queued until loading
// is complete.
TEST_P(NetworkErrorLoggingServiceTest, SendsCommandsToStoreDeferred) {
  if (!store())
    return;

  MockPersistentNelStore::CommandList expected_commands;
  NetworkErrorLoggingService::NelPolicy policy1 = MakePolicy(kNak_, kOrigin_);
  NetworkErrorLoggingService::NelPolicy policy2 =
      MakePolicy(kNak_, kOriginDifferentHost_);
  std::vector<NetworkErrorLoggingService::NelPolicy> prestored_policies = {
      policy1, policy2};
  store()->SetPrestoredPolicies(std::move(prestored_policies));

  // The first call to any of the public methods triggers a load.
  service()->OnHeader(kNak_, kOrigin_, kServerIP_, kHeader_);
  expected_commands.emplace_back(
      MockPersistentNelStore::Command::Type::LOAD_NEL_POLICIES);
  EXPECT_TRUE(store()->VerifyCommands(expected_commands));

  service()->OnRequest(
      MakeRequestDetails(kNak_, kOrigin_.GetURL(), ERR_CONNECTION_REFUSED));
  EXPECT_TRUE(store()->VerifyCommands(expected_commands));

  service()->QueueSignedExchangeReport(MakeSignedExchangeReportDetails(
      kNak_, false, "sxg.failed", kUrl_, kInnerUrl_, kCertUrl_, kServerIP_));
  EXPECT_TRUE(store()->VerifyCommands(expected_commands));

  // Removes policy1 but not policy2.
  service()->RemoveBrowsingData(
      base::BindRepeating([](const url::Origin& origin) -> bool {
        return origin.host() == "example.com";
      }));
  EXPECT_TRUE(store()->VerifyCommands(expected_commands));

  service()->RemoveAllBrowsingData();
  EXPECT_TRUE(store()->VerifyCommands(expected_commands));

  // The store has not yet been told to remove the policies because the tasks
  // to remove browsing data were queued pending initialization.
  EXPECT_EQ(2, store()->StoredPoliciesCount());

  FinishLoading(true /* load_success */);
  // DoOnHeader()
  expected_commands.emplace_back(
      MockPersistentNelStore::Command::Type::DELETE_NEL_POLICY, policy1);
  expected_commands.emplace_back(
      MockPersistentNelStore::Command::Type::ADD_NEL_POLICY, policy1);
  // DoOnRequest()
  expected_commands.emplace_back(
      MockPersistentNelStore::Command::Type::UPDATE_NEL_POLICY, policy1);
  // DoQueueSignedExchangeReport()
  expected_commands.emplace_back(
      MockPersistentNelStore::Command::Type::UPDATE_NEL_POLICY, policy1);
  // DoRemoveBrowsingData()
  expected_commands.emplace_back(
      MockPersistentNelStore::Command::Type::DELETE_NEL_POLICY, policy1);
  expected_commands.emplace_back(MockPersistentNelStore::Command::Type::FLUSH);
  // DoRemoveAllBrowsingData()
  expected_commands.emplace_back(
      MockPersistentNelStore::Command::Type::DELETE_NEL_POLICY, policy2);
  expected_commands.emplace_back(MockPersistentNelStore::Command::Type::FLUSH);
  EXPECT_TRUE(store()->VerifyCommands(expected_commands));
}

// These two tests check that if loading fails, the commands should still
// be sent to the store; the actual store impl will just ignore them.
TEST_P(NetworkErrorLoggingServiceTest,
       SendsCommandsToStoreSynchronousLoadFailed) {
  if (!store())
    return;

  MockPersistentNelStore::CommandList expected_commands;
  NetworkErrorLoggingService::NelPolicy policy1 = MakePolicy(kNak_, kOrigin_);
  NetworkErrorLoggingService::NelPolicy policy2 =
      MakePolicy(kNak_, kOriginDifferentHost_);
  std::vector<NetworkErrorLoggingService::NelPolicy> prestored_policies = {
      policy1, policy2};
  store()->SetPrestoredPolicies(std::move(prestored_policies));

  // The first call to any of the public methods triggers a load.
  service()->OnHeader(kNak_, kOrigin_, kServerIP_, kHeader_);
  expected_commands.emplace_back(
      MockPersistentNelStore::Command::Type::LOAD_NEL_POLICIES);
  EXPECT_TRUE(store()->VerifyCommands(expected_commands));

  // Make the rest of the test run synchronously.
  FinishLoading(false /* load_success */);
  // DoOnHeader() should now execute.
  // Because the load failed, there will be no policies in memory, so the store
  // is not told to delete anything.
  expected_commands.emplace_back(
      MockPersistentNelStore::Command::Type::ADD_NEL_POLICY, policy1);
  EXPECT_TRUE(store()->VerifyCommands(expected_commands));

  service()->OnRequest(
      MakeRequestDetails(kNak_, kOrigin_.GetURL(), ERR_CONNECTION_REFUSED));
  expected_commands.emplace_back(
      MockPersistentNelStore::Command::Type::UPDATE_NEL_POLICY, policy1);
  EXPECT_TRUE(store()->VerifyCommands(expected_commands));

  service()->QueueSignedExchangeReport(MakeSignedExchangeReportDetails(
      kNak_, false, "sxg.failed", kUrl_, kInnerUrl_, kCertUrl_, kServerIP_));
  expected_commands.emplace_back(
      MockPersistentNelStore::Command::Type::UPDATE_NEL_POLICY, policy1);
  EXPECT_TRUE(store()->VerifyCommands(expected_commands));

  // Removes policy1 but not policy2.
  service()->RemoveBrowsingData(
      base::BindRepeating([](const url::Origin& origin) -> bool {
        return origin.host() == "example.com";
      }));
  expected_commands.emplace_back(
      MockPersistentNelStore::Command::Type::DELETE_NEL_POLICY, policy1);
  expected_commands.emplace_back(MockPersistentNelStore::Command::Type::FLUSH);
  EXPECT_TRUE(store()->VerifyCommands(expected_commands));

  service()->RemoveAllBrowsingData();
  // We failed to load policy2 from the store, so there is nothing to remove
  // here.
  expected_commands.emplace_back(MockPersistentNelStore::Command::Type::FLUSH);
  EXPECT_TRUE(store()->VerifyCommands(expected_commands));
}

TEST_P(NetworkErrorLoggingServiceTest, SendsCommandsToStoreDeferredLoadFailed) {
  if (!store())
    return;

  MockPersistentNelStore::CommandList expected_commands;
  NetworkErrorLoggingService::NelPolicy policy1 = MakePolicy(kNak_, kOrigin_);
  NetworkErrorLoggingService::NelPolicy policy2 =
      MakePolicy(kNak_, kOriginDifferentHost_);
  std::vector<NetworkErrorLoggingService::NelPolicy> prestored_policies = {
      policy1, policy2};
  store()->SetPrestoredPolicies(std::move(prestored_policies));

  // The first call to any of the public methods triggers a load.
  service()->OnHeader(kNak_, kOrigin_, kServerIP_, kHeader_);
  expected_commands.emplace_back(
      MockPersistentNelStore::Command::Type::LOAD_NEL_POLICIES);
  EXPECT_TRUE(store()->VerifyCommands(expected_commands));

  service()->OnRequest(
      MakeRequestDetails(kNak_, kOrigin_.GetURL(), ERR_CONNECTION_REFUSED));
  EXPECT_TRUE(store()->VerifyCommands(expected_commands));

  service()->QueueSignedExchangeReport(MakeSignedExchangeReportDetails(
      kNak_, false, "sxg.failed", kUrl_, kInnerUrl_, kCertUrl_, kServerIP_));
  EXPECT_TRUE(store()->VerifyCommands(expected_commands));

  // Removes policy1 but not policy2.
  service()->RemoveBrowsingData(
      base::BindRepeating([](const url::Origin& origin) -> bool {
        return origin.host() == "example.com";
      }));
  EXPECT_TRUE(store()->VerifyCommands(expected_commands));

  service()->RemoveAllBrowsingData();
  EXPECT_TRUE(store()->VerifyCommands(expected_commands));

  FinishLoading(false /* load_success */);
  // DoOnHeader()
  // Because the load failed, there will be no policies in memory, so the store
  // is not told to delete anything.
  expected_commands.emplace_back(
      MockPersistentNelStore::Command::Type::ADD_NEL_POLICY, policy1);
  // DoOnRequest()
  expected_commands.emplace_back(
      MockPersistentNelStore::Command::Type::UPDATE_NEL_POLICY, policy1);
  // DoQueueSignedExchangeReport()
  expected_commands.emplace_back(
      MockPersistentNelStore::Command::Type::UPDATE_NEL_POLICY, policy1);
  // DoRemoveBrowsingData()
  expected_commands.emplace_back(
      MockPersistentNelStore::Command::Type::DELETE_NEL_POLICY, policy1);
  expected_commands.emplace_back(MockPersistentNelStore::Command::Type::FLUSH);
  // DoRemoveAllBrowsingData()
  // We failed to load policy2 from the store, so there is nothing to remove
  // here.
  expected_commands.emplace_back(MockPersistentNelStore::Command::Type::FLUSH);
  EXPECT_TRUE(store()->VerifyCommands(expected_commands));
}

TEST_P(NetworkErrorLoggingServiceTest, FlushesStoreOnDestruction) {
  auto store = std::make_unique<MockPersistentNelStore>();
  std::unique_ptr<NetworkErrorLoggingService> service =
      NetworkErrorLoggingService::Create(store.get());

  MockPersistentNelStore::CommandList expected_commands;

  service->OnHeader(kNak_, kOrigin_, kServerIP_, kHeader_);
  expected_commands.emplace_back(
      MockPersistentNelStore::Command::Type::LOAD_NEL_POLICIES);
  EXPECT_TRUE(store->VerifyCommands(expected_commands));

  store->FinishLoading(false /* load_success */);
  expected_commands.emplace_back(
      MockPersistentNelStore::Command::Type::ADD_NEL_POLICY,
      MakePolicy(kNak_, kOrigin_));
  EXPECT_TRUE(store->VerifyCommands(expected_commands));

  // Store should be flushed on destruction of service.
  service.reset();
  expected_commands.emplace_back(MockPersistentNelStore::Command::Type::FLUSH);
  EXPECT_TRUE(store->VerifyCommands(expected_commands));
}

TEST_P(NetworkErrorLoggingServiceTest,
       DoesntFlushStoreOnDestructionBeforeLoad) {
  auto store = std::make_unique<MockPersistentNelStore>();
  std::unique_ptr<NetworkErrorLoggingService> service =
      NetworkErrorLoggingService::Create(store.get());

  service.reset();
  EXPECT_EQ(0u, store->GetAllCommands().size());
}

TEST_P(NetworkErrorLoggingServiceTest, DoNothingIfShutDown) {
  if (!store())
    return;

  MockPersistentNelStore::CommandList expected_commands;

  // The first call to any of the public methods triggers a load.
  service()->OnHeader(kNak_, kOrigin_, kServerIP_, kHeader_);
  expected_commands.emplace_back(
      MockPersistentNelStore::Command::Type::LOAD_NEL_POLICIES);
  EXPECT_TRUE(store()->VerifyCommands(expected_commands));

  service()->OnRequest(
      MakeRequestDetails(kNak_, kOrigin_.GetURL(), ERR_CONNECTION_REFUSED));
  service()->QueueSignedExchangeReport(MakeSignedExchangeReportDetails(
      kNak_, false, "sxg.failed", kUrl_, kInnerUrl_, kCertUrl_, kServerIP_));
  service()->RemoveBrowsingData(
      base::BindRepeating([](const url::Origin& origin) -> bool {
        return origin.host() == "example.com";
      }));
  service()->RemoveAllBrowsingData();

  // Finish loading after the service has been shut down.
  service()->OnShutdown();
  FinishLoading(true /* load_success */);

  // Only the LOAD command should have been sent to the store.
  EXPECT_EQ(1u, store()->GetAllCommands().size());
  EXPECT_EQ(0u, PolicyCount());
  EXPECT_EQ(0u, reports().size());
}

INSTANTIATE_TEST_SUITE_P(NetworkErrorLoggingServiceStoreTest,
                         NetworkErrorLoggingServiceTest,
                         testing::Bool());

}  // namespace
}  // namespace net
