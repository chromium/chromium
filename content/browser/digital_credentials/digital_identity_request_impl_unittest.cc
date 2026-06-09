// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/digital_credentials/digital_identity_request_impl.h"

#include <optional>

#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "content/browser/digital_credentials/digital_credential_environment.h"
#include "content/browser/digital_credentials/virtual_wallet.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/webid/test/mock_digital_identity_provider.h"
#include "content/browser/webid/test/stub_digital_identity_provider.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/digital_identity_provider.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/navigation_simulator.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/network/public/cpp/permissions_policy/permissions_policy_declaration.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/webid/digital_identity_request.mojom.h"

namespace content {
namespace {

constexpr char kOpenid4vpProtocol[] = "openid4vp";
constexpr char kOpenid4vpSignedProtocol[] = "openid4vp-v1-signed";
constexpr char kOpenid4vpUnsignedProtocol[] = "openid4vp-v1-unsigned";

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
using RequestStatusForMetrics =
    DigitalIdentityProvider::RequestStatusForMetrics;
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

base::Value GenerateDpcExample1() {
  constexpr char kJson[] = R"({
    "client_metadata": {
      "vp_formats_supported": {
        "dc+sd-jwt": {
          "sd-jwt_alg_values": [
            "ES256"
          ]
        }
      }
    },
    "dcql_query": {
      "credentials": [
        {
          "format": "dc+sd-jwt",
          "id": "cred1",
          "meta": {
            "vct_values": [
              "com.emvco.dpc"
            ]
          }
        }
      ]
    },
    "nonce": "VICJxZXdOrXSygevOEOe7Fwsh3u5PSIuEGUB_z4-1iE",
    "response_mode": "dc_api",
    "response_type": "vp_token",
    "transaction_data": [
      "eyJ0eXBlIjogInVybjpldWRpOnNjYTpwYXltZW50OjEiLCAiY3JlZGVudGlhbF9pZHMiOiBbImNyZWQxIl0sICJ0cmFuc2FjdGlvbl9kYXRhX2hhc2hlc19hbGciOiBbInNoYS0yNTYiXSwgInBheWxvYWQiOiB7ImFtb3VudCI6IDEyLjUsICJjdXJyZW5jeSI6ICJVU0QiLCAicGF5ZWUiOiB7Im5hbWUiOiAiUm9jayBMZWdlbmRzIiwgImlkIjogIlBheWVlLWlkLTEyMyJ9LCAidHJhbnNhY3Rpb25faWQiOiAiMTIzNDU2Nzg5MCJ9fQ=="
    ]
  })";
  return ParseJsonAndCheck(kJson);
}

base::Value GenerateDpcExample2() {
  constexpr char kJson[] = R"({
    "client_metadata": {
      "vp_formats_supported": {
        "dc+sd-jwt": {
          "sd-jwt_alg_values": [
            "ES256"
          ]
        }
      }
    },
    "dcql_query": {
      "credentials": [
        {
          "format": "dc+sd-jwt",
          "id": "cred1",
          "meta": {
            "vct_values": [
              "dpc.cred.card"
            ]
          }
        }
      ]
    },
    "nonce": "Q3l91mQygPFDirjtQyvKbZ_K9MDJhr0e_gkBTYLmVv0",
    "response_mode": "dc_api",
    "response_type": "vp_token",
    "transaction_data": [
      "eyJ0eXBlIjogInVybjpldWRpOnNjYTpwYXltZW50OjEiLCAiY3JlZGVudGlhbF9pZHMiOiBbImNyZWQxIl0sICJ0cmFuc2FjdGlvbl9kYXRhX2hhc2hlc19hbGciOiBbInNoYS0yNTYiXSwgInBheWxvYWQiOiB7ImFtb3VudCI6IDEyLjUsICJjdXJyZW5jeSI6ICJVU0QiLCAicGF5ZWUiOiB7Im5hbWUiOiAiUm9jayBMZWdlbmRzIiwgImlkIjogIlBheWVlLWlkLTEyMyJ9LCAidHJhbnNhY3Rpb25faWQiOiAiMTIzNDU2Nzg5MCJ9fQ=="
    ]
  })";
  return ParseJsonAndCheck(kJson);
}

base::Value GenerateDpcExample3() {
  constexpr char kJson[] = R"({
    "client_metadata": {
      "vp_formats_supported": {
        "mso_mdoc": {
          "deviceauth_alg_values": [
            -7
          ],
          "issuerauth_alg_values": [
            -7
          ]
        }
      }
    },
    "dcql_query": {
      "credentials": [
        {
          "format": "mso_mdoc",
          "id": "cred1",
          "meta": {
            "doctype_value": "com.emvco.dpc"
          }
        }
      ]
    },
    "nonce": "JCK8iJuwBQyish5jmcCYIxNBT76ZYfVZliKzrcZMCTw",
    "response_mode": "dc_api",
    "response_type": "vp_token",
    "transaction_data": [
      "eyJ0eXBlIjogInVybjpldWRpOnNjYTpwYXltZW50OjEiLCAiY3JlZGVudGlhbF9pZHMiOiBbImNyZWQxIl0sICJ0cmFuc2FjdGlvbl9kYXRhX2hhc2hlc19hbGciOiBbInNoYS0yNTYiXSwgInBheWxvYWQiOiB7ImFtb3VudCI6IDEyLjUsICJjdXJyZW5jeSI6ICJVU0QiLCAicGF5ZWUiOiB7Im5hbWUiOiAiUm9jayBMZWdlbmRzIiwgImlkIjogIlBheWVlLWlkLTEyMyJ9LCAidHJhbnNhY3Rpb25faWQiOiAiMTIzNDU2Nzg5MCJ9fQ=="
    ]
  })";
  return ParseJsonAndCheck(kJson);
}

base::Value GenerateDpcExample1WithOtherData() {
  constexpr char kJson[] = R"({
    "client_metadata": {
      "vp_formats_supported": {
        "dc+sd-jwt": {
          "sd-jwt_alg_values": [
            "ES256"
          ]
        }
      }
    },
    "dcql_query": {
      "credentials": [
        {
          "format": "dc+sd-jwt",
          "id": "cred1",
          "meta": {
            "vct_values": [
              "com.emvco.dpc"
            ]
          },
          "claims": [
            {
              "path": [
                "given_name"
              ]
            }
          ]
        }
      ]
    },
    "nonce": "VICJxZXdOrXSygevOEOe7Fwsh3u5PSIuEGUB_z4-1iE",
    "response_mode": "dc_api",
    "response_type": "vp_token",
    "transaction_data": [
      "eyJ0eXBlIjogInVybjpldWRpOnNjYTpwYXltZW50OjEiLCAiY3JlZGVudGlhbF9pZHMiOiBbImNyZWQxIl0sICJ0cmFuc2FjdGlvbl9kYXRhX2hhc2hlc19hbGciOiBbInNoYS0yNTYiXSwgInBheWxvYWQiOiB7ImFtb3VudCI6IDEyLjUsICJjdXJyZW5jeSI6ICJVU0QiLCAicGF5ZWUiOiB7Im5hbWUiOiAiUm9jayBMZWdlbmRzIiwgImlkIjogIlBheWVlLWlkLTEyMyJ9LCAidHJhbnNhY3Rpb25faWQiOiAiMTIzNDU2Nzg5MCJ9fQ=="
    ]
  })";
  return ParseJsonAndCheck(kJson);
}

base::Value GenerateSdJwtAgeOnly() {
  constexpr char kJson[] = R"({
    "dcql_query": {
      "credentials": [
        {
          "id": "cred1",
          "format": "dc+sd-jwt",
          "meta": {
            "vct_values": [
              "eu.europa.ec.eudiw.pid.1"
            ]
          },
          "claims": [
            {
              "path": [
                "age_over_18"
              ]
            }
          ]
        }
      ]
    }
  })";
  return ParseJsonAndCheck(kJson);
}

base::Value GenerateSdJwtAgeAndSensitive() {
  constexpr char kJson[] = R"({
    "dcql_query": {
      "credentials": [
        {
          "id": "cred1",
          "format": "dc+sd-jwt",
          "meta": {
            "vct_values": [
              "eu.europa.ec.eudiw.pid.1"
            ]
          },
          "claims": [
            {
              "path": [
                "age_over_18"
              ]
            },
            {
              "path": [
                "given_name"
              ]
            }
          ]
        }
      ]
    }
  })";
  return ParseJsonAndCheck(kJson);
}

base::Value GenerateSdJwtAgeBundledWithDpc() {
  constexpr char kJson[] = R"({
    "dcql_query": {
      "credentials": [
        {
          "id": "dpc_token",
          "format": "dc+sd-jwt",
          "meta": {
            "vct_values": [
              "com.emvco.dpc"
            ]
          }
        },
        {
          "id": "eudi_pid",
          "format": "dc+sd-jwt",
          "meta": {
            "vct_values": [
              "eu.europa.ec.eudiw.pid.1"
            ]
          },
          "claims": [
            {
              "path": [
                "age_over_18"
              ]
            }
          ]
        }
      ]
    }
  })";
  return ParseJsonAndCheck(kJson);
}

base::Value GenerateSdJwtSensitiveBundledWithDpc() {
  constexpr char kJson[] = R"({
    "dcql_query": {
      "credentials": [
        {
          "id": "dpc_token",
          "format": "dc+sd-jwt",
          "meta": {
            "vct_values": [
              "com.emvco.dpc"
            ]
          }
        },
        {
          "id": "eudi_pid",
          "format": "dc+sd-jwt",
          "meta": {
            "vct_values": [
              "eu.europa.ec.eudiw.pid.1"
            ]
          },
          "claims": [
            {
              "path": [
                "given_name"
              ]
            }
          ]
        }
      ]
    }
  })";
  return ParseJsonAndCheck(kJson);
}

base::Value GenerateOpenid4VpRequestWithEmptyClaims() {
  constexpr char kJson[] = R"({
  "response_type": "vp_token",
  "response_mode": "dc_api",
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
        "claims": []
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
    base::ListValue& list = root.GetList();
    for (base::Value& list_item : list) {
      if (base::Value* out = FindValueWithKey(list_item, find_key)) {
        return out;
      }
    }
    return nullptr;
  }

  if (root.is_dict()) {
    base::DictValue& dict = root.GetDict();
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

TEST_F(DigitalIdentityRequestImplInterstitialTest,
       Openid4VpProtocolDCQL_ComputeInterstitialType_OnlyAgeOver) {
  EXPECT_EQ(ComputeInterstitialType(kOpenid4vpProtocol,
                                    GenerateOnlyAgeOpenid4VpRequestWithDCQL()),
            std::nullopt);
}

TEST_F(DigitalIdentityRequestImplInterstitialTest,
       Openid4VpProtocolDCQL_ComputeInterstitialType_WrongFormat) {
  base::Value request = GenerateOnlyAgeOpenid4VpRequestWithDCQL();
  base::Value* credentials = FindValueWithKey(request, "credentials");
  ASSERT_TRUE(IsNonEmptyList(credentials));
  credentials->GetList().front().GetDict().Set("format", "invalid_format");

  EXPECT_EQ(ComputeInterstitialType(kOpenid4vpProtocol, std::move(request)),
            InterstitialType::kLowRisk);
}

TEST_F(DigitalIdentityRequestImplInterstitialTest,
       Openid4VpProtocolDCQL_ComputeInterstitialType_WrongDocType) {
  base::Value request = GenerateOnlyAgeOpenid4VpRequestWithDCQL();
  base::Value* credentials = FindValueWithKey(request, "credentials");
  ASSERT_TRUE(IsNonEmptyList(credentials));
  base::Value* meta = FindValueWithKey(credentials->GetList().front(), "meta");
  ASSERT_TRUE(meta && meta->is_dict());
  meta->GetDict().Set("doctype_value", "invalid_doctype");

  EXPECT_EQ(ComputeInterstitialType(kOpenid4vpProtocol, std::move(request)),
            InterstitialType::kLowRisk);
}

TEST_F(DigitalIdentityRequestImplInterstitialTest,
       Openid4VpProtocolDCQL_ComputeInterstitialType_WrongNamespace) {
  base::Value request = GenerateOnlyAgeOpenid4VpRequestWithDCQL();
  base::Value* credentials = FindValueWithKey(request, "credentials");
  ASSERT_TRUE(IsNonEmptyList(credentials));
  base::Value* claims =
      FindValueWithKey(credentials->GetList().front(), "claims");
  ASSERT_TRUE(IsNonEmptyList(claims));
  base::Value* paths = FindValueWithKey(claims->GetList().front(), "path");
  ASSERT_TRUE(IsNonEmptyList(paths));
  paths->GetList().front() = base::Value("invalid_namespace");

  EXPECT_EQ(ComputeInterstitialType(kOpenid4vpProtocol, std::move(request)),
            InterstitialType::kLowRisk);
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
       Openid4VpProtocolDCQL_ComputeInterstitialType_EmptyPathList) {
  base::Value request = GenerateOnlyAgeOpenid4VpRequestWithDCQL();
  base::Value* paths = FindValueWithKey(request, "path");
  ASSERT_TRUE(IsNonEmptyList(paths));
  paths->GetList().resize(0);

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
       Openid4VpProtocolDCQL_ComputeInterstitialType_DpcExample1) {
  EXPECT_EQ(ComputeInterstitialType(kOpenid4vpProtocol, GenerateDpcExample1()),
            std::nullopt);
}

TEST_F(DigitalIdentityRequestImplInterstitialTest,
       Openid4VpProtocolDCQL_ComputeInterstitialType_DpcExample2) {
  EXPECT_EQ(ComputeInterstitialType(kOpenid4vpProtocol, GenerateDpcExample2()),
            std::nullopt);
}

TEST_F(DigitalIdentityRequestImplInterstitialTest,
       Openid4VpProtocolDCQL_ComputeInterstitialType_DpcExample3) {
  EXPECT_EQ(ComputeInterstitialType(kOpenid4vpProtocol, GenerateDpcExample3()),
            std::nullopt);
}

TEST_F(DigitalIdentityRequestImplInterstitialTest,
       Openid4VpProtocolDCQL_ComputeInterstitialType_DpcExample1WithOtherData) {
  EXPECT_EQ(ComputeInterstitialType(kOpenid4vpProtocol,
                                    GenerateDpcExample1WithOtherData()),
            InterstitialType::kLowRisk);
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

TEST_F(DigitalIdentityRequestImplInterstitialTest,
       Openid4VpProtocolDCQL_ComputeInterstitialType_SdJwtAgeOnly) {
  EXPECT_EQ(ComputeInterstitialType(kOpenid4vpProtocol, GenerateSdJwtAgeOnly()),
            std::nullopt);
}

TEST_F(DigitalIdentityRequestImplInterstitialTest,
       Openid4VpProtocolDCQL_ComputeInterstitialType_EmptyClaims) {
  EXPECT_EQ(ComputeInterstitialType(kOpenid4vpProtocol,
                                    GenerateOpenid4VpRequestWithEmptyClaims()),
            InterstitialType::kLowRisk);
}

TEST_F(DigitalIdentityRequestImplInterstitialTest,
       Openid4VpProtocolDCQL_ComputeInterstitialType_SdJwtAgeAndSensitive) {
  EXPECT_EQ(ComputeInterstitialType(kOpenid4vpProtocol,
                                    GenerateSdJwtAgeAndSensitive()),
            InterstitialType::kLowRisk);
}

TEST_F(DigitalIdentityRequestImplInterstitialTest,
       Openid4VpProtocolDCQL_ComputeInterstitialType_SdJwtAgeBundledWithDpc) {
  EXPECT_EQ(ComputeInterstitialType(kOpenid4vpProtocol,
                                    GenerateSdJwtAgeBundledWithDpc()),
            std::nullopt);
}

TEST_F(
    DigitalIdentityRequestImplInterstitialTest,
    Openid4VpProtocolDCQL_ComputeInterstitialType_SdJwtSensitiveBundledWithDpc) {
  EXPECT_EQ(ComputeInterstitialType(kOpenid4vpProtocol,
                                    GenerateSdJwtSensitiveBundledWithDpc()),
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
  base::DictValue request1_data;
  request1_data.Set("data", "request data 1");
  digital_credential_request1->data = base::Value(std::move(request1_data));

  DigitalCredentialCreateRequestPtr digital_credential_request2 =
      DigitalCredentialCreateRequest::New();
  digital_credential_request2->protocol = "protocol2";
  base::DictValue request2_data;
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
  base::DictValue request_data;
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
  base::DictValue request_data;
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

    // Tests in this fixture don't test the dialog behavior.
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

  void SetMockDigitalIdentityProvider(
      std::unique_ptr<MockDigitalIdentityProvider> provider) {
    mock_digital_identity_provider_ = provider.get();
    content_browser_client_.SetDigitalIdentityProvider(std::move(provider));
  }

  void RecreateService() {
    request_remote_.reset();
    digital_identity_request_impl_ = DigitalIdentityRequestImpl::CreateInstance(
        *web_contents()->GetPrimaryMainFrame(),
        request_remote_.BindNewPipeAndPassReceiver());
  }

  void reset_provider_pointer() { mock_digital_identity_provider_ = nullptr; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

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
  base::DictValue request_data;
  request_data.Set("data", "request data");
  digital_credential_request->data = base::Value(std::move(request_data));

  std::vector<DigitalCredentialGetRequestPtr> requests;
  requests.push_back(std::move(digital_credential_request));

  base::RunLoop run_loop;
  // Intercept the `Get()` call and verify that the request is formatted
  // properly.
  EXPECT_CALL(*mock_digital_identity_provider(), Get)
      .WillOnce(
          DoAll(WithArg<2>([](ValueView request) {
                  base::DictValue dict = request.ToValue().GetDict().Clone();
                  EXPECT_TRUE(dict.contains("requests"));
                  for (const base::Value& req : *dict.FindList("requests")) {
                    EXPECT_TRUE(req.GetDict().contains("protocol"));
                    EXPECT_TRUE(req.GetDict().contains("data"));
                    EXPECT_TRUE(req.GetDict().Find("data")->is_dict());
                  }
                }),
                base::test::RunOnceClosure(run_loop.QuitClosure())));
  digital_identity_request_impl()->Get(std::move(requests), base::DoNothing());
  run_loop.Run();
}

TEST_F(DigitalIdentityRequestImplTest, ShouldGetAndReturnProtocolInResponse) {
  const std::string kProtocolInRequest = "protocol_in_request";
  const std::string kProtocolInResponse = "protocol_in_response";
  const base::Value kResponseData(base::DictValue().Set("token", "token data"));

  DigitalCredentialGetRequestPtr digital_credential_request =
      DigitalCredentialGetRequest::New();
  digital_credential_request->protocol = kProtocolInRequest;
  base::DictValue request_data;
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
  const base::Value kResponseData(base::DictValue().Set("token", "token data"));
  const std::string kProtocolInResponse = "protocol1";
  std::vector<DigitalCredentialGetRequestPtr> requests;

  DigitalCredentialGetRequestPtr request1 = DigitalCredentialGetRequest::New();
  request1->protocol = "protocol1";
  base::DictValue request1_data;
  request1_data.Set("data", "request1 data");
  request1->data = base::Value(std::move(request1_data));

  DigitalCredentialGetRequestPtr request2 = DigitalCredentialGetRequest::New();
  request2->protocol = "protocol2";
  base::DictValue request2_data;
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
       ShouldReturnUserDeclinedWhenNoCredential) {
  const std::string kProtocol = "protocol";
  DigitalCredentialGetRequestPtr digital_credential_request =
      DigitalCredentialGetRequest::New();
  digital_credential_request->protocol = kProtocol;
  base::DictValue request_data;
  request_data.Set("data", "request data");
  digital_credential_request->data = base::Value(std::move(request_data));

  std::vector<DigitalCredentialGetRequestPtr> requests;
  requests.push_back(std::move(digital_credential_request));

  base::RunLoop run_loop;

  EXPECT_CALL(*mock_digital_identity_provider(), Get)
      .WillOnce(WithArg<3>([this](DigitalIdentityCallback callback) {
        // Running the `callback` will destroy the provider, reset the
        // pointer to avoid dangling pointers after invoking the callback.
        reset_provider_pointer();

        std::move(callback).Run(
            base::unexpected(RequestStatusForMetrics::kErrorNoCredential));
      }));

  base::MockCallback<GetCallback> mock_callback;
  EXPECT_CALL(mock_callback,
              Run(RequestDigitalIdentityStatus::kErrorUserDeclined,
                  testing::Eq(std::nullopt), _))
      .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));

  digital_identity_request_impl()->Get(std::move(requests),
                                       mock_callback.Get());

  run_loop.Run();
}

TEST_F(DigitalIdentityRequestImplTest,
       ShouldReturnUserDeclinedWhenUserDeclined) {
  const std::string kProtocol = "protocol";
  DigitalCredentialGetRequestPtr digital_credential_request =
      DigitalCredentialGetRequest::New();
  digital_credential_request->protocol = kProtocol;
  base::DictValue request_data;
  request_data.Set("data", "request data");
  digital_credential_request->data = base::Value(std::move(request_data));

  std::vector<DigitalCredentialGetRequestPtr> requests;
  requests.push_back(std::move(digital_credential_request));

  base::RunLoop run_loop;

  EXPECT_CALL(*mock_digital_identity_provider(), Get)
      .WillOnce(WithArg<3>([this](DigitalIdentityCallback callback) {
        // Running the `callback` will destroy the provider, reset the
        // pointer to avoid dangling pointers after invoking the callback.
        reset_provider_pointer();

        std::move(callback).Run(
            base::unexpected(RequestStatusForMetrics::kErrorUserDeclined));
      }));

  base::MockCallback<GetCallback> mock_callback;
  EXPECT_CALL(mock_callback,
              Run(RequestDigitalIdentityStatus::kErrorUserDeclined,
                  testing::Eq(std::nullopt), _))
      .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));

  digital_identity_request_impl()->Get(std::move(requests),
                                       mock_callback.Get());

  run_loop.Run();
}

// A DigitalIdentityProvider whose destructor runs an arbitrary closure. This
// models the production behavior where ~DigitalIdentityProviderDesktop tears
// down a views::Widget, which can synchronously fire activation/visibility
// observers. If one of those observers destroys the hosting WebContents (e.g. a
// transient bubble that closes on focus loss), DigitalIdentityRequestImpl is
// synchronously deleted while CompleteRequestWithStatus() is still on the
// stack.
class WebContentsDestroyingProvider : public StubDigitalIdentityProvider {
 public:
  explicit WebContentsDestroyingProvider(base::OnceClosure on_destroy)
      : on_destroy_(std::move(on_destroy)) {}
  ~WebContentsDestroyingProvider() override {
    if (on_destroy_) {
      std::move(on_destroy_).Run();
    }
  }

  // Keep the request pending; the test drives completion via Abort().
  void Get(WebContents*,
           const url::Origin&,
           base::ValueView,
           DigitalIdentityCallback) override {}

 private:
  base::OnceClosure on_destroy_;
};

class WebContentsDestroyingBrowserClient : public ContentBrowserClient {
 public:
  explicit WebContentsDestroyingBrowserClient(base::OnceClosure on_destroy)
      : on_destroy_(std::move(on_destroy)) {}
  ~WebContentsDestroyingBrowserClient() override = default;

  std::unique_ptr<DigitalIdentityProvider> CreateDigitalIdentityProvider()
      override {
    return std::make_unique<WebContentsDestroyingProvider>(
        std::move(on_destroy_));
  }

 private:
  base::OnceClosure on_destroy_;
};

class DigitalIdentityRequestImplProviderResetTest
    : public RenderViewHostTestHarness {
 public:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    // Skip the interstitial so provider_->Get() is reached synchronously.
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kWebIdentityDigitalCredentials, {{"dialog", "no_dialog"}});

    browser_client_ =
        std::make_unique<WebContentsDestroyingBrowserClient>(base::BindOnce(
            &DigitalIdentityRequestImplProviderResetTest::DeleteContents,
            base::Unretained(this)));
    old_client_ = content::SetBrowserClientForTesting(browser_client_.get());
  }

  void TearDown() override {
    content::SetBrowserClientForTesting(old_client_);
    RenderViewHostTestHarness::TearDown();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<WebContentsDestroyingBrowserClient> browser_client_;
  raw_ptr<ContentBrowserClient> old_client_ = nullptr;
};

// Regression test for a potential use-after-free in
// DigitalIdentityRequestImpl::CompleteRequestWithStatus(). If destroying the
// DigitalIdentityProvider synchronously destroys the WebContents that hosts the
// requesting frame, the DocumentService base class will `delete this` while
// CompleteRequestWithStatus() is still executing, and the subsequent accesses
// to `update_interstitial_on_abort_callback_` and `callback_` are
// use-after-free.
//
// This test models that destruction directly: the provider's destructor
// synchronously deletes the harness WebContents. Under ASAN this surfaces as a
// heap-use-after-free inside CompleteRequestWithStatus().
TEST_F(DigitalIdentityRequestImplProviderResetTest,
       AbortWhileProviderDestroysWebContents) {
  mojo::Remote<blink::mojom::DigitalIdentityRequest> remote;
  base::WeakPtr<DigitalIdentityRequestImpl> impl =
      DigitalIdentityRequestImpl::CreateInstance(
          *web_contents()->GetPrimaryMainFrame(),
          remote.BindNewPipeAndPassReceiver());
  ASSERT_TRUE(impl);

  DigitalCredentialGetRequestPtr request = DigitalCredentialGetRequest::New();
  request->protocol = "openid4vp";
  request->data = base::Value(base::DictValue());
  std::vector<DigitalCredentialGetRequestPtr> requests;
  requests.push_back(std::move(request));

  // Populates `provider_` (with WebContentsDestroyingProvider) and `callback_`.
  impl->Get(std::move(requests), base::DoNothing());

  // Abort() -> CompleteRequestWithStatus() -> provider_.reset() ->
  //   ~WebContentsDestroyingProvider -> DeleteContents() ->
  //     ~WebContentsImpl -> ~RenderFrameHostImpl ->
  //       DocumentService::ResetAndDeleteThisInternal -> delete impl
  // Control then returns to CompleteRequestWithStatus() which touches freed
  // members.
  ASSERT_NO_FATAL_FAILURE(impl->Abort());
}

TEST_F(DigitalIdentityRequestImplTest,
       ShouldReturnSameErrorForNoCredentialAndUserDeclined) {
  const std::string kProtocol = "protocol";

  auto get_status_for_provider_error =
      [&](RequestStatusForMetrics provider_error) {
        SetMockDigitalIdentityProvider(
            std::make_unique<MockDigitalIdentityProvider>());

        RecreateService();

        DigitalCredentialGetRequestPtr digital_credential_request =
            DigitalCredentialGetRequest::New();
        digital_credential_request->protocol = kProtocol;
        base::DictValue request_data;
        request_data.Set("data", "request data");
        digital_credential_request->data = base::Value(std::move(request_data));

        std::vector<DigitalCredentialGetRequestPtr> requests;
        requests.push_back(std::move(digital_credential_request));

        base::RunLoop run_loop;
        RequestDigitalIdentityStatus status_out;

        EXPECT_CALL(*mock_digital_identity_provider(), Get)
            .WillOnce(WithArg<3>([&](DigitalIdentityCallback callback) {
              reset_provider_pointer();
              std::move(callback).Run(base::unexpected(provider_error));
            }));

        base::MockCallback<GetCallback> mock_callback;
        EXPECT_CALL(mock_callback, Run)
            .WillOnce(
                ([&](RequestDigitalIdentityStatus status,
                     std::optional<std::string> protocol, base::Value token) {
                  status_out = status;
                  run_loop.Quit();
                }));

        digital_identity_request_impl()->Get(std::move(requests),
                                             mock_callback.Get());
        run_loop.Run();
        return status_out;
      };

  EXPECT_EQ(get_status_for_provider_error(
                RequestStatusForMetrics::kErrorNoCredential),
            get_status_for_provider_error(
                RequestStatusForMetrics::kErrorUserDeclined));
}

// Tests for browser-side Permissions Policy enforcement.
class DigitalIdentityRequestImplPermissionsPolicyTest
    : public RenderViewHostTestHarness {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kWebIdentityDigitalCredentials,
         features::kWebIdentityDigitalCredentialsCreation},
        {});
    RenderViewHostTestHarness::SetUp();
    NavigateAndCommit(GURL("https://example.test"));

    command_line_.GetProcessCommandLine()->AppendSwitch(
        switches::kUseFakeUIForDigitalIdentity);
  }

  std::vector<DigitalCredentialGetRequestPtr> MakeGetRequests() {
    DigitalCredentialGetRequestPtr request = DigitalCredentialGetRequest::New();
    request->protocol = kOpenid4vpProtocol;
    base::DictValue data;
    data.Set("data", "test");
    request->data = base::Value(std::move(data));
    std::vector<DigitalCredentialGetRequestPtr> requests;
    requests.push_back(std::move(request));
    return requests;
  }

  std::vector<DigitalCredentialCreateRequestPtr> MakeCreateRequests() {
    DigitalCredentialCreateRequestPtr request =
        DigitalCredentialCreateRequest::New();
    request->protocol = kOpenid4vpProtocol;
    base::DictValue data;
    data.Set("data", "test");
    request->data = base::Value(std::move(data));
    std::vector<DigitalCredentialCreateRequestPtr> requests;
    requests.push_back(std::move(request));
    return requests;
  }

  // Navigate to a page with a Permissions Policy header that denies the given
  // feature, then bind DigitalIdentityRequest through the Mojo pipe.
  void NavigateWithDeniedPolicyAndBind(
      network::mojom::PermissionsPolicyFeature feature) {
    network::ParsedPermissionsPolicy policy(1);
    policy[0].feature = feature;
    // Empty allowed_origins + matches_all_origins=false = deny all.

    auto simulator = NavigationSimulator::CreateRendererInitiated(
        GURL("https://example.test"), web_contents()->GetPrimaryMainFrame());
    simulator->SetPermissionsPolicyHeader(std::move(policy));
    simulator->Commit();

    auto* rfh = static_cast<TestRenderFrameHost*>(
        web_contents()->GetPrimaryMainFrame());
    mojo::Receiver<blink::mojom::BrowserInterfaceBroker>& bib =
        rfh->browser_interface_broker_receiver_for_testing();
    blink::mojom::BrowserInterfaceBroker* broker = bib.internal_state()->impl();
    request_remote_.reset();
    broker->GetInterface(request_remote_.BindNewPipeAndPassReceiver());
  }

  mojo::Remote<blink::mojom::DigitalIdentityRequest>& request_remote() {
    return request_remote_;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::ScopedCommandLine command_line_;

  mojo::Remote<blink::mojom::DigitalIdentityRequest> request_remote_;
};

TEST_F(DigitalIdentityRequestImplPermissionsPolicyTest,
       GetShouldRejectWhenPermissionsPolicyDisabled) {
  NavigateWithDeniedPolicyAndBind(
      network::mojom::PermissionsPolicyFeature::kDigitalCredentialsGet);

  mojo::test::BadMessageObserver bad_message_observer;
  request_remote()->Get(MakeGetRequests(), base::DoNothing());
  EXPECT_EQ("digital-credentials-get permissions policy is not enabled.",
            bad_message_observer.WaitForBadMessage());
}

TEST_F(DigitalIdentityRequestImplPermissionsPolicyTest,
       CreateShouldRejectWhenPermissionsPolicyDisabled) {
  NavigateWithDeniedPolicyAndBind(
      network::mojom::PermissionsPolicyFeature::kDigitalCredentialsCreate);

  mojo::test::BadMessageObserver bad_message_observer;
  request_remote()->Create(MakeCreateRequests(), base::DoNothing());
  EXPECT_EQ("digital-credentials-create permissions policy is not enabled.",
            bad_message_observer.WaitForBadMessage());
}

class DigitalIdentityRequestImplVirtualWalletTest
    : public RenderViewHostTestHarness {
 public:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    scoped_feature_list_.InitWithFeatures(
        {features::kWebIdentityDigitalCredentials,
         features::kWebIdentityDigitalCredentialsCreation},
        {});
    digital_identity_request_impl_ = DigitalIdentityRequestImpl::CreateInstance(
        *web_contents()->GetPrimaryMainFrame(),
        request_remote_.BindNewPipeAndPassReceiver());
  }

  void TearDown() override {
    digital_identity_request_impl_ = nullptr;
    DigitalCredentialEnvironment::GetInstance()->Reset();
    RenderViewHostTestHarness::TearDown();
  }

  DigitalIdentityRequestImpl* digital_identity_request_impl() {
    return digital_identity_request_impl_.get();
  }

  VirtualWallet* GetOrCreateVirtualWallet() {
    FrameTreeNode* node =
        FrameTreeNode::From(web_contents()->GetPrimaryMainFrame());
    return DigitalCredentialEnvironment::GetInstance()
        ->GetOrCreateVirtualWallet(node);
  }

  std::vector<DigitalCredentialGetRequestPtr> MakeGetRequests() {
    DigitalCredentialGetRequestPtr request = DigitalCredentialGetRequest::New();
    request->protocol = "protocol_in_request";
    request->data = GenerateOnlyAgeOpenid4VpRequestWithDCQL();
    std::vector<DigitalCredentialGetRequestPtr> requests;
    requests.push_back(std::move(request));
    return requests;
  }

  std::vector<DigitalCredentialCreateRequestPtr> MakeCreateRequests() {
    DigitalCredentialCreateRequestPtr request =
        DigitalCredentialCreateRequest::New();
    request->protocol = "protocol_in_request";
    base::DictValue request_data;
    request_data.Set("data", "create request data");
    request->data = base::Value(std::move(request_data));
    std::vector<DigitalCredentialCreateRequestPtr> requests;
    requests.push_back(std::move(request));
    return requests;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  mojo::Remote<blink::mojom::DigitalIdentityRequest> request_remote_;
  base::WeakPtr<DigitalIdentityRequestImpl> digital_identity_request_impl_;
};

TEST_F(DigitalIdentityRequestImplVirtualWalletTest, GetRespond) {
  const std::string kProtocolInWallet = "protocol_in_wallet";
  const base::Value kResponseData(
      base::DictValue().Set("token", "virtual-wallet-data"));

  VirtualWallet* wallet = GetOrCreateVirtualWallet();
  wallet->set_behavior(VirtualWallet::Behavior::kRespond);
  wallet->SetCredential(
      DigitalCredential(kProtocolInWallet, kResponseData.Clone()));

  base::RunLoop run_loop;
  base::MockCallback<GetCallback> mock_callback;
  EXPECT_CALL(mock_callback, Run(RequestDigitalIdentityStatus::kSuccess,
                                 Optional(kProtocolInWallet), _))
      .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));
  digital_identity_request_impl()->Get(MakeGetRequests(), mock_callback.Get());
  run_loop.Run();
}

TEST_F(DigitalIdentityRequestImplVirtualWalletTest, GetDecline) {
  VirtualWallet* wallet = GetOrCreateVirtualWallet();
  wallet->set_behavior(VirtualWallet::Behavior::kDecline);

  base::RunLoop run_loop;
  base::MockCallback<GetCallback> mock_callback;
  EXPECT_CALL(mock_callback,
              Run(RequestDigitalIdentityStatus::kErrorUserDeclined,
                  Eq(std::nullopt), _))
      .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));
  digital_identity_request_impl()->Get(MakeGetRequests(), mock_callback.Get());
  run_loop.Run();
}

TEST_F(DigitalIdentityRequestImplVirtualWalletTest, GetWaitIsAbortable) {
  VirtualWallet* wallet = GetOrCreateVirtualWallet();
  wallet->set_behavior(VirtualWallet::Behavior::kWait);

  base::RunLoop run_loop;
  base::MockCallback<GetCallback> mock_callback;
  EXPECT_CALL(mock_callback, Run(RequestDigitalIdentityStatus::kErrorCanceled,
                                 Eq(std::nullopt), _))
      .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));
  digital_identity_request_impl()->Get(MakeGetRequests(), mock_callback.Get());
  digital_identity_request_impl()->Abort();
  run_loop.Run();
}

TEST_F(DigitalIdentityRequestImplVirtualWalletTest, CreateRespond) {
  const std::string kProtocolInWallet = "protocol_in_wallet";
  const base::Value kResponseData(
      base::DictValue().Set("token", "virtual-wallet-data"));

  VirtualWallet* wallet = GetOrCreateVirtualWallet();
  wallet->set_behavior(VirtualWallet::Behavior::kRespond);
  wallet->SetCredential(
      DigitalCredential(kProtocolInWallet, kResponseData.Clone()));

  base::RunLoop run_loop;
  base::MockCallback<DigitalIdentityRequestImpl::CreateCallback> mock_callback;
  EXPECT_CALL(mock_callback, Run(RequestDigitalIdentityStatus::kSuccess,
                                 Optional(kProtocolInWallet), _))
      .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));
  digital_identity_request_impl()->Create(MakeCreateRequests(),
                                          mock_callback.Get());
  run_loop.Run();
}

TEST_F(DigitalIdentityRequestImplVirtualWalletTest, CreateDecline) {
  VirtualWallet* wallet = GetOrCreateVirtualWallet();
  wallet->set_behavior(VirtualWallet::Behavior::kDecline);

  base::RunLoop run_loop;
  base::MockCallback<DigitalIdentityRequestImpl::CreateCallback> mock_callback;
  EXPECT_CALL(mock_callback,
              Run(RequestDigitalIdentityStatus::kErrorUserDeclined,
                  Eq(std::nullopt), _))
      .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));
  digital_identity_request_impl()->Create(MakeCreateRequests(),
                                          mock_callback.Get());
  run_loop.Run();
}

TEST_F(DigitalIdentityRequestImplVirtualWalletTest, CreateWaitIsAbortable) {
  VirtualWallet* wallet = GetOrCreateVirtualWallet();
  wallet->set_behavior(VirtualWallet::Behavior::kWait);

  base::RunLoop run_loop;
  base::MockCallback<DigitalIdentityRequestImpl::CreateCallback> mock_callback;
  EXPECT_CALL(mock_callback, Run(RequestDigitalIdentityStatus::kErrorCanceled,
                                 Eq(std::nullopt), _))
      .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));
  digital_identity_request_impl()->Create(MakeCreateRequests(),
                                          mock_callback.Get());
  digital_identity_request_impl()->Abort();
  run_loop.Run();
}

TEST_F(DigitalIdentityRequestImplVirtualWalletTest, GetNoBehaviorFallsThrough) {
  GetOrCreateVirtualWallet();

  base::MockCallback<GetCallback> mock_callback;
  EXPECT_CALL(mock_callback,
              Run(RequestDigitalIdentityStatus::kError, Eq(std::nullopt), _));
  digital_identity_request_impl()->Get(MakeGetRequests(), mock_callback.Get());
}

TEST_F(DigitalIdentityRequestImplVirtualWalletTest,
       CreateNoBehaviorFallsThrough) {
  GetOrCreateVirtualWallet();

  base::MockCallback<DigitalIdentityRequestImpl::CreateCallback> mock_callback;
  EXPECT_CALL(mock_callback,
              Run(RequestDigitalIdentityStatus::kError, Eq(std::nullopt), _));
  digital_identity_request_impl()->Create(MakeCreateRequests(),
                                          mock_callback.Get());
}

TEST_F(DigitalIdentityRequestImplTest, RecordOpenId4VpResponseMode) {
  struct TestCase {
    std::string response_mode;
    int expected_bucket;
  } test_cases[] = {
      {"dc_api", 0},
      {"dc_api.jwt", 1},
      {"other_mode", 2},
  };

  for (const auto& test_case : test_cases) {
    RecreateService();
    base::HistogramTester histogram_tester;
    DigitalCredentialGetRequestPtr digital_credential_request =
        DigitalCredentialGetRequest::New();
    digital_credential_request->protocol = kOpenid4vpUnsignedProtocol;

    base::DictValue request_data;
    request_data.Set("response_mode", test_case.response_mode);
    digital_credential_request->data = base::Value(std::move(request_data));

    std::vector<DigitalCredentialGetRequestPtr> requests;
    requests.push_back(std::move(digital_credential_request));

    EXPECT_CALL(*mock_digital_identity_provider(), Get)
        .Times(testing::AnyNumber());

    digital_identity_request_impl()->Get(std::move(requests),
                                         base::DoNothing());

    histogram_tester.ExpectUniqueSample(
        "Blink.DigitalIdentityRequest.OpenId4VpResponseMode",
        test_case.expected_bucket, 1);
  }
}

TEST_F(DigitalIdentityRequestImplTest, RecordOpenId4VpResponseModeMissing) {
  base::HistogramTester histogram_tester;
  DigitalCredentialGetRequestPtr digital_credential_request =
      DigitalCredentialGetRequest::New();
  digital_credential_request->protocol = kOpenid4vpUnsignedProtocol;
  digital_credential_request->data = base::Value(base::DictValue());

  std::vector<DigitalCredentialGetRequestPtr> requests;
  requests.push_back(std::move(digital_credential_request));

  EXPECT_CALL(*mock_digital_identity_provider(), Get)
      .Times(testing::AnyNumber());

  digital_identity_request_impl()->Get(std::move(requests), base::DoNothing());

  histogram_tester.ExpectTotalCount(
      "Blink.DigitalIdentityRequest.OpenId4VpResponseMode", 0);
}

TEST_F(DigitalIdentityRequestImplTest, RecordOpenId4VpResponseModeFromJwt) {
  base::HistogramTester histogram_tester;
  DigitalCredentialGetRequestPtr digital_credential_request =
      DigitalCredentialGetRequest::New();
  digital_credential_request->protocol = kOpenid4vpUnsignedProtocol;

  base::Value request_data = GenerateSignedOnlyAgeOpenid4VpRequestWithDCQL();
  digital_credential_request->data = std::move(request_data);

  std::vector<DigitalCredentialGetRequestPtr> requests;
  requests.push_back(std::move(digital_credential_request));

  EXPECT_CALL(*mock_digital_identity_provider(), Get)
      .Times(testing::AnyNumber());

  digital_identity_request_impl()->Get(std::move(requests), base::DoNothing());

  histogram_tester.ExpectUniqueSample(
      "Blink.DigitalIdentityRequest.OpenId4VpResponseMode", 0, 1);
}

TEST_F(DigitalIdentityRequestImplTest,
       RecordOpenId4VpResponseModeInvalidRequest) {
  base::HistogramTester histogram_tester;
  DigitalCredentialGetRequestPtr digital_credential_request =
      DigitalCredentialGetRequest::New();
  digital_credential_request->protocol = kOpenid4vpUnsignedProtocol;
  digital_credential_request->data = base::Value("not a dict");

  std::vector<DigitalCredentialGetRequestPtr> requests;
  requests.push_back(std::move(digital_credential_request));

  EXPECT_CALL(*mock_digital_identity_provider(), Get)
      .Times(testing::AnyNumber());

  digital_identity_request_impl()->Get(std::move(requests), base::DoNothing());

  histogram_tester.ExpectTotalCount(
      "Blink.DigitalIdentityRequest.OpenId4VpResponseMode", 0);
}

}  // namespace content
