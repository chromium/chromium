// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/console_message.h"

#include <memory>
#include <utility>

#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

ConsoleMessage::ConsoleMessage(mojom::blink::ConsoleMessageSource source,
                               mojom::blink::ConsoleMessageLevel level,
                               const String& message,
                               const String& url,
                               DocumentLoader* loader,
                               uint64_t request_identifier)
    : ConsoleMessage(source, level, message, CaptureSourceLocation(url, 0, 0)) {
  request_identifier_ =
      IdentifiersFactory::RequestId(loader, request_identifier);
}

ConsoleMessage::ConsoleMessage(mojom::blink::ConsoleMessageLevel level,
                               const String& message,
                               std::unique_ptr<SourceLocation> location,
                               WorkerThread* worker_thread)
    : ConsoleMessage(mojom::blink::ConsoleMessageSource::kWorker,
                     level,
                     message,
                     std::move(location)) {
  worker_id_ =
      IdentifiersFactory::IdFromToken(worker_thread->GetDevToolsWorkerToken());
}

ConsoleMessage::ConsoleMessage(const WebConsoleMessage& message,
                               LocalFrame* local_frame)
    : ConsoleMessage(message.nodes.empty()
                         ? mojom::blink::ConsoleMessageSource::kOther
                         : mojom::blink::ConsoleMessageSource::kRecommendation,
                     message.level,
                     message.text,
                     std::make_unique<SourceLocation>(message.url,
                                                      String(),
                                                      message.line_number,
                                                      message.column_number,
                                                      nullptr)) {
  if (local_frame) {
    Vector<DOMNodeId> nodes;
    for (const WebNode& web_node : message.nodes)
      nodes.push_back(web_node.GetDomNodeId());
    SetNodes(local_frame, std::move(nodes));
  }
}

ConsoleMessage::ConsoleMessage(mojom::blink::ConsoleMessageSource source,
                               mojom::blink::ConsoleMessageLevel level,
                               const String& message,
                               std::unique_ptr<SourceLocation> location)
    : source_(source),
      level_(level),
      message_(message),
      location_(std::move(location)),
      timestamp_(base::Time::Now().InMillisecondsFSinceUnixEpoch()),
      frame_(nullptr) {
  DCHECK(location_);
}

ConsoleMessage::~ConsoleMessage() = default;

SourceLocation* ConsoleMessage::Location() const {
  return location_.get();
}

const String& ConsoleMessage::RequestIdentifier() const {
  return request_identifier_;
}

double ConsoleMessage::Timestamp() const {
  return timestamp_;
}

ConsoleMessage::Source ConsoleMessage::GetSource() const {
  return source_;
}

ConsoleMessage::Level ConsoleMessage::GetLevel() const {
  return level_;
}

const String& ConsoleMessage::Message() const {
  return message_;
}

const String& ConsoleMessage::WorkerId() const {
  return worker_id_;
}

LocalFrame* ConsoleMessage::Frame() const {
  // Do not reference detached frames.
  if (frame_ && frame_->Client())
    return frame_.Get();
  return nullptr;
}

Vector<DOMNodeId>& ConsoleMessage::Nodes() {
  return nodes_;
}

void ConsoleMessage::SetNodes(LocalFrame* frame, Vector<DOMNodeId> nodes) {
  frame_ = frame;
  nodes_ = std::move(nodes);
}

const std::optional<mojom::blink::ConsoleMessageCategory>&
ConsoleMessage::Category() const {
  return category_;
}

void ConsoleMessage::SetCategory(
    mojom::blink::ConsoleMessageCategory category) {
  category_ = category;
}

void ConsoleMessage::Trace(Visitor* visitor) const {
  visitor->Trace(frame_);
}

}  // namespace blink
