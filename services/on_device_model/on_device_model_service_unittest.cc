// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/on_device_model_service.h"

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "services/on_device_model/public/cpp/model_assets.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace on_device_model {
namespace {

using ::testing::ElementsAre;

class ResponseHolder : public mojom::StreamingResponder {
 public:
  mojo::PendingRemote<mojom::StreamingResponder> BindRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void OnResponse(const std::string& text) override {
    responses_.push_back(text);
  }

  void OnComplete(mojom::ResponseStatus status) override { run_loop_.Quit(); }

  void WaitForCompletion() { run_loop_.Run(); }

  const std::vector<std::string> responses() const { return responses_; }

 private:
  base::RunLoop run_loop_;
  mojo::Receiver<mojom::StreamingResponder> receiver_{this};
  std::vector<std::string> responses_;
};

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
        mojom::LoadModelParams::New(ModelAssets(), 0),
        remote.BindNewPipeAndPassReceiver(),
        base::BindLambdaForTesting([&](mojom::LoadModelResult result) {
          EXPECT_EQ(mojom::LoadModelResult::kSuccess, result);
          run_loop.Quit();
        }));
    run_loop.Run();
    return remote;
  }

  mojom::InputOptionsPtr MakeInput(const std::string& input) {
    return mojom::InputOptions::New(input, std::nullopt, std::nullopt, false,
                                    std::nullopt);
  }

 private:
  base::test::TaskEnvironment task_environment_;
  mojo::Remote<mojom::OnDeviceModelService> service_;
  OnDeviceModelService service_impl_;
};

TEST_F(OnDeviceModelServiceTest, Responds) {
  auto model = LoadModel();
  {
    ResponseHolder response;
    mojo::Remote<mojom::Session> session;
    model->StartSession(session.BindNewPipeAndPassReceiver());
    session->Execute(MakeInput("bar"), response.BindRemote());
    response.WaitForCompletion();
    const auto& responses = response.responses();
    EXPECT_EQ(responses.size(), 1u);
    EXPECT_EQ(responses[0], "Input: bar\n");
  }
  // Try another input on  the same model.
  {
    ResponseHolder response;
    mojo::Remote<mojom::Session> session;
    model->StartSession(session.BindNewPipeAndPassReceiver());
    session->Execute(MakeInput("cat"), response.BindRemote());
    response.WaitForCompletion();
    const auto& responses = response.responses();
    EXPECT_EQ(responses.size(), 1u);
    EXPECT_EQ(responses[0], "Input: cat\n");
  }
}

TEST_F(OnDeviceModelServiceTest, AddContext) {
  auto model = LoadModel();

  ResponseHolder response;
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

  ResponseHolder response;
  mojo::Remote<mojom::Session> session;
  model->StartSession(session.BindNewPipeAndPassReceiver());
  session->AddContext(MakeInput("cheese"), {});
  session->Execute(
      mojom::InputOptions::New("cheddar", std::nullopt, std::nullopt,
                               /*ignore_context=*/true, std::nullopt),
      response.BindRemote());
  response.WaitForCompletion();

  EXPECT_THAT(response.responses(), ElementsAre("Input: cheddar\n"));
}

TEST_F(OnDeviceModelServiceTest, AddContextWithTokenLimits) {
  auto model = LoadModel();

  ResponseHolder response;
  mojo::Remote<mojom::Session> session;
  model->StartSession(session.BindNewPipeAndPassReceiver());

  std::string input = "big cheese";
  ContextClientWaiter client1;
  session->AddContext(
      mojom::InputOptions::New(input, /*max_tokens=*/4, std::nullopt, false,
                               std::nullopt),
      client1.BindRemote());
  EXPECT_EQ(client1.WaitForCompletion(), 4);

  ContextClientWaiter client2;
  session->AddContext(
      mojom::InputOptions::New(input, std::nullopt, /*token_offset=*/4, false,
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

  ResponseHolder response1;
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
  ResponseHolder response2;
  session2->Execute(MakeInput("2"), response2.BindRemote());
  response2.WaitForCompletion();
  EXPECT_THAT(response2.responses(), ElementsAre("Input: 2\n"));
}

}  // namespace
}  // namespace on_device_model
