// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_FEDERATED_AUTH_REQUEST_PAGE_DATA_H_
#define CONTENT_BROWSER_WEBID_FEDERATED_AUTH_REQUEST_PAGE_DATA_H_

#include "content/public/browser/page_user_data.h"

namespace content {

class FederatedAuthRequestPageData
    : public PageUserData<FederatedAuthRequestPageData> {
 public:
  ~FederatedAuthRequestPageData() override = default;

  // Whether there is some Web Identity API request currently pending on |this|.
  // Used to ensure that we do not allow two separate calls on the same page.
  bool HasPendingWebIdentityRequest();
  // Sets whether we have some Web Identity API request.
  void SetHasPendingWebIdentityRequest(bool has_pending_request);

 private:
  explicit FederatedAuthRequestPageData(Page& page);

  friend class PageUserData<FederatedAuthRequestPageData>;
  PAGE_USER_DATA_KEY_DECL();

  // Whether there is some Web Identity API request currently pending. Used to
  // ensure that we do not allow two separate calls on the same page.
  bool has_pending_web_identity_request_ = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_FEDERATED_AUTH_REQUEST_PAGE_DATA_H_
