// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_METRICS_INTERNALS_STRUCTURED_METRICS_INTERNALS_HANDLER_H_
#define IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_METRICS_INTERNALS_STRUCTURED_METRICS_INTERNALS_HANDLER_H_

#include "components/metrics/structured/buildflags/buildflags.h"

#if BUILDFLAG(STRUCTURED_METRICS_DEBUG_ENABLED)

#include <memory>

#include "base/values.h"
#include "components/metrics/debug/structured/structured_metrics_internals_handler_base.h"
#include "ios/web/public/webui/web_ui_ios_message_handler.h"

// LINT.IfChange(structured_metrics_internals_handler)

// UI Handler for chrome://metrics-internals/structured
class StructuredMetricsInternalsHandler
    : public web::WebUIIOSMessageHandler,
      public metrics::structured::StructuredMetricsInternalsHandlerBase::
          Delegate {
 public:
  StructuredMetricsInternalsHandler();

  StructuredMetricsInternalsHandler(const StructuredMetricsInternalsHandler&) =
      delete;
  StructuredMetricsInternalsHandler& operator=(
      const StructuredMetricsInternalsHandler&) = delete;

  ~StructuredMetricsInternalsHandler() override;

  // web::WebUIIOSMessageHandler:
  void RegisterMessages() override;

  // metrics::structured::StructuredMetricsInternalsHandlerBase::Delegate:
  void ResolvePageCallback(const base::ValueView callback_id,
                           const base::ValueView response) override;

 private:
  void HandleFetchStructuredMetricsEvents(const base::ListValue& args);
  void HandleFetchStructuredMetricsSummary(const base::ListValue& args);

  std::unique_ptr<metrics::structured::StructuredMetricsInternalsHandlerBase>
      base_handler_;
};

// LINT.ThenChange(//chrome/browser/ui/webui/metrics_internals/structured_metrics_internals_handler.h)

#endif  // BUILDFLAG(STRUCTURED_METRICS_DEBUG_ENABLED)

#endif  // IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_METRICS_INTERNALS_STRUCTURED_METRICS_INTERNALS_HANDLER_H_
