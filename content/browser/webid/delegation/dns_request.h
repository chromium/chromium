// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_DELEGATION_DNS_REQUEST_H_
#define CONTENT_BROWSER_WEBID_DELEGATION_DNS_REQUEST_H_

#include "base/functional/callback.h"
#include "content/browser/webid/delegation/email_verifier_network_request_manager.h"
#include "content/common/content_export.h"
#include "url/origin.h"

namespace content::webid {

// Performs a DNS TXT query for a given origin.
class CONTENT_EXPORT DnsRequest {
 public:
  using NetworkRequestManagerGetter =
      base::RepeatingCallback<EmailVerifierNetworkRequestManager*()>;
  using DnsRequestCallback = base::OnceCallback<void(
      const std::optional<std::vector<std::string>>& text_records)>;

  explicit DnsRequest(
      NetworkRequestManagerGetter network_request_manager_getter);
  virtual ~DnsRequest();

  // Sends a DNS TXT request for the given hostname. The caller must ensure that
  // the `hostname` provided is valid.
  virtual void SendRequest(const std::string& hostname,
                           DnsRequestCallback callback);

 private:
  NetworkRequestManagerGetter network_manager_getter_;
};

}  // namespace content::webid

#endif  // CONTENT_BROWSER_WEBID_DELEGATION_DNS_REQUEST_H_
