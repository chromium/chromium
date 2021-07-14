// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_layout_invalidation_reason.h"

namespace blink {

namespace layout_invalidation_reason {
const char kUnknown[] = "Unknown";
const char kSizeChanged[] = "Size changed";
const char kAncestorMoved[] = "Ancestor moved";
const char kStyleChange[] = "Style changed";
const char kDomChanged[] = "DOM changed";
const char kTextChanged[] = "Text changed";
const char kPrintingChanged[] = "Printing changed";
const char kAttributeChanged[] = "Attribute changed";
const char kColumnsChanged[] = "Attribute changed";
const char kChildAnonymousBlockChanged[] = "Child anonymous block changed";
const char kAnonymousBlockChange[] = "Anonymous block change";
const char kFontsChanged[] = "Fonts changed";
const char kFullscreen[] = "Fullscreen change";
const char kChildChanged[] = "Child changed";
const char kListValueChange[] = "List value change";
const char kListStyleTypeChange[] = "List style type change";
const char kCounterStyleChange[] = "Counter style change";
const char kImageChanged[] = "Image changed";
const char kLineBoxesChanged[] = "Line boxes changed";
const char kSliderValueChanged[] = "Slider value changed";
const char kAncestorMarginCollapsing[] = "Ancestor margin collapsing";
const char kFieldsetChanged[] = "Fieldset changed";
const char kTextAutosizing[] = "Text autosizing (font boosting)";
const char kSvgResourceInvalidated[] = "SVG resource invalidated";
const char kFloatDescendantChanged[] = "Floating descendant changed";
const char kCountersChanged[] = "Counters changed";
const char kGridChanged[] = "Grid changed";
const char kMenuOptionsChanged[] = "Menu options changed";
const char kRemovedFromLayout[] = "Removed from layout";
const char kAddedToLayout[] = "Added to layout";
const char kTableChanged[] = "Table changed";
const char kPaddingChanged[] = "Padding changed";
const char kTextControlChanged[] = "Text control changed";
const char kSvgChanged[] = "SVG changed";
const char kScrollbarChanged[] = "Scrollbar changed";
const char kDisplayLock[] = "Display lock";
const char kCanvasFormattedTextRunChange[] = "CanvasFormattedText runs changed";
}  // namespace layout_invalidation_reason

}  // namespace blink
