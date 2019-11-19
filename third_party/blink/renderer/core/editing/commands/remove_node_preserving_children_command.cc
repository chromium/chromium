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

#include "third_party/blink/renderer/core/editing/commands/remove_node_preserving_children_command.h"

#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

RemoveNodePreservingChildrenCommand::RemoveNodePreservingChildrenCommand(
    Node* node,
    ShouldAssumeContentIsAlwaysEditable
        should_assume_content_is_always_editable)
    : CompositeEditCommand(node->GetDocument()),
      node_(node),
      should_assume_content_is_always_editable_(
          should_assume_content_is_always_editable) {
  DCHECK(node_);
}

void RemoveNodePreservingChildrenCommand::DoApply(EditingState* editing_state) {
  ABORT_EDITING_COMMAND_IF(!node_->parentNode());
  ABORT_EDITING_COMMAND_IF(!HasEditableStyle(*node_->parentNode()));
  auto* container_node = DynamicTo<ContainerNode>(node_.Get());
  if (container_node) {
    NodeVector children;
    GetChildNodes(*container_node, children);

    for (auto& current_child : children) {
      Node* child = current_child.Release();
      RemoveNode(child, editing_state,
                 should_assume_content_is_always_editable_);
      if (editing_state->IsAborted())
        return;
      InsertNodeBefore(child, node_, editing_state,
                       should_assume_content_is_always_editable_);
      if (editing_state->IsAborted())
        return;
    }
  }
  RemoveNode(node_, editing_state, should_assume_content_is_always_editable_);
}

void RemoveNodePreservingChildrenCommand::Trace(Visitor* visitor) {
  visitor->Trace(node_);
  CompositeEditCommand::Trace(visitor);
}

}  // namespace blink
