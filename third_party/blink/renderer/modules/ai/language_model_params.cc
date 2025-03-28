// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/language_model_params.h"

namespace blink {

LanguageModelParams::LanguageModelParams(uint64_t default_top_k,
                                         uint64_t max_top_k,
                                         float default_temperature,
                                         float max_temperature)
    : default_top_k_(default_top_k),
      max_top_k_(max_top_k),
      default_temperature_(default_temperature),
      max_temperature_(max_temperature) {}

void LanguageModelParams::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
