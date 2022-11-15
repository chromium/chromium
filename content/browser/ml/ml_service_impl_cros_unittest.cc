// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/ml/ml_service_impl_cros.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chromeos/services/machine_learning/public/cpp/fake_service_connection.h"
#include "chromeos/services/machine_learning/public/cpp/service_connection.h"
#include "components/ml/mojom/ml_service.mojom.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {

class MLServiceImplCrOSTest : public RenderViewHostTestHarness {
 public:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    chromeos::machine_learning::ServiceConnection::
        UseFakeServiceConnectionForTesting(&fake_ml_service_connection_);
    chromeos::machine_learning::ServiceConnection::GetInstance()->Initialize();
  }

  chromeos::machine_learning::FakeServiceConnectionImpl&
  GetMlServiceConnection() {
    return fake_ml_service_connection_;
  }

 private:
  chromeos::machine_learning::FakeServiceConnectionImpl
      fake_ml_service_connection_;
};

// Tests the successful case model loader creation.
TEST_F(MLServiceImplCrOSTest, CreateModelLoaderOK) {
  mojo::Remote<ml::model_loader::mojom::MLService> service_remote;
  CrOSMLServiceImpl::Create(service_remote.BindNewPipeAndPassReceiver());
  auto options = ml::model_loader::mojom::CreateModelLoaderOptions::New();
  options->num_threads = 2;
  options->model_format = ml::model_loader::mojom::ModelFormat::kTfLite;
  GetMlServiceConnection().SetCreateWebPlatformModelLoaderResult(
      ml::model_loader::mojom::CreateModelLoaderResult::kOk);
  bool is_callback_called = false;
  base::RunLoop run_loop;
  service_remote->CreateModelLoader(
      std::move(options),
      base::BindLambdaForTesting(
          [&](ml::model_loader::mojom::CreateModelLoaderResult result,
              mojo::PendingRemote<ml::model_loader::mojom::ModelLoader>
                  remote) {
            EXPECT_EQ(result,
                      ml::model_loader::mojom::CreateModelLoaderResult::kOk);
            is_callback_called = true;
            run_loop.Quit();
          }));
  run_loop.Run();
  EXPECT_TRUE(is_callback_called);
}

// Tests the failure case model loader creation.
TEST_F(MLServiceImplCrOSTest, CreateModelLoaderFailed) {
  mojo::Remote<ml::model_loader::mojom::MLService> service_remote;
  CrOSMLServiceImpl::Create(service_remote.BindNewPipeAndPassReceiver());
  auto options = ml::model_loader::mojom::CreateModelLoaderOptions::New();
  options->num_threads = 2;
  options->model_format = ml::model_loader::mojom::ModelFormat::kTfLite;
  GetMlServiceConnection().SetCreateWebPlatformModelLoaderResult(
      ml::model_loader::mojom::CreateModelLoaderResult::kUnknownError);
  bool is_callback_called = false;
  base::RunLoop run_loop;
  service_remote->CreateModelLoader(
      std::move(options),
      base::BindLambdaForTesting(
          [&](ml::model_loader::mojom::CreateModelLoaderResult result,
              mojo::PendingRemote<ml::model_loader::mojom::ModelLoader>
                  remote) {
            EXPECT_EQ(result, ml::model_loader::mojom::CreateModelLoaderResult::
                                  kUnknownError);
            is_callback_called = true;
            run_loop.Quit();
          }));
  run_loop.Run();
  EXPECT_TRUE(is_callback_called);
}

// Tests the failure case of model loading.
TEST_F(MLServiceImplCrOSTest, LoadModelFailed) {
  mojo::Remote<ml::model_loader::mojom::MLService> service_remote;
  CrOSMLServiceImpl::Create(service_remote.BindNewPipeAndPassReceiver());
  auto options = ml::model_loader::mojom::CreateModelLoaderOptions::New();
  options->num_threads = 2;
  options->model_format = ml::model_loader::mojom::ModelFormat::kTfLite;
  GetMlServiceConnection().SetCreateWebPlatformModelLoaderResult(
      ml::model_loader::mojom::CreateModelLoaderResult::kOk);
  mojo::Remote<ml::model_loader::mojom::ModelLoader> loader_remote;
  bool is_callback_called = false;
  base::RunLoop run_loop_create_loader;
  service_remote->CreateModelLoader(
      std::move(options),
      base::BindLambdaForTesting(
          [&](ml::model_loader::mojom::CreateModelLoaderResult result,
              mojo::PendingRemote<ml::model_loader::mojom::ModelLoader>
                  remote) {
            EXPECT_EQ(result,
                      ml::model_loader::mojom::CreateModelLoaderResult::kOk);
            loader_remote.Bind(std::move(remote));
            is_callback_called = true;
            run_loop_create_loader.Quit();
          }));
  run_loop_create_loader.Run();
  EXPECT_TRUE(is_callback_called);

  // Loads a model.
  GetMlServiceConnection().SetLoadWebPlatformModelResult(
      ml::model_loader::mojom::LoadModelResult::kUnknownError);
  mojo_base::BigBuffer model_buffer;
  base::RunLoop run_loop_load_model;
  is_callback_called = false;
  loader_remote->Load(
      std::move(model_buffer),
      base::BindLambdaForTesting(
          [&](ml::model_loader::mojom::LoadModelResult result,
              mojo::PendingRemote<ml::model_loader::mojom::Model> remote,
              ml::model_loader::mojom::ModelInfoPtr info) {
            EXPECT_EQ(result,
                      ml::model_loader::mojom::LoadModelResult::kUnknownError);
            EXPECT_TRUE(info.is_null());
            is_callback_called = true;
            run_loop_load_model.Quit();
          }));
  run_loop_load_model.Run();
  EXPECT_TRUE(is_callback_called);
}

// Tests the case that successfully loads a model and does a computation.
TEST_F(MLServiceImplCrOSTest, LoadModelAndCompute) {
  mojo::Remote<ml::model_loader::mojom::MLService> service_remote;
  CrOSMLServiceImpl::Create(service_remote.BindNewPipeAndPassReceiver());
  auto options = ml::model_loader::mojom::CreateModelLoaderOptions::New();
  options->num_threads = 2;
  options->model_format = ml::model_loader::mojom::ModelFormat::kTfLite;
  GetMlServiceConnection().SetCreateWebPlatformModelLoaderResult(
      ml::model_loader::mojom::CreateModelLoaderResult::kOk);
  mojo::Remote<ml::model_loader::mojom::ModelLoader> loader_remote;
  bool is_callback_called = false;
  base::RunLoop run_loop_create_loader;
  service_remote->CreateModelLoader(
      std::move(options),
      base::BindLambdaForTesting(
          [&](ml::model_loader::mojom::CreateModelLoaderResult result,
              mojo::PendingRemote<ml::model_loader::mojom::ModelLoader>
                  remote) {
            EXPECT_EQ(result,
                      ml::model_loader::mojom::CreateModelLoaderResult::kOk);
            loader_remote.Bind(std::move(remote));
            is_callback_called = true;
            run_loop_create_loader.Quit();
          }));
  run_loop_create_loader.Run();
  EXPECT_TRUE(is_callback_called);

  // Loads a model.
  GetMlServiceConnection().SetLoadWebPlatformModelResult(
      ml::model_loader::mojom::LoadModelResult::kOk);

  auto model_info = ml::model_loader::mojom::ModelInfo::New();

  model_info->input_tensor_info["input_test"] =
      ml::model_loader::mojom::TensorInfo::New(
          123, ml::model_loader::mojom::DataType::kBool,
          std::vector<uint32_t>{1, 2, 3});
  model_info->output_tensor_info["output_test1"] =
      ml::model_loader::mojom::TensorInfo::New(
          321, ml::model_loader::mojom::DataType::kInt32,
          std::vector<uint32_t>{10, 20, 30});
  model_info->output_tensor_info["output_test2"] =
      ml::model_loader::mojom::TensorInfo::New(
          567, ml::model_loader::mojom::DataType::kFloat32,
          std::vector<uint32_t>{8, 10, 12});

  GetMlServiceConnection().SetWebPlatformModelInfo(std::move(model_info));
  mojo::Remote<ml::model_loader::mojom::Model> model_remote;
  mojo_base::BigBuffer model_buffer;
  base::RunLoop run_loop_load_model;
  is_callback_called = false;
  loader_remote->Load(
      std::move(model_buffer),
      base::BindLambdaForTesting(
          [&](ml::model_loader::mojom::LoadModelResult result,
              mojo::PendingRemote<ml::model_loader::mojom::Model> remote,
              ml::model_loader::mojom::ModelInfoPtr info) {
            EXPECT_EQ(result, ml::model_loader::mojom::LoadModelResult::kOk);
            EXPECT_FALSE(info.is_null());

            ASSERT_EQ(info->input_tensor_info.size(), 1u);
            ASSERT_TRUE(info->input_tensor_info.contains("input_test"));
            EXPECT_EQ(info->input_tensor_info["input_test"]->byte_size, 123u);
            EXPECT_EQ(info->input_tensor_info["input_test"]->data_type,
                      ml::model_loader::mojom::DataType::kBool);
            EXPECT_EQ(info->input_tensor_info["input_test"]->dimensions,
                      std::vector<uint32_t>({1, 2, 3}));

            ASSERT_EQ(info->output_tensor_info.size(), 2u);
            ASSERT_TRUE(info->output_tensor_info.contains("output_test1"));
            EXPECT_EQ(info->output_tensor_info["output_test1"]->byte_size,
                      321u);
            EXPECT_EQ(info->output_tensor_info["output_test1"]->data_type,
                      ml::model_loader::mojom::DataType::kInt32);
            EXPECT_EQ(info->output_tensor_info["output_test1"]->dimensions,
                      std::vector<uint32_t>({10, 20, 30}));

            ASSERT_TRUE(info->output_tensor_info.contains("output_test2"));
            ASSERT_EQ(info->output_tensor_info["output_test2"]->byte_size,
                      567u);
            EXPECT_EQ(info->output_tensor_info["output_test2"]->data_type,
                      ml::model_loader::mojom::DataType::kFloat32);
            EXPECT_EQ(info->output_tensor_info["output_test2"]->dimensions,
                      std::vector<uint32_t>({8, 10, 12}));

            model_remote.Bind(std::move(remote));

            is_callback_called = true;
            run_loop_load_model.Quit();
          }));
  run_loop_load_model.Run();
  EXPECT_TRUE(is_callback_called);

  // Does a computation.
  GetMlServiceConnection().SetWebPlatformModelComputeResult(
      ml::model_loader::mojom::ComputeResult::kOk);
  base::flat_map<std::string, std::vector<uint8_t>> compute_output;
  compute_output["some output1"] = std::vector<uint8_t>({123, 23, 21});
  compute_output["some output2"] = std::vector<uint8_t>({8, 5, 2});
  GetMlServiceConnection().SetOutputWebPlatformModelCompute(
      std::move(compute_output));
  base::flat_map<std::string, std::vector<uint8_t>> compute_input;
  compute_input["random input"] = std::vector<uint8_t>({0, 3, 5});
  base::RunLoop run_loop_compute;
  is_callback_called = false;
  model_remote->Compute(
      std::move(compute_input),
      base::BindLambdaForTesting(
          [&](ml::model_loader::mojom::ComputeResult result,
              const absl::optional<base::flat_map<
                  std::string, std::vector<uint8_t>>>& output_tensors) {
            ASSERT_TRUE(output_tensors.has_value());
            ASSERT_EQ(output_tensors->size(), 2u);
            ASSERT_TRUE(output_tensors->contains("some output1"));
            EXPECT_EQ(output_tensors->find("some output1")->second,
                      std::vector<uint8_t>({123, 23, 21}));
            ASSERT_TRUE(output_tensors->contains("some output2"));
            EXPECT_EQ(output_tensors->find("some output2")->second,
                      std::vector<uint8_t>({8, 5, 2}));

            is_callback_called = true;
            run_loop_compute.Quit();
          }));
  run_loop_compute.Run();
  EXPECT_TRUE(is_callback_called);
}

}  // namespace content
