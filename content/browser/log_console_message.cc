// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/log_console_message.h"

#include "base/feature_list.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "content/public/browser/console_message.h"
#include "content/public/common/content_features.h"

namespace content {

void LogConsoleMessage(blink::mojom::ConsoleMessageLevel log_level,
                       const std::u16string& message,
                       int32_t line_number,
                       bool is_builtin_component,
                       bool is_off_the_record,
                       const std::u16string& source_id) {
  const int32_t resolved_level =
      is_builtin_component ? ConsoleMessageLevelToLogSeverity(log_level)
                           : ::logging::LOGGING_INFO;
  if (::logging::GetMinLogLevel() > resolved_level)
    return;

  // LogMessages can be persisted so this shouldn't be logged in incognito mode.
  // This rule is not applied to WebUI pages or other builtin components,
  // because WebUI and builtin components source code is a part of Chrome source
  // code, and we want to treat messages from WebUI and other builtin components
  // the same way as we treat log messages from native code.
  if (is_off_the_record && !is_builtin_component)
    return;

  if (!base::FeatureList::IsEnabled(features::kLogJsConsoleMessages))
    return;

  logging::LogMessage("CONSOLE", line_number, resolved_level).stream()
      << "\"" << message << "\", source: " << source_id << " (" << line_number
      << ")";
}

}  // namespace content
