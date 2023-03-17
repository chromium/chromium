// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBUI_WEB_UI_DATA_SOURCE_IMPL_H_
#define CONTENT_BROWSER_WEBUI_WEB_UI_DATA_SOURCE_IMPL_H_

#include <stdint.h>

#include <map>
#include <string>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/values.h"
#include "content/browser/webui/url_data_manager.h"
#include "content/browser/webui/url_data_source_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui_data_source.h"

namespace content {

// A data source that can help with implementing the common operations
// needed by the chrome WEBUI settings/history/downloads pages.
class CONTENT_EXPORT WebUIDataSourceImpl : public URLDataSourceImpl,
                                           public WebUIDataSource {
 public:
  WebUIDataSourceImpl(const WebUIDataSourceImpl&) = delete;
  WebUIDataSourceImpl& operator=(const WebUIDataSourceImpl&) = delete;

  // WebUIDataSource:
  void AddString(base::StringPiece name, const std::u16string& value) override;
  void AddString(base::StringPiece name, const std::string& value) override;
  void AddLocalizedString(base::StringPiece name, int ids) override;
  void AddLocalizedStrings(
      base::span<const webui::LocalizedString> strings) override;
  void AddLocalizedStrings(const base::Value::Dict& localized_strings) override;
  void AddBoolean(base::StringPiece name, bool value) override;
  void AddInteger(base::StringPiece name, int32_t value) override;
  void AddDouble(base::StringPiece name, double value) override;
  void UseStringsJs() override;
  void AddResourcePath(base::StringPiece path, int resource_id) override;
  void AddResourcePaths(base::span<const webui::ResourcePath> paths) override;
  void SetDefaultResource(int resource_id) override;
  void SetRequestFilter(const WebUIDataSource::ShouldHandleRequestCallback&
                            should_handle_request_callback,
                        const WebUIDataSource::HandleRequestCallback&
                            handle_request_callback) override;
  void DisableReplaceExistingSource() override;
  void DisableContentSecurityPolicy() override;
  void OverrideContentSecurityPolicy(network::mojom::CSPDirectiveName directive,
                                     const std::string& value) override;
  void OverrideCrossOriginOpenerPolicy(const std::string& value) override;
  void OverrideCrossOriginEmbedderPolicy(const std::string& value) override;
  void OverrideCrossOriginResourcePolicy(const std::string& value) override;
  void DisableTrustedTypesCSP() override;
  void DisableDenyXFrameOptions() override;
  void EnableReplaceI18nInJS() override;
  std::string GetSource() override;

  // Add the locale to the load time data defaults. May be called repeatedly.
  void EnsureLoadTimeDataDefaultsAdded();

  bool IsWebUIDataSourceImpl() const override;
  void AddFrameAncestor(const GURL& frame_ancestor) override;

 protected:
  explicit WebUIDataSourceImpl(const std::string& source_name);
  ~WebUIDataSourceImpl() override;

  // Completes a request by sending our dictionary of localized strings.
  void SendLocalizedStringsAsJSON(URLDataSource::GotDataCallback callback,
                                  bool from_js_module);

  // Protected for testing.
  virtual const base::Value::Dict* GetLocalizedStrings() const;

  // Protected for testing.
  int URLToIdrOrDefault(const GURL& url) const;

 private:
  class InternalDataSource;
  friend class InternalDataSource;
  friend class URLDataManagerBackend;
  friend class WebUIDataSource;
  friend class WebUIDataSourceTest;

  // Methods that match URLDataSource which are called by
  // InternalDataSource.
  std::string GetMimeType(const GURL& url) const;
  void StartDataRequest(const GURL& url,
                        const WebContents::Getter& wc_getter,
                        URLDataSource::GotDataCallback callback);

  // Note: this must be called before StartDataRequest() to have an effect.
  void disable_load_time_data_defaults_for_testing() {
    add_load_time_data_defaults_ = false;
  }

  bool ShouldReplaceI18nInJS() const;

  // The name of this source.
  // E.g., for favicons, this could be "favicon", which results in paths for
  // specific resources like "favicon/34" getting sent to this source.
  std::string source_name_;
  int default_resource_;
  bool use_strings_js_ = false;
  std::map<std::string, int> path_to_idr_map_;
  // The replacements are initialized in the main thread and then used in the
  // IO thread. The map is safe to read from multiple threads as long as no
  // futher changes are made to it after initialization.
  ui::TemplateReplacements replacements_;
  // The |replacements_| is intended to replace |localized_strings_|.
  // TODO(dschuyler): phase out |localized_strings_| in Q1 2017. (Or rename
  // to |load_time_flags_| if the usage is reduced to storing flags only).
  base::Value::Dict localized_strings_;
  WebUIDataSource::HandleRequestCallback filter_callback_;
  WebUIDataSource::ShouldHandleRequestCallback should_handle_request_callback_;

  bool add_csp_ = true;

  base::flat_map<network::mojom::CSPDirectiveName, std::string> csp_overrides_;
  std::string coop_value_;
  std::string coep_value_;
  std::string corp_value_;
  bool deny_xframe_options_ = true;
  bool add_load_time_data_defaults_ = true;
  bool replace_existing_source_ = true;
  bool should_replace_i18n_in_js_ = false;
  std::set<GURL> frame_ancestors_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBUI_WEB_UI_DATA_SOURCE_IMPL_H_
