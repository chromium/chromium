// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_REQUEST_PAGE_DATA_H_
#define CONTENT_BROWSER_WEBID_REQUEST_PAGE_DATA_H_

#include <memory>

#include "base/time/time.h"
#include "content/public/browser/page_user_data.h"
#include "url/gurl.h"

namespace content {

namespace webid {
class RequestService;
}

namespace webid {

using OnFederatedTokenReceivedCallback = base::OnceCallback<void(bool)>;

class CONTENT_EXPORT RequestPageData : public PageUserData<RequestPageData> {
 public:
  ~RequestPageData() override;

  // The currently pending web identity request, if any.
  // Used to ensure that we do not allow two separate calls on the same page.
  webid::RequestService* PendingWebIdentityRequest();
  // Sets the pending web identity request, or nullptr when a pending request
  // has finished.
  void SetPendingWebIdentityRequest(webid::RequestService* request);
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

  // Represents an embedder login request. The embedder may choose to request
  // a federated token from a specific account, and request to be notified when
  // the request is completed.
  struct EmbedderLoginRequest {
    EmbedderLoginRequest(const GURL& idp_url,
                         const std::string& account_id,
                         OnFederatedTokenReceivedCallback callback);
    EmbedderLoginRequest(const EmbedderLoginRequest&) = delete;
    EmbedderLoginRequest& operator=(const EmbedderLoginRequest&) = delete;
    EmbedderLoginRequest(EmbedderLoginRequest&&);
    EmbedderLoginRequest& operator=(EmbedderLoginRequest&&) = default;
    ~EmbedderLoginRequest();

    GURL idp_url;
    std::string account_id;
    OnFederatedTokenReceivedCallback on_federated_token_received_callback;
  };

  // Sets the embedder login request information. This is used to know whether a
  // current pending web identity request is an embedder login request, which
  // account to automatically select, and how to notify the embedder.
  // std::nullopt indicates that there is no active embedder login request.
  void SetEmbedderLoginRequest(std::optional<EmbedderLoginRequest> request);
  const std::optional<EmbedderLoginRequest>& GetEmbedderLoginRequest();

 private:
  explicit RequestPageData(Page& page);

  friend class PageUserData<RequestPageData>;
  PAGE_USER_DATA_KEY_DECL();

  // Non-null when there is some Web Identity API request currently pending.
  // Used to ensure that we do not allow two separate calls on the same page
  // and to access the currently pending request.
  raw_ptr<webid::RequestService> pending_web_identity_request_ = nullptr;

  // Time when the browser receives valid accounts from the IdP via the UserInfo
  // API.
  base::flat_map<GURL, base::TimeTicks> user_info_accounts_response_time_;

  // Information about the embedder login request.
  std::optional<EmbedderLoginRequest> embedder_login_request_;
};

}  // namespace webid
}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_REQUEST_PAGE_DATA_H_
