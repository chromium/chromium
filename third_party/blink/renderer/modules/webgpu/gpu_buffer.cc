// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_buffer.h"

#include <cinttypes>
#include <utility>

#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "gpu/command_buffer/client/webgpu_interface.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_buffer_descriptor.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_callback.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_queue.h"
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

bool ValidateRangeCreation(ExceptionState& exception_state,
                           const char* function_name,
                           uint64_t mapping_offset,
                           uint64_t mapping_size,
                           size_t max_size) {
  if (mapping_size > uint64_t(max_size) ||
      mapping_offset > uint64_t(max_size) - mapping_size) {
    exception_state.ThrowRangeError(
        WTF::String::Format("%s offset (%" PRIu64 " bytes) and size (%" PRIu64
                            " bytes) are too large for this implementation.",
                            function_name, mapping_offset, mapping_size));
    return false;
  }

  // TODO(crbug.com/dawn/22): Move this validation into Dawn (in both
  // getMappedRange and mapAsync).
  if (mapping_offset % 8 != 0) {
    exception_state.ThrowRangeError(WTF::String::Format(
        "%s offset (%" PRIu64 " bytes) is not a multiple of 8.", function_name,
        mapping_offset));
    return false;
  }

  return true;
}

WGPUBufferDescriptor AsDawnType(const GPUBufferDescriptor* webgpu_desc,
                                std::string* label) {
  DCHECK(webgpu_desc);
  DCHECK(label);

  WGPUBufferDescriptor dawn_desc = {};
  dawn_desc.nextInChain = nullptr;
  dawn_desc.usage = AsDawnEnum<WGPUBufferUsage>(webgpu_desc->usage());
  dawn_desc.size = webgpu_desc->size();
  dawn_desc.mappedAtCreation = webgpu_desc->mappedAtCreation();
  if (webgpu_desc->hasLabel()) {
    *label = webgpu_desc->label().Utf8();
    dawn_desc.label = label->c_str();
  }

  return dawn_desc;
}

}  // namespace

// static
GPUBuffer* GPUBuffer::Create(GPUDevice* device,
                             const GPUBufferDescriptor* webgpu_desc) {
  DCHECK(device);

  std::string label;
  WGPUBufferDescriptor dawn_desc = AsDawnType(webgpu_desc, &label);
  return MakeGarbageCollected<GPUBuffer>(
      device, dawn_desc.size, dawn_desc.mappedAtCreation,
      device->GetProcs().deviceCreateBuffer(device->GetHandle(), &dawn_desc));
}

GPUBuffer::GPUBuffer(GPUDevice* device,
                     uint64_t size,
                     bool mapped_at_creation,
                     WGPUBuffer buffer)
    : DawnObject<WGPUBuffer>(device, buffer), size_(size) {
  if (mapped_at_creation) {
    map_start_ = 0;
    map_end_ = size;
  }
}

GPUBuffer::~GPUBuffer() {
  if (IsDawnControlClientDestroyed()) {
    return;
  }
  GetProcs().bufferRelease(GetHandle());
}

void GPUBuffer::Trace(Visitor* visitor) const {
  visitor->Trace(mapped_array_buffers_);
  DawnObject<WGPUBuffer>::Trace(visitor);
}

ScriptPromise GPUBuffer::mapAsync(ScriptState* script_state,
                                  uint32_t mode,
                                  uint64_t offset,
                                  ExceptionState& exception_state) {
  return MapAsyncImpl(script_state, mode, offset, base::nullopt,
                      exception_state);
}

ScriptPromise GPUBuffer::mapAsync(ScriptState* script_state,
                                  uint32_t mode,
                                  uint64_t offset,
                                  uint64_t size,
                                  ExceptionState& exception_state) {
  return MapAsyncImpl(script_state, mode, offset, size, exception_state);
}

DOMArrayBuffer* GPUBuffer::getMappedRange(uint64_t offset,
                                          ExceptionState& exception_state) {
  return GetMappedRangeImpl(offset, base::nullopt, exception_state);
}

DOMArrayBuffer* GPUBuffer::getMappedRange(uint64_t offset,
                                          uint64_t size,
                                          ExceptionState& exception_state) {
  return GetMappedRangeImpl(offset, size, exception_state);
}

void GPUBuffer::unmap(ScriptState* script_state) {
  ResetMappingState(script_state);
  GetProcs().bufferUnmap(GetHandle());
}

void GPUBuffer::destroy(ScriptState* script_state) {
  ResetMappingState(script_state);
  GetProcs().bufferDestroy(GetHandle());
}

ScriptPromise GPUBuffer::MapAsyncImpl(ScriptState* script_state,
                                      uint32_t mode,
                                      uint64_t offset,
                                      base::Optional<uint64_t> size,
                                      ExceptionState& exception_state) {
  ScriptPromiseResolver* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  // Compute the defaulted size (which is "until the end of the buffer".)
  // (First, guard against overflows on (size_ - offset).)
  if (offset > size_) {
    GetProcs().deviceInjectError(device_->GetHandle(), WGPUErrorType_Validation,
                                 "mapAsync offset is larger than the buffer");
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kOperationError,
        WTF::String::Format("mapAsync offset (%" PRIu64
                            " bytes) is larger than the buffer (%" PRIu64
                            " bytes).",
                            offset, size_)));
    return promise;
  }
  uint64_t size_defaulted = size ? *size : (size_ - offset);

  // Check the offset and size are within the limits of the platform.
  // (Note this also checks for an 8-byte alignment, which is an ArrayBuffer
  // restriction, even though an ArrayBuffer is not created here.)
  if (!ValidateRangeCreation(exception_state, "mapAsync", offset,
                             size_defaulted,
                             std::numeric_limits<size_t>::max())) {
    GetProcs().deviceInjectError(device_->GetHandle(), WGPUErrorType_Validation,
                                 "mapAsync arguments were invalid");
    resolver->Reject(exception_state);
    return promise;
  }
  size_t map_offset = static_cast<size_t>(offset);
  size_t map_size = static_cast<size_t>(size_defaulted);

  // And send the command, leaving remaining validation to Dawn.

  auto* callback =
      BindDawnCallback(&GPUBuffer::OnMapAsyncCallback, WrapPersistent(this),
                       WrapPersistent(resolver), offset, size_defaulted);

  GetProcs().bufferMapAsync(GetHandle(), mode, map_offset, map_size,
                            callback->UnboundCallback(),
                            callback->AsUserdata());
  // WebGPU guarantees that promises are resolved in finite time so we
  // need to ensure commands are flushed.
  EnsureFlush();
  return promise;
}

DOMArrayBuffer* GPUBuffer::GetMappedRangeImpl(uint64_t offset,
                                              base::Optional<uint64_t> size,
                                              ExceptionState& exception_state) {
  // Compute the defaulted size (which is "until the end of the buffer".)
  // (First, guard against overflows on (size_ - offset).)
  if (offset > size_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kOperationError,
        WTF::String::Format("getMappedRange offset (%" PRIu64
                            " bytes) is larger than the buffer (%" PRIu64
                            " bytes).",
                            offset, size_));
    return nullptr;
  }
  uint64_t size_defaulted = size ? *size : (size_ - offset);

  // Check the offset and size are within the limits of the platform and the
  // ArrayBuffer spec+implementation.
  if (!ValidateRangeCreation(exception_state, "getMappedRange", offset,
                             size_defaulted, kLargestMappableSize)) {
    return nullptr;
  }
  size_t range_offset = static_cast<size_t>(offset);
  size_t range_size = static_cast<size_t>(size_defaulted);
  size_t range_end = range_offset + range_size;

  // Check if an overlapping range has already been returned.
  // TODO: keep mapped_ranges_ sorted (e.g. std::map), and do a binary search
  // (e.g. map.upper_bound()) to make this O(lg(n)) instead of linear.
  // (Note: std::map is not allowed in Blink.)
  for (const auto& overlap_candidate : mapped_ranges_) {
    size_t candidate_start = overlap_candidate.first;
    size_t candidate_end = overlap_candidate.second;
    if (range_end > candidate_start && range_offset < candidate_end) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kOperationError,
          WTF::String::Format("getMappedRange [%zu, %zu) overlaps with "
                              "previously returned range [%zu, %zu).",
                              range_offset, range_end, candidate_start,
                              candidate_end));
      return nullptr;
    }
  }

  // And send the command, leaving remaining validation to Dawn.

  const void* map_data_const = GetProcs().bufferGetConstMappedRange(
      GetHandle(), range_offset, range_size);
  // It is safe to const_cast the |data| pointer because it is a shadow
  // copy that Dawn wire makes and does not point to the mapped GPU
  // data. Dawn wire's copy of the data is not used outside of tests.
  uint8_t* map_data =
      const_cast<uint8_t*>(static_cast<const uint8_t*>(map_data_const));

  if (!map_data) {
    // TODO: have explanatory error messages here (or just leave them to the
    // asynchronous error reporting).
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "getMappedRange failed");
    return nullptr;
  }

  mapped_ranges_.push_back(std::make_pair(range_offset, range_end));
  return CreateArrayBufferForMappedData(map_data, range_size);
}

void GPUBuffer::OnMapAsyncCallback(ScriptPromiseResolver* resolver,
                                   uint64_t map_start,
                                   uint64_t map_end,
                                   WGPUBufferMapAsyncStatus status) {
  switch (status) {
    case WGPUBufferMapAsyncStatus_Success:
      map_start_ = map_start;
      map_end_ = map_end;
      resolver->Resolve();
      break;
    case WGPUBufferMapAsyncStatus_Error:
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kOperationError, "Could not mapAsync"));
      break;
    case WGPUBufferMapAsyncStatus_Unknown:
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kOperationError, "Unknown error in mapAsync"));
      break;
    case WGPUBufferMapAsyncStatus_DeviceLost:
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kOperationError, "Device is lost"));
      break;
    default:
      NOTREACHED();
  }
}

DOMArrayBuffer* GPUBuffer::CreateArrayBufferForMappedData(void* data,
                                                          size_t data_length) {
  DCHECK(data);
  DCHECK_LE(data_length, kLargestMappableSize);

  ArrayBufferContents contents(data, data_length,
                               v8::BackingStore::EmptyDeleter);

  DOMArrayBuffer* array_buffer = DOMArrayBuffer::Create(contents);
  mapped_array_buffers_.push_back(array_buffer);
  return array_buffer;
}

void GPUBuffer::ResetMappingState(ScriptState* script_state) {
  map_start_ = 0;
  map_end_ = 0;
  mapped_ranges_.clear();

  for (Member<DOMArrayBuffer>& mapped_array_buffer : mapped_array_buffers_) {
    v8::Isolate* isolate = script_state->GetIsolate();
    DOMArrayBuffer* array_buffer = mapped_array_buffer.Release();
    DCHECK(array_buffer->IsDetachable(isolate));

    // Detach the array buffer by transferring the contents out and dropping
    // them.
    ArrayBufferContents contents;
    bool did_detach = array_buffer->Transfer(isolate, contents);

    // |did_detach| would be false if the buffer were already detached.
    DCHECK(did_detach);
    DCHECK(array_buffer->IsDetached());
  }
  mapped_array_buffers_.clear();
}

}  // namespace blink
