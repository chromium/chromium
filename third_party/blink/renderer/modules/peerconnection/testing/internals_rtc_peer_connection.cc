// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/testing/internals_rtc_peer_connection.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"

namespace blink {

int InternalsRTCPeerConnection::peerConnectionCount(Internals& internals) {
  return RTCPeerConnection::PeerConnectionCount();
}

int InternalsRTCPeerConnection::peerConnectionCountLimit(Internals& internals) {
  return RTCPeerConnection::PeerConnectionCountLimit();
}

ScriptPromise<IDLAny>
InternalsRTCPeerConnection::waitForPeerConnectionDispatchEventsTaskCreated(
    ScriptState* script_state,
    Internals& internals,
    RTCPeerConnection* connection) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLAny>>(script_state);
  auto promise = resolver->Promise();
  CHECK(!connection->dispatch_events_task_created_callback_for_testing_);
  connection->dispatch_events_task_created_callback_for_testing_ =
      WTF::BindOnce(
          [](ScriptPromiseResolver<IDLAny>* resolver) { resolver->Resolve(); },
          WrapPersistent(resolver));
  return promise;
}

}  // namespace blink
