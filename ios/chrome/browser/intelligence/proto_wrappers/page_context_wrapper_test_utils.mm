// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper_test_utils.h"

#import "base/check.h"
#import "base/strings/strcat.h"
#import "base/strings/stringprintf.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "net/test/embedded_test_server/request_handler_util.h"

namespace {
// Helper function to return an HttpResponse with the given HTML content.
std::unique_ptr<net::test_server::HttpResponse> HandlePageWithHtml(
    const std::string& html,
    const net::test_server::HttpRequest& request) {
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_content_type("text/html");
  http_response->set_content(html);
  return http_response;
}
}  // namespace

// ParagraphComponent Implementation
ParagraphComponent::ParagraphComponent(std::string text)
    : text_(std::move(text)) {}

std::string ParagraphComponent::Resolve(PageContext* helper,
                                        TestOrigin parent_origin) const {
  return base::StrCat({"<p>", text_, "</p>"});
}

// RawHtmlComponent Implementation
RawHtmlComponent::RawHtmlComponent(std::string html) : html_(std::move(html)) {}

std::string RawHtmlComponent::Resolve(PageContext* helper,
                                      TestOrigin parent_origin) const {
  return html_;
}

// IframeComponent Implementation
IframeComponent::~IframeComponent() = default;

IframeComponent::IframeComponent(TestOrigin target_origin,
                                 std::unique_ptr<HtmlComponent> content,
                                 std::string test_id)
    : target_origin_(target_origin),
      content_(std::move(content)),
      test_id_(std::move(test_id)) {}

IframeComponent::IframeComponent(std::string url) : url_(std::move(url)) {}

void IframeComponent::PreRegister(PageContext* helper) const {
  if (url_.empty()) {
    // Recurse first so children get registered too.
    content_->PreRegister(helper);
    // Register this iframe.
    helper->PreRegisterContent(target_origin_, this, test_id_);
  }
}

std::string IframeComponent::Resolve(PageContext* helper,
                                     TestOrigin parent_origin) const {
  std::string src;
  if (!url_.empty()) {
    src = url_;
  } else {
    // Recursively resolve the content (now servers are running, so GetURL
    // works).
    std::string content_html = content_->Resolve(helper, target_origin_);

    // Update the registered content with the resolved HTML.
    helper->UpdateContent(this, content_html);

    // If frames are on same origin, use relative path to avoid full URL.
    bool use_relative = (target_origin_ == parent_origin);
    src = helper->GetRegisteredUrl(target_origin_, this, use_relative);
  }
  return base::StrCat({"<iframe src=\"", src, "\"></iframe>"});
}

// PageComponent Implementation
PageComponent::~PageComponent() = default;

PageComponent::PageComponent(
    std::string title,
    std::vector<std::unique_ptr<HtmlComponent>> body_components)
    : title_(std::move(title)), body_components_(std::move(body_components)) {}

void PageComponent::PreRegister(PageContext* helper) const {
  for (const auto& comp : body_components_) {
    comp->PreRegister(helper);
  }
}

std::string PageComponent::Resolve(PageContext* helper,
                                   TestOrigin parent_origin) const {
  std::string body;
  for (const auto& comp : body_components_) {
    base::StrAppend(&body, {comp->Resolve(helper, parent_origin)});
  }
  return base::StrCat({"<html><head><title>", title_, "</title></head><body>",
                       body, "</body></html>"});
}

// Helpers
std::unique_ptr<HtmlComponent> Paragraph(std::string text) {
  return std::make_unique<ParagraphComponent>(std::move(text));
}

std::unique_ptr<HtmlComponent> RawHtml(std::string html) {
  return std::make_unique<RawHtmlComponent>(std::move(html));
}

std::unique_ptr<HtmlComponent> Iframe(TestOrigin origin,
                                      std::unique_ptr<HtmlComponent> content,
                                      std::string test_id) {
  return std::make_unique<IframeComponent>(origin, std::move(content),
                                           std::move(test_id));
}

std::unique_ptr<HtmlComponent> Iframe(std::string url) {
  return std::make_unique<IframeComponent>(std::move(url));
}

// PageContext Implementation
PageContext::PageContext(net::EmbeddedTestServer* main_server,
                         net::EmbeddedTestServer* cross_origin_server_a,
                         net::EmbeddedTestServer* cross_origin_server_b,
                         net::EmbeddedTestServer* cross_origin_server_c)
    : main_server_(main_server),
      cross_origin_server_a_(cross_origin_server_a),
      cross_origin_server_b_(cross_origin_server_b),
      cross_origin_server_c_(cross_origin_server_c) {}

PageContext::~PageContext() = default;

void PageContext::PreRegisterContent(TestOrigin origin,
                                     const void* component_id,
                                     const std::string& test_id) {
  net::EmbeddedTestServer* server = GetServer(origin);
  std::string path = base::StringPrintf("/generated_%d.html", counter_++);

  // Create a mutable content for this component owned by the helper.
  auto content_holder = std::make_unique<std::string>();
  content_[component_id] = std::move(content_holder);
  component_paths_[component_id] = path;
  if (!test_id.empty()) {
    test_id_to_path_[test_id] = path;
    test_id_to_origin_[test_id] = origin;
  }

  // Register a handler that returns the *current* value of the content.
  // We use base::Unretained because the PageContext helper outlives the
  // server (and its IO thread where this callback runs).
  server->RegisterRequestHandler(base::BindRepeating(
      &net::test_server::HandlePrefixedRequest, path,
      base::BindRepeating(
          [](PageContext* helper, const void* component_id,
             const net::test_server::HttpRequest& request)
              -> std::unique_ptr<net::test_server::HttpResponse> {
            return helper->HandleRequest(component_id, request);
          },
          base::Unretained(this), component_id)));
}

std::unique_ptr<net::test_server::HttpResponse> PageContext::HandleRequest(
    const void* component_id,
    const net::test_server::HttpRequest& request) {
  auto it = content_.find(component_id);
  if (it == content_.end()) {
    return nullptr;
  }
  return HandlePageWithHtml(*(it->second), request);
}

void PageContext::UpdateContent(const void* component_id, std::string content) {
  auto it = content_.find(component_id);
  CHECK(it != content_.end()) << "Component not pre-registered";
  *(it->second) = std::move(content);
}

std::string PageContext::GetRegisteredUrl(TestOrigin origin,
                                          const void* component_id,
                                          bool return_relative_path) {
  auto it = component_paths_.find(component_id);
  CHECK(it != component_paths_.end()) << "Component not pre-registered";
  std::string path = it->second;

  if (!return_relative_path) {
    net::EmbeddedTestServer* server = GetServer(origin);
    CHECK(server->Started());
    return server->GetURL(path).spec();
  }
  return path;
}

void PageContext::StartAllServers() {
  if (!main_server_->Started()) {
    CHECK(main_server_->Start());
  }
  if (!cross_origin_server_a_->Started()) {
    CHECK(cross_origin_server_a_->Start());
  }
  if (!cross_origin_server_b_->Started()) {
    CHECK(cross_origin_server_b_->Start());
  }
  if (!cross_origin_server_c_->Started()) {
    CHECK(cross_origin_server_c_->Start());
  }
}

GURL PageContext::GetURL(TestOrigin origin, std::string path) {
  net::EmbeddedTestServer* server = GetServer(origin);
  CHECK(server->Started());
  return server->GetURL(path);
}

GURL PageContext::GetUrlForId(const std::string& test_id) {
  auto path_it = test_id_to_path_.find(test_id);
  CHECK(path_it != test_id_to_path_.end()) << "Test ID not found: " << test_id;
  auto origin_it = test_id_to_origin_.find(test_id);
  CHECK(origin_it != test_id_to_origin_.end());
  return GetURL(origin_it->second, path_it->second);
}

std::string PageContext::Build(const std::unique_ptr<HtmlComponent>& root) {
  // Pass 1: Pre-register all handlers to establish paths and ports
  // requirements.
  root->PreRegister(this);

  // Start everything now that handlers are registered.
  StartAllServers();

  // Pass 2: Resolve content (which now has access to real ports) and update
  // content for handlers dynamically.
  return root->Resolve(this, TestOrigin::kMain);
}

net::EmbeddedTestServer* PageContext::GetServer(TestOrigin origin) {
  switch (origin) {
    case TestOrigin::kMain:
      return main_server_;
    case TestOrigin::kCrossA:
      return cross_origin_server_a_;
    case TestOrigin::kCrossB:
      return cross_origin_server_b_;
    case TestOrigin::kCrossC:
      return cross_origin_server_c_;
  }
}
