// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_REMOTEPLAYBACK_AVAILABILITY_CALLBACK_WRAPPER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_REMOTEPLAYBACK_AVAILABILITY_CALLBACK_WRAPPER_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "third_party/blink/renderer/platform/bindings/name_client.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class RemotePlayback;
class V8RemotePlaybackAvailabilityCallback;

// Wraps either a base::OnceClosure or RemotePlaybackAvailabilityCallback object
// to be kept in the RemotePlayback's |availability_callbacks_| map.
class AvailabilityCallbackWrapper final
    : public GarbageCollected<AvailabilityCallbackWrapper>,
      public NameClient {
 public:
  explicit AvailabilityCallbackWrapper(V8RemotePlaybackAvailabilityCallback*);
  explicit AvailabilityCallbackWrapper(base::RepeatingClosure);
  ~AvailabilityCallbackWrapper() = default;

  void Run(RemotePlayback*, bool new_availability);

  virtual void Trace(blink::Visitor*);
  const char* NameInHeapSnapshot() const override {
    return "AvailabilityCallbackWrapper";
  }

 private:
  // Only one of these callbacks must be set.
  Member<V8RemotePlaybackAvailabilityCallback> bindings_cb_;
  base::RepeatingClosure internal_cb_;

  DISALLOW_COPY_AND_ASSIGN(AvailabilityCallbackWrapper);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_REMOTEPLAYBACK_AVAILABILITY_CALLBACK_WRAPPER_H_
