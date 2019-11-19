// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_LAYOUT_NG_TABLE_CELL_INTERFACE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_LAYOUT_NG_TABLE_CELL_INTERFACE_H_

#include "third_party/blink/renderer/core/layout/ng/table/interface_casting.h"

namespace blink {

class LayoutNGTableInterface;
class LayoutNGTableSectionInterface;
class LayoutNGTableRowInterface;
class LayoutTableCell;
class Length;

// Abstract class defining table cell methods.
// Used for Legacy/NG interoperability.
class LayoutNGTableCellInterface {
 public:
  virtual const LayoutTableCell* ToLayoutTableCell() const = 0;
  virtual const LayoutObject* ToLayoutObject() const = 0;
  virtual LayoutObject* ToMutableLayoutObject() = 0;
  virtual LayoutNGTableInterface* TableInterface() const = 0;
  virtual void ColSpanOrRowSpanChanged() = 0;
  virtual Length StyleOrColLogicalWidth() const = 0;
  virtual int IntrinsicPaddingBefore() const = 0;
  virtual int IntrinsicPaddingAfter() const = 0;
  virtual unsigned RowIndex() const = 0;
  virtual unsigned ResolvedRowSpan() const = 0;
  virtual unsigned AbsoluteColumnIndex() const = 0;
  virtual unsigned ColSpan() const = 0;
  virtual LayoutNGTableCellInterface* NextCellInterface() const = 0;
  virtual LayoutNGTableCellInterface* PreviousCellInterface() const = 0;
  virtual LayoutNGTableRowInterface* RowInterface() const = 0;
  virtual LayoutNGTableSectionInterface* SectionInterface() const = 0;
};

template <>
struct InterfaceDowncastTraits<LayoutNGTableCellInterface> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsTableCell();
  }
  static const LayoutNGTableCellInterface& ConvertFrom(
      const LayoutObject& object) {
    return *object.ToLayoutNGTableCellInterface();
  }
};

}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_LAYOUT_NG_TABLE_CELL_INTERFACE_H_
