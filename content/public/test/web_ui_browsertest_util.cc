// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/web_ui_browsertest_util.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/hash/hash.h"
#include "base/memory/ref_counted_memory.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/threading/thread_restrictions.h"
#include "content/browser/webui/web_ui_impl.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/url_constants.h"
#include "net/base/url_util.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "url/gurl.h"

namespace content {

namespace {

void GetResource(const std::string& id,
                 WebUIDataSource::GotDataCallback callback) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  if (id == "error") {
    std::move(callback).Run(nullptr);
    return;
  }

  std::string contents;
  base::FilePath path;
  CHECK(base::PathService::Get(content::DIR_TEST_DATA, &path));
  path = path.AppendASCII(id.substr(0, id.find("?")));
  CHECK(base::ReadFileToString(path, &contents)) << path.value();

  base::RefCountedString* ref_contents = new base::RefCountedString;
  ref_contents->as_string() = contents;
  std::move(callback).Run(ref_contents);
}

struct WebUIControllerConfig {
  WebUIControllerConfig();
  ~WebUIControllerConfig();

  BindingsPolicySet bindings = BindingsPolicySet({BindingsPolicyValue::kWebUi});
  std::string child_src = "child-src 'self' chrome://web-ui-subframe/;";
  bool disable_xfo = false;
  bool disable_trusted_types = false;
  std::vector<std::string> requestable_schemes;
  std::optional<std::vector<std::string>> frame_ancestors;
  std::optional<std::string> supported_scheme;
};

class TestWebUIController : public WebUIController {
 public:
  TestWebUIController(WebUI* web_ui,
                      const GURL& base_url,
                      const WebUIControllerConfig& config);

  TestWebUIController(const TestWebUIController&) = delete;
  void operator=(const TestWebUIController&) = delete;
};

std::unique_ptr<WebUIController> CreateTestWebUIControllerForURL(
    WebUI* web_ui,
    const GURL& url,
    bool disable_xfo) {
  WebUIControllerConfig config;
  config.disable_xfo = disable_xfo;

  if (url.has_query()) {
    std::string value;
    bool has_value = net::GetValueForKeyInQuery(url, "bindings", &value);
    if (has_value) {
      int64_t int_value;
      CHECK(base::StringToInt64(value, &int_value));
      config.bindings = BindingsPolicySet::FromEnumBitmask(int_value);
    }

    has_value = net::GetValueForKeyInQuery(url, "noxfo", &value);
    if (has_value && value == "true") {
      config.disable_xfo = true;
    }

    has_value = net::GetValueForKeyInQuery(url, "notrustedtypes", &value);
    if (has_value && value == "true") {
      config.disable_trusted_types = true;
    }

    has_value = net::GetValueForKeyInQuery(url, "childsrc", &value);
    if (has_value) {
      config.child_src = value;
    }

    has_value = net::GetValueForKeyInQuery(url, "requestableschemes", &value);
    if (has_value) {
      DCHECK(!value.empty());
      std::vector<std::string> schemes = base::SplitString(
          value, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

      config.requestable_schemes.insert(config.requestable_schemes.end(),
                                        schemes.begin(), schemes.end());
    }

    has_value = net::GetValueForKeyInQuery(url, "frameancestors", &value);
    if (has_value) {
      std::vector<std::string> frame_ancestors = base::SplitString(
          value, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

      config.frame_ancestors.emplace(frame_ancestors.begin(),
                                     frame_ancestors.end());
    }
    has_value = net::GetValueForKeyInQuery(url, "supported_scheme", &value);
    if (has_value) {
      config.supported_scheme = value;
    }
  }

  return std::make_unique<TestWebUIController>(web_ui, url, config);
}

}  // namespace

WebUIControllerConfig::WebUIControllerConfig() = default;
WebUIControllerConfig::~WebUIControllerConfig() = default;

TestWebUIController::TestWebUIController(WebUI* web_ui,
                                         const GURL& base_url,
                                         const WebUIControllerConfig& config)
    : WebUIController(web_ui) {
  web_ui->SetBindings(config.bindings);

  WebUIImpl* web_ui_impl = static_cast<WebUIImpl*>(web_ui);
  for (const auto& scheme : config.requestable_schemes) {
    web_ui_impl->AddRequestableScheme(scheme.c_str());
  }

  WebUIDataSource* data_source = WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(), base_url.host());
  data_source->SetRequestFilter(
      base::BindRepeating([](const std::string& path) { return true; }),
      base::BindRepeating(&GetResource));

  data_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ChildSrc, config.child_src);
  if (config.frame_ancestors.has_value()) {
    for (const auto& frame_ancestor : config.frame_ancestors.value()) {
      data_source->AddFrameAncestor(GURL(frame_ancestor));
    }
  }
  if (config.disable_xfo)
    data_source->DisableDenyXFrameOptions();
  if (config.disable_trusted_types)
    data_source->DisableTrustedTypesCSP();
  if (config.supported_scheme) {
    data_source->SetSupportedScheme(config.supported_scheme.value());
  }
}

TestUntrustedDataSourceHeaders::TestUntrustedDataSourceHeaders() = default;
TestUntrustedDataSourceHeaders::TestUntrustedDataSourceHeaders(
    const TestUntrustedDataSourceHeaders& other) = default;
TestUntrustedDataSourceHeaders::~TestUntrustedDataSourceHeaders() = default;

void AddUntrustedDataSource(
    BrowserContext* browser_context,
    const std::string& host,
    std::optional<TestUntrustedDataSourceHeaders> headers) {
  auto* untrusted_data_source = WebUIDataSource::CreateAndAdd(
      browser_context, GetChromeUntrustedUIURL(host).spec());
  untrusted_data_source->SetRequestFilter(
      base::BindRepeating([](const std::string& path) { return true; }),
      base::BindRepeating(&GetResource));
  if (headers.has_value()) {
    if (headers->child_src.has_value()) {
      untrusted_data_source->OverrideContentSecurityPolicy(
          network::mojom::CSPDirectiveName::ChildSrc,
          headers->child_src.value());
    }
    if (headers->script_src.has_value()) {
      untrusted_data_source->OverrideContentSecurityPolicy(
          network::mojom::CSPDirectiveName::ScriptSrc,
          headers->script_src.value());
    }
    if (headers->default_src.has_value()) {
      untrusted_data_source->OverrideContentSecurityPolicy(
          network::mojom::CSPDirectiveName::DefaultSrc,
          headers->default_src.value());
    }
    if (headers->no_trusted_types)
      untrusted_data_source->DisableTrustedTypesCSP();
    if (headers->no_xfo)
      untrusted_data_source->DisableDenyXFrameOptions();
    if (headers->frame_ancestors.has_value()) {
      for (const auto& frame_ancestor : headers->frame_ancestors.value()) {
        untrusted_data_source->AddFrameAncestor(GURL(frame_ancestor));
      }
    }
    if (headers->cross_origin_opener_policy.has_value()) {
      switch (headers->cross_origin_opener_policy.value()) {
        case network::mojom::CrossOriginOpenerPolicyValue::kUnsafeNone:
          break;
        case network::mojom::CrossOriginOpenerPolicyValue::kSameOrigin:
          untrusted_data_source->OverrideCrossOriginOpenerPolicy("same-origin");
          break;
        case network::mojom::CrossOriginOpenerPolicyValue::kSameOriginPlusCoep:
          untrusted_data_source->OverrideCrossOriginOpenerPolicy("same-origin");
          untrusted_data_source->OverrideCrossOriginEmbedderPolicy(
              "require-corp");
          break;
        case network::mojom::CrossOriginOpenerPolicyValue::
            kSameOriginAllowPopups:
        case network::mojom::CrossOriginOpenerPolicyValue::kRestrictProperties:
          NOTREACHED() << "COOP:restrict-properties is not supported in WebUI";
        case network::mojom::CrossOriginOpenerPolicyValue::
            kRestrictPropertiesPlusCoep:
          NOTREACHED()
              << "COOP:restrict-properties-plus-coep is not supported in WebUI";
        case network::mojom::CrossOriginOpenerPolicyValue::kNoopenerAllowPopups:
          NOTREACHED()
              << "COOP:noopener-allow-popups is not supported in WebUI";
      }
    }
  }
}

// static
GURL GetChromeUntrustedUIURL(const std::string& host_and_path) {
  return GURL(std::string(content::kChromeUIUntrustedScheme) +
              url::kStandardSchemeSeparator + host_and_path);
}

TestWebUIConfig::TestWebUIConfig(std::string_view host)
    : WebUIConfig(content::kChromeUIScheme, host) {}

std::unique_ptr<WebUIController> TestWebUIConfig::CreateWebUIController(
    content::WebUI* web_ui,
    const GURL& url) {
  if (!url.SchemeIs(scheme())) {
    return nullptr;
  }

  return CreateTestWebUIControllerForURL(web_ui, GURL(host()), true);
}

TestWebUIControllerFactory::TestWebUIControllerFactory()
    : supported_scheme_(kChromeUIScheme) {}

std::unique_ptr<WebUIController>
TestWebUIControllerFactory::CreateWebUIControllerForURL(WebUI* web_ui,
                                                        const GURL& url) {
  if (!url.SchemeIs(supported_scheme_)) {
    return nullptr;
  }

  return CreateTestWebUIControllerForURL(web_ui, url, disable_xfo_);
}

WebUI::TypeID TestWebUIControllerFactory::GetWebUIType(
    BrowserContext* browser_context,
    const GURL& url) {
  if (!url.SchemeIs(supported_scheme_)) {
    return WebUI::kNoWebUI;
  }

  return reinterpret_cast<WebUI::TypeID>(base::FastHash(url.host()));
}

bool TestWebUIControllerFactory::UseWebUIForURL(BrowserContext* browser_context,
                                                const GURL& url) {
  return GetWebUIType(browser_context, url) != WebUI::kNoWebUI;
}

void TestWebUIControllerFactory::SetSupportedScheme(const std::string& scheme) {
  supported_scheme_ = scheme;
}

}  // namespace content
