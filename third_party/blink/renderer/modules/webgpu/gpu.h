// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/supplementable.h"

struct WGPUDeviceProperties;

namespace blink {

class GPUAdapter;
class GPUBuffer;
class GPURequestAdapterOptions;
class NavigatorBase;
class ScriptPromiseResolver;
class ScriptState;
class DawnControlClientHolder;

class GPU final : public ScriptWrappable,
                  public Supplement<NavigatorBase>,
                  public ExecutionContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static const char kSupplementName[];

  // Getter for navigator.gpu
  static GPU* gpu(NavigatorBase&);

  explicit GPU(NavigatorBase&);
  ~GPU() override;

  // ScriptWrappable overrides
  void Trace(Visitor* visitor) const override;

  // ExecutionContextLifecycleObserver overrides
  void ContextDestroyed() override;

  // gpu.idl
  ScriptPromise requestAdapter(ScriptState* script_state,
                               const GPURequestAdapterOptions* options);

  // Store the buffer in a weak hash set so we can destroy it when the
  // context is destroyed. Note: there is no need to "untrack" buffers
  // because Oilpan automatically removes them from the weak hash set
  // when the GPUBuffer is garbage-collected.
  // https://chromium.googlesource.com/chromium/src/+/refs/heads/main/third_party/blink/renderer/platform/heap/BlinkGCAPIReference.md#weak-collections
  void TrackMappableBuffer(GPUBuffer* buffer);

 private:
  void OnRequestAdapterCallback(ScriptState* script_state,
                                const GPURequestAdapterOptions* options,
                                ScriptPromiseResolver* resolver,
                                int32_t adapter_server_id,
                                const WGPUDeviceProperties& properties,
                                const char* error_message);

  void RecordAdapterForIdentifiability(ScriptState* script_state,
                                       const GPURequestAdapterOptions* options,
                                       GPUAdapter* adapter) const;

  scoped_refptr<DawnControlClientHolder> dawn_control_client_;
  HeapHashSet<WeakMember<GPUBuffer>> mappable_buffers_;

  DISALLOW_COPY_AND_ASSIGN(GPU);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_H_
