// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_MOCK_FIDO_DEVICE_H_
#define DEVICE_FIDO_MOCK_FIDO_DEVICE_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/time/time.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_device.h"
#include "device/fido/fido_transport_protocol.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace cbor {
class Value;
}

namespace device {

class MockFidoDevice : public ::testing::StrictMock<FidoDevice> {
 public:
  // MakeU2f returns a fully initialized U2F device. This represents the state
  // after |DiscoverSupportedProtocolAndDeviceInfo| has been called by the
  // FidoDeviceDiscovery.
  static std::unique_ptr<MockFidoDevice> MakeU2f();
  // MakeCtap returns a fully initialized CTAP device. This represents the
  // state after |DiscoverSupportedProtocolAndDeviceInfo| has been called by
  // the FidoDeviceDiscovery.
  static std::unique_ptr<MockFidoDevice> MakeCtap(
      std::optional<AuthenticatorGetInfoResponse> device_info = std::nullopt);
  // MakeU2fWithDeviceInfoExpectation returns a uninitialized U2F device
  // suitable for injecting into a FidoDeviceDiscovery, which will determine its
  // protocol version by invoking |DiscoverSupportedProtocolAndDeviceInfo|.
  static std::unique_ptr<MockFidoDevice> MakeU2fWithGetInfoExpectation();
  // MakeCtapWithDeviceInfoExpectation returns a uninitialized CTAP device
  // suitable for injecting into a FidoDeviceDiscovery, which will determine its
  // protocol version by invoking |DiscoverSupportedProtocolAndDeviceInfo|. If a
  // response is supplied, the mock will use that to reply; otherwise it will
  // use |test_data::kTestAuthenticatorGetInfoResponse|.
  static std::unique_ptr<MockFidoDevice> MakeCtapWithGetInfoExpectation(
      std::optional<base::span<const uint8_t>> get_info_response =
          std::nullopt);
  // EncodeCBORRequest is a helper function for use with the |Expect*|
  // functions, below, that take a serialised request.
  static std::vector<uint8_t> EncodeCBORRequest(
      std::pair<CtapRequestCommand, std::optional<cbor::Value>> request);

  MockFidoDevice();
  MockFidoDevice(ProtocolVersion protocol_version,
                 std::optional<AuthenticatorGetInfoResponse> device_info);

  MockFidoDevice(const MockFidoDevice&) = delete;
  MockFidoDevice& operator=(const MockFidoDevice&) = delete;

  ~MockFidoDevice() override;

  // TODO(crbug.com/40524294): Remove these workarounds once support for
  // move-only types is added to GMock.
  MOCK_METHOD1(TryWinkRef, void(base::OnceClosure& cb));
  void TryWink(base::OnceClosure cb) override;

  // GMock cannot mock a method taking a move-only type.
  // TODO(crbug.com/40524294): Remove these workarounds once support for
  // move-only types is added to GMock.
  MOCK_METHOD2(DeviceTransactPtr,
               CancelToken(const std::vector<uint8_t>& command,
                           DeviceCallback& cb));

  // FidoDevice:
  CancelToken DeviceTransact(std::vector<uint8_t> command,
                             DeviceCallback cb) override;
  MOCK_METHOD1(Cancel, void(FidoDevice::CancelToken));
  MOCK_CONST_METHOD0(GetId, std::string(void));
  MOCK_CONST_METHOD0(GetDisplayName, std::string(void));
  FidoTransportProtocol DeviceTransport() const override;
  base::WeakPtr<FidoDevice> GetWeakPtr() override;

  void ExpectWinkedAtLeastOnce();
  void ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand command,
      std::optional<base::span<const uint8_t>> response,
      base::TimeDelta delay = base::TimeDelta(),
      testing::Matcher<base::span<const uint8_t>> request_matcher =
          testing::A<base::span<const uint8_t>>());
  void ExpectCtap2CommandAndRespondWithError(
      CtapRequestCommand command,
      CtapDeviceResponseCode response_code,
      base::TimeDelta delay = base::TimeDelta());
  void ExpectRequestAndRespondWith(
      base::span<const uint8_t> request,
      std::optional<base::span<const uint8_t>> response,
      base::TimeDelta delay = base::TimeDelta());
  void ExpectCtap2CommandAndDoNotRespond(CtapRequestCommand command);
  void ExpectRequestAndDoNotRespond(base::span<const uint8_t> request);
  void StubGetId();
  void SetDeviceTransport(FidoTransportProtocol transport_protocol);
  void StubGetDisplayName();

 private:
  FidoTransportProtocol transport_protocol_ =
      FidoTransportProtocol::kUsbHumanInterfaceDevice;
  base::WeakPtrFactory<FidoDevice> weak_factory_{this};
};

}  // namespace device

#endif  // DEVICE_FIDO_MOCK_FIDO_DEVICE_H_
