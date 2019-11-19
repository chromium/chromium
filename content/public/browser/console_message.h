// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_CONSOLE_MESSAGE_H_
#define CONTENT_PUBLIC_BROWSER_CONSOLE_MESSAGE_H_

#include "base/strings/string16.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "url/gurl.h"

namespace content {

// A collection of information about a message that has been added to the
// console.
struct ConsoleMessage {
  ConsoleMessage(blink::mojom::ConsoleMessageSource source,
                 blink::mojom::ConsoleMessageLevel message_level,
                 const base::string16& message,
                 int line_number,
                 const GURL& source_url)
      : source(source),
        message_level(message_level),
        message(message),
        line_number(line_number),
        source_url(source_url) {}

  // The type of source this came from.
  const blink::mojom::ConsoleMessageSource source;
  // The severity of the console message.
  const blink::mojom::ConsoleMessageLevel message_level;
  // The message that was logged to the console.
  const base::string16 message;
  // The line in the script file that the log was emitted at.
  const int line_number;
  // The URL that emitted the log.
  const GURL source_url;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_CONSOLE_MESSAGE_H_
