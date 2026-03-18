#include "third_party/blink/renderer/modules/ai/language_model.h"

#include "base/test/run_until.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_append_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_message.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_message_content.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_prompt_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_language_model_prompt.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_languagemodelmessagecontentsequence_string.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

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
  }
  void Append(Vector<mojom::blink::AILanguageModelPromptPtr> prompt,
              mojo::PendingRemote<mojom::blink::ModelStreamingResponder> client)
      override {
    last_prompts_ = std::move(prompt);
    call_count_++;
  }
  void MeasureInputUsage(Vector<mojom::blink::AILanguageModelPromptPtr> prompt,
                         MeasureInputUsageCallback callback) override {
    std::move(callback).Run(0);
  }
  void Destroy() override {}
  void Fork(
      mojo::PendingRemote<mojom::blink::AIManagerCreateLanguageModelClient>
          client) override {}

  Vector<mojom::blink::AILanguageModelPromptPtr> last_prompts_;
  int call_count_ = 0;
  mojo::Receiver<mojom::blink::AILanguageModel> receiver_;
};

class LanguageModelTest : public testing::Test {
 public:
  LanguageModel* CreateLanguageModel(ExecutionContext* context) {
    mojo::PendingRemote<mojom::blink::AILanguageModel> pending_remote;
    mock_remote_ = std::make_unique<MockAILanguageModel>(
        pending_remote.InitWithNewPipeAndPassReceiver());

    auto info = mojom::blink::AILanguageModelInstanceInfo::New();
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

 protected:
  test::TaskEnvironment task_environment_;
  std::unique_ptr<MockAILanguageModel> mock_remote_;
};

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

}  // namespace blink
