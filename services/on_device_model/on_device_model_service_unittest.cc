// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/on_device_model_service.h"

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/on_device_model/public/cpp/model_assets.h"
#include "services/on_device_model/public/cpp/test_support/test_response_holder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace on_device_model {
namespace {

using ::testing::ElementsAre;

class ContextClientWaiter : public mojom::ContextClient {
 public:
  mojo::PendingRemote<mojom::ContextClient> BindRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void OnComplete(uint32_t tokens_processed) override {
    tokens_processed_ = tokens_processed;
    run_loop_.Quit();
  }

  int WaitForCompletion() {
    run_loop_.Run();
    return tokens_processed_;
  }

 private:
  base::RunLoop run_loop_;
  mojo::Receiver<mojom::ContextClient> receiver_{this};
  int tokens_processed_ = 0;
};

class OnDeviceModelServiceTest : public testing::Test {
 public:
  OnDeviceModelServiceTest()
      : service_impl_(service_.BindNewPipeAndPassReceiver()) {}

  mojo::Remote<mojom::OnDeviceModelService>& service() { return service_; }

  mojo::Remote<mojom::OnDeviceModel> LoadModel() {
    base::RunLoop run_loop;
    mojo::Remote<mojom::OnDeviceModel> remote;
    service()->LoadModel(
        mojom::LoadModelParams::New(), remote.BindNewPipeAndPassReceiver(),
        base::BindLambdaForTesting([&](mojom::LoadModelResult result) {
          EXPECT_EQ(mojom::LoadModelResult::kSuccess, result);
          run_loop.Quit();
        }));
    run_loop.Run();
    return remote;
  }

  mojo::Remote<mojom::OnDeviceModel> LoadAdaptation(
      mojom::OnDeviceModel& model) {
    base::RunLoop run_loop;
    mojo::Remote<mojom::OnDeviceModel> remote;
    model.LoadAdaptation(
        mojom::LoadAdaptationParams::New(), remote.BindNewPipeAndPassReceiver(),
        base::BindLambdaForTesting([&](mojom::LoadModelResult result) {
          EXPECT_EQ(mojom::LoadModelResult::kSuccess, result);
          run_loop.Quit();
        }));
    run_loop.Run();
    return remote;
  }

  mojom::InputOptionsPtr MakeInput(const std::string& input) {
    return mojom::InputOptions::New(input, std::nullopt, std::nullopt, false,
                                    std::nullopt, std::nullopt, std::nullopt,
                                    std::nullopt);
  }

  std::vector<std::string> GetResponses(mojom::OnDeviceModel& model,
                                        const std::string& input) {
    TestResponseHolder response;
    mojo::Remote<mojom::Session> session;
    model.StartSession(session.BindNewPipeAndPassReceiver());
    session->Execute(MakeInput(input), response.BindRemote());
    response.WaitForCompletion();
    return response.responses();
  }

  size_t GetNumModels() { return service_impl_.NumModelsForTesting(); }

  void FlushService() { service_.FlushForTesting(); }

 private:
  base::test::TaskEnvironment task_environment_;
  mojo::Remote<mojom::OnDeviceModelService> service_;
  OnDeviceModelService service_impl_;
};

TEST_F(OnDeviceModelServiceTest, Responds) {
  auto model = LoadModel();
  EXPECT_THAT(GetResponses(*model, "bar"), ElementsAre("Input: bar\n"));
  // Try another input on  the same model.
  EXPECT_THAT(GetResponses(*model, "cat"), ElementsAre("Input: cat\n"));
}

TEST_F(OnDeviceModelServiceTest, AddContext) {
  auto model = LoadModel();

  TestResponseHolder response;
  mojo::Remote<mojom::Session> session;
  model->StartSession(session.BindNewPipeAndPassReceiver());
  session->AddContext(MakeInput("cheese"), {});
  session->AddContext(MakeInput("more"), {});
  session->Execute(MakeInput("cheddar"), response.BindRemote());
  response.WaitForCompletion();

  EXPECT_THAT(
      response.responses(),
      ElementsAre("Context: cheese\n", "Context: more\n", "Input: cheddar\n"));
}

TEST_F(OnDeviceModelServiceTest, IgnoresContext) {
  auto model = LoadModel();

  TestResponseHolder response;
  mojo::Remote<mojom::Session> session;
  model->StartSession(session.BindNewPipeAndPassReceiver());
  session->AddContext(MakeInput("cheese"), {});
  session->Execute(
      mojom::InputOptions::New("cheddar", std::nullopt, std::nullopt,
                               /*ignore_context=*/true, std::nullopt,
                               std::nullopt, std::nullopt, std::nullopt),
      response.BindRemote());
  response.WaitForCompletion();

  EXPECT_THAT(response.responses(), ElementsAre("Input: cheddar\n"));
}

TEST_F(OnDeviceModelServiceTest, AddContextWithTokenLimits) {
  auto model = LoadModel();

  TestResponseHolder response;
  mojo::Remote<mojom::Session> session;
  model->StartSession(session.BindNewPipeAndPassReceiver());

  std::string input = "big cheese";
  ContextClientWaiter client1;
  session->AddContext(
      mojom::InputOptions::New(input, /*max_tokens=*/4, std::nullopt, false,
                               std::nullopt, std::nullopt, std::nullopt,
                               std::nullopt),
      client1.BindRemote());
  EXPECT_EQ(client1.WaitForCompletion(), 4);

  ContextClientWaiter client2;
  session->AddContext(
      mojom::InputOptions::New(input, std::nullopt, /*token_offset=*/4, false,
                               std::nullopt, std::nullopt, std::nullopt,
                               std::nullopt),
      client2.BindRemote());
  EXPECT_EQ(client2.WaitForCompletion(), 6);

  session->Execute(MakeInput("cheddar"), response.BindRemote());
  response.WaitForCompletion();

  EXPECT_THAT(
      response.responses(),
      ElementsAre("Context: big \n", "Context: cheese\n", "Input: cheddar\n"));
}

TEST_F(OnDeviceModelServiceTest, CancelsPreviousSession) {
  auto model = LoadModel();

  TestResponseHolder response1;
  mojo::Remote<mojom::Session> session1;
  model->StartSession(session1.BindNewPipeAndPassReceiver());
  session1->Execute(MakeInput("1"), response1.BindRemote());

  mojo::Remote<mojom::Session> session2;
  model->StartSession(session2.BindNewPipeAndPassReceiver());

  // First session should get canceled.
  base::RunLoop run_loop;
  session1.set_disconnect_handler(run_loop.QuitClosure());
  run_loop.Run();

  // Response from first session should still work since it was sent before
  // cancel.
  response1.WaitForCompletion();
  EXPECT_THAT(response1.responses(), ElementsAre("Input: 1\n"));

  // Second session still works.
  TestResponseHolder response2;
  session2->Execute(MakeInput("2"), response2.BindRemote());
  response2.WaitForCompletion();
  EXPECT_THAT(response2.responses(), ElementsAre("Input: 2\n"));
}

TEST_F(OnDeviceModelServiceTest, LoadsAdaptation) {
  auto model = LoadModel();
  auto adaptation1 = LoadAdaptation(*model);
  EXPECT_THAT(GetResponses(*model, "foo"), ElementsAre("Input: foo\n"));
  EXPECT_THAT(GetResponses(*adaptation1, "foo"),
              ElementsAre("Adaptation: 1\n", "Input: foo\n"));

  auto adaptation2 = LoadAdaptation(*model);
  EXPECT_THAT(GetResponses(*model, "foo"), ElementsAre("Input: foo\n"));
  EXPECT_THAT(GetResponses(*adaptation1, "foo"),
              ElementsAre("Adaptation: 1\n", "Input: foo\n"));
  EXPECT_THAT(GetResponses(*adaptation2, "foo"),
              ElementsAre("Adaptation: 2\n", "Input: foo\n"));
}

TEST_F(OnDeviceModelServiceTest, LoadingAdaptationCancelsSession) {
  auto model = LoadModel();

  mojo::Remote<mojom::Session> session;
  model->StartSession(session.BindNewPipeAndPassReceiver());
  session.reset_on_disconnect();

  LoadAdaptation(*model);
  FlushService();
  EXPECT_FALSE(session);
}

TEST_F(OnDeviceModelServiceTest, DeletesModel) {
  auto model1 = LoadModel();
  auto adaptation1 = LoadAdaptation(*model1);
  auto adaptation2 = LoadAdaptation(*model1);
  EXPECT_EQ(GetNumModels(), 1u);

  auto model2 = LoadModel();
  auto adaptation3 = LoadAdaptation(*model2);
  EXPECT_EQ(GetNumModels(), 2u);

  adaptation1.reset();
  adaptation2.reset();
  FlushService();
  EXPECT_EQ(GetNumModels(), 2u);

  model1.reset();
  FlushService();
  EXPECT_EQ(GetNumModels(), 1u);

  model2.reset();
  FlushService();
  EXPECT_EQ(GetNumModels(), 1u);

  adaptation3.reset();
  FlushService();
  EXPECT_EQ(GetNumModels(), 0u);
}

}  // namespace
}  // namespace on_device_model
