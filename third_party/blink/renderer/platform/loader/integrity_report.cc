// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/integrity_report.h"

#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/console_logger.h"

namespace blink {

void IntegrityReport::AddUseCount(WebFeature feature) {
  use_counts_.push_back(feature);
}

void IntegrityReport::AddConsoleErrorMessage(const String& message) {
  messages_.push_back(message);
}

void IntegrityReport::Clear() {
  use_counts_.clear();
  messages_.clear();
}

void IntegrityReport::SendReports(
    UseCounterAndConsoleLogger* counter_logger) const {
  if (counter_logger) {
    for (WebFeature feature : use_counts_) {
      counter_logger->CountUse(feature);
    }
    for (const String& message : messages_) {
      counter_logger->AddConsoleMessage(
          mojom::blink::ConsoleMessageSource::kSecurity,
          mojom::blink::ConsoleMessageLevel::kError, message);
    }
  }
}

}  // namespace blink
