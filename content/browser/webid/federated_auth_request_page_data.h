// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_FEDERATED_AUTH_REQUEST_PAGE_DATA_H_
#define CONTENT_BROWSER_WEBID_FEDERATED_AUTH_REQUEST_PAGE_DATA_H_

#include <memory>

#include "base/time/time.h"
#include "content/public/browser/page_user_data.h"
#include "url/gurl.h"

namespace content {
class FederatedAuthRequestImpl;

class CONTENT_EXPORT FederatedAuthRequestPageData
    : public PageUserData<FederatedAuthRequestPageData> {
 public:
  ~FederatedAuthRequestPageData() override;

  // The currently pending web identity request, if any.
  // Used to ensure that we do not allow two separate calls on the same page.
  FederatedAuthRequestImpl* PendingWebIdentityRequest();
  // Sets the pending web identity request, or nullptr when a pending request
  // has finished.
  void SetPendingWebIdentityRequest(FederatedAuthRequestImpl* request);
  // Sets the accounts response time from the User Info API.
  void SetUserInfoAccountsResponseTime(const GURL& idp_url,
                                       const base::TimeTicks& time);
  // Gets the accounts response time from the User Info API. This is used in the
  // active flow where we measure the time gap between a User Info API call and
  // an active mode API call. Returns nullopt if no such response is available.
  // Once returned, the entry will be erased to avoid incorrect counting. e.g. a
  // user may close the active modal and trigger it again in which case we only
  // want to record the first one.
  std::optional<base::TimeTicks> ConsumeUserInfoAccountsResponseTime(
      const GURL& idp_url);

 private:
  explicit FederatedAuthRequestPageData(Page& page);

  friend class PageUserData<FederatedAuthRequestPageData>;
  PAGE_USER_DATA_KEY_DECL();

  // Non-null when there is some Web Identity API request currently pending.
  // Used to ensure that we do not allow two separate calls on the same page
  // and to access the currently pending request.
  raw_ptr<FederatedAuthRequestImpl> pending_web_identity_request_ = nullptr;

  // Time when the browser receives valid accounts from the IdP via the UserInfo
  // API.
  base::flat_map<GURL, base::TimeTicks> user_info_accounts_response_time_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_FEDERATED_AUTH_REQUEST_PAGE_DATA_H_
