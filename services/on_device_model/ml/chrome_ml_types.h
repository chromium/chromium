// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_ML_CHROME_ML_TYPES_H_
#define SERVICES_ON_DEVICE_MODEL_ML_CHROME_ML_TYPES_H_

#include <string>
#include <variant>

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

using InputPiece = std::variant<Token, std::string>;

}  // namespace ml

#endif  // SERVICES_ON_DEVICE_MODEL_ML_CHROME_ML_TYPES_H_
