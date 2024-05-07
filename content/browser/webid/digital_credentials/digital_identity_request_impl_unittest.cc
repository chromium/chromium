// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/digital_credentials/digital_identity_request_impl.h"

#include <optional>

#include "base/json/json_reader.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

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

bool IsNonEmptyList(const base::Value* value) {
  return value && value->is_list() && value->GetList().size() > 0u;
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

}  // anonymous namespace

TEST(DigitalIdentityRequestImpl, IsOnlyRequestingAge_OnlyAge) {
  EXPECT_TRUE(DigitalIdentityRequestImpl::IsOnlyRequestingAge(
      GenerateOnlyAgeOpenid4VpRequest()));
}

TEST(DigitalIdentityRequestImpl, IsOnlyRequestinAge_EmptyPathList) {
  base::Value request = GenerateOnlyAgeOpenid4VpRequest();
  base::Value* paths = FindValueWithKey(request, "path");
  ASSERT_TRUE(IsNonEmptyList(paths));
  paths->GetList().resize(0);

  EXPECT_FALSE(DigitalIdentityRequestImpl::IsOnlyRequestingAge(request));
}

TEST(DigitalIdentityRequestImpl, IsOnlyRequestingAge_RequestMultiplePaths) {
  base::Value request = GenerateOnlyAgeOpenid4VpRequest();
  base::Value* paths = FindValueWithKey(request, "path");
  ASSERT_TRUE(IsNonEmptyList(paths));

  base::Value::List& path_list = paths->GetList();
  path_list.Append(path_list.front().Clone());

  EXPECT_FALSE(DigitalIdentityRequestImpl::IsOnlyRequestingAge(request));
}

TEST(DigitalIdentityRequestImpl, IsOnlyRequestingAge_NoPath) {
  base::Value request = GenerateOnlyAgeOpenid4VpRequest();
  base::Value* fields = FindValueWithKey(request, "fields");
  ASSERT_TRUE(IsNonEmptyList(fields));
  RemoveDictKey(fields->GetList().front().GetDict(), "path");

  EXPECT_FALSE(DigitalIdentityRequestImpl::IsOnlyRequestingAge(request));
}

TEST(DigitalIdentityRequestImpl, IsOnlyRequestingAge_EmptyFieldsList) {
  base::Value request = GenerateOnlyAgeOpenid4VpRequest();
  base::Value* fields = FindValueWithKey(request, "fields");
  ASSERT_TRUE(IsNonEmptyList(fields));
  fields->GetList().resize(0);

  EXPECT_FALSE(DigitalIdentityRequestImpl::IsOnlyRequestingAge(request));
}

TEST(DigitalIdentityRequestImpl,
     IsOnlyRequestingAge_RequestMultipleAgeAssertions) {
  base::Value request = GenerateOnlyAgeOpenid4VpRequest();
  base::Value* fields = FindValueWithKey(request, "fields");
  ASSERT_TRUE(IsNonEmptyList(fields));

  base::Value new_field = ParseJsonAndCheck(R"({
    "path": [
      "$['org.iso.18013.5.1']['age_over_60']"
    ]
  })");
  fields->GetList().Append(std::move(new_field));

  EXPECT_FALSE(DigitalIdentityRequestImpl::IsOnlyRequestingAge(request));
}

TEST(DigitalIdentityRequestImpl, IsOnlyRequestingAge_RequestMultipleFields) {
  base::Value request = GenerateOnlyAgeOpenid4VpRequest();
  base::Value* fields = FindValueWithKey(request, "fields");
  ASSERT_TRUE(IsNonEmptyList(fields));

  base::Value new_field = ParseJsonAndCheck(R"({
    "path": [
      "$['org.iso.18013.5.1']['given_name']"
    ]
  })");
  fields->GetList().Append(std::move(new_field));

  EXPECT_FALSE(DigitalIdentityRequestImpl::IsOnlyRequestingAge(request));
}

TEST(DigitalIdentityRequestImpl, IsOnlyRequestingAge_NoConstraints) {
  base::Value request = GenerateOnlyAgeOpenid4VpRequest();
  base::Value* input_descriptors =
      FindValueWithKey(request, "input_descriptors");
  ASSERT_TRUE(IsNonEmptyList(input_descriptors));
  RemoveDictKey(input_descriptors->GetList().front().GetDict(), "constraints");

  EXPECT_FALSE(DigitalIdentityRequestImpl::IsOnlyRequestingAge(request));
}

TEST(DigitalIdentityRequestImpl, IsOnlyRequestingAge_EmptyInputDescriptorList) {
  base::Value request = GenerateOnlyAgeOpenid4VpRequest();
  base::Value* input_descriptors =
      FindValueWithKey(request, "input_descriptors");
  ASSERT_TRUE(IsNonEmptyList(input_descriptors));
  input_descriptors->GetList().resize(0);

  EXPECT_FALSE(DigitalIdentityRequestImpl::IsOnlyRequestingAge(request));
}

TEST(DigitalIdentityRequestImpl, IsOnlyRequestingAge_RequestMultipleDocuments) {
  base::Value request = GenerateOnlyAgeOpenid4VpRequest();
  base::Value* input_descriptors =
      FindValueWithKey(request, "input_descriptors");
  ASSERT_TRUE(IsNonEmptyList(input_descriptors));

  base::Value::List& input_descriptor_list = input_descriptors->GetList();
  input_descriptor_list.Append(input_descriptor_list.front().Clone());

  EXPECT_FALSE(DigitalIdentityRequestImpl::IsOnlyRequestingAge(request));
}

TEST(DigitalIdentityRequestImpl, IsOnlyRequestingAge_NonMdlInputDescriptorId) {
  base::Value request = GenerateOnlyAgeOpenid4VpRequest();
  base::Value* input_descriptors =
      FindValueWithKey(request, "input_descriptors");
  ASSERT_TRUE(IsNonEmptyList(input_descriptors));

  base::Value::List& input_descriptor_list = input_descriptors->GetList();
  ASSERT_TRUE(input_descriptor_list.front().is_dict());
  input_descriptor_list.front().GetDict().Set("id", "not_mdl");

  EXPECT_FALSE(DigitalIdentityRequestImpl::IsOnlyRequestingAge(request));
}

TEST(DigitalIdentityRequestImpl, IsOnlyRequestingAge_NoPresentationDefinition) {
  base::Value request = GenerateOnlyAgeOpenid4VpRequest();
  RemoveDictKey(request.GetDict(), "presentation_definition");

  EXPECT_FALSE(DigitalIdentityRequestImpl::IsOnlyRequestingAge(request));
}

}  // namespace content
