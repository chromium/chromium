// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_PAGE_CONTEXT_WRAPPER_TEST_UTILS_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_PAGE_CONTEXT_WRAPPER_TEST_UTILS_H_

#import <string>
#import <vector>

#import "base/memory/weak_ptr.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"

// Enum to manage multiple test origins/servers.
enum class TestOrigin { kMain, kCrossA, kCrossB, kCrossC };

class PageContext;

// Abstract base class for HTML content components.
class HtmlComponent {
 public:
  virtual ~HtmlComponent() = default;

  virtual void PreRegister(PageContext* helper) const {}

  // Resolves the component to an HTML string.
  // `helper` is used to register dependent resources (like iframes) and get
  // their URLs.
  virtual std::string Resolve(PageContext* helper,
                              TestOrigin parent_origin) const = 0;
};

// Represents a paragraph of text.
class ParagraphComponent : public HtmlComponent {
 public:
  explicit ParagraphComponent(std::string text);
  std::string Resolve(PageContext* helper,
                      TestOrigin parent_origin) const override;

 private:
  std::string text_;
};

// Represents a raw HTML string.
class RawHtmlComponent : public HtmlComponent {
 public:
  explicit RawHtmlComponent(std::string html);
  std::string Resolve(PageContext* helper,
                      TestOrigin parent_origin) const override;

 private:
  std::string html_;
};

// Represents an iframe that points to another page content, hosted on a
// specific origin.
class IframeComponent : public HtmlComponent {
 public:
  ~IframeComponent() override;
  // `target_origin`: The origin where the iframe content will be hosted.
  // `content`: The content of the iframe.
  // `test_id`: Optional ID for test URL lookup.
  IframeComponent(TestOrigin target_origin,
                  std::unique_ptr<HtmlComponent> content,
                  std::string test_id = "");

  // Alternative constructor for an existing URL (e.g. infinite loop test).
  explicit IframeComponent(std::string url);

  void PreRegister(PageContext* helper) const override;
  std::string Resolve(PageContext* helper,
                      TestOrigin parent_origin) const override;

  const std::string& test_id() const { return test_id_; }

 private:
  TestOrigin target_origin_ = TestOrigin::kMain;
  std::unique_ptr<HtmlComponent> content_;
  std::string url_;
  std::string test_id_;
};

// Represents a full HTML page with a title and body.
class PageComponent : public HtmlComponent {
 public:
  ~PageComponent() override;
  PageComponent(std::string title,
                std::vector<std::unique_ptr<HtmlComponent>> body_components);

  void PreRegister(PageContext* helper) const override;
  std::string Resolve(PageContext* helper,
                      TestOrigin parent_origin) const override;

 private:
  std::string title_;
  std::vector<std::unique_ptr<HtmlComponent>> body_components_;
};

// Helper for fluent syntax: Paragraph(...)
std::unique_ptr<HtmlComponent> Paragraph(std::string text);

// Helper for fluent syntax: RawHtml(...)
std::unique_ptr<HtmlComponent> RawHtml(std::string html);

// Helper for fluent syntax: Iframe(...)
std::unique_ptr<HtmlComponent> Iframe(TestOrigin origin,
                                      std::unique_ptr<HtmlComponent> content,
                                      std::string test_id = "");

// Helper for fluent syntax: Iframe(url)
std::unique_ptr<HtmlComponent> Iframe(std::string url);

// Helper for fluent syntax: HtmlPage(...) using variadic templates
template <typename... Args>
std::unique_ptr<HtmlComponent> HtmlPage(std::string title, Args... args) {
  std::vector<std::unique_ptr<HtmlComponent>> components;
  components.reserve(sizeof...(args));
  (components.push_back(std::move(args)), ...);
  return std::make_unique<PageComponent>(std::move(title),
                                         std::move(components));
}

// Central helper class to manage server registrations and HTML generation for
// iframes.
class PageContext {
 public:
  PageContext(net::EmbeddedTestServer* main_server,
              net::EmbeddedTestServer* cross_origin_server_a,
              net::EmbeddedTestServer* cross_origin_server_b,
              net::EmbeddedTestServer* cross_origin_server_c);

  ~PageContext();

  // Pre-registers content placeholders on servers without starting them.
  // Content will be filled later at the Resolve() stage.
  void PreRegisterContent(TestOrigin origin,
                          const void* component_id,
                          const std::string& test_id);

  // Handles requests for all servers on all origins by routing the component id
  // to its content.
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const void* component_id,
      const net::test_server::HttpRequest& request);

  // Updates the content for a pre-registered component.
  void UpdateContent(const void* component_id, std::string content);

  // Returns the path/URL for a pre-registered component.
  std::string GetRegisteredUrl(TestOrigin origin,
                               const void* component_id,
                               bool return_relative_path);

  void StartAllServers();

  GURL GetURL(TestOrigin origin, std::string path);

  GURL GetUrlForId(const std::string& test_id);

  std::string Build(const std::unique_ptr<HtmlComponent>& root);

 private:
  net::EmbeddedTestServer* GetServer(TestOrigin origin);

  raw_ptr<net::EmbeddedTestServer> main_server_;
  raw_ptr<net::EmbeddedTestServer> cross_origin_server_a_;
  raw_ptr<net::EmbeddedTestServer> cross_origin_server_b_;
  raw_ptr<net::EmbeddedTestServer> cross_origin_server_c_;
  int counter_ = 0;

  // Maps component ID (pointer) to its mutable content string.
  std::map<const void*, std::unique_ptr<std::string>> content_;
  // Maps component ID to its assigned path.
  std::map<const void*, std::string> component_paths_;
  // Maps string test_id to path.
  std::map<std::string, std::string> test_id_to_path_;
  // Maps string test_id to origin.
  std::map<std::string, TestOrigin> test_id_to_origin_;
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_PAGE_CONTEXT_WRAPPER_TEST_UTILS_H_
