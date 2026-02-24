// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_JS_FEATURES_WINDOW_ERROR_WEB_JS_ERROR_REPORT_PROCESSOR_H_
#define IOS_WEB_JS_FEATURES_WINDOW_ERROR_WEB_JS_ERROR_REPORT_PROCESSOR_H_

#import <memory>
#import <optional>
#import <string>

#import "base/containers/flat_map.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/supports_user_data.h"
#import "base/time/time.h"
#import "ios/web/js_features/window_error/ios_javascript_error_report.h"
#import "ios/web/js_features/window_error/script_error_details.h"

namespace network {
class SimpleURLLoader;
}  // namespace network

namespace url {
class Origin;
}  // namespace url

namespace web {

class BrowserState;

class WebJsErrorReportProcessor : public base::SupportsUserData::Data {
 public:
  static WebJsErrorReportProcessor* FromBrowserState(
      BrowserState* browser_state);

  static void LogProcessorUnavailable();

  WebJsErrorReportProcessor(BrowserState* browser_state);
  ~WebJsErrorReportProcessor() override;

  // Reports the error described by `details`.
  void ReportJavaScriptError(ScriptErrorDetails details);

  // Reports `error` for `api`, occurring on `origin`.
  void ReportJavaScriptExecutionFailed(std::string api,
                                       url::Origin origin,
                                       NSError* error,
                                       bool from_main_frame);

 private:
  std::string GetOsVersion();

  void SendErrorReport(IOSJavaScriptErrorReport error_report);
  bool ShouldUploadErrorReport(const std::string& error_report_key);

  void OnRequestComplete(std::unique_ptr<network::SimpleURLLoader> url_loader,
                         const std::string& error_report_key,
                         std::optional<std::string> response_body);

  raw_ptr<BrowserState> browser_state_;

  // A mapping of error report api+message to the last time we sent an error
  // message for that key.
  base::flat_map<std::string, base::Time> recent_error_reports_;

  // The time `recent_error_reports_` was last cleaned out of old timestamps.
  base::Time last_recent_error_reports_cleaning_;

  base::WeakPtrFactory<WebJsErrorReportProcessor> weak_factory_{this};
};

}  // namespace web

#endif  // IOS_WEB_JS_FEATURES_WINDOW_ERROR_WEB_JS_ERROR_REPORT_PROCESSOR_H_
