// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_WEB_UI_BROWSERTEST_UTIL_H_
#define CONTENT_PUBLIC_TEST_WEB_UI_BROWSERTEST_UTIL_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_controller_factory.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/bindings_policy.h"
#include "services/network/public/mojom/cross_origin_opener_policy.mojom.h"

namespace content {

struct TestUntrustedDataSourceHeaders {
  TestUntrustedDataSourceHeaders();
  TestUntrustedDataSourceHeaders(const TestUntrustedDataSourceHeaders& other);
  ~TestUntrustedDataSourceHeaders();

  std::optional<std::string> child_src = std::nullopt;
  std::optional<std::string> script_src = std::nullopt;
  std::optional<std::string> default_src = std::nullopt;
  // Trusted Types is enabled by default for TestUntrustedDataSource.
  // Setting this to true will disable Trusted Types.
  bool no_trusted_types = false;
  bool no_xfo = false;
  std::optional<std::vector<std::string>> frame_ancestors = std::nullopt;
  std::optional<network::mojom::CrossOriginOpenerPolicyValue>
      cross_origin_opener_policy = std::nullopt;
};

// Adds a DataSource for chrome-untrusted://|host| URLs.
void AddUntrustedDataSource(
    BrowserContext* browser_context,
    const std::string& host,
    std::optional<TestUntrustedDataSourceHeaders> headers = std::nullopt);

// Returns chrome-untrusted://|host_and_path| as a GURL.
GURL GetChromeUntrustedUIURL(const std::string& host_and_path);

class TestWebUIConfig : public content::WebUIConfig {
 public:
  explicit TestWebUIConfig(std::string_view host);

  ~TestWebUIConfig() override = default;

  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override;
};

// Returns WebUIControllers whose CSPs and headers can be controlled through
// query parameters.
// - "bindings" controls the bindings e.g. Mojo, chrome.send() or both, with
//   which the WebUIController will be created.
// - "noxfo" controls whether the "X-Frame-Options: DENY" header, which is
//   added by default, will be removed. Set to true to remove the header.
// - "childsrc" controls the child-src CSP. It's value is
//   "child-src 'self' chrome://web-ui-subframe/;" by default.
class TestWebUIControllerFactory : public WebUIControllerFactory {
 public:
  TestWebUIControllerFactory();
  TestWebUIControllerFactory(const TestWebUIControllerFactory&) = delete;
  void operator=(const TestWebUIControllerFactory&) = delete;

  // Set to true to remove the "X-Frame-Options: DENY" header from all new
  // WebUIControllers.
  void set_disable_xfo(bool disable) { disable_xfo_ = disable; }

  // WebUIControllerFactory
  std::unique_ptr<WebUIController> CreateWebUIControllerForURL(
      WebUI* web_ui,
      const GURL& url) override;
  WebUI::TypeID GetWebUIType(BrowserContext* browser_context,
                             const GURL& url) override;
  bool UseWebUIForURL(BrowserContext* browser_context,
                      const GURL& url) override;

  void SetSupportedScheme(const std::string& scheme);

 private:
  bool disable_xfo_ = false;

  // Scheme supported by the WebUIControllerFactory.
  std::string supported_scheme_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_WEB_UI_BROWSERTEST_UTIL_H_
