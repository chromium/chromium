// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/public/cpp/chrome_ml_types_mojom_traits.h"

#include "base/notreached.h"

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
ml::Token EnumTraits<on_device_model::mojom::Token, ml::Token>::FromMojom(
    on_device_model::mojom::Token input) {
  switch (input) {
    case on_device_model::mojom::Token::kSystem:
      return ml::Token::kSystem;
    case on_device_model::mojom::Token::kModel:
      return ml::Token::kModel;
    case on_device_model::mojom::Token::kUser:
      return ml::Token::kUser;
    case on_device_model::mojom::Token::kEnd:
      return ml::Token::kEnd;
    case on_device_model::mojom::Token::kToolCall:
      return ml::Token::kToolCall;
    case on_device_model::mojom::Token::kToolResponse:
      return ml::Token::kToolResponse;
  }
  NOTREACHED();
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
ml::ModelBackendType
EnumTraits<on_device_model::mojom::ModelBackendType, ml::ModelBackendType>::
    FromMojom(on_device_model::mojom::ModelBackendType input) {
  switch (input) {
    case on_device_model::mojom::ModelBackendType::kGpu:
      return ml::ModelBackendType::kGpuBackend;
    case on_device_model::mojom::ModelBackendType::kApu:
      return ml::ModelBackendType::kApuBackend;
    case on_device_model::mojom::ModelBackendType::kCpu:
      return ml::ModelBackendType::kCpuBackend;
  }
  NOTREACHED();
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
ml::ModelPerformanceHint
EnumTraits<on_device_model::mojom::ModelPerformanceHint,
           ml::ModelPerformanceHint>::
    FromMojom(on_device_model::mojom::ModelPerformanceHint input) {
  switch (input) {
    case on_device_model::mojom::ModelPerformanceHint::kHighestQuality:
      return ml::ModelPerformanceHint::kHighestQuality;
    case on_device_model::mojom::ModelPerformanceHint::kFastestInference:
      return ml::ModelPerformanceHint::kFastestInference;
  }
  NOTREACHED();
}

}  // namespace mojo
