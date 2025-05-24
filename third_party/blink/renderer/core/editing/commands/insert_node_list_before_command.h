// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_COMMANDS_INSERT_NODE_LIST_BEFORE_COMMAND_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_COMMANDS_INSERT_NODE_LIST_BEFORE_COMMAND_H_

#include "third_party/blink/renderer/core/editing/commands/edit_command.h"

namespace blink {

class InsertNodeListBeforeCommand final : public SimpleEditCommand {
 public:
  InsertNodeListBeforeCommand(Node& first_insert_child,
                              Node& parent,
                              Node* ref_child);

  void Trace(Visitor* visitor) const override;

 private:
  void DoApply(EditingState* editing_stae) override;
  void DoUnapply() override;
  String ToString() const override;

  HeapVector<Member<Node>> insert_children_;
  Member<Node> parent_;
  Member<Node> ref_child_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_COMMANDS_INSERT_NODE_LIST_BEFORE_COMMAND_H_
