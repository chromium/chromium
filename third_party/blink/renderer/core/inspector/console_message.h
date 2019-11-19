// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_CONSOLE_MESSAGE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_CONSOLE_MESSAGE_H_

#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class DocumentLoader;
class LocalFrame;
class SourceLocation;
class WorkerThread;
struct WebConsoleMessage;

class CORE_EXPORT ConsoleMessage final
    : public GarbageCollected<ConsoleMessage> {
 public:
  // Location must be non-null.
  static ConsoleMessage* Create(mojom::ConsoleMessageSource,
                                mojom::ConsoleMessageLevel,
                                const String& message,
                                std::unique_ptr<SourceLocation>);

  // Shortcut when location is unknown. Captures current location.
  static ConsoleMessage* Create(mojom::ConsoleMessageSource,
                                mojom::ConsoleMessageLevel,
                                const String& message);

  // This method captures current location if available.
  static ConsoleMessage* CreateForRequest(mojom::ConsoleMessageSource,
                                          mojom::ConsoleMessageLevel,
                                          const String& message,
                                          const String& url,
                                          DocumentLoader*,
                                          uint64_t request_identifier);

  // This creates message from WorkerMessageSource.
  static ConsoleMessage* CreateFromWorker(mojom::ConsoleMessageLevel,
                                          const String& message,
                                          std::unique_ptr<SourceLocation>,
                                          WorkerThread*);

  // Creates a ConsoleMessage from a similar WebConsoleMessage.
  static ConsoleMessage* CreateFromWebConsoleMessage(const WebConsoleMessage&,
                                                     LocalFrame*);

  ConsoleMessage(mojom::ConsoleMessageSource,
                 mojom::ConsoleMessageLevel,
                 const String& message,
                 std::unique_ptr<SourceLocation>);
  ~ConsoleMessage();

  SourceLocation* Location() const;
  const String& RequestIdentifier() const;
  double Timestamp() const;
  mojom::ConsoleMessageSource Source() const;
  mojom::ConsoleMessageLevel Level() const;
  const String& Message() const;
  const String& WorkerId() const;
  LocalFrame* Frame() const;
  Vector<DOMNodeId>& Nodes();
  void SetNodes(LocalFrame*, Vector<DOMNodeId> nodes);

  void Trace(blink::Visitor*);

 private:
  mojom::ConsoleMessageSource source_;
  mojom::ConsoleMessageLevel level_;
  String message_;
  std::unique_ptr<SourceLocation> location_;
  String request_identifier_;
  double timestamp_;
  String worker_id_;
  WeakMember<LocalFrame> frame_;
  Vector<DOMNodeId> nodes_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_CONSOLE_MESSAGE_H_
