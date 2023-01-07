// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOG_CONSOLE_MESSAGE_H_
#define CONTENT_BROWSER_LOG_CONSOLE_MESSAGE_H_

#include <string>

#include "base/logging.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"

namespace content {

// Optionally logs a message from the console, depending on the set logging
// levels and incognito state.
void LogConsoleMessage(blink::mojom::ConsoleMessageLevel log_level,
                       const std::u16string& message,
                       int32_t line_number,
                       bool is_builtin_component,
                       bool is_off_the_record,
                       const std::u16string& source_id);

}  // namespace content

#endif  // CONTENT_BROWSER_LOG_CONSOLE_MESSAGE_H_
