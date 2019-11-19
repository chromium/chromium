// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/logging/logging_utils.h"

#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"

namespace blink {

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

}  // namespace blink
