// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/test/simple_test_clock.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
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
  NetworkErrorLoggingServiceTest() {
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
      GURL url,
      Error error_type,
      std::string method = "GET",
      int status_code = 0,
      IPAddress server_ip = IPAddress()) {
    NetworkErrorLoggingService::RequestDetails details;

    details.uri = url;
    details.referrer = kReferrer_;
    details.user_agent = kUserAgent_;
    details.server_ip = server_ip.IsValid() ? server_ip : kServerIP_;
    details.method = std::move(method);
    details.status_code = status_code;
    details.elapsed_time = base::TimeDelta::FromSeconds(1);
    details.type = error_type;
    details.reporting_upload_depth = 0;

    return details;
  }

  NetworkErrorLoggingService::SignedExchangeReportDetails
  MakeSignedExchangeReportDetails(bool success,
                                  const std::string& type,
                                  const GURL& outer_url,
                                  const GURL& inner_url,
                                  const GURL& cert_url,
                                  const IPAddress& server_ip_address) {
    NetworkErrorLoggingService::SignedExchangeReportDetails details;
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
    details.elapsed_time = base::TimeDelta::FromMilliseconds(1234);
    details.user_agent = kUserAgent_;
    return details;
  }
  NetworkErrorLoggingService* service() { return service_.get(); }
  MockPersistentNelStore* store() { return store_.get(); }
  const std::vector<TestReportingService::Report>& reports() {
    return reporting_service_->reports();
  }

  const url::Origin MakeOrigin(size_t index) {
    GURL url(base::StringPrintf("https://example%zd.com/", index));
    return url::Origin::Create(url);
  }

  NetworkErrorLoggingService::NelPolicy MakePolicyForOrigin(
      url::Origin origin,
      base::Time expires = base::Time(),
      base::Time last_used = base::Time()) {
    NetworkErrorLoggingService::NelPolicy policy;
    policy.origin = std::move(origin);
    policy.expires = expires;
    policy.last_used = last_used;

    return policy;
  }

  // Returns whether the NetworkErrorLoggingService has a policy corresponding
  // to |origin|. Returns true if so, even if the policy is expired.
  bool HasPolicyForOrigin(const url::Origin& origin) {
    std::set<url::Origin> all_policy_origins =
        service_->GetPolicyOriginsForTesting();
    return all_policy_origins.find(origin) != all_policy_origins.end();
  }

  size_t PolicyCount() { return service_->GetPolicyOriginsForTesting().size(); }

  // Makes the rest of the test run synchronously.
  void FinishLoading(bool load_success) {
    if (store())
      store()->FinishLoading(load_success);
  }

  const GURL kUrl_ = GURL("https://example.com/path");
  const GURL kUrlDifferentPort_ = GURL("https://example.com:4433/path");
  const GURL kUrlSubdomain_ = GURL("https://subdomain.example.com/path");
  const GURL kUrlDifferentHost_ = GURL("https://example2.com/path");
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

  const std::string kHeader_ = "{\"report_to\":\"group\",\"max_age\":86400}";
  const std::string kHeaderSuccessFraction0_ =
      "{\"report_to\":\"group\",\"max_age\":86400,\"success_fraction\":0.0}";
  const std::string kHeaderSuccessFraction1_ =
      "{\"report_to\":\"group\",\"max_age\":86400,\"success_fraction\":1.0}";
  const std::string kHeaderIncludeSubdomains_ =
      "{\"report_to\":\"group\",\"max_age\":86400,\"include_subdomains\":true}";
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

  // |store_| needs to outlive |service_|.
  std::unique_ptr<MockPersistentNelStore> store_;
  std::unique_ptr<NetworkErrorLoggingService> service_;
  std::unique_ptr<TestReportingService> reporting_service_;
};

void ExpectDictDoubleValue(double expected_value,
                           const base::DictionaryValue& value,
                           const std::string& key) {
  double double_value = 0.0;
  EXPECT_TRUE(value.GetDouble(key, &double_value)) << key;
  EXPECT_DOUBLE_EQ(expected_value, double_value) << key;
}

TEST_P(NetworkErrorLoggingServiceTest, CreateService) {
  // Service is created by default in the test fixture..
  EXPECT_TRUE(service());
}

TEST_P(NetworkErrorLoggingServiceTest, NoReportingService) {
  service_ = NetworkErrorLoggingService::Create(store_.get());

  service()->OnHeader(kOrigin_, kServerIP_, kHeader_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  // Should not crash.
  service()->OnRequest(MakeRequestDetails(kUrl_, ERR_CONNECTION_REFUSED));
}

TEST_P(NetworkErrorLoggingServiceTest, NoPolicyForOrigin) {
  service()->OnRequest(MakeRequestDetails(kUrl_, ERR_CONNECTION_REFUSED));

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  EXPECT_TRUE(reports().empty());
}

TEST_P(NetworkErrorLoggingServiceTest, JsonTooLong) {
  service()->OnHeader(kOrigin_, kServerIP_, kHeaderTooLong_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  service()->OnRequest(MakeRequestDetails(kUrl_, ERR_CONNECTION_REFUSED));

  EXPECT_TRUE(reports().empty());
}

TEST_P(NetworkErrorLoggingServiceTest, JsonTooDeep) {
  service()->OnHeader(kOrigin_, kServerIP_, kHeaderTooDeep_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  service()->OnRequest(MakeRequestDetails(kUrl_, ERR_CONNECTION_REFUSED));

  EXPECT_TRUE(reports().empty());
}

TEST_P(NetworkErrorLoggingServiceTest, IncludeSubdomainsEtldRejected) {
  service()->OnHeader(kOriginEtld_, kServerIP_, kHeaderIncludeSubdomains_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  EXPECT_EQ(0u, PolicyCount());

  service()->OnRequest(MakeRequestDetails(kUrlEtld_, ERR_CONNECTION_REFUSED));

  EXPECT_TRUE(reports().empty());
}

TEST_P(NetworkErrorLoggingServiceTest, NonIncludeSubdomainsEtldAccepted) {
  service()->OnHeader(kOriginEtld_, kServerIP_, kHeader_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  EXPECT_EQ(1u, PolicyCount());

  service()->OnRequest(MakeRequestDetails(kUrlEtld_, ERR_CONNECTION_REFUSED));

  EXPECT_EQ(1u, reports().size());
  EXPECT_EQ(kUrlEtld_, reports()[0].url);
}

TEST_P(NetworkErrorLoggingServiceTest, SuccessReportQueued) {
  service()->OnHeader(kOrigin_, kServerIP_, kHeaderSuccessFraction1_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  service()->OnRequest(MakeRequestDetails(kUrl_, OK));

  ASSERT_EQ(1u, reports().size());
  EXPECT_EQ(kUrl_, reports()[0].url);
  EXPECT_EQ(kUserAgent_, reports()[0].user_agent);
  EXPECT_EQ(kGroup_, reports()[0].group);
  EXPECT_EQ(kType_, reports()[0].type);
  EXPECT_EQ(0, reports()[0].depth);

  const base::DictionaryValue* body;
  ASSERT_TRUE(reports()[0].body->GetAsDictionary(&body));
  base::ExpectDictStringValue(kReferrer_.spec(), *body,
                              NetworkErrorLoggingService::kReferrerKey);
  // TODO(juliatuttle): Extract these constants.
  ExpectDictDoubleValue(1.0, *body,
                        NetworkErrorLoggingService::kSamplingFractionKey);
  base::ExpectDictStringValue(kServerIP_.ToString(), *body,
                              NetworkErrorLoggingService::kServerIpKey);
  base::ExpectDictStringValue("", *body,
                              NetworkErrorLoggingService::kProtocolKey);
  base::ExpectDictStringValue("GET", *body,
                              NetworkErrorLoggingService::kMethodKey);
  base::ExpectDictIntegerValue(0, *body,
                               NetworkErrorLoggingService::kStatusCodeKey);
  base::ExpectDictIntegerValue(1000, *body,
                               NetworkErrorLoggingService::kElapsedTimeKey);
  base::ExpectDictStringValue("application", *body,
                              NetworkErrorLoggingService::kPhaseKey);
  base::ExpectDictStringValue("ok", *body,
                              NetworkErrorLoggingService::kTypeKey);
}

TEST_P(NetworkErrorLoggingServiceTest, FailureReportQueued) {
  static const std::string kHeaderFailureFraction1 =
      "{\"report_to\":\"group\",\"max_age\":86400,\"failure_fraction\":1.0}";
  service()->OnHeader(kOrigin_, kServerIP_, kHeaderFailureFraction1);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  service()->OnRequest(MakeRequestDetails(kUrl_, ERR_CONNECTION_REFUSED));

  ASSERT_EQ(1u, reports().size());
  EXPECT_EQ(kUrl_, reports()[0].url);
  EXPECT_EQ(kUserAgent_, reports()[0].user_agent);
  EXPECT_EQ(kGroup_, reports()[0].group);
  EXPECT_EQ(kType_, reports()[0].type);
  EXPECT_EQ(0, reports()[0].depth);

  const base::DictionaryValue* body;
  ASSERT_TRUE(reports()[0].body->GetAsDictionary(&body));
  base::ExpectDictStringValue(kReferrer_.spec(), *body,
                              NetworkErrorLoggingService::kReferrerKey);
  // TODO(juliatuttle): Extract these constants.
  ExpectDictDoubleValue(1.0, *body,
                        NetworkErrorLoggingService::kSamplingFractionKey);
  base::ExpectDictStringValue(kServerIP_.ToString(), *body,
                              NetworkErrorLoggingService::kServerIpKey);
  base::ExpectDictStringValue("", *body,
                              NetworkErrorLoggingService::kProtocolKey);
  base::ExpectDictStringValue("GET", *body,
                              NetworkErrorLoggingService::kMethodKey);
  base::ExpectDictIntegerValue(0, *body,
                               NetworkErrorLoggingService::kStatusCodeKey);
  base::ExpectDictIntegerValue(1000, *body,
                               NetworkErrorLoggingService::kElapsedTimeKey);
  base::ExpectDictStringValue("connection", *body,
                              NetworkErrorLoggingService::kPhaseKey);
  base::ExpectDictStringValue("tcp.refused", *body,
                              NetworkErrorLoggingService::kTypeKey);
}

TEST_P(NetworkErrorLoggingServiceTest, UnknownFailureReportQueued) {
  static const std::string kHeaderFailureFraction1 =
      "{\"report_to\":\"group\",\"max_age\":86400,\"failure_fraction\":1.0}";
  service()->OnHeader(kOrigin_, kServerIP_, kHeaderFailureFraction1);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  // This error code happens to not be mapped to a NEL report `type` field
  // value.
  service()->OnRequest(MakeRequestDetails(kUrl_, ERR_FILE_NO_SPACE));

  ASSERT_EQ(1u, reports().size());
  const base::DictionaryValue* body;
  ASSERT_TRUE(reports()[0].body->GetAsDictionary(&body));
  base::ExpectDictStringValue("application", *body,
                              NetworkErrorLoggingService::kPhaseKey);
  base::ExpectDictStringValue("unknown", *body,
                              NetworkErrorLoggingService::kTypeKey);
}

TEST_P(NetworkErrorLoggingServiceTest, UnknownCertFailureReportQueued) {
  static const std::string kHeaderFailureFraction1 =
      "{\"report_to\":\"group\",\"max_age\":86400,\"failure_fraction\":1.0}";
  service()->OnHeader(kOrigin_, kServerIP_, kHeaderFailureFraction1);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  // This error code happens to not be mapped to a NEL report `type` field
  // value.  Because it's a certificate error, we'll set the `phase` to be
  // `connection`.
  service()->OnRequest(MakeRequestDetails(kUrl_, ERR_CERT_NON_UNIQUE_NAME));

  ASSERT_EQ(1u, reports().size());
  const base::DictionaryValue* body;
  ASSERT_TRUE(reports()[0].body->GetAsDictionary(&body));
  base::ExpectDictStringValue("connection", *body,
                              NetworkErrorLoggingService::kPhaseKey);
  base::ExpectDictStringValue("unknown", *body,
                              NetworkErrorLoggingService::kTypeKey);
}

TEST_P(NetworkErrorLoggingServiceTest, HttpErrorReportQueued) {
  static const std::string kHeaderFailureFraction1 =
      "{\"report_to\":\"group\",\"max_age\":86400,\"failure_fraction\":1.0}";
  service()->OnHeader(kOrigin_, kServerIP_, kHeaderFailureFraction1);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  service()->OnRequest(MakeRequestDetails(kUrl_, OK, "GET", 504));

  ASSERT_EQ(1u, reports().size());
  EXPECT_EQ(kUrl_, reports()[0].url);
  EXPECT_EQ(kUserAgent_, reports()[0].user_agent);
  EXPECT_EQ(kGroup_, reports()[0].group);
  EXPECT_EQ(kType_, reports()[0].type);
  EXPECT_EQ(0, reports()[0].depth);

  const base::DictionaryValue* body;
  ASSERT_TRUE(reports()[0].body->GetAsDictionary(&body));
  base::ExpectDictStringValue(kReferrer_.spec(), *body,
                              NetworkErrorLoggingService::kReferrerKey);
  // TODO(juliatuttle): Extract these constants.
  ExpectDictDoubleValue(1.0, *body,
                        NetworkErrorLoggingService::kSamplingFractionKey);
  base::ExpectDictStringValue(kServerIP_.ToString(), *body,
                              NetworkErrorLoggingService::kServerIpKey);
  base::ExpectDictStringValue("", *body,
                              NetworkErrorLoggingService::kProtocolKey);
  base::ExpectDictStringValue("GET", *body,
                              NetworkErrorLoggingService::kMethodKey);
  base::ExpectDictIntegerValue(504, *body,
                               NetworkErrorLoggingService::kStatusCodeKey);
  base::ExpectDictIntegerValue(1000, *body,
                               NetworkErrorLoggingService::kElapsedTimeKey);
  base::ExpectDictStringValue("application", *body,
                              NetworkErrorLoggingService::kPhaseKey);
  base::ExpectDictStringValue("http.error", *body,
                              NetworkErrorLoggingService::kTypeKey);
}

TEST_P(NetworkErrorLoggingServiceTest, SuccessReportDowngraded) {
  service()->OnHeader(kOrigin_, kServerIP_, kHeaderSuccessFraction1_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  service()->OnRequest(
      MakeRequestDetails(kUrl_, OK, "GET", 200, kOtherServerIP_));

  ASSERT_EQ(1u, reports().size());
  EXPECT_EQ(kUrl_, reports()[0].url);
  EXPECT_EQ(kGroup_, reports()[0].group);
  EXPECT_EQ(kType_, reports()[0].type);
  EXPECT_EQ(0, reports()[0].depth);

  const base::DictionaryValue* body;
  ASSERT_TRUE(reports()[0].body->GetAsDictionary(&body));
  base::ExpectDictStringValue(kReferrer_.spec(), *body,
                              NetworkErrorLoggingService::kReferrerKey);
  ExpectDictDoubleValue(1.0, *body,
                        NetworkErrorLoggingService::kSamplingFractionKey);
  base::ExpectDictStringValue(kOtherServerIP_.ToString(), *body,
                              NetworkErrorLoggingService::kServerIpKey);
  base::ExpectDictStringValue("", *body,
                              NetworkErrorLoggingService::kProtocolKey);
  base::ExpectDictStringValue("GET", *body,
                              NetworkErrorLoggingService::kMethodKey);
  base::ExpectDictIntegerValue(0, *body,
                               NetworkErrorLoggingService::kStatusCodeKey);
  base::ExpectDictIntegerValue(0, *body,
                               NetworkErrorLoggingService::kElapsedTimeKey);
  base::ExpectDictStringValue("dns", *body,
                              NetworkErrorLoggingService::kPhaseKey);
  base::ExpectDictStringValue("dns.address_changed", *body,
                              NetworkErrorLoggingService::kTypeKey);
}

TEST_P(NetworkErrorLoggingServiceTest, FailureReportDowngraded) {
  service()->OnHeader(kOrigin_, kServerIP_, kHeaderSuccessFraction1_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  service()->OnRequest(MakeRequestDetails(kUrl_, ERR_CONNECTION_REFUSED, "GET",
                                          200, kOtherServerIP_));

  ASSERT_EQ(1u, reports().size());
  EXPECT_EQ(kUrl_, reports()[0].url);
  EXPECT_EQ(kGroup_, reports()[0].group);
  EXPECT_EQ(kType_, reports()[0].type);
  EXPECT_EQ(0, reports()[0].depth);

  const base::DictionaryValue* body;
  ASSERT_TRUE(reports()[0].body->GetAsDictionary(&body));
  base::ExpectDictStringValue(kReferrer_.spec(), *body,
                              NetworkErrorLoggingService::kReferrerKey);
  ExpectDictDoubleValue(1.0, *body,
                        NetworkErrorLoggingService::kSamplingFractionKey);
  base::ExpectDictStringValue(kOtherServerIP_.ToString(), *body,
                              NetworkErrorLoggingService::kServerIpKey);
  base::ExpectDictStringValue("", *body,
                              NetworkErrorLoggingService::kProtocolKey);
  base::ExpectDictStringValue("GET", *body,
                              NetworkErrorLoggingService::kMethodKey);
  base::ExpectDictIntegerValue(0, *body,
                               NetworkErrorLoggingService::kStatusCodeKey);
  base::ExpectDictIntegerValue(0, *body,
                               NetworkErrorLoggingService::kElapsedTimeKey);
  base::ExpectDictStringValue("dns", *body,
                              NetworkErrorLoggingService::kPhaseKey);
  base::ExpectDictStringValue("dns.address_changed", *body,
                              NetworkErrorLoggingService::kTypeKey);
}

TEST_P(NetworkErrorLoggingServiceTest, HttpErrorReportDowngraded) {
  service()->OnHeader(kOrigin_, kServerIP_, kHeaderSuccessFraction1_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  service()->OnRequest(
      MakeRequestDetails(kUrl_, OK, "GET", 504, kOtherServerIP_));

  ASSERT_EQ(1u, reports().size());
  EXPECT_EQ(kUrl_, reports()[0].url);
  EXPECT_EQ(kGroup_, reports()[0].group);
  EXPECT_EQ(kType_, reports()[0].type);
  EXPECT_EQ(0, reports()[0].depth);

  const base::DictionaryValue* body;
  ASSERT_TRUE(reports()[0].body->GetAsDictionary(&body));
  base::ExpectDictStringValue(kReferrer_.spec(), *body,
                              NetworkErrorLoggingService::kReferrerKey);
  ExpectDictDoubleValue(1.0, *body,
                        NetworkErrorLoggingService::kSamplingFractionKey);
  base::ExpectDictStringValue(kOtherServerIP_.ToString(), *body,
                              NetworkErrorLoggingService::kServerIpKey);
  base::ExpectDictStringValue("", *body,
                              NetworkErrorLoggingService::kProtocolKey);
  base::ExpectDictStringValue("GET", *body,
                              NetworkErrorLoggingService::kMethodKey);
  base::ExpectDictIntegerValue(0, *body,
                               NetworkErrorLoggingService::kStatusCodeKey);
  base::ExpectDictIntegerValue(0, *body,
                               NetworkErrorLoggingService::kElapsedTimeKey);
  base::ExpectDictStringValue("dns", *body,
                              NetworkErrorLoggingService::kPhaseKey);
  base::ExpectDictStringValue("dns.address_changed", *body,
                              NetworkErrorLoggingService::kTypeKey);
}

TEST_P(NetworkErrorLoggingServiceTest, DNSFailureReportNotDowngraded) {
  service()->OnHeader(kOrigin_, kServerIP_, kHeaderSuccessFraction1_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  service()->OnRequest(MakeRequestDetails(kUrl_, ERR_NAME_NOT_RESOLVED, "GET",
                                          0, kOtherServerIP_));

  ASSERT_EQ(1u, reports().size());
  EXPECT_EQ(kUrl_, reports()[0].url);
  EXPECT_EQ(kGroup_, reports()[0].group);
  EXPECT_EQ(kType_, reports()[0].type);
  EXPECT_EQ(0, reports()[0].depth);

  const base::DictionaryValue* body;
  ASSERT_TRUE(reports()[0].body->GetAsDictionary(&body));
  base::ExpectDictStringValue(kReferrer_.spec(), *body,
                              NetworkErrorLoggingService::kReferrerKey);
  ExpectDictDoubleValue(1.0, *body,
                        NetworkErrorLoggingService::kSamplingFractionKey);
  base::ExpectDictStringValue(kOtherServerIP_.ToString(), *body,
                              NetworkErrorLoggingService::kServerIpKey);
  base::ExpectDictStringValue("", *body,
                              NetworkErrorLoggingService::kProtocolKey);
  base::ExpectDictStringValue("GET", *body,
                              NetworkErrorLoggingService::kMethodKey);
  base::ExpectDictIntegerValue(0, *body,
                               NetworkErrorLoggingService::kStatusCodeKey);
  base::ExpectDictIntegerValue(1000, *body,
                               NetworkErrorLoggingService::kElapsedTimeKey);
  base::ExpectDictStringValue("dns", *body,
                              NetworkErrorLoggingService::kPhaseKey);
  base::ExpectDictStringValue("dns.name_not_resolved", *body,
                              NetworkErrorLoggingService::kTypeKey);
}

TEST_P(NetworkErrorLoggingServiceTest, SuccessPOSTReportQueued) {
  service()->OnHeader(kOrigin_, kServerIP_, kHeaderSuccessFraction1_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  service()->OnRequest(MakeRequestDetails(kUrl_, OK, "POST"));

  ASSERT_EQ(1u, reports().size());
  EXPECT_EQ(kUrl_, reports()[0].url);
  EXPECT_EQ(kGroup_, reports()[0].group);
  EXPECT_EQ(kType_, reports()[0].type);
  EXPECT_EQ(0, reports()[0].depth);

  const base::DictionaryValue* body;
  ASSERT_TRUE(reports()[0].body->GetAsDictionary(&body));
  base::ExpectDictStringValue(kReferrer_.spec(), *body,
                              NetworkErrorLoggingService::kReferrerKey);
  ExpectDictDoubleValue(1.0, *body,
                        NetworkErrorLoggingService::kSamplingFractionKey);
  base::ExpectDictStringValue(kServerIP_.ToString(), *body,
                              NetworkErrorLoggingService::kServerIpKey);
  base::ExpectDictStringValue("", *body,
                              NetworkErrorLoggingService::kProtocolKey);
  base::ExpectDictStringValue("POST", *body,
                              NetworkErrorLoggingService::kMethodKey);
  base::ExpectDictStringValue("application", *body,
                              NetworkErrorLoggingService::kPhaseKey);
  base::ExpectDictStringValue("ok", *body,
                              NetworkErrorLoggingService::kTypeKey);
}

TEST_P(NetworkErrorLoggingServiceTest, MaxAge0) {
  service()->OnHeader(kOrigin_, kServerIP_, kHeader_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  EXPECT_EQ(1u, PolicyCount());

  // Max_age of 0 removes the policy.
  service()->OnHeader(kOrigin_, kServerIP_, kHeaderMaxAge0_);
  EXPECT_EQ(0u, PolicyCount());

  service()->OnRequest(MakeRequestDetails(kUrl_, ERR_CONNECTION_REFUSED));

  EXPECT_TRUE(reports().empty());
}

TEST_P(NetworkErrorLoggingServiceTest, SuccessFraction0) {
  service()->OnHeader(kOrigin_, kServerIP_, kHeaderSuccessFraction0_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  // Each network error has a 0% chance of being reported.  Fire off several and
  // verify that no reports are produced.
  constexpr size_t kReportCount = 100;
  for (size_t i = 0; i < kReportCount; ++i)
    service()->OnRequest(MakeRequestDetails(kUrl_, OK));

  EXPECT_TRUE(reports().empty());
}

TEST_P(NetworkErrorLoggingServiceTest, SuccessFractionHalf) {
  // Include a different value for failure_fraction to ensure that we copy the
  // right value into sampling_fraction.
  static const std::string kHeaderSuccessFractionHalf =
      "{\"report_to\":\"group\",\"max_age\":86400,\"success_fraction\":0.5,"
      "\"failure_fraction\":0.25}";
  service()->OnHeader(kOrigin_, kServerIP_, kHeaderSuccessFractionHalf);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  // Each network error has a 50% chance of being reported.  Fire off several
  // and verify that some requests were reported and some weren't.  (We can't
  // verify exact counts because each decision is made randomly.)
  constexpr size_t kReportCount = 100;
  for (size_t i = 0; i < kReportCount; ++i)
    service()->OnRequest(MakeRequestDetails(kUrl_, OK));

  // If our random selection logic is correct, there is a 2^-100 chance that
  // every single report above was skipped.  If this check fails, it's much more
  // likely that our code is wrong.
  EXPECT_FALSE(reports().empty());

  // There's also a 2^-100 chance that every single report was logged.  Same as
  // above, that's much more likely to be a code error.
  EXPECT_GT(kReportCount, reports().size());

  for (const auto& report : reports()) {
    const base::DictionaryValue* body;
    ASSERT_TRUE(report.body->GetAsDictionary(&body));
    // Our header includes a different value for failure_fraction, so that this
    // check verifies that we copy the correct fraction into sampling_fraction.
    ExpectDictDoubleValue(0.5, *body,
                          NetworkErrorLoggingService::kSamplingFractionKey);
  }
}

TEST_P(NetworkErrorLoggingServiceTest, FailureFraction0) {
  static const std::string kHeaderFailureFraction0 =
      "{\"report_to\":\"group\",\"max_age\":86400,\"failure_fraction\":0.0}";
  service()->OnHeader(kOrigin_, kServerIP_, kHeaderFailureFraction0);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  // Each network error has a 0% chance of being reported.  Fire off several and
  // verify that no reports are produced.
  constexpr size_t kReportCount = 100;
  for (size_t i = 0; i < kReportCount; ++i)
    service()->OnRequest(MakeRequestDetails(kUrl_, ERR_CONNECTION_REFUSED));

  EXPECT_TRUE(reports().empty());
}

TEST_P(NetworkErrorLoggingServiceTest, FailureFractionHalf) {
  // Include a different value for success_fraction to ensure that we copy the
  // right value into sampling_fraction.
  static const std::string kHeaderFailureFractionHalf =
      "{\"report_to\":\"group\",\"max_age\":86400,\"failure_fraction\":0.5,"
      "\"success_fraction\":0.25}";
  service()->OnHeader(kOrigin_, kServerIP_, kHeaderFailureFractionHalf);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  // Each network error has a 50% chance of being reported.  Fire off several
  // and verify that some requests were reported and some weren't.  (We can't
  // verify exact counts because each decision is made randomly.)
  constexpr size_t kReportCount = 100;
  for (size_t i = 0; i < kReportCount; ++i)
    service()->OnRequest(MakeRequestDetails(kUrl_, ERR_CONNECTION_REFUSED));

  // If our random selection logic is correct, there is a 2^-100 chance that
  // every single report above was skipped.  If this check fails, it's much more
  // likely that our code is wrong.
  EXPECT_FALSE(reports().empty());

  // There's also a 2^-100 chance that every single report was logged.  Same as
  // above, that's much more likely to be a code error.
  EXPECT_GT(kReportCount, reports().size());

  for (const auto& report : reports()) {
    const base::DictionaryValue* body;
    ASSERT_TRUE(report.body->GetAsDictionary(&body));
    ExpectDictDoubleValue(0.5, *body,
                          NetworkErrorLoggingService::kSamplingFractionKey);
  }
}

TEST_P(NetworkErrorLoggingServiceTest,
       ExcludeSubdomainsDoesntMatchDifferentPort) {
  service()->OnHeader(kOrigin_, kServerIP_, kHeader_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  service()->OnRequest(
      MakeRequestDetails(kUrlDifferentPort_, ERR_CONNECTION_REFUSED));

  EXPECT_TRUE(reports().empty());
}

TEST_P(NetworkErrorLoggingServiceTest, ExcludeSubdomainsDoesntMatchSubdomain) {
  service()->OnHeader(kOrigin_, kServerIP_, kHeader_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  service()->OnRequest(
      MakeRequestDetails(kUrlSubdomain_, ERR_CONNECTION_REFUSED));

  EXPECT_TRUE(reports().empty());
}

TEST_P(NetworkErrorLoggingServiceTest, IncludeSubdomainsMatchesDifferentPort) {
  service()->OnHeader(kOrigin_, kServerIP_, kHeaderIncludeSubdomains_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  service()->OnRequest(
      MakeRequestDetails(kUrlDifferentPort_, ERR_NAME_NOT_RESOLVED));

  ASSERT_EQ(1u, reports().size());
  EXPECT_EQ(kUrlDifferentPort_, reports()[0].url);
}

TEST_P(NetworkErrorLoggingServiceTest, IncludeSubdomainsMatchesSubdomain) {
  service()->OnHeader(kOrigin_, kServerIP_, kHeaderIncludeSubdomains_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  service()->OnRequest(
      MakeRequestDetails(kUrlSubdomain_, ERR_NAME_NOT_RESOLVED));

  ASSERT_EQ(1u, reports().size());
}

TEST_P(NetworkErrorLoggingServiceTest,
       IncludeSubdomainsDoesntMatchSuperdomain) {
  service()->OnHeader(kOriginSubdomain_, kServerIP_, kHeaderIncludeSubdomains_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  service()->OnRequest(MakeRequestDetails(kUrl_, ERR_NAME_NOT_RESOLVED));

  EXPECT_TRUE(reports().empty());
}

TEST_P(NetworkErrorLoggingServiceTest,
       IncludeSubdomainsDoesntReportConnectionError) {
  service()->OnHeader(kOrigin_, kServerIP_, kHeaderIncludeSubdomains_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  service()->OnRequest(
      MakeRequestDetails(kUrlSubdomain_, ERR_CONNECTION_REFUSED));

  EXPECT_TRUE(reports().empty());
}

TEST_P(NetworkErrorLoggingServiceTest,
       IncludeSubdomainsDoesntReportApplicationError) {
  service()->OnHeader(kOrigin_, kServerIP_, kHeaderIncludeSubdomains_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  service()->OnRequest(
      MakeRequestDetails(kUrlSubdomain_, ERR_INVALID_HTTP_RESPONSE));

  EXPECT_TRUE(reports().empty());
}

TEST_P(NetworkErrorLoggingServiceTest, IncludeSubdomainsDoesntReportSuccess) {
  service()->OnHeader(kOrigin_, kServerIP_, kHeaderIncludeSubdomains_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  service()->OnRequest(MakeRequestDetails(kUrlSubdomain_, OK));

  EXPECT_TRUE(reports().empty());
}

TEST_P(NetworkErrorLoggingServiceTest,
       IncludeSubdomainsReportsSameOriginSuccess) {
  static const std::string kHeaderIncludeSubdomainsSuccess1 =
      "{\"report_to\":\"group\",\"max_age\":86400,"
      "\"include_subdomains\":true,\"success_fraction\":1.0}";
  service()->OnHeader(kOrigin_, kServerIP_, kHeaderIncludeSubdomainsSuccess1);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  service()->OnRequest(MakeRequestDetails(kUrl_, OK));

  ASSERT_EQ(1u, reports().size());
  EXPECT_EQ(kUrl_, reports()[0].url);
}

TEST_P(NetworkErrorLoggingServiceTest, RemoveAllBrowsingData) {
  service()->OnHeader(kOrigin_, kServerIP_, kHeader_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  EXPECT_EQ(1u, PolicyCount());
  EXPECT_TRUE(HasPolicyForOrigin(kOrigin_));

  service()->RemoveAllBrowsingData();

  service()->OnRequest(MakeRequestDetails(kUrl_, ERR_CONNECTION_REFUSED));

  EXPECT_EQ(0u, PolicyCount());
  EXPECT_FALSE(HasPolicyForOrigin(kOrigin_));
  EXPECT_TRUE(reports().empty());
}

TEST_P(NetworkErrorLoggingServiceTest, RemoveSomeBrowsingData) {
  service()->OnHeader(kOrigin_, kServerIP_, kHeader_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  service()->OnHeader(kOriginDifferentHost_, kServerIP_, kHeader_);
  EXPECT_EQ(2u, PolicyCount());

  // Remove policy for kOrigin_ but not kOriginDifferentHost_
  service()->RemoveBrowsingData(
      base::BindRepeating([](const GURL& origin) -> bool {
        return origin.host() == "example.com";
      }));
  EXPECT_EQ(1u, PolicyCount());
  EXPECT_TRUE(HasPolicyForOrigin(kOriginDifferentHost_));
  EXPECT_FALSE(HasPolicyForOrigin(kOrigin_));

  service()->OnRequest(MakeRequestDetails(kUrl_, ERR_CONNECTION_REFUSED));

  EXPECT_TRUE(reports().empty());

  service()->OnRequest(
      MakeRequestDetails(kUrlDifferentHost_, ERR_CONNECTION_REFUSED));

  ASSERT_EQ(1u, reports().size());
}

TEST_P(NetworkErrorLoggingServiceTest, Nested) {
  service()->OnHeader(kOrigin_, kServerIP_, kHeader_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  NetworkErrorLoggingService::RequestDetails details =
      MakeRequestDetails(kUrl_, ERR_CONNECTION_REFUSED);
  details.reporting_upload_depth =
      NetworkErrorLoggingService::kMaxNestedReportDepth;
  service()->OnRequest(details);

  ASSERT_EQ(1u, reports().size());
  EXPECT_EQ(NetworkErrorLoggingService::kMaxNestedReportDepth,
            reports()[0].depth);
}

TEST_P(NetworkErrorLoggingServiceTest, NestedTooDeep) {
  service()->OnHeader(kOrigin_, kServerIP_, kHeader_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  NetworkErrorLoggingService::RequestDetails details =
      MakeRequestDetails(kUrl_, ERR_CONNECTION_REFUSED);
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

  service()->OnHeader(kOrigin_, kServerIP_, kHeaderSuccessFraction1_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  service()->OnHeader(kOriginDifferentHost_, kServerIP_, kHeader_);
  service()->OnHeader(kOriginSubdomain_, kServerIP_, kHeaderIncludeSubdomains_);
  const std::string kHeaderWrongTypes =
      ("{\"report_to\":\"group\","
       "\"max_age\":86400,"
       // We'll ignore each of these fields because they're the wrong type.
       // We'll use a default value instead.
       "\"include_subdomains\":\"true\","
       "\"success_fraction\": \"1.0\","
       "\"failure_fraction\": \"0.0\"}");
  service()->OnHeader(
      url::Origin::Create(GURL("https://invalid-types.example.com")),
      kServerIP_, kHeaderWrongTypes);

  base::Value actual = service()->StatusAsValue();
  std::unique_ptr<base::Value> expected =
      base::test::ParseJsonDeprecated(R"json(
      {
        "originPolicies": [
          {
            "origin": "https://example.com",
            "includeSubdomains": false,
            "expires": "86400000",
            "reportTo": "group",
            "successFraction": 1.0,
            "failureFraction": 1.0,
          },
          {
            "origin": "https://example2.com",
            "includeSubdomains": false,
            "expires": "86400000",
            "reportTo": "group",
            "successFraction": 0.0,
            "failureFraction": 1.0,
          },
          {
            "origin": "https://invalid-types.example.com",
            "includeSubdomains": false,
            "expires": "86400000",
            "reportTo": "group",
            "successFraction": 0.0,
            "failureFraction": 1.0,
          },
          {
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
  EXPECT_EQ(*expected, actual);
}

TEST_P(NetworkErrorLoggingServiceTest, NoReportingService_SignedExchange) {
  service_ = NetworkErrorLoggingService::Create(store_.get());

  service()->OnHeader(kOrigin_, kServerIP_, kHeader_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  // Should not crash
  service()->QueueSignedExchangeReport(MakeSignedExchangeReportDetails(
      false, "sxg.failed", kUrl_, kInnerUrl_, kCertUrl_, kServerIP_));
}

TEST_P(NetworkErrorLoggingServiceTest, NoPolicyForOrigin_SignedExchange) {
  service()->QueueSignedExchangeReport(MakeSignedExchangeReportDetails(
      false, "sxg.failed", kUrl_, kInnerUrl_, kCertUrl_, kServerIP_));

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  EXPECT_TRUE(reports().empty());
}

TEST_P(NetworkErrorLoggingServiceTest, SuccessFraction0_SignedExchange) {
  service()->OnHeader(kOrigin_, kServerIP_, kHeaderSuccessFraction0_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  // Each network error has a 0% chance of being reported.  Fire off several and
  // verify that no reports are produced.
  constexpr size_t kReportCount = 100;
  for (size_t i = 0; i < kReportCount; ++i) {
    service()->QueueSignedExchangeReport(MakeSignedExchangeReportDetails(
        true, "ok", kUrl_, kInnerUrl_, kCertUrl_, kServerIP_));
  }

  EXPECT_TRUE(reports().empty());
}

TEST_P(NetworkErrorLoggingServiceTest, SuccessReportQueued_SignedExchange) {
  service()->OnHeader(kOrigin_, kServerIP_, kHeaderSuccessFraction1_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  service()->QueueSignedExchangeReport(MakeSignedExchangeReportDetails(
      true, "ok", kUrl_, kInnerUrl_, kCertUrl_, kServerIP_));
  ASSERT_EQ(1u, reports().size());
  EXPECT_EQ(kUrl_, reports()[0].url);
  EXPECT_EQ(kUserAgent_, reports()[0].user_agent);
  EXPECT_EQ(kGroup_, reports()[0].group);
  EXPECT_EQ(kType_, reports()[0].type);
  EXPECT_EQ(0, reports()[0].depth);

  const base::DictionaryValue* body;
  ASSERT_TRUE(reports()[0].body->GetAsDictionary(&body));
  base::ExpectDictStringValue(kReferrer_.spec(), *body,
                              NetworkErrorLoggingService::kReferrerKey);
  ExpectDictDoubleValue(1.0, *body,
                        NetworkErrorLoggingService::kSamplingFractionKey);
  base::ExpectDictStringValue(kServerIP_.ToString(), *body,
                              NetworkErrorLoggingService::kServerIpKey);
  base::ExpectDictStringValue("http/1.1", *body,
                              NetworkErrorLoggingService::kProtocolKey);
  base::ExpectDictStringValue("GET", *body,
                              NetworkErrorLoggingService::kMethodKey);
  base::ExpectDictIntegerValue(200, *body,
                               NetworkErrorLoggingService::kStatusCodeKey);
  base::ExpectDictIntegerValue(1234, *body,
                               NetworkErrorLoggingService::kElapsedTimeKey);
  base::ExpectDictStringValue(
      NetworkErrorLoggingService::kSignedExchangePhaseValue, *body,
      NetworkErrorLoggingService::kPhaseKey);
  base::ExpectDictStringValue("ok", *body,
                              NetworkErrorLoggingService::kTypeKey);

  const base::DictionaryValue* sxg_body;
  ASSERT_TRUE(body->FindKey(NetworkErrorLoggingService::kSignedExchangeBodyKey)
                  ->GetAsDictionary(&sxg_body));

  base::ExpectDictStringValue(kUrl_.spec(), *sxg_body,
                              NetworkErrorLoggingService::kOuterUrlKey);
  base::ExpectDictStringValue(kInnerUrl_.spec(), *sxg_body,
                              NetworkErrorLoggingService::kInnerUrlKey);
  base::ExpectStringValue(
      kCertUrl_.spec(),
      sxg_body->FindKey(NetworkErrorLoggingService::kCertUrlKey)->GetList()[0]);
}

TEST_P(NetworkErrorLoggingServiceTest, FailureReportQueued_SignedExchange) {
  service()->OnHeader(kOrigin_, kServerIP_, kHeader_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  service()->QueueSignedExchangeReport(MakeSignedExchangeReportDetails(
      false, "sxg.failed", kUrl_, kInnerUrl_, kCertUrl_, kServerIP_));
  ASSERT_EQ(1u, reports().size());
  EXPECT_EQ(kUrl_, reports()[0].url);
  EXPECT_EQ(kUserAgent_, reports()[0].user_agent);
  EXPECT_EQ(kGroup_, reports()[0].group);
  EXPECT_EQ(kType_, reports()[0].type);
  EXPECT_EQ(0, reports()[0].depth);

  const base::DictionaryValue* body;
  ASSERT_TRUE(reports()[0].body->GetAsDictionary(&body));
  base::ExpectDictStringValue(kReferrer_.spec(), *body,
                              NetworkErrorLoggingService::kReferrerKey);
  ExpectDictDoubleValue(1.0, *body,
                        NetworkErrorLoggingService::kSamplingFractionKey);
  base::ExpectDictStringValue(kServerIP_.ToString(), *body,
                              NetworkErrorLoggingService::kServerIpKey);
  base::ExpectDictStringValue("http/1.1", *body,
                              NetworkErrorLoggingService::kProtocolKey);
  base::ExpectDictStringValue("GET", *body,
                              NetworkErrorLoggingService::kMethodKey);
  base::ExpectDictIntegerValue(200, *body,
                               NetworkErrorLoggingService::kStatusCodeKey);
  base::ExpectDictIntegerValue(1234, *body,
                               NetworkErrorLoggingService::kElapsedTimeKey);
  base::ExpectDictStringValue(
      NetworkErrorLoggingService::kSignedExchangePhaseValue, *body,
      NetworkErrorLoggingService::kPhaseKey);
  base::ExpectDictStringValue("sxg.failed", *body,
                              NetworkErrorLoggingService::kTypeKey);

  const base::DictionaryValue* sxg_body;
  ASSERT_TRUE(body->FindKey(NetworkErrorLoggingService::kSignedExchangeBodyKey)
                  ->GetAsDictionary(&sxg_body));

  base::ExpectDictStringValue(kUrl_.spec(), *sxg_body,
                              NetworkErrorLoggingService::kOuterUrlKey);
  base::ExpectDictStringValue(kInnerUrl_.spec(), *sxg_body,
                              NetworkErrorLoggingService::kInnerUrlKey);
  base::ExpectStringValue(
      kCertUrl_.spec(),
      sxg_body->FindKey(NetworkErrorLoggingService::kCertUrlKey)->GetList()[0]);
}

TEST_P(NetworkErrorLoggingServiceTest, MismatchingSubdomain_SignedExchange) {
  service()->OnHeader(kOrigin_, kServerIP_, kHeaderIncludeSubdomains_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  service()->QueueSignedExchangeReport(MakeSignedExchangeReportDetails(
      false, "sxg.failed", kUrlSubdomain_, kInnerUrl_, kCertUrl_, kServerIP_));
  EXPECT_TRUE(reports().empty());
}

TEST_P(NetworkErrorLoggingServiceTest, MismatchingIPAddress_SignedExchange) {
  service()->OnHeader(kOrigin_, kServerIP_, kHeader_);

  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  service()->QueueSignedExchangeReport(MakeSignedExchangeReportDetails(
      false, "sxg.failed", kUrl_, kInnerUrl_, kCertUrl_, kOtherServerIP_));
  EXPECT_TRUE(reports().empty());
}

// When the max number of policies is exceeded, first try to remove expired
// policies before evicting the least recently used unexpired policy.
TEST_P(NetworkErrorLoggingServiceTest, EvictAllExpiredPoliciesFirst) {
  base::SimpleTestClock clock;
  service()->SetClockForTesting(&clock);

  // Add 100 policies then make them expired.
  for (size_t i = 0; i < 100; ++i) {
    service()->OnHeader(MakeOrigin(i), kServerIP_, kHeader_);
  }
  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  EXPECT_EQ(100u, PolicyCount());
  clock.Advance(base::TimeDelta::FromSeconds(86401));  // max_age is 86400 sec
  // Expired policies are allowed to linger before hitting the policy limit.
  EXPECT_EQ(100u, PolicyCount());

  // Reach the max policy limit.
  for (size_t i = 100; i < NetworkErrorLoggingService::kMaxPolicies; ++i) {
    service()->OnHeader(MakeOrigin(i), kServerIP_, kHeader_);
  }
  EXPECT_EQ(NetworkErrorLoggingService::kMaxPolicies, PolicyCount());

  // Add one more policy to trigger eviction of only the expired policies.
  service()->OnHeader(kOrigin_, kServerIP_, kHeader_);
  EXPECT_EQ(NetworkErrorLoggingService::kMaxPolicies - 100 + 1, PolicyCount());
}

TEST_P(NetworkErrorLoggingServiceTest, EvictLeastRecentlyUsedPolicy) {
  base::SimpleTestClock clock;
  service()->SetClockForTesting(&clock);

  // A policy's |last_used| is updated when it is added
  for (size_t i = 0; i < NetworkErrorLoggingService::kMaxPolicies; ++i) {
    service()->OnHeader(MakeOrigin(i), kServerIP_, kHeader_);
    clock.Advance(base::TimeDelta::FromSeconds(1));
  }
  // Make the rest of the test run synchronously.
  FinishLoading(true /* load_success */);

  EXPECT_EQ(PolicyCount(), NetworkErrorLoggingService::kMaxPolicies);

  // Set another policy which triggers eviction. None of the policies have
  // expired, so the least recently used (i.e. least recently added) policy
  // should be evicted.
  service()->OnHeader(kOrigin_, kServerIP_, kHeader_);
  clock.Advance(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(PolicyCount(), NetworkErrorLoggingService::kMaxPolicies);

  EXPECT_FALSE(HasPolicyForOrigin(MakeOrigin(0)));  // evicted
  std::set<url::Origin> all_policy_origins =
      service()->GetPolicyOriginsForTesting();
  for (size_t i = 1; i < NetworkErrorLoggingService::kMaxPolicies; ++i) {
    // Avoid n calls to HasPolicyForOrigin(), which would be O(n^2).
    EXPECT_EQ(1u, all_policy_origins.count(MakeOrigin(i)));
  }
  EXPECT_TRUE(HasPolicyForOrigin(kOrigin_));

  // Now use the policies in reverse order starting with kOrigin_, then add
  // another policy to trigger eviction, to check that the stalest policy is
  // identified correctly.
  service()->OnRequest(
      MakeRequestDetails(kOrigin_.GetURL(), ERR_CONNECTION_REFUSED));
  clock.Advance(base::TimeDelta::FromSeconds(1));
  for (size_t i = NetworkErrorLoggingService::kMaxPolicies - 1; i >= 1; --i) {
    service()->OnRequest(
        MakeRequestDetails(MakeOrigin(i).GetURL(), ERR_CONNECTION_REFUSED));
    clock.Advance(base::TimeDelta::FromSeconds(1));
  }
  service()->OnHeader(kOriginSubdomain_, kServerIP_, kHeader_);
  EXPECT_EQ(PolicyCount(), NetworkErrorLoggingService::kMaxPolicies);

  EXPECT_FALSE(HasPolicyForOrigin(kOrigin_));  // evicted
  all_policy_origins = service()->GetPolicyOriginsForTesting();
  for (size_t i = NetworkErrorLoggingService::kMaxPolicies - 1; i >= 1; --i) {
    // Avoid n calls to HasPolicyForOrigin(), which would be O(n^2).
    EXPECT_EQ(1u, all_policy_origins.count(MakeOrigin(i)));
  }
  EXPECT_TRUE(HasPolicyForOrigin(kOriginSubdomain_));  // most recently added

  // Note: This test advances the clock by ~2000 seconds, which is below the
  // specified max_age of 86400 seconds, so none of the policies expire during
  // this test.
}

TEST_P(NetworkErrorLoggingServiceTest, SendsCommandsToStoreSynchronous) {
  if (!store())
    return;

  MockPersistentNelStore::CommandList expected_commands;
  NetworkErrorLoggingService::NelPolicy policy1 = MakePolicyForOrigin(kOrigin_);
  NetworkErrorLoggingService::NelPolicy policy2 =
      MakePolicyForOrigin(kOriginDifferentHost_);
  std::vector<NetworkErrorLoggingService::NelPolicy> prestored_policies = {
      policy1, policy2};
  store()->SetPrestoredPolicies(std::move(prestored_policies));

  // The first call to any of the public methods triggers a load.
  service()->OnHeader(kOrigin_, kServerIP_, kHeader_);
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
      MakeRequestDetails(kOrigin_.GetURL(), ERR_CONNECTION_REFUSED));
  expected_commands.emplace_back(
      MockPersistentNelStore::Command::Type::UPDATE_NEL_POLICY, policy1);
  EXPECT_TRUE(store()->VerifyCommands(expected_commands));

  service()->QueueSignedExchangeReport(MakeSignedExchangeReportDetails(
      false, "sxg.failed", kUrl_, kInnerUrl_, kCertUrl_, kServerIP_));
  expected_commands.emplace_back(
      MockPersistentNelStore::Command::Type::UPDATE_NEL_POLICY, policy1);
  EXPECT_TRUE(store()->VerifyCommands(expected_commands));

  // Removes policy1 but not policy2.
  EXPECT_EQ(2, store()->StoredPoliciesCount());
  service()->RemoveBrowsingData(
      base::BindRepeating([](const GURL& origin) -> bool {
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

// Same as the above test, except that all the tasks are queued until loading
// is complete.
TEST_P(NetworkErrorLoggingServiceTest, SendsCommandsToStoreDeferred) {
  if (!store())
    return;

  MockPersistentNelStore::CommandList expected_commands;
  NetworkErrorLoggingService::NelPolicy policy1 = MakePolicyForOrigin(kOrigin_);
  NetworkErrorLoggingService::NelPolicy policy2 =
      MakePolicyForOrigin(kOriginDifferentHost_);
  std::vector<NetworkErrorLoggingService::NelPolicy> prestored_policies = {
      policy1, policy2};
  store()->SetPrestoredPolicies(std::move(prestored_policies));

  // The first call to any of the public methods triggers a load.
  service()->OnHeader(kOrigin_, kServerIP_, kHeader_);
  expected_commands.emplace_back(
      MockPersistentNelStore::Command::Type::LOAD_NEL_POLICIES);
  EXPECT_TRUE(store()->VerifyCommands(expected_commands));

  service()->OnRequest(
      MakeRequestDetails(kOrigin_.GetURL(), ERR_CONNECTION_REFUSED));
  EXPECT_TRUE(store()->VerifyCommands(expected_commands));

  service()->QueueSignedExchangeReport(MakeSignedExchangeReportDetails(
      false, "sxg.failed", kUrl_, kInnerUrl_, kCertUrl_, kServerIP_));
  EXPECT_TRUE(store()->VerifyCommands(expected_commands));

  // Removes policy1 but not policy2.
  service()->RemoveBrowsingData(
      base::BindRepeating([](const GURL& origin) -> bool {
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
  NetworkErrorLoggingService::NelPolicy policy1 = MakePolicyForOrigin(kOrigin_);
  NetworkErrorLoggingService::NelPolicy policy2 =
      MakePolicyForOrigin(kOriginDifferentHost_);
  std::vector<NetworkErrorLoggingService::NelPolicy> prestored_policies = {
      policy1, policy2};
  store()->SetPrestoredPolicies(std::move(prestored_policies));

  // The first call to any of the public methods triggers a load.
  service()->OnHeader(kOrigin_, kServerIP_, kHeader_);
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
  LOG(INFO) << store()->GetDebugString();

  service()->OnRequest(
      MakeRequestDetails(kOrigin_.GetURL(), ERR_CONNECTION_REFUSED));
  expected_commands.emplace_back(
      MockPersistentNelStore::Command::Type::UPDATE_NEL_POLICY, policy1);
  EXPECT_TRUE(store()->VerifyCommands(expected_commands));

  service()->QueueSignedExchangeReport(MakeSignedExchangeReportDetails(
      false, "sxg.failed", kUrl_, kInnerUrl_, kCertUrl_, kServerIP_));
  expected_commands.emplace_back(
      MockPersistentNelStore::Command::Type::UPDATE_NEL_POLICY, policy1);
  EXPECT_TRUE(store()->VerifyCommands(expected_commands));

  // Removes policy1 but not policy2.
  service()->RemoveBrowsingData(
      base::BindRepeating([](const GURL& origin) -> bool {
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
  NetworkErrorLoggingService::NelPolicy policy1 = MakePolicyForOrigin(kOrigin_);
  NetworkErrorLoggingService::NelPolicy policy2 =
      MakePolicyForOrigin(kOriginDifferentHost_);
  std::vector<NetworkErrorLoggingService::NelPolicy> prestored_policies = {
      policy1, policy2};
  store()->SetPrestoredPolicies(std::move(prestored_policies));

  // The first call to any of the public methods triggers a load.
  service()->OnHeader(kOrigin_, kServerIP_, kHeader_);
  expected_commands.emplace_back(
      MockPersistentNelStore::Command::Type::LOAD_NEL_POLICIES);
  EXPECT_TRUE(store()->VerifyCommands(expected_commands));

  service()->OnRequest(
      MakeRequestDetails(kOrigin_.GetURL(), ERR_CONNECTION_REFUSED));
  EXPECT_TRUE(store()->VerifyCommands(expected_commands));

  service()->QueueSignedExchangeReport(MakeSignedExchangeReportDetails(
      false, "sxg.failed", kUrl_, kInnerUrl_, kCertUrl_, kServerIP_));
  EXPECT_TRUE(store()->VerifyCommands(expected_commands));

  // Removes policy1 but not policy2.
  service()->RemoveBrowsingData(
      base::BindRepeating([](const GURL& origin) -> bool {
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

  service->OnHeader(kOrigin_, kServerIP_, kHeader_);
  expected_commands.emplace_back(
      MockPersistentNelStore::Command::Type::LOAD_NEL_POLICIES);
  EXPECT_TRUE(store->VerifyCommands(expected_commands));

  store->FinishLoading(false /* load_success */);
  expected_commands.emplace_back(
      MockPersistentNelStore::Command::Type::ADD_NEL_POLICY,
      MakePolicyForOrigin(kOrigin_));
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
  service()->OnHeader(kOrigin_, kServerIP_, kHeader_);
  expected_commands.emplace_back(
      MockPersistentNelStore::Command::Type::LOAD_NEL_POLICIES);
  EXPECT_TRUE(store()->VerifyCommands(expected_commands));

  service()->OnRequest(
      MakeRequestDetails(kOrigin_.GetURL(), ERR_CONNECTION_REFUSED));
  service()->QueueSignedExchangeReport(MakeSignedExchangeReportDetails(
      false, "sxg.failed", kUrl_, kInnerUrl_, kCertUrl_, kServerIP_));
  service()->RemoveBrowsingData(
      base::BindRepeating([](const GURL& origin) -> bool {
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
