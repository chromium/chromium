// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_browsertest.h"

#include "base/functional/callback_helpers.h"
#include "base/strings/escape.h"
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "ui/accessibility/platform/browser_accessibility.h"

namespace content {

constexpr char kInputContents[] =
    "Moz/5.0 (ST 6.x; WWW33) "
    "WebKit  \"KHTML, like\".";
constexpr char kTextareaContents[] =
    "Moz/5.0 (ST 6.x; WWW33)\n"
    "WebKit \n\"KHTML, like\".";

gfx::NativeViewAccessible AccessibilityBrowserTest::GetRendererAccessible() {
  content::WebContents* web_contents = shell()->web_contents();
  return web_contents->GetRenderWidgetHostView()->GetNativeViewAccessible();
}

void AccessibilityBrowserTest::ExecuteScript(const std::u16string& script) {
  shell()->web_contents()->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
      script, base::NullCallback(), ISOLATED_WORLD_ID_GLOBAL);
}

void AccessibilityBrowserTest::LoadInitialAccessibilityTreeFromHtml(
    const std::string& html,
    ui::AXMode accessibility_mode) {
  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         accessibility_mode,
                                         ax::mojom::Event::kLoadComplete);
  GURL html_data_url("data:text/html," +
                     base::EscapeQueryParamValue(html, false));
  EXPECT_TRUE(NavigateToURL(shell(), html_data_url));
  ASSERT_TRUE(waiter.WaitForNotification());
}

void AccessibilityBrowserTest::LoadInputField() {
  LoadInitialAccessibilityTreeFromHtml(std::string(
                                           R"HTML(<!DOCTYPE html>
          <html>
          <body>
            <form>
              <label for="textField">Browser name:</label>
              <input type="text" id="textField" name="name" value=")HTML") +
                                       base::EscapeForHTML(kInputContents) +
                                       std::string(R"HTML(">
            </form>
          </body>
          </html>)HTML"));
}

void AccessibilityBrowserTest::LoadScrollableInputField(std::string type) {
  LoadInitialAccessibilityTreeFromHtml(
      std::string(
          R"HTML(<!DOCTYPE html>
          <html>
          <body>
            <input type=")HTML") +
      type + std::string(R"HTML(" style="width: 150px;" value=")HTML") +
      base::EscapeForHTML(kInputContents) + std::string(R"HTML(">
          </body>
          </html>)HTML"));
}

void AccessibilityBrowserTest::LoadTextareaField() {
  LoadInitialAccessibilityTreeFromHtml(std::string(R"HTML(<!DOCTYPE html>
      <html>
      <body>
                    <textarea rows="3" cols="60">)HTML") +
                                       base::EscapeForHTML(kTextareaContents) +
                                       std::string(R"HTML(</textarea>
          </body>
          </html>)HTML"));
}

void AccessibilityBrowserTest::LoadSampleParagraph(
    ui::AXMode accessibility_mode) {
  LoadInitialAccessibilityTreeFromHtml(
      R"HTML(<!DOCTYPE html>
      <html>
      <body>
          <p><b>Game theory</b> is "the study of
              <a href="" title="Mathematical model">mathematical models</a>
              of conflict and<br>cooperation between intelligent rational
              decision-makers."
          </p>
      </body>
      </html>)HTML",
      accessibility_mode);
}

// Loads a page with a content editable whose text overflows its height.
// Places the caret at the beginning of the editable's last line but doesn't
// scroll the editable.
void AccessibilityBrowserTest::LoadSampleParagraphInScrollableEditable() {
  LoadInitialAccessibilityTreeFromHtml(
      R"HTML(<p contenteditable="true"
          style="height: 30px; overflow: scroll;">
          hello<br><br><br>hello
      </p>)HTML");

  AccessibilityNotificationWaiter selection_waiter(
      shell()->web_contents(), ui::kAXModeComplete,
      ui::AXEventGenerator::Event::TEXT_SELECTION_CHANGED);
  ExecuteScript(
      u"let selection=document.getSelection();"
      u"let range=document.createRange();"
      u"let editable=document.querySelector('p[contenteditable=\"true\"]');"
      u"editable.focus();"
      u"range.setStart(editable.lastChild, 0);"
      u"range.setEnd(editable.lastChild, 0);"
      u"selection.removeAllRanges();"
      u"selection.addRange(range);");
  ASSERT_TRUE(selection_waiter.WaitForNotification());
}

// Loads a page with a paragraph of sample text which is below the
// bottom of the screen.
void AccessibilityBrowserTest::LoadSampleParagraphInScrollableDocument(
    ui::AXMode accessibility_mode) {
  LoadInitialAccessibilityTreeFromHtml(
      R"HTML(<!DOCTYPE html>
      <html>
      <body>
        <p style="margin-top:50vh; margin-bottom:200vh">
            <b>Game theory</b> is "the study of
            <a href="" title="Mathematical model">mathematical models</a>
            of conflict and<br>cooperation between intelligent rational
            decision-makers."
        </p>
      </body>
      </html>)HTML",
      accessibility_mode);
}

// static
std::string AccessibilityBrowserTest::InputContentsString() {
  return kInputContents;
}

// static
std::string AccessibilityBrowserTest::TextAreaContentsString() {
  return kTextareaContents;
}

}  // namespace content
