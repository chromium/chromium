// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_FEDERATED_IDENTITY_API_PERMISSION_CONTEXT_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_FEDERATED_IDENTITY_API_PERMISSION_CONTEXT_DELEGATE_H_

namespace url {
class Origin;
}

namespace content {

// Delegate interface for the FedCM implementation to query whether the FedCM
// API is enabled in Site Settings.
class FederatedIdentityApiPermissionContextDelegate {
 public:
  enum class PermissionStatus {
    GRANTED,
    BLOCKED_VARIATIONS,
    BLOCKED_THIRD_PARTY_COOKIES_BLOCKED,
    BLOCKED_SETTINGS,
    BLOCKED_EMBARGO,
  };

  FederatedIdentityApiPermissionContextDelegate() = default;
  virtual ~FederatedIdentityApiPermissionContextDelegate() = default;

  // Returns the status of the FedCM API for the passed-in |rp_origin|.
  virtual PermissionStatus GetApiPermissionStatus(
      const url::Origin& rp_origin) = 0;

  // Records that the FedCM prompt was explicitly dismissed and places the
  // permission under embargo for the passed-in |rp_origin|.
  virtual void RecordDismissAndEmbargo(const url::Origin& rp_origin) = 0;

  // Clears any existing embargo status for |url| for the FEDERATED_IDENTITY_API
  // permission for the passed-in |rp_origin|. Clears the dismiss and ignore
  // counts.
  virtual void RemoveEmbargoAndResetCounts(const url::Origin& rp_origin) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_FEDERATED_IDENTITY_API_PERMISSION_CONTEXT_DELEGATE_H_
