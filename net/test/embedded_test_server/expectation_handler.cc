// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/embedded_test_server/expectation_handler.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace net::test_server {

// UrlResponseConfig implementation
struct ExpectationHandler::UrlResponseConfig {
  UrlResponseConfig() = default;
  ~UrlResponseConfig() = default;

  // Whether the URL Path is a prefix match
  bool is_prefix = false;
  HttpStatusCode status_code;
  // Content type of the response (e.g., "text/html")
  std::string_view content_type;
  std::string_view content;
  base::OnceCallback<void(HttpRequest)> value_setting_callback;
};

// ExpectationHandler implementation
ExpectationHandler::ExpectationHandler(EmbeddedTestServer* embedded_test_server)
    : embedded_test_server_(*embedded_test_server) {
  embedded_test_server_->RegisterRequestHandler(base::BindRepeating(
      &ExpectationHandler::HandleRequest, base::Unretained(this)));
}

ExpectationHandler::~ExpectationHandler() = default;

ExpectationHandler::ResponseBuilder ExpectationHandler::OnRequest(
    std::string_view path,
    bool is_prefix) {
  // Returns a ResponseBuilder that configures how to respond to requests for
  // this URL
  return ResponseBuilder(*this, path, is_prefix);
}

std::unique_ptr<HttpResponse> ExpectationHandler::HandleRequest(
    const HttpRequest& request) {
  base::AutoLock auto_lock(lock_);
  // First, try to find an exact match using lower_bound
  const auto lower_bound = url_responses_.lower_bound(request.relative_url);
  if (lower_bound != url_responses_.end() &&
      lower_bound->first == request.relative_url) {
    return ApplyConfig(request, lower_bound->second.get());
  }
  // Iterate backwards from lower_bound to find the longest prefix match
  auto it = lower_bound;
  while (it != url_responses_.begin()) {
    --it;
    const auto& config = it->second;
    if (!config->is_prefix) {
      continue;
    }
    if (base::StartsWith(request.relative_url, it->first)) {
      return ApplyConfig(request, config.get());
    }
  }
  return nullptr;
}

// Helper method to apply a configuration and create a response
std::unique_ptr<HttpResponse> ExpectationHandler::ApplyConfig(
    const HttpRequest& request,
    raw_ptr<UrlResponseConfig> config) {
  if (config->value_setting_callback) {
    std::move(config->value_setting_callback).Run(request);
  }
  if (!config->content_type.empty()) {
    auto response = std::make_unique<BasicHttpResponse>();
    response->set_code(config->status_code);
    response->set_content_type(config->content_type);
    response->set_content(config->content);
    return response;
  }
  return nullptr;
}

// ResponseBuilder implementation
ExpectationHandler::ResponseBuilder::ResponseBuilder(
    ExpectationHandler& handler,
    std::string_view path,
    bool is_prefix)
    : handler_(handler), path_(path) {
  base::AutoLock auto_lock(handler_->lock_);
  auto new_config = std::make_unique<ExpectationHandler::UrlResponseConfig>();
  new_config->is_prefix = is_prefix;
  config_ = new_config.get();

  handler_->url_responses_[path_] = std::move(new_config);
}

ExpectationHandler::ResponseBuilder::~ResponseBuilder() = default;

ExpectationHandler::ResponseBuilder&
ExpectationHandler::ResponseBuilder::RespondWith(
    const HttpStatusCode status_code,
    std::string_view content_type,
    std::string_view content) {
  base::AutoLock auto_lock(handler_->lock_);

  if (config_) {
    config_->status_code = status_code;
    config_->content_type = content_type;
    config_->content = content;
  }

  return *this;
}

ExpectationHandler::ResponseBuilder&
ExpectationHandler::ResponseBuilder::RespondWith(std::string_view content_type,
                                                 std::string_view content) {
  return RespondWith(HTTP_OK, content_type, content);
}

ExpectationHandler::ResponseBuilder&
ExpectationHandler::ResponseBuilder::SetValue(
    base::test::TestFuture<HttpRequest>& future) {
  base::AutoLock auto_lock(handler_->lock_);
  config_->value_setting_callback = future.GetSequenceBoundCallback();
  return *this;
}
}  // namespace net::test_server
