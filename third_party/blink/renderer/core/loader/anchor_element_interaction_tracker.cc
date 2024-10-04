// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/anchor_element_interaction_tracker.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/web_mouse_wheel_event.h"
#include "third_party/blink/public/common/input/web_pointer_properties.h"
#include "third_party/blink/public/mojom/preloading/anchor_element_interaction_host.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/pointer_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/screen.h"
#include "third_party/blink/renderer/core/html/anchor_element_metrics.h"
#include "third_party/blink/renderer/core/html/anchor_element_metrics_sender.h"
#include "third_party/blink/renderer/core/html/anchor_element_viewport_position_tracker.h"
#include "third_party/blink/renderer/core/pointer_type_names.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

namespace {
constexpr double eps = 1e-9;
const base::TimeDelta kMousePosQueueTimeDelta{base::Milliseconds(500)};
const base::TimeDelta kMouseAccelerationAndVelocityInterval{
    base::Milliseconds(50)};
}  // namespace

AnchorElementInteractionTracker::MouseMotionEstimator::MouseMotionEstimator(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : update_timer_(
          task_runner,
          this,
          &AnchorElementInteractionTracker::MouseMotionEstimator::OnTimer),
      clock_(base::DefaultTickClock::GetInstance()) {
  CHECK(clock_);
}

void AnchorElementInteractionTracker::MouseMotionEstimator::Trace(
    Visitor* visitor) const {
  visitor->Trace(update_timer_);
}

double AnchorElementInteractionTracker::MouseMotionEstimator::
    GetMouseTangentialAcceleration() const {
  // Tangential acceleration = (a.v)/|v|
  return DotProduct(acceleration_, velocity_) /
         std::max(static_cast<double>(velocity_.Length()), eps);
}

inline void AnchorElementInteractionTracker::MouseMotionEstimator::AddDataPoint(
    base::TimeTicks timestamp,
    gfx::PointF position) {
  mouse_position_and_timestamps_.push_front(
      MousePositionAndTimeStamp{.position = position, .ts = timestamp});
}
inline void
AnchorElementInteractionTracker::MouseMotionEstimator::RemoveOldDataPoints(
    base::TimeTicks now) {
  while (!mouse_position_and_timestamps_.empty() &&
         (now - mouse_position_and_timestamps_.back().ts) >
             kMousePosQueueTimeDelta) {
    mouse_position_and_timestamps_.pop_back();
  }
}

void AnchorElementInteractionTracker::MouseMotionEstimator::Update() {
  // Bases on the mouse position/timestamp data
  // (ts0,ts1,ts2,...),(px0,px1,px2,...),(py0,py1,py2,...), we like to find
  // acceleration (ax, ay) and velocity (vx,vy) values that best fit following
  // set of equations:
  // {px1 = 0.5*ax*(ts1-ts0)**2 + vx0*(ts1-ts0) + px0},
  // {py1 = 0.5*ay*(ts1-ts0)**2 + vy0*(ts1-ts0) + py0},
  // {px2 = 0.5*ax*(ts2-ts0)**2 + vx0*(ts2-ts0) + px0},
  // {py2 = 0.5*ay*(ts2-ts0)**2 + vy0*(ts2-ts0) + py0},
  // ...
  // It can be solved using least squares linear regression by computing metrics
  // A (2x2), X (2x1), and Y (2x1) where:
  // a11 = 0.25*[(ts1-ts0)**4+(ts2-ts0)**4+...]
  // a12 = a21 = 0.5*[(ts1-ts0)**3+(ts2-ts0)**3+...]
  // a22 = (ts1-ts0)**2+(ts2-ts0)**2+...
  // x1 = 0.5*(px1-px0)*(ts1-ts0)**2+0.5*(px2-px0)*(ts2-ts0)**2+...
  // x2 = (px1-px0)*(ts1-ts0)+(px2-px0)*(ts2-ts0)+...
  // y1 = 0.5*(py1-py0)*(ts1-ts0)**2+0.5*(py2-py0)*(ts2-ts0)**2+...
  // y2 = (py1-py0)*(ts1-ts0)+(py2-py0)*(ts2-ts0)+...
  // and the solution is:
  // | ax  ay |       | a11 a12 |   | x1 y1 |
  // | vx0 vy0| = inv(| a12 a22 |)* | x2 y2 |
  // At the end the latest velocity is:
  // vx = ax*(ts-ts0) + vx0
  // vy = ay*(ts-ts0) + vy0

  // Since, we use `(ts-ts0)**4` to construct the matrix A, measuring the time
  // in seconds will cause rounding errors and make the numerical solution
  // unstable. Therefore, we'll use milli-seconds for time measurement and then
  // we rescale the acceleration/velocity estimates at the end.
  constexpr double kRescaleVelocity = 1e3;
  constexpr double kRescaleAcceleration = 1e6;

  // We need at least 2 data points to compute the acceleration and velocity.
  if (mouse_position_and_timestamps_.size() <= 1u) {
    acceleration_ = {0.0, 0.0};
    velocity_ = {0.0, 0.0};
    return;
  }
  auto back = mouse_position_and_timestamps_.back();
  auto front = mouse_position_and_timestamps_.front();
  auto replace_zero_with_eps = [](double x) {
    return x >= 0.0 ? std::max(x, eps) : std::min(x, -eps);
  };
  // With 2 data points, we could assume acceleration is zero and just estimate
  // the velocity.
  if (mouse_position_and_timestamps_.size() == 2u) {
    acceleration_ = {0.0, 0.0};
    velocity_ = front.position - back.position;
    velocity_.InvScale(
        replace_zero_with_eps((front.ts - back.ts).InSecondsF()));
    return;
  }
  // with 3 or more data points, we can use the above mentioned linear
  // regression approach.
  double a11 = 0, a12 = 0, a22 = 0;
  double x1 = 0, x2 = 0;
  double y1 = 0, y2 = 0;
  for (wtf_size_t i = 0; i < mouse_position_and_timestamps_.size() - 1; i++) {
    const auto& mouse_data = mouse_position_and_timestamps_.at(i);
    double t = (mouse_data.ts - back.ts).InMilliseconds();
    double t_square = t * t;
    double t_cube = t * t_square;
    double t_quad = t * t_cube;
    double px = mouse_data.position.x() - back.position.x();
    double py = mouse_data.position.y() - back.position.y();
    a11 += t_quad;
    a12 += t_cube;
    a22 += t_square;
    x1 += px * t_square;
    x2 += px * t;
    y1 += py * t_square;
    y2 += py * t;
  }
  a11 *= 0.25;
  a12 *= 0.5;
  x1 *= 0.5;
  y1 *= 0.5;

  double determinant = replace_zero_with_eps(a11 * a22 - a12 * a12);
  acceleration_.set_x(kRescaleAcceleration * (a22 * x1 - a12 * x2) /
                      determinant);
  velocity_.set_x(kRescaleVelocity * (-a12 * x1 + a11 * x2) / determinant +
                  acceleration_.x() * (front.ts - back.ts).InSecondsF());

  acceleration_.set_y(kRescaleAcceleration * (a22 * y1 - a12 * y2) /
                      determinant);
  velocity_.set_y(kRescaleVelocity * (-a12 * y1 + a11 * y2) / determinant +
                  acceleration_.y() * (front.ts - back.ts).InSecondsF());
}

void AnchorElementInteractionTracker::MouseMotionEstimator::OnTimer(
    TimerBase*) {
  RemoveOldDataPoints(clock_->NowTicks());
  Update();
  if (IsEmpty()) {
    // If there are no new mouse movements for more than
    // `kMousePosQueueTimeDelta`, the `mouse_position_and_timestamps_` will be
    // empty. Returning without firing `update_timer_`
    // will prevent us from perpetually firing the timer event.
    return;
  }
  update_timer_.StartOneShot(kMouseAccelerationAndVelocityInterval, FROM_HERE);
}

void AnchorElementInteractionTracker::MouseMotionEstimator::OnMouseMoveEvent(
    gfx::PointF position) {
  AddDataPoint(clock_->NowTicks(), position);
  if (update_timer_.IsActive()) {
    update_timer_.Stop();
  }
  OnTimer(&update_timer_);
}

void AnchorElementInteractionTracker::MouseMotionEstimator::
    SetTaskRunnerForTesting(
        scoped_refptr<base::SingleThreadTaskRunner> task_runner,
        const base::TickClock* clock) {
  update_timer_.SetTaskRunnerForTesting(task_runner, clock);
  clock_ = clock;
}

AnchorElementInteractionTracker::AnchorElementInteractionTracker(
    Document& document)
    : mouse_motion_estimator_(MakeGarbageCollected<MouseMotionEstimator>(
          document.GetTaskRunner(TaskType::kUserInteraction))),
      interaction_host_(document.GetExecutionContext()),
      hover_timer_(document.GetTaskRunner(TaskType::kUserInteraction),
                   this,
                   &AnchorElementInteractionTracker::HoverTimerFired),
      clock_(base::DefaultTickClock::GetInstance()),
      document_(&document) {
  document.GetFrame()->GetBrowserInterfaceBroker().GetInterface(
      interaction_host_.BindNewPipeAndPassReceiver(
          document.GetExecutionContext()->GetTaskRunner(
              TaskType::kInternalDefault)));
}

AnchorElementInteractionTracker::~AnchorElementInteractionTracker() = default;

void AnchorElementInteractionTracker::Trace(Visitor* visitor) const {
  visitor->Trace(interaction_host_);
  visitor->Trace(hover_timer_);
  visitor->Trace(mouse_motion_estimator_);
  visitor->Trace(document_);
}

// static
base::TimeDelta AnchorElementInteractionTracker::GetHoverDwellTime() {
  static base::FeatureParam<base::TimeDelta> hover_dwell_time{
      &blink::features::kSpeculationRulesPointerHoverHeuristics,
      "HoverDwellTime", base::Milliseconds(200)};
  return hover_dwell_time.Get();
}

void AnchorElementInteractionTracker::OnMouseMoveEvent(
    const WebMouseEvent& mouse_event) {
  mouse_motion_estimator_->OnMouseMoveEvent(mouse_event.PositionInScreen());
}

void AnchorElementInteractionTracker::OnPointerEvent(
    EventTarget& target,
    const PointerEvent& pointer_event) {
  if (!target.ToNode()) {
    return;
  }
  if (!pointer_event.isPrimary()) {
    return;
  }

  const AtomicString& event_type = pointer_event.type();

  if (event_type == event_type_names::kPointerdown) {
    last_pointer_down_locations_[1] = last_pointer_down_locations_[0];
    last_pointer_down_locations_[0] = pointer_event.screenY();

    if (auto* viewport_position_tracker =
            AnchorElementViewportPositionTracker::MaybeGetOrCreateFor(
                *GetDocument())) {
      viewport_position_tracker->RecordPointerDown(pointer_event);
    }
  }

  HTMLAnchorElementBase* anchor =
      FirstAnchorElementIncludingSelf(target.ToNode());
  if (!anchor) {
    return;
  }
  KURL url = GetHrefEligibleForPreloading(*anchor);
  if (url.IsEmpty()) {
    return;
  }

  if (auto* sender =
          AnchorElementMetricsSender::GetForFrame(GetDocument()->GetFrame())) {
    sender->MaybeReportAnchorElementPointerEvent(*anchor, pointer_event);
  }

  // interaction_host_ might become unbound: Android's low memory detector
  // sometimes call NotifyContextDestroyed to save memory. This unbinds mojo
  // pipes using that ExecutionContext even if those pages can still navigate.
  if (!interaction_host_.is_bound()) {
    return;
  }

  if (event_type == event_type_names::kPointerdown) {
    // TODO(crbug.com/1297312): Check if user changed the default mouse
    // settings
    if (pointer_event.button() !=
            static_cast<int>(WebPointerProperties::Button::kLeft) &&
        pointer_event.button() !=
            static_cast<int>(WebPointerProperties::Button::kMiddle)) {
      return;
    }
    interaction_host_->OnPointerDown(url);
    return;
  }

  if (event_type == event_type_names::kPointerover) {
    hover_event_candidates_.insert(
        url, HoverEventCandidate{
                 .is_mouse =
                     pointer_event.pointerType() == pointer_type_names::kMouse,
                 .anchor_id = AnchorElementId(*anchor),
                 .timestamp = clock_->NowTicks() + GetHoverDwellTime()});
    if (!hover_timer_.IsActive()) {
      hover_timer_.StartOneShot(GetHoverDwellTime(), FROM_HERE);
    }
  } else if (event_type == event_type_names::kPointerout) {
    // Since the pointer is no longer hovering on the link, there is no need to
    // check the timer. We should just remove it here.
    hover_event_candidates_.erase(url);
  }
}

void AnchorElementInteractionTracker::OnClickEvent(
    HTMLAnchorElementBase& anchor,
    const MouseEvent& click_event) {
  if (auto* sender =
          AnchorElementMetricsSender::GetForFrame(GetDocument()->GetFrame())) {
    sender->MaybeReportClickedMetricsOnClick(anchor);
  }

  LocalFrame* frame = anchor.GetDocument().GetFrame();
  if (!frame->IsMainFrame() || !frame->View()) {
    return;
  }

  Screen* screen = frame->DomWindow()->screen();
  const int screen_height = screen->height();
  if (screen_height == 0) {
    return;
  }

  const char* orientation_pattern =
      screen->width() <= screen_height ? ".Portrait" : ".Landscape";
  double click_y = click_event.screenY();
  int normalized_click_y = base::ClampRound(100.0f * (click_y / screen_height));
  base::UmaHistogramPercentage(
      base::StrCat({"Blink.AnchorElementInteractionTracker.ClickLocationY",
                    orientation_pattern}),
      normalized_click_y);

  if (last_pointer_down_locations_[1]) {
    double click_distance = click_y - last_pointer_down_locations_[1].value();
    // Because a click could happen both above and below the previous pointer
    // down, |click_distance| can be any value between -|screen_height| and
    // |screen_height| inclusive. We shift and scale it to be values between 0
    // and 100 before recording to UMA.
    int shifted_normalized_click_distance =
        base::ClampRound(50.0f * (1 + click_distance / screen_height));
    base::UmaHistogramPercentage(
        base::StrCat({"Blink.AnchorElementInteractionTracker"
                      ".ClickDistanceFromPreviousPointerDown",
                      orientation_pattern}),
        shifted_normalized_click_distance);
  }
}

void AnchorElementInteractionTracker::HoverTimerFired(TimerBase*) {
  if (!interaction_host_.is_bound()) {
    return;
  }
  const base::TimeTicks now = clock_->NowTicks();
  auto next_fire_time = base::TimeTicks::Max();
  Vector<KURL> to_be_erased;
  for (const auto& hover_event_candidate : hover_event_candidates_) {
    // Check whether pointer hovered long enough on the link to send the
    // PointerHover event to interaction host.
    if (now >= hover_event_candidate.value.timestamp) {
      auto pointer_data = mojom::blink::AnchorElementPointerData::New(
          /*is_mouse_pointer=*/hover_event_candidate.value.is_mouse,
          /*mouse_velocity=*/
          mouse_motion_estimator_->GetMouseVelocity().Length(),
          /*mouse_acceleration=*/
          mouse_motion_estimator_->GetMouseTangentialAcceleration());

      if (hover_event_candidate.value.is_mouse) {
        if (auto* sender = AnchorElementMetricsSender::GetForFrame(
                GetDocument()->GetFrame())) {
          sender->MaybeReportAnchorElementPointerDataOnHoverTimerFired(
              hover_event_candidate.value.anchor_id, pointer_data->Clone());
        }
      }

      interaction_host_->OnPointerHover(
          /*target=*/hover_event_candidate.key, std::move(pointer_data));
      to_be_erased.push_back(hover_event_candidate.key);

      continue;
    }
    // Update next fire time
    next_fire_time =
        std::min(next_fire_time, hover_event_candidate.value.timestamp);
  }
  WTF::RemoveAll(hover_event_candidates_, to_be_erased);
  if (!next_fire_time.is_max()) {
    hover_timer_.StartOneShot(next_fire_time - now, FROM_HERE);
  }
}

void AnchorElementInteractionTracker::SetTaskRunnerForTesting(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const base::TickClock* clock) {
  hover_timer_.SetTaskRunnerForTesting(task_runner, clock);
  mouse_motion_estimator_->SetTaskRunnerForTesting(task_runner, clock);
  clock_ = clock;
}

HTMLAnchorElementBase*
AnchorElementInteractionTracker::FirstAnchorElementIncludingSelf(Node* node) {
  HTMLAnchorElementBase* anchor = nullptr;
  while (node && !anchor) {
    anchor = DynamicTo<HTMLAnchorElementBase>(node);
    node = node->parentNode();
  }
  return anchor;
}

KURL AnchorElementInteractionTracker::GetHrefEligibleForPreloading(
    const HTMLAnchorElementBase& anchor) {
  KURL url = anchor.Href();
  if (url.ProtocolIsInHTTPFamily()) {
    return url;
  }
  return KURL();
}

}  // namespace blink
