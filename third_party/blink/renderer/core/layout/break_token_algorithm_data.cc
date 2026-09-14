// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/break_token_algorithm_data.h"

#include "base/notreached.h"
#include "third_party/blink/renderer/core/layout/flex/flex_break_token_data.h"
#include "third_party/blink/renderer/core/layout/forms/fieldset_break_token_data.h"
#include "third_party/blink/renderer/core/layout/grid/grid_break_token_data.h"
#include "third_party/blink/renderer/core/layout/grid_lanes/grid_lanes_break_token_data.h"
#include "third_party/blink/renderer/core/layout/multicol_break_token_data.h"
#include "third_party/blink/renderer/core/layout/table/table_break_token_data.h"
#include "third_party/blink/renderer/core/layout/table/table_row_break_token_data.h"

namespace blink {

void BreakTokenAlgorithmData::Trace(Visitor* visitor) const {
  switch (Type()) {
    case kFieldsetData:
      To<FieldsetBreakTokenData>(this)->TraceAfterDispatch(visitor);
      return;
    case kFlexData:
      To<FlexBreakTokenData>(this)->TraceAfterDispatch(visitor);
      return;
    case kGridData:
      To<GridBreakTokenData>(this)->TraceAfterDispatch(visitor);
      return;
    case kGridLanesData:
      To<GridLanesBreakTokenData>(this)->TraceAfterDispatch(visitor);
      return;
    case kTableData:
      To<TableBreakTokenData>(this)->TraceAfterDispatch(visitor);
      return;
    case kTableRowData:
      To<TableRowBreakTokenData>(this)->TraceAfterDispatch(visitor);
      return;
    case kMulticolData:
      To<MulticolBreakTokenData>(this)->TraceAfterDispatch(visitor);
      return;
  }
  NOTREACHED();
}

}  // namespace blink
