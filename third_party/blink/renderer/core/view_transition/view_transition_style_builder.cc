// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/view_transition/view_transition_style_builder.h"

#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/geometry/transform.h"

namespace blink {
namespace {

const char* kGroupTagName = "html::view-transition-group";
const char* kImagePairTagName = "html::view-transition-image-pair";
const char* kNewImageTagName = "html::view-transition-new";
const char* kOldImageTagName = "html::view-transition-old";
const char* kKeyframeNamePrefix = "-ua-view-transition-group-anim-";

const char* TextOrientationToString(ETextOrientation text_orientation) {
  switch (text_orientation) {
    case ETextOrientation::kMixed:
      return "mixed";
    case ETextOrientation::kSideways:
      return "sideways";
    case ETextOrientation::kUpright:
      return "upright";
  }
  NOTREACHED();
  return "";
}

}  // namespace

void ViewTransitionStyleBuilder::AddUAStyle(const String& style) {
  builder_.Append(style);
}

String ViewTransitionStyleBuilder::Build() {
  return builder_.ReleaseString();
}

void ViewTransitionStyleBuilder::AddSelector(const String& name,
                                             const String& tag) {
  builder_.Append(name);
  builder_.Append("(");
  builder_.Append(tag);
  builder_.Append(")");
}

void ViewTransitionStyleBuilder::AddRules(const String& selector,
                                          const String& tag,
                                          const String& rules) {
  AddSelector(selector, tag);
  builder_.Append("{ ");
  builder_.Append(rules);
  builder_.Append(" }");
}

void ViewTransitionStyleBuilder::AddAnimations(
    AnimationType type,
    const String& tag,
    const ContainerProperties& source_properties) {
  switch (type) {
    case AnimationType::kOldOnly:
      AddRules(kOldImageTagName, tag,
               "animation-name: -ua-view-transition-fade-out");
      break;

    case AnimationType::kNewOnly:
      AddRules(kNewImageTagName, tag,
               "animation-name: -ua-view-transition-fade-in");
      break;

    case AnimationType::kBoth:
      AddRules(kOldImageTagName, tag,
               "animation-name: -ua-view-transition-fade-out, "
               "-ua-mix-blend-mode-plus-lighter");

      AddRules(kNewImageTagName, tag,
               "animation-name: -ua-view-transition-fade-in, "
               "-ua-mix-blend-mode-plus-lighter");

      AddRules(kImagePairTagName, tag, "isolation: isolate");

      const String& animation_name = AddKeyframes(tag, source_properties);
      StringBuilder rule_builder;
      rule_builder.Append("animation-name: ");
      rule_builder.Append(animation_name);
      rule_builder.Append(";\n");
      rule_builder.Append("animation-timing-function: ease;\n");
      rule_builder.Append("animation-delay: 0s;\n");
      rule_builder.Append("animation-iteration-count: 1;\n");
      rule_builder.Append("animation-direction: normal;\n");
      AddRules(kGroupTagName, tag, rule_builder.ReleaseString());
      break;
  }
}

String ViewTransitionStyleBuilder::AddKeyframes(
    const String& tag,
    const ContainerProperties& source_properties) {
  String keyframe_name = [&tag]() {
    StringBuilder builder;
    builder.Append(kKeyframeNamePrefix);
    builder.Append(tag);
    return builder.ReleaseString();
  }();

  builder_.Append("@keyframes ");
  builder_.Append(keyframe_name);
  builder_.AppendFormat(
      R"CSS({
        from {
          transform: %s;
          width: %.3fpx;
          height: %3fpx;
        }
      })CSS",
      ComputedStyleUtils::ValueForTransform(source_properties.snapshot_matrix,
                                            1, false)
          ->CssText()
          .Utf8()
          .c_str(),
      source_properties.border_box_size_in_css_space.width.ToFloat(),
      source_properties.border_box_size_in_css_space.height.ToFloat());
  return keyframe_name;
}

void ViewTransitionStyleBuilder::AddContainerStyles(
    const String& tag,
    const ContainerProperties& properties,
    WritingMode writing_mode,
    BlendMode blend_mode,
    ETextOrientation text_orientation,
    const String& color_scheme) {
  std::ostringstream writing_mode_stream;
  writing_mode_stream << writing_mode;

  StringBuilder rule_builder;
  rule_builder.AppendFormat(
      R"CSS(
        width: %.3fpx;
        height: %.3fpx;
        transform: %s;
        writing-mode: %s;
        mix-blend-mode: %s;
        text-orientation: %s;
        color-scheme: %s;
      )CSS",
      properties.border_box_size_in_css_space.width.ToFloat(),
      properties.border_box_size_in_css_space.height.ToFloat(),
      ComputedStyleUtils::ValueForTransform(properties.snapshot_matrix, 1,
                                            false)
          ->CssText()
          .Utf8()
          .c_str(),
      writing_mode_stream.str().c_str(),
      BlendModeToString(blend_mode).Utf8().c_str(),
      TextOrientationToString(text_orientation), color_scheme.Utf8().c_str());

  AddRules(kGroupTagName, tag, rule_builder.ReleaseString());
}

}  // namespace blink
