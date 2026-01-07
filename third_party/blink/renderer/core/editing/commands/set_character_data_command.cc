// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/commands/set_character_data_command.h"

#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"

namespace blink {

SetCharacterDataCommand::SetCharacterDataCommand(
    Text* node,
    unsigned offset,
    unsigned count,
    const String& text,
    PasswordEchoBehavior password_echo_behavior)
    : SimpleEditCommand(node->GetDocument()),
      node_(node),
      offset_(offset),
      count_(count),
      new_text_(text),
      password_echo_behavior_(password_echo_behavior) {
  DCHECK(node_);
  DCHECK_LE(offset_, node_->length());
  DCHECK_LE(offset_ + count_, node_->length());
  // Callers shouldn't be trying to perform no-op replacements
  DCHECK(!(count == 0 && text.length() == 0));
}

bool SetCharacterDataCommand::ShouldEchoPassword() const {
  const Settings* settings = GetDocument().GetSettings();
  switch (password_echo_behavior_) {
    case PasswordEchoBehavior::kEchoIfPasswordEchoPhysicalEnabled:
      return settings && settings->GetPasswordEchoEnabledPhysical();
    case PasswordEchoBehavior::kEchoIfPasswordEchoTouchEnabled:
      return settings && settings->GetPasswordEchoEnabledTouch();
    case PasswordEchoBehavior::kDoNotEcho:
      return false;
  }
  NOTREACHED();
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

  if (ShouldEchoPassword()) {
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

String SetCharacterDataCommand::ToString() const {
  return StrCat({"SetCharacterDataCommand {new_text:",
                 new_text_.EncodeForDebugging(), "}"});
}

void SetCharacterDataCommand::Trace(Visitor* visitor) const {
  visitor->Trace(node_);
  SimpleEditCommand::Trace(visitor);
}

}  // namespace blink
