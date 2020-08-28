// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_DOCUMENT_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_DOCUMENT_DATA_H_

#include "services/network/public/mojom/trust_tokens.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_regexp.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {

// The purpose of blink::DocumentData is to reduce the size of document.h.
// Data members which require huge headers should be stored in
// blink::DocumentData instead of blink::Document.
//
// Ownership: A Document has a strong reference to a single DocumentData.
//   Other instances should not have strong references to the DocumentData.
// Lifetime: A DocumentData instance is created on a Document creation, and
//   is never destructed before the Document.
class DocumentData final : public GarbageCollected<DocumentData> {
 public:
  explicit DocumentData(ExecutionContext* context)
      : permission_service_(context), has_trust_tokens_answerer_(context) {}

  void Trace(Visitor* visitor) const {
    visitor->Trace(permission_service_);
    visitor->Trace(has_trust_tokens_answerer_);
    visitor->Trace(pending_has_trust_tokens_resolvers_);
  }

 private:
  // Mojo remote used to determine if the document has permission to access
  // storage or not.
  HeapMojoRemote<mojom::blink::PermissionService> permission_service_;

  // Mojo remote used to answer API calls asking whether the user has trust
  // tokens (https://github.com/wicg/trust-token-api). The other endpoint
  // is in the network service, which may crash and restart. To handle this:
  //   1. |pending_has_trust_tokens_resolvers_| keeps track of promises
  // depending on |has_trust_tokens_answerer_|'s answers;
  //   2. |HasTrustTokensAnswererConnectionError| handles connection errors by
  // rejecting all pending promises and clearing the pending set.
  HeapMojoRemote<network::mojom::blink::HasTrustTokensAnswerer>
      has_trust_tokens_answerer_;

  // In order to be able to answer promises when the Mojo remote disconnects,
  // maintain all pending promises here, deleting them on successful completion
  // or on connection error, whichever comes first.
  HeapHashSet<Member<ScriptPromiseResolver>>
      pending_has_trust_tokens_resolvers_;

  // To do email regex checks.
  std::unique_ptr<ScriptRegexp> email_regexp_;

  friend class Document;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_DOCUMENT_DATA_H_
