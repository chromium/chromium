// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_highlight.h"

#include <memory>

#include "third_party/blink/renderer/core/css/css_color.h"
#include "third_party/blink/renderer/core/css/css_computed_style_declaration.h"
#include "third_party/blink/renderer/core/css/css_grid_auto_repeat_value.h"
#include "third_party/blink/renderer/core/css/css_grid_integer_repeat_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_property_name.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/inspector/dom_traversal_utils.h"
#include "third_party/blink/renderer/core/inspector/inspector_dom_agent.h"
#include "third_party/blink/renderer/core/inspector/node_content_visibility_state.h"
#include "third_party/blink/renderer/core/inspector/protocol/overlay.h"
#include "third_party/blink/renderer/core/layout/adjust_for_absolute_zoom.h"
#include "third_party/blink/renderer/core/layout/flex/devtools_flex_info.h"
#include "third_party/blink/renderer/core/layout/flex/layout_flexible_box.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_offset.h"
#include "third_party/blink/renderer/core/layout/grid/layout_grid.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/logical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/shapes/shape_outside_info.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/graphics/path.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"
#include "third_party/blink/renderer/platform/web_test_support.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"

namespace blink {

namespace {

class PathBuilder {
  STACK_ALLOCATED();

 public:
  PathBuilder() : path_(protocol::ListValue::create()) {}
  PathBuilder(const PathBuilder&) = delete;
  PathBuilder& operator=(const PathBuilder&) = delete;
  virtual ~PathBuilder() = default;

  std::unique_ptr<protocol::ListValue> Release() { return std::move(path_); }

  void AppendPath(const Path& path, float scale) {
    Path transform_path(path);
    transform_path.Transform(AffineTransform().Scale(scale));
    transform_path.Apply(this, &PathBuilder::AppendPathElement);
  }

 protected:
  virtual gfx::PointF TranslatePoint(const gfx::PointF& point) { return point; }

 private:
  static void AppendPathElement(void* path_builder,
                                const PathElement& path_element) {
    static_cast<PathBuilder*>(path_builder)->AppendPathElement(path_element);
  }

  void AppendPathElement(const PathElement&);
  void AppendPathCommandAndPoints(const char* command,
                                  base::span<const gfx::PointF> points);

  std::unique_ptr<protocol::ListValue> path_;
};

void PathBuilder::AppendPathCommandAndPoints(
    const char* command,
    base::span<const gfx::PointF> points) {
  path_->pushValue(protocol::StringValue::create(command));
  for (const auto& orig_point : points) {
    gfx::PointF point = TranslatePoint(orig_point);
    path_->pushValue(protocol::FundamentalValue::create(point.x()));
    path_->pushValue(protocol::FundamentalValue::create(point.y()));
  }
}

void PathBuilder::AppendPathElement(const PathElement& path_element) {
  switch (path_element.type) {
    // The points member will contain 1 value.
    case kPathElementMoveToPoint:
      AppendPathCommandAndPoints("M", path_element.points);
      break;
    // The points member will contain 1 value.
    case kPathElementAddLineToPoint:
      AppendPathCommandAndPoints("L", path_element.points);
      break;
    // The points member will contain 3 values.
    case kPathElementAddCurveToPoint:
      AppendPathCommandAndPoints("C", path_element.points);
      break;
    // The points member will contain 2 values.
    case kPathElementAddQuadCurveToPoint:
      AppendPathCommandAndPoints("Q", path_element.points);
      break;
    // The points member will contain no values.
    case kPathElementCloseSubpath:
      AppendPathCommandAndPoints("Z", path_element.points);
      break;
  }
}

class ShapePathBuilder : public PathBuilder {
 public:
  ShapePathBuilder(LocalFrameView& view,
                   LayoutObject& layout_object,
                   const ShapeOutsideInfo& shape_outside_info)
      : view_(&view),
        layout_object_(&layout_object),
        shape_outside_info_(shape_outside_info) {}

  static std::unique_ptr<protocol::ListValue> BuildPath(
      LocalFrameView& view,
      LayoutObject& layout_object,
      const ShapeOutsideInfo& shape_outside_info,
      const Path& path,
      float scale) {
    ShapePathBuilder builder(view, layout_object, shape_outside_info);
    builder.AppendPath(path, scale);
    return builder.Release();
  }

 protected:
  gfx::PointF TranslatePoint(const gfx::PointF& point) override {
    PhysicalOffset layout_object_point = PhysicalOffset::FromPointFRound(
        shape_outside_info_.ShapeToLayoutObjectPoint(point));
    // TODO(pfeldman): Is this kIgnoreTransforms correct?
    return gfx::PointF(view_->FrameToViewport(
        ToRoundedPoint(layout_object_->LocalToAbsolutePoint(
            layout_object_point, kIgnoreTransforms))));
  }

 private:
  LocalFrameView* view_;
  LayoutObject* const layout_object_;
  const ShapeOutsideInfo& shape_outside_info_;
};

std::unique_ptr<protocol::Array<double>> BuildArrayForQuad(
    const gfx::QuadF& quad) {
  return std::make_unique<std::vector<double>, std::initializer_list<double>>(
      {quad.p1().x(), quad.p1().y(), quad.p2().x(), quad.p2().y(),
       quad.p3().x(), quad.p3().y(), quad.p4().x(), quad.p4().y()});
}

Path QuadToPath(const gfx::QuadF& quad) {
  Path quad_path;
  quad_path.MoveTo(quad.p1());
  quad_path.AddLineTo(quad.p2());
  quad_path.AddLineTo(quad.p3());
  quad_path.AddLineTo(quad.p4());
  quad_path.CloseSubpath();
  return quad_path;
}

Path RowQuadToPath(const gfx::QuadF& quad, bool draw_end_line) {
  Path quad_path;
  quad_path.MoveTo(quad.p1());
  quad_path.AddLineTo(quad.p2());
  if (draw_end_line) {
    quad_path.MoveTo(quad.p3());
    quad_path.AddLineTo(quad.p4());
  }
  return quad_path;
}

Path ColumnQuadToPath(const gfx::QuadF& quad, bool draw_end_line) {
  Path quad_path;
  quad_path.MoveTo(quad.p1());
  quad_path.AddLineTo(quad.p4());
  if (draw_end_line) {
    quad_path.MoveTo(quad.p3());
    quad_path.AddLineTo(quad.p2());
  }
  return quad_path;
}

gfx::PointF FramePointToViewport(const LocalFrameView* view,
                                 gfx::PointF point_in_frame) {
  gfx::PointF point_in_root_frame = view->ConvertToRootFrame(point_in_frame);
  return view->GetPage()->GetVisualViewport().RootFrameToViewport(
      point_in_root_frame);
}

float PageScaleFromFrameView(const LocalFrameView* frame_view) {
  return 1.f / frame_view->GetPage()->GetVisualViewport().Scale();
}

float DeviceScaleFromFrameView(const LocalFrameView* frame_view) {
  return 1.f / frame_view->GetChromeClient()->WindowToViewportScalar(
                   &frame_view->GetFrame(), 1.f);
}

void FrameQuadToViewport(const LocalFrameView* view, gfx::QuadF& quad) {
  quad.set_p1(FramePointToViewport(view, quad.p1()));
  quad.set_p2(FramePointToViewport(view, quad.p2()));
  quad.set_p3(FramePointToViewport(view, quad.p3()));
  quad.set_p4(FramePointToViewport(view, quad.p4()));
}

const ShapeOutsideInfo* ShapeOutsideInfoForNode(Node* node,
                                                Shape::DisplayPaths* paths,
                                                gfx::QuadF* bounds) {
  LayoutObject* layout_object = node->GetLayoutObject();
  if (!layout_object || !layout_object->IsBox() ||
      !To<LayoutBox>(layout_object)->GetShapeOutsideInfo())
    return nullptr;

  LocalFrameView* containing_view = node->GetDocument().View();
  auto* layout_box = To<LayoutBox>(layout_object);
  const ShapeOutsideInfo* shape_outside_info =
      layout_box->GetShapeOutsideInfo();

  shape_outside_info->ComputedShape().BuildDisplayPaths(*paths);

  PhysicalRect shape_bounds =
      shape_outside_info->ComputedShapePhysicalBoundingBox();
  *bounds = layout_box->LocalRectToAbsoluteQuad(shape_bounds);
  FrameQuadToViewport(containing_view, *bounds);

  return shape_outside_info;
}

String ToHEXA(const Color& color) {
  return String::Format("#%02X%02X%02X%02X", color.Red(), color.Green(),
                        color.Blue(), color.AlphaAsInteger());
}

std::unique_ptr<protocol::ListValue> ToRGBAList(const Color& color) {
  SkColor4f skColor = color.toSkColor4f();

  std::unique_ptr<protocol::ListValue> list = protocol::ListValue::create();
  list->pushValue(protocol::FundamentalValue::create(skColor.fR));
  list->pushValue(protocol::FundamentalValue::create(skColor.fG));
  list->pushValue(protocol::FundamentalValue::create(skColor.fB));
  list->pushValue(protocol::FundamentalValue::create(skColor.fA));
  return list;
}

namespace ContrastAlgorithmEnum = protocol::Overlay::ContrastAlgorithmEnum;

String ContrastAlgorithmToString(const ContrastAlgorithm& contrast_algorithm) {
  // It reuses the protocol string constants to avoid duplicating the string
  // values. These string values are sent to the overlay code that is expected
  // to handle them properly.
  switch (contrast_algorithm) {
    case ContrastAlgorithm::kAa:
      return ContrastAlgorithmEnum::Aa;
    case ContrastAlgorithm::kAaa:
      return ContrastAlgorithmEnum::Aaa;
    case ContrastAlgorithm::kApca:
      return ContrastAlgorithmEnum::Apca;
  }
}
}  // namespace

void AppendStyleInfo(Element* element,
                     protocol::DictionaryValue* element_info,
                     const InspectorHighlightContrastInfo& node_contrast,
                     const ContrastAlgorithm& contrast_algorithm) {
  std::unique_ptr<protocol::DictionaryValue> computed_style =
      protocol::DictionaryValue::create();
  CSSComputedStyleDeclaration* style =
      MakeGarbageCollected<CSSComputedStyleDeclaration>(element, true);
  Vector<CSSPropertyID> properties;

  // For text nodes, we can show color & font properties.
  bool has_text_children = false;
  for (Node* child = element->firstChild(); !has_text_children && child;
       child = child->nextSibling()) {
    has_text_children = child->IsTextNode();
  }
  if (has_text_children) {
    properties.push_back(CSSPropertyID::kColor);
    properties.push_back(CSSPropertyID::kFontFamily);
    properties.push_back(CSSPropertyID::kFontSize);
    properties.push_back(CSSPropertyID::kLineHeight);
  }

  properties.push_back(CSSPropertyID::kPadding);
  properties.push_back(CSSPropertyID::kMargin);
  properties.push_back(CSSPropertyID::kBackgroundColor);

  for (wtf_size_t i = 0; i < properties.size(); ++i) {
    const CSSValue* value = style->GetPropertyCSSValue(properties[i]);
    if (!value)
      continue;
    AtomicString name = CSSPropertyName(properties[i]).ToAtomicString();
    if (value->IsColorValue()) {
      Color color = static_cast<const cssvalue::CSSColor*>(value)->Value();
      computed_style->setArray(name + "-unclamped-rgba", ToRGBAList(color));
      if (!Color::IsLegacyColorSpace(color.GetColorSpace())) {
        computed_style->setString(name + "-css-text", value->CssText());
      }
      computed_style->setString(name, ToHEXA(color));
    } else {
      computed_style->setString(name, value->CssText());
    }
  }
  element_info->setValue("style", std::move(computed_style));

  if (!node_contrast.font_size.empty()) {
    std::unique_ptr<protocol::DictionaryValue> contrast =
        protocol::DictionaryValue::create();
    contrast->setString("fontSize", node_contrast.font_size);
    contrast->setString("fontWeight", node_contrast.font_weight);
    contrast->setString("backgroundColor",
                        ToHEXA(node_contrast.background_color));
    contrast->setArray("backgroundColorUnclampedRgba",
                       ToRGBAList(node_contrast.background_color));
    contrast->setString("backgroundColorCssText",
                        node_contrast.background_color.SerializeAsCSSColor());
    contrast->setString("contrastAlgorithm",
                        ContrastAlgorithmToString(contrast_algorithm));
    contrast->setDouble("textOpacity", node_contrast.text_opacity);
    element_info->setValue("contrast", std::move(contrast));
  }
}

std::unique_ptr<protocol::DictionaryValue> BuildElementInfo(Element* element) {
  std::unique_ptr<protocol::DictionaryValue> element_info =
      protocol::DictionaryValue::create();
  Element* real_element = element;
  auto* pseudo_element = DynamicTo<PseudoElement>(element);
  if (pseudo_element) {
    real_element = element->ParentOrShadowHostElement();
  }
  bool is_xhtml = real_element->GetDocument().IsXHTMLDocument();
  element_info->setString(
      "tagName", is_xhtml ? real_element->nodeName()
                          : real_element->nodeName().DeprecatedLower());
  element_info->setString("idValue", real_element->GetIdAttribute());
  StringBuilder class_names;
  if (real_element->HasClass() && real_element->IsStyledElement()) {
    HashSet<AtomicString> used_class_names;
    const SpaceSplitString& class_names_string = real_element->ClassNames();
    wtf_size_t class_name_count = class_names_string.size();
    for (wtf_size_t i = 0; i < class_name_count; ++i) {
      const AtomicString& class_name = class_names_string[i];
      if (!used_class_names.insert(class_name).is_new_entry)
        continue;
      class_names.Append('.');
      class_names.Append(class_name);
    }
  }
  if (pseudo_element) {
    if (pseudo_element->GetPseudoId() == kPseudoIdBefore) {
      class_names.Append("::before");
    } else if (pseudo_element->GetPseudoId() == kPseudoIdAfter) {
      class_names.Append("::after");
    } else if (pseudo_element->GetPseudoId() == kPseudoIdMarker) {
      class_names.Append("::marker");
    } else if (pseudo_element->GetPseudoIdForStyling() ==
               kPseudoIdScrollMarkerGroup) {
      class_names.Append("::scroll-marker-group");
    } else if (pseudo_element->GetPseudoId() == kPseudoIdScrollMarker) {
      class_names.Append("::scroll-marker");
    } else if (pseudo_element->GetPseudoId() == kPseudoIdScrollNextButton) {
      class_names.Append("::scroll-next-button");
    } else if (pseudo_element->GetPseudoId() == kPseudoIdScrollPrevButton) {
      class_names.Append("::scroll-prev-button");
    }
  }
  if (!class_names.empty())
    element_info->setString("className", class_names.ToString());

  LayoutObject* layout_object = element->GetLayoutObject();
  LocalFrameView* containing_view = element->GetDocument().View();
  if (!layout_object || !containing_view)
    return element_info;

  // layoutObject the GetBoundingClientRect() data in the tooltip
  // to be consistent with the rulers (see http://crbug.com/262338).

  DCHECK(element->GetDocument().Lifecycle().GetState() >=
         DocumentLifecycle::kLayoutClean);
  gfx::RectF bounding_box = element->GetBoundingClientRectNoLifecycleUpdate();
  element_info->setString("nodeWidth", String::Number(bounding_box.width()));
  element_info->setString("nodeHeight", String::Number(bounding_box.height()));

  element_info->setBoolean("isKeyboardFocusable",
                           element->IsKeyboardFocusable());
  element_info->setString("accessibleName",
                          element->ComputedNameNoLifecycleUpdate());
  element_info->setString("accessibleRole",
                          element->ComputedRoleNoLifecycleUpdate());

  element_info->setString("layoutObjectName", layout_object->GetName());

  return element_info;
}

namespace {
std::unique_ptr<protocol::DictionaryValue> BuildTextNodeInfo(Text* text_node) {
  std::unique_ptr<protocol::DictionaryValue> text_info =
      protocol::DictionaryValue::create();
  LayoutObject* layout_object = text_node->GetLayoutObject();
  if (!layout_object || !layout_object->IsText())
    return text_info;
  PhysicalRect bounding_box =
      To<LayoutText>(layout_object)->VisualOverflowRect();
  text_info->setString("nodeWidth", bounding_box.Width().ToString());
  text_info->setString("nodeHeight", bounding_box.Height().ToString());
  text_info->setString("tagName", "#text");
  text_info->setBoolean("showAccessibilityInfo", false);
  return text_info;
}

void AppendLineStyleConfig(
    const std::optional<LineStyle>& line_style,
    std::unique_ptr<protocol::DictionaryValue>& parent_config,
    String line_name) {
  if (!line_style || line_style->IsFullyTransparent()) {
    return;
  }

  std::unique_ptr<protocol::DictionaryValue> config =
      protocol::DictionaryValue::create();
  config->setString("color", line_style->color.SerializeAsCSSColor());
  config->setString("pattern", line_style->pattern);

  parent_config->setValue(line_name, std::move(config));
}

void AppendBoxStyleConfig(
    const std::optional<BoxStyle>& box_style,
    std::unique_ptr<protocol::DictionaryValue>& parent_config,
    String box_name) {
  if (!box_style || box_style->IsFullyTransparent()) {
    return;
  }

  std::unique_ptr<protocol::DictionaryValue> config =
      protocol::DictionaryValue::create();
  config->setString("fillColor", box_style->fill_color.SerializeAsCSSColor());
  config->setString("hatchColor", box_style->hatch_color.SerializeAsCSSColor());

  parent_config->setValue(box_name, std::move(config));
}

std::unique_ptr<protocol::DictionaryValue>
BuildFlexContainerHighlightConfigInfo(
    const InspectorFlexContainerHighlightConfig& flex_config) {
  std::unique_ptr<protocol::DictionaryValue> flex_config_info =
      protocol::DictionaryValue::create();

  AppendLineStyleConfig(flex_config.container_border, flex_config_info,
                        "containerBorder");
  AppendLineStyleConfig(flex_config.line_separator, flex_config_info,
                        "lineSeparator");
  AppendLineStyleConfig(flex_config.item_separator, flex_config_info,
                        "itemSeparator");

  AppendBoxStyleConfig(flex_config.main_distributed_space, flex_config_info,
                       "mainDistributedSpace");
  AppendBoxStyleConfig(flex_config.cross_distributed_space, flex_config_info,
                       "crossDistributedSpace");
  AppendBoxStyleConfig(flex_config.row_gap_space, flex_config_info,
                       "rowGapSpace");
  AppendBoxStyleConfig(flex_config.column_gap_space, flex_config_info,
                       "columnGapSpace");
  AppendLineStyleConfig(flex_config.cross_alignment, flex_config_info,
                        "crossAlignment");

  return flex_config_info;
}

std::unique_ptr<protocol::DictionaryValue> BuildFlexItemHighlightConfigInfo(
    const InspectorFlexItemHighlightConfig& flex_config) {
  std::unique_ptr<protocol::DictionaryValue> flex_config_info =
      protocol::DictionaryValue::create();

  AppendBoxStyleConfig(flex_config.base_size_box, flex_config_info,
                       "baseSizeBox");
  AppendLineStyleConfig(flex_config.base_size_border, flex_config_info,
                        "baseSizeBorder");
  AppendLineStyleConfig(flex_config.flexibility_arrow, flex_config_info,
                        "flexibilityArrow");

  return flex_config_info;
}

std::unique_ptr<protocol::DictionaryValue> BuildGridHighlightConfigInfo(
    const InspectorGridHighlightConfig& grid_config) {
  std::unique_ptr<protocol::DictionaryValue> grid_config_info =
      protocol::DictionaryValue::create();
  grid_config_info->setBoolean("gridBorderDash", grid_config.grid_border_dash);
  grid_config_info->setBoolean("rowLineDash", grid_config.row_line_dash);
  grid_config_info->setBoolean("columnLineDash", grid_config.column_line_dash);
  grid_config_info->setBoolean("showGridExtensionLines",
                               grid_config.show_grid_extension_lines);
  grid_config_info->setBoolean("showPositiveLineNumbers",
                               grid_config.show_positive_line_numbers);
  grid_config_info->setBoolean("showNegativeLineNumbers",
                               grid_config.show_negative_line_numbers);
  grid_config_info->setBoolean("showAreaNames", grid_config.show_area_names);
  grid_config_info->setBoolean("showLineNames", grid_config.show_line_names);

  if (grid_config.grid_color != Color::kTransparent) {
    grid_config_info->setString("gridBorderColor",
                                grid_config.grid_color.SerializeAsCSSColor());
  }
  if (grid_config.row_line_color != Color::kTransparent) {
    grid_config_info->setString(
        "rowLineColor", grid_config.row_line_color.SerializeAsCSSColor());
  }
  if (grid_config.column_line_color != Color::kTransparent) {
    grid_config_info->setString(
        "columnLineColor", grid_config.column_line_color.SerializeAsCSSColor());
  }
  if (grid_config.row_gap_color != Color::kTransparent) {
    grid_config_info->setString(
        "rowGapColor", grid_config.row_gap_color.SerializeAsCSSColor());
  }
  if (grid_config.column_gap_color != Color::kTransparent) {
    grid_config_info->setString(
        "columnGapColor", grid_config.column_gap_color.SerializeAsCSSColor());
  }
  if (grid_config.row_hatch_color != Color::kTransparent) {
    grid_config_info->setString(
        "rowHatchColor", grid_config.row_hatch_color.SerializeAsCSSColor());
  }
  if (grid_config.column_hatch_color != Color::kTransparent) {
    grid_config_info->setString(
        "columnHatchColor",
        grid_config.column_hatch_color.SerializeAsCSSColor());
  }
  if (grid_config.area_border_color != Color::kTransparent) {
    grid_config_info->setString(
        "areaBorderColor", grid_config.area_border_color.SerializeAsCSSColor());
  }
  if (grid_config.grid_background_color != Color::kTransparent) {
    grid_config_info->setString(
        "gridBackgroundColor",
        grid_config.grid_background_color.SerializeAsCSSColor());
  }
  return grid_config_info;
}

std::unique_ptr<protocol::DictionaryValue>
BuildContainerQueryContainerHighlightConfigInfo(
    const InspectorContainerQueryContainerHighlightConfig& container_config) {
  std::unique_ptr<protocol::DictionaryValue> container_config_info =
      protocol::DictionaryValue::create();

  AppendLineStyleConfig(container_config.container_border,
                        container_config_info, "containerBorder");
  AppendLineStyleConfig(container_config.descendant_border,
                        container_config_info, "descendantBorder");

  return container_config_info;
}

std::unique_ptr<protocol::DictionaryValue>
BuildIsolationModeHighlightConfigInfo(
    const InspectorIsolationModeHighlightConfig& config) {
  std::unique_ptr<protocol::DictionaryValue> config_info =
      protocol::DictionaryValue::create();

  config_info->setString("resizerColor",
                         config.resizer_color.SerializeAsCSSColor());
  config_info->setString("resizerHandleColor",
                         config.resizer_handle_color.SerializeAsCSSColor());
  config_info->setString("maskColor", config.mask_color.SerializeAsCSSColor());

  return config_info;
}

// Swaps |left| and |top| of an offset.
PhysicalOffset Transpose(PhysicalOffset& offset) {
  return PhysicalOffset(offset.top, offset.left);
}

LayoutUnit TranslateRTLCoordinate(const LayoutObject* layout_object,
                                  LayoutUnit position,
                                  const Vector<LayoutUnit>& column_positions) {
  // This should only be called on grid layout objects.
  DCHECK(layout_object->IsLayoutGrid());
  DCHECK(!layout_object->StyleRef().IsLeftToRightDirection());

  LayoutUnit alignment_offset = column_positions.front();
  LayoutUnit right_grid_edge_position = column_positions.back();
  return right_grid_edge_position + alignment_offset - position;
}

LayoutUnit GetPositionForTrackAt(const LayoutObject* layout_object,
                                 wtf_size_t index,
                                 GridTrackSizingDirection direction,
                                 const Vector<LayoutUnit>& positions) {
  if (direction == kForRows)
    return positions.at(index);

  LayoutUnit position = positions.at(index);
  return layout_object->StyleRef().IsLeftToRightDirection()
             ? position
             : TranslateRTLCoordinate(layout_object, position, positions);
}

LayoutUnit GetPositionForFirstTrack(const LayoutObject* layout_object,
                                    GridTrackSizingDirection direction,
                                    const Vector<LayoutUnit>& positions) {
  return GetPositionForTrackAt(layout_object, 0, direction, positions);
}

LayoutUnit GetPositionForLastTrack(const LayoutObject* layout_object,
                                   GridTrackSizingDirection direction,
                                   const Vector<LayoutUnit>& positions) {
  wtf_size_t index = positions.size() - 1;
  return GetPositionForTrackAt(layout_object, index, direction, positions);
}

PhysicalOffset LocalToAbsolutePoint(Node* node,
                                    PhysicalOffset local,
                                    float scale) {
  LayoutObject* layout_object = node->GetLayoutObject();
  PhysicalOffset abs_point = layout_object->LocalToAbsolutePoint(local);
  gfx::PointF abs_point_in_viewport = FramePointToViewport(
      node->GetDocument().View(), gfx::PointF(abs_point.left, abs_point.top));
  PhysicalOffset scaled_abs_point =
      PhysicalOffset::FromPointFRound(abs_point_in_viewport);
  scaled_abs_point.Scale(scale);
  return scaled_abs_point;
}

String SnapAlignToString(const cc::SnapAlignment& value) {
  switch (value) {
    case cc::SnapAlignment::kNone:
      return "none";
    case cc::SnapAlignment::kStart:
      return "start";
    case cc::SnapAlignment::kEnd:
      return "end";
    case cc::SnapAlignment::kCenter:
      return "center";
  }
}

std::unique_ptr<protocol::ListValue> BuildPathFromQuad(
    const blink::LocalFrameView* containing_view,
    gfx::QuadF quad) {
  FrameQuadToViewport(containing_view, quad);
  PathBuilder builder;
  builder.AppendPath(QuadToPath(quad),
                     DeviceScaleFromFrameView(containing_view));
  return builder.Release();
}

void BuildSnapAlignment(const cc::ScrollSnapType& snap_type,
                        const cc::SnapAlignment& alignment_block,
                        const cc::SnapAlignment& alignment_inline,
                        std::unique_ptr<protocol::DictionaryValue>& result) {
  if (snap_type.axis == cc::SnapAxis::kBlock ||
      snap_type.axis == cc::SnapAxis::kBoth ||
      snap_type.axis == cc::SnapAxis::kY) {
    result->setString("alignBlock", SnapAlignToString(alignment_block));
  }
  if (snap_type.axis == cc::SnapAxis::kInline ||
      snap_type.axis == cc::SnapAxis::kBoth ||
      snap_type.axis == cc::SnapAxis::kX) {
    result->setString("alignInline", SnapAlignToString(alignment_inline));
  }
}

std::unique_ptr<protocol::DictionaryValue> BuildPosition(
    PhysicalOffset position) {
  std::unique_ptr<protocol::DictionaryValue> result =
      protocol::DictionaryValue::create();
  result->setDouble("x", position.left);
  result->setDouble("y", position.top);
  return result;
}

std::unique_ptr<protocol::ListValue> BuildGridTrackSizes(
    Node* node,
    GridTrackSizingDirection direction,
    float scale,
    LayoutUnit gap,
    LayoutUnit rtl_offset,
    const Vector<LayoutUnit>& positions,
    const Vector<LayoutUnit>& alt_axis_positions,
    const Vector<String>* authored_values) {
  LayoutObject* layout_object = node->GetLayoutObject();
  bool is_rtl = !layout_object->StyleRef().IsLeftToRightDirection();

  std::unique_ptr<protocol::ListValue> sizes = protocol::ListValue::create();
  wtf_size_t track_count = positions.size();
  LayoutUnit alt_axis_pos = GetPositionForFirstTrack(
      layout_object, direction == kForRows ? kForColumns : kForRows,
      alt_axis_positions);
  if (is_rtl && direction == kForRows)
    alt_axis_pos += rtl_offset;

  for (wtf_size_t i = 1; i < track_count; i++) {
    LayoutUnit current_position =
        GetPositionForTrackAt(layout_object, i, direction, positions);
    LayoutUnit prev_position =
        GetPositionForTrackAt(layout_object, i - 1, direction, positions);

    LayoutUnit gap_offset = i < track_count - 1 ? gap : LayoutUnit();
    LayoutUnit width = current_position - prev_position - gap_offset;
    if (is_rtl && direction == kForColumns)
      width = prev_position - current_position - gap_offset;
    LayoutUnit main_axis_pos = prev_position + width / 2;
    if (is_rtl && direction == kForColumns)
      main_axis_pos = rtl_offset + prev_position - width / 2;
    auto adjusted_size = AdjustForAbsoluteZoom::AdjustFloat(
        width * scale, layout_object->StyleRef());
    PhysicalOffset track_size_pos(main_axis_pos, alt_axis_pos);
    if (direction == kForRows)
      track_size_pos = Transpose(track_size_pos);
    std::unique_ptr<protocol::DictionaryValue> size_info =
        BuildPosition(LocalToAbsolutePoint(node, track_size_pos, scale));
    size_info->setDouble("computedSize", adjusted_size);
    if (i - 1 < authored_values->size()) {
      size_info->setString("authoredSize", authored_values->at(i - 1));
    }
    sizes->pushValue(std::move(size_info));
  }

  return sizes;
}

std::unique_ptr<protocol::ListValue> BuildGridPositiveLineNumberPositions(
    Node* node,
    const LayoutUnit& grid_gap,
    GridTrackSizingDirection direction,
    float scale,
    LayoutUnit rtl_offset,
    const Vector<LayoutUnit>& positions,
    const Vector<LayoutUnit>& alt_axis_positions) {
  auto* grid = To<LayoutGrid>(node->GetLayoutObject());
  bool is_rtl = !grid->StyleRef().IsLeftToRightDirection();

  std::unique_ptr<protocol::ListValue> number_positions =
      protocol::ListValue::create();

  wtf_size_t track_count = positions.size();
  LayoutUnit alt_axis_pos = GetPositionForFirstTrack(
      grid, direction == kForRows ? kForColumns : kForRows, alt_axis_positions);

  if (is_rtl && direction == kForRows)
    alt_axis_pos += rtl_offset;

  // Find index of the first explicit Grid Line.
  wtf_size_t first_explicit_index =
      grid->ExplicitGridStartForDirection(direction);

  // Go line by line, calculating the offset to fall in the middle of gaps
  // if needed.
  for (wtf_size_t i = first_explicit_index; i < track_count; ++i) {
    LayoutUnit gapOffset = grid_gap / 2;
    if (is_rtl && direction == kForColumns)
      gapOffset *= -1;
    // No need for a gap offset if there is no gap, or the first line is
    // explicit, or this is the last line.
    if (grid_gap == 0 || i == 0 || i == track_count - 1) {
      gapOffset = LayoutUnit();
    }
    LayoutUnit offset = GetPositionForTrackAt(grid, i, direction, positions);
    if (is_rtl && direction == kForColumns)
      offset += rtl_offset;
    PhysicalOffset number_position(offset - gapOffset, alt_axis_pos);
    if (direction == kForRows)
      number_position = Transpose(number_position);
    number_positions->pushValue(
        BuildPosition(LocalToAbsolutePoint(node, number_position, scale)));
  }

  return number_positions;
}

std::unique_ptr<protocol::ListValue> BuildGridNegativeLineNumberPositions(
    Node* node,
    const LayoutUnit& grid_gap,
    GridTrackSizingDirection direction,
    float scale,
    LayoutUnit rtl_offset,
    const Vector<LayoutUnit>& positions,
    const Vector<LayoutUnit>& alt_axis_positions) {
  auto* grid = To<LayoutGrid>(node->GetLayoutObject());
  bool is_rtl = !grid->StyleRef().IsLeftToRightDirection();

  std::unique_ptr<protocol::ListValue> number_positions =
      protocol::ListValue::create();

  wtf_size_t track_count = positions.size();
  LayoutUnit alt_axis_pos = GetPositionForLastTrack(
      grid, direction == kForRows ? kForColumns : kForRows, alt_axis_positions);
  if (is_rtl && direction == kForRows)
    alt_axis_pos += rtl_offset;

  // This is the number of tracks from the start of the grid, to the end of the
  // explicit grid (including any leading implicit tracks).
  size_t explicit_grid_end_track_count =
      grid->ExplicitGridEndForDirection(direction);

  {
    LayoutUnit first_offset =
        GetPositionForFirstTrack(grid, direction, positions);
    if (is_rtl && direction == kForColumns)
      first_offset += rtl_offset;

    // Always start negative numbers at the first line.
    std::unique_ptr<protocol::DictionaryValue> pos =
        protocol::DictionaryValue::create();
    PhysicalOffset number_position(first_offset, alt_axis_pos);
    if (direction == kForRows)
      number_position = Transpose(number_position);
    number_positions->pushValue(
        BuildPosition(LocalToAbsolutePoint(node, number_position, scale)));
  }

  // Then go line by line, calculating the offset to fall in the middle of gaps
  // if needed.
  for (wtf_size_t i = 1; i <= explicit_grid_end_track_count; i++) {
    LayoutUnit gapOffset = grid_gap / 2;
    if (is_rtl && direction == kForColumns)
      gapOffset *= -1;
    if (grid_gap == 0 ||
        (i == explicit_grid_end_track_count && i == track_count - 1)) {
      gapOffset = LayoutUnit();
    }
    LayoutUnit offset = GetPositionForTrackAt(grid, i, direction, positions);
    if (is_rtl && direction == kForColumns)
      offset += rtl_offset;
    PhysicalOffset number_position(offset - gapOffset, alt_axis_pos);
    if (direction == kForRows)
      number_position = Transpose(number_position);
    number_positions->pushValue(
        BuildPosition(LocalToAbsolutePoint(node, number_position, scale)));
  }

  return number_positions;
}

bool IsLayoutNGFlexibleBox(const LayoutObject& layout_object) {
  return layout_object.StyleRef().IsDisplayFlexibleBox() &&
         layout_object.IsFlexibleBox();
}

bool IsLayoutNGFlexItem(const LayoutObject& layout_object) {
  return !layout_object.GetNode()->IsDocumentNode() &&
         IsLayoutNGFlexibleBox(*layout_object.Parent()) &&
         To<LayoutBox>(layout_object).IsFlexItem();
}

std::unique_ptr<protocol::DictionaryValue> BuildAreaNamePaths(
    Node* node,
    float scale,
    const Vector<LayoutUnit>& rows,
    const Vector<LayoutUnit>& columns) {
  const auto* grid = To<LayoutGrid>(node->GetLayoutObject());
  LocalFrameView* containing_view = node->GetDocument().View();
  bool is_rtl = !grid->StyleRef().IsLeftToRightDirection();

  std::unique_ptr<protocol::DictionaryValue> area_paths =
      protocol::DictionaryValue::create();

  if (!grid->StyleRef().GridTemplateAreas()) {
    return area_paths;
  }

  LayoutUnit row_gap = grid->GridGap(kForRows);
  LayoutUnit column_gap = grid->GridGap(kForColumns);

  if (const NamedGridAreaMap* named_area_map =
          grid->CachedPlacementData().line_resolver.NamedAreasMap()) {
    for (const auto& item : *named_area_map) {
      const GridArea& area = item.value;
      const String& name = item.key;

      const auto start_column = GetPositionForTrackAt(
          grid, area.columns.StartLine(), kForColumns, columns);
      const auto end_column = GetPositionForTrackAt(
          grid, area.columns.EndLine(), kForColumns, columns);
      const auto start_row =
          GetPositionForTrackAt(grid, area.rows.StartLine(), kForRows, rows);
      const auto end_row =
          GetPositionForTrackAt(grid, area.rows.EndLine(), kForRows, rows);

      // Only subtract the gap size if the end line isn't the last line in the
      // container.
      const auto row_gap_offset =
          (area.rows.EndLine() == rows.size() - 1) ? LayoutUnit() : row_gap;
      auto column_gap_offset = (area.columns.EndLine() == columns.size() - 1)
                                   ? LayoutUnit()
                                   : column_gap;
      if (is_rtl) {
        column_gap_offset = -column_gap_offset;
      }

      PhysicalOffset position(start_column, start_row);
      PhysicalSize size(end_column - start_column - column_gap_offset,
                        end_row - start_row - row_gap_offset);
      gfx::QuadF area_quad = grid->LocalRectToAbsoluteQuad({position, size});
      FrameQuadToViewport(containing_view, area_quad);
      PathBuilder area_builder;
      area_builder.AppendPath(QuadToPath(area_quad), scale);

      area_paths->setValue(name, area_builder.Release());
    }
  }
  return area_paths;
}

std::unique_ptr<protocol::ListValue> BuildGridLineNames(
    Node* node,
    GridTrackSizingDirection direction,
    float scale,
    const Vector<LayoutUnit>& positions,
    const Vector<LayoutUnit>& alt_axis_positions) {
  auto* grid = To<LayoutGrid>(node->GetLayoutObject());
  const ComputedStyle& grid_container_style = grid->StyleRef();
  bool is_rtl = direction == kForColumns &&
                !grid_container_style.IsLeftToRightDirection();

  std::unique_ptr<protocol::ListValue> lines = protocol::ListValue::create();

  LayoutUnit gap = grid->GridGap(direction);
  LayoutUnit alt_axis_pos = GetPositionForFirstTrack(
      grid, direction == kForRows ? kForColumns : kForRows, alt_axis_positions);

  auto process_grid_lines_map = [&](const NamedGridLinesMap& named_lines_map) {
    for (const auto& item : named_lines_map) {
      const String& name = item.key;

      for (const wtf_size_t index : item.value) {
        LayoutUnit track =
            GetPositionForTrackAt(grid, index, direction, positions);

        LayoutUnit gap_offset =
            index > 0 && index < positions.size() - 1 ? gap / 2 : LayoutUnit();
        if (is_rtl)
          gap_offset *= -1;

        LayoutUnit main_axis_pos = track - gap_offset;
        PhysicalOffset line_name_pos(main_axis_pos, alt_axis_pos);

        if (direction == kForRows)
          line_name_pos = Transpose(line_name_pos);

        std::unique_ptr<protocol::DictionaryValue> line =
            BuildPosition(LocalToAbsolutePoint(node, line_name_pos, scale));

        line->setString("name", name);

        lines->pushValue(std::move(line));
      }
    }
  };

  const NamedGridLinesMap& explicit_lines_map =
      grid->CachedPlacementData().line_resolver.ExplicitNamedLinesMap(
          direction);
  process_grid_lines_map(explicit_lines_map);
  const NamedGridLinesMap& implicit_lines_map =
      grid->CachedPlacementData().line_resolver.ImplicitNamedLinesMap(
          direction);
  process_grid_lines_map(implicit_lines_map);

  return lines;
}

// Gets the rotation angle of the grid layout (clock-wise).
int GetRotationAngle(LayoutObject* layout_object) {
  // Local vector has 135deg bearing to the Y axis.
  int local_vector_bearing = 135;
  gfx::PointF local_a(0, 0);
  gfx::PointF local_b(1, 1);
  gfx::PointF abs_a = layout_object->LocalToAbsolutePoint(local_a);
  gfx::PointF abs_b = layout_object->LocalToAbsolutePoint(local_b);
  // Compute bearing of the absolute vector against the Y axis.
  double theta = atan2(abs_b.x() - abs_a.x(), abs_a.y() - abs_b.y());
  if (theta < 0.0)
    theta += kTwoPiDouble;
  int bearing = std::round(Rad2deg(theta));
  return bearing - local_vector_bearing;
}

String GetWritingMode(const ComputedStyle& computed_style) {
  // The grid overlay uses this to flip the grid lines and labels accordingly.
  switch (computed_style.GetWritingMode()) {
    case WritingMode::kVerticalLr:
      return "vertical-lr";
    case WritingMode::kVerticalRl:
      return "vertical-rl";
    case WritingMode::kSidewaysLr:
      return "sideways-lr";
    case WritingMode::kSidewaysRl:
      return "sideways-rl";
    case WritingMode::kHorizontalTb:
      return "horizontal-tb";
  }
}

// Gets the list of authored track size values resolving repeat() functions
// and skipping line names.
Vector<String> GetAuthoredGridTrackSizes(const CSSValue* value,
                                         size_t auto_repeat_count) {
  Vector<String> result;

  if (!value)
    return result;

  // TODO(alexrudenko): this would not handle track sizes defined using CSS
  // variables.
  const CSSValueList* value_list = DynamicTo<CSSValueList>(value);

  if (!value_list)
    return result;

  for (auto list_value : *value_list) {
    if (IsA<cssvalue::CSSGridAutoRepeatValue>(list_value.Get())) {
      Vector<String> repeated_track_sizes;
      for (auto auto_repeat_value : To<CSSValueList>(*list_value)) {
        if (!auto_repeat_value->IsGridLineNamesValue())
          repeated_track_sizes.push_back(auto_repeat_value->CssText());
      }
      // There could be only one auto repeat value in a |value_list|, therefore,
      // resetting auto_repeat_count to zero after inserting repeated values.
      for (; auto_repeat_count; --auto_repeat_count)
        result.AppendVector(repeated_track_sizes);
      continue;
    }

    if (auto* repeated_values =
            DynamicTo<cssvalue::CSSGridIntegerRepeatValue>(list_value.Get())) {
      size_t repetitions = repeated_values->Repetitions();
      for (size_t i = 0; i < repetitions; ++i) {
        for (auto repeated_value : *repeated_values) {
          if (repeated_value->IsGridLineNamesValue())
            continue;
          result.push_back(repeated_value->CssText());
        }
      }
      continue;
    }

    if (list_value->IsGridLineNamesValue())
      continue;

    result.push_back(list_value->CssText());
  }

  return result;
}

bool IsHorizontalFlex(LayoutObject* layout_flex) {
  return layout_flex->StyleRef().IsHorizontalWritingMode() !=
         layout_flex->StyleRef().ResolvedIsColumnFlexDirection();
}

DevtoolsFlexInfo GetFlexLinesAndItems(LayoutBox* layout_box,
                                      bool is_horizontal,
                                      bool is_reverse) {
  if (auto* layout_ng_flex = DynamicTo<LayoutFlexibleBox>(layout_box)) {
    const DevtoolsFlexInfo* flex_info_from_layout =
        layout_ng_flex->FlexLayoutData();
    if (flex_info_from_layout)
      return *flex_info_from_layout;
  }

  DevtoolsFlexInfo flex_info;
  Vector<DevtoolsFlexInfo::Line>& flex_lines = flex_info.lines;
  // Flex containers can't get fragmented yet, but this may change in the
  // future.
  for (const auto& fragment : layout_box->PhysicalFragments()) {
    LayoutUnit progression;

    for (const auto& child : fragment.Children()) {
      const PhysicalFragment* child_fragment = child.get();
      if (!child_fragment || child_fragment->IsOutOfFlowPositioned())
        continue;

      PhysicalSize fragment_size = child_fragment->Size();
      PhysicalOffset fragment_offset = child.Offset();

      const LayoutObject* object = child_fragment->GetLayoutObject();
      const auto* box = To<LayoutBox>(object);

      LayoutUnit baseline =
          LogicalBoxFragment(layout_box->StyleRef().GetWritingDirection(),
                             *To<PhysicalBoxFragment>(child_fragment))
              .FirstBaselineOrSynthesize(
                  layout_box->StyleRef().GetFontBaseline());
      float adjusted_baseline = AdjustForAbsoluteZoom::AdjustFloat(
          baseline + box->MarginTop(), box->StyleRef());

      PhysicalRect item_rect =
          PhysicalRect(fragment_offset.left - box->MarginLeft(),
                       fragment_offset.top - box->MarginTop(),
                       fragment_size.width + box->MarginWidth(),
                       fragment_size.height + box->MarginHeight());

      LayoutUnit item_start = is_horizontal ? item_rect.X() : item_rect.Y();
      LayoutUnit item_end = is_horizontal ? item_rect.X() + item_rect.Width()
                                          : item_rect.Y() + item_rect.Height();

      if (flex_lines.empty() ||
          (is_reverse ? item_end > progression : item_start < progression)) {
        flex_lines.emplace_back();
      }

      flex_lines.back().items.push_back(
          DevtoolsFlexInfo::Item(item_rect, LayoutUnit(adjusted_baseline)));

      progression = is_reverse ? item_start : item_end;
    }
  }

  return flex_info;
}

std::unique_ptr<protocol::DictionaryValue> BuildFlexContainerInfo(
    Element* element,
    const InspectorFlexContainerHighlightConfig&
        flex_container_highlight_config,
    float scale) {
  CSSComputedStyleDeclaration* style =
      MakeGarbageCollected<CSSComputedStyleDeclaration>(element, true);
  LocalFrameView* containing_view = element->GetDocument().View();
  LayoutObject* layout_object = element->GetLayoutObject();
  auto* layout_box = To<LayoutBox>(layout_object);
  DCHECK(layout_object);
  bool is_horizontal = IsHorizontalFlex(layout_object);
  bool is_reverse = layout_object->StyleRef().ResolvedIsReverseFlexDirection();

  std::unique_ptr<protocol::DictionaryValue> flex_info =
      protocol::DictionaryValue::create();

  // Create the path for the flex container
  PathBuilder container_builder;
  PhysicalRect content_box = layout_box->PhysicalContentBoxRect();
  gfx::QuadF content_quad = layout_object->LocalRectToAbsoluteQuad(content_box);
  FrameQuadToViewport(containing_view, content_quad);
  container_builder.AppendPath(QuadToPath(content_quad), scale);

  // Gather all flex items, sorted by flex line.
  DevtoolsFlexInfo flex_lines =
      GetFlexLinesAndItems(layout_box, is_horizontal, is_reverse);

  // We send a list of flex lines, each containing a list of flex items, with
  // their baselines, to the frontend.
  std::unique_ptr<protocol::ListValue> lines_info =
      protocol::ListValue::create();
  for (auto line : flex_lines.lines) {
    std::unique_ptr<protocol::ListValue> items_info =
        protocol::ListValue::create();
    for (auto item_data : line.items) {
      std::unique_ptr<protocol::DictionaryValue> item_info =
          protocol::DictionaryValue::create();

      gfx::QuadF item_margin_quad =
          layout_object->LocalRectToAbsoluteQuad(item_data.rect);
      FrameQuadToViewport(containing_view, item_margin_quad);
      PathBuilder item_builder;
      item_builder.AppendPath(QuadToPath(item_margin_quad), scale);

      item_info->setValue("itemBorder", item_builder.Release());
      item_info->setDouble("baseline", item_data.baseline);

      items_info->pushValue(std::move(item_info));
    }
    lines_info->pushValue(std::move(items_info));
  }

  flex_info->setValue("containerBorder", container_builder.Release());
  flex_info->setArray("lines", std::move(lines_info));
  flex_info->setBoolean("isHorizontalFlow", is_horizontal);
  flex_info->setBoolean("isReverse", is_reverse);
  flex_info->setString(
      "alignItemsStyle",
      style->GetPropertyCSSValue(CSSPropertyID::kAlignItems)->CssText());

  double row_gap_value = 0;
  const CSSValue* row_gap = style->GetPropertyCSSValue(CSSPropertyID::kRowGap);
  if (row_gap->IsNumericLiteralValue()) {
    row_gap_value = To<CSSNumericLiteralValue>(row_gap)->DoubleValue();
  }

  double column_gap_value = 0;
  const CSSValue* column_gap =
      style->GetPropertyCSSValue(CSSPropertyID::kColumnGap);
  if (column_gap->IsNumericLiteralValue()) {
    column_gap_value = To<CSSNumericLiteralValue>(column_gap)->DoubleValue();
  }

  flex_info->setDouble("mainGap",
                       is_horizontal ? column_gap_value : row_gap_value);
  flex_info->setDouble("crossGap",
                       is_horizontal ? row_gap_value : column_gap_value);

  flex_info->setValue(
      "flexContainerHighlightConfig",
      BuildFlexContainerHighlightConfigInfo(flex_container_highlight_config));

  return flex_info;
}

std::unique_ptr<protocol::DictionaryValue> BuildFlexItemInfo(
    Element* element,
    const InspectorFlexItemHighlightConfig& flex_item_highlight_config,
    float scale) {
  std::unique_ptr<protocol::DictionaryValue> flex_info =
      protocol::DictionaryValue::create();

  LayoutObject* layout_object = element->GetLayoutObject();
  bool is_horizontal = IsHorizontalFlex(layout_object->Parent());
  Length base_size = Length::Auto();

  const Length& flex_basis = layout_object->StyleRef().FlexBasis();
  const Length& size = is_horizontal ? layout_object->StyleRef().Width()
                                     : layout_object->StyleRef().Height();

  if (flex_basis.IsFixed()) {
    base_size = flex_basis;
  } else if (flex_basis.IsAuto() && size.IsFixed()) {
    base_size = size;
  }

  // For now, we only care about the cases where we can know the base size.
  if (base_size.IsFixed()) {
    flex_info->setDouble("baseSize", base_size.Pixels() * scale);
    flex_info->setBoolean("isHorizontalFlow", is_horizontal);
    auto box_sizing = layout_object->StyleRef().BoxSizing();
    flex_info->setString("boxSizing", box_sizing == EBoxSizing::kBorderBox
                                          ? "border"
                                          : "content");

    flex_info->setValue(
        "flexItemHighlightConfig",
        BuildFlexItemHighlightConfigInfo(flex_item_highlight_config));
  }

  return flex_info;
}

std::unique_ptr<protocol::DictionaryValue> BuildGridInfo(
    Element* element,
    const InspectorGridHighlightConfig& grid_highlight_config,
    float scale,
    bool isPrimary) {
  LocalFrameView* containing_view = element->GetDocument().View();
  DCHECK(element->GetLayoutObject());
  auto* grid = To<LayoutGrid>(element->GetLayoutObject());

  std::unique_ptr<protocol::DictionaryValue> grid_info =
      protocol::DictionaryValue::create();

  const Vector<LayoutUnit> rows = grid->RowPositions();
  const Vector<LayoutUnit> columns = grid->ColumnPositions();

  grid_info->setInteger("rotationAngle", GetRotationAngle(grid));

  // The grid track information collected in this method and sent to the overlay
  // frontend assumes that the grid layout is in a horizontal-tb writing-mode.
  // It is the responsibility of the frontend to flip the rendering of the grid
  // overlay based on the following writingMode value.
  grid_info->setString("writingMode", GetWritingMode(grid->StyleRef()));

  auto row_gap = grid->GridGap(kForRows) + grid->GridItemOffset(kForRows);
  auto column_gap =
      grid->GridGap(kForColumns) + grid->GridItemOffset(kForColumns);

  // The last column in RTL will not go to the extent of the grid if not
  // necessary, and will stop sooner if the tracks don't take up the full size
  // of the grid.
  LayoutUnit rtl_offset =
      grid->LogicalWidth() - columns.back() - grid->BorderAndPaddingInlineEnd();

  if (grid_highlight_config.show_track_sizes) {
    StyleResolver& style_resolver = element->GetDocument().GetStyleResolver();

    HeapHashMap<CSSPropertyName, Member<const CSSValue>> cascaded_values =
        style_resolver.CascadedValuesForElement(element, kPseudoIdNone);

    auto FindCSSValue =
        [&cascaded_values](CSSPropertyID id) -> const CSSValue* {
      auto it = cascaded_values.find(CSSPropertyName(id));
      return it != cascaded_values.end() ? it->value : nullptr;
    };
    Vector<String> column_authored_values = GetAuthoredGridTrackSizes(
        FindCSSValue(CSSPropertyID::kGridTemplateColumns),
        grid->AutoRepeatCountForDirection(kForColumns));
    Vector<String> row_authored_values = GetAuthoredGridTrackSizes(
        FindCSSValue(CSSPropertyID::kGridTemplateRows),
        grid->AutoRepeatCountForDirection(kForRows));

    grid_info->setValue(
        "columnTrackSizes",
        BuildGridTrackSizes(element, kForColumns, scale, column_gap, rtl_offset,
                            columns, rows, &column_authored_values));
    grid_info->setValue(
        "rowTrackSizes",
        BuildGridTrackSizes(element, kForRows, scale, row_gap, rtl_offset, rows,
                            columns, &row_authored_values));
  }

  bool is_ltr = grid->StyleRef().IsLeftToRightDirection();

  PathBuilder row_builder;
  PathBuilder row_gap_builder;
  LayoutUnit row_left = columns.front();
  if (!is_ltr) {
    row_left += rtl_offset;
  }
  LayoutUnit row_width = columns.back() - columns.front();
  for (wtf_size_t i = 1; i < rows.size(); ++i) {
    // Rows
    PhysicalOffset position(row_left, rows.at(i - 1));
    PhysicalSize size(row_width, rows.at(i) - rows.at(i - 1));
    if (i != rows.size() - 1)
      size.height -= row_gap;
    PhysicalRect row(position, size);
    gfx::QuadF row_quad = grid->LocalRectToAbsoluteQuad(row);
    FrameQuadToViewport(containing_view, row_quad);
    row_builder.AppendPath(
        RowQuadToPath(row_quad, i == rows.size() - 1 || row_gap > 0), scale);
    // Row Gaps
    if (i != rows.size() - 1) {
      PhysicalOffset gap_position(row_left, rows.at(i) - row_gap);
      PhysicalSize gap_size(row_width, row_gap);
      PhysicalRect gap(gap_position, gap_size);
      gfx::QuadF gap_quad = grid->LocalRectToAbsoluteQuad(gap);
      FrameQuadToViewport(containing_view, gap_quad);
      row_gap_builder.AppendPath(QuadToPath(gap_quad), scale);
    }
  }
  grid_info->setValue("rows", row_builder.Release());
  grid_info->setValue("rowGaps", row_gap_builder.Release());

  PathBuilder column_builder;
  PathBuilder column_gap_builder;
  LayoutUnit column_top = rows.front();
  LayoutUnit column_height = rows.back() - rows.front();
  for (wtf_size_t i = 1; i < columns.size(); ++i) {
    PhysicalSize size(columns.at(i) - columns.at(i - 1), column_height);
    if (i != columns.size() - 1)
      size.width -= column_gap;
    LayoutUnit line_left =
        GetPositionForTrackAt(grid, i - 1, kForColumns, columns);
    if (!is_ltr) {
      line_left += rtl_offset - size.width;
    }
    PhysicalOffset position(line_left, column_top);
    PhysicalRect column(position, size);
    gfx::QuadF column_quad = grid->LocalRectToAbsoluteQuad(column);
    FrameQuadToViewport(containing_view, column_quad);
    bool draw_end_line = is_ltr ? i == columns.size() - 1 : i == 1;
    column_builder.AppendPath(
        ColumnQuadToPath(column_quad, draw_end_line || column_gap > 0), scale);
    // Column Gaps
    if (i != columns.size() - 1) {
      LayoutUnit gap_left =
          GetPositionForTrackAt(grid, i, kForColumns, columns);
      if (is_ltr)
        gap_left -= column_gap;
      else
        gap_left += rtl_offset;
      PhysicalOffset gap_position(gap_left, column_top);
      PhysicalSize gap_size(column_gap, column_height);
      PhysicalRect gap(gap_position, gap_size);
      gfx::QuadF gap_quad = grid->LocalRectToAbsoluteQuad(gap);
      FrameQuadToViewport(containing_view, gap_quad);
      column_gap_builder.AppendPath(QuadToPath(gap_quad), scale);
    }
  }
  grid_info->setValue("columns", column_builder.Release());
  grid_info->setValue("columnGaps", column_gap_builder.Release());

  // Positive Row and column Line positions
  if (grid_highlight_config.show_positive_line_numbers) {
    grid_info->setValue(
        "positiveRowLineNumberPositions",
        BuildGridPositiveLineNumberPositions(element, row_gap, kForRows, scale,
                                             rtl_offset, rows, columns));
    grid_info->setValue(
        "positiveColumnLineNumberPositions",
        BuildGridPositiveLineNumberPositions(element, column_gap, kForColumns,
                                             scale, rtl_offset, columns, rows));
  }

  // Negative Row and column Line positions
  if (grid_highlight_config.show_negative_line_numbers) {
    grid_info->setValue(
        "negativeRowLineNumberPositions",
        BuildGridNegativeLineNumberPositions(element, row_gap, kForRows, scale,
                                             rtl_offset, rows, columns));
    grid_info->setValue(
        "negativeColumnLineNumberPositions",
        BuildGridNegativeLineNumberPositions(element, column_gap, kForColumns,
                                             scale, rtl_offset, columns, rows));
  }

  // Area names
  if (grid_highlight_config.show_area_names) {
    grid_info->setValue("areaNames",
                        BuildAreaNamePaths(element, scale, rows, columns));
  }

  // line names
  if (grid_highlight_config.show_line_names) {
    grid_info->setValue(
        "rowLineNameOffsets",
        BuildGridLineNames(element, kForRows, scale, rows, columns));
    grid_info->setValue(
        "columnLineNameOffsets",
        BuildGridLineNames(element, kForColumns, scale, columns, rows));
  }

  // Grid border
  PathBuilder grid_border_builder;
  PhysicalOffset grid_position(row_left, column_top);
  PhysicalSize grid_size(row_width, column_height);
  PhysicalRect grid_rect(grid_position, grid_size);
  gfx::QuadF grid_quad = grid->LocalRectToAbsoluteQuad(grid_rect);
  FrameQuadToViewport(containing_view, grid_quad);
  grid_border_builder.AppendPath(QuadToPath(grid_quad), scale);
  grid_info->setValue("gridBorder", grid_border_builder.Release());
  grid_info->setValue("gridHighlightConfig",
                      BuildGridHighlightConfigInfo(grid_highlight_config));

  grid_info->setBoolean("isPrimaryGrid", isPrimary);
  return grid_info;
}

std::unique_ptr<protocol::DictionaryValue> BuildGridInfo(
    Element* element,
    const InspectorHighlightConfig& highlight_config,
    float scale,
    bool isPrimary) {
  // Legacy support for highlight_config.css_grid
  if (highlight_config.css_grid != Color::kTransparent) {
    std::unique_ptr<InspectorGridHighlightConfig> grid_config =
        std::make_unique<InspectorGridHighlightConfig>();
    grid_config->row_line_color = highlight_config.css_grid;
    grid_config->column_line_color = highlight_config.css_grid;
    grid_config->row_line_dash = true;
    grid_config->column_line_dash = true;
    return BuildGridInfo(element, *grid_config, scale, isPrimary);
  }

  return BuildGridInfo(element, *(highlight_config.grid_highlight_config),
                       scale, isPrimary);
}

void CollectQuads(Node* node,
                  bool adjust_for_absolute_zoom,
                  Vector<gfx::QuadF>& out_quads) {
  LayoutObject* layout_object = node->GetLayoutObject();
  // For inline elements, absoluteQuads will return a line box based on the
  // line-height and font metrics, which is technically incorrect as replaced
  // elements like images should use their intristic height and expand the
  // linebox  as needed. To get an appropriate quads we descend
  // into the children and have them add their boxes.
  //
  // Elements with display:contents style (such as slots) do not have layout
  // objects and we always look at their contents.
  if (((layout_object && layout_object->IsLayoutInline()) ||
       (!layout_object && node->IsElementNode() &&
        To<Element>(node)->HasDisplayContentsStyle())) &&
      LayoutTreeBuilderTraversal::FirstChild(*node)) {
    for (Node* child = LayoutTreeBuilderTraversal::FirstChild(*node); child;
         child = LayoutTreeBuilderTraversal::NextSibling(*child))
      CollectQuads(child, adjust_for_absolute_zoom, out_quads);
  } else if (layout_object) {
    wtf_size_t old_size = out_quads.size();
    layout_object->AbsoluteQuads(out_quads);
    wtf_size_t new_size = out_quads.size();
    LocalFrameView* containing_view = layout_object->GetFrameView();
    for (wtf_size_t i = old_size; i < new_size; i++) {
      if (containing_view)
        FrameQuadToViewport(containing_view, out_quads[i]);
      if (adjust_for_absolute_zoom) {
        AdjustForAbsoluteZoom::AdjustQuadMaybeExcludingCSSZoom(out_quads[i],
                                                               *layout_object);
      }
    }
  }
}

std::unique_ptr<protocol::Array<double>> RectForPhysicalRect(
    const PhysicalRect& rect) {
  return std::make_unique<std::vector<double>, std::initializer_list<double>>(
      {rect.X(), rect.Y(), rect.Width(), rect.Height()});
}

// Returns |layout_object|'s bounding box in document coordinates.
PhysicalRect RectInRootFrame(const LayoutObject* layout_object) {
  LocalFrameView* local_frame_view = layout_object->GetFrameView();
  PhysicalRect rect_in_absolute =
      PhysicalRect::EnclosingRect(layout_object->AbsoluteBoundingBoxRectF());
  return local_frame_view
             ? local_frame_view->ConvertToRootFrame(rect_in_absolute)
             : rect_in_absolute;
}

PhysicalRect TextFragmentRectInRootFrame(
    const LayoutObject* layout_object,
    const LayoutText::TextBoxInfo& text_box) {
  PhysicalRect absolute_coords_text_box_rect =
      layout_object->LocalToAbsoluteRect(text_box.local_rect);
  LocalFrameView* local_frame_view = layout_object->GetFrameView();
  return local_frame_view ? local_frame_view->ConvertToRootFrame(
                                absolute_coords_text_box_rect)
                          : absolute_coords_text_box_rect;
}

}  // namespace

InspectorHighlightConfig::InspectorHighlightConfig()
    : show_info(false),
      show_styles(false),
      show_rulers(false),
      show_extension_lines(false),
      show_accessibility_info(true),
      color_format(ColorFormat::kHex) {}

InspectorHighlight::InspectorHighlight(float scale)
    : InspectorHighlightBase(scale),
      show_rulers_(false),
      show_extension_lines_(false),
      show_accessibility_info_(true),
      color_format_(ColorFormat::kHex) {}

InspectorSourceOrderConfig::InspectorSourceOrderConfig() = default;

LineStyle::LineStyle() = default;

BoxStyle::BoxStyle() = default;

InspectorGridHighlightConfig::InspectorGridHighlightConfig()
    : show_grid_extension_lines(false),
      grid_border_dash(false),
      row_line_dash(false),
      column_line_dash(false),
      show_positive_line_numbers(false),
      show_negative_line_numbers(false),
      show_area_names(false),
      show_line_names(false),
      show_track_sizes(false) {}

InspectorFlexContainerHighlightConfig::InspectorFlexContainerHighlightConfig() =
    default;

InspectorFlexItemHighlightConfig::InspectorFlexItemHighlightConfig() = default;

InspectorHighlightBase::InspectorHighlightBase(float scale)
    : highlight_paths_(protocol::ListValue::create()), scale_(scale) {}

InspectorHighlightBase::InspectorHighlightBase(Node* node)
    : highlight_paths_(protocol::ListValue::create()), scale_(1.f) {
  DCHECK(!DisplayLockUtilities::LockedAncestorPreventingPaint(*node));
  LocalFrameView* frame_view = node->GetDocument().View();
  if (frame_view)
    scale_ = DeviceScaleFromFrameView(frame_view);
}

bool InspectorHighlightBase::BuildNodeQuads(Node* node,
                                            gfx::QuadF* content,
                                            gfx::QuadF* padding,
                                            gfx::QuadF* border,
                                            gfx::QuadF* margin) {
  LayoutObject* layout_object = node->GetLayoutObject();
  if (!layout_object)
    return false;

  LocalFrameView* containing_view = layout_object->GetFrameView();
  if (!containing_view)
    return false;
  if (!layout_object->IsBox() && !layout_object->IsLayoutInline() &&
      !layout_object->IsText()) {
    return false;
  }

  PhysicalRect content_box;
  PhysicalRect padding_box;
  PhysicalRect border_box;
  PhysicalRect margin_box;

  if (layout_object->IsText()) {
    auto* layout_text = To<LayoutText>(layout_object);
    PhysicalRect text_rect = layout_text->VisualOverflowRect();
    content_box = text_rect;
    padding_box = text_rect;
    border_box = text_rect;
    margin_box = text_rect;
  } else if (layout_object->IsBox()) {
    auto* layout_box = To<LayoutBox>(layout_object);
    content_box = layout_box->PhysicalContentBoxRect();

    // Include scrollbars and gutters in the padding highlight.
    padding_box = layout_box->PhysicalPaddingBoxRect();
    PhysicalBoxStrut scrollbars = layout_box->ComputeScrollbars();
    padding_box.SetX(padding_box.X() - scrollbars.left);
    padding_box.SetY(padding_box.Y() - scrollbars.top);
    padding_box.SetWidth(padding_box.Width() + scrollbars.HorizontalSum());
    padding_box.SetHeight(padding_box.Height() + scrollbars.VerticalSum());

    border_box = layout_box->PhysicalBorderBoxRect();

    margin_box = PhysicalRect(border_box.X() - layout_box->MarginLeft(),
                              border_box.Y() - layout_box->MarginTop(),
                              border_box.Width() + layout_box->MarginWidth(),
                              border_box.Height() + layout_box->MarginHeight());
  } else {
    auto* layout_inline = To<LayoutInline>(layout_object);

    // LayoutInline's bounding box includes paddings and borders, excludes
    // margins.
    border_box = layout_inline->PhysicalLinesBoundingBox();
    padding_box =
        PhysicalRect(border_box.X() + layout_inline->BorderLeft(),
                     border_box.Y() + layout_inline->BorderTop(),
                     border_box.Width() - layout_inline->BorderLeft() -
                         layout_inline->BorderRight(),
                     border_box.Height() - layout_inline->BorderTop() -
                         layout_inline->BorderBottom());
    content_box =
        PhysicalRect(padding_box.X() + layout_inline->PaddingLeft(),
                     padding_box.Y() + layout_inline->PaddingTop(),
                     padding_box.Width() - layout_inline->PaddingLeft() -
                         layout_inline->PaddingRight(),
                     padding_box.Height() - layout_inline->PaddingTop() -
                         layout_inline->PaddingBottom());
    // Ignore marginTop and marginBottom for inlines.
    margin_box = PhysicalRect(
        border_box.X() - layout_inline->MarginLeft(), border_box.Y(),
        border_box.Width() + layout_inline->MarginWidth(), border_box.Height());
  }

  *content = layout_object->LocalRectToAbsoluteQuad(content_box);
  *padding = layout_object->LocalRectToAbsoluteQuad(padding_box);
  *border = layout_object->LocalRectToAbsoluteQuad(border_box);
  *margin = layout_object->LocalRectToAbsoluteQuad(margin_box);

  FrameQuadToViewport(containing_view, *content);
  FrameQuadToViewport(containing_view, *padding);
  FrameQuadToViewport(containing_view, *border);
  FrameQuadToViewport(containing_view, *margin);

  return true;
}

void InspectorHighlightBase::AppendQuad(const gfx::QuadF& quad,
                                        const Color& fill_color,
                                        const Color& outline_color,
                                        const String& name) {
  Path path = QuadToPath(quad);
  PathBuilder builder;
  builder.AppendPath(path, scale_);
  AppendPath(builder.Release(), fill_color, outline_color, name);
}

void InspectorHighlightBase::AppendPath(
    std::unique_ptr<protocol::ListValue> path,
    const Color& fill_color,
    const Color& outline_color,
    const String& name) {
  std::unique_ptr<protocol::DictionaryValue> object =
      protocol::DictionaryValue::create();
  object->setValue("path", std::move(path));
  object->setString("fillColor", fill_color.SerializeAsCSSColor());
  if (outline_color != Color::kTransparent)
    object->setString("outlineColor", outline_color.SerializeAsCSSColor());
  if (!name.empty())
    object->setString("name", name);
  highlight_paths_->pushValue(std::move(object));
}

InspectorSourceOrderHighlight::InspectorSourceOrderHighlight(
    Node* node,
    Color outline_color,
    int source_order_position)
    : InspectorHighlightBase(node),
      source_order_position_(source_order_position) {
  gfx::QuadF content, padding, border, margin;
  if (!BuildNodeQuads(node, &content, &padding, &border, &margin))
    return;
  AppendQuad(border, Color::kTransparent, outline_color, "border");
}

std::unique_ptr<protocol::DictionaryValue>
InspectorSourceOrderHighlight::AsProtocolValue() const {
  std::unique_ptr<protocol::DictionaryValue> object =
      protocol::DictionaryValue::create();
  object->setValue("paths", highlight_paths_->clone());
  object->setInteger("sourceOrder", source_order_position_);
  return object;
}

// static
InspectorSourceOrderConfig InspectorSourceOrderHighlight::DefaultConfig() {
  InspectorSourceOrderConfig config;
  config.parent_outline_color = Color(224, 90, 183, 1);
  config.child_outline_color = Color(0, 120, 212, 1);
  return config;
}

InspectorHighlight::InspectorHighlight(
    Node* node,
    const InspectorHighlightConfig& highlight_config,
    const InspectorHighlightContrastInfo& node_contrast,
    bool append_element_info,
    bool append_distance_info,
    NodeContentVisibilityState content_visibility_state)
    : InspectorHighlightBase(node),
      show_rulers_(highlight_config.show_rulers),
      show_extension_lines_(highlight_config.show_extension_lines),
      show_accessibility_info_(highlight_config.show_accessibility_info),
      color_format_(highlight_config.color_format) {
  DCHECK_GE(node->GetDocument().Lifecycle().GetState(),
            DocumentLifecycle::kLayoutClean);
  AppendPathsForShapeOutside(node, highlight_config);
  AppendNodeHighlight(node, highlight_config);
  auto* text_node = DynamicTo<Text>(node);
  auto* element = DynamicTo<Element>(node);
  if (append_element_info && element)
    element_info_ = BuildElementInfo(element);
  else if (append_element_info && text_node)
    element_info_ = BuildTextNodeInfo(text_node);
  if (element && element_info_ && highlight_config.show_styles) {
    AppendStyleInfo(element, element_info_.get(), node_contrast,
                    highlight_config.contrast_algorithm);
  }

  if (element_info_) {
    switch (content_visibility_state) {
      case NodeContentVisibilityState::kNone:
        break;
      case NodeContentVisibilityState::kIsLocked:
        element_info_->setBoolean("isLocked", true);
        break;
      case NodeContentVisibilityState::kIsLockedAncestor:
        element_info_->setBoolean("isLockedAncestor", true);
        break;
    }

    element_info_->setBoolean("showAccessibilityInfo",
                              show_accessibility_info_);
  }

  if (append_distance_info)
    AppendDistanceInfo(node);
}

InspectorHighlight::~InspectorHighlight() = default;

void InspectorHighlight::AppendDistanceInfo(Node* node) {
  if (!InspectorHighlight::GetBoxModel(node, &model_, false))
    return;
  boxes_ = std::make_unique<protocol::Array<protocol::Array<double>>>();
  computed_style_ = protocol::DictionaryValue::create();

  node->GetDocument().EnsurePaintLocationDataValidForNode(
      node, DocumentUpdateReason::kInspector);
  LayoutObject* layout_object = node->GetLayoutObject();
  if (!layout_object)
    return;

  if (Element* element = DynamicTo<Element>(node)) {
    CSSComputedStyleDeclaration* style =
        MakeGarbageCollected<CSSComputedStyleDeclaration>(element, true);
    for (unsigned i = 0; i < style->length(); ++i) {
      AtomicString name(style->item(i));
      const CSSValue* value = style->GetPropertyCSSValue(
          CssPropertyID(element->GetExecutionContext(), name));
      if (!value) {
        continue;
      }
      if (value->IsColorValue()) {
        Color color = static_cast<const cssvalue::CSSColor*>(value)->Value();
        computed_style_->setString(name, ToHEXA(color));
      } else {
        computed_style_->setString(name, value->CssText());
      }
    }
  }

  VisitAndCollectDistanceInfo(&(node->GetDocument()));
  PhysicalRect document_rect(
      node->GetDocument().GetLayoutView()->DocumentRect());
  LocalFrameView* local_frame_view = node->GetDocument().View();
  boxes_->emplace_back(
      RectForPhysicalRect(local_frame_view->ConvertToRootFrame(document_rect)));
}

void InspectorHighlight::VisitAndCollectDistanceInfo(Node* node) {
  LayoutObject* layout_object = node->GetLayoutObject();
  if (layout_object)
    AddLayoutBoxToDistanceInfo(layout_object);

  if (auto* element = DynamicTo<Element>(node)) {
    if (element->GetPseudoId()) {
      if (layout_object)
        VisitAndCollectDistanceInfo(element->GetPseudoId(), layout_object);
    } else {
      for (PseudoId pseudo_id :
           {kPseudoIdFirstLetter, kPseudoIdScrollMarkerGroupBefore,
            kPseudoIdBefore, kPseudoIdAfter, kPseudoIdScrollMarkerGroupAfter,
            kPseudoIdScrollMarker, kPseudoIdScrollNextButton,
            kPseudoIdScrollPrevButton}) {
        if (Node* pseudo_node = element->GetPseudoElement(pseudo_id))
          VisitAndCollectDistanceInfo(pseudo_node);
      }
    }
  }

  if (!node->IsContainerNode())
    return;
  for (Node* child = blink::dom_traversal_utils::FirstChild(*node, false);
       child; child = blink::dom_traversal_utils::NextSibling(*child, false)) {
    VisitAndCollectDistanceInfo(child);
  }
}

void InspectorHighlight::VisitAndCollectDistanceInfo(
    PseudoId pseudo_id,
    LayoutObject* layout_object) {
  protocol::DOM::PseudoType pseudo_type;
  if (pseudo_id == kPseudoIdNone)
    return;
  for (LayoutObject* child = layout_object->SlowFirstChild(); child;
       child = child->NextSibling()) {
    if (child->IsAnonymous())
      AddLayoutBoxToDistanceInfo(child);
  }
}

void InspectorHighlight::AddLayoutBoxToDistanceInfo(
    LayoutObject* layout_object) {
  if (layout_object->IsText()) {
    auto* layout_text = To<LayoutText>(layout_object);
    for (const auto& text_box : layout_text->GetTextBoxInfo()) {
      PhysicalRect text_rect(
          TextFragmentRectInRootFrame(layout_object, text_box));
      boxes_->emplace_back(RectForPhysicalRect(text_rect));
    }
  } else {
    PhysicalRect rect(RectInRootFrame(layout_object));
    boxes_->emplace_back(RectForPhysicalRect(rect));
  }
}

void InspectorHighlight::AppendEventTargetQuads(
    Node* event_target_node,
    const InspectorHighlightConfig& highlight_config) {
  if (event_target_node->GetLayoutObject()) {
    gfx::QuadF border, unused;
    if (BuildNodeQuads(event_target_node, &unused, &unused, &border, &unused))
      AppendQuad(border, highlight_config.event_target);
  }
}

void InspectorHighlight::AppendPathsForShapeOutside(
    Node* node,
    const InspectorHighlightConfig& config) {
  Shape::DisplayPaths paths;
  gfx::QuadF bounds_quad;

  const ShapeOutsideInfo* shape_outside_info =
      ShapeOutsideInfoForNode(node, &paths, &bounds_quad);
  if (!shape_outside_info)
    return;

  if (!paths.shape.length()) {
    AppendQuad(bounds_quad, config.shape);
    return;
  }

  AppendPath(ShapePathBuilder::BuildPath(
                 *node->GetDocument().View(), *node->GetLayoutObject(),
                 *shape_outside_info, paths.shape, scale_),
             config.shape, Color::kTransparent);
  if (paths.margin_shape.length())
    AppendPath(ShapePathBuilder::BuildPath(
                   *node->GetDocument().View(), *node->GetLayoutObject(),
                   *shape_outside_info, paths.margin_shape, scale_),
               config.shape_margin, Color::kTransparent);
}

void InspectorHighlight::AppendNodeHighlight(
    Node* node,
    const InspectorHighlightConfig& highlight_config) {
  LayoutObject* layout_object = node->GetLayoutObject();
  if (!layout_object)
    return;

  Vector<gfx::QuadF> svg_quads;
  if (BuildSVGQuads(node, svg_quads)) {
    for (wtf_size_t i = 0; i < svg_quads.size(); ++i) {
      AppendQuad(svg_quads[i], highlight_config.content,
                 highlight_config.content_outline);
    }
    return;
  }

  gfx::QuadF content, padding, border, margin;
  if (!BuildNodeQuads(node, &content, &padding, &border, &margin))
    return;
  AppendQuad(content, highlight_config.content,
             highlight_config.content_outline, "content");
  AppendQuad(padding, highlight_config.padding, Color::kTransparent, "padding");
  AppendQuad(border, highlight_config.border, Color::kTransparent, "border");
  AppendQuad(margin, highlight_config.margin, Color::kTransparent, "margin");

  // Don't append node's grid / flex info if it's locked since those values may
  // not be generated yet.
  if (auto* context = layout_object->GetDisplayLockContext()) {
    if (context->IsLocked())
      return;
  }

  if (highlight_config.css_grid != Color::kTransparent ||
      highlight_config.grid_highlight_config) {
    grid_info_ = protocol::ListValue::create();
    if (layout_object->IsLayoutGrid()) {
      grid_info_->pushValue(
          BuildGridInfo(To<Element>(node), highlight_config, scale_, true));
    }
  }

  if (highlight_config.flex_container_highlight_config) {
    flex_container_info_ = protocol::ListValue::create();
    // Some objects are flexible boxes even though display:flex is not set, we
    // need to avoid those.
    if (IsLayoutNGFlexibleBox(*layout_object)) {
      flex_container_info_->pushValue(BuildFlexContainerInfo(
          To<Element>(node),
          *(highlight_config.flex_container_highlight_config), scale_));
    }
  }

  if (highlight_config.flex_item_highlight_config) {
    flex_item_info_ = protocol::ListValue::create();
    if (IsLayoutNGFlexItem(*layout_object)) {
      flex_item_info_->pushValue(BuildFlexItemInfo(
          To<Element>(node), *(highlight_config.flex_item_highlight_config),
          scale_));
    }
  }

  if (highlight_config.container_query_container_highlight_config) {
    container_query_container_info_ = protocol::ListValue::create();
    container_query_container_info_->pushValue(BuildContainerQueryContainerInfo(
        node, *(highlight_config.container_query_container_highlight_config),
        scale_));
  }
}

std::unique_ptr<protocol::DictionaryValue> InspectorHighlight::AsProtocolValue()
    const {
  std::unique_ptr<protocol::DictionaryValue> object =
      protocol::DictionaryValue::create();
  object->setValue("paths", highlight_paths_->clone());
  object->setBoolean("showRulers", show_rulers_);
  object->setBoolean("showExtensionLines", show_extension_lines_);
  object->setBoolean("showAccessibilityInfo", show_accessibility_info_);
  switch (color_format_) {
    case ColorFormat::kRgb:
      object->setString("colorFormat", "rgb");
      break;
    case ColorFormat::kHsl:
      object->setString("colorFormat", "hsl");
      break;
    case ColorFormat::kHwb:
      object->setString("colorFormat", "hwb");
      break;
    case ColorFormat::kHex:
      object->setString("colorFormat", "hex");
      break;
  }

  if (model_) {
    std::unique_ptr<protocol::DictionaryValue> distance_info =
        protocol::DictionaryValue::create();
    distance_info->setArray(
        "boxes",
        protocol::ValueConversions<std::vector<
            std::unique_ptr<std::vector<double>>>>::toValue(boxes_.get()));
    distance_info->setArray(
        "content", protocol::ValueConversions<std::vector<double>>::toValue(
                       model_->getContent()));
    distance_info->setArray(
        "padding", protocol::ValueConversions<std::vector<double>>::toValue(
                       model_->getPadding()));
    distance_info->setArray(
        "border", protocol::ValueConversions<std::vector<double>>::toValue(
                      model_->getBorder()));
    distance_info->setValue("style", computed_style_->clone());
    object->setValue("distanceInfo", std::move(distance_info));
  }
  if (element_info_)
    object->setValue("elementInfo", element_info_->clone());
  if (grid_info_ && grid_info_->size() > 0)
    object->setValue("gridInfo", grid_info_->clone());
  if (flex_container_info_ && flex_container_info_->size() > 0)
    object->setValue("flexInfo", flex_container_info_->clone());
  if (flex_item_info_ && flex_item_info_->size() > 0)
    object->setValue("flexItemInfo", flex_item_info_->clone());
  if (container_query_container_info_ &&
      container_query_container_info_->size() > 0) {
    object->setValue("containerQueryInfo",
                     container_query_container_info_->clone());
  }
  return object;
}

// static
bool InspectorHighlight::GetBoxModel(
    Node* node,
    std::unique_ptr<protocol::DOM::BoxModel>* model,
    bool use_absolute_zoom) {
  node->GetDocument().EnsurePaintLocationDataValidForNode(
      node, DocumentUpdateReason::kInspector);
  LayoutObject* layout_object = node->GetLayoutObject();
  LocalFrameView* view = node->GetDocument().View();
  if (!layout_object || !view)
    return false;

  gfx::QuadF content, padding, border, margin;
  Vector<gfx::QuadF> svg_quads;
  if (BuildSVGQuads(node, svg_quads)) {
    if (!svg_quads.size())
      return false;
    content = svg_quads[0];
    padding = svg_quads[0];
    border = svg_quads[0];
    margin = svg_quads[0];
  } else if (!BuildNodeQuads(node, &content, &padding, &border, &margin)) {
    return false;
  }

  if (use_absolute_zoom) {
    AdjustForAbsoluteZoom::AdjustQuadMaybeExcludingCSSZoom(content,
                                                           *layout_object);
    AdjustForAbsoluteZoom::AdjustQuadMaybeExcludingCSSZoom(padding,
                                                           *layout_object);
    AdjustForAbsoluteZoom::AdjustQuadMaybeExcludingCSSZoom(border,
                                                           *layout_object);
    AdjustForAbsoluteZoom::AdjustQuadMaybeExcludingCSSZoom(margin,
                                                           *layout_object);
  }

  float scale = PageScaleFromFrameView(view);
  content.Scale(scale, scale);
  padding.Scale(scale, scale);
  border.Scale(scale, scale);
  margin.Scale(scale, scale);

  gfx::Rect bounding_box =
      view->ConvertToRootFrame(layout_object->AbsoluteBoundingBoxRect());
  auto* model_object = DynamicTo<LayoutBoxModelObject>(layout_object);

  *model = protocol::DOM::BoxModel::create()
               .setContent(BuildArrayForQuad(content))
               .setPadding(BuildArrayForQuad(padding))
               .setBorder(BuildArrayForQuad(border))
               .setMargin(BuildArrayForQuad(margin))
               .setWidth(model_object
                             ? AdjustForAbsoluteZoom::AdjustLayoutUnit(
                                   model_object->OffsetWidth(), *model_object)
                                   .Round()
                             : bounding_box.width())
               .setHeight(model_object
                              ? AdjustForAbsoluteZoom::AdjustLayoutUnit(
                                    model_object->OffsetHeight(), *model_object)
                                    .Round()
                              : bounding_box.height())
               .build();

  Shape::DisplayPaths paths;
  gfx::QuadF bounds_quad;
  protocol::ErrorSupport errors;
  if (const ShapeOutsideInfo* shape_outside_info =
          ShapeOutsideInfoForNode(node, &paths, &bounds_quad)) {
    auto shape = ShapePathBuilder::BuildPath(
        *view, *layout_object, *shape_outside_info, paths.shape, 1.f);
    auto margin_shape = ShapePathBuilder::BuildPath(
        *view, *layout_object, *shape_outside_info, paths.margin_shape, 1.f);
    (*model)->setShapeOutside(
        protocol::DOM::ShapeOutsideInfo::create()
            .setBounds(BuildArrayForQuad(bounds_quad))
            .setShape(protocol::ValueConversions<
                      protocol::Array<protocol::Value>>::fromValue(shape.get(),
                                                                   &errors))
            .setMarginShape(
                protocol::ValueConversions<protocol::Array<protocol::Value>>::
                    fromValue(margin_shape.get(), &errors))
            .build());
  }

  return true;
}

// static
bool InspectorHighlight::BuildSVGQuads(Node* node, Vector<gfx::QuadF>& quads) {
  LayoutObject* layout_object = node->GetLayoutObject();
  if (!layout_object)
    return false;
  if (!layout_object->GetNode() || !layout_object->GetNode()->IsSVGElement() ||
      layout_object->IsSVGRoot())
    return false;
  CollectQuads(node, false /* adjust_for_absolute_zoom */, quads);
  return true;
}

// static
bool InspectorHighlight::GetContentQuads(
    Node* node,
    std::unique_ptr<protocol::Array<protocol::Array<double>>>* result) {
  LocalFrameView* view = node->GetDocument().View();
  if (!view)
    return false;
  Vector<gfx::QuadF> quads;
  CollectQuads(node, true /* adjust_for_absolute_zoom */, quads);
  float scale = PageScaleFromFrameView(view);
  for (gfx::QuadF& quad : quads)
    quad.Scale(scale, scale);

  *result = std::make_unique<protocol::Array<protocol::Array<double>>>();
  for (gfx::QuadF& quad : quads)
    (*result)->emplace_back(BuildArrayForQuad(quad));
  return true;
}

std::unique_ptr<protocol::DictionaryValue> InspectorGridHighlight(
    Node* node,
    const InspectorGridHighlightConfig& config) {
  if (DisplayLockUtilities::LockedAncestorPreventingPaint(*node)) {
    // Skip if node is part of display locked tree.
    return nullptr;
  }

  LocalFrameView* frame_view = node->GetDocument().View();
  if (!frame_view)
    return nullptr;

  float scale = DeviceScaleFromFrameView(frame_view);
  LayoutObject* layout_object = node->GetLayoutObject();
  if (!layout_object || !layout_object->IsLayoutGrid()) {
    return nullptr;
  }

  std::unique_ptr<protocol::DictionaryValue> grid_info =
      BuildGridInfo(To<Element>(node), config, scale, true);
  return grid_info;
}

std::unique_ptr<protocol::DictionaryValue> InspectorFlexContainerHighlight(
    Node* node,
    const InspectorFlexContainerHighlightConfig& config) {
  if (DisplayLockUtilities::LockedAncestorPreventingPaint(*node)) {
    // Skip if node is part of display locked tree.
    return nullptr;
  }

  LocalFrameView* frame_view = node->GetDocument().View();
  if (!frame_view)
    return nullptr;

  float scale = DeviceScaleFromFrameView(frame_view);
  LayoutObject* layout_object = node->GetLayoutObject();
  if (!layout_object || !IsLayoutNGFlexibleBox(*layout_object)) {
    return nullptr;
  }

  return BuildFlexContainerInfo(To<Element>(node), config, scale);
}

std::unique_ptr<protocol::DictionaryValue> BuildSnapContainerInfo(Node* node) {
  if (!node)
    return nullptr;

  // If scroll snapping is enabled for the document element, we should use
  // document's layout box for reading snap areas.
  LayoutBox* layout_box = node == node->GetDocument().documentElement()
                              ? node->GetDocument().GetLayoutBoxForScrolling()
                              : node->GetLayoutBox();

  if (!layout_box)
    return nullptr;

  LocalFrameView* containing_view = node->GetDocument().View();

  if (!containing_view)
    return nullptr;

  auto* scrollable_area = layout_box->GetScrollableArea();
  if (!scrollable_area)
    return nullptr;

  std::unique_ptr<protocol::DictionaryValue> scroll_snap_info =
      protocol::DictionaryValue::create();
  auto scroll_position = scrollable_area->ScrollPosition();
  auto* container_data = scrollable_area->GetSnapContainerData();

  if (!container_data)
    return nullptr;

  gfx::QuadF snapport_quad =
      layout_box->LocalToAbsoluteQuad(gfx::QuadF(container_data->rect()));
  scroll_snap_info->setValue("snapport",
                             BuildPathFromQuad(containing_view, snapport_quad));

  auto padding_box = layout_box->PhysicalPaddingBoxRect();
  gfx::QuadF padding_box_quad =
      layout_box->LocalRectToAbsoluteQuad(padding_box);
  scroll_snap_info->setValue(
      "paddingBox", BuildPathFromQuad(containing_view, padding_box_quad));

  auto snap_type = container_data->scroll_snap_type();
  std::unique_ptr<protocol::ListValue> result_areas =
      protocol::ListValue::create();
  std::vector<cc::SnapAreaData> snap_area_items;
  snap_area_items.reserve(container_data->size());
  for (size_t i = 0; i < container_data->size(); i++) {
    cc::SnapAreaData data = container_data->at(i);
    data.rect.Offset(-scroll_position.x(), -scroll_position.y());
    snap_area_items.push_back(std::move(data));
  }

  std::sort(snap_area_items.begin(), snap_area_items.end(),
            [](const cc::SnapAreaData& a, const cc::SnapAreaData& b) -> bool {
              return a.rect.origin() < b.rect.origin();
            });

  for (const auto& data : snap_area_items) {
    std::unique_ptr<protocol::DictionaryValue> result_area =
        protocol::DictionaryValue::create();

    gfx::QuadF area_quad =
        layout_box->LocalToAbsoluteQuad(gfx::QuadF(data.rect));
    result_area->setValue("path",
                          BuildPathFromQuad(containing_view, area_quad));

    Node* area_node = DOMNodeIds::NodeForId(
        DOMNodeIdFromCompositorElementId(data.element_id));
    DCHECK(area_node);
    if (!area_node)
      continue;

    auto* area_layout_box = area_node->GetLayoutBox();
    gfx::QuadF area_box_quad = area_layout_box->LocalRectToAbsoluteQuad(
        area_layout_box->PhysicalBorderBoxRect());
    result_area->setValue("borderBox",
                          BuildPathFromQuad(containing_view, area_box_quad));

    BuildSnapAlignment(snap_type, data.scroll_snap_align.alignment_block,
                       data.scroll_snap_align.alignment_inline, result_area);

    result_areas->pushValue(std::move(result_area));
  }
  scroll_snap_info->setArray("snapAreas", std::move(result_areas));

  return scroll_snap_info;
}

std::unique_ptr<protocol::DictionaryValue> InspectorScrollSnapHighlight(
    Node* node,
    const InspectorScrollSnapContainerHighlightConfig& config) {
  std::unique_ptr<protocol::DictionaryValue> scroll_snap_info =
      BuildSnapContainerInfo(node);

  if (!scroll_snap_info)
    return nullptr;

  AppendLineStyleConfig(config.snapport_border, scroll_snap_info,
                        "snapportBorder");
  AppendLineStyleConfig(config.snap_area_border, scroll_snap_info,
                        "snapAreaBorder");
  scroll_snap_info->setString("scrollMarginColor",
                              config.scroll_margin_color.SerializeAsCSSColor());
  scroll_snap_info->setString(
      "scrollPaddingColor", config.scroll_padding_color.SerializeAsCSSColor());

  return scroll_snap_info;
}

Vector<gfx::QuadF> GetContainerQueryingDescendantQuads(Element* container) {
  Vector<gfx::QuadF> descendant_quads;
  for (Element* descendant :
       InspectorDOMAgent::GetContainerQueryingDescendants(container)) {
    LayoutBox* layout_box = descendant->GetLayoutBox();
    if (!layout_box)
      continue;
    auto content_box = layout_box->PhysicalContentBoxRect();
    gfx::QuadF content_quad = layout_box->LocalRectToAbsoluteQuad(content_box);
    descendant_quads.push_back(content_quad);
  }

  return descendant_quads;
}

std::unique_ptr<protocol::DictionaryValue> BuildContainerQueryContainerInfo(
    Node* node,
    const InspectorContainerQueryContainerHighlightConfig&
        container_query_container_highlight_config,
    float scale) {
  if (!node)
    return nullptr;

  LayoutBox* layout_box = node->GetLayoutBox();
  if (!layout_box)
    return nullptr;

  LocalFrameView* containing_view = node->GetDocument().View();
  if (!containing_view)
    return nullptr;

  std::unique_ptr<protocol::DictionaryValue> container_query_container_info =
      protocol::DictionaryValue::create();

  PathBuilder container_builder;
  auto content_box = layout_box->PhysicalContentBoxRect();
  gfx::QuadF content_quad = layout_box->LocalRectToAbsoluteQuad(content_box);
  FrameQuadToViewport(containing_view, content_quad);
  container_builder.AppendPath(QuadToPath(content_quad), scale);
  container_query_container_info->setValue("containerBorder",
                                           container_builder.Release());

  auto* element = DynamicTo<Element>(node);
  bool include_descendants =
      container_query_container_highlight_config.descendant_border &&
      !container_query_container_highlight_config.descendant_border
           ->IsFullyTransparent();
  if (element && include_descendants) {
    std::unique_ptr<protocol::ListValue> descendants_info =
        protocol::ListValue::create();
    for (auto& descendant_quad : GetContainerQueryingDescendantQuads(element)) {
      std::unique_ptr<protocol::DictionaryValue> descendant_info =
          protocol::DictionaryValue::create();
      descendant_info->setValue(
          "descendantBorder",
          BuildPathFromQuad(containing_view, descendant_quad));
      descendants_info->pushValue(std::move(descendant_info));
    }
    container_query_container_info->setArray("queryingDescendants",
                                             std::move(descendants_info));
  }

  container_query_container_info->setValue(
      "containerQueryContainerHighlightConfig",
      BuildContainerQueryContainerHighlightConfigInfo(
          container_query_container_highlight_config));

  return container_query_container_info;
}

std::unique_ptr<protocol::DictionaryValue> BuildIsolatedElementInfo(
    Element& element,
    const InspectorIsolationModeHighlightConfig& config,
    float scale) {
  LayoutBox* layout_box = element.GetLayoutBox();
  if (!layout_box)
    return nullptr;

  LocalFrameView* containing_view = element.GetDocument().View();
  if (!containing_view)
    return nullptr;

  auto isolated_element_info = protocol::DictionaryValue::create();

  auto element_box = layout_box->PhysicalContentBoxRect();
  gfx::QuadF element_box_quad =
      layout_box->LocalRectToAbsoluteQuad(element_box);
  FrameQuadToViewport(containing_view, element_box_quad);
  isolated_element_info->setDouble("currentX", element_box_quad.p1().x());
  isolated_element_info->setDouble("currentY", element_box_quad.p1().y());

  // Isolation mode's resizer size should be consistent with
  // Device Mode's resizer size, which is 20px.
  const LayoutUnit resizer_size(20 / scale);
  PhysicalRect width_resizer_box(
      layout_box->ContentLeft() + layout_box->ContentWidth(),
      layout_box->ContentTop(), resizer_size, layout_box->ContentHeight());
  isolated_element_info->setValue(
      "widthResizerBorder",
      BuildPathFromQuad(containing_view, layout_box->LocalRectToAbsoluteQuad(
                                             width_resizer_box)));
  PhysicalRect height_resizer_box(
      layout_box->ContentLeft(),
      layout_box->ContentTop() + layout_box->ContentHeight(),
      layout_box->ContentWidth(), resizer_size);
  isolated_element_info->setValue(
      "heightResizerBorder",
      BuildPathFromQuad(containing_view, layout_box->LocalRectToAbsoluteQuad(
                                             height_resizer_box)));

  PhysicalRect bidirection_resizer_box(
      layout_box->ContentLeft() + layout_box->ContentWidth(),
      layout_box->ContentTop() + layout_box->ContentHeight(), resizer_size,
      resizer_size);
  isolated_element_info->setValue(
      "bidirectionResizerBorder",
      BuildPathFromQuad(containing_view, layout_box->LocalRectToAbsoluteQuad(
                                             bidirection_resizer_box)));

  CSSComputedStyleDeclaration* style =
      MakeGarbageCollected<CSSComputedStyleDeclaration>(&element, true);
  const CSSValue* width = style->GetPropertyCSSValue(CSSPropertyID::kWidth);
  if (width && width->IsNumericLiteralValue()) {
    isolated_element_info->setDouble(
        "currentWidth", To<CSSNumericLiteralValue>(width)->DoubleValue());
  }
  const CSSValue* height = style->GetPropertyCSSValue(CSSPropertyID::kHeight);
  if (height && height->IsNumericLiteralValue()) {
    isolated_element_info->setDouble(
        "currentHeight", To<CSSNumericLiteralValue>(height)->DoubleValue());
  }

  isolated_element_info->setValue(
      "isolationModeHighlightConfig",
      BuildIsolationModeHighlightConfigInfo(config));

  return isolated_element_info;
}

std::unique_ptr<protocol::DictionaryValue> InspectorContainerQueryHighlight(
    Node* node,
    const InspectorContainerQueryContainerHighlightConfig& config) {
  LocalFrameView* frame_view = node->GetDocument().View();
  if (!frame_view)
    return nullptr;

  std::unique_ptr<protocol::DictionaryValue> container_query_container_info =
      BuildContainerQueryContainerInfo(node, config,
                                       DeviceScaleFromFrameView(frame_view));

  if (!container_query_container_info)
    return nullptr;

  return container_query_container_info;
}

std::unique_ptr<protocol::DictionaryValue> InspectorIsolatedElementHighlight(
    Element* element,
    const InspectorIsolationModeHighlightConfig& config) {
  LocalFrameView* frame_view = element->GetDocument().View();
  if (!frame_view)
    return nullptr;

  std::unique_ptr<protocol::DictionaryValue> isolated_element_info =
      BuildIsolatedElementInfo(*element, config,
                               DeviceScaleFromFrameView(frame_view));

  if (!isolated_element_info)
    return nullptr;

  isolated_element_info->setInteger("highlightIndex", config.highlight_index);
  return isolated_element_info;
}

// static
InspectorHighlightConfig InspectorHighlight::DefaultConfig() {
  InspectorHighlightConfig config;
  config.content = Color(255, 0, 0, 0);
  config.content_outline = Color(128, 0, 0, 0);
  config.padding = Color(0, 255, 0, 0);
  config.border = Color(0, 0, 255, 0);
  config.margin = Color(255, 255, 255, 0);
  config.event_target = Color(128, 128, 128, 0);
  config.shape = Color(0, 0, 0, 0);
  config.shape_margin = Color(128, 128, 128, 0);
  config.show_info = true;
  config.show_styles = false;
  config.show_rulers = true;
  config.show_extension_lines = true;
  config.css_grid = Color::kTransparent;
  config.color_format = ColorFormat::kHex;
  config.grid_highlight_config = std::make_unique<InspectorGridHighlightConfig>(
      InspectorHighlight::DefaultGridConfig());
  config.flex_container_highlight_config =
      std::make_unique<InspectorFlexContainerHighlightConfig>(
          InspectorHighlight::DefaultFlexContainerConfig());
  config.flex_item_highlight_config =
      std::make_unique<InspectorFlexItemHighlightConfig>(
          InspectorHighlight::DefaultFlexItemConfig());
  return config;
}

// static
InspectorGridHighlightConfig InspectorHighlight::DefaultGridConfig() {
  InspectorGridHighlightConfig config;
  config.grid_color = Color(255, 0, 0, 0);
  config.row_line_color = Color(128, 0, 0, 0);
  config.column_line_color = Color(128, 0, 0, 0);
  config.row_gap_color = Color(0, 255, 0, 0);
  config.column_gap_color = Color(0, 0, 255, 0);
  config.row_hatch_color = Color(255, 255, 255, 0);
  config.column_hatch_color = Color(128, 128, 128, 0);
  config.area_border_color = Color(255, 0, 0, 0);
  config.grid_background_color = Color(255, 0, 0, 0);
  config.show_grid_extension_lines = true;
  config.show_positive_line_numbers = true;
  config.show_negative_line_numbers = true;
  config.show_area_names = true;
  config.show_line_names = true;
  config.grid_border_dash = false;
  config.row_line_dash = true;
  config.column_line_dash = true;
  config.show_track_sizes = true;
  return config;
}

// static
InspectorFlexContainerHighlightConfig
InspectorHighlight::DefaultFlexContainerConfig() {
  InspectorFlexContainerHighlightConfig config;
  config.container_border =
      std::optional<LineStyle>(InspectorHighlight::DefaultLineStyle());
  config.line_separator =
      std::optional<LineStyle>(InspectorHighlight::DefaultLineStyle());
  config.item_separator =
      std::optional<LineStyle>(InspectorHighlight::DefaultLineStyle());
  config.main_distributed_space =
      std::optional<BoxStyle>(InspectorHighlight::DefaultBoxStyle());
  config.cross_distributed_space =
      std::optional<BoxStyle>(InspectorHighlight::DefaultBoxStyle());
  config.row_gap_space =
      std::optional<BoxStyle>(InspectorHighlight::DefaultBoxStyle());
  config.column_gap_space =
      std::optional<BoxStyle>(InspectorHighlight::DefaultBoxStyle());
  config.cross_alignment =
      std::optional<LineStyle>(InspectorHighlight::DefaultLineStyle());
  return config;
}

// static
InspectorFlexItemHighlightConfig InspectorHighlight::DefaultFlexItemConfig() {
  InspectorFlexItemHighlightConfig config;
  config.base_size_box =
      std::optional<BoxStyle>(InspectorHighlight::DefaultBoxStyle());
  config.base_size_border =
      std::optional<LineStyle>(InspectorHighlight::DefaultLineStyle());
  config.flexibility_arrow =
      std::optional<LineStyle>(InspectorHighlight::DefaultLineStyle());
  return config;
}

// static
LineStyle InspectorHighlight::DefaultLineStyle() {
  LineStyle style;
  style.color = Color(255, 0, 0, 0);
  style.pattern = "solid";
  return style;
}

// static
BoxStyle InspectorHighlight::DefaultBoxStyle() {
  BoxStyle style;
  style.fill_color = Color(255, 0, 0, 0);
  style.hatch_color = Color(255, 0, 0, 0);
  return style;
}

}  // namespace blink
