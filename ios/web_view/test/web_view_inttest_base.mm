// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/test/web_view_inttest_base.h"

#import <ChromeWebView/ChromeWebView.h>
#import <Foundation/Foundation.h>

#include "base/base64.h"
#include "base/functional/bind.h"
#import "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#import "ios/web/common/uikit_ui_util.h"
#import "ios/web_view/test/web_view_test_util.h"
#include "net/base/url_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Test server path which renders a basic html page.
const char kPageHtmlPath[] = "/PageHtml?";
// URL parameter for entire html. Value must be base64 encoded.
// kPageHtmlBodyParamName and kPageHtmlTitleParamName are ignored when this is
// given.
const char kPageHtmlParamName[] = "html";
// URL parameter for html body. Value must be base64 encoded.
const char kPageHtmlBodyParamName[] = "body";
// URL parameter for page title. Value must be base64 encoded.
const char kPageHtmlTitleParamName[] = "title";

// Generates html from title and body.
std::string CreatePageHTML(const std::string& title, const std::string& body) {
  return base::StringPrintf(
      "<html><head><title>%s</title></head><body>%s</body></html>",
      title.c_str(), body.c_str());
}

// Returns true if |string| starts with |prefix|. String comparison is case
// insensitive.
bool StartsWith(std::string string, std::string prefix) {
  return base::StartsWith(string, prefix, base::CompareCase::SENSITIVE);
}

// Encodes the |string| for use as the value of a url parameter.
std::string EncodeQueryParamValue(std::string string) {
  return base::Base64Encode(string);
}

// Decodes the |encoded_string|. Undoes the encoding performed by
// |EncodeQueryParamValue|.
std::string DecodeQueryParamValue(std::string encoded_string) {
  std::string decoded_string;
  base::Base64Decode(encoded_string, &decoded_string);
  return decoded_string;
}

// Maps test server requests to responses.
std::unique_ptr<net::test_server::HttpResponse> TestRequestHandler(
    const net::test_server::HttpRequest& request) {
  if (StartsWith(request.relative_url, kPageHtmlPath)) {
    std::string title;
    std::string body;
    std::string html;

    GURL request_url = request.GetURL();

    std::string encoded_html;
    bool html_found = net::GetValueForKeyInQuery(
        request_url, kPageHtmlParamName, &encoded_html);
    if (html_found) {
      html = DecodeQueryParamValue(encoded_html);
    } else {
      std::string encoded_title;
      bool title_found = net::GetValueForKeyInQuery(
          request_url, kPageHtmlTitleParamName, &encoded_title);
      if (title_found) {
        title = DecodeQueryParamValue(encoded_title);
      }

      std::string encoded_body;
      bool body_found = net::GetValueForKeyInQuery(
          request_url, kPageHtmlBodyParamName, &encoded_body);
      if (body_found) {
        body = DecodeQueryParamValue(encoded_body);
      }

      html = CreatePageHTML(title, body);
    }

    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_content(html);
    return std::move(http_response);
  }
  return nullptr;
}

}  // namespace

namespace ios_web_view {

WebViewInttestBase::WebViewInttestBase()
    : web_view_(test::CreateWebView()),
      test_server_(std::make_unique<net::EmbeddedTestServer>(
          net::test_server::EmbeddedTestServer::TYPE_HTTP)) {
  // The WKWebView must be present in the view hierarchy in order to prevent
  // WebKit optimizations which may pause internal parts of the web view
  // without notice. Work around this by adding the view directly.
  UIViewController* view_controller = [GetAnyKeyWindow() rootViewController];
  [view_controller.view addSubview:web_view_];

  test_server_->AddDefaultHandlers(FILE_PATH_LITERAL(base::FilePath()));
  test_server_->RegisterRequestHandler(
      base::BindRepeating(&TestRequestHandler));
}

WebViewInttestBase::~WebViewInttestBase() {
  [web_view_ removeFromSuperview];
}

GURL WebViewInttestBase::GetUrlForPageWithTitle(const std::string& title) {
  return GetUrlForPageWithTitleAndBody(title, std::string());
}

GURL WebViewInttestBase::GetUrlForPageWithHtmlBody(const std::string& html) {
  return GetUrlForPageWithTitleAndBody(std::string(), html);
}

GURL WebViewInttestBase::GetUrlForPageWithTitleAndBody(
    const std::string& title,
    const std::string& body) {
  GURL url = test_server_->GetURL(kPageHtmlPath);

  // Encode |title| and |body| in url query in order to build the server
  // response later in TestRequestHandler.
  std::string encoded_title = EncodeQueryParamValue(title);
  url = net::AppendQueryParameter(url, kPageHtmlTitleParamName, encoded_title);
  std::string encoded_body = EncodeQueryParamValue(body);
  url = net::AppendQueryParameter(url, kPageHtmlBodyParamName, encoded_body);

  return url;
}

GURL WebViewInttestBase::GetUrlForPageWithHtml(const std::string& html) {
  GURL url = test_server_->GetURL(kPageHtmlPath);

  // Encode |html| in url query in order to build the server
  // response later in TestRequestHandler.
  std::string encoded_html = EncodeQueryParamValue(html);
  url = net::AppendQueryParameter(url, kPageHtmlParamName, encoded_html);

  return url;
}

}  // namespace ios_web_view
