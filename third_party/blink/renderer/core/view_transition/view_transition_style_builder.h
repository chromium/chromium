// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_VIEW_TRANSITION_VIEW_TRANSITION_STYLE_BUILDER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_VIEW_TRANSITION_VIEW_TRANSITION_STYLE_BUILDER_H_

#include "third_party/blink/renderer/core/view_transition/view_transition_style_tracker.h"
#include "third_party/blink/renderer/platform/geometry/layout_size.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/geometry/transform.h"

namespace blink {

class ViewTransitionStyleBuilder {
 public:
  using ContainerProperties = ViewTransitionStyleTracker::ContainerProperties;

  ViewTransitionStyleBuilder() = default;

  void AddUAStyle(const String& style);

  void AddSelector(const String& name, const String& tag);
  void AddPlusLighter(const String& tag);

  void AddAnimationAndBlending(const String& tag,
                               const ContainerProperties& source_properties);

  void AddIncomingObjectViewBox(const String& tag, const String& value);
  void AddOutgoingObjectViewBox(const String& tag, const String& value);

  void AddContainerStyles(const String& tag, const String& rules);
  void AddContainerStyles(const String& tag,
                          const ContainerProperties& properties,
                          WritingMode writing_mode);

  void AddRootStyles(const gfx::RectF& snapshot_viewport_rect_css);

  String Build();

 private:
  // Adds the needed keyframes and returns the animation name to use.
  String AddKeyframes(const String& tag,
                      const ContainerProperties& source_properties);
  void AddObjectViewBox(const String& selector,
                        const String& tag,
                        const String& value);

  void AddRules(const String& selector, const String& tag, const String& rules);

  StringBuilder builder_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_VIEW_TRANSITION_VIEW_TRANSITION_STYLE_BUILDER_H_
