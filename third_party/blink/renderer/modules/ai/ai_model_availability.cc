// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/ai_model_availability.h"

namespace blink {

V8AIModelAvailability AIModelAvailabilityToV8(
    AIModelAvailability availability) {
  switch (availability) {
    case AIModelAvailability::kReadily:
      return V8AIModelAvailability(V8AIModelAvailability::Enum::kReadily);
    case AIModelAvailability::kAfterDownload:
      return V8AIModelAvailability(V8AIModelAvailability::Enum::kAfterDownload);
    case AIModelAvailability::kNo:
      return V8AIModelAvailability(V8AIModelAvailability::Enum::kNo);
  }
}

}  // namespace blink
