// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/view_transition/view_transition_style_builder.h"

#include "third_party/blink/renderer/core/css/dom_window_css.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/geometry/transform.h"

namespace blink {
namespace {

const char* const kGroupTagName = "html::view-transition-group";
const char* const kGroupChildrenTagName =
    "html::view-transition-group-children";
const char* const kImagePairTagName = "html::view-transition-image-pair";
const char* const kNewImageTagName = "html::view-transition-new";
const char* const kOldImageTagName = "html::view-transition-old";
const char* const kKeyframeNamePrefix = "-ua-view-transition-group-anim-";
const char* const kGroupChildrenKeyframeNamePrefix =
    "-ua-view-transition-group-children-anim-";

const char* const kGroupTagNameScoped = "::view-transition-group";
const char* const kGroupChildrenTagNameScoped =
    "::view-transition-group-children";
const char* const kImagePairTagNameScoped = "::view-transition-image-pair";
const char* const kNewImageTagNameScoped = "::view-transition-new";
const char* const kOldImageTagNameScoped = "::view-transition-old";

const char* GroupTagName() {
  return RuntimeEnabledFeatures::ScopedViewTransitionsEnabled()
             ? kGroupTagNameScoped
             : kGroupTagName;
}

const char* ImagePairTagName() {
  return RuntimeEnabledFeatures::ScopedViewTransitionsEnabled()
             ? kImagePairTagNameScoped
             : kImagePairTagName;
}

const char* NewImageTagName() {
  return RuntimeEnabledFeatures::ScopedViewTransitionsEnabled()
             ? kNewImageTagNameScoped
             : kNewImageTagName;
}

const char* OldImageTagName() {
  return RuntimeEnabledFeatures::ScopedViewTransitionsEnabled()
             ? kOldImageTagNameScoped
             : kOldImageTagName;
}

const char* GroupChildrenTagName() {
  return RuntimeEnabledFeatures::ScopedViewTransitionsEnabled()
             ? kGroupChildrenTagNameScoped
             : kGroupChildrenTagName;
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
  builder_.Append(DOMWindowCSS::escape(tag));
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
    const ContainerProperties& source_properties,
    const CapturedCssProperties& animated_css_properties,
    const gfx::Transform& parent_transform) {
  switch (type) {
    case AnimationType::kOldOnly:
      AddRules(OldImageTagName(), tag,
               "animation-name: -ua-view-transition-fade-out");
      break;

    case AnimationType::kNewOnly:
      AddRules(NewImageTagName(), tag,
               "animation-name: -ua-view-transition-fade-in");
      break;

    case AnimationType::kBoth:
      AddRules(OldImageTagName(), tag,
               "animation-name: -ua-view-transition-fade-out, "
               "-ua-mix-blend-mode-plus-lighter");

      AddRules(NewImageTagName(), tag,
               "animation-name: -ua-view-transition-fade-in, "
               "-ua-mix-blend-mode-plus-lighter");

      AddRules(ImagePairTagName(), tag, "isolation: isolate;\n");

      const String& animation_name = AddKeyframes(
          tag, source_properties, animated_css_properties, parent_transform);
      StringBuilder rule_builder;
      rule_builder.Append("animation-name: ");
      rule_builder.Append(animation_name);
      rule_builder.Append(";\n");
      rule_builder.Append("animation-timing-function: ease;\n");
      rule_builder.Append("animation-delay: 0s;\n");
      rule_builder.Append("animation-iteration-count: 1;\n");
      rule_builder.Append("animation-direction: normal;\n");
      AddRules(GroupTagName(), tag, rule_builder.ReleaseString());
      break;
  }
}

void ViewTransitionStyleBuilder::AddGroupChildrenAnimations(
    const String& tag,
    const CapturedCssProperties& animated_properties) {
  if (animated_properties.empty()) {
    return;
  }

  const String& animation_name =
      AddGroupChildrenKeyframes(tag, animated_properties);
  StringBuilder rule_builder;
  rule_builder.Append("animation-name: ");
  rule_builder.Append(animation_name);
  rule_builder.Append(";\n");
  rule_builder.Append("animation-timing-function: ease;\n");
  rule_builder.Append("animation-delay: 0s;\n");
  rule_builder.Append("animation-iteration-count: 1;\n");
  rule_builder.Append("animation-direction: normal;\n");
  AddRules(GroupChildrenTagName(), tag, rule_builder.ReleaseString());
}

namespace {
std::string GetTransformString(
    const ViewTransitionStyleBuilder::ContainerProperties& properties,
    const gfx::Transform& parent_transform) {
  return ComputedStyleUtils::ValueForTransform(
             properties.ComputeRelativeTransformWithCenterOrigin(
                 parent_transform),
             1, false)
      ->CssText()
      .Utf8();
}
}  // namespace

String ViewTransitionStyleBuilder::AddKeyframes(
    const String& tag,
    const ContainerProperties& source_properties,
    const CapturedCssProperties& animated_css_properties,
    const gfx::Transform& parent_transform) {
  String keyframe_name = [&tag]() {
    StringBuilder builder;
    builder.Append(kKeyframeNamePrefix);
    builder.Append(DOMWindowCSS::escape(tag));
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
      )CSS",
      GetTransformString(source_properties, parent_transform).c_str(),
      source_properties.GroupSize().width.ToFloat(),
      source_properties.GroupSize().height.ToFloat());

  for (const auto& [id, value] : animated_css_properties) {
    builder_.AppendFormat(
        "%s: %s;\n",
        CSSProperty::Get(id).GetPropertyNameAtomicString().Utf8().c_str(),
        value.Utf8().c_str());
  }
  builder_.Append("}}");
  return keyframe_name;
}

String ViewTransitionStyleBuilder::AddGroupChildrenKeyframes(
    const String& tag,
    const CapturedCssProperties& properties) {
  String keyframe_name = [&tag]() {
    StringBuilder builder;
    builder.Append(kGroupChildrenKeyframeNamePrefix);
    builder.Append(DOMWindowCSS::escape(tag));
    return builder.ReleaseString();
  }();

  builder_.Append("@keyframes ");
  builder_.Append(keyframe_name);
  builder_.Append("{\n from {\n");

  for (const auto& [id, value] : properties) {
    builder_.AppendFormat(
        "%s: %s;\n",
        CSSProperty::Get(id).GetPropertyNameAtomicString().Utf8().c_str(),
        value.Utf8().c_str());
  }
  builder_.Append("}}");
  return keyframe_name;
}

void ViewTransitionStyleBuilder::AddContainerStyles(
    const String& tag,
    const ContainerProperties& properties,
    const CapturedCssProperties& captured_css_properties,
    const gfx::Transform& parent_transform) {
  StringBuilder group_rule_builder;
  group_rule_builder.AppendFormat(
      R"CSS(
        width: %.3fpx;
        height: %.3fpx;
        transform: %s;
      )CSS",
      properties.GroupSize().width.ToFloat(),
      properties.GroupSize().height.ToFloat(),
      GetTransformString(properties, parent_transform).c_str());
  for (const auto& [id, value] : captured_css_properties) {
    group_rule_builder.AppendFormat(
        "%s: %s;\n",
        CSSProperty::Get(id).GetPropertyNameAtomicString().Utf8().c_str(),
        value.Utf8().c_str());
  }

  AddRules(GroupTagName(), tag, group_rule_builder.ReleaseString());
}

void ViewTransitionStyleBuilder::AddGroupChildrenStyles(
    const String& name,
    const CapturedCssProperties& captured_css_properties) {
  if (captured_css_properties.empty()) {
    return;
  }

  StringBuilder builder;
  for (const auto& [id, value] : captured_css_properties) {
    builder.Append(CSSProperty::Get(id).GetPropertyNameAtomicString());
    builder.Append(": ");
    builder.Append(value);
    builder.Append(";\n");
  }
  AddRules(GroupChildrenTagName(), name, builder.ReleaseString());
}

}  // namespace blink
