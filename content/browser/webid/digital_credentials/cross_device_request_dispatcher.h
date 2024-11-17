// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_DIGITAL_CREDENTIALS_CROSS_DEVICE_REQUEST_DISPATCHER_H_
#define CONTENT_BROWSER_WEBID_DIGITAL_CREDENTIALS_CROSS_DEVICE_REQUEST_DISPATCHER_H_

#include <optional>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/types/expected.h"
#include "content/common/content_export.h"
#include "content/public/browser/digital_credentials_cross_device.h"
#include "device/fido/fido_discovery_base.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace device {
class FidoAuthenticator;
}

namespace content::digital_credentials::cross_device {

// A RequestDispatcher fetches an identity document from a mobile
// device.
class CONTENT_EXPORT RequestDispatcher : device::FidoDiscoveryBase::Observer {
 public:
  using Error = absl::variant<ProtocolError, RemoteError>;
  using CompletionCallback =
      base::OnceCallback<void(base::expected<Response, Error>)>;

  RequestDispatcher(std::unique_ptr<device::FidoDiscoveryBase> v1_discovery,
                    std::unique_ptr<device::FidoDiscoveryBase> v2_discovery,
                    url::Origin origin,
                    base::Value request,
                    CompletionCallback callback);

  RequestDispatcher(const RequestDispatcher&) = delete;
  RequestDispatcher& operator=(const RequestDispatcher&) = delete;

  ~RequestDispatcher() override;

 private:
  // FidoDiscoveryBase::Observer:
  void AuthenticatorAdded(device::FidoDiscoveryBase* discovery,
                          device::FidoAuthenticator* authenticator) override;
  void AuthenticatorRemoved(device::FidoDiscoveryBase* discovery,
                            device::FidoAuthenticator* authenticator) override;

  void OnAuthenticatorReady(device::FidoAuthenticator* authenticator);
  void OnComplete(std::optional<std::vector<uint8_t>> response);

  const std::unique_ptr<device::FidoDiscoveryBase> v1_discovery_;
  const std::unique_ptr<device::FidoDiscoveryBase> v2_discovery_;
  const url::Origin origin_;
  base::Value request_;
  CompletionCallback callback_;

  SEQUENCE_CHECKER(my_sequence_checker_);
  base::WeakPtrFactory<RequestDispatcher> weak_factory_{this};
};

}  // namespace content::digital_credentials::cross_device

#endif  // CONTENT_BROWSER_WEBID_DIGITAL_CREDENTIALS_CROSS_DEVICE_REQUEST_DISPATCHER_H_
