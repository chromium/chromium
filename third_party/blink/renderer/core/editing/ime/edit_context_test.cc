// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/ime/edit_context.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_edit_context_init.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

class EditContextTest : public EditingTestBase {
 protected:
  EditContext* CreateEditContext(ScriptState* script_state,
                                 const String& text,
                                 uint32_t caret_pos) {
    return CreateEditContext(script_state, text, caret_pos, caret_pos);
  }

  EditContext* CreateEditContext(ScriptState* script_state,
                                 const String& text,
                                 uint32_t selection_start,
                                 uint32_t selection_end) {
    EditContextInit* init = EditContextInit::Create();
    init->setText(text);
    init->setSelectionStart(selection_start);
    init->setSelectionEnd(selection_end);
    return EditContext::Create(script_state, init);
  }
};

TEST_F(EditContextTest, DeleteSurroundingTextNormal) {
  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
  ScriptState::Scope script_scope(script_state);
  auto* edit_context = CreateEditContext(script_state, "abcdef", 2);

  edit_context->DeleteSurroundingText(1, 1);

  EXPECT_EQ(edit_context->text(), "adef");
  EXPECT_EQ(edit_context->selectionStart(), 1u);
  EXPECT_EQ(edit_context->selectionEnd(), 1u);
}

TEST_F(EditContextTest, DeleteSurroundingTextWithBackwardSelection) {
  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
  ScriptState::Scope script_scope(script_state);
  auto* edit_context = CreateEditContext(script_state, "abcdef", 4, 2);

  edit_context->DeleteSurroundingText(1, 1);

  EXPECT_EQ(edit_context->text(), "acdf");
  EXPECT_EQ(edit_context->selectionStart(), 3u);
  EXPECT_EQ(edit_context->selectionEnd(), 1u);
}

TEST_F(EditContextTest, DeleteSurroundingTextClampBefore) {
  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
  ScriptState::Scope script_scope(script_state);
  auto* edit_context = CreateEditContext(script_state, "abcdef", 2);

  edit_context->DeleteSurroundingText(5, 1);

  EXPECT_EQ(edit_context->text(), "def");
  EXPECT_EQ(edit_context->selectionStart(), 0u);
  EXPECT_EQ(edit_context->selectionEnd(), 0u);
}

TEST_F(EditContextTest, DeleteSurroundingTextClampAfter) {
  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
  ScriptState::Scope script_scope(script_state);
  auto* edit_context = CreateEditContext(script_state, "abcdef", 2);

  edit_context->DeleteSurroundingText(1, 5);

  EXPECT_EQ(edit_context->text(), "a");
  EXPECT_EQ(edit_context->selectionStart(), 1u);
  EXPECT_EQ(edit_context->selectionEnd(), 1u);
}

TEST_F(EditContextTest, DeleteSurroundingTextNegativeBefore) {
  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
  ScriptState::Scope script_scope(script_state);
  auto* edit_context = CreateEditContext(script_state, "abcdef", 2);

  edit_context->DeleteSurroundingText(-3, 1);

  EXPECT_EQ(edit_context->text(), "abdef");
  EXPECT_EQ(edit_context->selectionStart(), 2u);
  EXPECT_EQ(edit_context->selectionEnd(), 2u);
}

TEST_F(EditContextTest, DeleteSurroundingTextNegativeAfter) {
  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
  ScriptState::Scope script_scope(script_state);
  auto* edit_context = CreateEditContext(script_state, "abcdef", 2);

  edit_context->DeleteSurroundingText(1, -3);

  EXPECT_EQ(edit_context->text(), "acdef");
  EXPECT_EQ(edit_context->selectionStart(), 1u);
  EXPECT_EQ(edit_context->selectionEnd(), 1u);
}

TEST_F(EditContextTest,
       DeleteSurroundingTextInCodePointsWithMultiCodeTextOnTheLeft) {
  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
  ScriptState::Scope script_scope(script_state);
  // 'a' + "black star" + SPACE + "trophy" + SPACE + composed text (U+0E01
  // "ka kai" + U+0E49 "mai tho").
  // A "black star" is 1 grapheme cluster. It has 1 code point, and its length
  // is 1 (abbreviated as [1,1,1]). A "trophy": [1,1,2]. The composed text:
  // [1,2,2].
  const String& text = String::FromUtf8(
      "a\xE2\x98\x85 \xF0\x9F\x8F\x86 \xE0\xB8\x81\xE0\xB9\x89");

  auto* edit_context = CreateEditContext(script_state, text, 8);

  edit_context->DeleteSurroundingTextInCodePoints(2, 0);
  EXPECT_EQ(edit_context->text(),
            String::FromUtf8("a\xE2\x98\x85 \xF0\x9F\x8F\x86 "));
  EXPECT_EQ(edit_context->selectionStart(), 6u);
  EXPECT_EQ(edit_context->selectionEnd(), 6u);

  edit_context->DeleteSurroundingTextInCodePoints(4, 0);
  EXPECT_EQ(edit_context->text(), "a");
  EXPECT_EQ(edit_context->selectionStart(), 1u);
  EXPECT_EQ(edit_context->selectionEnd(), 1u);
}

TEST_F(EditContextTest,
       DeleteSurroundingTextInCodePointsWithMultiCodeTextOnTheRight) {
  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
  ScriptState::Scope script_scope(script_state);
  const String& text = String::FromUtf8(
      "a\xE2\x98\x85 \xF0\x9F\x8F\x86 \xE0\xB8\x81\xE0\xB9\x89");

  auto* edit_context = CreateEditContext(script_state, text, 0);

  edit_context->DeleteSurroundingTextInCodePoints(0, 5);
  EXPECT_EQ(edit_context->text(), String::FromUtf8("\xE0\xB8\x81\xE0\xB9\x89"));
  EXPECT_EQ(edit_context->selectionStart(), 0u);
  EXPECT_EQ(edit_context->selectionEnd(), 0u);

  edit_context->DeleteSurroundingTextInCodePoints(0, 1);
  // We should only delete 1 code point.
  EXPECT_EQ(edit_context->text(), String::FromUtf8("\xE0\xB9\x89"));
  EXPECT_EQ(edit_context->selectionStart(), 0u);
  EXPECT_EQ(edit_context->selectionEnd(), 0u);
}

TEST_F(EditContextTest,
       DeleteSurroundingTextInCodePointsWithInvalidSurrogatePair) {
  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
  ScriptState::Scope script_scope(script_state);
  DummyExceptionStateForTesting exception_state;

  // 'a' + high surrogate of "trophy" + "black star" + low surrogate of "trophy"
  // + SPACE
  const UChar kUText[] = {'a', 0xD83C, 0x2605, 0xDFC6, ' ', '\0'};
  const String& text = String(kUText);

  // Test deletion from the end: cursor at position 5 (after SPACE)
  auto* edit_context = CreateEditContext(script_state, text, 5);

  // Delete a SPACE (1 code point back).
  edit_context->DeleteSurroundingTextInCodePoints(1, 0);
  const UChar kExpected1[] = {'a', 0xD83C, 0x2605, 0xDFC6, '\0'};
  EXPECT_EQ(edit_context->text(), String(kExpected1));
  EXPECT_EQ(edit_context->selectionStart(), 4u);
  EXPECT_EQ(edit_context->selectionEnd(), 4u);

  // Reset and try again from end position with the full text.
  edit_context->updateText(0, edit_context->text().length(), text,
                           exception_state);
  edit_context->updateSelection(5, 5, exception_state);
  // Do nothing since there is an invalid surrogate in the requested range.
  edit_context->DeleteSurroundingTextInCodePoints(2, 0);
  EXPECT_EQ(edit_context->text(), text);
  EXPECT_EQ(edit_context->selectionStart(), 5u);
  EXPECT_EQ(edit_context->selectionEnd(), 5u);

  // Test deletion from the beginning: cursor at position 0
  edit_context->updateText(0, edit_context->text().length(), text,
                           exception_state);
  edit_context->updateSelection(0, 0, exception_state);
  // Delete 'a' (1 code point forward).
  edit_context->DeleteSurroundingTextInCodePoints(0, 1);
  const UChar kExpected2[] = {0xD83C, 0x2605, 0xDFC6, ' ', '\0'};
  EXPECT_EQ(edit_context->text(), String(kExpected2));
  EXPECT_EQ(edit_context->selectionStart(), 0u);
  EXPECT_EQ(edit_context->selectionEnd(), 0u);

  // Reset and try again from beginning with the full text.
  edit_context->updateText(0, edit_context->text().length(), text,
                           exception_state);
  edit_context->updateSelection(0, 0, exception_state);
  // Do nothing since there is an invalid surrogate in the requested range.
  edit_context->DeleteSurroundingTextInCodePoints(0, 2);
  EXPECT_EQ(edit_context->text(), text);
  EXPECT_EQ(edit_context->selectionStart(), 0u);
  EXPECT_EQ(edit_context->selectionEnd(), 0u);
}

TEST_F(EditContextTest, DeleteSurroundingTextInCodePointsEdgeCases) {
  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
  ScriptState::Scope script_scope(script_state);
  DummyExceptionStateForTesting exception_state;

  // Empty text
  auto* edit_context = CreateEditContext(script_state, "", 0);
  edit_context->DeleteSurroundingTextInCodePoints(1, 0);
  EXPECT_EQ(edit_context->text(), "");
  EXPECT_EQ(edit_context->selectionStart(), 0u);
  EXPECT_EQ(edit_context->selectionEnd(), 0u);

  edit_context->updateText(0, edit_context->text().length(), "",
                           exception_state);
  edit_context->updateSelection(0, 0, exception_state);
  edit_context->DeleteSurroundingTextInCodePoints(0, 1);
  EXPECT_EQ(edit_context->text(), "");
  EXPECT_EQ(edit_context->selectionStart(), 0u);
  EXPECT_EQ(edit_context->selectionEnd(), 0u);

  // Clamp before
  edit_context->updateText(0, edit_context->text().length(), "abcdef",
                           exception_state);
  edit_context->updateSelection(2, 2, exception_state);
  edit_context->DeleteSurroundingTextInCodePoints(5, 1);
  EXPECT_EQ(edit_context->text(), "def");
  EXPECT_EQ(edit_context->selectionStart(), 0u);
  EXPECT_EQ(edit_context->selectionEnd(), 0u);

  // Clamp after
  edit_context->updateText(0, edit_context->text().length(), "abcdef",
                           exception_state);
  edit_context->updateSelection(2, 2, exception_state);
  edit_context->DeleteSurroundingTextInCodePoints(1, 5);
  EXPECT_EQ(edit_context->text(), "a");
  EXPECT_EQ(edit_context->selectionStart(), 1u);
  EXPECT_EQ(edit_context->selectionEnd(), 1u);

  // Both sides
  const String& multi_code_text = String::FromUtf8(
      "a\xE2\x98\x85 \xF0\x9F\x8F\x86 \xE0\xB8\x81\xE0\xB9\x89");
  edit_context->updateText(0, edit_context->text().length(), multi_code_text,
                           exception_state);
  edit_context->updateSelection(3, 3, exception_state);
  edit_context->DeleteSurroundingTextInCodePoints(2, 2);
  EXPECT_EQ(edit_context->text(),
            String::FromUtf8("a\xE0\xB8\x81\xE0\xB9\x89"));
  EXPECT_EQ(edit_context->selectionStart(), 1u);
  EXPECT_EQ(edit_context->selectionEnd(), 1u);
}

TEST_F(EditContextTest, InRangeUpdatesDoNotRecordUseCounters) {
  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
  ScriptState::Scope script_scope(script_state);
  auto* edit_context = CreateEditContext(script_state, "abcdef", 2);
  DummyExceptionStateForTesting exception_state;

  edit_context->updateSelection(1, 3, exception_state);
  HeapVector<Member<DOMRect>> character_bounds = {DOMRect::Create()};
  edit_context->updateCharacterBounds(5, character_bounds);
  edit_context->updateText(0, 4, "xyz", exception_state);

  EXPECT_FALSE(exception_state.HadException());
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kEditContextUpdateTextRangeExceedsTextRange));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kEditContextUpdateSelectionRangeExceedsTextRange));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kEditContextUpdateCharacterBoundsExceedsTextRange));
}

TEST_F(EditContextTest, OutOfRangeUpdatesRecordUseCounters) {
  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
  ScriptState::Scope script_scope(script_state);
  auto* edit_context = CreateEditContext(script_state, "abcdef", 2);
  DummyExceptionStateForTesting exception_state;

  // "abcdef" has length 6, so an end offset of 10 is out of range.
  edit_context->updateSelection(1, 10, exception_state);

  EXPECT_FALSE(exception_state.HadException());
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kEditContextUpdateSelectionRangeExceedsTextRange));
  // Selection offsets should be clamped to the text length.
  EXPECT_EQ(edit_context->selectionStart(), 1u);
  EXPECT_EQ(edit_context->selectionEnd(), 6u);

  HeapVector<Member<DOMRect>> character_bounds = {DOMRect::Create()};
  edit_context->updateCharacterBounds(6, character_bounds);

  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kEditContextUpdateCharacterBoundsExceedsTextRange));

  edit_context->updateText(0, 10, "xyz", exception_state);

  EXPECT_FALSE(exception_state.HadException());
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kEditContextUpdateTextRangeExceedsTextRange));
  EXPECT_EQ(edit_context->text(), "xyz");
}

TEST_F(EditContextTest, UpdateTextClampsOutOfRangeBounds) {
  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
  ScriptState::Scope script_scope(script_state);
  auto* edit_context = CreateEditContext(script_state, "abcdef", 6);
  DummyExceptionStateForTesting exception_state;

  // Both offsets are past the end of the text, so they should be clamped to
  // offset 6 and the update should become an insertion at the end.
  edit_context->updateText(8, 10, "xyz", exception_state);

  EXPECT_FALSE(exception_state.HadException());
  EXPECT_EQ(edit_context->text(), "abcdefxyz");
  // Selection adjustment must use the clamped range and move the caret past
  // the inserted text.
  EXPECT_EQ(edit_context->selectionStart(), 9u);
  EXPECT_EQ(edit_context->selectionEnd(), 9u);
}

}  // namespace blink
