// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/devtools_protocol_test_support.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace content {

DevToolsProtocolTest::DevToolsProtocolTest() = default;
DevToolsProtocolTest::~DevToolsProtocolTest() = default;

void DevToolsProtocolTest::Attach() {
  AttachToWebContents(shell()->web_contents());
  shell()->web_contents()->SetDelegate(this);
}

void DevToolsProtocolTest::SetUpOnMainThread() {
  host_resolver()->AddRule("*", "127.0.0.1");
}

void DevToolsProtocolTest::TearDownOnMainThread() {
  DetachProtocolClient();
}

bool DevToolsProtocolTest::DidAddMessageToConsole(
    WebContents* source,
    blink::mojom::ConsoleMessageLevel log_level,
    const std::u16string& message,
    int32_t line_no,
    const std::u16string& source_id) {
  console_messages_.push_back(base::UTF16ToUTF8(message));
  return true;
}

}  // namespace content
