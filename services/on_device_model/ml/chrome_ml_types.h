// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_ML_CHROME_ML_TYPES_H_
#define SERVICES_ON_DEVICE_MODEL_ML_CHROME_ML_TYPES_H_

#include <string>
#include <variant>

#include "third_party/skia/include/core/SkBitmap.h"

namespace ml {

enum class Token {
  // Prefix for system text.
  kSystem,
  // Prefix for model text.
  kModel,
  // Prefix for user text.
  kUser,
  // End a system/model/user section.
  kEnd,
};

// If an InputPiece holds a `bool`, then the operation should fail. This means
// the input came from a future client version and can't be handled in the
// current library version.
using InputPiece = std::variant<Token, std::string, SkBitmap, bool>;

// Options for specifying the performance characteristics of the model to load.
enum class ModelPerformanceHint {
  kHighestQuality,
  kFastestInference,
};

// Type of the backend to run the model.
enum ModelBackendType {
  // The default WebGPU backend.
  kGpuBackend = 0,
  // The APU accelerator backend. Only available on devices with APU, and need
  // special APU model files.
  kApuBackend = 1,
};

}  // namespace ml

#endif  // SERVICES_ON_DEVICE_MODEL_ML_CHROME_ML_TYPES_H_
