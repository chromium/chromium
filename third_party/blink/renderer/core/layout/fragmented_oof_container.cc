// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/fragmented_oof_container.h"

#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

void FragmentedOofContainer::Trace(Visitor* visitor) const {
  visitor->Trace(children_);
  visitor->Trace(oof_nodes_);
  visitor->Trace(inline_containing_block_map_);
}

#ifndef NDEBUG

String FragmentedOofContainer::DumpTree() const {
  StringBuilder builder;
  DumpRecursive(/*box=*/nullptr, 0, &builder);
  return builder.ToString();
}

void FragmentedOofContainer::ShowTree() const {
  LOG(INFO) << "\n" << DumpTree().Utf8();
}

void FragmentedOofContainer::DumpRecursive(const LayoutBox* containing_block,
                                           int indentation,
                                           StringBuilder* builder) const {
  auto indent = [&builder](int indentation) {
    for (int i = 0; i < indentation; i++) {
      builder->Append(" ");
    }
  };

  indent(indentation);
  if (containing_block) {
    builder->Append(containing_block->ToString());
  } else {
    // At the root.
    builder->Append(".:: FragmentedOofContainer Tree ::.");
  }
  builder->Append("\n");

  int next_indentation = indentation + 2;

  for (const LogicalOofPositionedNode& oof : oof_nodes_) {
    indent(next_indentation);
    builder->Append("OOF: ");
    builder->Append(oof.Node().ToString());
    builder->Append("\n");
  }

  for (auto [box, container] : children_) {
    DCHECK(box);
    container->DumpRecursive(box, next_indentation, builder);
  }
}

#endif  // NDEBUG

}  // namespace blink
