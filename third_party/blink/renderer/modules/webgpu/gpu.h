// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_H_

#include <dawn/webgpu.h>

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/supplementable.h"

// Forward declarations from webgpu.h
typedef struct WGPUBufferImpl* WGPUBuffer;
// Forward declaration from dawn_proc.h
struct DawnProcTable;

namespace blink {

class GPUAdapter;
class GPUBuffer;
class GPURequestAdapterOptions;
class NavigatorBase;
class ScriptPromiseResolver;
class ScriptState;
class DawnControlClientHolder;

struct BoxedMappableWGPUBufferHandles
    : public RefCounted<BoxedMappableWGPUBufferHandles> {
 public:
  // Basic typed wrapper around |contents_|.
  void insert(WGPUBuffer buffer) { contents_.insert(buffer); }

  // Basic typed wrapper around |contents_|.
  void erase(WGPUBuffer buffer) { contents_.erase(buffer); }

  void ClearAndDestroyAll(const DawnProcTable& procs);

 private:
  // void* because HashSet tries to infer if T is GarbageCollected,
  // but WGPUBufferImpl has no real definition. We could define
  // IsGarbageCollectedType<struct WGPUBufferImpl> but it could easily
  // lead to a ODR violation.
  HashSet<void*> contents_;
};

class MODULES_EXPORT GPU final : public ScriptWrappable,
                                 public Supplement<NavigatorBase>,
                                 public ExecutionContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static const char kSupplementName[];

  // Getter for navigator.gpu
  static GPU* gpu(NavigatorBase&);

  explicit GPU(NavigatorBase&);

  GPU(const GPU&) = delete;
  GPU& operator=(const GPU&) = delete;

  ~GPU() override;

  // ScriptWrappable overrides
  void Trace(Visitor* visitor) const override;

  // ExecutionContextLifecycleObserver overrides
  void ContextDestroyed() override;

  // gpu.idl
  ScriptPromise requestAdapter(ScriptState* script_state,
                               const GPURequestAdapterOptions* options);
  String getPreferredCanvasFormat();

  // Store the buffer in a weak hash set so we can destroy it when the
  // context is destroyed.
  void TrackMappableBuffer(GPUBuffer* buffer);
  // Untrack the GPUBuffer. This is called eagerly when the buffer is
  // destroyed.
  void UntrackMappableBuffer(GPUBuffer* buffer);

  BoxedMappableWGPUBufferHandles* mappable_buffer_handles() const {
    return mappable_buffer_handles_.get();
  }

  void SetDawnControlClientHolderForTesting(
      scoped_refptr<DawnControlClientHolder> dawn_control_client);

 private:
  void OnRequestAdapterCallback(ScriptState* script_state,
                                const GPURequestAdapterOptions* options,
                                ScriptPromiseResolver* resolver,
                                WGPURequestAdapterStatus status,
                                WGPUAdapter adapter,
                                const char* error_message);

  void RecordAdapterForIdentifiability(ScriptState* script_state,
                                       const GPURequestAdapterOptions* options,
                                       GPUAdapter* adapter) const;

  void RequestAdapterImpl(ScriptState* script_state,
                          const GPURequestAdapterOptions* options,
                          ScriptPromiseResolver* resolver);

  scoped_refptr<DawnControlClientHolder> dawn_control_client_;
  WTF::Vector<base::OnceCallback<void()>>
      dawn_control_client_initialized_callbacks_;
  HeapHashSet<WeakMember<GPUBuffer>> mappable_buffers_;
  // Mappable buffers remove themselves from this set on destruction.
  // It is boxed in a scoped_refptr so GPUBuffer can access it in its
  // destructor.
  scoped_refptr<BoxedMappableWGPUBufferHandles> mappable_buffer_handles_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_H_
