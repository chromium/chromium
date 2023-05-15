// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_INK_OVERFLOW_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_INK_OVERFLOW_H_

#include "base/check_op.h"
#include "base/dcheck_is_on.h"
#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"

namespace blink {

class AffineTransform;
class AppliedTextDecoration;
class ComputedStyle;
class Font;
class NGFragmentItem;
class NGInlineCursor;
class NGInlinePaintContext;
struct NGTextFragmentPaintInfo;
class Text;

// Represents an ink-overflow rectangle. Used for:
// - Objects without children, such as text runs.
// - Objects that has only self or contents ink-overflow.
struct NGSingleInkOverflow {
  USING_FAST_MALLOC(NGSingleInkOverflow);

 public:
  explicit NGSingleInkOverflow(const PhysicalRect& ink_overflow)
      : ink_overflow(ink_overflow) {}

  PhysicalRect ink_overflow;
};

// Represents two ink-overflow rectangles, to keep self and contents ink
// overflow separately. Used for objects with children, such as boxes.
struct NGContainerInkOverflow : NGSingleInkOverflow {
  USING_FAST_MALLOC(NGContainerInkOverflow);

 public:
  NGContainerInkOverflow(const PhysicalRect& self, const PhysicalRect& contents)
      : NGSingleInkOverflow(self), contents_ink_overflow(contents) {}

  PhysicalRect SelfAndContentsInkOverflow() const {
    return UnionRect(ink_overflow, contents_ink_overflow);
  }

  PhysicalRect contents_ink_overflow;
};

// Represents multiple types of ink overflow in a size of a pointer.
//
// When no overflow, or when overflow is small, this class does not allocate
// memory.
//
// In order to keep the instance small, callers must keep |Type| separately.
// |Set*| functions return |Type|, which callers must keep and pass to following
// function calls. Functions have DCHECKs to ensure callers pass the correct
// |Type|.
class CORE_EXPORT NGInkOverflow {
 public:
  enum class Type {
    kNotSet,
    kInvalidated,
    kNone,
    kSmallSelf,
    kSelf,
    kSmallContents,
    kContents,
    kSelfAndContents
    // When adding values, make sure |NGFragmentItem| has enough storage.
  };
  constexpr static int kTypeBits = 3;

  NGInkOverflow() = default;
#if DCHECK_IS_ON()
  ~NGInkOverflow();
#endif

  // Regular copy is prohibited because |Type| is outside of the instance. Use
  // functions with |Type| below instead.
  NGInkOverflow(const NGInkOverflow&) = delete;
  NGInkOverflow& operator=(const NGInkOverflow&) = delete;

  // To copy/move, |Type| is required.
  NGInkOverflow(Type source_type, const NGInkOverflow& source);
  NGInkOverflow(Type source_type, NGInkOverflow&& source);

  // Get ink overflow of various types.
  PhysicalRect Self(Type type, const PhysicalSize& size) const;
  PhysicalRect Contents(Type type, const PhysicalSize& size) const;
  PhysicalRect SelfAndContents(Type type, const PhysicalSize& size) const;

  // Reset to |kNone|.
  Type Reset(Type type) { return Reset(type, Type::kNone); }
  // Reset to |kInvalidated|.
  Type Invalidate(Type type) { return Reset(type, Type::kInvalidated); }

  // Set self ink overflow rect.
  // If |this| had contents ink overflow, it is cleared.
  Type SetSelf(Type type, const PhysicalRect& self, const PhysicalSize& size);

  // Set contents ink overflow rect.
  // If |this| had self ink overflow, it is cleared.
  Type SetContents(Type type,
                   const PhysicalRect& contents,
                   const PhysicalSize& size);

  // Set self and contents ink overflow rects.
  Type Set(Type type,
           const PhysicalRect& self,
           const PhysicalRect& contents,
           const PhysicalSize& size);

  // Compute and set ink overflow for text.
  Type SetTextInkOverflow(Type type,
                          const NGInlineCursor& cursor,
                          const NGTextFragmentPaintInfo& text_info,
                          const ComputedStyle& style,
                          const PhysicalRect& rect_in_container,
                          const NGInlinePaintContext* inline_context,
                          PhysicalRect* ink_overflow_out);

  // Compute and set ink overflow for SVG text.
  // |rect| represents scaled rectangle, and |*ink_overflow_out| will store
  // unscaled rectangle.
  Type SetSvgTextInkOverflow(Type type,
                             const NGInlineCursor& cursor,
                             const NGTextFragmentPaintInfo& text_info,
                             const ComputedStyle& style,
                             const Font& scaled_font,
                             const gfx::RectF& rect,
                             float scaling_factor,
                             float length_adjust_scale,
                             const AffineTransform& transform,
                             PhysicalRect* ink_overflow_out);

  static absl::optional<PhysicalRect> ComputeTextInkOverflow(
      const NGInlineCursor& cursor,
      const NGTextFragmentPaintInfo& text_info,
      const ComputedStyle& style,
      const Font& scaled_font,
      const PhysicalRect& rect_in_container,
      const NGInlinePaintContext* inline_context);

  // Returns ink-overflow with emphasis mark overflow in logical direction.
  // |size| is a size of text item, e.g. |NGFragmentItem::Size()|.
  // Note: |style| should have emphasis mark and |ink_overflow| should be in
  // logical direction.
  static LayoutRect ComputeEmphasisMarkOverflow(const ComputedStyle& style,
                                                const PhysicalSize& size,
                                                const LayoutRect& ink_overflow);

  // Returns ink-overflow with text decoration overflow in logical direction.
  // |inline_context| may be null.
  // Note: |ink_overflow| should be in logical direction.
  // Returns ink-overflow with text decoration, markers and highlights
  // overflow in the logical direction.
  static LayoutRect ComputeDecorationOverflow(
      const NGInlineCursor& cursor,
      const ComputedStyle& style,
      const Font& scaled_font,
      const PhysicalOffset& container_offset,
      const LayoutRect& ink_overflow,
      const NGInlinePaintContext* inline_context);

#if DCHECK_IS_ON()
  struct ReadUnsetAsNoneScope {
    STACK_ALLOCATED();

   public:
    ReadUnsetAsNoneScope() { ++read_unset_as_none_; }
    ~ReadUnsetAsNoneScope() { --read_unset_as_none_; }

    static bool IsActive() { return read_unset_as_none_; }
  };
#endif

 private:
  static LayoutRect ComputeAppliedDecorationOverflow(
      const ComputedStyle& style,
      const Font& scaled_font,
      const PhysicalOffset& offset_in_container,
      const LayoutRect& ink_overflow,
      const NGInlinePaintContext* inline_context,
      const AppliedTextDecoration* decoration_override = nullptr);

  static LayoutRect ComputeSpellingOrGrammarOverflow(
      const DocumentMarkerVector& markers,
      const DocumentMarker::MarkerType type,
      const NGFragmentItem* fragment_item,
      Text* node,
      const ComputedStyle& style,
      const Font& scaled_font,
      const PhysicalOffset& offset_in_container,
      const LayoutRect& ink_overflow,
      const NGInlinePaintContext* inline_context);

  PhysicalRect FromOutsets(const PhysicalSize& size) const;

  void CheckType(Type type) const;
  Type SetType(Type type);

  Type Reset(Type type, Type new_type);

  bool TrySetOutsets(Type type,
                     LayoutUnit left_outset,
                     LayoutUnit top_outset,
                     LayoutUnit right_outset,
                     LayoutUnit bottom_outset);
  Type SetSingle(Type type,
                 const PhysicalRect& ink_overflow,
                 const PhysicalSize& size,
                 Type new_type,
                 Type new_small_type);

  // |SmallRawValue| can store small values without allocating memory.
  // |LayoutUnit| uses 6 bits (|kLayoutUnitFractionalBits|) for fraction.
#if defined(ARCH_CPU_32_BITS)
  // |uint8_t| can represent 0 to (4 - 1/64) using 2 bits for integer.
  using SmallRawValue = uint8_t;
#elif defined(ARCH_CPU_64_BITS)
  // |uint16_t| can represent 0 to (1024 - 1/64) using 10 bits for integer.
  using SmallRawValue = uint16_t;
#else
#error Only support 32/64 bits.
#endif

  union {
    // When only self or contents overflow.
    NGSingleInkOverflow* single_;
    // When both self and contents overflow.
    NGContainerInkOverflow* container_;
    // Outsets in small |LayoutUnit|s when overflow is small.
    SmallRawValue outsets_[4];
    static_assert(sizeof(outsets_) == sizeof(single_),
                  "outsets should be the size of a pointer");
  };

#if DCHECK_IS_ON()
  Type type_ = Type::kNotSet;

  static unsigned read_unset_as_none_;
#endif
};

#if DCHECK_IS_ON()
inline void NGInkOverflow::CheckType(Type type) const {
  DCHECK_EQ(type, type_);
}
inline NGInkOverflow::Type NGInkOverflow::SetType(Type type) {
  type_ = type;
  return type;
}
#else
inline void NGInkOverflow::CheckType(Type type) const {}
inline NGInkOverflow::Type NGInkOverflow::SetType(Type type) {
  return type;
}
#endif

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_INK_OVERFLOW_H_
