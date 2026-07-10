// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webui/ui_bundled/metrics_internals/metrics_internals_ui.h"

#import "components/grit/metrics_internals_resources.h"
#import "components/grit/metrics_internals_resources_map.h"
#import "components/metrics/structured/buildflags/buildflags.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/webui/ui_bundled/metrics_internals/field_trials_handler.h"
#import "ios/chrome/browser/webui/ui_bundled/metrics_internals/metrics_internals_handler.h"
#import "ios/chrome/browser/webui/ui_bundled/metrics_internals/runtime_mutable_features_handler.h"
#import "ios/web/public/webui/web_ui_ios.h"
#import "ios/web/public/webui/web_ui_ios_data_source.h"

#if BUILDFLAG(STRUCTURED_METRICS_DEBUG_ENABLED)
#import "ios/chrome/browser/webui/ui_bundled/metrics_internals/structured_metrics_internals_handler.h"
#endif

// LINT.IfChange(metrics_internals_ui)

MetricsInternalsUI::MetricsInternalsUI(web::WebUIIOS* web_ui,
                                       const std::string& host)
    : web::WebUIIOSController(web_ui, host) {
  // Set up the chrome://metrics-internals source.
  web::WebUIIOSDataSource* source =
      web::WebUIIOSDataSource::Create(kChromeUIMetricsInternalsHost);

  source->UseStringsJs();
  source->EnableReplaceI18nInJS();

  // Add required resources.
  source->AddResourcePaths(kMetricsInternalsResources);
  source->SetDefaultResource(IDR_METRICS_INTERNALS_METRICS_INTERNALS_HTML);

  web_ui->AddMessageHandler(std::make_unique<MetricsInternalsHandler>());
  web_ui->AddMessageHandler(std::make_unique<RuntimeMutableFeaturesHandler>());

  web_ui->AddMessageHandler(
      std::make_unique<FieldTrialsHandler>(ProfileIOS::FromWebUIIOS(web_ui)));

#if BUILDFLAG(STRUCTURED_METRICS_DEBUG_ENABLED)
  source->AddResourcePath(
      "structured", IDR_METRICS_INTERNALS_STRUCTURED_STRUCTURED_INTERNALS_HTML);
  source->AddResourcePath(
      "structured/",
      IDR_METRICS_INTERNALS_STRUCTURED_STRUCTURED_INTERNALS_HTML);
  web_ui->AddMessageHandler(
      std::make_unique<StructuredMetricsInternalsHandler>());
#endif

  source->AddBoolean("enablePrivateMetricsTab", false);

  web::WebUIIOSDataSource::Add(ProfileIOS::FromWebUIIOS(web_ui), source);
}

MetricsInternalsUI::~MetricsInternalsUI() = default;

// LINT.ThenChange(//chrome/browser/ui/webui/metrics_internals/metrics_internals_ui.cc)
