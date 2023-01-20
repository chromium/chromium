// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/anchor_element_metrics.h"

#include "base/containers/span.h"
#include "base/hash/hash.h"
#include "base/metrics/histogram_macros.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/loader/navigation_predictor.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/anchor_element_metrics_sender.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

namespace {

// Accumulated scroll offset of all frames up to the local root frame.
int AccumulatedScrollOffset(const HTMLAnchorElement& anchor_element) {
  int offset = 0;
  Frame* frame = anchor_element.GetDocument().GetFrame();
  while (frame && frame->View()) {
    auto* local_frame = DynamicTo<LocalFrame>(frame);
    if (!local_frame)
      break;

    offset += local_frame->View()->LayoutViewport()->ScrollOffsetInt().y();
    frame = frame->Tree().Parent();
  }
  return offset;
}

// Whether the element is inside an iframe.
bool IsInIFrame(const HTMLAnchorElement& anchor_element) {
  Frame* frame = anchor_element.GetDocument().GetFrame();
  while (auto* local_frame = DynamicTo<LocalFrame>(frame)) {
    HTMLFrameOwnerElement* owner = local_frame->GetDocument()->LocalOwner();
    if (owner && IsA<HTMLIFrameElement>(owner))
      return true;
    frame = frame->Tree().Parent();
  }
  return false;
}

// Whether the anchor element contains an image element.
bool ContainsImage(const HTMLAnchorElement& anchor_element) {
  for (Node* node = FlatTreeTraversal::FirstChild(anchor_element); node;
       node = FlatTreeTraversal::Next(*node, &anchor_element)) {
    if (IsA<HTMLImageElement>(*node))
      return true;
  }
  return false;
}

// Whether the link target has the same host as the root document.
bool IsSameHost(const HTMLAnchorElement& anchor_element) {
  String source_host = GetRootDocument(anchor_element)->Url().Host();
  String target_host = anchor_element.Href().Host();
  return source_host == target_host;
}

// Returns true if the two strings only differ by one number, and
// the second number equals the first number plus one. Examples:
// example.com/page9/cat5, example.com/page10/cat5 => true
// example.com/page9/cat5, example.com/page10/cat10 => false
bool IsStringIncrementedByOne(const String& source, const String& target) {
  // Consecutive numbers should differ in length by at most 1.
  int length_diff = target.length() - source.length();
  if (length_diff < 0 || length_diff > 1)
    return false;

  // The starting position of difference.
  unsigned int left = 0;
  while (left < source.length() && left < target.length() &&
         source[left] == target[left]) {
    left++;
  }

  // There is no difference, or the difference is not a digit.
  if (left == source.length() || left == target.length() ||
      !u_isdigit(source[left]) || !u_isdigit(target[left])) {
    return false;
  }

  // Expand towards right to extract the numbers.
  unsigned int source_right = left + 1;
  while (source_right < source.length() && u_isdigit(source[source_right]))
    source_right++;

  unsigned int target_right = left + 1;
  while (target_right < target.length() && u_isdigit(target[target_right]))
    target_right++;

  int source_number = source.Substring(left, source_right - left).ToInt();
  int target_number = target.Substring(left, target_right - left).ToInt();

  // The second number should increment by one and the rest of the strings
  // should be the same.
  return source_number + 1 == target_number &&
         source.Substring(source_right) == target.Substring(target_right);
}

// Extract source and target link url, and return IsStringIncrementedByOne().
bool IsUrlIncrementedByOne(const HTMLAnchorElement& anchor_element) {
  if (!IsSameHost(anchor_element))
    return false;

  String source_url = GetRootDocument(anchor_element)->Url().GetString();
  String target_url = anchor_element.Href().GetString();
  return IsStringIncrementedByOne(source_url, target_url);
}

// Returns the bounding box rect of a layout object, including visual
// overflows.
gfx::Rect AbsoluteElementBoundingBoxRect(const LayoutObject& layout_object) {
  Vector<PhysicalRect> rects = layout_object.OutlineRects(
      nullptr, PhysicalOffset(), NGOutlineType::kIncludeBlockVisualOverflow);
  return ToEnclosingRect(layout_object.LocalToAbsoluteRect(UnionRect(rects)));
}

bool IsNonEmptyTextNode(Node* node) {
  if (!node) {
    return false;
  }
  if (!node->IsTextNode()) {
    return false;
  }
  return !To<Text>(node)->wholeText().ContainsOnlyWhitespaceOrEmpty();
}

}  // anonymous namespace

// Helper function that returns the root document the anchor element is in.
Document* GetRootDocument(const HTMLAnchorElement& anchor) {
  return anchor.GetDocument().GetFrame()->LocalFrameRoot().GetDocument();
}

// Computes a unique ID for the anchor. We hash the pointer address of the
// object. Note that this implementation can lead to collisions if an element is
// destroyed and a new one is created with the same address. We don't mind this
// issue as the anchor ID is only used for metric collection.
uint32_t AnchorElementId(const HTMLAnchorElement& element) {
  return WTF::GetHash(&element);
}

mojom::blink::AnchorElementMetricsPtr CreateAnchorElementMetrics(
    const HTMLAnchorElement& anchor_element) {
  LocalFrame* local_frame = anchor_element.GetDocument().GetFrame();
  if (!local_frame) {
    return nullptr;
  }

  mojom::blink::AnchorElementMetricsPtr metrics =
      mojom::blink::AnchorElementMetrics::New();
  metrics->anchor_id = AnchorElementId(anchor_element);
  metrics->is_in_iframe = IsInIFrame(anchor_element);
  metrics->contains_image = ContainsImage(anchor_element);
  metrics->is_same_host = IsSameHost(anchor_element);
  metrics->is_url_incremented_by_one = IsUrlIncrementedByOne(anchor_element);
  metrics->source_url = GetRootDocument(anchor_element)->Url();
  metrics->target_url = anchor_element.Href();

  // Don't record size metrics for subframe document Anchors.
  if (anchor_element.GetDocument().ParentDocument())
    return metrics;

  LayoutObject* layout_object = anchor_element.GetLayoutObject();
  if (!layout_object)
    return metrics;

  LocalFrameView* local_frame_view = local_frame->View();
  LocalFrameView* root_frame_view = local_frame->LocalFrameRoot().View();
  if (!local_frame_view || !root_frame_view)
    return metrics;

  gfx::Rect viewport = root_frame_view->LayoutViewport()->VisibleContentRect();
  if (viewport.IsEmpty())
    return metrics;
  metrics->viewport_size = viewport.size();

  // Use the viewport size to normalize anchor element metrics.
  float base_height = static_cast<float>(viewport.height());
  float base_width = static_cast<float>(viewport.width());

  // The anchor element rect in the root frame.
  gfx::Rect target = local_frame_view->ConvertToRootFrame(
      AbsoluteElementBoundingBoxRect(*layout_object));

  // Limit the element size to the viewport size.
  float ratio_area = std::min(1.0f, target.height() / base_height) *
                     std::min(1.0f, target.width() / base_width);
  DCHECK_GE(1.0, ratio_area);
  metrics->ratio_area = ratio_area;

  float ratio_distance_top_to_visible_top = target.y() / base_height;
  metrics->ratio_distance_top_to_visible_top =
      ratio_distance_top_to_visible_top;

  float ratio_distance_center_to_visible_top =
      (target.y() + target.height() / 2.0) / base_height;
  metrics->ratio_distance_center_to_visible_top =
      ratio_distance_center_to_visible_top;

  float ratio_distance_root_top =
      (target.y() + AccumulatedScrollOffset(anchor_element)) / base_height;
  metrics->ratio_distance_root_top = ratio_distance_root_top;

  // Distance to the bottom is tricky if the element is inside sub/iframes.
  // Here we use the target location in the root viewport, and calculate
  // the distance from the bottom of the anchor element to the root bottom.
  int root_height = GetRootDocument(anchor_element)
                        ->GetLayoutView()
                        ->GetScrollableArea()
                        ->ContentsSize()
                        .height();

  int root_scrolled = root_frame_view->LayoutViewport()->ScrollOffsetInt().y();
  float ratio_distance_root_bottom =
      (root_height - root_scrolled - target.y() - target.height()) /
      base_height;
  metrics->ratio_distance_root_bottom = ratio_distance_root_bottom;

  // Get the anchor element rect that intersects with the viewport.
  gfx::Rect target_visible = target;
  target_visible.Intersect(gfx::Rect(viewport.size()));

  // It guarantees to be less or equal to 1.
  float ratio_visible_area = (target_visible.height() / base_height) *
                             (target_visible.width() / base_width);
  DCHECK_GE(1.0, ratio_visible_area);
  metrics->ratio_visible_area = ratio_visible_area;

  metrics->has_text_sibling =
      IsNonEmptyTextNode(anchor_element.nextSibling()) ||
      IsNonEmptyTextNode(anchor_element.previousSibling());

  const ComputedStyle& computed_style = anchor_element.ComputedStyleRef();
  metrics->font_weight =
      static_cast<uint32_t>(computed_style.GetFontWeight() + 0.5f);
  metrics->font_size_px = computed_style.FontSize();

  return metrics;
}

}  // namespace blink
