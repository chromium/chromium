// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_CORE_BROWSER_FAKE_CMTG_DEVICE_KEY_PROVIDER_H_
#define COMPONENTS_WEBAUTHN_CORE_BROWSER_FAKE_CMTG_DEVICE_KEY_PROVIDER_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/webauthn/core/browser/cmtg_device_key_provider.h"
#include "net/http/http_status_code.h"

namespace network {
struct ResourceRequest;
}  // namespace network

namespace webauthn {

// A fake implementation of CmtgDeviceKeyProvider for use in browsertests and
// unittests.
//
// The fake models the CryptAuth CMTG wrapper key service. It keeps track of a
// set of devices, each in a trust group, and answers as the current device
// (initially device 0). Each device's key is created the first time the device
// fetches keys. Making a credential returns the current device's key, and
// getting an assertion returns the current device's key followed by the keys of
// the other devices in its trust group. Tests can also inject a specific
// response, an error, or simulate a timeout by holding the callback.
//
// The fake can be used directly as the CmtgDeviceKeyProvider, or answer network
// requests sent to the service (see `GetCallback()`) so that tests can exercise
// the real CryptauthCmtgDeviceKeyProvider. Both modes are configured with the
// same methods.
class FakeCmtgDeviceKeyProvider : public CmtgDeviceKeyProvider {
 public:
  class RequestImpl : public CmtgDeviceKeyProvider::Request {
   public:
    RequestImpl();
    ~RequestImpl() override;
  };

  // The ID of the device that requests come from initially.
  static constexpr size_t kInitialDevice = 0;

  FakeCmtgDeviceKeyProvider();
  ~FakeCmtgDeviceKeyProvider() override;

  // If present, values of this type contain an HTTP status code (e.g. 200) and
  // the body of the response.
  using MaybeResponse =
      std::optional<std::pair<net::HttpStatusCode, std::string>>;

  // CmtgDeviceKeyProvider:
  std::unique_ptr<Request> GetDeviceKeys(Operation operation,
                                         Callback callback) override;

  // Returns a callback that processes network requests and, if they are for
  // the production CryptAuth CMTG wrapper key service, returns a response built
  // from the configured keys or error. Held requests (see `SetHoldCallback`)
  // are never answered.
  base::RepeatingCallback<MaybeResponse(const network::ResourceRequest&)>
  GetCallback();

  // Adds a new device in its own trust group and returns its ID.
  size_t AddDevice();

  // Merges the trust groups of `device_a` and `device_b`.
  void MergeTrustGroups(size_t device_a, size_t device_b);

  // Sets the device that subsequent requests come from.
  void SetCurrentDevice(size_t device);

  // Overrides the keys that will be returned on the next call to GetDeviceKeys
  // or the next network request, e.g. to simulate a misbehaving server.
  void SetNextKeys(std::vector<std::vector<uint8_t>> keys);

  // Sets the error that will be returned on the next call to GetDeviceKeys or
  // the next network request.
  void SetNextError(Error error);

  // If true, calls to GetDeviceKeys store the callback until `ResolvePending`
  // or `RejectPending` is called, and network requests are never answered.
  void SetHoldCallback(bool hold);

  // Resolves the pending callback with keys.
  void ResolvePending(std::vector<std::vector<uint8_t>> keys);

  // Rejects the pending callback with error.
  void RejectPending(Error error);

  bool has_pending_callback() const { return !pending_callback_.is_null(); }

 private:
  // A CMTG wrapper key known to the fake service.
  struct Key {
    Key(std::vector<uint8_t> key, base::Time create_time);
    Key(const Key&);
    Key& operator=(const Key&);
    ~Key();

    std::vector<uint8_t> key;
    base::Time create_time;
  };

  // A device known to the fake service.
  struct Device {
    explicit Device(size_t trust_group);
    Device(const Device&);
    Device& operator=(const Device&);
    ~Device();

    // An opaque identifier for the device's trust group. Devices in the same
    // trust group share the same value.
    size_t trust_group;

    // The device's key. Created the first time the device fetches keys.
    std::optional<Key> key;
  };

  // Executes the provided callback with the next result for `operation`.
  void DeliverResult(Operation operation, Callback callback);

  // Answers a network request sent to the CryptAuth CMTG wrapper key service.
  MaybeResponse OnRequest(const network::ResourceRequest& request);

  // Returns the current device's key, creating it if needed.
  const Key& GetOrCreateCurrentDeviceKey();

  // Returns the overridden next result if set, clearing it. Otherwise returns
  // the keys the service would return for `operation`.
  base::expected<std::vector<Key>, Error> TakeNextResult(Operation operation);

  // Overrides the result of the next callback execution or network request.
  std::optional<base::expected<std::vector<std::vector<uint8_t>>, Error>>
      next_result_;

  // If true, calls to GetDeviceKeys will not schedule the callback, and network
  // requests will not be answered.
  bool hold_callback_ = false;

  // Stores the callback when `hold_callback_` is true, allowing tests to
  // manually trigger it via `ResolvePending` or `RejectPending`.
  Callback pending_callback_;

  // The devices known to the fake service, indexed by device ID.
  std::vector<Device> devices_ = {Device(/*trust_group=*/kInitialDevice)};

  // The ID of the device that requests come from.
  size_t current_device_ = kInitialDevice;

  base::WeakPtrFactory<FakeCmtgDeviceKeyProvider> weak_ptr_factory_{this};
};

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_CORE_BROWSER_FAKE_CMTG_DEVICE_KEY_PROVIDER_H_
