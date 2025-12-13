// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_TABLE_LAYOUT_TABLE_COLUMN_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_TABLE_LAYOUT_TABLE_COLUMN_H_

#include "base/functional/function_ref.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"

namespace blink {

class LayoutTable;

// Represents <col> and <colgroup> elements.
class CORE_EXPORT LayoutTableColumn : public LayoutBox {
 public:
  explicit LayoutTableColumn(Element*);
  void Trace(Visitor*) const override;

  LayoutTable* Table() const;

  bool IsColumn() const {
    NOT_DESTROYED();
    return StyleRef().Display() == EDisplay::kTableColumn;
  }

  bool IsColumnGroup() const {
    NOT_DESTROYED();
    return StyleRef().Display() == EDisplay::kTableColumnGroup;
  }

  unsigned Span() const {
    NOT_DESTROYED();
    return span_;
  }

  // Clears needs-layout for child columns too.
  void ClearNeedsLayoutForChildren() const;

  PhysicalSize StitchedSize() const override;

  PhysicalOffset PhysicalLocation() const override;
  PhysicalRect BoundingBoxRelativeToFirstFragment() const override;

  void QuadsInAncestorInternal(Vector<gfx::QuadF>&,
                               const LayoutBoxModelObject* ancestor,
                               MapCoordinatesFlags) const override;

  // LayoutObject methods start.

  const char* GetName() const override {
    NOT_DESTROYED();
    if (IsColumn())
      return "LayoutTableCol";
    else
      return "LayoutTableColGroup";
  }

  void StyleDidChange(StyleDifference diff,
                      const ComputedStyle* old_style,
                      const StyleChangeContext&) final;

  void ImageChanged(WrappedImagePtr, CanDeferInvalidation) final;

  void InsertedIntoTree() override;

  void WillBeRemovedFromTree() override;

  bool VisualRectRespectsVisibility() const final {
    NOT_DESTROYED();
    return false;
  }

  void SetColumnIndex(wtf_size_t column_idx) {
    NOT_DESTROYED();
    column_idx_ = column_idx;
  }

 protected:
  bool IsLayoutTableCol() const final {
    NOT_DESTROYED();
    return true;
  }

  // Table row doesn't paint background by itself.
  bool ComputeCanCompositeBackgroundAttachmentFixed() const override {
    NOT_DESTROYED();
    return false;
  }

 private:
  bool IsChildAllowed(LayoutObject*, const ComputedStyle&) const override;

  bool CanHaveChildren() const override;

  void UpdateFromElement() override;

  PaintLayerType LayerTypeRequired() const override {
    NOT_DESTROYED();
    return kNoPaintLayer;
  }

  LayoutObjectChildList* VirtualChildren() override {
    NOT_DESTROYED();
    return &children_;
  }

  const LayoutObjectChildList* VirtualChildren() const override {
    NOT_DESTROYED();
    return &children_;
  }

  // LayoutObject methods end

  struct SynthesizedFragment {
    STACK_ALLOCATED();

   public:
    SynthesizedFragment(const PhysicalRect& rect,
                        PhysicalOffset additional_offset_from_table_fragment,
                        const PhysicalBoxFragment& table_fragment)
        : rect(rect),
          additional_offset_from_table_fragment(
              additional_offset_from_table_fragment),
          table_fragment(table_fragment) {}

    // The "fragment" rectangle, relatively to its container. If it's a
    // table-column-group, or a table-column without a table-column-group
    // parent, the container is the table. Otherwise it's the parent
    // table-column-group.
    PhysicalRect rect;

    // Additional offset from the table fragment to `rect`.
    PhysicalOffset additional_offset_from_table_fragment;

    const PhysicalBoxFragment& table_fragment;
  };

  // Synthesize fragment rectangles for a table-column or table-column-group and
  // iterate over them, and call the provided callback for each of them. If the
  // callback returns false, iteration will stop.
  //
  // Table column and table column groups don't create fragments
  // (PhysicalBoxFragment), so we have to synthesize their rects, in order to
  // support getClientRects(), and more.
  //
  // TODO(crbug.com/360905183): If we add separate layout objects (and
  // fragments) for the table wrapper and the table grid, it should become more
  // straight-forward to create fragments for table columns and column groups as
  // well. And then this machinery can go away.
  void ForAllSynthesizedFragments(
      base::FunctionRef<bool(const SynthesizedFragment&)>) const;

 private:
  unsigned span_ = 1;
  LayoutObjectChildList children_;
  wtf_size_t column_idx_;
};

// wtf/casting.h helper.
template <>
struct DowncastTraits<LayoutTableColumn> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsLayoutTableCol();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_TABLE_LAYOUT_TABLE_COLUMN_H_
