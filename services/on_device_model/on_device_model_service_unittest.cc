// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/on_device_model_service.h"

#include "base/files/scoped_temp_file.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/on_device_model/fake/fake_chrome_ml_api.h"
#include "services/on_device_model/fake/on_device_model_fake.h"
#include "services/on_device_model/ml/chrome_ml_types.h"
#include "services/on_device_model/ml/gpu_blocklist.h"
#include "services/on_device_model/on_device_model_mojom_impl.h"
#include "services/on_device_model/public/cpp/model_assets.h"
#include "services/on_device_model/public/cpp/service_client.h"
#include "services/on_device_model/public/cpp/test_support/test_response_holder.h"
#include "services/on_device_model/public/cpp/text_safety_assets.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
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
    return *tokens_processed_;
  }

  bool IsComplete() const { return tokens_processed_.has_value(); }

 private:
  base::RunLoop run_loop_;
  mojo::Receiver<mojom::ContextClient> receiver_{this};
  std::optional<int> tokens_processed_;
};

class FakeFile {
 public:
  explicit FakeFile(const std::string& content) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    CHECK(temp_file_.Create());
    base::File file(temp_file_.path(), base::File::FLAG_OPEN |
                                           base::File::FLAG_WRITE |
                                           base::File::FLAG_READ);
    CHECK(file.IsValid());
    file.WriteAtCurrentPos(base::as_byte_span(content));
  }
  ~FakeFile() = default;

  base::File Open() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    return base::File(temp_file_.path(), base::File::FLAG_OPEN |
                                             base::File::FLAG_WRITE |
                                             base::File::FLAG_READ);
  }

  base::FilePath Path() { return temp_file_.path(); }

 private:
  base::ScopedTempFile temp_file_;
};

class OnDeviceModelServiceTest : public testing::Test {
 public:
  OnDeviceModelServiceTest()
      : service_impl_(service_.BindNewPipeAndPassReceiver(),
                      *fake_ml::GetFakeChromeML()) {}

  mojo::Remote<mojom::OnDeviceModelService>& service() { return service_; }

  mojo::Remote<mojom::OnDeviceModel> LoadModel(
      ml::ModelBackendType backend_type = ml::ModelBackendType::kGpuBackend,
      ml::ModelPerformanceHint performance_hint =
          ml::ModelPerformanceHint::kHighestQuality) {
    mojo::Remote<mojom::OnDeviceModel> remote;
    auto params = mojom::LoadModelParams::New();
    params->backend_type = backend_type;
    params->performance_hint = performance_hint;
    params->max_tokens = 8000;
    params->assets = ModelAssets::FromPath(base::FilePath());
    base::test::TestFuture<mojom::LoadModelResult> future;
    service()->LoadModel(std::move(params), remote.BindNewPipeAndPassReceiver(),
                         future.GetCallback());
    EXPECT_EQ(future.Get(), mojom::LoadModelResult::kSuccess);
    return remote;
  }

  mojo::Remote<mojom::OnDeviceModel> LoadAdaptationWithParams(
      mojom::OnDeviceModel& model,
      mojom::LoadAdaptationParamsPtr adaptation_params) {
    mojo::Remote<mojom::OnDeviceModel> remote;
    base::test::TestFuture<mojom::LoadModelResult> future;
    model.LoadAdaptation(std::move(adaptation_params),
                         remote.BindNewPipeAndPassReceiver(),
                         future.GetCallback());
    EXPECT_EQ(future.Get(), mojom::LoadModelResult::kSuccess);
    return remote;
  }

  mojo::Remote<mojom::OnDeviceModel> LoadAdaptation(
      mojom::OnDeviceModel& model,
      base::File adaptation_data) {
    auto params = mojom::LoadAdaptationParams::New();
    params->assets.weights = std::move(adaptation_data);
    return LoadAdaptationWithParams(model, std::move(params));
  }

  mojo::Remote<mojom::OnDeviceModel> LoadAdaptation(
      mojom::OnDeviceModel& model,
      base::FilePath adaptation_path) {
    auto params = mojom::LoadAdaptationParams::New();
    params->assets.weights_path = std::move(adaptation_path);
    return LoadAdaptationWithParams(model, std::move(params));
  }

  mojom::AppendOptionsPtr MakeInput(const std::string& input) {
    return MakeInput({ml::InputPiece(input)});
  }

  mojom::AppendOptionsPtr MakeInput(std::vector<ml::InputPiece> input) {
    auto options = mojom::AppendOptions::New();
    options->input = mojom::Input::New(std::move(input));
    return options;
  }

  std::vector<std::string> GetResponses(mojom::OnDeviceModel& model,
                                        const std::string& input) {
    TestResponseHolder response;
    mojo::Remote<mojom::Session> session;
    model.StartSession(session.BindNewPipeAndPassReceiver(), nullptr);
    auto options = mojom::AppendOptions::New();
    options->input =
        mojom::Input::New(std::vector<ml::InputPiece>{ml::InputPiece(input)});
    session->Append(std::move(options), {});
    session->Generate(mojom::GenerateOptions::New(), response.BindRemote());
    response.WaitForCompletion();
    return response.responses();
  }

  std::unique_ptr<ContextClientWaiter> AppendAndFlush(
      mojo::Remote<mojom::Session>& session,
      const std::string& input) {
    auto client = std::make_unique<ContextClientWaiter>();
    session->Append(MakeInput(input), client->BindRemote());
    session.FlushForTesting();
    return client;
  }

  size_t GetNumModels() { return service_impl_.NumModelsForTesting(); }

  void ForceQueueing(bool force) {
    service_impl_.SetForceQueueingForTesting(force);
  }

  void FlushService() { service_.FlushForTesting(); }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  mojo::Remote<mojom::OnDeviceModelService> service_;
  OnDeviceModelService service_impl_;
  base::test::ScopedFeatureList feature_list_{
      ml::kOnDeviceModelAllowGpuForTesting};
};

TEST_F(OnDeviceModelServiceTest, IdleTimeout) {
  auto model = LoadModel();
  mojo::Remote<mojom::Session> session;
  model->StartSession(session.BindNewPipeAndPassReceiver(), nullptr);

  base::test::TestFuture<uint32_t, const std::string&> model_future;
  base::test::TestFuture<uint32_t, const std::string&> session_future;
  model.set_disconnect_with_reason_handler(model_future.GetCallback());
  session.set_disconnect_with_reason_handler(session_future.GetCallback());

  session->Append(MakeInput("foo"), {});
  task_environment_.FastForwardBy(kDefaultModelIdleTimeout - base::Seconds(1));
  EXPECT_FALSE(model_future.IsReady());
  EXPECT_FALSE(session_future.IsReady());

  // Another call to the session should reset timeout.
  session->Append(MakeInput("bar"), {});
  task_environment_.FastForwardBy(kDefaultModelIdleTimeout - base::Seconds(1));
  EXPECT_FALSE(model_future.IsReady());
  EXPECT_FALSE(session_future.IsReady());

  // A new session should reset timeout.
  mojo::Remote<mojom::Session> session2;
  model->StartSession(session2.BindNewPipeAndPassReceiver(), nullptr);
  task_environment_.FastForwardBy(kDefaultModelIdleTimeout - base::Seconds(1));
  EXPECT_FALSE(model_future.IsReady());
  EXPECT_FALSE(session_future.IsReady());

  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_EQ(std::get<0>(model_future.Get()),
            static_cast<uint32_t>(ModelDisconnectReason::kIdleShutdown));
  EXPECT_EQ(std::get<0>(session_future.Get()),
            static_cast<uint32_t>(ModelDisconnectReason::kIdleShutdown));
}

TEST_F(OnDeviceModelServiceTest, Responds) {
  auto model = LoadModel();
  EXPECT_THAT(GetResponses(*model, "bar"), ElementsAre("bar"));
  // Try another input on  the same model.
  EXPECT_THAT(GetResponses(*model, "cat"), ElementsAre("cat"));
}

TEST_F(OnDeviceModelServiceTest, Append) {
  auto model = LoadModel();

  TestResponseHolder response;
  mojo::Remote<mojom::Session> session;
  model->StartSession(session.BindNewPipeAndPassReceiver(), nullptr);
  session->Append(MakeInput("cheese"), {});
  session->Append(MakeInput("more"), {});
  session->Append(MakeInput("cheddar"), {});
  session->Generate(mojom::GenerateOptions::New(), response.BindRemote());
  response.WaitForCompletion();

  EXPECT_THAT(response.responses(), ElementsAre("cheese", "more", "cheddar"));
}

TEST_F(OnDeviceModelServiceTest, PerSessionSamplingParams) {
  auto model = LoadModel();

  // Sampling params passed at session creation are used during Generate().
  auto session_params = mojom::SessionParams::New();
  session_params->top_k = 2;
  session_params->temperature = 0.5;

  TestResponseHolder response;
  mojo::Remote<mojom::Session> session;
  model->StartSession(session.BindNewPipeAndPassReceiver(),
                      std::move(session_params));

  session->Append(MakeInput("cheese"), {});
  session->Append(MakeInput("more"), {});
  session->Append(MakeInput("cheddar"), {});
  session->Generate(mojom::GenerateOptions::New(), response.BindRemote());
  response.WaitForCompletion();

  EXPECT_THAT(response.responses(),
              ElementsAre("TopK: 2, Temp: 0.5", "cheese", "more", "cheddar"));
}

TEST_F(OnDeviceModelServiceTest, CloneContextAndContinue) {
  auto model = LoadModel();

  mojo::Remote<mojom::Session> session;
  model->StartSession(session.BindNewPipeAndPassReceiver(), nullptr);
  session->Append(MakeInput("cheese"), {});
  session->Append(MakeInput("more"), {});

  mojo::Remote<mojom::Session> cloned;
  session->Clone(cloned.BindNewPipeAndPassReceiver());

  {
    TestResponseHolder response;
    cloned->Generate(mojom::GenerateOptions::New(), response.BindRemote());
    response.WaitForCompletion();
    EXPECT_THAT(response.responses(), ElementsAre("cheese", "more"));
  }
  {
    TestResponseHolder response;
    session->Generate(mojom::GenerateOptions::New(), response.BindRemote());
    response.WaitForCompletion();
    EXPECT_THAT(response.responses(), ElementsAre("cheese", "more"));
  }

  session->Append(MakeInput("foo"), {});
  cloned->Append(MakeInput("bar"), {});
  {
    TestResponseHolder response;
    session->Generate(mojom::GenerateOptions::New(), response.BindRemote());
    response.WaitForCompletion();
    EXPECT_THAT(response.responses(), ElementsAre("cheese", "more", "foo"));
  }
  {
    TestResponseHolder response;
    cloned->Generate(mojom::GenerateOptions::New(), response.BindRemote());
    response.WaitForCompletion();
    EXPECT_THAT(response.responses(), ElementsAre("cheese", "more", "bar"));
  }
}

TEST_F(OnDeviceModelServiceTest, MultipleSessionsAppend) {
  auto model = LoadModel();

  TestResponseHolder response1, response2, response3, response4, response5;
  mojo::Remote<mojom::Session> session1, session2, session3, session4, session5;

  model->StartSession(session1.BindNewPipeAndPassReceiver(), nullptr);
  model->StartSession(session2.BindNewPipeAndPassReceiver(), nullptr);

  session1->Append(MakeInput("cheese"), {});
  session1->Append(MakeInput("more"), {});
  session2->Append(MakeInput("apple"), {});

  session1->Clone(session3.BindNewPipeAndPassReceiver());
  session1->Append(MakeInput("cheddar"), {});
  session1->Generate(mojom::GenerateOptions::New(), response1.BindRemote());

  session2->Append(MakeInput("banana"), {});

  session2->Clone(session4.BindNewPipeAndPassReceiver());
  session2->Append(MakeInput("candy"), {});
  session2->Generate(mojom::GenerateOptions::New(), response2.BindRemote());

  session4->Clone(session5.BindNewPipeAndPassReceiver());
  session4->Append(MakeInput("chip"), {});
  session4->Generate(mojom::GenerateOptions::New(), response3.BindRemote());

  session3->Append(MakeInput("choco"), {});
  session3->Generate(mojom::GenerateOptions::New(), response4.BindRemote());

  session5->Append(MakeInput("orange"), {});
  session5->Generate(mojom::GenerateOptions::New(), response5.BindRemote());

  response1.WaitForCompletion();
  response2.WaitForCompletion();
  response3.WaitForCompletion();
  response4.WaitForCompletion();
  response5.WaitForCompletion();

  EXPECT_THAT(response1.responses(), ElementsAre("cheese", "more", "cheddar"));
  EXPECT_THAT(response2.responses(), ElementsAre("apple", "banana", "candy"));
  EXPECT_THAT(response3.responses(), ElementsAre("apple", "banana", "chip"));
  EXPECT_THAT(response4.responses(), ElementsAre("cheese", "more", "choco"));
  EXPECT_THAT(response5.responses(), ElementsAre("apple", "banana", "orange"));
}

TEST_F(OnDeviceModelServiceTest, CountTokens) {
  auto model = LoadModel();

  TestResponseHolder response;
  mojo::Remote<mojom::Session> session;
  model->StartSession(session.BindNewPipeAndPassReceiver(), nullptr);
  session->Append(MakeInput("cheese"), {});
  session->Append(MakeInput("more"), {});

  std::string input = "cheddar";
  session->Append(MakeInput(input), {});
  session->Generate(mojom::GenerateOptions::New(), response.BindRemote());
  response.WaitForCompletion();

  // 3 context.
  EXPECT_THAT(response.output_token_count(), 3);
}

TEST_F(OnDeviceModelServiceTest, AppendWithTokenLimits) {
  auto model = LoadModel();

  TestResponseHolder response;
  mojo::Remote<mojom::Session> session;
  model->StartSession(session.BindNewPipeAndPassReceiver(), nullptr);

  std::string input = "big cheese";
  ContextClientWaiter client1;
  auto max_input = MakeInput("big cheese");
  max_input->max_tokens = 4;
  session->Append(std::move(max_input), client1.BindRemote());
  EXPECT_EQ(client1.WaitForCompletion(), 4);

  ContextClientWaiter client2;
  auto offset_input = MakeInput("big cheese");
  session->Append(std::move(offset_input), client2.BindRemote());
  EXPECT_EQ(client2.WaitForCompletion(), 10);

  session->Append(MakeInput("cheddar"), {});
  session->Generate(mojom::GenerateOptions::New(), response.BindRemote());
  response.WaitForCompletion();

  EXPECT_THAT(response.responses(),
              ElementsAre("big ", "big cheese", "cheddar"));
}

TEST_F(OnDeviceModelServiceTest, MultipleSessionsWaitPreviousSession) {
  auto model = LoadModel();

  TestResponseHolder response1;
  mojo::Remote<mojom::Session> session1;
  model->StartSession(session1.BindNewPipeAndPassReceiver(), nullptr);
  session1->Append(MakeInput("1"), {});
  session1->Generate(mojom::GenerateOptions::New(), response1.BindRemote());

  mojo::Remote<mojom::Session> session2;
  model->StartSession(session2.BindNewPipeAndPassReceiver(), nullptr);

  // First session should not get canceled.
  session1.reset_on_disconnect();
  FlushService();
  EXPECT_TRUE(session1);

  // Response from first session should still work.
  response1.WaitForCompletion();
  EXPECT_THAT(response1.responses(), ElementsAre("1"));

  // Second session still works.
  TestResponseHolder response2;
  session2->Append(MakeInput("2"), {});
  session2->Generate(mojom::GenerateOptions::New(), response2.BindRemote());
  response2.WaitForCompletion();
  EXPECT_THAT(response2.responses(), ElementsAre("2"));
}

TEST_F(OnDeviceModelServiceTest, LoadsAdaptation) {
  FakeFile weights1("Adapt1");
  FakeFile weights2("Adapt2");
  auto model = LoadModel();
  auto adaptation1 = LoadAdaptation(*model, weights1.Open());
  EXPECT_THAT(GetResponses(*model, "foo"), ElementsAre("foo"));
  EXPECT_THAT(GetResponses(*adaptation1, "foo"),
              ElementsAre("Adaptation: Adapt1 (0)", "foo"));

  auto adaptation2 = LoadAdaptation(*model, weights2.Open());
  EXPECT_THAT(GetResponses(*model, "foo"), ElementsAre("foo"));
  EXPECT_THAT(GetResponses(*adaptation1, "foo"),
              ElementsAre("Adaptation: Adapt1 (0)", "foo"));
  EXPECT_THAT(GetResponses(*adaptation2, "foo"),
              ElementsAre("Adaptation: Adapt2 (1)", "foo"));
  EXPECT_THAT(GetResponses(*adaptation1, "foo"),
              ElementsAre("Adaptation: Adapt1 (0)", "foo"));
}

TEST_F(OnDeviceModelServiceTest, LoadsAdaptationWithPath) {
  FakeFile weights1("Adapt1");
  FakeFile weights2("Adapt2");
  auto model = LoadModel(ml::ModelBackendType::kApuBackend);
  auto adaptation1 = LoadAdaptation(*model, weights1.Path());
  EXPECT_THAT(GetResponses(*model, "foo"), ElementsAre("foo"));
  EXPECT_THAT(GetResponses(*adaptation1, "foo"),
              ElementsAre("Adaptation: Adapt1 (0)", "foo"));

  auto adaptation2 = LoadAdaptation(*model, weights2.Path());
  EXPECT_THAT(GetResponses(*model, "foo"), ElementsAre("foo"));
  EXPECT_THAT(GetResponses(*adaptation1, "foo"),
              ElementsAre("Adaptation: Adapt1 (0)", "foo"));
  EXPECT_THAT(GetResponses(*adaptation2, "foo"),
              ElementsAre("Adaptation: Adapt2 (1)", "foo"));
  EXPECT_THAT(GetResponses(*adaptation1, "foo"),
              ElementsAre("Adaptation: Adapt1 (0)", "foo"));
}

TEST_F(OnDeviceModelServiceTest, LoadingAdaptationDoesNotCancelSession) {
  FakeFile weights1("Adapt1");
  auto model = LoadModel();

  mojo::Remote<mojom::Session> session;
  model->StartSession(session.BindNewPipeAndPassReceiver(), nullptr);
  session.reset_on_disconnect();

  LoadAdaptation(*model, weights1.Open());
  FlushService();
  EXPECT_TRUE(session);
}

TEST_F(OnDeviceModelServiceTest, DeletesModel) {
  FakeFile weights1("Adapt1");
  FakeFile weights2("Adapt2");
  FakeFile weights3("Adapt3");
  auto model1 = LoadModel();
  auto adaptation1 = LoadAdaptation(*model1, weights1.Open());
  auto adaptation2 = LoadAdaptation(*model1, weights2.Open());
  EXPECT_EQ(GetNumModels(), 1u);

  auto model2 = LoadModel();
  auto adaptation3 = LoadAdaptation(*model2, weights3.Open());
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

TEST_F(OnDeviceModelServiceTest, Score) {
  auto model = LoadModel();

  mojo::Remote<mojom::Session> session;
  model->StartSession(session.BindNewPipeAndPassReceiver(), nullptr);
  session->Append(MakeInput("hi"), {});

  {
    base::test::TestFuture<float> future;
    session->Score("x", future.GetCallback());
    EXPECT_EQ(future.Get(), float('x'));
  }
  {
    base::test::TestFuture<float> future;
    session->Score("y", future.GetCallback());
    EXPECT_EQ(future.Get(), float('y'));
  }
}

TEST_F(OnDeviceModelServiceTest, AppendWithTokens) {
  auto model = LoadModel();

  TestResponseHolder response;
  mojo::Remote<mojom::Session> session;
  model->StartSession(session.BindNewPipeAndPassReceiver(), nullptr);
  {
    std::vector<ml::InputPiece> pieces;
    pieces.push_back(ml::Token::kSystem);
    pieces.push_back("hi");
    pieces.push_back(ml::Token::kEnd);
    session->Append(MakeInput(std::move(pieces)), {});
  }
  {
    std::vector<ml::InputPiece> pieces;
    pieces.push_back(ml::Token::kModel);
    pieces.push_back("hello");
    pieces.push_back(ml::Token::kEnd);
    session->Append(MakeInput(std::move(pieces)), {});
  }
  {
    std::vector<ml::InputPiece> pieces;
    pieces.push_back(ml::Token::kUser);
    pieces.push_back("bye");
    session->Append(MakeInput(std::move(pieces)), {});
    session->Generate(mojom::GenerateOptions::New(), response.BindRemote());
  }
  response.WaitForCompletion();

  EXPECT_THAT(response.responses(),
              ElementsAre("System: hi End.", "Model: hello End.", "User: bye"));
}

TEST_F(OnDeviceModelServiceTest, AppendWithImages) {
  auto model = LoadModel();
  mojo::Remote<mojom::Session> session;
  auto params = mojom::SessionParams::New();
  params->capabilities.Put(CapabilityFlags::kImageInput);
  model->StartSession(session.BindNewPipeAndPassReceiver(), std::move(params));

  {
    std::vector<ml::InputPiece> pieces;
    pieces.push_back("cheddar");

    SkBitmap cheesy_bitmap;
    cheesy_bitmap.allocPixels(
        SkImageInfo::Make(7, 21, kRGBA_8888_SkColorType, kOpaque_SkAlphaType),
        0);
    cheesy_bitmap.eraseColor(SK_ColorYELLOW);
    pieces.push_back(cheesy_bitmap);

    pieces.push_back("cheese");

    session->Append(MakeInput(std::move(pieces)), {});
  }

  TestResponseHolder response;
  {
    std::vector<ml::InputPiece> pieces;
    pieces.push_back("bleu");

    SkBitmap moldy_cheese;
    moldy_cheese.allocPixels(
        SkImageInfo::Make(63, 42, kRGBA_8888_SkColorType, kOpaque_SkAlphaType),
        0);
    moldy_cheese.eraseColor(SK_ColorBLUE);
    pieces.push_back(moldy_cheese);

    pieces.push_back("cheese");

    session->Append(MakeInput(std::move(pieces)), {});
    session->Generate(mojom::GenerateOptions::New(), response.BindRemote());
    response.WaitForCompletion();
  }

  EXPECT_THAT(response.responses(),
              ElementsAre("cheddar[Bitmap of size 7x21]cheese",
                          "bleu[Bitmap of size 63x42]cheese"));
}

TEST_F(OnDeviceModelServiceTest, ClassifyTextSafety) {
  FakeFile ts_data("fake_ts_data");
  FakeFile ts_sp_model("fake_ts_sp_model");
  TextSafetyLoaderParams params;
  params.ts_paths.emplace();
  params.ts_paths->data = ts_data.Path();
  params.ts_paths->sp_model = ts_sp_model.Path();
  mojo::Remote<mojom::TextSafetyModel> model;
  service()->LoadTextSafetyModel(LoadTextSafetyParams(params),
                                 model.BindNewPipeAndPassReceiver());
  mojo::Remote<mojom::TextSafetySession> session;
  model->StartSession(session.BindNewPipeAndPassReceiver());
  base::test::TestFuture<mojom::SafetyInfoPtr> future1;
  base::test::TestFuture<mojom::SafetyInfoPtr> future2;
  session->ClassifyTextSafety("unsafe text", future1.GetCallback());
  session->ClassifyTextSafety("reasonable text", future2.GetCallback());
  auto resp1 = future1.Take();
  auto resp2 = future2.Take();

  ASSERT_TRUE(resp1);
  EXPECT_THAT(resp1->class_scores, ElementsAre(0.8, 0.8));
  ASSERT_TRUE(resp2);
  EXPECT_THAT(resp2->class_scores, ElementsAre(0.2, 0.2));
}

TEST_F(OnDeviceModelServiceTest, CloneTextSafety) {
  FakeFile ts_data("fake_ts_data");
  FakeFile ts_sp_model("fake_ts_sp_model");
  TextSafetyLoaderParams params;
  params.ts_paths.emplace();
  params.ts_paths->data = ts_data.Path();
  params.ts_paths->sp_model = ts_sp_model.Path();
  mojo::Remote<mojom::TextSafetyModel> model;
  service()->LoadTextSafetyModel(LoadTextSafetyParams(params),
                                 model.BindNewPipeAndPassReceiver());

  mojo::Remote<mojom::TextSafetySession> session;
  model->StartSession(session.BindNewPipeAndPassReceiver());
  {
    base::test::TestFuture<mojom::SafetyInfoPtr> future;
    session->ClassifyTextSafety("unsafe text", future.GetCallback());
    EXPECT_THAT(future.Take()->class_scores, ElementsAre(0.8, 0.8));
  }

  mojo::Remote<mojom::TextSafetySession> clone;
  session->Clone(clone.BindNewPipeAndPassReceiver());
  {
    base::test::TestFuture<mojom::SafetyInfoPtr> future;
    clone->ClassifyTextSafety("unsafe text", future.GetCallback());
    EXPECT_THAT(future.Take()->class_scores, ElementsAre(0.8, 0.8));
  }
}

TEST_F(OnDeviceModelServiceTest, GpuBlocked) {
  // The fake implementation of ChromeML always blocks GPU by default.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(ml::kOnDeviceModelAllowGpuForTesting);

  mojo::Remote<mojom::OnDeviceModel> remote;
  auto params = mojom::LoadModelParams::New();
  params->backend_type = ml::ModelBackendType::kGpuBackend;
  params->max_tokens = 8000;
  params->assets = ModelAssets::FromPath(base::FilePath());
  base::test::TestFuture<mojom::LoadModelResult> future;
  service()->LoadModel(std::move(params), remote.BindNewPipeAndPassReceiver(),
                       future.GetCallback());
  EXPECT_EQ(future.Get(), mojom::LoadModelResult::kGpuBlocked);
}

TEST_F(OnDeviceModelServiceTest, CpuModel) {
  // The fake implementation of ChromeML always blocks GPU by default.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(ml::kOnDeviceModelAllowGpuForTesting);

  auto model = LoadModel(ml::ModelBackendType::kCpuBackend);
  EXPECT_THAT(GetResponses(*model, "foo"), ElementsAre("CPU backend", "foo"));
}

TEST_F(OnDeviceModelServiceTest, PerformanceHint) {
  auto model = LoadModel(ml::ModelBackendType::kGpuBackend,
                         ml::ModelPerformanceHint::kFastestInference);
  EXPECT_THAT(GetResponses(*model, "foo"),
              ElementsAre("Fastest inference", "foo"));
}

TEST_F(OnDeviceModelServiceTest, Capabilities) {
  auto expect_capabilities = [&](const std::string& data,
                                 const Capabilities& expected) {
    FakeFile file(data);
    ModelFile model_file(file.Open());
    base::test::TestFuture<const Capabilities&> future;
    service()->GetCapabilities(std::move(model_file), future.GetCallback());
    EXPECT_EQ(expected, future.Take());
  };
  expect_capabilities("none", {});
  expect_capabilities("image", {CapabilityFlags::kImageInput});
  expect_capabilities("audio", {CapabilityFlags::kAudioInput});
  expect_capabilities("image audio", {CapabilityFlags::kImageInput,
                                      CapabilityFlags::kAudioInput});
}

TEST_F(OnDeviceModelServiceTest, CapabilitiesFromFilePath) {
  auto expect_capabilities = [&](const std::string& data,
                                 const Capabilities& expected) {
    FakeFile file(data);
    ModelFile model_file(file.Path());
    base::test::TestFuture<const Capabilities&> future;
    service()->GetCapabilities(std::move(model_file), future.GetCallback());
    EXPECT_EQ(expected, future.Take());
  };
  expect_capabilities("none", {});
  expect_capabilities("image", {CapabilityFlags::kImageInput});
  expect_capabilities("audio", {CapabilityFlags::kAudioInput});
  expect_capabilities("image audio", {CapabilityFlags::kImageInput,
                                      CapabilityFlags::kAudioInput});
}

TEST_F(OnDeviceModelServiceTest, SetPriority) {
  auto model = LoadModel();

  mojo::Remote<mojom::Session> background;
  model->StartSession(background.BindNewPipeAndPassReceiver(), nullptr);
  background->SetPriority(mojom::Priority::kBackground);

  mojo::Remote<mojom::Session> foreground;
  model->StartSession(foreground.BindNewPipeAndPassReceiver(), nullptr);

  base::HistogramTester histogram_tester;

  ForceQueueing(true);
  auto bg_waiter = AppendAndFlush(background, "bg");
  auto fg_waiter = AppendAndFlush(foreground, "fg");

  constexpr char kForegroundHistogram[] = "OnDeviceModel.QueueTime.Foreground";
  constexpr char kBackgroundHistogram[] = "OnDeviceModel.QueueTime.Background";
  histogram_tester.ExpectTotalCount(kForegroundHistogram, 0);
  histogram_tester.ExpectTotalCount(kBackgroundHistogram, 0);
  ForceQueueing(false);

  fg_waiter->WaitForCompletion();
  EXPECT_FALSE(bg_waiter->IsComplete());
  histogram_tester.ExpectTotalCount(kForegroundHistogram, 1);
  histogram_tester.ExpectTotalCount(kBackgroundHistogram, 0);

  ForceQueueing(true);

  // Add another call to fg client, should jump ahead of bg again.
  fg_waiter = AppendAndFlush(foreground, "fg");
  ForceQueueing(false);

  fg_waiter->WaitForCompletion();
  EXPECT_FALSE(bg_waiter->IsComplete());
  histogram_tester.ExpectTotalCount(kForegroundHistogram, 2);
  histogram_tester.ExpectTotalCount(kBackgroundHistogram, 0);

  bg_waiter->WaitForCompletion();
  histogram_tester.ExpectTotalCount(kForegroundHistogram, 2);
  histogram_tester.ExpectTotalCount(kBackgroundHistogram, 1);
}

TEST_F(OnDeviceModelServiceTest, SetPriorityAfterQueue) {
  auto model = LoadModel();

  mojo::Remote<mojom::Session> background;
  model->StartSession(background.BindNewPipeAndPassReceiver(), nullptr);

  mojo::Remote<mojom::Session> foreground;
  model->StartSession(foreground.BindNewPipeAndPassReceiver(), nullptr);

  ForceQueueing(true);
  auto bg_waiter = AppendAndFlush(background, "bg");
  auto fg_waiter = AppendAndFlush(foreground, "fg");

  background->SetPriority(mojom::Priority::kBackground);
  background.FlushForTesting();
  ForceQueueing(false);

  fg_waiter->WaitForCompletion();
  EXPECT_FALSE(bg_waiter->IsComplete());
  bg_waiter->WaitForCompletion();
}

TEST_F(OnDeviceModelServiceTest, SetPriorityBackToForeground) {
  auto model = LoadModel();

  mojo::Remote<mojom::Session> background;
  model->StartSession(background.BindNewPipeAndPassReceiver(), nullptr);
  background->SetPriority(mojom::Priority::kBackground);

  mojo::Remote<mojom::Session> foreground;
  model->StartSession(foreground.BindNewPipeAndPassReceiver(), nullptr);

  ForceQueueing(true);

  auto bg_waiter = AppendAndFlush(background, "bg");
  auto fg_waiter = AppendAndFlush(foreground, "fg");

  ForceQueueing(false);
  fg_waiter->WaitForCompletion();
  EXPECT_FALSE(bg_waiter->IsComplete());

  ForceQueueing(true);

  fg_waiter = AppendAndFlush(foreground, "fg");

  background->SetPriority(mojom::Priority::kForeground);
  background.FlushForTesting();

  ForceQueueing(false);
  bg_waiter->WaitForCompletion();

  EXPECT_FALSE(fg_waiter->IsComplete());
  fg_waiter->WaitForCompletion();
}

TEST_F(OnDeviceModelServiceTest, SetPriorityMultipleSessions) {
  auto model = LoadModel();

  mojo::Remote<mojom::Session> background1;
  model->StartSession(background1.BindNewPipeAndPassReceiver(), nullptr);
  background1->SetPriority(mojom::Priority::kBackground);

  mojo::Remote<mojom::Session> background2;
  model->StartSession(background2.BindNewPipeAndPassReceiver(), nullptr);
  background2->SetPriority(mojom::Priority::kBackground);

  mojo::Remote<mojom::Session> foreground1;
  model->StartSession(foreground1.BindNewPipeAndPassReceiver(), nullptr);

  mojo::Remote<mojom::Session> foreground2;
  model->StartSession(foreground2.BindNewPipeAndPassReceiver(), nullptr);

  std::set<ContextClientWaiter*> all;
  auto append = [&](mojo::Remote<mojom::Session>& session) {
    std::unique_ptr<ContextClientWaiter> waiter = AppendAndFlush(session, "in");
    all.insert(waiter.get());
    return waiter;
  };
  ForceQueueing(true);
  auto bg1_waiter1 = append(background1);
  auto bg2_waiter1 = append(background2);
  auto fg1_waiter1 = append(foreground1);
  auto fg2_waiter1 = append(foreground2);
  auto fg1_waiter2 = append(foreground1);
  auto bg2_waiter2 = append(background2);
  auto fg2_waiter2 = append(foreground2);
  auto bg1_waiter2 = append(background1);
  ForceQueueing(false);

  auto wait_for_next = [&](ContextClientWaiter* next) {
    next->WaitForCompletion();
    all.erase(next);
    for (auto* waiter : all) {
      EXPECT_FALSE(waiter->IsComplete());
    }
  };
  wait_for_next(fg1_waiter1.get());

  // Add another item, should be added at the end of fg items.
  ForceQueueing(true);
  fg1_waiter1 = append(foreground1);
  ForceQueueing(false);

  wait_for_next(fg2_waiter1.get());
  wait_for_next(fg1_waiter2.get());
  wait_for_next(fg2_waiter2.get());
  wait_for_next(fg1_waiter1.get());
  wait_for_next(bg1_waiter1.get());

  // Add a few fg and bg items, fg should run immediately, bg should run last.
  ForceQueueing(true);
  bg1_waiter1 = append(background1);
  fg1_waiter1 = append(foreground1);
  fg2_waiter1 = append(foreground2);
  ForceQueueing(false);

  wait_for_next(fg1_waiter1.get());
  wait_for_next(fg2_waiter1.get());
  wait_for_next(bg2_waiter1.get());
  wait_for_next(bg2_waiter2.get());
  wait_for_next(bg1_waiter2.get());

  // Add another bg item, but bump priority to fg, should run immediately.
  ForceQueueing(true);
  bg2_waiter1 = append(background2);
  background2->SetPriority(mojom::Priority::kForeground);
  background2.FlushForTesting();
  ForceQueueing(false);

  wait_for_next(bg2_waiter1.get());
  wait_for_next(bg1_waiter1.get());
}

TEST_F(OnDeviceModelServiceTest, SetPriorityCloneInherits) {
  auto model = LoadModel();

  mojo::Remote<mojom::Session> background;
  model->StartSession(background.BindNewPipeAndPassReceiver(), nullptr);
  background->SetPriority(mojom::Priority::kBackground);

  mojo::Remote<mojom::Session> foreground;
  model->StartSession(foreground.BindNewPipeAndPassReceiver(), nullptr);

  mojo::Remote<mojom::Session> clone;
  background->Clone(clone.BindNewPipeAndPassReceiver());
  background.FlushForTesting();

  ForceQueueing(true);
  auto bg_waiter = AppendAndFlush(background, "bg");
  auto clone_waiter = AppendAndFlush(clone, "clone");
  auto fg_waiter = AppendAndFlush(foreground, "fg");
  ForceQueueing(false);

  fg_waiter->WaitForCompletion();
  EXPECT_FALSE(bg_waiter->IsComplete());
  EXPECT_FALSE(clone_waiter->IsComplete());

  bg_waiter->WaitForCompletion();
  EXPECT_FALSE(clone_waiter->IsComplete());

  clone_waiter->WaitForCompletion();
}

#if defined(ENABLE_ON_DEVICE_CONSTRAINTS)
TEST_F(OnDeviceModelServiceTest, JSONSchemaConstraint) {
  auto model = LoadModel();

  TestResponseHolder response;
  mojo::Remote<mojom::Session> session;
  model->StartSession(session.BindNewPipeAndPassReceiver(), nullptr);

  auto options = mojom::GenerateOptions::New();
  options->constraint = mojom::ResponseConstraint::NewJsonSchema(R"({
    "type": "object",
    "required": ["Rating"],
    "additionalProperties": false,
    "properties": {
      "Rating": {
        "type": "number",
        "minimum": 1,
        "maximum": 5
      }
    }
  })");
  session->Generate(std::move(options), response.BindRemote());
  response.WaitForCompletion();

  EXPECT_THAT(response.responses(), ElementsAre(R"({"Rating":1})"));
}

TEST_F(OnDeviceModelServiceTest, JSONSchemaConstraintWithPrefix) {
  auto model = LoadModel();

  TestResponseHolder response;
  mojo::Remote<mojom::Session> session;
  model->StartSession(session.BindNewPipeAndPassReceiver(), nullptr);
  session->Append(MakeInput({"a", ml::Token::kModel, "{\"Rating\""}), {});

  auto options = mojom::GenerateOptions::New();
  options->constraint = mojom::ResponseConstraint::NewJsonSchema(R"({
    "type": "object",
    "required": ["Rating"],
    "additionalProperties": false,
    "properties": {
      "Rating": {
        "type": "number",
        "minimum": 1,
        "maximum": 5
      }
    }
  })");
  session->Generate(std::move(options), response.BindRemote());
  response.WaitForCompletion();

  EXPECT_THAT(response.responses(), ElementsAre("aModel: {\"Rating\"", ":1}"));
}

TEST_F(OnDeviceModelServiceTest, JSONSchemaConstraintWithInvalidPrefix) {
  auto model = LoadModel();

  TestResponseHolder response;
  mojo::Remote<mojom::Session> session;
  model->StartSession(session.BindNewPipeAndPassReceiver(), nullptr);
  session->Append(MakeInput({"a", ml::Token::kModel, "{\"bad\""}), {});

  auto options = mojom::GenerateOptions::New();
  options->constraint = mojom::ResponseConstraint::NewJsonSchema(R"({
    "type": "object",
    "required": ["Rating"],
    "additionalProperties": false,
    "properties": {
      "Rating": {
        "type": "number",
        "minimum": 1,
        "maximum": 5
      }
    }
  })");
  session->Generate(std::move(options), response.BindRemote());
  response.WaitForCompletion();

  // For now invalid prefix will cause a disconnect.
  // TODO:crbug.com/434766400 - Add better error messages.
  EXPECT_THAT(response.responses(), ElementsAre());
  EXPECT_TRUE(response.disconnected());
}

TEST_F(OnDeviceModelServiceTest, JSONSchemaConstraintInvalid) {
  auto model = LoadModel();

  TestResponseHolder response;
  mojo::Remote<mojom::Session> session;
  model->StartSession(session.BindNewPipeAndPassReceiver(), nullptr);
  session->Append(MakeInput("hi"), {});

  auto options = mojom::GenerateOptions::New();
  options->constraint = mojom::ResponseConstraint::NewJsonSchema("blah");
  session->Generate(std::move(options), response.BindRemote());
  response.WaitForCompletion();

  // For now invalid schema will cause a disconnect.
  EXPECT_THAT(response.responses(), ElementsAre());
  EXPECT_TRUE(response.disconnected());
}

TEST_F(OnDeviceModelServiceTest, RegexConstraint) {
  auto model = LoadModel();

  TestResponseHolder response;
  mojo::Remote<mojom::Session> session;
  model->StartSession(session.BindNewPipeAndPassReceiver(), nullptr);

  auto options = mojom::GenerateOptions::New();
  options->constraint = mojom::ResponseConstraint::NewRegex("hello");
  session->Generate(std::move(options), response.BindRemote());
  response.WaitForCompletion();

  EXPECT_THAT(response.responses(), ElementsAre("hello"));
}

TEST_F(OnDeviceModelServiceTest, RegexConstraintIgnoresUserPrefix) {
  auto model = LoadModel();

  TestResponseHolder response;
  mojo::Remote<mojom::Session> session;
  model->StartSession(session.BindNewPipeAndPassReceiver(), nullptr);
  session->Append(MakeInput({ml::Token::kUser, "hel"}), {});

  auto options = mojom::GenerateOptions::New();
  options->constraint = mojom::ResponseConstraint::NewRegex("hello");
  session->Generate(std::move(options), response.BindRemote());
  response.WaitForCompletion();

  EXPECT_THAT(response.responses(), ElementsAre("User: hel", "hello"));
}

TEST_F(OnDeviceModelServiceTest, RegexConstraintWithPrefix) {
  auto model = LoadModel();

  TestResponseHolder response;
  mojo::Remote<mojom::Session> session;
  model->StartSession(session.BindNewPipeAndPassReceiver(), nullptr);
  session->Append(MakeInput({"a", ml::Token::kModel, "hel"}), {});

  auto options = mojom::GenerateOptions::New();
  options->constraint = mojom::ResponseConstraint::NewRegex("hello");
  session->Generate(std::move(options), response.BindRemote());
  response.WaitForCompletion();

  EXPECT_THAT(response.responses(), ElementsAre("aModel: hel", "lo"));
}

TEST_F(OnDeviceModelServiceTest, RegexConstraintWithInvalidPrefix) {
  auto model = LoadModel();

  TestResponseHolder response;
  mojo::Remote<mojom::Session> session;
  model->StartSession(session.BindNewPipeAndPassReceiver(), nullptr);
  session->Append(MakeInput({ml::Token::kModel, "boo"}), {});

  auto options = mojom::GenerateOptions::New();
  options->constraint = mojom::ResponseConstraint::NewRegex("^hello$");
  session->Generate(std::move(options), response.BindRemote());
  response.WaitForCompletion();

  // For now invalid prefix will cause a disconnect.
  // TODO:crbug.com/434766400 - Add better error messages.
  EXPECT_THAT(response.responses(), ElementsAre());
  EXPECT_TRUE(response.disconnected());
}

#endif

}  // namespace
}  // namespace on_device_model
