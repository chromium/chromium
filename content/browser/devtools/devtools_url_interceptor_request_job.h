// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_URL_INTERCEPTOR_REQUEST_JOB_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_URL_INTERCEPTOR_REQUEST_JOB_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "content/browser/devtools/devtools_url_request_interceptor.h"
#include "content/browser/devtools/protocol/network.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/resource_type.h"
#include "net/cookies/canonical_cookie.h"
#include "net/http/http_raw_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"

namespace net {
class AuthChallengeInfo;
class UploadDataStream;
}

namespace content {

// A URLRequestJob that allows programmatic request blocking / modification or
// response mocking.  This class should only be accessed on the IO thread.
class DevToolsURLInterceptorRequestJob : public net::URLRequestJob {
 public:
  DevToolsURLInterceptorRequestJob(
      DevToolsURLRequestInterceptor* interceptor,
      const std::string& interception_id,
      intptr_t owning_entry_id,
      net::URLRequest* original_request,
      net::NetworkDelegate* original_network_delegate,
      const base::UnguessableToken& devtools_token,
      DevToolsNetworkInterceptor::RequestInterceptedCallback callback,
      ResourceType resource_type,
      DevToolsNetworkInterceptor::InterceptionStage stage_to_intercept);

  ~DevToolsURLInterceptorRequestJob() override;

  // net::URLRequestJob implementation:
  void SetExtraRequestHeaders(const net::HttpRequestHeaders& headers) override;
  void Start() override;
  void Kill() override;
  int ReadRawData(net::IOBuffer* buf, int buf_size) override;
  int GetResponseCode() const override;
  void GetResponseInfo(net::HttpResponseInfo* info) override;
  bool GetMimeType(std::string* mime_type) const override;
  bool GetCharset(std::string* charset) override;
  void GetLoadTimingInfo(net::LoadTimingInfo* load_timing_info) const override;
  bool NeedsAuth() override;
  void GetAuthChallengeInfo(
      scoped_refptr<net::AuthChallengeInfo>* auth_info) override;

  void SetAuth(const net::AuthCredentials& credentials) override;
  void CancelAuth() override;
  void SetRequestHeadersCallback(net::RequestHeadersCallback callback) override;
  void SetResponseHeadersCallback(
      net::ResponseHeadersCallback callback) override;
  void ContinueDespiteLastError() override;

  // Must be called on IO thread.
  void StopIntercepting();

  using ContinueInterceptedRequestCallback =
      protocol::Network::Backend::ContinueInterceptedRequestCallback;
  using GetResponseBodyForInterceptionCallback =
      protocol::Network::Backend::GetResponseBodyForInterceptionCallback;
  using InterceptionStage = DevToolsNetworkInterceptor::InterceptionStage;

  // Must be called only once per interception. Must be called on IO thread.
  void ContinueInterceptedRequest(
      std::unique_ptr<DevToolsNetworkInterceptor::Modifications> modifications,
      std::unique_ptr<ContinueInterceptedRequestCallback> callback);
  void GetResponseBody(
      std::unique_ptr<GetResponseBodyForInterceptionCallback> callback);

  intptr_t owning_entry_id() const { return owning_entry_id_; }

 private:
  std::unique_ptr<InterceptedRequestInfo> BuildRequestInfo();

  class SubRequest;
  class InterceptedRequest;
  class MockResponseDetails;

  // We keep a copy of the original request details to facilitate the
  // Network.modifyRequest command which could potentially change any of these
  // fields.
  struct RequestDetails {
    RequestDetails(const GURL& url,
                   const std::string& method,
                   std::unique_ptr<net::UploadDataStream> post_data,
                   const net::HttpRequestHeaders& extra_request_headers,
                   const std::string& referrer,
                   net::URLRequest::ReferrerPolicy referrer_policy,
                   const net::RequestPriority& priority,
                   const net::URLRequestContext* url_request_context);
    ~RequestDetails();

    GURL url;
    std::string method;
    std::unique_ptr<net::UploadDataStream> post_data;
    std::string cookie_line;
    net::HttpRequestHeaders extra_request_headers;
    std::string referrer;
    net::URLRequest::ReferrerPolicy referrer_policy;
    net::RequestPriority priority;
    const net::URLRequestContext* url_request_context;
  };

  void StartWithCookies(const net::CookieList& cookies);

  // Callbacks from SubRequest.
  void OnSubRequestAuthRequired(net::AuthChallengeInfo* auth_info);
  void OnSubRequestRedirectReceived(const net::URLRequest& request,
                                    const net::RedirectInfo& redirectinfo,
                                    bool* defer_redirect);
  void OnSubRequestResponseStarted(const net::Error& net_error);
  void OnSubRequestHeadersReceived(const net::Error& net_error);

  // Callbacks from InterceptedRequest.
  void OnInterceptedRequestResponseStarted(const net::Error& net_error);
  void OnInterceptedRequestResponseReady(const net::IOBuffer& buf, int result);

  // Retrieves the response headers from either the |sub_request_| or the
  // |mock_response_|.  In some cases (e.g. file access) this may be null.
  const net::HttpResponseHeaders* GetHttpResponseHeaders() const;

  void ProcessRedirect(int status_code, const std::string& new_url);
  void ProcessInterceptionResponse(
      std::unique_ptr<DevToolsNetworkInterceptor::Modifications> modification);

  void ProcessAuthResponse(
      const DevToolsNetworkInterceptor::AuthChallengeResponse& response);

  enum class WaitingForUserResponse {
    NOT_WAITING,
    WAITING_FOR_REQUEST_ACK,
    WAITING_FOR_RESPONSE_ACK,
    WAITING_FOR_AUTH_ACK,
  };

  DevToolsURLRequestInterceptor* const interceptor_;
  RequestDetails request_details_;
  std::unique_ptr<SubRequest> sub_request_;
  std::unique_ptr<MockResponseDetails> mock_response_details_;
  std::unique_ptr<net::RedirectInfo> redirect_;
  WaitingForUserResponse waiting_for_user_response_;
  scoped_refptr<net::AuthChallengeInfo> auth_info_;

  const std::string interception_id_;
  const intptr_t owning_entry_id_;
  const base::UnguessableToken devtools_token_;
  DevToolsNetworkInterceptor::RequestInterceptedCallback callback_;
  const ResourceType resource_type_;
  InterceptionStage stage_to_intercept_;
  std::vector<std::unique_ptr<GetResponseBodyForInterceptionCallback>>
      pending_body_requests_;

  net::RequestHeadersCallback request_headers_callback_;
  net::ResponseHeadersCallback response_headers_callback_;
  base::WeakPtrFactory<DevToolsURLInterceptorRequestJob> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsURLInterceptorRequestJob);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_URL_INTERCEPTOR_REQUEST_JOB_H_
