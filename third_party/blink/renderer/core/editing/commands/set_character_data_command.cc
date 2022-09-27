// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/commands/set_character_data_command.h"

#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"

namespace blink {

SetCharacterDataCommand::SetCharacterDataCommand(Text* node,
                                                 unsigned offset,
                                                 unsigned count,
                                                 const String& text)
    : SimpleEditCommand(node->GetDocument()),
      node_(node),
      offset_(offset),
      count_(count),
      new_text_(text) {
  DCHECK(node_);
  DCHECK_LE(offset_, node_->length());
  DCHECK_LE(offset_ + count_, node_->length());
  // Callers shouldn't be trying to perform no-op replacements
  DCHECK(!(count == 0 && text.length() == 0));
}

void SetCharacterDataCommand::DoApply(EditingState*) {
  // TODO(editing-dev): The use of updateStyleAndLayoutTree()
  // needs to be audited.  See http://crbug.com/590369 for more details.
  GetDocument().UpdateStyleAndLayoutTree();
  if (!IsEditable(*node_))
    return;

  DummyExceptionStateForTesting exception_state;
  previous_text_for_undo_ =
      node_->substringData(offset_, count_, exception_state);
  if (exception_state.HadException())
    return;

  const bool password_echo_enabled =
      GetDocument().GetSettings() &&
      GetDocument().GetSettings()->GetPasswordEchoEnabled();

  if (password_echo_enabled) {
    LayoutText* layout_text = node_->GetLayoutObject();
    if (layout_text && layout_text->IsSecure()) {
      layout_text->MomentarilyRevealLastTypedCharacter(offset_ +
                                                       new_text_.length() - 1);
    }
  }

  node_->replaceData(offset_, count_, new_text_, IGNORE_EXCEPTION_FOR_TESTING);
}

void SetCharacterDataCommand::DoUnapply() {
  // TODO(editing-dev): The use of updateStyleAndLayoutTree()
  // needs to be audited.  See http://crbug.com/590369 for more details.
  GetDocument().UpdateStyleAndLayoutTree();
  if (!IsEditable(*node_))
    return;

  node_->replaceData(offset_, new_text_.length(), previous_text_for_undo_,
                     IGNORE_EXCEPTION_FOR_TESTING);
}

void SetCharacterDataCommand::Trace(Visitor* visitor) const {
  visitor->Trace(node_);
  SimpleEditCommand::Trace(visitor);
}

}  // namespace blink
