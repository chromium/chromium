// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/bluetooth/frame_connected_bluetooth_devices.h"

#include <tuple>

#include "base/memory/raw_ptr.h"
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

using testing::_;
using testing::Return;
using testing::StrEq;

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
  std::ignore = client.BindNewEndpointAndPassDedicatedReceiver();
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
    contents()->GetPrimaryMainFrame()->InitializeRenderFrameIfNeeded();
    contents()->GetPrimaryMainFrame()->SetLastCommittedOriginForTesting(
        url::Origin::Create(GURL("https://blah.com")));
    TestRenderFrameHost* subframe =
        contents()->GetPrimaryMainFrame()->AppendChild("bluetooth_frame");
    subframe->InitializeRenderFrameIfNeeded();

    // Simulate two frames each connected to a bluetooth service.
    service_ptr0_ = WebBluetoothServiceImpl::CreateForTesting(
        contents()->GetPrimaryMainFrame(),
        service0_.BindNewPipeAndPassReceiver());
    map_ptr0_ = service_ptr0_->connected_devices_.get();

    service_ptr1_ = WebBluetoothServiceImpl::CreateForTesting(
        subframe, service1_.BindNewPipeAndPassReceiver());
    map_ptr1_ = service_ptr1_->connected_devices_.get();
  }

  void TearDown() override {
    // This normally happens as part of fixture destruction, but the test
    // fixture has pointers to several `DocumentUserData` that will dangle if
    // not explicitly torn down here.
    DeleteContents();
    RenderViewHostImplTestHarness::TearDown();
  }

  std::unique_ptr<NiceMockBluetoothGattConnection> GetConnection(
      const std::string& address) {
    return std::make_unique<NiceMockBluetoothGattConnection>(adapter_.get(),
                                                             address);
  }

  void ResetService0() {
    // This is a hack; destruction is normally implicitly triggered by
    // navigation or destruction of the frame itself, and not explicitly like
    // this test does.
    map_ptr0_ = nullptr;
    WebBluetoothServiceImpl::DeleteForCurrentDocument(
        &service_ptr0_.ExtractAsDangling()->render_frame_host());
  }

  void ResetService1() {
    // This is a hack; destruction is normally implicitly triggered by
    // navigation or destruction of the frame itself, and not explicitly like
    // this test does.
    map_ptr1_ = nullptr;
    WebBluetoothServiceImpl::DeleteForCurrentDocument(
        &service_ptr1_.ExtractAsDangling()->render_frame_host());
  }

  void DeleteContents() {
    // WebBluetoothServiceImpls are DocumentUserDatas, so null out these fields
    // before destroying the WebContents to avoid dangling pointers.
    service_ptr0_ = nullptr;
    map_ptr0_ = nullptr;
    service_ptr1_ = nullptr;
    map_ptr1_ = nullptr;
    RenderViewHostTestHarness::DeleteContents();
  }

 protected:
  raw_ptr<FrameConnectedBluetoothDevices> map_ptr0_ = nullptr;
  raw_ptr<FrameConnectedBluetoothDevices> map_ptr1_ = nullptr;

 private:
  mojo::Remote<blink::mojom::WebBluetoothService> service0_;
  raw_ptr<WebBluetoothServiceImpl> service_ptr0_ = nullptr;

  mojo::Remote<blink::mojom::WebBluetoothService> service1_;
  raw_ptr<WebBluetoothServiceImpl> service_ptr1_ = nullptr;

  scoped_refptr<NiceMockBluetoothAdapter> adapter_;
  NiceMockBluetoothDevice device0_;
  NiceMockBluetoothDevice device1_;
};

TEST_F(FrameConnectedBluetoothDevicesTest, Insert_Once) {
  map_ptr0_->Insert(kDeviceId0, GetConnection(kDeviceAddress0),
                    CreateServerClient());

  EXPECT_TRUE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_TRUE(map_ptr0_->IsConnectedToDeviceWithId(kDeviceId0));
}

TEST_F(FrameConnectedBluetoothDevicesTest, Insert_Twice) {
  map_ptr0_->Insert(kDeviceId0, GetConnection(kDeviceAddress0),
                    CreateServerClient());
  map_ptr0_->Insert(kDeviceId0, GetConnection(kDeviceAddress0),
                    CreateServerClient());

  EXPECT_TRUE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_TRUE(map_ptr0_->IsConnectedToDeviceWithId(kDeviceId0));
}

TEST_F(FrameConnectedBluetoothDevicesTest, Insert_TwoDevices) {
  map_ptr0_->Insert(kDeviceId0, GetConnection(kDeviceAddress0),
                    CreateServerClient());
  map_ptr0_->Insert(kDeviceId1, GetConnection(kDeviceAddress1),
                    CreateServerClient());

  EXPECT_TRUE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_TRUE(map_ptr0_->IsConnectedToDeviceWithId(kDeviceId0));
  EXPECT_TRUE(map_ptr0_->IsConnectedToDeviceWithId(kDeviceId1));
}

TEST_F(FrameConnectedBluetoothDevicesTest, Insert_TwoMaps) {
  map_ptr0_->Insert(kDeviceId0, GetConnection(kDeviceAddress0),
                    CreateServerClient());
  map_ptr1_->Insert(kDeviceId1, GetConnection(kDeviceAddress1),
                    CreateServerClient());

  EXPECT_TRUE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_TRUE(map_ptr0_->IsConnectedToDeviceWithId(kDeviceId0));
  EXPECT_TRUE(map_ptr1_->IsConnectedToDeviceWithId(kDeviceId1));
}

TEST_F(FrameConnectedBluetoothDevicesTest,
       CloseConnectionId_OneDevice_AddOnce_RemoveOnce) {
  map_ptr0_->Insert(kDeviceId0, GetConnection(kDeviceAddress0),
                    CreateServerClient());

  EXPECT_TRUE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_TRUE(map_ptr0_->IsConnectedToDeviceWithId(kDeviceId0));

  map_ptr0_->CloseConnectionToDeviceWithId(kDeviceId0);

  EXPECT_FALSE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_FALSE(map_ptr0_->IsConnectedToDeviceWithId(kDeviceId0));
}

TEST_F(FrameConnectedBluetoothDevicesTest,
       CloseConnectionId_OneDevice_AddOnce_RemoveTwice) {
  map_ptr0_->Insert(kDeviceId0, GetConnection(kDeviceAddress0),
                    CreateServerClient());

  EXPECT_TRUE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_TRUE(map_ptr0_->IsConnectedToDeviceWithId(kDeviceId0));

  map_ptr0_->CloseConnectionToDeviceWithId(kDeviceId0);
  map_ptr0_->CloseConnectionToDeviceWithId(kDeviceId0);

  EXPECT_FALSE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_FALSE(map_ptr0_->IsConnectedToDeviceWithId(kDeviceId0));
}

TEST_F(FrameConnectedBluetoothDevicesTest,
       CloseConnectionId_OneDevice_AddTwice_RemoveOnce) {
  map_ptr0_->Insert(kDeviceId0, GetConnection(kDeviceAddress0),
                    CreateServerClient());
  map_ptr0_->Insert(kDeviceId0, GetConnection(kDeviceAddress0),
                    CreateServerClient());

  EXPECT_TRUE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_TRUE(map_ptr0_->IsConnectedToDeviceWithId(kDeviceId0));

  map_ptr0_->CloseConnectionToDeviceWithId(kDeviceId0);

  EXPECT_FALSE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_FALSE(map_ptr0_->IsConnectedToDeviceWithId(kDeviceId0));
}

TEST_F(FrameConnectedBluetoothDevicesTest,
       CloseConnectionId_OneDevice_AddTwice_RemoveTwice) {
  map_ptr0_->Insert(kDeviceId0, GetConnection(kDeviceAddress0),
                    CreateServerClient());
  map_ptr0_->Insert(kDeviceId0, GetConnection(kDeviceAddress0),
                    CreateServerClient());

  EXPECT_TRUE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_TRUE(map_ptr0_->IsConnectedToDeviceWithId(kDeviceId0));

  map_ptr0_->CloseConnectionToDeviceWithId(kDeviceId0);
  map_ptr0_->CloseConnectionToDeviceWithId(kDeviceId0);

  EXPECT_FALSE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_FALSE(map_ptr0_->IsConnectedToDeviceWithId(kDeviceId0));
}

TEST_F(FrameConnectedBluetoothDevicesTest, CloseConnectionId_TwoDevices) {
  map_ptr0_->Insert(kDeviceId0, GetConnection(kDeviceAddress0),
                    CreateServerClient());
  map_ptr0_->Insert(kDeviceId1, GetConnection(kDeviceAddress1),
                    CreateServerClient());

  EXPECT_TRUE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_TRUE(map_ptr0_->IsConnectedToDeviceWithId(kDeviceId0));
  EXPECT_TRUE(map_ptr0_->IsConnectedToDeviceWithId(kDeviceId1));

  map_ptr0_->CloseConnectionToDeviceWithId(kDeviceId0);

  EXPECT_TRUE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_FALSE(map_ptr0_->IsConnectedToDeviceWithId(kDeviceId0));

  map_ptr0_->CloseConnectionToDeviceWithId(kDeviceId1);

  EXPECT_FALSE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_FALSE(map_ptr0_->IsConnectedToDeviceWithId(kDeviceId1));
}

TEST_F(FrameConnectedBluetoothDevicesTest, CloseConnectionId_TwoMaps) {
  map_ptr0_->Insert(kDeviceId0, GetConnection(kDeviceAddress0),
                    CreateServerClient());
  map_ptr1_->Insert(kDeviceId1, GetConnection(kDeviceAddress1),
                    CreateServerClient());

  EXPECT_TRUE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_TRUE(map_ptr0_->IsConnectedToDeviceWithId(kDeviceId0));
  EXPECT_TRUE(map_ptr1_->IsConnectedToDeviceWithId(kDeviceId1));

  map_ptr0_->CloseConnectionToDeviceWithId(kDeviceId0);

  EXPECT_TRUE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_FALSE(map_ptr0_->IsConnectedToDeviceWithId(kDeviceId0));

  map_ptr1_->CloseConnectionToDeviceWithId(kDeviceId1);

  EXPECT_FALSE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_FALSE(map_ptr1_->IsConnectedToDeviceWithId(kDeviceId1));
}

TEST_F(FrameConnectedBluetoothDevicesTest,
       CloseConnectionAddress_OneDevice_AddOnce_RemoveOnce) {
  map_ptr0_->Insert(kDeviceId0, GetConnection(kDeviceAddress0),
                    CreateServerClient());

  EXPECT_TRUE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_TRUE(map_ptr0_->IsConnectedToDeviceWithId(kDeviceId0));

  EXPECT_EQ(
      map_ptr0_->CloseConnectionToDeviceWithAddress(kDeviceAddress0).value(),
      kDeviceId0);

  EXPECT_FALSE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_FALSE(map_ptr0_->IsConnectedToDeviceWithId(kDeviceId0));
}

TEST_F(FrameConnectedBluetoothDevicesTest,
       CloseConnectionAddress_OneDevice_AddOnce_RemoveTwice) {
  map_ptr0_->Insert(kDeviceId0, GetConnection(kDeviceAddress0),
                    CreateServerClient());

  EXPECT_TRUE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_TRUE(map_ptr0_->IsConnectedToDeviceWithId(kDeviceId0));

  EXPECT_EQ(
      map_ptr0_->CloseConnectionToDeviceWithAddress(kDeviceAddress0).value(),
      kDeviceId0);
  EXPECT_FALSE(map_ptr0_->CloseConnectionToDeviceWithAddress(kDeviceAddress0));

  EXPECT_FALSE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_FALSE(map_ptr0_->IsConnectedToDeviceWithId(kDeviceId0));
}

TEST_F(FrameConnectedBluetoothDevicesTest,
       CloseConnectionAddress_OneDevice_AddTwice_RemoveOnce) {
  map_ptr0_->Insert(kDeviceId0, GetConnection(kDeviceAddress0),
                    CreateServerClient());
  map_ptr0_->Insert(kDeviceId0, GetConnection(kDeviceAddress0),
                    CreateServerClient());

  EXPECT_TRUE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_TRUE(map_ptr0_->IsConnectedToDeviceWithId(kDeviceId0));

  EXPECT_EQ(
      map_ptr0_->CloseConnectionToDeviceWithAddress(kDeviceAddress0).value(),
      kDeviceId0);

  EXPECT_FALSE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_FALSE(map_ptr0_->IsConnectedToDeviceWithId(kDeviceId0));
}

TEST_F(FrameConnectedBluetoothDevicesTest,
       CloseConnectionAddress_OneDevice_AddTwice_RemoveTwice) {
  map_ptr0_->Insert(kDeviceId0, GetConnection(kDeviceAddress0),
                    CreateServerClient());
  map_ptr0_->Insert(kDeviceId0, GetConnection(kDeviceAddress0),
                    CreateServerClient());

  EXPECT_TRUE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_TRUE(map_ptr0_->IsConnectedToDeviceWithId(kDeviceId0));

  EXPECT_EQ(
      map_ptr0_->CloseConnectionToDeviceWithAddress(kDeviceAddress0).value(),
      kDeviceId0);
  EXPECT_FALSE(map_ptr0_->CloseConnectionToDeviceWithAddress(kDeviceAddress0));

  EXPECT_FALSE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_FALSE(map_ptr0_->IsConnectedToDeviceWithId(kDeviceId0));
}

TEST_F(FrameConnectedBluetoothDevicesTest, CloseConnectionAddress_TwoDevices) {
  map_ptr0_->Insert(kDeviceId0, GetConnection(kDeviceAddress0),
                    CreateServerClient());
  map_ptr0_->Insert(kDeviceId1, GetConnection(kDeviceAddress1),
                    CreateServerClient());

  EXPECT_TRUE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_TRUE(map_ptr0_->IsConnectedToDeviceWithId(kDeviceId0));
  EXPECT_TRUE(map_ptr0_->IsConnectedToDeviceWithId(kDeviceId1));

  EXPECT_EQ(
      map_ptr0_->CloseConnectionToDeviceWithAddress(kDeviceAddress0).value(),
      kDeviceId0);

  EXPECT_TRUE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_FALSE(map_ptr0_->IsConnectedToDeviceWithId(kDeviceId0));
  EXPECT_TRUE(map_ptr0_->IsConnectedToDeviceWithId(kDeviceId1));

  EXPECT_EQ(
      map_ptr0_->CloseConnectionToDeviceWithAddress(kDeviceAddress1).value(),
      kDeviceId1);

  EXPECT_FALSE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_FALSE(map_ptr0_->IsConnectedToDeviceWithId(kDeviceId1));
}

TEST_F(FrameConnectedBluetoothDevicesTest, CloseConnectionAddress_TwoMaps) {
  map_ptr0_->Insert(kDeviceId0, GetConnection(kDeviceAddress0),
                    CreateServerClient());
  map_ptr1_->Insert(kDeviceId1, GetConnection(kDeviceAddress1),
                    CreateServerClient());

  EXPECT_TRUE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_TRUE(map_ptr0_->IsConnectedToDeviceWithId(kDeviceId0));
  EXPECT_TRUE(map_ptr1_->IsConnectedToDeviceWithId(kDeviceId1));

  EXPECT_EQ(
      map_ptr0_->CloseConnectionToDeviceWithAddress(kDeviceAddress0).value(),
      kDeviceId0);

  EXPECT_TRUE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_FALSE(map_ptr0_->IsConnectedToDeviceWithId(kDeviceId0));
  EXPECT_TRUE(map_ptr1_->IsConnectedToDeviceWithId(kDeviceId1));

  EXPECT_EQ(
      map_ptr1_->CloseConnectionToDeviceWithAddress(kDeviceAddress1).value(),
      kDeviceId1);

  EXPECT_FALSE(contents()->IsConnectedToBluetoothDevice());
  EXPECT_FALSE(map_ptr1_->IsConnectedToDeviceWithId(kDeviceId1));
}

TEST_F(FrameConnectedBluetoothDevicesTest, Destruction_MultipleDevices) {
  map_ptr0_->Insert(kDeviceId0, GetConnection(kDeviceAddress0),
                    CreateServerClient());
  map_ptr0_->Insert(kDeviceId1, GetConnection(kDeviceAddress1),
                    CreateServerClient());

  EXPECT_TRUE(contents()->IsConnectedToBluetoothDevice());

  ResetService0();

  EXPECT_FALSE(contents()->IsConnectedToBluetoothDevice());
}

TEST_F(FrameConnectedBluetoothDevicesTest, Destruction_MultipleMaps) {
  map_ptr0_->Insert(kDeviceId0, GetConnection(kDeviceAddress0),
                    CreateServerClient());
  map_ptr0_->Insert(kDeviceId1, GetConnection(kDeviceAddress1),
                    CreateServerClient());

  map_ptr1_->Insert(kDeviceId0, GetConnection(kDeviceAddress0),
                    CreateServerClient());
  map_ptr1_->Insert(kDeviceId1, GetConnection(kDeviceAddress1),
                    CreateServerClient());

  EXPECT_TRUE(contents()->IsConnectedToBluetoothDevice());

  ResetService0();

  // WebContents should still be connected because of map_ptr1_.
  EXPECT_TRUE(contents()->IsConnectedToBluetoothDevice());

  ResetService1();

  EXPECT_FALSE(contents()->IsConnectedToBluetoothDevice());
}

TEST_F(FrameConnectedBluetoothDevicesTest,
       DestroyedByWebContentsImplDestruction) {
  // Tests that we don't crash when FrameConnectedBluetoothDevices contains
  // at least one device, and it is destroyed while WebContentsImpl is being
  // destroyed.
  map_ptr0_->Insert(kDeviceId0, GetConnection(kDeviceAddress0),
                    CreateServerClient());

  DeleteContents();
}

}  // namespace content
