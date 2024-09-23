// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/trust_token_request_signing_helper.h"

#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "net/base/request_priority.h"
#include "net/http/structured_headers.h"
#include "net/log/net_log.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/public/cpp/trust_token_parameterization.h"
#include "services/network/public/mojom/trust_tokens.mojom-shared.h"
#include "services/network/test/trust_token_test_util.h"
#include "services/network/trust_tokens/proto/public.pb.h"
#include "services/network/trust_tokens/trust_token_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using ::testing::IsEmpty;
using ::testing::Matches;
using ::testing::Not;
using ::testing::StrEq;
using ::testing::UnorderedElementsAre;

namespace network {

namespace {

using TrustTokenRequestSigningHelperTest = TrustTokenRequestHelperTest;

MATCHER_P(Header, name, base::StringPrintf("The header %s is present", name)) {
  return arg.extra_request_headers().HasHeader(name);
}
MATCHER_P2(Header,
           name,
           other_matcher,
           "Evaluate the given matcher on the given header, if "
           "present.") {
  std::optional<std::string> header =
      arg.extra_request_headers().GetHeader(name);
  if (!header) {
    return false;
  }
  return Matches(other_matcher)(header);
}

SuitableTrustTokenOrigin CreateSuitableOriginOrDie(std::string_view spec) {
  std::optional<SuitableTrustTokenOrigin> maybe_origin =
      SuitableTrustTokenOrigin::Create(GURL(spec));
  CHECK(maybe_origin) << "Failed to create a SuitableTrustTokenOrigin!";
  return *maybe_origin;
}

bool ExtractRedemptionRecordsFromHeader(
    std::string_view sec_redemption_record_header,
    std::map<SuitableTrustTokenOrigin, std::string>*
        redemption_records_per_issuer_out,
    std::string* error_out) {
  std::optional<net::structured_headers::List> maybe_list =
      net::structured_headers::ParseList(sec_redemption_record_header);

  std::string dummy;
  if (!error_out)
    error_out = &dummy;

  if (!maybe_list) {
    *error_out = "Header wasn't a valid Structured Headers list";
    return false;
  }

  for (auto& issuer_and_params : *maybe_list) {
    net::structured_headers::Item& issuer_item =
        issuer_and_params.member.front().item;
    if (!issuer_item.is_string()) {
      *error_out = "Non-string item in the RR header's list";
      return false;
    }

    const net::structured_headers::Parameters& params_for_issuer =
        issuer_and_params.params;
    if (params_for_issuer.size() != 1) {
      *error_out =
          base::StrCat({"Unexpected number of parameters for RR header list "
                        "item; expected 1 parameter but there were ",
                        base::NumberToString(params_for_issuer.size())});
      return false;
    }
    if (params_for_issuer.front().first != "redemption-record") {
      *error_out = base::ReplaceStringPlaceholders(
          "Unexpected parameter key $1 for RR header list item",
          {params_for_issuer.front().first}, /*offsets=*/nullptr);
      return false;
    }

    const net::structured_headers::Item& redemption_record_item =
        params_for_issuer.front().second;
    if (!redemption_record_item.is_string()) {
      *error_out = "Unexpected parameter value type for RR header list item";
      return false;
    }

    std::optional<SuitableTrustTokenOrigin> maybe_issuer =
        SuitableTrustTokenOrigin::Create(GURL(issuer_item.GetString()));
    if (!maybe_issuer) {
      *error_out = "Unsuitable Trust Tokens issuer origin in RR header item";
      return false;
    }

    // GetString also gets a byte sequence.
    redemption_records_per_issuer_out->emplace(
        std::move(*maybe_issuer), redemption_record_item.GetString());
  }
  return true;
}

}  // namespace

TEST_F(TrustTokenRequestSigningHelperTest, ProvidesMajorVersionHeader) {
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();

  TrustTokenRequestSigningHelper::Params params(
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com")),
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com")));

  TrustTokenRedemptionRecord my_record;
  my_record.set_body("look at me, I'm an RR body");
  store->SetRedemptionRecord(params.issuers.front(), params.toplevel,
                             my_record);

  TrustTokenRequestSigningHelper helper(store.get(), std::move(params));

  auto my_request = MakeURLRequest("https://destination.com/");
  my_request->set_initiator(url::Origin::Create(GURL("https://issuer.com/")));
  mojom::TrustTokenOperationStatus result =
      ExecuteBeginOperationAndWaitForResult(&helper, my_request.get());

  EXPECT_EQ(result, mojom::TrustTokenOperationStatus::kOk);
  // This test's expectation should change whenever the supported Trust Tokens
  // major version changes.
  EXPECT_THAT(*my_request, Header("Sec-Private-State-Token-Crypto-Version",
                                  "PrivateStateTokenV3"));
}

// Test RR attachment:
// - The two issuers with stored redemption records should appear in the header.
// - A third issuer without a corresponding redemption record in storage
// shouldn't appear in the header.
TEST_F(TrustTokenRequestSigningHelperTest,
       RedemptionRecordAttachmentWithoutSigning) {
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();

  TrustTokenRequestSigningHelper::Params params(
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com")),
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com")));
  params.issuers.push_back(
      *SuitableTrustTokenOrigin::Create(GURL("https://second-issuer.example")));

  TrustTokenRedemptionRecord first_issuer_record;
  first_issuer_record.set_body("look at me! I'm a redemption record");
  store->SetRedemptionRecord(params.issuers.front(), params.toplevel,
                             first_issuer_record);

  TrustTokenRedemptionRecord second_issuer_record;
  second_issuer_record.set_body(
      "I'm another redemption record, distinct from the first");
  store->SetRedemptionRecord(params.issuers.back(), params.toplevel,
                             second_issuer_record);

  // Attempting to sign with an issuer with no redemption record in storage
  // should be fine, resulting in the issuer getting ignored.
  params.issuers.push_back(
      *SuitableTrustTokenOrigin::Create(GURL("https://third-issuer.example")));

  TrustTokenRequestSigningHelper helper(store.get(), std::move(params));

  auto my_request = MakeURLRequest("https://destination.com/");
  my_request->set_initiator(url::Origin::Create(GURL("https://issuer.com/")));
  mojom::TrustTokenOperationStatus result =
      ExecuteBeginOperationAndWaitForResult(&helper, my_request.get());

  ASSERT_EQ(result, mojom::TrustTokenOperationStatus::kOk);

  std::string redemption_record_header = my_request->extra_request_headers()
                                             .GetHeader("Sec-Redemption-Record")
                                             .value();
  std::map<SuitableTrustTokenOrigin, std::string> redemption_records_per_issuer;
  std::string error;
  ASSERT_TRUE(ExtractRedemptionRecordsFromHeader(
      redemption_record_header, &redemption_records_per_issuer, &error))
      << error;

  EXPECT_THAT(
      redemption_records_per_issuer,
      UnorderedElementsAre(
          Pair(CreateSuitableOriginOrDie("https://issuer.com"),
               StrEq(first_issuer_record.body())),
          Pair(CreateSuitableOriginOrDie("https://second-issuer.example"),
               StrEq(second_issuer_record.body()))));
}

}  // namespace network
