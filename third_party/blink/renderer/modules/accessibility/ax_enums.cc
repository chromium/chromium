// Copyright 2017 The Chromium Authors

#include "third_party/blink/renderer/modules/accessibility/ax_enums.h"

#include "third_party/blink/public/web/web_ax_enums.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

STATIC_ASSERT_ENUM(kWebAXExpandedUndefined, kExpandedUndefined);
STATIC_ASSERT_ENUM(kWebAXExpandedCollapsed, kExpandedCollapsed);
STATIC_ASSERT_ENUM(kWebAXExpandedExpanded, kExpandedExpanded);

STATIC_ASSERT_ENUM(kWebAXOrientationUndefined,
                   kAccessibilityOrientationUndefined);
STATIC_ASSERT_ENUM(kWebAXOrientationVertical,
                   kAccessibilityOrientationVertical);
STATIC_ASSERT_ENUM(kWebAXOrientationHorizontal,
                   kAccessibilityOrientationHorizontal);

STATIC_ASSERT_ENUM(WebAXStringAttribute::kAriaKeyShortcuts,
                   AXStringAttribute::kAriaKeyShortcuts);
STATIC_ASSERT_ENUM(WebAXStringAttribute::kAriaRoleDescription,
                   AXStringAttribute::kAriaRoleDescription);
STATIC_ASSERT_ENUM(WebAXObjectAttribute::kAriaActiveDescendant,
                   AXObjectAttribute::kAriaActiveDescendant);
STATIC_ASSERT_ENUM(WebAXObjectAttribute::kAriaErrorMessage,
                   AXObjectAttribute::kAriaErrorMessage);
STATIC_ASSERT_ENUM(WebAXObjectVectorAttribute::kAriaControls,
                   AXObjectVectorAttribute::kAriaControls);
STATIC_ASSERT_ENUM(WebAXObjectVectorAttribute::kAriaDetails,
                   AXObjectVectorAttribute::kAriaDetails);
STATIC_ASSERT_ENUM(WebAXObjectVectorAttribute::kAriaFlowTo,
                   AXObjectVectorAttribute::kAriaFlowTo);

String IgnoredReasonName(AXIgnoredReason reason) {
  // LINT.IfChange(AXIgnoredReason)
  // This must be kept in sync with the `AXPropertyName` definition in the
  // Chrome DevTools protocol.
  switch (reason) {
    case kAXActiveFullscreenElement:
      return "activeFullscreenElement";
    case kAXActiveModalDialog:
      return "activeModalDialog";
    case kAXAriaModalDialog:
      return "activeAriaModalDialog";
    case kAXAriaHiddenElement:
      return "ariaHiddenElement";
    case kAXAriaHiddenSubtree:
      return "ariaHiddenSubtree";
    case kAXEmptyAlt:
      return "emptyAlt";
    case kAXEmptyText:
      return "emptyText";
    case kAXInertElement:
      return "inertElement";
    case kAXInertSubtree:
      return "inertSubtree";
    case kAXInertStyle:
      // TODO(crbug.com/370065759): Should either use "inertStyle" when devtools
      // can handle that, or just drop kAXInertStyle and use kAXInertElement to
      // indicate that the computed value of interactivity is 'inert'.
      return "inertElement";
    case kAXLabelContainer:
      return "labelContainer";
    case kAXLabelFor:
      return "labelFor";
    case kAXNotRendered:
      return "notRendered";
    case kAXNotVisible:
      return "notVisible";
    case kAXPresentational:
      return "presentationalRole";
    case kAXProbablyPresentational:
      return "probablyPresentational";
    case kAXInactiveCarouselTabContent:
      return "inactiveCarouselTabContent";
    case kAXUninteresting:
      return "uninteresting";
  }
  // LINT.ThenChange(//third_party/blink/public/devtools_protocol/domains/Accessibility.pdl:AXIgnoredReason)
  NOTREACHED();
}

}  // namespace blink
