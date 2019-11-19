// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PORTAL_PORTAL_HOST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PORTAL_PORTAL_HOST_H_

#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/mojom/portal/portal.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/messaging/blink_transferable_message.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class Document;
class ExecutionContext;
class LocalDOMWindow;
class ScriptValue;
class SecurityOrigin;
class WindowPostMessageOptions;

class CORE_EXPORT PortalHost : public EventTargetWithInlineData,
                               public Supplement<LocalDOMWindow> {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(PortalHost);

 public:
  explicit PortalHost(LocalDOMWindow& window);

  void Trace(Visitor* visitor) override;

  static const char kSupplementName[];
  static PortalHost& From(LocalDOMWindow& window);

  // EventTarget overrides
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;
  PortalHost* ToPortalHost() override;

  Document* GetDocument() const;

  // Called immediately before dispatching the onactivate event.
  void OnPortalActivated();

  // idl implementation
  void postMessage(ScriptState* script_state,
                   const ScriptValue& message,
                   const String& target_origin,
                   HeapVector<ScriptValue>& transfer,
                   ExceptionState& exception_state);
  void postMessage(ScriptState* script_state,
                   const ScriptValue& message,
                   const WindowPostMessageOptions* options,
                   ExceptionState& exception_state);
  EventListener* onmessage();
  void setOnmessage(EventListener* listener);
  EventListener* onmessageerror();
  void setOnmessageerror(EventListener* listener);

  void ReceiveMessage(BlinkTransferableMessage message,
                      scoped_refptr<const SecurityOrigin> source_origin,
                      scoped_refptr<const SecurityOrigin> target_origin);

 private:
  mojom::blink::PortalHost& GetPortalHostInterface();

  mojo::AssociatedRemote<mojom::blink::PortalHost> portal_host_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PORTAL_PORTAL_HOST_H_
