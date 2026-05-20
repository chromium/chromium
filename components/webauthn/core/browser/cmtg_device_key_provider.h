// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_CORE_BROWSER_CMTG_DEVICE_KEY_PROVIDER_H_
#define COMPONENTS_WEBAUTHN_CORE_BROWSER_CMTG_DEVICE_KEY_PROVIDER_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/types/expected.h"

namespace webauthn {

// Interface representing a service that vends device keys for Credential
// Manager Trust Group (CMTG) key operations.
class CmtgDeviceKeyProvider {
 public:
  enum class Error {
    kNetworkError,
  };

  using Callback = base::OnceCallback<void(
      base::expected<std::vector<std::vector<uint8_t>>, Error>)>;

  // Handle to an ongoing asynchronous request. Destroying this object cancels
  // the request before its callback executes.
  class Request {
   public:
    Request() = default;
    Request(const Request&) = delete;
    Request& operator=(const Request&) = delete;
    virtual ~Request() = default;
  };

  virtual ~CmtgDeviceKeyProvider() = default;

  // Fetches the device keys.
  [[nodiscard]] virtual std::unique_ptr<Request> GetDeviceKeys(
      Callback callback) = 0;
};

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_CORE_BROWSER_CMTG_DEVICE_KEY_PROVIDER_H_
