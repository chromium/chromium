// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/digital_credentials/digital_identity_request_impl.h"

#include <optional>

#include "base/json/json_reader.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "content/browser/webid/test/stub_digital_identity_provider.h"
#include "content/public/common/content_features.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

constexpr char kOpenid4vpProtocol[] = "openid4vp";
constexpr char kPreviewProtocol[] = "preview";

using InterstitialType = content::DigitalIdentityInterstitialType;

// StubDigitalIdentityProvider which enables overriding
// DigitalIdentityProvider::IsLowRiskOrigin().
class TestDigitalIdentityProviderWithCustomRisk
    : public StubDigitalIdentityProvider {
 public:
  explicit TestDigitalIdentityProviderWithCustomRisk(bool are_origins_low_risk)
      : are_origins_low_risk_(are_origins_low_risk) {}
  ~TestDigitalIdentityProviderWithCustomRisk() override = default;

  bool IsLowRiskOrigin(const url::Origin& to_check) const override {
    return are_origins_low_risk_;
  }

 private:
  bool are_origins_low_risk_;
};

base::Value ParseJsonAndCheck(const std::string& json) {
  std::optional<base::Value> parsed = base::JSONReader::Read(json);
  return parsed.has_value() ? std::move(*parsed) : base::Value();
}

base::Value GenerateOnlyAgeOpenid4VpRequest() {
  constexpr char kJson[] = R"({
    "client_id": "digital-credentials.dev",
    "client_id_scheme": "web-origin",
    "response_type": "vp_token",
    "nonce": "Q7pqzU98nsE9O9t--NpdmmeZSQOkmYKXsWCiJi39UGs=",
    "presentation_definition": {
      "id": "mDL-request-demo",
      "input_descriptors": [
        {
          "id": "org.iso.18013.5.1.mDL",
          "format": {
            "mso_mdoc": {
              "alg": [
                "ES256"
              ]
            }
          },
          "constraints": {
            "limit_disclosure": "required",
            "fields": [
              {
                "path": [
                  "$['org.iso.18013.5.1']['age_over_78']"
                ],
                "intent_to_retain": false
              }
            ]
          }
        }
      ]
    }
  })";

  return ParseJsonAndCheck(kJson);
}

base::Value GenerateOnlyAgePreviewRequest() {
  constexpr char kJson[] = R"({
    "selector": {
      "format": [
        "mdoc"
      ],
      "doctype": "org.iso.18013.5.1.mDL",
      "fields": [
        {
          "namespace": "org.iso.18013.5.1",
          "name": "age_over_21",
          "intentToRetain": false
        }
      ]
    },
    "nonce": "vvm3Q1VN1tXybccprmZhbZFIjBGSB4VNMuqQfD4Uiko=",
    "readerPublicKey": "BMK9ink7wCHIKXxxWQy-S6TLN4jo1ab7NBlC-lSvqqMUmgMSadLa9PYYDocWitOmafZqWmZc5lQvdCZQx5mTNvs="
  })";

  return ParseJsonAndCheck(kJson);
}

// Does depth-first traversal of nested dicts rooted at `root`. Returns first
// matching base::Value with key `find_key`.
base::Value* FindValueWithKey(base::Value& root, const std::string& find_key) {
  if (root.is_list()) {
    base::Value::List& list = root.GetList();
    for (base::Value& list_item : list) {
      if (base::Value* out = FindValueWithKey(list_item, find_key)) {
        return out;
      }
    }
    return nullptr;
  }

  if (root.is_dict()) {
    base::Value::Dict& dict = root.GetDict();
    for (auto it : dict) {
      if (it.first == find_key) {
        return &it.second;
      }
      if (base::Value* out = FindValueWithKey(it.second, find_key)) {
        return out;
      }
    }
  }

  return nullptr;
}

bool HasNoListElements(const base::Value* value) {
  return !value || !value->is_list() || value->GetList().size() == 0u;
}

bool IsNonEmptyList(const base::Value* value) {
  return !HasNoListElements(value);
}

// Removes `find_key` if present from `dict`. Ignores nested base::Value::Dicts.
void RemoveDictKey(base::Value::Dict& dict, const std::string& find_key) {
  for (auto it = dict.begin(); it != dict.end(); ++it) {
    if (it->first == find_key) {
      dict.erase(it);
      break;
    }
  }
}

// Used to modify an Openid4VpRequest on the fly.
bool SetPathItem(base::Value& to_modify, const std::string& path_item) {
  base::Value* paths = FindValueWithKey(to_modify, "path");
  if (HasNoListElements(paths)) {
    return false;
  }
  paths->GetList().resize(0);
  paths->GetList().Append(path_item);
  return true;
}

// Used to modify a Preview on the fly.
bool SetFieldNameValue(base::Value& to_modify,
                       const std::string& field_name_value) {
  base::Value* fields = FindValueWithKey(to_modify, "fields");
  if (HasNoListElements(fields)) {
    return false;
  }
  fields->GetList().front().GetDict().Set("name", field_name_value);
  return true;
}

std::optional<InterstitialType> ComputeInterstitialType(
    const std::string& protocol,
    base::Value request) {
  auto provider = std::make_unique<TestDigitalIdentityProviderWithCustomRisk>(
      /*are_origins_low_risk=*/false);
  return DigitalIdentityRequestImpl::ComputeInterstitialType(
      url::Origin(), provider.get(), protocol, std::move(request));
}

}  // anonymous namespace

class DigitalIdentityRequestImplTest : public testing::Test {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kWebIdentityDigitalCredentials, {{"dialog", ""}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(DigitalIdentityRequestImplTest,
       Openid4VpProtocol_ComputeInterstitialType_OnlyAgeOver) {
  EXPECT_EQ(std::nullopt,
            ComputeInterstitialType(kOpenid4vpProtocol,
                                    GenerateOnlyAgeOpenid4VpRequest()));
}

TEST_F(DigitalIdentityRequestImplTest,
       Openid4VpProtocol_ComputeInterstitialType_OnlyAgeInYears) {
  base::Value request = GenerateOnlyAgeOpenid4VpRequest();
  ASSERT_TRUE(SetPathItem(request, "$['org.iso.18013.5.1']['age_in_years']"));
  EXPECT_EQ(std::nullopt,
            ComputeInterstitialType(kOpenid4vpProtocol, std::move(request)));
}

TEST_F(DigitalIdentityRequestImplTest,
       Openid4VpProtocol_ComputeIntersitialType_OnlyAgeBirthYear) {
  base::Value request = GenerateOnlyAgeOpenid4VpRequest();
  ASSERT_TRUE(SetPathItem(request, "$['org.iso.18013.5.1']['age_birth_year']"));
  EXPECT_EQ(std::nullopt,
            ComputeInterstitialType(kOpenid4vpProtocol, std::move(request)));
}

TEST_F(DigitalIdentityRequestImplTest,
       Openid4VpProtocol_ComputeIntersitialType_OnlyBirthDate) {
  base::Value request = GenerateOnlyAgeOpenid4VpRequest();
  ASSERT_TRUE(SetPathItem(request, "$['org.iso.18013.5.1']['birth_date']"));
  EXPECT_EQ(std::nullopt,
            ComputeInterstitialType(kOpenid4vpProtocol, std::move(request)));
}

base::Value GenerateNonAgeOpenid4VpRequest() {
  base::Value request = GenerateOnlyAgeOpenid4VpRequest();
  CHECK(SetPathItem(request, "$['org.iso.18013.5.1']['given_name']"));
  return request;
}

TEST_F(DigitalIdentityRequestImplTest,
       Openid4VpProtocol_ComputeIntersitialType_OnlyNonAgeDataElement) {
  EXPECT_EQ(InterstitialType::kLowRisk,
            ComputeInterstitialType(kOpenid4vpProtocol,
                                    GenerateNonAgeOpenid4VpRequest()));
}

TEST_F(DigitalIdentityRequestImplTest,
       Openid4VpProtocol_LowRiskOriginTakesPrecedenceOverRequestType) {
  auto provider = std::make_unique<TestDigitalIdentityProviderWithCustomRisk>(
      /*are_origins_low_risk=*/true);
  EXPECT_EQ(std::nullopt, DigitalIdentityRequestImpl::ComputeInterstitialType(
                              url::Origin(), provider.get(), kOpenid4vpProtocol,
                              GenerateNonAgeOpenid4VpRequest()));
}

TEST_F(DigitalIdentityRequestImplTest,
       Openid4VpProtocol_ComputeInterstitialType_EmptyPathList) {
  base::Value request = GenerateOnlyAgeOpenid4VpRequest();
  base::Value* paths = FindValueWithKey(request, "path");
  ASSERT_TRUE(IsNonEmptyList(paths));
  paths->GetList().resize(0);

  EXPECT_EQ(InterstitialType::kLowRisk,
            ComputeInterstitialType(kOpenid4vpProtocol, std::move(request)));
}

TEST_F(DigitalIdentityRequestImplTest,
       Openid4VpProtocol_ComputeInterstitialType_RequestMultiplePaths) {
  base::Value request = GenerateOnlyAgeOpenid4VpRequest();
  base::Value* paths = FindValueWithKey(request, "path");
  ASSERT_TRUE(IsNonEmptyList(paths));

  base::Value::List& path_list = paths->GetList();
  path_list.Append(path_list.front().Clone());

  EXPECT_EQ(InterstitialType::kLowRisk,
            ComputeInterstitialType(kOpenid4vpProtocol, std::move(request)));
}

TEST_F(DigitalIdentityRequestImplTest,
       Openid4VpProtocol_ComputeInterstitialType_NoPath) {
  base::Value request = GenerateOnlyAgeOpenid4VpRequest();
  base::Value* fields = FindValueWithKey(request, "fields");
  ASSERT_TRUE(IsNonEmptyList(fields));
  RemoveDictKey(fields->GetList().front().GetDict(), "path");

  EXPECT_EQ(InterstitialType::kLowRisk,
            ComputeInterstitialType(kOpenid4vpProtocol, std::move(request)));
}

TEST_F(DigitalIdentityRequestImplTest,
       Openid4VpProtocol_ComputeInterstitialType_EmptyFieldsList) {
  base::Value request = GenerateOnlyAgeOpenid4VpRequest();
  base::Value* fields = FindValueWithKey(request, "fields");
  ASSERT_TRUE(IsNonEmptyList(fields));
  fields->GetList().resize(0);

  EXPECT_EQ(InterstitialType::kLowRisk,
            ComputeInterstitialType(kOpenid4vpProtocol, std::move(request)));
}

TEST_F(DigitalIdentityRequestImplTest,
       Openid4VpProtocol_ComputeInterstitialType_RequestMultipleAgeAssertions) {
  base::Value request = GenerateOnlyAgeOpenid4VpRequest();
  base::Value* fields = FindValueWithKey(request, "fields");
  ASSERT_TRUE(IsNonEmptyList(fields));

  base::Value new_field = ParseJsonAndCheck(R"({
    "path": [
      "$['org.iso.18013.5.1']['age_over_60']"
    ]
  })");
  fields->GetList().Append(std::move(new_field));

  EXPECT_EQ(InterstitialType::kLowRisk,
            ComputeInterstitialType(kOpenid4vpProtocol, std::move(request)));
}

TEST_F(DigitalIdentityRequestImplTest,
       Openid4VpProtocol_ComputeInterstitialType_RequestMultipleFields) {
  base::Value request = GenerateOnlyAgeOpenid4VpRequest();
  base::Value* fields = FindValueWithKey(request, "fields");
  ASSERT_TRUE(IsNonEmptyList(fields));

  base::Value new_field = ParseJsonAndCheck(R"({
    "path": [
      "$['org.iso.18013.5.1']['given_name']"
    ]
  })");
  fields->GetList().Append(std::move(new_field));

  EXPECT_EQ(InterstitialType::kLowRisk,
            ComputeInterstitialType(kOpenid4vpProtocol, std::move(request)));
}

TEST_F(DigitalIdentityRequestImplTest,
       Openid4VpProtocol_ComputeInterstitialType_NoConstraints) {
  base::Value request = GenerateOnlyAgeOpenid4VpRequest();
  base::Value* input_descriptors =
      FindValueWithKey(request, "input_descriptors");
  ASSERT_TRUE(IsNonEmptyList(input_descriptors));
  RemoveDictKey(input_descriptors->GetList().front().GetDict(), "constraints");

  EXPECT_EQ(InterstitialType::kLowRisk,
            ComputeInterstitialType(kOpenid4vpProtocol, std::move(request)));
}

TEST_F(DigitalIdentityRequestImplTest,
       Openid4VpProtocol_ComputeInterstitialType_EmptyInputDescriptorList) {
  base::Value request = GenerateOnlyAgeOpenid4VpRequest();
  base::Value* input_descriptors =
      FindValueWithKey(request, "input_descriptors");
  ASSERT_TRUE(IsNonEmptyList(input_descriptors));
  input_descriptors->GetList().resize(0);

  EXPECT_EQ(InterstitialType::kLowRisk,
            ComputeInterstitialType(kOpenid4vpProtocol, std::move(request)));
}

TEST_F(DigitalIdentityRequestImplTest,
       Openid4VpProtocol_ComputeInterstitialType_RequestMultipleDocuments) {
  base::Value request = GenerateOnlyAgeOpenid4VpRequest();
  base::Value* input_descriptors =
      FindValueWithKey(request, "input_descriptors");
  ASSERT_TRUE(IsNonEmptyList(input_descriptors));

  base::Value::List& input_descriptor_list = input_descriptors->GetList();
  input_descriptor_list.Append(input_descriptor_list.front().Clone());

  EXPECT_EQ(InterstitialType::kLowRisk,
            ComputeInterstitialType(kOpenid4vpProtocol, std::move(request)));
}

TEST_F(DigitalIdentityRequestImplTest,
       Openid4VpProtocol_ComputeInterstitialType_NonMdlInputDescriptorId) {
  base::Value request = GenerateOnlyAgeOpenid4VpRequest();
  base::Value* input_descriptors =
      FindValueWithKey(request, "input_descriptors");
  ASSERT_TRUE(IsNonEmptyList(input_descriptors));

  base::Value::List& input_descriptor_list = input_descriptors->GetList();
  ASSERT_TRUE(input_descriptor_list.front().is_dict());
  input_descriptor_list.front().GetDict().Set("id", "not_mdl");

  EXPECT_EQ(InterstitialType::kLowRisk,
            ComputeInterstitialType(kOpenid4vpProtocol, std::move(request)));
}

TEST_F(DigitalIdentityRequestImplTest,
       Openid4VpProtocol_ComputeInterstitialType_NoPresentationDefinition) {
  base::Value request = GenerateOnlyAgeOpenid4VpRequest();
  RemoveDictKey(request.GetDict(), "presentation_definition");

  EXPECT_EQ(InterstitialType::kLowRisk,
            ComputeInterstitialType(kOpenid4vpProtocol, std::move(request)));
}

TEST_F(DigitalIdentityRequestImplTest,
       PreviewProtocol_ComputeInterstitialType_OnlyAgeOver) {
  EXPECT_EQ(std::nullopt,
            ComputeInterstitialType(kPreviewProtocol,
                                    GenerateOnlyAgePreviewRequest()));
}

TEST_F(DigitalIdentityRequestImplTest,
       PreviewProtocol_ComputeInterstitialType_OnlyAgeInYears) {
  base::Value request = GenerateOnlyAgePreviewRequest();
  ASSERT_TRUE(SetFieldNameValue(request, "age_in_years"));
  EXPECT_EQ(std::nullopt,
            ComputeInterstitialType(kPreviewProtocol, std::move(request)));
}

TEST_F(DigitalIdentityRequestImplTest,
       PreviewProtocol_ComputeIntersitialType_OnlyAgeBirthYear) {
  base::Value request = GenerateOnlyAgePreviewRequest();
  ASSERT_TRUE(SetFieldNameValue(request, "age_birth_year"));
  EXPECT_EQ(std::nullopt,
            ComputeInterstitialType(kPreviewProtocol, std::move(request)));
}

TEST_F(DigitalIdentityRequestImplTest,
       PreviewProtocol_ComputeIntersitialType_OnlyBirthDate) {
  base::Value request = GenerateOnlyAgePreviewRequest();
  ASSERT_TRUE(SetFieldNameValue(request, "birth_date"));
  EXPECT_EQ(std::nullopt,
            ComputeInterstitialType(kPreviewProtocol, std::move(request)));
}

TEST_F(DigitalIdentityRequestImplTest,
       PreviewProtocol_ComputeIntersitialType_GivenName) {
  base::Value request = GenerateOnlyAgePreviewRequest();
  ASSERT_TRUE(SetFieldNameValue(request, "given_name"));
  EXPECT_EQ(InterstitialType::kLowRisk,
            ComputeInterstitialType(kPreviewProtocol, std::move(request)));
}

}  // namespace content
