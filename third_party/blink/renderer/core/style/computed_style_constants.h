/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_COMPUTED_STYLE_CONSTANTS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_COMPUTED_STYLE_CONSTANTS_H_

#include <cstddef>
#include <cstdint>

#include "base/check_op.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_animation_trigger_behavior.h"
#include "third_party/blink/renderer/core/style/computed_style_base_constants.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

template <typename Enum>
inline bool EnumHasFlags(Enum v, Enum mask) {
  return static_cast<unsigned>(v) & static_cast<unsigned>(mask);
}

// Some enums are automatically generated in ComputedStyleBaseConstants

// TODO(sashab): Change these enums to enum classes with an unsigned underlying
// type. Enum classes provide better type safety, and forcing an unsigned
// underlying type prevents msvc from interpreting enums as negative numbers.
// See: crbug.com/628043

// Sides used when drawing borders and outlines. The values should run clockwise
// from top.
enum class BoxSide : unsigned { kTop, kRight, kBottom, kLeft };

// Static pseudo styles. Dynamic ones are produced on the fly.
enum PseudoId : uint8_t {
  // The order must be NOP ID, public IDs, and then internal IDs.
  // If you add or remove a public ID, you must update the field_size of
  // "PseudoElementStyles" in computed_style_extra_fields.json5 to
  // (kLastTrackedPublicPseudoId - kFirstPublicPseudoId + 1).
  //
  // The above is necessary because presence of a public pseudo-element style
  // for an element is tracked on the element's ComputedStyle. This is done for
  // all public IDs until kLastTrackedPublicPseudoId.
  kPseudoIdNone,
  kPseudoIdFirstLine,
  kPseudoIdFirstLetter,
  kPseudoIdCheckMark,
  kPseudoIdBefore,
  kPseudoIdAfter,
  kPseudoIdPickerIcon,
  kPseudoIdInterestHint,
  kPseudoIdMarker,
  kPseudoIdBackdrop,
  kPseudoIdSelection,
  kPseudoIdScrollbar,
  kPseudoIdScrollMarker,
  kPseudoIdScrollMarkerGroup,
  kPseudoIdScrollButton,
  kPseudoIdScrollButtonBlockStart,
  kPseudoIdScrollButtonInlineStart,
  kPseudoIdScrollButtonInlineEnd,
  kPseudoIdScrollButtonBlockEnd,
  kPseudoIdColumn,
  kPseudoIdSearchText,
  kPseudoIdTargetText,
  kPseudoIdHighlight,
  kPseudoIdSpellingError,
  kPseudoIdGrammarError,
  // The following IDs are public but not tracked.
  kPseudoIdViewTransition,
  kPseudoIdViewTransitionGroup,
  kPseudoIdViewTransitionGroupChildren,
  kPseudoIdViewTransitionImagePair,
  kPseudoIdViewTransitionOld,
  kPseudoIdViewTransitionNew,

  kPseudoIdOverscrollAreaParent,

  // Internal IDs follow:
  kPseudoIdFirstLineInherited,

  // These five must be together, due to code in
  // CollectMatchingRulesInternal().
  kPseudoIdScrollbarThumb,
  kPseudoIdScrollbarButton,
  kPseudoIdScrollbarTrack,
  kPseudoIdScrollbarTrackPiece,
  kPseudoIdScrollbarCorner,

  kPseudoIdScrollMarkerGroupAfter,
  kPseudoIdScrollMarkerGroupBefore,
  kPseudoIdResizer,
  kPseudoIdInputListButton,
  kPseudoIdPlaceholder,
  kPseudoIdFileSelectorButton,
  kPseudoIdDetailsContent,
  kPseudoIdPickerSelect,
  kPseudoIdPermissionIcon,

  // Special values follow:
  kAfterLastInternalPseudoId,
  kPseudoIdInvalid,
  kFirstPublicPseudoId = kPseudoIdFirstLine,
  kLastTrackedPublicPseudoId = kPseudoIdGrammarError,
  kLastPublicPseudoId = kPseudoIdOverscrollAreaParent,
  kFirstInternalPseudoId = kPseudoIdFirstLineInherited,
};

// Stores a set of PseudoId flags, but only in the range
// [kFirstPublicPseudoId, kLastTrackedPublicPseudoId].
class PseudoIdFlags {
 public:
  PseudoIdFlags() = default;

  static const PseudoId kFirstValid = kFirstPublicPseudoId;
  static const PseudoId kLastValid = kLastTrackedPublicPseudoId;

  static PseudoIdFlags FromBits(uint32_t bits) { return PseudoIdFlags(bits); }

  // See comment on similar constructor in CSSBitsetBase.
  template <int N>
  explicit constexpr PseudoIdFlags(const PseudoId (&list)[N]) {
    for (PseudoId pseudo_id : list) {
      bits_ |= uint32_t{1} << Bit(pseudo_id);
    }
  }

  bool operator==(const PseudoIdFlags& o) const { return bits_ == o.bits_; }

  PseudoIdFlags& operator|=(const PseudoIdFlags& o) {
    bits_ |= o.bits_;
    return *this;
  }

  void Set(PseudoId pseudo_id) {
    DCHECK_LT(Bit(pseudo_id), 32u);
    bits_ |= (uint32_t{1} << Bit(pseudo_id));
  }

  void MaybeSet(PseudoId pseudo_id) {
    if (pseudo_id >= kFirstValid && pseudo_id <= kLastValid) {
      Set(pseudo_id);
    }
  }

  bool Has(PseudoId pseudo_id) const {
    DCHECK_LT(Bit(pseudo_id), 32u);
    return bits_ & (uint32_t{1} << Bit(pseudo_id));
  }

  bool HasAny() const { return bits_; }

  uint32_t Bits() const { return bits_; }

 private:
  explicit PseudoIdFlags(uint32_t bits) : bits_(bits) {}

  static constexpr uint32_t Bit(PseudoId pseudo_id) {
    return pseudo_id - kFirstValid;
  }

  static_assert((kLastValid - kFirstValid) < 32);
  uint32_t bits_ = 0;
};

inline bool IsHighlightPseudoElement(PseudoId pseudo_id) {
  switch (pseudo_id) {
    case kPseudoIdSelection:
    case kPseudoIdSearchText:
    case kPseudoIdTargetText:
    case kPseudoIdHighlight:
    case kPseudoIdSpellingError:
    case kPseudoIdGrammarError:
      return true;
    default:
      return false;
  }
}

inline bool IsTransitionPseudoElement(PseudoId pseudo_id) {
  switch (pseudo_id) {
    case kPseudoIdViewTransition:
    case kPseudoIdViewTransitionGroup:
    case kPseudoIdViewTransitionGroupChildren:
    case kPseudoIdViewTransitionImagePair:
    case kPseudoIdViewTransitionOld:
    case kPseudoIdViewTransitionNew:
      return true;
    default:
      return false;
  }
}

inline bool PseudoElementHasArguments(PseudoId pseudo_id) {
  switch (pseudo_id) {
    case kPseudoIdHighlight:
    case kPseudoIdViewTransitionGroup:
    case kPseudoIdViewTransitionGroupChildren:
    case kPseudoIdViewTransitionImagePair:
    case kPseudoIdViewTransitionNew:
    case kPseudoIdViewTransitionOld:
      return true;
    default:
      return false;
  }
}

enum class OutlineIsAuto : bool { kOff = false, kOn = true };

// Random visual rendering model attributes. Not inherited.

enum class EVerticalAlign : unsigned {
  kBaseline,
  kMiddle,
  kSub,
  kSuper,
  kTextTop,
  kTextBottom,
  kTop,
  kBottom,
  kBaselineMiddle,
  kLength
};

enum class EFillAttachment : unsigned { kScroll, kLocal, kFixed };

// `EFillBox` is used for {-webkit-}background-clip, {-webkit-}mask-clip, and
// {-webkit-}mask-origin. Not all properties support all of these values.
//
// Background-clip (https://drafts.csswg.org/css-backgrounds/#background-clip)
// supports <visual-box> (border-box, padding-box, content-box), as well as the
// non-standard `text` value.
//
// Mask-clip (https://drafts.fxtf.org/css-masking/#the-mask-clip) supports
// <coord-box> (border-box, padding-box, content-box, fill-box, stroke-box,
// view-box), `no-clip`, as well as the non-standard `text` value.
//
// Mask-origin (https://drafts.fxtf.org/css-masking/#the-mask-origin) supports
// <coord-box> (border-box, padding-box, content-box, fill-box, stroke-box,
// view-box).
enum class EFillBox : unsigned {
  kBorder,
  kPadding,
  kContent,
  kText,
  kFillBox,
  kStrokeBox,
  kViewBox,
  kNoClip
};

inline EFillBox EnclosingFillBox(EFillBox box_a, EFillBox box_b) {
  if (box_a == EFillBox::kNoClip || box_b == EFillBox::kNoClip) {
    return EFillBox::kNoClip;
  }
  if (box_a == EFillBox::kViewBox || box_b == EFillBox::kViewBox) {
    return EFillBox::kViewBox;
  }
  if (box_a == EFillBox::kStrokeBox || box_b == EFillBox::kStrokeBox) {
    return EFillBox::kStrokeBox;
  }
  // background-clip:text is clipped to the border box.
  if (box_a == EFillBox::kBorder || box_a == EFillBox::kText ||
      box_b == EFillBox::kBorder || box_b == EFillBox::kText) {
    return EFillBox::kBorder;
  }
  if (box_a == EFillBox::kPadding || box_b == EFillBox::kPadding) {
    return EFillBox::kPadding;
  }
  if (box_a == EFillBox::kFillBox || box_b == EFillBox::kFillBox) {
    return EFillBox::kFillBox;
  }
  DCHECK_EQ(box_a, EFillBox::kContent);
  DCHECK_EQ(box_b, EFillBox::kContent);
  return EFillBox::kContent;
}

enum class EFillRepeat : unsigned {
  kRepeatFill,
  kNoRepeatFill,
  kRoundFill,
  kSpaceFill
};

enum class EFillMaskMode : unsigned { kAlpha, kLuminance, kMatchSource };

enum class EFillLayerType : unsigned { kBackground, kMask };

// CSS3 Background Values
enum class EFillSizeType : unsigned {
  kContain,
  kCover,
  kSizeLength,
  kSizeNone
};

// CSS3 Background Position
enum class BackgroundEdgeOrigin : unsigned { kTop, kRight, kBottom, kLeft };

// CSS3 Image Values
enum class QuoteType : unsigned { kOpen, kClose, kNoOpen, kNoClose };

enum class EAnimPlayState : unsigned { kPlaying, kPaused };

enum class OffsetRotationType : unsigned { kAuto, kFixed };

static const size_t kGridAutoFlowBits = 4;
enum InternalGridAutoFlowAlgorithm {
  kInternalAutoFlowAlgorithmSparse = 0x1,
  kInternalAutoFlowAlgorithmDense = 0x2
};

enum InternalGridAutoFlowDirection {
  kInternalAutoFlowDirectionRow = 0x4,
  kInternalAutoFlowDirectionColumn = 0x8
};

enum GridAutoFlow {
  kAutoFlowRow = int(kInternalAutoFlowAlgorithmSparse) |
                 int(kInternalAutoFlowDirectionRow),
  kAutoFlowColumn = int(kInternalAutoFlowAlgorithmSparse) |
                    int(kInternalAutoFlowDirectionColumn),
  kAutoFlowRowDense =
      int(kInternalAutoFlowAlgorithmDense) | int(kInternalAutoFlowDirectionRow),
  kAutoFlowColumnDense = int(kInternalAutoFlowAlgorithmDense) |
                         int(kInternalAutoFlowDirectionColumn)
};

static const size_t kContainmentBits = 6;
enum Containment {
  kContainsNone = 0x0,
  kContainsLayout = 0x1,
  kContainsStyle = 0x2,
  kContainsPaint = 0x4,
  kContainsBlockSize = 0x8,
  kContainsInlineSize = 0x10,
  kContainsViewTransition = 0x20,
  kContainsSize = kContainsBlockSize | kContainsInlineSize,
  kContainsStrict =
      kContainsStyle | kContainsLayout | kContainsPaint | kContainsSize,
  kContainsContent = kContainsStyle | kContainsLayout | kContainsPaint,
};
inline Containment operator|(Containment a, Containment b) {
  return Containment(int(a) | int(b));
}
inline Containment& operator|=(Containment& a, Containment b) {
  return a = a | b;
}

static const size_t kContainerTypeBits = 4;
enum EContainerType {
  kContainerTypeNormal = 0x0,
  kContainerTypeInlineSize = 0x1,
  kContainerTypeBlockSize = 0x2,
  kContainerTypeScrollState = 0x4,
  kContainerTypeAnchored = 0x8,
  kContainerTypeSize = kContainerTypeInlineSize | kContainerTypeBlockSize,
};
inline EContainerType operator|(EContainerType a, EContainerType b) {
  return EContainerType(int(a) | int(b));
}
inline EContainerType& operator|=(EContainerType& a, EContainerType b) {
  return a = a | b;
}

static const size_t kTextUnderlinePositionBits = 4;
enum class TextUnderlinePosition : unsigned {
  kAuto = 0x0,
  kFromFont = 0x1,
  kUnder = 0x2,
  kLeft = 0x4,
  kRight = 0x8
};
inline TextUnderlinePosition operator|(TextUnderlinePosition a,
                                       TextUnderlinePosition b) {
  return TextUnderlinePosition(int(a) | int(b));
}
inline TextUnderlinePosition& operator|=(TextUnderlinePosition& a,
                                         TextUnderlinePosition b) {
  return a = a | b;
}

enum class ItemPosition : unsigned {
  kLegacy,
  kAuto,
  kNormal,
  kStretch,
  kBaseline,
  kLastBaseline,
  kAnchorCenter,
  kCenter,
  kStart,
  kEnd,
  kSelfStart,
  kSelfEnd,
  kFlexStart,
  kFlexEnd,
  kLeft,
  kRight
};

enum class OverflowAlignment : unsigned { kDefault, kUnsafe, kSafe };

enum class ItemPositionType : unsigned { kNonLegacy, kLegacy };

enum class ContentPosition : unsigned {
  kNormal,
  kBaseline,
  kLastBaseline,
  kCenter,
  kStart,
  kEnd,
  kFlexStart,
  kFlexEnd,
  kLeft,
  kRight
};

enum class ContentDistributionType : unsigned {
  kDefault,
  kSpaceBetween,
  kSpaceAround,
  kSpaceEvenly,
  kStretch
};

// Reasonable maximum to prevent insane font sizes from causing crashes on some
// platforms (such as Windows).
static const float kMaximumAllowedFontSize = 10000.0f;

enum class CSSBoxType : unsigned {
  kMissing,
  kMargin,
  kBorder,
  kPadding,
  kContent
};

enum class TextEmphasisPosition : unsigned {
  kOverRight,
  kOverLeft,
  kUnderRight,
  kUnderLeft,
  kAuto,
};

inline bool IsOver(TextEmphasisPosition position) {
  return position == TextEmphasisPosition::kOverRight ||
         position == TextEmphasisPosition::kOverLeft;
}

inline bool IsRight(TextEmphasisPosition position) {
  return position == TextEmphasisPosition::kOverRight ||
         position == TextEmphasisPosition::kUnderRight;
}

inline bool IsLeft(TextEmphasisPosition position) {
  return !IsRight(position);
}

enum class LineLogicalSide {
  kOver,
  kUnder,
};
inline bool operator==(LineLogicalSide line_logical_side,
                       RubyPosition ruby_position) {
  return (line_logical_side == LineLogicalSide::kOver &&
          ruby_position == RubyPosition::kOver) ||
         (line_logical_side == LineLogicalSide::kUnder &&
          ruby_position == RubyPosition::kUnder);
}

constexpr size_t kScrollbarGutterBits = 2;
enum ScrollbarGutter {
  kScrollbarGutterAuto = 0x0,
  kScrollbarGutterStable = 0x1,
  kScrollbarGutterBothEdges = 0x2,
};
inline ScrollbarGutter operator|(ScrollbarGutter a, ScrollbarGutter b) {
  return ScrollbarGutter(int(a) | int(b));
}
inline ScrollbarGutter& operator|=(ScrollbarGutter& a, ScrollbarGutter b) {
  return a = a | b;
}

enum class EBaselineShiftType : unsigned { kLength, kSub, kSuper };

enum EPaintOrderType : uint8_t {
  PT_NONE = 0,
  PT_FILL = 1,
  PT_STROKE = 2,
  PT_MARKERS = 3
};

enum EPaintOrder {
  kPaintOrderNormal,
  kPaintOrderFillStrokeMarkers,
  kPaintOrderFillMarkersStroke,
  kPaintOrderStrokeFillMarkers,
  kPaintOrderStrokeMarkersFill,
  kPaintOrderMarkersFillStroke,
  kPaintOrderMarkersStrokeFill
};

constexpr size_t kViewportUnitFlagBits = 2;
enum class ViewportUnitFlag {
  // v*, sv*, lv*
  kStatic = 0x1,
  // dv*
  kDynamic = 0x2,
};

enum class TimelineAxis { kBlock, kInline, kX, kY };
enum class TimelineScroller { kNearest, kRoot, kSelf };

enum class CoordBox {
  kContentBox,
  kPaddingBox,
  kBorderBox,
  kFillBox,
  kStrokeBox,
  kViewBox
};

// https://drafts.fxtf.org/css-masking/#typedef-geometry-box
enum class GeometryBox {
  // <box> = border-box | padding-box | content-box
  kBorderBox,
  kPaddingBox,
  kContentBox,
  // <shape-box> = <box> | margin-box
  kMarginBox,
  // <geometry-box> = <shape-box> | fill-box | stroke-box | view-box
  kFillBox,
  kStrokeBox,
  kViewBox,
  // Additional value for border-shape: a box halfway between border and padding
  kHalfBorderBox
};

// https://drafts.fxtf.org/css-masking/#typedef-compositing-operator
enum class CompositingOperator : unsigned {
  // <compositing-operator> = add | subtract | intersect | exclude
  kAdd,
  kSubtract,
  kIntersect,
  kExclude,

  // The following are non-standard values used by -webkit-mask-composite.
  kClear,
  kCopy,
  kSourceOver,
  kSourceIn,
  kSourceOut,
  kSourceAtop,
  kDestinationOver,
  kDestinationIn,
  kDestinationOut,
  kDestinationAtop,
  kXOR,
  kPlusLighter
};

// https://drafts.csswg.org/css-anchor-position-1/#typedef-position-try-fallbacks-try-tactic
enum class TryTactic : uint8_t {
  kNone,
  kFlipBlock,
  kFlipInline,
  kFlipStart,
  kFlipX,
  kFlipY,
};

typedef V8AnimationTriggerBehavior::Enum EAnimationTriggerBehavior;

// TODO(crbug.com/332933527): Support anchors-valid.
static const size_t kPositionVisibilityBits = 2;
enum class PositionVisibility : uint8_t {
  kAlways = 0x0,
  kAnchorsVisible = 0x1,
  kNoOverflow = 0x2,
};
inline PositionVisibility operator|(PositionVisibility a,
                                    PositionVisibility b) {
  return PositionVisibility(int(a) | int(b));
}
inline PositionVisibility& operator|=(PositionVisibility& a,
                                      PositionVisibility b) {
  return a = a | b;
}

inline PositionVisibility InitialPositionVisibilityKeyword() {
  if (RuntimeEnabledFeatures::AnchorsVisibleInitialValueEnabled()) {
    return PositionVisibility::kAnchorsVisible;
  }
  return PositionVisibility::kAlways;
}

enum class FlexWrapMode : uint8_t { kNowrap, kWrap, kWrapReverse };

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_COMPUTED_STYLE_CONSTANTS_H_
