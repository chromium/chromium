// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webui/url_data_manager_backend.h"

#include <set>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "content/browser/webui/shared_resources_data_source.h"
#include "content/browser/webui/url_data_source_impl.h"
#include "content/browser/webui/web_ui_data_source_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/filter/source_stream.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/log/net_log_util.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/template_expressions.h"
#include "ui/base/webui/i18n_source_stream.h"
#include "url/url_util.h"

namespace content {

namespace {

const char kChromeURLContentSecurityPolicyHeaderName[] =
    "Content-Security-Policy";

const char kChromeURLCrossOriginOpenerPolicyName[] =
    "Cross-Origin-Opener-Policy";
const char kChromeURLCrossOriginEmbedderPolicyName[] =
    "Cross-Origin-Embedder-Policy";
const char kChromeURLCrossOriginResourcePolicyName[] =
    "Cross-Origin-Resource-Policy";

const char kChromeURLXFrameOptionsHeaderName[] = "X-Frame-Options";
const char kChromeURLXFrameOptionsHeaderValue[] = "DENY";
const char kNetworkErrorKey[] = "netError";
const char kURLDataManagerBackendKeyName[] = "url_data_manager_backend";

bool SchemeIsInSchemes(const std::string& scheme,
                       const std::vector<std::string>& schemes) {
  return base::Contains(schemes, scheme);
}

bool g_disallow_webui_scheme_caching_for_testing = false;

std::vector<std::string> GetWebUISchemesSlow() {
  std::vector<std::string> schemes;
  schemes.emplace_back(kChromeUIScheme);
  schemes.emplace_back(kChromeUIUntrustedScheme);
  GetContentClient()->browser()->GetAdditionalWebUISchemes(&schemes);
  return schemes;
}

std::vector<std::string> GetWebUISchemesCached() {
  // It's OK to cache this in a static because the class implementing
  // GetAdditionalWebUISchemes() won't change while the application is
  // running, and because those methods always add the same items.
  //
  // However, be careful using this with unit tests which use
  // GetAdditionalWebUISchemes() to change the list of WebUI schemes, since
  // this caching may persist across tests. For those, this caching should be
  // disabled via SetDisallowWebUISchemeCachingForTesting().
  static base::NoDestructor<std::vector<std::string>> webui_schemes(
      GetWebUISchemesSlow());

  return *webui_schemes;
}

}  // namespace

URLDataManagerBackend::URLDataManagerBackend() {
  {
    // Add a shared data source for chrome://resources.
    auto* source = new WebUIDataSourceImpl(kChromeUIResourcesHost);
    PopulateSharedResourcesDataSource(source);
    AddDataSource(source);  // Takes ownership.
  }

  {
    // Add a shared data source for chrome-untrusted://resources.
    auto* source = new WebUIDataSourceImpl(kChromeUIUntrustedResourcesURL);
    PopulateSharedResourcesDataSource(source);
    AddDataSource(source);  // Takes ownership.
  }
}

URLDataManagerBackend::~URLDataManagerBackend() = default;

URLDataManagerBackend* URLDataManagerBackend::GetForBrowserContext(
    BrowserContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!context->GetUserData(kURLDataManagerBackendKeyName)) {
    context->SetUserData(kURLDataManagerBackendKeyName,
                         std::make_unique<URLDataManagerBackend>());
  }
  return static_cast<URLDataManagerBackend*>(
      context->GetUserData(kURLDataManagerBackendKeyName));
}

void URLDataManagerBackend::AddDataSource(URLDataSourceImpl* source) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!source->source()->ShouldReplaceExistingSource()) {
    auto i = data_sources_.find(source->source_name());
    if (i != data_sources_.end())
      return;
  }
  data_sources_[source->source_name()] = source;
  source->backend_ = weak_factory_.GetWeakPtr();
}

void URLDataManagerBackend::UpdateWebUIDataSource(
    const std::string& source_name,
    const base::Value::Dict& update) {
  auto it = data_sources_.find(source_name);
  if (it == data_sources_.end() || !it->second->IsWebUIDataSourceImpl()) {
    NOTREACHED_IN_MIGRATION();
    return;
  }
  static_cast<WebUIDataSourceImpl*>(it->second.get())
      ->AddLocalizedStrings(update);
}

URLDataSourceImpl* URLDataManagerBackend::GetDataSourceFromURL(
    const GURL& url) {
  // chrome-untrusted:// sources keys are of the form "chrome-untrusted://host".
  if (url.scheme() == kChromeUIUntrustedScheme) {
    auto i = data_sources_.find(url.DeprecatedGetOriginAsURL().spec());
    if (i == data_sources_.end())
      return nullptr;
    return i->second.get();
  }

  // The input usually looks like: chrome://source_name/extra_bits?foo
  // so do a lookup using the host of the URL.
  auto i = data_sources_.find(url.host());
  if (i != data_sources_.end())
    return i->second.get();

  // No match using the host of the URL, so do a lookup using the scheme for
  // URLs on the form source_name://extra_bits/foo .
  i = data_sources_.find(url.scheme() + "://");
  if (i != data_sources_.end())
    return i->second.get();

  // No matches found, so give up.
  return nullptr;
}

scoped_refptr<net::HttpResponseHeaders> URLDataManagerBackend::GetHeaders(
    URLDataSourceImpl* source_impl,
    const GURL& url,
    const std::string& origin) {
  // Set the headers so that requests serviced by ChromeURLDataManager return a
  // status code of 200. Without this they return a 0, which makes the status
  // indistiguishable from other error types. Instant relies on getting a 200.
  auto headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  if (!source_impl)
    return headers;

  URLDataSource* source = source_impl->source();
  // Determine the least-privileged content security policy header, if any,
  // that is compatible with a given WebUI URL, and append it to the existing
  // response headers.
  if (source->ShouldAddContentSecurityPolicy()) {
    std::string csp_header;

    const network::mojom::CSPDirectiveName kAllDirectives[] = {
        network::mojom::CSPDirectiveName::BaseURI,
        network::mojom::CSPDirectiveName::ChildSrc,
        network::mojom::CSPDirectiveName::ConnectSrc,
        network::mojom::CSPDirectiveName::DefaultSrc,
        network::mojom::CSPDirectiveName::FencedFrameSrc,
        network::mojom::CSPDirectiveName::FormAction,
        network::mojom::CSPDirectiveName::FontSrc,
        network::mojom::CSPDirectiveName::FrameSrc,
        network::mojom::CSPDirectiveName::ImgSrc,
        network::mojom::CSPDirectiveName::MediaSrc,
        network::mojom::CSPDirectiveName::ObjectSrc,
        network::mojom::CSPDirectiveName::RequireTrustedTypesFor,
        network::mojom::CSPDirectiveName::ScriptSrc,
        network::mojom::CSPDirectiveName::StyleSrc,
        network::mojom::CSPDirectiveName::TrustedTypes,
        network::mojom::CSPDirectiveName::WorkerSrc};

    for (auto& directive : kAllDirectives) {
      csp_header.append(source->GetContentSecurityPolicy(directive));
    }

    // TODO(crbug.com/40118579): Both CSP frame ancestors and XFO headers may be
    // added to the response but frame ancestors would take precedence. In the
    // future, XFO will be removed so when that happens remove the check and
    // always add frame ancestors.
    if (source->ShouldDenyXFrameOptions()) {
      csp_header.append(source->GetContentSecurityPolicy(
          network::mojom::CSPDirectiveName::FrameAncestors));
    }

    headers->SetHeader(kChromeURLContentSecurityPolicyHeaderName, csp_header);
  }

  if (source->ShouldDenyXFrameOptions()) {
    headers->SetHeader(kChromeURLXFrameOptionsHeaderName,
                       kChromeURLXFrameOptionsHeaderValue);
  }

  if (!source->AllowCaching())
    headers->SetHeader("Cache-Control", "no-cache");

  std::string mime_type = source->GetMimeType(url);
  if (source->ShouldServeMimeTypeAsContentTypeHeader() && !mime_type.empty())
    headers->SetHeader(net::HttpRequestHeaders::kContentType, mime_type);

  const std::string coop_value = source->GetCrossOriginOpenerPolicy();
  if (!coop_value.empty()) {
    headers->SetHeader(kChromeURLCrossOriginOpenerPolicyName, coop_value);
  }
  const std::string coep_value = source->GetCrossOriginEmbedderPolicy();
  if (!coep_value.empty()) {
    headers->SetHeader(kChromeURLCrossOriginEmbedderPolicyName, coep_value);
  }
  const std::string corp_value = source->GetCrossOriginResourcePolicy();
  if (!corp_value.empty()) {
    headers->SetHeader(kChromeURLCrossOriginResourcePolicyName, corp_value);
  }

  if (!origin.empty()) {
    std::string header = source->GetAccessControlAllowOriginForOrigin(origin);
    DCHECK(header.empty() || header == origin || header == "*" ||
           header == "null");
    if (!header.empty()) {
      headers->SetHeader("Access-Control-Allow-Origin", header);
      headers->SetHeader("Vary", "Origin");
    }
  }

  return headers;
}

bool URLDataManagerBackend::CheckURLIsValid(const GURL& url) {
  std::vector<std::string> additional_schemes;
  DCHECK(url.SchemeIs(kChromeUIScheme) ||
         url.SchemeIs(kChromeUIUntrustedScheme) ||
         (GetContentClient()->browser()->GetAdditionalWebUISchemes(
              &additional_schemes),
          SchemeIsInSchemes(url.scheme(), additional_schemes)));

  if (!url.is_valid()) {
    NOTREACHED_IN_MIGRATION();
    return false;
  }

  return true;
}

bool URLDataManagerBackend::IsValidNetworkErrorCode(int error_code) {
  base::Value::Dict error_codes = net::GetNetConstants();
  const base::Value::Dict* net_error_codes_dict =
      error_codes.FindDict(kNetworkErrorKey);

  if (net_error_codes_dict != nullptr) {
    for (auto it = net_error_codes_dict->begin();
         it != net_error_codes_dict->end(); ++it) {
      if (error_code == it->second.GetInt())
        return true;
    }
  }
  return false;
}

std::vector<std::string> URLDataManagerBackend::GetWebUISchemes() {
  if (g_disallow_webui_scheme_caching_for_testing) {
    return GetWebUISchemesSlow();
  }

  return GetWebUISchemesCached();
}

void URLDataManagerBackend::SetDisallowWebUISchemeCachingForTesting(
    bool disallow_caching) {
  g_disallow_webui_scheme_caching_for_testing = disallow_caching;
}

}  // namespace content
