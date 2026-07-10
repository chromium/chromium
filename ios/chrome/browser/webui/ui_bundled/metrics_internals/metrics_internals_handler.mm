// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webui/ui_bundled/metrics_internals/metrics_internals_handler.h"

#import "base/functional/bind.h"
#import "base/values.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/web/public/webui/web_ui_ios.h"

// LINT.IfChange(metrics_internals_handler)

MetricsInternalsHandler::MetricsInternalsHandler() {
  base_handler_ = std::make_unique<metrics::MetricsInternalsHandlerBase>(
      this, GetApplicationContext()->GetMetricsService(),
      GetApplicationContext()->GetMetricsServicesManager());
}

MetricsInternalsHandler::~MetricsInternalsHandler() {
  base_handler_->StopObserving();
}

void MetricsInternalsHandler::RegisterMessages() {
  base_handler_->StartObserving();

  web_ui()->RegisterMessageCallback(
      "fetchVariationsSummary",
      base::BindRepeating(
          &MetricsInternalsHandler::HandleFetchVariationsSummary,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "fetchStoredLatestSeedInfo",
      base::BindRepeating(&MetricsInternalsHandler::HandleFetchStoredSeedInfo,
                          base::Unretained(this),
                          variations::VariationsSeedStore::SeedType::LATEST));
  web_ui()->RegisterMessageCallback(
      "fetchStoredSafeSeedInfo",
      base::BindRepeating(&MetricsInternalsHandler::HandleFetchStoredSeedInfo,
                          base::Unretained(this),
                          variations::VariationsSeedStore::SeedType::SAFE));
  web_ui()->RegisterMessageCallback(
      "fetchUmaSummary",
      base::BindRepeating(&MetricsInternalsHandler::HandleFetchUmaSummary,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "fetchUmaLogsData",
      base::BindRepeating(&MetricsInternalsHandler::HandleFetchUmaLogsData,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "isUsingMetricsServiceObserver",
      base::BindRepeating(
          &MetricsInternalsHandler::HandleIsUsingMetricsServiceObserver,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "fetchEncryptionPublicKey",
      base::BindRepeating(
          &MetricsInternalsHandler::HandleFetchEncryptionPublicKey,
          base::Unretained(this)));
}

void MetricsInternalsHandler::ResolvePageCallback(
    const base::ValueView callback_id,
    const base::ValueView response) {
  web_ui()->ResolveJavascriptCallback(callback_id, response);
}

void MetricsInternalsHandler::FireWebUIListener(std::string_view event_name) {
  web_ui()->FireWebUIListener(event_name);
}

void MetricsInternalsHandler::FireWebUIListener(std::string_view event_name,
                                                const base::ValueView arg1) {
  web_ui()->FireWebUIListener(event_name, arg1);
}

void MetricsInternalsHandler::HandleFetchVariationsSummary(
    const base::ListValue& args) {
  base_handler_->HandleFetchVariationsSummary(args[0]);
}

void MetricsInternalsHandler::HandleFetchStoredSeedInfo(
    variations::VariationsSeedStore::SeedType seed_type,
    const base::ListValue& args) {
  base_handler_->HandleFetchStoredSeedInfo(seed_type, args[0]);
}

void MetricsInternalsHandler::HandleFetchUmaSummary(
    const base::ListValue& args) {
  base_handler_->HandleFetchUmaSummary(args[0]);
}

void MetricsInternalsHandler::HandleFetchUmaLogsData(
    const base::ListValue& args) {
  DCHECK_EQ(args.size(), 2U);
  base_handler_->HandleFetchUmaLogsData(args[0], args[1].GetBool());
}

void MetricsInternalsHandler::HandleFetchEncryptionPublicKey(
    const base::ListValue& args) {
  base_handler_->HandleFetchEncryptionPublicKey(args[0]);
}

void MetricsInternalsHandler::HandleIsUsingMetricsServiceObserver(
    const base::ListValue& args) {
  base_handler_->HandleIsUsingMetricsServiceObserver(args[0]);
}

// LINT.ThenChange(//chrome/browser/ui/webui/metrics_internals/metrics_internals_handler.cc)
