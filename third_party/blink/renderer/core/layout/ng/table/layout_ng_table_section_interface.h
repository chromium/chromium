// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_LAYOUT_NG_TABLE_SECTION_INTERFACE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_LAYOUT_NG_TABLE_SECTION_INTERFACE_H_

#include "third_party/blink/renderer/core/layout/ng/table/interface_casting.h"

namespace blink {

class LayoutNGTableRowInterface;
class LayoutObject;

// Abstract class defining table section methods.
// Used for Legacy/NG interoperability.
class LayoutNGTableSectionInterface {
 public:
  virtual const LayoutObject* ToLayoutObject() const = 0;
  virtual LayoutNGTableInterface* TableInterface() const = 0;
  // TODO(crbug.com/1081425) Existing methods can be used by NG, should be
  // removed. Single caller is MarkBoxForRelayoutAfterSplit.
  virtual void SetNeedsCellRecalc() = 0;
  // TODO(crbug.com/1081425) Method not used by NG, should be removed.
  virtual bool IsRepeatingHeaderGroup() const = 0;
  // TODO(crbug.com/1081425) Method not used by NG, should be removed.
  virtual bool IsRepeatingFooterGroup() const = 0;
  virtual unsigned NumRows() const = 0;
  virtual unsigned NumCols(unsigned row) const = 0;
  virtual unsigned NumEffectiveColumns() const = 0;
  virtual LayoutNGTableRowInterface* FirstRowInterface() const = 0;
  virtual LayoutNGTableRowInterface* LastRowInterface() const = 0;
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
