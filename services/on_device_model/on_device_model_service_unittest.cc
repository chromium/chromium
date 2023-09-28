// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/on_device_model_service.h"

#include "base/files/file_path.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace on_device_model {
namespace {

class ResponseHolder : public mojom::StreamingResponder {
 public:
  mojo::PendingRemote<mojom::StreamingResponder> BindRemote() {
    mojo::PendingRemote<mojom::StreamingResponder> remote;
    receiver_.Bind(remote.InitWithNewPipeAndPassReceiver());
    return remote;
  }

  void OnResponse(const std::string& text) override {
    responses_.push_back(text);
  }

  void OnComplete() override { run_loop_.Quit(); }

  void WaitForCompletion() { run_loop_.Run(); }

  const std::vector<std::string> responses() const { return responses_; }

 private:
  base::RunLoop run_loop_;
  mojo::Receiver<mojom::StreamingResponder> receiver_{this};
  std::vector<std::string> responses_;
};

class OnDeviceModelServiceTest : public testing::Test {
 public:
  OnDeviceModelServiceTest()
      : service_impl_(service_.BindNewPipeAndPassReceiver()) {}

  mojo::Remote<mojom::OnDeviceModelService>& service() { return service_; }

  mojo::Remote<mojom::OnDeviceModel> LoadModel(
      mojom::LoadModelParamsPtr params) {
    base::RunLoop run_loop;
    mojo::Remote<mojom::OnDeviceModel> remote;
    service()->LoadModel(
        std::move(params),
        base::BindLambdaForTesting([&](mojom::LoadModelResultPtr result) {
          remote.Bind(std::move(result->get_model()));
          run_loop.Quit();
        }));
    run_loop.Run();
    return remote;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  mojo::Remote<mojom::OnDeviceModelService> service_;
  OnDeviceModelService service_impl_;
};

TEST_F(OnDeviceModelServiceTest, Responds) {
  auto model = LoadModel(
      mojom::LoadModelParams::New(base::FilePath(FILE_PATH_LITERAL("foo"))));
  {
    ResponseHolder response;
    model->Execute("bar", response.BindRemote());
    response.WaitForCompletion();
    const auto& responses = response.responses();
    EXPECT_EQ(responses.size(), 2u);
    EXPECT_EQ(responses[0], "Model: foo");
    EXPECT_EQ(responses[1], "Input: bar");
  }
  // Try another input on  the same model.
  {
    ResponseHolder response;
    model->Execute("cat", response.BindRemote());
    response.WaitForCompletion();
    const auto& responses = response.responses();
    EXPECT_EQ(responses.size(), 2u);
    EXPECT_EQ(responses[0], "Model: foo");
    EXPECT_EQ(responses[1], "Input: cat");
  }
}

}  // namespace
}  // namespace on_device_model
