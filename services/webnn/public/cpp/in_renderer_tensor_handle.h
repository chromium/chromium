// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_PUBLIC_CPP_IN_RENDERER_TENSOR_HANDLE_H_
#define SERVICES_WEBNN_PUBLIC_CPP_IN_RENDERER_TENSOR_HANDLE_H_

#include <cstdint>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace webnn {

// A thread-safe side channel between Blink's `MLTensor` (main thread) and a
// WebNN backend that runs *in the renderer process* (owning task runner).
// It bypasses the mojo `WebNNTensor` interface for `writeTensor`/`readTensor`
// so we can avoid `BigBuffer` allocation, shared-memory setup and mojo message
// serialization.
//
// Deletion is deferred to `owning_task_runner` (via
// `base::RefCountedDeleteOnSequence`) rather than using plain
// `RefCountedThreadSafe`, because subclasses typically hold sequence-affine
// resources (e.g. `QueueableResourceState`) that must never be destroyed off
// their owning sequence, even though the last reference to this handle can be
// dropped from Blink's main thread (e.g. during garbage collection).
class COMPONENT_EXPORT(WEBNN_PUBLIC_CPP) InRendererTensorHandle
    : public base::RefCountedDeleteOnSequence<InRendererTensorHandle> {
 public:
  // Invoked on the caller's sequence once the tensor is locked for reading.
  // `src` views backend-owned memory and is only valid until `read_complete`
  // is run. The caller must copy out of it (ideally straight into its final
  // destination) and then run `read_complete` exactly once, which unlocks the
  // tensor for the next queued operation.
  using ReadCallback =
      base::OnceCallback<void(base::span<const uint8_t> src,
                              base::OnceClosure read_complete)>;

  // Copies `src` synchronously into a heap buffer on the caller's sequence,
  // then schedules the write on the backend's owning task runner.
  virtual void WriteTensor(base::span<const uint8_t> src) = 0;

  // Schedules a read on the backend's owning task runner, then invokes
  // `ready_to_copy` on the caller's sequence once the tensor is locked for
  // reading. See `ReadCallback` above.
  virtual void BeginReadTensor(ReadCallback ready_to_copy) = 0;

  // Destroys the backend tensor once all previously scheduled operations on
  // the owning task runner (including `Dispatch()`) have been processed.
  virtual void Destroy() = 0;

 protected:
  friend class base::RefCountedDeleteOnSequence<InRendererTensorHandle>;
  friend class base::DeleteHelper<InRendererTensorHandle>;

  explicit InRendererTensorHandle(
      scoped_refptr<base::SequencedTaskRunner> owning_task_runner);
  virtual ~InRendererTensorHandle();
};

// Process-wide registry mapping `blink::WebNNTensorToken` to the fast-path
// handle exposed by the in-renderer backend. Populated by the service side at
// tensor creation time, looked up by Blink after `CreateTensor` completes.
// All methods are thread-safe.
class COMPONENT_EXPORT(WEBNN_PUBLIC_CPP) InRendererTensorRegistry {
 public:
  static void Register(const blink::WebNNTensorToken& token,
                       scoped_refptr<InRendererTensorHandle> handle);
  static void Unregister(const blink::WebNNTensorToken& token);
  static scoped_refptr<InRendererTensorHandle> Get(
      const blink::WebNNTensorToken& token);
};

}  // namespace webnn

#endif  // SERVICES_WEBNN_PUBLIC_CPP_IN_RENDERER_TENSOR_HANDLE_H_
