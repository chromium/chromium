// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/netinfo/worker_navigator_network_information.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/workers/worker_navigator.h"
#include "third_party/blink/renderer/modules/netinfo/network_information.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

WorkerNavigatorNetworkInformation::WorkerNavigatorNetworkInformation(
    WorkerNavigator& navigator,
    ExecutionContext* context)
    : Supplement<WorkerNavigator>(navigator) {}

WorkerNavigatorNetworkInformation& WorkerNavigatorNetworkInformation::From(
    WorkerNavigator& navigator,
    ExecutionContext* context) {
  WorkerNavigatorNetworkInformation* supplement =
      ToWorkerNavigatorNetworkInformation(navigator, context);
  if (!supplement) {
    supplement = MakeGarbageCollected<WorkerNavigatorNetworkInformation>(
        navigator, context);
    ProvideTo(navigator, supplement);
  }
  return *supplement;
}

WorkerNavigatorNetworkInformation*
WorkerNavigatorNetworkInformation::ToWorkerNavigatorNetworkInformation(
    WorkerNavigator& navigator,
    ExecutionContext* context) {
  return Supplement<WorkerNavigator>::From<WorkerNavigatorNetworkInformation>(
      navigator);
}

const char WorkerNavigatorNetworkInformation::kSupplementName[] =
    "WorkerNavigatorNetworkInformation";

NetworkInformation* WorkerNavigatorNetworkInformation::connection(
    ScriptState* script_state,
    WorkerNavigator& navigator) {
  ExecutionContext* context = ExecutionContext::From(script_state);
  return WorkerNavigatorNetworkInformation::From(navigator, context)
      .connection(context);
}

void WorkerNavigatorNetworkInformation::Trace(blink::Visitor* visitor) {
  visitor->Trace(connection_);
  Supplement<WorkerNavigator>::Trace(visitor);
}

NetworkInformation* WorkerNavigatorNetworkInformation::connection(
    ExecutionContext* context) {
  DCHECK(context);
  if (!connection_)
    connection_ = MakeGarbageCollected<NetworkInformation>(context);
  return connection_.Get();
}

}  // namespace blink
