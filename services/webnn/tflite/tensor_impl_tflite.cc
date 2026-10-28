// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/tflite/tensor_impl_tflite.h"

#include <climits>
#include <utility>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notimplemented.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "services/webnn/public/cpp/in_renderer_tensor_handle.h"
#include "services/webnn/public/cpp/webnn_trace.h"
#include "services/webnn/public/mojom/webnn_tensor.mojom.h"
#include "services/webnn/queueable_resource_state.h"
#include "services/webnn/queueable_resource_state_base.h"
#include "services/webnn/resource_task.h"
#include "services/webnn/tflite/buffer_content_tflite.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_utils.h"
#include "third_party/tflite/src/tensorflow/lite/util.h"

namespace webnn::tflite {

namespace {

// Fast-path handle registered on the in-renderer path. Blink invokes
// `WriteTensor`/`ReadTensor` directly from the renderer main thread, and each
// schedules a `ResourceTask` on `owning_task_runner_` to lock the tensor's
// `QueueableResourceState` — exactly the same locking behavior as the mojo
// `WebNNTensor` path. `WriteTensor` copies the payload once into a new
// `BufferContent` on the caller's sequence and swaps it in while exclusively
// locked. `ReadTensor` hands the locked buffer's contents back to the
// caller's sequence directly, keeping the shared lock held until the caller
// has copied out of it. Either way, the payload is only copied once.
class TfliteInRendererTensorHandle final : public InRendererTensorHandle {
 public:
  TfliteInRendererTensorHandle(
      scoped_refptr<QueueableResourceState<BufferContent>> buffer_state,
      base::WeakPtr<WebNNContextImpl> context,
      const blink::WebNNTensorToken& tensor_token,
      scoped_refptr<base::SequencedTaskRunner> owning_task_runner)
      : InRendererTensorHandle(std::move(owning_task_runner)),
        buffer_state_(std::move(buffer_state)),
        context_(std::move(context)),
        tensor_token_(tensor_token) {}

  TfliteInRendererTensorHandle(const TfliteInRendererTensorHandle&) = delete;
  TfliteInRendererTensorHandle& operator=(const TfliteInRendererTensorHandle&) =
      delete;

  void WriteTensor(base::span<const uint8_t> src) override {
    // Copy on the caller's sequence into a new `BufferContent`. We cannot
    // access V8 ArrayBuffer storage from the compute sequence, so this copy
    // is unavoidable until a future zero-copy write path is implemented.
    // Allocating the replacement buffer here (rather than a plain heap
    // buffer) lets `EnqueueWrite` swap it directly into the
    // tensor's `QueueableResourceState`, avoiding a second copy.
    auto new_buffer = std::make_unique<BufferContent>(src.size());
    new_buffer->AsSpan().copy_from(src);
    owning_task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&TfliteInRendererTensorHandle::EnqueueWrite,
                                  this, std::move(new_buffer)));
  }

  void BeginReadTensor(ReadCallback ready_to_copy) override {
    // Bind the reply back to the caller's sequence.
    ReadCallback bound_ready_to_copy =
        base::BindPostTaskToCurrentDefault(std::move(ready_to_copy), FROM_HERE);
    owning_task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&TfliteInRendererTensorHandle::EnqueueRead,
                                  this, std::move(bound_ready_to_copy)));
  }

  void Destroy() override {
    owning_task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&TfliteInRendererTensorHandle::DestroyOnOwningSequence,
                       this));
  }

 private:
  ~TfliteInRendererTensorHandle() override = default;

  void EnqueueWrite(std::unique_ptr<BufferContent> new_buffer) {
    ScopedTrace scoped_trace("TfliteInRendererTensorHandle::Write");
    std::vector<scoped_refptr<QueueableResourceStateBase>> exclusive = {
        buffer_state_};
    scoped_trace.AddStep("Wait for tensor");
    auto task = base::MakeRefCounted<ResourceTask>(
        /*shared_resources=*/std::vector<
            scoped_refptr<QueueableResourceStateBase>>(),
        /*exclusive_resources=*/std::move(exclusive),
        base::BindOnce(
            [](scoped_refptr<QueueableResourceState<BufferContent>>
                   content_handle,
               std::unique_ptr<BufferContent> new_buffer,
               ScopedTrace scoped_trace, base::OnceClosure completion_closure) {
              scoped_trace.AddStep("Begin write");
              content_handle->ReplaceExclusivelyLockedResource(
                  std::move(new_buffer));
              scoped_trace.AddStep("End write");
              std::move(completion_closure).Run();
            },
            buffer_state_, std::move(new_buffer), std::move(scoped_trace)));
    task->Enqueue();
  }

  void EnqueueRead(ReadCallback ready_to_copy) {
    ScopedTrace scoped_trace("TfliteInRendererTensorHandle::Read");
    std::vector<scoped_refptr<QueueableResourceStateBase>> shared = {
        buffer_state_};
    scoped_trace.AddStep("Wait for tensor");
    auto task = base::MakeRefCounted<ResourceTask>(
        std::move(shared),
        /*exclusive_resources=*/
        std::vector<scoped_refptr<QueueableResourceStateBase>>(),
        base::BindOnce(
            [](scoped_refptr<QueueableResourceState<BufferContent>>
                   content_handle,
               ReadCallback ready_to_copy, ScopedTrace scoped_trace,
               base::OnceClosure completion_closure) {
              base::span<const uint8_t> src =
                  content_handle->GetSharedLockedResource().AsSpan();
              // Hand `src` straight to the caller instead of copying it into
              // an intermediate buffer here; the caller is expected to copy
              // directly into its final destination. The actual read thus
              // happens in `ready_to_copy`, so the "read" step spans from
              // here until the caller runs `read_complete`.
              scoped_trace.AddStep("Begin read");
              base::OnceClosure read_complete = base::BindOnce(
                  [](ScopedTrace scoped_trace,
                     base::OnceClosure completion_closure) {
                    scoped_trace.AddStep("End read");
                    std::move(completion_closure).Run();
                  },
                  std::move(scoped_trace), std::move(completion_closure));
              // `completion_closure` must run on this (owning) sequence per
              // `ResourceTask::Complete()`'s sequence check, so wrap it to
              // bounce back here no matter which sequence the caller runs it
              // from — the same trick `BeginReadTensor()` uses for
              // `ready_to_copy` in the other direction.
              std::move(ready_to_copy)
                  .Run(src, base::BindPostTaskToCurrentDefault(
                                std::move(read_complete), FROM_HERE));
            },
            buffer_state_, std::move(ready_to_copy), std::move(scoped_trace)));
    task->Enqueue();
  }

  void DestroyOnOwningSequence() {
    // Already-queued reads/writes keep `buffer_state_` alive, so they still
    // complete safely after the tensor is removed from the context.
    if (context_) {
      context_->DestroyTensorFromInRenderer(tensor_token_);
    }
  }

  const scoped_refptr<QueueableResourceState<BufferContent>> buffer_state_;
  const base::WeakPtr<WebNNContextImpl> context_;
  const blink::WebNNTensorToken tensor_token_;
};

}  // namespace

// static
base::expected<scoped_refptr<WebNNTensorImpl>, mojom::ErrorPtr>
TensorImplTflite::Create(
    mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
    WebNNContextImpl& context,
    mojom::TensorInfoPtr tensor_info) {
  size_t size = tensor_info->descriptor.PackedByteLength();
  // Invalid values are rejected in GraphBuilder.
  CHECK(base::IsValueInRangeForNumericType<int>(size));

  auto buffer_content = std::make_unique<BufferContent>(size);
  auto buffer_state =
      base::MakeRefCounted<QueueableResourceState<BufferContent>>(
          std::move(buffer_content));
  auto tensor = base::MakeRefCounted<TensorImplTflite>(
      std::move(receiver), context, std::move(tensor_info), buffer_state,
      base::PassKey<TensorImplTflite>());

  // On the in-renderer path, register a fast-path handle so Blink can bypass
  // the mojo `WebNNTensor` interface for writeTensor/readTensor. Registration
  // happens synchronously before `WebNNContextImpl::CreateTensor` posts the
  // `CreateTensor` reply, so Blink is guaranteed to observe the entry when
  // the reply is delivered.
  if (tensor->IsInRenderer()) {
    InRendererTensorRegistry::Register(
        tensor->handle(),
        base::MakeRefCounted<TfliteInRendererTensorHandle>(
            buffer_state, context.AsWeakPtr(), tensor->handle(),
            base::WrapRefCounted(context.owning_task_runner())));
  }
  return tensor;
}

TensorImplTflite::TensorImplTflite(
    mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
    WebNNContextImpl& context,
    mojom::TensorInfoPtr tensor_info,
    scoped_refptr<QueueableResourceState<BufferContent>> buffer_state,
    base::PassKey<TensorImplTflite>)
    : WebNNTensorImpl(std::move(receiver), context, std::move(tensor_info)),
      buffer_state_(std::move(buffer_state)) {}

TensorImplTflite::~TensorImplTflite() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (IsInRenderer()) {
    InRendererTensorRegistry::Unregister(handle());
  }
}

const scoped_refptr<QueueableResourceState<BufferContent>>&
TensorImplTflite::GetBufferState() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return buffer_state_;
}

void TensorImplTflite::ReadTensorImpl(ReadTensorCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ScopedTrace scoped_trace("TensorImplTflite::ReadTensorImpl");

  // Lock the buffer contents as shared/read-only.
  std::vector<scoped_refptr<QueueableResourceStateBase>> shared_resources = {
      buffer_state_};

  scoped_trace.AddStep("Wait for tensor");
  auto task = base::MakeRefCounted<ResourceTask>(
      std::move(shared_resources),
      /*exclusive_resources=*/
      std::vector<scoped_refptr<QueueableResourceStateBase>>(),
      base::BindOnce(
          [](base::WeakPtr<WebNNContextImpl> context,
             scoped_refptr<QueueableResourceState<BufferContent>>
                 content_handle,
             ReadTensorCallback callback, ScopedTrace scoped_trace,
             base::OnceClosure completion_closure) {
            if (context) {
              scoped_trace.AddStep("Begin read");
              // Memory copies are fast, avoid the overhead of posting a task
              // to the thread pool and do the work synchronously.
              std::move(callback).Run(mojom::ReadTensorResult::NewBuffer(
                  context->WriteDataToDataPipeOrBigBuffer(
                      content_handle->GetSharedLockedResource().AsSpan())));

              scoped_trace.AddStep("End read");
            }
            std::move(completion_closure).Run();
          },
          context_->AsWeakPtr(), buffer_state_, std::move(callback),
          std::move(scoped_trace)));
  task->Enqueue();
}

void TensorImplTflite::WriteTensorImpl(mojo_base::BigBuffer src_buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ScopedTrace scoped_trace("TensorImplTflite::WriteTensorImpl");

  // Take an exclusive lock to the buffer contents while reading.
  std::vector<scoped_refptr<QueueableResourceStateBase>> exclusive_resources = {
      buffer_state_};

  scoped_trace.AddStep("Wait for tensor");
  auto task = base::MakeRefCounted<ResourceTask>(
      /*shared_resources=*/std::vector<
          scoped_refptr<QueueableResourceStateBase>>(),
      /*exclusive_resources=*/std::move(exclusive_resources),
      base::BindOnce(
          [](base::WeakPtr<WebNNContextImpl> context,
             scoped_refptr<QueueableResourceState<BufferContent>>
                 content_handle,
             mojo_base::BigBuffer src_buffer, ScopedTrace scoped_trace,
             base::OnceClosure completion_closure) {
            if (context) {
              scoped_trace.AddStep("Begin write");
              // Memory copies are fast, avoid the overhead of posting a task to
              // the thread pool and do the work synchronously.
              context->ReadDataFromBigBufferOrDataPipe(
                  std::move(src_buffer),
                  content_handle->GetExclusivelyLockedResource()->AsSpan());

              scoped_trace.AddStep("End write");
            }
            std::move(completion_closure).Run();
          },
          context_->AsWeakPtr(), buffer_state_, std::move(src_buffer),
          std::move(scoped_trace)));
  task->Enqueue();
}

bool TensorImplTflite::ImportTensorImpl(ScopedAccessPtr access) {
  NOTIMPLEMENTED();
  return false;
}

void TensorImplTflite::ExportTensorImpl(ScopedAccessPtr access) {
  NOTIMPLEMENTED();
}

}  // namespace webnn::tflite
