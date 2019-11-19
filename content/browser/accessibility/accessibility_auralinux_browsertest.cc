// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atk/atk.h>
#include <dlfcn.h>

#include "base/bind_helpers.h"
#include "base/macros.h"
#include "content/browser/accessibility/accessibility_browsertest.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "ui/accessibility/platform/ax_platform_node_auralinux.h"

namespace content {

namespace {

AtkObject* FindAtkObjectParentFrame(AtkObject* atk_object) {
  while (atk_object) {
    if (atk_object_get_role(atk_object) == ATK_ROLE_FRAME)
      return atk_object;
    atk_object = atk_object_get_parent(atk_object);
  }
  return nullptr;
}

static bool IsAtkObjectFocused(AtkObject* object) {
  AtkStateSet* state_set = atk_object_ref_state_set(object);
  bool result = atk_state_set_contains_state(state_set, ATK_STATE_FOCUSED);
  g_object_unref(state_set);
  return result;
}

}  // namespace

class AccessibilityAuraLinuxBrowserTest : public AccessibilityBrowserTest {
 public:
  AccessibilityAuraLinuxBrowserTest() = default;
  ~AccessibilityAuraLinuxBrowserTest() override = default;

 protected:
  static bool HasObjectWithAtkRoleFrameInAncestry(AtkObject* object) {
    while (object) {
      if (atk_object_get_role(object) == ATK_ROLE_FRAME)
        return true;
      object = atk_object_get_parent(object);
    }
    return false;
  }

  static void CheckTextAtOffset(AtkText* text_object,
                                int offset,
                                AtkTextBoundary boundary_type,
                                int expected_start_offset,
                                int expected_end_offset,
                                const char* expected_text);

  AtkText* SetUpInputField();
  AtkText* SetUpTextareaField();
  AtkText* SetUpSampleParagraph();
  AtkText* SetUpSampleParagraphInScrollableDocument();

  AtkText* GetSampleParagraph();
  AtkText* GetAtkTextForChild(AtkRole expected_role);

 private:
  DISALLOW_COPY_AND_ASSIGN(AccessibilityAuraLinuxBrowserTest);
};

AtkText* AccessibilityAuraLinuxBrowserTest::GetAtkTextForChild(
    AtkRole expected_role) {
  AtkObject* document = GetRendererAccessible();
  EXPECT_EQ(1, atk_object_get_n_accessible_children(document));

  AtkObject* parent_element = atk_object_ref_accessible_child(document, 0);
  int number_of_children = atk_object_get_n_accessible_children(parent_element);
  EXPECT_LT(0, number_of_children);

  // The input field is always the last child.
  AtkObject* input =
      atk_object_ref_accessible_child(parent_element, number_of_children - 1);
  EXPECT_EQ(expected_role, atk_object_get_role(input));

  EXPECT_TRUE(ATK_IS_TEXT(input));
  AtkText* atk_text = ATK_TEXT(input);

  g_object_unref(parent_element);

  return atk_text;
}

// Loads a page with  an input text field and places sample text in it.
AtkText* AccessibilityAuraLinuxBrowserTest::SetUpInputField() {
  LoadInputField();
  return GetAtkTextForChild(ATK_ROLE_ENTRY);
}

// Loads a page with  a textarea text field and places sample text in it. Also,
// places the caret before the last character.
AtkText* AccessibilityAuraLinuxBrowserTest::SetUpTextareaField() {
  LoadTextareaField();
  return GetAtkTextForChild(ATK_ROLE_ENTRY);
}

// Loads a page with a paragraph of sample text.
AtkText* AccessibilityAuraLinuxBrowserTest::SetUpSampleParagraph() {
  LoadSampleParagraph();

  AtkObject* document = GetRendererAccessible();
  EXPECT_EQ(1, atk_object_get_n_accessible_children(document));

  int number_of_children = atk_object_get_n_accessible_children(document);
  EXPECT_LT(0, number_of_children);

  // The input field is always the last child.
  AtkObject* input = atk_object_ref_accessible_child(document, 0);
  EXPECT_EQ(ATK_ROLE_PARAGRAPH, atk_object_get_role(input));

  EXPECT_TRUE(ATK_IS_TEXT(input));
  return ATK_TEXT(input);
}

AtkText* AccessibilityAuraLinuxBrowserTest::GetSampleParagraph() {
  AtkObject* document = GetRendererAccessible();
  EXPECT_EQ(1, atk_object_get_n_accessible_children(document));

  int number_of_children = atk_object_get_n_accessible_children(document);
  EXPECT_LT(0, number_of_children);

  // The input field is always the last child.
  AtkObject* input = atk_object_ref_accessible_child(document, 0);
  EXPECT_EQ(ATK_ROLE_PARAGRAPH, atk_object_get_role(input));

  EXPECT_TRUE(ATK_IS_TEXT(input));
  return ATK_TEXT(input);
}

// Ensures that the text and the start and end offsets retrieved using
// get_textAtOffset match the expected values.
void AccessibilityAuraLinuxBrowserTest::CheckTextAtOffset(
    AtkText* text_object,
    int offset,
    AtkTextBoundary boundary_type,
    int expected_start_offset,
    int expected_end_offset,
    const char* expected_text) {
  testing::Message message;
  message << "While checking at index \'" << offset << "\' for \'"
          << expected_text << "\' at " << expected_start_offset << '-'
          << expected_end_offset << '.';
  SCOPED_TRACE(message);

  int start_offset = 0;
  int end_offset = 0;
  char* text = atk_text_get_text_at_offset(text_object, offset, boundary_type,
                                           &start_offset, &end_offset);
  EXPECT_EQ(expected_start_offset, start_offset);
  EXPECT_EQ(expected_end_offset, end_offset);
  EXPECT_STREQ(expected_text, text);
  g_free(text);
}

IN_PROC_BROWSER_TEST_F(AccessibilityAuraLinuxBrowserTest,
                       AuraLinuxBrowserAccessibleParent) {
  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);
  EXPECT_TRUE(NavigateToURL(shell(), GURL("data:text/html,")));
  waiter.WaitForNotification();

  // Get the BrowserAccessibilityManager.
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  BrowserAccessibilityManager* manager =
      web_contents->GetRootBrowserAccessibilityManager();
  ASSERT_NE(nullptr, manager);

  auto* host_view = static_cast<RenderWidgetHostViewAura*>(
      web_contents->GetRenderWidgetHostView());
  ASSERT_NE(nullptr, host_view->GetNativeViewAccessible());

  AtkObject* host_view_parent =
      host_view->AccessibilityGetNativeViewAccessible();
  ASSERT_NE(nullptr, host_view_parent);
  ASSERT_TRUE(
      AccessibilityAuraLinuxBrowserTest::HasObjectWithAtkRoleFrameInAncestry(
          host_view_parent));
}

IN_PROC_BROWSER_TEST_F(AccessibilityAuraLinuxBrowserTest,
                       TestMultilingualTextAtOffsetWithBoundaryCharacter) {
  AtkText* atk_text = SetUpInputField();
  ASSERT_NE(nullptr, atk_text);
  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kValueChanged);
  // Place an e acute, and two emoticons in the text field.
  ExecuteScript(base::UTF8ToUTF16(R"SCRIPT(
      const input = document.querySelector('input');
      input.value =
          'e\u0301\uD83D\uDC69\u200D\u2764\uFE0F\u200D\uD83D\uDC69\uD83D\uDC36';
      )SCRIPT"));
  waiter.WaitForNotification();

  int character_count = atk_text_get_character_count(atk_text);
  // "character_count" is the number of actual characters, not the number of
  // UTF16 code units.
  //
  // Currently, this doesn't properly count grapheme clusters, but it does
  // handle surrogate pairs.
  // TODO(nektar): Implement support for base::OffsetAdjuster in AXPosition.
  ASSERT_EQ(9, character_count);

  // The expected text consists of an e acute, and two emoticons, but
  // not every multi-byte character is a surrogate pair.
  CheckTextAtOffset(atk_text, 0, ATK_TEXT_BOUNDARY_CHAR, 0, 2,
                    base::WideToUTF8(L"e\x0301").c_str());
  CheckTextAtOffset(atk_text, 1, ATK_TEXT_BOUNDARY_CHAR, 0, 2,
                    base::WideToUTF8(L"e\x0301").c_str());
  CheckTextAtOffset(atk_text, 2, ATK_TEXT_BOUNDARY_CHAR, 2, 8,
                    "\xF0\x9F\x91\xA9\xE2\x80\x8D\xE2\x9D\xA4\xEF\xB8\x8F\xE2"
                    "\x80\x8D\xF0\x9F\x91\xA9");
  CheckTextAtOffset(atk_text, 3, ATK_TEXT_BOUNDARY_CHAR, 2, 8,
                    "\xF0\x9F\x91\xA9\xE2\x80\x8D\xE2\x9D\xA4\xEF\xB8\x8F\xE2"
                    "\x80\x8D\xF0\x9F\x91\xA9");
  CheckTextAtOffset(atk_text, 4, ATK_TEXT_BOUNDARY_CHAR, 2, 8,
                    "\xF0\x9F\x91\xA9\xE2\x80\x8D\xE2\x9D\xA4\xEF\xB8\x8F\xE2"
                    "\x80\x8D\xF0\x9F\x91\xA9");
  CheckTextAtOffset(atk_text, 5, ATK_TEXT_BOUNDARY_CHAR, 2, 8,
                    "\xF0\x9F\x91\xA9\xE2\x80\x8D\xE2\x9D\xA4\xEF\xB8\x8F\xE2"
                    "\x80\x8D\xF0\x9F\x91\xA9");
  CheckTextAtOffset(atk_text, 6, ATK_TEXT_BOUNDARY_CHAR, 2, 8,
                    "\xF0\x9F\x91\xA9\xE2\x80\x8D\xE2\x9D\xA4\xEF\xB8\x8F\xE2"
                    "\x80\x8D\xF0\x9F\x91\xA9");
  CheckTextAtOffset(atk_text, 7, ATK_TEXT_BOUNDARY_CHAR, 2, 8,
                    "\xF0\x9F\x91\xA9\xE2\x80\x8D\xE2\x9D\xA4\xEF\xB8\x8F\xE2"
                    "\x80\x8D\xF0\x9F\x91\xA9");
  CheckTextAtOffset(atk_text, 8, ATK_TEXT_BOUNDARY_CHAR, 8, 9,
                    "\xF0\x9F\x90\xB6");
  CheckTextAtOffset(atk_text, 9, ATK_TEXT_BOUNDARY_CHAR, 0, 0, nullptr);

  g_object_unref(atk_text);
}

IN_PROC_BROWSER_TEST_F(AccessibilityAuraLinuxBrowserTest,
                       TestTextAtOffsetWithBoundaryLine) {
  AtkText* atk_text = SetUpInputField();

  // Single line text fields should return the whole text.
  CheckTextAtOffset(atk_text, 0, ATK_TEXT_BOUNDARY_LINE_START, 0,
                    InputContentsString().size(),
                    InputContentsString().c_str());

  g_object_unref(atk_text);
}

IN_PROC_BROWSER_TEST_F(AccessibilityAuraLinuxBrowserTest,
                       TestMultiLineTextAtOffsetWithBoundaryLine) {
  AtkText* atk_text = SetUpTextareaField();

  CheckTextAtOffset(atk_text, 0, ATK_TEXT_BOUNDARY_LINE_START, 0, 24,
                    "Moz/5.0 (ST 6.x; WWW33)\n");

  // If the offset is at the newline, return the line preceding it.
  CheckTextAtOffset(atk_text, 31, ATK_TEXT_BOUNDARY_LINE_START, 24, 32,
                    "WebKit \n");

  // Last line does not have a trailing newline.
  CheckTextAtOffset(atk_text, 32, ATK_TEXT_BOUNDARY_LINE_START, 32,
                    InputContentsString().size(), "\"KHTML, like\".");

  g_object_unref(atk_text);
}

IN_PROC_BROWSER_TEST_F(AccessibilityAuraLinuxBrowserTest,
                       TestParagraphTextAtOffsetWithBoundaryLine) {
  AtkText* atk_text = SetUpSampleParagraph();

  // There should be two lines in this paragraph.
  const int newline_offset = 46;
  int n_characters = atk_text_get_character_count(atk_text);
  ASSERT_LT(newline_offset, n_characters);

  const base::string16 string16_embed(
      1, ui::AXPlatformNodeAuraLinux::kEmbeddedCharacter);
  std::string expected_string = "Game theory is \"the study of " +
                                base::UTF16ToUTF8(string16_embed) +
                                " of conflict and\n";
  for (int i = 0; i <= newline_offset; ++i) {
    CheckTextAtOffset(atk_text, i, ATK_TEXT_BOUNDARY_LINE_START, 0,
                      newline_offset + 1, expected_string.c_str());
  }

  for (int i = newline_offset + 1; i < n_characters; ++i) {
    CheckTextAtOffset(
        atk_text, i, ATK_TEXT_BOUNDARY_LINE_START, newline_offset + 1,
        n_characters,
        "cooperation between intelligent rational decision-makers.\"");
  }

  g_object_unref(atk_text);
}

IN_PROC_BROWSER_TEST_F(AccessibilityAuraLinuxBrowserTest,
                       TestParagraphTextAtOffsetWithBoundarySentence) {
  LoadInitialAccessibilityTreeFromHtml(std::string(
      R"HTML(<!DOCTYPE html>
          <html>
          <body>
            <div>One sentence. Two sentences. Three sentences!</div>
          </body>
          </html>)HTML"));

  AtkObject* document = GetRendererAccessible();
  EXPECT_EQ(1, atk_object_get_n_accessible_children(document));

  AtkText* div_element = ATK_TEXT(atk_object_ref_accessible_child(document, 0));
  EXPECT_EQ(1, atk_object_get_n_accessible_children(ATK_OBJECT(div_element)));
  AtkText* atk_text =
      ATK_TEXT(atk_object_ref_accessible_child(ATK_OBJECT(div_element), 0));

  int first_sentence_offset = 14;
  int second_sentence_offset = first_sentence_offset + 15;
  int third_sentence_offset = second_sentence_offset + 16;

  for (int i = 0; i < first_sentence_offset; ++i) {
    CheckTextAtOffset(atk_text, i, ATK_TEXT_BOUNDARY_SENTENCE_START, 0,
                      first_sentence_offset, "One sentence. ");
  }

  for (int i = first_sentence_offset + 1; i < second_sentence_offset; ++i) {
    CheckTextAtOffset(atk_text, i, ATK_TEXT_BOUNDARY_SENTENCE_START,
                      first_sentence_offset, second_sentence_offset,
                      "Two sentences. ");
  }

  for (int i = second_sentence_offset + 1; i < third_sentence_offset; ++i) {
    CheckTextAtOffset(atk_text, i, ATK_TEXT_BOUNDARY_SENTENCE_START,
                      second_sentence_offset, third_sentence_offset,
                      "Three sentences!");
  }

  g_object_unref(atk_text);
  g_object_unref(div_element);
}

#if defined(ATK_CHECK_VERSION) && ATK_CHECK_VERSION(2, 30, 0)
#define ATK_230
#endif

IN_PROC_BROWSER_TEST_F(AccessibilityAuraLinuxBrowserTest,
                       TestCharacterExtentsWithInvalidArguments) {
  AtkText* atk_text = SetUpSampleParagraph();

  int invalid_offset = -3;
  int x = -1, y = -1;
  int width = -1, height = -1;

  atk_text_get_character_extents(atk_text, invalid_offset, &x, &y, &width,
                                 &height, ATK_XY_SCREEN);
  EXPECT_EQ(0, x);
  EXPECT_EQ(0, y);
  EXPECT_EQ(0, width);
  EXPECT_EQ(0, height);

#ifdef ATK_230
  atk_text_get_character_extents(atk_text, invalid_offset, &x, &y, &width,
                                 &height, ATK_XY_PARENT);
  EXPECT_EQ(0, x);
  EXPECT_EQ(0, y);
  EXPECT_EQ(0, width);
  EXPECT_EQ(0, height);
#endif  // ATK_230

  atk_text_get_character_extents(atk_text, invalid_offset, &x, &y, &width,
                                 &height, ATK_XY_WINDOW);
  EXPECT_EQ(0, x);
  EXPECT_EQ(0, y);
  EXPECT_EQ(0, width);
  EXPECT_EQ(0, height);

  int n_characters = atk_text_get_character_count(atk_text);
  ASSERT_LT(0, n_characters);

  atk_text_get_character_extents(atk_text, invalid_offset, &x, &y, &width,
                                 &height, ATK_XY_SCREEN);
  EXPECT_EQ(0, x);
  EXPECT_EQ(0, y);
  EXPECT_EQ(0, width);
  EXPECT_EQ(0, height);

#ifdef ATK_230
  atk_text_get_character_extents(atk_text, invalid_offset, &x, &y, &width,
                                 &height, ATK_XY_PARENT);
  EXPECT_EQ(0, x);
  EXPECT_EQ(0, y);
  EXPECT_EQ(0, width);
  EXPECT_EQ(0, height);
#endif  // ATK_230

  atk_text_get_character_extents(atk_text, invalid_offset, &x, &y, &width,
                                 &height, ATK_XY_WINDOW);
  EXPECT_EQ(0, x);
  EXPECT_EQ(0, y);
  EXPECT_EQ(0, width);
  EXPECT_EQ(0, height);

  g_object_unref(atk_text);
}

AtkCoordType kCoordinateTypes[] = {
    ATK_XY_SCREEN,
    ATK_XY_WINDOW,
#ifdef ATK_230
    ATK_XY_PARENT,
#endif  // ATK_230
};

IN_PROC_BROWSER_TEST_F(AccessibilityAuraLinuxBrowserTest,
                       TestCharacterExtentsInEditable) {
  AtkText* atk_text = SetUpSampleParagraph();

  constexpr int newline_offset = 46;
  int n_characters = atk_text_get_character_count(atk_text);
  ASSERT_EQ(105, n_characters);

  int x, y, width, height;
  int previous_x, previous_y, previous_height;
  for (AtkCoordType coordinate_type : kCoordinateTypes) {
    atk_text_get_character_extents(atk_text, 0, &x, &y, &width, &height,
                                   coordinate_type);
    EXPECT_LT(0, x) << "at offset 0";
    EXPECT_LT(0, y) << "at offset 0";
    EXPECT_LT(1, width) << "at offset 0";
    EXPECT_LT(1, height) << "at offset 0";

    gfx::Rect combined_extents(x, y, width, height);
    for (int offset = 1; offset < newline_offset; ++offset) {
      testing::Message message;
      message << "While checking at offset " << offset;
      SCOPED_TRACE(message);

      previous_x = x;
      previous_y = y;
      previous_height = height;

      atk_text_get_character_extents(atk_text, offset, &x, &y, &width, &height,
                                     coordinate_type);
      EXPECT_LT(previous_x, x);
      EXPECT_EQ(previous_y, y);
      EXPECT_LT(1, width);
      EXPECT_EQ(previous_height, height);

      combined_extents.Union(gfx::Rect(x, y, width, height));
      atk_text_get_character_extents(atk_text, offset, &x, &y, &width, &height,
                                     coordinate_type);

      AtkTextRectangle atk_rect;
      atk_text_get_range_extents(atk_text, 0, offset + 1, coordinate_type,
                                 &atk_rect);
      EXPECT_EQ(combined_extents.x(), atk_rect.x);
      EXPECT_EQ(combined_extents.y(), atk_rect.y);
      EXPECT_EQ(combined_extents.width(), atk_rect.width);
      EXPECT_EQ(combined_extents.height(), atk_rect.height);
    }

    {
      testing::Message message;
      message << "While checking at offset " << newline_offset + 1;
      SCOPED_TRACE(message);

      atk_text_get_character_extents(atk_text, newline_offset + 1, &x, &y,
                                     &width, &height, coordinate_type);
      EXPECT_LE(0, x);
      EXPECT_GT(previous_x, x);
      EXPECT_LT(previous_y, y);
      EXPECT_LT(1, width);
      EXPECT_EQ(previous_height, height);
    }

    combined_extents = gfx::Rect(x, y, width, height);
    for (int offset = newline_offset + 2; offset < n_characters; ++offset) {
      testing::Message message;
      message << "While checking at offset " << offset;
      SCOPED_TRACE(message);

      previous_x = x;
      previous_y = y;
      previous_height = height;

      atk_text_get_character_extents(atk_text, offset, &x, &y, &width, &height,
                                     coordinate_type);
      EXPECT_LT(previous_x, x);
      EXPECT_EQ(previous_y, y);
      EXPECT_LT(1, width);
      EXPECT_EQ(previous_height, height);

      combined_extents.Union(gfx::Rect(x, y, width, height));
      atk_text_get_character_extents(atk_text, offset, &x, &y, &width, &height,
                                     coordinate_type);

      AtkTextRectangle atk_rect;
      atk_text_get_range_extents(atk_text, newline_offset + 1, offset + 1,
                                 coordinate_type, &atk_rect);
      EXPECT_EQ(combined_extents.x(), atk_rect.x);
      EXPECT_EQ(combined_extents.y(), atk_rect.y);
      EXPECT_EQ(combined_extents.width(), atk_rect.width);
      EXPECT_EQ(combined_extents.height(), atk_rect.height);
    }
  }

  g_object_unref(atk_text);
}

IN_PROC_BROWSER_TEST_F(AccessibilityAuraLinuxBrowserTest,
                       TestCharacterExtentsInScrollableEditable) {
  LoadSampleParagraphInScrollableEditable();

  // By construction, only the first line of the content editable is visible.
  AtkText* atk_text = GetSampleParagraph();

  constexpr int first_line_end = 5;
  constexpr int last_line_start = 8;

  int n_characters = atk_text_get_character_count(atk_text);
  ASSERT_EQ(13, n_characters);

  int x, y, width, height;
  int previous_x, previous_y, previous_height;
  for (AtkCoordType coordinate_type : kCoordinateTypes) {
    // Test that non offscreen characters have increasing x coordinates and a
    // height that is greater than 1px.
    {
      testing::Message message;
      message << "While checking at offset 0";
      SCOPED_TRACE(message);

      atk_text_get_character_extents(atk_text, 0, &x, &y, &width, &height,
                                     coordinate_type);
      EXPECT_LT(0, x);
      EXPECT_LT(0, y);
      EXPECT_LT(1, width);
      EXPECT_LT(1, height);
    }

    for (int offset = 1; offset < first_line_end; ++offset) {
      testing::Message message;
      message << "While checking at offset " << offset;
      SCOPED_TRACE(message);

      previous_x = x;
      previous_y = y;
      previous_height = height;

      atk_text_get_character_extents(atk_text, offset, &x, &y, &width, &height,
                                     coordinate_type);
      EXPECT_LT(previous_x, x);
      EXPECT_EQ(previous_y, y);
      EXPECT_LT(1, width);
      EXPECT_EQ(previous_height, height);
    }

    {
      testing::Message message;
      message << "While checking at offset " << last_line_start;
      SCOPED_TRACE(message);

      atk_text_get_character_extents(atk_text, last_line_start, &x, &y, &width,
                                     &height, coordinate_type);
      EXPECT_LT(0, x);
      EXPECT_LT(previous_y, y);
      EXPECT_LT(1, width);
      EXPECT_EQ(previous_height, height);
    }

    for (int offset = last_line_start + 1; offset < n_characters; ++offset) {
      testing::Message message;
      message << "While checking at offset " << offset;
      SCOPED_TRACE(message);

      previous_x = x;
      previous_y = y;

      atk_text_get_character_extents(atk_text, offset, &x, &y, &width, &height,
                                     coordinate_type);
      EXPECT_LT(previous_x, x);
      EXPECT_EQ(previous_y, y);
      EXPECT_LT(1, width);
      EXPECT_EQ(previous_height, height);
    }
  }

  g_object_unref(atk_text);
}

#if defined(ATK_230)
typedef bool (*ScrollToPointFunc)(AtkComponent* component,
                                  AtkCoordType coords,
                                  gint x,
                                  gint y);
typedef bool (*ScrollToFunc)(AtkComponent* component, AtkScrollType type);

IN_PROC_BROWSER_TEST_F(AccessibilityAuraLinuxBrowserTest, TestScrollToPoint) {
  // There's a chance we may be compiled with a newer version of ATK and then
  // run with an older one, so we need to do a runtime check for this method
  // that is available in ATK 2.30 instead of linking directly.
  ScrollToPointFunc scroll_to_point = reinterpret_cast<ScrollToPointFunc>(
      dlsym(RTLD_DEFAULT, "atk_component_scroll_to_point"));
  if (!scroll_to_point) {
    LOG(WARNING)
        << "Skipping AccessibilityAuraLinuxBrowserTest::TestScrollToPoint"
           " because ATK version < 2.30 detected.";
    return;
  }

  LoadSampleParagraphInScrollableDocument();
  AtkText* atk_text = GetSampleParagraph();
  ASSERT_TRUE(ATK_IS_COMPONENT(atk_text));
  AtkComponent* atk_component = ATK_COMPONENT(atk_text);

  int prev_x, prev_y, x, y;
  atk_component_get_extents(atk_component, &prev_x, &prev_y, nullptr, nullptr,
                            ATK_XY_SCREEN);

  AccessibilityNotificationWaiter location_changed_waiter(
      shell()->web_contents(), ui::kAXModeComplete,
      ax::mojom::Event::kLocationChanged);
  scroll_to_point(atk_component, ATK_XY_PARENT, 0, 0);
  location_changed_waiter.WaitForNotification();

  atk_component_get_extents(atk_component, &x, &y, nullptr, nullptr,
                            ATK_XY_SCREEN);
  EXPECT_EQ(prev_x, x);
  EXPECT_GT(prev_y, y);

  constexpr int kScrollToY = 0;
  scroll_to_point(atk_component, ATK_XY_SCREEN, 0, kScrollToY);
  location_changed_waiter.WaitForNotification();
  atk_component_get_extents(atk_component, &x, &y, nullptr, nullptr,
                            ATK_XY_SCREEN);
  EXPECT_EQ(kScrollToY, y);

  constexpr int kScrollToY_2 = 243;
  scroll_to_point(atk_component, ATK_XY_SCREEN, 0, kScrollToY_2);
  location_changed_waiter.WaitForNotification();
  atk_component_get_extents(atk_component, nullptr, &y, nullptr, nullptr,
                            ATK_XY_SCREEN);
  EXPECT_EQ(kScrollToY_2, y);

  scroll_to_point(atk_component, ATK_XY_SCREEN, 0, 129);
  location_changed_waiter.WaitForNotification();
  atk_component_get_extents(atk_component, nullptr, &y, nullptr, nullptr,
                            ATK_XY_SCREEN);

  AtkObject* frame = FindAtkObjectParentFrame(ATK_OBJECT(atk_component));
  int frame_y;
  atk_component_get_extents(ATK_COMPONENT(frame), nullptr, &frame_y, nullptr,
                            nullptr, ATK_XY_SCREEN);
  EXPECT_EQ(frame_y, y);

  g_object_unref(atk_text);
}

IN_PROC_BROWSER_TEST_F(AccessibilityAuraLinuxBrowserTest, TestScrollTo) {
  // There's a chance we may be compiled with a newer version of ATK and then
  // run with an older one, so we need to do a runtime check for this method
  // that is available in ATK 2.30 instead of linking directly.
  ScrollToFunc scroll_to = reinterpret_cast<ScrollToFunc>(
      dlsym(RTLD_DEFAULT, "atk_component_scroll_to"));
  if (!scroll_to) {
    LOG(WARNING) << "Skipping AccessibilityAuraLinuxBrowserTest::TestScrollTo"
                    " because ATK version < 2.30 detected.";
    return;
  }

  LoadInitialAccessibilityTreeFromHtml(
      R"HTML(<!DOCTYPE html>
      <html>
      <body>
        <div style="height: 5000px; width: 5000px;"></div>
        <img style="display: relative; left: 1000px;" alt="Target1">
        <div style="height: 5000px;"></div>
        <img style="display: relative; left: 1000px;" alt="Target2">
        <div style="height: 5000px;"></div>
        <span>Target 3</span>
        <div style="height: 5000px;"></div>
      </body>
      </html>)HTML");

  // Retrieve the AtkObject interface for the document node.
  AtkObject* document = GetRendererAccessible();
  ASSERT_TRUE(ATK_IS_COMPONENT(document));

  // Get the dimensions of the document.
  int doc_x, doc_y, doc_width, doc_height;
  atk_component_get_extents(ATK_COMPONENT(document), &doc_x, &doc_y, &doc_width,
                            &doc_height, ATK_XY_SCREEN);

  // The document should only have three children, two img elements
  // and a single span element.
  ASSERT_EQ(3, atk_object_get_n_accessible_children(document));

  AtkObject* target = atk_object_ref_accessible_child(document, 0);
  AtkObject* target2 = atk_object_ref_accessible_child(document, 1);
  AtkObject* target3 = atk_object_ref_accessible_child(document, 2);

  ASSERT_TRUE(ATK_IS_COMPONENT(target));
  ASSERT_TRUE(ATK_IS_COMPONENT(target2));
  ASSERT_TRUE(ATK_IS_COMPONENT(target3));

  ASSERT_EQ(ATK_ROLE_IMAGE, atk_object_get_role(target));
  ASSERT_EQ(ATK_ROLE_IMAGE, atk_object_get_role(target2));

  AccessibilityNotificationWaiter waiter(
      shell()->web_contents(), ui::kAXModeComplete,
      ax::mojom::Event::kScrollPositionChanged);
  ASSERT_TRUE(scroll_to(ATK_COMPONENT(target), ATK_SCROLL_TOP_EDGE));
  waiter.WaitForNotification();
  int x, y;
  atk_component_get_extents(ATK_COMPONENT(target), &x, &y, nullptr, nullptr,
                            ATK_XY_SCREEN);
  EXPECT_EQ(y, doc_y);
  EXPECT_NE(x, doc_x);

  ASSERT_TRUE(scroll_to(ATK_COMPONENT(target), ATK_SCROLL_TOP_LEFT));
  waiter.WaitForNotification();
  atk_component_get_extents(ATK_COMPONENT(target), &x, &y, nullptr, nullptr,
                            ATK_XY_SCREEN);
  EXPECT_EQ(y, doc_y);
  EXPECT_EQ(x, doc_x);

  ASSERT_TRUE(scroll_to(ATK_COMPONENT(target), ATK_SCROLL_BOTTOM_EDGE));
  waiter.WaitForNotification();
  atk_component_get_extents(ATK_COMPONENT(target), &x, &y, nullptr, nullptr,
                            ATK_XY_SCREEN);
  EXPECT_NE(y, doc_y);
  EXPECT_EQ(x, doc_x);

  ASSERT_TRUE(scroll_to(ATK_COMPONENT(target), ATK_SCROLL_RIGHT_EDGE));
  waiter.WaitForNotification();
  atk_component_get_extents(ATK_COMPONENT(target), &x, &y, nullptr, nullptr,
                            ATK_XY_SCREEN);
  EXPECT_NE(y, doc_y);
  EXPECT_NE(x, doc_x);

  ASSERT_TRUE(scroll_to(ATK_COMPONENT(target2), ATK_SCROLL_LEFT_EDGE));
  waiter.WaitForNotification();
  atk_component_get_extents(ATK_COMPONENT(target2), &x, &y, nullptr, nullptr,
                            ATK_XY_SCREEN);
  EXPECT_NE(y, doc_y);
  EXPECT_EQ(x, doc_x);

  ASSERT_TRUE(scroll_to(ATK_COMPONENT(target2), ATK_SCROLL_TOP_LEFT));
  waiter.WaitForNotification();
  atk_component_get_extents(ATK_COMPONENT(target2), &x, &y, nullptr, nullptr,
                            ATK_XY_SCREEN);
  EXPECT_EQ(y, doc_y);
  EXPECT_EQ(x, doc_x);

  // Orca expects atk_text_set_caret_offset to operate like scroll to the
  // target node like atk_component_scroll_to, so we test that here.
  ASSERT_TRUE(ATK_IS_TEXT(target3));
  AccessibilityNotificationWaiter waiter3(
      shell()->web_contents(), ui::kAXModeComplete,
      ax::mojom::Event::kScrollPositionChanged);
  atk_text_set_caret_offset(ATK_TEXT(target3), 0);
  waiter3.WaitForNotification();

  // The text area should be scrolled to somewhere near the bottom of the
  // screen. We check that it is in the bottom quarter of the screen here,
  // but still fully onscreen.
  int height;
  atk_component_get_extents(ATK_COMPONENT(target3), nullptr, &y, nullptr,
                            &height, ATK_XY_SCREEN);

  int doc_bottom = doc_y + doc_height;
  int bottom_third = doc_bottom - (float{doc_height} / 3.0);
  EXPECT_GT(y, bottom_third);
  EXPECT_LT(y, doc_bottom - height + 1);

  g_object_unref(target);
  g_object_unref(target2);
  g_object_unref(target3);
}
#endif  //  defined(ATK_230)

IN_PROC_BROWSER_TEST_F(AccessibilityAuraLinuxBrowserTest, TestSetSelection) {
  AtkText* atk_text = SetUpInputField();

  int start_offset, end_offset;
  gchar* selected_text =
      atk_text_get_selection(atk_text, 0, &start_offset, &end_offset);
  EXPECT_EQ(nullptr, selected_text);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(0, end_offset);

  AccessibilityNotificationWaiter waiter(
      shell()->web_contents(), ui::kAXModeComplete,
      ax::mojom::Event::kTextSelectionChanged);
  int contents_string_length = int{InputContentsString().size()};
  start_offset = 0;
  end_offset = contents_string_length;

  EXPECT_TRUE(atk_text_set_selection(atk_text, 0, start_offset, end_offset));
  waiter.WaitForNotification();
  selected_text =
      atk_text_get_selection(atk_text, 0, &start_offset, &end_offset);
  EXPECT_NE(nullptr, selected_text);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(contents_string_length, end_offset);
  g_free(selected_text);

  start_offset = contents_string_length;
  end_offset = 1;
  EXPECT_TRUE(atk_text_set_selection(atk_text, 0, start_offset, end_offset));
  waiter.WaitForNotification();
  selected_text =
      atk_text_get_selection(atk_text, 0, &start_offset, &end_offset);
  EXPECT_NE(nullptr, selected_text);
  EXPECT_EQ(1, start_offset);
  EXPECT_EQ(contents_string_length, end_offset);
  g_free(selected_text);

  g_object_unref(atk_text);
}

IN_PROC_BROWSER_TEST_F(AccessibilityAuraLinuxBrowserTest, TestAtkTextListItem) {
  LoadInitialAccessibilityTreeFromHtml(
      R"HTML(<!DOCTYPE html>
      <html>
      <body>
        <li>Text 1</li>
        <li>Text 2</li>
        <li>Text 3</li>
      </body>
      </html>)HTML");

  // Retrieve the AtkObject interface for the document node.
  AtkObject* document = GetRendererAccessible();
  EXPECT_EQ(3, atk_object_get_n_accessible_children(document));
  AtkObject* list_item_1 = atk_object_ref_accessible_child(document, 0);
  AtkObject* list_item_2 = atk_object_ref_accessible_child(document, 1);

  EXPECT_TRUE(ATK_IS_TEXT(list_item_1));

  const base::string16 string16_embed(
      1, ui::AXPlatformNodeAuraLinux::kEmbeddedCharacter);
  std::string expected_string = base::UTF16ToUTF8(string16_embed) + "Text 1";

  // The text of the list item should include the list marker as an embedded
  // object.
  gchar* text = atk_text_get_text(ATK_TEXT(list_item_1), 0, -1);
  ASSERT_STREQ(text, expected_string.c_str());
  g_free(text);
  text = atk_text_get_text_at_offset(
      ATK_TEXT(list_item_2), 0, ATK_TEXT_BOUNDARY_WORD_START, nullptr, nullptr);
  ASSERT_STREQ(text, base::UTF16ToUTF8(string16_embed).c_str());
  g_free(text);

  text = atk_text_get_text_at_offset(
      ATK_TEXT(list_item_2), 1, ATK_TEXT_BOUNDARY_WORD_START, nullptr, nullptr);
  ASSERT_STREQ(text, "Text ");
  g_free(text);

  g_object_unref(list_item_1);
  g_object_unref(list_item_2);
}

IN_PROC_BROWSER_TEST_F(AccessibilityAuraLinuxBrowserTest,
                       TestTextSelectionChangedDuplicateSignals) {
  LoadInitialAccessibilityTreeFromHtml(
      R"HTML(<!DOCTYPE html>
      <html>
      <body>
      <div>
        Sufficiently long div content
      </div>
      </body>
      </html>)HTML");

  // Retrieve the AtkObject interface for the document node.
  AtkObject* document = GetRendererAccessible();
  ASSERT_TRUE(ATK_IS_COMPONENT(document));

  AtkObject* div = atk_object_ref_accessible_child(document, 0);
  EXPECT_NE(div, nullptr);

  int selection_changed_signals = 0;
  g_signal_connect(div, "text-selection-changed",
                   G_CALLBACK(+[](AtkText*, int* count) { *count += 1; }),
                   &selection_changed_signals);

  int caret_moved_signals = 0;
  g_signal_connect(div, "text-caret-moved",
                   G_CALLBACK(+[](AtkText*, gint, int* count) { *count += 1; }),
                   &caret_moved_signals);

  auto waiter = std::make_unique<AccessibilityNotificationWaiter>(
      shell()->web_contents(), ui::kAXModeComplete,
      ax::mojom::Event::kTextSelectionChanged);
  atk_text_set_caret_offset(ATK_TEXT(div), 0);
  waiter->WaitForNotification();
  ASSERT_EQ(selection_changed_signals, 0);
  ASSERT_EQ(caret_moved_signals, 1);

  caret_moved_signals = selection_changed_signals = 0;
  atk_text_set_selection(ATK_TEXT(div), 0, 0, 3);
  waiter->WaitForNotification();
  ASSERT_EQ(selection_changed_signals, 1);
  ASSERT_EQ(caret_moved_signals, 1);

  caret_moved_signals = selection_changed_signals = 0;
  atk_text_set_caret_offset(ATK_TEXT(div), 3);
  waiter->WaitForNotification();
  ASSERT_EQ(selection_changed_signals, 1);
  ASSERT_EQ(caret_moved_signals, 0);

  caret_moved_signals = selection_changed_signals = 0;
  atk_text_set_caret_offset(ATK_TEXT(div), 2);
  waiter->WaitForNotification();
  ASSERT_EQ(selection_changed_signals, 0);
  ASSERT_EQ(caret_moved_signals, 1);

  g_object_unref(div);
}

IN_PROC_BROWSER_TEST_F(AccessibilityAuraLinuxBrowserTest,
                       TestTextSelectionAcrossElements) {
  LoadInitialAccessibilityTreeFromHtml(std::string(
      R"HTML(<!DOCTYPE html>
          <html>
          <body>
            <div id="parent" contenteditable="true">
              <div id="child1">Child 1</div>
              <div id="child2">Child 2</div>
            </div>
          </body>
          </html>)HTML"));

  AtkObject* document = GetRendererAccessible();
  EXPECT_EQ(1, atk_object_get_n_accessible_children(document));

  AtkText* parent = ATK_TEXT(atk_object_ref_accessible_child(document, 0));
  EXPECT_EQ(2, atk_object_get_n_accessible_children(ATK_OBJECT(parent)));
  AtkText* child1 =
      ATK_TEXT(atk_object_ref_accessible_child(ATK_OBJECT(parent), 0));
  AtkText* child2 =
      ATK_TEXT(atk_object_ref_accessible_child(ATK_OBJECT(parent), 1));
  EXPECT_NE(nullptr, child1);
  EXPECT_NE(nullptr, child2);

  auto callback = G_CALLBACK(+[](AtkText*, bool* flag) { *flag = true; });
  bool saw_selection_change_in_parent = false;
  g_signal_connect(parent, "text-selection-changed", callback,
                   &saw_selection_change_in_parent);
  bool saw_selection_change_in_child1 = false;
  g_signal_connect(child1, "text-selection-changed", callback,
                   &saw_selection_change_in_child1);
  bool saw_selection_change_in_child2 = false;
  g_signal_connect(child2, "text-selection-changed", callback,
                   &saw_selection_change_in_child2);

  AccessibilityNotificationWaiter selection_waiter(
      shell()->web_contents(), ui::kAXModeComplete,
      ax::mojom::Event::kTextSelectionChanged);
  ExecuteScript(
      base::UTF8ToUTF16("let parent = document.getElementById('parent');"
                        "let child1 = document.getElementById('child1');"
                        "let child2 = document.getElementById('child2');"
                        "let range = document.createRange();"
                        "range.setStart(child1.firstChild, 3);"
                        "range.setEnd(child1.firstChild, 5);"
                        "parent.focus();"
                        "document.getSelection().removeAllRanges();"
                        "document.getSelection().addRange(range);"));
  selection_waiter.WaitForNotification();

  EXPECT_FALSE(saw_selection_change_in_parent);
  EXPECT_TRUE(saw_selection_change_in_child1);
  EXPECT_FALSE(saw_selection_change_in_child2);

  saw_selection_change_in_parent = false;
  saw_selection_change_in_child1 = false;
  saw_selection_change_in_child2 = false;

  EXPECT_TRUE(atk_text_remove_selection(parent, 0));
  selection_waiter.WaitForNotification();

  EXPECT_FALSE(saw_selection_change_in_parent);
  EXPECT_TRUE(saw_selection_change_in_child1);
  EXPECT_FALSE(saw_selection_change_in_child2);

  saw_selection_change_in_parent = false;
  saw_selection_change_in_child1 = false;
  saw_selection_change_in_child2 = false;

  ExecuteScript(
      base::UTF8ToUTF16("let range2 = document.createRange();"
                        "range2.setStart(child1.firstChild, 0);"
                        "range2.setEnd(child2.firstChild, 3);"
                        "parent.focus();"
                        "document.getSelection().removeAllRanges();"
                        "document.getSelection().addRange(range2);"));
  selection_waiter.WaitForNotification();

  EXPECT_TRUE(saw_selection_change_in_parent);
  EXPECT_FALSE(saw_selection_change_in_child1);
  EXPECT_FALSE(saw_selection_change_in_child2);
}

// TODO(crbug.com/981913): This flakes on linux.
IN_PROC_BROWSER_TEST_F(
    AccessibilityAuraLinuxBrowserTest,
    DISABLED_TestSetCaretSetsSequentialFocusNavigationStartingPoint) {
  LoadInitialAccessibilityTreeFromHtml(
      R"HTML(<!DOCTYPE html>
      <html>
      <body>
      <div>
        0
        <a href="http://google.com">1</a>
        2
        <a href="http://google.com">3</a>
        4
        <a href="http://google.com">5</a>
        6
        <a href="http://google.com"><div>7</div></a>
        8
      </div>
      </body>
      </html>)HTML");

  // Retrieve the AtkObject interface for the document node.
  AtkObject* document = GetRendererAccessible();
  ASSERT_TRUE(ATK_IS_COMPONENT(document));

  AtkObject* child_2 = atk_object_ref_accessible_child(document, 2);
  AtkObject* child_3 = atk_object_ref_accessible_child(document, 3);
  AtkObject* child_7 = atk_object_ref_accessible_child(document, 7);
  EXPECT_NE(child_2, nullptr);
  EXPECT_NE(child_3, nullptr);
  EXPECT_NE(child_7, nullptr);

  // Move the caret to the "3" div. This should also set the sequential
  // focus navigation starting point.
  atk_text_set_caret_offset(ATK_TEXT(child_2), 0);

  auto waiter = std::make_unique<AccessibilityNotificationWaiter>(
      shell()->web_contents(), ui::kAXModeComplete, ax::mojom::Event::kFocus);

  // Now send two tab presses to advance the focus.
  // TODO(mrobinson): For some reason, in the test harness two tabs are
  // necessary to advance focus after setting the selection (caret). This isn't
  // necessary when running interactively.
  SimulateKeyPress(shell()->web_contents(), ui::DomKey::TAB, ui::DomCode::TAB,
                   ui::VKEY_TAB, false, false, false, false);
  waiter->WaitForNotification();

  SimulateKeyPress(shell()->web_contents(), ui::DomKey::TAB, ui::DomCode::TAB,
                   ui::VKEY_TAB, false, false, false, false);
  waiter->WaitForNotification();
  ASSERT_TRUE(IsAtkObjectFocused(child_3));

  // Now we repeat a similar test, but this time setting the caret offset on
  // the document. In this case, the sequential navigation starting point
  // should move to the appropriate child.
  atk_text_set_caret_offset(ATK_TEXT(document), 13);
  SimulateKeyPress(shell()->web_contents(), ui::DomKey::TAB, ui::DomCode::TAB,
                   ui::VKEY_TAB, false, false, false, false);
  waiter->WaitForNotification();

  SimulateKeyPress(shell()->web_contents(), ui::DomKey::TAB, ui::DomCode::TAB,
                   ui::VKEY_TAB, false, false, false, false);
  waiter->WaitForNotification();

  ASSERT_TRUE(IsAtkObjectFocused(child_7));

  // Now test setting the caret in a node that can accept focus. That
  // node should actually receive focus.
  atk_text_set_caret_offset(ATK_TEXT(child_3), 0);
  SimulateKeyPress(shell()->web_contents(), ui::DomKey::TAB, ui::DomCode::TAB,
                   ui::VKEY_TAB, false, false, false, false);
  waiter->WaitForNotification();
  ASSERT_TRUE(IsAtkObjectFocused(child_3));

  AtkObject* link_section = atk_object_ref_accessible_child(child_7, 0);
  EXPECT_NE(link_section, nullptr);
  AtkObject* link_text = atk_object_ref_accessible_child(link_section, 0);
  EXPECT_NE(link_text, nullptr);
  atk_text_set_caret_offset(ATK_TEXT(link_text), 0);
  waiter->WaitForNotification();
  ASSERT_TRUE(IsAtkObjectFocused(child_7));

  g_object_unref(link_section);
  g_object_unref(link_text);
  g_object_unref(child_2);
  g_object_unref(child_3);
  g_object_unref(child_7);
}

IN_PROC_BROWSER_TEST_F(AccessibilityAuraLinuxBrowserTest,
                       TestFocusInputTextFields) {
  auto verify_selection = [](AtkObject* object, const char* selection) {
    gchar* selected_text =
        atk_text_get_selection(ATK_TEXT(object), 0, nullptr, nullptr);
    EXPECT_STREQ(selected_text, selection);
    g_free(selected_text);

    int n_selections = atk_text_get_n_selections(ATK_TEXT(object));
    EXPECT_EQ(n_selections, selection ? 1 : 0);
  };

  LoadInitialAccessibilityTreeFromHtml(
      R"HTML(<!DOCTYPE html>
      <html>
      <body>
      <div>
        <input value="First Field">
        <input value="Second Field">
        <input value="Third Field">
        <input value="Fourth Field">
      </div>
      </body>
      </html>)HTML");

  // Retrieve the AtkObject interface for the document node.
  AtkObject* document = GetRendererAccessible();
  ASSERT_TRUE(ATK_IS_COMPONENT(document));

  AtkObject* container = atk_object_ref_accessible_child(document, 0);

  AtkObject* field_1 = atk_object_ref_accessible_child(container, 0);
  AtkObject* field_2 = atk_object_ref_accessible_child(container, 1);
  AtkObject* field_3 = atk_object_ref_accessible_child(container, 2);
  AtkObject* field_4 = atk_object_ref_accessible_child(container, 3);
  EXPECT_NE(field_1, nullptr);
  EXPECT_NE(field_2, nullptr);
  EXPECT_NE(field_3, nullptr);
  EXPECT_NE(field_4, nullptr);

  auto waiter = std::make_unique<AccessibilityNotificationWaiter>(
      shell()->web_contents(), ui::kAXModeComplete, ax::mojom::Event::kFocus);
  atk_component_grab_focus(ATK_COMPONENT(field_1));
  waiter->WaitForNotification();

  waiter = std::make_unique<AccessibilityNotificationWaiter>(
      shell()->web_contents(), ui::kAXModeComplete,
      ax::mojom::Event::kTextSelectionChanged);
  EXPECT_TRUE(atk_text_set_selection(ATK_TEXT(field_1), 0, 0, 5));
  waiter->WaitForNotification();

  EXPECT_TRUE(atk_text_set_selection(ATK_TEXT(field_2), 0, 0, -1));
  waiter->WaitForNotification();

  // Only the field that is currently focused should return a selection.
  ASSERT_FALSE(IsAtkObjectFocused(field_1));
  ASSERT_TRUE(IsAtkObjectFocused(field_2));
  verify_selection(field_1, nullptr);
  verify_selection(field_2, "Second Field");
  verify_selection(field_3, nullptr);
  verify_selection(field_4, nullptr);

  waiter = std::make_unique<AccessibilityNotificationWaiter>(
      shell()->web_contents(), ui::kAXModeComplete, ax::mojom::Event::kFocus);
  atk_component_grab_focus(ATK_COMPONENT(field_1));
  waiter->WaitForNotification();

  // Now that the focus has returned to the first field, it should return the
  // original selection that we set on it.
  ASSERT_TRUE(IsAtkObjectFocused(field_1));
  ASSERT_FALSE(IsAtkObjectFocused(field_2));
  verify_selection(field_1, "First");
  verify_selection(field_2, nullptr);
  verify_selection(field_3, nullptr);
  verify_selection(field_4, nullptr);

  g_object_unref(field_1);
  g_object_unref(field_2);
  g_object_unref(field_3);
  g_object_unref(field_4);
  g_object_unref(container);
}

IN_PROC_BROWSER_TEST_F(AccessibilityAuraLinuxBrowserTest,
                       TestTextEventsInStaticText) {
  LoadInitialAccessibilityTreeFromHtml(std::string(
      R"HTML(<!DOCTYPE html>
          <html>
          <body>
            <div contenteditable="true">Text inside field</div>
            anonymous block
          </body>
          </html>)HTML"));

  AtkObject* document = GetRendererAccessible();
  EXPECT_EQ(2, atk_object_get_n_accessible_children(document));

  AtkText* div_element = ATK_TEXT(atk_object_ref_accessible_child(document, 0));
  EXPECT_EQ(1, atk_object_get_n_accessible_children(ATK_OBJECT(div_element)));
  AtkText* text =
      ATK_TEXT(atk_object_ref_accessible_child(ATK_OBJECT(div_element), 0));
  AtkText* anonymous_block =
      ATK_TEXT(atk_object_ref_accessible_child(document, 1));

  auto callback = G_CALLBACK(+[](AtkText*, gint, bool* flag) { *flag = true; });

  bool saw_caret_move_in_text = false;
  g_signal_connect(text, "text-caret-moved", callback, &saw_caret_move_in_text);

  bool saw_caret_move_in_div = false;
  g_signal_connect(div_element, "text-caret-moved", callback,
                   &saw_caret_move_in_div);

  bool saw_caret_move_in_anonymous_block = false;
  g_signal_connect(anonymous_block, "text-caret-moved", callback,
                   &saw_caret_move_in_anonymous_block);

  bool saw_caret_move_in_document = false;
  g_signal_connect(document, "text-caret-moved", callback,
                   &saw_caret_move_in_document);

  AccessibilityNotificationWaiter selection_waiter(
      shell()->web_contents(), ui::kAXModeComplete,
      ax::mojom::Event::kTextSelectionChanged);
  ExecuteScript(base::UTF8ToUTF16(
      "let selection = document.getSelection();"
      "let editable = document.querySelector('div[contenteditable=\"true\"]');"
      "editable.focus();"
      "let range = document.createRange();"
      "range.setStart(editable.lastChild, 4);"
      "range.setEnd(editable.lastChild, 4);"
      "selection.removeAllRanges();"
      "selection.addRange(range);"));
  selection_waiter.WaitForNotification();

  // We should see the event happen in div and not the static text element.
  EXPECT_TRUE(saw_caret_move_in_div);
  EXPECT_FALSE(saw_caret_move_in_text);
  EXPECT_FALSE(saw_caret_move_in_anonymous_block);
  EXPECT_FALSE(saw_caret_move_in_document);

  saw_caret_move_in_div = false;

  atk_text_set_caret_offset(anonymous_block, 3);
  selection_waiter.WaitForNotification();

  EXPECT_FALSE(saw_caret_move_in_div);
  EXPECT_FALSE(saw_caret_move_in_text);
  EXPECT_FALSE(saw_caret_move_in_anonymous_block);
  EXPECT_TRUE(saw_caret_move_in_document);

  g_object_unref(div_element);
  g_object_unref(anonymous_block);
  g_object_unref(text);
}

IN_PROC_BROWSER_TEST_F(AccessibilityAuraLinuxBrowserTest,
                       TextAttributesInInputWithAriaHidden) {
  LoadInitialAccessibilityTreeFromHtml(std::string(
      R"HTML(<!DOCTYPE html>
          <html>
          <body>
            <input aria-hidden="true">
          </body>
          </html>)HTML"));

  AtkObject* document = GetRendererAccessible();
  EXPECT_EQ(1, atk_object_get_n_accessible_children(document));

  AtkObject* section = atk_object_ref_accessible_child(document, 0);
  AtkText* input_element =
      ATK_TEXT(atk_object_ref_accessible_child(section, 0));

  AtkAttributeSet* attributes =
      atk_text_get_run_attributes(input_element, 0, nullptr, nullptr);
  ASSERT_NE(attributes, nullptr);
  atk_attribute_set_free(attributes);

  g_object_unref(input_element);
  g_object_unref(section);
}

IN_PROC_BROWSER_TEST_F(AccessibilityAuraLinuxBrowserTest,
                       TestFindInPageEvents) {
  LoadInitialAccessibilityTreeFromHtml(
      R"HTML(<!DOCTYPE html>
      <html>
      <body>
      <div contenteditable="true">
        Sufficiently long div content
      </div>
      <div contenteditable="true">
        Second sufficiently long div content
      </div>
      </body>
      </html>)HTML");

  // Retrieve the AtkObject interface for the document node.
  AtkObject* document = GetRendererAccessible();
  ASSERT_TRUE(ATK_IS_COMPONENT(document));

  AtkObject* div1 = atk_object_ref_accessible_child(document, 0);
  AtkObject* div2 = atk_object_ref_accessible_child(document, 1);
  EXPECT_NE(div1, nullptr);
  EXPECT_NE(div2, nullptr);

  auto selection_callback =
      G_CALLBACK(+[](AtkText*, int* count) { *count += 1; });
  int selection_changed_signals = 0;
  g_signal_connect(div1, "text-selection-changed", selection_callback,
                   &selection_changed_signals);
  g_signal_connect(div2, "text-selection-changed", selection_callback,
                   &selection_changed_signals);

  auto caret_callback = G_CALLBACK(
      +[](AtkText*, int new_position, int* caret_position_from_event) {
        *caret_position_from_event = new_position;
      });
  int caret_position_from_event = -1;
  g_signal_connect(div1, "text-caret-moved", caret_callback,
                   &caret_position_from_event);
  g_signal_connect(div2, "text-caret-moved", caret_callback,
                   &caret_position_from_event);

  AccessibilityNotificationWaiter waiter(
      shell()->web_contents(), ui::kAXModeComplete,
      ax::mojom::Event::kTextSelectionChanged);
  atk_text_set_caret_offset(ATK_TEXT(div1), 4);
  waiter.WaitForNotification();

  ASSERT_EQ(atk_text_get_caret_offset(ATK_TEXT(div1)), 4);
  ASSERT_EQ(caret_position_from_event, 4);
  ASSERT_EQ(selection_changed_signals, 0);

  caret_position_from_event = -1;
  selection_changed_signals = 0;
  auto* node = static_cast<ui::AXPlatformNodeAuraLinux*>(
      ui::AXPlatformNode::FromNativeViewAccessible(div2));
  node->ActivateFindInPageResult(1, 3);

  ASSERT_EQ(selection_changed_signals, 1);
  ASSERT_EQ(caret_position_from_event, 3);
  ASSERT_EQ(atk_text_get_caret_offset(ATK_TEXT(div2)), 3);
  ASSERT_EQ(atk_text_get_caret_offset(ATK_TEXT(div1)), 4);

  caret_position_from_event = -1;
  selection_changed_signals = 0;
  node->TerminateFindInPage();

  ASSERT_EQ(selection_changed_signals, 0);
  ASSERT_EQ(caret_position_from_event, -1);
  ASSERT_EQ(atk_text_get_caret_offset(ATK_TEXT(div2)), -1);
  ASSERT_EQ(atk_text_get_caret_offset(ATK_TEXT(div1)), 4);

  g_object_unref(div1);
  g_object_unref(div2);
}

}  // namespace content
