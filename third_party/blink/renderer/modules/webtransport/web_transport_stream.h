// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_WEB_TRANSPORT_STREAM_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_WEB_TRANSPORT_STREAM_H_

#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

// Base class for SendStream, ReceiveStream and BidirectionalStream, used by
// WebTransport to hold references to them. It is not part of the standard.
class WebTransportStream : public GarbageCollectedMixin {
 public:
  virtual ~WebTransportStream() = default;

  // Process an IncomingStreamClosed message from the network service. This is
  // called by WebTransport objects. May execute user JavaScript.
  virtual void OnIncomingStreamClosed(bool fin_received) = 0;

  // Called from WebTransport whenever the mojo connection is torn down. Should
  // close and free data pipes. May execute user JavaScript.
  virtual void Reset() = 0;

  // Called when the ExecutionContext is destroyed. This is used instead of
  // ExecutionContextLifecycleObserver to ensure strict ordering for garbage
  // collection. Must not execute JavaScript.
  virtual void ContextDestroyed() = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_WEB_TRANSPORT_STREAM_H_
