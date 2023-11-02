// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_DEVTOOLS_PROTOCOL_TEST_SUPPORT_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_DEVTOOLS_PROTOCOL_TEST_SUPPORT_H_

#include <string>
#include <vector>

#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/test_devtools_protocol_client.h"

namespace content {

class DevToolsProtocolTest : virtual public ContentBrowserTest,
                             public WebContentsDelegate,
                             public TestDevToolsProtocolClient {
 public:
  DevToolsProtocolTest();
  ~DevToolsProtocolTest() override;

 protected:
  void Detach() { DetachProtocolClient(); }
  void Attach();

  // WebContentsDelegate overrides.
  bool DidAddMessageToConsole(WebContents* source,
                              blink::mojom::ConsoleMessageLevel log_level,
                              const std::u16string& message,
                              int32_t line_no,
                              const std::u16string& source_id) override;

  // ContentBrowserTest overrides.
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  std::vector<std::string> console_messages_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_DEVTOOLS_PROTOCOL_TEST_SUPPORT_H_
