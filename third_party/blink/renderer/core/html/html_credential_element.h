// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_CREDENTIAL_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_CREDENTIAL_ELEMENT_H_

#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/html_element.h"

namespace blink {

// <credential> element is used to provide configuration for Federated
// Credential Management (FedCM) requests when used as a child of a <login>
// element.
// See https://github.com/fedidcg/login-element for the explainer.
class CORE_EXPORT HTMLCredentialElement : public HTMLElement {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit HTMLCredentialElement(Document&);

  // Returns the FederatedAuthRequest options derived from the element's
  // attributes (e.g. configURL, clientID, etc.). Returns null if the element is
  // not a valid credential configuration.
  mojom::blink::IdentityProviderRequestOptionsPtr GetFederatedRequestOptions()
      const;

 private:
  void ParseAttribute(const AttributeModificationParams&) override;
  bool IsURLAttribute(const Attribute&) const override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_CREDENTIAL_ELEMENT_H_
