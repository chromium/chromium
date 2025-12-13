// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_CONSOLE_LOGGER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_CONSOLE_LOGGER_H_

#include <optional>

#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink-forward.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ConsoleMessage;

// A pure virtual interface for console logging.
// Retaining an instance of ConsoleLogger may be dangerous because after the
// associated fetcher is detached it leads to a leak. Use
// DetachableConsoleLogger in such a case.
class PLATFORM_EXPORT ConsoleLogger : public GarbageCollectedMixin {
 public:
  ConsoleLogger() = default;
  virtual ~ConsoleLogger() = default;

  // Please familiarize yourself with http://goo.gle/devtools-console-policy
  // prior to adding new console messages, and make sure that you understand the
  // implications on the developer experience. A good console message should be
  // actionable and relevant to what the developer is currently doing. Using the
  // DevTools Console panel as a means to advertise best practices or Chromium
  // agendas has shown to be counterproductive.
  void AddConsoleMessage(mojom::blink::ConsoleMessageSource source,
                         mojom::blink::ConsoleMessageLevel level,
                         const String& message,
                         bool discard_duplicates = false,
                         std::optional<mojom::blink::ConsoleMessageCategory>
                             category = std::nullopt) {
    AddConsoleMessageImpl(source, level, message, discard_duplicates, category);
  }

  // Please familiarize yourself with http://goo.gle/devtools-console-policy
  // prior to adding new console messages, and make sure that you understand the
  // implications on the developer experience. A good console message should be
  // actionable and relevant to what the developer is currently doing. Using the
  // DevTools Console panel as a means to advertise best practices or Chromium
  // agendas has shown to be counterproductive.
  void AddConsoleMessage(ConsoleMessage* message,
                         bool discard_duplicates = false) {
    AddConsoleMessageImpl(message, discard_duplicates);
  }

 private:
  virtual void AddConsoleMessageImpl(
      mojom::blink::ConsoleMessageSource,
      mojom::blink::ConsoleMessageLevel,
      const String& message,
      bool discard_duplicates,
      std::optional<mojom::blink::ConsoleMessageCategory> category) = 0;
  virtual void AddConsoleMessageImpl(ConsoleMessage* message,
                                     bool discard_duplicates) = 0;
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
      std::optional<mojom::blink::ConsoleMessageCategory> category) override {
    if (!logger_) {
      return;
    }
    logger_->AddConsoleMessage(source, level, message, discard_duplicates,
                               category);
  }
  void AddConsoleMessageImpl(ConsoleMessage* message,
                             bool discard_duplicates) override {
    if (!logger_) {
      return;
    }
    logger_->AddConsoleMessage(message, discard_duplicates);
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_CONSOLE_LOGGER_H_
