// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/url_data_source.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "content/browser/webui/url_data_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/url_constants.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"

namespace {
// A chrome-untrusted data source's name starts with chrome-untrusted://.
bool IsChromeUntrustedDataSource(content::URLDataSource* source) {
  static const base::NoDestructor<std::string> kChromeUntrustedSourceNamePrefix(
      base::StrCat(
          {content::kChromeUIUntrustedScheme, url::kStandardSchemeSeparator}));

  return base::StartsWith(source->GetSource(),
                          *kChromeUntrustedSourceNamePrefix,
                          base::CompareCase::SENSITIVE);
}
}  // namespace

namespace content {

// static
void URLDataSource::Add(BrowserContext* browser_context,
                        std::unique_ptr<URLDataSource> source) {
  URLDataManager::AddDataSource(browser_context, std::move(source));
}

// static
std::string URLDataSource::URLToRequestPath(const GURL& url) {
  const std::string& spec = url.possibly_invalid_spec();
  const url::Parsed& parsed = url.parsed_for_possibly_invalid_spec();
  // + 1 to skip the slash at the beginning of the path.
  int offset = parsed.CountCharactersBefore(url::Parsed::PATH, false) + 1;

  if (offset < static_cast<int>(spec.size()))
    return spec.substr(offset);

  return std::string();
}

bool URLDataSource::ShouldReplaceExistingSource() {
  return true;
}

bool URLDataSource::AllowCaching() {
  return true;
}

bool URLDataSource::ShouldAddContentSecurityPolicy() {
  return true;
}

std::string URLDataSource::GetContentSecurityPolicy(
    network::mojom::CSPDirectiveName directive) {
  switch (directive) {
    case network::mojom::CSPDirectiveName::ChildSrc:
      return "child-src 'none';";
    case network::mojom::CSPDirectiveName::DefaultSrc:
      return IsChromeUntrustedDataSource(this) ? "default-src 'self';"
                                               : std::string();
    case network::mojom::CSPDirectiveName::ObjectSrc:
      return "object-src 'none';";
    case network::mojom::CSPDirectiveName::ScriptSrc:
      // Note: Do not add 'unsafe-eval' here. Instead override CSP for the
      // specific pages that need it, see context http://crbug.com/525224.
      return IsChromeUntrustedDataSource(this)
                 ? "script-src chrome-untrusted://resources 'self';"
                 : "script-src chrome://resources 'self';";
    case network::mojom::CSPDirectiveName::FrameAncestors:
      return "frame-ancestors 'none';";
    case network::mojom::CSPDirectiveName::RequireTrustedTypesFor:
      return "require-trusted-types-for 'script';";
    case network::mojom::CSPDirectiveName::TrustedTypes:
      return "trusted-types;";
    case network::mojom::CSPDirectiveName::BaseURI:
      return IsChromeUntrustedDataSource(this) ? "base-uri 'none';"
                                               : std::string();
    case network::mojom::CSPDirectiveName::FormAction:
      return IsChromeUntrustedDataSource(this) ? "form-action 'none';"
                                               : std::string();
    case network::mojom::CSPDirectiveName::BlockAllMixedContent:
    case network::mojom::CSPDirectiveName::ConnectSrc:
    case network::mojom::CSPDirectiveName::FencedFrameSrc:
    case network::mojom::CSPDirectiveName::FrameSrc:
    case network::mojom::CSPDirectiveName::FontSrc:
    case network::mojom::CSPDirectiveName::ImgSrc:
    case network::mojom::CSPDirectiveName::ManifestSrc:
    case network::mojom::CSPDirectiveName::MediaSrc:
    case network::mojom::CSPDirectiveName::ReportURI:
    case network::mojom::CSPDirectiveName::Sandbox:
    case network::mojom::CSPDirectiveName::ScriptSrcAttr:
    case network::mojom::CSPDirectiveName::ScriptSrcElem:
    case network::mojom::CSPDirectiveName::StyleSrc:
    case network::mojom::CSPDirectiveName::StyleSrcAttr:
    case network::mojom::CSPDirectiveName::StyleSrcElem:
    case network::mojom::CSPDirectiveName::UpgradeInsecureRequests:
    case network::mojom::CSPDirectiveName::TreatAsPublicAddress:
    case network::mojom::CSPDirectiveName::WorkerSrc:
    case network::mojom::CSPDirectiveName::ReportTo:
    case network::mojom::CSPDirectiveName::Unknown:
      return std::string();
  }
}

std::string URLDataSource::GetCrossOriginOpenerPolicy() {
  return std::string();
}

std::string URLDataSource::GetCrossOriginEmbedderPolicy() {
  return std::string();
}

std::string URLDataSource::GetCrossOriginResourcePolicy() {
  return std::string();
}

bool URLDataSource::ShouldDenyXFrameOptions() {
  return true;
}

bool URLDataSource::ShouldServiceRequest(const GURL& url,
                                         BrowserContext* browser_context,
                                         int render_process_id) {
  return url.SchemeIs(kChromeDevToolsScheme) || url.SchemeIs(kChromeUIScheme) ||
         url.SchemeIs(kChromeUIUntrustedScheme);
}

bool URLDataSource::ShouldServeMimeTypeAsContentTypeHeader() {
  return false;
}

std::string URLDataSource::GetAccessControlAllowOriginForOrigin(
    const std::string& origin) {
  return std::string();
}

const ui::TemplateReplacements* URLDataSource::GetReplacements() {
  return nullptr;
}

bool URLDataSource::ShouldReplaceI18nInJS() {
  return false;
}

}  // namespace content
