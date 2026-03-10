// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/public/cpp/chrome_ml_types_mojom_traits.h"

#include <algorithm>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/numerics/checked_math.h"
#include "base/values.h"

namespace mojo {

// static
on_device_model::mojom::Token
EnumTraits<on_device_model::mojom::Token, ml::Token>::ToMojom(ml::Token input) {
  switch (input) {
    case ml::Token::kSystem:
      return on_device_model::mojom::Token::kSystem;
    case ml::Token::kModel:
      return on_device_model::mojom::Token::kModel;
    case ml::Token::kUser:
      return on_device_model::mojom::Token::kUser;
    case ml::Token::kEnd:
      return on_device_model::mojom::Token::kEnd;
    case ml::Token::kToolCall:
      return on_device_model::mojom::Token::kToolCall;
    case ml::Token::kToolResponse:
      return on_device_model::mojom::Token::kToolResponse;
  }
  NOTREACHED();
}

// static
bool EnumTraits<on_device_model::mojom::Token, ml::Token>::FromMojom(
    on_device_model::mojom::Token input,
    ml::Token* output) {
  switch (input) {
    case on_device_model::mojom::Token::kSystem:
      *output = ml::Token::kSystem;
      return true;
    case on_device_model::mojom::Token::kModel:
      *output = ml::Token::kModel;
      return true;
    case on_device_model::mojom::Token::kUser:
      *output = ml::Token::kUser;
      return true;
    case on_device_model::mojom::Token::kEnd:
      *output = ml::Token::kEnd;
      return true;
    case on_device_model::mojom::Token::kToolCall:
      *output = ml::Token::kToolCall;
      return true;
    case on_device_model::mojom::Token::kToolResponse:
      *output = ml::Token::kToolResponse;
      return true;
  }
  return false;
}

// static
on_device_model::mojom::InputPieceDataView::Tag
UnionTraits<on_device_model::mojom::InputPieceDataView, ml::InputPiece>::GetTag(
    const ml::InputPiece& input_piece) {
  if (std::holds_alternative<ml::Token>(input_piece)) {
    return on_device_model::mojom::InputPieceDataView::Tag::kToken;
  }
  if (std::holds_alternative<std::string>(input_piece)) {
    return on_device_model::mojom::InputPieceDataView::Tag::kText;
  }
  if (std::holds_alternative<SkBitmap>(input_piece)) {
    return on_device_model::mojom::InputPieceDataView::Tag::kBitmap;
  }
  if (std::holds_alternative<ml::AudioBuffer>(input_piece)) {
    return on_device_model::mojom::InputPieceDataView::Tag::kAudio;
  }
  if (std::holds_alternative<ml::ToolDeclaration>(input_piece)) {
    return on_device_model::mojom::InputPieceDataView::Tag::kToolDeclaration;
  }
  if (std::holds_alternative<ml::ToolResponse>(input_piece)) {
    return on_device_model::mojom::InputPieceDataView::Tag::kToolResponse;
  }
  if (std::holds_alternative<bool>(input_piece)) {
    return on_device_model::mojom::InputPieceDataView::Tag::kUnknownType;
  }
  NOTREACHED();
}

// static
bool UnionTraits<on_device_model::mojom::InputPieceDataView, ml::InputPiece>::
    Read(on_device_model::mojom::InputPieceDataView in, ml::InputPiece* out) {
  switch (in.tag()) {
    case on_device_model::mojom::InputPieceDataView::Tag::kToken: {
      ml::Token token;
      if (!in.ReadToken(&token)) {
        return false;
      }
      *out = token;
      return true;
    }
    case on_device_model::mojom::InputPieceDataView::Tag::kText: {
      std::string text;
      if (!in.ReadText(&text)) {
        return false;
      }
      *out = std::move(text);
      return true;
    }
    case on_device_model::mojom::InputPieceDataView::Tag::kBitmap: {
      SkBitmap bitmap;
      if (!in.ReadBitmap(&bitmap)) {
        return false;
      }
      *out = std::move(bitmap);
      return true;
    }
    case on_device_model::mojom::InputPieceDataView::Tag::kAudio: {
      ml::AudioBuffer audio;
      if (!in.ReadAudio(&audio)) {
        return false;
      }
      *out = std::move(audio);
      return true;
    }
    case on_device_model::mojom::InputPieceDataView::Tag::kToolDeclaration: {
      ml::ToolDeclaration tool_declaration;
      if (!in.ReadToolDeclaration(&tool_declaration)) {
        return false;
      }
      *out = std::move(tool_declaration);
      return true;
    }
    case on_device_model::mojom::InputPieceDataView::Tag::kToolResponse: {
      ml::ToolResponse tool_response;
      if (!in.ReadToolResponse(&tool_response)) {
        return false;
      }
      *out = std::move(tool_response);
      return true;
    }
    case on_device_model::mojom::InputPieceDataView::Tag::kUnknownType: {
      *out = in.unknown_type();
      return true;
    }
  }
  return false;
}

// static
on_device_model::mojom::ModelBackendType
EnumTraits<on_device_model::mojom::ModelBackendType,
           ml::ModelBackendType>::ToMojom(ml::ModelBackendType input) {
  switch (input) {
    case ml::ModelBackendType::kGpuBackend:
      return on_device_model::mojom::ModelBackendType::kGpu;
    case ml::ModelBackendType::kApuBackend:
      return on_device_model::mojom::ModelBackendType::kApu;
    case ml::ModelBackendType::kCpuBackend:
      return on_device_model::mojom::ModelBackendType::kCpu;
  }
  NOTREACHED();
}

// static
bool EnumTraits<on_device_model::mojom::ModelBackendType,
                ml::ModelBackendType>::
    FromMojom(on_device_model::mojom::ModelBackendType input,
              ml::ModelBackendType* output) {
  switch (input) {
    case on_device_model::mojom::ModelBackendType::kGpu:
      *output = ml::ModelBackendType::kGpuBackend;
      return true;
    case on_device_model::mojom::ModelBackendType::kApu:
      *output = ml::ModelBackendType::kApuBackend;
      return true;
    case on_device_model::mojom::ModelBackendType::kCpu:
      *output = ml::ModelBackendType::kCpuBackend;
      return true;
  }
  return false;
}

// static
on_device_model::mojom::ModelPerformanceHint
EnumTraits<on_device_model::mojom::ModelPerformanceHint,
           ml::ModelPerformanceHint>::ToMojom(ml::ModelPerformanceHint input) {
  switch (input) {
    case ml::ModelPerformanceHint::kHighestQuality:
      return on_device_model::mojom::ModelPerformanceHint::kHighestQuality;
    case ml::ModelPerformanceHint::kFastestInference:
      return on_device_model::mojom::ModelPerformanceHint::kFastestInference;
  }
  NOTREACHED();
}

// static
bool EnumTraits<on_device_model::mojom::ModelPerformanceHint,
                ml::ModelPerformanceHint>::
    FromMojom(on_device_model::mojom::ModelPerformanceHint input,
              ml::ModelPerformanceHint* output) {
  switch (input) {
    case on_device_model::mojom::ModelPerformanceHint::kHighestQuality:
      *output = ml::ModelPerformanceHint::kHighestQuality;
      return true;
    case on_device_model::mojom::ModelPerformanceHint::kFastestInference:
      *output = ml::ModelPerformanceHint::kFastestInference;
      return true;
  }
  return false;
}

bool StructTraits<on_device_model::mojom::AudioDataDataView, ml::AudioBuffer>::
    Read(on_device_model::mojom::AudioDataDataView in, ml::AudioBuffer* out) {
  out->sample_rate_hz = in.sample_rate();
  out->num_channels = in.channel_count();
  out->num_frames = in.frame_count();

  base::CheckedNumeric<size_t> expected_num_samples = out->num_channels;
  expected_num_samples *= out->num_frames;
  if (!expected_num_samples.IsValid()) {
    return false;
  }

  mojo::ArrayDataView<float> data_view;
  in.GetDataDataView(&data_view);
  if (data_view.size() != expected_num_samples.ValueOrDie()) {
    return false;
  }

  out->data.reserve(data_view.size());
  std::copy_n(data_view.data(), data_view.size(),
              std::back_inserter(out->data));
  return true;
}

// TODO(crbug.com/422803232): Explore keeping structured types (base::DictValue)
// through the system and deferring JSON serialization to the C API boundary,
// rather than converting at the typemap layer. The ml::* tool types in the :api
// target use JSON strings because :api cannot depend on //base.

// static
base::DictValue
StructTraits<on_device_model::mojom::ToolCallDataView, ml::ToolCall>::arguments(
    const ml::ToolCall& input) {
  // Parse JSON string back to base::DictValue for mojom serialization.
  auto parsed_json =
      base::JSONReader::Read(input.arguments_json, base::JSON_PARSE_RFC);
  if (parsed_json.has_value() && parsed_json->is_dict()) {
    return std::move(parsed_json->GetDict());
  }
  return base::DictValue();
}

// static
bool StructTraits<on_device_model::mojom::ToolCallDataView, ml::ToolCall>::Read(
    on_device_model::mojom::ToolCallDataView in,
    ml::ToolCall* out) {
  if (!in.ReadCallId(&out->call_id) || !in.ReadName(&out->name)) {
    return false;
  }

  base::DictValue arguments;
  if (!in.ReadArguments(&arguments)) {
    return false;
  }

  std::string json_string;
  if (!base::JSONWriter::Write(arguments, &json_string)) {
    return false;
  }
  out->arguments_json = std::move(json_string);
  return true;
}

// static
std::optional<base::Value>
StructTraits<on_device_model::mojom::ToolResponseDataView,
             ml::ToolResponse>::result(const ml::ToolResponse& input) {
  // Parse JSON string back to base::Value for mojom serialization.
  if (input.result_json.empty()) {
    return std::nullopt;
  }
  auto parsed_json =
      base::JSONReader::Read(input.result_json, base::JSON_PARSE_RFC);
  if (parsed_json.has_value()) {
    return std::move(parsed_json.value());
  }
  return std::nullopt;
}

// static
std::optional<std::string>
StructTraits<on_device_model::mojom::ToolResponseDataView,
             ml::ToolResponse>::error_message(const ml::ToolResponse& input) {
  if (input.error_message.empty()) {
    return std::nullopt;
  }
  return input.error_message;
}

// static
bool StructTraits<
    on_device_model::mojom::ToolResponseDataView,
    ml::ToolResponse>::Read(on_device_model::mojom::ToolResponseDataView in,
                            ml::ToolResponse* out) {
  if (!in.ReadCallId(&out->call_id) || !in.ReadName(&out->name)) {
    return false;
  }
  std::optional<std::string> error_message;
  if (!in.ReadErrorMessage(&error_message)) {
    return false;
  }
  std::optional<base::Value> result_value;
  if (!in.ReadResult(&result_value)) {
    return false;
  }

  // Validate mutual exclusivity: exactly one of error_message or result must be
  // set.
  bool has_error = error_message.has_value() && !error_message->empty();
  bool has_result = result_value.has_value() && !result_value->is_none();
  if ((has_error && has_result) || (!has_error && !has_result)) {
    // Both set or neither set - invalid state.
    return false;
  }

  // Store error_message if present.
  if (has_error) {
    out->error_message = std::move(error_message.value());
  }

  // Store result if present.
  if (has_result) {
    std::string json_string;
    if (!base::JSONWriter::Write(*result_value, &json_string)) {
      return false;
    }
    out->result_json = std::move(json_string);
  }

  return true;
}

// static
base::DictValue StructTraits<
    on_device_model::mojom::ToolDeclarationDataView,
    ml::ToolDeclaration>::input_schema(const ml::ToolDeclaration& input) {
  // Parse JSON string back to base::DictValue for mojom serialization.
  auto parsed_json =
      base::JSONReader::Read(input.input_schema_json, base::JSON_PARSE_RFC);
  if (parsed_json.has_value() && parsed_json->is_dict()) {
    return std::move(parsed_json->GetDict());
  }
  return base::DictValue();
}

// static
bool StructTraits<on_device_model::mojom::ToolDeclarationDataView,
                  ml::ToolDeclaration>::
    Read(on_device_model::mojom::ToolDeclarationDataView in,
         ml::ToolDeclaration* out) {
  if (!in.ReadName(&out->name) || !in.ReadDescription(&out->description)) {
    return false;
  }
  // Convert mojo DictValue to JSON string for ml:: layer storage.
  base::DictValue input_schema;
  if (!in.ReadInputSchema(&input_schema)) {
    return false;
  }
  std::string json_string;
  if (!base::JSONWriter::Write(input_schema, &json_string)) {
    return false;
  }
  out->input_schema_json = std::move(json_string);
  return true;
}

}  // namespace mojo
