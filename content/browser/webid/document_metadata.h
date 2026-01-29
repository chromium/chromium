// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_DOCUMENT_METADATA_H_
#define CONTENT_BROWSER_WEBID_DOCUMENT_METADATA_H_

#include <vector>

#include "base/functional/callback.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/document_metadata/document_metadata.mojom.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"

namespace content {
class RenderFrameHost;
}

namespace content::webid {

// DocumentMetadata provides access to the structured login metadata of a
// document. It currently supports retrievining https://schema.org/LoginAction
// entities from the document declared in JSON-LD.
class CONTENT_EXPORT DocumentMetadata {
 public:
  using LoginActionsCallback = base::OnceCallback<void(
      std::vector<blink::mojom::IdentityProviderGetParametersPtr>)>;

  explicit DocumentMetadata(RenderFrameHost* rfh);
  ~DocumentMetadata();

  DocumentMetadata(const DocumentMetadata&) = delete;
  DocumentMetadata& operator=(const DocumentMetadata&) = delete;

  // Requests the federated LoginAction entities from the document converted
  // into FedCM's IdentityProviderGetParameters.
  void GetLoginActions(LoginActionsCallback callback);

 private:
  void OnGetEntities(LoginActionsCallback callback,
                     blink::mojom::WebPagePtr page);

  mojo::Remote<blink::mojom::DocumentMetadata> metadata_remote_;
};

}  // namespace content::webid

#endif  // CONTENT_BROWSER_WEBID_DOCUMENT_METADATA_H_
