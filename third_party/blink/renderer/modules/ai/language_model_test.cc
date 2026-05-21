#include "third_party/blink/renderer/modules/ai/language_model.h"

#include "base/test/run_until.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_readable_stream_read_result.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_append_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_clone_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_message.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_message_content.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_prompt_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_blob_htmlcanvaselement_htmlimageelement_htmlvideoelement_imagebitmap_imagedata_offscreencanvas_svgimageelement_videoframe.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_language_model_message_value.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_language_model_prompt.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_languagemodelmessagecontentsequence_string.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {

class MockAILanguageModel : public mojom::blink::AILanguageModel {
 public:
  explicit MockAILanguageModel(
      mojo::PendingReceiver<mojom::blink::AILanguageModel> receiver)
      : receiver_(this, std::move(receiver)) {}

  void Prompt(Vector<mojom::blink::AILanguageModelPromptPtr> prompt,
              on_device_model::mojom::blink::ResponseConstraintPtr constraint,
              mojo::PendingRemote<mojom::blink::ModelStreamingResponder>
                  pending_responder) override {
    last_prompts_ = std::move(prompt);
    call_count_++;
    if (reset_client_on_call_) {
      pending_responder.reset();
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
    std::move(callback).Run(0);
  }
  void Destroy() override {}
  void Fork(
      mojo::PendingRemote<mojom::blink::AIManagerCreateLanguageModelClient>
          client) override {
    if (reset_client_on_call_) {
      client.reset();
    }
  }

  Vector<mojom::blink::AILanguageModelPromptPtr> last_prompts_;
  int call_count_ = 0;
  bool reset_client_on_call_ = false;
  mojo::Receiver<mojom::blink::AILanguageModel> receiver_;
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

}  // namespace blink
