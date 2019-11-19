// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_LAYOUT_NG_TABLE_ROW_INTERFACE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_LAYOUT_NG_TABLE_ROW_INTERFACE_H_

#include "third_party/blink/renderer/core/layout/ng/table/interface_casting.h"

namespace blink {

class LayoutObject;
class LayoutNGTableInterface;
class LayoutNGTableSectionInterface;
class LayoutNGTableCellInterface;

// Abstract class defining table row methods.
// Used for Legacy/NG interoperability.
class LayoutNGTableRowInterface {
 public:
  virtual const LayoutObject* ToLayoutObject() const = 0;
  virtual const LayoutTableRow* ToLayoutTableRow() const = 0;
  virtual LayoutNGTableInterface* TableInterface() const = 0;
  virtual unsigned RowIndex() const = 0;
  virtual LayoutNGTableSectionInterface* SectionInterface() const = 0;
  virtual LayoutNGTableRowInterface* PreviousRowInterface() const = 0;
  virtual LayoutNGTableRowInterface* NextRowInterface() const = 0;
  virtual LayoutNGTableCellInterface* FirstCellInterface() const = 0;
  virtual LayoutNGTableCellInterface* LastCellInterface() const = 0;
};

template <>
struct InterfaceDowncastTraits<LayoutNGTableRowInterface> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsTableRow();
  }
  static const LayoutNGTableRowInterface& ConvertFrom(
      const LayoutObject& object) {
    return *object.ToLayoutNGTableRowInterface();
  }
};

}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_LAYOUT_NG_TABLE_ROW_INTERFACE_H_
