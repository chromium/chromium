// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <d3d11.h>
#include <wrl.h>

#include <numeric>

#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "services/webnn/dml/adapter.h"
#include "services/webnn/dml/command_recorder.h"
#include "services/webnn/dml/error.h"
#include "services/webnn/dml/test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_angle_util_win.h"

namespace webnn::dml {

namespace {

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

size_t CalculateDMLBufferTensorSize(DML_TENSOR_DATA_TYPE data_type,
                                    const std::vector<uint32_t>& dimensions) {
  size_t element_size;
  switch (data_type) {
    case DML_TENSOR_DATA_TYPE_FLOAT32:
    case DML_TENSOR_DATA_TYPE_UINT32:
    case DML_TENSOR_DATA_TYPE_INT32:
      element_size = 4;
      break;
    case DML_TENSOR_DATA_TYPE_FLOAT16:
    case DML_TENSOR_DATA_TYPE_UINT16:
    case DML_TENSOR_DATA_TYPE_INT16:
      element_size = 2;
      break;
    case DML_TENSOR_DATA_TYPE_UINT8:
    case DML_TENSOR_DATA_TYPE_INT8:
      element_size = 1;
      break;
    case DML_TENSOR_DATA_TYPE_FLOAT64:
    case DML_TENSOR_DATA_TYPE_UINT64:
    case DML_TENSOR_DATA_TYPE_INT64:
      element_size = 8;
      break;
    default:
      NOTREACHED_NORETURN();
  }
  const size_t buffer_tensor_size =
      std::accumulate(dimensions.begin(), dimensions.end(), 1,
                      std::multiplies<uint32_t>()) *
      element_size;

  // DirectML requires buffer tensor size to be DWORD aligned.
  const size_t alignment = sizeof(DWORD);
  size_t aligned_buffer_tensor_size =
      buffer_tensor_size % alignment == 0
          ? buffer_tensor_size
          : (buffer_tensor_size / alignment + 1) * alignment;

  return aligned_buffer_tensor_size;
}

// The `dimensions` should outlive the returned `DML_BUFFER_TENSOR_DESC`.
DML_BUFFER_TENSOR_DESC CreateDMLBufferTensorDesc(
    DML_TENSOR_DATA_TYPE data_type,
    const std::vector<uint32_t>& dimensions,
    DML_TENSOR_FLAGS flags = DML_TENSOR_FLAG_NONE) {
  return {.DataType = data_type,
          .Flags = flags,
          .DimensionCount = base::checked_cast<uint32_t>(dimensions.size()),
          .Sizes = dimensions.data(),
          .Strides = nullptr,
          .TotalTensorSizeInBytes =
              CalculateDMLBufferTensorSize(data_type, dimensions)};
}

}  // namespace

using Microsoft::WRL::ComPtr;

const size_t kBufferSize = 16;

class WebNNCommandRecorderTest : public TestBase {
 public:
  void SetUp() override;

 protected:
  scoped_refptr<Adapter> adapter_;
};

void WebNNCommandRecorderTest::SetUp() {
  SKIP_TEST_IF(!UseGPUInTests());
  ASSERT_TRUE(InitializeGLDisplay());
  ComPtr<ID3D11Device> d3d11_device = gl::QueryD3D11DeviceObjectFromANGLE();
  ASSERT_NE(d3d11_device.Get(), nullptr);
  ComPtr<IDXGIDevice> dxgi_device;
  d3d11_device.As(&dxgi_device);
  ComPtr<IDXGIAdapter> dxgi_adapter;
  dxgi_device->GetAdapter(&dxgi_adapter);
  ASSERT_NE(dxgi_adapter.Get(), nullptr);
  adapter_ = Adapter::Create(dxgi_adapter);
  ASSERT_NE(adapter_.get(), nullptr);
}

TEST_F(WebNNCommandRecorderTest, Create) {
  EXPECT_NE(CommandRecorder::Create(adapter_), nullptr);
}

TEST_F(WebNNCommandRecorderTest, OpenCloseAndExecute) {
  auto command_recorder = CommandRecorder::Create(adapter_);
  ASSERT_NE(command_recorder.get(), nullptr);
  EXPECT_HRESULT_SUCCEEDED(command_recorder->Open());
  EXPECT_HRESULT_SUCCEEDED(command_recorder->CloseAndExecute());
  EXPECT_HRESULT_SUCCEEDED(
      command_recorder->GetCommandQueue()->WaitSyncForTesting());
}

TEST_F(WebNNCommandRecorderTest, CopyBufferRegionFromUploadToDefault) {
  // Test copying data from upload buffer to default GPU buffer.
  ComPtr<ID3D12Resource> upload_resource;
  ASSERT_HRESULT_SUCCEEDED(
      adapter_->CreateUploadBuffer(kBufferSize, upload_resource));
  ComPtr<ID3D12Resource> default_resource;
  ASSERT_HRESULT_SUCCEEDED(
      adapter_->CreateDefaultBuffer(kBufferSize, default_resource));
  auto command_recorder = CommandRecorder::Create(adapter_);
  ASSERT_NE(command_recorder.get(), nullptr);
  EXPECT_HRESULT_SUCCEEDED(command_recorder->Open());
  D3D12_RESOURCE_BARRIER barriers[1];
  barriers[0] = CreateTransitionBarrier(default_resource.Get(),
                                        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                                        D3D12_RESOURCE_STATE_COPY_DEST);
  command_recorder->ResourceBarrier(barriers);
  command_recorder->CopyBufferRegion(default_resource.Get(), 0,
                                     upload_resource.Get(), 0, kBufferSize);
  EXPECT_HRESULT_SUCCEEDED(command_recorder->CloseAndExecute());
  EXPECT_HRESULT_SUCCEEDED(
      command_recorder->GetCommandQueue()->WaitSyncForTesting());
}

TEST_F(WebNNCommandRecorderTest, CopyBufferRegionFromDefaultToDefault) {
  // Testing copying data from default GPU buffer to default buffer.
  ComPtr<ID3D12Resource> src_resource;
  ASSERT_HRESULT_SUCCEEDED(
      adapter_->CreateDefaultBuffer(kBufferSize, src_resource));
  ComPtr<ID3D12Resource> dst_resource;
  ASSERT_HRESULT_SUCCEEDED(
      adapter_->CreateDefaultBuffer(kBufferSize, dst_resource));
  auto command_recorder = CommandRecorder::Create(adapter_);
  ASSERT_NE(command_recorder.get(), nullptr);
  EXPECT_HRESULT_SUCCEEDED(command_recorder->Open());
  D3D12_RESOURCE_BARRIER barriers[2];
  barriers[0] = CreateTransitionBarrier(dst_resource.Get(),
                                        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                                        D3D12_RESOURCE_STATE_COPY_DEST);
  barriers[1] = CreateTransitionBarrier(src_resource.Get(),
                                        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                                        D3D12_RESOURCE_STATE_COPY_SOURCE);
  command_recorder->ResourceBarrier(barriers);
  command_recorder->CopyBufferRegion(dst_resource.Get(), 0, src_resource.Get(),
                                     0, kBufferSize);
  EXPECT_HRESULT_SUCCEEDED(command_recorder->CloseAndExecute());
  EXPECT_HRESULT_SUCCEEDED(
      command_recorder->GetCommandQueue()->WaitSyncForTesting());
}

TEST_F(WebNNCommandRecorderTest, CopyBufferRegionFromDefaultToReadback) {
  // Testing copying data from default GPU buffer to readback buffer.
  ComPtr<ID3D12Resource> default_resource;
  ASSERT_HRESULT_SUCCEEDED(
      adapter_->CreateDefaultBuffer(kBufferSize, default_resource));
  ComPtr<ID3D12Resource> readback_resource;
  ASSERT_HRESULT_SUCCEEDED(
      adapter_->CreateReadbackBuffer(kBufferSize, readback_resource));
  auto command_recorder = CommandRecorder::Create(adapter_);
  ASSERT_NE(command_recorder.get(), nullptr);
  EXPECT_HRESULT_SUCCEEDED(command_recorder->Open());
  D3D12_RESOURCE_BARRIER barriers[1];
  barriers[0] = CreateTransitionBarrier(default_resource.Get(),
                                        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                                        D3D12_RESOURCE_STATE_COPY_SOURCE);
  command_recorder->ResourceBarrier(barriers);
  command_recorder->CopyBufferRegion(readback_resource.Get(), 0,
                                     default_resource.Get(), 0, kBufferSize);
  EXPECT_HRESULT_SUCCEEDED(command_recorder->CloseAndExecute());
  EXPECT_HRESULT_SUCCEEDED(
      command_recorder->GetCommandQueue()->WaitSyncForTesting());
}

TEST_F(WebNNCommandRecorderTest, MultipleSubmissionsWithOneWait) {
  // Test submitting multiple command lists with one wait for GPU to complete.
  // Submit the command that copies data from upload buffer to default GPU
  // buffer.
  ComPtr<ID3D12Resource> upload_resource;
  ASSERT_HRESULT_SUCCEEDED(
      adapter_->CreateUploadBuffer(kBufferSize, upload_resource));
  ComPtr<ID3D12Resource> default_resource;
  ASSERT_HRESULT_SUCCEEDED(
      adapter_->CreateDefaultBuffer(kBufferSize, default_resource));
  auto command_recorder = CommandRecorder::Create(adapter_);
  ASSERT_NE(command_recorder.get(), nullptr);
  EXPECT_HRESULT_SUCCEEDED(command_recorder->Open());
  D3D12_RESOURCE_BARRIER barriers[1];
  barriers[0] = CreateTransitionBarrier(default_resource.Get(),
                                        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                                        D3D12_RESOURCE_STATE_COPY_DEST);
  command_recorder->ResourceBarrier(barriers);
  command_recorder->CopyBufferRegion(default_resource.Get(), 0,
                                     upload_resource.Get(), 0, kBufferSize);
  EXPECT_HRESULT_SUCCEEDED(command_recorder->CloseAndExecute());

  // Submit the command that copies data from default buffer to readback buffer.
  ComPtr<ID3D12Resource> readback_resource;
  ASSERT_HRESULT_SUCCEEDED(
      adapter_->CreateReadbackBuffer(kBufferSize, readback_resource));
  EXPECT_HRESULT_SUCCEEDED(command_recorder->Open());
  barriers[0] = CreateTransitionBarrier(default_resource.Get(),
                                        D3D12_RESOURCE_STATE_COPY_DEST,
                                        D3D12_RESOURCE_STATE_COPY_SOURCE);
  command_recorder->ResourceBarrier(barriers);
  command_recorder->CopyBufferRegion(readback_resource.Get(), 0,
                                     default_resource.Get(), 0, kBufferSize);
  EXPECT_HRESULT_SUCCEEDED(command_recorder->CloseAndExecute());

  // Wait for GPU to complete the execution of both command lists.
  EXPECT_HRESULT_SUCCEEDED(
      command_recorder->GetCommandQueue()->WaitSyncForTesting());
}

TEST_F(WebNNCommandRecorderTest, InitializeReluOperator) {
  // Test initializing a DirectML Relu operator.
  //
  // Create a Relu operator.
  const std::vector<uint32_t> dimensions({1, 2, 3, 4});
  DML_BUFFER_TENSOR_DESC buffer_tensor_desc =
      CreateDMLBufferTensorDesc(DML_TENSOR_DATA_TYPE_FLOAT32, dimensions);
  DML_TENSOR_DESC tensor_desc{.Type = DML_TENSOR_TYPE_BUFFER,
                              .Desc = &buffer_tensor_desc};

  DML_ACTIVATION_RELU_OPERATOR_DESC relu_operator_desc{
      .InputTensor = &tensor_desc, .OutputTensor = &tensor_desc};
  DML_OPERATOR_DESC operator_desc{.Type = DML_OPERATOR_ACTIVATION_RELU,
                                  .Desc = &relu_operator_desc};
  ComPtr<IDMLOperator> dml_operator;
  ASSERT_HRESULT_SUCCEEDED(adapter_->dml_device()->CreateOperator(
      &operator_desc, IID_PPV_ARGS(&dml_operator)));

  // Compile the operator.
  ComPtr<IDMLCompiledOperator> compiled_operator;
  ASSERT_HRESULT_SUCCEEDED(adapter_->dml_device()->CompileOperator(
      dml_operator.Get(), DML_EXECUTION_FLAG_NONE,
      IID_PPV_ARGS(&compiled_operator)));

  // Relu operator should not require any persistent resources.
  ASSERT_EQ(compiled_operator->GetBindingProperties().PersistentResourceSize,
            0u);

  // Initialize the operator.
  auto command_recorder = CommandRecorder::Create(adapter_);
  ASSERT_NE(command_recorder.get(), nullptr);
  ASSERT_HRESULT_SUCCEEDED(command_recorder->Open());
  // Relu operator initializer deson't need to bind any input and persistent
  // resources.
  EXPECT_HRESULT_SUCCEEDED(command_recorder->InitializeOperator(
      compiled_operator.Get(), absl::nullopt, absl::nullopt));
  EXPECT_HRESULT_SUCCEEDED(command_recorder->CloseAndExecute());
  EXPECT_HRESULT_SUCCEEDED(
      command_recorder->GetCommandQueue()->WaitSyncForTesting());
  adapter_->command_queue()->ReleaseCompletedResources();
  EXPECT_HRESULT_SUCCEEDED(adapter_->dml_device()->GetDeviceRemovedReason());
  EXPECT_HRESULT_SUCCEEDED(adapter_->d3d12_device()->GetDeviceRemovedReason());
}

TEST_F(WebNNCommandRecorderTest, InitializeConvolutionOperator) {
  // Test initializing a DirectML Convolution operator which requires binding
  // filter resource as input and persistent resource as output for the operator
  // initializer.
  //
  // Create a Convolution operator.
  const std::vector<uint32_t> input_dimensions({1, 1, 3, 3});
  DML_BUFFER_TENSOR_DESC input_buffer_tensor_desc =
      CreateDMLBufferTensorDesc(DML_TENSOR_DATA_TYPE_FLOAT32, input_dimensions);
  DML_TENSOR_DESC input_tensor_desc{.Type = DML_TENSOR_TYPE_BUFFER,
                                    .Desc = &input_buffer_tensor_desc};

  // Set DML_TENSOR_FLAG_OWNED_BY_DML flag to filter tensor, so that its
  // resource should be bound for operator initializer.
  const std::vector<uint32_t> filter_dimensions({1, 1, 2, 2});
  DML_BUFFER_TENSOR_DESC filter_buffer_tensor_desc =
      CreateDMLBufferTensorDesc(DML_TENSOR_DATA_TYPE_FLOAT32, filter_dimensions,
                                DML_TENSOR_FLAG_OWNED_BY_DML);
  DML_TENSOR_DESC filter_tensor_desc{.Type = DML_TENSOR_TYPE_BUFFER,
                                     .Desc = &filter_buffer_tensor_desc};

  const std::vector<uint32_t> output_dimensions({1, 1, 2, 2});
  DML_BUFFER_TENSOR_DESC output_buffer_tensor_desc = CreateDMLBufferTensorDesc(
      DML_TENSOR_DATA_TYPE_FLOAT32, output_dimensions);
  DML_TENSOR_DESC output_tensor_desc{.Type = DML_TENSOR_TYPE_BUFFER,
                                     .Desc = &output_buffer_tensor_desc};

  const std::vector<uint32_t> strides({1, 1});
  const std::vector<uint32_t> dilations({1, 1});
  const std::vector<uint32_t> start_padding({0, 0});
  const std::vector<uint32_t> end_padding({0, 0});
  const std::vector<uint32_t> output_padding({0, 0});
  DML_CONVOLUTION_OPERATOR_DESC conv_operator_desc{
      .InputTensor = &input_tensor_desc,
      .FilterTensor = &filter_tensor_desc,
      .BiasTensor = nullptr,
      .OutputTensor = &output_tensor_desc,
      .Mode = DML_CONVOLUTION_MODE_CROSS_CORRELATION,
      .Direction = DML_CONVOLUTION_DIRECTION_FORWARD,
      .DimensionCount = 2,
      .Strides = strides.data(),
      .Dilations = dilations.data(),
      .StartPadding = start_padding.data(),
      .EndPadding = end_padding.data(),
      .OutputPadding = output_padding.data(),
      .GroupCount = 1,
      .FusedActivation = nullptr};
  DML_OPERATOR_DESC operator_desc{.Type = DML_OPERATOR_CONVOLUTION,
                                  .Desc = &conv_operator_desc};
  ComPtr<IDMLOperator> dml_operator;
  ASSERT_HRESULT_SUCCEEDED(adapter_->dml_device()->CreateOperator(
      &operator_desc, IID_PPV_ARGS(&dml_operator)));

  // Compile the operator.
  ComPtr<IDMLCompiledOperator> compiled_operator;
  ASSERT_HRESULT_SUCCEEDED(adapter_->dml_device()->CompileOperator(
      dml_operator.Get(), DML_EXECUTION_FLAG_NONE,
      IID_PPV_ARGS(&compiled_operator)));

  // Create filter resource that will be bound for operator initializer.
  ComPtr<ID3D12Resource> filter_buffer;
  const size_t filter_buffer_size = CalculateDMLBufferTensorSize(
      DML_TENSOR_DATA_TYPE_FLOAT32, filter_dimensions);
  ASSERT_HRESULT_SUCCEEDED(
      adapter_->CreateDefaultBuffer(filter_buffer_size, filter_buffer));

  // Upload weights to filter resource.
  std::vector<float> weights({1.0, 2.0, 3.0, 4.0});
  ComPtr<ID3D12Resource> upload_buffer;
  ASSERT_HRESULT_SUCCEEDED(
      adapter_->CreateUploadBuffer(filter_buffer_size, upload_buffer));
  void* upload_buffer_data = nullptr;
  ASSERT_HRESULT_SUCCEEDED(upload_buffer->Map(0, nullptr, &upload_buffer_data));
  memcpy(upload_buffer_data, weights.data(), filter_buffer_size);
  upload_buffer->Unmap(0, nullptr);

  // Copy the weights from upload buffer to filter buffer.
  auto command_recorder = CommandRecorder::Create(adapter_);
  ASSERT_NE(command_recorder.get(), nullptr);
  ASSERT_HRESULT_SUCCEEDED(command_recorder->Open());
  D3D12_RESOURCE_BARRIER barriers[1];
  barriers[0] = CreateTransitionBarrier(filter_buffer.Get(),
                                        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                                        D3D12_RESOURCE_STATE_COPY_DEST);
  command_recorder->ResourceBarrier(barriers);
  command_recorder->CopyBufferRegion(
      filter_buffer.Get(), 0, upload_buffer.Get(), 0, filter_buffer_size);
  // The bound resources should be in D3D12_RESOURCE_STATE_UNORDERED_ACCESS
  // state before the execution of RecordDispatch on the GPU.
  barriers[0] = CreateTransitionBarrier(filter_buffer.Get(),
                                        D3D12_RESOURCE_STATE_COPY_DEST,
                                        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
  command_recorder->ResourceBarrier(barriers);

  // Create the input resources binding for operator initialization. Only the
  // filter resource needs to be bound.
  std::vector<DML_BUFFER_BINDING> input_buffer_bindings(
      {// Input.
       {.Buffer = nullptr, .Offset = 0, .SizeInBytes = 0},
       // Filter.
       {.Buffer = filter_buffer.Get(),
        .Offset = 0,
        .SizeInBytes = filter_buffer_size},
       // Bias.
       {.Buffer = nullptr, .Offset = 0, .SizeInBytes = 0}});
  DML_BUFFER_ARRAY_BINDING input_buffer_array_bindings{
      .BindingCount =
          base::checked_cast<uint32_t>(input_buffer_bindings.size()),
      .Bindings = input_buffer_bindings.data()};
  DML_BINDING_DESC input_buffer_array_binding_desc{
      .Type = DML_BINDING_TYPE_BUFFER_ARRAY,
      .Desc = &input_buffer_array_bindings};

  // Create the persistent resource required by Convolution operator which is
  // bound as output of operator initializer.
  DML_BINDING_PROPERTIES execution_binding_properties =
      compiled_operator->GetBindingProperties();
  auto persistent_buffer_size =
      execution_binding_properties.PersistentResourceSize;
  ASSERT_GT(persistent_buffer_size, 0u);
  ComPtr<ID3D12Resource> persistent_buffer;
  ASSERT_HRESULT_SUCCEEDED(
      adapter_->CreateDefaultBuffer(persistent_buffer_size, persistent_buffer));
  DML_BUFFER_BINDING persistent_buffer_binding{
      .Buffer = persistent_buffer.Get(),
      .Offset = 0,
      .SizeInBytes = persistent_buffer_size};
  DML_BINDING_DESC persistent_buffer_binding_desc{
      .Type = DML_BINDING_TYPE_BUFFER, .Desc = &persistent_buffer_binding};

  // Initialize the operator and bind the input and persistent resources to
  // the operator initializer.
  EXPECT_HRESULT_SUCCEEDED(command_recorder->InitializeOperator(
      compiled_operator.Get(), input_buffer_array_binding_desc,
      persistent_buffer_binding_desc));
  EXPECT_HRESULT_SUCCEEDED(command_recorder->CloseAndExecute());
  EXPECT_HRESULT_SUCCEEDED(
      command_recorder->GetCommandQueue()->WaitSyncForTesting());
  adapter_->command_queue()->ReleaseCompletedResources();
  EXPECT_HRESULT_SUCCEEDED(adapter_->dml_device()->GetDeviceRemovedReason());
  EXPECT_HRESULT_SUCCEEDED(adapter_->d3d12_device()->GetDeviceRemovedReason());
}

}  // namespace webnn::dml
