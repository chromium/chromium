// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/fido_discovery_factory.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/fido/cable/v2_constants.h"
#include "device/fido/features.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_discovery_base.h"
#include "device/fido/fido_transport_protocol.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include "device/fido/win/fake_webauthn_api.h"
#endif  // BUILDFLAG(IS_WIN)

namespace device {

#if BUILDFLAG(IS_WIN)
// Tests that a hybrid discovery is not created if Windows handles hybrid.
TEST(FidoDiscoveryFactoryTest, CreateWindowsHybridDiscovery) {
  base::test::ScopedFeatureList scoped_feature_list{
      device::kWebAuthnSkipHybridConfigIfSystemSupported};
  std::unique_ptr<BluetoothAdapterFactory::GlobalOverrideValues>
      override_values =
          BluetoothAdapterFactory::Get()->InitGlobalOverrideValues();
  override_values->SetLESupported(true);

  device::FakeWinWebAuthnApi fake_win_webauthn_api;
  device::WinWebAuthnApi::ScopedOverride win_webauthn_api_override(
      &fake_win_webauthn_api);

  for (const bool windows_has_hybrid : {false, true}) {
    SCOPED_TRACE(windows_has_hybrid);
    fake_win_webauthn_api.set_version(windows_has_hybrid ? 7 : 4);

    FidoDiscoveryFactory discovery_factory;
    discovery_factory.set_cable_data(
        FidoRequestType::kGetAssertion, /*cable_data=*/{},
        /*qr_generator_key=*/std::array<uint8_t, cablev2::kQRKeySize>());
    std::vector<std::unique_ptr<FidoDiscoveryBase>> discoveries =
        discovery_factory.Create(FidoTransportProtocol::kHybrid);
    EXPECT_EQ(discoveries.empty(), windows_has_hybrid);
  }
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace device
