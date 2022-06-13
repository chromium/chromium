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

#include "third_party/blink/renderer/core/editing/commands/delete_from_text_node_command.h"

#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

DeleteFromTextNodeCommand::DeleteFromTextNodeCommand(Text* node,
                                                     unsigned offset,
                                                     unsigned count)
    : SimpleEditCommand(node->GetDocument()),
      node_(node),
      offset_(offset),
      count_(count) {
  DCHECK(node_);
  DCHECK_LE(offset_, node_->length());
  DCHECK_LE(offset_ + count_, node_->length());
}

void DeleteFromTextNodeCommand::DoApply(EditingState*) {
  DCHECK(node_);

  GetDocument().UpdateStyleAndLayoutTree();
  if (!IsEditable(*node_))
    return;

  DummyExceptionStateForTesting exception_state;
  text_ = node_->substringData(offset_, count_, exception_state);
  if (exception_state.HadException())
    return;

  node_->deleteData(offset_, count_, exception_state);
}

void DeleteFromTextNodeCommand::DoUnapply() {
  DCHECK(node_);

  if (!IsEditable(*node_))
    return;

  node_->insertData(offset_, text_, IGNORE_EXCEPTION_FOR_TESTING);
}

void DeleteFromTextNodeCommand::Trace(Visitor* visitor) const {
  visitor->Trace(node_);
  SimpleEditCommand::Trace(visitor);
}

}  // namespace blink
