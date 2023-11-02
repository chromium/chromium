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

#include "third_party/blink/renderer/core/editing/commands/merge_identical_elements_command.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

MergeIdenticalElementsCommand::MergeIdenticalElementsCommand(Element* first,
                                                             Element* second)
    : SimpleEditCommand(first->GetDocument()),
      element1_(first),
      element2_(second) {
  DCHECK(element1_);
  DCHECK(element2_);
  DCHECK_EQ(element1_->nextSibling(), element2_);
}

void MergeIdenticalElementsCommand::DoApply(EditingState*) {
  if (element1_->nextSibling() != element2_ || !IsEditable(*element1_) ||
      !IsEditable(*element2_))
    return;

  at_child_ = element2_->firstChild();

  NodeVector children;
  GetChildNodes(*element1_, children);

  for (auto& child : children) {
    element2_->InsertBefore(child.Release(), at_child_.Get(),
                            IGNORE_EXCEPTION_FOR_TESTING);
  }

  element1_->remove(IGNORE_EXCEPTION_FOR_TESTING);
}

void MergeIdenticalElementsCommand::DoUnapply() {
  DCHECK(element1_);
  DCHECK(element2_);

  Node* at_child = at_child_.Release();

  ContainerNode* parent = element2_->parentNode();
  if (!parent || !IsEditable(*parent))
    return;

  DummyExceptionStateForTesting exception_state;

  parent->InsertBefore(element1_.Get(), element2_.Get(), exception_state);
  if (exception_state.HadException())
    return;

  HeapVector<Member<Node>> children;
  for (Node* child = element2_->firstChild(); child && child != at_child;
       child = child->nextSibling())
    children.push_back(child);

  for (auto& child : children)
    element1_->AppendChild(child.Release(), exception_state);
}

void MergeIdenticalElementsCommand::Trace(Visitor* visitor) const {
  visitor->Trace(element1_);
  visitor->Trace(element2_);
  visitor->Trace(at_child_);
  SimpleEditCommand::Trace(visitor);
}

}  // namespace blink
