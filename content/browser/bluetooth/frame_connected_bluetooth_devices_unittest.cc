// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/bluetooth/frame_connected_bluetooth_devices.h"

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/browser/bluetooth/web_bluetooth_service_impl.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "device/bluetooth/bluetooth_gatt_connection.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_connection.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom.h"

namespace content {

typedef testing::NiceMock<device::MockBluetoothAdapter>
    NiceMockBluetoothAdapter;
typedef testing::NiceMock<device::MockBluetoothDevice> NiceMockBluetoothDevice;
typedef testing::NiceMock<device::MockBluetoothGattConnection>
    NiceMockBluetoothGattConnection;

using testing::Return;
using testing::StrEq;
using testing::_;

namespace {

const blink::WebBluetoothDeviceId kDeviceId0("000000000000000000000A==");
constexpr char kDeviceAddress0[] = "0";
constexpr char kDeviceName0[] = "Device0";

const blink::WebBluetoothDeviceId kDeviceId1("111111111111111111111A==");
constexpr char kDeviceAddress1[] = "1";
constexpr char kDeviceName1[] = "Device1";

mojo::AssociatedRemote<blink::mojom::WebBluetoothServerClient>
CreateServerClient() {
  mojo::AssociatedRemote<blink::mojom::WebBluetoothServerClient> client;
  ignore_result(client.BindNewEndpointAndPassDedicatedReceiverForTesting());
  return client;
}

}  // namespace

class FrameConnectedBluetoothDevicesTest
    : public RenderViewHostImplTestHarness {
 public:
  FrameConnectedBluetoothDevicesTest()
      : adapter_(new NiceMockBluetoothAdapter()),
        device0_(adapter_.get(),
                 0 /* class */,
                 kDeviceName0,
                 kDeviceAddress0,
                 false /* paired */,
                 false /* connected */),
        device1_(adapter_.get(),
                 0 /* class */,
                 kDeviceName1,
                 kDeviceAddress1,
                 false /* paired */,
                 false /* connected */) {
    ON_CALL(*adapter_, GetDevice(_)).WillByDefault(Return(nullptr));
    ON_CALL(*adapter_, GetDevice(StrEq(kDeviceAddress0)))
        .WillByDefault(Return(&device0_));
    ON_CALL(*adapter_, GetDevice(StrEq(kDeviceAddress1)))
        .WillByDefault(Return(&device1_));
  }

  ~FrameConnectedBluetoothDevicesTest() override {}

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();

    // Create subframe to simulate two maps on the same WebContents.
    contents()->GetMainFrame()->InitializeRenderFrameIfNeeded();
    TestRenderFrameHost* subframe =
        contents()->GetMainFrame()->AppendChild("bluetooth_frame");
    subframe->InitializeRenderFrameIfNeeded();

    // Simulate two frames each connected to a bluetooth service.
    service0_ =
        contents()->GetMainFrame()->CreateWebBluetoothServiceForTesting();
    map0_ = service0_->connected_devices_.get();

    service1_ = subframe->CreateWebBluetoothServiceForTesting();
    map1_ = service1_->connected_devices_.get();
  }

  void TearDown() override {
    map1_ = nullptr;
    service1_ = nullptr;
    map0_ = nullptr;
    service0_ = nullptr;
    RenderViewHostImplTestHarness::TearDown();
  }

  std::unique_ptr<NiceMockBluetoothGattConnection> GetConnection(
      const std::string& address) {
    return std::make_unique<NiceMockBluetoothGattConnection>(adapter_.get(),
                                                             address);
  }

  void ResetService0() {
    service0_->ClearState();
    map0_ = nullptr;
  }

  void ResetService1() {
    service1_->ClearState();
    map1_ = nullptr;
  }

 protected:
  FrameConnectedBluetoothDevices* map0_;
  WebBluetoothServiceImpl* service0_;

  FrameConnectedBluetoothDevices* map1_;
  WebBluetoothServiceImpl* service1_;

 private:
  scoped_refptr<NiceMockBluetoothAdapter> adapter_;
  NiceMockBluetoothDevice device0_;
  NiceMockBluetoothDevice device1_;
};

TEST_F(FrameConnectedBluetoothDevicesTest, Insert_Once) {
  map0_->Insert(kDeviceId0, GetConnection(kDeviceAddress0),
                CreateServerClient());

  EXPECT_TRUE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_TRUE(map0_->IsConnectedToDeviceWithId(kDeviceId0));
}

TEST_F(FrameConnectedBluetoothDevicesTest, Insert_Twice) {
  map0_->Insert(kDeviceId0, GetConnection(kDeviceAddress0),
                CreateServerClient());
  map0_->Insert(kDeviceId0, GetConnection(kDeviceAddress0),
                CreateServerClient());

  EXPECT_TRUE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_TRUE(map0_->IsConnectedToDeviceWithId(kDeviceId0));
}

TEST_F(FrameConnectedBluetoothDevicesTest, Insert_TwoDevices) {
  map0_->Insert(kDeviceId0, GetConnection(kDeviceAddress0),
                CreateServerClient());
  map0_->Insert(kDeviceId1, GetConnection(kDeviceAddress1),
                CreateServerClient());

  EXPECT_TRUE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_TRUE(map0_->IsConnectedToDeviceWithId(kDeviceId0));
  EXPECT_TRUE(map0_->IsConnectedToDeviceWithId(kDeviceId1));
}

TEST_F(FrameConnectedBluetoothDevicesTest, Insert_TwoMaps) {
  map0_->Insert(kDeviceId0, GetConnection(kDeviceAddress0),
                CreateServerClient());
  map1_->Insert(kDeviceId1, GetConnection(kDeviceAddress1),
                CreateServerClient());

  EXPECT_TRUE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_TRUE(map0_->IsConnectedToDeviceWithId(kDeviceId0));
  EXPECT_TRUE(map1_->IsConnectedToDeviceWithId(kDeviceId1));
}

TEST_F(FrameConnectedBluetoothDevicesTest,
       CloseConnectionId_OneDevice_AddOnce_RemoveOnce) {
  map0_->Insert(kDeviceId0, GetConnection(kDeviceAddress0),
                CreateServerClient());

  EXPECT_TRUE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_TRUE(map0_->IsConnectedToDeviceWithId(kDeviceId0));

  map0_->CloseConnectionToDeviceWithId(kDeviceId0);

  EXPECT_FALSE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_FALSE(map0_->IsConnectedToDeviceWithId(kDeviceId0));
}

TEST_F(FrameConnectedBluetoothDevicesTest,
       CloseConnectionId_OneDevice_AddOnce_RemoveTwice) {
  map0_->Insert(kDeviceId0, GetConnection(kDeviceAddress0),
                CreateServerClient());

  EXPECT_TRUE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_TRUE(map0_->IsConnectedToDeviceWithId(kDeviceId0));

  map0_->CloseConnectionToDeviceWithId(kDeviceId0);
  map0_->CloseConnectionToDeviceWithId(kDeviceId0);

  EXPECT_FALSE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_FALSE(map0_->IsConnectedToDeviceWithId(kDeviceId0));
}

TEST_F(FrameConnectedBluetoothDevicesTest,
       CloseConnectionId_OneDevice_AddTwice_RemoveOnce) {
  map0_->Insert(kDeviceId0, GetConnection(kDeviceAddress0),
                CreateServerClient());
  map0_->Insert(kDeviceId0, GetConnection(kDeviceAddress0),
                CreateServerClient());

  EXPECT_TRUE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_TRUE(map0_->IsConnectedToDeviceWithId(kDeviceId0));

  map0_->CloseConnectionToDeviceWithId(kDeviceId0);

  EXPECT_FALSE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_FALSE(map0_->IsConnectedToDeviceWithId(kDeviceId0));
}

TEST_F(FrameConnectedBluetoothDevicesTest,
       CloseConnectionId_OneDevice_AddTwice_RemoveTwice) {
  map0_->Insert(kDeviceId0, GetConnection(kDeviceAddress0),
                CreateServerClient());
  map0_->Insert(kDeviceId0, GetConnection(kDeviceAddress0),
                CreateServerClient());

  EXPECT_TRUE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_TRUE(map0_->IsConnectedToDeviceWithId(kDeviceId0));

  map0_->CloseConnectionToDeviceWithId(kDeviceId0);
  map0_->CloseConnectionToDeviceWithId(kDeviceId0);

  EXPECT_FALSE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_FALSE(map0_->IsConnectedToDeviceWithId(kDeviceId0));
}

TEST_F(FrameConnectedBluetoothDevicesTest, CloseConnectionId_TwoDevices) {
  map0_->Insert(kDeviceId0, GetConnection(kDeviceAddress0),
                CreateServerClient());
  map0_->Insert(kDeviceId1, GetConnection(kDeviceAddress1),
                CreateServerClient());

  EXPECT_TRUE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_TRUE(map0_->IsConnectedToDeviceWithId(kDeviceId0));
  EXPECT_TRUE(map0_->IsConnectedToDeviceWithId(kDeviceId1));

  map0_->CloseConnectionToDeviceWithId(kDeviceId0);

  EXPECT_TRUE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_FALSE(map0_->IsConnectedToDeviceWithId(kDeviceId0));

  map0_->CloseConnectionToDeviceWithId(kDeviceId1);

  EXPECT_FALSE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_FALSE(map0_->IsConnectedToDeviceWithId(kDeviceId1));
}

TEST_F(FrameConnectedBluetoothDevicesTest, CloseConnectionId_TwoMaps) {
  map0_->Insert(kDeviceId0, GetConnection(kDeviceAddress0),
                CreateServerClient());
  map1_->Insert(kDeviceId1, GetConnection(kDeviceAddress1),
                CreateServerClient());

  EXPECT_TRUE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_TRUE(map0_->IsConnectedToDeviceWithId(kDeviceId0));
  EXPECT_TRUE(map1_->IsConnectedToDeviceWithId(kDeviceId1));

  map0_->CloseConnectionToDeviceWithId(kDeviceId0);

  EXPECT_TRUE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_FALSE(map0_->IsConnectedToDeviceWithId(kDeviceId0));

  map1_->CloseConnectionToDeviceWithId(kDeviceId1);

  EXPECT_FALSE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_FALSE(map1_->IsConnectedToDeviceWithId(kDeviceId1));
}

TEST_F(FrameConnectedBluetoothDevicesTest,
       CloseConnectionAddress_OneDevice_AddOnce_RemoveOnce) {
  map0_->Insert(kDeviceId0, GetConnection(kDeviceAddress0),
                CreateServerClient());

  EXPECT_TRUE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_TRUE(map0_->IsConnectedToDeviceWithId(kDeviceId0));

  EXPECT_EQ(map0_->CloseConnectionToDeviceWithAddress(kDeviceAddress0).value(),
            kDeviceId0);

  EXPECT_FALSE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_FALSE(map0_->IsConnectedToDeviceWithId(kDeviceId0));
}

TEST_F(FrameConnectedBluetoothDevicesTest,
       CloseConnectionAddress_OneDevice_AddOnce_RemoveTwice) {
  map0_->Insert(kDeviceId0, GetConnection(kDeviceAddress0),
                CreateServerClient());

  EXPECT_TRUE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_TRUE(map0_->IsConnectedToDeviceWithId(kDeviceId0));

  EXPECT_EQ(map0_->CloseConnectionToDeviceWithAddress(kDeviceAddress0).value(),
            kDeviceId0);
  EXPECT_FALSE(map0_->CloseConnectionToDeviceWithAddress(kDeviceAddress0));

  EXPECT_FALSE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_FALSE(map0_->IsConnectedToDeviceWithId(kDeviceId0));
}

TEST_F(FrameConnectedBluetoothDevicesTest,
       CloseConnectionAddress_OneDevice_AddTwice_RemoveOnce) {
  map0_->Insert(kDeviceId0, GetConnection(kDeviceAddress0),
                CreateServerClient());
  map0_->Insert(kDeviceId0, GetConnection(kDeviceAddress0),
                CreateServerClient());

  EXPECT_TRUE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_TRUE(map0_->IsConnectedToDeviceWithId(kDeviceId0));

  EXPECT_EQ(map0_->CloseConnectionToDeviceWithAddress(kDeviceAddress0).value(),
            kDeviceId0);

  EXPECT_FALSE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_FALSE(map0_->IsConnectedToDeviceWithId(kDeviceId0));
}

TEST_F(FrameConnectedBluetoothDevicesTest,
       CloseConnectionAddress_OneDevice_AddTwice_RemoveTwice) {
  map0_->Insert(kDeviceId0, GetConnection(kDeviceAddress0),
                CreateServerClient());
  map0_->Insert(kDeviceId0, GetConnection(kDeviceAddress0),
                CreateServerClient());

  EXPECT_TRUE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_TRUE(map0_->IsConnectedToDeviceWithId(kDeviceId0));

  EXPECT_EQ(map0_->CloseConnectionToDeviceWithAddress(kDeviceAddress0).value(),
            kDeviceId0);
  EXPECT_FALSE(map0_->CloseConnectionToDeviceWithAddress(kDeviceAddress0));

  EXPECT_FALSE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_FALSE(map0_->IsConnectedToDeviceWithId(kDeviceId0));
}

TEST_F(FrameConnectedBluetoothDevicesTest, CloseConnectionAddress_TwoDevices) {
  map0_->Insert(kDeviceId0, GetConnection(kDeviceAddress0),
                CreateServerClient());
  map0_->Insert(kDeviceId1, GetConnection(kDeviceAddress1),
                CreateServerClient());

  EXPECT_TRUE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_TRUE(map0_->IsConnectedToDeviceWithId(kDeviceId0));
  EXPECT_TRUE(map0_->IsConnectedToDeviceWithId(kDeviceId1));

  EXPECT_EQ(map0_->CloseConnectionToDeviceWithAddress(kDeviceAddress0).value(),
            kDeviceId0);

  EXPECT_TRUE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_FALSE(map0_->IsConnectedToDeviceWithId(kDeviceId0));
  EXPECT_TRUE(map0_->IsConnectedToDeviceWithId(kDeviceId1));

  EXPECT_EQ(map0_->CloseConnectionToDeviceWithAddress(kDeviceAddress1).value(),
            kDeviceId1);

  EXPECT_FALSE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_FALSE(map0_->IsConnectedToDeviceWithId(kDeviceId1));
}

TEST_F(FrameConnectedBluetoothDevicesTest, CloseConnectionAddress_TwoMaps) {
  map0_->Insert(kDeviceId0, GetConnection(kDeviceAddress0),
                CreateServerClient());
  map1_->Insert(kDeviceId1, GetConnection(kDeviceAddress1),
                CreateServerClient());

  EXPECT_TRUE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_TRUE(map0_->IsConnectedToDeviceWithId(kDeviceId0));
  EXPECT_TRUE(map1_->IsConnectedToDeviceWithId(kDeviceId1));

  EXPECT_EQ(map0_->CloseConnectionToDeviceWithAddress(kDeviceAddress0).value(),
            kDeviceId0);

  EXPECT_TRUE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_FALSE(map0_->IsConnectedToDeviceWithId(kDeviceId0));
  EXPECT_TRUE(map1_->IsConnectedToDeviceWithId(kDeviceId1));

  EXPECT_EQ(map1_->CloseConnectionToDeviceWithAddress(kDeviceAddress1).value(),
            kDeviceId1);

  EXPECT_FALSE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_FALSE(map1_->IsConnectedToDeviceWithId(kDeviceId1));
}

TEST_F(FrameConnectedBluetoothDevicesTest, Destruction_MultipleDevices) {
  map0_->Insert(kDeviceId0, GetConnection(kDeviceAddress0),
                CreateServerClient());
  map0_->Insert(kDeviceId1, GetConnection(kDeviceAddress1),
                CreateServerClient());

  EXPECT_TRUE(contents()->IsConnectedToBluetoothDevice());

  ResetService0();

  EXPECT_FALSE(contents()->IsConnectedToBluetoothDevice());
}

TEST_F(FrameConnectedBluetoothDevicesTest, Destruction_MultipleMaps) {
  map0_->Insert(kDeviceId0, GetConnection(kDeviceAddress0),
                CreateServerClient());
  map0_->Insert(kDeviceId1, GetConnection(kDeviceAddress1),
                CreateServerClient());

  map1_->Insert(kDeviceId0, GetConnection(kDeviceAddress0),
                CreateServerClient());
  map1_->Insert(kDeviceId1, GetConnection(kDeviceAddress1),
                CreateServerClient());

  EXPECT_TRUE(contents()->IsConnectedToBluetoothDevice());

  ResetService0();

  // WebContents should still be connected because of map1_.
  EXPECT_TRUE(contents()->IsConnectedToBluetoothDevice());

  ResetService1();

  EXPECT_FALSE(contents()->IsConnectedToBluetoothDevice());
}

TEST_F(FrameConnectedBluetoothDevicesTest,
       DestroyedByWebContentsImplDestruction) {
  // Tests that we don't crash when FrameConnectedBluetoothDevices contains
  // at least one device, and it is destroyed while WebContentsImpl is being
  // destroyed.
  map0_->Insert(kDeviceId0, GetConnection(kDeviceAddress0),
                CreateServerClient());
  DeleteContents();
}

}  // namespace content
