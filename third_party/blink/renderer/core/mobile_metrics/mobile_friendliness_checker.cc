// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/mobile_metrics/mobile_friendliness_checker.h"

#include "third_party/blink/public/mojom/mobile_metrics/mobile_friendliness.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/page_scale_constraints_set.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/viewport_description.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

using mojom::blink::ViewportStatus;
static constexpr int kSmallFontThreshold = 9;
const base::Feature kBadTapTargetsRatio{"BadTapTargetsRatio",
                                        base::FEATURE_DISABLED_BY_DEFAULT};
static constexpr int kTimeBudgetExceeded = -2;

// Finding bad tap targets may takes too time for big page and should abort if
// it takes more than 5ms.
static constexpr base::TimeDelta kTimeBudgetForBadTapTarget =
    base::TimeDelta::FromMilliseconds(5);

MobileFriendlinessChecker::MobileFriendlinessChecker(LocalFrameView& frame_view)
    : frame_view_(&frame_view),
      font_size_check_enabled_(frame_view_->GetFrame().GetWidgetForLocalRoot()),
      tap_target_check_enabled_(
          base::FeatureList::IsEnabled(kBadTapTargetsRatio) &&
          frame_view_->GetFrame().GetWidgetForLocalRoot()),
      viewport_scalar_(
          font_size_check_enabled_
              ? frame_view_->GetPage()
                    ->GetChromeClient()
                    .WindowToViewportScalar(&frame_view_->GetFrame(), 1)
              : 0),
      fcp_detected_(false) {}

MobileFriendlinessChecker::~MobileFriendlinessChecker() = default;

void MobileFriendlinessChecker::NotifyFirstContentfulPaint() {
  fcp_detected_ = true;
}

namespace {

bool IsTimeBudgetExpired(const base::Time& from) {
  return base::Time::Now() - from > kTimeBudgetForBadTapTarget;
}

// Fenwick tree is a data structure which can efficiently update elements and
// calculate prefix sums in an array of numbers. We use it here to track tap
// targets which are too close.
class FenwickTree {
 public:
  explicit FenwickTree(size_t n) : tree(n + 1) {}

  // Returns prefix sum of the array from 0 to |index|.
  int sum(size_t index) const {
    int sum = 0;
    for (index += 1; 0 < index; index -= index & -index)
      sum += tree[index];
    return sum;
  }

  // Adds |val| at |index| of the array.
  void add(size_t index, int val) {
    for (index += 1; index <= tree.size() - 1; index += index & -index)
      tree[index] += val;
  }

 private:
  Vector<int> tree;
};

// Stands for a vertex in the view, this is four corner or center of tap targets
// rectangles.
// Start edge means top edge of rectangle, End edge means bottom edge of
// rectangle, Center means arithmetic mean of four corners.
// In bad tap targets context, "Bad target" means a targets hard to tap
// precisely because there are other targets which are too close to the target.
struct EdgeOrCenter {
  enum Type : int { kStartEdge = 0, kCenter = 1, kEndEdge = 2 } type;

  union EdgeOrCenterUnion {
    // Valid iff |type| is Edge.
    struct Edge {
      int left;
      int right;
    } edge;

    // Valid iff |type| is Center.
    int center;
  } v;

  static EdgeOrCenter StartEdge(int left, int right) {
    EdgeOrCenter edge;
    edge.type = EdgeOrCenter::kStartEdge;
    edge.v.edge.left = left;
    edge.v.edge.right = right;
    return edge;
  }

  static EdgeOrCenter EndEdge(int left, int right) {
    EdgeOrCenter edge;
    edge.type = EdgeOrCenter::kEndEdge;
    edge.v.edge.left = left;
    edge.v.edge.right = right;
    return edge;
  }

  static EdgeOrCenter Center(int center) {
    EdgeOrCenter edge;
    edge.type = EdgeOrCenter::kCenter;
    edge.v.center = center;
    return edge;
  }
};

bool IsTapTargetCandidate(const Node* node) {
  return IsA<HTMLFormControlElement>(node) ||
         (IsA<HTMLAnchorElement>(node) &&
          !To<HTMLAnchorElement>(node)->Href().IsEmpty());
}

// Scans full DOM tree and register all tap regions.
// root: DOM tree's root.
// finger_radius: Extends every tap regions with given pixels.
// x_positions: Collects and inserts every x dimension positions.
// vertices: Inserts y dimension keyed vertex positions with its attribute.
// Returns total count of tap targets.
// Returns -1 if time limit exceeded.
int ExtractAndCountAllTapTargets(
    LayoutObject* const root,
    int finger_radius,
    Vector<int>& x_positions,
    const base::Time& started,
    Vector<std::pair<int, EdgeOrCenter>>& vertices) {
  vertices.clear();
  int tap_targets = 0;
  for (LayoutObject* object = root; object;) {
    Node* node = object->GetNode();
    const ComputedStyle* style = object->Style();
    if (!node || !IsTapTargetCandidate(node)) {
      object = object->NextInPreOrder();
      continue;
    }
    if (object->IsElementContinuation() ||
        style->Visibility() != EVisibility::kVisible ||
        style->ContentVisibility() != EContentVisibility::kVisible) {
      // Skip the whole subtree in this case. Some elements in subtree may have
      // visibility: visible property which should not be ignored for
      // correctness, but it is rare and we priority performance.
      object = object->NextInPreOrderAfterChildren();
      continue;
    }
    if (Element* element = DynamicTo<Element>(object->GetNode())) {
      // Expand each corner by the size of fingertips.
      const FloatRect rect = element->GetBoundingClientRectNoLifecycleUpdate();
      if (rect.IsEmpty()) {
        object = object->NextInPreOrder();
        continue;
      }
      const int top = rect.Y() - finger_radius;
      const int bottom = rect.MaxY() + finger_radius;
      const int left = rect.X() - finger_radius;
      const int right = rect.MaxX() + finger_radius;
      const int center = (left + right) / 2;
      vertices.emplace_back(top, EdgeOrCenter::StartEdge(left, right));
      vertices.emplace_back((top + bottom) / 2, EdgeOrCenter::Center(center));
      vertices.emplace_back(bottom, EdgeOrCenter::EndEdge(left, right));
      x_positions.push_back(left);
      x_positions.push_back(right);
      x_positions.push_back(center);
      tap_targets++;

      if (IsTimeBudgetExpired(started))
        return -1;
    }
    object = object->NextInPreOrder();
  }
  return tap_targets;
}

// Compress the x-dimension range and overwrites the value.
// Precondition: |positions| must be sorted and unique.
void CompressKeyWithVector(const Vector<int>& positions,
                           Vector<std::pair<int, EdgeOrCenter>>& vertices) {
  // Overwrite the vertex key with the position of the map.
  for (auto& it : vertices) {
    EdgeOrCenter& vertex = it.second;
    switch (vertex.type) {
      case EdgeOrCenter::kStartEdge:
      case EdgeOrCenter::kEndEdge: {
        vertex.v.edge.left =
            std::distance(positions.begin(),
                          std::lower_bound(positions.begin(), positions.end(),
                                           vertex.v.edge.left));
        vertex.v.edge.right =
            std::distance(positions.begin(),
                          std::lower_bound(positions.begin(), positions.end(),
                                           vertex.v.edge.right));
        break;
      }
      case EdgeOrCenter::kCenter: {
        vertex.v.center =
            std::distance(positions.begin(),
                          std::lower_bound(positions.begin(), positions.end(),
                                           vertex.v.center));
        break;
      }
    }
  }
}

// Scans the vertices from top to bottom with updating FenwickTree to track
// tap target regions.
// Precondition: |vertex| must be sorted by its |first|.
// rightmost_position: Rightmost x position in all vertices.
// Returns bad tap targets count.
// Returns -1 if time limit exceeded.
int CountBadTapTargets(int rightmost_position,
                       const Vector<std::pair<int, EdgeOrCenter>>& vertices,
                       const base::Time& started) {
  FenwickTree tree(rightmost_position);
  int bad_tap_targets = 0;
  for (const auto& it : vertices) {
    const EdgeOrCenter& vertex = it.second;
    switch (vertex.type) {
      case EdgeOrCenter::kStartEdge: {
        // Tap region begins.
        tree.add(vertex.v.edge.left, 1);
        tree.add(vertex.v.edge.right, -1);
        break;
      }
      case EdgeOrCenter::kEndEdge: {
        // Tap region ends.
        tree.add(vertex.v.edge.left, -1);
        tree.add(vertex.v.edge.right, 1);
        break;
      }
      case EdgeOrCenter::kCenter: {
        // Iff the center of a tap target is included other than itself, it is a
        // Bad Target.
        if (tree.sum(vertex.v.center) > 1)
          bad_tap_targets++;
        break;
      }
    }
    if (IsTimeBudgetExpired(started))
      return -1;
  }
  return bad_tap_targets;
}

}  // namespace

// Counts and calculate ration of bad tap targets. The process is a surface scan
// with region tracking by Fenwick tree. The detail of the algorithm is
// go/bad-tap-target-ukm
void MobileFriendlinessChecker::ComputeBadTapTargetsRatio() {
  base::Time started = base::Time::Now();
  constexpr float kOneDipInMm = 0.15875;
  const float scale_factor = frame_view_->GetChromeClient()
                                 ->GetScreenInfo(frame_view_->GetFrame())
                                 .device_scale_factor;
  const int finger_radius =
      std::floor((3 / kOneDipInMm) / scale_factor);  // 3mm in logical pixel.
  Vector<std::pair<int, EdgeOrCenter>> vertices;
  Vector<int> x_positions;

  // Scan full DOM tree and extract every corner and center position of tap
  // targets.
  int all_tap_targets = ExtractAndCountAllTapTargets(
      frame_view_->GetFrame().GetDocument()->GetLayoutView(), finger_radius,
      x_positions, started, vertices);
  if (all_tap_targets == -1) {
    mobile_friendliness_.bad_tap_targets_ratio = kTimeBudgetExceeded;
    return;
  }

  // Compress x dimension of all vertices to save memory.
  // This will reduce rightmost position of vertices without sacrificing
  // accuracy so that required memory by Fenwick Tree will be reduced.
  std::sort(x_positions.begin(), x_positions.end());
  x_positions.erase(std::unique(x_positions.begin(), x_positions.end()),
                    x_positions.end());
  CompressKeyWithVector(x_positions, vertices);

  // Reorder vertices by y dimension for sweeping full page from top to bottom.
  std::sort(vertices.begin(), vertices.end(),
            [](const std::pair<int, EdgeOrCenter>& a,
               const std::pair<int, EdgeOrCenter>& b) {
              // Ordering with kStart < kCenter < kEnd.
              return std::tie(a.first, a.second.type) <
                     std::tie(b.first, b.second.type);
            });

  // Sweep x-compressed y-ordered vertices to detect bad tap targets.
  const int bad_tap_targets =
      CountBadTapTargets(x_positions.size(), vertices, started);
  if (bad_tap_targets == -1) {
    mobile_friendliness_.bad_tap_targets_ratio = kTimeBudgetExceeded;
    return;
  }

  if (all_tap_targets > 0) {
    mobile_friendliness_.bad_tap_targets_ratio =
        bad_tap_targets * 100 / all_tap_targets;
  } else {
    mobile_friendliness_.bad_tap_targets_ratio = 0;
  }
}

void MobileFriendlinessChecker::NotifyDocumentUnload() {
  // If detached, there's no need to calculate any metrics.
  if (!frame_view_->GetChromeClient())
    return;

  if (tap_target_check_enabled_)
    ComputeBadTapTargetsRatio();

  if (font_size_check_enabled_)
    mobile_friendliness_.small_text_ratio = text_area_sizes_.SmallTextRatio();

  // As long as evaluated as MF, TextOutsideViewportPercentage UKM must not be
  // -1 (means unknown). Even if there is no call of
  // ComputeTextContentOutsideViewport(), as far as there are FCP notification
  // and unload event, that value is not -1 anymore and to be 0.
  mobile_friendliness_.text_content_outside_viewport_percentage = std::max(
      0, mobile_friendliness_.text_content_outside_viewport_percentage);

  if (fcp_detected_)
    frame_view_->DidChangeMobileFriendliness(mobile_friendliness_);
}

void MobileFriendlinessChecker::NotifyViewportUpdated(
    const ViewportDescription& viewport) {
  switch (viewport.type) {
    case ViewportDescription::Type::kUserAgentStyleSheet:
      if (mobile_friendliness_.viewport_device_width ==
          ViewportStatus::kUnknown)
        mobile_friendliness_.viewport_device_width = ViewportStatus::kNo;

      if (mobile_friendliness_.allow_user_zoom == ViewportStatus::kUnknown)
        mobile_friendliness_.allow_user_zoom = ViewportStatus::kYes;
      break;
    case ViewportDescription::Type::kViewportMeta:
      mobile_friendliness_.viewport_device_width =
          viewport.max_width.IsDeviceWidth() ? ViewportStatus::kYes
                                             : ViewportStatus::kNo;
      if (viewport.max_width.IsFixed()) {
        mobile_friendliness_.viewport_hardcoded_width =
            viewport.max_width.GetFloatValue();
      }
      if (viewport.zoom_is_explicit) {
        mobile_friendliness_.viewport_initial_scale_x10 =
            std::round(viewport.zoom * 10);
      }
      if (viewport.user_zoom_is_explicit) {
        mobile_friendliness_.allow_user_zoom =
            viewport.user_zoom ? ViewportStatus::kYes : ViewportStatus::kNo;
      }
      break;
    default:
      return;
  }
}

int MobileFriendlinessChecker::TextAreaWithFontSize::SmallTextRatio() const {
  if (total_text_area == 0)
    return 0;
  return small_font_area * 100 / total_text_area;
}

void MobileFriendlinessChecker::NotifyInvalidatePaint(
    const LayoutObject& object) {
  ComputeTextContentOutsideViewport(object);

  if (font_size_check_enabled_)
    ComputeSmallTextRatio(object);
}

void MobileFriendlinessChecker::ComputeSmallTextRatio(
    const LayoutObject& object) {
  if (const auto* text = DynamicTo<LayoutText>(object)) {
    const ComputedStyle* style = text->Style();

    if (style->Visibility() != EVisibility::kVisible)
      return;

    double actual_font_size = style->FontSize();
    double initial_scale = frame_view_->GetPage()
                               ->GetPageScaleConstraintsSet()
                               .FinalConstraints()
                               .initial_scale;
    if (initial_scale > 0)
      actual_font_size *= initial_scale;
    actual_font_size /= viewport_scalar_;

    double area = text->PhysicalAreaSize();
    if (actual_font_size < kSmallFontThreshold)
      text_area_sizes_.small_font_area += area;

    text_area_sizes_.total_text_area += area;
  }
}

constexpr int kMaxAncestorCount = 5;
bool CheckParentHasOverflowXHidden(const LayoutObject* obj) {
  int ancestor_count = kMaxAncestorCount;
  while (obj && ancestor_count > 0) {
    const ComputedStyle* style = obj->Style();
    if (style->OverflowX() == EOverflow::kHidden)
      return true;
    obj = obj->Parent();
    --ancestor_count;
  }
  return false;
}

void MobileFriendlinessChecker::ComputeTextContentOutsideViewport(
    const LayoutObject& object) {
  if (!frame_view_->GetFrame().IsMainFrame())
    return;

  int frame_width = frame_view_->GetPage()->GetVisualViewport().Size().Width();
  if (frame_width == 0) {
    return;
  }

  int total_text_width;
  if (const auto* text = DynamicTo<LayoutText>(object)) {
    const ComputedStyle* style = text->Style();
    if (style->Visibility() != EVisibility::kVisible ||
        style->ContentVisibility() != EContentVisibility::kVisible ||
        style->Opacity() == 0.0 || CheckParentHasOverflowXHidden(&object))
      return;
    total_text_width = text->PhysicalRightOffset().ToInt();
  } else if (const auto* image = DynamicTo<LayoutImage>(object)) {
    const ComputedStyle* style = image->Style();
    if (style->Visibility() != EVisibility::kVisible ||
        style->ContentVisibility() != EContentVisibility::kVisible ||
        style->Opacity() == 0.0 || CheckParentHasOverflowXHidden(&object))
      return;
    total_text_width = image->FrameRect().MaxX().ToInt();
  } else {
    return;
  }

  double initial_scale = frame_view_->GetPage()
                             ->GetPageScaleConstraintsSet()
                             .FinalConstraints()
                             .initial_scale;
  if (initial_scale > 0)
    total_text_width *= initial_scale;

  int text_content_outside_viewport_percentage = 0;
  if (total_text_width > frame_width) {
    // We use ceil function here because we want to treat 100.1% as 101 which
    // requires a scroll bar.
    text_content_outside_viewport_percentage =
        std::ceil((total_text_width - frame_width) * 100.0 / frame_width);
  }

  mobile_friendliness_.text_content_outside_viewport_percentage =
      std::max(mobile_friendliness_.text_content_outside_viewport_percentage,
               text_content_outside_viewport_percentage);
}

void MobileFriendlinessChecker::Trace(Visitor* visitor) const {
  visitor->Trace(frame_view_);
}

}  // namespace blink
