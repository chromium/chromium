// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_CORE_BROWSER_CMTG_DEVICE_KEY_PROVIDER_H_
#define COMPONENTS_WEBAUTHN_CORE_BROWSER_CMTG_DEVICE_KEY_PROVIDER_H_

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "components/keyed_service/core/keyed_service.h"

namespace webauthn {

// Interface representing a service that vends device keys for Credential
// Manager Trust Group (CMTG) key operations.
class CmtgDeviceKeyProvider : public KeyedService {
 public:
  enum class Error {
    kNetworkError,
  };

  enum class Operation {
    // Calls GetCmtgWrapperKeys to fetch wrapper keys for all devices in the
    // trust group.
    kGetAssertion,
    // Calls GetOrCreateCmtgWrapperKey to fetch or create the calling device's
    // own wrapper key.
    kMakeCredential,
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

  ~CmtgDeviceKeyProvider() override = default;

  // Fetches the device keys for the given `operation`.
  [[nodiscard]] virtual std::unique_ptr<Request> GetDeviceKeys(
      Operation operation,
      Callback callback) = 0;
};

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_CORE_BROWSER_CMTG_DEVICE_KEY_PROVIDER_H_
