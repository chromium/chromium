// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_PHYSICAL_BOX_FRAGMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_PHYSICAL_BOX_FRAGMENT_H_

#include "base/dcheck_is_on.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/geometry/box_sides.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_box_strut.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_items.h"
#include "third_party/blink/renderer/core/layout/ng/mathml/ng_mathml_paint_info.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/physical_fragment_rare_data.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_borders.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_fragment_data.h"
#include "third_party/blink/renderer/core/style/style_overflow_clip_margin.h"
#include "third_party/blink/renderer/platform/graphics/overlay_scrollbar_clip_behavior.h"
#include "third_party/blink/renderer/platform/wtf/bit_field.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class NGBoxFragmentBuilder;
enum class NGOutlineType;
struct FrameSetLayoutData;

class CORE_EXPORT NGPhysicalBoxFragment final : public NGPhysicalFragment {
 public:
  static const NGPhysicalBoxFragment* Create(
      NGBoxFragmentBuilder* builder,
      WritingMode block_or_line_writing_mode);

  // Creates a shallow copy of |other|.
  static const NGPhysicalBoxFragment* Clone(const NGPhysicalBoxFragment& other);

  // Creates a shallow copy of |other| but uses the "post-layout" fragments to
  // ensure fragment-tree consistency.
  static const NGPhysicalBoxFragment* CloneWithPostLayoutFragments(
      const NGPhysicalBoxFragment& other,
      const absl::optional<PhysicalRect> updated_layout_overflow =
          absl::nullopt);

  using PassKey = base::PassKey<NGPhysicalBoxFragment>;
  NGPhysicalBoxFragment(PassKey,
                        NGBoxFragmentBuilder* builder,
                        bool has_layout_overflow,
                        const PhysicalRect& layout_overflow,
                        bool has_borders,
                        const NGPhysicalBoxStrut& borders,
                        bool has_padding,
                        const NGPhysicalBoxStrut& padding,
                        const absl::optional<PhysicalRect>& inflow_bounds,
                        bool has_fragment_items,
                        WritingMode block_or_line_writing_mode);

  // Make a shallow copy. The child fragment pointers are just shallowly
  // copied. Fragment *items* are cloned (but not box fragments associated with
  // items), though. Additionally, the copy will set new overflow information,
  // based on the parameters, rather than copying it from the original fragment.
  NGPhysicalBoxFragment(PassKey,
                        const NGPhysicalBoxFragment& other,
                        bool has_layout_overflow,
                        const PhysicalRect& layout_overflow);

  ~NGPhysicalBoxFragment();

#if DCHECK_IS_ON()
  class AllowPostLayoutScope {
    STACK_ALLOCATED();

   public:
    AllowPostLayoutScope();
    ~AllowPostLayoutScope();
    static bool IsAllowed() { return allow_count_; }

   private:
    static unsigned allow_count_;
  };
#endif

  void TraceAfterDispatch(Visitor* visitor) const;

  const NGPhysicalBoxFragment* PostLayout() const;

  // Returns the children of |this|.
  //
  // Note, children in this collection maybe old generations. Items in this
  // collection are safe, but their children (grandchildren of |this|) maybe
  // from deleted nodes or LayoutObjects. Also see |PostLayoutChildren()|.
  base::span<const NGLink> Children() const {
    DCHECK(children_valid_);
    return base::make_span(children_);
  }

  // Similar to |Children()| but all children are the latest generation of
  // post-layout, and therefore all descendants are safe.
  NGPhysicalFragment::PostLayoutChildLinkList PostLayoutChildren() const {
    DCHECK(children_valid_);
    return PostLayoutChildLinkList(children_.size(), children_.data());
  }

  // This exposes a mutable part of the fragment for |NGOutOfFlowLayoutPart|.
  class MutableChildrenForOutOfFlow final {
    STACK_ALLOCATED();

   protected:
    friend class NGOutOfFlowLayoutPart;
    base::span<NGLink> Children() const {
      return base::make_span(buffer_, num_children_);
    }

   private:
    friend class NGPhysicalBoxFragment;
    MutableChildrenForOutOfFlow(const NGLink* buffer, wtf_size_t num_children)
        : buffer_(const_cast<NGLink*>(buffer)), num_children_(num_children) {}

    NGLink* buffer_;
    wtf_size_t num_children_;
  };

  MutableChildrenForOutOfFlow GetMutableChildrenForOutOfFlow() const {
    DCHECK(children_valid_);
    return MutableChildrenForOutOfFlow(children_.data(), children_.size());
  }

  // Returns |NGFragmentItems| if this fragment has one.
  bool HasItems() const {
    // Use get_concurrently because it can be called from a background thread in
    // TraceAfterDispatch().
    return bit_field_.get_concurrently<ConstHasFragmentItemsFlag>();
  }
  const NGFragmentItems* Items() const {
    return HasItems() ? ComputeItemsAddress() : nullptr;
  }

  absl::optional<LayoutUnit> FirstBaseline() const {
    if (has_first_baseline_)
      return first_baseline_;
    return absl::nullopt;
  }

  absl::optional<LayoutUnit> LastBaseline() const {
    if (has_last_baseline_)
      return last_baseline_;
    return absl::nullopt;
  }

  bool UseLastBaselineForInlineBaseline() const {
    return use_last_baseline_for_inline_baseline_;
  }

  bool UseBlockEndMarginEdgeForInlineBaseline() const {
    if (!use_last_baseline_for_inline_baseline_)
      return false;
    if (const auto* layout_block = DynamicTo<LayoutBlock>(GetLayoutObject()))
      return layout_block->UseLogicalBottomMarginEdgeForInlineBlockBaseline();
    return false;
  }

  LogicalRect TableGridRect() const {
    return rare_data_->GetField(FieldId::kTableGridRect)->table_grid_rect;
  }

  const NGTableFragmentData::ColumnGeometries* TableColumnGeometries() const {
    return rare_data_->table_column_geometries_;
  }

  const NGTableBorders* TableCollapsedBorders() const {
    if (const auto* field = GetRareField(FieldId::kTableCollapsedBorders)) {
      return field->table_collapsed_borders.get();
    }
    return nullptr;
  }

  const NGTableFragmentData::CollapsedBordersGeometry*
  TableCollapsedBordersGeometry() const {
    if (const auto* field =
            GetRareField(FieldId::kTableCollapsedBordersGeometry)) {
      return field->table_collapsed_borders_geometry.get();
    }
    return nullptr;
  }

  wtf_size_t TableCellColumnIndex() const {
    return rare_data_->GetField(FieldId::kTableCellColumnIndex)
        ->table_cell_column_index;
  }

  absl::optional<wtf_size_t> TableSectionStartRowIndex() const {
    DCHECK(IsTableNGSection());
    if (const auto* field = GetRareField(FieldId::kTableSectionStartRowIndex)) {
      return field->table_section_start_row_index;
    }
    return absl::nullopt;
  }

  const Vector<LayoutUnit>* TableSectionRowOffsets() const {
    DCHECK(IsTableNGSection());
    if (const auto* field = GetRareField(FieldId::kTableSectionRowOffsets)) {
      return &field->table_section_row_offsets;
    }
    return nullptr;
  }

  // The name of the page (if any) to which this fragment belongs. The page name
  // is propagated all the way up to the page fragment, which is needed in order
  // to support e.g. page orientation. See https://drafts.csswg.org/css-page-3
  AtomicString PageName() const {
    if (const auto* field = GetRareField(FieldId::kPageName)) {
      return field->page_name;
    }
    return g_null_atom;
  }

  // Returns the layout-overflow for this fragment.
  const PhysicalRect LayoutOverflow() const {
    if (RuntimeEnabledFeatures::LayoutOverflowNoCloneEnabled()) {
      if (const auto* field = GetRareField(FieldId::kLayoutOverflow)) {
        return field->layout_overflow;
      }
      return {{}, Size()};
    }
    if (!HasLayoutOverflow())
      return {{}, Size()};
    return *ComputeLayoutOverflowAddress();
  }

  bool HasLayoutOverflow() const {
    if (RuntimeEnabledFeatures::LayoutOverflowNoCloneEnabled()) {
      return GetRareField(FieldId::kLayoutOverflow);
    }
    return bit_field_.get<HasLayoutOverflowFlag>();
  }

  const NGPhysicalBoxStrut Borders() const {
    if (RuntimeEnabledFeatures::RareBorderPaddingInflowEnabled()) {
      if (const auto* field = GetRareField(FieldId::kBorders)) {
        return field->borders;
      }
      return NGPhysicalBoxStrut();
    }
    if (!HasBorders())
      return NGPhysicalBoxStrut();
    return *ComputeBordersAddress();
  }

  const NGPhysicalBoxStrut Padding() const {
    if (RuntimeEnabledFeatures::RareBorderPaddingInflowEnabled()) {
      if (const auto* field = GetRareField(FieldId::kPadding)) {
        return field->padding;
      }
      return NGPhysicalBoxStrut();
    }
    if (!HasPadding())
      return NGPhysicalBoxStrut();
    return *ComputePaddingAddress();
  }

  const NGPhysicalBoxStrut Margins() const {
    if (const auto* field = GetRareField(FieldId::kMargins)) {
      return field->margins;
    }
    return NGPhysicalBoxStrut();
  }

  const PhysicalOffset ContentOffset() const {
    if (!HasBorders() && !HasPadding())
      return PhysicalOffset();
    PhysicalOffset offset;
    if (HasBorders())
      offset += Borders().Offset();
    if (HasPadding())
      offset += Padding().Offset();
    return offset;
  }

  // Returns the bounds of any inflow children for this fragment (specifically
  // no out-of-flow positioned objects). This will return |absl::nullopt| if:
  //  - The fragment is *not* a scroll container.
  //  - The scroll container contains no inflow children.
  // This is normally the union of all inflow children's border-box rects
  // (without relative positioning applied), however for grid layout it is the
  // size and position of the grid instead.
  // This is used for scrollable overflow calculations.
  const absl::optional<PhysicalRect> InflowBounds() const {
    if (RuntimeEnabledFeatures::RareBorderPaddingInflowEnabled()) {
      if (const auto* field = GetRareField(FieldId::kInflowBounds)) {
        return field->inflow_bounds;
      }
      return absl::nullopt;
    }
    if (!HasInflowBounds())
      return absl::nullopt;
    return *ComputeInflowBoundsAddress();
  }

  // Return true if this is either a container that establishes an inline
  // formatting context, or if it's non-atomic inline content participating in
  // one. Empty blocks don't establish an inline formatting context.
  //
  // The return value from this method is undefined and irrelevant if the object
  // establishes a different type of formatting context than block/inline, such
  // as table or flexbox.
  //
  // Example:
  // <div>                                       <!-- false -->
  //   <div>                                     <!-- true -->
  //     <div style="float:left;"></div>         <!-- false -->
  //     <div style="float:left;">               <!-- true -->
  //       xxx                                   <!-- true -->
  //     </div>
  //     <div style="float:left;">               <!-- false -->
  //       <div style="float:left;"></div>       <!-- false -->
  //     </div>
  //     <span>                                  <!-- true -->
  //       xxx                                   <!-- true -->
  //       <span style="display:inline-block;">  <!-- false -->
  //         <div></div>                         <!-- false -->
  //       </span>
  //       <span style="display:inline-block;">  <!-- true -->
  //         xxx                                 <!-- true -->
  //       </span>
  //       <span style="display:inline-flex;">   <!-- N/A -->
  bool IsInlineFormattingContext() const {
    return bit_field_.get<IsInlineFormattingContextFlag>();
  }

  // The |LayoutBox| whose |PhysicalFragments()| contains |this|. This is
  // different from |GetLayoutObject()| if |this.IsColumnBox()|.
  const LayoutBox* OwnerLayoutBox() const;
  LayoutBox* MutableOwnerLayoutBox() const;

  // Returns the offset in the |OwnerLayoutBox| coordinate system. This is only
  // supported for CSS boxes (i.e. not for fragmentainers, for instance).
  PhysicalOffset OffsetFromOwnerLayoutBox() const;

  PhysicalRect ScrollableOverflow(TextHeightType height_type) const;
  PhysicalRect ScrollableOverflowFromChildren(TextHeightType height_type) const;

  // TODO(layout-dev): These three methods delegate to legacy layout for now,
  // update them to use LayoutNG based overflow information from the fragment
  // and change them to use NG geometry types once LayoutNG supports overflow.
  PhysicalRect OverflowClipRect(
      const PhysicalOffset& location,
      OverlayScrollbarClipBehavior = kIgnoreOverlayScrollbarSize) const;
  PhysicalRect OverflowClipRect(
      const PhysicalOffset& location,
      const NGBlockBreakToken* incoming_break_token,
      OverlayScrollbarClipBehavior = kIgnoreOverlayScrollbarSize) const;
  gfx::Vector2d PixelSnappedScrolledContentOffset() const;
  PhysicalSize ScrollSize() const;

  NGInkOverflow::Type InkOverflowType() const {
    return static_cast<NGInkOverflow::Type>(
        bit_field_.get<InkOverflowTypeValue>());
  }
  bool IsInkOverflowComputed() const {
    return InkOverflowType() != NGInkOverflow::Type::kNotSet &&
           InkOverflowType() != NGInkOverflow::Type::kInvalidated;
  }
  bool HasInkOverflow() const {
    return InkOverflowType() != NGInkOverflow::Type::kNone;
  }

  // 3 types of ink overflows:
  // * |SelfInkOverflow| includes box decorations that are outside of the
  //   border box.
  //   Returns |LocalRect| when there are no overflow.
  // * |ContentsInkOverflow| includes anything that would bleed out of the box
  //   and would be clipped by the overflow clip ('overflow' != visible). This
  //   corresponds to children that overflows their parent.
  //   Returns an empty rect when there are no overflow.
  // * |InkOverflow| includes self and contents ink overflow, unless it has
  //   clipping, in that case it includes self ink overflow only.
  //   Returns |LocalRect| when there are no overflow.
  PhysicalRect InkOverflow() const;
  PhysicalRect SelfInkOverflow() const;
  PhysicalRect ContentsInkOverflow() const;

  // Fast check if |NodeAtPoint| may find a hit.
  bool MayIntersect(const HitTestResult& result,
                    const HitTestLocation& hit_test_location,
                    const PhysicalOffset& accumulated_offset) const;

  // In order to paint united outline rectangles, the "owner" fragment paints
  // outlines for non-owner fragments.
  bool IsOutlineOwner() const {
    return !IsInlineBox() || InlineContainerFragmentIfOutlineOwner();
  }
  const NGPhysicalBoxFragment* InlineContainerFragmentIfOutlineOwner() const;

  // Fragment offset is this fragment's offset from parent.
  // Needed to compensate for LayoutInline Legacy code offsets.
  void AddSelfOutlineRects(const PhysicalOffset& additional_offset,
                           NGOutlineType include_block_overflows,
                           OutlineRectCollector& collector,
                           LayoutObject::OutlineInfo* info) const;
  // Same as |AddSelfOutlineRects|, except when |this.IsInlineBox()|, in which
  // case the coordinate system is relative to the inline formatting context.
  void AddOutlineRects(const PhysicalOffset& additional_offset,
                       NGOutlineType include_block_overflows,
                       OutlineRectCollector& collector) const;

  PositionWithAffinity PositionForPoint(PhysicalOffset) const;

  // The outsets to apply to the border-box of this fragment for
  // |overflow-clip-margin|.
  NGPhysicalBoxStrut OverflowClipMarginOutsets() const;

  PhysicalBoxSides SidesToInclude() const {
    return PhysicalBoxSides(IncludeBorderTop(), IncludeBorderRight(),
                            IncludeBorderBottom(), IncludeBorderLeft());
  }

  const NGBlockBreakToken* BreakToken() const {
    return To<NGBlockBreakToken>(NGPhysicalFragment::BreakToken());
  }

  // Return true if this is the first fragment generated from a node.
  bool IsFirstForNode() const { return bit_field_.get<IsFirstForNodeFlag>(); }

  // Return true if this is the only fragment generated from a node.
  bool IsOnlyForNode() const { return IsFirstForNode() && !BreakToken(); }

  bool HasDescendantsForTablePart() const {
    DCHECK(IsTableNGPart() || IsTableNGCell());
    return bit_field_.get<HasDescendantsForTablePartFlag>();
  }

  bool IsFragmentationContextRoot() const {
    return bit_field_.get<IsFragmentationContextRootFlag>();
  }

  bool IsMonolithic() const { return bit_field_.get<IsMonolithicFlag>(); }

#if DCHECK_IS_ON()
  void CheckSameForSimplifiedLayout(const NGPhysicalBoxFragment&,
                                    bool check_same_block_size,
                                    bool check_no_fragmentation) const;
#endif

  const FrameSetLayoutData* GetFrameSetLayoutData() const {
    return rare_data_->GetField(FieldId::kFrameSetLayoutData)
        ->frame_set_layout_data.get();
  }

  bool HasExtraMathMLPainting() const {
    if (IsMathMLFraction())
      return true;

    return GetRareField(FieldId::kMathMLPaintInfo);
  }
  const NGMathMLPaintInfo& GetMathMLPaintInfo() const {
    return *rare_data_->GetField(FieldId::kMathMLPaintInfo)
                ->mathml_paint_info.get();
  }

  class MutableForStyleRecalc {
    STACK_ALLOCATED();

   public:
    MutableForStyleRecalc(base::PassKey<NGPhysicalBoxFragment>,
                          NGPhysicalBoxFragment& fragment);
    void SetLayoutOverflow(const PhysicalRect& layout_overflow);

   private:
    NGPhysicalBoxFragment& fragment_;
  };
  MutableForStyleRecalc GetMutableForStyleRecalc() const;

  class MutableForContainerLayout {
    STACK_ALLOCATED();

   public:
    MutableForContainerLayout(base::PassKey<NGPhysicalBoxFragment>,
                              NGPhysicalBoxFragment& fragment);
    void SetMargins(const NGPhysicalBoxStrut& margins);

   private:
    NGPhysicalBoxFragment& fragment_;
  };

  MutableForContainerLayout GetMutableForContainerLayout() const;

  // Painters can use const methods only, except for these explicitly declared
  // methods.
  class MutableForPainting {
    STACK_ALLOCATED();

   public:
    void RecalcInkOverflow() { fragment_.RecalcInkOverflow(); }
    void RecalcInkOverflow(const PhysicalRect& contents) {
      fragment_.RecalcInkOverflow(contents);
    }
#if DCHECK_IS_ON()
    void InvalidateInkOverflow() { fragment_.InvalidateInkOverflow(); }
#endif

   private:
    friend class NGPhysicalBoxFragment;
    explicit MutableForPainting(const NGPhysicalBoxFragment& fragment)
        : fragment_(const_cast<NGPhysicalBoxFragment&>(fragment)) {}

    NGPhysicalBoxFragment& fragment_;
  };
  MutableForPainting GetMutableForPainting() const {
    return MutableForPainting(*this);
  }

  class MutableForCloning {
    STACK_ALLOCATED();
    friend class NGFragmentRepeater;
    friend class NGPhysicalBoxFragment;

   public:
    void ClearIsFirstForNode() {
      fragment_.bit_field_.set<IsFirstForNodeFlag>(false);
    }
    void ClearPropagatedOOFs() { fragment_.ClearOutOfFlowData(); }
    void SetBreakToken(const NGBlockBreakToken* token) {
      fragment_.break_token_ = token;
    }
    base::span<NGLink> Children() const {
      DCHECK(fragment_.children_valid_);
      return base::make_span(fragment_.children_);
    }

   private:
    explicit MutableForCloning(const NGPhysicalBoxFragment& fragment)
        : fragment_(const_cast<NGPhysicalBoxFragment&>(fragment)) {}

    NGPhysicalBoxFragment& fragment_;
  };
  friend class MutableForCloning;
  MutableForCloning GetMutableForCloning() const {
    return MutableForCloning(*this);
  }

  // Returns if this fragment can compute ink overflow.
  bool CanUseFragmentsForInkOverflow() const {
    return !layout_object_->IsLayoutReplaced();
  }
  // Recalculates and updates |*InkOverflow|.
  void RecalcInkOverflow();
  // |RecalcInkOverflow| using the given contents ink overflow rect.
  void RecalcInkOverflow(const PhysicalRect& contents);

#if DCHECK_IS_ON()
  void InvalidateInkOverflow();
  void AssertFragmentTreeSelf() const;
  void AssertFragmentTreeChildren(bool allow_destroyed_or_moved = false) const;
#endif

 protected:
  friend class NGPhysicalFragment;
  void Dispose();

 private:
  using BitField = WTF::ConcurrentlyReadBitField<uint32_t>;
  using ConstHasFragmentItemsFlag =
      BitField::DefineFirstValue<bool, 1, WTF::BitFieldValueConstness::kConst>;
  using IsInlineFormattingContextFlag =
      ConstHasFragmentItemsFlag::DefineNextValue<bool, 1>;
  using IncludeBorderTopFlag =
      IsInlineFormattingContextFlag::DefineNextValue<bool, 1>;
  using IncludeBorderRightFlag = IncludeBorderTopFlag::DefineNextValue<bool, 1>;
  using IncludeBorderBottomFlag =
      IncludeBorderRightFlag::DefineNextValue<bool, 1>;
  using IncludeBorderLeftFlag =
      IncludeBorderBottomFlag::DefineNextValue<bool, 1>;
  using HasLayoutOverflowFlag = IncludeBorderLeftFlag::DefineNextValue<bool, 1>;
  using InkOverflowTypeValue =
      HasLayoutOverflowFlag::DefineNextValue<uint8_t, NGInkOverflow::kTypeBits>;
  using HasBordersFlag = InkOverflowTypeValue::DefineNextValue<bool, 1>;
  using HasPaddingFlag = HasBordersFlag::DefineNextValue<bool, 1>;
  using HasInflowBoundsFlag = HasPaddingFlag::DefineNextValue<bool, 1>;
  using IsFirstForNodeFlag = HasInflowBoundsFlag::DefineNextValue<bool, 1>;
  using HasDescendantsForTablePartFlag =
      IsFirstForNodeFlag::DefineNextValue<bool, 1>;
  using IsFragmentationContextRootFlag =
      HasDescendantsForTablePartFlag::DefineNextValue<bool, 1>;
  using IsMonolithicFlag =
      IsFragmentationContextRootFlag::DefineNextValue<bool, 1>;

  bool IncludeBorderTop() const {
    return bit_field_.get<IncludeBorderTopFlag>();
  }
  bool IncludeBorderRight() const {
    return bit_field_.get<IncludeBorderRightFlag>();
  }
  bool IncludeBorderBottom() const {
    return bit_field_.get<IncludeBorderBottomFlag>();
  }
  bool IncludeBorderLeft() const {
    return bit_field_.get<IncludeBorderLeftFlag>();
  }
  bool HasBorders() const {
    return RuntimeEnabledFeatures::RareBorderPaddingInflowEnabled()
               ? !!GetRareField(FieldId::kBorders)
               : bit_field_.get<HasBordersFlag>();
  }
  bool HasPadding() const {
    return RuntimeEnabledFeatures::RareBorderPaddingInflowEnabled()
               ? !!GetRareField(FieldId::kPadding)
               : bit_field_.get<HasPaddingFlag>();
  }
  bool HasInflowBounds() const {
    return RuntimeEnabledFeatures::RareBorderPaddingInflowEnabled()
               ? !!GetRareField(FieldId::kInflowBounds)
               : bit_field_.get<HasInflowBoundsFlag>();
  }

  static size_t AdditionalByteSize(bool has_fragment_items,
                                   bool has_layout_overflow,
                                   bool has_borders,
                                   bool has_padding,
                                   bool has_inflow_bounds);

  using FieldId = PhysicalFragmentRareData::FieldId;
  ALWAYS_INLINE const PhysicalFragmentRareData::RareField* GetRareField(
      FieldId id) const {
    if (rare_data_) {
      return rare_data_->GetField(id);
    }
    return nullptr;
  }
  PhysicalFragmentRareData::RareField& EnsureRareField(FieldId id);

  const NGFragmentItems* ComputeItemsAddress() const {
#if DCHECK_IS_ON()
    if (RuntimeEnabledFeatures::LayoutOverflowNoCloneEnabled()) {
      DCHECK(HasItems() || HasBorders() || HasPadding() || HasInflowBounds());
    } else {
      DCHECK(HasItems() || HasLayoutOverflow() || HasBorders() ||
             HasPadding() || HasInflowBounds());
    }
#endif
    return reinterpret_cast<const NGFragmentItems*>(base::bits::AlignUp(
        reinterpret_cast<const uint8_t*>(this + 1), alignof(NGFragmentItems)));
  }

  const PhysicalRect* ComputeLayoutOverflowAddress() const {
    DCHECK(!RuntimeEnabledFeatures::LayoutOverflowNoCloneEnabled());
    DCHECK(HasLayoutOverflow() || HasBorders() || HasPadding() ||
           HasInflowBounds());
    const NGFragmentItems* items = ComputeItemsAddress();
    const uint8_t* unaligned_layout_overflow =
        HasItems() ? reinterpret_cast<const uint8_t*>(items + 1)
                   : reinterpret_cast<const uint8_t*>(items);
    return reinterpret_cast<const PhysicalRect*>(
        base::bits::AlignUp(unaligned_layout_overflow, alignof(PhysicalRect)));
  }

  const NGPhysicalBoxStrut* ComputeBordersAddress() const {
    DCHECK(!RuntimeEnabledFeatures::RareBorderPaddingInflowEnabled());
    DCHECK(HasBorders() || HasPadding() || HasInflowBounds());
    if (RuntimeEnabledFeatures::LayoutOverflowNoCloneEnabled()) {
      const NGFragmentItems* address = ComputeItemsAddress();
      const uint8_t* unaligned_border_address =
          HasItems() ? reinterpret_cast<const uint8_t*>(address + 1)
                     : reinterpret_cast<const uint8_t*>(address);
      return reinterpret_cast<const NGPhysicalBoxStrut*>(base::bits::AlignUp(
          unaligned_border_address, alignof(NGPhysicalBoxStrut)));
    }
    const PhysicalRect* address = ComputeLayoutOverflowAddress();
    const uint8_t* unaligned_border_address =
        HasLayoutOverflow() ? reinterpret_cast<const uint8_t*>(address + 1)
                            : reinterpret_cast<const uint8_t*>(address);
    return reinterpret_cast<const NGPhysicalBoxStrut*>(base::bits::AlignUp(
        unaligned_border_address, alignof(NGPhysicalBoxStrut)));
  }

  const NGPhysicalBoxStrut* ComputePaddingAddress() const {
    DCHECK(!RuntimeEnabledFeatures::RareBorderPaddingInflowEnabled());
    DCHECK(HasPadding() || HasInflowBounds());
    const NGPhysicalBoxStrut* address = ComputeBordersAddress();
    const uint8_t* unaligned_address =
        HasBorders() ? reinterpret_cast<const uint8_t*>(address + 1)
                     : reinterpret_cast<const uint8_t*>(address);
    return reinterpret_cast<const NGPhysicalBoxStrut*>(
        base::bits::AlignUp(unaligned_address, alignof(NGPhysicalBoxStrut)));
  }

  const PhysicalRect* ComputeInflowBoundsAddress() const {
    DCHECK(!RuntimeEnabledFeatures::RareBorderPaddingInflowEnabled());
    DCHECK(HasInflowBounds());
    NGPhysicalBoxStrut* address =
        const_cast<NGPhysicalBoxStrut*>(ComputePaddingAddress());
    const uint8_t* unaligned_address =
        HasPadding() ? reinterpret_cast<const uint8_t*>(address + 1)
                     : reinterpret_cast<const uint8_t*>(address);
    return reinterpret_cast<const PhysicalRect*>(
        base::bits::AlignUp(unaligned_address, alignof(PhysicalRect)));
  }

  void SetInkOverflow(const PhysicalRect& self, const PhysicalRect& contents);
  void SetInkOverflowType(NGInkOverflow::Type type) {
    bit_field_.set<InkOverflowTypeValue>(static_cast<uint8_t>(type));
  }
  PhysicalRect RecalcContentsInkOverflow();
  PhysicalRect ComputeSelfInkOverflow() const;

  void AddOutlineRects(const PhysicalOffset& additional_offset,
                       NGOutlineType include_block_overflows,
                       bool inline_container_relative,
                       OutlineRectCollector& collector) const;
  void AddOutlineRectsForInlineBox(PhysicalOffset additional_offset,
                                   NGOutlineType include_block_overflows,
                                   bool inline_container_relative,
                                   OutlineRectCollector& collector) const;

  PositionWithAffinity PositionForPointByClosestChild(
      PhysicalOffset point_in_contents) const;

  PositionWithAffinity PositionForPointInBlockFlowDirection(
      PhysicalOffset point_in_contents) const;

  PositionWithAffinity PositionForPointInTable(
      PhysicalOffset point_in_contents) const;

  PositionWithAffinity PositionForPointRespectingEditingBoundaries(
      const NGPhysicalBoxFragment& child,
      PhysicalOffset point_in_child) const;

#if DCHECK_IS_ON()
  void CheckIntegrity() const;
#endif

  BitField bit_field_;

  LayoutUnit first_baseline_;
  LayoutUnit last_baseline_;
  Member<PhysicalFragmentRareData> rare_data_;
  NGInkOverflow ink_overflow_;
  HeapVector<NGLink> children_;
  // fragment_items is after |children_| if they are not empty/initial.
};

template <>
struct DowncastTraits<NGPhysicalBoxFragment> {
  static bool AllowFrom(const NGPhysicalFragment& fragment) {
    return fragment.Type() == NGPhysicalFragment::kFragmentBox;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_PHYSICAL_BOX_FRAGMENT_H_
