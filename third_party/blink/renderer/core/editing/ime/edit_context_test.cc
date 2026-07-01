// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/ime/edit_context.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_edit_context_init.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

class EditContextTest : public EditingTestBase {
 protected:
  EditContext* CreateEditContext(ScriptState* script_state,
                                 const String& text,
                                 uint32_t caret_pos) {
    EditContextInit* init = EditContextInit::Create();
    init->setText(text);
    init->setSelectionStart(caret_pos);
    init->setSelectionEnd(caret_pos);
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

}  // namespace blink
