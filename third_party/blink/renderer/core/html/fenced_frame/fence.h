// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FENCED_FRAME_FENCE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FENCED_FRAME_FENCE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/html/fenced_frame/fenced_frame_config.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"

namespace blink {

class ExceptionState;
class FenceEvent;
class LocalDOMWindow;
class V8UnionFenceEventOrString;

// Fence is a deprecated collection of fencedframe-related APIs exposed at
// `window.fence`. It is maintained as a stub.
class CORE_EXPORT Fence final : public ScriptWrappable,
                                public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit Fence(LocalDOMWindow& window);

  void reportEvent(const V8UnionFenceEventOrString* event,
                   ExceptionState& exception_state);

  void setReportEventDataForAutomaticBeacons(const FenceEvent* event,
                                             ExceptionState& exception_state);

  HeapVector<Member<FencedFrameConfig>> getNestedConfigs(
      ExceptionState& exception_state);

  void Trace(Visitor*) const override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FENCED_FRAME_FENCE_H_
