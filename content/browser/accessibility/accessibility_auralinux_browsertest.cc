// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atk/atk.h>

#include <string>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/version.h"
#include "build/build_config.h"
#include "content/browser/accessibility/accessibility_browsertest.h"
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "ui/accessibility/platform/ax_platform_node_auralinux.h"
#include "ui/accessibility/platform/browser_accessibility.h"

// TODO(crbug.com/40248581): Remove this again.
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

namespace content {

namespace {

const char16_t kBullet[2] = {u'\x2022', ' '};
const std::u16string kString16Bullet = std::u16string(kBullet, 2);

AtkObject* FindAtkObjectParentFrame(AtkObject* atk_object) {
  while (atk_object) {
    if (atk_object_get_role(atk_object) == ATK_ROLE_FRAME) {
      return atk_object;
    }
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

static bool IsAtkObjectEditable(AtkObject* object) {
  AtkStateSet* state_set = atk_object_ref_state_set(object);
  bool result = atk_state_set_contains_state(state_set, ATK_STATE_EDITABLE);
  g_object_unref(state_set);
  return result;
}

}  // namespace

class AccessibilityAuraLinuxBrowserTest : public AccessibilityBrowserTest {
 public:
  AccessibilityAuraLinuxBrowserTest() = default;

  AccessibilityAuraLinuxBrowserTest(const AccessibilityAuraLinuxBrowserTest&) =
      delete;
  AccessibilityAuraLinuxBrowserTest& operator=(
      const AccessibilityAuraLinuxBrowserTest&) = delete;

  ~AccessibilityAuraLinuxBrowserTest() override = default;

 protected:
  static bool HasObjectWithAtkRoleFrameInAncestry(AtkObject* object) {
    while (object) {
      if (atk_object_get_role(object) == ATK_ROLE_FRAME) {
        return true;
      }
      object = atk_object_get_parent(object);
    }
    return false;
  }

  // Ensures that the text and the start and end offsets retrieved using
  // get_textAtOffset match the expected values.
  static void CheckTextAtOffset(AtkText* text_object,
                                int offset,
                                AtkTextBoundary boundary_type,
                                int expected_start_offset,
                                int expected_end_offset,
                                const char* expected_text);

  // Loads a page with  an input text field and places sample text in it.
  // Returns a pointer to the field's AtkText interface.
  AtkText* SetUpInputField();

  // Loads a page with  a textarea text field, places sample text in it, and
  // places the caret after the last character.
  //  Returns a pointer to the field's AtkText interface.
  AtkText* SetUpTextareaField();

  // Does a few checks on the scrollable input field and returns
  // a pointer to the field's AtkText interface.
  AtkText* GetScrollableInputField();

  // Loads a page with a paragraph of sample text and returns its AtkText
  // interface.
  AtkText* SetUpSampleParagraph();

  // Retrieves a pointer to the already loaded paragraph's AtkText interface.
  AtkText* GetSampleParagraph();

  // Searches the accessibility tree in pre-order debth-first traversal for a
  // node with the given role and returns its AtkText interface if found,
  // otherwise returns nullptr.
  AtkText* FindNode(const AtkRole role);

  void TestCharacterExtentsInScrollableInput();

 private:
  // Searches the accessibility tree in pre-order debth-first traversal starting
  // at a given node and for a node with the given role and returns its AtkText
  // interface if found, otherwise returns nullptr.
  AtkText* FindNode(AtkObject* root, const AtkRole role) const;
};

void AccessibilityAuraLinuxBrowserTest::CheckTextAtOffset(
    AtkText* text_object,
    int offset,
    AtkTextBoundary boundary_type,
    int expected_start_offset,
    int expected_end_offset,
    const char* expected_text) {
  ::testing::Message message;
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

AtkText* AccessibilityAuraLinuxBrowserTest::SetUpInputField() {
  LoadInputField();
  return FindNode(ATK_ROLE_ENTRY);
}

AtkText* AccessibilityAuraLinuxBrowserTest::SetUpTextareaField() {
  LoadTextareaField();
  return FindNode(ATK_ROLE_ENTRY);
}

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

  // The paragraph is the last child.
  AtkObject* paragraph = atk_object_ref_accessible_child(document, 0);
  EXPECT_EQ(ATK_ROLE_PARAGRAPH, atk_object_get_role(paragraph));

  EXPECT_TRUE(ATK_IS_TEXT(paragraph));
  return ATK_TEXT(paragraph);
}

AtkText* AccessibilityAuraLinuxBrowserTest::GetScrollableInputField() {
  AtkObject* document = GetRendererAccessible();
  EXPECT_EQ(1, atk_object_get_n_accessible_children(document));

  AtkObject* div = atk_object_ref_accessible_child(document, 0);
  EXPECT_NE(div, nullptr);
  int n_div_children = atk_object_get_n_accessible_children(div);
  EXPECT_GT(n_div_children, 0);

  // The input field is always the last child.
  AtkObject* input = atk_object_ref_accessible_child(div, n_div_children - 1);
  EXPECT_EQ(ATK_ROLE_ENTRY, atk_object_get_role(input));

  // Retrieve the IAccessibleText interface for the field.
  AtkText* atk_text_field = ATK_TEXT(input);

  // Set the caret before the last character.
  AccessibilityNotificationWaiter waiter(
      shell()->web_contents(), ui::kAXModeComplete,
      ui::AXEventGenerator::Event::TEXT_SELECTION_CHANGED);
  long caret_offset = InputContentsString().size() - 1;
  ExecuteScript(base::ASCIIToUTF16(
      base::StrCat({"let textField = document.querySelector('input,textarea');"
                    "textField.focus();"
                    "textField.setSelectionRange(",
                    base::NumberToString(caret_offset), ",",
                    base::NumberToString(caret_offset),
                    ");"
                    "textField.scrollLeft = 1000;"})));
  EXPECT_TRUE(waiter.WaitForNotification());

  g_object_unref(div);
  return atk_text_field;
}

AtkText* AccessibilityAuraLinuxBrowserTest::FindNode(const AtkRole role) {
  AtkObject* document = GetRendererAccessible();
  EXPECT_NE(nullptr, document);
  return FindNode(document, role);
}

AtkText* AccessibilityAuraLinuxBrowserTest::FindNode(AtkObject* root,
                                                     const AtkRole role) const {
  EXPECT_NE(nullptr, root);
  if (atk_object_get_role(root) == role) {
    EXPECT_TRUE(ATK_IS_TEXT(root));
    g_object_ref(root);
    AtkText* root_text = ATK_TEXT(root);
    return root_text;
  }

  for (int i = 0; i < atk_object_get_n_accessible_children(root); ++i) {
    AtkObject* child = atk_object_ref_accessible_child(root, i);
    EXPECT_NE(nullptr, child);
    if (atk_object_get_role(child) == role) {
      EXPECT_TRUE(ATK_IS_TEXT(child));
      AtkText* child_text = ATK_TEXT(child);
      return child_text;
    }

    if (AtkText* descendant_text = FindNode(child, role)) {
      g_object_unref(child);
      return descendant_text;
    }

    g_object_unref(child);
  }

  return nullptr;
}

IN_PROC_BROWSER_TEST_F(AccessibilityAuraLinuxBrowserTest,
                       AuraLinuxBrowserAccessibleParent) {
  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);
  EXPECT_TRUE(NavigateToURL(shell(), GURL("data:text/html,")));
  ASSERT_TRUE(waiter.WaitForNotification());

  // Get the BrowserAccessibilityManager.
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  ui::BrowserAccessibilityManager* manager =
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
                       TestTextAtOffsetWithBoundaryCharacterAndEmbeddedObject) {
  LoadInitialAccessibilityTreeFromHtml(R"HTML(<!DOCTYPE html>
      <div contenteditable>
        Before<img alt="image">after.
      </div>
      )HTML");

  AtkObject* document = GetRendererAccessible();
  ASSERT_EQ(1, atk_object_get_n_accessible_children(document));

  AtkObject* contenteditable = atk_object_ref_accessible_child(document, 0);
  ASSERT_NE(nullptr, contenteditable);
  ASSERT_EQ(ATK_ROLE_SECTION, atk_object_get_role(contenteditable));
  ASSERT_TRUE(ATK_IS_TEXT(contenteditable));

  AtkText* contenteditable_text = ATK_TEXT(contenteditable);
  int character_count = atk_text_get_character_count(contenteditable_text);
  ASSERT_EQ(13, character_count);

  const std::u16string embedded_character(
      1, ui::AXPlatformNodeAuraLinux::kEmbeddedCharacter);
  const std::vector<std::string> expected_hypertext = {
      "B", "e", "f", "o", "r", "e", base::UTF16ToUTF8(embedded_character),
      "a", "f", "t", "e", "r", "."};

  // "Before".
  //
  // The embedded object character representing the image is at offset 6.
  for (int i = 0; i < 6; ++i) {
    CheckTextAtOffset(contenteditable_text, i, ATK_TEXT_BOUNDARY_CHAR, i,
                      (i + 1), expected_hypertext[i].c_str());
  }

  // "after.".
  //
  // Note that according to the ATK Spec, an offset that is equal to
  // "character_count" is not permitted.
  for (int i = 7; i < character_count; ++i) {
    CheckTextAtOffset(contenteditable_text, i, ATK_TEXT_BOUNDARY_CHAR, i,
                      (i + 1), expected_hypertext[i].c_str());
  }

  ASSERT_EQ(3, atk_object_get_n_accessible_children(contenteditable));
  // The image is the second child.
  AtkObject* image = atk_object_ref_accessible_child(contenteditable, 1);
  ASSERT_NE(nullptr, image);
  ASSERT_EQ(ATK_ROLE_IMAGE, atk_object_get_role(image));

  // The alt text of the image is not navigable as text.
  ASSERT_FALSE(ATK_IS_TEXT(image));

  g_object_unref(image);
  g_object_unref(contenteditable_text);
}

IN_PROC_BROWSER_TEST_F(AccessibilityAuraLinuxBrowserTest,
                       TestMultilingualTextAtOffsetWithBoundaryCharacter) {
  AtkText* atk_text = SetUpInputField();
  ASSERT_NE(nullptr, atk_text);
  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kValueChanged);
  // Place an e acute, and two emoticons in the text field.
  ExecuteScript(
      uR"SCRIPT(
      const input = document.querySelector('input');
      input.value =
          'é👩\u200D❤\uFE0F\u200D👩🐶';
      )SCRIPT");
  ASSERT_TRUE(waiter.WaitForNotification());

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
                    static_cast<int>(InputContentsString().size()),
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
                    static_cast<int>(InputContentsString().size()),
                    "\"KHTML, like\".");

  g_object_unref(atk_text);
}

IN_PROC_BROWSER_TEST_F(AccessibilityAuraLinuxBrowserTest,
                       TestBlankLineTextAtOffsetWithBoundaryLine) {
  AtkText* atk_text = SetUpTextareaField();

  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kValueChanged);
  // Add a blank line at the end of the textarea.
  ExecuteScript(
      uR"SCRIPT(
      const textarea = document.querySelector('textarea');
      textarea.value += '\n';
      )SCRIPT");
  ASSERT_TRUE(waiter.WaitForNotification());

  // The second last line should have an additional trailing newline. Also,
  // Blink represents the blank line with a newline character, so in total there
  // should be two more newlines. The second newline is not part of the HTML
  // value attribute however.
  int contents_string_length =
      static_cast<int>(InputContentsString().size()) + 1;
  CheckTextAtOffset(atk_text, 32, ATK_TEXT_BOUNDARY_LINE_START, 32,
                    contents_string_length, "\"KHTML, like\".\n");
  CheckTextAtOffset(atk_text, 46, ATK_TEXT_BOUNDARY_LINE_START, 32,
                    contents_string_length, "\"KHTML, like\".\n");

  // An offset one past the last character should return the last line which is
  // blank. This is represented by Blink with yet another line break.
  CheckTextAtOffset(atk_text, contents_string_length,
                    ATK_TEXT_BOUNDARY_LINE_START, contents_string_length,
                    (contents_string_length + 1), "\n");

  {
    // There should be no text after the blank line.
    int start_offset = 0;
    int end_offset = 0;
    char* text = atk_text_get_text_at_offset(
        atk_text, (contents_string_length + 1), ATK_TEXT_BOUNDARY_LINE_START,
        &start_offset, &end_offset);
    EXPECT_EQ(0, start_offset);
    EXPECT_EQ(0, end_offset);
    EXPECT_EQ(nullptr, text);
  }

  g_object_unref(atk_text);
}

IN_PROC_BROWSER_TEST_F(AccessibilityAuraLinuxBrowserTest,
                       TestParagraphTextAtOffsetWithBoundaryLine) {
  AtkText* atk_text = SetUpSampleParagraph();

  // There should be two lines in this paragraph.
  const int newline_offset = 46;
  int n_characters = atk_text_get_character_count(atk_text);
  ASSERT_LT(newline_offset, n_characters);

  const std::u16string embedded_character(
      1, ui::AXPlatformNodeAuraLinux::kEmbeddedCharacter);
  std::string expected_string = "Game theory is \"the study of " +
                                base::UTF16ToUTF8(embedded_character) +
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
                       DISABLED_TestParagraphTextAtOffsetWithBoundarySentence) {
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

IN_PROC_BROWSER_TEST_F(AccessibilityAuraLinuxBrowserTest,
                       TestCharacterExtentsWithInvalidArguments) {
  AtkText* atk_text = SetUpSampleParagraph();

  int invalid_offset = -3;
  int x = -1, y = -1;
  int width = -1, height = -1;

  // When given invalid arguments, atk_text_get_character_extents returns 0
  // before 2.35.1 and -1 after:
  // https://gitlab.gnome.org/GNOME/atk/-/merge_requests/44#9f621eb5fd3bcb2fa5c7bd228c9b1ad42edc46c8_32_33
  // https://gnome.pages.gitlab.gnome.org/at-spi2-core/atk/AtkText.html#atk-text-get-character-extents
  base::Version atk_version(atk_get_version());
  int expect = atk_version.CompareTo(base::Version("2.35.1")) >= 0 ? -1 : 0;

  atk_text_get_character_extents(atk_text, invalid_offset, &x, &y, &width,
                                 &height, ATK_XY_SCREEN);
  EXPECT_EQ(expect, x);
  EXPECT_EQ(expect, y);
  EXPECT_EQ(expect, width);
  EXPECT_EQ(expect, height);

  atk_text_get_character_extents(atk_text, invalid_offset, &x, &y, &width,
                                 &height, ATK_XY_PARENT);
  EXPECT_EQ(expect, x);
  EXPECT_EQ(expect, y);
  EXPECT_EQ(expect, width);
  EXPECT_EQ(expect, height);

  atk_text_get_character_extents(atk_text, invalid_offset, &x, &y, &width,
                                 &height, ATK_XY_WINDOW);
  EXPECT_EQ(expect, x);
  EXPECT_EQ(expect, y);
  EXPECT_EQ(expect, width);
  EXPECT_EQ(expect, height);

  int n_characters = atk_text_get_character_count(atk_text);
  ASSERT_LT(0, n_characters);

  atk_text_get_character_extents(atk_text, invalid_offset, &x, &y, &width,
                                 &height, ATK_XY_SCREEN);
  EXPECT_EQ(expect, x);
  EXPECT_EQ(expect, y);
  EXPECT_EQ(expect, width);
  EXPECT_EQ(expect, height);

  atk_text_get_character_extents(atk_text, invalid_offset, &x, &y, &width,
                                 &height, ATK_XY_PARENT);
  EXPECT_EQ(expect, x);
  EXPECT_EQ(expect, y);
  EXPECT_EQ(expect, width);
  EXPECT_EQ(expect, height);

  atk_text_get_character_extents(atk_text, invalid_offset, &x, &y, &width,
                                 &height, ATK_XY_WINDOW);
  EXPECT_EQ(expect, x);
  EXPECT_EQ(expect, y);
  EXPECT_EQ(expect, width);
  EXPECT_EQ(expect, height);

  g_object_unref(atk_text);
}

AtkCoordType kCoordinateTypes[] = {
    ATK_XY_SCREEN,
    ATK_XY_WINDOW,
    ATK_XY_PARENT,
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
      ::testing::Message message;
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
      ::testing::Message message;
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
      ::testing::Message message;
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
      ::testing::Message message;
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
      ::testing::Message message;
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
      ::testing::Message message;
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
      ::testing::Message message;
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

void AccessibilityAuraLinuxBrowserTest::
    TestCharacterExtentsInScrollableInput() {
  AtkText* input_text = GetScrollableInputField();
  int contents_string_length = InputContentsString().length();
  int visible_characters_start = 21;
  int n_characters = atk_text_get_character_count(input_text);
  EXPECT_EQ(contents_string_length, n_characters);

  int caret_offset = atk_text_get_caret_offset(input_text);
  EXPECT_EQ(contents_string_length - 1, caret_offset);

  int x, y, width, height;
  int previous_x, previous_y, previous_height;
  for (int coordinate = ATK_XY_SCREEN; coordinate <= ATK_XY_WINDOW;
       ++coordinate) {
    auto coordinate_type = static_cast<AtkCoordType>(coordinate);

    atk_text_get_character_extents(input_text, 0, &x, &y, &width, &height,
                                   coordinate_type);
    EXPECT_GT(0, x + width) << "at offset 0";
    EXPECT_LT(0, y) << "at offset 0";
    EXPECT_LT(1, width) << "at offset 0";
    EXPECT_LT(1, height) << "at offset 0";

    for (int offset = 1; offset < (visible_characters_start - 1); ++offset) {
      previous_x = x;
      previous_y = y;
      previous_height = height;

      atk_text_get_character_extents(input_text, offset, &x, &y, &width,
                                     &height, coordinate_type);
      EXPECT_LT(previous_x, x) << "at offset " << offset;
      EXPECT_EQ(previous_y, y) << "at offset " << offset;
      EXPECT_LT(1, width) << "at offset " << offset;
      EXPECT_EQ(previous_height, height) << "at offset " << offset;
    }

    // Test that non offscreen characters have increasing x coordinates and a
    // width that is greater than 1px.
    atk_text_get_character_extents(input_text, visible_characters_start, &x, &y,
                                   &width, &height, coordinate_type);
    EXPECT_LT(previous_x, x) << "at offset " << visible_characters_start;
    EXPECT_EQ(previous_y, y) << "at offset " << visible_characters_start;
    EXPECT_LT(1, width) << "at offset " << visible_characters_start;
    EXPECT_EQ(previous_height, height)
        << "at offset " << visible_characters_start;

    // Exclude the dot at the end of the text field, because it has a width of
    // one anyway.
    for (int offset = visible_characters_start + 1; offset < (n_characters - 1);
         ++offset) {
      previous_x = x;
      previous_y = y;
      previous_height = height;

      atk_text_get_character_extents(input_text, offset, &x, &y, &width,
                                     &height, coordinate_type);
      EXPECT_LT(previous_x, x) << "at offset " << offset;
      EXPECT_EQ(previous_y, y) << "at offset " << offset;
      EXPECT_LT(1, width) << "at offset " << offset;
      EXPECT_EQ(previous_height, height) << "at offset " << offset;
    }
  }

  g_object_unref(input_text);
}

IN_PROC_BROWSER_TEST_F(AccessibilityAuraLinuxBrowserTest,
                       TestCharacterExtentsInScrollableInputField) {
  LoadScrollableInputField("text");
  TestCharacterExtentsInScrollableInput();
}

IN_PROC_BROWSER_TEST_F(AccessibilityAuraLinuxBrowserTest,
                       TestCharacterExtentsInScrollableInputTypeSearchField) {
  LoadScrollableInputField("search");
  TestCharacterExtentsInScrollableInput();
}

typedef bool (*ScrollToPointFunc)(AtkComponent* component,
                                  AtkCoordType coords,
                                  gint x,
                                  gint y);
typedef bool (*ScrollToFunc)(AtkComponent* component, AtkScrollType type);

// TODO(crbug.com/40866728): Enable this test.
IN_PROC_BROWSER_TEST_F(AccessibilityAuraLinuxBrowserTest,
                       DISABLED_TestScrollToPoint) {
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
  atk_component_scroll_to_point(atk_component, ATK_XY_PARENT, 0, 0);
  ASSERT_TRUE(location_changed_waiter.WaitForNotification());

  atk_component_get_extents(atk_component, &x, &y, nullptr, nullptr,
                            ATK_XY_SCREEN);
  EXPECT_EQ(prev_x, x);
  EXPECT_GT(prev_y, y);

  constexpr int kScrollToY = 0;
  atk_component_scroll_to_point(atk_component, ATK_XY_SCREEN, 0, kScrollToY);
  ASSERT_TRUE(location_changed_waiter.WaitForNotification());
  atk_component_get_extents(atk_component, &x, &y, nullptr, nullptr,
                            ATK_XY_SCREEN);
  EXPECT_EQ(kScrollToY, y);

  constexpr int kScrollToY_2 = 243;
  atk_component_scroll_to_point(atk_component, ATK_XY_SCREEN, 0, kScrollToY_2);
  ASSERT_TRUE(location_changed_waiter.WaitForNotification());
  atk_component_get_extents(atk_component, nullptr, &y, nullptr, nullptr,
                            ATK_XY_SCREEN);
  EXPECT_EQ(kScrollToY_2, y);

  atk_component_scroll_to_point(atk_component, ATK_XY_SCREEN, 0, 129);
  ASSERT_TRUE(location_changed_waiter.WaitForNotification());
  atk_component_get_extents(atk_component, nullptr, &y, nullptr, nullptr,
                            ATK_XY_SCREEN);

  AtkObject* frame = FindAtkObjectParentFrame(ATK_OBJECT(atk_component));
  int frame_y;
  atk_component_get_extents(ATK_COMPONENT(frame), nullptr, &frame_y, nullptr,
                            nullptr, ATK_XY_SCREEN);
  EXPECT_EQ(frame_y, y);

  g_object_unref(atk_text);
}

// TODO(crbug.com/40866728): Enable this test.
IN_PROC_BROWSER_TEST_F(AccessibilityAuraLinuxBrowserTest,
                       DISABLED_TestScrollTo) {
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
  ASSERT_TRUE(
      atk_component_scroll_to(ATK_COMPONENT(target), ATK_SCROLL_TOP_EDGE));
  ASSERT_TRUE(waiter.WaitForNotification());
  int x, y;
  atk_component_get_extents(ATK_COMPONENT(target), &x, &y, nullptr, nullptr,
                            ATK_XY_SCREEN);
  EXPECT_EQ(y, doc_y);
  EXPECT_NE(x, doc_x);

  ASSERT_TRUE(
      atk_component_scroll_to(ATK_COMPONENT(target), ATK_SCROLL_TOP_LEFT));
  ASSERT_TRUE(waiter.WaitForNotification());
  atk_component_get_extents(ATK_COMPONENT(target), &x, &y, nullptr, nullptr,
                            ATK_XY_SCREEN);
  EXPECT_EQ(y, doc_y);
  EXPECT_EQ(x, doc_x);

  ASSERT_TRUE(
      atk_component_scroll_to(ATK_COMPONENT(target), ATK_SCROLL_BOTTOM_EDGE));
  ASSERT_TRUE(waiter.WaitForNotification());
  atk_component_get_extents(ATK_COMPONENT(target), &x, &y, nullptr, nullptr,
                            ATK_XY_SCREEN);
  EXPECT_NE(y, doc_y);
  EXPECT_EQ(x, doc_x);

  ASSERT_TRUE(
      atk_component_scroll_to(ATK_COMPONENT(target), ATK_SCROLL_RIGHT_EDGE));
  ASSERT_TRUE(waiter.WaitForNotification());
  atk_component_get_extents(ATK_COMPONENT(target), &x, &y, nullptr, nullptr,
                            ATK_XY_SCREEN);
  EXPECT_NE(y, doc_y);
  EXPECT_NE(x, doc_x);

  ASSERT_TRUE(
      atk_component_scroll_to(ATK_COMPONENT(target2), ATK_SCROLL_LEFT_EDGE));
  ASSERT_TRUE(waiter.WaitForNotification());
  atk_component_get_extents(ATK_COMPONENT(target2), &x, &y, nullptr, nullptr,
                            ATK_XY_SCREEN);
  EXPECT_NE(y, doc_y);
  EXPECT_EQ(x, doc_x);

  ASSERT_TRUE(
      atk_component_scroll_to(ATK_COMPONENT(target2), ATK_SCROLL_TOP_LEFT));
  ASSERT_TRUE(waiter.WaitForNotification());
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
  ASSERT_TRUE(waiter3.WaitForNotification());

  // The text area should be scrolled to somewhere near the bottom of the
  // screen. We check that it is in the bottom quarter of the screen here,
  // but still fully onscreen.
  int height;
  atk_component_get_extents(ATK_COMPONENT(target3), nullptr, &y, nullptr,
                            &height, ATK_XY_SCREEN);

  int doc_bottom = doc_y + doc_height;
  int bottom_third = doc_bottom - (static_cast<float>(doc_height) / 3.0);
  EXPECT_GT(y, bottom_third);
  EXPECT_LT(y, doc_bottom - height + 1);

  g_object_unref(target);
  g_object_unref(target2);
  g_object_unref(target3);
}

// TODO(crbug.com/40866728): Enable this test.
IN_PROC_BROWSER_TEST_F(AccessibilityAuraLinuxBrowserTest,
                       DISABLED_TestScrollSubstringTo) {
  LoadInitialAccessibilityTreeFromHtml(
      R"HTML(<!DOCTYPE html>
      <html>
      <body>
        <div style="height: 50000px; width: 50000px;"></div>
        <div style="position: relative; left: 1000px;">Target 1</div>
        <div style="height: 50000px;"></div>
        <div style="position: relative; left: 10000px;"> Target 2</div>
        <div style="height: 50000px;"></div>
        <div style="position: relative; left: 100000px;"> Target 3</div>
        <div style="height: 50000px;"></div>
      </body>
      </html>)HTML");

  // Retrieve the AtkObject interface for the document node.
  AtkObject* document = GetRendererAccessible();
  ASSERT_TRUE(ATK_IS_COMPONENT(document));

  // Get the dimensions of the document.
  int doc_x, doc_y, doc_width, doc_height;
  atk_component_get_extents(ATK_COMPONENT(document), &doc_x, &doc_y, &doc_width,
                            &doc_height, ATK_XY_SCREEN);

  // The document should only have three children, three span elements.
  ASSERT_EQ(3, atk_object_get_n_accessible_children(document));

  AtkObject* target1 = atk_object_ref_accessible_child(document, 0);
  ASSERT_TRUE(ATK_IS_TEXT(target1));
  ASSERT_EQ(ATK_ROLE_SECTION, atk_object_get_role(target1));

  AccessibilityNotificationWaiter waiter(
      shell()->web_contents(), ui::kAXModeComplete,
      ax::mojom::Event::kScrollPositionChanged);
  ASSERT_TRUE(atk_text_scroll_substring_to(ATK_TEXT(target1), 1, 2,
                                           ATK_SCROLL_TOP_EDGE));
  ASSERT_TRUE(waiter.WaitForNotification());
  int x, y;
  atk_text_get_character_extents(ATK_TEXT(target1), 1, &x, &y, nullptr, nullptr,
                                 ATK_XY_SCREEN);
  EXPECT_EQ(y, doc_y);
  EXPECT_NE(x, doc_x);

  ASSERT_TRUE(atk_text_scroll_substring_to(ATK_TEXT(target1), 1, 2,
                                           ATK_SCROLL_TOP_LEFT));
  ASSERT_TRUE(waiter.WaitForNotification());
  atk_text_get_character_extents(ATK_TEXT(target1), 1, &x, &y, nullptr, nullptr,
                                 ATK_XY_SCREEN);
  EXPECT_EQ(y, doc_y);
  EXPECT_EQ(x, doc_x);

  ASSERT_TRUE(atk_text_scroll_substring_to(ATK_TEXT(target1), 1, 2,
                                           ATK_SCROLL_BOTTOM_EDGE));
  ASSERT_TRUE(waiter.WaitForNotification());
  atk_text_get_character_extents(ATK_TEXT(target1), 1, &x, &y, nullptr, nullptr,
                                 ATK_XY_SCREEN);
  EXPECT_NE(y, doc_y);
  EXPECT_EQ(x, doc_x);

  ASSERT_TRUE(atk_text_scroll_substring_to(ATK_TEXT(target1), 1, 2,
                                           ATK_SCROLL_RIGHT_EDGE));
  ASSERT_TRUE(waiter.WaitForNotification());
  atk_text_get_character_extents(ATK_TEXT(target1), 1, &x, &y, nullptr, nullptr,
                                 ATK_XY_SCREEN);
  EXPECT_NE(y, doc_y);
  EXPECT_NE(x, doc_x);

  ASSERT_TRUE(atk_text_scroll_substring_to(ATK_TEXT(target1), 1, 2,
                                           ATK_SCROLL_LEFT_EDGE));
  ASSERT_TRUE(waiter.WaitForNotification());
  atk_text_get_character_extents(ATK_TEXT(target1), 1, &x, &y, nullptr, nullptr,
                                 ATK_XY_SCREEN);
  EXPECT_NE(y, doc_y);
  EXPECT_EQ(x, doc_x);

  ASSERT_TRUE(atk_text_scroll_substring_to(ATK_TEXT(target1), 1, 2,
                                           ATK_SCROLL_TOP_LEFT));
  ASSERT_TRUE(waiter.WaitForNotification());
  atk_text_get_character_extents(ATK_TEXT(target1), 1, &x, &y, nullptr, nullptr,
                                 ATK_XY_SCREEN);
  EXPECT_EQ(y, doc_y);
  EXPECT_EQ(x, doc_x);

  g_object_unref(target1);
}

// TODO(crbug.com/40866728): Enable this test.
IN_PROC_BROWSER_TEST_F(AccessibilityAuraLinuxBrowserTest,
                       DISABLED_TestScrollSubstringToPoint) {
  LoadSampleParagraphInScrollableDocument();
  AtkText* atk_text = GetSampleParagraph();
  ASSERT_TRUE(ATK_IS_COMPONENT(atk_text));
  AtkComponent* atk_component = ATK_COMPONENT(atk_text);

  int prev_x, prev_y, x, y;
  atk_text_get_character_extents(atk_text, 1, &prev_x, &prev_y, nullptr,
                                 nullptr, ATK_XY_SCREEN);

  AccessibilityNotificationWaiter location_changed_waiter(
      shell()->web_contents(), ui::kAXModeComplete,
      ax::mojom::Event::kLocationChanged);
  atk_text_scroll_substring_to_point(atk_text, 1, 2, ATK_XY_PARENT, 0, 0);
  ASSERT_TRUE(location_changed_waiter.WaitForNotification());

  atk_text_get_character_extents(atk_text, 1, &x, &y, nullptr, nullptr,
                                 ATK_XY_SCREEN);
  EXPECT_EQ(prev_x, x);
  EXPECT_GT(prev_y, y);

  constexpr int kScrollToY = 0;
  atk_text_scroll_substring_to_point(atk_text, 1, 2, ATK_XY_SCREEN, 0,
                                     kScrollToY);
  ASSERT_TRUE(location_changed_waiter.WaitForNotification());
  atk_text_get_character_extents(atk_text, 1, &x, &y, nullptr, nullptr,
                                 ATK_XY_SCREEN);
  EXPECT_EQ(kScrollToY, y);

  constexpr int kScrollToY_2 = 243;
  atk_text_scroll_substring_to_point(atk_text, 1, 2, ATK_XY_SCREEN, 0,
                                     kScrollToY_2);
  ASSERT_TRUE(location_changed_waiter.WaitForNotification());
  atk_text_get_character_extents(atk_text, 1, &x, &y, nullptr, nullptr,
                                 ATK_XY_SCREEN);
  EXPECT_EQ(kScrollToY_2, y);

  atk_text_scroll_substring_to_point(atk_text, 1, 2, ATK_XY_SCREEN, 0, 129);
  ASSERT_TRUE(location_changed_waiter.WaitForNotification());
  atk_text_get_character_extents(atk_text, 1, &x, &y, nullptr, nullptr,
                                 ATK_XY_SCREEN);

  AtkObject* frame = FindAtkObjectParentFrame(ATK_OBJECT(atk_component));
  int frame_y;
  atk_component_get_extents(ATK_COMPONENT(frame), nullptr, &frame_y, nullptr,
                            nullptr, ATK_XY_SCREEN);

  // We do a check that the vertical position is within 5 pixels of the frame
  // position.
  EXPECT_LT(std::abs(frame_y - y), 5);

  g_object_unref(atk_text);
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// Flaky on crbug.com/1026149
#define MAYBE_TestSetSelection DISABLED_TestSetSelection
#else
#define MAYBE_TestSetSelection TestSetSelection
#endif
IN_PROC_BROWSER_TEST_F(AccessibilityAuraLinuxBrowserTest,
                       MAYBE_TestSetSelection) {
  AtkText* atk_text = SetUpInputField();

  int start_offset, end_offset;
  gchar* selected_text =
      atk_text_get_selection(atk_text, 0, &start_offset, &end_offset);
  EXPECT_EQ(nullptr, selected_text);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(0, end_offset);

  AccessibilityNotificationWaiter waiter(
      shell()->web_contents(), ui::kAXModeComplete,
      ui::AXEventGenerator::Event::TEXT_SELECTION_CHANGED);
  int contents_string_length = static_cast<int>(InputContentsString().size());
  start_offset = 0;
  end_offset = contents_string_length;

  EXPECT_TRUE(atk_text_set_selection(atk_text, 0, start_offset, end_offset));
  ASSERT_TRUE(waiter.WaitForNotification());
  selected_text =
      atk_text_get_selection(atk_text, 0, &start_offset, &end_offset);
  EXPECT_NE(nullptr, selected_text);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(contents_string_length, end_offset);
  g_free(selected_text);

  start_offset = contents_string_length;
  end_offset = 1;
  EXPECT_TRUE(atk_text_set_selection(atk_text, 0, start_offset, end_offset));
  ASSERT_TRUE(waiter.WaitForNotification());
  selected_text =
      atk_text_get_selection(atk_text, 0, &start_offset, &end_offset);
  EXPECT_NE(nullptr, selected_text);
  EXPECT_EQ(1, start_offset);
  EXPECT_EQ(contents_string_length, end_offset);
  g_free(selected_text);

  g_object_unref(atk_text);
}

IN_PROC_BROWSER_TEST_F(AccessibilityAuraLinuxBrowserTest,
                       SetSelectionWithIgnoredObjects) {
  LoadInitialAccessibilityTreeFromHtml(R"HTML(<!DOCTYPE html>
      <html>
        <body>
          <ul>
            <li>
              <div role="presentation"></div>
              <p role="presentation">
                <span>Banana</span>
              </p>
              <span>fruit.</span>
            </li>
          </ul>
        </body>
      </html>)HTML");

  AtkText* atk_list_item = FindNode(ATK_ROLE_LIST_ITEM);
  ASSERT_NE(nullptr, atk_list_item);

  // The hypertext expose by "list_item_text" includes a bullet (U+2022)
  // followed by a space for the list bullet and the joined word "Bananafruit.".
  // The word "Banana" is exposed as text because its container paragraph is
  // ignored.
  int n_characters = atk_text_get_character_count(atk_list_item);
  ASSERT_EQ(14, n_characters);

  AccessibilityNotificationWaiter waiter(
      shell()->web_contents(), ui::kAXModeComplete,
      ax::mojom::Event::kDocumentSelectionChanged);

  // First select the whole of the text found in the hypertext.
  int start_offset = 0;
  int end_offset = n_characters;
  std::string bullet = base::UTF16ToUTF8(kString16Bullet);
  char* selected_text = nullptr;

  EXPECT_TRUE(
      atk_text_set_selection(atk_list_item, 0, start_offset, end_offset));
  ASSERT_TRUE(waiter.WaitForNotification());

  selected_text =
      atk_text_get_selection(atk_list_item, 0, &start_offset, &end_offset);
  ASSERT_NE(nullptr, selected_text);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(n_characters, end_offset);
  // The list bullet should be represented by a bullet character (U+2022)
  // followed by a space.
  EXPECT_STREQ((bullet + std::string("Bananafruit.")).c_str(), selected_text);
  g_free(selected_text);

  // Select only the list bullet.
  start_offset = 0;
  end_offset = 2;
  EXPECT_TRUE(
      atk_text_set_selection(atk_list_item, 0, start_offset, end_offset));
  ASSERT_TRUE(waiter.WaitForNotification());

  selected_text =
      atk_text_get_selection(atk_list_item, 0, &start_offset, &end_offset);
  ASSERT_NE(nullptr, selected_text);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(2, end_offset);
  // The list bullet should be represented by a bullet character (U+2022)
  // followed by a space.
  EXPECT_STREQ(bullet.c_str(), selected_text);
  g_free(selected_text);

  // Select the word "Banana" in the ignored paragraph.
  start_offset = 2;
  end_offset = 8;
  EXPECT_TRUE(
      atk_text_set_selection(atk_list_item, 0, start_offset, end_offset));
  ASSERT_TRUE(waiter.WaitForNotification());

  selected_text =
      atk_text_get_selection(atk_list_item, 0, &start_offset, &end_offset);
  ASSERT_NE(nullptr, selected_text);
  EXPECT_EQ(2, start_offset);
  EXPECT_EQ(8, end_offset);
  EXPECT_STREQ("Banana", selected_text);
  g_free(selected_text);

  // Select both the list bullet and the word "Banana" in the ignored paragraph.
  start_offset = 0;
  end_offset = 8;
  EXPECT_TRUE(
      atk_text_set_selection(atk_list_item, 0, start_offset, end_offset));
  ASSERT_TRUE(waiter.WaitForNotification());

  selected_text =
      atk_text_get_selection(atk_list_item, 0, &start_offset, &end_offset);
  ASSERT_NE(nullptr, selected_text);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(8, end_offset);
  // The list bullet should be represented by a bullet character (U+2022)
  // followed by a space.
  EXPECT_STREQ((bullet + std::string("Banana")).c_str(), selected_text);
  g_free(selected_text);

  // Select the joined word "Bananafruit." both in the ignored paragraph and in
  // the unignored span.
  start_offset = 2;
  end_offset = n_characters;
  EXPECT_TRUE(
      atk_text_set_selection(atk_list_item, 0, start_offset, end_offset));
  ASSERT_TRUE(waiter.WaitForNotification());

  selected_text =
      atk_text_get_selection(atk_list_item, 0, &start_offset, &end_offset);
  ASSERT_NE(nullptr, selected_text);
  EXPECT_EQ(2, start_offset);
  EXPECT_EQ(n_characters, end_offset);
  EXPECT_STREQ("Bananafruit.", selected_text);
  g_free(selected_text);

  g_object_unref(atk_list_item);
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

  std::string expected_string = base::UTF16ToUTF8(kString16Bullet) + "Text 1";

  // The text of the list item should include the list marker as a bullet char.
  gchar* text = atk_text_get_text(ATK_TEXT(list_item_1), 0, -1);
  EXPECT_STREQ(text, expected_string.c_str());
  g_free(text);

  text = atk_text_get_text_at_offset(
      ATK_TEXT(list_item_2), 0, ATK_TEXT_BOUNDARY_WORD_START, nullptr, nullptr);
  ASSERT_STREQ(text, base::UTF16ToUTF8(kString16Bullet).c_str());
  g_free(text);

  text = atk_text_get_text_at_offset(
      ATK_TEXT(list_item_2), 2, ATK_TEXT_BOUNDARY_WORD_START, nullptr, nullptr);
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
      ax::mojom::Event::kDocumentSelectionChanged);
  atk_text_set_caret_offset(ATK_TEXT(div), 0);
  ASSERT_TRUE(waiter->WaitForNotification());
  ASSERT_EQ(selection_changed_signals, 0);
  ASSERT_EQ(caret_moved_signals, 1);

  caret_moved_signals = selection_changed_signals = 0;
  atk_text_set_selection(ATK_TEXT(div), 0, 0, 3);
  ASSERT_TRUE(waiter->WaitForNotification());
  ASSERT_EQ(selection_changed_signals, 1);
  ASSERT_EQ(caret_moved_signals, 1);

  caret_moved_signals = selection_changed_signals = 0;
  atk_text_set_caret_offset(ATK_TEXT(div), 3);
  ASSERT_TRUE(waiter->WaitForNotification());
  ASSERT_EQ(selection_changed_signals, 1);
  ASSERT_EQ(caret_moved_signals, 0);

  caret_moved_signals = selection_changed_signals = 0;
  atk_text_set_caret_offset(ATK_TEXT(div), 2);
  ASSERT_TRUE(waiter->WaitForNotification());
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
      ax::mojom::Event::kDocumentSelectionChanged);
  ExecuteScript(
      u"let parent = document.getElementById('parent');"
      u"let child1 = document.getElementById('child1');"
      u"let child2 = document.getElementById('child2');"
      u"let range = document.createRange();"
      u"range.setStart(child1.firstChild, 3);"
      u"range.setEnd(child1.firstChild, 5);"
      u"parent.focus();"
      u"document.getSelection().removeAllRanges();"
      u"document.getSelection().addRange(range);");
  ASSERT_TRUE(selection_waiter.WaitForNotification());

  EXPECT_FALSE(saw_selection_change_in_parent);
  EXPECT_TRUE(saw_selection_change_in_child1);
  EXPECT_FALSE(saw_selection_change_in_child2);

  saw_selection_change_in_parent = false;
  saw_selection_change_in_child1 = false;
  saw_selection_change_in_child2 = false;

  EXPECT_TRUE(atk_text_remove_selection(parent, 0));
  ASSERT_TRUE(selection_waiter.WaitForNotification());

  EXPECT_FALSE(saw_selection_change_in_parent);
  EXPECT_TRUE(saw_selection_change_in_child1);
  EXPECT_FALSE(saw_selection_change_in_child2);

  saw_selection_change_in_parent = false;
  saw_selection_change_in_child1 = false;
  saw_selection_change_in_child2 = false;

  ExecuteScript(
      u"let range2 = document.createRange();"
      u"range2.setStart(child1.firstChild, 0);"
      u"range2.setEnd(child2.firstChild, 3);"
      u"parent.focus();"
      u"document.getSelection().removeAllRanges();"
      u"document.getSelection().addRange(range2);");
  ASSERT_TRUE(selection_waiter.WaitForNotification());

  EXPECT_TRUE(saw_selection_change_in_parent);
  EXPECT_FALSE(saw_selection_change_in_child1);
  EXPECT_FALSE(saw_selection_change_in_child2);
}

IN_PROC_BROWSER_TEST_F(AccessibilityAuraLinuxBrowserTest,
                       SetCaretInTextWithGeneratedContent) {
  LoadInitialAccessibilityTreeFromHtml(
      R"HTML(<!DOCTYPE html>
      <html>
      <body contenteditable>
      <style>h1.generated::before{content:"   [   ";}</style>
      <style>h1.generated::after{content:"   ]   ";}</style>
      <h1 class="generated">Foo</h1>
      </body>
      </html>)HTML");

  AtkObject* document = GetRendererAccessible();

  AtkObject* contenteditable = atk_object_ref_accessible_child(document, 0);
  ASSERT_NE(nullptr, contenteditable);
  ASSERT_EQ(ATK_ROLE_SECTION, atk_object_get_role(contenteditable));
  ASSERT_TRUE(ATK_IS_TEXT(contenteditable));

  AtkObject* heading = atk_object_ref_accessible_child(contenteditable, 0);
  ASSERT_NE(nullptr, heading);
  ASSERT_EQ(atk_object_get_role(heading), ATK_ROLE_HEADING);

  // The accessible text for the heading should match the rendered text and
  // not the DOM text.
  gchar* text = atk_text_get_text(ATK_TEXT(heading), 0, -1);
  ASSERT_STREQ(text, "[ Foo ]");
  g_free(text);

  AccessibilityNotificationWaiter waiter(
      shell()->web_contents(), ui::kAXModeComplete,
      ax::mojom::Event::kDocumentSelectionChanged);

  // Caret can't be set inside generated content, it will go to the closest
  // allowed place. Ordered the targets so that the caret will always actually
  // move somewhere between steps, and thus the waiter will always be satisfied.
  std::vector<int> target_offset = {0, 3, 1, 4, 2, 4, 5, 4, 6};
  std::vector<int> expect_offset = {2, 3, 2, 4, 2, 4, 2, 4, 2};
  for (size_t i = 0; i < target_offset.size(); i++) {
    atk_text_set_caret_offset(ATK_TEXT(heading), target_offset[i]);
    ASSERT_TRUE(waiter.WaitForNotification());
    ASSERT_EQ(expect_offset[i], atk_text_get_caret_offset(ATK_TEXT(heading)));
  }

  g_object_unref(heading);
  g_object_unref(contenteditable);
}

// TODO(crbug.com/41469621): This flakes on linux.
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
  ASSERT_TRUE(waiter->WaitForNotification());

  SimulateKeyPress(shell()->web_contents(), ui::DomKey::TAB, ui::DomCode::TAB,
                   ui::VKEY_TAB, false, false, false, false);
  ASSERT_TRUE(waiter->WaitForNotification());
  ASSERT_TRUE(IsAtkObjectFocused(child_3));

  // Now we repeat a similar test, but this time setting the caret offset on
  // the document. In this case, the sequential navigation starting point
  // should move to the appropriate child.
  atk_text_set_caret_offset(ATK_TEXT(document), 13);
  SimulateKeyPress(shell()->web_contents(), ui::DomKey::TAB, ui::DomCode::TAB,
                   ui::VKEY_TAB, false, false, false, false);
  ASSERT_TRUE(waiter->WaitForNotification());

  SimulateKeyPress(shell()->web_contents(), ui::DomKey::TAB, ui::DomCode::TAB,
                   ui::VKEY_TAB, false, false, false, false);
  ASSERT_TRUE(waiter->WaitForNotification());

  ASSERT_TRUE(IsAtkObjectFocused(child_7));

  // Now test setting the caret in a node that can accept focus. That
  // node should actually receive focus.
  atk_text_set_caret_offset(ATK_TEXT(child_3), 0);
  SimulateKeyPress(shell()->web_contents(), ui::DomKey::TAB, ui::DomCode::TAB,
                   ui::VKEY_TAB, false, false, false, false);
  ASSERT_TRUE(waiter->WaitForNotification());
  ASSERT_TRUE(IsAtkObjectFocused(child_3));

  AtkObject* link_section = atk_object_ref_accessible_child(child_7, 0);
  EXPECT_NE(link_section, nullptr);
  AtkObject* link_text = atk_object_ref_accessible_child(link_section, 0);
  EXPECT_NE(link_text, nullptr);
  atk_text_set_caret_offset(ATK_TEXT(link_text), 0);
  ASSERT_TRUE(waiter->WaitForNotification());
  ASSERT_TRUE(IsAtkObjectFocused(child_7));

  g_object_unref(link_section);
  g_object_unref(link_text);
  g_object_unref(child_2);
  g_object_unref(child_3);
  g_object_unref(child_7);
}

IN_PROC_BROWSER_TEST_F(AccessibilityAuraLinuxBrowserTest,
                       SelectionTriggersReparentingOnSelectionStart) {
  LoadInitialAccessibilityTreeFromHtml(
      R"HTML(<!DOCTYPE html>
      <html>
      <head>
        <script>
          document.onselectstart = () => {
            var tomove = document.getElementById("tomove");
            document.getElementById("div").appendChild(tomove);
          }
        </script>
      </head>
      <body>
         <div id="tomove">Move me</div>
         <p id="paragraph">hello world</p>
         <div id="div"></div>
      </body>
      </html>)HTML");

  AtkObject* document = GetRendererAccessible();
  AtkObject* paragraph = atk_object_ref_accessible_child(document, 1);
  ASSERT_EQ(atk_object_get_role(paragraph), ATK_ROLE_PARAGRAPH);

  AccessibilityNotificationWaiter waiter(
      shell()->web_contents(), ui::kAXModeComplete,
      ax::mojom::Event::kDocumentSelectionChanged);

  EXPECT_TRUE(atk_text_set_selection(ATK_TEXT(paragraph), 0, 0, 5));
  ASSERT_TRUE(waiter.WaitForNotification());

  gchar* selected =
      atk_text_get_selection(ATK_TEXT(paragraph), 0, nullptr, nullptr);
  EXPECT_STREQ(selected, "hello");
  g_free(selected);

  g_object_unref(paragraph);
}

IN_PROC_BROWSER_TEST_F(AccessibilityAuraLinuxBrowserTest,
                       SelectionTriggersAnchorDeletionOnSelectionStart) {
  LoadInitialAccessibilityTreeFromHtml(
      R"HTML(<!DOCTYPE html>
      <html>
      <head>
        <script>
          document.onselectstart = () => {
          document.getElementById("p").removeChild(p.childNodes[0]);
          }
        </script>
      </head>
      <body>
         <p id="p"><span>hello</span> <span>world</span></p>
         <button id="button">ok</button>
      </body>
      </html>)HTML");

  AtkObject* document = GetRendererAccessible();
  AtkObject* paragraph = atk_object_ref_accessible_child(document, 0);
  ASSERT_EQ(atk_object_get_role(paragraph), ATK_ROLE_PARAGRAPH);

  AtkObject* button = atk_object_ref_accessible_child(document, 1);
  ASSERT_EQ(atk_object_get_role(button), ATK_ROLE_PUSH_BUTTON);

  AccessibilityNotificationWaiter waiter(
      shell()->web_contents(), ui::kAXModeComplete, ax::mojom::Event::kFocus);

  EXPECT_TRUE(atk_text_set_selection(ATK_TEXT(paragraph), 0, 0, 11));
  atk_component_grab_focus(ATK_COMPONENT(button));
  ASSERT_TRUE(waiter.WaitForNotification());

  gchar* selected =
      atk_text_get_selection(ATK_TEXT(paragraph), 0, nullptr, nullptr);
  EXPECT_STREQ(selected, nullptr);
  g_free(selected);

  g_object_unref(paragraph);
  g_object_unref(button);
}

IN_PROC_BROWSER_TEST_F(AccessibilityAuraLinuxBrowserTest,
                       SelectionTriggersFocusDeletionOnSelectionStart) {
  LoadInitialAccessibilityTreeFromHtml(
      R"HTML(<!DOCTYPE html>
      <head>
        <script>
          document.onselectstart = () => {
          document.getElementById("p").removeChild(p.childNodes[2]);
          }
        </script>
      </head>
      <body>
         <p id="p"><span>hello</span> <span>world</span></p>
         <button id="button">ok</button>
      </body>
      </html>)HTML");

  AtkObject* document = GetRendererAccessible();
  AtkObject* paragraph = atk_object_ref_accessible_child(document, 0);
  ASSERT_EQ(atk_object_get_role(paragraph), ATK_ROLE_PARAGRAPH);

  AtkObject* button = atk_object_ref_accessible_child(document, 1);
  ASSERT_EQ(atk_object_get_role(button), ATK_ROLE_PUSH_BUTTON);

  AccessibilityNotificationWaiter waiter(
      shell()->web_contents(), ui::kAXModeComplete, ax::mojom::Event::kFocus);

  EXPECT_TRUE(atk_text_set_selection(ATK_TEXT(paragraph), 0, 0, 11));
  atk_component_grab_focus(ATK_COMPONENT(button));
  ASSERT_TRUE(waiter.WaitForNotification());

  gchar* selected =
      atk_text_get_selection(ATK_TEXT(paragraph), 0, nullptr, nullptr);
  EXPECT_STREQ(selected, nullptr);
  g_free(selected);

  g_object_unref(paragraph);
  g_object_unref(button);
}

IN_PROC_BROWSER_TEST_F(AccessibilityAuraLinuxBrowserTest,
                       SelectionTriggersReparentingOnFocus) {
  LoadInitialAccessibilityTreeFromHtml(
      R"HTML(<!DOCTYPE html>
      <html>
      <head>
        <script>
          function go() {
            var edit = document.getElementById("edit");
            document.getElementById("search").appendChild(edit);
          }
        </script>
      </head>
      <body>
        <span id="edit" tabindex="0" contenteditable onfocusin="go()">foo</span>
        <div id="search" role="search"></div>
      </body>
      </html>)HTML");

  AtkObject* document = GetRendererAccessible();
  AtkObject* section = atk_object_ref_accessible_child(document, 0);
  AtkObject* edit = atk_object_ref_accessible_child(section, 0);
  ASSERT_TRUE(IsAtkObjectEditable(edit));
  ASSERT_FALSE(IsAtkObjectFocused(edit));

  AccessibilityNotificationWaiter waiter(
      shell()->web_contents(), ui::kAXModeComplete,
      ax::mojom::Event::kDocumentSelectionChanged);

  EXPECT_TRUE(atk_text_set_selection(ATK_TEXT(edit), 0, 1, 2));
  ASSERT_TRUE(waiter.WaitForNotification());

  // When the unfocused contenteditable span has its selection set, focus will
  // be set. That will trigger the script in the source to move that span to
  // a different parent, causing focus to be removed and the selection cleared.
  ASSERT_FALSE(IsAtkObjectFocused(edit));
  gchar* selected = atk_text_get_selection(ATK_TEXT(edit), 0, nullptr, nullptr);
  EXPECT_STREQ(selected, nullptr);
  g_free(selected);

  g_object_unref(edit);
  g_object_unref(section);
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
  ASSERT_TRUE(waiter->WaitForNotification());

  waiter = std::make_unique<AccessibilityNotificationWaiter>(
      shell()->web_contents(), ui::kAXModeComplete,
      ui::AXEventGenerator::Event::TEXT_SELECTION_CHANGED);
  EXPECT_TRUE(atk_text_set_selection(ATK_TEXT(field_1), 0, 0, 5));
  ASSERT_TRUE(waiter->WaitForNotification());

  EXPECT_TRUE(atk_text_set_selection(ATK_TEXT(field_2), 0, 0, -1));
  ASSERT_TRUE(waiter->WaitForNotification());

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
  ASSERT_TRUE(waiter->WaitForNotification());

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
      ui::AXEventGenerator::Event::TEXT_SELECTION_CHANGED);
  ExecuteScript(
      u"let selection = document.getSelection();"
      u"let editable = document.querySelector('div[contenteditable=\"true\"]');"
      u"editable.focus();"
      u"let range = document.createRange();"
      u"range.setStart(editable.lastChild, 4);"
      u"range.setEnd(editable.lastChild, 4);"
      u"selection.removeAllRanges();"
      u"selection.addRange(range);");
  ASSERT_TRUE(selection_waiter.WaitForNotification());

  // We should see the event happen in div and not the static text element.
  EXPECT_TRUE(saw_caret_move_in_div);
  EXPECT_FALSE(saw_caret_move_in_text);
  EXPECT_FALSE(saw_caret_move_in_anonymous_block);
  EXPECT_FALSE(saw_caret_move_in_document);

  saw_caret_move_in_div = false;

  AccessibilityNotificationWaiter document_selection_waiter(
      shell()->web_contents(), ui::kAXModeComplete,
      ax::mojom::Event::kDocumentSelectionChanged);
  atk_text_set_caret_offset(anonymous_block, 3);
  ASSERT_TRUE(document_selection_waiter.WaitForNotification());

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
            <input aria-hidden="true" autofocus>
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
      ui::AXEventGenerator::Event::TEXT_SELECTION_CHANGED);
  atk_text_set_caret_offset(ATK_TEXT(div1), 4);
  ASSERT_TRUE(waiter.WaitForNotification());

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

IN_PROC_BROWSER_TEST_F(AccessibilityAuraLinuxBrowserTest,
                       TestOffsetsOfSelectionAll) {
  LoadInitialAccessibilityTreeFromHtml(R"HTML(
      <p>Hello world.</p>
      <p>Another paragraph.</p>
      <p>Goodbye world.</p>
      <script>
      var root = document.documentElement;
      window.getSelection().selectAllChildren(root);
      </script>)HTML");

  // Retrieve the AtkObject interface for the document node.
  AtkObject* document = GetRendererAccessible();
  ASSERT_TRUE(ATK_IS_COMPONENT(document));

  {
    auto* node = static_cast<ui::AXPlatformNodeAuraLinux*>(
        ui::AXPlatformNode::FromNativeViewAccessible(document));
    std::pair<int, int> offsets = node->GetSelectionOffsetsForAtk();
    EXPECT_EQ(0, offsets.first);
    EXPECT_EQ(3, offsets.second);
  }

  std::vector<int> expected = {12, 18, 14};  // text length of each child
  int number_of_children = atk_object_get_n_accessible_children(document);
  for (int i = 0; i < number_of_children; i++) {
    AtkObject* p = atk_object_ref_accessible_child(document, i);
    EXPECT_NE(p, nullptr);
    auto* node = static_cast<ui::AXPlatformNodeAuraLinux*>(
        ui::AXPlatformNode::FromNativeViewAccessible(p));
    std::pair<int, int> offsets = node->GetSelectionOffsetsForAtk();
    EXPECT_EQ(0, offsets.first);
    EXPECT_EQ(expected[i], offsets.second);
    g_object_unref(p);
  }
}

IN_PROC_BROWSER_TEST_F(AccessibilityAuraLinuxBrowserTest,
                       UniqueIdIsStableAfterRoleChange) {
  LoadInitialAccessibilityTreeFromHtml("<h1>Hello</h1>");

  AtkObject* document = GetRendererAccessible();
  AtkObject* atk_heading = atk_object_ref_accessible_child(document, 0);
  auto* heading = static_cast<ui::AXPlatformNodeBase*>(
      ui::AXPlatformNode::FromNativeViewAccessible(atk_heading));
  EXPECT_EQ(heading->GetRole(), ax::mojom::Role::kHeading);
  int32_t heading_unique_id = heading->GetUniqueId();
  EXPECT_GT(heading_unique_id, 0);

  // Change the heading to a group. This will cause it to get a new AXObject on
  // the renderer side, but the id will remain the same.
  AccessibilityNotificationWaiter waiter(
      shell()->web_contents(), ui::kAXModeComplete,
      ui::AXEventGenerator::Event::ROLE_CHANGED);
  ExecuteScript(u"document.querySelector('h1').setAttribute('role', 'group');");
  ASSERT_TRUE(waiter.WaitForNotification());

  AtkObject* atk_group = atk_object_ref_accessible_child(document, 0);
  auto* group = static_cast<ui::AXPlatformNodeBase*>(
      ui::AXPlatformNode::FromNativeViewAccessible(atk_group));
  EXPECT_EQ(group->GetRole(), ax::mojom::Role::kGroup);
  int32_t group_unique_id = group->GetUniqueId();
  EXPECT_GT(group_unique_id, 0);

  // The incoming id from the renderer remains the same.
  ASSERT_EQ(heading->GetNodeId(), group->GetNodeId());
  // The outgoing id assigned on the browser side, which is unique within the
  // window, also remains the same.
  ASSERT_EQ(heading_unique_id, group_unique_id);

  g_object_unref(atk_heading);
  g_object_unref(atk_group);
}

IN_PROC_BROWSER_TEST_F(AccessibilityAuraLinuxBrowserTest,
                       UniqueIdIsStableAfterLayoutObjectReplacement) {
  LoadInitialAccessibilityTreeFromHtml(
      "<main style='display:block'>Hello</main>");

  AtkObject* document = GetRendererAccessible();
  AtkObject* atk_block = atk_object_ref_accessible_child(document, 0);
  auto* block = static_cast<ui::AXPlatformNodeBase*>(
      ui::AXPlatformNode::FromNativeViewAccessible(atk_block));
  EXPECT_EQ(block->GetRole(), ax::mojom::Role::kMain);
  int32_t block_unique_id = block->GetUniqueId();
  EXPECT_GT(block_unique_id, 0);

  // Change the block to a inline_block. This will cause it to get a new
  // AXObject on the renderer side, but the id will remain the same.
  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kEndOfTest);
  ExecuteScript(
      u"document.querySelector('main').style.display = 'inline-block';");
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  ui::BrowserAccessibilityManager* manager =
      web_contents->GetRootBrowserAccessibilityManager();
  manager->SignalEndOfTest();
  ASSERT_TRUE(waiter.WaitForNotification());

  AtkObject* atk_inline_block = atk_object_ref_accessible_child(document, 0);
  auto* inline_block = static_cast<ui::AXPlatformNodeBase*>(
      ui::AXPlatformNode::FromNativeViewAccessible(atk_inline_block));
  EXPECT_EQ(inline_block->GetRole(), ax::mojom::Role::kMain);
  int32_t inline_block_unique_id = inline_block->GetUniqueId();
  EXPECT_GT(inline_block_unique_id, 0);

  // The incoming id from the renderer remains the same.
  ASSERT_EQ(block->GetNodeId(), inline_block->GetNodeId());
  // The outgoing id assigned on the browser side, which is unique within the
  // window, also remains the same.
  ASSERT_EQ(block_unique_id, inline_block_unique_id);

  g_object_unref(atk_block);
  g_object_unref(atk_inline_block);
}

IN_PROC_BROWSER_TEST_F(AccessibilityAuraLinuxBrowserTest,
                       TestGetIndexInParent) {
  LoadInitialAccessibilityTreeFromHtml(R"HTML(
      <p>Hello world</p>
      <p>Another paragraph.</p>
      <p>Goodbye world.</p>
      )HTML");

  // Retrieve the AtkObject interface for the document node.
  AtkObject* document = GetRendererAccessible();
  ASSERT_TRUE(ATK_IS_COMPONENT(document));
  EXPECT_EQ(0, atk_object_get_index_in_parent(document));

  int number_of_children = atk_object_get_n_accessible_children(document);
  for (int i = 0; i < number_of_children; i++) {
    AtkObject* p = atk_object_ref_accessible_child(document, i);
    EXPECT_NE(p, nullptr);
    EXPECT_EQ(i, atk_object_get_index_in_parent(p));
    g_object_unref(p);
  }
}

IN_PROC_BROWSER_TEST_F(AccessibilityAuraLinuxBrowserTest,
                       HitTestOnAncestorOfWebRoot) {
  // Load the page.
  LoadInitialAccessibilityTreeFromHtml(R"HTML(
      <button>This is a button</button>
      )HTML");

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  ui::BrowserAccessibilityManager* manager =
      web_contents->GetRootBrowserAccessibilityManager();

  // Find a node to hit test. Note that this is a really simple page,
  // so synchronous hit testing will work fine.
  ui::BrowserAccessibility* node = manager->GetBrowserAccessibilityRoot();
  while (node && node->GetRole() != ax::mojom::Role::kButton) {
    node = manager->NextInTreeOrder(node);
  }
  DCHECK(node);

  // Get the screen bounds of the hit target and find the point in the middle.
  gfx::Rect bounds = node->GetClippedScreenBoundsRect();
  gfx::Point point = bounds.CenterPoint();

  // Get the root AXPlatformNodeAuraLinux.
  ui::AXPlatformNodeAuraLinux* root_platform_node =
      static_cast<ui::AXPlatformNodeAuraLinux*>(
          ui::AXPlatformNode::FromNativeViewAccessible(
              manager->GetBrowserAccessibilityRoot()
                  ->GetNativeViewAccessible()));

  // First test that calling accHitTest on the root node returns the button.
  {
    gfx::NativeViewAccessible hit_child = root_platform_node->HitTestSync(
        point.x(), point.y(), AtkCoordType::ATK_XY_SCREEN);
    ASSERT_NE(nullptr, hit_child);
    ui::AXPlatformNode* hit_child_node =
        ui::AXPlatformNode::FromNativeViewAccessible(hit_child);
    ASSERT_NE(nullptr, hit_child_node);
    EXPECT_EQ(node->GetId(), hit_child_node->GetDelegate()->GetData().id);
  }

  // Now test it again, but this time caliing accHitTest on the parent
  // IAccessible of the web root node.
  {
    RenderWidgetHostViewAura* rwhva = static_cast<RenderWidgetHostViewAura*>(
        shell()->web_contents()->GetRenderWidgetHostView());
    gfx::NativeViewAccessible ancestor = rwhva->GetParentNativeViewAccessible();

    ASSERT_NE(nullptr, ancestor);

    ui::AXPlatformNodeAuraLinux* ancestor_node =
        static_cast<ui::AXPlatformNodeAuraLinux*>(
            ui::AXPlatformNode::FromNativeViewAccessible(ancestor));
    ASSERT_NE(nullptr, ancestor_node);

    gfx::NativeViewAccessible hit_child = ancestor_node->HitTestSync(
        point.x(), point.y(), AtkCoordType::ATK_XY_SCREEN);
    ASSERT_NE(nullptr, hit_child);
    ui::AXPlatformNode* hit_child_node =
        ui::AXPlatformNode::FromNativeViewAccessible(hit_child);
    ASSERT_NE(nullptr, hit_child_node);
    EXPECT_EQ(node->GetId(), hit_child_node->GetDelegate()->GetData().id);
  }
}

// Tests if it does not DCHECK when textarea has a placeholder break element.
IN_PROC_BROWSER_TEST_F(AccessibilityAuraLinuxBrowserTest,
                       TestGetTextContainerFromTextArea) {
  std::string content = std::string("<textarea style=\"height:100px;\">hello") +
                        std::string("\n") + std::string("</textarea>");
  LoadInitialAccessibilityTreeFromHtml(content);

  AtkText* atk_text = FindNode(ATK_ROLE_ENTRY);
  AtkTextRectangle atk_rect;
  // atk_text_get_range_extents() calls GetTextContainerForPlainTextField() and
  // DCHECK on checking children counts.
  atk_text_get_range_extents(atk_text, 0, 7, AtkCoordType::ATK_XY_SCREEN,
                             &atk_rect);
  g_object_unref(atk_text);
}

IN_PROC_BROWSER_TEST_F(AccessibilityAuraLinuxBrowserTest,
                       TestCaretMovedInNumberInput) {
  LoadInitialAccessibilityTreeFromHtml(
      R"HTML(<input type="number" value="12">
      )HTML");
  AccessibilityNotificationWaiter waiter(
      shell()->web_contents(), ui::kAXModeComplete,
      ui::AXEventGenerator::Event::TEXT_SELECTION_CHANGED);
  auto caret_callback =
      G_CALLBACK(+[](AtkText*, int new_position, int* out_caret_position) {
        *out_caret_position = new_position;
      });
  int out_caret_position = -1;
  AtkText* input_text = FindNode(ATK_ROLE_SPIN_BUTTON);
  g_signal_connect(input_text, "text-caret-moved", caret_callback,
                   &out_caret_position);

  atk_text_set_caret_offset(input_text, 0);
  ASSERT_TRUE(waiter.WaitForNotification());
  EXPECT_EQ(atk_text_get_caret_offset(input_text), 0);
  EXPECT_EQ(out_caret_position, 0);

  atk_text_set_caret_offset(input_text, 1);
  ASSERT_TRUE(waiter.WaitForNotification());
  EXPECT_EQ(atk_text_get_caret_offset(input_text), 1);
  EXPECT_EQ(out_caret_position, 1);

  atk_text_set_caret_offset(input_text, 2);
  ASSERT_TRUE(waiter.WaitForNotification());
  EXPECT_EQ(atk_text_get_caret_offset(input_text), 2);
  EXPECT_EQ(out_caret_position, 2);

  g_object_unref(input_text);
}

}  // namespace content
