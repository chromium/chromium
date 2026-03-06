// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEBID_FEDERATED_EMBEDDER_LOGIN_REQUEST_H_
#define CONTENT_PUBLIC_BROWSER_WEBID_FEDERATED_EMBEDDER_LOGIN_REQUEST_H_

#include <string>

#include "base/functional/callback.h"
#include "base/timer/timer.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/origin.h"

namespace content {
class WebContents;
}

namespace content::webid {

enum class FederatedLoginResult;

// Represents an embedder-initiated login request (e.g., from the browser's
// Actor feature). The embedder may choose to request a federated token from a
// specific account, and request to be notified when the request is completed.
class CONTENT_EXPORT FederatedEmbedderLoginRequest
    : public WebContentsUserData<FederatedEmbedderLoginRequest> {
 public:
  FederatedEmbedderLoginRequest(
      WebContents* web_contents,
      const url::Origin& idp_origin,
      const std::string& account_id,
      base::OnceCallback<void(FederatedLoginResult)> callback);
  FederatedEmbedderLoginRequest(const FederatedEmbedderLoginRequest&) = delete;
  FederatedEmbedderLoginRequest& operator=(
      const FederatedEmbedderLoginRequest&) = delete;
  ~FederatedEmbedderLoginRequest() override;

  const url::Origin& idp_origin() const { return idp_origin_; }
  const std::string& account_id() const { return account_id_; }
  void OnFederatedResultReceived(FederatedLoginResult result);

  // Sets the embedder login request information. This is used to know whether a
  // current pending web identity request is an embedder login request, which
  // account to automatically select, and how to notify the embedder.
  static void Set(WebContents* web_contents,
                  const url::Origin& idp_origin,
                  const std::string& account_id,
                  base::OnceCallback<void(FederatedLoginResult)> callback);

  // Retrieves the embedder login request for the given WebContents. Checks the
  // opener chain if not found in the given WebContents.
  static FederatedEmbedderLoginRequest* Get(WebContents* web_contents);

  WEB_CONTENTS_USER_DATA_KEY_DECL();

 private:
  void Unset();
  void OnTimeout();

  url::Origin idp_origin_;
  std::string account_id_;
  base::OnceCallback<void(FederatedLoginResult)>
      on_federated_result_received_callback_;
  base::OneShotTimer timeout_timer_;
};

}  // namespace content::webid

#endif  // CONTENT_PUBLIC_BROWSER_WEBID_FEDERATED_EMBEDDER_LOGIN_REQUEST_H_
