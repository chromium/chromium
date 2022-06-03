// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_LAYOUT_NG_TABLE_INTERFACE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_LAYOUT_NG_TABLE_INTERFACE_H_

#include "third_party/blink/renderer/core/layout/ng/table/interface_casting.h"

namespace blink {

class LayoutNGTableCellInterface;
class LayoutNGTableSectionInterface;
class LayoutUnit;

// Abstract class defining table methods.
// Used for Legacy/NG interoperability.
enum SkipEmptySectionsValue { kDoNotSkipEmptySections, kSkipEmptySections };

class LayoutNGTableInterface {
 public:
  virtual const LayoutObject* ToLayoutObject() const = 0;
  // Non-const version required by TextAutosizer, AXLayoutObject.
  virtual LayoutObject* ToMutableLayoutObject() = 0;
  virtual bool ShouldCollapseBorders() const = 0;
  // TODO(crbug.com/1081425) Method not used by NG, should be removed.
  virtual bool HasCollapsedBorders() const = 0;
  virtual bool IsFixedTableLayout() const = 0;
  virtual int16_t HBorderSpacing() const = 0;
  virtual int16_t VBorderSpacing() const = 0;
  // TODO(crbug.com/1081425) Method not used by NG, should be removed.
  virtual bool HasColElements() const = 0;
  virtual unsigned AbsoluteColumnToEffectiveColumn(
      unsigned absolute_column_index) const = 0;
  virtual void RecalcSectionsIfNeeded() const = 0;
  virtual void ForceSectionsRecalc() = 0;
  // TODO(crbug.com/1081425) Method not used by NG, should be removed.
  virtual LayoutUnit RowOffsetFromRepeatingFooter() const = 0;
  // TODO(crbug.com/1081425) Method not used by NG, should be removed.
  virtual LayoutUnit RowOffsetFromRepeatingHeader() const = 0;
  virtual LayoutNGTableSectionInterface* FirstBodyInterface() const = 0;
  virtual LayoutNGTableSectionInterface* TopSectionInterface() const = 0;
  // TODO(crbug.com/1081425) Method not used by NG, should be removed.
  virtual LayoutNGTableSectionInterface* TopNonEmptySectionInterface()
      const = 0;
  // TODO(crbug.com/1081425) Method not used by NG, should be removed.
  virtual LayoutNGTableSectionInterface* BottomSectionInterface() const = 0;
  virtual LayoutNGTableSectionInterface* BottomNonEmptySectionInterface()
      const = 0;
  virtual LayoutNGTableSectionInterface* SectionBelowInterface(
      const LayoutNGTableSectionInterface*,
      SkipEmptySectionsValue) const = 0;
  virtual bool IsFirstCell(const LayoutNGTableCellInterface&) const = 0;
};

template <>
struct InterfaceDowncastTraits<LayoutNGTableInterface> {
  static bool AllowFrom(const LayoutObject& object) { return object.IsTable(); }
  static const LayoutNGTableInterface& ConvertFrom(const LayoutObject& object) {
    return *object.ToLayoutNGTableInterface();
  }
};

}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_LAYOUT_NG_TABLE_INTERFACE_H_
