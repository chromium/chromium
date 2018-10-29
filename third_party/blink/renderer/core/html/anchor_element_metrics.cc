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
#include "third_party/blink/renderer/platform/histogram.h"
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
  while (frame && frame->View() && frame->IsLocalFrame()) {
    offset += ToLocalFrame(frame)->View()->LayoutViewport()->ScrollOffsetInt();
    frame = frame->Tree().Parent();
  }
  return offset.Height();
}

// Whether the element is inside an iframe.
bool IsInIFrame(const HTMLAnchorElement& anchor_element) {
  Frame* frame = anchor_element.GetDocument().GetFrame();
  while (frame && frame->IsLocalFrame()) {
    HTMLFrameOwnerElement* owner =
        ToLocalFrame(frame)->GetDocument()->LocalOwner();
    if (owner && IsHTMLIFrameElement(owner))
      return true;
    frame = frame->Tree().Parent();
  }
  return false;
}

// Whether the anchor element contains an image element.
bool ContainsImage(const HTMLAnchorElement& anchor_element) {
  for (Node* node = FlatTreeTraversal::FirstChild(anchor_element); node;
       node = FlatTreeTraversal::Next(*node, &anchor_element)) {
    if (IsHTMLImageElement(*node))
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
  Vector<LayoutRect> rects;
  layout_object->AddElementVisualOverflowRects(rects, LayoutPoint());

  return layout_object
      ->LocalToAbsoluteQuad(FloatQuad(FloatRect(UnionRect(rects))))
      .EnclosingBoundingBox();
}

}  // anonymous namespace

// Webpage with more than |kMaxAnchorElementMetricsSize| anchor element metrics
// to report will be ignored, so it should be large enough to cover most pages.
const int AnchorElementMetrics::kMaxAnchorElementMetricsSize = 40;

// static
base::Optional<AnchorElementMetrics> AnchorElementMetrics::Create(
    const HTMLAnchorElement* anchor_element) {
  LocalFrame* local_frame = anchor_element->GetDocument().GetFrame();
  LayoutObject* layout_object = anchor_element->GetLayoutObject();
  if (!local_frame || !layout_object)
    return base::nullopt;

  LocalFrameView* local_frame_view = local_frame->View();
  LocalFrameView* root_frame_view = local_frame->LocalFrameRoot().View();
  if (!local_frame_view || !root_frame_view)
    return base::nullopt;

  IntRect viewport = root_frame_view->LayoutViewport()->VisibleContentRect();
  if (viewport.Size().IsEmpty())
    return base::nullopt;

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
  float ratio_root_height = root_height / base_height;

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
      ratio_distance_root_top, ratio_distance_root_bottom, ratio_root_height,
      IsInIFrame(*anchor_element), ContainsImage(*anchor_element),
      IsSameHost(*anchor_element), IsUrlIncrementedByOne(*anchor_element));
}

// static
base::Optional<AnchorElementMetrics>
AnchorElementMetrics::MaybeReportClickedMetricsOnClick(
    const HTMLAnchorElement* anchor_element) {
  if (!base::FeatureList::IsEnabled(features::kRecordAnchorMetricsClicked) ||
      !anchor_element->Href().ProtocolIsInHTTPFamily() ||
      !GetRootDocument(*anchor_element)->Url().ProtocolIsInHTTPFamily() ||
      !anchor_element->GetDocument().BaseURL().ProtocolIsInHTTPFamily()) {
    return base::nullopt;
  }

  auto anchor_metrics = Create(anchor_element);
  if (anchor_metrics.has_value()) {
    anchor_metrics.value().RecordMetricsOnClick();

    // Send metrics of the anchor element to the browser process.
    AnchorElementMetricsSender::From(*GetRootDocument(*anchor_element))
        ->SendClickedAnchorMetricsToBrowser(
            anchor_metrics.value().CreateMetricsPtr());
  }

  return anchor_metrics;
}

// static
void AnchorElementMetrics::MaybeReportViewportMetricsOnLoad(
    Document& document) {
  DCHECK(document.GetFrame());
  if (!base::FeatureList::IsEnabled(features::kRecordAnchorMetricsVisible) ||
      document.ParentDocument() || !document.View() ||
      !document.Url().ProtocolIsInHTTPFamily() ||
      !document.BaseURL().ProtocolIsInHTTPFamily()) {
    return;
  }

  Vector<mojom::blink::AnchorElementMetricsPtr> anchor_elements_metrics;
  AnchorElementMetricsSender* sender =
      AnchorElementMetricsSender::From(document);
  for (const auto& member_element : sender->GetAnchorElements()) {
    const HTMLAnchorElement& anchor_element = *member_element;

    // We ignore anchor elements that are not in the visual viewport.
    if (!anchor_element.Href().ProtocolIsInHTTPFamily() ||
        anchor_element.VisibleBoundsInVisualViewport().IsEmpty()) {
      continue;
    }

    base::Optional<AnchorElementMetrics> anchor_metric =
        Create(&anchor_element);
    if (!anchor_metric.has_value())
      continue;

    anchor_elements_metrics.push_back(anchor_metric.value().CreateMetricsPtr());

    if (anchor_elements_metrics.size() > kMaxAnchorElementMetricsSize)
      return;
  }

  if (anchor_elements_metrics.IsEmpty())
    return;

  sender->SendAnchorMetricsVectorToBrowser(std::move(anchor_elements_metrics));
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
  UMA_HISTOGRAM_PERCENTAGE("AnchorElementMetrics.Clicked.RatioArea",
                           static_cast<int>(ratio_area_ * 100));

  UMA_HISTOGRAM_PERCENTAGE("AnchorElementMetrics.Clicked.RatioVisibleArea",
                           static_cast<int>(ratio_visible_area_ * 100));

  UMA_HISTOGRAM_PERCENTAGE(
      "AnchorElementMetrics.Clicked.RatioDistanceTopToVisibleTop",
      static_cast<int>(std::min(ratio_distance_top_to_visible_top_, 1.0f) *
                       100));

  UMA_HISTOGRAM_PERCENTAGE(
      "AnchorElementMetrics.Clicked.RatioDistanceCenterToVisibleTop",
      static_cast<int>(std::min(ratio_distance_center_to_visible_top_, 1.0f) *
                       100));

  UMA_HISTOGRAM_COUNTS_10000(
      "AnchorElementMetrics.Clicked.RatioDistanceRootTop",
      static_cast<int>(std::min(ratio_distance_root_top_, 100.0f) * 100));

  UMA_HISTOGRAM_COUNTS_10000(
      "AnchorElementMetrics.Clicked.RatioDistanceRootBottom",
      static_cast<int>(std::min(ratio_distance_root_bottom_, 100.0f) * 100));

  UMA_HISTOGRAM_COUNTS_10000(
      "AnchorElementMetrics.Clicked.RatioRootHeight",
      static_cast<int>(std::min(ratio_root_height_, 100.0f) * 100));

  UMA_HISTOGRAM_BOOLEAN("AnchorElementMetrics.Clicked.IsInIFrame",
                        is_in_iframe_);

  UMA_HISTOGRAM_BOOLEAN("AnchorElementMetrics.Clicked.ContainsImage",
                        contains_image_);

  UMA_HISTOGRAM_BOOLEAN("AnchorElementMetrics.Clicked.IsSameHost",
                        is_same_host_);

  UMA_HISTOGRAM_BOOLEAN("AnchorElementMetrics.Clicked.IsUrlIncrementedByOne",
                        is_url_incremented_by_one_);
}

}  // namespace blink
