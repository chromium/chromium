// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_highlight.h"

#include "base/macros.h"
#include "third_party/blink/renderer/core/css/css_color_value.h"
#include "third_party/blink/renderer/core/css/css_computed_style_declaration.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/inspector/inspector_dom_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_dom_snapshot_agent.h"
#include "third_party/blink/renderer/core/layout/adjust_for_absolute_zoom.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_grid.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/shapes/shape_outside_info.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/graphics/path.h"
#include "third_party/blink/renderer/platform/web_test_support.h"

namespace blink {

namespace {

class PathBuilder {
  STACK_ALLOCATED();

 public:
  PathBuilder() : path_(protocol::ListValue::create()) {}
  virtual ~PathBuilder() = default;

  std::unique_ptr<protocol::ListValue> Release() { return std::move(path_); }

  void AppendPath(const Path& path, float scale) {
    Path transform_path(path);
    transform_path.Transform(AffineTransform().Scale(scale));
    transform_path.Apply(this, &PathBuilder::AppendPathElement);
  }

 protected:
  virtual FloatPoint TranslatePoint(const FloatPoint& point) { return point; }

 private:
  static void AppendPathElement(void* path_builder,
                                const PathElement* path_element) {
    static_cast<PathBuilder*>(path_builder)->AppendPathElement(path_element);
  }

  void AppendPathElement(const PathElement*);
  void AppendPathCommandAndPoints(const char* command,
                                  const FloatPoint points[],
                                  size_t length);

  std::unique_ptr<protocol::ListValue> path_;
  DISALLOW_COPY_AND_ASSIGN(PathBuilder);
};

void PathBuilder::AppendPathCommandAndPoints(const char* command,
                                             const FloatPoint points[],
                                             size_t length) {
  path_->pushValue(protocol::StringValue::create(command));
  for (size_t i = 0; i < length; i++) {
    FloatPoint point = TranslatePoint(points[i]);
    path_->pushValue(protocol::FundamentalValue::create(point.X()));
    path_->pushValue(protocol::FundamentalValue::create(point.Y()));
  }
}

void PathBuilder::AppendPathElement(const PathElement* path_element) {
  switch (path_element->type) {
    // The points member will contain 1 value.
    case kPathElementMoveToPoint:
      AppendPathCommandAndPoints("M", path_element->points, 1);
      break;
    // The points member will contain 1 value.
    case kPathElementAddLineToPoint:
      AppendPathCommandAndPoints("L", path_element->points, 1);
      break;
    // The points member will contain 3 values.
    case kPathElementAddCurveToPoint:
      AppendPathCommandAndPoints("C", path_element->points, 3);
      break;
    // The points member will contain 2 values.
    case kPathElementAddQuadCurveToPoint:
      AppendPathCommandAndPoints("Q", path_element->points, 2);
      break;
    // The points member will contain no values.
    case kPathElementCloseSubpath:
      AppendPathCommandAndPoints("Z", nullptr, 0);
      break;
  }
}

class ShapePathBuilder : public PathBuilder {
 public:
  ShapePathBuilder(LocalFrameView& view,
                   LayoutObject& layout_object,
                   const ShapeOutsideInfo& shape_outside_info)
      : view_(&view),
        layout_object_(layout_object),
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
  FloatPoint TranslatePoint(const FloatPoint& point) override {
    PhysicalOffset layout_object_point = PhysicalOffset::FromFloatPointRound(
        shape_outside_info_.ShapeToLayoutObjectPoint(point));
    // TODO(pfeldman): Is this kIgnoreTransforms correct?
    return FloatPoint(view_->FrameToViewport(
        RoundedIntPoint(layout_object_.LocalToAbsolutePoint(
            layout_object_point, kIgnoreTransforms))));
  }

 private:
  Member<LocalFrameView> view_;
  LayoutObject& layout_object_;
  const ShapeOutsideInfo& shape_outside_info_;
};

std::unique_ptr<protocol::Array<double>> BuildArrayForQuad(
    const FloatQuad& quad) {
  return std::make_unique<std::vector<double>, std::initializer_list<double>>(
      {quad.P1().X(), quad.P1().Y(), quad.P2().X(), quad.P2().Y(),
       quad.P3().X(), quad.P3().Y(), quad.P4().X(), quad.P4().Y()});
}

Path QuadToPath(const FloatQuad& quad) {
  Path quad_path;
  quad_path.MoveTo(quad.P1());
  quad_path.AddLineTo(quad.P2());
  quad_path.AddLineTo(quad.P3());
  quad_path.AddLineTo(quad.P4());
  quad_path.CloseSubpath();
  return quad_path;
}

FloatPoint FramePointToViewport(const LocalFrameView* view,
                                FloatPoint point_in_frame) {
  FloatPoint point_in_root_frame = view->ConvertToRootFrame(point_in_frame);
  return view->GetPage()->GetVisualViewport().RootFrameToViewport(
      point_in_root_frame);
}

void FrameQuadToViewport(const LocalFrameView* view, FloatQuad& quad) {
  quad.SetP1(FramePointToViewport(view, quad.P1()));
  quad.SetP2(FramePointToViewport(view, quad.P2()));
  quad.SetP3(FramePointToViewport(view, quad.P3()));
  quad.SetP4(FramePointToViewport(view, quad.P4()));
}

const ShapeOutsideInfo* ShapeOutsideInfoForNode(Node* node,
                                                Shape::DisplayPaths* paths,
                                                FloatQuad* bounds) {
  LayoutObject* layout_object = node->GetLayoutObject();
  if (!layout_object || !layout_object->IsBox() ||
      !ToLayoutBox(layout_object)->GetShapeOutsideInfo())
    return nullptr;

  LocalFrameView* containing_view = node->GetDocument().View();
  LayoutBox* layout_box = ToLayoutBox(layout_object);
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
                        color.Blue(), color.Alpha());
}

void AppendStyleInfo(Node* node,
                     protocol::DictionaryValue* element_info,
                     const InspectorHighlightContrastInfo& node_contrast) {
  std::unique_ptr<protocol::DictionaryValue> computed_style =
      protocol::DictionaryValue::create();
  CSSComputedStyleDeclaration* style =
      MakeGarbageCollected<CSSComputedStyleDeclaration>(node, true);
  Vector<CSSPropertyID> properties;

  // For text nodes, we can show color & font properties.
  bool has_text_children = false;
  for (Node* child = node->firstChild(); !has_text_children && child;
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

  for (size_t i = 0; i < properties.size(); ++i) {
    const CSSValue* value = style->GetPropertyCSSValue(properties[i]);
    if (!value)
      continue;
    AtomicString name = CSSPropertyName(properties[i]).ToAtomicString();
    if (value->IsColorValue()) {
      Color color = static_cast<const cssvalue::CSSColorValue*>(value)->Value();
      computed_style->setString(name, ToHEXA(color));
    } else {
      computed_style->setString(name, value->CssText());
    }
  }
  element_info->setValue("style", std::move(computed_style));

  if (!node_contrast.font_size.IsEmpty()) {
    std::unique_ptr<protocol::DictionaryValue> contrast =
        protocol::DictionaryValue::create();
    contrast->setString("fontSize", node_contrast.font_size);
    contrast->setString("fontWeight", node_contrast.font_weight);
    contrast->setString("backgroundColor",
                        ToHEXA(node_contrast.background_color));
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
    if (pseudo_element->GetPseudoId() == kPseudoIdBefore)
      class_names.Append("::before");
    else if (pseudo_element->GetPseudoId() == kPseudoIdAfter)
      class_names.Append("::after");
  }
  if (!class_names.IsEmpty())
    element_info->setString("className", class_names.ToString());

  LayoutObject* layout_object = element->GetLayoutObject();
  LocalFrameView* containing_view = element->GetDocument().View();
  if (!layout_object || !containing_view)
    return element_info;

  // if (auto* context = element->GetDisplayLockContext()) {
  //  if (context->IsLocked()) {
  //    // If it's a locked element, use the values from the locked frame rect.
  //    // TODO(vmpstr): Verify that these values are correct here.
  //    element_info->setString(
  //        "nodeWidth",
  //        String::Number(context->GetLockedContentLogicalWidth().ToDouble()));
  //    element_info->setString(
  //        "nodeHeight",
  //        String::Number(context->GetLockedContentLogicalHeight().ToDouble()));
  //  }
  //  return element_info;
  //}

  // layoutObject the getBoundingClientRect() data in the tooltip
  // to be consistent with the rulers (see http://crbug.com/262338).
  DOMRect* bounding_box = element->getBoundingClientRect();
  element_info->setString("nodeWidth", String::Number(bounding_box->width()));
  element_info->setString("nodeHeight", String::Number(bounding_box->height()));
  return element_info;
}

std::unique_ptr<protocol::DictionaryValue> BuildTextNodeInfo(Text* text_node) {
  std::unique_ptr<protocol::DictionaryValue> text_info =
      protocol::DictionaryValue::create();
  LayoutObject* layout_object = text_node->GetLayoutObject();
  if (!layout_object || !layout_object->IsText())
    return text_info;
  PhysicalRect bounding_box =
      ToLayoutText(layout_object)->PhysicalVisualOverflowRect();
  text_info->setString("nodeWidth", bounding_box.Width().ToString());
  text_info->setString("nodeHeight", bounding_box.Height().ToString());
  text_info->setString("tagName", "#text");
  return text_info;
}

std::unique_ptr<protocol::DictionaryValue> BuildGridInfo(
    LocalFrameView* containing_view,
    LayoutGrid* layout_grid,
    Color color,
    float scale,
    bool isPrimary) {
  std::unique_ptr<protocol::DictionaryValue> grid_info =
      protocol::DictionaryValue::create();

  const auto& rows = layout_grid->RowPositions();
  const auto& columns = layout_grid->ColumnPositions();

  PathBuilder cell_builder;
  auto row_gap =
      layout_grid->GridGap(kForRows) + layout_grid->GridItemOffset(kForRows);
  auto column_gap = layout_grid->GridGap(kForColumns) +
                    layout_grid->GridItemOffset(kForColumns);

  for (size_t i = 1; i < rows.size(); ++i) {
    for (size_t j = 1; j < columns.size(); ++j) {
      PhysicalOffset position(columns.at(j - 1), rows.at(i - 1));
      PhysicalSize size(columns.at(j) - columns.at(j - 1),
                        rows.at(i) - rows.at(i - 1));
      if (i != rows.size() - 1)
        size.height -= row_gap;
      if (j != columns.size() - 1)
        size.width -= column_gap;
      PhysicalRect cell(position, size);
      FloatQuad cell_quad = layout_grid->LocalRectToAbsoluteQuad(cell);
      FrameQuadToViewport(containing_view, cell_quad);
      cell_builder.AppendPath(QuadToPath(cell_quad), scale);
    }
  }
  grid_info->setValue("cells", cell_builder.Release());
  grid_info->setString("color", color.Serialized());
  grid_info->setBoolean("isPrimaryGrid", isPrimary);
  return grid_info;
}

void CollectQuadsRecursive(Node* node, Vector<FloatQuad>& out_quads) {
  LayoutObject* layout_object = node->GetLayoutObject();
  if (!layout_object)
    return;

  // For inline elements, absoluteQuads will return a line box based on the
  // line-height and font metrics, which is technically incorrect as replaced
  // elements like images should use their intristic height and expand the
  // linebox  as needed. To get an appropriate quads we descend
  // into the children and have them add their boxes.
  if (layout_object->IsLayoutInline() &&
      LayoutTreeBuilderTraversal::FirstChild(*node)) {
    for (Node* child = LayoutTreeBuilderTraversal::FirstChild(*node); child;
         child = LayoutTreeBuilderTraversal::NextSibling(*child))
      CollectQuadsRecursive(child, out_quads);
  } else {
    layout_object->AbsoluteQuads(out_quads);
  }
}

void CollectQuads(Node* node, Vector<FloatQuad>& out_quads) {
  CollectQuadsRecursive(node, out_quads);
  LocalFrameView* containing_view =
      node->GetLayoutObject() ? node->GetLayoutObject()->GetFrameView()
                              : nullptr;
  if (containing_view) {
    for (FloatQuad& quad : out_quads)
      FrameQuadToViewport(containing_view, quad);
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
  PhysicalRect rect_in_absolute = PhysicalRect::EnclosingRect(
      layout_object->AbsoluteBoundingBoxFloatRect());
  return local_frame_view
             ? local_frame_view->ConvertToRootFrame(rect_in_absolute)
             : rect_in_absolute;
}

PhysicalRect TextFragmentRectInRootFrame(
    const LayoutObject* layout_object,
    const LayoutText::TextBoxInfo& text_box) {
  PhysicalRect absolute_coords_text_box_rect =
      layout_object->LocalToAbsoluteRect(
          layout_object->FlipForWritingMode(text_box.local_rect));
  LocalFrameView* local_frame_view = layout_object->GetFrameView();
  return local_frame_view ? local_frame_view->ConvertToRootFrame(
                                absolute_coords_text_box_rect)
                          : absolute_coords_text_box_rect;
}

}  // namespace

InspectorHighlight::InspectorHighlight(float scale)
    : highlight_paths_(protocol::ListValue::create()),
      show_rulers_(false),
      show_extension_lines_(false),
      scale_(scale) {}

InspectorHighlightConfig::InspectorHighlightConfig()
    : show_info(false),
      show_styles(false),
      show_rulers(false),
      show_extension_lines(false) {}

InspectorHighlight::InspectorHighlight(
    Node* node,
    const InspectorHighlightConfig& highlight_config,
    const InspectorHighlightContrastInfo& node_contrast,
    bool append_element_info,
    bool append_distance_info,
    bool is_locked_ancestor)
    : highlight_paths_(protocol::ListValue::create()),
      show_rulers_(highlight_config.show_rulers),
      show_extension_lines_(highlight_config.show_extension_lines),
      scale_(1.f) {
  DCHECK(!DisplayLockUtilities::NearestLockedExclusiveAncestor(*node));
  LocalFrameView* frame_view = node->GetDocument().View();
  if (frame_view) {
    scale_ = 1.f / frame_view->GetChromeClient()->WindowToViewportScalar(
                       &frame_view->GetFrame(), 1.f);
  }
  AppendPathsForShapeOutside(node, highlight_config);
  AppendNodeHighlight(node, highlight_config);
  auto* text_node = DynamicTo<Text>(node);
  auto* element = DynamicTo<Element>(node);
  if (append_element_info && element)
    element_info_ = BuildElementInfo(element);
  else if (append_element_info && text_node)
    element_info_ = BuildTextNodeInfo(text_node);
  if (element_info_ && highlight_config.show_styles)
    AppendStyleInfo(node, element_info_.get(), node_contrast);

  if (element_info_ && is_locked_ancestor)
    element_info_->setString("isLockedAncestor", "true");
  if (append_distance_info)
    AppendDistanceInfo(node);
}

InspectorHighlight::~InspectorHighlight() = default;

void InspectorHighlight::AppendDistanceInfo(Node* node) {
  if (!InspectorHighlight::GetBoxModel(node, &model_, false))
    return;
  boxes_ = std::make_unique<protocol::Array<protocol::Array<double>>>();
  computed_style_ = protocol::DictionaryValue::create();

  node->GetDocument().EnsurePaintLocationDataValidForNode(node);
  LayoutObject* layout_object = node->GetLayoutObject();
  if (!layout_object)
    return;

  CSSComputedStyleDeclaration* style =
      MakeGarbageCollected<CSSComputedStyleDeclaration>(node, true);
  for (size_t i = 0; i < style->length(); ++i) {
    AtomicString name(style->item(i));
    const CSSValue* value = style->GetPropertyCSSValue(cssPropertyID(name));
    if (!value)
      continue;
    if (value->IsColorValue()) {
      Color color = static_cast<const cssvalue::CSSColorValue*>(value)->Value();
      String hex_color =
          String::Format("#%02X%02X%02X%02X", color.Red(), color.Green(),
                         color.Blue(), color.Alpha());
      computed_style_->setString(name, hex_color);
    } else {
      computed_style_->setString(name, value->CssText());
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
           {kPseudoIdFirstLetter, kPseudoIdBefore, kPseudoIdAfter}) {
        if (Node* pseudo_node = element->GetPseudoElement(pseudo_id))
          VisitAndCollectDistanceInfo(pseudo_node);
      }
    }
  }

  if (!node->IsContainerNode())
    return;
  Node* first_child = InspectorDOMSnapshotAgent::FirstChild(*node, false);
  for (Node* child = first_child; child;
       child = InspectorDOMSnapshotAgent::NextSibling(*child, false))
    VisitAndCollectDistanceInfo(child);
}

void InspectorHighlight::VisitAndCollectDistanceInfo(
    PseudoId pseudo_id,
    LayoutObject* layout_object) {
  protocol::DOM::PseudoType pseudo_type;
  if (!InspectorDOMAgent::GetPseudoElementType(pseudo_id, &pseudo_type))
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
    LayoutText* layout_text = ToLayoutText(layout_object);
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

void InspectorHighlight::AppendQuad(const FloatQuad& quad,
                                    const Color& fill_color,
                                    const Color& outline_color,
                                    const String& name) {
  Path path = QuadToPath(quad);
  PathBuilder builder;
  builder.AppendPath(path, scale_);
  AppendPath(builder.Release(), fill_color, outline_color, name);
}

void InspectorHighlight::AppendPath(std::unique_ptr<protocol::ListValue> path,
                                    const Color& fill_color,
                                    const Color& outline_color,
                                    const String& name) {
  std::unique_ptr<protocol::DictionaryValue> object =
      protocol::DictionaryValue::create();
  object->setValue("path", std::move(path));
  object->setString("fillColor", fill_color.Serialized());
  if (outline_color != Color::kTransparent)
    object->setString("outlineColor", outline_color.Serialized());
  if (!name.IsEmpty())
    object->setString("name", name);
  highlight_paths_->pushValue(std::move(object));
}

void InspectorHighlight::AppendEventTargetQuads(
    Node* event_target_node,
    const InspectorHighlightConfig& highlight_config) {
  if (event_target_node->GetLayoutObject()) {
    FloatQuad border, unused;
    if (BuildNodeQuads(event_target_node, &unused, &unused, &border, &unused))
      AppendQuad(border, highlight_config.event_target);
  }
}

void InspectorHighlight::AppendPathsForShapeOutside(
    Node* node,
    const InspectorHighlightConfig& config) {
  Shape::DisplayPaths paths;
  FloatQuad bounds_quad;

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

  Vector<FloatQuad> svg_quads;
  if (BuildSVGQuads(node, svg_quads)) {
    for (wtf_size_t i = 0; i < svg_quads.size(); ++i) {
      AppendQuad(svg_quads[i], highlight_config.content,
                 highlight_config.content_outline);
    }
    return;
  }

  FloatQuad content, padding, border, margin;
  if (!BuildNodeQuads(node, &content, &padding, &border, &margin))
    return;
  AppendQuad(content, highlight_config.content,
             highlight_config.content_outline, "content");
  AppendQuad(padding, highlight_config.padding, Color::kTransparent, "padding");
  AppendQuad(border, highlight_config.border, Color::kTransparent, "border");
  AppendQuad(margin, highlight_config.margin, Color::kTransparent, "margin");

  if (highlight_config.css_grid == Color::kTransparent)
    return;
  grid_info_ = protocol::ListValue::create();
  if (layout_object->IsLayoutGrid()) {
    grid_info_->pushValue(
        BuildGridInfo(node->GetDocument().View(), ToLayoutGrid(layout_object),
                      highlight_config.css_grid, scale_, true));
  }
  LayoutObject* parent = layout_object->Parent();
  if (!parent || !parent->IsLayoutGrid())
    return;
  if (!BuildNodeQuads(parent->GetNode(), &content, &padding, &border, &margin))
    return;
  grid_info_->pushValue(
      BuildGridInfo(node->GetDocument().View(), ToLayoutGrid(parent),
                    highlight_config.css_grid, scale_, false));
}

std::unique_ptr<protocol::DictionaryValue> InspectorHighlight::AsProtocolValue()
    const {
  std::unique_ptr<protocol::DictionaryValue> object =
      protocol::DictionaryValue::create();
  object->setValue("paths", highlight_paths_->clone());
  object->setBoolean("showRulers", show_rulers_);
  object->setBoolean("showExtensionLines", show_extension_lines_);
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
  return object;
}

// static
bool InspectorHighlight::GetBoxModel(
    Node* node,
    std::unique_ptr<protocol::DOM::BoxModel>* model,
    bool use_absolute_zoom) {
  node->GetDocument().EnsurePaintLocationDataValidForNode(node);
  LayoutObject* layout_object = node->GetLayoutObject();
  LocalFrameView* view = node->GetDocument().View();
  if (!layout_object || !view)
    return false;

  FloatQuad content, padding, border, margin;
  Vector<FloatQuad> svg_quads;
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
    AdjustForAbsoluteZoom::AdjustFloatQuad(content, *layout_object);
    AdjustForAbsoluteZoom::AdjustFloatQuad(padding, *layout_object);
    AdjustForAbsoluteZoom::AdjustFloatQuad(border, *layout_object);
    AdjustForAbsoluteZoom::AdjustFloatQuad(margin, *layout_object);
  }

  float scale = 1 / view->GetPage()->GetVisualViewport().Scale();
  content.Scale(scale, scale);
  padding.Scale(scale, scale);
  border.Scale(scale, scale);
  margin.Scale(scale, scale);

  IntRect bounding_box =
      view->ConvertToRootFrame(layout_object->AbsoluteBoundingBoxRect());
  LayoutBoxModelObject* model_object =
      layout_object->IsBoxModelObject() ? ToLayoutBoxModelObject(layout_object)
                                        : nullptr;

  *model =
      protocol::DOM::BoxModel::create()
          .setContent(BuildArrayForQuad(content))
          .setPadding(BuildArrayForQuad(padding))
          .setBorder(BuildArrayForQuad(border))
          .setMargin(BuildArrayForQuad(margin))
          .setWidth(model_object ? AdjustForAbsoluteZoom::AdjustInt(
                                       model_object->PixelSnappedOffsetWidth(
                                           model_object->OffsetParent()),
                                       model_object)
                                 : bounding_box.Width())
          .setHeight(model_object ? AdjustForAbsoluteZoom::AdjustInt(
                                        model_object->PixelSnappedOffsetHeight(
                                            model_object->OffsetParent()),
                                        model_object)
                                  : bounding_box.Height())
          .build();

  Shape::DisplayPaths paths;
  FloatQuad bounds_quad;
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
bool InspectorHighlight::BuildSVGQuads(Node* node, Vector<FloatQuad>& quads) {
  LayoutObject* layout_object = node->GetLayoutObject();
  if (!layout_object)
    return false;
  if (!layout_object->GetNode() || !layout_object->GetNode()->IsSVGElement() ||
      layout_object->IsSVGRoot())
    return false;
  CollectQuads(node, quads);
  return true;
}

// static
bool InspectorHighlight::GetContentQuads(
    Node* node,
    std::unique_ptr<protocol::Array<protocol::Array<double>>>* result) {
  LayoutObject* layout_object = node->GetLayoutObject();
  LocalFrameView* view = node->GetDocument().View();
  if (!layout_object || !view)
    return false;
  Vector<FloatQuad> quads;
  CollectQuads(node, quads);
  float scale = 1 / view->GetPage()->GetVisualViewport().Scale();
  for (FloatQuad& quad : quads) {
    AdjustForAbsoluteZoom::AdjustFloatQuad(quad, *layout_object);
    quad.Scale(scale, scale);
  }

  result->reset(new protocol::Array<protocol::Array<double>>());
  for (FloatQuad& quad : quads)
    (*result)->emplace_back(BuildArrayForQuad(quad));
  return true;
}

bool InspectorHighlight::BuildNodeQuads(Node* node,
                                        FloatQuad* content,
                                        FloatQuad* padding,
                                        FloatQuad* border,
                                        FloatQuad* margin) {
  LayoutObject* layout_object = node->GetLayoutObject();
  if (!layout_object)
    return false;

  LocalFrameView* containing_view = layout_object->GetFrameView();
  if (!containing_view)
    return false;
  if (!layout_object->IsBox() && !layout_object->IsLayoutInline() &&
      !layout_object->IsText())
    return false;

  PhysicalRect content_box;
  PhysicalRect padding_box;
  PhysicalRect border_box;
  PhysicalRect margin_box;

  if (layout_object->IsText()) {
    LayoutText* layout_text = ToLayoutText(layout_object);
    PhysicalRect text_rect = layout_text->PhysicalVisualOverflowRect();
    content_box = text_rect;
    padding_box = text_rect;
    border_box = text_rect;
    margin_box = text_rect;
  } else if (layout_object->IsBox()) {
    LayoutBox* layout_box = ToLayoutBox(layout_object);

    // LayoutBox returns the "pure" content area box, exclusive of the
    // scrollbars (if present), which also count towards the content area in
    // CSS.
    const int vertical_scrollbar_width = layout_box->VerticalScrollbarWidth();
    const int horizontal_scrollbar_height =
        layout_box->HorizontalScrollbarHeight();
    content_box = layout_box->PhysicalContentBoxRect();
    content_box.SetWidth(content_box.Width() + vertical_scrollbar_width);
    content_box.SetHeight(content_box.Height() + horizontal_scrollbar_height);

    padding_box = layout_box->PhysicalPaddingBoxRect();
    padding_box.SetWidth(padding_box.Width() + vertical_scrollbar_width);
    padding_box.SetHeight(padding_box.Height() + horizontal_scrollbar_height);

    border_box = layout_box->PhysicalBorderBoxRect();

    margin_box = PhysicalRect(border_box.X() - layout_box->MarginLeft(),
                              border_box.Y() - layout_box->MarginTop(),
                              border_box.Width() + layout_box->MarginWidth(),
                              border_box.Height() + layout_box->MarginHeight());
  } else {
    LayoutInline* layout_inline = ToLayoutInline(layout_object);

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
  config.css_grid = Color(128, 128, 128, 0);
  return config;
}

}  // namespace blink
