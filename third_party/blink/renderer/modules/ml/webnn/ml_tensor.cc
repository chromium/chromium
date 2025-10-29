// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_tensor.h"

#include "base/metrics/histogram_functions.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "services/webnn/public/cpp/ml_tensor_usage.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/mojom/webnn_tensor.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_tensor_descriptor.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/ml/ml_context.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_error.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_utils.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_buffer.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_mailbox_buffer.h"

namespace blink {

namespace {

const char kTensorDestroyedError[] =
    "Tensor has been destroyed or context is lost.";

const char kTensorExportedError[] = "Tensor has been exported to WebGPU.";

const char kTensorWebGPUInteropUnsupportedError[] =
    "The tensor does not support WebGPU interop.";

void RecordReadTensorTime(base::ElapsedTimer read_tensor_timer) {
  base::UmaHistogramMediumTimes("WebNN.MLTensor.TimingMs.Read",
                                read_tensor_timer.Elapsed());
}

}  // namespace

MLTensor::MLTensor(
    ExecutionContext* execution_context,
    MLContext* context,
    webnn::OperandDescriptor descriptor,
    webnn::MLTensorUsage usage,
    scoped_refptr<gpu::ClientSharedImage> shared_image,
    GPUDevice* gpu_device,
    webnn::mojom::blink::CreateTensorSuccessPtr create_tensor_success,
    base::PassKey<MLContext> /*pass_key*/)
    : ml_context_(context),
      descriptor_(std::move(descriptor)),
      usage_(usage),
      webnn_handle_(std::move(create_tensor_success->tensor_handle)),
      remote_tensor_(execution_context),
      shared_image_(std::move(shared_image)),
      gpu_device_(gpu_device) {
  remote_tensor_.Bind(
      std::move(create_tensor_success->tensor_remote),
      execution_context->GetTaskRunner(TaskType::kMachineLearning));
  remote_tensor_.set_disconnect_handler(
      BindOnce(&MLTensor::OnConnectionError, WrapWeakPersistent(this)));
}

MLTensor::~MLTensor() = default;

void MLTensor::Trace(Visitor* visitor) const {
  visitor->Trace(ml_context_);
  visitor->Trace(remote_tensor_);
  visitor->Trace(pending_resolvers_);
  visitor->Trace(pending_byob_resolvers_);
  visitor->Trace(pending_gpu_buffer_resolver_);
  visitor->Trace(gpu_buffer_);
  visitor->Trace(gpu_device_);
  ScriptWrappable::Trace(visitor);
}

V8MLOperandDataType MLTensor::dataType() const {
  return ToBlinkDataType(descriptor_.data_type());
}

Vector<uint32_t> MLTensor::shape() const {
  return Vector<uint32_t>(descriptor_.shape());
}

GPUDevice* MLTensor::gpuDevice() const {
  return gpu_device_;
}

bool MLTensor::readable() const {
  return usage_.Has(webnn::MLTensorUsageFlags::kRead);
}

bool MLTensor::writable() const {
  return usage_.Has(webnn::MLTensorUsageFlags::kWrite);
}

bool MLTensor::constant() const {
  return usage_.Has(webnn::MLTensorUsageFlags::kGraphConstant);
}

void MLTensor::destroy() {
  // Calling OnConnectionError() will disconnect and destroy the tensor in
  // the service. The remote tensor must remain unbound after calling
  // OnConnectionError() because it is valid to call destroy() multiple times.
  OnConnectionError();
}

const webnn::OperandDescriptor& MLTensor::Descriptor() const {
  return descriptor_;
}

webnn::OperandDataType MLTensor::DataType() const {
  return descriptor_.data_type();
}

const std::vector<uint32_t>& MLTensor::Shape() const {
  return descriptor_.shape();
}

const webnn::MLTensorUsage& MLTensor::Usage() const {
  return usage_;
}

uint64_t MLTensor::PackedByteLength() const {
  return descriptor_.PackedByteLength();
}

ScriptPromise<DOMArrayBuffer> MLTensor::ReadTensorImpl(
    webnn::ScopedTrace scoped_trace,
    ScriptState* script_state,
    ExceptionState& exception_state) {
  // Remote context gets automatically unbound when the execution context
  // destructs.
  if (!remote_tensor_.is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kTensorDestroyedError);
    return EmptyPromise();
  }

  if (gpu_buffer_) {
    exception_state.ThrowTypeError(kTensorExportedError);
    return EmptyPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<DOMArrayBuffer>>(
      script_state, exception_state.GetContext());
  pending_resolvers_.insert(resolver);

  base::ElapsedTimer read_tensor_timer;
  remote_tensor_->ReadTensor(blink::BindOnce(
      &MLTensor::OnDidReadTensor, WrapPersistent(this), std::move(scoped_trace),
      WrapPersistent(resolver), std::move(read_tensor_timer)));

  return resolver->Promise();
}

ScriptPromise<IDLUndefined> MLTensor::ReadTensorImpl(
    webnn::ScopedTrace scoped_trace,
    ScriptState* script_state,
    AllowSharedBufferSource* dst_data,
    ExceptionState& exception_state) {
  // Remote context gets automatically unbound when the execution context
  // destructs.
  if (!remote_tensor_.is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kTensorDestroyedError);
    return EmptyPromise();
  }

  base::span<uint8_t> bytes = AsByteSpan(*dst_data);
  if (bytes.size() < PackedByteLength()) {
    exception_state.ThrowTypeError("The destination tensor is too small.");
    return EmptyPromise();
  }

  if (gpu_buffer_) {
    exception_state.ThrowTypeError(kTensorExportedError);
    return EmptyPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  pending_byob_resolvers_.insert(resolver);

  base::ElapsedTimer read_tensor_timer;
  remote_tensor_->ReadTensor(
      blink::BindOnce(&MLTensor::OnDidReadTensorByob, WrapPersistent(this),
                      std::move(scoped_trace), WrapPersistent(resolver),
                      WrapPersistent(dst_data), std::move(read_tensor_timer)));
  return resolver->Promise();
}

void MLTensor::OnDidReadTensor(
    webnn::ScopedTrace scoped_trace,
    ScriptPromiseResolver<DOMArrayBuffer>* resolver,
    base::ElapsedTimer read_tensor_timer,
    webnn::mojom::blink::ReadTensorResultPtr result) {
  pending_resolvers_.erase(resolver);

  if (result->is_error()) {
    const webnn::mojom::blink::Error& read_tensor_error = *result->get_error();
    resolver->RejectWithDOMException(
        WebNNErrorCodeToDOMExceptionCode(read_tensor_error.code),
        read_tensor_error.message);
    return;
  }

  if (result->get_buffer().size() == 0) {
    if (!ml_context_->read_tensor_consumer()) {
      resolver->RejectWithDOMException(
          DOMExceptionCode::kInvalidStateError,
          "ReadTensor(): No data pipe to read tensor data.");
      return;
    }
    ArrayBufferContents contents(
        descriptor_.PackedByteLength(), 1, ArrayBufferContents::kNotShared,
        ArrayBufferContents::kDontInitialize,
        ArrayBufferContents::AllocationFailureBehavior::kCrash);
    size_t bytes_read = 0;
    if (ml_context_->read_tensor_consumer()->ReadData(
            MOJO_READ_DATA_FLAG_ALL_OR_NONE, contents.ByteSpan(), bytes_read) !=
        MOJO_RESULT_OK) {
      resolver->RejectWithDOMException(
          DOMExceptionCode::kDataError,
          "ReadTensor(): Failed to read tensor data from the data pipe.");
      return;
    }
    CHECK_EQ(bytes_read, descriptor_.PackedByteLength());
    resolver->Resolve(DOMArrayBuffer::Create(std::move(contents)));
  } else {
    CHECK_EQ(result->get_buffer().size(), descriptor_.PackedByteLength());

    resolver->Resolve(DOMArrayBuffer::Create(result->get_buffer()));
  }

  RecordReadTensorTime(std::move(read_tensor_timer));
}

void MLTensor::OnDidReadTensorByob(
    webnn::ScopedTrace scoped_trace,
    ScriptPromiseResolver<IDLUndefined>* resolver,
    AllowSharedBufferSource* dst_data,
    base::ElapsedTimer read_tensor_timer,
    webnn::mojom::blink::ReadTensorResultPtr result) {
  pending_byob_resolvers_.erase(resolver);

  if (result->is_error()) {
    const webnn::mojom::blink::Error& read_tensor_error = *result->get_error();
    resolver->RejectWithDOMException(
        WebNNErrorCodeToDOMExceptionCode(read_tensor_error.code),
        read_tensor_error.message);
    return;
  }

  base::span<uint8_t> bytes = AsByteSpan(*dst_data);
  if (bytes.size() == 0) {
    resolver->RejectWithTypeError("Buffer was detached.");
    return;
  }

  if (result->get_buffer().size() == 0) {
    if (!ml_context_->read_tensor_consumer()) {
      resolver->RejectWithDOMException(
          DOMExceptionCode::kInvalidStateError,
          "ReadTensor(): No data pipe to read tensor data.");
      return;
    }
    size_t bytes_read = 0;
    if (ml_context_->read_tensor_consumer()->ReadData(
            MOJO_READ_DATA_FLAG_ALL_OR_NONE,
            bytes.first(descriptor_.PackedByteLength()),
            bytes_read) != MOJO_RESULT_OK) {
      resolver->RejectWithDOMException(
          DOMExceptionCode::kDataError,
          "ReadTensor(): Failed to read tensor data from the data pipe.");
      return;
    }
    CHECK_EQ(bytes_read, descriptor_.PackedByteLength());
  } else {
    CHECK_EQ(result->get_buffer().size(), descriptor_.PackedByteLength());

    // It is safe to write into `dst_data` even though it was not transferred
    // because this method is called in a task which runs on same thread where
    // script executes, so script can't observe a partially written state
    // (unless `dst_data` is a SharedArrayBuffer).
    bytes.copy_prefix_from(result->get_buffer());
  }

  resolver->Resolve();

  RecordReadTensorTime(std::move(read_tensor_timer));
}

void MLTensor::WriteTensorImpl(base::span<const uint8_t> src_data,
                               ExceptionState& exception_state) {
  // Remote context gets automatically unbound when the execution context
  // destructs.
  if (!remote_tensor_.is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kTensorDestroyedError);
    return;
  }

  if (gpu_buffer_) {
    exception_state.ThrowTypeError(kTensorExportedError);
    return;
  }

  // Return early since empty written data can be ignored with no observable
  // effect.
  if (src_data.size() == 0) {
    return;
  }

  // Copy src data.
  if (ml_context_->write_tensor_producer() &&
      src_data.size() > mojo_base::BigBuffer::kMaxInlineBytes &&
      ml_context_->write_tensor_producer()->WriteAllData(src_data) ==
          MOJO_RESULT_OK) {
    remote_tensor_->WriteTensor({});
  } else {
    remote_tensor_->WriteTensor(src_data);
  }
}

void MLTensor::OnConnectionError() {
  remote_tensor_.reset();

  for (const auto& resolver : pending_resolvers_) {
    resolver->RejectWithDOMException(DOMExceptionCode::kInvalidStateError,
                                     kTensorDestroyedError);
  }
  pending_resolvers_.clear();

  for (const auto& resolver : pending_byob_resolvers_) {
    resolver->RejectWithDOMException(DOMExceptionCode::kInvalidStateError,
                                     kTensorDestroyedError);
  }
  pending_byob_resolvers_.clear();

  if (pending_gpu_buffer_resolver_) {
    pending_gpu_buffer_resolver_->RejectWithDOMException(
        DOMExceptionCode::kInvalidStateError, kTensorDestroyedError);
  }
  pending_gpu_buffer_resolver_.Clear();
}

// MLTensor::ExportToGPUImpl creates a GPUBuffer on top of the shared
// image. We use 3 different IPC interfaces to talk to the GPU service, hence
// we need to rely on sync tokens for synchronizing different GPU contexts. The
// high level approach is: before using the buffer on a given interface, wait
// on a sync token that was generated by a context on which the buffer
// originated from.
//
// Key:
// - SII - SharedImageInterface
// - WebGPU - WebGPUInterface
// - WebNN - WebNNContext
// - s1 - SI created by SharedImage service.
// - t1 - SyncToken created for SharedImage service.
// - t2 - SyncToken created for WebNN service.
// - t3 - SyncToken created for WebGPU service.
//
// clang-format off
//
//           SII                      WebGPU                    WebNN
//       s1=CreateSI()                   │                        │
//       t1=GenSyncToken()               |                        |
//            |                          |                 WaitSyncToken(t1)
//            |                          |                 CreateTensor(s1)
//            |                          │                 t2=GenSyncToken()
//            │                   WaitSyncToken(t2)               │
//            │                   Associate(s1)                   │
//            │                          |                        │
//
// Once WebGPU destroyed the buffer, WebNN resumes use of the tensor since the
// SharedImage only exists to access the tensor from WebGPU.
//
//            |                    Dissociate(s1)                 |
//            |                    t3=GenSyncToken()              |
//            |                          |                        |
//        DestroySI(s1)                  |                        │
//            │                          |                  WaitSyncToken(t3)
//            |                          |                        |
//
// clang-format off
//
// The method is annotated with the comment taken from the diagram above to make
// it more clear which part of the code corresponds to which step.
ScriptPromise<GPUBuffer> MLTensor::ExportToGPUImpl(
    webnn::ScopedTrace scoped_trace,
    ScriptState* script_state,
    ExceptionState& exception_state) {

  if (!gpu_device_) {
    exception_state.ThrowTypeError(kTensorWebGPUInteropUnsupportedError);
    return EmptyPromise();
  }

  // Remote context gets automatically unbound when the execution context
  // destructs.
  if (!remote_tensor_.is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kTensorDestroyedError);
    return EmptyPromise();
  }

  if (pending_gpu_buffer_resolver_) {
    exception_state.ThrowTypeError("Tensor pending export to WebGPU.");
    return EmptyPromise();
  }

  if (gpu_buffer_) {
    return ToResolvedPromise<GPUBuffer>(script_state, gpu_buffer_);
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<GPUBuffer>>(
      script_state, exception_state.GetContext());
  pending_gpu_buffer_resolver_ = resolver;

  remote_tensor_->ExportTensor(
      blink::BindOnce(&MLTensor::OnDidExportTensor, WrapPersistent(this),
                    std::move(scoped_trace), WrapPersistent(resolver)));

  return resolver->Promise();
}

void MLTensor::OnDidExportTensor(
    webnn::ScopedTrace scoped_trace,
    ScriptPromiseResolver<GPUBuffer>* resolver,
    base::expected<gpu::SyncToken, webnn::mojom::blink::ErrorPtr> result) {
  pending_gpu_buffer_resolver_ = nullptr;

  if (!result.has_value()) {
    const webnn::mojom::blink::Error& export_tensor_error = *result.error();
    resolver->RejectWithDOMException(
        WebNNErrorCodeToDOMExceptionCode(export_tensor_error.code),
        export_tensor_error.message);
    return;
  }

  if (gpu_device_->IsDestroyed()) {
    resolver->RejectWithDOMException(DOMExceptionCode::kInvalidStateError,
                                     "GPUDevice was lost or destroyed.");
    return;
  }

  auto webgpu_finished_access_callback = blink::BindOnce(
      [](MLTensor* tensor, gpu::ClientSharedImage* shared_image,
         const gpu::SyncToken& webgpu_finished_access_token) {
        // Update the SyncToken to ensure that we will wait for it even if we
        // immediately destroy the exported tensor.
        shared_image->UpdateDestructionSyncToken(webgpu_finished_access_token);

        // If the tensor is missing, it must be destroyed and cannot be
        // imported again.
        if (!tensor || !tensor->IsValid()) {
          return;
        }

        // WaitSyncToken(t3)
        // WebNNTensor::ImportTensor calls WaitSyncToken.
        tensor->remote_tensor_->ImportTensor(webgpu_finished_access_token);

        // Resume use of MLTensor.
        tensor->gpu_buffer_.Clear();
      },
      WrapWeakPersistent(this), base::RetainedRef(shared_image_));

  // TODO(crbug.com/345352987): use the label from MLTensor.
  const wgpu::BufferDescriptor tensor_buffer_desc = {
      .label = "tensor",
      .usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc |
               wgpu::BufferUsage::CopyDst,
      .size = PackedByteLength(),
  };

  // WaitSyncToken(t2)
  // ClientSharedImage::BeginWebGPUBufferAccess calls WaitSyncToken.
  scoped_refptr<WebGPUMailboxBuffer> mailbox_buffer =
      WebGPUMailboxBuffer::FromExistingSharedImage(
          gpu_device_->GetDawnControlClient(), gpu_device_->GetHandle(),
          tensor_buffer_desc, shared_image_, result.value(),
          std::move(webgpu_finished_access_callback));
  CHECK(mailbox_buffer);

  gpu_buffer_ = MakeGarbageCollected<GPUBuffer>(gpu_device_, tensor_buffer_desc.size,
                                                std::move(mailbox_buffer),
                                                String::FromUTF8(tensor_buffer_desc.label));
  resolver->Resolve(gpu_buffer_);
}

}  // namespace blink
