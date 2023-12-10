// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_FEDERATED_IDENTITY_API_PERMISSION_CONTEXT_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_FEDERATED_IDENTITY_API_PERMISSION_CONTEXT_DELEGATE_H_

#include "content/common/content_export.h"
#include "url/gurl.h"

namespace url {
class Origin;
}

namespace content {

class RenderFrameHost;

// Delegate interface for the FedCM implementation to query whether the FedCM
// API is enabled in Site Settings.
class CONTENT_EXPORT FederatedIdentityApiPermissionContextDelegate {
 public:
  enum class PermissionStatus {
    GRANTED,
    BLOCKED_VARIATIONS,
    BLOCKED_SETTINGS,
    BLOCKED_EMBARGO,
  };

  FederatedIdentityApiPermissionContextDelegate() = default;
  virtual ~FederatedIdentityApiPermissionContextDelegate() = default;

  // Returns the status of the FedCM API for the passed-in
  // |relying_party_embedder|.
  virtual PermissionStatus GetApiPermissionStatus(
      const url::Origin& relying_party_embedder) = 0;

  // Records that the FedCM prompt was explicitly dismissed and places the
  // permission under embargo for the passed-in |relying_party_embedder|.
  virtual void RecordDismissAndEmbargo(
      const url::Origin& relying_party_embedder) = 0;

  // Clears any existing embargo status for the FEDERATED_IDENTITY_API
  // permission for the passed-in |relying_party_embedder|. Clears the dismiss
  // and ignore counts.
  virtual void RemoveEmbargoAndResetCounts(
      const url::Origin& relying_party_embedder) = 0;

  // This function is so we can avoid the delay in tests. It does not really
  // belong on this delegate but we don't have a better one and it seems
  // wasteful to add one just for this one testing function.
  virtual bool ShouldCompleteRequestImmediately() const;

  // Checks if the IdP has third-party cookies access on the RP top frame.
  virtual bool HasThirdPartyCookiesAccess(
      RenderFrameHost& host,
      const GURL& provider_url,
      const url::Origin& relying_party_embedder) const = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_FEDERATED_IDENTITY_API_PERMISSION_CONTEXT_DELEGATE_H_
