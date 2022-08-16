// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/pending_beacon_dispatcher.h"

#include <algorithm>

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_counted_set.h"

namespace blink {

// static
const char PendingBeaconDispatcher::kSupplementName[] =
    "PendingBeaconDispatcher";

PendingBeaconDispatcher::PendingBeaconDispatcher(
    ExecutionContext& ec,
    base::PassKey<PendingBeaconDispatcher> key)
    : Supplement(ec), remote_(&ec) {
  auto task_runner = ec.GetTaskRunner(kTaskType);

  mojo::PendingReceiver<mojom::blink::PendingBeaconHost> host_receiver =
      remote_.BindNewPipeAndPassReceiver(task_runner);
  ec.GetBrowserInterfaceBroker().GetInterface(std::move(host_receiver));
}

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

void PendingBeaconDispatcher::CreateHostBeacon(
    mojo::PendingReceiver<mojom::blink::PendingBeacon> receiver,
    const KURL& url,
    mojom::blink::BeaconMethod method) {
  remote_->CreateBeacon(std::move(receiver), url, method);
}

void PendingBeaconDispatcher::Trace(Visitor* visitor) const {
  Supplement::Trace(visitor);
  visitor->Trace(remote_);
}

}  // namespace blink
