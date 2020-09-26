// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/crash_report_private/crash_report_private_api.h"

#include "base/time/default_clock.h"
#include "components/crash/content/browser/error_reporting/javascript_error_report.h"
#include "components/crash/content/browser/error_reporting/send_javascript_error_report.h"

namespace extensions {
namespace api {

namespace {

// Used for throttling the API calls.
base::Time g_last_called_time;

base::Clock*& GetClock() {
  static base::Clock* clock = base::DefaultClock::GetInstance();
  return clock;
}

}  // namespace

CrashReportPrivateReportErrorFunction::CrashReportPrivateReportErrorFunction() =
    default;

CrashReportPrivateReportErrorFunction::
    ~CrashReportPrivateReportErrorFunction() = default;

ExtensionFunction::ResponseAction CrashReportPrivateReportErrorFunction::Run() {
  // Ensure we don't send too many crash reports. Limit to one report per hour.
  if (!g_last_called_time.is_null() &&
      GetClock()->Now() - g_last_called_time < base::TimeDelta::FromHours(1)) {
    return RespondNow(Error("Too many calls to this API"));
  }
  g_last_called_time = base::Time::Now();

  // TODO(https://crbug.com/986166): Use crash_reporter for Chrome OS.
  const auto params = crash_report_private::ReportError::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  JavaScriptErrorReport error_report;
  error_report.message = std::move(params->info.message);
  error_report.url = std::move(params->info.url);
  if (params->info.product) {
    error_report.product = std::move(*params->info.product);
  }

  if (params->info.version) {
    error_report.version = std::move(*params->info.version);
  }

  if (params->info.line_number) {
    error_report.line_number = *params->info.line_number;
  }

  if (params->info.column_number) {
    error_report.column_number = *params->info.column_number;
  }

  if (params->info.stack_trace) {
    error_report.stack_trace = std::move(*params->info.stack_trace);
  }

  SendJavaScriptErrorReport(
      std::move(error_report),
      base::BindOnce(&CrashReportPrivateReportErrorFunction::OnReportComplete,
                     this),
      browser_context());

  return RespondLater();
}

void CrashReportPrivateReportErrorFunction::OnReportComplete() {
  Respond(NoArguments());
}

void SetClockForTesting(base::Clock* clock) {
  GetClock() = clock;
}

}  // namespace api
}  // namespace extensions
