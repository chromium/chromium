// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/semantic_embedder.h"

#include "base/test/run_until.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/ai/ai_semantic_embedder.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_semantic_embedder_create_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_semantic_embedder_embed_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_semantic_embedder_input.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_semantic_embedder_input_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_semantic_embedder_result.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_semantic_embedder_task_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_typedefs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_semanticembedderinput_semanticembedderinputorstringsequence_string.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_semanticembedderinput_string.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

class MockAISemanticEmbedder : public mojom::blink::AISemanticEmbedder {
 public:
  explicit MockAISemanticEmbedder(
      mojo::PendingReceiver<mojom::blink::AISemanticEmbedder> receiver)
      : receiver_(this, std::move(receiver)) {}

  void Embed(const Vector<String>& inputs,
             mojom::blink::AISemanticEmbedderEmbedOptionsPtr options,
             EmbedCallback callback) override {
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
      held_callback_ = std::move(callback);
      return;
    }

    std::move(callback).Run(std::move(result));
  }

  void set_hold_callback(bool hold) { hold_callback_ = hold; }

  const Vector<String>& last_inputs() const { return last_inputs_; }
  const mojom::blink::AISemanticEmbedderEmbedOptions* last_options() const {
    return last_options_.get();
  }
  int call_count() const { return call_count_; }

 private:
  Vector<String> last_inputs_;
  mojom::blink::AISemanticEmbedderEmbedOptionsPtr last_options_;
  int call_count_ = 0;
  bool hold_callback_ = false;
  EmbedCallback held_callback_;
  mojo::Receiver<mojom::blink::AISemanticEmbedder> receiver_;
};

class EmbedderTest : public testing::Test {
 public:
  SemanticEmbedder* CreateSemanticEmbedder(ScriptState* script_state) {
    mojo::PendingRemote<mojom::blink::AISemanticEmbedder> pending_remote;
    mock_remote_ = std::make_unique<MockAISemanticEmbedder>(
        pending_remote.InitWithNewPipeAndPassReceiver());

    return MakeGarbageCollected<SemanticEmbedder>(
        script_state,
        ExecutionContext::From(script_state)
            ->GetTaskRunner(TaskType::kInternalDefault),
        std::move(pending_remote), SemanticEmbedderCreateOptions::Create());
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

}  // namespace blink
