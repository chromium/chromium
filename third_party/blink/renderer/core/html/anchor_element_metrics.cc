// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/anchor_element_metrics.h"

#include "base/metrics/histogram_macros.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/loader/navigation_predictor.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/anchor_element_metrics_sender.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/platform/geometry/int_size.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

// Helper function that returns the root document the anchor element is in.
Document* GetRootDocument(const HTMLAnchorElement& anchor) {
  return anchor.GetDocument().GetFrame()->LocalFrameRoot().GetDocument();
}

// Accumulated scroll offset of all frames up to the local root frame.
int AccumulatedScrollOffset(const HTMLAnchorElement& anchor_element) {
  IntSize offset;
  Frame* frame = anchor_element.GetDocument().GetFrame();
  while (frame && frame->View()) {
    auto* local_frame = DynamicTo<LocalFrame>(frame);
    if (!local_frame)
      break;

    offset += local_frame->View()->LayoutViewport()->ScrollOffsetInt();
    frame = frame->Tree().Parent();
  }
  return offset.Height();
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
IntRect AbsoluteElementBoundingBoxRect(const LayoutObject* layout_object) {
  Vector<PhysicalRect> rects = layout_object->OutlineRects(
      PhysicalOffset(), NGOutlineType::kIncludeBlockVisualOverflow);
  return EnclosingIntRect(layout_object->LocalToAbsoluteRect(UnionRect(rects)));
}

}  // anonymous namespace

// static
base::Optional<AnchorElementMetrics> AnchorElementMetrics::Create(
    const HTMLAnchorElement* anchor_element) {
  LocalFrame* local_frame = anchor_element->GetDocument().GetFrame();
  if (!local_frame)
    return base::nullopt;

  AnchorElementMetrics anchor_metrics(
      anchor_element, 0, 0, 0, 0, 0, 0, IsInIFrame(*anchor_element),
      ContainsImage(*anchor_element), IsSameHost(*anchor_element),
      IsUrlIncrementedByOne(*anchor_element));

  // Don't record size metrics for subframe document Anchors.
  if (anchor_element->GetDocument().ParentDocument())
    return anchor_metrics;

  LayoutObject* layout_object = anchor_element->GetLayoutObject();
  if (!layout_object)
    return anchor_metrics;

  LocalFrameView* local_frame_view = local_frame->View();
  LocalFrameView* root_frame_view = local_frame->LocalFrameRoot().View();
  if (!local_frame_view || !root_frame_view)
    return anchor_metrics;

  IntRect viewport = root_frame_view->LayoutViewport()->VisibleContentRect();
  if (viewport.Size().IsEmpty())
    return anchor_metrics;

  // Use the viewport size to normalize anchor element metrics.
  float base_height = static_cast<float>(viewport.Height());
  float base_width = static_cast<float>(viewport.Width());

  // The anchor element rect in the root frame.
  IntRect target = local_frame_view->ConvertToRootFrame(
      AbsoluteElementBoundingBoxRect(layout_object));

  // Limit the element size to the viewport size.
  float ratio_area = std::min(1.0f, target.Height() / base_height) *
                     std::min(1.0f, target.Width() / base_width);
  DCHECK_GE(1.0, ratio_area);
  float ratio_distance_top_to_visible_top = target.Y() / base_height;
  float ratio_distance_center_to_visible_top =
      (target.Y() + target.Height() / 2.0) / base_height;

  float ratio_distance_root_top =
      (target.Y() + AccumulatedScrollOffset(*anchor_element)) / base_height;

  // Distance to the bottom is tricky if the element is inside sub/iframes.
  // Here we use the target location in the root viewport, and calculate
  // the distance from the bottom of the anchor element to the root bottom.
  int root_height = GetRootDocument(*anchor_element)
                        ->GetLayoutView()
                        ->GetScrollableArea()
                        ->ContentsSize()
                        .Height();

  int root_scrolled =
      root_frame_view->LayoutViewport()->ScrollOffsetInt().Height();
  float ratio_distance_root_bottom =
      (root_height - root_scrolled - target.Y() - target.Height()) /
      base_height;

  // Get the anchor element rect that intersects with the viewport.
  IntRect target_visible(target);
  target_visible.Intersect(IntRect(IntPoint(), viewport.Size()));

  // It guarantees to be less or equal to 1.
  float ratio_visible_area = (target_visible.Height() / base_height) *
                             (target_visible.Width() / base_width);
  DCHECK_GE(1.0, ratio_visible_area);

  return AnchorElementMetrics(
      anchor_element, ratio_area, ratio_visible_area,
      ratio_distance_top_to_visible_top, ratio_distance_center_to_visible_top,
      ratio_distance_root_top, ratio_distance_root_bottom,
      IsInIFrame(*anchor_element), ContainsImage(*anchor_element),
      IsSameHost(*anchor_element), IsUrlIncrementedByOne(*anchor_element));
}

// static
base::Optional<AnchorElementMetrics>
AnchorElementMetrics::MaybeReportClickedMetricsOnClick(
    const HTMLAnchorElement* anchor_element) {
  if (!base::FeatureList::IsEnabled(features::kNavigationPredictor) ||
      !anchor_element->Href().ProtocolIsInHTTPFamily() ||
      !GetRootDocument(*anchor_element)->Url().ProtocolIsInHTTPFamily() ||
      !anchor_element->GetDocument().BaseURL().ProtocolIsInHTTPFamily()) {
    return base::nullopt;
  }

  // Create metrics that don't have sizes set. The browser only records
  // metrics unrelated to sizes.
  AnchorElementMetrics anchor_metrics(
      anchor_element, 0, 0, 0, 0, 0, 0, IsInIFrame(*anchor_element),
      ContainsImage(*anchor_element), IsSameHost(*anchor_element),
      IsUrlIncrementedByOne(*anchor_element));

  anchor_metrics.RecordMetricsOnClick();

  // Send metrics of the anchor element to the browser process.
  AnchorElementMetricsSender::From(*GetRootDocument(*anchor_element))
      ->SendClickedAnchorMetricsToBrowser(anchor_metrics.CreateMetricsPtr());

  return anchor_metrics;
}

// static
void AnchorElementMetrics::NotifyOnLoad(Document& document) {
  DCHECK(document.GetFrame());
  if (!base::FeatureList::IsEnabled(features::kNavigationPredictor) ||
      !document.GetFrame()->IsMainFrame() || !document.View() ||
      !document.Url().ProtocolIsInHTTPFamily() ||
      !document.BaseURL().ProtocolIsInHTTPFamily()) {
    return;
  }

  AnchorElementMetricsSender* sender =
      AnchorElementMetricsSender::From(document);

  document.View()->RegisterForLifecycleNotifications(sender);
}

// static
void AnchorElementMetrics::MaybeReportViewportMetricsOnLoad(
    Document& document) {
  DCHECK(document.GetFrame());
  DCHECK(base::FeatureList::IsEnabled(features::kNavigationPredictor));
  DCHECK(document.GetFrame()->IsMainFrame());
  DCHECK(document.View());
  DCHECK(document.Url().ProtocolIsInHTTPFamily());
  DCHECK(document.BaseURL().ProtocolIsInHTTPFamily());

  DCHECK_GE(document.Lifecycle().GetState(),
            DocumentLifecycle::kAfterPerformLayout);

  AnchorElementMetricsSender* sender =
      AnchorElementMetricsSender::From(document);

  Vector<mojom::blink::AnchorElementMetricsPtr> anchor_elements_metrics;
  for (const auto& member_element : sender->GetAnchorElements()) {
    const HTMLAnchorElement& anchor_element = *member_element;

    if (!anchor_element.Href().ProtocolIsInHTTPFamily())
      continue;

    // If the anchor doesn't have a valid frame/root document, skip it.
    if (!anchor_element.GetDocument().GetFrame() ||
        !GetRootDocument(anchor_element)) {
      continue;
    }

    // Only anchors with width/height should be evaluated.
    if (!anchor_element.GetLayoutObject() ||
        anchor_element.GetLayoutObject()->AbsoluteBoundingBoxRect().IsEmpty()) {
      continue;
    }

    base::Optional<AnchorElementMetrics> anchor_metric =
        Create(&anchor_element);
    if (!anchor_metric.has_value())
      continue;

    anchor_elements_metrics.push_back(anchor_metric.value().CreateMetricsPtr());

    // Webpages with more than 100 anchors will stop processing at the 100th
    // anchor element.
    if (anchor_elements_metrics.size() >= 100)
      break;
  }

  if (anchor_elements_metrics.IsEmpty())
    return;

  LocalFrame* local_frame = document.GetFrame();
  LocalFrameView* root_frame_view = local_frame->LocalFrameRoot().View();
  IntRect viewport = root_frame_view->LayoutViewport()->VisibleContentRect();

  sender->SendAnchorMetricsVectorToBrowser(std::move(anchor_elements_metrics),
                                           viewport.Size());
}

mojom::blink::AnchorElementMetricsPtr AnchorElementMetrics::CreateMetricsPtr()
    const {
  auto metrics = mojom::blink::AnchorElementMetrics::New();
  metrics->ratio_area = ratio_area_;
  DCHECK_GE(1.0, metrics->ratio_area);
  metrics->ratio_visible_area = ratio_visible_area_;
  DCHECK_GE(1.0, metrics->ratio_visible_area);
  metrics->ratio_distance_top_to_visible_top =
      ratio_distance_top_to_visible_top_;
  metrics->ratio_distance_center_to_visible_top =
      ratio_distance_center_to_visible_top_;
  metrics->ratio_distance_root_top = ratio_distance_root_top_;
  metrics->ratio_distance_root_bottom = ratio_distance_root_bottom_;
  metrics->is_in_iframe = is_in_iframe_;
  metrics->contains_image = contains_image_;
  metrics->is_same_host = is_same_host_;
  metrics->is_url_incremented_by_one = is_url_incremented_by_one_;

  metrics->source_url = GetRootDocument(*anchor_element_)->Url();
  metrics->target_url = anchor_element_->Href();

  return metrics;
}

void AnchorElementMetrics::RecordMetricsOnClick() const {
  UMA_HISTOGRAM_BOOLEAN("AnchorElementMetrics.Clicked.IsSameHost",
                        is_same_host_);
}

}  // namespace blink
