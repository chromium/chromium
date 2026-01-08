// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/strings/stringprintf.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/test_timeouts.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/scoped_accessibility_mode_override.h"
#include "content/shell/browser/shell.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/platform/browser_accessibility.h"
#include "ui/accessibility/platform/browser_accessibility_manager.h"

namespace content {

namespace {

const char* kEditableTextId = "text-id";

std::vector<std::string> GetEditableTextHtmlTestCases() {
  return {
      base::StringPrintf(R"(<input type="text" id="%s">)", kEditableTextId),
      base::StringPrintf(R"(<textarea id="%s"></textarea>)", kEditableTextId),
      base::StringPrintf(
          R"(<div contenteditable role="textbox" id="%s"></div>)",
          kEditableTextId)};
}

}  // namespace

class ImeAccessibilityBrowserTest
    : public ContentBrowserTest,
      public ::testing::WithParamInterface<std::string> {
 public:
  ImeAccessibilityBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kAccessibilityTextChangeTypes);
  }
  ~ImeAccessibilityBrowserTest() override = default;

 protected:
  ui::BrowserAccessibility* GetFocusedNode() {
    WebContentsImpl* web_contents =
        static_cast<WebContentsImpl*>(shell()->web_contents());
    ui::BrowserAccessibilityManager* manager =
        web_contents->GetRootBrowserAccessibilityManager();
    return manager->GetFocus();
  }

  RenderWidgetHostImpl* GetRenderWidgetHost() {
    return RenderWidgetHostImpl::From(
        shell()->web_contents()->GetPrimaryMainFrame()->GetRenderWidgetHost());
  }

 private:
  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    scoped_accessibility_mode_ =
        std::make_unique<ScopedAccessibilityModeOverride>(ui::kAXModeComplete);
  }

  std::unique_ptr<ScopedAccessibilityModeOverride> scoped_accessibility_mode_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(ImeAccessibilityBrowserTest,
                       AccessibilityNodeAttributes) {
  base::test::ScopedRunLoopTimeout increase_timeout(
      FROM_HERE, TestTimeouts::action_max_timeout());

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  // Wait for page to load.
  AccessibilityNotificationWaiter waiter(web_contents,
                                         ax::mojom::Event::kLoadComplete);
  EXPECT_TRUE(NavigateToURL(shell(), GURL("data:text/html," + GetParam())));
  ASSERT_TRUE(waiter.WaitForNotification());

  // Focus the input field.
  AccessibilityNotificationWaiter focus_waiter(web_contents,
                                               ax::mojom::Event::kFocus);
  ASSERT_TRUE(ExecJs(
      web_contents, base::StringPrintf("document.querySelector('#%s').focus();",
                                       kEditableTextId)));
  ASSERT_TRUE(focus_waiter.WaitForNotification());

  ui::BrowserAccessibility* focused_node = GetFocusedNode();
  ASSERT_TRUE(focused_node);
  EXPECT_EQ(ax::mojom::Role::kTextField, focused_node->GetRole());

  RenderWidgetHostImpl* rwh = GetRenderWidgetHost();

  // Set composition text when a text suggestion is not selected by IME.
  {
    RunUntilInputProcessed(rwh);
    rwh->ImeSetComposition(u"test1", {}, gfx::Range::InvalidRange(), 0, 5,
                           blink::mojom::ImeState::kNone);
    EXPECT_TRUE(base::test::RunUntil([&]() {
      ui::BrowserAccessibility* node = GetFocusedNode();
      return node->GetBoolAttribute(
                 ax::mojom::BoolAttribute::kHasComposition) &&
             !node->GetBoolAttribute(
                 ax::mojom::BoolAttribute::kTextSuggestionSelectedByIME) &&
             node->GetIntAttribute(
                 ax::mojom::IntAttribute::kCommittedTextLength) == 0;
    }));
  }

  // Set composition text when a text suggestion is selected by IME.
  {
    RunUntilInputProcessed(rwh);
    rwh->ImeSetComposition(u"test2", {}, gfx::Range::InvalidRange(), 0, 5,
                           blink::mojom::ImeState::kTextSuggestionSelected);
    EXPECT_TRUE(base::test::RunUntil([&]() {
      ui::BrowserAccessibility* node = GetFocusedNode();
      return node->GetBoolAttribute(
                 ax::mojom::BoolAttribute::kHasComposition) &&
             node->GetBoolAttribute(
                 ax::mojom::BoolAttribute::kTextSuggestionSelectedByIME) &&
             node->GetIntAttribute(
                 ax::mojom::IntAttribute::kCommittedTextLength) == 0;
    }));
  }

  // Commit text.
  {
    const std::u16string commit_text = u"test3";
    RunUntilInputProcessed(rwh);
    rwh->ImeCommitText(commit_text, {}, gfx::Range::InvalidRange(), 5);
    EXPECT_TRUE(base::test::RunUntil([&]() {
      ui::BrowserAccessibility* node = GetFocusedNode();
      return !node->GetBoolAttribute(
                 ax::mojom::BoolAttribute::kHasComposition) &&
             !node->GetBoolAttribute(
                 ax::mojom::BoolAttribute::kTextSuggestionSelectedByIME) &&
             node->GetIntAttribute(
                 ax::mojom::IntAttribute::kCommittedTextLength) ==
                 static_cast<int>(commit_text.length());
    }));
  }
}

IN_PROC_BROWSER_TEST_P(ImeAccessibilityBrowserTest,
                       CommitTextShouldFireValueChangedEvent) {
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  // Wait for page to load.
  AccessibilityNotificationWaiter waiter(web_contents,
                                         ax::mojom::Event::kLoadComplete);
  EXPECT_TRUE(NavigateToURL(shell(), GURL("data:text/html," + GetParam())));
  ASSERT_TRUE(waiter.WaitForNotification());

  // Focus the input field.
  AccessibilityNotificationWaiter focus_waiter(web_contents,
                                               ax::mojom::Event::kFocus);
  ASSERT_TRUE(ExecJs(
      web_contents, base::StringPrintf("document.querySelector('#%s').focus();",
                                       kEditableTextId)));
  ASSERT_TRUE(focus_waiter.WaitForNotification());

  ui::BrowserAccessibility* focused_node = GetFocusedNode();
  ASSERT_TRUE(focused_node);
  EXPECT_EQ(ax::mojom::Role::kTextField, focused_node->GetRole());

  RenderWidgetHostImpl* rwh = GetRenderWidgetHost();
  const std::u16string text = u"test";

  // Set composition text. This should fire a kValueChanged event.
  {
    AccessibilityNotificationWaiter value_changed_waiter(
        web_contents, ax::mojom::Event::kValueChanged);
    RunUntilInputProcessed(rwh);
    rwh->ImeSetComposition(text, {}, gfx::Range::InvalidRange(), 0, 4,
                           blink::mojom::ImeState::kNone);
    ASSERT_TRUE(value_changed_waiter.WaitForNotification());
  }

  // Commit the exact same text. The text value of the input field
  // doesn't change, but a `kValueChanged` event should be generated because
  // of the IME commit action.
  {
    AccessibilityNotificationWaiter value_changed_waiter(
        web_contents, ax::mojom::Event::kValueChanged);
    RunUntilInputProcessed(rwh);
    rwh->ImeCommitText(text, {}, gfx::Range::InvalidRange(), 4);
    ASSERT_TRUE(value_changed_waiter.WaitForNotification());
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         ImeAccessibilityBrowserTest,
                         ::testing::ValuesIn(GetEditableTextHtmlTestCases()));

}  // namespace content
