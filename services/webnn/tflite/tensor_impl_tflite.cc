// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/tflite/tensor_impl_tflite.h"

#include <climits>
#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/heap_array.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
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
#include "services/webnn/tflite/buffer_content_tflite.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_utils.h"
#include "third_party/tflite/src/tensorflow/lite/util.h"

namespace webnn::tflite {

namespace {

// Fast-path handle registered on the in-renderer path. Blink invokes
// `WriteTensor`/`BeginReadTensor` directly from the renderer main thread, and
// each posts a task to `owning_task_runner_` so all tensor I/O and graph
// dispatches execute synchronously in FIFO order on the context's sequence.
class TfliteInRendererTensorHandle final : public InRendererTensorHandle {
 public:
  TfliteInRendererTensorHandle(
      base::WeakPtr<TensorImplTflite> tensor,
      base::WeakPtr<WebNNContextImpl> context,
      const blink::WebNNTensorToken& tensor_token,
      scoped_refptr<base::SequencedTaskRunner> owning_task_runner)
      : InRendererTensorHandle(std::move(owning_task_runner)),
        tensor_(std::move(tensor)),
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
    // buffer) lets `WriteOnOwningSequence` swap it directly into the tensor,
    // avoiding a second copy.
    auto new_buffer = std::make_unique<BufferContent>(src.size());
    new_buffer->AsSpan().copy_from(src);
    owning_task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&TfliteInRendererTensorHandle::WriteOnOwningSequence,
                       this, std::move(new_buffer)));
  }

  void BeginReadTensor(ReadCallback ready_to_copy) override {
    // Bind the reply back to the caller's sequence.
    ReadCallback bound_ready_to_copy =
        base::BindPostTaskToCurrentDefault(std::move(ready_to_copy), FROM_HERE);
    owning_task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&TfliteInRendererTensorHandle::ReadOnOwningSequence,
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

  void WriteOnOwningSequence(std::unique_ptr<BufferContent> new_buffer) {
    ScopedTrace scoped_trace("TfliteInRendererTensorHandle::Write");
    if (!tensor_) {
      return;
    }
    scoped_trace.AddStep("Begin write");
    tensor_->SetBufferContent(std::move(new_buffer));
    scoped_trace.AddStep("End write");
  }

  void ReadOnOwningSequence(ReadCallback ready_to_copy) {
    ScopedTrace scoped_trace("TfliteInRendererTensorHandle::Read");
    if (!tensor_) {
      std::move(ready_to_copy)
          .Run(base::span<const uint8_t>(), base::DoNothing());
      return;
    }
    scoped_trace.AddStep("Begin read");
    auto copy = base::HeapArray<uint8_t>::CopiedFrom(
        tensor_->GetBufferContent().AsSpan());
    base::span<const uint8_t> src = copy.as_span();
    base::OnceClosure read_complete = base::BindOnce(
        [](base::HeapArray<uint8_t> /*keep_alive*/, ScopedTrace scoped_trace) {
          scoped_trace.AddStep("End read");
        },
        std::move(copy), std::move(scoped_trace));
    std::move(ready_to_copy).Run(src, std::move(read_complete));
  }

  void DestroyOnOwningSequence() {
    if (context_) {
      context_->DestroyTensorFromInRenderer(tensor_token_);
    }
  }

  const base::WeakPtr<TensorImplTflite> tensor_;
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
  auto tensor = base::MakeRefCounted<TensorImplTflite>(
      std::move(receiver), context, std::move(tensor_info),
      std::move(buffer_content), base::PassKey<TensorImplTflite>());

  // On the in-renderer path, register a fast-path handle so Blink can bypass
  // the mojo `WebNNTensor` interface for writeTensor/readTensor. Registration
  // happens synchronously before `WebNNContextImpl::CreateTensor` posts the
  // `CreateTensor` reply, so Blink is guaranteed to observe the entry when
  // the reply is delivered.
  if (tensor->IsInRenderer()) {
    InRendererTensorRegistry::Register(
        tensor->handle(),
        base::MakeRefCounted<TfliteInRendererTensorHandle>(
            tensor->AsWeakPtr(), context.AsWeakPtr(), tensor->handle(),
            base::WrapRefCounted(context.owning_task_runner())));
  }
  return tensor;
}

TensorImplTflite::TensorImplTflite(
    mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
    WebNNContextImpl& context,
    mojom::TensorInfoPtr tensor_info,
    std::unique_ptr<BufferContent> buffer_content,
    base::PassKey<TensorImplTflite>)
    : WebNNTensorImpl(std::move(receiver), context, std::move(tensor_info)),
      buffer_content_(std::move(buffer_content)) {}

TensorImplTflite::~TensorImplTflite() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (IsInRenderer()) {
    InRendererTensorRegistry::Unregister(handle());
  }
}

const BufferContent& TensorImplTflite::GetBufferContent() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return *buffer_content_;
}

void TensorImplTflite::SetBufferContent(
    std::unique_ptr<BufferContent> buffer_content) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(buffer_content->AsSpan().size(), PackedByteLength());
  buffer_content_ = std::move(buffer_content);
}

void TensorImplTflite::ReadTensorImpl(ReadTensorCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ScopedTrace scoped_trace("TensorImplTflite::ReadTensorImpl");
  scoped_trace.AddStep("Begin read");
  std::move(callback).Run(mojom::ReadTensorResult::NewBuffer(
      context_->WriteDataToDataPipeOrBigBuffer(buffer_content_->AsSpan())));
  scoped_trace.AddStep("End read");
}

void TensorImplTflite::WriteTensorImpl(mojo_base::BigBuffer src_buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ScopedTrace scoped_trace("TensorImplTflite::WriteTensorImpl");
  scoped_trace.AddStep("Begin write");
  context_->ReadDataFromBigBufferOrDataPipe(std::move(src_buffer),
                                            buffer_content_->AsSpan());
  scoped_trace.AddStep("End write");
}

bool TensorImplTflite::ImportTensorImpl(ScopedAccessPtr access) {
  NOTIMPLEMENTED();
  return false;
}

void TensorImplTflite::ExportTensorImpl(ScopedAccessPtr access) {
  NOTIMPLEMENTED();
}

}  // namespace webnn::tflite
