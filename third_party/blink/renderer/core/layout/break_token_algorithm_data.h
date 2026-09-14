// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_BREAK_TOKEN_ALGORITHM_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_BREAK_TOKEN_ALGORITHM_DATA_H_

#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

// Additional algorithm-specific data for block break tokens.
// The type tag selects tracing.
struct BreakTokenAlgorithmData
    : public GarbageCollected<BreakTokenAlgorithmData> {
 public:
  enum DataType {
    kFieldsetData,
    kFlexData,
    kGridData,
    kGridLanesData,
    kTableData,
    kTableRowData,
    kMulticolData,
    // When adding new values, ensure |type| below has enough bits.
  };
  DataType Type() const { return static_cast<DataType>(type); }

  // One note about type checking and downcasting: It's generally not safe to
  // assume that a node has a specific break token data type. Break tokens
  // aren't always created by the layout algorithm normally associated with a
  // given node type, e.g. if we add a break-before break token.
  bool IsFieldsetType() const { return Type() == kFieldsetData; }
  bool IsFlexType() const { return Type() == kFlexData; }
  bool IsGridType() const { return Type() == kGridData; }
  bool IsGridLanesType() const { return Type() == kGridLanesData; }
  bool IsTableType() const { return Type() == kTableData; }
  bool IsTableRowType() const { return Type() == kTableRowData; }
  bool IsMulticolType() const { return Type() == kMulticolData; }

  // Dispatches to the trace method for this data type.
  void Trace(Visitor* visitor) const;

  // Base trace hook called by each subclass after dispatch.
  void TraceAfterDispatch(Visitor* visitor) const {}

  unsigned type : 3;

 protected:
  // Keep construction and copying within the concrete data type.
  explicit BreakTokenAlgorithmData(DataType type) : type(type) {}
  BreakTokenAlgorithmData(const BreakTokenAlgorithmData&) = default;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_BREAK_TOKEN_ALGORITHM_DATA_H_
