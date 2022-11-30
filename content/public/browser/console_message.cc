// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/console_message.h"

namespace content {

logging::LogSeverity ConsoleMessageLevelToLogSeverity(
    blink::mojom::ConsoleMessageLevel level) {
  logging::LogSeverity log_severity = logging::LOG_VERBOSE;
  switch (level) {
    case blink::mojom::ConsoleMessageLevel::kVerbose:
      log_severity = logging::LOG_VERBOSE;
      break;
    case blink::mojom::ConsoleMessageLevel::kInfo:
      log_severity = logging::LOG_INFO;
      break;
    case blink::mojom::ConsoleMessageLevel::kWarning:
      log_severity = logging::LOG_WARNING;
      break;
    case blink::mojom::ConsoleMessageLevel::kError:
      log_severity = logging::LOG_ERROR;
      break;
  }

  return log_severity;
}

const char* MessageSourceToString(blink::mojom::ConsoleMessageSource source) {
  switch (source) {
    case blink::mojom::ConsoleMessageSource::kXml:
      return "XML";
    case blink::mojom::ConsoleMessageSource::kJavaScript:
      return "JS";
    case blink::mojom::ConsoleMessageSource::kNetwork:
      return "Network";
    case blink::mojom::ConsoleMessageSource::kConsoleApi:
      return "ConsoleAPI";
    case blink::mojom::ConsoleMessageSource::kStorage:
      return "Storage";
    case blink::mojom::ConsoleMessageSource::kRendering:
      return "Rendering";
    case blink::mojom::ConsoleMessageSource::kSecurity:
      return "Security";
    case blink::mojom::ConsoleMessageSource::kOther:
      return "Other";
    case blink::mojom::ConsoleMessageSource::kDeprecation:
      return "Deprecation";
    case blink::mojom::ConsoleMessageSource::kWorker:
      return "Worker";
    case blink::mojom::ConsoleMessageSource::kViolation:
      return "Violation";
    case blink::mojom::ConsoleMessageSource::kIntervention:
      return "Intervention";
    case blink::mojom::ConsoleMessageSource::kRecommendation:
      return "Recommendation";
  }
  LOG(FATAL) << "Unreachable code.";
  return nullptr;
}

}  // namespace content
