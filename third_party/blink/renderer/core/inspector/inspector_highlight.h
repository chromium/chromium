// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_HIGHLIGHT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_HIGHLIGHT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/inspector/protocol/DOM.h"
#include "third_party/blink/renderer/platform/geometry/float_quad.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class Color;

struct CORE_EXPORT InspectorHighlightConfig {
  USING_FAST_MALLOC(InspectorHighlightConfig);

 public:
  InspectorHighlightConfig();

  Color content;
  Color content_outline;
  Color padding;
  Color border;
  Color margin;
  Color event_target;
  Color shape;
  Color shape_margin;
  Color css_grid;

  bool show_info;
  bool show_styles;
  bool show_rulers;
  bool show_extension_lines;

  String selector_list;
};

struct InspectorHighlightContrastInfo {
  Color background_color;
  String font_size;
  String font_weight;
};

class CORE_EXPORT InspectorHighlight {
  STACK_ALLOCATED();

 public:
  InspectorHighlight(Node*,
                     const InspectorHighlightConfig&,
                     const InspectorHighlightContrastInfo&,
                     bool append_element_info,
                     bool append_distance_info,
                     bool is_locked_ancestor);
  explicit InspectorHighlight(float scale);
  ~InspectorHighlight();

  static bool GetBoxModel(Node*,
                          std::unique_ptr<protocol::DOM::BoxModel>*,
                          bool use_absolute_zoom);
  static bool GetContentQuads(
      Node*,
      std::unique_ptr<protocol::Array<protocol::Array<double>>>*);
  static InspectorHighlightConfig DefaultConfig();

  void AppendPath(std::unique_ptr<protocol::ListValue> path,
                  const Color& fill_color,
                  const Color& outline_color,
                  const String& name = String());
  void AppendQuad(const FloatQuad&,
                  const Color& fill_color,
                  const Color& outline_color = Color::kTransparent,
                  const String& name = String());
  void AppendEventTargetQuads(Node* event_target_node,
                              const InspectorHighlightConfig&);
  std::unique_ptr<protocol::DictionaryValue> AsProtocolValue() const;

 private:
  static bool BuildSVGQuads(Node*, Vector<FloatQuad>& quads);
  static bool BuildNodeQuads(Node*,
                             FloatQuad* content,
                             FloatQuad* padding,
                             FloatQuad* border,
                             FloatQuad* margin);
  void AppendNodeHighlight(Node*, const InspectorHighlightConfig&);
  void AppendPathsForShapeOutside(Node*, const InspectorHighlightConfig&);

  void AppendDistanceInfo(Node* node);
  void VisitAndCollectDistanceInfo(Node* node);
  void VisitAndCollectDistanceInfo(PseudoId pseudo_id,
                                   LayoutObject* layout_object);
  void AddLayoutBoxToDistanceInfo(LayoutObject* layout_object);

  std::unique_ptr<protocol::Array<protocol::Array<double>>> boxes_;
  std::unique_ptr<protocol::DictionaryValue> computed_style_;
  std::unique_ptr<protocol::DOM::BoxModel> model_;
  std::unique_ptr<protocol::DictionaryValue> distance_info_;
  std::unique_ptr<protocol::DictionaryValue> element_info_;
  std::unique_ptr<protocol::ListValue> highlight_paths_;
  std::unique_ptr<protocol::ListValue> grid_info_;
  bool show_rulers_;
  bool show_extension_lines_;
  float scale_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_HIGHLIGHT_H_
