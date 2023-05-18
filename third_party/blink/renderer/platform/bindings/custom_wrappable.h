// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_CUSTOM_WRAPPABLE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_CUSTOM_WRAPPABLE_H_

#include "third_party/blink/renderer/platform/bindings/name_client.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_v8_reference.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {
class ScriptState;

// See the comment on CustomWrappableAdaptor.
class PLATFORM_EXPORT CustomWrappable
    : public GarbageCollected<CustomWrappable>,
      public NameClient {
 public:
  CustomWrappable(const CustomWrappable&) = delete;
  CustomWrappable& operator=(const CustomWrappable&) = delete;
  ~CustomWrappable() override = default;
  virtual void Trace(Visitor*) const;
  const char* NameInHeapSnapshot() const override { return "CustomWrappable"; }

  v8::Local<v8::Object> Wrap(ScriptState*);

 protected:
  CustomWrappable() = default;

  // Internal wrapper reference is needed as Oilpan looks up its roots from V8
  // by following all configured wrapper references.
  TraceWrapperV8Reference<v8::Object> wrapper_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_CUSTOM_WRAPPABLE_H_
