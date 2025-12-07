// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_INVALIDATION_REASON_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_INVALIDATION_REASON_H_

namespace blink {

// From a web developer's perspective: what caused this layout? This is strictly
// for tracing. Blink logic must not depend on these.
namespace layout_invalidation_reason {
extern const char kUnknown[];
extern const char kSizeChanged[];
extern const char kAncestorMoved[];
extern const char kStyleChange[];
extern const char kDomChanged[];
extern const char kTextChanged[];
extern const char kPrintingChanged[];
extern const char kPaintPreview[];
extern const char kAttributeChanged[];
extern const char kColumnsChanged[];
extern const char kChildAnonymousBlockChanged[];
extern const char kAnonymousBlockChange[];
extern const char kFontsChanged[];
extern const char kFullscreen[];
extern const char kChildChanged[];
extern const char kListValueChange[];
extern const char kListStyleTypeChange[];
extern const char kCounterStyleChange[];
extern const char kImageChanged[];
extern const char kSliderValueChanged[];
extern const char kAncestorMarginCollapsing[];
extern const char kFieldsetChanged[];
extern const char kTextAutosizing[];
extern const char kSvgResourceInvalidated[];
extern const char kFloatDescendantChanged[];
extern const char kCountersChanged[];
extern const char kGridChanged[];
extern const char kMenuOptionsChanged[];
extern const char kRemovedFromLayout[];
extern const char kAddedToLayout[];
extern const char kTableChanged[];
extern const char kPaddingChanged[];
extern const char kTextControlChanged[];
// FIXME: This is too generic, we should be able to split out transform and
// size related invalidations.
extern const char kSvgChanged[];
extern const char kScrollbarChanged[];
extern const char kDisplayLock[];
extern const char kDevtools[];
extern const char kAnchorPositioning[];
extern const char kScrollMarkersChanged[];
extern const char kOutOfFlowAlignmentChanged[];
}  // namespace layout_invalidation_reason

// LayoutInvalidationReasonForTracing is strictly for tracing. Blink logic must
// not depend on this value.
using LayoutInvalidationReasonForTracing = const char[];

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_INVALIDATION_REASON_H_
