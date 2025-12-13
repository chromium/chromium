// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_MATHML_LAYOUT_TABLE_CELL_WITH_ANONYMOUS_MROW_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_MATHML_LAYOUT_TABLE_CELL_WITH_ANONYMOUS_MROW_H_

#include "third_party/blink/renderer/core/layout/table/layout_table_cell.h"

namespace blink {

class LayoutTableCellWithAnonymousMrow : public LayoutTableCell {
 public:
  explicit LayoutTableCellWithAnonymousMrow(Element*);

  void AddChild(LayoutObject* new_child,
                LayoutObject* before_child = nullptr) override;

  // TODO(crbug.com/40060619): This override was added because of the legacy
  // multicol implementation, which required its own special anonymous child
  // wrapper, which came in conflict with what AddChild() does here. It should
  // now be safe to allow multicol again (if that's useful at all).
  bool AllowsColumns() const override {
    NOT_DESTROYED();
    return false;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_MATHML_LAYOUT_TABLE_CELL_WITH_ANONYMOUS_MROW_H_
