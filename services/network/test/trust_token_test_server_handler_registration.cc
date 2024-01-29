// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/test/trust_token_test_server_handler_registration.h"

#include <memory>
#include <optional>
#include <string_view>

#include "base/base64.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/test/bind.h"
#include "net/http/http_request_headers.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/test/trust_token_request_handler.h"
#include "services/network/trust_tokens/suitable_trust_token_origin.h"

namespace network::test {

namespace {

const char kIssuanceRelativePath[] = "/issue";
const char kRedemptionRelativePath[] = "/redeem";
const char kSignedRequestVerificationRelativePath[] = "/sign";

std::unique_ptr<net::test_server::HttpResponse>
MakeTrustTokenFailureResponse() {
  // No need to report a failure HTTP code here: returning a vanilla OK should
  // fail the Trust Tokens operation client-side.
  auto ret = std::make_unique<net::test_server::BasicHttpResponse>();
  ret->AddCustomHeader("Access-Control-Allow-Origin", "*");
  return ret;
}

// Constructs and returns an HTTP response bearing the given base64-encoded
// Trust Tokens issuance or redemption protocol response message.
std::unique_ptr<net::test_server::HttpResponse> MakeTrustTokenResponse(
    std::string_view contents) {
  CHECK([&]() {
    std::string temp;
    return base::Base64Decode(contents, &temp);
  }());

  auto ret = std::make_unique<net::test_server::BasicHttpResponse>();
  ret->AddCustomHeader("Sec-Private-State-Token", std::string(contents));
  ret->AddCustomHeader("Access-Control-Allow-Origin", "*");
  return ret;
}

}  // namespace

void RegisterTrustTokenTestHandlers(net::EmbeddedTestServer* test_server,
                                    TrustTokenRequestHandler* handler) {
  test_server->RegisterRequestHandler(base::BindLambdaForTesting(
      [handler](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        // Decline to handle the request if it isn't destined for this
        // endpoint.
        if (request.relative_url != kIssuanceRelativePath)
          return nullptr;

        if (!base::Contains(request.headers, "Sec-Private-State-Token") ||
            !base::Contains(request.headers,
                            "Sec-Private-State-Token-Crypto-Version")) {
          return MakeTrustTokenFailureResponse();
        }

        std::optional<std::string> operation_result =
            handler->Issue(request.headers.at("Sec-Private-State-Token"));

        if (!operation_result)
          return MakeTrustTokenFailureResponse();

        return MakeTrustTokenResponse(*operation_result);
      }));

  test_server->RegisterRequestHandler(base::BindLambdaForTesting(
      [handler](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (request.relative_url != kRedemptionRelativePath)
          return nullptr;

        if (!base::Contains(request.headers, "Sec-Private-State-Token") ||
            !base::Contains(request.headers,
                            "Sec-Private-State-Token-Crypto-Version")) {
          return MakeTrustTokenFailureResponse();
        }

        std::optional<std::string> operation_result =
            handler->Redeem(request.headers.at("Sec-Private-State-Token"));

        if (!operation_result)
          return MakeTrustTokenFailureResponse();

        return MakeTrustTokenResponse(*operation_result);
      }));

  test_server->RegisterRequestHandler(base::BindLambdaForTesting(
      [handler](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (request.relative_url != kSignedRequestVerificationRelativePath)
          return nullptr;

        GURL::Replacements replacements;
        std::string host_and_maybe_port =
            request.headers.at(net::HttpRequestHeaders::kHost);
        if (base::Contains(host_and_maybe_port, ':'))
          host_and_maybe_port.resize(host_and_maybe_port.find(':'));
        replacements.SetHostStr(host_and_maybe_port);
        GURL destination_url = request.GetURL().ReplaceComponents(replacements);

        net::HttpRequestHeaders headers;
        for (const auto& name_and_value : request.headers)
          headers.SetHeader(name_and_value.first, name_and_value.second);

        handler->RecordSignedRequest(destination_url, headers);

        // Unlike issuance and redemption, there's no special state to return
        // on success for signing.
        auto ret = std::make_unique<net::test_server::BasicHttpResponse>();
        ret->AddCustomHeader("Access-Control-Allow-Origin", "*");
        return ret;
      }));
}

}  // namespace network::test
