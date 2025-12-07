// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/digital_credentials/digital_identity_request_impl.h"

#include <optional>

#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "content/browser/webid/test/mock_digital_identity_provider.h"
#include "content/browser/webid/test/stub_digital_identity_provider.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/digital_identity_provider.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/webid/digital_identity_request.mojom.h"

namespace content {
namespace {

constexpr char kOpenid4vpProtocol[] = "openid4vp";
constexpr char kOpenid4vpSignedProtocol[] = "openid4vp-v1-signed";
constexpr char kOpenid4vpUnsignedProtocol[] = "openid4vp-v1-unsigned";
constexpr char kPreviewProtocol[] = "preview";

using base::Value;
using base::ValueView;
using testing::_;
using testing::DoAll;
using testing::Eq;
using testing::Optional;
using testing::WithArg;

using InterstitialType = content::DigitalIdentityInterstitialType;
using DigitalCredentialCreateRequestPtr =
    blink::mojom::DigitalCredentialCreateRequestPtr;
using DigitalCredentialCreateRequest =
    blink::mojom::DigitalCredentialCreateRequest;
using DigitalCredentialGetRequestPtr =
    blink::mojom::DigitalCredentialGetRequestPtr;
using DigitalCredentialGetRequest = blink::mojom::DigitalCredentialGetRequest;
using RequestDigitalIdentityStatus = blink::mojom::RequestDigitalIdentityStatus;
using DigitalIdentityCallback =
    DigitalIdentityProvider::DigitalIdentityCallback;
using DigitalCredential = DigitalIdentityProvider::DigitalCredential;
using GetCallback = blink::mojom::DigitalIdentityRequest::GetCallback;

// StubDigitalIdentityProvider which enables overriding
// DigitalIdentityProvider::IsLastCommittedOriginLowRisk().
class TestDigitalIdentityProviderWithCustomRisk
    : public StubDigitalIdentityProvider {
 public:
  explicit TestDigitalIdentityProviderWithCustomRisk(bool are_origins_low_risk)
      : are_origins_low_risk_(are_origins_low_risk) {}
  ~TestDigitalIdentityProviderWithCustomRisk() override = default;

  bool IsLastCommittedOriginLowRisk(
      RenderFrameHost& render_frame_host) const override {
    return are_origins_low_risk_;
  }

 private:
  bool are_origins_low_risk_;
};

base::Value ParseJsonAndCheck(const std::string& json) {
  std::optional<base::Value> parsed =
      base::JSONReader::Read(json, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  return parsed.has_value() ? std::move(*parsed) : base::Value();
}

base::Value GenerateOnlyAgeOpenid4VpRequestWithPresentationDefinition() {
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

base::Value GenerateOnlyAgeOpenid4VpRequestWithDCQL() {
  constexpr char kJson[] = R"({
  "response_type": "vp_token",
  "response_mode": "w3c_dc_api",
  "client_id": "web-origin:https://www.digital-credentials.dev",
  "nonce": "d_xvsQ_PF1oPVZbjAfWu_xgwh3dJf_W5zgWB3U2xWw8",
  "dcql_query": {
    "credentials": [
      {
        "id": "cred1",
        "format": "mso_mdoc",
        "meta": {
          "doctype_value": "org.iso.18013.5.1.mDL"
        },
        "claims": [
          {
            "path": [
              "org.iso.18013.5.1",
              "age_over_21"
            ]
          }
        ]
      }
    ]
  }
})";

  return ParseJsonAndCheck(kJson);
}

base::Value GenerateSignedOnlyAgeOpenid4VpRequestWithDCQL() {
  constexpr char kJson[] = R"({
    "request": "eyJhbGciOiJFUzI1NiIsInR5cCI6Im9hdXRoLWF1dGh6LXJlcStqd3QiLCJ4NWMiOlsiTUlJQ3FqQ0NBbEdnQXdJQkFnSVVXcVlEaTZjZy9YeEFDbDU1U1hJS3I5cVRGak13Q2dZSUtvWkl6ajBFQXdJd2VqRUxNQWtHQTFVRUJoTUNWVk14RXpBUkJnTlZCQWdNQ2tOaGJHbG1iM0p1YVdFeEZqQVVCZ05WQkFjTURVMXZkVzUwWVdsdUlGWnBaWGN4SERBYUJnTlZCQW9NRTBScFoybDBZV3dnUTNKbFpHVnVkR2xoYkhNeElEQWVCZ05WQkFNTUYyUnBaMmwwWVd3dFkzSmxaR1Z1ZEdsaGJITXVaR1YyTUI0WERUSTFNRFF5TlRFek5URXdPRm9YRFRNMU1EUXhNekV6TlRFd09Gb3dlakVMTUFrR0ExVUVCaE1DVlZNeEV6QVJCZ05WQkFnTUNrTmhiR2xtYjNKdWFXRXhGakFVQmdOVkJBY01EVTF2ZFc1MFlXbHVJRlpwWlhjeEhEQWFCZ05WQkFvTUUwUnBaMmwwWVd3Z1EzSmxaR1Z1ZEdsaGJITXhJREFlQmdOVkJBTU1GMlJwWjJsMFlXd3RZM0psWkdWdWRHbGhiSE11WkdWMk1Ga3dFd1lIS29aSXpqMENBUVlJS29aSXpqMERBUWNEUWdBRVgxbEVSYzlqNHBlbjdvRmszVU1FSEJjUEhvVlFYWDdXdkRwUkhNM1czdDNjeFNLRlA3T3dRcVJHNlZWQ01QdzJqeXM0NHpQTHZDaCtWNndDQlZ4R2FhT0J0RENCc1RBZEJnTlZIUTRFRmdRVTAwRm1Kd2kvazNlT09CU2Q5MnM0K1dYbnpIOHdId1lEVlIwakJCZ3dGb0FVMDBGbUp3aS9rM2VPT0JTZDkyczQrV1huekg4d0R3WURWUjBUQVFIL0JBVXdBd0VCL3pBT0JnTlZIUThCQWY4RUJBTUNCNEF3S2dZRFZSMFNCQ013SVlZZmFIUjBjSE02THk5a2FXZHBkR0ZzTFdOeVpXUmxiblJwWVd4ekxtUmxkakFpQmdOVkhSRUVHekFaZ2hka2FXZHBkR0ZzTFdOeVpXUmxiblJwWVd4ekxtUmxkakFLQmdncWhrak9QUVFEQWdOSEFEQkVBaUVBdzA4bVozQUdtc1lKZnZ2Vldia0MwV1E2WVdVUlhXaFE2Vnh5ZlllQ2pnc0NIeElnLzh5TkJ6RDBNRndRcnBXWVhKTkxvbHdHYlpPSjM0TUI0eVdsemtvPSJdfQ.eyJyZXNwb25zZV90eXBlIjogInZwX3Rva2VuIiwgInJlc3BvbnNlX21vZGUiOiAiZGNfYXBpIiwgIm5vbmNlIjogIjNmRzI4SjByNEVlTzNibUcxYzNuRGFfa2NVLTEyTEo4RmZFUFZjNnVnME0iLCAiZGNxbF9xdWVyeSI6IHsiY3JlZGVudGlhbHMiOiBbeyJpZCI6ICJjcmVkMSIsICJmb3JtYXQiOiAibXNvX21kb2MiLCAibWV0YSI6IHsiZG9jdHlwZV92YWx1ZSI6ICJvcmcuaXNvLjE4MDEzLjUuMS5tREwifSwgImNsYWltcyI6IFt7InBhdGgiOiBbIm9yZy5pc28uMTgwMTMuNS4xIiwgImFnZV9vdmVyXzIxIl19XX1dfSwgImNsaWVudF9tZXRhZGF0YSI6IHsidnBfZm9ybWF0c19zdXBwb3J0ZWQiOiB7Im1zb19tZG9jIjogeyJpc3N1ZXJhdXRoX2FsZ192YWx1ZXMiOiBbLTddLCAiZGV2aWNlYXV0aF9hbGdfdmFsdWVzIjogWy03XX19fSwgImNsaWVudF9pZCI6ICJ4NTA5X3Nhbl9kbnM6ZGlnaXRhbC1jcmVkZW50aWFscy5kZXYiLCAiZXhwZWN0ZWRfb3JpZ2lucyI6IFsiaHR0cHM6Ly9kaWdpdGFsLWNyZWRlbnRpYWxzLmRldiJdfQ.iHaZvr69d57S3qc40MUOqnU04zpX-uRuAstKjCDYH6mCxzPTgy8YXs1WEWNY-xOatPlS74IvunAvJr7wJaYqvg"
  })";

  return ParseJsonAndCheck(kJson);
}

base::Value GenerateSignedGivenNameOpenid4VpRequestWithDCQL() {
  constexpr char kJson[] = R"({
    "request": "eyJhbGciOiJFUzI1NiIsInR5cCI6Im9hdXRoLWF1dGh6LXJlcStqd3QiLCJ4NWMiOlsiTUlJQ3FqQ0NBbEdnQXdJQkFnSVVXcVlEaTZjZy9YeEFDbDU1U1hJS3I5cVRGak13Q2dZSUtvWkl6ajBFQXdJd2VqRUxNQWtHQTFVRUJoTUNWVk14RXpBUkJnTlZCQWdNQ2tOaGJHbG1iM0p1YVdFeEZqQVVCZ05WQkFjTURVMXZkVzUwWVdsdUlGWnBaWGN4SERBYUJnTlZCQW9NRTBScFoybDBZV3dnUTNKbFpHVnVkR2xoYkhNeElEQWVCZ05WQkFNTUYyUnBaMmwwWVd3dFkzSmxaR1Z1ZEdsaGJITXVaR1YyTUI0WERUSTFNRFF5TlRFek5URXdPRm9YRFRNMU1EUXhNekV6TlRFd09Gb3dlakVMTUFrR0ExVUVCaE1DVlZNeEV6QVJCZ05WQkFnTUNrTmhiR2xtYjNKdWFXRXhGakFVQmdOVkJBY01EVTF2ZFc1MFlXbHVJRlpwWlhjeEhEQWFCZ05WQkFvTUUwUnBaMmwwWVd3Z1EzSmxaR1Z1ZEdsaGJITXhJREFlQmdOVkJBTU1GMlJwWjJsMFlXd3RZM0psWkdWdWRHbGhiSE11WkdWMk1Ga3dFd1lIS29aSXpqMENBUVlJS29aSXpqMERBUWNEUWdBRVgxbEVSYzlqNHBlbjdvRmszVU1FSEJjUEhvVlFYWDdXdkRwUkhNM1czdDNjeFNLRlA3T3dRcVJHNlZWQ01QdzJqeXM0NHpQTHZDaCtWNndDQlZ4R2FhT0J0RENCc1RBZEJnTlZIUTRFRmdRVTAwRm1Kd2kvazNlT09CU2Q5MnM0K1dYbnpIOHdId1lEVlIwakJCZ3dGb0FVMDBGbUp3aS9rM2VPT0JTZDkyczQrV1huekg4d0R3WURWUjBUQVFIL0JBVXdBd0VCL3pBT0JnTlZIUThCQWY4RUJBTUNCNEF3S2dZRFZSMFNCQ013SVlZZmFIUjBjSE02THk5a2FXZHBkR0ZzTFdOeVpXUmxiblJwWVd4ekxtUmxkakFpQmdOVkhSRUVHekFaZ2hka2FXZHBkR0ZzTFdOeVpXUmxiblJwWVd4ekxtUmxkakFLQmdncWhrak9QUVFEQWdOSEFEQkVBaUVBdzA4bVozQUdtc1lKZnZ2Vldia0MwV1E2WVdVUlhXaFE2Vnh5ZlllQ2pnc0NIeElnLzh5TkJ6RDBNRndRcnBXWVhKTkxvbHdHYlpPSjM0TUI0eVdsemtvPSJdfQ.eyJyZXNwb25zZV90eXBlIjogInZwX3Rva2VuIiwgInJlc3BvbnNlX21vZGUiOiAiZGNfYXBpIiwgIm5vbmNlIjogIk1iNzVFQTROdWdlZTRqdjdYVGZjWk1ZN1dLYUJ0eVB3YjJuTGpSb0NkTGsiLCAiZGNxbF9xdWVyeSI6IHsiY3JlZGVudGlhbHMiOiBbeyJpZCI6ICJjcmVkMSIsICJmb3JtYXQiOiAibXNvX21kb2MiLCAibWV0YSI6IHsiZG9jdHlwZV92YWx1ZSI6ICJvcmcuaXNvLjE4MDEzLjUuMS5tREwifSwgImNsYWltcyI6IFt7InBhdGgiOiBbIm9yZy5pc28uMTgwMTMuNS4xIiwgImdpdmVuX25hbWUiXX1dfV19LCAiY2xpZW50X21ldGFkYXRhIjogeyJ2cF9mb3JtYXRzX3N1cHBvcnRlZCI6IHsibXNvX21kb2MiOiB7Imlzc3VlcmF1dGhfYWxnX3ZhbHVlcyI6IFstN10sICJkZXZpY2VhdXRoX2FsZ192YWx1ZXMiOiBbLTddfX19LCAiY2xpZW50X2lkIjogIng1MDlfc2FuX2RuczpkaWdpdGFsLWNyZWRlbnRpYWxzLmRldiIsICJleHBlY3RlZF9vcmlnaW5zIjogWyJodHRwczovL2RpZ2l0YWwtY3JlZGVudGlhbHMuZGV2Il19.1JymSi56nHbnm2Ggye6YHsYCa3Mchmdf-WJUoEpjYGu3gMcHM50cfCMkUmxkvWV7E1R_ot2VAYLLBcSJRw_3fg"

  })";

  return ParseJsonAndCheck(kJson);
}

base::Value GenerateSignedGivenNameAndAgeOpenid4VpRequestWithDCQL() {
  constexpr char kJson[] = R"({
    "request": "eyJhbGciOiJFUzI1NiIsInR5cCI6Im9hdXRoLWF1dGh6LXJlcStqd3QiLCJ4NWMiOlsiTUlJQ3FqQ0NBbEdnQXdJQkFnSVVXcVlEaTZjZy9YeEFDbDU1U1hJS3I5cVRGak13Q2dZSUtvWkl6ajBFQXdJd2VqRUxNQWtHQTFVRUJoTUNWVk14RXpBUkJnTlZCQWdNQ2tOaGJHbG1iM0p1YVdFeEZqQVVCZ05WQkFjTURVMXZkVzUwWVdsdUlGWnBaWGN4SERBYUJnTlZCQW9NRTBScFoybDBZV3dnUTNKbFpHVnVkR2xoYkhNeElEQWVCZ05WQkFNTUYyUnBaMmwwWVd3dFkzSmxaR1Z1ZEdsaGJITXVaR1YyTUI0WERUSTFNRFF5TlRFek5URXdPRm9YRFRNMU1EUXhNekV6TlRFd09Gb3dlakVMTUFrR0ExVUVCaE1DVlZNeEV6QVJCZ05WQkFnTUNrTmhiR2xtYjNKdWFXRXhGakFVQmdOVkJBY01EVTF2ZFc1MFlXbHVJRlpwWlhjeEhEQWFCZ05WQkFvTUUwUnBaMmwwWVd3Z1EzSmxaR1Z1ZEdsaGJITXhJREFlQmdOVkJBTU1GMlJwWjJsMFlXd3RZM0psWkdWdWRHbGhiSE11WkdWMk1Ga3dFd1lIS29aSXpqMENBUVlJS29aSXpqMERBUWNEUWdBRVgxbEVSYzlqNHBlbjdvRmszVU1FSEJjUEhvVlFYWDdXdkRwUkhNM1czdDNjeFNLRlA3T3dRcVJHNlZWQ01QdzJqeXM0NHpQTHZDaCtWNndDQlZ4R2FhT0J0RENCc1RBZEJnTlZIUTRFRmdRVTAwRm1Kd2kvazNlT09CU2Q5MnM0K1dYbnpIOHdId1lEVlIwakJCZ3dGb0FVMDBGbUp3aS9rM2VPT0JTZDkyczQrV1huekg4d0R3WURWUjBUQVFIL0JBVXdBd0VCL3pBT0JnTlZIUThCQWY4RUJBTUNCNEF3S2dZRFZSMFNCQ013SVlZZmFIUjBjSE02THk5a2FXZHBkR0ZzTFdOeVpXUmxiblJwWVd4ekxtUmxkakFpQmdOVkhSRUVHekFaZ2hka2FXZHBkR0ZzTFdOeVpXUmxiblJwWVd4ekxtUmxkakFLQmdncWhrak9QUVFEQWdOSEFEQkVBaUVBdzA4bVozQUdtc1lKZnZ2Vldia0MwV1E2WVdVUlhXaFE2Vnh5ZlllQ2pnc0NIeElnLzh5TkJ6RDBNRndRcnBXWVhKTkxvbHdHYlpPSjM0TUI0eVdsemtvPSJdfQ.eyJyZXNwb25zZV90eXBlIjogInZwX3Rva2VuIiwgInJlc3BvbnNlX21vZGUiOiAiZGNfYXBpIiwgIm5vbmNlIjogIlZHc1NWdWFDUUdYSEFaMktZVzB5QVMzVy02LTQycFI5bTlUa3lvYjd1UFkiLCAiZGNxbF9xdWVyeSI6IHsiY3JlZGVudGlhbHMiOiBbeyJpZCI6ICJjcmVkMSIsICJmb3JtYXQiOiAibXNvX21kb2MiLCAibWV0YSI6IHsiZG9jdHlwZV92YWx1ZSI6ICJvcmcuaXNvLjE4MDEzLjUuMS5tREwifSwgImNsYWltcyI6IFt7InBhdGgiOiBbIm9yZy5pc28uMTgwMTMuNS4xIiwgImdpdmVuX25hbWUiXX0sIHsicGF0aCI6IFsib3JnLmlzby4xODAxMy41LjEiLCAiYWdlX292ZXJfMjEiXX1dfV19LCAiY2xpZW50X21ldGFkYXRhIjogeyJ2cF9mb3JtYXRzX3N1cHBvcnRlZCI6IHsibXNvX21kb2MiOiB7Imlzc3VlcmF1dGhfYWxnX3ZhbHVlcyI6IFstN10sICJkZXZpY2VhdXRoX2FsZ192YWx1ZXMiOiBbLTddfX19LCAiY2xpZW50X2lkIjogIng1MDlfc2FuX2RuczpkaWdpdGFsLWNyZWRlbnRpYWxzLmRldiIsICJleHBlY3RlZF9vcmlnaW5zIjogWyJodHRwczovL2RpZ2l0YWwtY3JlZGVudGlhbHMuZGV2Il19.ZHtU19tEM1P5GNIl5oJTSyN6TQvMJAo7ruA1xHy-fvYanQLadoMVTTIuXgni6zMX2oSkZEBXp8ZD7wp-UJL1Pg"
  })";

  return ParseJsonAndCheck(kJson);
}

base::Value GenerateVerifyPhoneNumberOpenid4VpRequest() {
  constexpr char kJson[] = R"({
  "response_type": "vp_token",
  "response_mode": "dc_api",
  "client_id": "web-origin:https://www.digital-credentials.dev",
  "nonce": "y9f67H0Kb2QF7nSbYh-XxBKkvGTCHk5MQo9OLBkKWD0",
  "dcql_query": {
    "credentials": [
      {
        "claims": [
          {
            "path": [
              "subscription_hint"
            ],
            "values": [
              1
            ]
          },
          {
            "path": [
              "carrier_hint"
            ],
            "values": [
              "310250"
            ]
          },
          {
            "path": [
              "android_carrier_hint"
            ],
            "values": [
              7
            ]
          }
        ],
        "format": "dc-authorization+sd-jwt",
        "id": "aggregator1",
        "meta": {
          "credential_authorization_jwt": "eyJhbGciOiJFUzI1NiIsInR5cCI6Im9hdXRoLWF1dGh6LXJlcStqd3QiLCJ4NWMiOlsiTUlJQ3BUQ0NBa3VnQXdJQkFnSVVDOWZOSnBkVU1RWWRCbDFuaDgrUml0UndNRDh3Q2dZSUtvWkl6ajBFQXdJd2VERUxNQWtHQTFVRUJoTUNWVk14RXpBUkJnTlZCQWdNQ2tOaGJHbG1iM0p1YVdFeEZqQVVCZ05WQkFjTURVMXZkVzUwWVdsdUlGWnBaWGN4R3pBWkJnTlZCQW9NRWtWNFlXMXdiR1VnUVdkbmNtVm5ZWFJ2Y2pFZk1CMEdBMVVFQXd3V1pYaGhiWEJzWlMxaFoyZHlaV2RoZEc5eUxtUmxkakFlRncweU5UQTFNVEV5TWpRd01EVmFGdzB6TlRBME1qa3lNalF3TURWYU1IZ3hDekFKQmdOVkJBWVRBbFZUTVJNd0VRWURWUVFJREFwRFlXeHBabTl5Ym1saE1SWXdGQVlEVlFRSERBMU5iM1Z1ZEdGcGJpQldhV1YzTVJzd0dRWURWUVFLREJKRmVHRnRjR3hsSUVGblozSmxaMkYwYjNJeEh6QWRCZ05WQkFNTUZtVjRZVzF3YkdVdFlXZG5jbVZuWVhSdmNpNWtaWFl3V1RBVEJnY3Foa2pPUFFJQkJnZ3Foa2pPUFFNQkJ3TkNBQVJRcW5LTGw5U2g4dFcwM0h5aVBnOVRUcGlyQVg2V2haKzlJSWhVWFJGcDlxRFM0eW5YeG1GbjMzWk5nMTlQR1VzRWpxNGwzam9Penh2cHhqWDRoL1JlbzRHeU1JR3ZNQjBHQTFVZERnUVdCQlFBV1I5czRrWFRjeHJPeTFLSE12UldTSkg5YmpBZkJnTlZIU01FR0RBV2dCUUFXUjlzNGtYVGN4ck95MUtITXZSV1NKSDliakFQQmdOVkhSTUJBZjhFQlRBREFRSC9NQTRHQTFVZER3RUIvd1FFQXdJSGdEQXBCZ05WSFJJRUlqQWdoaDVvZEhSd2N6b3ZMMlY0WVcxd2JHVXRZV2RuY21WbllYUnZjaTVqYjIwd0lRWURWUjBSQkJvd0dJSVdaWGhoYlhCc1pTMWhaMmR5WldkaGRHOXlMbU52YlRBS0JnZ3Foa2pPUFFRREFnTklBREJGQWlCeERROUZiby9EUVRkbVNaS0NURUlHOXZma0JkWU5jVHcxUkkzT0k2L25KUUloQUw1NmU3YkVNOTlSTTFTUDAyd3gzbHhxZFZCWnhiVEhJcllCQkY3Y0FzYjMiXX0.eyJpc3MiOiAiaHR0cHM6Ly9kY2FnZ3JlZ2F0b3IuZGV2IiwgIm5vbmNlIjogInk5ZjY3SDBLYjJRRjduU2JZaC1YeEJLa3ZHVENIazVNUW85T0xCa0tXRDAiLCAiZW5jcnlwdGVkX3Jlc3BvbnNlX2VuY192YWx1ZXNfc3VwcG9ydGVkIjogWyJBMTI4R0NNIl0sICJqd2tzIjogeyJrZXlzIjogW3sia3R5IjogIkVDIiwgInVzZSI6ICJlbmMiLCAiYWxnIjogIkVDREgtRVMiLCAia2lkIjogIjEiLCAiY3J2IjogIlAtMjU2IiwgIngiOiAiY2g1eFFhSUtCdjlPdG95Mmlmb2hMUWJTTWRlVE5paVFyWEVvcjRreHBtSSIsICJ5IjogInU2MlBuTkQwZUhEay1tRFFjOFI0YUEyRVdjRkE5VVo0YVpjOG1KZDlTX00ifV19LCAiY29uc2VudF9kYXRhIjogImV5SmpiMjV6Wlc1MFgzUmxlSFFpT2lBaVVtbGtaWElnY0hKdlkyVnpjMlZ6SUhsdmRYSWdjR1Z5YzI5dVlXd2daR0YwWVNCaFkyTnZjbVJwYm1jZ2RHOGdiM1Z5SUhCeWFYWmhZM2tnY0c5c2FXTjVJaXdnSW5CdmJHbGplVjlzYVc1cklqb2dJbWgwZEhCek9pOHZaR1YyWld4dmNHVnlMbUZ1WkhKdmFXUXVZMjl0TDJsa1pXNTBhWFI1TDJScFoybDBZV3d0WTNKbFpHVnVkR2xoYkhNdlkzSmxaR1Z1ZEdsaGJDMTJaWEpwWm1sbGNpSXNJQ0p3YjJ4cFkzbGZkR1Y0ZENJNklDSk1aV0Z5YmlCaFltOTFkQ0J3Y21sMllXTjVJSEJ2YkdsamVTSjkifQ.rlVyABcvR1a-g7eyPSKJBeIgrsUkIsVHNKAFrEKeeQ5Qyscys02T_z3I72g0jGqbAddEBgq9rLuncc7z3ayp-Q",
          "vct_values": [
            "number-verification/verify/ts43"
          ]
        }
      }
    ]
  }
})";

  return ParseJsonAndCheck(kJson);
}

base::Value GenerateGetPhoneNumberOpenid4VpRequest() {
  constexpr char kJson[] = R"({
  "response_type": "vp_token",
  "response_mode": "dc_api",
  "client_id": "web-origin:https://www.digital-credentials.dev",
  "nonce": "y9f67H0Kb2QF7nSbYh-XxBKkvGTCHk5MQo9OLBkKWD0",
  "dcql_query": {
    "credentials": [
      {
        "format": "dc-authorization+sd-jwt",
        "id": "aggregator1",
        "meta": {
          "credential_authorization_jwt": "eyJhbGciOiJFUzI1NiIsInR5cCI6Im9hdXRoLWF1dGh6LXJlcStqd3QiLCJ4NWMiOlsiTUlJQ3BUQ0NBa3VnQXdJQkFnSVVDOWZOSnBkVU1RWWRCbDFuaDgrUml0UndNRDh3Q2dZSUtvWkl6ajBFQXdJd2VERUxNQWtHQTFVRUJoTUNWVk14RXpBUkJnTlZCQWdNQ2tOaGJHbG1iM0p1YVdFeEZqQVVCZ05WQkFjTURVMXZkVzUwWVdsdUlGWnBaWGN4R3pBWkJnTlZCQW9NRWtWNFlXMXdiR1VnUVdkbmNtVm5ZWFJ2Y2pFZk1CMEdBMVVFQXd3V1pYaGhiWEJzWlMxaFoyZHlaV2RoZEc5eUxtUmxkakFlRncweU5UQTFNVEV5TWpRd01EVmFGdzB6TlRBME1qa3lNalF3TURWYU1IZ3hDekFKQmdOVkJBWVRBbFZUTVJNd0VRWURWUVFJREFwRFlXeHBabTl5Ym1saE1SWXdGQVlEVlFRSERBMU5iM1Z1ZEdGcGJpQldhV1YzTVJzd0dRWURWUVFLREJKRmVHRnRjR3hsSUVGblozSmxaMkYwYjNJeEh6QWRCZ05WQkFNTUZtVjRZVzF3YkdVdFlXZG5jbVZuWVhSdmNpNWtaWFl3V1RBVEJnY3Foa2pPUFFJQkJnZ3Foa2pPUFFNQkJ3TkNBQVJRcW5LTGw5U2g4dFcwM0h5aVBnOVRUcGlyQVg2V2haKzlJSWhVWFJGcDlxRFM0eW5YeG1GbjMzWk5nMTlQR1VzRWpxNGwzam9Penh2cHhqWDRoL1JlbzRHeU1JR3ZNQjBHQTFVZERnUVdCQlFBV1I5czRrWFRjeHJPeTFLSE12UldTSkg5YmpBZkJnTlZIU01FR0RBV2dCUUFXUjlzNGtYVGN4ck95MUtITXZSV1NKSDliakFQQmdOVkhSTUJBZjhFQlRBREFRSC9NQTRHQTFVZER3RUIvd1FFQXdJSGdEQXBCZ05WSFJJRUlqQWdoaDVvZEhSd2N6b3ZMMlY0WVcxd2JHVXRZV2RuY21WbllYUnZjaTVqYjIwd0lRWURWUjBSQkJvd0dJSVdaWGhoYlhCc1pTMWhaMmR5WldkaGRHOXlMbU52YlRBS0JnZ3Foa2pPUFFRREFnTklBREJGQWlCeERROUZiby9EUVRkbVNaS0NURUlHOXZma0JkWU5jVHcxUkkzT0k2L25KUUloQUw1NmU3YkVNOTlSTTFTUDAyd3gzbHhxZFZCWnhiVEhJcllCQkY3Y0FzYjMiXX0.eyJpc3MiOiAiaHR0cHM6Ly9kY2FnZ3JlZ2F0b3IuZGV2IiwgIm5vbmNlIjogImVnOXFPRjZjQXdsV1ZrTVNIRjREWkdJZF9xZVhLcG9yUzdPUnJUTE5RODAiLCAiZW5jcnlwdGVkX3Jlc3BvbnNlX2VuY192YWx1ZXNfc3VwcG9ydGVkIjogWyJBMTI4R0NNIl0sICJqd2tzIjogeyJrZXlzIjogW3sia3R5IjogIkVDIiwgInVzZSI6ICJlbmMiLCAiYWxnIjogIkVDREgtRVMiLCAia2lkIjogIjEiLCAiY3J2IjogIlAtMjU2IiwgIngiOiAiUThLT25XYzdWWDdkb3RuRU9jT0daOVFudUg5MlBFSWQwR0dDQ3lXT0R3TSIsICJ5IjogIm1LNjd2emRnOGxveHpQNWVkazVLb0FnNmZpenhoVXgyN3Q0cFdTb1lMVVUifV19LCAiY29uc2VudF9kYXRhIjogImV5SmpiMjV6Wlc1MFgzUmxlSFFpT2lBaVVtbGtaWElnY0hKdlkyVnpjMlZ6SUhsdmRYSWdjR1Z5YzI5dVlXd2daR0YwWVNCaFkyTnZjbVJwYm1jZ2RHOGdiM1Z5SUhCeWFYWmhZM2tnY0c5c2FXTjVJaXdnSW5CdmJHbGplVjlzYVc1cklqb2dJbWgwZEhCek9pOHZaR1YyWld4dmNHVnlMbUZ1WkhKdmFXUXVZMjl0TDJsa1pXNTBhWFI1TDJScFoybDBZV3d0WTNKbFpHVnVkR2xoYkhNdlkzSmxaR1Z1ZEdsaGJDMTJaWEpwWm1sbGNpSXNJQ0p3YjJ4cFkzbGZkR1Y0ZENJNklDSk1aV0Z5YmlCaFltOTFkQ0J3Y21sMllXTjVJSEJ2YkdsamVTSjkifQ.vtHXBcFG_lzTCfiVfrupmSd4k7CptvBAknq821A5QmNqGVQmnzmYUlTF6a9bFdigeE2q_yJRfchJoiHXSUM_Uw",
          "vct_values": [
            "number-verification/device-phone-number/ts43"
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

// Used to modify an Openid4VpRequest with Presentation Definition on the fly.
bool SetPathItem(base::Value& to_modify, const std::string& path_item) {
  base::Value* paths = FindValueWithKey(to_modify, "path");
  if (HasNoListElements(paths)) {
    return false;
  }
  paths->GetList().resize(0);
  paths->GetList().Append(path_item);
  return true;
}

// Used to modify an Openid4VpRequest with DCQL on the fly.
bool SetDCQLPathItem(base::Value& to_modify,
                     const std::string& field_name_value) {
  base::Value* paths = FindValueWithKey(to_modify, "path");
  if (HasNoListElements(paths)) {
    return false;
  }
  paths->GetList().resize(1);
  paths->GetList().Append(field_name_value);
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

}  // anonymous namespace

class DigitalIdentityRequestImplInterstitialTest
    : public RenderViewHostTestHarness {
 public:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kWebIdentityDigitalCredentials, {{"dialog", ""}});
  }

  std::optional<InterstitialType> ComputeInterstitialType(
      const std::string& protocol,
      base::Value request_data,
      bool are_origins_low_risk = false) {
    auto provider = std::make_unique<TestDigitalIdentityProviderWithCustomRisk>(
        are_origins_low_risk);
    DigitalCredentialGetRequestPtr digital_credential_request =
        DigitalCredentialGetRequest::New();
    digital_credential_request->protocol = protocol;
    digital_credential_request->data = std::move(request_data);
    std::vector<DigitalCredentialGetRequestPtr> requests;
    requests.emplace_back(std::move(digital_credential_request));
    return DigitalIdentityRequestImpl::ComputeInterstitialType(
        *main_rfh(), provider.get(), std::move(requests));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(
    DigitalIdentityRequestImplInterstitialTest,
    Openid4VpProtocolPresentationDefinition_ComputeInterstitialType_OnlyAgeOver) {
  EXPECT_EQ(ComputeInterstitialType(
                kOpenid4vpProtocol,
                GenerateOnlyAgeOpenid4VpRequestWithPresentationDefinition()),
            std::nullopt);
}

TEST_F(
    DigitalIdentityRequestImplInterstitialTest,
    Openid4VpProtocolPresentationDefinition_ComputeInterstitialType_OnlyAgeInYears) {
  base::Value request =
      GenerateOnlyAgeOpenid4VpRequestWithPresentationDefinition();
  ASSERT_TRUE(SetPathItem(request, "$['org.iso.18013.5.1']['age_in_years']"));
  EXPECT_EQ(ComputeInterstitialType(kOpenid4vpProtocol, std::move(request)),
            std::nullopt);
}

TEST_F(
    DigitalIdentityRequestImplInterstitialTest,
    Openid4VpProtocolPresentationDefinition_ComputeInterstitialType_OnlyAgeBirthYear) {
  base::Value request =
      GenerateOnlyAgeOpenid4VpRequestWithPresentationDefinition();
  ASSERT_TRUE(SetPathItem(request, "$['org.iso.18013.5.1']['age_birth_year']"));
  EXPECT_EQ(ComputeInterstitialType(kOpenid4vpProtocol, std::move(request)),
            std::nullopt);
}

TEST_F(
    DigitalIdentityRequestImplInterstitialTest,
    Openid4VpProtocolPresentationDefinition_ComputeInterstitialType_OnlyBirthDate) {
  base::Value request =
      GenerateOnlyAgeOpenid4VpRequestWithPresentationDefinition();
  ASSERT_TRUE(SetPathItem(request, "$['org.iso.18013.5.1']['birth_date']"));
  EXPECT_EQ(ComputeInterstitialType(kOpenid4vpProtocol, std::move(request)),
            std::nullopt);
}

base::Value GenerateNonAgeOpenid4VpRequest() {
  base::Value request =
      GenerateOnlyAgeOpenid4VpRequestWithPresentationDefinition();
  CHECK(SetPathItem(request, "$['org.iso.18013.5.1']['given_name']"));
  return request;
}

TEST_F(
    DigitalIdentityRequestImplInterstitialTest,
    Openid4VpProtocolPresentationDefinition_ComputeInterstitialType_OnlyNonAgeDataElement) {
  EXPECT_EQ(ComputeInterstitialType(kOpenid4vpProtocol,
                                    GenerateNonAgeOpenid4VpRequest()),
            InterstitialType::kLowRisk);
}

TEST_F(
    DigitalIdentityRequestImplInterstitialTest,
    Openid4VpProtocolPresentationDefinition_LowRiskOriginTakesPrecedenceOverRequestType) {
  EXPECT_EQ(ComputeInterstitialType(kOpenid4vpProtocol,
                                    GenerateNonAgeOpenid4VpRequest(),
                                    /*are_origins_low_risk=*/true),
            std::nullopt);
}

TEST_F(
    DigitalIdentityRequestImplInterstitialTest,
    Openid4VpProtocolPresentationDefinition_ComputeInterstitialType_EmptyPathList) {
  base::Value request =
      GenerateOnlyAgeOpenid4VpRequestWithPresentationDefinition();
  base::Value* paths = FindValueWithKey(request, "path");
  ASSERT_TRUE(IsNonEmptyList(paths));
  paths->GetList().resize(0);

  EXPECT_EQ(ComputeInterstitialType(kOpenid4vpProtocol, std::move(request)),
            InterstitialType::kLowRisk);
}

TEST_F(
    DigitalIdentityRequestImplInterstitialTest,
    Openid4VpProtocolPresentationDefinition_ComputeInterstitialType_RequestMultiplePaths) {
  base::Value request =
      GenerateOnlyAgeOpenid4VpRequestWithPresentationDefinition();
  base::Value* paths = FindValueWithKey(request, "path");
  ASSERT_TRUE(IsNonEmptyList(paths));

  base::Value::List& path_list = paths->GetList();
  path_list.Append(path_list.front().Clone());

  EXPECT_EQ(ComputeInterstitialType(kOpenid4vpProtocol, std::move(request)),
            InterstitialType::kLowRisk);
}

TEST_F(DigitalIdentityRequestImplInterstitialTest,
       Openid4VpProtocolPresentationDefinition_ComputeInterstitialType_NoPath) {
  base::Value request =
      GenerateOnlyAgeOpenid4VpRequestWithPresentationDefinition();
  base::Value* fields = FindValueWithKey(request, "fields");
  ASSERT_TRUE(IsNonEmptyList(fields));
  RemoveDictKey(fields->GetList().front().GetDict(), "path");

  EXPECT_EQ(ComputeInterstitialType(kOpenid4vpProtocol, std::move(request)),
            InterstitialType::kLowRisk);
}

TEST_F(
    DigitalIdentityRequestImplInterstitialTest,
    Openid4VpProtocolPresentationDefinition_ComputeInterstitialType_EmptyFieldsList) {
  base::Value request =
      GenerateOnlyAgeOpenid4VpRequestWithPresentationDefinition();
  base::Value* fields = FindValueWithKey(request, "fields");
  ASSERT_TRUE(IsNonEmptyList(fields));
  fields->GetList().resize(0);

  EXPECT_EQ(ComputeInterstitialType(kOpenid4vpProtocol, std::move(request)),
            InterstitialType::kLowRisk);
}

TEST_F(
    DigitalIdentityRequestImplInterstitialTest,
    Openid4VpProtocolPresentationDefinition_ComputeInterstitialType_RequestMultipleAgeAssertions) {
  base::Value request =
      GenerateOnlyAgeOpenid4VpRequestWithPresentationDefinition();
  base::Value* fields = FindValueWithKey(request, "fields");
  ASSERT_TRUE(IsNonEmptyList(fields));

  base::Value new_field = ParseJsonAndCheck(R"({
    "path": [
      "$['org.iso.18013.5.1']['age_over_60']"
    ]
  })");
  fields->GetList().Append(std::move(new_field));

  EXPECT_EQ(ComputeInterstitialType(kOpenid4vpProtocol, std::move(request)),
            InterstitialType::kLowRisk);
}

TEST_F(
    DigitalIdentityRequestImplInterstitialTest,
    Openid4VpProtocolPresentationDefinition_ComputeInterstitialType_RequestMultipleFields) {
  base::Value request =
      GenerateOnlyAgeOpenid4VpRequestWithPresentationDefinition();
  base::Value* fields = FindValueWithKey(request, "fields");
  ASSERT_TRUE(IsNonEmptyList(fields));

  base::Value new_field = ParseJsonAndCheck(R"({
    "path": [
      "$['org.iso.18013.5.1']['given_name']"
    ]
  })");
  fields->GetList().Append(std::move(new_field));

  EXPECT_EQ(ComputeInterstitialType(kOpenid4vpProtocol, std::move(request)),
            InterstitialType::kLowRisk);
}

TEST_F(
    DigitalIdentityRequestImplInterstitialTest,
    Openid4VpProtocolPresentationDefinition_ComputeInterstitialType_NoConstraints) {
  base::Value request =
      GenerateOnlyAgeOpenid4VpRequestWithPresentationDefinition();
  base::Value* input_descriptors =
      FindValueWithKey(request, "input_descriptors");
  ASSERT_TRUE(IsNonEmptyList(input_descriptors));
  RemoveDictKey(input_descriptors->GetList().front().GetDict(), "constraints");

  EXPECT_EQ(ComputeInterstitialType(kOpenid4vpProtocol, std::move(request)),
            InterstitialType::kLowRisk);
}

TEST_F(
    DigitalIdentityRequestImplInterstitialTest,
    Openid4VpProtocolPresentationDefinition_ComputeInterstitialType_EmptyInputDescriptorList) {
  base::Value request =
      GenerateOnlyAgeOpenid4VpRequestWithPresentationDefinition();
  base::Value* input_descriptors =
      FindValueWithKey(request, "input_descriptors");
  ASSERT_TRUE(IsNonEmptyList(input_descriptors));
  input_descriptors->GetList().resize(0);

  EXPECT_EQ(ComputeInterstitialType(kOpenid4vpProtocol, std::move(request)),
            InterstitialType::kLowRisk);
}

TEST_F(
    DigitalIdentityRequestImplInterstitialTest,
    Openid4VpProtocolPresentationDefinition_ComputeInterstitialType_RequestMultipleDocuments) {
  base::Value request =
      GenerateOnlyAgeOpenid4VpRequestWithPresentationDefinition();
  base::Value* input_descriptors =
      FindValueWithKey(request, "input_descriptors");
  ASSERT_TRUE(IsNonEmptyList(input_descriptors));

  base::Value::List& input_descriptor_list = input_descriptors->GetList();
  input_descriptor_list.Append(input_descriptor_list.front().Clone());

  EXPECT_EQ(ComputeInterstitialType(kOpenid4vpProtocol, std::move(request)),
            InterstitialType::kLowRisk);
}

TEST_F(
    DigitalIdentityRequestImplInterstitialTest,
    Openid4VpProtocolPresentationDefinition_ComputeInterstitialType_NonMdlInputDescriptorId) {
  base::Value request =
      GenerateOnlyAgeOpenid4VpRequestWithPresentationDefinition();
  base::Value* input_descriptors =
      FindValueWithKey(request, "input_descriptors");
  ASSERT_TRUE(IsNonEmptyList(input_descriptors));

  base::Value::List& input_descriptor_list = input_descriptors->GetList();
  ASSERT_TRUE(input_descriptor_list.front().is_dict());
  input_descriptor_list.front().GetDict().Set("id", "not_mdl");

  EXPECT_EQ(ComputeInterstitialType(kOpenid4vpProtocol, std::move(request)),
            InterstitialType::kLowRisk);
}

TEST_F(
    DigitalIdentityRequestImplInterstitialTest,
    Openid4VpProtocolPresentationDefinition_ComputeInterstitialType_NoPresentationDefinition) {
  base::Value request =
      GenerateOnlyAgeOpenid4VpRequestWithPresentationDefinition();
  RemoveDictKey(request.GetDict(), "presentation_definition");

  EXPECT_EQ(ComputeInterstitialType(kOpenid4vpProtocol, std::move(request)),
            InterstitialType::kLowRisk);
}

TEST_F(DigitalIdentityRequestImplInterstitialTest,
       PreviewProtocol_ComputeInterstitialType_OnlyAgeOver) {
  EXPECT_EQ(ComputeInterstitialType(kPreviewProtocol,
                                    GenerateOnlyAgePreviewRequest()),
            std::nullopt);
}

TEST_F(DigitalIdentityRequestImplInterstitialTest,
       PreviewProtocol_ComputeInterstitialType_OnlyAgeInYears) {
  base::Value request = GenerateOnlyAgePreviewRequest();
  ASSERT_TRUE(SetFieldNameValue(request, "age_in_years"));
  EXPECT_EQ(ComputeInterstitialType(kPreviewProtocol, std::move(request)),
            std::nullopt);
}

TEST_F(DigitalIdentityRequestImplInterstitialTest,
       PreviewProtocol_ComputeInterstitialType_OnlyAgeBirthYear) {
  base::Value request = GenerateOnlyAgePreviewRequest();
  ASSERT_TRUE(SetFieldNameValue(request, "age_birth_year"));
  EXPECT_EQ(ComputeInterstitialType(kPreviewProtocol, std::move(request)),
            std::nullopt);
}

TEST_F(DigitalIdentityRequestImplInterstitialTest,
       PreviewProtocol_ComputeInterstitialType_OnlyBirthDate) {
  base::Value request = GenerateOnlyAgePreviewRequest();
  ASSERT_TRUE(SetFieldNameValue(request, "birth_date"));
  EXPECT_EQ(ComputeInterstitialType(kPreviewProtocol, std::move(request)),
            std::nullopt);
}

TEST_F(DigitalIdentityRequestImplInterstitialTest,
       PreviewProtocol_ComputeInterstitialType_GivenName) {
  base::Value request = GenerateOnlyAgePreviewRequest();
  ASSERT_TRUE(SetFieldNameValue(request, "given_name"));
  EXPECT_EQ(ComputeInterstitialType(kPreviewProtocol, std::move(request)),
            InterstitialType::kLowRisk);
}

TEST_F(DigitalIdentityRequestImplInterstitialTest,
       Openid4VpProtocolDCQL_ComputeInterstitialType_OnlyAgeOver) {
  EXPECT_EQ(ComputeInterstitialType(kOpenid4vpProtocol,
                                    GenerateOnlyAgeOpenid4VpRequestWithDCQL()),
            std::nullopt);
}

TEST_F(DigitalIdentityRequestImplInterstitialTest,
       Openid4VpProtocolDCQL_ComputeInterstitialType_OnlyAgeBirthYear) {
  base::Value request = GenerateOnlyAgeOpenid4VpRequestWithDCQL();
  ASSERT_TRUE(SetDCQLPathItem(request, "age_birth_year"));
  EXPECT_EQ(ComputeInterstitialType(kOpenid4vpProtocol, std::move(request)),
            std::nullopt);
}

TEST_F(DigitalIdentityRequestImplInterstitialTest,
       Openid4VpProtocolDCQL_ComputeInterstitialType_OnlyBirthDate) {
  base::Value request = GenerateOnlyAgeOpenid4VpRequestWithDCQL();
  ASSERT_TRUE(SetDCQLPathItem(request, "birth_date"));
  EXPECT_EQ(ComputeInterstitialType(kOpenid4vpProtocol, std::move(request)),
            std::nullopt);
}

TEST_F(DigitalIdentityRequestImplInterstitialTest,
       Openid4VpProtocolDCQL_ComputeInterstitialType_GivenName) {
  base::Value request = GenerateOnlyAgeOpenid4VpRequestWithDCQL();
  ASSERT_TRUE(SetDCQLPathItem(request, "given_name"));
  EXPECT_EQ(ComputeInterstitialType(kOpenid4vpProtocol, std::move(request)),
            InterstitialType::kLowRisk);
}

TEST_F(DigitalIdentityRequestImplInterstitialTest,
       Openid4VpUnsignedProtocolDCQL_ComputeInterstitialType_OnlyAgeOver) {
  EXPECT_EQ(ComputeInterstitialType(kOpenid4vpUnsignedProtocol,
                                    GenerateOnlyAgeOpenid4VpRequestWithDCQL()),
            std::nullopt);
}

TEST_F(DigitalIdentityRequestImplInterstitialTest,
       Openid4VpProtocolDCQL_ComputeInterstitialType_GivenNameAndAgeOver) {
  base::Value request = ParseJsonAndCheck(R"({
  "response_type": "vp_token",
  "response_mode": "dc_api",
  "nonce": "EReTrXMsLOF7BTUnvmiuYqIbqc9zgEcHON9qalEKtP4",
  "dcql_query": {
    "credentials": [
      {
        "id": "cred1",
        "format": "mso_mdoc",
        "meta": {
          "doctype_value": "org.iso.18013.5.1.mDL"
        },
        "claims": [
          {
            "path": [
              "org.iso.18013.5.1",
              "given_name"
            ]
          },
          {
            "path": [
              "org.iso.18013.5.1",
              "age_over_21"
            ]
          }
        ]
      }
    ]
  }
})");
  EXPECT_EQ(ComputeInterstitialType(kOpenid4vpProtocol, std::move(request)),
            InterstitialType::kLowRisk);
}

TEST_F(DigitalIdentityRequestImplInterstitialTest,
       Openid4VpAndPreviewProtocol_ComputeInterstitialType_AgeOver) {
  base::Value openid4vp_request = GenerateOnlyAgeOpenid4VpRequestWithDCQL();
  base::Value preview_request = GenerateOnlyAgePreviewRequest();

  DigitalCredentialGetRequestPtr request1 = DigitalCredentialGetRequest::New();
  request1->protocol = kOpenid4vpProtocol;
  request1->data = std::move(openid4vp_request);

  DigitalCredentialGetRequestPtr request2 = DigitalCredentialGetRequest::New();
  request2->protocol = kPreviewProtocol;
  request2->data = std::move(preview_request);

  std::vector<DigitalCredentialGetRequestPtr> requests;
  requests.emplace_back(std::move(request1));
  requests.emplace_back(std::move(request2));

  auto provider = std::make_unique<TestDigitalIdentityProviderWithCustomRisk>(
      /*are_origins_low_risk=*/false);
  EXPECT_EQ(DigitalIdentityRequestImpl::ComputeInterstitialType(
                *main_rfh(), provider.get(), std::move(requests)),
            std::nullopt);
}

TEST_F(
    DigitalIdentityRequestImplInterstitialTest,
    Openid4VpAndPreviewProtocol_ComputeInterstitialType_AgeOverAndGivenName) {
  base::Value openid4vp_request = GenerateOnlyAgeOpenid4VpRequestWithDCQL();
  base::Value preview_request = GenerateOnlyAgePreviewRequest();
  ASSERT_TRUE(SetFieldNameValue(preview_request, "given_name"));

  DigitalCredentialGetRequestPtr request1 = DigitalCredentialGetRequest::New();
  request1->protocol = kOpenid4vpProtocol;
  request1->data = std::move(openid4vp_request);

  DigitalCredentialGetRequestPtr request2 = DigitalCredentialGetRequest::New();
  request2->protocol = kPreviewProtocol;
  request2->data = std::move(preview_request);

  std::vector<DigitalCredentialGetRequestPtr> requests;
  requests.emplace_back(std::move(request1));
  requests.emplace_back(std::move(request2));

  auto provider = std::make_unique<TestDigitalIdentityProviderWithCustomRisk>(
      /*are_origins_low_risk=*/false);
  EXPECT_EQ(DigitalIdentityRequestImpl::ComputeInterstitialType(
                *main_rfh(), provider.get(), std::move(requests)),
            InterstitialType::kLowRisk);
}

TEST_F(DigitalIdentityRequestImplInterstitialTest,
       Openid4VpProtocolDCQL_ComputeInterstitialType_MalformedRequest) {
  // Malformed request that's missing the claim_name entry.
  base::Value malformed_request = ParseJsonAndCheck(R"({
  "response_type": "vp_token",
  "response_mode": "w3c_dc_api",
  "client_id": "web-origin:https://www.digital-credentials.dev",
  "nonce": "CL0BDiED_T5qDttEddJASo8Ft5yR9C0wmLy6WFtHsCQ",
  "dcql_query": {
    "credentials": [
      {
        "id": "cred1",
        "format": "mso_mdoc",
        "meta": {
          "doctype_value": "org.iso.18013.5.1.mDL"
        },
        "claims": [
          {
            "namespace": "org.iso.18013.5.1",
          },
        ]
      }
    ]
  }
})");
  EXPECT_EQ(
      ComputeInterstitialType(kOpenid4vpProtocol, std::move(malformed_request)),
      InterstitialType::kLowRisk);
}

TEST_F(DigitalIdentityRequestImplInterstitialTest,
       Openid4VpProtocolDCQL_ComputeInterstitialType_VerifyPhoneNumber) {
  EXPECT_EQ(
      ComputeInterstitialType(kOpenid4vpProtocol,
                              GenerateVerifyPhoneNumberOpenid4VpRequest()),
      std::nullopt);
}

TEST_F(DigitalIdentityRequestImplInterstitialTest,
       Openid4VpProtocolDCQL_ComputeInterstitialType_GetPhoneNumber) {
  EXPECT_EQ(ComputeInterstitialType(kOpenid4vpProtocol,
                                    GenerateGetPhoneNumberOpenid4VpRequest()),
            std::nullopt);
}

TEST_F(DigitalIdentityRequestImplInterstitialTest,
       Openid4VpProtocolSignedDCQL_ComputeInterstitialType_OnlyAgeOver) {
  EXPECT_EQ(
      ComputeInterstitialType(kOpenid4vpSignedProtocol,
                              GenerateSignedOnlyAgeOpenid4VpRequestWithDCQL()),
      std::nullopt);
}

TEST_F(DigitalIdentityRequestImplInterstitialTest,
       Openid4VpProtocolSignedDCQL_ComputeInterstitialType_GivenName) {
  EXPECT_EQ(ComputeInterstitialType(
                kOpenid4vpSignedProtocol,
                GenerateSignedGivenNameOpenid4VpRequestWithDCQL()),
            InterstitialType::kLowRisk);
}

TEST_F(
    DigitalIdentityRequestImplInterstitialTest,
    Openid4VpProtocolSignedDCQL_ComputeInterstitialType_GivenNameAndAgeOver) {
  EXPECT_EQ(ComputeInterstitialType(
                kOpenid4vpSignedProtocol,
                GenerateSignedGivenNameAndAgeOpenid4VpRequestWithDCQL()),
            InterstitialType::kLowRisk);
}

class DigitalIdentityRequestImplWithCreationEnabledTest
    : public RenderViewHostTestHarness {
 public:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    digital_identity_request_impl_ = DigitalIdentityRequestImpl::CreateInstance(
        *web_contents()->GetPrimaryMainFrame(),
        request_remote_.BindNewPipeAndPassReceiver());

    content::RenderFrameHostTester::For(web_contents()->GetPrimaryMainFrame())
        ->SimulateUserActivation();

    command_line_.GetProcessCommandLine()->AppendSwitch(
        switches::kUseFakeUIForDigitalIdentity);
  }

  void TearDown() override {
    // Reset here to avoid dangling pointer upon the destruction of the rvh.
    digital_identity_request_impl_ = nullptr;
    RenderViewHostTestHarness::TearDown();
  }

  DigitalIdentityRequestImpl* digital_identity_request_impl() {
    return digital_identity_request_impl_.get();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kWebIdentityDigitalCredentialsCreation};
  base::test::ScopedCommandLine command_line_;

  mojo::Remote<blink::mojom::DigitalIdentityRequest> request_remote_;
  base::WeakPtr<DigitalIdentityRequestImpl> digital_identity_request_impl_;
};

TEST_F(DigitalIdentityRequestImplWithCreationEnabledTest,
       ShouldReturnErrorWhenNoRequest) {
  base::MockCallback<DigitalIdentityRequestImpl::CreateCallback> callback;

  EXPECT_CALL(callback,
              Run(RequestDigitalIdentityStatus::kErrorNoRequests, _, _));
  digital_identity_request_impl()->Create({}, callback.Get());
}

TEST_F(DigitalIdentityRequestImplWithCreationEnabledTest,
       ShouldReturnErrorWhenAnotherRequestIsInFlight) {
  DigitalCredentialCreateRequestPtr digital_credential_request1 =
      DigitalCredentialCreateRequest::New();
  digital_credential_request1->protocol = "protocol1";
  base::Value::Dict request1_data;
  request1_data.Set("data", "request data 1");
  digital_credential_request1->data = base::Value(std::move(request1_data));

  DigitalCredentialCreateRequestPtr digital_credential_request2 =
      DigitalCredentialCreateRequest::New();
  digital_credential_request2->protocol = "protocol2";
  base::Value::Dict request2_data;
  request2_data.Set("data", "request data 2");
  digital_credential_request2->data = base::Value(std::move(request2_data));

  std::vector<blink::mojom::DigitalCredentialCreateRequestPtr> requests1;
  requests1.push_back(std::move(digital_credential_request1));
  digital_identity_request_impl()->Create(std::move(requests1),
                                          base::DoNothing());

  base::MockCallback<DigitalIdentityRequestImpl::CreateCallback> callback;
  EXPECT_CALL(callback,
              Run(RequestDigitalIdentityStatus::kErrorTooManyRequests, _, _));
  std::vector<blink::mojom::DigitalCredentialCreateRequestPtr> requests2;
  requests2.push_back(std::move(digital_credential_request2));
  digital_identity_request_impl()->Create(std::move(requests2), callback.Get());
}

TEST_F(DigitalIdentityRequestImplWithCreationEnabledTest,
       ShouldSucceedWhenValidRequest) {
  const std::string kProtocol = "protocol";
  base::MockCallback<DigitalIdentityRequestImpl::CreateCallback> callback;
  DigitalCredentialCreateRequestPtr digital_credential_request =
      DigitalCredentialCreateRequest::New();
  digital_credential_request->protocol = kProtocol;
  base::Value::Dict request_data;
  request_data.Set("data", "request data");
  digital_credential_request->data = base::Value(std::move(request_data));

  std::vector<blink::mojom::DigitalCredentialCreateRequestPtr> requests;
  requests.push_back(std::move(digital_credential_request));

  base::RunLoop run_loop;
  EXPECT_CALL(callback, Run(RequestDigitalIdentityStatus::kSuccess,
                            testing::Optional(kProtocol), _))
      .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));
  digital_identity_request_impl()->Create(std::move(requests), callback.Get());
  run_loop.Run();
}

TEST_F(DigitalIdentityRequestImplWithCreationEnabledTest,
       ShouldReturnErrorWhenAbort) {
  const std::string kProtocol = "protocol";
  base::MockCallback<DigitalIdentityRequestImpl::CreateCallback> callback;
  DigitalCredentialCreateRequestPtr digital_credential_request =
      DigitalCredentialCreateRequest::New();
  digital_credential_request->protocol = kProtocol;
  base::Value::Dict request_data;
  request_data.Set("data", "request data");
  digital_credential_request->data = base::Value(std::move(request_data));

  std::vector<blink::mojom::DigitalCredentialCreateRequestPtr> requests;
  requests.push_back(std::move(digital_credential_request));

  EXPECT_CALL(callback,
              Run(RequestDigitalIdentityStatus::kErrorCanceled, _, _));
  digital_identity_request_impl()->Create(std::move(requests), callback.Get());
  digital_identity_request_impl()->Abort();
}

class ContentBrowserClientWithMockDigitalIdentityProvider
    : public ContentBrowserClient {
 public:
  ContentBrowserClientWithMockDigitalIdentityProvider() = default;
  ~ContentBrowserClientWithMockDigitalIdentityProvider() override = default;

  // ContentBrowserClient overrides:
  std::unique_ptr<DigitalIdentityProvider> CreateDigitalIdentityProvider()
      override {
    return std::move(provider_);
  }

  void SetDigitalIdentityProvider(
      std::unique_ptr<DigitalIdentityProvider> provider) {
    provider_ = std::move(provider);
  }

 private:
  std::unique_ptr<DigitalIdentityProvider> provider_;
};

class DigitalIdentityRequestImplTest : public RenderViewHostTestHarness {
 public:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    auto mock_digital_identity_provider =
        std::make_unique<MockDigitalIdentityProvider>();
    mock_digital_identity_provider_ = mock_digital_identity_provider.get();
    content_browser_client_.SetDigitalIdentityProvider(
        std::move(mock_digital_identity_provider));
    content::SetBrowserClientForTesting(&content_browser_client_);

    digital_identity_request_impl_ = DigitalIdentityRequestImpl::CreateInstance(
        *web_contents()->GetPrimaryMainFrame(),
        request_remote_.BindNewPipeAndPassReceiver());

    content::RenderFrameHostTester::For(web_contents()->GetPrimaryMainFrame())
        ->SimulateUserActivation();

    // Tests in this fixture don't test the dialog behaviour.
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kWebIdentityDigitalCredentials, {{"dialog", "no_dialog"}});
  }

  void TearDown() override {
    // Reset here to avoid dangling pointer upon the destruction of the rvh.
    digital_identity_request_impl_ = nullptr;
    mock_digital_identity_provider_ = nullptr;
    RenderViewHostTestHarness::TearDown();
  }

  DigitalIdentityRequestImpl* digital_identity_request_impl() {
    return digital_identity_request_impl_.get();
  }

  MockDigitalIdentityProvider* mock_digital_identity_provider() {
    return mock_digital_identity_provider_;
  }

  void reset_provider_pointer() { mock_digital_identity_provider_ = nullptr; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  // base::test::ScopedCommandLine command_line_;

  raw_ptr<MockDigitalIdentityProvider> mock_digital_identity_provider_;
  ContentBrowserClientWithMockDigitalIdentityProvider content_browser_client_;

  mojo::Remote<blink::mojom::DigitalIdentityRequest> request_remote_;
  base::WeakPtr<DigitalIdentityRequestImpl> digital_identity_request_impl_;
};

TEST_F(DigitalIdentityRequestImplTest, ShouldGetWithProperFormatting) {
  const std::string kProtocol = "protocol";

  DigitalCredentialGetRequestPtr digital_credential_request =
      DigitalCredentialGetRequest::New();
  digital_credential_request->protocol = kProtocol;
  base::Value::Dict request_data;
  request_data.Set("data", "request data");
  digital_credential_request->data = base::Value(std::move(request_data));

  std::vector<DigitalCredentialGetRequestPtr> requests;
  requests.push_back(std::move(digital_credential_request));

  base::RunLoop run_loop;
  // Intercept the `Get()` call and verify that the request is formatted
  // properly.
  EXPECT_CALL(*mock_digital_identity_provider(), Get)
      .WillOnce(DoAll(WithArg<2>([](ValueView request) {
                        Value::Dict dict = request.ToValue().GetDict().Clone();
                        EXPECT_TRUE(dict.contains("requests"));
                        for (const Value& req : *dict.FindList("requests")) {
                          EXPECT_TRUE(req.GetDict().contains("protocol"));
                          EXPECT_TRUE(req.GetDict().contains("data"));
                          EXPECT_TRUE(req.GetDict().Find("data")->is_dict());
                        }
                      }),
                      base::test::RunOnceClosure(run_loop.QuitClosure())));
  digital_identity_request_impl()->Get(std::move(requests), base::DoNothing());
  run_loop.Run();
}

TEST_F(DigitalIdentityRequestImplTest, ShouldGetAndReturnProtocolInRequest) {
  const std::string kProtocol = "protocol";
  const Value kResponseData(Value::Dict().Set("token", "token data"));

  DigitalCredentialGetRequestPtr digital_credential_request =
      DigitalCredentialGetRequest::New();
  digital_credential_request->protocol = kProtocol;
  base::Value::Dict request_data;
  request_data.Set("data", "request data");
  digital_credential_request->data = base::Value(std::move(request_data));

  std::vector<DigitalCredentialGetRequestPtr> requests;
  requests.push_back(std::move(digital_credential_request));

  base::RunLoop run_loop;

  // Simulate a provider that returns a response without a protocol.
  EXPECT_CALL(*mock_digital_identity_provider(), Get)
      .WillOnce(
          WithArg<3>([this, &kResponseData](DigitalIdentityCallback callback) {
            // Running the `callback` will destroy the provider, reset the
            // pointer to avoid dangling pointers after invoking the callback.
            reset_provider_pointer();

            std::move(callback).Run(
                DigitalCredential(std::nullopt, kResponseData.Clone()));
          }));

  base::MockCallback<GetCallback> mock_callback;
  // The protocol in the request should be used when invoking the callback,
  // since no protocol was available in the response.
  EXPECT_CALL(mock_callback, Run(RequestDigitalIdentityStatus::kSuccess,
                                 Optional(kProtocol), _))
      .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));

  digital_identity_request_impl()->Get(std::move(requests),
                                       mock_callback.Get());

  run_loop.Run();
}

TEST_F(DigitalIdentityRequestImplTest, ShouldGetAndReturnProtocolInResponse) {
  const std::string kProtocolInRequest = "protocol_in_request";
  const std::string kProtocolInResponse = "protocol_in_response";
  const Value kResponseData(Value::Dict().Set("token", "token data"));

  DigitalCredentialGetRequestPtr digital_credential_request =
      DigitalCredentialGetRequest::New();
  digital_credential_request->protocol = kProtocolInRequest;
  base::Value::Dict request_data;
  request_data.Set("data", "request data");
  digital_credential_request->data = base::Value(std::move(request_data));

  std::vector<DigitalCredentialGetRequestPtr> requests;
  requests.push_back(std::move(digital_credential_request));

  base::RunLoop run_loop;

  // Simulate a provider that returns a response with a protocol.
  EXPECT_CALL(*mock_digital_identity_provider(), Get)
      .WillOnce(WithArg<3>([this, &kProtocolInResponse,
                            &kResponseData](DigitalIdentityCallback callback) {
        // Running the `callback` will destroy the provider, reset the pointer
        // to avoid dangling pointers after invoking the callback.
        reset_provider_pointer();

        std::move(callback).Run(
            DigitalCredential(kProtocolInResponse, kResponseData.Clone()));
      }));

  base::MockCallback<GetCallback> mock_callback;
  // The protocol in the response should be used when invoking the callback.
  EXPECT_CALL(mock_callback, Run(RequestDigitalIdentityStatus::kSuccess,
                                 Optional(kProtocolInResponse), _))
      .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));

  digital_identity_request_impl()->Get(std::move(requests),
                                       mock_callback.Get());

  run_loop.Run();
}

TEST_F(DigitalIdentityRequestImplTest,
       ShouldGetWhenMultipleRequestsAndReturnProtocolInResponse) {
  const Value kResponseData(Value::Dict().Set("token", "token data"));
  const std::string kProtocolInResponse = "protocol1";
  std::vector<DigitalCredentialGetRequestPtr> requests;

  DigitalCredentialGetRequestPtr request1 = DigitalCredentialGetRequest::New();
  request1->protocol = "protocol1";
  base::Value::Dict request1_data;
  request1_data.Set("data", "request1 data");
  request1->data = base::Value(std::move(request1_data));

  DigitalCredentialGetRequestPtr request2 = DigitalCredentialGetRequest::New();
  request2->protocol = "protocol2";
  base::Value::Dict request2_data;
  request2_data.Set("data", "request2 data");
  request2->data = base::Value(std::move(request2_data));

  requests.push_back(std::move(request1));
  requests.push_back(std::move(request2));

  base::RunLoop run_loop;

  // Simulate a provider that returns a response without a protocol.
  EXPECT_CALL(*mock_digital_identity_provider(), Get)
      .WillOnce(WithArg<3>([this, &kProtocolInResponse,
                            &kResponseData](DigitalIdentityCallback callback) {
        // Running the `callback` will destroy the provider, reset the
        // pointer to avoid dangling pointers after invoking the callback.
        reset_provider_pointer();

        std::move(callback).Run(
            DigitalCredential(kProtocolInResponse, kResponseData.Clone()));
      }));

  base::MockCallback<GetCallback> mock_callback;
  // The protocol in the response should be used when invoking the callback.
  EXPECT_CALL(mock_callback, Run(RequestDigitalIdentityStatus::kSuccess,
                                 Optional(kProtocolInResponse), _))
      .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));

  digital_identity_request_impl()->Get(std::move(requests),
                                       mock_callback.Get());

  run_loop.Run();
}

TEST_F(DigitalIdentityRequestImplTest,
       ShouldErrorWhenMultipleRequestsAndNoProtocolInResponse) {
  const Value kResponseData(Value::Dict().Set("token", "token data"));

  std::vector<DigitalCredentialGetRequestPtr> requests;

  DigitalCredentialGetRequestPtr request1 = DigitalCredentialGetRequest::New();
  request1->protocol = "protocol1";
  base::Value::Dict request1_data;
  request1_data.Set("data", "request1 data");
  request1->data = base::Value(std::move(request1_data));

  DigitalCredentialGetRequestPtr request2 = DigitalCredentialGetRequest::New();
  request2->protocol = "protocol2";
  base::Value::Dict request2_data;
  request2_data.Set("data", "request2 data");
  request2->data = base::Value(std::move(request2_data));

  requests.push_back(std::move(request1));
  requests.push_back(std::move(request2));

  base::RunLoop run_loop;

  // Simulate a provider that returns a response without a protocol.
  EXPECT_CALL(*mock_digital_identity_provider(), Get)
      .WillOnce(
          WithArg<3>([this, &kResponseData](DigitalIdentityCallback callback) {
            // Running the `callback` will destroy the provider, reset the
            // pointer to avoid dangling pointers after invoking the callback.
            reset_provider_pointer();

            std::move(callback).Run(DigitalCredential(
                /*protocol=*/std::nullopt, kResponseData.Clone()));
          }));

  base::MockCallback<GetCallback> mock_callback;
  // The callback should be invoked with an error since the digital wallet
  // response indicates that it doesn't support multiple requests.
  EXPECT_CALL(mock_callback, Run(RequestDigitalIdentityStatus::kError, _, _))
      .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));

  digital_identity_request_impl()->Get(std::move(requests),
                                       mock_callback.Get());

  run_loop.Run();
}

}  // namespace content
