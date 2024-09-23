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

#include "third_party/blink/renderer/core/editing/commands/insert_node_before_command.h"

#include "third_party/blink/renderer/core/editing/commands/editing_state.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

InsertNodeBeforeCommand::InsertNodeBeforeCommand(
    Node* insert_child,
    Node* ref_child,
    ShouldAssumeContentIsAlwaysEditable
        should_assume_content_is_always_editable)
    : SimpleEditCommand(ref_child->GetDocument()),
      insert_child_(insert_child),
      ref_child_(ref_child),
      should_assume_content_is_always_editable_(
          should_assume_content_is_always_editable) {
  DCHECK(insert_child_);
  DCHECK(!insert_child_->parentNode()) << insert_child_;
  DCHECK(ref_child_);
  DCHECK(ref_child_->parentNode()) << ref_child_;

  DCHECK(IsEditable(*ref_child_->parentNode()) ||
         !ref_child_->parentNode()->InActiveDocument())
      << ref_child_->parentNode();
}

void InsertNodeBeforeCommand::DoApply(EditingState* editing_state) {
  ContainerNode* parent = ref_child_->parentNode();
  GetDocument().UpdateStyleAndLayoutTree();
  if (!parent || (should_assume_content_is_always_editable_ ==
                      kDoNotAssumeContentIsAlwaysEditable &&
                  !IsEditable(*parent)))
    return;
  DCHECK(IsEditable(*parent)) << parent;

  DummyExceptionStateForTesting exception_state;
  parent->InsertBefore(insert_child_.Get(), ref_child_.Get(), exception_state);
  ABORT_EDITING_COMMAND_IF(exception_state.HadException());
}

void InsertNodeBeforeCommand::DoUnapply() {
  GetDocument().UpdateStyleAndLayoutTree();
  if (RuntimeEnabledFeatures::PreventUndoIfNotEditableEnabled()) {
    ContainerNode* parent = ref_child_->parentNode();
    if (!parent || !IsEditable(*parent)) {
      return;
    }
  } else {
    if (!IsEditable(*insert_child_)) {
      return;
    }
  }
  insert_child_->remove(IGNORE_EXCEPTION_FOR_TESTING);
}

void InsertNodeBeforeCommand::Trace(Visitor* visitor) const {
  visitor->Trace(insert_child_);
  visitor->Trace(ref_child_);
  SimpleEditCommand::Trace(visitor);
}

}  // namespace blink
