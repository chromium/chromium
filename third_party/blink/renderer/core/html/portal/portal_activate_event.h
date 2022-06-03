// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PORTAL_PORTAL_ACTIVATE_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PORTAL_PORTAL_ACTIVATE_EVENT_H_

#include "base/unguessable_token.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/portal/portal.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/unpacked_serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/world_safe_v8_reference.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_v8_reference.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class Document;
class HTMLPortalElement;
class LocalFrame;
class PortalActivateEventInit;
class ScriptState;
class ScriptValue;
using OnPortalActivatedCallback =
    base::OnceCallback<void(mojom::blink::PortalActivateResult)>;

class CORE_EXPORT PortalActivateEvent : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // For use by Blink.
  static PortalActivateEvent* Create(
      LocalFrame* frame,
      const PortalToken& predecessor_portal_token,
      mojo::PendingAssociatedRemote<mojom::blink::Portal> predecessor_portal,
      mojo::PendingAssociatedReceiver<mojom::blink::PortalClient>
          predecessor_portal_client_receiver,
      scoped_refptr<SerializedScriptValue> data,
      MessagePortArray* ports,
      OnPortalActivatedCallback callback);

  // Web-exposed and called directly by authors.
  static PortalActivateEvent* Create(const AtomicString& type,
                                     const PortalActivateEventInit*);

  PortalActivateEvent(
      Document* document,
      const PortalToken& predecessor_portal_token,
      mojo::PendingAssociatedRemote<mojom::blink::Portal> predecessor_portal,
      mojo::PendingAssociatedReceiver<mojom::blink::PortalClient>
          predecessor_portal_client_receiver,
      UnpackedSerializedScriptValue* data,
      MessagePortArray*,
      OnPortalActivatedCallback callback);
  PortalActivateEvent(const AtomicString& type, const PortalActivateEventInit*);

  ~PortalActivateEvent() override;

  void Trace(Visitor*) const override;

  // Event overrides
  const AtomicString& InterfaceName() const override;

  // IDL implementation.
  ScriptValue data(ScriptState*);
  HTMLPortalElement* adoptPredecessor(ExceptionState& exception_state);

  // Invoked when this event should no longer keep its guest contents alive
  // due to the portalactivate event.
  void ExpireAdoptionLifetime();

 private:
  Member<Document> document_;
  Member<HTMLPortalElement> adopted_portal_;

  PortalToken predecessor_portal_token_;
  mojo::PendingAssociatedRemote<mojom::blink::Portal> predecessor_portal_;
  mojo::PendingAssociatedReceiver<mojom::blink::PortalClient>
      predecessor_portal_client_receiver_;

  // Set if this came from a serialized value.
  Member<UnpackedSerializedScriptValue> data_;
  Member<MessagePortArray> ports_;

  // Set if this came from an init dictionary.
  WorldSafeV8Reference<v8::Value> data_from_init_;

  // Per-ScriptState cache of the data, derived either from |data_| or
  // |data_from_init_|.
  HeapHashMap<WeakMember<ScriptState>, TraceWrapperV8Reference<v8::Value>>
      v8_data_;
  OnPortalActivatedCallback on_portal_activated_callback_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PORTAL_PORTAL_ACTIVATE_EVENT_H_
