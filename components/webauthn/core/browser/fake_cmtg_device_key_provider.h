// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_CORE_BROWSER_FAKE_CMTG_DEVICE_KEY_PROVIDER_H_
#define COMPONENTS_WEBAUTHN_CORE_BROWSER_FAKE_CMTG_DEVICE_KEY_PROVIDER_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/webauthn/core/browser/cmtg_device_key_provider.h"

namespace webauthn {

// A fake implementation of CmtgDeviceKeyProvider for use in browsertests and
// unittests. Allows injecting successful key responses, specific error codes,
// or simulating a timeout by holding the callback.
class FakeCmtgDeviceKeyProvider : public CmtgDeviceKeyProvider {
 public:
  class RequestImpl : public CmtgDeviceKeyProvider::Request {
   public:
    RequestImpl();
    ~RequestImpl() override;
  };

  FakeCmtgDeviceKeyProvider();
  ~FakeCmtgDeviceKeyProvider() override;

  // CmtgDeviceKeyProvider:
  std::unique_ptr<Request> GetDeviceKeys(Callback callback) override;

  // Sets the keys that will be returned on the next call to GetDeviceKeys.
  void SetNextKeys(std::vector<std::vector<uint8_t>> keys);

  // Sets the error that will be returned on the next call to GetDeviceKeys.
  void SetNextError(std::optional<Error> error);

  // If true, incoming requests will not schedule the callback.
  void SetHoldCallback(bool hold);

  // Resolves the pending callback with keys.
  void ResolvePending(std::vector<std::vector<uint8_t>> keys);

  // Rejects the pending callback with error.
  void RejectPending(Error error);

  bool has_pending_callback() const { return !pending_callback_.is_null(); }

 private:
  // Executes the provided callback with either the configured next error or
  // keys.
  void DeliverResult(Callback callback);

  // The keys to be returned in the next callback execution.
  std::optional<std::vector<std::vector<uint8_t>>> next_keys_;

  // The error to be returned in the next callback execution.
  std::optional<Error> next_error_;

  // If true, incoming requests will not schedule the callback.
  bool hold_callback_ = false;

  // Stores the callback when `hold_callback_` is true, allowing tests to
  // manually trigger it via `ResolvePending` or `RejectPending`.
  Callback pending_callback_;

  base::WeakPtrFactory<FakeCmtgDeviceKeyProvider> weak_ptr_factory_{this};
};

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_CORE_BROWSER_FAKE_CMTG_DEVICE_KEY_PROVIDER_H_
