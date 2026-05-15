// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_CORE_BROWSER_REMOTE_VALIDATION_H_
#define COMPONENTS_WEBAUTHN_CORE_BROWSER_REMOTE_VALIDATION_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/webauthn/core/browser/webauthn_security_utils.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {
struct RedirectInfo;
}  // namespace net

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
namespace mojom {
class URLResponseHead;
}  // namespace mojom
}  // namespace network

namespace webauthn {

// A RemoteValidation represents a pending remote validation of an RP ID.
class RemoteValidation {
 public:
  ~RemoteValidation();

  // Create and start a remote validation. The `callback` argument may be
  // invoked before this function returns if the network request could not be
  // started. In that case, the return value will be `nullptr`. Otherwise the
  // caller should hold the result and wait for |callback| to be invoked. If
  // the return value is destroyed then the fetch will be canceled and
  // |callback| will never be invoked.
  static std::unique_ptr<RemoteValidation> Create(
      const url::Origin& caller_origin,
      const std::string& relying_party_id,
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
      std::vector<network::mojom::ContentSecurityPolicyPtr>
          content_security_policies,
      base::OnceClosure log_use_counter_callback,
      base::OnceCallback<void(ValidationStatus)> callback);

  // ValidateWellKnownJSON implements the core of remote validation. It isn't
  // intended to be called externally except for testing.
  [[nodiscard]] static ValidationStatus ValidateWellKnownJSON(
      const url::Origin& caller_origin,
      std::string_view json);

 private:
  RemoteValidation(const url::Origin& caller_origin,
                   std::vector<network::mojom::ContentSecurityPolicyPtr>
                       content_security_policies,
                   base::OnceClosure log_use_counter_callback,
                   base::OnceCallback<void(ValidationStatus)> callback);

  void OnFetchComplete(std::optional<std::string> body);

  void OnRedirect(const GURL& url_before_redirects,
                  const net::RedirectInfo& redirect_info,
                  const network::mojom::URLResponseHead& response_head,
                  std::vector<std::string>* removed_headers);

  void CheckCsp(const GURL& url, bool has_followed_redirect);

  const url::Origin caller_origin_;
  const std::vector<network::mojom::ContentSecurityPolicyPtr>
      content_security_policies_;
  base::OnceClosure log_use_counter_callback_;
  base::OnceCallback<void(ValidationStatus)> callback_;
  std::unique_ptr<network::SimpleURLLoader> loader_;

  // This is tracked only for logging purposes for now.
  bool was_disallowed_by_csp_ = false;

  base::WeakPtrFactory<RemoteValidation> weak_factory_{this};
};

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_CORE_BROWSER_REMOTE_VALIDATION_H_
