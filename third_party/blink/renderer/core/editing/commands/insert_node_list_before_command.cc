// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/commands/insert_node_list_before_command.h"

#include "third_party/blink/renderer/core/editing/commands/editing_state.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"

namespace blink {

InsertNodeListBeforeCommand::InsertNodeListBeforeCommand(
    Node& first_insert_child,
    Node& parent,
    Node* ref_child)
    : SimpleEditCommand(parent.GetDocument()),
      parent_(parent),
      ref_child_(ref_child) {
  DCHECK(IsEditable(parent) || !parent.InActiveDocument()) << parent;
  wtf_size_t count = 0;
  for (Node* child = &first_insert_child; child; child = child->nextSibling()) {
    ++count;
  }
  insert_children_.reserve(count);
  for (Node* child = &first_insert_child; child; child = child->nextSibling()) {
    insert_children_.push_back(child);
  }
}

void InsertNodeListBeforeCommand::DoApply(EditingState* editing_state) {
  GetDocument().UpdateStyleAndLayoutTree();
  if (!IsEditable(*parent_)) {
    return;
  }

  DummyExceptionStateForTesting exception_state;
  for (auto& child : insert_children_) {
    parent_->insertBefore(child.Get(), ref_child_.Get(), exception_state);
    ABORT_EDITING_COMMAND_IF(exception_state.HadException());
    ABORT_EDITING_COMMAND_IF(child->parentNode() != parent_);
  }
}

void InsertNodeListBeforeCommand::DoUnapply() {
  GetDocument().UpdateStyleAndLayoutTree();
  if (!IsEditable(*parent_)) {
    return;
  }
  for (auto& child : insert_children_) {
    child->remove(IGNORE_EXCEPTION_FOR_TESTING);
  }
}

String InsertNodeListBeforeCommand::ToString() const {
  return StrCat({"InsertNodeListBeforeCommand {insert_children:[",
                 String::Number(insert_children_.size()), " nodes]}"});
}

void InsertNodeListBeforeCommand::Trace(Visitor* visitor) const {
  visitor->Trace(insert_children_);
  visitor->Trace(parent_);
  visitor->Trace(ref_child_);
  SimpleEditCommand::Trace(visitor);
}

}  // namespace blink
