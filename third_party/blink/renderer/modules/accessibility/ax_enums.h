// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_ENUMS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_ENUMS_H_

#include "third_party/blink/renderer/core/core_export.h"

namespace blink {

enum AccessibilityOrientation {
  kAccessibilityOrientationUndefined = 0,
  kAccessibilityOrientationVertical,
  kAccessibilityOrientationHorizontal,
};

// The input restriction on an object.
enum AXRestriction {
  kRestrictionNone = 0,  // An object that is not disabled.
  kRestrictionReadOnly,
  kRestrictionDisabled,
};

enum AccessibilityExpanded {
  kExpandedUndefined = 0,
  kExpandedCollapsed,
  kExpandedExpanded,
};

enum AccessibilityGrabbedState {
  kGrabbedStateUndefined = 0,
  kGrabbedStateFalse,
  kGrabbedStateTrue,
};

enum AccessibilitySelectedState {
  kSelectedStateUndefined = 0,
  kSelectedStateFalse,
  kSelectedStateTrue,
};

enum class AXBoolAttribute {
  kAriaBusy,
};

enum class AXIntAttribute {
  kAriaColumnCount,
  kAriaRowCount,
};

enum class AXUIntAttribute {
  kAriaColumnIndex,
  kAriaColumnSpan,
  kAriaRowIndex,
  kAriaRowSpan,
};

enum class AXStringAttribute {
  kAriaKeyShortcuts,
  kAriaRoleDescription,
};

enum class AXObjectAttribute {
  kAriaActiveDescendant,
  kAriaDetails,
  kAriaErrorMessage,
};

enum class AXObjectVectorAttribute {
  kAriaControls,
  kAriaFlowTo,
};

enum AXObjectInclusion {
  kIncludeObject,
  kIgnoreObject,
  kDefaultBehavior,
};

enum AccessibilityOptionalBool {
  kOptionalBoolUndefined = 0,
  kOptionalBoolTrue,
  kOptionalBoolFalse
};

// The potential native HTML-based text (name, description or placeholder)
// sources for an element.  See
// http://rawgit.com/w3c/aria/master/html-aam/html-aam.html#accessible-name-and-description-calculation
enum AXTextFromNativeHTML {
  kAXTextFromNativeHTMLUninitialized = -1,
  kAXTextFromNativeHTMLFigcaption,
  kAXTextFromNativeHTMLLabel,
  kAXTextFromNativeHTMLLabelFor,
  kAXTextFromNativeHTMLLabelWrapped,
  kAXTextFromNativeHTMLLegend,
  kAXTextFromNativeHTMLTableCaption,
  kAXTextFromNativeHTMLTitleElement,
};

enum AXIgnoredReason {
  kAXActiveModalDialog,
  kAXAncestorIsLeafNode,
  kAXAriaHiddenElement,
  kAXAriaHiddenSubtree,
  kAXEmptyAlt,
  kAXEmptyText,
  kAXInertElement,
  kAXInertSubtree,
  kAXInheritsPresentation,
  kAXLabelContainer,
  kAXLabelFor,
  kAXNotRendered,
  kAXNotVisible,
  kAXPresentational,
  kAXProbablyPresentational,
  kAXStaticTextUsedAsNameFor,
  kAXUninteresting
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_ENUMS_H_
