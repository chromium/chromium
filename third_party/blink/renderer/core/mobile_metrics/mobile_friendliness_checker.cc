// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/mobile_metrics/mobile_friendliness_checker.h"

#include <cmath>

#include "third_party/blink/public/common/mobile_metrics/mobile_friendliness.h"
#include "third_party/blink/public/mojom/mobile_metrics/mobile_friendliness.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_get_root_node_options.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/page_scale_constraints_set.h"
#include "third_party/blink/renderer/core/frame/root_frame_viewport.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/layout/adjust_for_absolute_zoom.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/viewport_description.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "ui/display/screen_info.h"

namespace blink {

static constexpr int kSmallFontThresholdInDips = 9;
static constexpr int kTimeBudgetExceeded = -2;

// Values of maximum-scale smaller than this threshold will be considered to
// prevent the user from scaling the page as if user-scalable=no was set.
static constexpr double kMaximumScalePreventsZoomingThreshold = 1.2;

// Finding bad tap targets may takes too time for big page and should abort if
// it takes more than 5ms.
static constexpr base::TimeDelta kTimeBudgetForBadTapTarget =
    base::Milliseconds(5);
// Extracting tap targets phase is the major part of finding bad tap targets.
static constexpr base::TimeDelta kTimeBudgetForTapTargetExtraction =
    base::Milliseconds(4);
// Checking clock itself is heavy on excessive call, skip checking by this
// stride.
constexpr int kTimeBudgetCheckStride = 32;
static constexpr base::TimeDelta kEvaluationDelay = base::Milliseconds(3000);
static constexpr base::TimeDelta kEvaluationInterval = base::Minutes(1);

MobileFriendlinessChecker::MobileFriendlinessChecker(LocalFrameView& frame_view)
    : frame_view_(&frame_view),
      timer_(frame_view_->GetFrame().GetTaskRunner(TaskType::kInternalDefault),
             this,
             &MobileFriendlinessChecker::Activate) {}

MobileFriendlinessChecker::~MobileFriendlinessChecker() = default;

void MobileFriendlinessChecker::NotifyPaint() {
  DCHECK(frame_view_->GetFrame().Client()->IsLocalFrameClientImpl());
  DCHECK(frame_view_->GetFrame().IsLocalRoot());
  if (timer_.IsActive() ||
      base::TimeTicks::Now() - last_evaluated_ < kEvaluationInterval) {
    return;
  }

  timer_.StartOneShot(kEvaluationDelay, FROM_HERE);
}

void MobileFriendlinessChecker::WillBeRemovedFromFrame() {
  timer_.Stop();
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
  explicit FenwickTree(wtf_size_t n) : tree(n + 1) {}

  // Returns prefix sum of the array from 0 to |index|.
  int sum(wtf_size_t index) const {
    int sum = 0;
    for (index += 1; 0 < index; index -= index & -index)
      sum += tree[index];
    return sum;
  }

  // Adds |val| at |index| of the array.
  void add(wtf_size_t index, int val) {
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

  union PositionOrIndexUnion {
    int position;
    wtf_size_t index;
  };

  union EdgeOrCenterUnion {
    // Valid iff |type| is Edge.
    struct Edge {
      PositionOrIndexUnion left;
      PositionOrIndexUnion right;
    } edge;

    // Valid iff |type| is Center.
    PositionOrIndexUnion center;
  } v;

  static EdgeOrCenter StartEdge(int left, int right) {
    EdgeOrCenter edge;
    edge.type = EdgeOrCenter::kStartEdge;
    edge.v.edge.left.position = left;
    edge.v.edge.right.position = right;
    return edge;
  }

  static EdgeOrCenter EndEdge(int left, int right) {
    EdgeOrCenter edge;
    edge.type = EdgeOrCenter::kEndEdge;
    edge.v.edge.left.position = left;
    edge.v.edge.right.position = right;
    return edge;
  }

  static EdgeOrCenter Center(int center) {
    EdgeOrCenter edge;
    edge.type = EdgeOrCenter::kCenter;
    edge.v.center.position = center;
    return edge;
  }
};

bool IsTapTargetCandidate(Node* node) {
  if (const auto* anchor = DynamicTo<HTMLAnchorElement>(node)) {
    return !anchor->Href().IsEmpty();
  } else if (auto* element = DynamicTo<HTMLElement>(node);
             element && element->WillRespondToMouseClickEvents()) {
    return true;
  }
  return IsA<HTMLFormControlElement>(node);
}

// Skip the whole subtree if the object is invisible. Some elements in subtree
// may have visibility: visible property which should not be ignored for
// correctness, but it is rare and we prioritize performance.
bool ShouldSkipSubtree(const LayoutObject* object) {
  const auto& style = object->StyleRef();
  return object->IsElementContinuation() ||
         style.Visibility() != EVisibility::kVisible ||
         !style.IsContentVisibilityVisible();
}

// Appends |object| to evaluation targets if the object is a tap target.
// Returns false only if |object| is already inserted.
bool AddElement(const LayoutObject* object,
                WTF::HashSet<Member<const LayoutObject>>* tap_targets,
                int finger_radius,
                Vector<int>& x_positions,
                Vector<std::pair<int, EdgeOrCenter>>& vertices) {
  Node* node = object->GetNode();
  if (!node || !IsTapTargetCandidate(node))
    return true;

  if (Element* element = DynamicTo<Element>(object->GetNode())) {
    // Expand each corner by the size of fingertips.
    const gfx::RectF rect = element->GetBoundingClientRectNoLifecycleUpdate();
    if (!tap_targets->insert(object).is_new_entry)
      return false;

    if (!rect.IsEmpty()) {
      const int top = ClampTo<int>(rect.y() - finger_radius);
      const int bottom = ClampTo<int>(rect.bottom() + finger_radius);
      const int left = ClampTo<int>(rect.x() - finger_radius);
      const int right = ClampTo<int>(rect.right() + finger_radius);
      const int center = right / 2 + left / 2;
      vertices.emplace_back(top, EdgeOrCenter::StartEdge(left, right));
      vertices.emplace_back(bottom / 2 + top / 2, EdgeOrCenter::Center(center));
      vertices.emplace_back(bottom, EdgeOrCenter::EndEdge(left, right));
      x_positions.push_back(left);
      x_positions.push_back(right);
      x_positions.push_back(center);
    }
  }
  return true;
}

// Scans full DOM tree and register all tap regions.
// frame_view: DOM tree's root.
// finger_radius: Extends every tap regions with given pixels.
// x_positions: Collects and inserts every x dimension positions.
// vertices: Inserts y dimension keyed vertex positions with its attribute.
// Returns total count of tap targets.
int ExtractAndCountAllTapTargets(
    const LocalFrameView& frame_view,
    int finger_radius,
    Vector<int>& x_positions,
    const base::Time& started,
    Vector<std::pair<int, EdgeOrCenter>>& vertices) {
  LayoutObject* const root =
      frame_view.GetFrame().GetDocument()->GetLayoutView();
  WTF::HashSet<Member<const LayoutObject>> tap_targets;

  int object_count = 0;
  // Simultaneously iterate front-to-back and back-to-front to consider
  // both page headers and footers using the same time budget.
  for (const LayoutObject *forward = root, *backward = root;
       forward && backward;) {
    if ((++object_count % kTimeBudgetCheckStride) == 0 &&
        base::Time::Now() - started > kTimeBudgetForTapTargetExtraction) {
      return static_cast<int>(tap_targets.size());
    }

    blink::GetRootNodeOptions options;
    if (forward->GetNode() != nullptr &&
        forward->GetNode()->getRootNode(&options)->IsInUserAgentShadowRoot()) {
      // Ignore shadow elements that may contain overlapping tap targets.
      forward = forward->NextInPreOrderAfterChildren();
    } else if (ShouldSkipSubtree(forward)) {
      forward = forward->NextInPreOrderAfterChildren();
    } else {
      if (!AddElement(forward, &tap_targets, finger_radius, x_positions,
                      vertices)) {
        break;
      }

      forward = forward->NextInPreOrder();
    }

    if (backward->GetNode() != nullptr &&
        backward->GetNode()->getRootNode(&options)->IsInUserAgentShadowRoot()) {
      // Ignore shadow elements that may contain overlapping tap targets.
      backward = backward->PreviousInPostOrderBeforeChildren(nullptr);
    } else if (ShouldSkipSubtree(backward)) {
      backward = backward->PreviousInPostOrderBeforeChildren(nullptr);
    } else {
      if (!AddElement(backward, &tap_targets, finger_radius, x_positions,
                      vertices)) {
        break;
      }

      backward = backward->PreviousInPostOrder(nullptr);
    }
  }

  return static_cast<int>(tap_targets.size());
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
        vertex.v.edge.left.index = static_cast<wtf_size_t>(
            std::distance(positions.begin(),
                          std::lower_bound(positions.begin(), positions.end(),
                                           vertex.v.edge.left.position)));
        vertex.v.edge.right.index = static_cast<wtf_size_t>(
            std::distance(positions.begin(),
                          std::lower_bound(positions.begin(), positions.end(),
                                           vertex.v.edge.right.position)));
        break;
      }
      case EdgeOrCenter::kCenter: {
        vertex.v.center.index = static_cast<wtf_size_t>(
            std::distance(positions.begin(),
                          std::lower_bound(positions.begin(), positions.end(),
                                           vertex.v.center.position)));
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
// Returns kTimeBudgetExceeded if time limit exceeded.
int CountBadTapTargets(wtf_size_t rightmost_position,
                       const Vector<std::pair<int, EdgeOrCenter>>& vertices,
                       const base::Time& started) {
  FenwickTree tree(rightmost_position);
  int bad_tap_targets = 0;
  for (const auto& it : vertices) {
    const EdgeOrCenter& vertex = it.second;
    switch (vertex.type) {
      case EdgeOrCenter::kStartEdge: {
        // Tap region begins.
        tree.add(vertex.v.edge.left.index, 1);
        tree.add(vertex.v.edge.right.index, -1);
        break;
      }
      case EdgeOrCenter::kEndEdge: {
        // Tap region ends.
        tree.add(vertex.v.edge.left.index, -1);
        tree.add(vertex.v.edge.right.index, 1);
        break;
      }
      case EdgeOrCenter::kCenter: {
        // Iff the center of a tap target is included other than itself, it is a
        // Bad Target.
        if (tree.sum(vertex.v.center.index) > 1)
          bad_tap_targets++;
        break;
      }
    }
    if (IsTimeBudgetExpired(started))
      return kTimeBudgetExceeded;
  }
  return bad_tap_targets;
}

}  // namespace

// Counts and calculate ration of bad tap targets. The process is a surface scan
// with region tracking by Fenwick tree. The detail of the algorithm is
// go/bad-tap-target-ukm
int MobileFriendlinessChecker::ComputeBadTapTargetsRatio() {
  DCHECK(frame_view_->GetFrame().IsLocalRoot());
  base::Time started = base::Time::Now();
  constexpr float kOneDipInMm = 0.15875;
  double initial_scale = frame_view_->GetPage()
                             ->GetPageScaleConstraintsSet()
                             .FinalConstraints()
                             .initial_scale;
  DCHECK_GT(initial_scale, 0);

  const int finger_radius =
      std::floor((3 / kOneDipInMm) / initial_scale);  // 3mm in logical pixel.

  Vector<std::pair<int, EdgeOrCenter>> vertices;
  vertices.ReserveInitialCapacity(1024);
  Vector<int> x_positions;
  x_positions.ReserveInitialCapacity(1024);

  // Recursively evaluate MF values into subframes.
  int all_tap_targets = 0;
  for (const Frame* frame = &frame_view_->GetFrame(); frame;
       frame = frame->Tree().TraverseNext()) {
    const auto* local_frame = DynamicTo<LocalFrame>(frame);
    if (!local_frame)
      continue;

    const LocalFrameView* view = local_frame->View();

    // Scan full DOM tree and extract every corner and center position of tap
    // targets.
    const int got_tap_targets = ExtractAndCountAllTapTargets(
        *view, finger_radius, x_positions, started, vertices);

    all_tap_targets += got_tap_targets;

    if (base::Time::Now() - started > kTimeBudgetForTapTargetExtraction)
      break;
  }
  if (all_tap_targets == 0)
    return 0;  // Means there is no tap target.

  // Compress x dimension of all vertices to save memory.
  // This will reduce rightmost position of vertices without sacrificing
  // accuracy so that required memory by Fenwick Tree will be reduced.
  std::sort(x_positions.begin(), x_positions.end());
  x_positions.erase(std::unique(x_positions.begin(), x_positions.end()),
                    x_positions.end());
  CompressKeyWithVector(x_positions, vertices);
  if (IsTimeBudgetExpired(started))
    return kTimeBudgetExceeded;

  // Reorder vertices by y dimension for sweeping full page from top to bottom.
  std::sort(vertices.begin(), vertices.end(),
            [](const std::pair<int, EdgeOrCenter>& a,
               const std::pair<int, EdgeOrCenter>& b) {
              // Ordering with kStart < kCenter < kEnd.
              return std::tie(a.first, a.second.type) <
                     std::tie(b.first, b.second.type);
            });
  if (IsTimeBudgetExpired(started))
    return kTimeBudgetExceeded;

  // Sweep x-compressed y-ordered vertices to detect bad tap targets.
  const int bad_tap_targets =
      CountBadTapTargets(x_positions.size(), vertices, started);
  if (bad_tap_targets == kTimeBudgetExceeded)
    return kTimeBudgetExceeded;

  return std::ceil(bad_tap_targets * 100.0 / all_tap_targets);
}

void MobileFriendlinessChecker::Activate(TimerBase*) {
  DCHECK(frame_view_->GetFrame().Client()->IsLocalFrameClientImpl());

  // If detached, there's no need to calculate any metrics.
  if (!frame_view_->GetChromeClient())
    return;

  frame_view_->RegisterForLifecycleNotifications(this);
  frame_view_->ScheduleAnimation();
}

void MobileFriendlinessChecker::DidFinishLifecycleUpdate(
    const LocalFrameView&) {
  DCHECK(frame_view_->GetFrame().Client()->IsLocalFrameClientImpl());
  DCHECK(frame_view_->GetFrame().IsLocalRoot());

  frame_view_->UnregisterFromLifecycleNotifications(this);
  frame_view_->DidChangeMobileFriendliness(MobileFriendliness{
      .viewport_device_width = viewport_device_width_,
      .viewport_initial_scale_x10 = viewport_initial_scale_x10_,
      .viewport_hardcoded_width = viewport_hardcoded_width_,
      .allow_user_zoom = allow_user_zoom_,
      .small_text_ratio = text_area_sizes_.SmallTextRatio(),
      .text_content_outside_viewport_percentage =
          ComputeContentOutsideViewport(),
      .bad_tap_targets_ratio = ComputeBadTapTargetsRatio()});
  last_evaluated_ = base::TimeTicks::Now();
}

void MobileFriendlinessChecker::NotifyViewportUpdated(
    const ViewportDescription& viewport) {
  DCHECK(frame_view_->GetFrame().Client()->IsLocalFrameClientImpl());
  DCHECK(frame_view_->GetFrame().IsLocalRoot());

  if (viewport.type != ViewportDescription::Type::kViewportMeta)
    return;

  const double zoom = viewport.zoom_is_explicit ? viewport.zoom : 1.0;
  viewport_device_width_ = viewport.max_width.IsDeviceWidth();
  if (viewport.max_width.IsFixed()) {
    viewport_hardcoded_width_ = viewport.max_width.GetFloatValue();
    // Convert value from Blink space to device-independent pixels.
    const double viewport_scalar =
        frame_view_->GetPage()->GetChromeClient().WindowToViewportScalar(
            &frame_view_->GetFrame(), 1);
    if (viewport_scalar != 0)
      viewport_hardcoded_width_ /= viewport_scalar;
  }

  if (viewport.zoom_is_explicit)
    viewport_initial_scale_x10_ = std::round(viewport.zoom * 10);

  if (viewport.user_zoom_is_explicit) {
    allow_user_zoom_ = viewport.user_zoom;
    // If zooming is only allowed slightly.
    if (viewport.max_zoom / zoom < kMaximumScalePreventsZoomingThreshold)
      allow_user_zoom_ = false;
  }
}

int MobileFriendlinessChecker::TextAreaWithFontSize::SmallTextRatio() const {
  if (total_text_area == 0)
    return 0;

  return small_font_area * 100 / total_text_area;
}

void MobileFriendlinessChecker::NotifyInvalidatePaint(
    const LayoutObject& object) {
  DCHECK(frame_view_->GetFrame().Client()->IsLocalFrameClientImpl());
  DCHECK(frame_view_->GetFrame().IsLocalRoot());

  // Compute small text ratio.
  if (const auto* text = DynamicTo<LayoutText>(object)) {
    const auto& style = text->StyleRef();

    // Ignore elements that users cannot see.
    if (style.Visibility() != EVisibility::kVisible)
      return;

    // Ignore elements intended only for screen readers.
    if (style.HasOutOfFlowPosition() && style.ClipLeft().IsZero() &&
        style.ClipRight().IsZero() && style.ClipTop().IsZero() &&
        style.ClipBottom().IsZero())
      return;

    const double viewport_scalar =
        frame_view_->GetPage()->GetChromeClient().WindowToViewportScalar(
            &frame_view_->GetFrame(), 1);

    double initial_scale = frame_view_->GetPage()
                               ->GetPageScaleConstraintsSet()
                               .FinalConstraints()
                               .initial_scale;
    DCHECK_GT(initial_scale, 0);

    double actual_font_size =
        style.FontSize() * initial_scale / viewport_scalar;
    double area = text->PhysicalAreaSize();
    if (std::round(actual_font_size) < kSmallFontThresholdInDips)
      text_area_sizes_.small_font_area += area;

    text_area_sizes_.total_text_area += area;
  }
}

int MobileFriendlinessChecker::ComputeContentOutsideViewport() {
  int frame_width = frame_view_->GetPage()->GetVisualViewport().Size().width();
  if (frame_width == 0) {
    return 0;
  }

  const auto* root_frame_viewport = frame_view_->GetRootFrameViewport();
  if (root_frame_viewport == nullptr) {
    return 0;
  }

  double initial_scale = frame_view_->GetPage()
                             ->GetPageScaleConstraintsSet()
                             .FinalConstraints()
                             .initial_scale;
  int content_width =
      root_frame_viewport->LayoutViewport().ContentsSize().width() *
      initial_scale;
  int max_scroll_offset = content_width - frame_width;

  // We use ceil function here because we want to treat 100.1% as 101 which
  // requires a scroll bar.
  return std::ceil(max_scroll_offset * 100.0 / frame_width);
}

void MobileFriendlinessChecker::Trace(Visitor* visitor) const {
  visitor->Trace(frame_view_);
  visitor->Trace(timer_);
}

}  // namespace blink
