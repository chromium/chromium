// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/document_transition/document_transition_style_builder.h"

#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/platform/geometry/layout_size.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
namespace {

const char* kTransitionRootName = "html::page-transition";
const char* kContainerTagName = "html::page-transition-container";
const char* kImageWrapperTagName = "html::page-transition-image-wrapper";
const char* kIncomingImageTagName = "html::page-transition-incoming-image";
const char* kOutgoingImageTagName = "html::page-transition-outgoing-image";
const char* kKeyframeNamePrefix = "-ua-page-transition-container-anim-";

}  // namespace

void DocumentTransitionStyleBuilder::AddUAStyle(const String& style) {
  builder_.Append(style);
}

String DocumentTransitionStyleBuilder::Build() {
  return builder_.ReleaseString();
}

void DocumentTransitionStyleBuilder::AddSelector(const String& name,
                                                 const String& tag) {
  builder_.Append(name);
  builder_.Append("(");
  builder_.Append(tag);
  builder_.Append(")");
}

void DocumentTransitionStyleBuilder::AddRules(const String& selector,
                                              const String& tag,
                                              const String& rules) {
  AddSelector(selector, tag);
  builder_.Append("{ ");
  builder_.Append(rules);
  builder_.Append(" }");
}

void DocumentTransitionStyleBuilder::AddPlusLighter(const String& tag) {
  AddRules(kImageWrapperTagName, tag, "isolation: isolate");
  AddRules(kIncomingImageTagName, tag, "mix-blend-mode: plus-lighter");
  AddRules(kOutgoingImageTagName, tag, "mix-blend-mode: plus-lighter");
}

void DocumentTransitionStyleBuilder::AddAnimationAndBlending(
    const String& tag,
    const ContainerProperties& source_properties) {
  const String& animation_name = AddKeyframes(tag, source_properties);
  StringBuilder rule_builder;
  rule_builder.Append("animation-name: ");
  rule_builder.Append(animation_name);
  rule_builder.Append(";\n");
  rule_builder.Append("animation-duration: 0.25s;\n");
  rule_builder.Append("animation-fill-mode: both;\n");
  rule_builder.Append("animation-timing-function: ease;\n");
  rule_builder.Append("animation-delay: 0s;\n");
  rule_builder.Append("animation-iteration-count: 1;\n");
  rule_builder.Append("animation-direction: normal;\n");
  AddRules(kContainerTagName, tag, rule_builder.ReleaseString());

  // Add plus-lighter blending.
  AddPlusLighter(tag);
}

String DocumentTransitionStyleBuilder::AddKeyframes(
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
      ComputedStyleUtils::ValueForTransformationMatrix(
          source_properties.snapshot_matrix, 1, false)
          ->CssText()
          .Utf8()
          .c_str(),
      source_properties.border_box_size_in_css_space.Width().ToFloat(),
      source_properties.border_box_size_in_css_space.Height().ToFloat());
  return keyframe_name;
}

void DocumentTransitionStyleBuilder::AddIncomingObjectViewBox(
    const String& tag,
    const String& value) {
  AddObjectViewBox(kIncomingImageTagName, tag, value);
}

void DocumentTransitionStyleBuilder::AddOutgoingObjectViewBox(
    const String& tag,
    const String& value) {
  AddObjectViewBox(kOutgoingImageTagName, tag, value);
}

void DocumentTransitionStyleBuilder::AddObjectViewBox(const String& selector,
                                                      const String& tag,
                                                      const String& value) {
  StringBuilder rule_builder;
  rule_builder.AppendFormat("object-view-box: %s", value.Utf8().c_str());
  AddRules(selector, tag, rule_builder.ReleaseString());
}

void DocumentTransitionStyleBuilder::AddContainerStyles(const String& tag,
                                                        const String& rules) {
  AddRules(kContainerTagName, tag, rules);
}

void DocumentTransitionStyleBuilder::AddContainerStyles(
    const String& tag,
    const ContainerProperties& properties,
    WritingMode writing_mode) {
  std::ostringstream writing_mode_stream;
  writing_mode_stream << writing_mode;

  StringBuilder rule_builder;
  rule_builder.AppendFormat(
      R"CSS(
        width: %.3fpx;
        height: %.3fpx;
        transform: %s;
        writing-mode: %s;
      )CSS",
      properties.border_box_size_in_css_space.Width().ToFloat(),
      properties.border_box_size_in_css_space.Height().ToFloat(),
      ComputedStyleUtils::ValueForTransformationMatrix(
          properties.snapshot_matrix, 1, false)
          ->CssText()
          .Utf8()
          .c_str(),
      writing_mode_stream.str().c_str());

  AddContainerStyles(tag, rule_builder.ReleaseString());
}

void DocumentTransitionStyleBuilder::AddRootStyles(const String& rules) {
  builder_.Append(kTransitionRootName);
  builder_.Append("{ ");
  builder_.Append(rules);
  builder_.Append(" }");
}

}  // namespace blink
