// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_CONSOLE_LOGGER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_CONSOLE_LOGGER_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink-forward.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

// A pure virtual interface for console logging.
// Retaining an instance of ConsoleLogger may be dangerous because after the
// associated fetcher is detached it leads to a leak. Use
// DetachableConsoleLogger in such a case.
class PLATFORM_EXPORT ConsoleLogger : public GarbageCollectedMixin {
 public:
  ConsoleLogger() = default;
  virtual ~ConsoleLogger() = default;

  void AddConsoleMessage(mojom::blink::ConsoleMessageSource source,
                         mojom::blink::ConsoleMessageLevel level,
                         const String& message,
                         bool discard_duplicates = false,
                         absl::optional<mojom::blink::ConsoleMessageCategory>
                             category = absl::nullopt) {
    AddConsoleMessageImpl(source, level, message, discard_duplicates, category);
  }

 private:
  virtual void AddConsoleMessageImpl(
      mojom::blink::ConsoleMessageSource,
      mojom::blink::ConsoleMessageLevel,
      const String& message,
      bool discard_duplicates,
      absl::optional<mojom::blink::ConsoleMessageCategory> category) = 0;
};

// A ConsoleLogger subclass which has Detach() method. An instance of
// DetachableConsoleLogger can be kept even when the associated fetcher is
// detached.
class PLATFORM_EXPORT DetachableConsoleLogger final
    : public GarbageCollected<DetachableConsoleLogger>,
      public ConsoleLogger {
 public:
  DetachableConsoleLogger() : DetachableConsoleLogger(nullptr) {}
  DetachableConsoleLogger(ConsoleLogger* logger) : logger_(logger) {}

  // Detaches |logger_|. After this function is called AddConsoleMessage will
  // be no-op.
  void Detach() { logger_ = nullptr; }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(logger_);
    ConsoleLogger::Trace(visitor);
  }

  Member<ConsoleLogger> logger_;

 private:
  void AddConsoleMessageImpl(
      mojom::blink::ConsoleMessageSource source,
      mojom::blink::ConsoleMessageLevel level,
      const String& message,
      bool discard_duplicates,
      absl::optional<mojom::blink::ConsoleMessageCategory> category) override {
    if (!logger_) {
      return;
    }
    logger_->AddConsoleMessage(source, level, message, discard_duplicates,
                               category);
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_CONSOLE_LOGGER_H_
