// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/dml/utils.h"

#include <string.h>

#include "base/bits.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/strings/strcat.h"
#include "base/trace_event/trace_event.h"
#include "services/webnn/dml/error.h"
#include "services/webnn/dml/tensor_impl_dml.h"
#include "services/webnn/public/cpp/webnn_errors.h"

namespace webnn::dml {

namespace {

const char kBackendName[] = "DirectML: ";

// Note that the element count is considered as 1 when the given dimensions is
// empty.
uint64_t CalculateElementCount(const std::vector<uint32_t>& dimensions,
                               const std::vector<uint32_t>& strides = {}) {
  base::CheckedNumeric<uint64_t> checked_element_count = 1;
  if (strides.empty()) {
    for (const auto& d : dimensions) {
      checked_element_count *= d;
    }
  } else {
    CHECK_EQ(dimensions.size(), strides.size());
    base::CheckedNumeric<uint32_t> index_of_last_element = 0;
    for (size_t i = 0; i < dimensions.size(); ++i) {
      index_of_last_element += (dimensions[i] - 1) * strides[i];
    }
    checked_element_count = index_of_last_element + 1;
  }

  return checked_element_count.ValueOrDie();
}

D3D12_HEAP_PROPERTIES CreateHeapProperties(D3D12_HEAP_TYPE type) {
  return {.Type = type,
          .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
          .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
          .CreationNodeMask = 1,
          .VisibleNodeMask = 1};
}

D3D12_RESOURCE_DESC CreateResourceDesc(
    uint64_t size,
    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE) {
  return {.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
          .Alignment = 0,
          .Width = size,
          .Height = 1,
          .DepthOrArraySize = 1,
          .MipLevels = 1,
          .Format = DXGI_FORMAT_UNKNOWN,
          .SampleDesc = {1, 0},
          .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
          .Flags = flags};
}

}  // namespace

uint64_t CalculateDMLBufferTensorSize(
    DML_TENSOR_DATA_TYPE data_type,
    const std::vector<uint32_t>& dimensions,
    const std::vector<uint32_t>& strides = {}) {
  uint64_t element_size_in_bits;
  switch (data_type) {
    case DML_TENSOR_DATA_TYPE_FLOAT64:
    case DML_TENSOR_DATA_TYPE_UINT64:
    case DML_TENSOR_DATA_TYPE_INT64:
      element_size_in_bits = 64;
      break;
    case DML_TENSOR_DATA_TYPE_FLOAT32:
    case DML_TENSOR_DATA_TYPE_UINT32:
    case DML_TENSOR_DATA_TYPE_INT32:
      element_size_in_bits = 32;
      break;
    case DML_TENSOR_DATA_TYPE_FLOAT16:
    case DML_TENSOR_DATA_TYPE_UINT16:
    case DML_TENSOR_DATA_TYPE_INT16:
      element_size_in_bits = 16;
      break;
    case DML_TENSOR_DATA_TYPE_UINT8:
    case DML_TENSOR_DATA_TYPE_INT8:
      element_size_in_bits = 8;
      break;
    case DML_TENSOR_DATA_TYPE_UINT4:
    case DML_TENSOR_DATA_TYPE_INT4:
      element_size_in_bits = 4;
      break;
    default:
      NOTREACHED();
  }

  base::CheckedNumeric<uint64_t> checked_buffer_length_in_bytes =
      (base::CheckedNumeric<uint64_t>(element_size_in_bits) *
           CalculateElementCount(dimensions, strides) +
       7) /
      8;

  // Calculate the total size of the tensor in bytes. It should be rounded up to
  // the nearest 4 bytes according to the alignment requirement:
  // https://learn.microsoft.com/en-us/windows/ai/directml/dml-helper-functions#dmlcalcbuffertensorsize
  uint64_t buffer_tensor_size = base::bits::AlignUp<uint64_t>(
      checked_buffer_length_in_bytes.ValueOrDie<uint64_t>(), 4);

  CHECK_NE(buffer_tensor_size, 0u);
  return buffer_tensor_size;
}

std::vector<uint32_t> CalculateStrides(base::span<const uint32_t> dimensions) {
  size_t dim_size = dimensions.size();
  std::vector<uint32_t> strides(dim_size);
  base::CheckedNumeric<uint32_t> stride = 1;
  for (size_t i = dim_size; i-- > 0;) {
    strides[i] = stride.ValueOrDie();
    stride *= dimensions[i];
  }
  return strides;
}

Microsoft::WRL::ComPtr<ID3D12Device> GetD3D12Device(IDMLDevice1* dml_device) {
  CHECK(dml_device);
  Microsoft::WRL::ComPtr<ID3D12Device> d3d12_device;
  CHECK_EQ(dml_device->GetParentDevice(IID_PPV_ARGS(&d3d12_device)), S_OK);
  return d3d12_device;
}

DML_FEATURE_LEVEL GetMaxSupportedDMLFeatureLevel(IDMLDevice1* dml_device) {
  CHECK(dml_device);

  // WebNN targets DML_FEATURE_LEVEL_4_0 for GPU and DML_FEATURE_LEVEL_6_4 for
  // NPU. Query all levels up to DML_FEATURE_LEVEL_4_0. This allows downlevel
  // hardware to still run unit-tests that may only require a lower level.
  DML_FEATURE_LEVEL feature_levels_requested[] = {
      DML_FEATURE_LEVEL_1_0, DML_FEATURE_LEVEL_2_0, DML_FEATURE_LEVEL_2_1,
      DML_FEATURE_LEVEL_3_0, DML_FEATURE_LEVEL_3_1, DML_FEATURE_LEVEL_4_0,
      DML_FEATURE_LEVEL_4_1, DML_FEATURE_LEVEL_5_0, DML_FEATURE_LEVEL_5_1,
      DML_FEATURE_LEVEL_5_2, DML_FEATURE_LEVEL_6_0, DML_FEATURE_LEVEL_6_1,
      DML_FEATURE_LEVEL_6_2, DML_FEATURE_LEVEL_6_4};

  DML_FEATURE_QUERY_FEATURE_LEVELS feature_levels_query = {
      std::size(feature_levels_requested), feature_levels_requested};

  // DML_FEATURE_FEATURE_LEVELS was introduced in DirectML version 1.1
  // and is not supported by DirectML version 1.0 which uses
  // DML_FEATURE_LEVEL_1_0.
  // https://learn.microsoft.com/en-us/windows/ai/directml/dml-feature-level-history
  DML_FEATURE_DATA_FEATURE_LEVELS feature_levels_supported = {};
  if (FAILED(dml_device->CheckFeatureSupport(
          DML_FEATURE_FEATURE_LEVELS, sizeof(feature_levels_query),
          &feature_levels_query, sizeof(feature_levels_supported),
          &feature_levels_supported))) {
    return DML_FEATURE_LEVEL_1_0;
  }

  return feature_levels_supported.MaxSupportedFeatureLevel;
}

std::string_view DMLFeatureLevelToString(DML_FEATURE_LEVEL dml_feature_level) {
  switch (dml_feature_level) {
    case DML_FEATURE_LEVEL_1_0:
      return "DML_FEATURE_LEVEL_1_0";
    case DML_FEATURE_LEVEL_2_0:
      return "DML_FEATURE_LEVEL_2_0";
    case DML_FEATURE_LEVEL_2_1:
      return "DML_FEATURE_LEVEL_2_1";
    case DML_FEATURE_LEVEL_3_0:
      return "DML_FEATURE_LEVEL_3_0";
    case DML_FEATURE_LEVEL_3_1:
      return "DML_FEATURE_LEVEL_3_1";
    case DML_FEATURE_LEVEL_4_0:
      return "DML_FEATURE_LEVEL_4_0";
    case DML_FEATURE_LEVEL_4_1:
      return "DML_FEATURE_LEVEL_4_1";
    case DML_FEATURE_LEVEL_5_0:
      return "DML_FEATURE_LEVEL_5_0";
    case DML_FEATURE_LEVEL_5_1:
      return "DML_FEATURE_LEVEL_5_1";
    case DML_FEATURE_LEVEL_5_2:
      return "DML_FEATURE_LEVEL_5_2";
    case DML_FEATURE_LEVEL_6_0:
      return "DML_FEATURE_LEVEL_6_0";
    case DML_FEATURE_LEVEL_6_1:
      return "DML_FEATURE_LEVEL_6_1";
    case DML_FEATURE_LEVEL_6_2:
      return "DML_FEATURE_LEVEL_6_2";
    case DML_FEATURE_LEVEL_6_4:
      return "DML_FEATURE_LEVEL_6_4";
    default:
      return "Unknown DML_FEATURE_LEVEL";
  }
}

D3D12_RESOURCE_BARRIER CreateTransitionBarrier(ID3D12Resource* resource,
                                               D3D12_RESOURCE_STATES before,
                                               D3D12_RESOURCE_STATES after) {
  return {.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
          .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
          .Transition = {.pResource = resource,
                         .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                         .StateBefore = before,
                         .StateAfter = after}};
}

void UploadBufferWithBarrier(CommandRecorder* command_recorder,
                             Microsoft::WRL::ComPtr<ID3D12Resource> dst_buffer,
                             Microsoft::WRL::ComPtr<ID3D12Resource> src_buffer,
                             size_t buffer_size) {
  TRACE_EVENT0("gpu", "dml::UploadBufferWithBarrier");
  // Copy the data from source buffer to destination buffer.
  D3D12_RESOURCE_BARRIER barriers[1];
  barriers[0] = CreateTransitionBarrier(dst_buffer.Get(),
                                        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                                        D3D12_RESOURCE_STATE_COPY_DEST);
  command_recorder->ResourceBarrier(barriers);
  command_recorder->CopyBufferRegion(dst_buffer, 0, std::move(src_buffer), 0,
                                     buffer_size);
  // The bound resources should be in D3D12_RESOURCE_STATE_UNORDERED_ACCESS
  // state before the execution of RecordDispatch on the GPU.
  barriers[0] =
      CreateTransitionBarrier(dst_buffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
                              D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
  command_recorder->ResourceBarrier(barriers);
}

void ReadbackBufferWithBarrier(
    CommandRecorder* command_recorder,
    Microsoft::WRL::ComPtr<ID3D12Resource> readback_buffer,
    Microsoft::WRL::ComPtr<ID3D12Resource> default_buffer,
    size_t buffer_size) {
  TRACE_EVENT0("gpu", "dml::ReadbackBufferWithBarrier");
  // Copy the data from source buffer to destination buffer.
  D3D12_RESOURCE_BARRIER barriers[1];
  barriers[0] = CreateTransitionBarrier(default_buffer.Get(),
                                        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                                        D3D12_RESOURCE_STATE_COPY_SOURCE);
  command_recorder->ResourceBarrier(barriers);
  command_recorder->CopyBufferRegion(std::move(readback_buffer), 0,
                                     default_buffer, 0, buffer_size);
  // The bound resources should be in D3D12_RESOURCE_STATE_UNORDERED_ACCESS
  // state before the execution of RecordDispatch on the GPU.
  barriers[0] = CreateTransitionBarrier(default_buffer.Get(),
                                        D3D12_RESOURCE_STATE_COPY_SOURCE,
                                        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
  command_recorder->ResourceBarrier(barriers);
}

void UploadTensorWithBarrier(CommandRecorder* command_recorder,
                             TensorImplDml* dst_tensor,
                             Microsoft::WRL::ComPtr<ID3D12Resource> src_buffer,
                             size_t buffer_size) {
  UploadBufferWithBarrier(command_recorder, dst_tensor->buffer(),
                          std::move(src_buffer), buffer_size);
  command_recorder->OnTensorAccessed(dst_tensor);
}

void ReadbackTensorWithBarrier(
    CommandRecorder* command_recorder,
    Microsoft::WRL::ComPtr<ID3D12Resource> dst_buffer,
    TensorImplDml* src_tensor,
    size_t buffer_size) {
  ReadbackBufferWithBarrier(command_recorder, dst_buffer, src_tensor->buffer(),
                            buffer_size);
  command_recorder->OnTensorAccessed(src_tensor);
}

mojom::ErrorPtr CreateError(mojom::Error::Code error_code,
                            const std::string& error_message,
                            std::string_view label) {
  LOG(ERROR) << "[WebNN] CreateError: " << error_message;
  if (!label.empty()) {
    return mojom::Error::New(
        error_code, base::StrCat({kBackendName, GetErrorLabelPrefix(label),
                                  error_message}));
  }
  return mojom::Error::New(error_code, kBackendName + error_message);
}

HRESULT CreateDefaultBuffer(ID3D12Device* device,
                            uint64_t size,
                            const wchar_t* name_for_debugging,
                            Microsoft::WRL::ComPtr<ID3D12Resource>& resource) {
  TRACE_EVENT2("gpu", "dml::CreateDefaultBuffer", "size", size, "name",
               name_for_debugging);
  auto heap_properties = CreateHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
  auto resource_desc =
      CreateResourceDesc(size, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
  RETURN_IF_FAILED(device->CreateCommittedResource(
      &heap_properties, D3D12_HEAP_FLAG_NONE, &resource_desc,
      D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&resource)));
  CHECK(resource.Get());

  CHECK_NE(name_for_debugging, nullptr);
  CHECK_EQ(resource->SetName(name_for_debugging), S_OK);
  return S_OK;
}

HRESULT CreateUploadBuffer(ID3D12Device* device,
                           uint64_t size,
                           const wchar_t* name_for_debugging,
                           Microsoft::WRL::ComPtr<ID3D12Resource>& resource) {
  TRACE_EVENT2("gpu", "dml::CreateUploadBuffer", "size", size, "name",
               name_for_debugging);
  auto heap_properties = CreateHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
  auto resource_desc = CreateResourceDesc(size, D3D12_RESOURCE_FLAG_NONE);
  RETURN_IF_FAILED(device->CreateCommittedResource(
      &heap_properties, D3D12_HEAP_FLAG_NONE, &resource_desc,
      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&resource)));
  CHECK(resource.Get());

  CHECK_NE(name_for_debugging, nullptr);
  CHECK_EQ(resource->SetName(name_for_debugging), S_OK);
  return S_OK;
}

HRESULT CreateReadbackBuffer(ID3D12Device* device,
                             uint64_t size,
                             const wchar_t* name_for_debugging,
                             Microsoft::WRL::ComPtr<ID3D12Resource>& resource) {
  CHECK(device);
  TRACE_EVENT2("gpu", "dml::CreateReadbackBuffer", "size", size, "name",
               name_for_debugging);
  auto heap_properties = CreateHeapProperties(D3D12_HEAP_TYPE_READBACK);
  auto resource_desc = CreateResourceDesc(size, D3D12_RESOURCE_FLAG_NONE);
  RETURN_IF_FAILED(device->CreateCommittedResource(
      &heap_properties, D3D12_HEAP_FLAG_NONE, &resource_desc,
      D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&resource)));
  CHECK(resource.Get());

  CHECK_NE(name_for_debugging, nullptr);
  CHECK_EQ(resource->SetName(name_for_debugging), S_OK);
  return S_OK;
}

HRESULT CreateCustomUploadBuffer(
    ID3D12Device* device,
    uint64_t size,
    const wchar_t* name_for_debugging,
    Microsoft::WRL::ComPtr<ID3D12Resource>& resource) {
  CHECK(device);
  TRACE_EVENT2("gpu", "dml::CreateCustomUploadBuffer", "size", size, "name",
               name_for_debugging);
  // Create the equivalent custom heap properties regarding to upload heap,
  // based on the adapter's architectural properties.
  // https://learn.microsoft.com/en-us/previous-versions/dn788678(v=vs.85)
  auto heap_properties =
      device->GetCustomHeapProperties(0, D3D12_HEAP_TYPE_UPLOAD);
  auto resource_desc =
      CreateResourceDesc(size, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
  RETURN_IF_FAILED(device->CreateCommittedResource(
      &heap_properties, D3D12_HEAP_FLAG_NONE, &resource_desc,
      D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&resource)));
  CHECK(resource.Get());

  CHECK_NE(name_for_debugging, nullptr);
  CHECK_EQ(resource->SetName(name_for_debugging), S_OK);
  return S_OK;
}

HRESULT CreateCustomReadbackBuffer(
    ID3D12Device* device,
    uint64_t size,
    const wchar_t* name_for_debugging,
    Microsoft::WRL::ComPtr<ID3D12Resource>& resource) {
  CHECK(device);
  TRACE_EVENT2("gpu", "dml::CreateCustomReadbackBuffer", "size", size, "name",
               name_for_debugging);
  // Create the equivalent custom heap properties regarding to readback heap,
  // based on the adapter's architectural properties.
  // https://learn.microsoft.com/en-us/previous-versions/dn788678(v=vs.85)
  auto heap_properties =
      device->GetCustomHeapProperties(0, D3D12_HEAP_TYPE_READBACK);
  auto resource_desc =
      CreateResourceDesc(size, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
  RETURN_IF_FAILED(device->CreateCommittedResource(
      &heap_properties, D3D12_HEAP_FLAG_NONE, &resource_desc,
      D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&resource)));
  CHECK(resource.Get());

  CHECK_NE(name_for_debugging, nullptr);
  CHECK_EQ(resource->SetName(name_for_debugging), S_OK);
  return S_OK;
}

HRESULT CreateDescriptorHeap(
    ID3D12Device* device,
    uint32_t num_descriptors,
    const wchar_t* name_for_debugging,
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptor_heap) {
  CHECK(device);
  TRACE_EVENT2("gpu", "dml::CreateDescriptorHeap", "num_descriptors",
               num_descriptors, "name", name_for_debugging);
  CHECK_GT(num_descriptors, 0u);
  D3D12_DESCRIPTOR_HEAP_DESC descriptor_heap_desc{
      .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
      .NumDescriptors = num_descriptors,
      .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE};
  RETURN_IF_FAILED(device->CreateDescriptorHeap(
      &descriptor_heap_desc, IID_PPV_ARGS(&descriptor_heap)));
  CHECK(descriptor_heap.Get());
  descriptor_heap_desc = descriptor_heap->GetDesc();
  CHECK_EQ(descriptor_heap_desc.NumDescriptors, num_descriptors);

  CHECK_NE(name_for_debugging, nullptr);
  CHECK_EQ(descriptor_heap->SetName(name_for_debugging), S_OK);
  return S_OK;
}

}  // namespace webnn::dml
