// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/language_model_prompt_builder.h"

#include <variant>

#include "base/feature_list.h"
#include "base/values.h"
#include "media/base/audio_bus.h"
#include "services/on_device_model/public/cpp/features.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom-blink-forward.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_create_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_message.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_message_content.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_tool_error.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_tool_result_content.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_tool_success.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_blob_htmlcanvaselement_htmlimageelement_htmlvideoelement_imagebitmap_imagedata_offscreencanvas_svgimageelement_videoframe.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_language_model_message_value.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_language_model_prompt.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_languagemodelmessagecontentsequence_string.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/fileapi/file_reader_client.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/modules/ai/ai_utils.h"
#include "third_party/blink/renderer/modules/ai/language_model.h"
#include "third_party/blink/renderer/modules/ai/language_model_tool_error.h"
#include "third_party/blink/renderer/modules/ai/language_model_tool_success.h"
#include "third_party/blink/renderer/modules/canvas/imagebitmap/image_bitmap_factories.h"
#include "third_party/blink/renderer/modules/canvas/imagebitmap/image_bitmap_source_util.h"
#include "third_party/blink/renderer/modules/webaudio/audio_buffer.h"
#include "third_party/blink/renderer/platform/audio/audio_bus.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"

namespace blink {
namespace {

constexpr char kSchemaPrefix[] =
    "\n\nRemember to respond in JSON that follows this \"JSON Schema\" "
    "specification:\n";

using ResolveCallback =
    base::OnceCallback<void(Vector<mojom::blink::AILanguageModelPromptPtr>)>;
using RejectCallback = base::OnceCallback<void(const ScriptValue& error)>;

LanguageModelMessageContent* MakeMessageTextContent(const String& value) {
  auto* content = MakeGarbageCollected<LanguageModelMessageContent>();
  content->setType(
      V8LanguageModelMessageType(V8LanguageModelMessageType::Enum::kText));
  content->setValue(MakeGarbageCollected<V8LanguageModelMessageValue>(value));
  return content;
}

// Normalizes a prompt message into a list of canonical
// LanguageModelMessageContent. Shorthand styles are converted into canonical
// formats. Adds an empty text content in place of empty sequences.
HeapVector<Member<LanguageModelMessageContent>> NormalizePrompt(
    const LanguageModelMessage* message) {
  V8UnionLanguageModelMessageContentSequenceOrString* content =
      message->content();
  CHECK(content);
  if (content->IsLanguageModelMessageContentSequence()) {
    auto content_list = content->GetAsLanguageModelMessageContentSequence();
    if (content_list.empty()) {
      content_list.push_back(MakeMessageTextContent(g_empty_string));
    }
    return content_list;
  }
  CHECK(content->IsString());
  HeapVector<Member<LanguageModelMessageContent>> content_list;
  content_list.push_back(MakeMessageTextContent(content->GetAsString()));
  return content_list;
}

// Normalizes a prompt into a list of canonical LanguageModelMessage. Shorthand
// styles are converted into canonical formats. Yields empty user-role text
// content for empty message sequences.
HeapVector<Member<LanguageModelMessage>> NormalizePrompt(
    const V8LanguageModelPrompt* prompt) {
  CHECK(prompt);
  if (prompt->IsLanguageModelMessageSequence()) {
    auto messages = prompt->GetAsLanguageModelMessageSequence();
    if (messages.empty()) {
      auto* message = MakeGarbageCollected<LanguageModelMessage>();
      message->setRole(
          V8LanguageModelMessageRole(V8LanguageModelMessageRole::Enum::kUser));
      message->setContent(MakeGarbageCollected<
                          V8UnionLanguageModelMessageContentSequenceOrString>(
          g_empty_string));
      messages.push_back(std::move(message));
    }
    for (const auto& message : messages) {
      message->setContent(MakeGarbageCollected<
                          V8UnionLanguageModelMessageContentSequenceOrString>(
          NormalizePrompt(message)));
    }
    return messages;
  }
  CHECK(prompt->IsString());
  HeapVector<Member<LanguageModelMessageContent>> content_list;
  content_list.push_back(MakeMessageTextContent(prompt->GetAsString()));

  HeapVector<Member<LanguageModelMessage>> messages;
  auto* message = MakeGarbageCollected<LanguageModelMessage>();
  message->setRole(
      V8LanguageModelMessageRole(V8LanguageModelMessageRole::Enum::kUser));
  message->setContent(
      MakeGarbageCollected<V8UnionLanguageModelMessageContentSequenceOrString>(
          std::move(content_list)));
  messages.push_back(std::move(message));
  return messages;
}

// Converts a LanguageModelToolSuccess to DictValue for mojo transport.
// Format: {"callID":"...","name":"...","result":[...]}
// Returns std::nullopt on error.
std::optional<base::DictValue> ConvertToolSuccessToDictValue(
    ScriptState* script_state,
    const LanguageModelToolSuccess* tool_success) {
  base::DictValue dict;
  dict.Set("callID", tool_success->callID().Utf8());
  dict.Set("name", tool_success->name().Utf8());

  // Convert the result array to base::Value.
  base::ListValue result_list;
  const auto& result_items = tool_success->result();

  // Create V8 value converter for direct V8 to base::Value conversion.
  std::unique_ptr<WebV8ValueConverter> converter =
      Platform::Current()->CreateWebV8ValueConverter();

  ScriptState::Scope scope(script_state);
  v8::Local<v8::Context> context = script_state->GetContext();

  for (const auto& result_item : result_items) {
    // Each result item can be text, image, audio, or object.
    // TODO(crbug.com/422803232): Properly handle image/audio result types.
    v8::Local<v8::Value> value_v8 = result_item->value().V8Value();

    // Detect if a DOM object (API wrapper) is being passed with wrong type.
    // API wrappers like ImageBitmap/AudioBuffer get converted to empty dict {}.
    bool is_api_wrapper =
        value_v8->IsObject() && value_v8.As<v8::Object>()->IsApiWrapper();
    if (is_api_wrapper &&
        result_item->type() == V8LanguageModelToolResultType::Enum::kObject) {
      return std::nullopt;
    }

    std::unique_ptr<base::Value> converted_value =
        converter->FromV8Value(value_v8, context);
    // V8ValueConverter returns nullptr for unsupported types, or a valid
    // base::Value that may contain Type::NONE for circular references or
    // unsupported types nested within the structure.
    if (!converted_value || ContainsNoneType(*converted_value)) {
      return std::nullopt;
    }

    result_list.Append(std::move(*converted_value));
  }

  dict.Set("result", std::move(result_list));
  return dict;
}

// Converts a LanguageModelToolError to DictValue for mojo transport.
// Format: {"callID":"...","name":"...","errorMessage":"..."}
base::DictValue ConvertToolErrorToDictValue(
    const LanguageModelToolError* tool_error) {
  base::DictValue dict;
  dict.Set("callID", tool_error->callID().Utf8());
  dict.Set("name", tool_error->name().Utf8());
  dict.Set("errorMessage", tool_error->errorMessage().Utf8());

  return dict;
}

// Helper class for converting types and managing async processing.
class LanguageModelPromptBuilder
    : public GarbageCollected<LanguageModelPromptBuilder>,
      public ContextLifecycleObserver {
 public:
  explicit LanguageModelPromptBuilder(
      ScriptState* script_state,
      AbortSignal* abort_signal,
      const blink::mojom::blink::AILanguageModelInstanceInfoPtr& info,
      const V8LanguageModelPrompt* input,
      const String& json_schema,
      ResolveCallback resolve_callback,
      RejectCallback reject_callback);
  void Trace(Visitor*) const override;
  void ContextDestroyed() override;

 private:
  // Contains some metadata about a input being processed.
  struct PendingEntry : public GarbageCollected<PendingEntry> {
    PendingEntry() = delete;
    explicit PendingEntry(LanguageModelMessageContent* content,
                          size_t message_index,
                          size_t content_index)
        : content(content),
          message_index(message_index),
          content_index(content_index) {}
    void Trace(Visitor* visitor) const { visitor->Trace(content); }
    Member<LanguageModelMessageContent> content;
    size_t message_index = 0;
    size_t content_index = 0;
  };
  void Build(const V8LanguageModelPrompt* input);
  // Called to reject the promise and return an error to the callback.
  void Reject(DOMException* value);
  void Reject(ScriptValue value);
  // Resolve the promise and invoke the callback with `processed_prompts_`.
  void Resolve();
  void Cleanup();

  // Callback when an entry is finished processing.
  void OnPromptContentProcessed(
      mojom::blink::AILanguageModelPromptContentPtr content,
      PendingEntry* entry);
  // Process an entry asynchronously. OnPromptContentProcessed() will be
  // called when finished, or Reject() on failure.
  void ProcessEntry(PendingEntry* pending_entry);

  // ToMojo converts various V8 types to AILanguageModelPromptContent and calls
  // OnPromptContentProcessed() when finished, or Reject() on failure.
  void ToMojo(String prompt, PendingEntry* entry);
  void ToMojo(AudioBuffer* audio_buffer, PendingEntry* entry);
  void ToMojo(Blob* blob, PendingEntry* entry);
  void AudioToMojo(base::span<uint8_t> bytes, PendingEntry* entry);
  void BitmapToMojo(std::variant<DOMDataView*, V8ImageBitmapSource*> source,
                    PendingEntry* entry);

  // Processes tool response (ToolSuccess or ToolError) and calls
  // OnPromptContentProcessed() when finished, or Reject() on failure.
  void ProcessToolResponse(V8LanguageModelMessageValue* content_value,
                           PendingEntry* entry);

  // Called when an ImageBitmap is finished decoding.
  void OnBitmapLoaded(PendingEntry* entry,
                      ScriptState* script_state,
                      ImageBitmap* bitmap);

  SelfKeepAlive<LanguageModelPromptBuilder> keep_alive_{this};
  Vector<mojom::blink::AILanguageModelPromptPtr> processed_prompts_;

  int processed_remaining_ = 0;
  Member<ScriptState> script_state_;
  Member<AbortSignal> abort_signal_;
  blink::mojom::blink::AILanguageModelInstanceInfoPtr info_;
  String json_schema_;

  ResolveCallback resolve_callback_;
  RejectCallback reject_callback_;
};

LanguageModelPromptBuilder::LanguageModelPromptBuilder(
    ScriptState* script_state,
    AbortSignal* abort_signal,
    const blink::mojom::blink::AILanguageModelInstanceInfoPtr& info,
    const V8LanguageModelPrompt* input,
    const String& json_schema,
    ResolveCallback resolve_callback,
    RejectCallback reject_callback)
    : script_state_(script_state),
      abort_signal_(abort_signal),
      info_(info.Clone()),
      json_schema_(json_schema),
      resolve_callback_(std::move(resolve_callback)),
      reject_callback_(std::move(reject_callback)) {
  SetContextLifecycleNotifier(ExecutionContext::From(script_state));
  Build(input);
}

void LanguageModelPromptBuilder::Reject(DOMException* value) {
  Reject(ScriptValue::From(script_state_, value));
}

void LanguageModelPromptBuilder::Reject(ScriptValue value) {
  if (resolve_callback_.is_null() || reject_callback_.is_null() ||
      !script_state_->ContextIsValid()) {
    return;
  }
  std::move(reject_callback_).Run(value);
  Cleanup();
}

void LanguageModelPromptBuilder::Resolve() {
  if (resolve_callback_.is_null() || reject_callback_.is_null() ||
      !script_state_->ContextIsValid()) {
    return;
  }
  if (!json_schema_.empty()) {
    // Make sure the last prompt is a user prompt that the schema instructions
    // get appended to.
    mojom::blink::AILanguageModelPromptPtr prefix_prompt;
    // Pop the prefix prompt to make sure it will always be at the end.
    if (!processed_prompts_.empty() && processed_prompts_.back()->is_prefix) {
      prefix_prompt = std::move(processed_prompts_.back());
      processed_prompts_.pop_back();
    }
    if (processed_prompts_.empty() ||
        processed_prompts_.back()->role !=
            mojom::blink::AILanguageModelPromptRole::kUser) {
      auto prompt = mojom::blink::AILanguageModelPrompt::New();
      prompt->role = mojom::blink::AILanguageModelPromptRole::kUser;
      processed_prompts_.push_back(std::move(prompt));
    }
    processed_prompts_.back()->content.push_back(
        mojom::blink::AILanguageModelPromptContent::NewText(
            StrCat({kSchemaPrefix, json_schema_})));
    if (prefix_prompt) {
      processed_prompts_.push_back(std::move(prefix_prompt));
    }
  }
  std::move(resolve_callback_).Run(std::move(processed_prompts_));
  Cleanup();
}

void LanguageModelPromptBuilder::Cleanup() {
  keep_alive_.Clear();
}

void LanguageModelPromptBuilder::OnPromptContentProcessed(
    mojom::blink::AILanguageModelPromptContentPtr content,
    PendingEntry* entry) {
  if (resolve_callback_.is_null() || reject_callback_.is_null() ||
      !script_state_->ContextIsValid()) {
    return;
  }
  CHECK(entry);
  mojom::blink::AILanguageModelPromptContentPtr& dest_ptr =
      processed_prompts_.at(entry->message_index)
          ->content.at(entry->content_index);
  CHECK(!dest_ptr);  // Each slot should only be written once.
  dest_ptr = std::move(content);
  if (!reject_callback_.is_null() && --processed_remaining_ == 0) {
    Resolve();
  }
}

void LanguageModelPromptBuilder::Build(const V8LanguageModelPrompt* input) {
  CHECK_EQ(processed_remaining_, 0);  // Prevent parallel Build() calls.
  HeapVector<Member<LanguageModelMessage>> messages = NormalizePrompt(input);
  // NormalizePrompt ensures that the messages list is never empty.
  CHECK(!messages.empty());
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      ExecutionContext::From(script_state_)
          ->GetTaskRunner(TaskType::kInternalDefault);
  // In order to maintain the ordering of the messages and their respective
  // content pre-allocate, the message and content lists.
  processed_prompts_.reserve(messages.size());
  for (const auto& message : messages) {
    // Should be normalized to just contain content list.
    CHECK(message->content()->IsLanguageModelMessageContentSequence());

    HeapVector<Member<LanguageModelMessageContent>> content_sequence =
        message->content()->GetAsLanguageModelMessageContentSequence();
    // Normalization ensures every message has at least one content entry.
    CHECK(!content_sequence.empty());
    processed_remaining_ += content_sequence.size();
    // Pre-allocate the mojo struct and content array.
    processed_prompts_.push_back(mojom::blink::AILanguageModelPrompt::New(
        LanguageModel::ConvertRoleToMojo(message->role()),
        Vector<mojom::blink::AILanguageModelPromptContentPtr>(
            content_sequence.size()),
        message->prefix()));

    bool is_multimodal = false;
    for (const auto& content : content_sequence) {
      V8LanguageModelMessageType::Enum content_type = content->type().AsEnum();
      if (content_type == V8LanguageModelMessageType::Enum::kImage ||
          content_type == V8LanguageModelMessageType::Enum::kAudio) {
        is_multimodal = true;
        break;
      }
    }
    if (is_multimodal) {
      if (!RuntimeEnabledFeatures::AIPromptAPIMultimodalInputEnabled(
              ExecutionContext::From(script_state_))) {
        v8::Isolate* isolate = script_state_->GetIsolate();
        Reject(ScriptValue(isolate, V8ThrowException::CreateTypeError(
                                        isolate, "Input type not supported")));
        return;
      }
      if (message->role() == V8LanguageModelMessageRole::Enum::kAssistant) {
        Reject(DOMException::Create(
            "Multimodal input is not supported for the assistant role.",
            DOMException::GetErrorName(DOMExceptionCode::kNotSupportedError)));
        return;
      }
    }
    if (message->prefix()) {
      if (!RuntimeEnabledFeatures::AIPromptAPIMultimodalInputEnabled(
              ExecutionContext::From(script_state_))) {
        Reject(DOMException::Create(
            "Assistant response prefix is not supported.",
            DOMException::GetErrorName(DOMExceptionCode::kNotSupportedError)));
        return;
      }
      if (message->role() != V8LanguageModelMessageRole::Enum::kAssistant) {
        Reject(DOMException::Create(
            "Assistant response prefix must use the assistant role.",
            DOMException::GetErrorName(DOMExceptionCode::kSyntaxError)));
        return;
      }
      if (message != messages.back()) {
        Reject(DOMException::Create(
            "Assistant response prefix must be the last message.",
            DOMException::GetErrorName(DOMExceptionCode::kSyntaxError)));
        return;
      }
    }
  }

  size_t message_index = 0;
  for (const auto& message : messages) {
    auto content_sequence =
        message->content()->GetAsLanguageModelMessageContentSequence();
    CHECK(!content_sequence.empty());
    size_t content_index = 0;
    for (const auto& content : content_sequence) {
      CHECK(content);
      // Track the pre-allocated message and content slot that the resulting
      // content will be written to.
      PendingEntry* pending_entry = MakeGarbageCollected<PendingEntry>(
          content, message_index, content_index++);
      task_runner->PostTask(
          FROM_HERE,
          BindOnce(&LanguageModelPromptBuilder::ProcessEntry,
                   WrapPersistent(this), WrapPersistent(pending_entry)));
    }
    message_index++;
  }
  // As every message is guaranteed to have at least one content entry,
  // processed_remaining_ will always be greater than 0 here.
  CHECK_GT(processed_remaining_, 0);
}

void LanguageModelPromptBuilder::Trace(Visitor* visitor) const {
  visitor->Trace(script_state_);
  visitor->Trace(abort_signal_);
  ContextLifecycleObserver::Trace(visitor);
}

void LanguageModelPromptBuilder::ContextDestroyed() {
  Cleanup();
}

void LanguageModelPromptBuilder::ProcessEntry(PendingEntry* pending_entry) {
  if (!script_state_->ContextIsValid()) {
    return;
  }
  ScriptState::Scope scope(script_state_);
  LanguageModelMessageContent* content = pending_entry->content;
  V8LanguageModelMessageValue* content_value = content->value();
  switch (content->type().AsEnum()) {
    case V8LanguageModelMessageType::Enum::kText:
      if (!content_value->IsString()) {
        // TODO(crbug.com/409615288): Throw a TypeError to match the explainer.
        Reject(DOMException::Create(
            "The value must be a String for type:'text'",
            DOMException::GetErrorName(DOMExceptionCode::kSyntaxError)));
        return;
      }
      ToMojo(content_value->GetAsString(), pending_entry);
      return;
    case V8LanguageModelMessageType::Enum::kImage: {
      if (!info_->input_types ||
          !info_->input_types->Contains(
              mojom::blink::AILanguageModelPromptType::kImage)) {
        Reject(DOMException::Create(
            "Image not supported. Session is not initialized with image "
            "support.",
            DOMException::GetErrorName(DOMExceptionCode::kNotSupportedError)));
        return;
      }

      UseCounter::Count(ExecutionContext::From(script_state_),
                        WebFeature::kLanguageModel_Prompt_Input_Image);
      if (content_value->IsV8ImageBitmapSource()) {
        BitmapToMojo(content_value->GetAsV8ImageBitmapSource(), pending_entry);
        return;
      }
      if (content_value->IsArrayBuffer()) {
        DOMArrayBuffer* array_buffer = content_value->GetAsArrayBuffer();
        BitmapToMojo(
            DOMDataView::Create(array_buffer, 0, array_buffer->ByteLength()),
            pending_entry);
        return;
      }
      if (content_value->IsArrayBufferView()) {
        NotShared<DOMArrayBufferView> array_buffer_view =
            content_value->GetAsArrayBufferView();
        BitmapToMojo(DOMDataView::Create(array_buffer_view->BufferBase(),
                                         array_buffer_view->byteOffset(),
                                         array_buffer_view->byteLength()),
                     pending_entry);
        return;
      }
      Reject(DOMException::Create(
          "Unsupported image type",
          DOMException::GetErrorName(DOMExceptionCode::kSyntaxError)));
      return;
    }
    case V8LanguageModelMessageType::Enum::kAudio: {
      if (!info_->input_types ||
          !info_->input_types->Contains(
              mojom::blink::AILanguageModelPromptType::kAudio)) {
        Reject(DOMException::Create(
            "Audio not supported. Session is not initialized with audio "
            "support.",
            DOMException::GetErrorName(DOMExceptionCode::kNotSupportedError)));
        return;
      }
      UseCounter::Count(ExecutionContext::From(script_state_),
                        WebFeature::kLanguageModel_Prompt_Input_Audio);
      switch (content_value->GetContentType()) {
        case V8LanguageModelMessageValue::ContentType::kAudioBuffer:
          ToMojo(content_value->GetAsAudioBuffer(), pending_entry);
          return;
        case V8LanguageModelMessageValue::ContentType::kBlob:
          ToMojo(content_value->GetAsBlob(), pending_entry);
          return;
        case V8LanguageModelMessageValue::ContentType::kArrayBuffer:
          AudioToMojo(content_value->GetAsArrayBuffer()->Content()->ByteSpan(),
                      pending_entry);
          return;
        case V8LanguageModelMessageValue::ContentType::kArrayBufferView:
          AudioToMojo(content_value->GetAsArrayBufferView()->ByteSpan(),
                      pending_entry);
          return;
        default:
          Reject(DOMException::Create(
              "Unsupported audio content type",
              DOMException::GetErrorName(DOMExceptionCode::kSyntaxError)));
          return;
      }
    }
    case V8LanguageModelMessageType::Enum::kToolCall:
      // Tool calls are generated by the model, not provided as input.
      // TODO(crbug.com/422803232): Maybe allow kToolCall from input.
      Reject(DOMException::Create(
          "Tool calls cannot be provided as input. Tool calls are generated "
          "by the model, not provided to it.",
          DOMException::GetErrorName(DOMExceptionCode::kNotSupportedError)));
      return;
    case V8LanguageModelMessageType::Enum::kToolResponse: {
      if (!info_->input_types ||
          !info_->input_types->Contains(
              mojom::blink::AILanguageModelPromptType::kToolResponse)) {
        Reject(DOMException::Create(
            "Tool responses not supported. Session is not initialized with "
            "tool support.",
            DOMException::GetErrorName(DOMExceptionCode::kNotSupportedError)));
        return;
      }
      ProcessToolResponse(content_value, pending_entry);
      return;
    }
  }
}

void LanguageModelPromptBuilder::ProcessToolResponse(
    V8LanguageModelMessageValue* content_value,
    PendingEntry* entry) {
  LanguageModelToolSuccess* tool_success = nullptr;
  LanguageModelToolError* tool_error = nullptr;

  if (content_value->IsLanguageModelToolSuccess()) {
    tool_success = content_value->GetAsLanguageModelToolSuccess();
  } else if (content_value->IsLanguageModelToolError()) {
    tool_error = content_value->GetAsLanguageModelToolError();
  } else {
    // Neither ToolSuccess nor ToolError.
    Reject(DOMException::Create(
        "The value must be a LanguageModelToolSuccess or "
        "LanguageModelToolError for type:'tool-response'",
        DOMException::GetErrorName(DOMExceptionCode::kSyntaxError)));
    return;
  }

  // JavaScript manually sends tool execution results back.
  if (tool_success) {
    // TODO(crbug.com/422803232): Properly handle image/audio result types.
    for (const auto& result_item : tool_success->result()) {
      V8LanguageModelToolResultType result_type = result_item->type();
      if (result_type == V8LanguageModelToolResultType::Enum::kImage ||
          result_type == V8LanguageModelToolResultType::Enum::kAudio) {
        Reject(DOMException::Create(
            "Tool responses currently only support 'text' and 'object' "
            "types. Image and audio types in tool responses are not yet "
            "supported.",
            DOMException::GetErrorName(DOMExceptionCode::kNotSupportedError)));
        return;
      }
    }

    // Convert to base::DictValue for mojo transport.
    std::optional<base::DictValue> converted_value =
        ConvertToolSuccessToDictValue(script_state_, tool_success);
    if (!converted_value.has_value()) {
      Reject(DOMException::Create(
          "Failed to serialize tool result. Value may contain circular "
          "references, non-serializable types (functions, BigInt), or DOM "
          "objects (ImageBitmap, AudioBuffer) incorrectly labeled as "
          "'object' type.",
          DOMException::GetErrorName(DOMExceptionCode::kDataError)));
      return;
    }

    OnPromptContentProcessed(
        mojom::blink::AILanguageModelPromptContent::NewToolResponse(
            std::move(*converted_value)),
        entry);
  } else {
    base::DictValue converted_value = ConvertToolErrorToDictValue(tool_error);
    OnPromptContentProcessed(
        mojom::blink::AILanguageModelPromptContent::NewToolResponse(
            std::move(converted_value)),
        entry);
  }
}

void LanguageModelPromptBuilder::ToMojo(String prompt, PendingEntry* entry) {
  OnPromptContentProcessed(
      mojom::blink::AILanguageModelPromptContent::NewText(prompt), entry);
}

void LanguageModelPromptBuilder::ToMojo(AudioBuffer* audio_buffer,
                                        PendingEntry* entry) {
  VLOG(1) << "AudioBuffer " << audio_buffer->numberOfChannels() << " channels,"
          << audio_buffer->sampleRate() << "Hz, len:" << audio_buffer->length();
  scoped_refptr<AudioBus> bus =
      AudioBus::Create(audio_buffer->numberOfChannels(), audio_buffer->length(),
                       /*allocate=*/false);
  bus->SetSampleRate(audio_buffer->sampleRate());
  for (unsigned i = 0; i < audio_buffer->numberOfChannels(); ++i) {
    if (audio_buffer->getChannelData(i)->length() != audio_buffer->length()) {
      Reject(DOMException::Create(
          "AudioBuffer channel data length mismatch.",
          DOMException::GetErrorName(DOMExceptionCode::kNotSupportedError)));
      return;
    }
    bus->SetChannelMemory(i, audio_buffer->getChannelData(i)->AsSpan());
  }

  auto audio_data = on_device_model::mojom::blink::AudioData::New();
  // TODO(crbug.com/476192657) Remove hardcoded values after we initiate ODMS
  // session earlier.
  audio_data->sample_rate = info_->audio_sample_rate_hz.value_or(
      base::FeatureList::IsEnabled(
          on_device_model::features::kOnDeviceModelLitertLmBackend)
          ? 16000
          : 48000);
  audio_data->channel_count = info_->audio_channel_count.value_or(1);
  CHECK_EQ(audio_data->channel_count, 1) << "Multi-channel audio not supported";
  const bool mix_to_mono = audio_buffer->numberOfChannels() > 1u;
  if (audio_data->sample_rate != audio_buffer->sampleRate() || mix_to_mono) {
    bus = AudioBus::TryCreateBySampleRateConverting(bus.get(), mix_to_mono,
                                                    audio_data->sample_rate);
    if (!bus) {
      Reject(DOMException::Create(
          "Failed to convert audio data (not enough memory).",
          DOMException::GetErrorName(DOMExceptionCode::kNotSupportedError)));
      return;
    }
    VLOG(1) << "Converted to " << bus->NumberOfChannels() << " channels, "
            << bus->SampleRate() << "Hz, len:" << bus->length();
  }
  // TODO(crbug.com/382180351): Avoid a copy.
  audio_data->frame_count = bus->length();
  audio_data->data.append_range(bus->Channel(0)->Span());
  OnPromptContentProcessed(mojom::blink::AILanguageModelPromptContent::NewAudio(
                               std::move(audio_data)),
                           entry);
}

void LanguageModelPromptBuilder::AudioToMojo(base::span<uint8_t> bytes,
                                             PendingEntry* entry) {
  auto audio_data = on_device_model::mojom::blink::AudioData::New();
  // TODO(crbug.com/476192657) Remove hardcoded values after we initiate ODMS
  // session earlier.
  audio_data->sample_rate = info_->audio_sample_rate_hz.value_or(
      base::FeatureList::IsEnabled(
          on_device_model::features::kOnDeviceModelLitertLmBackend)
          ? 16000
          : 48000);
  audio_data->channel_count = info_->audio_channel_count.value_or(1);
  CHECK_EQ(audio_data->channel_count, 1) << "Multi-channel audio not supported";
  scoped_refptr<AudioBus> bus = AudioBus::CreateBusFromInMemoryAudioFile(
      bytes, /*mix_to_mono=*/true, audio_data->sample_rate);
  if (!bus || bus->SampleRate() != audio_data->sample_rate ||
      static_cast<int>(bus->NumberOfChannels()) != audio_data->channel_count) {
    // TODO(crbug.com/409615288): This should throw a TypeError according to the
    // spec.
    Reject(DOMException::Create(
        "Missing or invalid audio data.",
        DOMException::GetErrorName(DOMExceptionCode::kDataError)));
    return;
  }
  VLOG(1) << "AudioBus " << bus->NumberOfChannels() << " channels, "
          << bus->SampleRate() << "Hz, len:" << bus->length();
  // TODO(crbug.com/382180351): Avoid a copy.
  audio_data->frame_count = bus->length();
  audio_data->data.append_range(bus->Channel(0)->Span());
  OnPromptContentProcessed(mojom::blink::AILanguageModelPromptContent::NewAudio(
                               std::move(audio_data)),
                           entry);
}

void LanguageModelPromptBuilder::ToMojo(Blob* blob, PendingEntry* entry) {
  // TODO(crbug.com/382180351): Make blob reading async or alternatively
  // use FileReaderSync instead (fix linker and exception issues).
  SyncedFileReaderAccumulator* blobReader =
      MakeGarbageCollected<SyncedFileReaderAccumulator>();

  auto [error_code, reader_data] = blobReader->Load(
      blob->GetBlobDataHandle(), ExecutionContext::From(script_state_)
                                     ->GetTaskRunner(TaskType::kFileReading));
  if (error_code != FileErrorCode::kOK) {
    Reject(DOMException::Create(
        "Failed to read blob.",
        DOMException::GetErrorName(DOMExceptionCode::kDataError)));
    return;
  }
  ArrayBufferContents audio_contents =
      std::move(reader_data).AsArrayBufferContents();
  if (!audio_contents.IsValid()) {
    Reject(DOMException::Create(
        "Failed to read contents of blob.",
        DOMException::GetErrorName(DOMExceptionCode::kDataError)));
    return;
  }
  AudioToMojo(audio_contents.ByteSpan(), entry);
}

// ThenCallable wrapper that calls a callback with the resolved/rejected value.
template <typename Type, typename ReactType = Type*>
class ThenCallback : public ThenCallable<Type, ThenCallback<Type, ReactType>> {
 public:
  explicit ThenCallback(
      base::OnceCallback<void(ScriptState*, ReactType)> callback)
      : callback(std::move(callback)) {}
  void React(ScriptState* script_state, ReactType value) {
    std::move(callback).Run(script_state, value);
  }
  base::OnceCallback<void(ScriptState*, ReactType)> callback;
};

void LanguageModelPromptBuilder::BitmapToMojo(
    std::variant<DOMDataView*, V8ImageBitmapSource*> source,
    PendingEntry* entry) {
  v8::Isolate* isolate = script_state_->GetIsolate();
  v8::TryCatch try_catch(isolate);
  ExceptionState exception_state(isolate);

  // Note: GetBitmapFromV8ImageBitmapSource doesn't support async which is
  // required for blobs so async ImageBitmapFactories::CreateImageBitmap is
  // preferred.
  // TODO(crbug.com/419321438): Change CreateImageBitmap to not use JS promises.
  ScriptPromise<ImageBitmap> promise = std::visit(
      absl::Overload{
          [&](const DOMDataView* data_view) {
            return ImageBitmapFactories::CreateImageBitmap(
                script_state_, data_view,
                MakeGarbageCollected<ImageBitmapOptions>(), exception_state);
          },
          [&](const V8ImageBitmapSource* bitmap_source) {
            return ImageBitmapFactories::CreateImageBitmap(
                script_state_, bitmap_source,
                MakeGarbageCollected<ImageBitmapOptions>(), exception_state);
          }},
      source);

  promise.Then(
      script_state_,
      MakeGarbageCollected<ThenCallback<ImageBitmap>>(
          BindOnce(&LanguageModelPromptBuilder::OnBitmapLoaded,
                   WrapPersistent(this), WrapPersistent(entry))),
      MakeGarbageCollected<ThenCallback<IDLAny, ScriptValue>>(BindOnce(
          [](LanguageModelPromptBuilder* builder, ScriptState* script_state,
             ScriptValue value) { builder->Reject(std::move(value)); },
          WrapPersistent(this))));
  if (exception_state.HadException()) {
    CHECK(try_catch.HasCaught());
    this->Reject(ScriptValue(isolate, try_catch.Exception()));
  }
}

void LanguageModelPromptBuilder::OnBitmapLoaded(PendingEntry* entry,
                                                ScriptState* script_state,
                                                ImageBitmap* bitmap) {
  v8::Isolate* isolate = script_state->GetIsolate();
  v8::TryCatch try_catch(isolate);
  ExceptionState exception_state(isolate);
  if (!bitmap) {
    Reject(DOMException::Create(
        "Invalid image bitmap.",
        DOMException::GetErrorName(DOMExceptionCode::kDataError)));
    return;
  }
  std::optional<SkBitmap> skia_bitmap =
      GetBitmapFromCanvasImageSource(*bitmap, exception_state);
  if (!skia_bitmap) {
    CHECK(exception_state.HadException() && try_catch.HasCaught());
    Reject(ScriptValue(isolate, try_catch.Exception()));
    return;
  }
  OnPromptContentProcessed(
      mojom::blink::AILanguageModelPromptContent::NewBitmap(
          skia_bitmap.value()),
      entry);
}

}  // namespace

void ConvertPromptInputsToMojo(
    ScriptState* script_state,
    AbortSignal* abort_signal,
    const V8LanguageModelPrompt* input,
    const blink::mojom::blink::AILanguageModelInstanceInfoPtr& info,
    const String& json_schema,
    ResolveCallback resolve_callback,
    RejectCallback reject_callback) {
  MakeGarbageCollected<LanguageModelPromptBuilder>(
      script_state, abort_signal, info, input, json_schema,
      std::move(resolve_callback), std::move(reject_callback));
}

}  // namespace blink
