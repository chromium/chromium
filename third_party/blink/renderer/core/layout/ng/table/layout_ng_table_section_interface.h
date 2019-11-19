// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_LAYOUT_NG_TABLE_SECTION_INTERFACE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_LAYOUT_NG_TABLE_SECTION_INTERFACE_H_

#include "third_party/blink/renderer/core/layout/ng/table/interface_casting.h"

namespace blink {

class LayoutNGTableRowInterface;
class LayoutNGTableCellInterface;
class LayoutTableSection;
class LayoutObject;

// Abstract class defining table section methods.
// Used for Legacy/NG interoperability.
class LayoutNGTableSectionInterface {
 public:
  virtual const LayoutTableSection* ToLayoutTableSection() const = 0;
  virtual const LayoutObject* ToLayoutObject() const = 0;
  virtual LayoutObject* ToMutableLayoutObject() = 0;
  virtual LayoutNGTableInterface* TableInterface() const = 0;
  virtual void SetNeedsCellRecalc() = 0;
  virtual bool IsRepeatingHeaderGroup() const = 0;
  virtual bool IsRepeatingFooterGroup() const = 0;
  virtual unsigned NumRows() const = 0;
  virtual unsigned NumCols(unsigned row) const = 0;
  virtual unsigned NumEffectiveColumns() const = 0;
  virtual LayoutNGTableRowInterface* FirstRowInterface() const = 0;
  virtual LayoutNGTableRowInterface* LastRowInterface() const = 0;
  virtual const LayoutNGTableCellInterface* PrimaryCellInterfaceAt(
      unsigned row,
      unsigned effective_column) const = 0;
};

template <>
struct InterfaceDowncastTraits<LayoutNGTableSectionInterface> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsTableSection();
  }
  static const LayoutNGTableSectionInterface& ConvertFrom(
      const LayoutObject& object) {
    return *object.ToLayoutNGTableSectionInterface();
  }
};

}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_LAYOUT_NG_TABLE_SECTION_INTERFACE_H_
