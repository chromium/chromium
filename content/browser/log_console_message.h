// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOG_CONSOLE_MESSAGE_H_
#define CONTENT_BROWSER_LOG_CONSOLE_MESSAGE_H_

#include "base/logging.h"
#include "base/strings/string16.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"

namespace content {

logging::LogSeverity ConsoleMessageLevelToLogSeverity(
    blink::mojom::ConsoleMessageLevel level);

// Optionally logs a message from the console, depending on the set logging
// levels and incognito state.
void LogConsoleMessage(blink::mojom::ConsoleMessageLevel log_level,
                       const base::string16& message,
                       int32_t line_number,
                       bool is_builtin_component,
                       bool is_off_the_record,
                       const base::string16& source_id);

}  // namespace content

#endif  // CONTENT_BROWSER_LOG_CONSOLE_MESSAGE_H_
