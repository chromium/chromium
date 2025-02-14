// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/ai_capability_availability.h"

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-blink.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/ai/ai_availability.h"
#include "third_party/blink/renderer/modules/ai/ai_metrics.h"
#include "third_party/blink/renderer/modules/ai/exception_helpers.h"

// TODO(crbug.com/395509560): remove this file.

namespace blink {

AICapabilityAvailability AIAvailabilityToAICapabilityAvailability(
    AIAvailability availablity) {
  switch (availablity) {
    case AIAvailability::kAvailable: {
      return AICapabilityAvailability::kReadily;
    }
    case AIAvailability::kDownloadable:
    case AIAvailability::kDownloading: {
      return AICapabilityAvailability::kAfterDownload;
    }
    case AIAvailability::kUnavailable: {
      return AICapabilityAvailability::kNo;
    }
  }
  NOTREACHED();
}

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
