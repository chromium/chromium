// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/decision_model.h"

#include "base/notreached.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/ai/ai_decision_model.mojom-blink.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-blink-test-utils.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_decision_model_backend.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_decision_model_create_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_decision_model_decide_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_decision_model_decision.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_decision_model_option.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_decision_model_question.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_decision_model_question_type.h"
#include "third_party/blink/renderer/core/dom/abort_controller.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {
namespace {

void ExpectDOMException(V8TestingScope& scope,
                        v8::Local<v8::Value> value,
                        const char* expected_name) {
  auto* dom_exception = V8DOMException::ToWrappable(scope.GetIsolate(), value);
  ASSERT_TRUE(dom_exception);
  EXPECT_EQ(dom_exception->name(), expected_name);
}

}  // namespace

class MockAIDecisionModel : public mojom::blink::AIDecisionModel {
 public:
  explicit MockAIDecisionModel(
      mojo::PendingReceiver<mojom::blink::AIDecisionModel> receiver)
      : receiver_(this, std::move(receiver)) {}

  void Decide(const String& input, DecideCallback callback) override {
    last_input_ = input;
    call_count_++;

    if (should_defer_next_decide_) {
      should_defer_next_decide_ = false;
      deferred_callback_ = std::move(callback);
      deferred_decide_future_.SetValue();
      return;
    }

    if (next_error_) {
      std::move(callback).Run(base::unexpected(std::move(next_error_)));
      return;
    }

    std::move(callback).Run(CreateDefaultResult());
  }

  void set_next_error(mojom::blink::AIDecisionModelDecideErrorPtr error) {
    next_error_ = std::move(error);
  }

  void DeferNextDecide() {
    deferred_decide_future_.Clear();
    should_defer_next_decide_ = true;
  }

  [[nodiscard]] bool WaitForDeferredDecide() {
    return deferred_decide_future_.Wait();
  }

  void FinishDeferredDecide() {
    ASSERT_TRUE(deferred_callback_);
    std::move(deferred_callback_).Run(CreateDefaultResult());
  }

  const String& last_input() const { return last_input_; }
  int call_count() const { return call_count_; }

 private:
  static mojom::blink::AIDecisionModelResultPtr CreateDefaultResult() {
    auto result = mojom::blink::AIDecisionModelResult::New();

    auto decision = mojom::blink::AIDecisionModelDecision::New();
    decision->question_id = "is_urgent";
    decision->top_label = "false";
    decision->confidence = 0.92f;
    decision->probabilities.push_back(
        mojom::blink::AIDecisionModelOptionProbability::New("true", 0.08f));
    decision->probabilities.push_back(
        mojom::blink::AIDecisionModelOptionProbability::New("false", 0.92f));
    result->decisions.push_back(std::move(decision));
    return result;
  }

  String last_input_;
  mojom::blink::AIDecisionModelDecideErrorPtr next_error_;
  DecideCallback deferred_callback_;
  base::test::TestFuture<void> deferred_decide_future_;
  bool should_defer_next_decide_ = false;
  int call_count_ = 0;
  mojo::Receiver<mojom::blink::AIDecisionModel> receiver_;
};

class DecisionModelTest : public testing::Test {
 public:
  DecisionModel* CreateDecisionModel(ScriptState* script_state,
                                     DecisionModelCreateOptions* options =
                                         DecisionModelCreateOptions::Create()) {
    mojo::PendingRemote<mojom::blink::AIDecisionModel> pending_remote;
    mock_remote_ = std::make_unique<MockAIDecisionModel>(
        pending_remote.InitWithNewPipeAndPassReceiver());

    return MakeGarbageCollected<DecisionModel>(
        script_state,
        ExecutionContext::From(script_state)
            ->GetTaskRunner(TaskType::kInternalDefault),
        std::move(pending_remote), options);
  }

 protected:
  test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<MockAIDecisionModel> mock_remote_;
};

TEST_F(DecisionModelTest, DecideSucceeds) {
  V8TestingScope scope;
  DecisionModel* model = CreateDecisionModel(scope.GetScriptState());

  DummyExceptionStateForTesting exception_state;
  DecisionModelDecideOptions* options = DecisionModelDecideOptions::Create();

  auto promise = model->decide(scope.GetScriptState(),
                               "Production database connection pool exhausted",
                               options, exception_state);
  ScriptPromiseTester tester(scope.GetScriptState(), promise);
  tester.WaitUntilSettled();

  EXPECT_TRUE(tester.IsFulfilled());
  EXPECT_EQ(mock_remote_->call_count(), 1);
  EXPECT_EQ(mock_remote_->last_input(),
            "Production database connection pool exhausted");
}

TEST_F(DecisionModelTest, ValidateSchemaRejectsInvalidQuestions) {
  V8TestingScope scope;

  // Empty questions array throws TypeError.
  {
    DummyExceptionStateForTesting exception_state;
    auto* opts = DecisionModelCreateOptions::Create();
    opts->setQuestions({});
    DecisionModel::availability(scope.GetScriptState(), opts, exception_state);
    EXPECT_TRUE(exception_state.HadException());
    EXPECT_EQ(exception_state.CodeAs<ESErrorType>(), ESErrorType::kTypeError);
  }

  // Boolean question with options throws TypeError.
  {
    DummyExceptionStateForTesting exception_state;
    auto* q = DecisionModelQuestion::Create();
    q->setId("bool_q");
    q->setPrompt("Boolean question");
    q->setType(V8DecisionModelQuestionType::Enum::kBoolean);
    auto* opt = DecisionModelOption::Create();
    opt->setLabel("yes");
    q->setOptions({opt});

    auto* opts = DecisionModelCreateOptions::Create();
    opts->setQuestions({q});
    DecisionModel::availability(scope.GetScriptState(), opts, exception_state);
    EXPECT_TRUE(exception_state.HadException());
    EXPECT_EQ(exception_state.CodeAs<ESErrorType>(), ESErrorType::kTypeError);
  }

  // Choice question with < 2 options throws TypeError.
  {
    DummyExceptionStateForTesting exception_state;
    auto* q = DecisionModelQuestion::Create();
    q->setId("cat");
    q->setPrompt("Select category");
    q->setType(V8DecisionModelQuestionType::Enum::kChoice);
    auto* opt = DecisionModelOption::Create();
    opt->setLabel("only_one");
    q->setOptions({opt});

    auto* opts = DecisionModelCreateOptions::Create();
    opts->setQuestions({q});
    DecisionModel::create(scope.GetScriptState(), opts, exception_state);
    EXPECT_TRUE(exception_state.HadException());
    EXPECT_EQ(exception_state.CodeAs<ESErrorType>(), ESErrorType::kTypeError);
  }

  // Empty decide() input throws TypeError.
  {
    DecisionModel* model = CreateDecisionModel(scope.GetScriptState());
    DummyExceptionStateForTesting exception_state;
    model->decide(scope.GetScriptState(), "   ",
                  DecisionModelDecideOptions::Create(), exception_state);
    EXPECT_TRUE(exception_state.HadException());
    EXPECT_EQ(exception_state.CodeAs<ESErrorType>(), ESErrorType::kTypeError);
  }
}

TEST_F(DecisionModelTest, PreferenceAttributeReflectsCreateOptions) {
  V8TestingScope scope;

  DecisionModel* default_model = CreateDecisionModel(scope.GetScriptState());
  EXPECT_EQ(default_model->preference().AsEnum(),
            V8PerformancePreference::Enum::kAuto);

  DecisionModelCreateOptions* speed_options =
      DecisionModelCreateOptions::Create();
  speed_options->setPreference(V8PerformancePreference::Enum::kSpeed);
  DecisionModel* speed_model =
      CreateDecisionModel(scope.GetScriptState(), speed_options);
  EXPECT_EQ(speed_model->preference().AsEnum(),
            V8PerformancePreference::Enum::kSpeed);

  DecisionModelCreateOptions* capability_options =
      DecisionModelCreateOptions::Create();
  capability_options->setPreference(V8PerformancePreference::Enum::kCapability);
  DecisionModel* capability_model =
      CreateDecisionModel(scope.GetScriptState(), capability_options);
  EXPECT_EQ(capability_model->preference().AsEnum(),
            V8PerformancePreference::Enum::kCapability);
}

TEST_F(DecisionModelTest, DecideWithAbortedSignalRejects) {
  V8TestingScope scope;
  DecisionModel* model = CreateDecisionModel(scope.GetScriptState());

  DummyExceptionStateForTesting exception_state;
  AbortController* pre_aborted =
      AbortController::Create(scope.GetScriptState());
  pre_aborted->abort(scope.GetScriptState());

  DecisionModelDecideOptions* pre_options =
      DecisionModelDecideOptions::Create();
  pre_options->setSignal(pre_aborted->signal());

  // An aborted signal takes precedence over empty-input TypeError validation.
  for (const char* input : {"Aborted input", ""}) {
    v8::TryCatch try_catch(scope.GetIsolate());
    auto empty_promise = model->decide(scope.GetScriptState(), input,
                                       pre_options, exception_state);
    EXPECT_TRUE(empty_promise.IsEmpty());
    ASSERT_TRUE(try_catch.HasCaught());
    ExpectDOMException(scope, try_catch.Exception(), "AbortError");
  }
  EXPECT_EQ(mock_remote_->call_count(), 0);

  AbortController* in_flight = AbortController::Create(scope.GetScriptState());
  DecisionModelDecideOptions* in_flight_options =
      DecisionModelDecideOptions::Create();
  in_flight_options->setSignal(in_flight->signal());

  auto promise =
      model->decide(scope.GetScriptState(), "In-flight aborted input",
                    in_flight_options, exception_state);
  [[maybe_unused]] auto* handle = in_flight->signal()->AddAlgorithm(BindOnce(
      &DecisionModel::destroy, WrapPersistent(model),
      WrapPersistent(scope.GetScriptState()), std::ref(exception_state)));
  in_flight->abort(scope.GetScriptState());
  ScriptPromiseTester tester(scope.GetScriptState(), promise);
  tester.WaitUntilSettled();

  ASSERT_TRUE(tester.IsRejected());
  ExpectDOMException(scope, tester.Value().V8Value(), "AbortError");
}

TEST_F(DecisionModelTest, DecideRejectsEmptyInput) {
  V8TestingScope scope;
  DecisionModel* model = CreateDecisionModel(scope.GetScriptState());

  DummyExceptionStateForTesting exception_state;
  DecisionModelDecideOptions* options = DecisionModelDecideOptions::Create();
  model->decide(scope.GetScriptState(), "   ", options, exception_state);

  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(exception_state.CodeAs<ESErrorType>(), ESErrorType::kTypeError);
  EXPECT_EQ(mock_remote_->call_count(), 0);
}

TEST_F(DecisionModelTest, DestroyRejectsSubsequentDecide) {
  V8TestingScope scope;
  DecisionModel* model = CreateDecisionModel(scope.GetScriptState());

  DummyExceptionStateForTesting exception_state;
  DecisionModelDecideOptions* options = DecisionModelDecideOptions::Create();
  auto pending_promise = model->decide(scope.GetScriptState(), "Pending input",
                                       options, exception_state);
  model->destroy(scope.GetScriptState(), exception_state);

  ScriptPromiseTester tester(scope.GetScriptState(), pending_promise);
  tester.WaitUntilSettled();
  ASSERT_TRUE(tester.IsRejected());
  ExpectDOMException(scope, tester.Value().V8Value(), "AbortError");

  for (const char* input : {"Test input after destroy", ""}) {
    v8::TryCatch try_catch(scope.GetIsolate());
    auto promise =
        model->decide(scope.GetScriptState(), input, options, exception_state);
    EXPECT_TRUE(promise.IsEmpty());
    ASSERT_TRUE(try_catch.HasCaught());
    ExpectDOMException(scope, try_catch.Exception(), "AbortError");
  }

  DecisionModel* disconnected = CreateDecisionModel(scope.GetScriptState());
  mock_remote_.reset();
  task_environment_.RunUntilIdle();
  disconnected->decide(scope.GetScriptState(), "Disconnected input", options,
                       exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(exception_state.CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kInvalidStateError);
}

TEST_F(DecisionModelTest, CreateAbortSignalAbortedAfterCreateRejectsDecide) {
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  AbortController* create_controller = AbortController::Create(script_state);
  DecisionModelCreateOptions* create_options =
      DecisionModelCreateOptions::Create();
  create_options->setSignal(create_controller->signal());
  DecisionModel* model = CreateDecisionModel(script_state, create_options);

  DummyExceptionStateForTesting exception_state;
  DecisionModelDecideOptions* options = DecisionModelDecideOptions::Create();
  auto pending_promise =
      model->decide(script_state, "Pending input", options, exception_state);
  create_controller->abort(
      script_state,
      ScriptValue(scope.GetIsolate(),
                  V8String(scope.GetIsolate(), "create abort reason")));

  ScriptPromiseTester tester(script_state, pending_promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsRejected());
  EXPECT_EQ(tester.ValueAsString(), "create abort reason");

  v8::TryCatch try_catch(scope.GetIsolate());
  auto empty_promise = model->decide(script_state, "Input after create abort",
                                     options, exception_state);
  EXPECT_TRUE(empty_promise.IsEmpty());
  ASSERT_TRUE(try_catch.HasCaught());
  EXPECT_EQ(
      ToCoreString(scope.GetIsolate(), try_catch.Exception().As<v8::String>()),
      "create abort reason");
}

TEST_F(DecisionModelTest, DecideRejectsInputTooLargeWithQuotaExceededError) {
  V8TestingScope scope;
  DecisionModel* model = CreateDecisionModel(scope.GetScriptState());
  mock_remote_->set_next_error(mojom::blink::AIDecisionModelDecideError::New(
      mojom::blink::ModelStreamingResponseStatus::kErrorInputTooLarge,
      mojom::blink::QuotaErrorInfo::New(/*requested=*/12000, /*quota=*/10000)));

  DummyExceptionStateForTesting exception_state;
  auto promise =
      model->decide(scope.GetScriptState(), "Very long input",
                    DecisionModelDecideOptions::Create(), exception_state);
  ScriptPromiseTester tester(scope.GetScriptState(), promise);
  tester.WaitUntilSettled();

  ASSERT_TRUE(tester.IsRejected());
  ExpectDOMException(scope, tester.Value().V8Value(), "QuotaExceededError");
}

TEST_F(DecisionModelTest, DecideKeepsModelAliveAcrossGC) {
  V8TestingScope scope;
  WeakPersistent<DecisionModel> weak_model;
  DummyExceptionStateForTesting exception_state;
  {
    v8::HandleScope handle_scope(scope.GetIsolate());
    weak_model = CreateDecisionModel(scope.GetScriptState());
  }
  ScriptPromiseTester tester(
      scope.GetScriptState(),
      weak_model->decide(scope.GetScriptState(), "Unreferenced model call",
                         DecisionModelDecideOptions::Create(),
                         exception_state));

  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_TRUE(weak_model);

  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());
  EXPECT_EQ(mock_remote_->call_count(), 1);

  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_FALSE(weak_model);
}

TEST_F(DecisionModelTest, DecideRecordsSuccessMetrics) {
  V8TestingScope scope;
  DecisionModel* model = CreateDecisionModel(scope.GetScriptState());
  base::HistogramTester histogram_tester;

  // Use a 16-bit string where CharactersSizeInBytes() != length().
  const String kInput = String::FromUtf8("こんにちは");
  ASSERT_FALSE(kInput.Is8Bit());
  EXPECT_NE(kInput.CharactersSizeInBytes(), kInput.length());

  mock_remote_->DeferNextDecide();
  DummyExceptionStateForTesting exception_state;
  auto promise =
      model->decide(scope.GetScriptState(), kInput,
                    DecisionModelDecideOptions::Create(), exception_state);
  ASSERT_TRUE(mock_remote_->WaitForDeferredDecide());
  task_environment_.FastForwardBy(base::Milliseconds(250));
  mock_remote_->FinishDeferredDecide();

  ScriptPromiseTester tester(scope.GetScriptState(), promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());

  histogram_tester.ExpectUniqueSample(
      "AI.Session.DecisionModel.PromptRequestSize", kInput.length(), 1);
  histogram_tester.ExpectUniqueSample(
      "AI.Session.DecisionModel.PromptResponseStatus",
      mojom::blink::ModelStreamingResponseStatus::kComplete, 1);
  histogram_tester.ExpectTotalCount(
      "AI.Session.DecisionModel.ResponseCompleteTime", 1);
  histogram_tester.ExpectTimeBucketCount(
      "AI.Session.DecisionModel.ResponseCompleteTime", base::Milliseconds(250),
      1);
}

TEST_F(DecisionModelTest, DecideRecordsErrorAndDisconnectMetrics) {
  V8TestingScope scope;
  DecisionModel* model = CreateDecisionModel(scope.GetScriptState());

  // Explicit Mojo error records PromptRequestSize and the error status without
  // ResponseCompleteTime.
  {
    base::HistogramTester histogram_tester;
    mock_remote_->set_next_error(mojom::blink::AIDecisionModelDecideError::New(
        mojom::blink::ModelStreamingResponseStatus::kErrorInputTooLarge,
        mojom::blink::QuotaErrorInfo::New(/*requested=*/12000,
                                          /*quota=*/10000)));

    const String kInput = "Oversized request";
    DummyExceptionStateForTesting exception_state;
    auto promise =
        model->decide(scope.GetScriptState(), kInput,
                      DecisionModelDecideOptions::Create(), exception_state);
    ScriptPromiseTester tester(scope.GetScriptState(), promise);
    tester.WaitUntilSettled();
    ASSERT_TRUE(tester.IsRejected());
    ExpectDOMException(scope, tester.Value().V8Value(), "QuotaExceededError");

    histogram_tester.ExpectUniqueSample(
        "AI.Session.DecisionModel.PromptRequestSize", kInput.length(), 1);
    histogram_tester.ExpectUniqueSample(
        "AI.Session.DecisionModel.PromptResponseStatus",
        mojom::blink::ModelStreamingResponseStatus::kErrorInputTooLarge, 1);
    histogram_tester.ExpectTotalCount(
        "AI.Session.DecisionModel.ResponseCompleteTime", 0);
  }

  // Remote disconnect while a request is in flight records
  // kErrorSessionDestroyed without ResponseCompleteTime.
  {
    base::HistogramTester histogram_tester;
    mock_remote_->DeferNextDecide();

    const String kInput = "In-flight disconnect";
    DummyExceptionStateForTesting exception_state;
    auto promise =
        model->decide(scope.GetScriptState(), kInput,
                      DecisionModelDecideOptions::Create(), exception_state);
    ASSERT_TRUE(mock_remote_->WaitForDeferredDecide());
    ASSERT_EQ(mock_remote_->call_count(), 2);

    mock_remote_.reset();
    ScriptPromiseTester tester(scope.GetScriptState(), promise);
    tester.WaitUntilSettled();
    ASSERT_TRUE(tester.IsRejected());
    ExpectDOMException(scope, tester.Value().V8Value(), "InvalidStateError");

    histogram_tester.ExpectUniqueSample(
        "AI.Session.DecisionModel.PromptRequestSize", kInput.length(), 1);
    histogram_tester.ExpectUniqueSample(
        "AI.Session.DecisionModel.PromptResponseStatus",
        mojom::blink::ModelStreamingResponseStatus::kErrorSessionDestroyed, 1);
    histogram_tester.ExpectTotalCount(
        "AI.Session.DecisionModel.ResponseCompleteTime", 0);
  }

  // Subsequent decide() on an already-disconnected session throws synchronously
  // without recording session metrics.
  {
    base::HistogramTester histogram_tester;
    DummyExceptionStateForTesting exception_state;
    model->decide(scope.GetScriptState(), "After disconnect",
                  DecisionModelDecideOptions::Create(), exception_state);
    EXPECT_TRUE(exception_state.HadException());
    EXPECT_EQ(exception_state.CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kInvalidStateError);

    histogram_tester.ExpectTotalCount(
        "AI.Session.DecisionModel.PromptRequestSize", 0);
    histogram_tester.ExpectTotalCount(
        "AI.Session.DecisionModel.PromptResponseStatus", 0);
    histogram_tester.ExpectTotalCount(
        "AI.Session.DecisionModel.ResponseCompleteTime", 0);
  }
}

TEST_F(DecisionModelTest, DecideSkipsMetricsOnValidationAndAbort) {
  V8TestingScope scope;

  // Pre-flight validation, pre-aborted signal, and destroyed session emit no
  // session metrics.
  {
    DecisionModel* model = CreateDecisionModel(scope.GetScriptState());
    base::HistogramTester histogram_tester;

    DummyExceptionStateForTesting empty_exception_state;
    model->decide(scope.GetScriptState(), "   ",
                  DecisionModelDecideOptions::Create(), empty_exception_state);
    EXPECT_TRUE(empty_exception_state.HadException());

    AbortController* pre_aborted =
        AbortController::Create(scope.GetScriptState());
    pre_aborted->abort(scope.GetScriptState());
    DecisionModelDecideOptions* pre_options =
        DecisionModelDecideOptions::Create();
    pre_options->setSignal(pre_aborted->signal());
    {
      v8::TryCatch try_catch(scope.GetIsolate());
      DummyExceptionStateForTesting abort_exception_state;
      model->decide(scope.GetScriptState(), "Aborted before send", pre_options,
                    abort_exception_state);
      EXPECT_TRUE(try_catch.HasCaught());
    }

    DummyExceptionStateForTesting destroy_exception_state;
    model->destroy(scope.GetScriptState(), destroy_exception_state);
    {
      v8::TryCatch try_catch(scope.GetIsolate());
      DummyExceptionStateForTesting destroyed_exception_state;
      model->decide(scope.GetScriptState(), "After destroy",
                    DecisionModelDecideOptions::Create(),
                    destroyed_exception_state);
      EXPECT_TRUE(try_catch.HasCaught());
    }

    histogram_tester.ExpectTotalCount(
        "AI.Session.DecisionModel.PromptRequestSize", 0);
    histogram_tester.ExpectTotalCount(
        "AI.Session.DecisionModel.PromptResponseStatus", 0);
    histogram_tester.ExpectTotalCount(
        "AI.Session.DecisionModel.ResponseCompleteTime", 0);
  }

  // In-flight signal abort and session destroy record PromptRequestSize once
  // per dispatched call, but skip PromptResponseStatus and
  // ResponseCompleteTime.
  {
    DecisionModel* model = CreateDecisionModel(scope.GetScriptState());
    DummyExceptionStateForTesting exception_state;

    {
      base::HistogramTester histogram_tester;
      AbortController* in_flight =
          AbortController::Create(scope.GetScriptState());
      DecisionModelDecideOptions* in_flight_options =
          DecisionModelDecideOptions::Create();
      in_flight_options->setSignal(in_flight->signal());

      mock_remote_->DeferNextDecide();
      const String kFirstInput = "Aborted while pending";
      auto first_promise = model->decide(scope.GetScriptState(), kFirstInput,
                                         in_flight_options, exception_state);
      ASSERT_TRUE(mock_remote_->WaitForDeferredDecide());
      in_flight->abort(scope.GetScriptState());
      mock_remote_->FinishDeferredDecide();
      ScriptPromiseTester first_tester(scope.GetScriptState(), first_promise);
      first_tester.WaitUntilSettled();
      EXPECT_TRUE(first_tester.IsRejected());

      histogram_tester.ExpectUniqueSample(
          "AI.Session.DecisionModel.PromptRequestSize", kFirstInput.length(),
          1);
      histogram_tester.ExpectTotalCount(
          "AI.Session.DecisionModel.PromptResponseStatus", 0);
      histogram_tester.ExpectTotalCount(
          "AI.Session.DecisionModel.ResponseCompleteTime", 0);
    }

    {
      base::HistogramTester histogram_tester;
      mock_remote_->DeferNextDecide();
      const String kSecondInput = "Destroyed while pending";
      auto second_promise =
          model->decide(scope.GetScriptState(), kSecondInput,
                        DecisionModelDecideOptions::Create(), exception_state);
      ASSERT_TRUE(mock_remote_->WaitForDeferredDecide());
      model->destroy(scope.GetScriptState(), exception_state);
      ScriptPromiseTester second_tester(scope.GetScriptState(), second_promise);
      second_tester.WaitUntilSettled();
      EXPECT_TRUE(second_tester.IsRejected());

      histogram_tester.ExpectUniqueSample(
          "AI.Session.DecisionModel.PromptRequestSize", kSecondInput.length(),
          1);
      histogram_tester.ExpectTotalCount(
          "AI.Session.DecisionModel.PromptResponseStatus", 0);
      histogram_tester.ExpectTotalCount(
          "AI.Session.DecisionModel.ResponseCompleteTime", 0);
    }
  }

  // ExecutionContext teardown while a request is in flight records
  // PromptRequestSize, without emitting a spurious kErrorSessionDestroyed
  // response status when HeapMojoRemote resets on context destruction.
  {
    base::HistogramTester histogram_tester;
    const String kThirdInput = "Context destroyed while pending";
    {
      V8TestingScope inner_scope;
      DecisionModel* model = CreateDecisionModel(inner_scope.GetScriptState());
      mock_remote_->DeferNextDecide();
      DummyExceptionStateForTesting exception_state;
      [[maybe_unused]] auto pending_promise =
          model->decide(inner_scope.GetScriptState(), kThirdInput,
                        DecisionModelDecideOptions::Create(), exception_state);
      ASSERT_TRUE(mock_remote_->WaitForDeferredDecide());
    }

    histogram_tester.ExpectUniqueSample(
        "AI.Session.DecisionModel.PromptRequestSize", kThirdInput.length(), 1);
    histogram_tester.ExpectTotalCount(
        "AI.Session.DecisionModel.PromptResponseStatus", 0);
    histogram_tester.ExpectTotalCount(
        "AI.Session.DecisionModel.ResponseCompleteTime", 0);
  }
}

namespace {

class MockAIManager : public mojom::blink::AIManagerInterceptorForTesting {
 public:
  MockAIManager() = default;
  ~MockAIManager() override = default;

  mojom::blink::AIManager* GetForwardingInterface() override { NOTREACHED(); }

  void Bind(mojo::ScopedMessagePipeHandle handle) {
    receiver_.reset();
    receiver_.Bind(
        mojo::PendingReceiver<mojom::blink::AIManager>(std::move(handle)));
  }

  void CanCreateDecisionModel(
      mojom::blink::AIDecisionModelCreateOptionsPtr options,
      CanCreateDecisionModelCallback callback) override {
    last_can_create_options_ = std::move(options);
    std::move(callback).Run(
        mojom::blink::ModelAvailabilityCheckResult::kAvailable);
  }

  void CreateDecisionModel(
      mojo::PendingRemote<mojom::blink::AIManagerCreateDecisionModelClient>
          client,
      mojom::blink::AIDecisionModelCreateOptionsPtr options,
      mojo::PendingRemote<on_device_model::mojom::blink::DownloadObserver>
          monitor) override {
    last_create_options_ = std::move(options);
    mojo::PendingRemote<mojom::blink::AIDecisionModel> pending_remote;
    mock_decision_model_ = std::make_unique<MockAIDecisionModel>(
        pending_remote.InitWithNewPipeAndPassReceiver());
    mojo::Remote<mojom::blink::AIManagerCreateDecisionModelClient>(
        std::move(client))
        ->OnSessionCreated(std::move(pending_remote));
  }

  mojom::blink::AIDecisionModelCreateOptionsPtr last_can_create_options_;
  mojom::blink::AIDecisionModelCreateOptionsPtr last_create_options_;
  std::unique_ptr<MockAIDecisionModel> mock_decision_model_;
  mojo::Receiver<mojom::blink::AIManager> receiver_{this};
};

}  // namespace

TEST_F(DecisionModelTest, BackendOptionPassedToMojo) {
  V8TestingScope scope;
  MockAIManager mock_ai_manager;
  scope.GetExecutionContext()->GetBrowserInterfaceBroker().SetBinderForTesting(
      mojom::blink::AIManager::Name_,
      blink::BindRepeating(&MockAIManager::Bind,
                           blink::Unretained(&mock_ai_manager)));

  auto* q = DecisionModelQuestion::Create();
  q->setId("urgent");
  q->setPrompt("Is it urgent?");
  q->setType(V8DecisionModelQuestionType::Enum::kBoolean);

  for (const auto& [backend, expected] :
       std::initializer_list<std::pair<V8DecisionModelBackend::Enum,
                                       mojom::blink::AIDecisionModelBackend>>{
           {V8DecisionModelBackend::Enum::kEmbeddingGemmaV1,
            mojom::blink::AIDecisionModelBackend::kEmbeddingGemmaV1},
           {V8DecisionModelBackend::Enum::kLanguageModel,
            mojom::blink::AIDecisionModelBackend::kLanguageModel},
           {V8DecisionModelBackend::Enum::kLayaEnWint8,
            mojom::blink::AIDecisionModelBackend::kLayaEnWint8},
           {V8DecisionModelBackend::Enum::kLayaMlWint8,
            mojom::blink::AIDecisionModelBackend::kLayaMlWint8},
       }) {
    DummyExceptionStateForTesting exception_state;
    auto* options = DecisionModelCreateOptions::Create();
    options->setQuestions({q});
    options->setDevTrialBackend(backend);
    auto promise =
        DecisionModel::create(scope.GetScriptState(), options, exception_state);
    ScriptPromiseTester tester(scope.GetScriptState(), promise);
    tester.WaitUntilSettled();
    EXPECT_TRUE(tester.IsFulfilled());
    ASSERT_TRUE(mock_ai_manager.last_create_options_);
    ASSERT_TRUE(mock_ai_manager.last_create_options_->backend.has_value());
    EXPECT_EQ(mock_ai_manager.last_create_options_->backend.value(), expected);
  }

  scope.GetExecutionContext()->GetBrowserInterfaceBroker().SetBinderForTesting(
      mojom::blink::AIManager::Name_, {});
}

}  // namespace blink
