// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/semantic_embedder.h"

#include <cstdint>
#include <optional>

#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/test/run_until.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-blink-test-utils.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-blink.h"
#include "third_party/blink/public/mojom/ai/ai_semantic_embedder.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_quota_exceeded_error.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_content_embedding.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_semantic_embedder_create_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_semantic_embedder_embed_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_semantic_embedder_input.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_semantic_embedder_input_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_semantic_embedder_params.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_semantic_embedder_result.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_semantic_embedder_space_info.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_semantic_embedder_task_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_typedefs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_semanticembedderinput_semanticembedderinputorstringsequence_string.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_semanticembedderinput_string.h"
#include "third_party/blink/renderer/core/dom/abort_controller.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/quota_exceeded_error.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

class MockAISemanticEmbedder : public mojom::blink::AISemanticEmbedder {
 public:
  explicit MockAISemanticEmbedder(
      mojo::PendingReceiver<mojom::blink::AISemanticEmbedder> receiver)
      : receiver_(this, std::move(receiver)) {}

  void Embed(uint64_t request_id,
             const Vector<String>& inputs,
             mojom::blink::AISemanticEmbedderEmbedOptionsPtr options,
             EmbedCallback callback) override {
    last_request_id_ = request_id;
    last_inputs_ = inputs;
    last_options_ = std::move(options);
    call_count_++;

    auto result = mojom::blink::SemanticEmbedderResult::New();

    auto embedding1 = mojom::blink::ContentEmbedding::New();
    embedding1->values = {1.0f, 2.0f, 3.0f};
    result->embeddings.push_back(std::move(embedding1));

    auto embedding2 = mojom::blink::ContentEmbedding::New();
    embedding2->values = {4.0f, 5.0f, 6.0f};
    result->embeddings.push_back(std::move(embedding2));

    if (hold_callback_) {
      held_callbacks_.push_back(std::move(callback));
      return;
    }

    std::move(callback).Run(std::move(result));
  }

  void CancelEmbed(uint64_t request_id) override {
    cancelled_request_ids_.push_back(request_id);
  }

  void set_hold_callback(bool hold) { hold_callback_ = hold; }
  void RunHeldCallback() {
    for (auto& callback : held_callbacks_) {
      std::move(callback).Run(mojom::blink::SemanticEmbedderResult::New());
    }
    held_callbacks_.clear();
  }
  // Waits until the SemanticEmbedder has handled every reply sent so far, and
  // this mock has received every call the SemanticEmbedder made before then.
  void FlushForTesting() { receiver_.FlushForTesting(); }
  uint64_t last_request_id() const { return last_request_id_; }
  const Vector<uint64_t>& cancelled_request_ids() const {
    return cancelled_request_ids_;
  }

  const Vector<String>& last_inputs() const { return last_inputs_; }
  const mojom::blink::AISemanticEmbedderEmbedOptions* last_options() const {
    return last_options_.get();
  }
  int call_count() const { return call_count_; }

 private:
  uint64_t last_request_id_ = 0;
  Vector<uint64_t> cancelled_request_ids_;
  Vector<String> last_inputs_;
  mojom::blink::AISemanticEmbedderEmbedOptionsPtr last_options_;
  int call_count_ = 0;
  bool hold_callback_ = false;
  Vector<EmbedCallback> held_callbacks_;
  mojo::Receiver<mojom::blink::AISemanticEmbedder> receiver_;
};

namespace {

// Serves AIManager.GetSupportedEmbeddingSpaces() to the static params() entry
// point. Bound through the execution context's browser interface broker.
// File-local: other test files in this target define their own AIManager
// mocks in namespace blink.
class MockParamsAIManager
    : public mojom::blink::AIManagerInterceptorForTesting {
 public:
  MockParamsAIManager() = default;
  ~MockParamsAIManager() override = default;

  mojom::blink::AIManager* GetForwardingInterface() override { NOTREACHED(); }

  void Bind(mojo::ScopedMessagePipeHandle handle) {
    receiver_.reset();
    receiver_.Bind(
        mojo::PendingReceiver<mojom::blink::AIManager>(std::move(handle)));
  }

  void GetSupportedEmbeddingSpaces(
      GetSupportedEmbeddingSpacesCallback callback) override {
    params_call_count_++;
    if (hold_params_callback_) {
      held_params_callback_ = std::move(callback);
      return;
    }
    Vector<mojom::blink::AISemanticEmbedderSpaceInfoPtr> spaces;
    for (const auto& space : supported_spaces_) {
      spaces.push_back(space->Clone());
    }
    std::move(callback).Run(std::move(spaces));
  }

  void AddSupportedSpace(const String& embedding_space,
                         std::optional<base::Time> shutdown_date) {
    supported_spaces_.push_back(mojom::blink::AISemanticEmbedderSpaceInfo::New(
        embedding_space, shutdown_date));
  }
  void set_hold_params_callback(bool hold) { hold_params_callback_ = hold; }
  // Simulates the browser going away with a params() reply outstanding.
  void Disconnect() {
    receiver_.reset();
    held_params_callback_.Reset();
  }
  int params_call_count() const { return params_call_count_; }

 private:
  Vector<mojom::blink::AISemanticEmbedderSpaceInfoPtr> supported_spaces_;
  bool hold_params_callback_ = false;
  GetSupportedEmbeddingSpacesCallback held_params_callback_;
  int params_call_count_ = 0;
  mojo::Receiver<mojom::blink::AIManager> receiver_{this};
};

}  // namespace

class EmbedderTest : public testing::Test {
 public:
  static constexpr char kEmbeddingSpace[] = "embeddinggemma-300m";

  SemanticEmbedder* CreateSemanticEmbedder(ScriptState* script_state) {
    mojo::PendingRemote<mojom::blink::AISemanticEmbedder> pending_remote;
    mock_remote_ = std::make_unique<MockAISemanticEmbedder>(
        pending_remote.InitWithNewPipeAndPassReceiver());

    auto* options = SemanticEmbedderCreateOptions::Create();
    options->setEmbeddingSpace(kEmbeddingSpace);
    return MakeGarbageCollected<SemanticEmbedder>(
        script_state,
        ExecutionContext::From(script_state)
            ->GetTaskRunner(TaskType::kInternalDefault),
        std::move(pending_remote), options);
  }

  // params() talks to AIManager directly; the params tests bind a mock
  // manager into the execution context's browser interface broker. The
  // returned runner removes the binder again, also when an ASSERT ends the
  // test early, so later tests never reach the destroyed mock.
  [[nodiscard]] base::ScopedClosureRunner BindMockAIManager(
      V8TestingScope& scope) {
    const BrowserInterfaceBrokerProxy& broker =
        scope.GetExecutionContext()->GetBrowserInterfaceBroker();
    broker.SetBinderForTesting(mojom::blink::AIManager::Name_,
                               BindRepeating(&MockParamsAIManager::Bind,
                                             Unretained(&mock_ai_manager_)));
    return base::ScopedClosureRunner(BindOnce(
        [](const BrowserInterfaceBrokerProxy* broker) {
          broker->SetBinderForTesting(mojom::blink::AIManager::Name_, {});
        },
        Unretained(&broker)));
  }

  SemanticEmbedderParams* ParamsFrom(V8TestingScope& scope,
                                     const ScriptPromiseTester& tester) {
    DummyExceptionStateForTesting exception_state;
    auto* params = NativeValueTraits<SemanticEmbedderParams>::NativeValue(
        scope.GetIsolate(), tester.Value().V8Value(), exception_state);
    EXPECT_FALSE(exception_state.HadException());
    return params;
  }

  // embed() input for one bare string.
  static V8SemanticEmbedderInputs* MakeInput(const String& text) {
    return MakeGarbageCollected<V8SemanticEmbedderInputs>(text);
  }

  // embed() input for a sequence of bare strings.
  static V8SemanticEmbedderInputs* MakeInput(const Vector<String>& texts) {
    HeapVector<Member<V8SemanticEmbedderInputItem>> items;
    for (const String& text : texts) {
      items.push_back(MakeGarbageCollected<V8SemanticEmbedderInputItem>(text));
    }
    return MakeGarbageCollected<V8SemanticEmbedderInputs>(std::move(items));
  }

  // A {type: "text", value} item.
  static SemanticEmbedderInput* MakeTextItem(const String& text) {
    auto* item = SemanticEmbedderInput::Create();
    item->setType(
        V8SemanticEmbedderInputType(V8SemanticEmbedderInputType::Enum::kText));
    item->setValue(text);
    return item;
  }

 protected:
  test::TaskEnvironment task_environment_;
  std::unique_ptr<MockAISemanticEmbedder> mock_remote_;
  MockParamsAIManager mock_ai_manager_;
};

TEST_F(EmbedderTest, EmbedSucceeds) {
  V8TestingScope scope;
  SemanticEmbedder* embedder = CreateSemanticEmbedder(scope.GetScriptState());

  DummyExceptionStateForTesting exception_state;
  SemanticEmbedderEmbedOptions* options =
      SemanticEmbedderEmbedOptions::Create();
  options->setTaskType(V8SemanticEmbedderTaskType(
      V8SemanticEmbedderTaskType::Enum::kClassification));
  auto* input = MakeInput("hello");
  auto promise =
      embedder->embed(scope.GetScriptState(), input, options, exception_state);
  EXPECT_FALSE(exception_state.HadException());

  ScriptPromiseTester tester(scope.GetScriptState(), promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());

  EXPECT_EQ(mock_remote_->call_count(), 1);
  EXPECT_EQ(mock_remote_->last_inputs().size(), 1u);
  EXPECT_EQ(mock_remote_->last_inputs()[0], "hello");
  EXPECT_TRUE(mock_remote_->last_options());
  EXPECT_EQ(mock_remote_->last_options()->task_type,
            mojom::blink::AISemanticEmbedderTaskType::kClassification);
}

TEST_F(EmbedderTest, EmbedBatchSucceeds) {
  V8TestingScope scope;
  SemanticEmbedder* embedder = CreateSemanticEmbedder(scope.GetScriptState());

  Vector<String> inputs = {"hello", "world"};
  DummyExceptionStateForTesting exception_state;
  auto* input = MakeInput(inputs);
  auto promise =
      embedder->embed(scope.GetScriptState(), input,
                      SemanticEmbedderEmbedOptions::Create(), exception_state);
  EXPECT_FALSE(exception_state.HadException());

  ScriptPromiseTester tester(scope.GetScriptState(), promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());

  EXPECT_EQ(mock_remote_->call_count(), 1);
  EXPECT_EQ(mock_remote_->last_inputs().size(), 2u);
  EXPECT_EQ(mock_remote_->last_inputs()[0], "hello");
  EXPECT_EQ(mock_remote_->last_inputs()[1], "world");
}

TEST_F(EmbedderTest, EmbedAcceptsTypedTextItems) {
  V8TestingScope scope;
  SemanticEmbedder* embedder = CreateSemanticEmbedder(scope.GetScriptState());

  // One {type: "text", value} item.
  {
    DummyExceptionStateForTesting exception_state;
    auto promise = embedder->embed(
        scope.GetScriptState(),
        MakeGarbageCollected<V8SemanticEmbedderInputs>(MakeTextItem("hello")),
        SemanticEmbedderEmbedOptions::Create(), exception_state);
    EXPECT_FALSE(exception_state.HadException());
    ScriptPromiseTester tester(scope.GetScriptState(), promise);
    tester.WaitUntilSettled();
    EXPECT_TRUE(tester.IsFulfilled());
    EXPECT_EQ(mock_remote_->last_inputs(), Vector<String>{"hello"});
  }

  // A mixed sequence: a bare string and a typed item reach the browser as
  // their text, in order.
  {
    HeapVector<Member<V8SemanticEmbedderInputItem>> items;
    items.push_back(
        MakeGarbageCollected<V8SemanticEmbedderInputItem>(String("hello")));
    items.push_back(MakeGarbageCollected<V8SemanticEmbedderInputItem>(
        MakeTextItem("world")));
    DummyExceptionStateForTesting exception_state;
    auto promise = embedder->embed(
        scope.GetScriptState(),
        MakeGarbageCollected<V8SemanticEmbedderInputs>(std::move(items)),
        SemanticEmbedderEmbedOptions::Create(), exception_state);
    EXPECT_FALSE(exception_state.HadException());
    ScriptPromiseTester tester(scope.GetScriptState(), promise);
    tester.WaitUntilSettled();
    EXPECT_TRUE(tester.IsFulfilled());
    EXPECT_EQ(mock_remote_->last_inputs(), (Vector<String>{"hello", "world"}));
  }
  EXPECT_EQ(mock_remote_->call_count(), 2);
}

TEST_F(EmbedderTest, ResultCarriesEmbeddingSpace) {
  V8TestingScope scope;
  SemanticEmbedder* embedder = CreateSemanticEmbedder(scope.GetScriptState());
  EXPECT_EQ(embedder->embeddingSpace(), kEmbeddingSpace);

  DummyExceptionStateForTesting exception_state;
  auto* input = MakeInput(Vector<String>{"hello", "world"});
  auto promise =
      embedder->embed(scope.GetScriptState(), input,
                      SemanticEmbedderEmbedOptions::Create(), exception_state);
  EXPECT_FALSE(exception_state.HadException());

  ScriptPromiseTester tester(scope.GetScriptState(), promise);
  tester.WaitUntilSettled();
  ASSERT_TRUE(tester.IsFulfilled());

  SemanticEmbedderResult* result =
      NativeValueTraits<SemanticEmbedderResult>::NativeValue(
          scope.GetIsolate(), tester.Value().V8Value(), exception_state);
  ASSERT_FALSE(exception_state.HadException());
  ASSERT_EQ(result->embeddings().size(), 2u);
  for (const auto& embedding : result->embeddings()) {
    EXPECT_EQ(embedding->embeddingSpace(), kEmbeddingSpace);
  }
}

TEST_F(EmbedderTest, DisconnectRejectsPromise) {
  V8TestingScope scope;
  SemanticEmbedder* embedder = CreateSemanticEmbedder(scope.GetScriptState());

  mock_remote_->set_hold_callback(true);

  DummyExceptionStateForTesting exception_state;
  auto* input = MakeInput("hello");
  auto promise =
      embedder->embed(scope.GetScriptState(), input,
                      SemanticEmbedderEmbedOptions::Create(), exception_state);
  EXPECT_FALSE(exception_state.HadException());

  // Simulate utility process crash / pipe disconnect.
  mock_remote_.reset();

  ScriptPromiseTester tester(scope.GetScriptState(), promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsRejected());

  auto* dom_exception =
      V8DOMException::ToWrappable(scope.GetIsolate(), tester.Value().V8Value());
  ASSERT_TRUE(dom_exception);
  EXPECT_EQ(DOMException(DOMExceptionCode::kInvalidStateError).name(),
            dom_exception->name());
}

TEST_F(EmbedderTest, AbortRejectsAndCancels) {
  V8TestingScope scope;
  SemanticEmbedder* embedder = CreateSemanticEmbedder(scope.GetScriptState());
  mock_remote_->set_hold_callback(true);

  auto* controller = AbortController::Create(scope.GetScriptState());
  auto* options = SemanticEmbedderEmbedOptions::Create();
  options->setSignal(controller->signal());
  DummyExceptionStateForTesting exception_state;
  auto* input = MakeInput("hello");
  auto promise =
      embedder->embed(scope.GetScriptState(), input, options, exception_state);
  EXPECT_FALSE(exception_state.HadException());
  ASSERT_TRUE(
      base::test::RunUntil([&] { return mock_remote_->call_count() == 1; }));

  controller->abort(scope.GetScriptState());

  // The browser was asked to drop this request, by id.
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return !mock_remote_->cancelled_request_ids().empty(); }));
  EXPECT_EQ(mock_remote_->cancelled_request_ids(),
            Vector<uint64_t>{mock_remote_->last_request_id()});

  ScriptPromiseTester tester(scope.GetScriptState(), promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsRejected());
  auto* dom_exception =
      V8DOMException::ToWrappable(scope.GetIsolate(), tester.Value().V8Value());
  ASSERT_TRUE(dom_exception);
  EXPECT_EQ(DOMException(DOMExceptionCode::kAbortError).name(),
            dom_exception->name());

  // A late reply is ignored.
  mock_remote_->RunHeldCallback();
  mock_remote_->FlushForTesting();
  EXPECT_TRUE(tester.IsRejected());
}

TEST_F(EmbedderTest, AlreadyAbortedSignalRejectsWithoutRequest) {
  V8TestingScope scope;
  SemanticEmbedder* embedder = CreateSemanticEmbedder(scope.GetScriptState());

  auto* controller = AbortController::Create(scope.GetScriptState());
  controller->abort(scope.GetScriptState());
  auto* options = SemanticEmbedderEmbedOptions::Create();
  options->setSignal(controller->signal());
  DummyExceptionStateForTesting exception_state;
  auto* input = MakeInput("hello");
  // The abort reason is rethrown on the isolate, not through exception_state.
  v8::TryCatch try_catch(scope.GetIsolate());
  embedder->embed(scope.GetScriptState(), input, options, exception_state);
  EXPECT_TRUE(try_catch.HasCaught() || exception_state.HadException());
  mock_remote_->FlushForTesting();
  EXPECT_EQ(mock_remote_->call_count(), 0);
}

TEST_F(EmbedderTest, TooManyInFlightRequestsRejectWithQuotaError) {
  V8TestingScope scope;
  SemanticEmbedder* embedder = CreateSemanticEmbedder(scope.GetScriptState());
  mock_remote_->set_hold_callback(true);

  auto* input = MakeInput("hello");
  ScriptPromise<SemanticEmbedderResult> last_held;
  for (int i = 0; i < SemanticEmbedder::kMaxInFlightEmbedRequests; ++i) {
    DummyExceptionStateForTesting exception_state;
    last_held = embedder->embed(scope.GetScriptState(), input,
                                SemanticEmbedderEmbedOptions::Create(),
                                exception_state);
    EXPECT_FALSE(exception_state.HadException());
  }

  DummyExceptionStateForTesting exception_state;
  auto promise =
      embedder->embed(scope.GetScriptState(), input,
                      SemanticEmbedderEmbedOptions::Create(), exception_state);
  ScriptPromiseTester tester(scope.GetScriptState(), promise);
  tester.WaitUntilSettled();
  ASSERT_TRUE(tester.IsRejected());
  auto* dom_exception =
      V8DOMException::ToWrappable(scope.GetIsolate(), tester.Value().V8Value());
  ASSERT_TRUE(dom_exception);
  EXPECT_EQ(DOMException(DOMExceptionCode::kQuotaExceededError).name(),
            dom_exception->name());
  EXPECT_TRUE(V8QuotaExceededError::ToWrappable(scope.GetIsolate(),
                                                tester.Value().V8Value()));
  // The rejected call never reached the browser.
  ASSERT_TRUE(base::test::RunUntil([&] {
    return mock_remote_->call_count() ==
           SemanticEmbedder::kMaxInFlightEmbedRequests;
  }));

  // Once the held calls settle there is room again. Replies arrive in order,
  // so once the last one has settled, all of them have.
  mock_remote_->set_hold_callback(false);
  mock_remote_->RunHeldCallback();
  ScriptPromiseTester last_held_tester(scope.GetScriptState(), last_held);
  last_held_tester.WaitUntilSettled();
  EXPECT_TRUE(last_held_tester.IsFulfilled());
  DummyExceptionStateForTesting exception_state2;
  auto promise2 =
      embedder->embed(scope.GetScriptState(), input,
                      SemanticEmbedderEmbedOptions::Create(), exception_state2);
  ScriptPromiseTester tester2(scope.GetScriptState(), promise2);
  tester2.WaitUntilSettled();
  EXPECT_TRUE(tester2.IsFulfilled());
}

TEST_F(EmbedderTest, AbortedCallsCountUntilTheBrowserReplies) {
  V8TestingScope scope;
  SemanticEmbedder* embedder = CreateSemanticEmbedder(scope.GetScriptState());
  mock_remote_->set_hold_callback(true);

  auto* controller = AbortController::Create(scope.GetScriptState());
  auto* options = SemanticEmbedderEmbedOptions::Create();
  options->setSignal(controller->signal());
  auto* input = MakeInput("hello");
  for (int i = 0; i < SemanticEmbedder::kMaxInFlightEmbedRequests; ++i) {
    DummyExceptionStateForTesting exception_state;
    embedder->embed(scope.GetScriptState(), input, options, exception_state);
    EXPECT_FALSE(exception_state.HadException());
  }
  ASSERT_TRUE(base::test::RunUntil([&] {
    return mock_remote_->call_count() ==
           SemanticEmbedder::kMaxInFlightEmbedRequests;
  }));
  controller->abort(scope.GetScriptState());

  // The browser has not replied to the aborted calls, so they still count.
  DummyExceptionStateForTesting exception_state;
  auto promise =
      embedder->embed(scope.GetScriptState(), input,
                      SemanticEmbedderEmbedOptions::Create(), exception_state);
  ScriptPromiseTester tester(scope.GetScriptState(), promise);
  scope.PerformMicrotaskCheckpoint();
  ASSERT_TRUE(tester.IsRejected());
  EXPECT_TRUE(V8QuotaExceededError::ToWrappable(scope.GetIsolate(),
                                                tester.Value().V8Value()));

  // Once the browser replies to them, there is room again.
  mock_remote_->set_hold_callback(false);
  mock_remote_->RunHeldCallback();
  mock_remote_->FlushForTesting();
  DummyExceptionStateForTesting exception_state2;
  auto promise2 =
      embedder->embed(scope.GetScriptState(), input,
                      SemanticEmbedderEmbedOptions::Create(), exception_state2);
  ScriptPromiseTester tester2(scope.GetScriptState(), promise2);
  tester2.WaitUntilSettled();
  EXPECT_TRUE(tester2.IsFulfilled());
}

TEST_F(EmbedderTest, ParamsListsSupportedSpaces) {
  constexpr int64_t kShutdownDateMs = 4102444800000;  // 2100-01-01Z
  V8TestingScope scope;
  base::ScopedClosureRunner unbind = BindMockAIManager(scope);
  mock_ai_manager_.AddSupportedSpace(kEmbeddingSpace, std::nullopt);
  mock_ai_manager_.AddSupportedSpace(
      "deprecated-space",
      base::Time::FromMillisecondsSinceUnixEpoch(kShutdownDateMs));

  DummyExceptionStateForTesting exception_state;
  auto promise =
      SemanticEmbedder::params(scope.GetScriptState(), exception_state);
  EXPECT_FALSE(exception_state.HadException());
  ScriptPromiseTester tester(scope.GetScriptState(), promise);
  tester.WaitUntilSettled();
  ASSERT_TRUE(tester.IsFulfilled());
  EXPECT_EQ(mock_ai_manager_.params_call_count(), 1);

  SemanticEmbedderParams* params = ParamsFrom(scope, tester);
  ASSERT_TRUE(params);
  const auto& spaces = params->supportedSpaces();
  ASSERT_EQ(spaces.size(), 2u);
  EXPECT_EQ(spaces[0]->embeddingSpace(), kEmbeddingSpace);
  EXPECT_FALSE(spaces[0]->shutdownDate().has_value());
  EXPECT_EQ(spaces[1]->embeddingSpace(), "deprecated-space");
  ASSERT_TRUE(spaces[1]->shutdownDate().has_value());
  EXPECT_EQ(*spaces[1]->shutdownDate(), static_cast<uint64_t>(kShutdownDateMs));
}

TEST_F(EmbedderTest, ParamsResolvesEmptyWithoutSpaces) {
  V8TestingScope scope;
  base::ScopedClosureRunner unbind = BindMockAIManager(scope);

  DummyExceptionStateForTesting exception_state;
  auto promise =
      SemanticEmbedder::params(scope.GetScriptState(), exception_state);
  EXPECT_FALSE(exception_state.HadException());
  ScriptPromiseTester tester(scope.GetScriptState(), promise);
  tester.WaitUntilSettled();
  ASSERT_TRUE(tester.IsFulfilled());

  SemanticEmbedderParams* params = ParamsFrom(scope, tester);
  ASSERT_TRUE(params);
  EXPECT_TRUE(params->supportedSpaces().empty());
}

TEST_F(EmbedderTest, ParamsRejectsWhenManagerDisconnects) {
  V8TestingScope scope;
  base::ScopedClosureRunner unbind = BindMockAIManager(scope);
  mock_ai_manager_.set_hold_params_callback(true);

  DummyExceptionStateForTesting exception_state;
  auto promise =
      SemanticEmbedder::params(scope.GetScriptState(), exception_state);
  EXPECT_FALSE(exception_state.HadException());
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return mock_ai_manager_.params_call_count() == 1; }));

  // The browser goes away before replying: the promise rejects instead of
  // hanging (RejectOnDestruction, as for availability()).
  mock_ai_manager_.Disconnect();
  ScriptPromiseTester tester(scope.GetScriptState(), promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsRejected());
}

}  // namespace blink
