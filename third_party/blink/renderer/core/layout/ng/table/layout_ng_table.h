// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_LAYOUT_NG_TABLE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_LAYOUT_NG_TABLE_H_

#include "base/dcheck_is_on.h"
#include "base/notreached.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block.h"
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

class CORE_EXPORT LayoutNGTable : public LayoutNGBlock,
                                  public LayoutNGTableInterface {
 public:
  explicit LayoutNGTable(Element*);
  ~LayoutNGTable() override;

  wtf_size_t ColumnCount() const;

  const NGTableBorders* GetCachedTableBorders() const {
    NOT_DESTROYED();
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

  // Table paints column backgrounds.
  // Returns true if table, or columns' style.HasBackground().
  bool HasBackgroundForPaint() const;

  // LayoutBlock methods start.

  const char* GetName() const override {
    NOT_DESTROYED();
    return "LayoutNGTable";
  }

  void UpdateBlockLayout(bool relayout_children) override;

  void AddChild(LayoutObject* child,
                LayoutObject* before_child = nullptr) override;

  void RemoveChild(LayoutObject*) override;

  void StyleDidChange(StyleDifference diff,
                      const ComputedStyle* old_style) override;

  LayoutBox* CreateAnonymousBoxWithSameTypeAs(
      const LayoutObject* parent) const override;

  LayoutUnit BorderTop() const override;

  LayoutUnit BorderBottom() const override;

  LayoutUnit BorderLeft() const override;

  LayoutUnit BorderRight() const override;

  // The collapsing border model disallows paddings on table.
  // See http://www.w3.org/TR/CSS2/tables.html#collapsing-borders.
  LayoutUnit PaddingTop() const override;

  LayoutUnit PaddingBottom() const override;

  LayoutUnit PaddingLeft() const override;

  LayoutUnit PaddingRight() const override;

  LayoutRectOutsets BorderBoxOutsets() const override;

  // TODO(1151101)
  // ClientLeft/Top are incorrect for tables, but cannot be fixed
  // by subclassing ClientLeft/Top.

  PhysicalRect OverflowClipRect(const PhysicalOffset&,
                                OverlayScrollbarClipBehavior) const override;

#if DCHECK_IS_ON()
  void AddVisualEffectOverflow() final;
#endif

  bool VisualRectRespectsVisibility() const override {
    NOT_DESTROYED();
    return false;
  }

  // Whether a table has opaque foreground depends on many factors, e.g. border
  // spacing, missing cells, etc. For simplicity, just conservatively assume
  // foreground of all tables are not opaque.
  // Copied from LayoutTable.
  bool ForegroundIsKnownToBeOpaqueInRect(
      const PhysicalRect& local_rect,
      unsigned max_depth_to_test) const override {
    NOT_DESTROYED();
    return false;
  }

  // LayoutBlock methods end.

  // LayoutNGTableInterface methods start.

  const LayoutNGTableInterface* ToLayoutNGTableInterface() const final {
    NOT_DESTROYED();
    return this;
  }

  const LayoutObject* ToLayoutObject() const final {
    NOT_DESTROYED();
    return this;
  }

  // Non-const version required by TextAutosizer, AXLayoutObject.
  LayoutObject* ToMutableLayoutObject() final {
    NOT_DESTROYED();
    return this;
  }

  bool ShouldCollapseBorders() const final {
    NOT_DESTROYED();
    return StyleRef().BorderCollapse() == EBorderCollapse::kCollapse;
  }

  bool HasCollapsedBorders() const;

  int16_t HBorderSpacing() const final {
    NOT_DESTROYED();
    return ShouldCollapseBorders() ? 0 : StyleRef().HorizontalBorderSpacing();
  }
  int16_t VBorderSpacing() const final {
    NOT_DESTROYED();
    return ShouldCollapseBorders() ? 0 : StyleRef().VerticalBorderSpacing();
  }

  unsigned AbsoluteColumnToEffectiveColumn(
      unsigned absolute_column_index) const final;

  // NG does not need this method. Sections are not cached.
  void RecalcSectionsIfNeeded() const final { NOT_DESTROYED(); }

  // Not used by NG. Legacy caches sections.
  void ForceSectionsRecalc() final { NOT_DESTROYED(); }

  // Used in paint for printing. Should not be needed by NG.
  LayoutUnit RowOffsetFromRepeatingFooter() const final {
    NOT_DESTROYED();
    NOTIMPLEMENTED();  // OK, never used.
    return LayoutUnit();
  }
  // Used in paint for printing. Should not be needed by NG.
  LayoutUnit RowOffsetFromRepeatingHeader() const final {
    NOT_DESTROYED();
    NOTIMPLEMENTED();  // OK, never used.
    return LayoutUnit();
  }

  bool IsFirstCell(const LayoutNGTableCellInterface&) const final;

  LayoutNGTableSectionInterface* FirstBodyInterface() const final;

  LayoutNGTableSectionInterface* FirstSectionInterface() const final;
  LayoutNGTableSectionInterface* FirstNonEmptySectionInterface() const final;
  LayoutNGTableSectionInterface* LastSectionInterface() const final;
  LayoutNGTableSectionInterface* LastNonEmptySectionInterface() const final;

  LayoutNGTableSectionInterface* NextSectionInterface(
      const LayoutNGTableSectionInterface*,
      SkipEmptySectionsValue) const final;

  LayoutNGTableSectionInterface* PreviousSectionInterface(
      const LayoutNGTableSectionInterface*,
      SkipEmptySectionsValue) const final;

  // LayoutNGTableInterface methods end.

 protected:
  bool IsOfType(LayoutObjectType type) const override {
    NOT_DESTROYED();
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
