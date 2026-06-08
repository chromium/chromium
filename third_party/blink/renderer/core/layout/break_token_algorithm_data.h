// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_BREAK_TOKEN_ALGORITHM_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_BREAK_TOKEN_ALGORITHM_DATA_H_

#include "base/notreached.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

// Additional algorithm-specific data for block break tokens.
struct BreakTokenAlgorithmData
    : public GarbageCollected<BreakTokenAlgorithmData> {
 public:
  enum DataType {
    kFieldsetData,
    kFlexData,
    kGridData,
    kTableData,
    kTableRowData,
    kMulticolData,
    // When adding new values, ensure |type| below has enough bits.
  };
  DataType Type() const { return static_cast<DataType>(type); }

  explicit BreakTokenAlgorithmData(DataType type) : type(type) {}
  virtual ~BreakTokenAlgorithmData() = default;

  // One note about type checking and downcasting: It's generally not safe to
  // assume that a node has a specific break token data type. Break tokens
  // aren't always created by the layout algorithm normally associated with a
  // given node type, e.g. if we add a break-before break token.
  bool IsFieldsetType() const { return Type() == kFieldsetData; }
  bool IsFlexType() const { return Type() == kFlexData; }
  bool IsGridType() const { return Type() == kGridData; }
  bool IsTableType() const { return Type() == kTableData; }
  bool IsTableRowType() const { return Type() == kTableRowData; }
  bool IsMulticolType() const { return Type() == kMulticolData; }

  // Returns the total row gap count across all fragments including any gaps
  // suppressed during fragmentation.
  virtual wtf_size_t GetTotalRowGapCount() const { NOTREACHED(); }

  // Returns the first row gap index associated with this break token data
  // in the stitched container. In other words, it would be the index of the
  // gap in the container had it not been fragmented.
  virtual wtf_size_t GetFirstUnprocessedRowGapIndex() const { NOTREACHED(); }

  virtual void Trace(Visitor* visitor) const {}

  unsigned type : 3;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_BREAK_TOKEN_ALGORITHM_DATA_H_
