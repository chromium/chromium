// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/decision_model.h"

#include "base/test/run_until.h"
#include "base/types/expected.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/ai/ai_decision_model.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
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
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

class MockAIDecisionModel : public mojom::blink::AIDecisionModel {
 public:
  explicit MockAIDecisionModel(
      mojo::PendingReceiver<mojom::blink::AIDecisionModel> receiver)
      : receiver_(this, std::move(receiver)) {}

  void Decide(const String& input, DecideCallback callback) override {
    last_input_ = input;
    call_count_++;

    if (next_error_) {
      std::move(callback).Run(base::unexpected(std::move(next_error_)));
      return;
    }

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

    std::move(callback).Run(std::move(result));
  }

  void set_next_error(mojom::blink::AIDecisionModelDecideErrorPtr error) {
    next_error_ = std::move(error);
  }

  const String& last_input() const { return last_input_; }
  int call_count() const { return call_count_; }

 private:
  String last_input_;
  mojom::blink::AIDecisionModelDecideErrorPtr next_error_;
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
  test::TaskEnvironment task_environment_;
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

  {
    v8::TryCatch try_catch(scope.GetIsolate());
    auto empty_promise = model->decide(scope.GetScriptState(), "Aborted input",
                                       pre_options, exception_state);
    EXPECT_TRUE(empty_promise.IsEmpty());
    EXPECT_TRUE(try_catch.HasCaught());
  }
  EXPECT_EQ(mock_remote_->call_count(), 0);

  AbortController* in_flight = AbortController::Create(scope.GetScriptState());
  DecisionModelDecideOptions* in_flight_options =
      DecisionModelDecideOptions::Create();
  in_flight_options->setSignal(in_flight->signal());

  auto promise =
      model->decide(scope.GetScriptState(), "In-flight aborted input",
                    in_flight_options, exception_state);
  in_flight->abort(scope.GetScriptState());
  ScriptPromiseTester tester(scope.GetScriptState(), promise);
  tester.WaitUntilSettled();

  EXPECT_TRUE(tester.IsRejected());
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
  model->destroy(scope.GetScriptState(), exception_state);

  DecisionModelDecideOptions* options = DecisionModelDecideOptions::Create();
  auto promise =
      model->decide(scope.GetScriptState(), "Test input after destroy", options,
                    exception_state);
  ScriptPromiseTester tester(scope.GetScriptState(), promise);
  tester.WaitUntilSettled();

  EXPECT_TRUE(tester.IsRejected());
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
  auto* dom_exception =
      V8DOMException::ToWrappable(scope.GetIsolate(), tester.Value().V8Value());
  ASSERT_TRUE(dom_exception);
  EXPECT_EQ(dom_exception->name(), "QuotaExceededError");
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

}  // namespace blink
