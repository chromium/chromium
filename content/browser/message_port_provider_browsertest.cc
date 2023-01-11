// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/message_port_provider.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "third_party/blink/public/common/messaging/string_message_codec.h"

namespace content {

// This test verifies the functionality of the Message Port Provider API.

class MessagePortProviderBrowserTest : public ContentBrowserTest {
};

// Verify that messages can be posted to main frame.
IN_PROC_BROWSER_TEST_F(MessagePortProviderBrowserTest, PostMessage) {
  // Listen for a message.
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(ExecJs(shell(), R"(
      onmessage = function(e) {
          domAutomationController.send(e.origin + ':' + e.data);
      } )"));

  // Post a message.
  const std::string target_origin(url.DeprecatedGetOriginAsURL().spec());
  const std::string source_origin("https://source.origin.com");
  const std::string message("success");
  DOMMessageQueue msg_queue(shell()->web_contents());
  MessagePortProvider::PostMessageToFrame(
      shell()->web_contents()->GetPrimaryPage(),
      base::UTF8ToUTF16(source_origin), base::UTF8ToUTF16(target_origin),
      base::UTF8ToUTF16(message));

  // Verify that the message was received (and had the expected payload).
  std::string expected_test_reply =
      base::StrCat({"\"", source_origin, ":", message, "\""});
  std::string actual_test_reply;
  EXPECT_TRUE(msg_queue.WaitForMessage(&actual_test_reply));
  EXPECT_EQ(expected_test_reply, actual_test_reply);
}

IN_PROC_BROWSER_TEST_F(MessagePortProviderBrowserTest, PostArrayBufferMessage) {
  // Listen for a message.
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(ExecJs(shell(), R"(
      onmessage = function(e) {
          domAutomationController.send(e.origin + ':' +
            new Uint8Array(e.data).join());
      } )"));

  // Post a message.
  const std::string target_origin(url.DeprecatedGetOriginAsURL().spec());
  const std::string source_origin("https://source.origin.com");
  const std::vector<uint8_t> message = {0x01, 0x02, 0x03, 0x04};
  DOMMessageQueue msg_queue(shell()->web_contents());
  MessagePortProvider::PostMessageToFrame(
      shell()->web_contents()->GetPrimaryPage(),
      base::UTF8ToUTF16(source_origin), base::UTF8ToUTF16(target_origin),
      blink::WebMessageArrayBufferPayload::CreateForTesting(message));

  // Verify that the message was received (and had the expected payload).
  std::string expected_test_reply =
      base::StrCat({"\"", source_origin, ":1,2,3,4\""});
  std::string actual_test_reply;
  EXPECT_TRUE(msg_queue.WaitForMessage(&actual_test_reply));
  EXPECT_EQ(expected_test_reply, actual_test_reply);
}

}  // namespace content
