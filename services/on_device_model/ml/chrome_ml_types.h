// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_ML_CHROME_ML_TYPES_H_
#define SERVICES_ON_DEVICE_MODEL_ML_CHROME_ML_TYPES_H_

#include <string>
#include <variant>
#include <vector>

#include "services/on_device_model/ml/chrome_ml_audio_buffer.h"

class SkBitmap;

namespace ml {

inline constexpr uint32_t kMinTopK = 1;
inline constexpr float kMinTemperature = 0.0f;

enum class Token {
  // Prefix for system text.
  kSystem,
  // Prefix for model text.
  kModel,
  // Prefix for user text.
  kUser,
  // End a system/model/user section.
  kEnd,
  // Prefix for tool call (function invocation by the model).
  kToolCall,
  // Prefix for tool response (results from tool execution).
  kToolResponse,
};

// If an InputPiece holds a `bool`, then the operation should fail. This means
// the input came from a future client version and can't be handled in the
// current library version.
using InputPiece =
    std::variant<Token, std::string, SkBitmap, AudioBuffer, bool>;

// Options for specifying the performance characteristics of the model to load.
enum class ModelPerformanceHint {
  kHighestQuality,
  kFastestInference,
};

// Type of the backend to run the model.
enum class ModelBackendType {
  // The default WebGPU backend.
  kGpuBackend,
  // The APU accelerator backend. Only available on devices with APU, and need
  // special APU model files.
  kApuBackend,
  // The CPU backend.
  kCpuBackend,
};

}  // namespace ml

#endif  // SERVICES_ON_DEVICE_MODEL_ML_CHROME_ML_TYPES_H_
