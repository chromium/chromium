// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_LAYOUT_NG_TABLE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_LAYOUT_NG_TABLE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_mixin.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_interface.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_layout_algorithm_types.h"

namespace blink {

class NGTableBorders;

// Invalidation: LayoutNGTable differences from block invalidation:
//
// Cached collapsed borders:
// Table caches collapsed borders as NGTableBorders.
// NGTableBorders are stored on Table fragment for painting.
// If NGTableBorders change, table fragment must be regenerated.
// Any table part that contributes to collapsed borders must invalidate
// cached borders on border change by calling GridBordersChanged.
//
// Cached column constraints:
// Column constraints are used in layout. They must be regenerated
// whenever table geometry changes.
// The validation state is a IsTableColumnsConstraintsDirty flag
// on LayoutObject. They are invalidated inside
// LayoutObject::SetNeeds*Layout.

class CORE_EXPORT LayoutNGTable : public LayoutNGMixin<LayoutBlock>,
                                  public LayoutNGTableInterface {
 public:
  explicit LayoutNGTable(Element*);

  // TODO(atotic) Replace all H/VBorderSpacing with BorderSpacing?
  LogicalSize BorderSpacing() const {
    if (ShouldCollapseBorders())
      return LogicalSize();
    return LogicalSize(LayoutUnit(HBorderSpacing()),
                       LayoutUnit(VBorderSpacing()));
  }

  wtf_size_t ColumnCount() const;

  const NGTableBorders* GetCachedTableBorders() const {
    return cached_table_borders_.get();
  }

  void SetCachedTableBorders(scoped_refptr<const NGTableBorders>);

  const NGTableTypes::Columns* GetCachedTableColumnConstraints();

  void SetCachedTableColumnConstraints(
      scoped_refptr<const NGTableTypes::Columns>);

  // Any borders in table grid have changed.
  void GridBordersChanged();

  // Table descendants have been added/removed, and number of rows/columns
  // might have changed.
  void TableGridStructureChanged();

  // LayoutBlock methods start.

  const char* GetName() const override { return "LayoutNGTable"; }

  void UpdateBlockLayout(bool relayout_children) override;

  void AddChild(LayoutObject* child,
                LayoutObject* before_child = nullptr) override;

  void RemoveChild(LayoutObject*) override;

  void StyleDidChange(StyleDifference diff,
                      const ComputedStyle* old_style) override;

  LayoutBox* CreateAnonymousBoxWithSameTypeAs(
      const LayoutObject* parent) const override;

  void Paint(const PaintInfo&) const final;

  LayoutUnit BorderTop() const override;

  LayoutUnit BorderBottom() const override;

  LayoutUnit BorderLeft() const override;

  LayoutUnit BorderRight() const override;

  LayoutRectOutsets BorderBoxOutsets() const override;

  PhysicalRect OverflowClipRect(const PhysicalOffset&,
                                OverlayScrollbarClipBehavior) const override;

  void AddVisualEffectOverflow() final;

  bool VisualRectRespectsVisibility() const override {
    NOT_DESTROYED();
    return false;
  }

  // LayoutBlock methods end.

  // LayoutNGTableInterface methods start.

  const LayoutNGTableInterface* ToLayoutNGTableInterface() const final {
    return this;
  }

  const LayoutObject* ToLayoutObject() const final { return this; }

  // Non-const version required by TextAutosizer, AXLayoutObject.
  LayoutObject* ToMutableLayoutObject() final { return this; }

  bool ShouldCollapseBorders() const final {
    return StyleRef().BorderCollapse() == EBorderCollapse::kCollapse;
  }

  // Used in table painting for invalidation. Should not be needed by NG.
  bool HasCollapsedBorders() const final {
    NOTREACHED();
    return false;
  }

  bool HasColElements() const final {
    NOTREACHED();
    return false;
  }

  bool IsFixedTableLayout() const final {
    return StyleRef().TableLayout() == ETableLayout::kFixed &&
           !StyleRef().LogicalWidth().IsAuto();
  }
  int16_t HBorderSpacing() const final {
    return ShouldCollapseBorders() ? 0 : StyleRef().HorizontalBorderSpacing();
  }
  int16_t VBorderSpacing() const final {
    return ShouldCollapseBorders() ? 0 : StyleRef().VerticalBorderSpacing();
  }

  // Legacy had a concept of colspan column compression. This is a legacy
  // method to map between absolute and compressed columns.
  // Because NG does not compress columns, absolute and effective are the same.
  unsigned AbsoluteColumnToEffectiveColumn(
      unsigned absolute_column_index) const final {
    return absolute_column_index;
  }

  // Legacy caches sections. Might not be needed by NG.
  void RecalcSectionsIfNeeded() const final {}

  // Legacy caches sections. Might not be needed by NG.
  void ForceSectionsRecalc() final {}

  // Used in paint for printing. Should not be needed by NG.
  LayoutUnit RowOffsetFromRepeatingFooter() const final {
    NOTIMPLEMENTED();  // OK, never used.
    return LayoutUnit();
  }
  // Used in paint for printing. Should not be needed by NG.
  LayoutUnit RowOffsetFromRepeatingHeader() const final {
    NOTIMPLEMENTED();  // OK, never used.
    return LayoutUnit();
  }

  bool IsFirstCell(const LayoutNGTableCellInterface&) const final;

  LayoutNGTableSectionInterface* FirstBodyInterface() const final;

  LayoutNGTableSectionInterface* TopSectionInterface() const final;

  LayoutNGTableSectionInterface* SectionBelowInterface(
      const LayoutNGTableSectionInterface*,
      SkipEmptySectionsValue) const final;

  // Following methods are called during printing, not in TablesNG.
  LayoutNGTableSectionInterface* TopNonEmptySectionInterface() const final {
    NOTREACHED();
    return nullptr;
  }

  LayoutNGTableSectionInterface* BottomSectionInterface() const final {
    NOTREACHED();
    return nullptr;
  }

  LayoutNGTableSectionInterface* BottomNonEmptySectionInterface() const final {
    NOTREACHED();
    return nullptr;
  }

  // LayoutNGTableInterface methods end.

 protected:
  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectTable ||
           LayoutNGMixin<LayoutBlock>::IsOfType(type);
  }

 private:
  void InvalidateCachedTableBorders();

  // Table borders are cached because computing collapsed borders is expensive.
  scoped_refptr<const NGTableBorders> cached_table_borders_;

  // Table columns do not depend on any outside data (e.g. NGConstraintSpace).
  // They are cached because computing them is expensive.
  scoped_refptr<const NGTableTypes::Columns> cached_table_columns_;
};

// wtf/casting.h helper.
template <>
struct DowncastTraits<LayoutNGTable> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsTable() && object.IsLayoutNGObject();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_LAYOUT_NG_TABLE_H_
