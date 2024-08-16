// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/ai_capability_availability.h"

namespace blink {

V8AICapabilityAvailability AICapabilityAvailabilityToV8(
    AICapabilityAvailability availability) {
  switch (availability) {
    case AICapabilityAvailability::kReadily:
      return V8AICapabilityAvailability(
          V8AICapabilityAvailability::Enum::kReadily);
    case AICapabilityAvailability::kAfterDownload:
      return V8AICapabilityAvailability(
          V8AICapabilityAvailability::Enum::kAfterDownload);
    case AICapabilityAvailability::kNo:
      return V8AICapabilityAvailability(V8AICapabilityAvailability::Enum::kNo);
  }
}

}  // namespace blink
