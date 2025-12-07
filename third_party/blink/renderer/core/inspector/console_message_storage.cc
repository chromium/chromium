// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/console_message_storage.h"

#include "base/notreached.h"
#include "base/trace_event/trace_event.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"

namespace blink {

static const unsigned kMaxConsoleMessageCount = 1000;

namespace {

const char* MessageSourceToString(mojom::ConsoleMessageSource source) {
  switch (source) {
    case mojom::ConsoleMessageSource::kXml:
      return "XML";
    case mojom::ConsoleMessageSource::kJavaScript:
      return "JS";
    case mojom::ConsoleMessageSource::kNetwork:
      return "Network";
    case mojom::ConsoleMessageSource::kConsoleApi:
      return "ConsoleAPI";
    case mojom::ConsoleMessageSource::kStorage:
      return "Storage";
    case mojom::ConsoleMessageSource::kRendering:
      return "Rendering";
    case mojom::ConsoleMessageSource::kSecurity:
      return "Security";
    case mojom::ConsoleMessageSource::kOther:
      return "Other";
    case mojom::ConsoleMessageSource::kDeprecation:
      return "Deprecation";
    case mojom::ConsoleMessageSource::kWorker:
      return "Worker";
    case mojom::ConsoleMessageSource::kViolation:
      return "Violation";
    case mojom::ConsoleMessageSource::kIntervention:
      return "Intervention";
    case mojom::ConsoleMessageSource::kRecommendation:
      return "Recommendation";
  }
  NOTREACHED();
}

std::unique_ptr<TracedValue> MessageTracedValue(ConsoleMessage* message) {
  auto value = std::make_unique<TracedValue>();
  value->SetString("content", message->Message());
  if (!message->Location()->Url().empty()) {
    value->SetString("url", message->Location()->Url());
  }
  return value;
}

void TraceConsoleMessageEvent(ConsoleMessage* message) {
  // Change in this function requires adjustment of Catapult/Telemetry metric
  // tracing/tracing/metrics/console_error_metric.html.
  // See https://crbug.com/880432
  if (message->GetLevel() == ConsoleMessage::Level::kError) {
    TRACE_EVENT_INSTANT2("blink.console", "ConsoleMessage::Error",
                         TRACE_EVENT_SCOPE_THREAD, "source",
                         MessageSourceToString(message->GetSource()), "message",
                         MessageTracedValue(message));
  }
}
}  // anonymous namespace

ConsoleMessageStorage::ConsoleMessageStorage() : expired_count_(0) {}

bool ConsoleMessageStorage::AddConsoleMessage(ExecutionContext* context,
                                              ConsoleMessage* message,
                                              bool discard_duplicates) {
  DCHECK(messages_.size() <= kMaxConsoleMessageCount);
  if (discard_duplicates) {
    for (auto& console_message : messages_) {
      if (message->Message() == console_message->Message())
        return false;
    }
  }
  TraceConsoleMessageEvent(message);
  probe::ConsoleMessageAdded(context, message);
  if (messages_.size() == kMaxConsoleMessageCount) {
    ++expired_count_;
    messages_.pop_front();
  }
  messages_.push_back(message);
  return true;
}

void ConsoleMessageStorage::Clear() {
  messages_.clear();
  expired_count_ = 0;
}

wtf_size_t ConsoleMessageStorage::size() const {
  return messages_.size();
}

ConsoleMessage* ConsoleMessageStorage::at(wtf_size_t index) const {
  return messages_[index].Get();
}

int ConsoleMessageStorage::ExpiredCount() const {
  return expired_count_;
}

void ConsoleMessageStorage::Trace(Visitor* visitor) const {
  visitor->Trace(messages_);
}

}  // namespace blink
