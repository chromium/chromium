// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_PENDING_BEACON_DISPATCHER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_PENDING_BEACON_DISPATCHER_H_

#include "base/types/pass_key.h"
#include "third_party/blink/public/mojom/frame/pending_beacon.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class ExecutionContext;
class KURL;

// `PendingBeaconDispatcher` helps the caller connect to a `blink::Document`'s
// PendingBeaconHost by wrapping a shared HeapMojoRemote `remote_` that connects
// to a PendingBeaconHost instance running in the browser.
//
// Every PendingBeacon from the same Document should use this class to make
// calls to the corresponding PendingBeaconHost.
//
// PendingBeaconDispatcher is only created and attached to an ExecutionContext
// lazily by `PendingBeaconDispatcher::FromOrAttachTo()` if a PendingBeacon is
// ever created by users in that context (document).
//
// The lifetime of PendingBeaconDispatcher is the same as the ExecutionContext
// it is attached to.
//
// TODO(crbug.com/1293679): Implement dispatching beacons on timeout.
// TODO(crbug.com/1293679): Implement dispatching beacons on (page hidden +
// backgroundTimeout) msec.
class CORE_EXPORT PendingBeaconDispatcher
    : public GarbageCollected<PendingBeaconDispatcher>,
      public Supplement<ExecutionContext> {
 public:
  static const char kSupplementName[];
  // TODO(crbug.com/1293679): Update to proper TaskType once the spec finalized.
  // Using the `TaskType::kMiscPlatformAPI` as pending beacons are not yet
  // associated with any specific task runner in the spec.
  // See also
  // https://chromium.googlesource.com/chromium/src/+/main/third_party/blink/renderer/platform/scheduler/TaskSchedulingInBlink.md#task-types-and-task-sources
  static constexpr TaskType kTaskType = TaskType::kMiscPlatformAPI;

  explicit PendingBeaconDispatcher(ExecutionContext& ec,
                                   base::PassKey<PendingBeaconDispatcher> key);

  // Not copyable or movable
  PendingBeaconDispatcher(const PendingBeaconDispatcher&) = delete;
  PendingBeaconDispatcher& operator=(const PendingBeaconDispatcher&) = delete;
  virtual ~PendingBeaconDispatcher() = default;

  void Trace(Visitor* visitor) const override;

  // Returns an instance of this class of `ec` if already stored in `ec`.
  // Otherwise, constructs a new one attached to `ec` and returns it.
  static PendingBeaconDispatcher& FromOrAttachTo(ExecutionContext& ec);

  // Returns a pointer to an instance of this class stored in `ec` if exists.
  // Otherwise, returns nullptr.
  static PendingBeaconDispatcher* From(ExecutionContext& ec);

  // Asks the PendingBeaconHost in the browser process to create and store a new
  // PendingBeacon that holds `receiver`. The caller `beacon` will be able to
  // communicate with it by sending messages to `receiver`.
  //
  // This method also retains an extra reference to `beacon` for later use in
  // `ScheduleDispatchingBeacons()`.
  void CreateHostBeacon(
      mojo::PendingReceiver<mojom::blink::PendingBeacon> receiver,
      const KURL& url,
      mojom::blink::BeaconMethod method);

 private:
  // Connects to a PendingBeaconHost running in browser process.
  HeapMojoRemote<mojom::blink::PendingBeaconHost> remote_;
};

}  // namespace blink

#endif  // #define
        // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_PENDING_BEACON_DISPATCHER_H_
