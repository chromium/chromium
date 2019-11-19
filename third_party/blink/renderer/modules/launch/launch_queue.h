// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_LAUNCH_LAUNCH_QUEUE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_LAUNCH_LAUNCH_QUEUE_H_

#include "third_party/blink/renderer/modules/launch/launch_params.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"

namespace blink {

class V8LaunchConsumer;
class Visitor;

class LaunchQueue final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  LaunchQueue();
  ~LaunchQueue() override;

  void Enqueue(LaunchParams* params);

  // IDL implementation:
  void setConsumer(V8LaunchConsumer*);

  // ScriptWrappable:
  void Trace(blink::Visitor* visitor) override;

 private:
  HeapVector<Member<LaunchParams>> unconsumed_launch_params_;
  Member<V8LaunchConsumer> consumer_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_LAUNCH_LAUNCH_QUEUE_H_
