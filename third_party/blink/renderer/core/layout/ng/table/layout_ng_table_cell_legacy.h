// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_LAYOUT_NG_TABLE_CELL_LEGACY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_LAYOUT_NG_TABLE_CELL_LEGACY_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_table_cell.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow_mixin.h"

namespace blink {

// This is a LayoutNG variant of LayoutTableCell.
// There are 3 table cell classes in Chrome
// LayoutNGTableCell - TablesNG cell, whole table is NG.
// LayoutNGTableCellLegacy - NG cell inside a legacy table.
// LayoutTableCell - Legacy cell inside legacy table.
class CORE_EXPORT LayoutNGTableCellLegacy final
    : public LayoutNGBlockFlowMixin<LayoutTableCell> {
 public:
  explicit LayoutNGTableCellLegacy(Element*);

  void UpdateBlockLayout(bool relayout_children) override;

  // This class used to be called LayoutNGTableCell, and many test baselines
  // expect its name to be LayoutNGTableCell, not LayoutNGTableCellLegacy.
  const char* GetName() const override { return "LayoutNGTableCell"; }

 protected:
  bool IsOfType(LayoutObjectType type) const final {
    NOT_DESTROYED();
    return type == kLayoutObjectTableCellLegacy ||
           LayoutNGBlockFlowMixin<LayoutTableCell>::IsOfType(type);
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_LAYOUT_NG_TABLE_CELL_LEGACY_H_
