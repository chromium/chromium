/*
 * Copyright (C) 2006, 2007 Apple, Inc.  All rights reserved.
 * Copyright (C) 2012 Google, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/editing/editor.h"

#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/editing/commands/editing_command_filter.h"
#include "third_party/blink/renderer/core/editing/commands/editor_command.h"
#include "third_party/blink/renderer/core/editing/editing_behavior.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/ime/edit_context.h"
#include "third_party/blink/renderer/core/editing/ime/input_method_controller.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

bool Editor::HandleEditingKeyboardEvent(KeyboardEvent* evt) {
  const WebKeyboardEvent* key_event = evt->KeyEvent();
  if (!key_event)
    return false;

  WritingMode writing_mode = WritingMode::kHorizontalTb;
  const Node* node =
      frame_->Selection().GetSelectionInDOMTree().Focus().AnchorNode();
  if (!node) {
    node = frame_->GetDocument()->FocusedElement();
  }
  if (node) {
    if (const ComputedStyle* style = node->GetComputedStyle()) {
      writing_mode = style->GetWritingMode();
    }
  }
  String command_name = Behavior().InterpretKeyEvent(*evt, writing_mode);
  if (IsCommandFilteredOut(command_name)) {
    return false;
  }

  const EditorCommand command = CreateCommand(command_name);

  if (key_event->GetType() == WebInputEvent::Type::kRawKeyDown) {
    // WebKit doesn't have enough information about mode to decide how
    // commands that just insert text if executed via Editor should be treated,
    // so we leave it upon WebCore to either handle them immediately
    // (e.g. Tab that changes focus) or let a keypress event be generated
    // (e.g. Tab that inserts a Tab character, or Enter).
    if (command.IsTextInsertion() || command_name.empty())
      return false;
    return command.Execute(evt);
  }

  if (command.Execute(evt))
    return true;

  if (!Behavior().ShouldInsertCharacter(*evt))
    return false;

  // If EditContext is active, redirect text to EditContext, otherwise, send
  // text to the focused element.
  if (auto* edit_context =
          GetFrame().GetInputMethodController().GetActiveEditContext()) {
    if (DispatchBeforeInputInsertText(evt->target()->ToNode(),
                                      key_event->text.data()) !=
        DispatchEventResult::kNotCanceled) {
      return true;
    }

    WebString text(WTF::String(key_event->text.data()));
    edit_context->InsertText(text);
    return true;
  }

  if (!CanEdit())
    return false;

  const Element* const focused_element =
      frame_->GetDocument()->FocusedElement();
  if (!focused_element) {
    // We may lose focused element by |command.execute(evt)|.
    return false;
  }
  // We should not insert text at selection start if selection doesn't have
  // focus.
  if (!frame_->Selection().SelectionHasFocus())
    return false;

  // Return true to prevent default action. e.g. Space key scroll.
  if (DispatchBeforeInputInsertText(evt->target()->ToNode(),
                                    key_event->text.data()) !=
      DispatchEventResult::kNotCanceled) {
    return true;
  }

  return InsertText(key_event->text.data(), evt);
}

void Editor::HandleKeyboardEvent(KeyboardEvent* evt) {
  // Give the embedder a chance to handle the keyboard event.
  if (frame_->Client()->HandleCurrentKeyboardEvent() ||
      HandleEditingKeyboardEvent(evt)) {
    evt->SetDefaultHandled();
  }
}

}  // namespace blink
