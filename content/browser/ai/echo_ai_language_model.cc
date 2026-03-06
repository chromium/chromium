// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/ai/echo_ai_language_model.h"

#include <optional>
#include <string_view>

#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/notimplemented.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "content/browser/ai/echo_ai_manager_impl.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/mojom/ai/ai_common.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom.h"

namespace content {

namespace {

constexpr char kResponsePrefix[] =
    "On-device model is not available in Chromium, this API is just echoing "
    "back the input:\n";
// Trigger prefix for generating mock tool calls in tests.
// When input starts with this prefix, the echo model will generate tool calls
// for all available tools. The prefix itself is stripped from the echoed text.
constexpr std::string_view kGenerateToolCallsTrigger =
    "<GenerateSimpleToolCalls>";
// Trigger prefix for generating multiple batches of tool calls in tests.
// When input starts with this prefix, the echo model will generate two batches
// of tool calls.
constexpr std::string_view kGenerateMultipleToolCallsTrigger =
    "<GenerateMultipleToolCalls>";

uint32_t MeasureUsage(const blink::mojom::AILanguageModelPrompt& prompt) {
  size_t total = 0;
  for (const auto& content : prompt.content) {
    if (content->is_text()) {
      total += content->get_text().size();
    } else {
      total += 100;  // TODO(crbug.com/415304330): Improve estimate.
    }
  }
  return total;
}

uint32_t MeasureUsage(
    const std::vector<blink::mojom::AILanguageModelPromptPtr>& prompts) {
  size_t total = 0;
  for (const auto& prompt : prompts) {
    total += MeasureUsage(*prompt);
  }
  return total;
}

}  // namespace

EchoAILanguageModel::EchoAILanguageModel(
    blink::mojom::AILanguageModelSamplingParamsPtr sampling_params,
    base::flat_set<blink::mojom::AILanguageModelPromptType> input_types,
    std::vector<blink::mojom::AILanguageModelPromptPtr> initial_prompts,
    uint32_t initial_tokens_size,
    std::vector<blink::mojom::AILanguageModelToolDeclarationPtr> tools)
    : current_tokens_(initial_tokens_size),
      sampling_params_(std::move(sampling_params)),
      input_types_(input_types),
      initial_prompts_(std::move(initial_prompts)),
      tools_(std::move(tools)),
      pending_response_(base::StrCat(
          {kResponsePrefix, PromptsToText(initial_prompts_).value_or("")})) {}

EchoAILanguageModel::~EchoAILanguageModel() = default;

void EchoAILanguageModel::DoMockExecution(
    std::vector<blink::mojom::AILanguageModelPromptPtr> prompts,
    bool generate_response,
    mojo::RemoteSetElementId responder_id) {
  blink::mojom::ModelStreamingResponder* responder =
      responder_set_.Get(responder_id);
  if (!responder) {
    return;
  }

  auto prompts_as_string = PromptsToText(prompts);
  if (!prompts_as_string.has_value()) {
    return;
  }

  const auto usage = MeasureUsage(prompts);
  uint32_t context_window = EchoAIManagerImpl::kMaxContextSizeInTokens;
  if (usage > context_window) {
    responder->OnError(
        blink::mojom::ModelStreamingResponseStatus::kErrorInputTooLarge,
        blink::mojom::QuotaErrorInfo::New(usage, context_window));
    return;
  }

  current_tokens_ += usage;
  prompt_history_.insert(prompt_history_.end(),
                         std::make_move_iterator(prompts.begin()),
                         std::make_move_iterator(prompts.end()));
  prompts.clear();

  // Check if input contains the trigger prefix for generating tool calls.
  bool should_generate_tool_calls = false;
  bool should_generate_multiple_batches = false;
  std::string_view text_to_echo = prompts_as_string.value();
  if (!tools_.empty()) {
    if (text_to_echo.starts_with(kGenerateMultipleToolCallsTrigger)) {
      should_generate_tool_calls = true;
      should_generate_multiple_batches = true;
      text_to_echo.remove_prefix(kGenerateMultipleToolCallsTrigger.size());
    } else if (text_to_echo.starts_with(kGenerateToolCallsTrigger)) {
      should_generate_tool_calls = true;
      text_to_echo.remove_prefix(kGenerateToolCallsTrigger.size());
    }
  }
  pending_response_.append(text_to_echo);

  if (generate_response) {
    auto prompt = blink::mojom::AILanguageModelPrompt::New();
    prompt->role = blink::mojom::AILanguageModelPromptRole::kAssistant;
    prompt->content.push_back(
        blink::mojom::AILanguageModelPromptContent::NewText(pending_response_));
    current_tokens_ += MeasureUsage(*prompt);
    prompt_history_.push_back(std::move(prompt));
    responder->OnStreaming(pending_response_);
    pending_response_.clear();

    // Generate tool calls if triggered and tools are available.
    if (should_generate_tool_calls) {
      if (should_generate_multiple_batches) {
        // Split tools into two arbitrary batches to test that the JS side
        // correctly accumulates tool calls across multiple OnToolCalls events.
        std::vector<blink::mojom::ToolCallPtr> first_batch =
            GenerateToolCallsForRange(0, tools_.size() / 2);
        if (!first_batch.empty()) {
          responder->OnToolCalls(std::move(first_batch));
        }

        std::vector<blink::mojom::ToolCallPtr> second_batch =
            GenerateToolCallsForRange(tools_.size() / 2, tools_.size());
        if (!second_batch.empty()) {
          responder->OnToolCalls(std::move(second_batch));
        }
      } else {
        // Single batch: All tools at once.
        std::vector<blink::mojom::ToolCallPtr> tool_calls =
            GenerateSimpleToolCalls();
        if (!tool_calls.empty()) {
          responder->OnToolCalls(std::move(tool_calls));
        }
      }
    }
  }

  // Overflow and prompt history eviction logic.
  bool overflow = false;
  while (current_tokens_ > context_window && !prompt_history_.empty()) {
    auto& prompt = prompt_history_.front();
    current_tokens_ = base::ClampSub(current_tokens_, MeasureUsage(*prompt));
    prompt_history_.erase(prompt_history_.begin());
    overflow = true;
  }
  if (overflow) {
    responder->OnContextOverflow();
  }

  responder->OnCompletion(
      blink::mojom::ModelExecutionContextInfo::New(current_tokens_));
}

void EchoAILanguageModel::Prompt(
    std::vector<blink::mojom::AILanguageModelPromptPtr> prompts,
    on_device_model::mojom::ResponseConstraintPtr constraint,
    mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
        pending_responder) {
  AppendOrPrompt(std::move(prompts), std::move(pending_responder),
                 /*generate_response=*/true);
}

void EchoAILanguageModel::Append(
    std::vector<blink::mojom::AILanguageModelPromptPtr> prompts,
    mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
        pending_responder) {
  AppendOrPrompt(std::move(prompts), std::move(pending_responder),
                 /*generate_response=*/false);
}

void EchoAILanguageModel::AppendOrPrompt(
    std::vector<blink::mojom::AILanguageModelPromptPtr> prompts,
    mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
        pending_responder,
    bool generate_response) {
  if (is_destroyed_) {
    mojo::Remote<blink::mojom::ModelStreamingResponder> responder(
        std::move(pending_responder));
    responder->OnError(
        blink::mojom::ModelStreamingResponseStatus::kErrorSessionDestroyed,
        /*quota_error_info=*/nullptr);
    return;
  }

  mojo::RemoteSetElementId responder_id =
      responder_set_.Add(std::move(pending_responder));
  // Simulate the time taken by model execution.
  content::GetUIThreadTaskRunner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&EchoAILanguageModel::DoMockExecution,
                     weak_ptr_factory_.GetWeakPtr(), std::move(prompts),
                     generate_response, responder_id),
      base::Seconds(1));
}

void EchoAILanguageModel::Fork(
    mojo::PendingRemote<blink::mojom::AIManagerCreateLanguageModelClient>
        client) {
  mojo::Remote<blink::mojom::AIManagerCreateLanguageModelClient> client_remote(
      std::move(client));
  mojo::PendingRemote<blink::mojom::AILanguageModel> language_model;

  // This sessions initial prompts + history is copied into the
  // forked session as initial prompts.
  std::vector<blink::mojom::AILanguageModelPromptPtr> prompts_copy;
  for (const auto& prompt : initial_prompts_) {
    prompts_copy.push_back(prompt.Clone());
  }
  for (const auto& prompt : prompt_history_) {
    prompts_copy.push_back(prompt.Clone());
  }

  // Clone tools for the forked session.
  std::vector<blink::mojom::AILanguageModelToolDeclarationPtr> cloned_tools;
  for (const auto& tool : tools_) {
    cloned_tools.push_back(tool.Clone());
  }

  mojo::MakeSelfOwnedReceiver(
      std::make_unique<EchoAILanguageModel>(
          sampling_params_.Clone(), input_types_, std::move(prompts_copy),
          current_tokens_, std::move(cloned_tools)),
      language_model.InitWithNewPipeAndPassReceiver());
  client_remote->OnResult(
      std::move(language_model),
      blink::mojom::AILanguageModelInstanceInfo::New(
          EchoAIManagerImpl::kMaxContextSizeInTokens, current_tokens_,
          sampling_params_->Clone(), base::ToVector(input_types_),
          /*audio_sample_rate_hz=*/std::nullopt,
          /*audio_channel_count=*/std::nullopt));
}

void EchoAILanguageModel::Destroy() {
  is_destroyed_ = true;

  for (auto& responder : responder_set_) {
    responder->OnError(
        blink::mojom::ModelStreamingResponseStatus::kErrorSessionDestroyed,
        /*quota_error_info=*/nullptr);
  }
  responder_set_.Clear();
}

void EchoAILanguageModel::MeasureInputUsage(
    std::vector<blink::mojom::AILanguageModelPromptPtr> input,
    MeasureInputUsageCallback callback) {
  std::move(callback).Run(MeasureUsage(input));
}

std::optional<std::string> EchoAILanguageModel::PromptsToText(
    const std::vector<blink::mojom::AILanguageModelPromptPtr>& prompts) {
  std::string response = "";
  for (const auto& prompt : prompts) {
    for (auto& content : prompt->content) {
      switch (content->which()) {
        case blink::mojom::AILanguageModelPromptContent::Tag::kText:
          response += content->get_text();
          break;
        case blink::mojom::AILanguageModelPromptContent::Tag::kBitmap:
          if (!input_types_.contains(
                  blink::mojom::AILanguageModelPromptType::kImage)) {
            mojo::ReportBadMessage("Image input is not supported.");
            return std::nullopt;
          }
          response += "<image>";
          break;
        case blink::mojom::AILanguageModelPromptContent::Tag::kAudio:
          if (!input_types_.contains(
                  blink::mojom::AILanguageModelPromptType::kAudio)) {
            mojo::ReportBadMessage("Audio input is not supported.");
            return std::nullopt;
          }
          response += "<audio>";
          break;
        case blink::mojom::AILanguageModelPromptContent::Tag::kToolCall:
          // TODO(crbug.com/380461823): Support tool call input for session
          // history and providing examples in the system prompt.
          mojo::ReportBadMessage("Tool call input is not supported.");
          return std::nullopt;
        case blink::mojom::AILanguageModelPromptContent::Tag::kToolResponse:
          if (!input_types_.contains(
                  blink::mojom::AILanguageModelPromptType::kToolResponse)) {
            mojo::ReportBadMessage("Tool response input is not supported.");
            return std::nullopt;
          }
          response += ExtractToolResponseText(content->get_tool_response());
          break;
        default:
          NOTIMPLEMENTED_LOG_ONCE();
          break;
      }
    }
  }
  return response;
}

std::string EchoAILanguageModel::ExtractToolResponseText(
    const base::DictValue& tool_response_dict) {
  // Extract common fields.
  const std::string* call_id = tool_response_dict.FindString("callID");
  const std::string* name = tool_response_dict.FindString("name");

  // Check if this is a success (has "result") or error (has "errorMessage").
  const base::ListValue* result_list = tool_response_dict.FindList("result");
  const std::string* error_msg = tool_response_dict.FindString("errorMessage");

  if (result_list) {
    // Tool success output format. Example:
    // <tool-success id='call_1' name='get_weather' result=[rainy, 52F]>
    std::string result = "<tool-success";
    if (call_id) {
      base::StrAppend(&result, {" id='", *call_id, "'"});
    }
    if (name) {
      base::StrAppend(&result, {" name='", *name, "'"});
    }
    base::StrAppend(&result, {" result=["});

    for (size_t i = 0; i < result_list->size(); ++i) {
      if (i > 0) {
        base::StrAppend(&result, {", "});
      }
      const base::Value& result_item = (*result_list)[i];

      if (result_item.is_dict()) {
        const base::DictValue& item_dict = result_item.GetDict();
        const std::string* type_str = item_dict.FindString("type");

        // Extract value based on type.
        if (type_str && *type_str == "text") {
          const std::string* text_value = item_dict.FindString("value");
          base::StrAppend(&result, {text_value ? *text_value : "<text>"});
        } else {
          // For other types (image, audio, object), just note the type.
          base::StrAppend(&result,
                          {"<", type_str ? *type_str : "unknown", ">"});
        }
      } else if (result_item.is_string()) {
        base::StrAppend(&result, {result_item.GetString()});
      }
    }
    base::StrAppend(&result, {"]>"});
    return result;
  }

  // Tool error output format. Example:
  // <tool-error id='call_2' name='get_weather' message='Location Not Found'>
  if (error_msg) {
    std::string result = "<tool-error";
    if (call_id) {
      base::StrAppend(&result, {" id='", *call_id, "'"});
    }
    if (name) {
      base::StrAppend(&result, {" name='", *name, "'"});
    }
    base::StrAppend(&result, {" message='", *error_msg, "'>"});
    return result;
  }

  return "<tool-response>";
}

std::optional<base::Value> EchoAILanguageModel::ExtractJsonHint(
    const std::string& description,
    const std::string& hint_marker) {
  // Look for "HintMarker: {...}" pattern in the tool description.
  size_t marker_pos = description.find(hint_marker);
  if (marker_pos == std::string::npos) {
    return std::nullopt;  // Marker not found.
  }

  // Find the JSON object starting position.
  size_t json_start = marker_pos + hint_marker.length();
  if (json_start >= description.length() || description[json_start] != '{') {
    return std::nullopt;  // Invalid format - no JSON object after marker.
  }

  // Find the closing brace for the JSON object.
  // This assumes no nested objects - finds the first '}' after the opening
  // '{'. This is sufficient for simple hint structures like Args and
  // ToolCallResponsePrefix.
  size_t json_end = description.find('}', json_start);
  if (json_end == std::string::npos || json_end <= json_start) {
    return std::nullopt;  // No valid closing brace found.
  }

  // Extract potential JSON substring.
  std::string_view candidate =
      std::string_view(description)
          .substr(json_start, json_end - json_start + 1);

  // Use base::JSONReader to validate and parse the JSON.
  auto parsed = base::JSONReader::Read(candidate, base::JSON_PARSE_RFC);
  if (!parsed.has_value() || !parsed->is_dict()) {
    return std::nullopt;  // Invalid JSON or not an object.
  }

  return std::move(*parsed);
}

base::DictValue EchoAILanguageModel::ExtractArgumentHints(
    const std::string& description) {
  std::optional<base::Value> args_json = ExtractJsonHint(description, "Args: ");
  if (args_json.has_value()) {
    return std::move(args_json->GetDict());
  }
  return base::DictValue();
}

std::vector<blink::mojom::ToolCallPtr>
EchoAILanguageModel::GenerateSimpleToolCalls() {
  // Generate tool calls for all available tools using argument hints from
  // descriptions. If no hints are provided, use empty arguments to let test
  // fail if needed.
  return GenerateToolCallsForRange(0, tools_.size());
}

std::vector<blink::mojom::ToolCallPtr>
EchoAILanguageModel::GenerateToolCallsForRange(size_t start_index,
                                               size_t end_index) {
  std::vector<blink::mojom::ToolCallPtr> tool_calls;

  CHECK_LE(start_index, end_index);
  CHECK_LE(end_index, tools_.size());

  for (size_t i = start_index; i < end_index; ++i) {
    const auto& tool = tools_[i];
    auto tool_call = blink::mojom::ToolCall::New();
    tool_call->call_id = base::StringPrintf("call_%d", static_cast<int>(i) + 1);
    tool_call->name = tool->name;

    tool_call->arguments = ExtractArgumentHints(tool->description);
    tool_calls.push_back(std::move(tool_call));
  }
  return tool_calls;
}

}  // namespace content
