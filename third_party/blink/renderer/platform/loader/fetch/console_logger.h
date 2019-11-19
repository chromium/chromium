// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_CONSOLE_LOGGER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_CONSOLE_LOGGER_H_

#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink-forward.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
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

  void AddConsoleMessage(mojom::ConsoleMessageSource source,
                         mojom::ConsoleMessageLevel level,
                         const String& message,
                         bool discard_duplicates = false) {
    AddConsoleMessageImpl(source, level, message, discard_duplicates);
  }

 private:
  virtual void AddConsoleMessageImpl(mojom::ConsoleMessageSource,
                                     mojom::ConsoleMessageLevel,
                                     const String& message,
                                     bool discard_duplicates) = 0;
};

// A ConsoleLogger subclass which has Detach() method. An instance of
// DetachableConsoleLogger can be kept even when the associated fetcher is
// detached.
class PLATFORM_EXPORT DetachableConsoleLogger final
    : public GarbageCollected<DetachableConsoleLogger>,
      public ConsoleLogger {
  USING_GARBAGE_COLLECTED_MIXIN(DetachableConsoleLogger);

 public:
  DetachableConsoleLogger() : DetachableConsoleLogger(nullptr) {}
  DetachableConsoleLogger(ConsoleLogger* logger) : logger_(logger) {}

  // Detaches |logger_|. After this function is called AddConsoleMessage will
  // be no-op.
  void Detach() { logger_ = nullptr; }

  void Trace(Visitor* visitor) override {
    visitor->Trace(logger_);
    ConsoleLogger::Trace(visitor);
  }

  Member<ConsoleLogger> logger_;

 private:
  void AddConsoleMessageImpl(mojom::ConsoleMessageSource source,
                             mojom::ConsoleMessageLevel level,
                             const String& message,
                             bool discard_duplicates) override {
    if (!logger_) {
      return;
    }
    logger_->AddConsoleMessage(source, level, message, discard_duplicates);
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_CONSOLE_LOGGER_H_
