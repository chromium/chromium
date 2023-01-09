// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_PENDING_BEACON_DISPATCHER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_PENDING_BEACON_DISPATCHER_H_

#include "base/gtest_prod_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "third_party/blink/public/mojom/frame/pending_beacon.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/page/page_visibility_observer.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cancellable_task.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class ExecutionContext;
class KURL;

// `PendingBeaconDispatcher` connects a renderer `PendingBeacon` to its browser
// counterpart.
//
// It supports the following requests:
//
// (1) Create browser-side PendingBeacon:
//     On constructed, every `PendingBeacon` from the same Document should call
//     `CreateHostBeacon()` to make calls to the corresponding
//     PendingBeaconHost, and to register itself within this dispatcher.
//
// (2) Dispatch every registered `PendingBeacon` on its background timeout.
//     Implicitly triggered when the page enters `hidden` state within
//     `PageVisibilityChanged()`. In such case, it schedules a series of tasks
//     to send out every beacons according to their individual background
//     timeouts. If the page enters `visible` state, all the pending tasks will
//     be canceled.
//     See `ScheduleDispatchBeacons()` for the actual scheduling algorithm.
//
// Internally, it connects to a `blink::Document`'s corresponding
// PendingBeaconHost instance running in the browser via `remote_`.
//
// PendingBeaconDispatcher is only created and attached to an ExecutionContext
// lazily by `PendingBeaconDispatcher::FromOrAttachTo()` if a PendingBeacon is
// ever created by users in that context (document).
//
// The lifetime of PendingBeaconDispatcher is the same as the ExecutionContext
// it is attached to.
//
// Example:
//   // Accesses an instance of this class within a document.
//   auto& dispatcher = PendingBeaconDispatcher::FromOrAttachTo(ec);
//
//   // When creating a renderer-side PendingBeacon, also call the following
//   // to create browser-side counterpart and to register itself for later
//   // dispatching.
//   dispatcher.CreateHostBeacon(pending_beacon, ...);
//
//   // When a PendingBeacon becomes non-pending.
//   dispatcher.Unregister(pending_beacon);
class CORE_EXPORT PendingBeaconDispatcher
    : public GarbageCollected<PendingBeaconDispatcher>,
      public Supplement<ExecutionContext>,
      public ExecutionContextLifecycleObserver,
      public PageVisibilityObserver {
 public:
  // `PendingBeacon` is an interface to represent a reference to renderer-side
  // pending beacon object. "pending" means this beacon is ok to send.
  // PendingBeaconDispatcher uses this abstraction, instead of the entire
  // blink::PendingBeacon, to schedule tasks to send out pending beacons.
  class CORE_EXPORT PendingBeacon : public GarbageCollectedMixin {
   public:
    // Returns a background timeout to help schedule calls to `Send()` when the
    // page where this beacon created enters hidden visibility state.
    // Implementation should ensure the returned TimeDelta is not negative.
    virtual base::TimeDelta GetBackgroundTimeout() const = 0;
    // Triggers beacon sending action.
    //
    // The sending action may not be triggered if it decides not to do so.
    // If triggered, implementation should also transitions this beacon into
    // non-pending state, and call `PendingBeaconDispatcher::Unregister()` to
    // unregister itself from further scheduling.
    // If not triggered, the dispatcher will schedule to send this next time as
    // long as this is still registered.
    virtual void Send() = 0;

    virtual bool IsPending() const = 0;
    virtual void MarkNotPending() = 0;
    // Provides ExecutionContext where this beacon is created.
    virtual ExecutionContext* GetExecutionContext() = 0;

   protected:
    // Unregisters this beacon from the PendingBeaconDispatcher associated with
    // `GetExecutionContext()`.
    //
    // Calling this method will reduce the lifetime of this instance back to the
    // lifetime of the corresponding JS object, i.e. it won't be extended by the
    // PendingBeaconDispatcher anymore.
    //
    // After this call, all existing timers, either in this PendingBeacon or in
    // PendingBeaconDispatcher, are not cancelled, but will be no-op when their
    // callbacks are triggered.
    void UnregisterFromDispatcher();
  };

  static const char kSupplementName[];

  // TODO(crbug.com/1293679): Update to proper TaskType once the spec finalized.
  // Using the `TaskType::kNetworkingUnfreezable` as pending beacons needs to
  // work when Document is put into BackForwardCache (frozen).
  // See also
  // https://chromium.googlesource.com/chromium/src/+/main/third_party/blink/renderer/platform/scheduler/TaskSchedulingInBlink.md#task-types-and-task-sources
  static constexpr TaskType kTaskType = TaskType::kNetworkingUnfreezable;

  // Returns an instance of this class of `ec` if already stored in `ec`.
  // Otherwise, constructs a new one attached to `ec` and returns it.
  static PendingBeaconDispatcher& FromOrAttachTo(ExecutionContext& ec);

  // Returns a pointer to an instance of this class stored in `ec` if exists.
  // Otherwise, returns nullptr.
  static PendingBeaconDispatcher* From(ExecutionContext& ec);

  explicit PendingBeaconDispatcher(ExecutionContext& ec,
                                   base::PassKey<PendingBeaconDispatcher> key);

  // Not copyable or movable
  PendingBeaconDispatcher(const PendingBeaconDispatcher&) = delete;
  PendingBeaconDispatcher& operator=(const PendingBeaconDispatcher&) = delete;

  void Trace(Visitor* visitor) const override;

  // Asks the PendingBeaconHost in the browser process to create and store a new
  // PendingBeacon that holds `receiver`. The caller `pending_beacon` will be
  // able to communicate with the browser-side PendingBeacon by sending messages
  // to `receiver`.
  //
  // Calling this method will also make this dispatcher retain at least one
  // strong reference to `pending_beacon`, so that `pending_beacon` can be
  // scheduled to dispatch even if its original reference is gone.
  void CreateHostBeacon(
      PendingBeacon* pending_beacon,
      mojo::PendingReceiver<mojom::blink::PendingBeacon> receiver,
      const KURL& url,
      mojom::blink::BeaconMethod method);

  // Unregisters `pending_beacon` from this dispatcher so that it won't be
  // scheduled to send anymore.
  //
  // But it will still be able to send itself out when it is still alive.
  // Note that some of references to `pending_beacon` in this dispatcher might
  // not be cleared immediately.
  void Unregister(PendingBeacon* pending_beacon);

  // `ExecutionContextLifecycleObserver` implementation.
  void ContextDestroyed() override;

  // `PageVisibilityObserver` implementation.
  void PageVisibilityChanged() override;

  // Handles pagehide event.
  //
  // The browser will force sending out all beacons on navigating to a new page,
  // i.e. on pagehide event. Whether or not the old page is put into
  // BackForwardCache is not important.
  //
  // This method asks all owned `pending_beacons_` to update their state to
  // non-pending and unregisters them from this dispatcher.
  void OnDispatchPagehide();

 private:
  // Schedules a series of tasks to dispatch pending beacons according to
  // their `PendingBeacon::GetBackgroundTimeout()`.
  //
  // Internally, it doesn't send all of pending beacons out at once. Instead, it
  // bundles pending beacons with similar background timeout, and sends them out
  // in batch to reduce the number of task callbacks triggered.
  void ScheduleDispatchBeacons();

  // Cancels the scheduled task held by `task_handle_` if exists, and clears
  // all pending beacons held in `background_timeout_descending_beacons_`.
  void CancelDispatchBeacons();

  // Internal method to schedule sending a bundle of beacons. see
  // `GetStartIndexForNextBundledBeacons()` for more details.
  void ScheduleDispatchNextBundledBeacons();

  // Sends out beacons in the range [`start_index`, end) from
  // `background_timeout_descending_beacons_`.
  // It also schedules the next call to itself if feasible.
  void OnDispatchBeaconsAndRepeat(wtf_size_t start_index);

  // Returns the starting index of a range of beacons that can be sent out
  // together by looking into beacons in
  // `background_timeout_descending_beacons_`. In other words,
  // background_timeout_descending_beacons_[returned index, end) is the next
  // bundle.
  wtf_size_t GetStartIndexForNextBundledBeacons() const;

  // Returns a TaskRunner to schedule beacon sending tasks.
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner();

  // Connects to a PendingBeaconHost running in browser process.
  HeapMojoRemote<mojom::blink::PendingBeaconHost> remote_;

  // Retains strong references to the pending beacons so that they can be
  // scheduled to send even if the original references are gone.
  //
  // A new reference is inserted every time `CreateHostBeacon()` is called.
  // A reference is removed if
  //   - it is manually un-registered by `Unregistered()`.
  //   - it is about to send in `OnDispatchBeaconsAndRepeat()`.
  //
  // This field should be the source of truth when deciding if a pending beacon
  // is still *pending*, i.e. ok to send, or not.
  HeapHashSet<Member<PendingBeacon>> pending_beacons_;

  // Retains additional references to the ones in `pending_beacons_` to process.
  //
  // These are sorted by their `PendingBeacon::GetBackgroundTimeout()` in
  // non-ascending order: the earliest expired beacon is put last so that they
  // can be easily removed.
  // This field is empty until the sending process kicks off, i.e.
  // `ScheduleDispatchBeacons()` is called.
  // Must be cleared every time `CancelDispatchBeacons()` is called.
  HeapVector<Member<PendingBeacon>> background_timeout_descending_beacons_;

  // The accumulated delay indicating how long it has passed since the initial
  // call to `ScheduleDispatchBeacons()`.
  //
  // Must be reset to 0 every time `CancelDispatchBeacons()` is called.
  base::TimeDelta previous_delayed_;

  // Points to the most recent bundled-beacons-sending task scheduled in
  // `ScheduleDispatchNextBundledBeacons()`.
  //
  // It is canceled when `CancelDispatchBeacons()` is called.
  TaskHandle task_handle_;

  // For testing:
  bool HasPendingBeaconForTesting(PendingBeacon* pending_beacon) const;
  FRIEND_TEST_ALL_PREFIXES(PendingBeaconDispatcherBasicBeaconsTest,
                           DispatchBeaconsOnBackgroundTimeout);
  FRIEND_TEST_ALL_PREFIXES(PendingBeaconDispatcherBackgroundTimeoutBundledTest,
                           DispatchOrderedBeacons);
  FRIEND_TEST_ALL_PREFIXES(PendingBeaconDispatcherBackgroundTimeoutBundledTest,
                           DispatchReversedBeacons);
  FRIEND_TEST_ALL_PREFIXES(PendingBeaconDispatcherBackgroundTimeoutBundledTest,
                           DispatchDuplicatedBeacons);
  FRIEND_TEST_ALL_PREFIXES(PendingBeaconDispatcherOnPagehideTest,
                           OnPagehideUpdateAndUnregisterAllBeacons);
  FRIEND_TEST_ALL_PREFIXES(PendingBeaconCreateTest, CreateFromSecureContext);
  FRIEND_TEST_ALL_PREFIXES(PendingBeaconSendTest, Send);
  FRIEND_TEST_ALL_PREFIXES(PendingBeaconSendTest, SendNow);
  FRIEND_TEST_ALL_PREFIXES(PendingBeaconSendTest,
                           SetNonPendingAfterTimeoutTimerStart);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_PENDING_BEACON_DISPATCHER_H_
