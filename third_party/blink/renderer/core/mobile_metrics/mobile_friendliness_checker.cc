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
#include "third_party/blink/renderer/core/frame/viewport_data.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/layout/adjust_for_absolute_zoom.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_offset.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_size.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/viewport_description.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/graphics/paint/transform_paint_property_node.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "ui/display/screen_info.h"

namespace blink {

static constexpr int kSmallFontThresholdInDips = 9;
static constexpr int kTimeBudgetExceeded = -2;

// Values of maximum-scale smaller than this threshold will be considered to
// prevent the user from scaling the page as if user-scalable=no was set.
static constexpr double kMaximumScalePreventsZoomingThreshold = 1.2;

// Finding bad tap targets may costs too long time for big page and should abort
// if it takes more than 5ms.
static constexpr base::TimeDelta kTimeBudgetForBadTapTarget =
    base::Milliseconds(5);
// Extracting tap targets phase is the major part of finding bad tap targets.
// This phase will abort when it consumes more than 4ms.
static constexpr base::TimeDelta kTimeBudgetForTapTargetExtraction =
    base::Milliseconds(4);
static constexpr base::TimeDelta kEvaluationInterval = base::Minutes(1);

MobileFriendlinessChecker::MobileFriendlinessChecker(LocalFrameView& frame_view)
    : frame_view_(&frame_view),
      viewport_scalar_(
          frame_view_->GetFrame().GetWidgetForLocalRoot()
              ? frame_view_->GetPage()
                    ->GetChromeClient()
                    .WindowToViewportScalar(&frame_view_->GetFrame(), 1)
              : 1.0),
      last_evaluated_(base::TimeTicks::Now() - kEvaluationInterval -
                      base::Seconds(5)) {}

MobileFriendlinessChecker::~MobileFriendlinessChecker() = default;

void MobileFriendlinessChecker::NotifyPaintBegin() {
  DCHECK(frame_view_->GetFrame().Client()->IsLocalFrameClientImpl());
  DCHECK(frame_view_->GetFrame().IsOutermostMainFrame());

  ignore_beyond_viewport_scope_count_ =
      frame_view_->LayoutViewport()->MaximumScrollOffset().x() == 0 &&
      frame_view_->GetPage()
              ->GetVisualViewport()
              .MaximumScrollOffsetAtScale(initial_scale_)
              .x() == 0;
  is_painting_ = true;
  viewport_transform_ = &frame_view_->GetLayoutView()
                             ->FirstFragment()
                             .ContentsProperties()
                             .Transform();
  previous_transform_ = viewport_transform_;
  current_x_offset_ = 0.0;

  const ViewportDescription& viewport = frame_view_->GetFrame()
                                            .GetDocument()
                                            ->GetViewportData()
                                            .GetViewportDescription();
  if (viewport.type == ViewportDescription::Type::kViewportMeta) {
    const double zoom = viewport.zoom_is_explicit ? viewport.zoom : 1.0;
    viewport_device_width_ = viewport.max_width.IsDeviceWidth();
    if (viewport.max_width.IsFixed()) {
      viewport_hardcoded_width_ = viewport.max_width.GetFloatValue();
      // Convert value from Blink space to device-independent pixels.
      viewport_hardcoded_width_ /= viewport_scalar_;
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

  initial_scale_ = frame_view_->GetPage()
                       ->GetPageScaleConstraintsSet()
                       .FinalConstraints()
                       .initial_scale;
  int frame_width = frame_view_->GetPage()->GetVisualViewport().Size().width();
  viewport_width_ = frame_width * viewport_scalar_ / initial_scale_;
}

void MobileFriendlinessChecker::NotifyPaintEnd() {
  DCHECK(frame_view_->GetFrame().Client()->IsLocalFrameClientImpl());
  DCHECK(frame_view_->GetFrame().IsOutermostMainFrame());
  ignore_beyond_viewport_scope_count_ = 0;
  is_painting_ = false;
}

namespace {

bool IsTimeBudgetExpired(const base::TimeTicks& from) {
  return base::TimeTicks::Now() - from > kTimeBudgetForBadTapTarget;
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
  if (const LayoutBox* box = DynamicTo<LayoutBox>(object)) {
    const auto& rect = box->LocalVisualRect();
    if ((rect.Width() == LayoutUnit() &&
         style.OverflowX() != EOverflow::kVisible) ||
        (rect.Height() == LayoutUnit() &&
         style.OverflowY() != EOverflow::kVisible)) {
      return true;
    }
  }
  return object->IsElementContinuation() ||
         style.Visibility() != EVisibility::kVisible ||
         !style.IsContentVisibilityVisible();
}

void UnionAllChildren(const LayoutObject* parent, gfx::RectF& rect) {
  const LayoutObject* obj = parent;
  while (obj) {
    blink::GetRootNodeOptions options;
    if (obj->GetNode() &&
        obj->GetNode()->getRootNode(&options)->IsInUserAgentShadowRoot()) {
      obj = obj->NextInPreOrderAfterChildren(parent);
    } else if (ShouldSkipSubtree(obj)) {
      obj = obj->NextInPreOrderAfterChildren(parent);
    } else {
      if (auto* element = DynamicTo<HTMLElement>(obj->GetNode())) {
        rect.Union(element->GetBoundingClientRectNoLifecycleUpdate());
      }
      obj = obj->NextInPreOrder(parent);
    }
  }
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

  if (auto* element = DynamicTo<HTMLElement>(object->GetNode())) {
    // Ignore body tag even if it is a tappable element because majority of such
    // case does not mean "bad" tap target.
    if (element->IsHTMLBodyElement())
      return true;

    if (!tap_targets->insert(object).is_new_entry)
      return false;

    gfx::RectF rect = element->GetBoundingClientRectNoLifecycleUpdate();
    if (auto* anchor = DynamicTo<HTMLAnchorElement>(element))
      UnionAllChildren(object, rect);

    if (!rect.IsEmpty() && !isnan(rect.x()) && !isnan(rect.y()) &&
        !isnan(rect.right()) && !isnan(rect.bottom())) {
      // Expand each corner by the size of fingertips.
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
    const base::TimeTicks& started,
    Vector<std::pair<int, EdgeOrCenter>>& vertices) {
  LayoutObject* const root =
      frame_view.GetFrame().GetDocument()->GetLayoutView();
  WTF::HashSet<Member<const LayoutObject>> tap_targets;

  // Simultaneously iterate front-to-back and back-to-front to consider
  // both page headers and footers using the same time budget.
  for (const LayoutObject *forward = root, *backward = root;
       forward && backward;) {
    if (base::TimeTicks::Now() - started > kTimeBudgetForTapTargetExtraction)
      return static_cast<int>(tap_targets.size());

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
                       const base::TimeTicks& started) {
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

MobileFriendlinessChecker* MobileFriendlinessChecker::Create(
    LocalFrameView& frame_view) {
  // Only run the mobile friendliness checker for the outermost main
  // frame. The checker will iterate through all local frames in the
  // current blink::Page. Also skip the mobile friendliness checks for
  // "non-ordinary" pages by checking IsLocalFrameClientImpl(), since
  // it's not useful to generate mobile friendliness metrics for
  // devtools, svg, etc.
  if (!frame_view.GetFrame().Client()->IsLocalFrameClientImpl() ||
      !frame_view.GetFrame().IsOutermostMainFrame()) {
    return nullptr;
  }
  return MakeGarbageCollected<MobileFriendlinessChecker>(frame_view);
}

MobileFriendlinessChecker* MobileFriendlinessChecker::From(
    const Document& document) {
  DCHECK(document.GetFrame());

  auto* local_frame = DynamicTo<LocalFrame>(document.GetFrame()->Top());
  if (local_frame == nullptr)
    return nullptr;

  MobileFriendlinessChecker* mfc =
      local_frame->View()->GetMobileFriendlinessChecker();
  if (!mfc || !mfc->is_painting_)
    return nullptr;

  DCHECK_EQ(DocumentLifecycle::kInPaint, document.Lifecycle().GetState());
  DCHECK(!document.IsPrintingOrPaintingPreview());
  return mfc;
}

// Counts and calculate ration of bad tap targets. The process is a surface scan
// with region tracking by Fenwick tree. The detail of the algorithm is
// go/bad-tap-target-ukm
int MobileFriendlinessChecker::ComputeBadTapTargetsRatio() {
  DCHECK(frame_view_->GetFrame().IsOutermostMainFrame());
  base::TimeTicks started = base::TimeTicks::Now();
  constexpr float kOneDipInMm = 0.15875;

  const int finger_radius =
      std::floor((3 / kOneDipInMm) / initial_scale_);  // 3mm in logical pixel.

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

    if (base::TimeTicks::Now() - started > kTimeBudgetForTapTargetExtraction)
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

void MobileFriendlinessChecker::MaybeRecompute() {
  DCHECK(frame_view_->GetFrame().Client()->IsLocalFrameClientImpl());
  DCHECK(frame_view_->GetFrame().IsOutermostMainFrame());
  base::TimeTicks now = base::TimeTicks::Now();
  if (now - last_evaluated_ < kEvaluationInterval)
    return;

  ComputeNow();
}

void MobileFriendlinessChecker::ComputeNow() {
  frame_view_->DidChangeMobileFriendliness(MobileFriendliness{
      .viewport_device_width = viewport_device_width_,
      .viewport_initial_scale_x10 = viewport_initial_scale_x10_,
      .viewport_hardcoded_width = viewport_hardcoded_width_,
      .allow_user_zoom = allow_user_zoom_,
      .small_text_ratio = area_sizes_.SmallTextRatio(),
      .text_content_outside_viewport_percentage =
          area_sizes_.TextContentsOutsideViewportPercentage(
              // Use SizeF when computing the area to avoid integer overflow.
              gfx::SizeF(frame_view_->GetPage()->GetVisualViewport().Size())
                  .GetArea()),
      .bad_tap_targets_ratio = ComputeBadTapTargetsRatio()});

  last_evaluated_ = base::TimeTicks::Now();
}

int MobileFriendlinessChecker::AreaSizes::SmallTextRatio() const {
  if (total_text_area == 0)
    return 0;

  return small_font_area * 100 / total_text_area;
}

int MobileFriendlinessChecker::AreaSizes::TextContentsOutsideViewportPercentage(
    double viewport_area) const {
  return std::ceil(content_beyond_viewport_area * 100 / viewport_area);
}

void MobileFriendlinessChecker::UpdateTextAreaSizes(
    const PhysicalRect& text_rect,
    int font_size) {
  double actual_font_size = font_size * initial_scale_ / viewport_scalar_;
  double area = text_rect.Width() * text_rect.Height();
  if (std::round(actual_font_size) < kSmallFontThresholdInDips)
    area_sizes_.small_font_area += area;

  area_sizes_.total_text_area += area;
}

void MobileFriendlinessChecker::UpdateBeyondViewportAreaSizes(
    const PhysicalRect& paint_rect,
    const TransformPaintPropertyNodeOrAlias& current_transform) {
  DCHECK(is_painting_);
  if (ignore_beyond_viewport_scope_count_ != 0)
    return;

  if (previous_transform_ != &current_transform) {
    auto projection = GeometryMapper::SourceToDestinationProjection(
        current_transform, *viewport_transform_);
    if (projection.IsIdentityOr2DTranslation()) {
      current_x_offset_ = projection.Translation2D().x();
      previous_transform_ = &current_transform;
    } else {
      // For now we ignore offsets caused by non-2d-translation transforms.
      current_x_offset_ = 0;
    }
  }

  float right = paint_rect.Right() + current_x_offset_;
  float width = paint_rect.Width();
  float width_beyond_viewport =
      std::min(std::max(right - viewport_width_, 0.f), width);

  area_sizes_.content_beyond_viewport_area +=
      width_beyond_viewport * paint_rect.Height();
}

void MobileFriendlinessChecker::NotifyPaintTextFragment(
    const PhysicalRect& paint_rect,
    int font_size,
    const TransformPaintPropertyNodeOrAlias& current_transform) {
  DCHECK(frame_view_->GetFrame().Client()->IsLocalFrameClientImpl());
  DCHECK(frame_view_->GetFrame().IsOutermostMainFrame());

  UpdateTextAreaSizes(paint_rect, font_size);
  UpdateBeyondViewportAreaSizes(paint_rect, current_transform);
}

void MobileFriendlinessChecker::NotifyPaintReplaced(
    const PhysicalRect& paint_rect,
    const TransformPaintPropertyNodeOrAlias& current_transform) {
  DCHECK(frame_view_->GetFrame().Client()->IsLocalFrameClientImpl());
  DCHECK(frame_view_->GetFrame().IsLocalRoot());

  UpdateBeyondViewportAreaSizes(paint_rect, current_transform);
}

void MobileFriendlinessChecker::Trace(Visitor* visitor) const {
  visitor->Trace(frame_view_);
}

}  // namespace blink
