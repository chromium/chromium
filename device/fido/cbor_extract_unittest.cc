// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "components/cbor/values.h"
#include "device/fido/cbor_extract.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {
namespace {

using cbor_extract::IntKey;
using cbor_extract::Is;
using cbor_extract::Map;
using cbor_extract::Stop;
using cbor_extract::StringKey;

template <typename T>
bool VectorSpanEqual(const std::vector<T>& v, base::span<const T> s) {
  if (v.size() != s.size()) {
    return false;
  }
  return std::equal(v.begin(), v.end(), s.begin());
}

struct MakeCredRequest {
  const std::vector<uint8_t>* client_data_hash;
  const std::string* rp_id;
  const std::vector<uint8_t>* user_id;
  const std::vector<cbor::Value>* cred_params;
  const std::vector<cbor::Value>* excluded_credentials;
  const bool* resident_key;
  const bool* user_verification;
  const bool* large_test;
  const bool* negative_test;
};

TEST(CBORExtract, Basic) {
  cbor::Value::MapValue rp;
  rp.emplace("id", "example.com");
  rp.emplace("name", "Example");

  static const uint8_t kUserId[] = {1, 2, 3, 4};
  cbor::Value::MapValue user;
  user.emplace("id", base::span<const uint8_t>(kUserId));
  user.emplace("name", "Joe");

  std::vector<cbor::Value> cred_params;
  static const int64_t kAlgs[] = {-7, -257};
  for (const int64_t alg : kAlgs) {
    cbor::Value::MapValue cred_param;
    cred_param.emplace("type", "public-key");
    cred_param.emplace("alg", alg);
    cred_params.emplace_back(std::move(cred_param));
  }

  std::vector<cbor::Value> excluded_creds;
  for (int i = 0; i < 3; i++) {
    cbor::Value::MapValue excluded_cred;
    uint8_t id[1] = {static_cast<uint8_t>(i)};
    excluded_cred.emplace("type", "public-key");
    excluded_cred.emplace("id", base::span<const uint8_t>(id));
    excluded_creds.emplace_back(std::move(excluded_cred));
  }

  cbor::Value::MapValue options;
  options.emplace("rk", true);

  static const uint8_t kClientDataHash[32] = {4, 3, 2, 1, 0};
  cbor::Value::MapValue make_cred;
  make_cred.emplace(1, base::span<const uint8_t>(kClientDataHash));
  make_cred.emplace(2, std::move(rp));
  make_cred.emplace(3, std::move(user));
  make_cred.emplace(4, std::move(cred_params));
  make_cred.emplace(5, std::move(excluded_creds));
  make_cred.emplace(7, std::move(options));
  make_cred.emplace(100, false);
  make_cred.emplace(-3, true);

  static constexpr cbor_extract::StepOrByte<MakeCredRequest>
      kMakeCredParseSteps[] = {
          // clang-format off
      ELEMENT(Is::kRequired, MakeCredRequest, client_data_hash),
      IntKey<MakeCredRequest>(1),

      Map<MakeCredRequest>(),
      IntKey<MakeCredRequest>(2),
        ELEMENT(Is::kRequired, MakeCredRequest, rp_id),
        StringKey<MakeCredRequest>(), 'i', 'd', '\0',
      Stop<MakeCredRequest>(),

      Map<MakeCredRequest>(),
      IntKey<MakeCredRequest>(3),
        ELEMENT(Is::kRequired, MakeCredRequest, user_id),
        StringKey<MakeCredRequest>(), 'i', 'd', '\0',
      Stop<MakeCredRequest>(),

      ELEMENT(Is::kRequired, MakeCredRequest, cred_params),
      IntKey<MakeCredRequest>(4),
      ELEMENT(Is::kRequired, MakeCredRequest, excluded_credentials),
      IntKey<MakeCredRequest>(5),

      Map<MakeCredRequest>(),
      IntKey<MakeCredRequest>(7),
        ELEMENT(Is::kOptional, MakeCredRequest, resident_key),
        StringKey<MakeCredRequest>(), 'r', 'k', '\0',
        ELEMENT(Is::kOptional, MakeCredRequest, user_verification),
        StringKey<MakeCredRequest>(), 'u', 'v', '\0',
      Stop<MakeCredRequest>(),

      ELEMENT(Is::kRequired, MakeCredRequest, large_test),
      IntKey<MakeCredRequest>(100),

      ELEMENT(Is::kRequired, MakeCredRequest, negative_test),
      IntKey<MakeCredRequest>(-3),

      Stop<MakeCredRequest>(),
          // clang-format on
      };

  MakeCredRequest make_cred_request;
  ASSERT_TRUE(cbor_extract::Extract<MakeCredRequest>(
      &make_cred_request, kMakeCredParseSteps, make_cred));
  EXPECT_TRUE(VectorSpanEqual<uint8_t>(*make_cred_request.client_data_hash,
                                       kClientDataHash));
  EXPECT_EQ(*make_cred_request.rp_id, "example.com");
  EXPECT_TRUE(VectorSpanEqual<uint8_t>(*make_cred_request.user_id, kUserId));
  EXPECT_EQ(make_cred_request.cred_params->size(), 2u);
  EXPECT_EQ(make_cred_request.excluded_credentials->size(), 3u);
  EXPECT_TRUE(*make_cred_request.resident_key);
  EXPECT_TRUE(make_cred_request.user_verification == nullptr);
  EXPECT_FALSE(*make_cred_request.large_test);
  EXPECT_TRUE(*make_cred_request.negative_test);

  std::vector<int64_t> algs;
  EXPECT_TRUE(cbor_extract::ForEachPublicKeyEntry(
      *make_cred_request.cred_params, cbor::Value("alg"),
      base::BindRepeating(
          [](std::vector<int64_t>* out, const cbor::Value& value) -> bool {
            if (!value.is_integer()) {
              return false;
            }
            out->push_back(value.GetInteger());
            return true;
          },
          base::Unretained(&algs))));

  EXPECT_TRUE(VectorSpanEqual<int64_t>(algs, kAlgs));
}

TEST(CBORExtract, MissingRequired) {
  struct Dummy {
    const int64_t* value;
  };

  static constexpr cbor_extract::StepOrByte<Dummy> kSteps[] = {
      ELEMENT(Is::kRequired, Dummy, value),
      IntKey<Dummy>(1),
      Stop<Dummy>(),
  };

  cbor::Value::MapValue map;
  Dummy dummy;
  EXPECT_FALSE(cbor_extract::Extract<Dummy>(&dummy, kSteps, map));
}

TEST(CBORExtract, WrongType) {
  struct Dummy {
    const int64_t* value;
  };

  static constexpr cbor_extract::StepOrByte<Dummy> kSteps[] = {
      ELEMENT(Is::kRequired, Dummy, value),
      IntKey<Dummy>(1),
      Stop<Dummy>(),
  };

  cbor::Value::MapValue map;
  map.emplace(1, "string");

  Dummy dummy;
  EXPECT_FALSE(cbor_extract::Extract<Dummy>(&dummy, kSteps, map));
}

}  // namespace
}  // namespace device
