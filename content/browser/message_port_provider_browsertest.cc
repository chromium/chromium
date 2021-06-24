// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/macros.h"
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
  const std::string target_origin(url.GetOrigin().spec());
  const std::string source_origin("https://source.origin.com");
  const std::string message("success");
  DOMMessageQueue msg_queue;
  MessagePortProvider::PostMessageToFrame(
      shell()->web_contents()->GetPrimaryPage(),
      base::UTF8ToUTF16(source_origin), base::UTF8ToUTF16(target_origin),
      base::UTF8ToUTF16(message));

  // Verify that the message was received (and had the expected payload).
  std::string expected_test_reply;
  expected_test_reply += '"';
  expected_test_reply += source_origin;
  expected_test_reply += ':';
  expected_test_reply += message;
  expected_test_reply += '"';
  std::string actual_test_reply;
  EXPECT_TRUE(msg_queue.WaitForMessage(&actual_test_reply));
  EXPECT_EQ(expected_test_reply, actual_test_reply);
}

}  // namespace content
