// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_CHROME_ML_TYPES_MOJOM_TRAITS_H_
#define SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_CHROME_ML_TYPES_MOJOM_TRAITS_H_

#include "mojo/public/cpp/base/values_mojom_traits.h"
#include "services/on_device_model/ml/chrome_ml_types.h"
#include "services/on_device_model/public/mojom/on_device_model_service.mojom-shared.h"
#include "skia/public/mojom/bitmap_skbitmap_mojom_traits.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace base {
class Value;
}

namespace mojo {

template <>
struct EnumTraits<on_device_model::mojom::Token, ml::Token> {
  static on_device_model::mojom::Token ToMojom(ml::Token input);
  static bool FromMojom(on_device_model::mojom::Token input, ml::Token* output);
};

template <>
struct UnionTraits<on_device_model::mojom::InputPieceDataView, ml::InputPiece> {
  static on_device_model::mojom::InputPieceDataView::Tag GetTag(
      const ml::InputPiece& input_piece);

  static ml::Token token(const ml::InputPiece& input_piece) {
    return std::get<ml::Token>(input_piece);
  }

  static const std::string& text(const ml::InputPiece& input_piece) {
    return std::get<std::string>(input_piece);
  }

  static const SkBitmap& bitmap(const ml::InputPiece& input_piece) {
    return std::get<SkBitmap>(input_piece);
  }

  static const ml::AudioBuffer& audio(const ml::InputPiece& input_piece) {
    return std::get<ml::AudioBuffer>(input_piece);
  }

  static const ml::ToolDeclaration& tool_declaration(
      const ml::InputPiece& input_piece) {
    return std::get<ml::ToolDeclaration>(input_piece);
  }

  static const ml::ToolResponse& tool_response(
      const ml::InputPiece& input_piece) {
    return std::get<ml::ToolResponse>(input_piece);
  }

  static bool unknown_type(const ml::InputPiece& input_piece) {
    return std::get<bool>(input_piece);
  }

  static bool Read(on_device_model::mojom::InputPieceDataView in,
                   ml::InputPiece* out);
};

template <>
struct EnumTraits<on_device_model::mojom::ModelBackendType,
                  ml::ModelBackendType> {
  static on_device_model::mojom::ModelBackendType ToMojom(
      ml::ModelBackendType input);
  static bool FromMojom(on_device_model::mojom::ModelBackendType input,
                        ml::ModelBackendType* output);
};

template <>
struct EnumTraits<on_device_model::mojom::ModelPerformanceHint,
                  ml::ModelPerformanceHint> {
  static on_device_model::mojom::ModelPerformanceHint ToMojom(
      ml::ModelPerformanceHint input);
  static bool FromMojom(on_device_model::mojom::ModelPerformanceHint input,
                        ml::ModelPerformanceHint* output);
};

template <>
struct StructTraits<on_device_model::mojom::AudioDataDataView,
                    ml::AudioBuffer> {
  static int32_t sample_rate(const ml::AudioBuffer& input) {
    return input.sample_rate_hz;
  }

  static int32_t channel_count(const ml::AudioBuffer& input) {
    return input.num_channels;
  }

  static int32_t frame_count(const ml::AudioBuffer& input) {
    return input.num_frames;
  }

  static const std::vector<float>& data(const ml::AudioBuffer& input) {
    return input.data;
  }

  static bool Read(on_device_model::mojom::AudioDataDataView in,
                   ml::AudioBuffer* out);
};

template <>
struct StructTraits<on_device_model::mojom::ToolCallDataView, ml::ToolCall> {
  static const std::string& call_id(const ml::ToolCall& input) {
    return input.call_id;
  }

  static const std::string& name(const ml::ToolCall& input) {
    return input.name;
  }

  static base::DictValue arguments(const ml::ToolCall& input);

  static bool Read(on_device_model::mojom::ToolCallDataView in,
                   ml::ToolCall* out);
};

template <>
struct StructTraits<on_device_model::mojom::ToolResponseDataView,
                    ml::ToolResponse> {
  static const std::string& call_id(const ml::ToolResponse& input) {
    return input.call_id;
  }

  static const std::string& name(const ml::ToolResponse& input) {
    return input.name;
  }

  static std::optional<base::Value> result(const ml::ToolResponse& input);

  static std::optional<std::string> error_message(
      const ml::ToolResponse& input);

  static bool Read(on_device_model::mojom::ToolResponseDataView in,
                   ml::ToolResponse* out);
};

template <>
struct StructTraits<on_device_model::mojom::ToolDeclarationDataView,
                    ml::ToolDeclaration> {
  static const std::string& name(const ml::ToolDeclaration& input) {
    return input.name;
  }

  static const std::string& description(const ml::ToolDeclaration& input) {
    return input.description;
  }

  static base::DictValue input_schema(const ml::ToolDeclaration& input);

  static bool Read(on_device_model::mojom::ToolDeclarationDataView in,
                   ml::ToolDeclaration* out);
};

}  // namespace mojo

#endif  // SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_CHROME_ML_TYPES_MOJOM_TRAITS_H_
