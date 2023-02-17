// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_FEDERATED_IDENTITY_AUTO_REAUTHN_PERMISSION_CONTEXT_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_FEDERATED_IDENTITY_AUTO_REAUTHN_PERMISSION_CONTEXT_DELEGATE_H_

#include "content/common/content_export.h"

namespace url {
class Origin;
}

namespace content {

// Delegate interface for the FedCM implementation to query whether the FedCM
// API's auto re-authn is enabled in Site Settings.
class CONTENT_EXPORT FederatedIdentityAutoReauthnPermissionContextDelegate {
 public:
  FederatedIdentityAutoReauthnPermissionContextDelegate() = default;
  virtual ~FederatedIdentityAutoReauthnPermissionContextDelegate() = default;

  // Returns the permission status of the FedCM API's auto re-authn feature for
  // the passed-in |relying_party_embedder|.
  virtual bool HasAutoReauthnPermission(
      const url::Origin& relying_party_embedder) = 0;

  // Records that an auto re-authn prompt was displayed to the user and places
  // the permission under embargo for the passed-in |relying_party_embedder|.
  virtual void RecordDisplayAndEmbargo(
      const url::Origin& relying_party_embedder) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_FEDERATED_IDENTITY_AUTO_REAUTHN_PERMISSION_CONTEXT_DELEGATE_H_
