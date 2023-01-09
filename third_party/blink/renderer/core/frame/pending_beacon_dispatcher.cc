// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/pending_beacon_dispatcher.h"

#include <algorithm>
#include <functional>

#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/page/page.h"

namespace blink {
namespace {

// Bundles beacons and sends them out to reduce the number of timer callback
// triggered. A bundle has beacons fall within the same 100x milliseconds.
// Spec says: The beacon is not guaranteed to be sent at exactly this many
// milliseconds after hidden; bundling/batching of beacons is possible.
// https://github.com/WICG/pending-beacon/blob/main/README.md#properties
constexpr base::TimeDelta kBeaconTimeoutInterval = base::Milliseconds(100);

struct ReverseBeaconTimeoutSorter {
  bool operator()(const Member<PendingBeaconDispatcher::PendingBeacon>& lhs,
                  const Member<PendingBeaconDispatcher::PendingBeacon>& rhs) {
    // Negative timeout is not accepted.
    DCHECK(!lhs->GetBackgroundTimeout().is_negative());
    DCHECK(!rhs->GetBackgroundTimeout().is_negative());
    return lhs->GetBackgroundTimeout() > rhs->GetBackgroundTimeout();
  }
};

}  // namespace

void PendingBeaconDispatcher::PendingBeacon::UnregisterFromDispatcher() {
  auto* ec = GetExecutionContext();
  DCHECK(ec);
  auto* dispatcher = PendingBeaconDispatcher::From(*ec);
  DCHECK(dispatcher);
  dispatcher->Unregister(this);
}

// static
const char PendingBeaconDispatcher::kSupplementName[] =
    "PendingBeaconDispatcher";

// static
PendingBeaconDispatcher& PendingBeaconDispatcher::FromOrAttachTo(
    ExecutionContext& ec) {
  PendingBeaconDispatcher* dispatcher =
      Supplement::From<PendingBeaconDispatcher>(ec);
  if (!dispatcher) {
    dispatcher = MakeGarbageCollected<PendingBeaconDispatcher>(
        ec, base::PassKey<PendingBeaconDispatcher>());
    ProvideTo(ec, dispatcher);
  }
  return *dispatcher;
}

// static
PendingBeaconDispatcher* PendingBeaconDispatcher::From(ExecutionContext& ec) {
  return Supplement::From<PendingBeaconDispatcher>(ec);
}

PendingBeaconDispatcher::PendingBeaconDispatcher(
    ExecutionContext& ec,
    base::PassKey<PendingBeaconDispatcher> key)
    : Supplement(ec),
      ExecutionContextLifecycleObserver(&ec),
      PageVisibilityObserver(DomWindow() ? DomWindow()->GetFrame()->GetPage()
                                         : nullptr),
      remote_(&ec) {
  auto task_runner = ec.GetTaskRunner(kTaskType);

  mojo::PendingReceiver<mojom::blink::PendingBeaconHost> host_receiver =
      remote_.BindNewPipeAndPassReceiver(task_runner);
  ec.GetBrowserInterfaceBroker().GetInterface(std::move(host_receiver));
}

void PendingBeaconDispatcher::CreateHostBeacon(
    PendingBeacon* pending_beacon,
    mojo::PendingReceiver<mojom::blink::PendingBeacon> receiver,
    const KURL& url,
    mojom::blink::BeaconMethod method) {
  DCHECK(!pending_beacons_.Contains(pending_beacon));
  pending_beacons_.insert(pending_beacon);

  remote_->CreateBeacon(std::move(receiver), url, method);
}

void PendingBeaconDispatcher::Unregister(PendingBeacon* pending_beacon) {
  pending_beacons_.erase(pending_beacon);
}

void PendingBeaconDispatcher::ContextDestroyed() {
  // Cancels all pending tasks when the Document is destroyed.
  // The browser will take over the responsibility.
  CancelDispatchBeacons();

  pending_beacons_.clear();
}

void PendingBeaconDispatcher::PageVisibilityChanged() {
  DCHECK(GetPage());

  // Handles a PendingBeacon's `backgroundTimeout` properties.
  // https://github.com/WICG/pending-beacon/blob/main/README.md#properties
  if (GetPage()->IsPageVisible()) {
    // The timer should be reset if the page enters `visible` visibility state
    // before the `backgroundTimeout` expires.
    CancelDispatchBeacons();
  } else {
    // The timer should start after the page enters `hidden` visibility state.
    ScheduleDispatchBeacons();
  }
}

void PendingBeaconDispatcher::ScheduleDispatchBeacons() {
  if (pending_beacons_.empty()) {
    return;
  }

  // Example:
  //
  // `pending_beacons_`'s content:
  // ----------------------------------------------------
  // |  [0]  |  [1]  |  [2] | [3] |  [4]  | [5] |  [6]  |
  // |---------------------------------------------------
  // | 100ms | 201ms | 99ms | 0ms | 101ms | 1ms | 500ms |
  // |---------------------------------------------------
  //
  // `background_timeout_descending_beacons_` is empty on entering this method,
  // but will be populated with:
  //
  // ----------------------------------------------------
  // |  [0]  |  [1]  |  [2]  |  [3]  |  [4] | [5] | [6] |
  // |---------------------------------------------------
  // | 500ms | 201ms | 101ms | 100ms | 99ms | 1ms | 0ms |
  // |---------------------------------------------------
  //
  background_timeout_descending_beacons_.assign(pending_beacons_);
  std::sort(background_timeout_descending_beacons_.begin(),
            background_timeout_descending_beacons_.end(),
            ReverseBeaconTimeoutSorter());
  previous_delayed_ = base::Microseconds(0);

  ScheduleDispatchNextBundledBeacons();
}

void PendingBeaconDispatcher::ScheduleDispatchNextBundledBeacons() {
  if (background_timeout_descending_beacons_.empty()) {
    return;
  }

  // Prepares a task to send out next bundle of beacons from the tail of
  // `background_timeout_descending_beacons_`.
  // The beacons with backgroundTimeout falls into the same interval,
  // `kBeaconTimeoutInterval`, are indicated by [`start_index`, end).
  //
  // Using the same example from within `ScheduleDispatchBeacons()`:
  //   - Bundle 1:
  //     - `start_index` = [4], end = [7]
  //     - `delayed` = 99ms
  //     - `previous_delayed_` = 0ms => 99ms
  //   - Bundle 2:
  //     - `start_index` = [2], end = [4]
  //     - `delayed` = 2ms
  //     - `previous_delayed_` = 99ms => 101ms
  //   - Bundle 3:
  //     - `start_index` = [1], end = [2]
  //     - `delayed` = 100ms
  //     - `previous_delayed_` = 101ms => 201ms
  //   - Bundle 4:
  //     - `start_index` = [0], end = [1]
  //     - `delayed` = 299ms
  //     - `previous_delayed_` = 201ms => 500ms
  auto task_runner = GetTaskRunner();
  const auto start_index = GetStartIndexForNextBundledBeacons();
  const auto delayed = background_timeout_descending_beacons_[start_index]
                           ->GetBackgroundTimeout() -
                       previous_delayed_;
  DCHECK(!delayed.is_negative());
  previous_delayed_ += delayed;
  // Uses `WrapWeakPersistent(this)` because if the associated Document is
  // destroyed, the browser process should be responsible for sending out and
  // destroy all queued beacons, which will unbound the receivers. In such case,
  // this class and members should not outlive the Document (ExecutionContext).
  task_handle_ = PostNonNestableDelayedCancellableTask(
      *task_runner, FROM_HERE,
      WTF::BindOnce(&PendingBeaconDispatcher::OnDispatchBeaconsAndRepeat,
                    WrapWeakPersistent(this), start_index),
      delayed);
}

void PendingBeaconDispatcher::OnDispatchBeaconsAndRepeat(
    wtf_size_t start_index) {
  DCHECK(start_index < background_timeout_descending_beacons_.size());

  // Dispatches all beacons within the same bundle.
  for (auto i = start_index; i < background_timeout_descending_beacons_.size();
       i++) {
    auto beacon = background_timeout_descending_beacons_[i];
    beacon->Send();
  }
  background_timeout_descending_beacons_.resize(start_index);

  // Schedules the next bundle of beacons to dispatch.
  ScheduleDispatchNextBundledBeacons();
}

wtf_size_t PendingBeaconDispatcher::GetStartIndexForNextBundledBeacons() const {
  DCHECK(background_timeout_descending_beacons_.size());
  if (background_timeout_descending_beacons_.size() == 1) {
    return 0;
  }

  // Locates an index `i` (or the returned value) such that the range
  // [`i`, `background_timeout_descending_beacons_.size()`) contains the beacons
  // with their background timeout values fall in the range
  // [`floor_timeout`, `ceiling_timeout`), where (`ceiling_timeout` - 1ms) is
  // the maximum background timeout which represents this bundle and will be
  // used in scheduling.
  // `floor_timeout` is the background timeout from the first beacon of this
  // bundle.
  //
  // Using the same example from within `ScheduleDispatchBeacons()`:
  //   - Bundle 1:
  //     - `floor_timeout` = 0ms
  //     - `ceiling_timeout` = 100ms
  //     - returned index = [4]
  //   - Bundle 2:
  //     - `floor_timeout` = 100ms
  //     - `ceiling_timeout` = 200ms
  //     - returned index = [2]
  //   - Bundle 3:
  //     - `floor_timeout` = 201ms
  //     - `ceiling_timeout` = 300ms (not 301ms)
  //     - returned index = [1]
  //   - Bundle 4:
  //     - `floor_timeout` = 500ms
  //     - `ceiling_timeout` = 600ms
  //     - returned index = [0]
  const auto floor_timeout =
      background_timeout_descending_beacons_.back()->GetBackgroundTimeout();
  // Rounds the the nearest 100x ms.
  const auto ceiling_timeout =
      (floor_timeout + kBeaconTimeoutInterval).IntDiv(kBeaconTimeoutInterval) *
      kBeaconTimeoutInterval;
  // Locates the first element such that
  // element.backgroundTimeout >= ceiling_timeout is false.
  const auto* const it = base::ranges::lower_bound(
      background_timeout_descending_beacons_, ceiling_timeout,
      std::greater_equal(),
      [](const Member<PendingBeaconDispatcher::PendingBeacon>& b) {
        return b->GetBackgroundTimeout();
      });
  // The element with `floor_timeout`, i.e. last element, guarantees the
  // existence of `it`.
  return base::checked_cast<wtf_size_t>(
      std::distance(background_timeout_descending_beacons_.begin(), it));
}

void PendingBeaconDispatcher::CancelDispatchBeacons() {
  // Tasks must be canceled before clearing beacon references.
  task_handle_.Cancel();
  previous_delayed_ = base::Milliseconds(0);
  background_timeout_descending_beacons_.clear();
}

scoped_refptr<base::SingleThreadTaskRunner>
PendingBeaconDispatcher::GetTaskRunner() {
  DCHECK(GetSupplementable());
  return GetSupplementable()->GetTaskRunner(kTaskType);
}

void PendingBeaconDispatcher::Trace(Visitor* visitor) const {
  Supplement::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
  PageVisibilityObserver::Trace(visitor);
  visitor->Trace(remote_);
  visitor->Trace(pending_beacons_);
  visitor->Trace(background_timeout_descending_beacons_);
}

bool PendingBeaconDispatcher::HasPendingBeaconForTesting(
    PendingBeacon* pending_beacon) const {
  return pending_beacons_.Contains(pending_beacon);
}

void PendingBeaconDispatcher::OnDispatchPagehide() {
  if (!features::kPendingBeaconAPIForcesSendingOnNavigation.Get()) {
    return;
  }

  // At this point, the renderer can assume that all beacons on this document
  // have (or will have) been sent out by browsers. The only work left is to
  // update all beacons pending state such that they cannot be updated anymore.
  //
  // This is to mitigate potential privacy issue that when network changes
  // after users think they have left a page, beacons queued in that page
  // still exist and get sent through the new network, which leaks navigation
  // history to the new network.
  // See https://github.com/WICG/pending-beacon/issues/30.
  //
  // Note that the pagehide event might be dispatched a bit earlier than when
  // beacons get sents by browser in same-site navigation.

  for (auto& pending_beacon : pending_beacons_) {
    if (pending_beacon->IsPending()) {
      pending_beacon->MarkNotPending();
    }
  }
  pending_beacons_.clear();
}

}  // namespace blink
