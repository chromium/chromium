// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_change_reason.h"

#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/wtf/static_constructors.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

namespace style_change_reason {
const char kActiveStylesheetsUpdate[] = "ActiveStylesheetsUpdate";
const char kAnimation[] = "Animation";
const char kAttribute[] = "Attribute";
const char kControlValue[] = "ControlValue";
const char kControl[] = "Control";
const char kDeclarativeContent[] = "Extension declarativeContent.css";
const char kDesignMode[] = "DesignMode";
const char kDialog[] = "Dialog";
const char kDisplayLock[] = "DisplayLock";
const char kViewTransition[] = "ViewTransition";
const char kFlatTreeChange[] = "FlatTreeChange";
const char kFonts[] = "Fonts";
const char kFrame[] = "Frame";
const char kFullscreen[] = "Fullscreen";
const char kInheritedStyleChangeFromParentFrame[] =
    "InheritedStyleChangeFromParentFrame";
const char kInlineCSSStyleMutated[] =
    "Inline CSS style declaration was mutated";
const char kInspector[] = "Inspector";
const char kLanguage[] = "Language";
const char kLinkColorChange[] = "LinkColorChange";
const char kPictureSourceChanged[] = "PictureSourceChange";
const char kPlatformColorChange[] = "PlatformColorChange";
const char kPluginChanged[] = "Plugin Changed";
const char kPopoverVisibilityChange[] = "Popover Visibility Change";
const char kPropertyRegistration[] = "PropertyRegistration";
const char kPseudoClass[] = "PseudoClass";
const char kScrollTimeline[] = "ScrollTimeline";
const char kSVGContainerSizeChange[] = "SVGContainerSizeChange";
const char kSettings[] = "Settings";
const char kShadow[] = "Shadow";
const char kStyleInvalidator[] = "StyleInvalidator";
const char kStyleSheetChange[] = "StyleSheetChange";
const char kUseFallback[] = "UseFallback";
const char kViewportDefiningElement[] = "ViewportDefiningElement";
const char kViewportUnits[] = "ViewportUnits";
const char kVisuallyOrdered[] = "VisuallyOrdered";
const char kWritingModeChange[] = "WritingModeChange";
const char kZoom[] = "Zoom";
}  // namespace style_change_reason

namespace style_change_extra_data {
DEFINE_GLOBAL(AtomicString, g_active);
DEFINE_GLOBAL(AtomicString, g_disabled);
DEFINE_GLOBAL(AtomicString, g_drag);
DEFINE_GLOBAL(AtomicString, g_focus);
DEFINE_GLOBAL(AtomicString, g_focus_visible);
DEFINE_GLOBAL(AtomicString, g_focus_within);
DEFINE_GLOBAL(AtomicString, g_hover);
DEFINE_GLOBAL(AtomicString, g_past);
DEFINE_GLOBAL(AtomicString, g_toggle);
DEFINE_GLOBAL(AtomicString, g_unresolved);

void Init() {
  DCHECK(IsMainThread());

  new (NotNullTag::kNotNull, (void*)&g_active) AtomicString(":active");
  new (NotNullTag::kNotNull, (void*)&g_disabled) AtomicString(":disabled");
  new (NotNullTag::kNotNull, (void*)&g_drag) AtomicString(":-webkit-drag");
  new (NotNullTag::kNotNull, (void*)&g_focus) AtomicString(":focus");
  new (NotNullTag::kNotNull, (void*)&g_focus_visible)
      AtomicString(":focus-visible");
  new (NotNullTag::kNotNull, (void*)&g_focus_within)
      AtomicString(":focus-within");
  new (NotNullTag::kNotNull, (void*)&g_hover) AtomicString(":hover");
  new (NotNullTag::kNotNull, (void*)&g_past) AtomicString(":past");
  new (NotNullTag::kNotNull, (void*)&g_toggle) AtomicString(":toggle");
  new (NotNullTag::kNotNull, (void*)&g_unresolved) AtomicString(":unresolved");
}

}  // namespace style_change_extra_data

}  // namespace blink
