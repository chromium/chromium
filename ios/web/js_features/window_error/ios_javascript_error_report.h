// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_JS_FEATURES_WINDOW_ERROR_IOS_JAVASCRIPT_ERROR_REPORT_H_
#define IOS_WEB_JS_FEATURES_WINDOW_ERROR_IOS_JAVASCRIPT_ERROR_REPORT_H_

#include <optional>
#include <string>

// A report about a JavaScript error that we might want to send back to Google
// so it can be fixed. This differs significantly from the JavaScriptErrorReport
// implementation on other platforms because those are always associated with a
// WebUI page. However, these reports can come from any webpage. The intent is
// to only report errors associated with the iOS Chrome scripts injected into
// webpages, so care is taken to ensure only reports relevant to errors in
// Chrome scripts are sent.
struct IOSJavaScriptErrorReport {
  IOSJavaScriptErrorReport();
  IOSJavaScriptErrorReport(const IOSJavaScriptErrorReport& rhs);
  IOSJavaScriptErrorReport(IOSJavaScriptErrorReport&& rhs) noexcept;
  ~IOSJavaScriptErrorReport();

  IOSJavaScriptErrorReport& operator=(const IOSJavaScriptErrorReport& rhs);
  IOSJavaScriptErrorReport& operator=(IOSJavaScriptErrorReport&& rhs) noexcept;

  // Name of the gCrWeb API which threw the error. For example,
  // "autofill.extractForms". Required.
  std::string api;

  // The error message. Required.
  std::string error_message;

  // Whether or not the error originated from the main frame. Required.
  bool from_main_frame;

  // The system that created the error report. Useful for checking that each of
  // the various different systems that generate JavaScript error reports is
  // working as expected.
  enum class SourceSystem {
    kUnknown,
    kNativeScriptExecutionFailed,
    kScriptErrorMessageHandler,
  };
  SourceSystem source_system = SourceSystem::kUnknown;

  // String containing the stack trace for the error. Not sent if not present.
  std::optional<std::string> stack_trace;

  // URL of the page the user was on when the error occurred. Must include the
  // protocol (e.g. http://www.example.com) but not query, fragment, or other
  // privacy-sensitive details we don't want to send.
  std::optional<std::string> page_url;
};

#endif  // IOS_WEB_JS_FEATURES_WINDOW_ERROR_IOS_JAVASCRIPT_ERROR_REPORT_H_
