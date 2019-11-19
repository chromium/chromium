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
#include "third_party/blink/renderer/core/style/computed_style_base_constants.h"

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
enum PseudoId {
  // The order must be NOP ID, public IDs, and then internal IDs.
  // If you add or remove a public ID, you must update the field_size of
  // "PseudoBits" in computed_style_extra_fields.json5.
  kPseudoIdNone,
  kPseudoIdFirstLine,
  kPseudoIdFirstLetter,
  kPseudoIdBefore,
  kPseudoIdAfter,
  kPseudoIdMarker,
  kPseudoIdBackdrop,
  kPseudoIdSelection,
  kPseudoIdScrollbar,
  // Internal IDs follow:
  kPseudoIdFirstLineInherited,
  kPseudoIdScrollbarThumb,
  kPseudoIdScrollbarButton,
  kPseudoIdScrollbarTrack,
  kPseudoIdScrollbarTrackPiece,
  kPseudoIdScrollbarCorner,
  kPseudoIdResizer,
  kPseudoIdInputListButton,
  // Special values follow:
  kAfterLastInternalPseudoId,
  kFirstPublicPseudoId = kPseudoIdFirstLine,
  kFirstInternalPseudoId = kPseudoIdFirstLineInherited,
  kElementPseudoIdMask = (1 << (kPseudoIdBefore - kFirstPublicPseudoId)) |
                         (1 << (kPseudoIdAfter - kFirstPublicPseudoId)) |
                         (1 << (kPseudoIdMarker - kFirstPublicPseudoId)) |
                         (1 << (kPseudoIdBackdrop - kFirstPublicPseudoId))
};

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

enum class EFillBox : unsigned { kBorder, kPadding, kContent, kText };

inline EFillBox EnclosingFillBox(EFillBox box_a, EFillBox box_b) {
  if (box_a == EFillBox::kBorder || box_b == EFillBox::kBorder)
    return EFillBox::kBorder;
  if (box_a == EFillBox::kPadding || box_b == EFillBox::kPadding)
    return EFillBox::kPadding;
  if (box_a == EFillBox::kContent || box_b == EFillBox::kContent)
    return EFillBox::kContent;
  return EFillBox::kText;
}

enum class EFillRepeat : unsigned {
  kRepeatFill,
  kNoRepeatFill,
  kRoundFill,
  kSpaceFill
};

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

// CSS Mask Source Types
enum class EMaskSourceType : unsigned { kAlpha, kLuminance };

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
  kAutoFlowRow =
      kInternalAutoFlowAlgorithmSparse | kInternalAutoFlowDirectionRow,
  kAutoFlowColumn =
      kInternalAutoFlowAlgorithmSparse | kInternalAutoFlowDirectionColumn,
  kAutoFlowRowDense =
      kInternalAutoFlowAlgorithmDense | kInternalAutoFlowDirectionRow,
  kAutoFlowColumnDense =
      kInternalAutoFlowAlgorithmDense | kInternalAutoFlowDirectionColumn
};

static const size_t kContainmentBits = 4;
enum Containment {
  kContainsNone = 0x0,
  kContainsLayout = 0x1,
  kContainsStyle = 0x2,
  kContainsPaint = 0x4,
  kContainsSize = 0x8,
  kContainsStrict = kContainsLayout | kContainsPaint | kContainsSize,
  kContainsContent = kContainsLayout | kContainsPaint,
};
inline Containment operator|(Containment a, Containment b) {
  return Containment(int(a) | int(b));
}
inline Containment& operator|=(Containment& a, Containment b) {
  return a = a | b;
}

static const size_t kTextUnderlinePositionBits = 3;
enum TextUnderlinePosition {
  kTextUnderlinePositionAuto = 0x0,
  kTextUnderlinePositionUnder = 0x1,
  kTextUnderlinePositionLeft = 0x2,
  kTextUnderlinePositionRight = 0x4
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
};

enum class LineLogicalSide {
  kOver,
  kUnder,
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_COMPUTED_STYLE_CONSTANTS_H_
