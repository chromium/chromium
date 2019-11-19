// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PORTAL_PORTAL_CONTENTS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PORTAL_PORTAL_CONTENTS_H_

#include "base/memory/scoped_refptr.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "services/network/public/mojom/referrer_policy.mojom-shared.h"
#include "third_party/blink/public/mojom/portal/portal.mojom-blink.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class Document;
class HTMLPortalElement;
class KURL;
class RemoteFrame;
class ScriptPromise;
class ScriptPromiseResolver;
class ScriptState;
class SecurityOrigin;
struct BlinkTransferableMessage;

// The contents of an HTMLPortalElement, corresponding to ownership of a
// content::WebContents.
//
// This object is usually owned by an HTMLPortalElement but may be briefly owned
// elsewhere. For example, during activation it is necessary to keep the Mojo
// connection to the browser-side portal state alive (so that the reply can be
// received), but this should not prevent the <portal> element from being reused
// in the meantime.
class PortalContents : public GarbageCollected<PortalContents>,
                       public mojom::blink::PortalClient {
 public:
  PortalContents(
      HTMLPortalElement& portal_element,
      const base::UnguessableToken& portal_token,
      mojo::PendingAssociatedRemote<mojom::blink::Portal> remote_portal,
      mojo::PendingAssociatedReceiver<mojom::blink::PortalClient>
          portal_client_receiver);
  ~PortalContents() override;

  // Returns true if this object corresponds to an existent portal contents.
  bool IsValid() const { return remote_portal_.is_bound(); }

  // Returns true if this contents is currently being activated.
  bool IsActivating() const { return activate_resolver_; }

  // Returns an unguessable token which uniquely identifies the contents, if
  // valid.
  const base::UnguessableToken& GetToken() const { return portal_token_; }

  // Returns the RemoteFrame associated with this portal, if any.
  RemoteFrame* GetFrame() const;

  // Activates the portal contents, and produces a promise which resolves when
  // complete. The caller is expected to do all necessary preflight checks in
  // advance.
  ScriptPromise Activate(ScriptState*, BlinkTransferableMessage data);

  // Posts a message which will be delivered in the guest contents via the
  // PortalHost object.
  void PostMessageToGuest(
      BlinkTransferableMessage message,
      const scoped_refptr<const SecurityOrigin>& target_origin);

  // Request navigation to the specified URL. May be a no-op if navigation to
  // this URL is not permitted.
  void Navigate(const KURL&, network::mojom::ReferrerPolicy);

  // Tears down the internal state of this object. If ownership has not been
  // transferred (via adoption), then the underlying contents will also be torn
  // down.
  void Destroy();

  // blink::mojom::PortalClient implementation
  void ForwardMessageFromGuest(
      BlinkTransferableMessage message,
      const scoped_refptr<const SecurityOrigin>& source_origin,
      const scoped_refptr<const SecurityOrigin>& target_origin) override;
  void DispatchLoadEvent() override;

  void Trace(Visitor* visitor);

 private:
  // Returns the document which controls the lifetime of this portal (usually,
  // the document of the HTMLPortalElement which owns this).
  Document& GetDocument() const { return *document_; }

  // Called on response to the request to activate the portal contents.
  void OnActivateResponse(mojom::blink::PortalActivateResult);

  // The document which owns this contents.
  // TODO(jbroman): Should this be a DocumentShutdownObserver instead?
  Member<Document> document_;

  // The element which owns this contents, if any.
  Member<HTMLPortalElement> portal_element_;

  // Set if the portal contents is currently being activated.
  // If so it will be the activating portal contents of the associated
  // DocumentPortals.
  Member<ScriptPromiseResolver> activate_resolver_;

  // Uniquely identifies the portal, this token is used by the browser process
  // to reference this portal when communicating with the renderer.
  base::UnguessableToken portal_token_;

  mojo::AssociatedRemote<mojom::blink::Portal> remote_portal_;
  mojo::AssociatedReceiver<mojom::blink::PortalClient> portal_client_receiver_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PORTAL_PORTAL_CONTENTS_H_
