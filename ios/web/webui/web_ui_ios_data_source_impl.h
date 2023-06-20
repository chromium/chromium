// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEBUI_WEB_UI_IOS_DATA_SOURCE_IMPL_H_
#define IOS_WEB_WEBUI_WEB_UI_IOS_DATA_SOURCE_IMPL_H_

#include <map>
#include <string>

#include "base/functional/callback.h"
#include "base/values.h"
#include "ios/web/public/webui/url_data_source_ios.h"
#include "ios/web/public/webui/web_ui_ios_data_source.h"
#include "ios/web/webui/url_data_manager_ios.h"
#include "ios/web/webui/url_data_source_ios_impl.h"
#include "ui/base/template_expressions.h"

namespace web {

class WebUIIOSDataSourceImpl : public URLDataSourceIOSImpl,
                               public WebUIIOSDataSource {
 public:
  WebUIIOSDataSourceImpl(const WebUIIOSDataSourceImpl&) = delete;
  WebUIIOSDataSourceImpl& operator=(const WebUIIOSDataSourceImpl&) = delete;

  // WebUIIOSDataSource implementation:
  void AddString(const std::string& name, const std::u16string& value) override;
  void AddString(const std::string& name, const std::string& value) override;
  void AddLocalizedString(const std::string& name, int ids) override;
  void AddLocalizedStrings(const base::Value::Dict& localized_strings) override;
  void AddLocalizedStrings(
      base::span<const webui::LocalizedString> strings) override;
  void AddBoolean(const std::string& name, bool value) override;
  void UseStringsJs() override;
  void EnableReplaceI18nInJS() override;
  bool ShouldReplaceI18nInJS() const override;
  void AddResourcePath(const std::string& path, int resource_id) override;
  void AddResourcePaths(base::span<const webui::ResourcePath> paths) override;
  void SetDefaultResource(int resource_id) override;
  void DisableDenyXFrameOptions() override;
  const ui::TemplateReplacements* GetReplacements() const override;

 protected:
  ~WebUIIOSDataSourceImpl() override;

  // Completes a request by sending our dictionary of localized strings.
  void SendLocalizedStringsAsJSON(URLDataSourceIOS::GotDataCallback callback,
                                  bool from_js_module);

 private:
  class InternalDataSource;
  friend class InternalDataSource;
  friend class WebUIIOSDataSourceTest;
  friend class WebUIIOSDataSource;

  explicit WebUIIOSDataSourceImpl(const std::string& source_name);

  // Adds the locale to the load time data defaults. May be called repeatedly.
  void EnsureLoadTimeDataDefaultsAdded();

  // Methods that match URLDataSource which are called by
  // InternalDataSource.
  std::string GetSource() const;
  std::string GetMimeType(const std::string& path) const;
  void StartDataRequest(const std::string& path,
                        URLDataSourceIOS::GotDataCallback callback);

  int PathToIdrOrDefault(const std::string& path) const;

  // The name of this source.
  // E.g., for favicons, this could be "favicon", which results in paths for
  // specific resources like "favicon/34" getting sent to this source.
  std::string source_name_;
  int default_resource_;
  bool use_strings_js_ = false;
  std::map<std::string, int> path_to_idr_map_;
  // The replacements are initiallized in the main thread and then used in the
  // IO thread. The map is safe to read from multiple threads as long as no
  // further changes are made to it after initialization.
  ui::TemplateReplacements replacements_;
  // The `replacements_` is intended to replace `localized_strings_`.
  base::Value::Dict localized_strings_;
  bool deny_xframe_options_;
  bool load_time_data_defaults_added_;
  bool replace_existing_source_;
  bool should_replace_i18n_in_js_;
};

}  // web

#endif  // IOS_WEB_WEBUI_WEB_UI_IOS_DATA_SOURCE_IMPL_H_
