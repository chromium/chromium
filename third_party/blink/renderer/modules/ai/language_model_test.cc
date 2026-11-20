// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/language_model.h"

#include <optional>

#include "base/notreached.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom-blink.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-blink-test-utils.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_readable_stream_read_result.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_append_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_clone_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_create_core_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_create_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_message.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_message_content.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_prompt_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_sampling_mode.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_thinking_effort.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_blob_htmlcanvaselement_htmlimageelement_htmlvideoelement_imagebitmap_imagedata_offscreencanvas_svgimageelement_videoframe.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_language_model_message_value.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_language_model_prompt.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_languagemodelmessagecontentsequence_string.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/console_message_storage.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {

class MockAILanguageModel : public mojom::blink::AILanguageModel {
 public:
  explicit MockAILanguageModel(
      mojo::PendingReceiver<mojom::blink::AILanguageModel> receiver)
      : receiver_(this, std::move(receiver)) {}

  void Prompt(Vector<mojom::blink::AILanguageModelPromptPtr> prompt,
              on_device_model::mojom::blink::ResponseConstraintPtr constraint,
              std::optional<mojom::blink::AILanguageModelThinkingEffort>
                  thinking_effort,
              mojo::PendingRemote<mojom::blink::ModelStreamingResponder>
                  pending_responder) override {
    last_prompts_ = std::move(prompt);
    last_thinking_effort_ = thinking_effort;
    call_count_++;
    if (reset_client_on_call_) {
      pending_responder.reset();
    } else {
      mojo::Remote<mojom::blink::ModelStreamingResponder> responder(
          std::move(pending_responder));
      responder->OnStreaming("Hello response");
      responder->OnCompletion(nullptr);
    }
  }
  void Append(Vector<mojom::blink::AILanguageModelPromptPtr> prompt,
              mojo::PendingRemote<mojom::blink::ModelStreamingResponder> client)
      override {
    last_prompts_ = std::move(prompt);
    call_count_++;
    if (reset_client_on_call_) {
      client.reset();
    }
  }
  void MeasureInputUsage(Vector<mojom::blink::AILanguageModelPromptPtr> prompt,
                         MeasureInputUsageCallback callback) override {
    last_prompts_ = std::move(prompt);
    call_count_++;
    if (reset_client_on_call_) {
      receiver_.reset();
    } else {
      std::move(callback).Run(0);
    }
  }
  void Destroy() override {}
  void Fork(
      mojo::PendingRemote<mojom::blink::AIManagerCreateLanguageModelClient>
          client) override {
    if (reset_client_on_call_) {
      client.reset();
      return;
    }
    if (fork_info_) {
      mojo::PendingRemote<mojom::blink::AILanguageModel> pending_remote;
      forked_model_ = std::make_unique<MockAILanguageModel>(
          pending_remote.InitWithNewPipeAndPassReceiver());
      mojo::Remote<mojom::blink::AIManagerCreateLanguageModelClient>(
          std::move(client))
          ->OnResult(std::move(pending_remote), fork_info_.Clone());
    }
  }

  Vector<mojom::blink::AILanguageModelPromptPtr> last_prompts_;
  std::optional<mojom::blink::AILanguageModelThinkingEffort>
      last_thinking_effort_;
  int call_count_ = 0;
  bool reset_client_on_call_ = false;
  // When set, Fork() creates `forked_model_` and reports this info for it.
  mojom::blink::AILanguageModelInstanceInfoPtr fork_info_;
  std::unique_ptr<MockAILanguageModel> forked_model_;
  mojo::Receiver<mojom::blink::AILanguageModel> receiver_;
};

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

  void CanCreateLanguageModel(
      mojom::blink::AILanguageModelCreateOptionsPtr options,
      CanCreateLanguageModelCallback callback) override {
    last_can_create_options_ = std::move(options);
    std::move(callback).Run(
        mojom::blink::ModelAvailabilityCheckResult::kAvailable);
  }

  void CreateLanguageModel(
      mojo::PendingRemote<mojom::blink::AIManagerCreateLanguageModelClient>
          client,
      mojom::blink::AILanguageModelCreateOptionsPtr options,
      mojo::PendingRemote<on_device_model::mojom::blink::DownloadObserver>
          monitor) override {
    last_create_options_ = std::move(options);
    mojo::PendingRemote<mojom::blink::AILanguageModel> pending_remote;
    mock_language_model_ = std::make_unique<MockAILanguageModel>(
        pending_remote.InitWithNewPipeAndPassReceiver());
    auto info = mojom::blink::AILanguageModelInstanceInfo::New();
    info->sampling_params =
        mojom::blink::AILanguageModelSamplingParams::New(3, 1.0f);
    mojo::Remote<mojom::blink::AIManagerCreateLanguageModelClient>(
        std::move(client))
        ->OnResult(std::move(pending_remote), std::move(info));
  }

  mojom::blink::AILanguageModelCreateOptionsPtr last_can_create_options_;
  mojom::blink::AILanguageModelCreateOptionsPtr last_create_options_;
  std::unique_ptr<MockAILanguageModel> mock_language_model_;
  mojo::Receiver<mojom::blink::AIManager> receiver_{this};
};

class LanguageModelTest : public testing::Test {
 public:
  LanguageModel* CreateLanguageModel(
      ExecutionContext* context,
      mojom::blink::AILanguageModelInstanceInfoPtr info = nullptr) {
    mojo::PendingRemote<mojom::blink::AILanguageModel> pending_remote;
    mock_remote_ = std::make_unique<MockAILanguageModel>(
        pending_remote.InitWithNewPipeAndPassReceiver());

    if (!info) {
      info = mojom::blink::AILanguageModelInstanceInfo::New();
    }
    return MakeGarbageCollected<LanguageModel>(
        context, std::move(pending_remote),
        context->GetTaskRunner(TaskType::kInternalDefault), std::move(info));
  }

  V8LanguageModelPrompt* CreateEmptySequence() {
    return MakeGarbageCollected<V8LanguageModelPrompt>(
        HeapVector<Member<LanguageModelMessage>>());
  }

  V8LanguageModelPrompt* CreateStringPrompt(const String& prompt) {
    return MakeGarbageCollected<V8LanguageModelPrompt>(prompt);
  }

  V8LanguageModelPrompt* CreateImagePrompt(V8LanguageModelMessageValue* value) {
    auto* content = MakeGarbageCollected<LanguageModelMessageContent>();
    content->setType(
        V8LanguageModelMessageType(V8LanguageModelMessageType::Enum::kImage));
    content->setValue(value);
    HeapVector<Member<LanguageModelMessageContent>> content_sequence;
    content_sequence.push_back(content);

    auto* message = MakeGarbageCollected<LanguageModelMessage>();
    message->setRole(
        V8LanguageModelMessageRole(V8LanguageModelMessageRole::Enum::kUser));
    message->setContent(MakeGarbageCollected<
                        V8UnionLanguageModelMessageContentSequenceOrString>(
        content_sequence));
    HeapVector<Member<LanguageModelMessage>> messages;
    messages.push_back(message);
    return MakeGarbageCollected<V8LanguageModelPrompt>(messages);
  }

 protected:
  test::TaskEnvironment task_environment_;
  std::unique_ptr<MockAILanguageModel> mock_remote_;
};

template <typename IDLType>
DOMExceptionCode GetRejectedExceptionCode(ScriptState* script_state,
                                          ScriptPromise<IDLType> promise) {
  ScriptPromiseTester tester(script_state, promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsRejected());
  auto* dom_exception = V8DOMException::ToWrappable(script_state->GetIsolate(),
                                                    tester.Value().V8Value());
  EXPECT_TRUE(dom_exception);
  return static_cast<DOMExceptionCode>(dom_exception->code());
}

TEST_F(LanguageModelTest, EmptyInputSucceeds) {
  V8TestingScope scope;
  LanguageModel* language_model =
      CreateLanguageModel(scope.GetExecutionContext());
  V8LanguageModelPrompt* input = CreateEmptySequence();

  {
    DummyExceptionStateForTesting exception_state;
    language_model->prompt(scope.GetScriptState(), input,
                           LanguageModelPromptOptions::Create(),
                           exception_state);
    EXPECT_FALSE(exception_state.HadException());
  }

  {
    DummyExceptionStateForTesting exception_state;
    language_model->promptStreaming(scope.GetScriptState(), input,
                                    LanguageModelPromptOptions::Create(),
                                    exception_state);
    EXPECT_FALSE(exception_state.HadException());
  }

  {
    DummyExceptionStateForTesting exception_state;
    language_model->append(scope.GetScriptState(), input,
                           LanguageModelAppendOptions::Create(),
                           exception_state);
    EXPECT_FALSE(exception_state.HadException());
  }

  {
    DummyExceptionStateForTesting exception_state;
    language_model->measureContextUsage(scope.GetScriptState(), input,
                                        LanguageModelPromptOptions::Create(),
                                        exception_state);
    EXPECT_FALSE(exception_state.HadException());
  }
}

TEST_F(LanguageModelTest, VerifyMojoConversion) {
  V8TestingScope scope;
  LanguageModel* language_model =
      CreateLanguageModel(scope.GetExecutionContext());

  // Test empty string: prompt("")
  // Should result in 1 message (user role) and 1 empty text content entry.
  {
    DummyExceptionStateForTesting exception_state;
    language_model->prompt(scope.GetScriptState(), CreateStringPrompt(""),
                           LanguageModelPromptOptions::Create(),
                           exception_state);
    EXPECT_TRUE(
        base::test::RunUntil([&]() { return mock_remote_->call_count_ == 1; }));
    ASSERT_EQ(mock_remote_->last_prompts_.size(), 1u);
    EXPECT_EQ(mock_remote_->last_prompts_[0]->role,
              mojom::blink::AILanguageModelPromptRole::kUser);
    ASSERT_EQ(mock_remote_->last_prompts_[0]->content.size(), 1u);
    EXPECT_EQ(mock_remote_->last_prompts_[0]->content[0]->get_text(), "");
  }

  // Test empty sequence: prompt([])
  // Should result in 1 message (user role) and 1 empty text content entry.
  {
    mock_remote_->last_prompts_.clear();
    DummyExceptionStateForTesting exception_state;
    language_model->prompt(scope.GetScriptState(), CreateEmptySequence(),
                           LanguageModelPromptOptions::Create(),
                           exception_state);
    EXPECT_TRUE(
        base::test::RunUntil([&]() { return mock_remote_->call_count_ == 2; }));
    ASSERT_EQ(mock_remote_->last_prompts_.size(), 1u);
    EXPECT_EQ(mock_remote_->last_prompts_[0]->role,
              mojom::blink::AILanguageModelPromptRole::kUser);
    ASSERT_EQ(mock_remote_->last_prompts_[0]->content.size(), 1u);
    EXPECT_EQ(mock_remote_->last_prompts_[0]->content[0]->get_text(), "");
  }

  // Test explicit empty content sequence:
  // prompt([{role: 'user', content: []}])
  // Should result in 1 message (user role) and 1 empty text content entry.
  {
    mock_remote_->last_prompts_.clear();
    auto* message = MakeGarbageCollected<LanguageModelMessage>();
    message->setRole(
        V8LanguageModelMessageRole(V8LanguageModelMessageRole::Enum::kUser));
    message->setContent(MakeGarbageCollected<
                        V8UnionLanguageModelMessageContentSequenceOrString>(
        HeapVector<Member<LanguageModelMessageContent>>()));

    HeapVector<Member<LanguageModelMessage>> messages;
    messages.push_back(message);
    V8LanguageModelPrompt* input =
        MakeGarbageCollected<V8LanguageModelPrompt>(messages);

    DummyExceptionStateForTesting exception_state;
    language_model->prompt(scope.GetScriptState(), input,
                           LanguageModelPromptOptions::Create(),
                           exception_state);
    EXPECT_TRUE(
        base::test::RunUntil([&]() { return mock_remote_->call_count_ == 3; }));
    ASSERT_EQ(mock_remote_->last_prompts_.size(), 1u);
    EXPECT_EQ(mock_remote_->last_prompts_[0]->role,
              mojom::blink::AILanguageModelPromptRole::kUser);
    ASSERT_EQ(mock_remote_->last_prompts_[0]->content.size(), 1u);
    EXPECT_EQ(mock_remote_->last_prompts_[0]->content[0]->get_text(), "");
  }
}

TEST_F(LanguageModelTest, TextPrefixWithoutMultimodalInput) {
  ScopedAIPromptAPIMultimodalInputForTest scoped_multimodal(false);
  V8TestingScope scope;
  LanguageModel* language_model =
      CreateLanguageModel(scope.GetExecutionContext());
  auto* message = MakeGarbageCollected<LanguageModelMessage>();
  message->setRole(
      V8LanguageModelMessageRole(V8LanguageModelMessageRole::Enum::kAssistant));
  message->setContent(
      MakeGarbageCollected<V8UnionLanguageModelMessageContentSequenceOrString>(
          "hello, "));
  message->setPrefix(true);
  HeapVector<Member<LanguageModelMessage>> messages;
  messages.push_back(message);
  auto* input = MakeGarbageCollected<V8LanguageModelPrompt>(messages);

  DummyExceptionStateForTesting exception_state;
  auto promise = language_model->prompt(scope.GetScriptState(), input,
                                        LanguageModelPromptOptions::Create(),
                                        exception_state);
  ASSERT_FALSE(exception_state.HadException());
  ScriptPromiseTester tester(scope.GetScriptState(), promise);
  tester.WaitUntilSettled();

  EXPECT_EQ(mock_remote_->call_count_, 1);
  ASSERT_EQ(mock_remote_->last_prompts_.size(), 1u);
  const auto& prompt = mock_remote_->last_prompts_[0];
  EXPECT_EQ(prompt->role, mojom::blink::AILanguageModelPromptRole::kAssistant);
  EXPECT_TRUE(prompt->is_prefix);
  ASSERT_EQ(prompt->content.size(), 1u);
  EXPECT_EQ(prompt->content[0]->get_text(), "hello, ");
}

TEST_F(LanguageModelTest, MojoDisconnectPrompt) {
  V8TestingScope scope;
  LanguageModel* language_model =
      CreateLanguageModel(scope.GetExecutionContext());
  mock_remote_->reset_client_on_call_ = true;
  V8LanguageModelPrompt* input = CreateStringPrompt("foobar");
  EXPECT_TRUE(mock_remote_->receiver_.is_bound());

  ScriptState* script_state = scope.GetScriptState();
  DummyExceptionStateForTesting exception_state;
  auto promise = language_model->prompt(script_state, input,
                                        LanguageModelPromptOptions::Create(),
                                        exception_state);

  // Check that the promise will be rejected
  EXPECT_EQ(GetRejectedExceptionCode(script_state, promise),
            DOMExceptionCode::kInvalidStateError);
}

TEST_F(LanguageModelTest, MojoDisconnectPromptStreaming) {
  V8TestingScope scope;
  LanguageModel* language_model =
      CreateLanguageModel(scope.GetExecutionContext());
  mock_remote_->reset_client_on_call_ = true;
  V8LanguageModelPrompt* input = CreateStringPrompt("foobar");
  EXPECT_TRUE(mock_remote_->receiver_.is_bound());

  ScriptState* script_state = scope.GetScriptState();
  DummyExceptionStateForTesting exception_state;
  ReadableStream* stream = language_model->promptStreaming(
      script_state, input, LanguageModelPromptOptions::Create(),
      exception_state);

  // Check that the InvalidStateError is passed to the stream.
  auto* reader =
      stream->GetDefaultReaderForTesting(script_state, ASSERT_NO_EXCEPTION);
  auto read_promise = reader->read(script_state, ASSERT_NO_EXCEPTION);
  EXPECT_EQ(GetRejectedExceptionCode(script_state, read_promise),
            DOMExceptionCode::kInvalidStateError);
}

TEST_F(LanguageModelTest, MojoDisconnectAppend) {
  V8TestingScope scope;
  LanguageModel* language_model =
      CreateLanguageModel(scope.GetExecutionContext());
  mock_remote_->reset_client_on_call_ = true;
  V8LanguageModelPrompt* input = CreateStringPrompt("foobar");
  EXPECT_TRUE(mock_remote_->receiver_.is_bound());

  ScriptState* script_state = scope.GetScriptState();
  DummyExceptionStateForTesting exception_state;
  auto promise = language_model->append(script_state, input,
                                        LanguageModelAppendOptions::Create(),
                                        exception_state);

  // Check that the promise will be rejected
  EXPECT_EQ(GetRejectedExceptionCode(script_state, promise),
            DOMExceptionCode::kInvalidStateError);
}

TEST_F(LanguageModelTest, MojoDisconnectClone) {
  V8TestingScope scope;
  LanguageModel* language_model =
      CreateLanguageModel(scope.GetExecutionContext());
  mock_remote_->reset_client_on_call_ = true;
  EXPECT_TRUE(mock_remote_->receiver_.is_bound());

  ScriptState* script_state = scope.GetScriptState();
  DummyExceptionStateForTesting exception_state;
  auto promise = language_model->clone(
      script_state, LanguageModelCloneOptions::Create(), exception_state);

  // Check that the promise will be rejected
  EXPECT_EQ(GetRejectedExceptionCode(script_state, promise),
            DOMExceptionCode::kInvalidStateError);
}

TEST_F(LanguageModelTest, MojoDisconnectMeasureContextUsage) {
  V8TestingScope scope;
  LanguageModel* language_model =
      CreateLanguageModel(scope.GetExecutionContext());
  mock_remote_->reset_client_on_call_ = true;
  V8LanguageModelPrompt* input = CreateStringPrompt("foobar");
  EXPECT_TRUE(mock_remote_->receiver_.is_bound());

  ScriptState* script_state = scope.GetScriptState();
  DummyExceptionStateForTesting exception_state;
  auto promise = language_model->measureContextUsage(
      script_state, input, LanguageModelPromptOptions::Create(),
      exception_state);

  // Check that the promise will be rejected
  ScriptPromiseTester tester(script_state, promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsRejected());
}

TEST_F(LanguageModelTest, PromptStreamingWithFaultyImage) {
  V8TestingScope scope;
  ScopedAIPromptAPIMultimodalInputForTest scoped_multimodal(true);
  auto info = mojom::blink::AILanguageModelInstanceInfo::New();
  info->input_types = Vector<mojom::blink::AILanguageModelPromptType>{
      mojom::blink::AILanguageModelPromptType::kImage};
  LanguageModel* language_model =
      CreateLanguageModel(scope.GetExecutionContext(), std::move(info));

  // HTMLImageElement has no source loaded, so it fails CheckUsability().
  auto* image_element =
      MakeGarbageCollected<HTMLImageElement>(scope.GetDocument());
  auto* message_value =
      MakeGarbageCollected<V8LanguageModelMessageValue>(image_element);
  V8LanguageModelPrompt* input = CreateImagePrompt(message_value);

  ScriptState* script_state = scope.GetScriptState();
  DummyExceptionStateForTesting exception_state;
  ReadableStream* stream = language_model->promptStreaming(
      script_state, input, LanguageModelPromptOptions::Create(),
      exception_state);

  auto* reader =
      stream->GetDefaultReaderForTesting(script_state, ASSERT_NO_EXCEPTION);
  auto read_promise = reader->read(script_state, ASSERT_NO_EXCEPTION);
  EXPECT_EQ(GetRejectedExceptionCode(script_state, read_promise),
            DOMExceptionCode::kInvalidStateError);
}

TEST_F(LanguageModelTest, PromptStreamingWithTaintedImage) {
  V8TestingScope scope;
  ScopedAIPromptAPIMultimodalInputForTest scoped_multimodal(true);
  auto info = mojom::blink::AILanguageModelInstanceInfo::New();
  info->input_types = Vector<mojom::blink::AILanguageModelPromptType>{
      mojom::blink::AILanguageModelPromptType::kImage};
  LanguageModel* language_model =
      CreateLanguageModel(scope.GetExecutionContext(), std::move(info));

  SkImageInfo image_info = SkImageInfo::MakeN32Premul(10, 10);
  sk_sp<SkSurface> surface = SkSurfaces::Raster(image_info);
  auto image =
      UnacceleratedStaticBitmapImage::Create(surface->makeImageSnapshot());
  image->SetOriginClean(false);
  auto* image_bitmap = MakeGarbageCollected<ImageBitmap>(image);
  auto* message_value =
      MakeGarbageCollected<V8LanguageModelMessageValue>(image_bitmap);
  V8LanguageModelPrompt* input = CreateImagePrompt(message_value);

  ScriptState* script_state = scope.GetScriptState();
  DummyExceptionStateForTesting exception_state;
  ReadableStream* stream = language_model->promptStreaming(
      script_state, input, LanguageModelPromptOptions::Create(),
      exception_state);

  auto* reader =
      stream->GetDefaultReaderForTesting(script_state, ASSERT_NO_EXCEPTION);
  auto read_promise = reader->read(script_state, ASSERT_NO_EXCEPTION);
  EXPECT_EQ(GetRejectedExceptionCode(script_state, read_promise),
            DOMExceptionCode::kSecurityError);
}

TEST_F(LanguageModelTest, PromptResolvesToStringWhenOutputTypesIsTextOnly) {
  V8TestingScope scope;
  auto info = mojom::blink::AILanguageModelInstanceInfo::New();
  info->output_types = Vector<mojom::blink::AILanguageModelPromptType>{
      mojom::blink::AILanguageModelPromptType::kText};
  LanguageModel* language_model =
      CreateLanguageModel(scope.GetExecutionContext(), std::move(info));

  ScriptState* script_state = scope.GetScriptState();
  DummyExceptionStateForTesting exception_state;
  auto promise = language_model->prompt(
      script_state, CreateStringPrompt("Hello"),
      LanguageModelPromptOptions::Create(), exception_state);

  ScriptPromiseTester tester(script_state, promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());
  EXPECT_TRUE(tester.Value().V8Value()->IsString());
}

TEST_F(LanguageModelTest,
       PromptResolvesToSequenceWhenOutputTypesIncludesToolCall) {
  V8TestingScope scope;
  ScopedAIPromptAPIToolUseForTest scoped_tool_use(true);
  auto info = mojom::blink::AILanguageModelInstanceInfo::New();
  info->output_types = Vector<mojom::blink::AILanguageModelPromptType>{
      mojom::blink::AILanguageModelPromptType::kText,
      mojom::blink::AILanguageModelPromptType::kToolCall};
  LanguageModel* language_model =
      CreateLanguageModel(scope.GetExecutionContext(), std::move(info));

  ScriptState* script_state = scope.GetScriptState();
  DummyExceptionStateForTesting exception_state;
  auto promise = language_model->prompt(
      script_state, CreateStringPrompt("Hello"),
      LanguageModelPromptOptions::Create(), exception_state);

  ScriptPromiseTester tester(script_state, promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());
  EXPECT_TRUE(tester.Value().V8Value()->IsArray());
  auto array = tester.Value().V8Value().As<v8::Array>();
  EXPECT_EQ(array->Length(), 1u);
}

TEST_F(LanguageModelTest, SamplingModeTakesPrecedenceOverLegacyParams) {
  ScopedAIPromptAPIForTest scoped_prompt_api(true);
  ScopedAIPromptAPIParamsForTest scoped_params(true);
  ScopedAIPromptAPILegacyParamsForTest scoped_legacy_params(true);
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

  MockAIManager mock_ai_manager;
  scope.GetExecutionContext()->GetBrowserInterfaceBroker().SetBinderForTesting(
      mojom::blink::AIManager::Name_,
      BindRepeating(&MockAIManager::Bind, Unretained(&mock_ai_manager)));

  auto check_create =
      [&](LanguageModelCreateOptions* options,
          mojom::blink::AILanguageModelSamplingMode expected_mojo_mode) {
        DummyExceptionStateForTesting exception_state;
        auto promise =
            LanguageModel::create(script_state, options, exception_state);
        ASSERT_FALSE(exception_state.HadException());

        ScriptPromiseTester tester(script_state, promise);
        tester.WaitUntilSettled();
        ASSERT_TRUE(tester.IsFulfilled());

        LanguageModel* model = V8LanguageModel::ToWrappable(
            scope.GetIsolate(), tester.Value().V8Value());
        ASSERT_TRUE(model);
        EXPECT_EQ(model->samplingMode(), options->samplingMode());
        EXPECT_NE(model->temperature(), 0.8f);
        EXPECT_NE(model->topK(), 10u);

        ASSERT_TRUE(mock_ai_manager.last_can_create_options_);
        EXPECT_EQ(mock_ai_manager.last_can_create_options_->sampling_mode,
                  expected_mojo_mode);
        EXPECT_FALSE(mock_ai_manager.last_can_create_options_->sampling_params);

        ASSERT_TRUE(mock_ai_manager.last_create_options_);
        EXPECT_EQ(mock_ai_manager.last_create_options_->sampling_mode,
                  expected_mojo_mode);
        EXPECT_FALSE(mock_ai_manager.last_create_options_->sampling_params);
      };

  {
    auto* options = LanguageModelCreateOptions::Create();
    options->setSamplingMode(
        V8LanguageModelSamplingMode::Enum::kMostPredictable);
    options->setTemperature(0.8);
    check_create(options,
                 mojom::blink::AILanguageModelSamplingMode::kMostPredictable);
  }

  {
    auto* options = LanguageModelCreateOptions::Create();
    options->setSamplingMode(V8LanguageModelSamplingMode::Enum::kPredictable);
    options->setTopK(10);
    check_create(options,
                 mojom::blink::AILanguageModelSamplingMode::kPredictable);
  }

  {
    auto* options = LanguageModelCreateOptions::Create();
    options->setSamplingMode(V8LanguageModelSamplingMode::Enum::kCreative);
    options->setTemperature(0.8);
    options->setTopK(10);
    check_create(options, mojom::blink::AILanguageModelSamplingMode::kCreative);
  }

  scope.GetExecutionContext()->GetBrowserInterfaceBroker().SetBinderForTesting(
      mojom::blink::AIManager::Name_, {});
}

TEST_F(LanguageModelTest, MeasureContextUsageResponseConstraint) {
  ScopedAIPromptAPIStructuredOutputForTest scoped_structured_output(true);
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  v8::Isolate* isolate = scope.GetIsolate();
  v8::Local<v8::Context> context = scope.GetContext();
  LanguageModel* language_model =
      CreateLanguageModel(scope.GetExecutionContext());

  v8::Local<v8::Object> schema_obj =
      v8::JSON::Parse(context, V8String(isolate, R"({"type":"string"})"))
          .ToLocalChecked()
          .As<v8::Object>();

  // JSON schema constraint appends schema instructions by default.
  {
    auto* options = LanguageModelPromptOptions::Create();
    options->setResponseConstraint(ScriptObject(isolate, schema_obj));
    DummyExceptionStateForTesting exception_state;
    auto promise = language_model->measureContextUsage(
        script_state, CreateStringPrompt("Hello"), options, exception_state);
    ASSERT_FALSE(exception_state.HadException());

    ScriptPromiseTester tester(script_state, promise);
    tester.WaitUntilSettled();
    EXPECT_TRUE(tester.IsFulfilled());
    EXPECT_EQ(mock_remote_->call_count_, 1);
    ASSERT_EQ(mock_remote_->last_prompts_.size(), 1u);
    EXPECT_EQ(mock_remote_->last_prompts_[0]->role,
              mojom::blink::AILanguageModelPromptRole::kUser);
    ASSERT_EQ(mock_remote_->last_prompts_[0]->content.size(), 2u);
    EXPECT_EQ(mock_remote_->last_prompts_[0]->content[0]->get_text(), "Hello");
    EXPECT_EQ(mock_remote_->last_prompts_[0]->content[1]->get_text(),
              "\n\nRemember to respond in JSON that follows this \"JSON "
              "Schema\" specification:\n{\"type\":\"string\"}");
  }

  // omitResponseConstraintInput: true omits schema instructions.
  {
    mock_remote_->last_prompts_.clear();
    auto* options = LanguageModelPromptOptions::Create();
    options->setResponseConstraint(ScriptObject(isolate, schema_obj));
    options->setOmitResponseConstraintInput(true);
    DummyExceptionStateForTesting exception_state;
    auto promise = language_model->measureContextUsage(
        script_state, CreateStringPrompt("Hello"), options, exception_state);
    ASSERT_FALSE(exception_state.HadException());

    ScriptPromiseTester tester(script_state, promise);
    tester.WaitUntilSettled();
    EXPECT_TRUE(tester.IsFulfilled());
    EXPECT_EQ(mock_remote_->call_count_, 2);
    ASSERT_EQ(mock_remote_->last_prompts_.size(), 1u);
    ASSERT_EQ(mock_remote_->last_prompts_[0]->content.size(), 1u);
    EXPECT_EQ(mock_remote_->last_prompts_[0]->content[0]->get_text(), "Hello");
  }

  // RegExp constraint does not append schema instructions.
  {
    mock_remote_->last_prompts_.clear();
    v8::Local<v8::RegExp> regex_obj =
        v8::RegExp::New(context, V8String(isolate, "[a-z]+"), v8::RegExp::kNone)
            .ToLocalChecked();
    auto* options = LanguageModelPromptOptions::Create();
    options->setResponseConstraint(ScriptObject(isolate, regex_obj));
    DummyExceptionStateForTesting exception_state;
    auto promise = language_model->measureContextUsage(
        script_state, CreateStringPrompt("Hello"), options, exception_state);
    ASSERT_FALSE(exception_state.HadException());

    ScriptPromiseTester tester(script_state, promise);
    tester.WaitUntilSettled();
    EXPECT_TRUE(tester.IsFulfilled());
    EXPECT_EQ(mock_remote_->call_count_, 3);
    ASSERT_EQ(mock_remote_->last_prompts_.size(), 1u);
    ASSERT_EQ(mock_remote_->last_prompts_[0]->content.size(), 1u);
    EXPECT_EQ(mock_remote_->last_prompts_[0]->content[0]->get_text(), "Hello");
  }

  // Circular reference in responseConstraint throws NotSupportedError.
  {
    v8::Local<v8::Object> circular_obj = v8::Object::New(isolate);
    ASSERT_TRUE(
        circular_obj->Set(context, V8String(isolate, "self"), circular_obj)
            .FromJust());
    auto* options = LanguageModelPromptOptions::Create();
    options->setResponseConstraint(ScriptObject(isolate, circular_obj));
    DummyExceptionStateForTesting exception_state;
    language_model->measureContextUsage(
        script_state, CreateStringPrompt("Hello"), options, exception_state);
    EXPECT_TRUE(exception_state.HadException());
    EXPECT_EQ(exception_state.CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kNotSupportedError);
    EXPECT_EQ(mock_remote_->call_count_, 3);
  }
}

TEST_F(LanguageModelTest, ThinkingEffortDefaultsToNoneWhenUnset) {
  V8TestingScope scope;
  ScopedAIPromptAPIThinkingModeForTest scoped_thinking(true);
  LanguageModel* language_model =
      CreateLanguageModel(scope.GetExecutionContext());

  EXPECT_EQ(language_model->thinkingEffort(),
            V8LanguageModelThinkingEffort::Enum::kNone);
}

TEST_F(LanguageModelTest, PromptAndStreamingSendThinkingEffortOverride) {
  V8TestingScope scope;
  ScopedAIPromptAPIThinkingModeForTest scoped_thinking(true);
  auto info = mojom::blink::AILanguageModelInstanceInfo::New();
  info->thinking_effort = mojom::blink::AILanguageModelThinkingEffort::kMedium;
  LanguageModel* language_model =
      CreateLanguageModel(scope.GetExecutionContext(), std::move(info));
  EXPECT_EQ(language_model->thinkingEffort(),
            V8LanguageModelThinkingEffort::Enum::kMedium);

  // Per prompt override is sent for that turn only without changing session
  // `thinkingEffort`.
  auto* prompt_options = LanguageModelPromptOptions::Create();
  prompt_options->setThinkingEffort(V8LanguageModelThinkingEffort(
      V8LanguageModelThinkingEffort::Enum::kHigh));
  DummyExceptionStateForTesting exception_state;
  language_model->prompt(scope.GetScriptState(), CreateStringPrompt("hi"),
                         prompt_options, exception_state);
  EXPECT_FALSE(exception_state.HadException());
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return mock_remote_->call_count_ == 1; }));
  EXPECT_EQ(mock_remote_->last_thinking_effort_,
            mojom::blink::AILanguageModelThinkingEffort::kHigh);
  EXPECT_EQ(language_model->thinkingEffort(),
            V8LanguageModelThinkingEffort::Enum::kMedium);

  auto* stream_options = LanguageModelPromptOptions::Create();
  stream_options->setThinkingEffort(
      V8LanguageModelThinkingEffort(V8LanguageModelThinkingEffort::Enum::kLow));
  language_model->promptStreaming(scope.GetScriptState(),
                                  CreateStringPrompt("hi"), stream_options,
                                  exception_state);
  EXPECT_FALSE(exception_state.HadException());
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return mock_remote_->call_count_ == 2; }));
  EXPECT_EQ(mock_remote_->last_thinking_effort_,
            mojom::blink::AILanguageModelThinkingEffort::kLow);

  // Without an override, no effort is sent and the session effort applies.
  language_model->prompt(scope.GetScriptState(), CreateStringPrompt("hi"),
                         LanguageModelPromptOptions::Create(), exception_state);
  EXPECT_FALSE(exception_state.HadException());
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return mock_remote_->call_count_ == 3; }));
  EXPECT_FALSE(mock_remote_->last_thinking_effort_.has_value());
}

TEST_F(LanguageModelTest, CreateAvailabilityAndClonePreserveThinkingEffort) {
  ScopedAIPromptAPIForTest scoped_prompt_api(true);
  ScopedAIPromptAPIThinkingModeForTest scoped_thinking(true);
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  MockAIManager mock_ai_manager;
  scope.GetExecutionContext()->GetBrowserInterfaceBroker().SetBinderForTesting(
      mojom::blink::AIManager::Name_,
      BindRepeating(&MockAIManager::Bind, Unretained(&mock_ai_manager)));

  auto* avail_options = LanguageModelCreateCoreOptions::Create();
  avail_options->setThinkingEffort(
      V8LanguageModelThinkingEffort(V8LanguageModelThinkingEffort::Enum::kLow));
  DummyExceptionStateForTesting exception_state;
  ScriptPromiseTester avail_tester(
      script_state, LanguageModel::availability(script_state, avail_options,
                                                exception_state));
  avail_tester.WaitUntilSettled();
  ASSERT_TRUE(avail_tester.IsFulfilled());
  EXPECT_EQ(avail_tester.ValueAsString(), "available");
  ASSERT_TRUE(mock_ai_manager.last_can_create_options_);
  EXPECT_EQ(mock_ai_manager.last_can_create_options_->thinking_effort,
            mojom::blink::AILanguageModelThinkingEffort::kLow);

  auto* create_options = LanguageModelCreateOptions::Create();
  create_options->setThinkingEffort(V8LanguageModelThinkingEffort(
      V8LanguageModelThinkingEffort::Enum::kHigh));
  ScriptPromiseTester create_tester(
      script_state,
      LanguageModel::create(script_state, create_options, exception_state));
  create_tester.WaitUntilSettled();
  ASSERT_TRUE(create_tester.IsFulfilled());
  ASSERT_TRUE(mock_ai_manager.last_create_options_);
  EXPECT_EQ(mock_ai_manager.last_create_options_->thinking_effort,
            mojom::blink::AILanguageModelThinkingEffort::kHigh);
  LanguageModel* session = V8LanguageModel::ToWrappable(
      scope.GetIsolate(), create_tester.Value().V8Value());
  ASSERT_TRUE(session);
  EXPECT_EQ(session->thinkingEffort(),
            V8LanguageModelThinkingEffort::Enum::kHigh);

  mock_ai_manager.mock_language_model_->fork_info_ =
      mojom::blink::AILanguageModelInstanceInfo::New();
  mock_ai_manager.mock_language_model_->fork_info_->sampling_params =
      mojom::blink::AILanguageModelSamplingParams::New(3, 1.0f);
  ScriptPromiseTester clone_tester(
      script_state,
      session->clone(script_state, LanguageModelCloneOptions::Create(),
                     exception_state));
  clone_tester.WaitUntilSettled();
  ASSERT_TRUE(clone_tester.IsFulfilled());
  LanguageModel* clone = V8LanguageModel::ToWrappable(
      scope.GetIsolate(), clone_tester.Value().V8Value());
  ASSERT_TRUE(clone);
  EXPECT_EQ(clone->thinkingEffort(),
            V8LanguageModelThinkingEffort::Enum::kHigh);

  scope.GetExecutionContext()->GetBrowserInterfaceBroker().SetBinderForTesting(
      mojom::blink::AIManager::Name_, {});
}

TEST_F(LanguageModelTest, WarnsOnceWhenThinkingUsesNonBalancedSampling) {
  ScopedAIPromptAPIForTest scoped_prompt_api(true);
  ScopedAIPromptAPIParamsForTest scoped_params(true);
  ScopedAIPromptAPIThinkingModeForTest scoped_thinking(true);
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  MockAIManager mock_ai_manager;
  scope.GetExecutionContext()->GetBrowserInterfaceBroker().SetBinderForTesting(
      mojom::blink::AIManager::Name_,
      BindRepeating(&MockAIManager::Bind, Unretained(&mock_ai_manager)));
  auto& console_messages = scope.GetPage().GetConsoleMessageStorage();
  auto count_thinking_warnings = [&]() {
    size_t count = 0;
    for (wtf_size_t i = 0; i < console_messages.size(); ++i) {
      const ConsoleMessage* msg = console_messages.at(i);
      if (msg->GetLevel() == mojom::blink::ConsoleMessageLevel::kWarning &&
          msg->Message() ==
              "Thinking mode works best with the default (\"balanced\") "
              "sampling configuration. Using custom sampling options may "
              "degrade reasoning quality.") {
        ++count;
      }
    }
    return count;
  };

  auto create_session = [&](LanguageModelCreateOptions* options) {
    DummyExceptionStateForTesting exception_state;
    ScriptPromiseTester tester(
        script_state,
        LanguageModel::create(script_state, options, exception_state));
    tester.WaitUntilSettled();
    EXPECT_TRUE(tester.IsFulfilled());
    return V8LanguageModel::ToWrappable(scope.GetIsolate(),
                                        tester.Value().V8Value());
  };

  // Default creation uses `"balanced"` sampling even when `AIPromptAPIParams`
  // is disabled, so thinking does not warn.
  {
    ScopedAIPromptAPIParamsForTest disabled_params(false);
    auto* options = LanguageModelCreateOptions::Create();
    options->setThinkingEffort(V8LanguageModelThinkingEffort(
        V8LanguageModelThinkingEffort::Enum::kHigh));
    LanguageModel* session = create_session(options);
    ASSERT_TRUE(session);
    ASSERT_TRUE(mock_ai_manager.last_create_options_);
    EXPECT_EQ(mock_ai_manager.last_create_options_->sampling_mode,
              mojom::blink::AILanguageModelSamplingMode::kBalanced);
    EXPECT_EQ(count_thinking_warnings(), 0u);
  }

  // Creating with thinking enabled on a non `"balanced"` session warns once at
  // `create()`, and neither subsequent prompts nor `clone()` repeat it.
  {
    auto* options = LanguageModelCreateOptions::Create();
    options->setSamplingMode(V8LanguageModelSamplingMode::Enum::kCreative);
    options->setThinkingEffort(V8LanguageModelThinkingEffort(
        V8LanguageModelThinkingEffort::Enum::kMedium));
    LanguageModel* session = create_session(options);
    ASSERT_TRUE(session);
    EXPECT_EQ(count_thinking_warnings(), 1u);

    DummyExceptionStateForTesting exception_state;
    ScriptPromiseTester prompt_tester(
        script_state,
        session->prompt(script_state, CreateStringPrompt("hi"),
                        LanguageModelPromptOptions::Create(), exception_state));
    prompt_tester.WaitUntilSettled();
    EXPECT_EQ(count_thinking_warnings(), 1u);

    mock_ai_manager.mock_language_model_->fork_info_ =
        mojom::blink::AILanguageModelInstanceInfo::New();
    mock_ai_manager.mock_language_model_->fork_info_->sampling_params =
        mojom::blink::AILanguageModelSamplingParams::New(3, 1.0f);
    ScriptPromiseTester clone_tester(
        script_state,
        session->clone(script_state, LanguageModelCloneOptions::Create(),
                       exception_state));
    clone_tester.WaitUntilSettled();
    ASSERT_TRUE(clone_tester.IsFulfilled());
    LanguageModel* clone = V8LanguageModel::ToWrappable(
        scope.GetIsolate(), clone_tester.Value().V8Value());
    ASSERT_TRUE(clone);
    ScriptPromiseTester clone_prompt_tester(
        script_state,
        clone->prompt(script_state, CreateStringPrompt("hi"),
                      LanguageModelPromptOptions::Create(), exception_state));
    clone_prompt_tester.WaitUntilSettled();
    EXPECT_EQ(count_thinking_warnings(), 1u);
  }

  // Creating with thinking off on a non `"balanced"` session warns only on the
  // first prompt that enables thinking.
  {
    auto* options = LanguageModelCreateOptions::Create();
    options->setSamplingMode(V8LanguageModelSamplingMode::Enum::kPredictable);
    LanguageModel* session = create_session(options);
    ASSERT_TRUE(session);
    EXPECT_EQ(count_thinking_warnings(), 1u);

    DummyExceptionStateForTesting exception_state;
    ScriptPromiseTester none_tester(
        script_state,
        session->prompt(script_state, CreateStringPrompt("hi"),
                        LanguageModelPromptOptions::Create(), exception_state));
    none_tester.WaitUntilSettled();
    EXPECT_EQ(count_thinking_warnings(), 1u);

    auto* low_options = LanguageModelPromptOptions::Create();
    low_options->setThinkingEffort(V8LanguageModelThinkingEffort(
        V8LanguageModelThinkingEffort::Enum::kLow));
    ScriptPromiseTester low_tester(
        script_state, session->prompt(script_state, CreateStringPrompt("hi"),
                                      low_options, exception_state));
    low_tester.WaitUntilSettled();
    EXPECT_EQ(count_thinking_warnings(), 2u);

    auto* high_options = LanguageModelPromptOptions::Create();
    high_options->setThinkingEffort(V8LanguageModelThinkingEffort(
        V8LanguageModelThinkingEffort::Enum::kHigh));
    ReadableStream* stream = session->promptStreaming(
        script_state, CreateStringPrompt("hi"), high_options, exception_state);
    ASSERT_TRUE(stream);
    EXPECT_TRUE(base::test::RunUntil([&]() {
      return mock_ai_manager.mock_language_model_->call_count_ == 3;
    }));
    EXPECT_EQ(count_thinking_warnings(), 2u);
  }

  // Legacy `temperature` / `topK` with `AIPromptAPIParams` disabled (e.g.
  // extensions) also warns once.
  {
    ScopedAIPromptAPIParamsForTest disabled_params(false);
    ScopedAIPromptAPILegacyParamsForTest scoped_legacy_params(true);
    auto* options = LanguageModelCreateOptions::Create();
    options->setTemperature(0.2);
    options->setTopK(10);
    options->setThinkingEffort(V8LanguageModelThinkingEffort(
        V8LanguageModelThinkingEffort::Enum::kLow));
    LanguageModel* session = create_session(options);
    ASSERT_TRUE(session);
    EXPECT_EQ(count_thinking_warnings(), 3u);
  }

  scope.GetExecutionContext()->GetBrowserInterfaceBroker().SetBinderForTesting(
      mojom::blink::AIManager::Name_, {});
}

TEST_F(LanguageModelTest, ThinkingEffortUseCounterAndMetrics) {
  ScopedAIPromptAPIForTest scoped_prompt_api(true);
  ScopedAIPromptAPIThinkingModeForTest scoped_thinking(true);
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  Document& document = scope.GetDocument();
  MockAIManager mock_ai_manager;
  scope.GetExecutionContext()->GetBrowserInterfaceBroker().SetBinderForTesting(
      mojom::blink::AIManager::Name_,
      BindRepeating(&MockAIManager::Bind, Unretained(&mock_ai_manager)));
  base::HistogramTester histogram_tester;

  auto TakeUseCounted = [&](WebFeature feature) {
    bool is_counted = document.IsUseCounted(feature);
    document.ClearUseCounterForTesting(feature);
    return is_counted;
  };
  auto AwaitFulfilled = [&](auto promise) {
    ScriptPromiseTester tester(script_state, std::move(promise));
    tester.WaitUntilSettled();
    EXPECT_TRUE(tester.IsFulfilled());
  };

  // Omitted `thinkingEffort` records neither UMA nor UseCounters.
  AwaitFulfilled(LanguageModel::availability(
      script_state, LanguageModelCreateCoreOptions::Create(),
      ASSERT_NO_EXCEPTION));
  AwaitFulfilled(LanguageModel::create(
      script_state, LanguageModelCreateOptions::Create(), ASSERT_NO_EXCEPTION));
  EXPECT_FALSE(
      TakeUseCounted(WebFeature::kLanguageModel_Create_ThinkingEffort));
  EXPECT_FALSE(
      TakeUseCounted(WebFeature::kLanguageModel_Prompt_ThinkingEffort));

  // `availability({ thinkingEffort })` records UMA only, not UseCounters.
  auto* avail_options = LanguageModelCreateCoreOptions::Create();
  avail_options->setThinkingEffort(V8LanguageModelThinkingEffort::Enum::kLow);
  AwaitFulfilled(LanguageModel::availability(script_state, avail_options,
                                             ASSERT_NO_EXCEPTION));
  EXPECT_FALSE(
      TakeUseCounted(WebFeature::kLanguageModel_Create_ThinkingEffort));
  EXPECT_FALSE(
      TakeUseCounted(WebFeature::kLanguageModel_Prompt_ThinkingEffort));
  histogram_tester.ExpectUniqueSample(
      "AI.LanguageModel.APIUsage.availability.ThinkingEffort",
      mojom::blink::AILanguageModelThinkingEffort::kLow, 1);

  // `create({ thinkingEffort })` records both UMA and the create UseCounter,
  // including explicit `"none"`.
  for (auto effort : {V8LanguageModelThinkingEffort::Enum::kNone,
                      V8LanguageModelThinkingEffort::Enum::kHigh}) {
    auto* create_options = LanguageModelCreateOptions::Create();
    create_options->setThinkingEffort(effort);
    AwaitFulfilled(LanguageModel::create(script_state, create_options,
                                         ASSERT_NO_EXCEPTION));
    EXPECT_TRUE(
        TakeUseCounted(WebFeature::kLanguageModel_Create_ThinkingEffort));
    EXPECT_FALSE(
        TakeUseCounted(WebFeature::kLanguageModel_Prompt_ThinkingEffort));
  }
  histogram_tester.ExpectBucketCount(
      "AI.LanguageModel.APIUsage.create.ThinkingEffort",
      mojom::blink::AILanguageModelThinkingEffort::kNone, 1);
  histogram_tester.ExpectBucketCount(
      "AI.LanguageModel.APIUsage.create.ThinkingEffort",
      mojom::blink::AILanguageModelThinkingEffort::kHigh, 1);
  histogram_tester.ExpectTotalCount(
      "AI.LanguageModel.APIUsage.create.ThinkingEffort", 2);

  // `prompt()` and `promptStreaming()` count only the prompt UseCounter when
  // `thinkingEffort` is set.
  LanguageModel* session = CreateLanguageModel(scope.GetExecutionContext());
  AwaitFulfilled(session->prompt(script_state, CreateStringPrompt("hi"),
                                 LanguageModelPromptOptions::Create(),
                                 ASSERT_NO_EXCEPTION));
  EXPECT_FALSE(
      TakeUseCounted(WebFeature::kLanguageModel_Prompt_ThinkingEffort));

  auto* prompt_options = LanguageModelPromptOptions::Create();
  prompt_options->setThinkingEffort(
      V8LanguageModelThinkingEffort::Enum::kMedium);
  AwaitFulfilled(session->prompt(script_state, CreateStringPrompt("hi"),
                                 prompt_options, ASSERT_NO_EXCEPTION));
  EXPECT_TRUE(TakeUseCounted(WebFeature::kLanguageModel_Prompt_ThinkingEffort));
  EXPECT_FALSE(
      TakeUseCounted(WebFeature::kLanguageModel_Create_ThinkingEffort));

  EXPECT_TRUE(session->promptStreaming(script_state, CreateStringPrompt("hi"),
                                       LanguageModelPromptOptions::Create(),
                                       ASSERT_NO_EXCEPTION));
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return mock_remote_->call_count_ == 3; }));
  EXPECT_FALSE(
      TakeUseCounted(WebFeature::kLanguageModel_Prompt_ThinkingEffort));

  auto* stream_options = LanguageModelPromptOptions::Create();
  stream_options->setThinkingEffort(V8LanguageModelThinkingEffort::Enum::kLow);
  EXPECT_TRUE(session->promptStreaming(script_state, CreateStringPrompt("hi"),
                                       stream_options, ASSERT_NO_EXCEPTION));
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return mock_remote_->call_count_ == 4; }));
  EXPECT_TRUE(TakeUseCounted(WebFeature::kLanguageModel_Prompt_ThinkingEffort));
  EXPECT_FALSE(
      TakeUseCounted(WebFeature::kLanguageModel_Create_ThinkingEffort));

  scope.GetExecutionContext()->GetBrowserInterfaceBroker().SetBinderForTesting(
      mojom::blink::AIManager::Name_, {});
}

}  // namespace blink
