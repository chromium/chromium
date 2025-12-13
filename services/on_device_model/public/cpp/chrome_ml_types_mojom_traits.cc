// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/public/cpp/chrome_ml_types_mojom_traits.h"

#include <algorithm>

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
  mojo::ArrayDataView<float> data_view;
  in.GetDataDataView(&data_view);
  out->data.reserve(data_view.size());
  std::copy_n(data_view.data(), data_view.size(),
              std::back_inserter(out->data));
  return true;
}

}  // namespace mojo
