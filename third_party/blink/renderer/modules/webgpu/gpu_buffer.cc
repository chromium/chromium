// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_buffer.h"

#include <utility>

#include "gpu/command_buffer/client/webgpu_interface.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_callback.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_buffer_descriptor.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

// crbug.com/951196
// Currently, this value is less than the maximum ArrayBuffer length which is
// theoretically 2^53 - 1 (Number.MAX_SAFE_INTEGER). However, creating a typed
// array from an ArrayBuffer of size greater than TypedArray::kMaxLength crashes
// DevTools and gives obscure errors.
constexpr size_t kLargestMappableSize = v8::TypedArray::kMaxLength;

bool ValidateMapSize(uint64_t buffer_size,
                     ExceptionState& exception_state) {
  if (buffer_size > kLargestMappableSize) {
    WTF::StringBuilder message_builder;
    message_builder.Append(WTF::StringView("Buffer of "));
    message_builder.AppendNumber(buffer_size);
    message_builder.Append(WTF::StringView(" bytes too large for mapping"));

    WTF::String message = message_builder.ToString();

    exception_state.ThrowRangeError(message);
    return false;
  }
  return true;
}

WGPUBufferDescriptor AsDawnType(const GPUBufferDescriptor* webgpu_desc) {
  DCHECK(webgpu_desc);

  WGPUBufferDescriptor dawn_desc = {};
  dawn_desc.nextInChain = nullptr;
  dawn_desc.usage = AsDawnEnum<WGPUBufferUsage>(webgpu_desc->usage());
  dawn_desc.size = webgpu_desc->size();
  if (webgpu_desc->hasLabel()) {
    dawn_desc.label = webgpu_desc->label().Utf8().data();
  }

  return dawn_desc;
}

DOMArrayBuffer* CreateArrayBufferForMappedData(void* data, size_t data_length) {
  DCHECK(data);

  ArrayBufferContents contents(data, data_length,
                               [](void* data, size_t length, void* info) {
                                 // DataDeleter does nothing because Dawn wire
                                 // owns the memory.
                               });

  return DOMArrayBuffer::Create(contents);
}

}  // namespace

// static
GPUBuffer* GPUBuffer::Create(GPUDevice* device,
                             const GPUBufferDescriptor* webgpu_desc) {
  DCHECK(device);

  WGPUBufferDescriptor dawn_desc = AsDawnType(webgpu_desc);
  return MakeGarbageCollected<GPUBuffer>(
      device, dawn_desc.size,
      device->GetProcs().deviceCreateBuffer(device->GetHandle(), &dawn_desc));
}

// static
std::pair<GPUBuffer*, DOMArrayBuffer*> GPUBuffer::CreateMapped(
    GPUDevice* device,
    const GPUBufferDescriptor* webgpu_desc,
    ExceptionState& exception_state) {
  DCHECK(device);

  WGPUBufferDescriptor dawn_desc = AsDawnType(webgpu_desc);

  if (!ValidateMapSize(dawn_desc.size, exception_state)) {
    return std::make_pair(nullptr, nullptr);
  }

  WGPUCreateBufferMappedResult result =
      device->GetProcs().deviceCreateBufferMapped(device->GetHandle(),
                                                  &dawn_desc);

  GPUBuffer* gpu_buffer =
      MakeGarbageCollected<GPUBuffer>(device, dawn_desc.size, result.buffer);

  DCHECK_LE(result.dataLength, kLargestMappableSize);
  DCHECK(result.data);
  gpu_buffer->mapped_buffer_ = CreateArrayBufferForMappedData(
      result.data, static_cast<size_t>(result.dataLength));

  return std::make_pair(gpu_buffer, gpu_buffer->mapped_buffer_);
}

GPUBuffer::GPUBuffer(GPUDevice* device, uint64_t size, WGPUBuffer buffer)
    : DawnObject<WGPUBuffer>(device, buffer), size_(size) {}

GPUBuffer::~GPUBuffer() {
  if (IsDawnControlClientDestroyed()) {
    return;
  }
  GetProcs().bufferRelease(GetHandle());
}

void GPUBuffer::Trace(blink::Visitor* visitor) {
  visitor->Trace(mapped_buffer_);
  DawnObject<WGPUBuffer>::Trace(visitor);
}

void GPUBuffer::setSubData(uint64_t dst_byte_offset,
                           const FlexibleArrayBufferView& src,
                           uint64_t src_byte_offset,
                           uint64_t byte_length,
                           ExceptionState& exception_state) {
  const uint8_t* src_base =
      reinterpret_cast<const uint8_t*>(src.BaseAddressMaybeOnStack());
  size_t src_byte_length = src.ByteLength();

  if (src_byte_offset > src_byte_length) {
    exception_state.ThrowRangeError("srcOffset is too large");
    return;
  }

  if (byte_length == 0) {
    byte_length = src_byte_length - src_byte_offset;
  } else if (byte_length > src_byte_length - src_byte_offset) {
    exception_state.ThrowRangeError("byteLength is too large");
    return;
  }

  const uint8_t* data = &src_base[src_byte_offset];
  GetProcs().bufferSetSubData(GetHandle(), dst_byte_offset, byte_length, data);
}

void GPUBuffer::OnMapAsyncCallback(ScriptPromiseResolver* resolver,
                                   WGPUBufferMapAsyncStatus status,
                                   void* data,
                                   uint64_t data_length) {
  switch (status) {
    case WGPUBufferMapAsyncStatus_Success:
      DCHECK(data);
      DCHECK_LE(data_length, kLargestMappableSize);
      mapped_buffer_ = CreateArrayBufferForMappedData(
          data, static_cast<size_t>(data_length));
      resolver->Resolve(mapped_buffer_);
      break;
    case WGPUBufferMapAsyncStatus_Error:
    case WGPUBufferMapAsyncStatus_Unknown:
    case WGPUBufferMapAsyncStatus_DeviceLost:
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kOperationError));
      break;
    default:
      NOTREACHED();
      resolver->Reject(
          MakeGarbageCollected<DOMException>(DOMExceptionCode::kAbortError));
      break;
  }
}

ScriptPromise GPUBuffer::mapReadAsync(ScriptState* script_state,
                                      ExceptionState& exception_state) {
  ScriptPromiseResolver* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  if (!ValidateMapSize(size_, exception_state)) {
    resolver->Reject(exception_state);
    return promise;
  }

  auto* callback =
      BindDawnCallback(&GPUBuffer::OnMapAsyncCallback, WrapPersistent(this),
                       WrapPersistent(resolver));

  using Callback = std::remove_reference_t<decltype(*callback)>;

  GetProcs().bufferMapReadAsync(
      GetHandle(),
      [](WGPUBufferMapAsyncStatus status, const void* data,
         uint64_t data_length, void* userdata) {
        // It is safe to const_cast the |data| pointer because it is a shadow
        // copy that Dawn wire makes and does not point to the mapped GPU data.
        // Dawn wire's copy of the data is not used outside of tests.
        return Callback::CallUnboundCallback(status, const_cast<void*>(data),
                                             data_length, userdata);
      },
      callback->AsUserdata());
  // WebGPU guarantees callbacks complete in finite time. Flush now so that
  // commands reach the GPU process.
  device_->GetInterface()->FlushCommands();

  return promise;
}

ScriptPromise GPUBuffer::mapWriteAsync(ScriptState* script_state,
                                       ExceptionState& exception_state) {
  ScriptPromiseResolver* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  if (!ValidateMapSize(size_, exception_state)) {
    resolver->Reject(exception_state);
    return promise;
  }

  auto* callback =
      BindDawnCallback(&GPUBuffer::OnMapAsyncCallback, WrapPersistent(this),
                       WrapPersistent(resolver));

  GetProcs().bufferMapWriteAsync(GetHandle(), callback->UnboundCallback(),
                                 callback->AsUserdata());
  // WebGPU guarantees callbacks complete in finite time. Flush now so that
  // commands reach the GPU process.
  device_->GetInterface()->FlushCommands();

  return promise;
}

void GPUBuffer::unmap(ScriptState* script_state) {
  DetachArrayBufferForCurrentMapping(script_state);
  GetProcs().bufferUnmap(GetHandle());
}

void GPUBuffer::destroy(ScriptState* script_state) {
  DetachArrayBufferForCurrentMapping(script_state);
  GetProcs().bufferDestroy(GetHandle());
}

void GPUBuffer::DetachArrayBufferForCurrentMapping(ScriptState* script_state) {
  if (!mapped_buffer_) {
    return;
  }
  v8::Isolate* isolate = script_state->GetIsolate();
  DOMArrayBuffer* mapped_buffer = mapped_buffer_.Release();
  DCHECK(mapped_buffer->IsDetachable(isolate));

  // Detach the array buffer by transferring the contents out and dropping them.
  ArrayBufferContents contents;
  DCHECK(mapped_buffer->Transfer(isolate, contents));
}

}  // namespace blink
