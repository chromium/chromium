// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_ENUMS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_ENUMS_H_

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
  kAriaBrailleLabel,
  kAriaBrailleRoleDescription,
  kAriaKeyShortcuts,
  kAriaRoleDescription,
  // TODO(bebeaudr): kAriaVirtualContent is currently a string attribute to
  // facilitate prototyping. Make it an enum when we're done prototyping.
  kAriaVirtualContent,
};

enum class AXObjectAttribute {
  kAriaActiveDescendant,
  kAriaErrorMessage,
};

enum class AXObjectVectorAttribute {
  kAriaControls,
  kAriaDetails,
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

// The potential native host-language-based text (name, description or
// placeholder) sources for an element.  See
// https://w3c.github.io/html-aam/#accessible-name-and-description-computation
enum AXTextSource {
  kAXTextFromNativeSourceUninitialized = -1,
  kAXTextFromNativeHTMLLabel,
  kAXTextFromNativeHTMLLabelFor,
  kAXTextFromNativeHTMLLabelWrapped,
  kAXTextFromNativeHTMLLegend,
  kAXTextFromNativeHTMLRubyAnnotation,
  kAXTextFromNativeHTMLTableCaption,
  kAXTextFromNativeSVGDescElement,
  kAXTextFromNativeTitleElement,  // HTML and SVG
};

enum AXIgnoredReason {
  kAXActiveFullscreenElement,
  kAXActiveModalDialog,
  kAXAriaModalDialog,
  kAXAriaHiddenElement,
  kAXAriaHiddenSubtree,
  kAXEmptyAlt,
  kAXEmptyText,
  kAXHiddenByChildTree,
  kAXInertElement,
  kAXInertSubtree,
  kAXLabelContainer,
  kAXLabelFor,
  kAXNotRendered,
  kAXNotVisible,
  kAXPresentational,
  kAXProbablyPresentational,
  kAXUninteresting
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_ENUMS_H_
