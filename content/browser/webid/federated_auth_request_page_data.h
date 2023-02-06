// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_FEDERATED_AUTH_REQUEST_PAGE_DATA_H_
#define CONTENT_BROWSER_WEBID_FEDERATED_AUTH_REQUEST_PAGE_DATA_H_

#include "content/public/browser/page_user_data.h"

namespace content {
class FederatedAuthRequestImpl;

class FederatedAuthRequestPageData
    : public PageUserData<FederatedAuthRequestPageData> {
 public:
  ~FederatedAuthRequestPageData() override = default;

  // The currently pending web identity request, if any.
  // Used to ensure that we do not allow two separate calls on the same page.
  FederatedAuthRequestImpl* PendingWebIdentityRequest();
  // Sets the pending web identity request, or nullptr when a pending request
  // has finished.
  void SetPendingWebIdentityRequest(FederatedAuthRequestImpl* request);

 private:
  explicit FederatedAuthRequestPageData(Page& page);

  friend class PageUserData<FederatedAuthRequestPageData>;
  PAGE_USER_DATA_KEY_DECL();

  // Non-null when there is some Web Identity API request currently pending.
  // Used to ensure that we do not allow two separate calls on the same page
  // and to access the currently pending request.
  raw_ptr<FederatedAuthRequestImpl> pending_web_identity_request_ = nullptr;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_FEDERATED_AUTH_REQUEST_PAGE_DATA_H_
