// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/ime/edit_context.h"

#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_range.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_edit_context_init.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/editing/ime/input_method_controller.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "ui/base/ime/ime_text_span.h"

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

namespace {

class CallbackEventListener final : public NativeEventListener {
 public:
  using Callback = base::RepeatingCallback<void(Event*)>;
  explicit CallbackEventListener(Callback callback)
      : callback_(std::move(callback)) {}

  void Invoke(ExecutionContext*, Event* event) override {
    callback_.Run(event);
  }

 private:
  Callback callback_;
};

}  // namespace

TEST_F(EditContextTest, InvalidateMarksContextDirtyAndFiresContextRequest) {
  ScopedEditContextOnDemandForTest scoped_feature(true);
  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
  ScriptState::Scope script_scope(script_state);
  auto* edit_context = CreateEditContext(script_state, "", 0);

  int context_request_count = 0;

  edit_context->addEventListener(
      event_type_names::kContextrequest,
      MakeGarbageCollected<CallbackEventListener>(
          base::BindLambdaForTesting([&](Event* event) {
            ++context_request_count;
            EXPECT_EQ(event->type(), event_type_names::kContextrequest);
            EXPECT_FALSE(event->bubbles());
            EXPECT_FALSE(event->cancelable());
            // Dirty flag remains set throughout synchronous dispatch until
            // `CleanContextIfNeeded()` finishes.
            EXPECT_TRUE(edit_context->IsContextDirtyForTesting());
            DummyExceptionStateForTesting exception_state;
            edit_context->updateText(0, edit_context->text().length(), "abcdef",
                                     exception_state);
            edit_context->updateSelection(2, 2, exception_state);
          })));

  EXPECT_FALSE(edit_context->IsContextDirtyForTesting());
  edit_context->invalidate();
  EXPECT_TRUE(edit_context->IsContextDirtyForTesting());

  // Performing an IME deletion when dirty fires `contextrequest` synchronously
  // before applying the deletion.
  edit_context->DeleteSurroundingText(1, 1);

  EXPECT_EQ(context_request_count, 1);
  EXPECT_FALSE(edit_context->IsContextDirtyForTesting());
  EXPECT_EQ(edit_context->text(), "adef");
  EXPECT_EQ(edit_context->selectionStart(), 1u);
  EXPECT_EQ(edit_context->selectionEnd(), 1u);

  // Subsequent IME operations while clean do not re-fire `contextrequest`.
  edit_context->DeleteSurroundingText(0, 1);
  EXPECT_EQ(context_request_count, 1);
  EXPECT_EQ(edit_context->text(), "aef");
}

TEST_F(EditContextTest, InvalidateCancelsActiveComposition) {
  ScopedEditContextOnDemandForTest scoped_feature(true);
  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
  ScriptState::Scope script_scope(script_state);
  auto* edit_context = CreateEditContext(script_state, "hello ", 6);

  int composition_end_count = 0;
  edit_context->addEventListener(
      event_type_names::kCompositionend,
      MakeGarbageCollected<CallbackEventListener>(base::BindLambdaForTesting(
          [&](Event*) { ++composition_end_count; })));

  edit_context->SetComposition("world", {}, WebRange(), 5, 5);
  EXPECT_FALSE(edit_context->CompositionRange().IsEmpty());

  edit_context->invalidate();
  EXPECT_EQ(composition_end_count, 1);
  EXPECT_TRUE(edit_context->CompositionRange().IsEmpty());
  EXPECT_TRUE(edit_context->IsContextDirtyForTesting());
}

TEST_F(EditContextTest,
       UpdateTextOrSelectionOutsideContextRequestDoesNotClearDirty) {
  ScopedEditContextOnDemandForTest scoped_feature(true);
  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
  ScriptState::Scope script_scope(script_state);
  auto* edit_context = CreateEditContext(script_state, "old", 3);

  int context_request_count = 0;
  edit_context->addEventListener(
      event_type_names::kContextrequest,
      MakeGarbageCollected<CallbackEventListener>(
          base::BindLambdaForTesting([&](Event*) {
            ++context_request_count;
            DummyExceptionStateForTesting exception_state;
            edit_context->updateText(0, edit_context->text().length(),
                                     "Paragraph 1\nParagraph 2",
                                     exception_state);
            edit_context->updateSelection(12, 23, exception_state);
          })));

  edit_context->invalidate();
  EXPECT_TRUE(edit_context->IsContextDirtyForTesting());

  // Partial or unrelated `updateSelection()` / `updateText()` calls before
  // `CleanContextIfNeeded()` must not prematurely clear `is_context_dirty_`.
  DummyExceptionStateForTesting exception_state;
  edit_context->updateSelection(0, 0, exception_state);
  EXPECT_TRUE(edit_context->IsContextDirtyForTesting());
  edit_context->updateText(0, 3, "partial", exception_state);
  EXPECT_TRUE(edit_context->IsContextDirtyForTesting());

  edit_context->CleanContextIfNeeded();
  EXPECT_EQ(context_request_count, 1);
  EXPECT_FALSE(edit_context->IsContextDirtyForTesting());
  EXPECT_EQ(edit_context->text(), "Paragraph 1\nParagraph 2");
  EXPECT_EQ(edit_context->selectionStart(), 12u);
  EXPECT_EQ(edit_context->selectionEnd(), 23u);
  EXPECT_EQ(edit_context->text().substr(
                edit_context->selectionStart(),
                edit_context->selectionEnd() - edit_context->selectionStart()),
            "Paragraph 2");
}

TEST_F(EditContextTest, ImeOperationsPullContextOnDemand) {
  ScopedEditContextOnDemandForTest scoped_feature(true);
  SetBodyInnerHTML("<div id='editor'></div>");
  auto* editor =
      To<HTMLElement>(GetDocument().getElementById(AtomicString("editor")));
  ASSERT_NE(editor, nullptr);

  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
  ScriptState::Scope script_scope(script_state);
  DummyExceptionStateForTesting init_exception_state;
  auto* edit_context = CreateEditContext(script_state, "", 0);
  editor->setEditContext(edit_context, init_exception_state);
  editor->Focus();

  int context_request_count = 0;
  edit_context->addEventListener(
      event_type_names::kContextrequest,
      MakeGarbageCollected<CallbackEventListener>(
          base::BindLambdaForTesting([&](Event* event) {
            ++context_request_count;
            EXPECT_EQ(event->type(), event_type_names::kContextrequest);
            DummyExceptionStateForTesting exception_state;
            edit_context->updateText(0, edit_context->text().length(), "hello",
                                     exception_state);
            edit_context->updateSelection(5, 5, exception_state);
          })));

  // 1. InsertText
  edit_context->invalidate();
  EXPECT_TRUE(edit_context->InsertText("!"));
  EXPECT_EQ(context_request_count, 1);
  EXPECT_FALSE(edit_context->IsContextDirtyForTesting());
  EXPECT_EQ(edit_context->text(), "hello!");

  // 2. CommitText
  edit_context->invalidate();
  EXPECT_TRUE(edit_context->CommitText(" world", {}, WebRange(), 0));
  EXPECT_EQ(context_request_count, 2);
  EXPECT_FALSE(edit_context->IsContextDirtyForTesting());
  EXPECT_EQ(edit_context->text(), "hello world");

  // 3. SetComposition
  edit_context->invalidate();
  EXPECT_TRUE(edit_context->SetComposition(" there", {}, WebRange(), 6, 6));
  EXPECT_EQ(context_request_count, 3);
  EXPECT_FALSE(edit_context->IsContextDirtyForTesting());
  EXPECT_EQ(edit_context->text(), "hello there");
  edit_context->FinishComposingText(WebInputMethodController::kKeepSelection);

  // 4. SetCompositionFromExistingText (pre-invalidation absolute offsets bail
  // out after pulling fresh context; subsequent call while clean succeeds)
  edit_context->invalidate();
  EXPECT_FALSE(edit_context->SetCompositionFromExistingText(0, 5, {}));
  EXPECT_EQ(context_request_count, 4);
  EXPECT_FALSE(edit_context->IsContextDirtyForTesting());
  EXPECT_TRUE(edit_context->CompositionRange().IsEmpty());
  EXPECT_TRUE(edit_context->SetCompositionFromExistingText(0, 5, {}));
  EXPECT_EQ(context_request_count, 4);
  EXPECT_EQ(edit_context->CompositionRange().StartOffset(), 0);
  EXPECT_EQ(edit_context->CompositionRange().EndOffset(), 5);
  edit_context->FinishComposingText(WebInputMethodController::kKeepSelection);

  // 5. DeleteBackward (via EnsureSelectionWithinTextBounds)
  edit_context->invalidate();
  edit_context->DeleteBackward();
  EXPECT_EQ(context_request_count, 5);
  EXPECT_FALSE(edit_context->IsContextDirtyForTesting());
  EXPECT_EQ(edit_context->text(), "hell");

  // 6. ExtendSelectionAndDelete
  edit_context->invalidate();
  edit_context->ExtendSelectionAndDelete(2, 0);
  EXPECT_EQ(context_request_count, 6);
  EXPECT_FALSE(edit_context->IsContextDirtyForTesting());
  EXPECT_EQ(edit_context->text(), "hel");

  // 7. DeleteSurroundingTextInCodePoints
  edit_context->invalidate();
  edit_context->DeleteSurroundingTextInCodePoints(1, 0);
  EXPECT_EQ(context_request_count, 7);
  EXPECT_FALSE(edit_context->IsContextDirtyForTesting());
  EXPECT_EQ(edit_context->text(), "hell");
}

TEST_F(EditContextTest, TextInputInfoDoesNotEagerlyPullContext) {
  ScopedEditContextOnDemandForTest scoped_feature(true);
  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
  ScriptState::Scope script_scope(script_state);
  auto* edit_context = CreateEditContext(script_state, "initial", 7);

  int context_request_count = 0;
  edit_context->addEventListener(
      event_type_names::kContextrequest,
      MakeGarbageCollected<CallbackEventListener>(base::BindLambdaForTesting(
          [&](Event*) { ++context_request_count; })));

  edit_context->invalidate();
  EXPECT_TRUE(edit_context->IsContextDirtyForTesting());

  // WidgetBase::DidBeginMainFrame calls TextInputInfo() on every animation
  // frame; it must not eagerly fire `contextrequest` or clear the dirty bit.
  edit_context->TextInputInfo();
  EXPECT_EQ(context_request_count, 0);
  EXPECT_TRUE(edit_context->IsContextDirtyForTesting());
}

TEST_F(
    EditContextTest,
    CleanContextIfNeededCoalescesReentrantCallsAndPreservesMidDispatchInvalidate) {
  ScopedEditContextOnDemandForTest scoped_feature(true);
  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
  ScriptState::Scope script_scope(script_state);
  auto* edit_context = CreateEditContext(script_state, "abc", 3);

  int context_request_count = 0;
  bool invalidate_during_dispatch = false;
  edit_context->addEventListener(event_type_names::kContextrequest,
                                 MakeGarbageCollected<CallbackEventListener>(
                                     base::BindLambdaForTesting([&](Event*) {
                                       ++context_request_count;
                                       // Nested `CleanContextIfNeeded()` calls
                                       // during dispatch must be coalesced
                                       // rather than recursively firing another
                                       // `contextrequest`.
                                       edit_context->CleanContextIfNeeded();
                                       if (invalidate_during_dispatch) {
                                         edit_context->invalidate();
                                       }
                                     })));

  // Case 1: Normal re-entrant call without mid-dispatch `invalidate()` clears
  // the dirty bit once dispatch completes.
  edit_context->invalidate();
  edit_context->CleanContextIfNeeded();
  EXPECT_EQ(context_request_count, 1);
  EXPECT_FALSE(edit_context->IsContextDirtyForTesting());

  // Case 2: Calling `invalidate()` inside the `contextrequest` listener keeps
  // the EditContext marked dirty after dispatch returns, so the next pull fires
  // `contextrequest` again.
  invalidate_during_dispatch = true;
  edit_context->invalidate();
  edit_context->CleanContextIfNeeded();
  EXPECT_EQ(context_request_count, 2);
  EXPECT_TRUE(edit_context->IsContextDirtyForTesting());

  invalidate_during_dispatch = false;
  edit_context->CleanContextIfNeeded();
  EXPECT_EQ(context_request_count, 3);
  EXPECT_FALSE(edit_context->IsContextDirtyForTesting());
}

TEST_F(EditContextTest,
       ReentrantInvalidateDuringCompositionTeardownDoesNotCorruptText) {
  ScopedEditContextOnDemandForTest scoped_feature(true);
  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
  ScriptState::Scope script_scope(script_state);

  // 1. Re-entrant `invalidate()` inside `textupdate` and `compositionend`
  // during `invalidate()` -> `CancelComposition()` does not recurse infinitely
  // or delete surrounding text.
  {
    auto* edit_context = CreateEditContext(script_state, "hello ", 6);
    ASSERT_TRUE(edit_context->SetComposition("world", {}, WebRange(), 5, 5));
    EXPECT_EQ(edit_context->text(), "hello world");

    int composition_end_count = 0;
    edit_context->addEventListener(
        event_type_names::kTextupdate,
        MakeGarbageCollected<CallbackEventListener>(base::BindLambdaForTesting(
            [&](Event*) { edit_context->invalidate(); })));
    edit_context->addEventListener(event_type_names::kCompositionend,
                                   MakeGarbageCollected<CallbackEventListener>(
                                       base::BindLambdaForTesting([&](Event*) {
                                         ++composition_end_count;
                                         edit_context->invalidate();
                                       })));

    edit_context->invalidate();
    EXPECT_EQ(composition_end_count, 1);
    EXPECT_EQ(edit_context->text(), "hello ");
    EXPECT_TRUE(edit_context->CompositionRange().IsEmpty());
    EXPECT_TRUE(edit_context->IsContextDirtyForTesting());
  }

  // 2. Re-entrant `invalidate()` inside `compositionend` during `CommitText()`
  // marks the context dirty without canceling/deleting the committed text.
  {
    auto* edit_context = CreateEditContext(script_state, "hello ", 6);
    ASSERT_TRUE(edit_context->SetComposition("comp", {}, WebRange(), 4, 4));

    int composition_end_count = 0;
    edit_context->addEventListener(event_type_names::kCompositionend,
                                   MakeGarbageCollected<CallbackEventListener>(
                                       base::BindLambdaForTesting([&](Event*) {
                                         ++composition_end_count;
                                         edit_context->invalidate();
                                       })));

    EXPECT_TRUE(edit_context->CommitText("committed", {}, WebRange(), 0));
    EXPECT_EQ(composition_end_count, 1);
    EXPECT_EQ(edit_context->text(), "hello committed");
    EXPECT_TRUE(edit_context->CompositionRange().IsEmpty());
    EXPECT_TRUE(edit_context->IsContextDirtyForTesting());
  }

  // 3. Re-entrant `invalidate()` inside `compositionend` during
  // `FinishComposingText()` preserves the confirmed composition text.
  {
    auto* edit_context = CreateEditContext(script_state, "hello ", 6);
    ASSERT_TRUE(
        edit_context->SetComposition("confirmed", {}, WebRange(), 9, 9));

    int composition_end_count = 0;
    edit_context->addEventListener(event_type_names::kCompositionend,
                                   MakeGarbageCollected<CallbackEventListener>(
                                       base::BindLambdaForTesting([&](Event*) {
                                         ++composition_end_count;
                                         edit_context->invalidate();
                                       })));

    EXPECT_TRUE(edit_context->FinishComposingText(
        WebInputMethodController::kKeepSelection));
    EXPECT_EQ(composition_end_count, 1);
    EXPECT_EQ(edit_context->text(), "hello confirmed");
    EXPECT_TRUE(edit_context->CompositionRange().IsEmpty());
    EXPECT_TRUE(edit_context->IsContextDirtyForTesting());
  }
}

TEST_F(EditContextTest, InvalidateInDetachedFrameDuringActiveComposition) {
  ScopedEditContextOnDemandForTest scoped_feature(true);
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize();
  LocalFrame* main_frame = web_view_helper.LocalMainFrame()->GetFrame();
  main_frame->GetDocument()->body()->SetInnerHTMLWithoutTrustedTypes(
      "<iframe id='child'></iframe>");
  main_frame->GetDocument()->UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  auto* iframe = To<HTMLIFrameElement>(
      main_frame->GetDocument()->getElementById(AtomicString("child")));
  ASSERT_NE(iframe, nullptr);
  auto* child_frame = To<LocalFrame>(iframe->ContentFrame());
  ASSERT_NE(child_frame, nullptr);
  Document* child_document = child_frame->GetDocument();
  ASSERT_NE(child_document, nullptr);
  child_document->body()->SetInnerHTMLWithoutTrustedTypes(
      "<div id='editor'></div>");
  auto* child_editor =
      To<HTMLElement>(child_document->getElementById(AtomicString("editor")));
  ASSERT_NE(child_editor, nullptr);

  ScriptState* child_script_state = ToScriptStateForMainWorld(child_frame);
  ScriptState::Scope script_scope(child_script_state);
  auto* edit_context = CreateEditContext(child_script_state, "hello ", 6);
  auto* edit_context_detached_frame =
      CreateEditContext(child_script_state, "abc", 3);
  ASSERT_TRUE(edit_context->SetComposition("world", {}, WebRange(), 5, 5));
  ASSERT_TRUE(
      edit_context_detached_frame->SetComposition("def", {}, WebRange(), 3, 3));
  EXPECT_FALSE(edit_context->CompositionRange().IsEmpty());

  // Detaching the <iframe> shuts down `child_document`, clearing its
  // ExecutionContext. Associating `child_editor` leaves `DomWindow()` null on
  // `edit_context`, while `edit_context_detached_frame` has a non-null
  // `DomWindow()` with a null `GetFrame()`.
  iframe->remove();
  DummyExceptionStateForTesting exception_state;
  child_editor->setEditContext(edit_context, exception_state);
  ASSERT_EQ(edit_context->DomWindow(), nullptr);

  edit_context->invalidate();
  EXPECT_TRUE(edit_context->CompositionRange().IsEmpty());
  EXPECT_EQ(edit_context->text(), "hello ");
  EXPECT_TRUE(edit_context->IsContextDirtyForTesting());

  edit_context_detached_frame->invalidate();
  EXPECT_TRUE(edit_context_detached_frame->CompositionRange().IsEmpty());
  EXPECT_EQ(edit_context_detached_frame->text(), "abc");
  EXPECT_TRUE(edit_context_detached_frame->IsContextDirtyForTesting());
}

TEST_F(EditContextTest,
       CompositionEdgeCasesWithInvalidateAndEmptySetCompositionNoOp) {
  ScopedEditContextOnDemandForTest scoped_feature(true);
  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
  ScriptState::Scope script_scope(script_state);

  // 1. Empty `SetComposition` when dirty and not composing is a no-op and must
  // not fire `contextrequest`.
  {
    auto* edit_context = CreateEditContext(script_state, "stale", 5);
    int context_request_count = 0;
    edit_context->addEventListener(
        event_type_names::kContextrequest,
        MakeGarbageCollected<CallbackEventListener>(base::BindLambdaForTesting(
            [&](Event*) { ++context_request_count; })));

    edit_context->invalidate();
    EXPECT_TRUE(edit_context->SetComposition("", {}, WebRange(), 0, 0));
    EXPECT_EQ(context_request_count, 0);
    EXPECT_TRUE(edit_context->IsContextDirtyForTesting());
  }

  // 2. Calling `invalidate()` inside `compositionstart` leaves both
  // `is_context_dirty_` and `has_composition_` set; a subsequent `InsertText`
  // cleanly cancels the composition and pulls fresh context without DCHECKing.
  // Also verifies that if `compositionend` triggers a nested
  // `CleanContextIfNeeded()`, the outer `CleanContextIfNeeded()` does not fire
  // a duplicate `contextrequest`.
  {
    auto* edit_context = CreateEditContext(script_state, "old", 3);
    int context_request_count = 0;
    edit_context->addEventListener(
        event_type_names::kCompositionstart,
        MakeGarbageCollected<CallbackEventListener>(base::BindLambdaForTesting(
            [&](Event*) { edit_context->invalidate(); })));
    edit_context->addEventListener(
        event_type_names::kCompositionend,
        MakeGarbageCollected<CallbackEventListener>(base::BindLambdaForTesting(
            [&](Event*) { edit_context->CleanContextIfNeeded(); })));
    edit_context->addEventListener(
        event_type_names::kContextrequest,
        MakeGarbageCollected<CallbackEventListener>(
            base::BindLambdaForTesting([&](Event*) {
              ++context_request_count;
              DummyExceptionStateForTesting exception_state;
              edit_context->updateText(0, edit_context->text().length(),
                                       "fresh", exception_state);
              edit_context->updateSelection(5, 5, exception_state);
            })));

    EXPECT_TRUE(edit_context->SetComposition("comp", {}, WebRange(), 4, 4));
    EXPECT_TRUE(edit_context->IsContextDirtyForTesting());
    EXPECT_FALSE(edit_context->CompositionRange().IsEmpty());

    EXPECT_TRUE(edit_context->InsertText("!"));
    EXPECT_EQ(context_request_count, 1);
    EXPECT_TRUE(edit_context->CompositionRange().IsEmpty());
    EXPECT_FALSE(edit_context->IsContextDirtyForTesting());
    EXPECT_EQ(edit_context->text(), "fresh!");
  }

  // 3. Calling `invalidate()` inside `textupdate` during `SetComposition` and
  // `SetCompositionFromExistingText` returns `false` without hitting
  // `DCHECK(has_composition_)` in `DispatchTextFormatEvent`, and deletes the
  // in-flight composition text cleanly.
  {
    auto* edit_context = CreateEditContext(script_state, "hello", 5);
    edit_context->addEventListener(
        event_type_names::kTextupdate,
        MakeGarbageCollected<CallbackEventListener>(base::BindLambdaForTesting(
            [&](Event*) { edit_context->invalidate(); })));

    EXPECT_FALSE(edit_context->SetComposition("!", {ui::ImeTextSpan()},
                                              WebRange(), 1, 1));
    EXPECT_EQ(edit_context->text(), "hello");
    EXPECT_EQ(edit_context->selectionStart(), 5u);
    EXPECT_EQ(edit_context->selectionEnd(), 5u);
    EXPECT_TRUE(edit_context->CompositionRange().IsEmpty());
    EXPECT_TRUE(edit_context->IsContextDirtyForTesting());
  }
  {
    auto* edit_context = CreateEditContext(script_state, "hello ", 6);
    ASSERT_TRUE(edit_context->SetComposition("world", {}, WebRange(), 5, 5));
    EXPECT_EQ(edit_context->text(), "hello world");

    edit_context->addEventListener(
        event_type_names::kTextupdate,
        MakeGarbageCollected<CallbackEventListener>(base::BindLambdaForTesting(
            [&](Event*) { edit_context->invalidate(); })));

    // Shortening an active composition before `textupdate` calls `invalidate()`
    // must delete the shortened composition range `[6, 7]` rather than reading
    // the stale `[6, 11]` range.
    EXPECT_FALSE(edit_context->SetComposition("x", {ui::ImeTextSpan()},
                                              WebRange(), 1, 1));
    EXPECT_EQ(edit_context->text(), "hello ");
    EXPECT_EQ(edit_context->selectionStart(), 6u);
    EXPECT_EQ(edit_context->selectionEnd(), 6u);
    EXPECT_TRUE(edit_context->CompositionRange().IsEmpty());
    EXPECT_TRUE(edit_context->IsContextDirtyForTesting());
  }
  {
    auto* edit_context = CreateEditContext(script_state, "hello", 5);
    edit_context->addEventListener(
        event_type_names::kTextupdate,
        MakeGarbageCollected<CallbackEventListener>(base::BindLambdaForTesting(
            [&](Event*) { edit_context->invalidate(); })));

    EXPECT_FALSE(edit_context->SetCompositionFromExistingText(
        0, 5, {ui::ImeTextSpan()}));
    EXPECT_TRUE(edit_context->CompositionRange().IsEmpty());
    EXPECT_TRUE(edit_context->IsContextDirtyForTesting());
  }
}

TEST_F(EditContextTest,
       DeleteSurroundingTextInCodePointsDispatchesSingleContextRequest) {
  ScopedEditContextOnDemandForTest scoped_feature(true);
  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
  ScriptState::Scope script_scope(script_state);

  // 1. 8-bit fast path: even if `contextrequest` re-invalidates the context,
  // `DeleteSurroundingTextInCodePoints` must not fire a second `contextrequest`
  // when delegating to `DeleteSurroundingTextInternal`.
  {
    auto* edit_context = CreateEditContext(script_state, "", 0);
    int context_request_count = 0;
    edit_context->addEventListener(
        event_type_names::kContextrequest,
        MakeGarbageCollected<CallbackEventListener>(
            base::BindLambdaForTesting([&](Event*) {
              ++context_request_count;
              DummyExceptionStateForTesting exception_state;
              edit_context->updateText(0, edit_context->text().length(), "abc",
                                       exception_state);
              edit_context->updateSelection(2, 2, exception_state);
              edit_context->invalidate();
            })));

    edit_context->invalidate();
    edit_context->DeleteSurroundingTextInCodePoints(1, 0);
    EXPECT_EQ(context_request_count, 1);
    EXPECT_EQ(edit_context->text(), "ac");
    EXPECT_EQ(edit_context->selectionStart(), 1u);
    EXPECT_EQ(edit_context->selectionEnd(), 1u);
    EXPECT_TRUE(edit_context->IsContextDirtyForTesting());
  }

  // 2. UTF-16 surrogate pair path (`u"a\U0001F600b"`, selection at offset 3
  // right after the 2-code-unit emoji).
  {
    auto* edit_context = CreateEditContext(script_state, "", 0);
    int context_request_count = 0;
    edit_context->addEventListener(
        event_type_names::kContextrequest,
        MakeGarbageCollected<CallbackEventListener>(
            base::BindLambdaForTesting([&](Event*) {
              ++context_request_count;
              DummyExceptionStateForTesting exception_state;
              edit_context->updateText(0, edit_context->text().length(),
                                       String(u"a\U0001F600b"),
                                       exception_state);
              edit_context->updateSelection(3, 3, exception_state);
              edit_context->invalidate();
            })));

    edit_context->invalidate();
    edit_context->DeleteSurroundingTextInCodePoints(1, 0);
    EXPECT_EQ(context_request_count, 1);
    EXPECT_EQ(edit_context->text(), "ab");
    EXPECT_EQ(edit_context->selectionStart(), 1u);
    EXPECT_EQ(edit_context->selectionEnd(), 1u);
    EXPECT_TRUE(edit_context->IsContextDirtyForTesting());
  }
}

TEST_F(EditContextTest,
       ClampsOutOfBoundsSelectionAndStaleReplacementRangeAfterContextRequest) {
  ScopedEditContextOnDemandForTest scoped_feature(true);
  // Match production Chrome where `updateText()` does not adjust selection
  // offsets when shrinking text.
  ScopedEditContextHandleTextOrSelectionUpdateDuringCompositionForTest
      scoped_update_during_composition(false);
  SetBodyInnerHTML("<div id='editor'></div>");
  auto* editor =
      To<HTMLElement>(GetDocument().getElementById(AtomicString("editor")));
  ASSERT_NE(editor, nullptr);

  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
  ScriptState::Scope script_scope(script_state);

  // 1. `contextrequest` shrinks `text` via `updateText(0, 10, "hi")` without
  // calling `updateSelection()`, leaving selection at 10 until
  // `CleanContextIfNeeded()` clamps it.
  {
    auto* edit_context = CreateEditContext(script_state, "0123456789", 10, 10);
    edit_context->addEventListener(
        event_type_names::kContextrequest,
        MakeGarbageCollected<CallbackEventListener>(
            base::BindLambdaForTesting([&](Event*) {
              DummyExceptionStateForTesting exception_state;
              edit_context->updateText(0, edit_context->text().length(), "hi",
                                       exception_state);
            })));

    edit_context->invalidate();
    EXPECT_TRUE(edit_context->InsertText("!"));
    EXPECT_EQ(edit_context->text(), "hi!");
    EXPECT_EQ(edit_context->selectionStart(), 3u);
    EXPECT_EQ(edit_context->selectionEnd(), 3u);

    edit_context->DeleteSurroundingText(1, 0);
    EXPECT_EQ(edit_context->text(), "hi");
    EXPECT_EQ(edit_context->selectionStart(), 2u);
    EXPECT_EQ(edit_context->selectionEnd(), 2u);

    DummyExceptionStateForTesting exception_state;
    edit_context->updateText(0, edit_context->text().length(), "0123456789",
                             exception_state);
    edit_context->updateSelection(10, 10, exception_state);
    edit_context->invalidate();
    edit_context->DeleteSurroundingText(1, 0);
    EXPECT_EQ(edit_context->text(), "h");
    EXPECT_EQ(edit_context->selectionStart(), 1u);
    EXPECT_EQ(edit_context->selectionEnd(), 1u);
  }

  // 2. Pre-invalidation explicit `replacement_range` offsets in `CommitText`
  // and `SetComposition` bail out after pulling fresh context rather than
  // splicing stale offsets onto a different serialized window ("cat dog" ->
  // "dog cat"), while out-of-bounds `replacement_range` on a clean buffer is
  // clamped safely.
  {
    DummyExceptionStateForTesting attach_exception_state;
    auto* edit_context = CreateEditContext(script_state, "cat dog", 7, 7);
    editor->setEditContext(edit_context, attach_exception_state);
    editor->Focus();
    edit_context->addEventListener(
        event_type_names::kContextrequest,
        MakeGarbageCollected<CallbackEventListener>(
            base::BindLambdaForTesting([&](Event*) {
              DummyExceptionStateForTesting exception_state;
              edit_context->updateText(0, edit_context->text().length(),
                                       "dog cat", exception_state);
              edit_context->updateSelection(7, 7, exception_state);
            })));

    edit_context->invalidate();
    EXPECT_FALSE(edit_context->CommitText("fox", {}, WebRange(4, 3), 0));
    EXPECT_FALSE(edit_context->IsContextDirtyForTesting());
    EXPECT_EQ(edit_context->text(), "dog cat");

    edit_context->invalidate();
    EXPECT_FALSE(edit_context->SetComposition("fox", {}, WebRange(4, 3), 3, 3));
    EXPECT_FALSE(edit_context->IsContextDirtyForTesting());
    EXPECT_EQ(edit_context->text(), "dog cat");

    // Once clean, an out-of-bounds `replacement_range` (e.g. after `updateText`
    // shortened `text_`) clamps safely without crashing.
    DummyExceptionStateForTesting exception_state;
    edit_context->updateText(0, edit_context->text().length(), "hi",
                             exception_state);
    edit_context->updateSelection(2, 2, exception_state);
    EXPECT_TRUE(edit_context->CommitText("!", {}, WebRange(8, 2), 0));
    EXPECT_EQ(edit_context->text(), "hi!");
    EXPECT_EQ(edit_context->selectionStart(), 3u);
    EXPECT_EQ(edit_context->selectionEnd(), 3u);

    EXPECT_TRUE(edit_context->SetComposition("?", {}, WebRange(8, 2), 1, 1));
    EXPECT_EQ(edit_context->text(), "hi!?");
    EXPECT_EQ(edit_context->CompositionRange().StartOffset(), 3);
    EXPECT_EQ(edit_context->CompositionRange().EndOffset(), 4);
  }
}

TEST_F(EditContextTest, BlurringElementInsideContextRequestDoesNotCrash) {
  ScopedEditContextOnDemandForTest scoped_feature(true);
  SetBodyInnerHTML("<div id='editor'></div>");
  auto* editor =
      To<HTMLElement>(GetDocument().getElementById(AtomicString("editor")));
  ASSERT_NE(editor, nullptr);

  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
  ScriptState::Scope script_scope(script_state);
  auto* edit_context = CreateEditContext(script_state, "", 0);
  DummyExceptionStateForTesting exception_state;
  editor->setEditContext(edit_context, exception_state);
  editor->Focus();
  ASSERT_EQ(GetFrame().GetInputMethodController().GetActiveEditContext(),
            edit_context);

  edit_context->addEventListener(
      event_type_names::kContextrequest,
      MakeGarbageCollected<CallbackEventListener>(
          base::BindLambdaForTesting([&](Event*) {
            DummyExceptionStateForTesting inner_exception_state;
            edit_context->updateText(0, edit_context->text().length(), "abc",
                                     inner_exception_state);
            edit_context->updateSelection(3, 3, inner_exception_state);
            editor->blur();
          })));

  edit_context->invalidate();
  EXPECT_TRUE(edit_context->InsertText("x"));
  EXPECT_EQ(GetFrame().GetInputMethodController().GetActiveEditContext(),
            nullptr);
  EXPECT_EQ(edit_context->text(), "abcx");

  editor->Focus();
  ASSERT_EQ(GetFrame().GetInputMethodController().GetActiveEditContext(),
            edit_context);
  edit_context->invalidate();
  EXPECT_TRUE(edit_context->SetComposition("y", {}, WebRange(), 1, 1));
  EXPECT_EQ(GetFrame().GetInputMethodController().GetActiveEditContext(),
            nullptr);
  EXPECT_EQ(edit_context->text(), "abcy");
}

TEST_F(EditContextTest,
       BlurringElementInsideTextUpdateDuringCompositionTeardownDoesNotCrash) {
  ScopedEditContextOnDemandForTest scoped_feature(true);
  SetBodyInnerHTML("<div id='editor'></div>");
  auto* editor =
      To<HTMLElement>(GetDocument().getElementById(AtomicString("editor")));
  ASSERT_NE(editor, nullptr);

  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
  ScriptState::Scope script_scope(script_state);
  DummyExceptionStateForTesting exception_state;

  // 1. `editor->blur()` inside `textupdate` during `invalidate()` ->
  // `OnCancelComposition()` must not clear `has_composition_` prematurely via
  // `Blur()` -> `FinishComposingText()`.
  {
    auto* edit_context = CreateEditContext(script_state, "hello ", 6);
    editor->setEditContext(edit_context, exception_state);
    editor->Focus();
    ASSERT_TRUE(edit_context->SetComposition("world", {}, WebRange(), 5, 5));

    int composition_end_count = 0;
    edit_context->addEventListener(
        event_type_names::kTextupdate,
        MakeGarbageCollected<CallbackEventListener>(
            base::BindLambdaForTesting([&](Event*) { editor->blur(); })));
    edit_context->addEventListener(
        event_type_names::kCompositionend,
        MakeGarbageCollected<CallbackEventListener>(base::BindLambdaForTesting(
            [&](Event*) { ++composition_end_count; })));

    edit_context->invalidate();
    EXPECT_EQ(composition_end_count, 1);
    EXPECT_EQ(edit_context->text(), "hello ");
    EXPECT_TRUE(edit_context->CompositionRange().IsEmpty());
    EXPECT_TRUE(edit_context->IsContextDirtyForTesting());
  }

  // 2. `editor->blur()` inside `textupdate` during `CommitText()` with an
  // active composition.
  {
    auto* edit_context = CreateEditContext(script_state, "hello ", 6);
    editor->setEditContext(edit_context, exception_state);
    editor->Focus();
    ASSERT_TRUE(edit_context->SetComposition("comp", {}, WebRange(), 4, 4));

    int composition_end_count = 0;
    edit_context->addEventListener(
        event_type_names::kTextupdate,
        MakeGarbageCollected<CallbackEventListener>(
            base::BindLambdaForTesting([&](Event*) { editor->blur(); })));
    edit_context->addEventListener(
        event_type_names::kCompositionend,
        MakeGarbageCollected<CallbackEventListener>(base::BindLambdaForTesting(
            [&](Event*) { ++composition_end_count; })));

    EXPECT_TRUE(edit_context->CommitText("done", {}, WebRange(), 0));
    EXPECT_EQ(composition_end_count, 1);
    EXPECT_EQ(edit_context->text(), "hello done");
    EXPECT_TRUE(edit_context->CompositionRange().IsEmpty());
  }
}

TEST_F(EditContextTest, SetSelectionFromIMEPullsCleanContextAndClampsOffsets) {
  ScopedEditContextOnDemandForTest scoped_feature(true);
  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
  ScriptState::Scope script_scope(script_state);
  auto* edit_context = CreateEditContext(script_state, "0123456789", 10, 10);

  int context_request_count = 0;
  int text_update_count = 0;
  edit_context->addEventListener(
      event_type_names::kContextrequest,
      MakeGarbageCollected<CallbackEventListener>(
          base::BindLambdaForTesting([&](Event*) {
            ++context_request_count;
            DummyExceptionStateForTesting exception_state;
            edit_context->updateText(0, edit_context->text().length(), "hi",
                                     exception_state);
            edit_context->updateSelection(0, 0, exception_state);
          })));
  edit_context->addEventListener(
      event_type_names::kTextupdate,
      MakeGarbageCollected<CallbackEventListener>(
          base::BindLambdaForTesting([&](Event*) { ++text_update_count; })));

  edit_context->invalidate();
  // When dirty, `SetSelectionFromIME` pulls fresh context and bails out rather
  // than applying stale pre-invalidation offsets `(8, 8)`.
  edit_context->SetSelectionFromIME(8, 8);
  EXPECT_EQ(context_request_count, 1);
  EXPECT_EQ(text_update_count, 0);
  EXPECT_FALSE(edit_context->IsContextDirtyForTesting());
  EXPECT_EQ(edit_context->selectionStart(), 0u);
  EXPECT_EQ(edit_context->selectionEnd(), 0u);

  // Once clean, `SetSelectionFromIME` clamps `(8, 8)` to `(2, 2)` and
  // dispatches `textupdate`.
  edit_context->SetSelectionFromIME(8, 8);
  EXPECT_EQ(context_request_count, 1);
  EXPECT_EQ(text_update_count, 1);
  EXPECT_EQ(edit_context->selectionStart(), 2u);
  EXPECT_EQ(edit_context->selectionEnd(), 2u);

  // Calling `SetSelectionFromIME` with unchanged offsets is a no-op and does
  // not dispatch a duplicate `textupdate`.
  edit_context->SetSelectionFromIME(2, 2);
  EXPECT_EQ(text_update_count, 1);

  EXPECT_TRUE(edit_context->InsertText("!"));
  EXPECT_EQ(context_request_count, 1);
  EXPECT_EQ(edit_context->text(), "hi!");
}

TEST_F(EditContextTest,
       UpdateTextBeforeInvalidateWithCompositionUpdateFlagDisabled) {
  ScopedEditContextOnDemandForTest scoped_on_demand(true);
  ScopedEditContextHandleTextOrSelectionUpdateDuringCompositionForTest
      scoped_update_during_composition(false);
  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
  ScriptState::Scope script_scope(script_state);
  DummyExceptionStateForTesting exception_state;

  // 1. Inserting text before the active composition shifts the composition
  // range so `invalidate()` cancels "world" rather than deleting "hello".
  {
    auto* edit_context = CreateEditContext(script_state, "hello ", 6);
    ASSERT_TRUE(edit_context->SetComposition("world", {}, WebRange(), 5, 5));
    EXPECT_EQ(edit_context->text(), "hello world");

    edit_context->updateText(0, 0, "prefix ", exception_state);
    EXPECT_EQ(edit_context->text(), "prefix hello world");

    edit_context->invalidate();
    EXPECT_EQ(edit_context->text(), "prefix hello ");
    EXPECT_TRUE(edit_context->CompositionRange().IsEmpty());
    EXPECT_TRUE(edit_context->IsContextDirtyForTesting());
  }

  // 2. Deleting text before the active composition shifts the composition
  // range downward without slicing out of bounds during `invalidate()`.
  {
    auto* edit_context = CreateEditContext(script_state, "hello ", 6);
    ASSERT_TRUE(edit_context->SetComposition("world", {}, WebRange(), 5, 5));
    EXPECT_EQ(edit_context->text(), "hello world");

    edit_context->updateText(0, 6, "", exception_state);
    EXPECT_EQ(edit_context->text(), "world");

    edit_context->invalidate();
    EXPECT_EQ(edit_context->text(), "");
    EXPECT_TRUE(edit_context->CompositionRange().IsEmpty());
    EXPECT_TRUE(edit_context->IsContextDirtyForTesting());
  }

  // 3. Updating text overlapping the active composition aborts the composition
  // cleanly before applying the update, and subsequent `invalidate()` marks the
  // context dirty without double-canceling.
  {
    auto* edit_context = CreateEditContext(script_state, "hello ", 6);
    ASSERT_TRUE(edit_context->SetComposition("world", {}, WebRange(), 5, 5));
    int composition_end_count = 0;
    edit_context->addEventListener(
        event_type_names::kCompositionend,
        MakeGarbageCollected<CallbackEventListener>(base::BindLambdaForTesting(
            [&](Event*) { ++composition_end_count; })));

    edit_context->updateText(4, 8, "X", exception_state);
    EXPECT_EQ(composition_end_count, 1);
    EXPECT_TRUE(edit_context->CompositionRange().IsEmpty());
    EXPECT_EQ(edit_context->text(), "hellX");

    edit_context->invalidate();
    EXPECT_EQ(composition_end_count, 1);
    EXPECT_EQ(edit_context->text(), "hellX");
    EXPECT_TRUE(edit_context->IsContextDirtyForTesting());
  }
}

TEST_F(EditContextTest,
       InvalidateInTextFormatUpdateSkipsCharacterBoundsUpdate) {
  ScopedEditContextOnDemandForTest scoped_feature(true);
  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
  ScriptState::Scope script_scope(script_state);

  {
    auto* edit_context = CreateEditContext(script_state, "hello ", 6);
    int char_bounds_count = 0;
    edit_context->addEventListener(
        event_type_names::kTextformatupdate,
        MakeGarbageCollected<CallbackEventListener>(base::BindLambdaForTesting(
            [&](Event*) { edit_context->invalidate(); })));
    edit_context->addEventListener(
        event_type_names::kCharacterboundsupdate,
        MakeGarbageCollected<CallbackEventListener>(
            base::BindLambdaForTesting([&](Event*) { ++char_bounds_count; })));

    EXPECT_FALSE(edit_context->SetComposition("world", {ui::ImeTextSpan()},
                                              WebRange(), 5, 5));
    EXPECT_EQ(char_bounds_count, 0);
    EXPECT_EQ(edit_context->text(), "hello ");
    EXPECT_TRUE(edit_context->CompositionRange().IsEmpty());
    EXPECT_TRUE(edit_context->IsContextDirtyForTesting());
  }

  {
    auto* edit_context = CreateEditContext(script_state, "hello", 5);
    int char_bounds_count = 0;
    edit_context->addEventListener(
        event_type_names::kTextformatupdate,
        MakeGarbageCollected<CallbackEventListener>(base::BindLambdaForTesting(
            [&](Event*) { edit_context->invalidate(); })));
    edit_context->addEventListener(
        event_type_names::kCharacterboundsupdate,
        MakeGarbageCollected<CallbackEventListener>(
            base::BindLambdaForTesting([&](Event*) { ++char_bounds_count; })));

    EXPECT_FALSE(edit_context->SetCompositionFromExistingText(
        0, 5, {ui::ImeTextSpan()}));
    EXPECT_EQ(char_bounds_count, 0);
    EXPECT_TRUE(edit_context->CompositionRange().IsEmpty());
    EXPECT_TRUE(edit_context->IsContextDirtyForTesting());
  }
}

TEST_F(EditContextTest,
       FlagDisabledPreservesLegacyCompositionAndSelectionBehavior) {
  ScopedEditContextOnDemandForTest scoped_feature(false);
  SetBodyInnerHTML("<div id='editor'></div>");
  auto* editor =
      To<HTMLElement>(GetDocument().getElementById(AtomicString("editor")));
  ASSERT_NE(editor, nullptr);

  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
  ScriptState::Scope script_scope(script_state);
  DummyExceptionStateForTesting exception_state;

  // 1. With `EditContextOnDemand` disabled, `SetComposition()` updates
  // `composition_range` after dispatching `textupdate` rather than before.
  {
    auto* edit_context = CreateEditContext(script_state, "hello ", 6);
    editor->setEditContext(edit_context, exception_state);
    editor->Focus();
    WebRange range_during_textupdate;
    edit_context->addEventListener(event_type_names::kTextupdate,
                                   MakeGarbageCollected<CallbackEventListener>(
                                       base::BindLambdaForTesting([&](Event*) {
                                         range_during_textupdate =
                                             edit_context->CompositionRange();
                                       })));

    EXPECT_TRUE(edit_context->SetComposition("world", {}, WebRange(), 5, 5));
    EXPECT_TRUE(range_during_textupdate.IsEmpty());
    EXPECT_EQ(edit_context->CompositionRange().StartOffset(), 6);
    EXPECT_EQ(edit_context->CompositionRange().EndOffset(), 11);
  }

  // 2. With `EditContextOnDemand` disabled, calling `editor->blur()` inside
  // `CommitText()`'s `textupdate` listener finishes the composition
  // synchronously inside `blur()` rather than deferring until `CommitText()`
  // exits.
  {
    auto* edit_context = CreateEditContext(script_state, "hello ", 6);
    editor->setEditContext(edit_context, exception_state);
    editor->Focus();
    ASSERT_TRUE(edit_context->SetComposition("comp", {}, WebRange(), 4, 4));

    bool finished_inside_blur = false;
    int composition_end_count = 0;
    edit_context->addEventListener(
        event_type_names::kCompositionend,
        MakeGarbageCollected<CallbackEventListener>(base::BindLambdaForTesting(
            [&](Event*) { ++composition_end_count; })));
    edit_context->addEventListener(event_type_names::kTextupdate,
                                   MakeGarbageCollected<CallbackEventListener>(
                                       base::BindLambdaForTesting([&](Event*) {
                                         editor->blur();
                                         finished_inside_blur =
                                             (composition_end_count == 1);
                                       })));

    EXPECT_TRUE(edit_context->CommitText("done", {}, WebRange(), 0));
    EXPECT_TRUE(finished_inside_blur);
    EXPECT_EQ(composition_end_count, 1);
  }

  // 3. With `EditContextOnDemand` disabled, `SetSelectionFromIME` preserves
  // unclamped selection behavior.
  {
    auto* edit_context = CreateEditContext(script_state, "hi", 2, 2);
    int text_update_count = 0;
    edit_context->addEventListener(
        event_type_names::kTextupdate,
        MakeGarbageCollected<CallbackEventListener>(
            base::BindLambdaForTesting([&](Event*) { ++text_update_count; })));
    edit_context->SetSelectionFromIME(8, 8);
    EXPECT_EQ(text_update_count, 1);
    EXPECT_EQ(edit_context->selectionStart(), 8u);
    EXPECT_EQ(edit_context->selectionEnd(), 8u);
  }
}

TEST_F(EditContextTest, CommitTextHandlesDetachAndDeactivationInContextRequest) {
  SetBodyInnerHTML("<div id='editor'></div>");
  auto* editor =
      To<HTMLElement>(GetDocument().getElementById(AtomicString("editor")));
  ASSERT_NE(editor, nullptr);

  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
  ScriptState::Scope script_scope(script_state);
  DummyExceptionStateForTesting exception_state;

  // 1. Detaching the hosting `<iframe>` during `contextrequest` detaches the
  // frame (`DomWindow()->GetFrame() == nullptr`); `CommitText()` must bail out
  // cleanly.
  {
    frame_test_helpers::WebViewHelper web_view_helper;
    web_view_helper.Initialize();
    LocalFrame* main_frame = web_view_helper.LocalMainFrame()->GetFrame();
    main_frame->GetDocument()->body()->SetInnerHTMLWithoutTrustedTypes(
        "<iframe id='sub'></iframe>");
    main_frame->GetDocument()->UpdateStyleAndLayout(
        DocumentUpdateReason::kTest);

    auto* iframe = To<HTMLIFrameElement>(
        main_frame->GetDocument()->getElementById(AtomicString("sub")));
    ASSERT_NE(iframe, nullptr);
    auto* sub_frame = To<LocalFrame>(iframe->ContentFrame());
    ASSERT_NE(sub_frame, nullptr);
    sub_frame->GetDocument()->body()->SetInnerHTMLWithoutTrustedTypes(
        "<div id='sub_editor'></div>");
    auto* sub_editor = To<HTMLElement>(
        sub_frame->GetDocument()->getElementById(AtomicString("sub_editor")));
    ASSERT_NE(sub_editor, nullptr);

    ScriptState* sub_script_state = ToScriptStateForMainWorld(sub_frame);
    ScriptState::Scope sub_scope(sub_script_state);
    auto* sub_ec = CreateEditContext(sub_script_state, "hello ", 6);
    sub_editor->setEditContext(sub_ec, exception_state);
    sub_editor->Focus();
    sub_ec->invalidate();

    int text_update_count = 0;
    sub_ec->addEventListener(
        event_type_names::kContextrequest,
        MakeGarbageCollected<CallbackEventListener>(
            base::BindLambdaForTesting([&](Event*) { iframe->remove(); })));
    sub_ec->addEventListener(
        event_type_names::kTextupdate,
        MakeGarbageCollected<CallbackEventListener>(
            base::BindLambdaForTesting([&](Event*) { ++text_update_count; })));

    EXPECT_FALSE(sub_ec->CommitText("world", {}, WebRange(), 0));
    EXPECT_FALSE(sub_frame->IsAttached());
    EXPECT_EQ(text_update_count, 0);
  }

  // 2. Moving `#main_editor` into a detached iframe's document during
  // `contextrequest` clears `DomWindow()` to null (`Element::InsertedInto` ->
  // `SetExecutionContext`); `CommitText()` must bail out without dereferencing
  // `DomWindow()->document()`.
  {
    frame_test_helpers::WebViewHelper web_view_helper;
    web_view_helper.Initialize();
    LocalFrame* main_frame = web_view_helper.LocalMainFrame()->GetFrame();
    main_frame->GetDocument()->body()->SetInnerHTMLWithoutTrustedTypes(
        "<div id='main_editor'></div><iframe id='detached_sub'></iframe>");
    main_frame->GetDocument()->UpdateStyleAndLayout(
        DocumentUpdateReason::kTest);

    auto* main_editor = To<HTMLElement>(
        main_frame->GetDocument()->getElementById(AtomicString("main_editor")));
    auto* detached_iframe =
        To<HTMLIFrameElement>(main_frame->GetDocument()->getElementById(
            AtomicString("detached_sub")));
    ASSERT_NE(main_editor, nullptr);
    ASSERT_NE(detached_iframe, nullptr);
    Document* detached_doc =
        To<LocalFrame>(detached_iframe->ContentFrame())->GetDocument();
    ASSERT_NE(detached_doc, nullptr);
    detached_iframe->remove();
    ASSERT_EQ(detached_doc->GetExecutionContext(), nullptr);

    ScriptState* main_script_state = ToScriptStateForMainWorld(main_frame);
    ScriptState::Scope main_scope(main_script_state);
    auto* edit_context = CreateEditContext(main_script_state, "hello ", 6);
    main_editor->setEditContext(edit_context, exception_state);
    main_editor->Focus();
    edit_context->invalidate();

    int beforeinput_count = 0;
    int text_update_count = 0;
    main_editor->addEventListener(
        event_type_names::kBeforeinput,
        MakeGarbageCollected<CallbackEventListener>(
            base::BindLambdaForTesting([&](Event*) { ++beforeinput_count; })));
    edit_context->addEventListener(
        event_type_names::kContextrequest,
        MakeGarbageCollected<CallbackEventListener>(base::BindLambdaForTesting(
            [&](Event*) { detached_doc->body()->AppendChild(main_editor); })));
    edit_context->addEventListener(
        event_type_names::kTextupdate,
        MakeGarbageCollected<CallbackEventListener>(
            base::BindLambdaForTesting([&](Event*) { ++text_update_count; })));

    EXPECT_FALSE(edit_context->CommitText("world", {}, WebRange(), 0));
    EXPECT_EQ(edit_context->DomWindow(), nullptr);
    EXPECT_EQ(beforeinput_count, 0);
    EXPECT_EQ(text_update_count, 0);
  }

  // 3. Blurring `#editor` inside `contextrequest` deactivates the EditContext;
  // `CommitText()` outside composition bails out without firing `beforeinput`
  // or `textupdate`.
  {
    auto* edit_context = CreateEditContext(script_state, "hello ", 6);
    editor->setEditContext(edit_context, exception_state);
    editor->Focus();
    edit_context->invalidate();

    int beforeinput_count = 0;
    int text_update_count = 0;
    editor->addEventListener(
        event_type_names::kBeforeinput,
        MakeGarbageCollected<CallbackEventListener>(
            base::BindLambdaForTesting([&](Event*) { ++beforeinput_count; })));
    edit_context->addEventListener(
        event_type_names::kContextrequest,
        MakeGarbageCollected<CallbackEventListener>(
            base::BindLambdaForTesting([&](Event*) { editor->blur(); })));
    edit_context->addEventListener(
        event_type_names::kTextupdate,
        MakeGarbageCollected<CallbackEventListener>(
            base::BindLambdaForTesting([&](Event*) { ++text_update_count; })));

    EXPECT_FALSE(edit_context->CommitText("world", {}, WebRange(), 0));
    EXPECT_EQ(beforeinput_count, 0);
    EXPECT_EQ(text_update_count, 0);
  }
}

TEST_F(EditContextTest,
       CommitTextDispatchesSingleContextRequestWhenHandlerReInvalidates) {
  SetBodyInnerHTML("<div id='editor'></div>");
  auto* editor =
      To<HTMLElement>(GetDocument().getElementById(AtomicString("editor")));
  ASSERT_NE(editor, nullptr);

  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
  ScriptState::Scope script_scope(script_state);
  DummyExceptionStateForTesting exception_state;

  // 1. Pre-commit invalidation whose `contextrequest` handler re-invalidates
  // dispatches `contextrequest` only once across the entire `CommitText()`.
  {
    auto* edit_context = CreateEditContext(script_state, "base ", 5);
    editor->setEditContext(edit_context, exception_state);
    editor->Focus();
    edit_context->invalidate();

    int context_request_count = 0;
    edit_context->addEventListener(event_type_names::kContextrequest,
                                   MakeGarbageCollected<CallbackEventListener>(
                                       base::BindLambdaForTesting([&](Event*) {
                                         ++context_request_count;
                                         edit_context->invalidate();
                                       })));

    EXPECT_TRUE(edit_context->CommitText("done", {}, WebRange(), 0));
    EXPECT_EQ(context_request_count, 1);
    EXPECT_EQ(edit_context->text(), "base done");
    EXPECT_TRUE(edit_context->IsContextDirtyForTesting());
  }

  // 2. `beforeinput` invalidation whose `contextrequest` handler re-invalidates
  // dispatches `contextrequest` only once after `beforeinput` (not a second
  // time inside `InsertText`).
  {
    auto* edit_context = CreateEditContext(script_state, "start ", 6);
    editor->setEditContext(edit_context, exception_state);
    editor->Focus();

    int context_request_count = 0;
    editor->addEventListener(
        event_type_names::kBeforeinput,
        MakeGarbageCollected<CallbackEventListener>(base::BindLambdaForTesting(
            [&](Event*) { edit_context->invalidate(); })));
    edit_context->addEventListener(
        event_type_names::kContextrequest,
        MakeGarbageCollected<CallbackEventListener>(
            base::BindLambdaForTesting([&](Event*) {
              ++context_request_count;
              edit_context->updateText(0, edit_context->text().length(),
                                       "updated ", exception_state);
              edit_context->updateSelection(8, 8, exception_state);
              edit_context->invalidate();
            })));

    EXPECT_TRUE(edit_context->CommitText("end", {}, WebRange(), 0));
    EXPECT_EQ(context_request_count, 1);
    EXPECT_EQ(edit_context->text(), "updated end");
    EXPECT_TRUE(edit_context->IsContextDirtyForTesting());
    editor->setEditContext(nullptr, exception_state);
  }
}

}  // namespace blink
