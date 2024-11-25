// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/ml/chrome_ml_types_traits.h"

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
    case on_device_model::mojom::InputPieceDataView::Tag::kUnknownType: {
      *out = in.unknown_type();
      return true;
    }
  }
  return false;
}

}  // namespace mojo
