// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_MOCK_FIDO_DEVICE_H_
#define DEVICE_FIDO_MOCK_FIDO_DEVICE_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_device.h"
#include "device/fido/fido_transport_protocol.h"
#include "testing/gmock/include/gmock/gmock.h"

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
      base::Optional<AuthenticatorGetInfoResponse> device_info = base::nullopt);
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
      base::Optional<base::span<const uint8_t>> get_info_response =
          base::nullopt);

  MockFidoDevice();
  MockFidoDevice(ProtocolVersion protocol_version,
                 base::Optional<AuthenticatorGetInfoResponse> device_info);
  ~MockFidoDevice() override;

  // TODO(crbug.com/729950): Remove these workarounds once support for move-only
  // types is added to GMock.
  MOCK_METHOD1(TryWinkRef, void(WinkCallback& cb));
  void TryWink(WinkCallback cb) override;

  MOCK_METHOD0(Cancel, void(void));

  MOCK_CONST_METHOD0(GetId, std::string(void));
  // GMock cannot mock a method taking a move-only type.
  // TODO(crbug.com/729950): Remove these workarounds once support for move-only
  // types is added to GMock.
  MOCK_METHOD2(DeviceTransactPtr,
               void(const std::vector<uint8_t>& command, DeviceCallback& cb));
  void DeviceTransact(std::vector<uint8_t> command, DeviceCallback cb) override;

  // FidoDevice:
  FidoTransportProtocol DeviceTransport() const override;
  base::WeakPtr<FidoDevice> GetWeakPtr() override;

  void ExpectWinkedAtLeastOnce();
  void ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand command,
      base::Optional<base::span<const uint8_t>> response,
      base::TimeDelta delay = base::TimeDelta());
  void ExpectCtap2CommandAndRespondWithError(
      CtapRequestCommand command,
      CtapDeviceResponseCode response_code,
      base::TimeDelta delay = base::TimeDelta());
  void ExpectRequestAndRespondWith(
      base::span<const uint8_t> request,
      base::Optional<base::span<const uint8_t>> response,
      base::TimeDelta delay = base::TimeDelta());
  void ExpectCtap2CommandAndDoNotRespond(CtapRequestCommand command);
  void ExpectRequestAndDoNotRespond(base::span<const uint8_t> request);
  void StubGetId();
  void SetDeviceTransport(FidoTransportProtocol transport_protocol);

 private:
  FidoTransportProtocol transport_protocol_ =
      FidoTransportProtocol::kUsbHumanInterfaceDevice;
  base::WeakPtrFactory<FidoDevice> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MockFidoDevice);
};

}  // namespace device

#endif  // DEVICE_FIDO_MOCK_FIDO_DEVICE_H_
