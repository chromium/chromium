// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/console_message.h"

#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// static
ConsoleMessage* ConsoleMessage::CreateForRequest(
    mojom::ConsoleMessageSource source,
    mojom::ConsoleMessageLevel level,
    const String& message,
    const String& url,
    DocumentLoader* loader,
    uint64_t request_identifier) {
  ConsoleMessage* console_message = ConsoleMessage::Create(
      source, level, message, SourceLocation::Capture(url, 0, 0));
  console_message->request_identifier_ =
      IdentifiersFactory::RequestId(loader, request_identifier);
  return console_message;
}

// static
ConsoleMessage* ConsoleMessage::Create(
    mojom::ConsoleMessageSource source,
    mojom::ConsoleMessageLevel level,
    const String& message,
    std::unique_ptr<SourceLocation> location) {
  return MakeGarbageCollected<ConsoleMessage>(source, level, message,
                                              std::move(location));
}

// static
ConsoleMessage* ConsoleMessage::Create(mojom::ConsoleMessageSource source,
                                       mojom::ConsoleMessageLevel level,
                                       const String& message) {
  return ConsoleMessage::Create(source, level, message,
                                SourceLocation::Capture());
}

// static
ConsoleMessage* ConsoleMessage::CreateFromWorker(
    mojom::ConsoleMessageLevel level,
    const String& message,
    std::unique_ptr<SourceLocation> location,
    WorkerThread* worker_thread) {
  ConsoleMessage* console_message =
      ConsoleMessage::Create(mojom::ConsoleMessageSource::kWorker, level,
                             message, std::move(location));
  console_message->worker_id_ =
      IdentifiersFactory::IdFromToken(worker_thread->GetDevToolsWorkerToken());
  return console_message;
}

ConsoleMessage* ConsoleMessage::CreateFromWebConsoleMessage(
    const WebConsoleMessage& message,
    LocalFrame* local_frame) {
  mojom::ConsoleMessageSource message_source =
      message.nodes.empty() ? mojom::ConsoleMessageSource::kOther
                            : mojom::ConsoleMessageSource::kRecommendation;

  ConsoleMessage* console_message = ConsoleMessage::Create(
      message_source, message.level, message.text,
      std::make_unique<SourceLocation>(message.url, message.line_number,
                                       message.column_number, nullptr));

  if (local_frame) {
    Vector<DOMNodeId> nodes;
    for (const WebNode& web_node : message.nodes)
      nodes.push_back(DOMNodeIds::IdForNode(&(*web_node)));
    console_message->SetNodes(local_frame, std::move(nodes));
  }

  return console_message;
}

ConsoleMessage::ConsoleMessage(mojom::ConsoleMessageSource source,
                               mojom::ConsoleMessageLevel level,
                               const String& message,
                               std::unique_ptr<SourceLocation> location)
    : source_(source),
      level_(level),
      message_(message),
      location_(std::move(location)),
      timestamp_(base::Time::Now().ToDoubleT() * 1000.0),
      frame_(nullptr) {}

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

mojom::ConsoleMessageSource ConsoleMessage::Source() const {
  return source_;
}

mojom::ConsoleMessageLevel ConsoleMessage::Level() const {
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
    return frame_;
  return nullptr;
}

Vector<DOMNodeId>& ConsoleMessage::Nodes() {
  return nodes_;
}

void ConsoleMessage::SetNodes(LocalFrame* frame, Vector<DOMNodeId> nodes) {
  frame_ = frame;
  nodes_ = std::move(nodes);
}

void ConsoleMessage::Trace(blink::Visitor* visitor) {
  visitor->Trace(frame_);
}

}  // namespace blink
