// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_contrast.h"

#include "base/trace_event/trace_event.h"
#include "third_party/blink/renderer/core/css/css_color.h"
#include "third_party/blink/renderer/core/css/css_computed_style_declaration.h"
#include "third_party/blink/renderer/core/css/css_gradient_value.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/html_embed_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/html_object_element.h"
#include "third_party/blink/renderer/core/html/html_picture_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/inspector/inspector_dom_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_dom_snapshot_agent.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/style/style_generated_image.h"
#include "ui/gfx/color_utils.h"

namespace blink {

namespace {

bool NodeIsElementWithLayoutObject(Node* node) {
  if (auto* element = DynamicTo<Element>(node)) {
    if (element->GetLayoutObject())
      return true;
  }
  return false;
}

// Blends the colors from the given gradient with the existing colors.
void BlendWithColorsFromGradient(cssvalue::CSSGradientValue* gradient,
                                 Vector<Color>& colors,
                                 bool& found_non_transparent_color,
                                 bool& found_opaque_color,
                                 const LayoutObject& layout_object) {
  const Document& document = layout_object.GetDocument();
  const ComputedStyle& style = layout_object.StyleRef();

  Vector<Color> stop_colors = gradient->GetStopColors(document, style);
  if (colors.empty()) {
    colors.AppendRange(stop_colors.begin(), stop_colors.end());
  } else {
    if (colors.size() > 1) {
      // Gradient on gradient is too complicated, bail out.
      colors.clear();
      return;
    }

    Color existing_color = colors.front();
    colors.clear();
    for (auto stop_color : stop_colors) {
      found_non_transparent_color =
          found_non_transparent_color || !stop_color.IsFullyTransparent();
      colors.push_back(existing_color.Blend(stop_color));
    }
  }
  found_opaque_color =
      found_opaque_color || gradient->KnownToBeOpaque(document, style);
}

// Gets the colors from an image style, if one exists and it is a gradient.
void AddColorsFromImageStyle(const ComputedStyle& style,
                             const LayoutObject& layout_object,
                             Vector<Color>& colors,
                             bool& found_opaque_color,
                             bool& found_non_transparent_color) {
  const FillLayer& background_layers = style.BackgroundLayers();
  if (!background_layers.AnyLayerHasImage())
    return;

  StyleImage* style_image = background_layers.GetImage();
  // hasImage() does not always indicate that this is non-null
  if (!style_image)
    return;

  if (!style_image->IsGeneratedImage()) {
    // Make no assertions about the colors in non-generated images
    colors.clear();
    found_opaque_color = false;
    return;
  }

  StyleGeneratedImage* gen_image = To<StyleGeneratedImage>(style_image);
  CSSValue* image_css = gen_image->CssValue();
  if (auto* gradient = DynamicTo<cssvalue::CSSGradientValue>(image_css)) {
    BlendWithColorsFromGradient(gradient, colors, found_non_transparent_color,
                                found_opaque_color, layout_object);
  }
}

PhysicalRect GetNodeRect(Node* node) {
  PhysicalRect rect = node->BoundingBox();
  Document* document = &node->GetDocument();
  while (!document->IsInMainFrame()) {
    HTMLFrameOwnerElement* owner_element = document->LocalOwner();
    if (!owner_element)
      break;
    rect.offset.left += owner_element->BoundingBox().offset.left;
    rect.offset.top += owner_element->BoundingBox().offset.top;
    document = &owner_element->GetDocument();
  }
  return rect;
}

}  // namespace

InspectorContrast::InspectorContrast(Document* document) {
  if (!document->IsInMainFrame()) {
    // If document is in a frame, use the top level document to collect nodes
    // for all frames.
    for (HTMLFrameOwnerElement* owner_element = document->LocalOwner();
         owner_element;
         owner_element = owner_element->GetDocument().LocalOwner()) {
      document = &owner_element->GetDocument();
    }
  }

  document_ = document;
}

void InspectorContrast::CollectNodesAndBuildRTreeIfNeeded() {
  TRACE_EVENT0("devtools.contrast",
               "InspectorContrast::CollectNodesAndBuildRTreeIfNeeded");

  if (rtree_built_)
    return;

  LocalFrame* frame = document_->GetFrame();
  if (!frame)
    return;
  LayoutView* layout_view = frame->ContentLayoutObject();
  if (!layout_view)
    return;

  if (!layout_view->GetFrameView()->UpdateLifecycleToPrePaintClean(
          DocumentUpdateReason::kInspector)) {
    return;
  }

  InspectorDOMAgent::CollectNodes(
      document_, INT_MAX, true, InspectorDOMAgent::IncludeWhitespaceEnum::NONE,
      WTF::BindRepeating(&NodeIsElementWithLayoutObject), &elements_);
  SortElementsByPaintOrder(elements_, document_);
  rtree_.Build(
      elements_.size(),
      [this](size_t index) {
        return ToPixelSnappedRect(
            GetNodeRect(elements_[static_cast<wtf_size_t>(index)]));
      },
      [this](size_t index) {
        return elements_[static_cast<wtf_size_t>(index)];
      });

  rtree_built_ = true;
}

std::vector<ContrastInfo> InspectorContrast::GetElementsWithContrastIssues(
    bool report_aaa,
    size_t max_elements = 0) {
  TRACE_EVENT0("devtools.contrast",
               "InspectorContrast::GetElementsWithContrastIssues");
  CollectNodesAndBuildRTreeIfNeeded();
  std::vector<ContrastInfo> result;
  for (Node* node : elements_) {
    auto info = GetContrast(To<Element>(node));
    if (info.able_to_compute_contrast &&
        ((info.contrast_ratio < info.threshold_aa) ||
         (info.contrast_ratio < info.threshold_aaa && report_aaa))) {
      result.push_back(std::move(info));
      if (max_elements && result.size() >= max_elements)
        return result;
    }
  }
  return result;
}

static bool IsLargeFont(const TextInfo& text_info) {
  String font_size_css = text_info.font_size;
  String font_weight = text_info.font_weight;
  // font_size_css always has 'px' appended at the end;
  String font_size_str = font_size_css.Substring(0, font_size_css.length() - 2);
  double font_size_px = font_size_str.ToDouble();
  double font_size_pt = font_size_px * 72 / 96;
  bool is_bold = font_weight == "bold" || font_weight == "bolder" ||
                 font_weight == "600" || font_weight == "700" ||
                 font_weight == "800" || font_weight == "900";
  if (is_bold) {
    return font_size_pt >= 14;
  }
  return font_size_pt >= 18;
}

ContrastInfo InspectorContrast::GetContrast(Element* top_element) {
  TRACE_EVENT0("devtools.contrast", "InspectorContrast::GetContrast");

  ContrastInfo result;

  auto* text_node = DynamicTo<Text>(top_element->firstChild());
  if (!text_node || text_node->nextSibling())
    return result;

  const String& text = text_node->data().StripWhiteSpace();
  if (text.empty())
    return result;

  const LayoutObject* layout_object = top_element->GetLayoutObject();
  const CSSValue* text_color_value = ComputedStyleUtils::ComputedPropertyValue(
      CSSProperty::Get(CSSPropertyID::kColor), layout_object->StyleRef());
  if (!text_color_value->IsColorValue())
    return result;

  float text_opacity = 1.0f;
  Vector<Color> bgcolors = GetBackgroundColors(top_element, &text_opacity);
  // TODO(crbug/1174511): Compute contrast only if the element has a single
  // color background to be consistent with the current UI. In the future, we
  // should return a range of contrast values.
  if (bgcolors.size() != 1)
    return result;

  Color text_color =
      static_cast<const cssvalue::CSSColor*>(text_color_value)->Value();

  text_color.SetAlpha(text_opacity * text_color.Alpha());

  float contrast_ratio = color_utils::GetContrastRatio(
      bgcolors.at(0).Blend(text_color).toSkColor4f(),
      bgcolors.at(0).toSkColor4f());

  auto text_info = GetTextInfo(top_element);
  bool is_large_font = IsLargeFont(text_info);

  result.able_to_compute_contrast = true;
  result.contrast_ratio = contrast_ratio;
  result.threshold_aa = is_large_font ? 3.0 : 4.5;
  result.threshold_aaa = is_large_font ? 4.5 : 7.0;
  result.font_size = text_info.font_size;
  result.font_weight = text_info.font_weight;
  result.element = top_element;

  return result;
}

TextInfo InspectorContrast::GetTextInfo(Element* element) {
  TextInfo info;
  auto* computed_style_info =
      MakeGarbageCollected<CSSComputedStyleDeclaration>(element, true);
  const CSSValue* font_size =
      computed_style_info->GetPropertyCSSValue(CSSPropertyID::kFontSize);
  if (font_size)
    info.font_size = font_size->CssText();
  const CSSValue* font_weight =
      computed_style_info->GetPropertyCSSValue(CSSPropertyID::kFontWeight);
  if (font_weight)
    info.font_weight = font_weight->CssText();
  return info;
}

Vector<Color> InspectorContrast::GetBackgroundColors(Element* element,
                                                     float* text_opacity) {
  Vector<Color> colors;
  // TODO: only support the single text child node here.
  // Follow up with a larger fix post-merge.
  auto* text_node = DynamicTo<Text>(element->firstChild());
  if (!text_node || element->firstChild()->nextSibling()) {
    return colors;
  }

  PhysicalRect content_bounds = GetNodeRect(text_node);
  LocalFrameView* view = text_node->GetDocument().View();
  if (!view)
    return colors;

  // Start with the "default" page color (typically white).
  colors.push_back(view->BaseBackgroundColor());

  GetColorsFromRect(content_bounds, text_node->GetDocument(), element, colors,
                    text_opacity);

  return colors;
}

// Get the elements which overlap the given rectangle.
HeapVector<Member<Node>> InspectorContrast::ElementsFromRect(
    const PhysicalRect& rect,
    Document& document) {
  CollectNodesAndBuildRTreeIfNeeded();
  HeapVector<Member<Node>> overlapping_elements;
  rtree_.Search(ToPixelSnappedRect(rect),
                [&overlapping_elements](const Member<Node>& payload,
                                        const gfx::Rect& rect) {
                  overlapping_elements.push_back(payload);
                });
  return overlapping_elements;
}

bool InspectorContrast::GetColorsFromRect(PhysicalRect rect,
                                          Document& document,
                                          Element* top_element,
                                          Vector<Color>& colors,
                                          float* text_opacity) {
  HeapVector<Member<Node>> elements_under_rect =
      ElementsFromRect(rect, document);

  bool found_opaque_color = false;
  bool found_top_element = false;

  *text_opacity = 1.0f;

  for (const Member<Node>& node : elements_under_rect) {
    if (found_top_element) {
      break;
    }
    const Element* element = To<Element>(node.Get());
    if (element == top_element)
      found_top_element = true;

    const LayoutObject* layout_object = element->GetLayoutObject();

    if (IsA<HTMLCanvasElement>(element) || IsA<HTMLEmbedElement>(element) ||
        IsA<HTMLImageElement>(element) || IsA<HTMLObjectElement>(element) ||
        IsA<HTMLPictureElement>(element) || element->IsSVGElement() ||
        IsA<HTMLVideoElement>(element)) {
      colors.clear();
      found_opaque_color = false;
      continue;
    }

    const ComputedStyle* style = layout_object->Style();
    if (!style)
      continue;

    // If background elements are hidden, ignore their background colors.
    if (element != top_element &&
        style->UsedVisibility() == EVisibility::kHidden) {
      continue;
    }

    Color background_color =
        style->VisitedDependentColor(GetCSSPropertyBackgroundColor());

    // Opacity applies to the entire element so mix it with the alpha channel.
    if (style->HasOpacity()) {
      background_color.SetAlpha(background_color.Alpha() * style->Opacity());
      // If the background element is the ancestor of the top element or is the
      // top element, the opacity affects the text color of the top element.
      if (element == top_element ||
          FlatTreeTraversal::IsDescendantOf(*top_element, *element)) {
        *text_opacity *= style->Opacity();
      }
    }

    bool found_non_transparent_color = false;
    if (!background_color.IsFullyTransparent()) {
      found_non_transparent_color = true;
      if (!background_color.IsOpaque()) {
        if (colors.empty()) {
          colors.push_back(background_color);
        } else {
          for (auto& color : colors)
            color = color.Blend(background_color);
        }
      } else {
        colors.clear();
        colors.push_back(background_color);
        found_opaque_color = true;
      }
    }

    AddColorsFromImageStyle(*style, *layout_object, colors, found_opaque_color,
                            found_non_transparent_color);

    bool contains = found_top_element || GetNodeRect(node).Contains(rect);
    if (!contains && found_non_transparent_color) {
      // Only return colors if some opaque element covers up this one.
      colors.clear();
      found_opaque_color = false;
    }
  }
  return found_opaque_color;
}

// Sorts unsorted_elements in place, first painted go first.
void InspectorContrast::SortElementsByPaintOrder(
    HeapVector<Member<Node>>& unsorted_elements,
    Document* document) {
  InspectorDOMSnapshotAgent::PaintOrderMap* paint_layer_tree =
      InspectorDOMSnapshotAgent::BuildPaintLayerTree(document);

  std::stable_sort(
      unsorted_elements.begin(), unsorted_elements.end(),
      [&paint_layer_tree = paint_layer_tree](Node* a, Node* b) {
        const LayoutObject* a_layout = To<Element>(a)->GetLayoutObject();
        const LayoutObject* b_layout = To<Element>(b)->GetLayoutObject();
        int a_order = 0;
        int b_order = 0;

        auto a_item = paint_layer_tree->find(a_layout->PaintingLayer());
        if (a_item != paint_layer_tree->end())
          a_order = a_item->value;

        auto b_item = paint_layer_tree->find(b_layout->PaintingLayer());
        if (b_item != paint_layer_tree->end())
          b_order = b_item->value;

        return a_order < b_order;
      });
}

}  // namespace blink
