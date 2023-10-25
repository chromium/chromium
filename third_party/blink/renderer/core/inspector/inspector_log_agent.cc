// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_log_agent.h"

#include "base/format_macros.h"
#include "third_party/blink/renderer/core/frame/performance_monitor.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/console_message_storage.h"
#include "third_party/blink/renderer/core/inspector/inspector_dom_agent.h"
#include "third_party/blink/renderer/core/inspector/resolve_node.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/bindings/source_location.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

String MessageSourceValue(mojom::blink::ConsoleMessageSource source) {
  DCHECK(source != mojom::blink::ConsoleMessageSource::kConsoleApi);
  switch (source) {
    case mojom::blink::ConsoleMessageSource::kXml:
      return protocol::Log::LogEntry::SourceEnum::Xml;
    case mojom::blink::ConsoleMessageSource::kJavaScript:
      return protocol::Log::LogEntry::SourceEnum::Javascript;
    case mojom::blink::ConsoleMessageSource::kNetwork:
      return protocol::Log::LogEntry::SourceEnum::Network;
    case mojom::blink::ConsoleMessageSource::kStorage:
      return protocol::Log::LogEntry::SourceEnum::Storage;
    case mojom::blink::ConsoleMessageSource::kRendering:
      return protocol::Log::LogEntry::SourceEnum::Rendering;
    case mojom::blink::ConsoleMessageSource::kSecurity:
      return protocol::Log::LogEntry::SourceEnum::Security;
    case mojom::blink::ConsoleMessageSource::kOther:
      return protocol::Log::LogEntry::SourceEnum::Other;
    case mojom::blink::ConsoleMessageSource::kDeprecation:
      return protocol::Log::LogEntry::SourceEnum::Deprecation;
    case mojom::blink::ConsoleMessageSource::kWorker:
      return protocol::Log::LogEntry::SourceEnum::Worker;
    case mojom::blink::ConsoleMessageSource::kViolation:
      return protocol::Log::LogEntry::SourceEnum::Violation;
    case mojom::blink::ConsoleMessageSource::kIntervention:
      return protocol::Log::LogEntry::SourceEnum::Intervention;
    case mojom::blink::ConsoleMessageSource::kRecommendation:
      return protocol::Log::LogEntry::SourceEnum::Recommendation;
    default:
      return protocol::Log::LogEntry::SourceEnum::Other;
  }
}

String MessageLevelValue(mojom::blink::ConsoleMessageLevel level) {
  switch (level) {
    case mojom::blink::ConsoleMessageLevel::kVerbose:
      return protocol::Log::LogEntry::LevelEnum::Verbose;
    case mojom::blink::ConsoleMessageLevel::kInfo:
      return protocol::Log::LogEntry::LevelEnum::Info;
    case mojom::blink::ConsoleMessageLevel::kWarning:
      return protocol::Log::LogEntry::LevelEnum::Warning;
    case mojom::blink::ConsoleMessageLevel::kError:
      return protocol::Log::LogEntry::LevelEnum::Error;
  }
  return protocol::Log::LogEntry::LevelEnum::Info;
}

String MessageCategoryValue(mojom::blink::ConsoleMessageCategory category) {
  switch (category) {
    case mojom::blink::ConsoleMessageCategory::Cors:
      return protocol::Log::LogEntry::CategoryEnum::Cors;
  }
  return WTF::g_empty_string;
}

}  // namespace

using protocol::Log::ViolationSetting;

InspectorLogAgent::InspectorLogAgent(
    ConsoleMessageStorage* storage,
    PerformanceMonitor* performance_monitor,
    v8_inspector::V8InspectorSession* v8_session)
    : storage_(storage),
      performance_monitor_(performance_monitor),
      v8_session_(v8_session),
      enabled_(&agent_state_, /*default_value=*/false),
      violation_thresholds_(&agent_state_, -1.0) {}

InspectorLogAgent::~InspectorLogAgent() = default;

void InspectorLogAgent::Trace(Visitor* visitor) const {
  visitor->Trace(storage_);
  visitor->Trace(performance_monitor_);
  InspectorBaseAgent::Trace(visitor);
  PerformanceMonitor::Client::Trace(visitor);
}

void InspectorLogAgent::Restore() {
  if (!enabled_.Get())
    return;
  InnerEnable();
  if (violation_thresholds_.IsEmpty())
    return;
  auto settings = std::make_unique<protocol::Array<ViolationSetting>>();
  for (const WTF::String& key : violation_thresholds_.Keys()) {
    settings->emplace_back(ViolationSetting::create()
                               .setName(key)
                               .setThreshold(violation_thresholds_.Get(key))
                               .build());
  }
  startViolationsReport(std::move(settings));
}

void InspectorLogAgent::ConsoleMessageAdded(ConsoleMessage* message) {
  DCHECK(enabled_.Get());

  std::unique_ptr<protocol::Log::LogEntry> entry =
      protocol::Log::LogEntry::create()
          .setSource(MessageSourceValue(message->GetSource()))
          .setLevel(MessageLevelValue(message->GetLevel()))
          .setText(message->Message())
          .setTimestamp(message->Timestamp())
          .build();
  if (!message->Location()->Url().empty())
    entry->setUrl(message->Location()->Url());
  std::unique_ptr<v8_inspector::protocol::Runtime::API::StackTrace>
      stack_trace = message->Location()->BuildInspectorObject();
  if (stack_trace)
    entry->setStackTrace(std::move(stack_trace));
  if (message->Location()->LineNumber())
    entry->setLineNumber(message->Location()->LineNumber() - 1);
  if (message->GetSource() == ConsoleMessage::Source::kWorker &&
      !message->WorkerId().empty()) {
    entry->setWorkerId(message->WorkerId());
  }
  if (message->GetSource() == ConsoleMessage::Source::kNetwork &&
      !message->RequestIdentifier().IsNull()) {
    entry->setNetworkRequestId(message->RequestIdentifier());
  }

  if (v8_session_ && message->Frame() && !message->Nodes().empty()) {
    ScriptForbiddenScope::AllowUserAgentScript allow_script;
    auto remote_objects = std::make_unique<
        protocol::Array<v8_inspector::protocol::Runtime::API::RemoteObject>>();
    for (DOMNodeId node_id : message->Nodes()) {
      std::unique_ptr<v8_inspector::protocol::Runtime::API::RemoteObject>
          remote_object;
      Node* node = DOMNodeIds::NodeForId(node_id);
      if (node) {
        remote_object =
            ResolveNode(v8_session_, node, "console", protocol::Maybe<int>());
      }
      if (!remote_object) {
        remote_object =
            NullRemoteObject(v8_session_, message->Frame(), "console");
      }
      if (remote_object) {
        remote_objects->emplace_back(std::move(remote_object));
      } else {
        // If a null object could not be referenced, we do not send the message
        // at all, to avoid situations in which the arguments are misleading.
        return;
      }
    }
    entry->setArgs(std::move(remote_objects));
  }

  if (auto category = message->Category()) {
    entry->setCategory(MessageCategoryValue(*category));
  }

  GetFrontend()->entryAdded(std::move(entry));
  GetFrontend()->flush();
}

void InspectorLogAgent::InnerEnable() {
  instrumenting_agents_->AddInspectorLogAgent(this);
  if (storage_->ExpiredCount()) {
    std::unique_ptr<protocol::Log::LogEntry> expired =
        protocol::Log::LogEntry::create()
            .setSource(protocol::Log::LogEntry::SourceEnum::Other)
            .setLevel(protocol::Log::LogEntry::LevelEnum::Warning)
            .setText(String::Number(storage_->ExpiredCount()) +
                     String(" log entries are not shown."))
            .setTimestamp(0)
            .build();
    GetFrontend()->entryAdded(std::move(expired));
    GetFrontend()->flush();
  }
  for (wtf_size_t i = 0; i < storage_->size(); ++i)
    ConsoleMessageAdded(storage_->at(i));
}

protocol::Response InspectorLogAgent::enable() {
  if (enabled_.Get())
    return protocol::Response::Success();
  enabled_.Set(true);
  InnerEnable();
  return protocol::Response::Success();
}

protocol::Response InspectorLogAgent::disable() {
  if (!enabled_.Get())
    return protocol::Response::Success();
  enabled_.Clear();
  stopViolationsReport();
  instrumenting_agents_->RemoveInspectorLogAgent(this);
  return protocol::Response::Success();
}

protocol::Response InspectorLogAgent::clear() {
  storage_->Clear();
  return protocol::Response::Success();
}

static PerformanceMonitor::Violation ParseViolation(const String& name) {
  if (name == ViolationSetting::NameEnum::DiscouragedAPIUse)
    return PerformanceMonitor::kDiscouragedAPIUse;
  if (name == ViolationSetting::NameEnum::LongTask)
    return PerformanceMonitor::kLongTask;
  if (name == ViolationSetting::NameEnum::LongLayout)
    return PerformanceMonitor::kLongLayout;
  if (name == ViolationSetting::NameEnum::BlockedEvent)
    return PerformanceMonitor::kBlockedEvent;
  if (name == ViolationSetting::NameEnum::BlockedParser)
    return PerformanceMonitor::kBlockedParser;
  if (name == ViolationSetting::NameEnum::Handler)
    return PerformanceMonitor::kHandler;
  if (name == ViolationSetting::NameEnum::RecurringHandler)
    return PerformanceMonitor::kRecurringHandler;
  return PerformanceMonitor::kAfterLast;
}

protocol::Response InspectorLogAgent::startViolationsReport(
    std::unique_ptr<protocol::Array<ViolationSetting>> settings) {
  if (!enabled_.Get())
    return protocol::Response::ServerError("Log is not enabled");
  if (!performance_monitor_) {
    return protocol::Response::ServerError(
        "Violations are not supported for this target");
  }
  performance_monitor_->UnsubscribeAll(this);
  violation_thresholds_.Clear();
  for (const std::unique_ptr<ViolationSetting>& setting : *settings) {
    const WTF::String& name = setting->getName();
    double threshold = setting->getThreshold();
    PerformanceMonitor::Violation violation = ParseViolation(name);
    if (violation == PerformanceMonitor::kAfterLast)
      continue;
    performance_monitor_->Subscribe(violation, base::Milliseconds(threshold),
                                    this);
    violation_thresholds_.Set(name, threshold);
  }
  return protocol::Response::Success();
}

protocol::Response InspectorLogAgent::stopViolationsReport() {
  violation_thresholds_.Clear();
  if (!performance_monitor_) {
    return protocol::Response::ServerError(
        "Violations are not supported for this target");
  }
  performance_monitor_->UnsubscribeAll(this);
  return protocol::Response::Success();
}

void InspectorLogAgent::ReportLongLayout(base::TimeDelta duration) {
  String message_text = String::Format(
      "Forced reflow while executing JavaScript took %" PRId64 "ms",
      duration.InMilliseconds());
  auto* message = MakeGarbageCollected<ConsoleMessage>(
      mojom::blink::ConsoleMessageSource::kViolation,
      mojom::blink::ConsoleMessageLevel::kVerbose, message_text);
  ConsoleMessageAdded(message);
}

void InspectorLogAgent::ReportGenericViolation(PerformanceMonitor::Violation,
                                               const String& text,
                                               base::TimeDelta time,
                                               SourceLocation* location) {
  auto* message = MakeGarbageCollected<ConsoleMessage>(
      mojom::blink::ConsoleMessageSource::kViolation,
      mojom::blink::ConsoleMessageLevel::kVerbose, text, location->Clone());
  ConsoleMessageAdded(message);
}

}  // namespace blink
