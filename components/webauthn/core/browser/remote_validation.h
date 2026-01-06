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
#include "url/origin.h"

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
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
      base::OnceCallback<void(ValidationStatus)> callback);

  // ValidateWellKnownJSON implements the core of remote validation. It isn't
  // intended to be called externally except for testing.
  [[nodiscard]] static ValidationStatus ValidateWellKnownJSON(
      const url::Origin& caller_origin,
      std::string_view json);

 private:
  RemoteValidation(const url::Origin& caller_origin,
                   base::OnceCallback<void(ValidationStatus)> callback);

  void OnFetchComplete(std::optional<std::string> body);

  const url::Origin caller_origin_;
  base::OnceCallback<void(ValidationStatus)> callback_;
  std::unique_ptr<network::SimpleURLLoader> loader_;

  base::WeakPtrFactory<RemoteValidation> weak_factory_{this};
};

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_CORE_BROWSER_REMOTE_VALIDATION_H_
