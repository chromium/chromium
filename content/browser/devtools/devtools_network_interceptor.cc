// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_network_interceptor.h"
#include "base/strings/pattern.h"
#include "content/browser/devtools/protocol/network_handler.h"
#include "url/gurl.h"

namespace content {

DevToolsNetworkInterceptor::AuthChallengeResponse::AuthChallengeResponse(
    ResponseType response_type)
    : response_type(response_type) {
  DCHECK_NE(kProvideCredentials, response_type);
}

DevToolsNetworkInterceptor::AuthChallengeResponse::AuthChallengeResponse(
    const base::string16& username,
    const base::string16& password)
    : response_type(kProvideCredentials), credentials(username, password) {}

InterceptedRequestInfo::InterceptedRequestInfo()
    : is_navigation(false), response_error_code(net::OK) {}

InterceptedRequestInfo::~InterceptedRequestInfo() = default;

DevToolsNetworkInterceptor::FilterEntry::FilterEntry(
    const base::UnguessableToken& target_id,
    std::vector<Pattern> patterns,
    RequestInterceptedCallback callback)
    : target_id(target_id),
      patterns(std::move(patterns)),
      callback(std::move(callback)) {}

DevToolsNetworkInterceptor::FilterEntry::FilterEntry(FilterEntry&&) {}
DevToolsNetworkInterceptor::FilterEntry::~FilterEntry() {}

DevToolsNetworkInterceptor::Modifications::Modifications() = default;

DevToolsNetworkInterceptor::Modifications::Modifications(
    base::Optional<net::Error> error_reason,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    std::unique_ptr<std::string> response_body,
    protocol::Maybe<std::string> modified_url,
    protocol::Maybe<std::string> modified_method,
    protocol::Maybe<std::string> modified_post_data,
    std::unique_ptr<HeadersVector> modified_headers,
    std::unique_ptr<AuthChallengeResponse> auth_challenge_response)
    : error_reason(std::move(error_reason)),
      response_headers(std::move(response_headers)),
      response_body(std::move(response_body)),
      modified_url(std::move(modified_url)),
      modified_method(std::move(modified_method)),
      modified_post_data(std::move(modified_post_data)),
      modified_headers(std::move(modified_headers)),
      auth_challenge_response(std::move(auth_challenge_response)) {}

DevToolsNetworkInterceptor::Modifications::~Modifications() {}

DevToolsNetworkInterceptor::Pattern::~Pattern() = default;

DevToolsNetworkInterceptor::Pattern::Pattern(const Pattern& other) = default;

DevToolsNetworkInterceptor::Pattern::Pattern(
    const std::string& url_pattern,
    base::flat_set<ResourceType> resource_types,
    InterceptionStage interception_stage)
    : url_pattern(url_pattern),
      resource_types(std::move(resource_types)),
      interception_stage(interception_stage) {}

bool DevToolsNetworkInterceptor::Pattern::Matches(
    const std::string& url,
    ResourceType resource_type) const {
  if (!resource_types.empty() &&
      resource_types.find(resource_type) == resource_types.end()) {
    return false;
  }
  return base::MatchPattern(url, url_pattern);
}

}  // namespace content
