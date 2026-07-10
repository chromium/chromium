// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webui/ui_bundled/metrics_internals/structured_metrics_internals_handler.h"

#import "components/metrics/structured/buildflags/buildflags.h"

#if BUILDFLAG(STRUCTURED_METRICS_DEBUG_ENABLED)

#import "base/functional/bind.h"
#import "base/values.h"
#import "components/metrics_services_manager/metrics_services_manager.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/web/public/webui/web_ui_ios.h"

// LINT.IfChange(structured_metrics_internals_handler)

StructuredMetricsInternalsHandler::StructuredMetricsInternalsHandler()
    : base_handler_(std::make_unique<
                    metrics::structured::StructuredMetricsInternalsHandlerBase>(
          this,
          GetApplicationContext()
              ->GetMetricsServicesManager()
              ->GetStructuredMetricsService())) {}

StructuredMetricsInternalsHandler::~StructuredMetricsInternalsHandler() =
    default;

void StructuredMetricsInternalsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "fetchStructuredMetricsEvents",
      base::BindRepeating(&StructuredMetricsInternalsHandler::
                              HandleFetchStructuredMetricsEvents,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "fetchStructuredMetricsSummary",
      base::BindRepeating(&StructuredMetricsInternalsHandler::
                              HandleFetchStructuredMetricsSummary,
                          base::Unretained(this)));
}

void StructuredMetricsInternalsHandler::ResolvePageCallback(
    const base::ValueView callback_id,
    const base::ValueView response) {
  web_ui()->ResolveJavascriptCallback(callback_id, response);
}

void StructuredMetricsInternalsHandler::HandleFetchStructuredMetricsEvents(
    const base::ListValue& args) {
  // args[0]: Callback ID.
  CHECK_EQ(args.size(), 1U);
  base_handler_->HandleFetchStructuredMetricsEvents(args[0]);
}

void StructuredMetricsInternalsHandler::HandleFetchStructuredMetricsSummary(
    const base::ListValue& args) {
  // args[0]: Callback ID.
  CHECK_EQ(args.size(), 1U);
  base_handler_->HandleFetchStructuredMetricsSummary(args[0]);
}

// LINT.ThenChange(//chrome/browser/ui/webui/metrics_internals/structured_metrics_internals_handler.cc)

#endif  // BUILDFLAG(STRUCTURED_METRICS_DEBUG_ENABLED)
