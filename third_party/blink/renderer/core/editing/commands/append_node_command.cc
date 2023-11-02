/*
 * Copyright (C) 2005, 2006, 2008 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/editing/commands/append_node_command.h"

#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

AppendNodeCommand::AppendNodeCommand(ContainerNode* parent, Node* node)
    : SimpleEditCommand(parent->GetDocument()), parent_(parent), node_(node) {
  DCHECK(parent_);
  DCHECK(node_);
  DCHECK(!node_->parentNode()) << node_;

  DCHECK(IsEditable(*parent_) || !parent_->InActiveDocument()) << parent_;
}

void AppendNodeCommand::DoApply(EditingState*) {
  if (!IsEditable(*parent_) && parent_->InActiveDocument())
    return;

  parent_->AppendChild(node_.Get(), IGNORE_EXCEPTION_FOR_TESTING);
}

void AppendNodeCommand::DoUnapply() {
  if (!IsEditable(*parent_))
    return;

  node_->remove(IGNORE_EXCEPTION_FOR_TESTING);
}

void AppendNodeCommand::Trace(Visitor* visitor) const {
  visitor->Trace(parent_);
  visitor->Trace(node_);
  SimpleEditCommand::Trace(visitor);
}

}  // namespace blink
