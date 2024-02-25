// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/webui/web_ui_ios_data_source_impl.h"

#import <string>

#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/ref_counted_memory.h"
#import "base/strings/string_util.h"
#import "base/strings/utf_string_conversions.h"
#import "ios/web/public/web_client.h"
#import "ui/base/webui/jstemplate_builder.h"
#import "ui/base/webui/resource_path.h"
#import "ui/base/webui/web_ui_util.h"

namespace web {

// static
WebUIIOSDataSource* WebUIIOSDataSource::Create(const std::string& source_name) {
  return new WebUIIOSDataSourceImpl(source_name);
}

// static
void WebUIIOSDataSource::Add(BrowserState* browser_state,
                             WebUIIOSDataSource* source) {
  URLDataManagerIOS::AddWebUIIOSDataSource(browser_state, source);
}

// Internal class to hide the fact that WebUIIOSDataSourceImpl implements
// URLDataSourceIOS.
class WebUIIOSDataSourceImpl::InternalDataSource : public URLDataSourceIOS {
 public:
  InternalDataSource(WebUIIOSDataSourceImpl* parent) : parent_(parent) {}

  ~InternalDataSource() override {}

  // URLDataSourceIOS implementation.
  std::string GetSource() const override { return parent_->GetSource(); }
  std::string GetMimeType(const std::string& path) const override {
    return parent_->GetMimeType(path);
  }
  void StartDataRequest(const std::string& path,
                        URLDataSourceIOS::GotDataCallback callback) override {
    return parent_->StartDataRequest(path, std::move(callback));
  }
  bool ShouldReplaceExistingSource() const override {
    return parent_->replace_existing_source_;
  }
  bool ShouldReplaceI18nInJS() const override {
    return parent_->ShouldReplaceI18nInJS();
  }
  bool AllowCaching() const override { return false; }
  bool ShouldDenyXFrameOptions() const override {
    return parent_->deny_xframe_options_;
  }

 private:
  raw_ptr<WebUIIOSDataSourceImpl> parent_;
};

WebUIIOSDataSourceImpl::WebUIIOSDataSourceImpl(const std::string& source_name)
    : URLDataSourceIOSImpl(source_name, new InternalDataSource(this)),
      source_name_(source_name),
      default_resource_(-1),
      deny_xframe_options_(true),
      load_time_data_defaults_added_(false),
      replace_existing_source_(true),
      should_replace_i18n_in_js_(false) {}

WebUIIOSDataSourceImpl::~WebUIIOSDataSourceImpl() {}

void WebUIIOSDataSourceImpl::AddString(const std::string& name,
                                       const std::u16string& value) {
  localized_strings_.Set(name, value);
  replacements_[name] = base::UTF16ToUTF8(value);
}

void WebUIIOSDataSourceImpl::AddString(const std::string& name,
                                       const std::string& value) {
  localized_strings_.Set(name, value);
  replacements_[name] = value;
}

void WebUIIOSDataSourceImpl::AddLocalizedString(const std::string& name,
                                                int ids) {
  localized_strings_.Set(name, GetWebClient()->GetLocalizedString(ids));
  replacements_[name] =
      base::UTF16ToUTF8(GetWebClient()->GetLocalizedString(ids));
}

void WebUIIOSDataSourceImpl::AddLocalizedStrings(
    const base::Value::Dict& localized_strings) {
  localized_strings_.Merge(localized_strings.Clone());
  ui::TemplateReplacementsFromDictionaryValue(localized_strings,
                                              &replacements_);
}

void WebUIIOSDataSourceImpl::AddLocalizedStrings(
    base::span<const webui::LocalizedString> strings) {
  for (const auto& str : strings) {
    AddLocalizedString(str.name, str.id);
  }
}

void WebUIIOSDataSourceImpl::AddBoolean(const std::string& name, bool value) {
  localized_strings_.Set(name, value);
}

void WebUIIOSDataSourceImpl::UseStringsJs() {
  use_strings_js_ = true;
}

void WebUIIOSDataSourceImpl::EnableReplaceI18nInJS() {
  should_replace_i18n_in_js_ = true;
}

bool WebUIIOSDataSourceImpl::ShouldReplaceI18nInJS() const {
  return should_replace_i18n_in_js_;
}

void WebUIIOSDataSourceImpl::AddResourcePath(const std::string& path,
                                             int resource_id) {
  path_to_idr_map_[path] = resource_id;
}

void WebUIIOSDataSourceImpl::AddResourcePaths(
    base::span<const webui::ResourcePath> paths) {
  for (const auto& path : paths) {
    AddResourcePath(path.path, path.id);
  }
}

void WebUIIOSDataSourceImpl::SetDefaultResource(int resource_id) {
  default_resource_ = resource_id;
}

void WebUIIOSDataSourceImpl::DisableDenyXFrameOptions() {
  deny_xframe_options_ = false;
}

const ui::TemplateReplacements* WebUIIOSDataSourceImpl::GetReplacements()
    const {
  return &replacements_;
}

std::string WebUIIOSDataSourceImpl::GetSource() const {
  return source_name_;
}

std::string WebUIIOSDataSourceImpl::GetMimeType(const std::string& path) const {
  if (base::EndsWith(path, ".js", base::CompareCase::INSENSITIVE_ASCII))
    return "application/javascript";

  if (base::EndsWith(path, ".json", base::CompareCase::INSENSITIVE_ASCII))
    return "application/json";

  if (base::EndsWith(path, ".pdf", base::CompareCase::INSENSITIVE_ASCII))
    return "application/pdf";

  if (base::EndsWith(path, ".css", base::CompareCase::INSENSITIVE_ASCII))
    return "text/css";

  if (base::EndsWith(path, ".svg", base::CompareCase::INSENSITIVE_ASCII))
    return "image/svg+xml";

  return "text/html";
}

void WebUIIOSDataSourceImpl::EnsureLoadTimeDataDefaultsAdded() {
  if (load_time_data_defaults_added_)
    return;

  load_time_data_defaults_added_ = true;
  base::Value::Dict defaults;
  webui::SetLoadTimeDataDefaults(web::GetWebClient()->GetApplicationLocale(),
                                 &defaults);
  AddLocalizedStrings(defaults);
}

void WebUIIOSDataSourceImpl::StartDataRequest(
    const std::string& path,
    URLDataSourceIOS::GotDataCallback callback) {
  EnsureLoadTimeDataDefaultsAdded();

  if (use_strings_js_) {
    bool from_js_module = path == "strings.m.js";
    if (from_js_module || path == "strings.js") {
      SendLocalizedStringsAsJSON(std::move(callback), from_js_module);
      return;
    }
  }

  int resource_id = PathToIdrOrDefault(path);
  DCHECK_NE(resource_id, -1);
  scoped_refptr<base::RefCountedMemory> response(
      GetWebClient()->GetDataResourceBytes(resource_id));
  std::move(callback).Run(response);
}

void WebUIIOSDataSourceImpl::SendLocalizedStringsAsJSON(
    URLDataSourceIOS::GotDataCallback callback,
    bool from_js_module) {
  std::string template_data;
  webui::AppendJsonJS(localized_strings_, &template_data, from_js_module);
  std::move(callback).Run(
      base::MakeRefCounted<base::RefCountedString>(std::move(template_data)));
}

int WebUIIOSDataSourceImpl::PathToIdrOrDefault(const std::string& path) const {
  auto it = path_to_idr_map_.find(path);
  return it == path_to_idr_map_.end() ? default_resource_ : it->second;
}

}  // namespace web
