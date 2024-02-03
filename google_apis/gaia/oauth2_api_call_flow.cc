// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/oauth2_api_call_flow.h"

#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "google_apis/credentials_mode.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

OAuth2ApiCallFlow::OAuth2ApiCallFlow() : state_(INITIAL) {
}

OAuth2ApiCallFlow::~OAuth2ApiCallFlow() = default;

void OAuth2ApiCallFlow::Start(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& access_token) {
  CHECK(state_ == INITIAL);
  state_ = API_CALL_STARTED;

  url_loader_ = CreateURLLoader(access_token);
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory.get(),
      base::BindOnce(&OAuth2ApiCallFlow::OnURLLoadComplete,
                     base::Unretained(this)));
}

net::HttpRequestHeaders OAuth2ApiCallFlow::CreateApiCallHeaders() {
  return net::HttpRequestHeaders();
}

std::string OAuth2ApiCallFlow::CreateAuthorizationHeaderValue(
    const std::string& access_token) {
  return base::StrCat({"Bearer ", access_token});
}

void OAuth2ApiCallFlow::EndApiCall(std::unique_ptr<std::string> body) {
  CHECK_EQ(API_CALL_STARTED, state_);
  std::unique_ptr<network::SimpleURLLoader> source = std::move(url_loader_);

  int status_code = 0;
  if (source->ResponseInfo() && source->ResponseInfo()->headers)
    status_code = source->ResponseInfo()->headers->response_code();
  if (source->NetError() != net::OK || !IsExpectedSuccessCode(status_code)) {
    state_ = ERROR_STATE;
    ProcessApiCallFailure(source->NetError(), source->ResponseInfo(),
                          std::move(body));
  } else {
    state_ = API_CALL_DONE;
    ProcessApiCallSuccess(source->ResponseInfo(), std::move(body));
  }
}

std::string OAuth2ApiCallFlow::CreateApiCallBodyContentType() {
  return "application/x-www-form-urlencoded";
}

std::string OAuth2ApiCallFlow::GetRequestTypeForBody(const std::string& body) {
  return body.empty() ? "GET" : "POST";
}

bool OAuth2ApiCallFlow::IsExpectedSuccessCode(int code) const {
  return code == net::HTTP_OK || code == net::HTTP_NO_CONTENT;
}

void OAuth2ApiCallFlow::OnURLLoadComplete(std::unique_ptr<std::string> body) {
  CHECK_EQ(API_CALL_STARTED, state_);
  EndApiCall(std::move(body));
}

std::unique_ptr<network::SimpleURLLoader> OAuth2ApiCallFlow::CreateURLLoader(
    const std::string& access_token) {
  std::string body = CreateApiCallBody();
  std::string request_type = GetRequestTypeForBody(body);
  net::NetworkTrafficAnnotationTag traffic_annotation =
      CompleteNetworkTrafficAnnotation("oauth2_api_call_flow",
                                       GetNetworkTrafficAnnotationTag(), R"(
          policy {
            cookies_allowed: NO
          })");

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = CreateApiCallUrl();
  request->method = request_type;
  request->credentials_mode =
      google_apis::GetOmitCredentialsModeForGaiaRequests();
  request->headers = CreateApiCallHeaders();
  request->headers.SetHeader("Authorization",
                             CreateAuthorizationHeaderValue(access_token));

  std::unique_ptr<network::SimpleURLLoader> result =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation);

  // Fetchers are sometimes cancelled because a network change was detected,
  // especially at startup and after sign-in on ChromeOS. Retrying once should
  // be enough in those cases; let the fetcher retry up to 3 times just in case.
  // http://crbug.com/163710
  result->SetRetryOptions(3, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);
  result->SetAllowHttpErrorResults(true);

  // Even if the the body is empty, we still set the Content-Type because an
  // empty string may be a meaningful value. For example, a Protocol Buffer
  // message with only default values will be serialized as an empty string.
  if (request_type != "GET")
    result->AttachStringForUpload(body, CreateApiCallBodyContentType());

  return result;
}
