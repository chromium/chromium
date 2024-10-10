// Copyright 2019 The Chromium Authors
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
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_buffer_map_state.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_adapter.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_queue.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_callback.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

// A size that if used to create a dawn_wire buffer, will guarantee we'll OOM
// immediately. It is an implementation detail of dawn_wire but that's tested
// on CQ in Dawn. Note that we set kGuaranteedBufferOOMSize to
// (wgpu::kWholeMapSize - 1) to ensure we never pass wgpu::kWholeMapSize from
// blink to wire_client.
constexpr uint64_t kGuaranteedBufferOOMSize = wgpu::kWholeMapSize - 1u;

wgpu::BufferDescriptor AsDawnType(const GPUBufferDescriptor* webgpu_desc,
                                  std::string* label) {
  DCHECK(webgpu_desc);
  DCHECK(label);

  wgpu::BufferDescriptor dawn_desc = {
      .usage = AsDawnFlags<wgpu::BufferUsage>(webgpu_desc->usage()),
      .size = webgpu_desc->size(),
      .mappedAtCreation = webgpu_desc->mappedAtCreation(),
  };
  *label = webgpu_desc->label().Utf8();
  if (!label->empty()) {
    dawn_desc.label = label->c_str();
  }

  return dawn_desc;
}

}  // namespace

// GPUMappedDOMArrayBuffer is returned from mappings created from
// GPUBuffer which point to shared memory. This memory is owned by
// the underlying wgpu::Buffer used to implement GPUBuffer.
// GPUMappedDOMArrayBuffer exists because mapped DOMArrayBuffers need
// to keep their owning GPUBuffer alive, or the shared memory may be
// freed while it is in use. It derives from DOMArrayBuffer and holds
// a Member<GPUBuffer> to its owner. Alternative ideas might be to keep
// the wgpu::Buffer alive using a custom deleter of v8::BackingStore or
// ArrayBufferContents. However, since these are non-GC objects, it
// becomes complex to handle destruction when the last reference to
// the wgpu::Buffer may be held either by a GC object, or a non-GC object.
class GPUMappedDOMArrayBuffer : public DOMArrayBuffer {
  static constexpr char kWebGPUBufferMappingDetachKey[] = "WebGPUBufferMapping";

 public:
  static GPUMappedDOMArrayBuffer* Create(v8::Isolate* isolate,
                                         GPUBuffer* owner,
                                         ArrayBufferContents contents) {
    auto* mapped_array_buffer = MakeGarbageCollected<GPUMappedDOMArrayBuffer>(
        owner, std::move(contents));
    mapped_array_buffer->SetDetachKey(isolate, kWebGPUBufferMappingDetachKey);
    return mapped_array_buffer;
  }

  GPUMappedDOMArrayBuffer(GPUBuffer* owner, ArrayBufferContents contents)
      : DOMArrayBuffer(std::move(contents)), owner_(owner) {}
  ~GPUMappedDOMArrayBuffer() override = default;

  void DetachContents(v8::Isolate* isolate) {
    if (IsDetached()) {
      return;
    }
    NonThrowableExceptionState exception_state;
    // Detach the array buffer by transferring the contents out and dropping
    // them.
    ArrayBufferContents contents;
    bool result = DOMArrayBuffer::Transfer(
        isolate, V8AtomicString(isolate, kWebGPUBufferMappingDetachKey),
        contents, exception_state);
    // TODO(crbug.com/1326210): Temporary CHECK to prevent aliased array
    // buffers.
    CHECK(result && IsDetached());
  }

  // Due to an unusual non-owning backing these array buffers can't be shared
  // for internal use.
  bool ShareNonSharedForInternalUse(ArrayBufferContents& result) override {
    result.Detach();
    return false;
  }

  void Trace(Visitor* visitor) const override {
    DOMArrayBuffer::Trace(visitor);
    visitor->Trace(owner_);
  }

 private:
  Member<GPUBuffer> owner_;
};

// static
GPUBuffer* GPUBuffer::Create(GPUDevice* device,
                             const GPUBufferDescriptor* webgpu_desc,
                             ExceptionState& exception_state) {
  DCHECK(device);

  std::string label;
  wgpu::BufferDescriptor dawn_desc = AsDawnType(webgpu_desc, &label);

  // Save the requested size of the buffer, for reflection and defaults.
  uint64_t buffer_size = dawn_desc.size;
  // If the buffer is mappable, make sure the size stays in a size_t but still
  // guarantees that we have an OOM.
  bool is_mappable = dawn_desc.usage & (wgpu::BufferUsage::MapRead |
                                        wgpu::BufferUsage::MapWrite) ||
                     dawn_desc.mappedAtCreation;
  if (is_mappable) {
    dawn_desc.size = std::min(dawn_desc.size, kGuaranteedBufferOOMSize);
  }

  wgpu::Buffer wgpuBuffer = device->GetHandle().CreateBuffer(&dawn_desc);
  // dawn_wire::client will return nullptr when mappedAtCreation == true and
  // dawn_wire::client fails to allocate memory for initializing an active
  // buffer mapping, which is required by latest WebGPU SPEC.
  if (wgpuBuffer == nullptr) {
    DCHECK(dawn_desc.mappedAtCreation);
    exception_state.ThrowRangeError(
        "createBuffer failed, size is too large for the implementation when "
        "mappedAtCreation == true");
    return nullptr;
  }

  GPUBuffer* buffer = MakeGarbageCollected<GPUBuffer>(
      device, buffer_size, std::move(wgpuBuffer), webgpu_desc->label());

  if (is_mappable) {
    GPU* gpu = device->adapter()->gpu();
    gpu->TrackMappableBuffer(buffer);
    device->TrackMappableBuffer(buffer);
    buffer->mappable_buffer_handles_ = gpu->mappable_buffer_handles();
  }

  return buffer;
}

GPUBuffer::GPUBuffer(GPUDevice* device,
                     uint64_t size,
                     wgpu::Buffer buffer,
                     const String& label)
    : DawnObject<wgpu::Buffer>(device, std::move(buffer), label), size_(size) {}

GPUBuffer::~GPUBuffer() {
  if (mappable_buffer_handles_) {
    mappable_buffer_handles_->erase(GetHandle());
  }
}

void GPUBuffer::Trace(Visitor* visitor) const {
  visitor->Trace(mapped_array_buffers_);
  DawnObject<wgpu::Buffer>::Trace(visitor);
}

ScriptPromise<IDLUndefined> GPUBuffer::mapAsync(
    ScriptState* script_state,
    uint32_t mode,
    uint64_t offset,
    ExceptionState& exception_state) {
  return MapAsyncImpl(script_state, mode, offset, std::nullopt,
                      exception_state);
}

ScriptPromise<IDLUndefined> GPUBuffer::mapAsync(
    ScriptState* script_state,
    uint32_t mode,
    uint64_t offset,
    uint64_t size,
    ExceptionState& exception_state) {
  return MapAsyncImpl(script_state, mode, offset, size, exception_state);
}

DOMArrayBuffer* GPUBuffer::getMappedRange(ScriptState* script_state,
                                          uint64_t offset,
                                          ExceptionState& exception_state) {
  return GetMappedRangeImpl(script_state, offset, std::nullopt,
                            exception_state);
}

DOMArrayBuffer* GPUBuffer::getMappedRange(ScriptState* script_state,
                                          uint64_t offset,
                                          uint64_t size,
                                          ExceptionState& exception_state) {
  return GetMappedRangeImpl(script_state, offset, size, exception_state);
}

void GPUBuffer::unmap(v8::Isolate* isolate) {
  ResetMappingState(isolate);
  GetHandle().Unmap();
}

void GPUBuffer::destroy(v8::Isolate* isolate) {
  ResetMappingState(isolate);
  GetHandle().Destroy();
  // Destroyed, so it can never be mapped again. Stop tracking.
  device_->adapter()->gpu()->UntrackMappableBuffer(this);
  device_->UntrackMappableBuffer(this);
  // Drop the reference to the mapped buffer handles. No longer
  // need to remove the wgpu::Buffer from this set in ~GPUBuffer.
  mappable_buffer_handles_ = nullptr;
}

uint64_t GPUBuffer::size() const {
  return size_;
}

uint32_t GPUBuffer::usage() const {
  return static_cast<uint32_t>(GetHandle().GetUsage());
}

V8GPUBufferMapState GPUBuffer::mapState() const {
  return FromDawnEnum(GetHandle().GetMapState());
}

ScriptPromise<IDLUndefined> GPUBuffer::MapAsyncImpl(
    ScriptState* script_state,
    uint32_t mode,
    uint64_t offset,
    std::optional<uint64_t> size,
    ExceptionState& exception_state) {
  // Compute the defaulted size which is "until the end of the buffer" or 0 if
  // offset is past the end of the buffer.
  uint64_t size_defaulted = 0;
  if (size) {
    size_defaulted = *size;
  } else if (offset <= size_) {
    size_defaulted = size_ - offset;
  }

  // We need to convert from uint64_t to size_t. Either of these two variables
  // are bigger or equal to the guaranteed OOM size then mapAsync should be an
  // error so. That OOM size fits in a size_t so we can clamp size and offset
  // with it.
  size_t map_offset =
      static_cast<size_t>(std::min(offset, kGuaranteedBufferOOMSize));
  size_t map_size =
      static_cast<size_t>(std::min(size_defaulted, kGuaranteedBufferOOMSize));

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  // And send the command, leaving remaining validation to Dawn.
  auto* callback = MakeWGPUOnceCallback(resolver->WrapCallbackInScriptScope(
      WTF::BindOnce(&GPUBuffer::OnMapAsyncCallback, WrapPersistent(this))));

  GetHandle().MapAsync(static_cast<wgpu::MapMode>(mode), map_offset, map_size,
                       wgpu::CallbackMode::AllowSpontaneous,
                       callback->UnboundCallback(), callback->AsUserdata());

  // WebGPU guarantees that promises are resolved in finite time so we
  // need to ensure commands are flushed.
  EnsureFlush(ToEventLoop(script_state));
  return promise;
}

DOMArrayBuffer* GPUBuffer::GetMappedRangeImpl(ScriptState* script_state,
                                              uint64_t offset,
                                              std::optional<uint64_t> size,
                                              ExceptionState& exception_state) {
  // Compute the defaulted size which is "until the end of the buffer" or 0 if
  // offset is past the end of the buffer.
  uint64_t size_defaulted = 0;
  if (size) {
    size_defaulted = *size;
  } else if (offset <= size_) {
    size_defaulted = size_ - offset;
  }

  // We need to convert from uint64_t to size_t. Either of these two variables
  // are bigger or equal to the guaranteed OOM size then getMappedRange should
  // be an error so. That OOM size fits in a size_t so we can clamp size and
  // offset with it.
  size_t range_offset =
      static_cast<size_t>(std::min(offset, kGuaranteedBufferOOMSize));
  size_t range_size =
      static_cast<size_t>(std::min(size_defaulted, kGuaranteedBufferOOMSize));

  if (range_size > std::numeric_limits<size_t>::max() - range_offset) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kOperationError,
        "getMappedRange failed, offset + size overflows size_t");
    return nullptr;
  }
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
  const void* map_data_const =
      GetHandle().GetConstMappedRange(range_offset, range_size);

  if (!map_data_const) {
    // Ensure that GPU process error messages are bubbled back to the renderer process.
    EnsureFlush(ToEventLoop(script_state));
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "getMappedRange failed");
    return nullptr;
  }

  // The maximum size that can be mapped in JS so that we can ensure we don't
  // create mappable buffers bigger than it. According to ECMAScript SPEC, a
  // RangeError exception will be thrown if it is impossible to allocate an
  // array buffer.
  // This could eventually be upgrade to the max ArrayBuffer size instead of the
  // max TypedArray size. See crbug.com/951196
  // Note that we put this check after the checks in Dawn because the latest
  // WebGPU SPEC requires the checks on the buffer state (mapped or not) should
  // be done before the creation of ArrayBuffer.
  if (range_size > v8::TypedArray::kMaxByteLength) {
    exception_state.ThrowRangeError(
        "getMappedRange failed, size is too large for the implementation");
    return nullptr;
  }

  // It is safe to const_cast the |data| pointer because it is a shadow
  // copy that Dawn wire makes and does not point to the mapped GPU
  // data. Dawn wire's copy of the data is not used outside of tests.
  uint8_t* map_data =
      const_cast<uint8_t*>(static_cast<const uint8_t*>(map_data_const));

  mapped_ranges_.push_back(std::make_pair(range_offset, range_end));
  return CreateArrayBufferForMappedData(script_state->GetIsolate(), map_data,
                                        range_size);
}

void GPUBuffer::OnMapAsyncCallback(
    ScriptPromiseResolver<IDLUndefined>* resolver,
    wgpu::MapAsyncStatus status,
    const char* message) {
  switch (status) {
    case wgpu::MapAsyncStatus::Success:
      resolver->Resolve();
      break;
    case wgpu::MapAsyncStatus::InstanceDropped:
      resolver->RejectWithDOMException(DOMExceptionCode::kAbortError, message);
      break;
    case wgpu::MapAsyncStatus::Error:
      resolver->RejectWithDOMException(DOMExceptionCode::kOperationError,
                                       message);
      break;
    case wgpu::MapAsyncStatus::Aborted:
      resolver->RejectWithDOMException(DOMExceptionCode::kAbortError, message);
      break;
    case wgpu::MapAsyncStatus::Unknown:
      resolver->RejectWithDOMException(DOMExceptionCode::kOperationError,
                                       message);
      break;
  }
}

DOMArrayBuffer* GPUBuffer::CreateArrayBufferForMappedData(v8::Isolate* isolate,
                                                          void* data,
                                                          size_t data_length) {
  DCHECK(data);
  DCHECK_LE(static_cast<uint64_t>(data_length), v8::TypedArray::kMaxByteLength);

  ArrayBufferContents contents(v8::ArrayBuffer::NewBackingStore(
      data, data_length, v8::BackingStore::EmptyDeleter, nullptr));
  GPUMappedDOMArrayBuffer* array_buffer =
      GPUMappedDOMArrayBuffer::Create(isolate, this, contents);
  mapped_array_buffers_.push_back(array_buffer);
  return array_buffer;
}

void GPUBuffer::ResetMappingState(v8::Isolate* isolate) {
  mapped_ranges_.clear();
  DetachMappedArrayBuffers(isolate);
}

void GPUBuffer::DetachMappedArrayBuffers(v8::Isolate* isolate) {
  for (Member<GPUMappedDOMArrayBuffer>& mapped_array_buffer :
       mapped_array_buffers_) {
    GPUMappedDOMArrayBuffer* array_buffer = mapped_array_buffer.Release();
    array_buffer->DetachContents(isolate);
  }
  mapped_array_buffers_.clear();
}

}  // namespace blink
