// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webui/web_ui_data_source_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "content/grit/content_resources.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/template_expressions.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "ui/base/webui/resource_path.h"
#include "ui/base/webui/web_ui_util.h"

namespace content {

// static
WebUIDataSource* WebUIDataSource::CreateAndAdd(BrowserContext* browser_context,
                                               const std::string& source_name) {
  auto* data_source = new WebUIDataSourceImpl(source_name);
  URLDataManager::AddWebUIDataSource(browser_context, data_source);
  return data_source;
}

// static
void WebUIDataSource::Update(BrowserContext* browser_context,
                             const std::string& source_name,
                             const base::Value::Dict& update) {
  URLDataManager::UpdateWebUIDataSource(browser_context, source_name,
                                        std::move(update));
}

namespace {

void GetDataResourceBytesOnWorkerThread(
    int resource_id,
    URLDataSource::GotDataCallback callback) {
  base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN, base::MayBlock()})
      ->PostTask(
          FROM_HERE,
          base::BindOnce(
              [](int resource_id, URLDataSource::GotDataCallback callback) {
                ContentClient* content_client = GetContentClient();
                DCHECK(content_client);
                std::move(callback).Run(
                    content_client->GetDataResourceBytes(resource_id));
              },
              resource_id, std::move(callback)));
}

const int kNonExistentResource = -1;

}  // namespace

// Internal class to hide the fact that WebUIDataSourceImpl implements
// URLDataSource.
class WebUIDataSourceImpl::InternalDataSource : public URLDataSource {
 public:
  explicit InternalDataSource(WebUIDataSourceImpl* parent) : parent_(parent) {}

  ~InternalDataSource() override {}

  // URLDataSource implementation.
  std::string GetSource() override { return parent_->GetSource(); }
  std::string GetMimeType(const GURL& url) override {
    return parent_->GetMimeType(url);
  }
  void StartDataRequest(const GURL& url,
                        const WebContents::Getter& wc_getter,
                        URLDataSource::GotDataCallback callback) override {
    return parent_->StartDataRequest(url, wc_getter, std::move(callback));
  }
  bool AllowCaching() override { return false; }
  std::string GetContentSecurityPolicy(
      network::mojom::CSPDirectiveName directive) override {
    if (parent_->csp_overrides_.contains(directive)) {
      return parent_->csp_overrides_.at(directive);
    } else if (directive == network::mojom::CSPDirectiveName::FrameAncestors) {
      std::string frame_ancestors;
      if (parent_->frame_ancestors_.size() == 0)
        frame_ancestors += " 'none'";
      for (const GURL& frame_ancestor : parent_->frame_ancestors_) {
        frame_ancestors += " " + frame_ancestor.spec();
      }
      return "frame-ancestors" + frame_ancestors + ";";
    }

    return URLDataSource::GetContentSecurityPolicy(directive);
  }
  std::string GetCrossOriginOpenerPolicy() override {
    return parent_->coop_value_;
  }
  std::string GetCrossOriginEmbedderPolicy() override {
    return parent_->coep_value_;
  }
  std::string GetCrossOriginResourcePolicy() override {
    return parent_->corp_value_;
  }
  bool ShouldDenyXFrameOptions() override {
    return parent_->deny_xframe_options_;
  }
  bool ShouldServeMimeTypeAsContentTypeHeader() override { return true; }
  const ui::TemplateReplacements* GetReplacements() override {
    return &parent_->replacements_;
  }
  bool ShouldReplaceI18nInJS() override {
    return parent_->ShouldReplaceI18nInJS();
  }
  bool ShouldServiceRequest(const GURL& url,
                            BrowserContext* browser_context,
                            int render_process_id) override {
    if (parent_->supported_scheme_.has_value()) {
      return url.SchemeIs(parent_->supported_scheme_.value());
    }

    return URLDataSource::ShouldServiceRequest(url, browser_context,
                                               render_process_id);
  }

 private:
  raw_ptr<WebUIDataSourceImpl> parent_;
};

WebUIDataSourceImpl::WebUIDataSourceImpl(const std::string& source_name)
    : URLDataSourceImpl(source_name,
                        std::make_unique<InternalDataSource>(this)),
      source_name_(source_name),
      default_resource_(kNonExistentResource) {}

WebUIDataSourceImpl::~WebUIDataSourceImpl() = default;

void WebUIDataSourceImpl::AddString(std::string_view name,
                                    std::u16string_view value) {
  // TODO(dschuyler): Share only one copy of these strings.
  localized_strings_.Set(name, value);
  replacements_[std::string(name)] = base::UTF16ToUTF8(value);
}

void WebUIDataSourceImpl::AddString(std::string_view name,
                                    std::string_view value) {
  localized_strings_.Set(name, value);
  replacements_[std::string(name)] = value;
}

void WebUIDataSourceImpl::AddLocalizedString(std::string_view name, int ids) {
  std::string utf8_str =
      base::UTF16ToUTF8(GetContentClient()->GetLocalizedString(ids));
  localized_strings_.Set(name, utf8_str);
  replacements_[std::string(name)] = utf8_str;
}

void WebUIDataSourceImpl::AddLocalizedStrings(
    base::span<const webui::LocalizedString> strings) {
  for (const auto& str : strings)
    AddLocalizedString(str.name, str.id);
}

void WebUIDataSourceImpl::AddLocalizedStrings(
    const base::Value::Dict& localized_strings) {
  localized_strings_.Merge(localized_strings.Clone());
  ui::TemplateReplacementsFromDictionaryValue(localized_strings,
                                              &replacements_);
}

void WebUIDataSourceImpl::AddBoolean(std::string_view name, bool value) {
  localized_strings_.Set(name, value);
  // TODO(dschuyler): Change name of |localized_strings_| to |load_time_data_|
  // or similar. These values haven't been found as strings for
  // localization. The boolean values are not added to |replacements_|
  // for the same reason, that they are used as flags, rather than string
  // replacements.
}

void WebUIDataSourceImpl::AddInteger(std::string_view name, int32_t value) {
  localized_strings_.Set(name, value);
}

void WebUIDataSourceImpl::AddDouble(std::string_view name, double value) {
  localized_strings_.Set(name, value);
}

void WebUIDataSourceImpl::UseStringsJs() {
  use_strings_js_ = true;
}

void WebUIDataSourceImpl::AddResourcePath(std::string_view path,
                                          int resource_id) {
  path_to_idr_map_[std::string(path)] = resource_id;
}

void WebUIDataSourceImpl::AddResourcePaths(
    base::span<const webui::ResourcePath> paths) {
  for (const auto& path : paths)
    AddResourcePath(path.path, path.id);
}

void WebUIDataSourceImpl::SetDefaultResource(int resource_id) {
  default_resource_ = resource_id;
}

void WebUIDataSourceImpl::SetRequestFilter(
    const ShouldHandleRequestCallback& should_handle_request_callback,
    const HandleRequestCallback& handle_request_callback) {
  CHECK(!should_handle_request_callback_);
  CHECK(!filter_callback_);
  should_handle_request_callback_ = should_handle_request_callback;
  filter_callback_ = handle_request_callback;
}

bool WebUIDataSourceImpl::IsWebUIDataSourceImpl() const {
  return true;
}

void WebUIDataSourceImpl::OverrideContentSecurityPolicy(
    network::mojom::CSPDirectiveName directive,
    const std::string& value) {
  csp_overrides_.insert_or_assign(directive, value);
}

void WebUIDataSourceImpl::OverrideCrossOriginOpenerPolicy(
    const std::string& value) {
  coop_value_ = value;
}

void WebUIDataSourceImpl::OverrideCrossOriginEmbedderPolicy(
    const std::string& value) {
  coep_value_ = value;
}

void WebUIDataSourceImpl::OverrideCrossOriginResourcePolicy(
    const std::string& value) {
  corp_value_ = value;
}

void WebUIDataSourceImpl::DisableTrustedTypesCSP() {
  // TODO(crbug.com/40137141): Trusted Type remaining WebUI
  // This removes require-trusted-types-for and trusted-types directives
  // from the CSP header.
  OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::RequireTrustedTypesFor, std::string());
  OverrideContentSecurityPolicy(network::mojom::CSPDirectiveName::TrustedTypes,
                                std::string());
}

void WebUIDataSourceImpl::AddFrameAncestor(const GURL& frame_ancestor) {
  // Do not allow a wildcard to be a frame ancestor or it will allow any website
  // to embed the WebUI.
  CHECK(frame_ancestor.SchemeIs(kChromeUIScheme) ||
        frame_ancestor.SchemeIs(kChromeUIUntrustedScheme));
  frame_ancestors_.insert(frame_ancestor);
}

void WebUIDataSourceImpl::DisableDenyXFrameOptions() {
  deny_xframe_options_ = false;
}

void WebUIDataSourceImpl::EnableReplaceI18nInJS() {
  should_replace_i18n_in_js_ = true;
}

void WebUIDataSourceImpl::EnsureLoadTimeDataDefaultsAdded() {
  if (!add_load_time_data_defaults_)
    return;

  add_load_time_data_defaults_ = false;
  std::string locale = GetContentClient()->browser()->GetApplicationLocale();
  base::Value::Dict defaults;
  webui::SetLoadTimeDataDefaults(locale, &defaults);
  AddLocalizedStrings(defaults);
}

std::string WebUIDataSourceImpl::GetSource() {
  return source_name_;
}

std::string WebUIDataSourceImpl::GetScheme() {
  auto pos = source_name_.find("://");
  if (pos == std::string::npos) {
    return kChromeUIScheme;
  }
  return source_name_.substr(0, pos);
}

void WebUIDataSourceImpl::SetSupportedScheme(std::string_view scheme) {
  CHECK(!supported_scheme_.has_value());

  supported_scheme_ = scheme;
}

std::string WebUIDataSourceImpl::GetMimeType(const GURL& url) const {
  const std::string_view file_path = url.path_piece();

  if (base::EndsWith(file_path, ".css", base::CompareCase::INSENSITIVE_ASCII)) {
    return "text/css";
  }

  if (base::EndsWith(file_path, ".js", base::CompareCase::INSENSITIVE_ASCII)) {
    return "application/javascript";
  }

  if (base::EndsWith(file_path, ".ts", base::CompareCase::INSENSITIVE_ASCII)) {
    return "application/typescript";
  }

  if (base::EndsWith(file_path, ".json",
                     base::CompareCase::INSENSITIVE_ASCII)) {
    return "application/json";
  }

  if (base::EndsWith(file_path, ".pdf", base::CompareCase::INSENSITIVE_ASCII)) {
    return "application/pdf";
  }

  if (base::EndsWith(file_path, ".svg", base::CompareCase::INSENSITIVE_ASCII)) {
    return "image/svg+xml";
  }

  if (base::EndsWith(file_path, ".jpg", base::CompareCase::INSENSITIVE_ASCII)) {
    return "image/jpeg";
  }

  if (base::EndsWith(file_path, ".png", base::CompareCase::INSENSITIVE_ASCII)) {
    return "image/png";
  }

  if (base::EndsWith(file_path, ".mp4", base::CompareCase::INSENSITIVE_ASCII)) {
    return "video/mp4";
  }

  if (base::EndsWith(file_path, ".wasm",
                     base::CompareCase::INSENSITIVE_ASCII)) {
    return "application/wasm";
  }

  if (base::EndsWith(file_path, ".woff2",
                     base::CompareCase::INSENSITIVE_ASCII)) {
    return "application/font-woff2";
  }

  return "text/html";
}

void WebUIDataSourceImpl::StartDataRequest(
    const GURL& url,
    const WebContents::Getter& wc_getter,
    URLDataSource::GotDataCallback callback) {
  const std::string path = URLDataSource::URLToRequestPath(url);
  TRACE_EVENT1("ui", "WebUIDataSourceImpl::StartDataRequest", "path", path);

  if (!should_handle_request_callback_.is_null() &&
      should_handle_request_callback_.Run(path)) {
    filter_callback_.Run(path, std::move(callback));
    return;
  }

  EnsureLoadTimeDataDefaultsAdded();

  if (use_strings_js_) {
    bool from_js_module = path == "strings.m.js";
    if (from_js_module || path == "strings.js") {
      SendLocalizedStringsAsJSON(std::move(callback), from_js_module);
      return;
    }
  }

  int resource_id = URLToIdrOrDefault(url);
  if (resource_id == kNonExistentResource) {
    std::move(callback).Run(nullptr);
  } else {
    GetDataResourceBytesOnWorkerThread(resource_id, std::move(callback));
  }
}

void WebUIDataSourceImpl::SendLocalizedStringsAsJSON(
    URLDataSource::GotDataCallback callback,
    bool from_js_module) {
  std::string template_data;
  webui::AppendJsonJS(localized_strings_, &template_data, from_js_module);
  std::move(callback).Run(
      base::MakeRefCounted<base::RefCountedString>(std::move(template_data)));
}

const base::Value::Dict* WebUIDataSourceImpl::GetLocalizedStrings() const {
  return &localized_strings_;
}

bool WebUIDataSourceImpl::ShouldReplaceI18nInJS() const {
  return should_replace_i18n_in_js_;
}

int WebUIDataSourceImpl::URLToIdrOrDefault(const GURL& url) const {
  const std::string path(url.path_piece().substr(1));
  auto it = path_to_idr_map_.find(path);
  if (it != path_to_idr_map_.end())
    return it->second;

  if (default_resource_ != kNonExistentResource)
    return default_resource_;

  // Use GetMimeType() to check for most file requests. It returns text/html by
  // default regardless of the extension if it does not match a different file
  // type, so check for HTML file requests separately.
  if (GetMimeType(url) != "text/html" ||
      base::EndsWith(path, ".html", base::CompareCase::INSENSITIVE_ASCII)) {
    return kNonExistentResource;
  }

  // If not a file request, try to get the resource for the empty key.
  auto it_empty = path_to_idr_map_.find("");
  return (it_empty != path_to_idr_map_.end()) ? it_empty->second
                                              : kNonExistentResource;
}

}  // namespace content
