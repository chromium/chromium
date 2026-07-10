// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_METRICS_INTERNALS_METRICS_INTERNALS_HANDLER_H_
#define IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_METRICS_INTERNALS_METRICS_INTERNALS_HANDLER_H_

#include <memory>

#include "components/metrics/debug/metrics_internals_handler_base.h"
#include "ios/web/public/webui/web_ui_ios_message_handler.h"

// LINT.IfChange(metrics_internals_handler)

// UI Handler for chrome://metrics-internals.
class MetricsInternalsHandler
    : public web::WebUIIOSMessageHandler,
      public metrics::MetricsInternalsHandlerBase::Delegate {
 public:
  MetricsInternalsHandler();

  MetricsInternalsHandler(const MetricsInternalsHandler&) = delete;
  MetricsInternalsHandler& operator=(const MetricsInternalsHandler&) = delete;

  ~MetricsInternalsHandler() override;

  // web::WebUIIOSMessageHandler:
  void RegisterMessages() override;

  // metrics::MetricsInternalsHandlerBase::Delegate:
  void ResolvePageCallback(const base::ValueView callback_id,
                           const base::ValueView response) override;
  void FireWebUIListener(std::string_view event_name) override;
  void FireWebUIListener(std::string_view event_name,
                         const base::ValueView arg1) override;

 private:
  void HandleFetchVariationsSummary(const base::ListValue& args);
  void HandleFetchStoredSeedInfo(
      variations::VariationsSeedStore::SeedType seed_type,
      const base::ListValue& args);
  void HandleFetchUmaSummary(const base::ListValue& args);
  void HandleFetchUmaLogsData(const base::ListValue& args);
  void HandleFetchEncryptionPublicKey(const base::ListValue& args);
  void HandleIsUsingMetricsServiceObserver(const base::ListValue& args);

  std::unique_ptr<metrics::MetricsInternalsHandlerBase> base_handler_;
};

// LINT.ThenChange(//chrome/browser/ui/webui/metrics_internals/metrics_internals_handler.h)

#endif  // IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_METRICS_INTERNALS_METRICS_INTERNALS_HANDLER_H_
