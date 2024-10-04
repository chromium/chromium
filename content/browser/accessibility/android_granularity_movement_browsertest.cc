// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/accessibility/browser_accessibility_android.h"
#include "content/browser/accessibility/browser_accessibility_manager_android.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

const int GRANULARITY_CHARACTER =
    ANDROID_ACCESSIBILITY_NODE_INFO_MOVEMENT_GRANULARITY_CHARACTER;
const int GRANULARITY_WORD =
    ANDROID_ACCESSIBILITY_NODE_INFO_MOVEMENT_GRANULARITY_WORD;
const int GRANULARITY_LINE =
    ANDROID_ACCESSIBILITY_NODE_INFO_MOVEMENT_GRANULARITY_LINE;

class AndroidGranularityMovementBrowserTest : public ContentBrowserTest {
 public:
  AndroidGranularityMovementBrowserTest() {}
  ~AndroidGranularityMovementBrowserTest() override {}

  ui::BrowserAccessibility* LoadUrlAndGetAccessibilityRoot(const GURL& url) {
    EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

    // Load the page.
    AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                           ui::kAXModeComplete,
                                           ax::mojom::Event::kLoadComplete);
    EXPECT_TRUE(NavigateToURL(shell(), url));
    EXPECT_TRUE(waiter.WaitForNotification());

    // Get the BrowserAccessibilityManager.
    WebContentsImpl* web_contents =
        static_cast<WebContentsImpl*>(shell()->web_contents());
    return web_contents->GetRootBrowserAccessibilityManager()
        ->GetBrowserAccessibilityRoot();
  }

  // First, set accessibility focus to a node and wait for the update that
  // loads inline text boxes for that node. (We load inline text boxes
  // asynchronously on Android since we only ever need them for the node
  // with accessibility focus.)
  //
  // Then call NextAtGranularity repeatedly and return a string that
  // concatenates all of the text of the returned text ranges.
  //
  // As an example, if the node's text is "cat dog" and you traverse by
  // word, this returns "'cat', 'dog'".
  //
  // Also calls PreviousAtGranularity from the end back to the beginning
  // and fails (by logging an error and returning the empty string) if
  // the result when traversing backwards is not the same
  // (but in reverse order).
  std::u16string TraverseNodeAtGranularity(ui::BrowserAccessibility* node,
                                           int granularity) {
    AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                           ui::kAXModeComplete,
                                           ax::mojom::Event::kTreeChanged);
    node->manager()->LoadInlineTextBoxes(*node);
    EXPECT_TRUE(waiter.WaitForNotification());

    int start_index = -1;
    int end_index = -1;
    BrowserAccessibilityAndroid* android_node =
        static_cast<BrowserAccessibilityAndroid*>(node);
    BrowserAccessibilityManagerAndroid* manager =
        static_cast<BrowserAccessibilityManagerAndroid*>(node->manager());
    std::u16string text = android_node->GetTextContentUTF16();
    std::u16string concatenated;
    int previous_end_index = -1;
    while (manager->NextAtGranularity(granularity, end_index, android_node,
                                      &start_index, &end_index)) {
      int len = end_index - start_index;
      std::u16string selection = text.substr(start_index, len);
      if (base::EndsWith(selection, u"\n",
                         base::CompareCase::INSENSITIVE_ASCII)) {
        selection.erase(selection.size() - 1);
      }
      if (!selection.empty()) {
        if (!concatenated.empty()) {
          concatenated += u", ";
        }
        concatenated += u"'" + selection + u"'";
      }

      // Prevent an endless loop.
      if (end_index == previous_end_index) {
        LOG(ERROR) << "Bailing from loop, NextAtGranularity didn't advance";
        break;
      }
      previous_end_index = end_index;
    }

    std::u16string reverse;
    previous_end_index = -1;
    start_index = end_index;
    while (manager->PreviousAtGranularity(
        granularity, start_index, android_node, &start_index, &end_index)) {
      int len = end_index - start_index;
      std::u16string selection = text.substr(start_index, len);
      if (base::EndsWith(selection, u"\n",
                         base::CompareCase::INSENSITIVE_ASCII)) {
        selection = selection.substr(0, selection.size() - 1);
      }
      if (!reverse.empty()) {
        reverse = u", " + reverse;
      }
      reverse = u"'" + selection + u"'" + reverse;

      // Prevent an endless loop.
      if (end_index == previous_end_index) {
        LOG(ERROR) << "Bailing from loop, PreviousAtGranularity didn't advance";
        break;
      }
      previous_end_index = end_index;
    }

    if (concatenated != reverse) {
      LOG(ERROR) << "Did not get the same sequence in the forwards and "
                 << "reverse directions!";
      LOG(ERROR) << "Forwards: " << concatenated;
      LOG(ERROR) << "Backwards " << reverse;
      return std::u16string();
    }

    return concatenated;
  }
};

IN_PROC_BROWSER_TEST_F(AndroidGranularityMovementBrowserTest,
                       NavigateByCharacters) {
  GURL url(
      "data:text/html,"
      "<body>"
      "<p>One, two, three!</p>"
      "<p>"
      "<button aria-label='Seven, eight, nine!'>Four, five, six!</button>"
      "</p>"
      "</body></html>");
  ui::BrowserAccessibility* root = LoadUrlAndGetAccessibilityRoot(url);
  ASSERT_EQ(2U, root->PlatformChildCount());
  ui::BrowserAccessibility* para = root->PlatformGetChild(0);
  ASSERT_EQ(0U, para->PlatformChildCount());
  ui::BrowserAccessibility* button_container = root->PlatformGetChild(1);
  ASSERT_EQ(1U, button_container->PlatformChildCount());
  ui::BrowserAccessibility* button = button_container->PlatformGetChild(0);
  ASSERT_EQ(0U, button->PlatformChildCount());

  EXPECT_EQ(
      u"'O', 'n', 'e', ',', ' ', 't', 'w', 'o', "
      u"',', ' ', 't', 'h', 'r', 'e', 'e', '!'",
      TraverseNodeAtGranularity(para, GRANULARITY_CHARACTER));
  EXPECT_EQ(
      u"'S', 'e', 'v', 'e', 'n', ',', ' ', 'e', 'i', 'g', "
      u"'h', 't', ',', ' ', 'n', 'i', 'n', 'e', '!'",
      TraverseNodeAtGranularity(button, GRANULARITY_CHARACTER));
}

IN_PROC_BROWSER_TEST_F(AndroidGranularityMovementBrowserTest, NavigateByWords) {
  GURL url(
      "data:text/html,"
      "<body>"
      "<p>One, two, three!</p>"
      "<p>"
      "<button aria-label='Seven, eight, nine!'>Four, five, six!</button>"
      "</p>"
      "</body></html>");
  ui::BrowserAccessibility* root = LoadUrlAndGetAccessibilityRoot(url);
  ASSERT_EQ(2U, root->PlatformChildCount());
  ui::BrowserAccessibility* para = root->PlatformGetChild(0);
  ASSERT_EQ(0U, para->PlatformChildCount());
  ui::BrowserAccessibility* button_container = root->PlatformGetChild(1);
  ASSERT_EQ(1U, button_container->PlatformChildCount());
  ui::BrowserAccessibility* button = button_container->PlatformGetChild(0);
  ASSERT_EQ(0U, button->PlatformChildCount());

  EXPECT_EQ(u"'One', ',', 'two', ',', 'three', '!'",
            TraverseNodeAtGranularity(para, GRANULARITY_WORD));
  EXPECT_EQ(u"'Seven', 'eight', 'nine'",
            TraverseNodeAtGranularity(button, GRANULARITY_WORD));
}

IN_PROC_BROWSER_TEST_F(AndroidGranularityMovementBrowserTest, NavigateByLine) {
  GURL url(
      "data:text/html,"
      "<body>"
      "<pre>One,%0dtwo,%0dthree!</pre>"
      "</body>");
  ui::BrowserAccessibility* root = LoadUrlAndGetAccessibilityRoot(url);
  ASSERT_EQ(1U, root->PlatformChildCount());
  ui::BrowserAccessibility* pre = root->PlatformGetChild(0);
  ASSERT_EQ(0U, pre->PlatformChildCount());

  EXPECT_EQ(u"'One,', 'two,', 'three!'",
            TraverseNodeAtGranularity(pre, GRANULARITY_LINE));
}

}  // namespace content
