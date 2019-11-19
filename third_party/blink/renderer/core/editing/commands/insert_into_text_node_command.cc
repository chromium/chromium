/*
 * Copyright (C) 2005, 2008 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/editing/commands/insert_into_text_node_command.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

InsertIntoTextNodeCommand::InsertIntoTextNodeCommand(Text* node,
                                                     unsigned offset,
                                                     const String& text)
    : SimpleEditCommand(node->GetDocument()),
      node_(node),
      offset_(offset),
      text_(text) {
  DCHECK(node_);
  DCHECK_LE(offset_, node_->length());
  DCHECK(!text_.IsEmpty());
}

void InsertIntoTextNodeCommand::DoApply(EditingState*) {
  bool password_echo_enabled =
      GetDocument().GetSettings() &&
      GetDocument().GetSettings()->GetPasswordEchoEnabled();
  if (password_echo_enabled)
    GetDocument().UpdateStyleAndLayout();

  if (!HasEditableStyle(*node_))
    return;

  if (password_echo_enabled) {
    LayoutText* layout_text = node_->GetLayoutObject();
    if (layout_text && layout_text->IsSecure())
      layout_text->MomentarilyRevealLastTypedCharacter(offset_ +
                                                       text_.length() - 1);
  }

  node_->insertData(offset_, text_, IGNORE_EXCEPTION_FOR_TESTING);
}

void InsertIntoTextNodeCommand::DoUnapply() {
  if (!HasEditableStyle(*node_))
    return;

  node_->deleteData(offset_, text_.length(), IGNORE_EXCEPTION_FOR_TESTING);
}

void InsertIntoTextNodeCommand::Trace(Visitor* visitor) {
  visitor->Trace(node_);
  SimpleEditCommand::Trace(visitor);
}

}  // namespace blink
