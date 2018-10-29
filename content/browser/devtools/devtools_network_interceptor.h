// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_NETWORK_INTERCEPTOR_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_NETWORK_INTERCEPTOR_H_

#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/optional.h"
#include "base/unguessable_token.h"
#include "content/browser/devtools/protocol/network.h"
#include "content/public/common/resource_type.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/auth.h"
#include "net/base/net_errors.h"

namespace net {
class AuthChallengeInfo;
class HttpResponseHeaders;
}  // namespace net

namespace content {

struct InterceptedRequestInfo {
  InterceptedRequestInfo();
  ~InterceptedRequestInfo();

  std::string interception_id;
  base::UnguessableToken frame_id;
  ResourceType resource_type;
  bool is_navigation;
  int response_error_code;
  std::unique_ptr<protocol::Network::Request> network_request;
  scoped_refptr<net::AuthChallengeInfo> auth_challenge;
  scoped_refptr<net::HttpResponseHeaders> response_headers;
  protocol::Maybe<bool> is_download;
  protocol::Maybe<protocol::String> redirect_url;
};

class DevToolsNetworkInterceptor {
 public:
  virtual ~DevToolsNetworkInterceptor() = default;

  using RequestInterceptedCallback =
      base::RepeatingCallback<void(std::unique_ptr<InterceptedRequestInfo>)>;
  using ContinueInterceptedRequestCallback =
      protocol::Network::Backend::ContinueInterceptedRequestCallback;
  using GetResponseBodyForInterceptionCallback =
      protocol::Network::Backend::GetResponseBodyForInterceptionCallback;
  using TakeResponseBodyPipeCallback =
      base::OnceCallback<void(protocol::Response,
                              mojo::ScopedDataPipeConsumerHandle,
                              const std::string& mime_type)>;

  struct AuthChallengeResponse {
    enum ResponseType {
      kDefault,
      kCancelAuth,
      kProvideCredentials,
    };

    explicit AuthChallengeResponse(ResponseType response_type);
    AuthChallengeResponse(const base::string16& username,
                          const base::string16& password);

    const ResponseType response_type;
    const net::AuthCredentials credentials;

    DISALLOW_COPY_AND_ASSIGN(AuthChallengeResponse);
  };

  struct Modifications {
    using HeadersVector = std::vector<std::pair<std::string, std::string>>;

    Modifications();
    Modifications(
        base::Optional<net::Error> error_reason,
        scoped_refptr<net::HttpResponseHeaders> response_headers,
        std::unique_ptr<std::string> response_body,
        protocol::Maybe<std::string> modified_url,
        protocol::Maybe<std::string> modified_method,
        protocol::Maybe<std::string> modified_post_data,
        std::unique_ptr<HeadersVector> modified_headers,
        std::unique_ptr<AuthChallengeResponse> auth_challenge_response);
    ~Modifications();

    // If none of the following are set then the request will be allowed to
    // continue unchanged.
    base::Optional<net::Error> error_reason;   // Finish with error.
    // If either of the below fields is set, complete the request by
    // responding with the provided headers and body.
    scoped_refptr<net::HttpResponseHeaders> response_headers;
    std::unique_ptr<std::string> response_body;

    // Optionally modify before sending to network.
    protocol::Maybe<std::string> modified_url;
    protocol::Maybe<std::string> modified_method;
    protocol::Maybe<std::string> modified_post_data;
    std::unique_ptr<HeadersVector> modified_headers;
    // AuthChallengeResponse is mutually exclusive with the above.
    std::unique_ptr<AuthChallengeResponse> auth_challenge_response;
  };

  enum InterceptionStage {
    DONT_INTERCEPT = 0,
    REQUEST = (1 << 0),
    RESPONSE = (1 << 1),
    // Note: Both is not sent from front-end. It is used if both Request
    // and HeadersReceived was found it upgrades it to Both.
    BOTH = (REQUEST | RESPONSE),
  };

  struct Pattern {
   public:
    ~Pattern();
    Pattern(const Pattern& other);
    Pattern(const std::string& url_pattern,
            base::flat_set<ResourceType> resource_types,
            InterceptionStage interception_stage);

    bool Matches(const std::string& url, ResourceType resource_type) const;

    const std::string url_pattern;
    const base::flat_set<ResourceType> resource_types;
    const InterceptionStage interception_stage;
  };

  struct FilterEntry {
    FilterEntry(const base::UnguessableToken& target_id,
                std::vector<Pattern> patterns,
                RequestInterceptedCallback callback);
    FilterEntry(FilterEntry&&);
    ~FilterEntry();

    const base::UnguessableToken target_id;
    std::vector<Pattern> patterns;
    const RequestInterceptedCallback callback;

    DISALLOW_COPY_AND_ASSIGN(FilterEntry);
  };

  virtual void AddFilterEntry(std::unique_ptr<FilterEntry> entry) = 0;
  virtual void RemoveFilterEntry(const FilterEntry* entry) = 0;
  virtual void UpdatePatterns(FilterEntry* entry,
                              std::vector<Pattern> patterns) = 0;
  virtual void GetResponseBody(
      std::string interception_id,
      std::unique_ptr<GetResponseBodyForInterceptionCallback> callback) = 0;
  virtual void ContinueInterceptedRequest(
      std::string interception_id,
      std::unique_ptr<Modifications> modifications,
      std::unique_ptr<ContinueInterceptedRequestCallback> callback) = 0;
};

inline DevToolsNetworkInterceptor::InterceptionStage& operator|=(
    DevToolsNetworkInterceptor::InterceptionStage& a,
    const DevToolsNetworkInterceptor::InterceptionStage& b) {
  a = static_cast<DevToolsNetworkInterceptor::InterceptionStage>(a | b);
  return a;
}

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_NETWORK_INTERCEPTOR_H_
