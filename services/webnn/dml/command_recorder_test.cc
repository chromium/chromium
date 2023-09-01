// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <wrl.h>

#include "base/numerics/safe_conversions.h"
#include "services/webnn/dml/adapter.h"
#include "services/webnn/dml/command_queue.h"
#include "services/webnn/dml/command_recorder.h"
#include "services/webnn/dml/error.h"
#include "services/webnn/dml/tensor_desc.h"
#include "services/webnn/dml/test_base.h"
#include "services/webnn/dml/utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webnn::dml {

using Microsoft::WRL::ComPtr;

const size_t kBufferSize = 16;

class WebNNCommandRecorderTest : public TestBase {
 public:
  void SetUp() override;

 protected:
  void Upload(CommandRecorder* command_recorder,
              void* src_buffer,
              size_t buffer_size,
              ID3D12Resource* dst_resource);
  void Download(CommandRecorder* command_recorder,
                void* dst_buffer,
                size_t buffer_size,
                ID3D12Resource* src_resource);

  scoped_refptr<Adapter> adapter_;
};

void WebNNCommandRecorderTest::SetUp() {
  SKIP_TEST_IF(!UseGPUInTests());
  ASSERT_TRUE(InitializeGLDisplay());
  Adapter::EnableDebugLayerForTesting();
  adapter_ = Adapter::GetInstanceForTesting();
  ASSERT_NE(adapter_.get(), nullptr);
}

void WebNNCommandRecorderTest::Upload(CommandRecorder* command_recorder,
                                      void* src_buffer,
                                      size_t buffer_size,
                                      ID3D12Resource* dst_resource) {
  // Copy the contents from source buffer to upload buffer.
  ComPtr<ID3D12Resource> upload_buffer;
  ASSERT_HRESULT_SUCCEEDED(
      command_recorder->CreateUploadBuffer(buffer_size, upload_buffer));
  void* upload_buffer_data = nullptr;
  ASSERT_HRESULT_SUCCEEDED(upload_buffer->Map(0, nullptr, &upload_buffer_data));
  memcpy(upload_buffer_data, src_buffer, buffer_size);
  upload_buffer->Unmap(0, nullptr);

  // Copy the input data from upload buffer to input buffer.
  UploadBufferWithBarrier(command_recorder, dst_resource, upload_buffer.Get(),
                          buffer_size);

  // Keep the upload_buffer alive until the GPU work is done.
  adapter_->command_queue()->ReferenceUntilCompleted(std::move(upload_buffer));
}

void WebNNCommandRecorderTest::Download(CommandRecorder* command_recorder,
                                        void* dst_buffer,
                                        size_t buffer_size,
                                        ID3D12Resource* src_resource) {
  ComPtr<ID3D12Resource> readback_buffer;
  ASSERT_HRESULT_SUCCEEDED(
      command_recorder->CreateReadbackBuffer(buffer_size, readback_buffer));
  // Copy the result from output buffer to readback buffer.
  D3D12_RESOURCE_BARRIER barriers[1];
  barriers[0] = CreateTransitionBarrier(src_resource,
                                        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                                        D3D12_RESOURCE_STATE_COPY_SOURCE);
  command_recorder->ResourceBarrier(barriers);
  command_recorder->CopyBufferRegion(readback_buffer.Get(), 0, src_resource, 0,
                                     buffer_size);
  barriers[0] =
      CreateTransitionBarrier(src_resource, D3D12_RESOURCE_STATE_COPY_SOURCE,
                              D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
  command_recorder->ResourceBarrier(barriers);

  // Close, execute and wait for completion.
  ASSERT_HRESULT_SUCCEEDED(command_recorder->CloseAndExecute());
  ASSERT_HRESULT_SUCCEEDED(
      command_recorder->GetCommandQueue()->WaitSyncForTesting());

  // Release the resources referred by GPU execution.
  adapter_->command_queue()->ReleaseCompletedResources();
  ASSERT_HRESULT_SUCCEEDED(adapter_->dml_device()->GetDeviceRemovedReason());
  ASSERT_HRESULT_SUCCEEDED(adapter_->d3d12_device()->GetDeviceRemovedReason());

  // Copy the contents from readback buffer to destination buffer.
  void* readback_buffer_data = nullptr;
  ASSERT_HRESULT_SUCCEEDED(
      readback_buffer->Map(0, nullptr, &readback_buffer_data));
  memcpy(dst_buffer, readback_buffer_data, buffer_size);
  readback_buffer->Unmap(0, nullptr);
}

TEST_F(WebNNCommandRecorderTest, Create) {
  EXPECT_NE(CommandRecorder::Create(adapter_->command_queue(),
                                    adapter_->dml_device()),
            nullptr);
}

TEST_F(WebNNCommandRecorderTest, OpenCloseAndExecute) {
  auto command_recorder = CommandRecorder::Create(adapter_->command_queue(),
                                                  adapter_->dml_device());
  ASSERT_NE(command_recorder.get(), nullptr);
  EXPECT_HRESULT_SUCCEEDED(command_recorder->Open());
  EXPECT_HRESULT_SUCCEEDED(command_recorder->CloseAndExecute());
  EXPECT_HRESULT_SUCCEEDED(
      command_recorder->GetCommandQueue()->WaitSyncForTesting());
}

TEST_F(WebNNCommandRecorderTest, CopyBufferRegionFromUploadToDefault) {
  // Test copying data from upload buffer to default GPU buffer.
  auto command_recorder = CommandRecorder::Create(adapter_->command_queue(),
                                                  adapter_->dml_device());
  ASSERT_NE(command_recorder.get(), nullptr);
  ComPtr<ID3D12Resource> upload_resource;
  ASSERT_HRESULT_SUCCEEDED(
      command_recorder->CreateUploadBuffer(kBufferSize, upload_resource));
  ComPtr<ID3D12Resource> default_resource;
  ASSERT_HRESULT_SUCCEEDED(
      command_recorder->CreateDefaultBuffer(kBufferSize, default_resource));
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
  auto command_recorder = CommandRecorder::Create(adapter_->command_queue(),
                                                  adapter_->dml_device());
  ASSERT_NE(command_recorder.get(), nullptr);
  ComPtr<ID3D12Resource> src_resource;
  ASSERT_HRESULT_SUCCEEDED(
      command_recorder->CreateDefaultBuffer(kBufferSize, src_resource));
  ComPtr<ID3D12Resource> dst_resource;
  ASSERT_HRESULT_SUCCEEDED(
      command_recorder->CreateDefaultBuffer(kBufferSize, dst_resource));
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
  auto command_recorder = CommandRecorder::Create(adapter_->command_queue(),
                                                  adapter_->dml_device());
  ASSERT_NE(command_recorder.get(), nullptr);
  ComPtr<ID3D12Resource> default_resource;
  ASSERT_HRESULT_SUCCEEDED(
      command_recorder->CreateDefaultBuffer(kBufferSize, default_resource));
  ComPtr<ID3D12Resource> readback_resource;
  ASSERT_HRESULT_SUCCEEDED(
      command_recorder->CreateReadbackBuffer(kBufferSize, readback_resource));
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
  auto command_recorder = CommandRecorder::Create(adapter_->command_queue(),
                                                  adapter_->dml_device());
  ASSERT_NE(command_recorder.get(), nullptr);
  ComPtr<ID3D12Resource> upload_resource;
  ASSERT_HRESULT_SUCCEEDED(
      command_recorder->CreateUploadBuffer(kBufferSize, upload_resource));
  ComPtr<ID3D12Resource> default_resource;
  ASSERT_HRESULT_SUCCEEDED(
      command_recorder->CreateDefaultBuffer(kBufferSize, default_resource));
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
      command_recorder->CreateReadbackBuffer(kBufferSize, readback_resource));
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

TEST_F(WebNNCommandRecorderTest, InitializeAndExecuteReluOperator) {
  // Test initializing and executing a DirectML Relu operator.
  //
  // Create a Relu operator.
  TensorDesc input_tensor_desc(DML_TENSOR_DATA_TYPE_FLOAT32, {1, 1, 2, 2});
  DML_ACTIVATION_RELU_OPERATOR_DESC relu_operator_desc{
      .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
      .OutputTensor = &input_tensor_desc.GetDMLTensorDesc()};
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
  auto command_recorder = CommandRecorder::Create(adapter_->command_queue(),
                                                  adapter_->dml_device());
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

  // Create input and output resources that will be bound for operator for
  // execution.
  const uint64_t buffer_size = input_tensor_desc.GetTotalTensorSizeInBytes();
  ComPtr<ID3D12Resource> input_buffer;
  ASSERT_HRESULT_SUCCEEDED(
      command_recorder->CreateDefaultBuffer(buffer_size, input_buffer));
  ComPtr<ID3D12Resource> output_buffer;
  ASSERT_HRESULT_SUCCEEDED(
      command_recorder->CreateDefaultBuffer(buffer_size, output_buffer));

  // Re-open the command recorder for recording operator execution commands.
  ASSERT_HRESULT_SUCCEEDED(command_recorder->Open());

  // Upload input data to input resource.
  std::vector<float> input_data({-2.0, -1.0, 1.0, 2.0});
  Upload(command_recorder.get(), input_data.data(), buffer_size,
         input_buffer.Get());

  // Create the input and output resources binding for operator execution.
  DML_BUFFER_BINDING input_buffer_binding{
      .Buffer = input_buffer.Get(), .Offset = 0, .SizeInBytes = buffer_size};
  std::vector<DML_BINDING_DESC> input_bindings(
      {{.Type = DML_BINDING_TYPE_BUFFER, .Desc = &input_buffer_binding}});
  DML_BUFFER_BINDING output_buffer_binding{
      .Buffer = output_buffer.Get(), .Offset = 0, .SizeInBytes = buffer_size};
  std::vector<DML_BINDING_DESC> output_bindings(
      {{.Type = DML_BINDING_TYPE_BUFFER, .Desc = &output_buffer_binding}});

  // Execute the operator with input and output bindings.
  EXPECT_HRESULT_SUCCEEDED(command_recorder->ExecuteOperator(
      compiled_operator.Get(), input_bindings, output_bindings, absl::nullopt));

  // Download the result from output resource.
  std::vector<float> result(buffer_size / sizeof(float));
  Download(command_recorder.get(), result.data(), buffer_size,
           output_buffer.Get());

  // Compare the result against expected.
  EXPECT_EQ(result, std::vector<float>({0.0, 0.0, 1.0, 2.0}));
}

TEST_F(WebNNCommandRecorderTest, ExecuteReluOperatorForMultipleBindings) {
  // Test dispatching a DirectML Relu operator twice for different input and
  // output bindings before waiting for GPU work to complete.
  //
  // Create a Relu operator.
  TensorDesc input_tensor_desc(DML_TENSOR_DATA_TYPE_FLOAT32, {1, 1, 2, 2});
  DML_ACTIVATION_RELU_OPERATOR_DESC relu_operator_desc{
      .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
      .OutputTensor = &input_tensor_desc.GetDMLTensorDesc()};
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
  auto command_recorder = CommandRecorder::Create(adapter_->command_queue(),
                                                  adapter_->dml_device());
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

  // Create input and output resources that will be bound for the two operator
  // executions.
  const uint64_t buffer_size = input_tensor_desc.GetTotalTensorSizeInBytes();
  ComPtr<ID3D12Resource> input_buffers[2];
  ASSERT_HRESULT_SUCCEEDED(
      command_recorder->CreateDefaultBuffer(buffer_size, input_buffers[0]));
  ASSERT_HRESULT_SUCCEEDED(
      command_recorder->CreateDefaultBuffer(buffer_size, input_buffers[1]));
  ComPtr<ID3D12Resource> output_buffers[2];
  ASSERT_HRESULT_SUCCEEDED(
      command_recorder->CreateDefaultBuffer(buffer_size, output_buffers[0]));
  ASSERT_HRESULT_SUCCEEDED(
      command_recorder->CreateDefaultBuffer(buffer_size, output_buffers[1]));

  // Create the input and output resources binding for operator executions.
  DML_BUFFER_BINDING input_buffer_bindings[2] = {
      {.Buffer = input_buffers[0].Get(),
       .Offset = 0,
       .SizeInBytes = buffer_size},
      {.Buffer = input_buffers[1].Get(),
       .Offset = 0,
       .SizeInBytes = buffer_size}};
  std::vector<DML_BINDING_DESC> input_bindings[2] = {
      {{.Type = DML_BINDING_TYPE_BUFFER, .Desc = &input_buffer_bindings[0]}},
      {{.Type = DML_BINDING_TYPE_BUFFER, .Desc = &input_buffer_bindings[1]}}};
  DML_BUFFER_BINDING output_buffer_bindings[2] = {
      {.Buffer = output_buffers[0].Get(),
       .Offset = 0,
       .SizeInBytes = buffer_size},
      {.Buffer = output_buffers[1].Get(),
       .Offset = 0,
       .SizeInBytes = buffer_size}};
  std::vector<DML_BINDING_DESC> output_bindings[2] = {
      {{.Type = DML_BINDING_TYPE_BUFFER, .Desc = &output_buffer_bindings[0]}},
      {{.Type = DML_BINDING_TYPE_BUFFER, .Desc = &output_buffer_bindings[1]}}};

  // Re-open the command recorder for recording operator execution commands.
  ASSERT_HRESULT_SUCCEEDED(command_recorder->Open());

  // Upload first input data and execute the operator.
  std::vector<float> input_data({-2.0, -1.0, 1.0, 2.0});
  Upload(command_recorder.get(), input_data.data(), buffer_size,
         input_buffers[0].Get());
  EXPECT_HRESULT_SUCCEEDED(command_recorder->ExecuteOperator(
      compiled_operator.Get(), input_bindings[0], output_bindings[0],
      absl::nullopt));

  // Upload second input data and execute the operator again.
  input_data = {2.0, 1.0, -1.0, -2.0};
  Upload(command_recorder.get(), input_data.data(), buffer_size,
         input_buffers[1].Get());
  EXPECT_HRESULT_SUCCEEDED(command_recorder->ExecuteOperator(
      compiled_operator.Get(), input_bindings[1], output_bindings[1],
      absl::nullopt));

  // Download result from output resources.
  ComPtr<ID3D12Resource> readback_buffers[2];
  ASSERT_HRESULT_SUCCEEDED(
      command_recorder->CreateReadbackBuffer(buffer_size, readback_buffers[0]));
  ASSERT_HRESULT_SUCCEEDED(
      command_recorder->CreateReadbackBuffer(buffer_size, readback_buffers[1]));

  // Copy the result from output buffers to readback buffers.
  D3D12_RESOURCE_BARRIER barriers[1];
  barriers[0] = CreateTransitionBarrier(output_buffers[0].Get(),
                                        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                                        D3D12_RESOURCE_STATE_COPY_SOURCE);
  command_recorder->ResourceBarrier(barriers);
  command_recorder->CopyBufferRegion(readback_buffers[0].Get(), 0,
                                     output_buffers[0].Get(), 0, buffer_size);
  barriers[0] = CreateTransitionBarrier(output_buffers[0].Get(),
                                        D3D12_RESOURCE_STATE_COPY_SOURCE,
                                        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
  command_recorder->ResourceBarrier(barriers);

  barriers[0] = CreateTransitionBarrier(output_buffers[1].Get(),
                                        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                                        D3D12_RESOURCE_STATE_COPY_SOURCE);
  command_recorder->ResourceBarrier(barriers);
  command_recorder->CopyBufferRegion(readback_buffers[1].Get(), 0,
                                     output_buffers[1].Get(), 0, buffer_size);
  barriers[0] = CreateTransitionBarrier(output_buffers[1].Get(),
                                        D3D12_RESOURCE_STATE_COPY_SOURCE,
                                        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
  command_recorder->ResourceBarrier(barriers);

  // Close, execute and wait for completion.
  ASSERT_HRESULT_SUCCEEDED(command_recorder->CloseAndExecute());
  ASSERT_HRESULT_SUCCEEDED(
      command_recorder->GetCommandQueue()->WaitSyncForTesting());

  // Release the resources referred by GPU execution.
  adapter_->command_queue()->ReleaseCompletedResources();
  ASSERT_HRESULT_SUCCEEDED(adapter_->dml_device()->GetDeviceRemovedReason());
  ASSERT_HRESULT_SUCCEEDED(adapter_->d3d12_device()->GetDeviceRemovedReason());

  // Verify the result of the 1st execution.
  std::vector<float> result(buffer_size / sizeof(float));
  void* readback_buffer_data = nullptr;
  ASSERT_HRESULT_SUCCEEDED(
      readback_buffers[0]->Map(0, nullptr, &readback_buffer_data));
  memcpy(result.data(), readback_buffer_data, buffer_size);
  readback_buffers[0]->Unmap(0, nullptr);
  EXPECT_EQ(result, std::vector<float>({0.0, 0.0, 1.0, 2.0}));

  // Verify the result of the 2nd execution.
  ASSERT_HRESULT_SUCCEEDED(
      readback_buffers[1]->Map(0, nullptr, &readback_buffer_data));
  memcpy(result.data(), readback_buffer_data, buffer_size);
  readback_buffers[1]->Unmap(0, nullptr);
  EXPECT_EQ(result, std::vector<float>({2.0, 1.0, 0.0, 0.0}));
}

TEST_F(WebNNCommandRecorderTest, InitializeAndExecuteConvolutionOperator) {
  // Test initializing a DirectML Convolution operator which requires binding
  // filter resource as input and persistent resource as output for the operator
  // initializer. Also test executing this operator with input and output
  // resources.
  //
  // Create a Convolution operator.
  TensorDesc input_tensor_desc(DML_TENSOR_DATA_TYPE_FLOAT32, {1, 1, 3, 3});
  // Set DML_TENSOR_FLAG_OWNED_BY_DML flag to filter tensor, so that its
  // resource should be bound for operator initializer.
  TensorDesc filter_tensor_desc(DML_TENSOR_DATA_TYPE_FLOAT32,
                                DML_TENSOR_FLAG_OWNED_BY_DML, {1, 1, 2, 2});
  TensorDesc output_tensor_desc(DML_TENSOR_DATA_TYPE_FLOAT32, {1, 1, 2, 2});

  const std::vector<uint32_t> strides({1, 1});
  const std::vector<uint32_t> dilations({1, 1});
  const std::vector<uint32_t> start_padding({0, 0});
  const std::vector<uint32_t> end_padding({0, 0});
  const std::vector<uint32_t> output_padding({0, 0});
  DML_CONVOLUTION_OPERATOR_DESC conv_operator_desc{
      .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
      .FilterTensor = &filter_tensor_desc.GetDMLTensorDesc(),
      .BiasTensor = nullptr,
      .OutputTensor = &output_tensor_desc.GetDMLTensorDesc(),
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
  const uint64_t filter_buffer_size =
      filter_tensor_desc.GetTotalTensorSizeInBytes();

  auto command_recorder = CommandRecorder::Create(adapter_->command_queue(),
                                                  adapter_->dml_device());
  ASSERT_NE(command_recorder.get(), nullptr);
  ASSERT_HRESULT_SUCCEEDED(
      command_recorder->CreateDefaultBuffer(filter_buffer_size, filter_buffer));

  ASSERT_HRESULT_SUCCEEDED(command_recorder->Open());

  // Upload weights to filter resource.
  std::vector<float> weights({0.5, 0.5, 0.5, 0.5});
  Upload(command_recorder.get(), weights.data(), filter_buffer_size,
         filter_buffer.Get());

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
  ASSERT_HRESULT_SUCCEEDED(command_recorder->CreateDefaultBuffer(
      persistent_buffer_size, persistent_buffer));
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

  // Create input and output resources that will be bound for operator for
  // execution.
  const uint64_t input_buffer_size =
      input_tensor_desc.GetTotalTensorSizeInBytes();
  ComPtr<ID3D12Resource> input_buffer;
  ASSERT_HRESULT_SUCCEEDED(
      command_recorder->CreateDefaultBuffer(input_buffer_size, input_buffer));
  const uint64_t output_buffer_size =
      output_tensor_desc.GetTotalTensorSizeInBytes();
  ComPtr<ID3D12Resource> output_buffer;
  ASSERT_HRESULT_SUCCEEDED(
      command_recorder->CreateDefaultBuffer(output_buffer_size, output_buffer));

  // Re-open the command recorder for recording operator execution commands.
  ASSERT_HRESULT_SUCCEEDED(command_recorder->Open());

  // Upload input data to input resource.
  std::vector<float> input_data({1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0});
  Upload(command_recorder.get(), input_data.data(), input_buffer_size,
         input_buffer.Get());

  // Create the input and output resources binding for operator execution.
  DML_BUFFER_BINDING input_buffer_binding{.Buffer = input_buffer.Get(),
                                          .Offset = 0,
                                          .SizeInBytes = input_buffer_size};
  std::vector<DML_BINDING_DESC> input_bindings(
      {// Input.
       {.Type = DML_BINDING_TYPE_BUFFER, .Desc = &input_buffer_binding},
       // Filter.
       {.Type = DML_BINDING_TYPE_NONE, .Desc = nullptr},
       // Bias.
       {.Type = DML_BINDING_TYPE_NONE, .Desc = nullptr}});
  DML_BUFFER_BINDING output_buffer_binding{.Buffer = output_buffer.Get(),
                                           .Offset = 0,
                                           .SizeInBytes = output_buffer_size};
  std::vector<DML_BINDING_DESC> output_bindings(
      {{.Type = DML_BINDING_TYPE_BUFFER, .Desc = &output_buffer_binding}});

  // Execute the operator with persistent, input and output bindings.
  EXPECT_HRESULT_SUCCEEDED(command_recorder->ExecuteOperator(
      compiled_operator.Get(), input_bindings, output_bindings,
      persistent_buffer_binding_desc));

  // Download the result from output resource.
  std::vector<float> result(output_buffer_size / sizeof(float));
  Download(command_recorder.get(), result.data(), output_buffer_size,
           output_buffer.Get());

  // Compare the result against expected.
  EXPECT_EQ(result, std::vector<float>({6.0, 8.0, 12.0, 14.0}));
}

}  // namespace webnn::dml
