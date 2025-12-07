// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SCREEN_AI_PUBLIC_CPP_METRICS_H_
#define SERVICES_SCREEN_AI_PUBLIC_CPP_METRICS_H_

#include <optional>
#include <string>

#include "services/screen_ai/proto/chrome_screen_ai.pb.h"

namespace screen_ai {

// Counts detected languages in `ocr_data` and returns the hash code of the most
// detected language. Returns empty if no language is found.
std::optional<uint64_t> GetMostDetectedLanguageInOcrData(
    const chrome_screen_ai::VisualAnnotation& ocr_data);

}  // namespace screen_ai

#endif  // SERVICES_SCREEN_AI_PUBLIC_CPP_METRICS_H_
