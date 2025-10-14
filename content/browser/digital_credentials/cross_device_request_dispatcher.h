// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DIGITAL_CREDENTIALS_CROSS_DEVICE_REQUEST_DISPATCHER_H_
#define CONTENT_BROWSER_DIGITAL_CREDENTIALS_CROSS_DEVICE_REQUEST_DISPATCHER_H_

#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/types/expected.h"
#include "content/common/content_export.h"
#include "content/public/browser/cross_device_request_info.h"
#include "content/public/browser/digital_credentials_cross_device.h"
#include "device/fido/fido_discovery_base.h"

namespace device {
class FidoAuthenticator;
}

namespace content::digital_credentials::cross_device {

// A RequestDispatcher fetches an identity document from a mobile
// device.
class CONTENT_EXPORT RequestDispatcher
    : public device::FidoDiscoveryBase::Observer {
 public:
  using Error = std::variant<ProtocolError, RemoteError>;
  using CompletionCallback =
      base::OnceCallback<void(base::expected<Response, Error>)>;

  RequestDispatcher(std::unique_ptr<device::FidoDiscoveryBase> v1_discovery,
                    std::unique_ptr<device::FidoDiscoveryBase> v2_discovery,
                    RequestInfo request_info,
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
  RequestInfo request_info_;
  CompletionCallback callback_;

  SEQUENCE_CHECKER(my_sequence_checker_);
  base::WeakPtrFactory<RequestDispatcher> weak_factory_{this};
};

}  // namespace content::digital_credentials::cross_device

#endif  // CONTENT_BROWSER_DIGITAL_CREDENTIALS_CROSS_DEVICE_REQUEST_DISPATCHER_H_
