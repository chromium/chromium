// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/table_grid_cell.h"

#include "third_party/blink/renderer/core/layout/layout_table_cell.h"

namespace blink {

TableGridCell::TableGridCell() = default;

TableGridCell::~TableGridCell() = default;

void TableGridCell::Trace(Visitor* visitor) const {
  visitor->Trace(cells_);
}

}  // namespace blink
