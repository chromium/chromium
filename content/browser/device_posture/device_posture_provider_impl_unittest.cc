// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/device_posture/device_posture_provider_impl.h"

#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/test_future.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/device_posture/device_posture_provider.mojom.h"

namespace content {

using ::testing::_;

class MockDevicePostureClient : public blink::mojom::DevicePostureClient {
 public:
  MockDevicePostureClient() = default;
  ~MockDevicePostureClient() override = default;

  mojo::PendingRemote<blink::mojom::DevicePostureClient> BindAndGetRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  // blink::mojom::DevicePostureClient:
  MOCK_METHOD(void,
              OnPostureChanged,
              (blink::mojom::DevicePostureType posture),
              (override));

  void Flush() { receiver_.FlushForTesting(); }

 private:
  mojo::Receiver<blink::mojom::DevicePostureClient> receiver_{this};
};

class DevicePostureProviderImplTest : public RenderViewHostTestHarness {
 public:
  DevicePostureProviderImplTest() = default;

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    DevicePostureProviderImpl::GetOrCreate(web_contents());
  }

  DevicePostureProviderImpl* provider() {
    return DevicePostureProviderImpl::FromWebContents(web_contents());
  }

  TestWebContents* test_web_contents() {
    return static_cast<TestWebContents*>(web_contents());
  }
};

TEST_F(DevicePostureProviderImplTest, DeferUpdatesWhileHidden) {
  MockDevicePostureClient client;

  // Initially visible.
  test_web_contents()->WasShown();

  // Add listener.
  base::test::TestFuture<blink::mojom::DevicePostureType> posture_future;
  blink::mojom::DevicePostureProvider* posture_provider = provider();
  posture_provider->AddListenerAndGetCurrentPosture(
      client.BindAndGetRemote(), posture_future.GetCallback());
  EXPECT_EQ(posture_future.Get(), blink::mojom::DevicePostureType::kContinuous);

  // Trigger posture change while visible.
  DevicePosturePlatformProvider::Observer* observer = provider();
  base::test::TestFuture<blink::mojom::DevicePostureType> change_future_1;
  EXPECT_CALL(client, OnPostureChanged(_))
      .WillOnce(base::test::InvokeFuture(change_future_1));
  observer->OnDevicePostureChanged(blink::mojom::DevicePostureType::kFolded);
  EXPECT_EQ(change_future_1.Take(), blink::mojom::DevicePostureType::kFolded);

  // Hide the web contents.
  test_web_contents()->WasHidden();

  // Trigger posture change while hidden.
  EXPECT_CALL(client, OnPostureChanged(_)).Times(0);
  observer->OnDevicePostureChanged(
      blink::mojom::DevicePostureType::kContinuous);
  client.Flush();

  // Show the web contents again.
  base::test::TestFuture<blink::mojom::DevicePostureType> change_future_2;
  EXPECT_CALL(client, OnPostureChanged(_))
      .WillOnce(base::test::InvokeFuture(change_future_2));
  test_web_contents()->WasShown();
  EXPECT_EQ(change_future_2.Take(),
            blink::mojom::DevicePostureType::kContinuous);
}

TEST_F(DevicePostureProviderImplTest, DeferUpdatesWhileOccluded) {
  MockDevicePostureClient client;

  // Initially visible.
  test_web_contents()->WasShown();

  // Add listener.
  base::test::TestFuture<blink::mojom::DevicePostureType> posture_future;
  blink::mojom::DevicePostureProvider* posture_provider = provider();
  posture_provider->AddListenerAndGetCurrentPosture(
      client.BindAndGetRemote(), posture_future.GetCallback());
  EXPECT_EQ(posture_future.Get(), blink::mojom::DevicePostureType::kContinuous);

  // Trigger posture change while visible.
  DevicePosturePlatformProvider::Observer* observer = provider();
  base::test::TestFuture<blink::mojom::DevicePostureType> change_future_1;
  EXPECT_CALL(client, OnPostureChanged(_))
      .WillOnce(base::test::InvokeFuture(change_future_1));
  observer->OnDevicePostureChanged(blink::mojom::DevicePostureType::kFolded);
  EXPECT_EQ(change_future_1.Take(), blink::mojom::DevicePostureType::kFolded);

  // Occlude the web contents.
  test_web_contents()->WasOccluded();

  // Trigger posture change while occluded.
  EXPECT_CALL(client, OnPostureChanged(_)).Times(0);
  observer->OnDevicePostureChanged(
      blink::mojom::DevicePostureType::kContinuous);
  client.Flush();

  // Show the web contents again.
  base::test::TestFuture<blink::mojom::DevicePostureType> change_future_2;
  EXPECT_CALL(client, OnPostureChanged(_))
      .WillOnce(base::test::InvokeFuture(change_future_2));
  test_web_contents()->WasShown();
  EXPECT_EQ(change_future_2.Take(),
            blink::mojom::DevicePostureType::kContinuous);
}

TEST_F(DevicePostureProviderImplTest, DeferUpdatesWhileHiddenEmulation) {
  MockDevicePostureClient client;

  // Initially visible.
  test_web_contents()->WasShown();

  // Add listener.
  base::test::TestFuture<blink::mojom::DevicePostureType> posture_future;
  blink::mojom::DevicePostureProvider* posture_provider = provider();
  posture_provider->AddListenerAndGetCurrentPosture(
      client.BindAndGetRemote(), posture_future.GetCallback());
  EXPECT_EQ(posture_future.Get(), blink::mojom::DevicePostureType::kContinuous);

  // Enable emulation and override.
  base::test::TestFuture<blink::mojom::DevicePostureType> change_future_1;
  EXPECT_CALL(client, OnPostureChanged(_))
      .WillOnce(base::test::InvokeFuture(change_future_1));
  provider()->OverrideDevicePostureForEmulation(
      blink::mojom::DevicePostureType::kFolded);
  EXPECT_EQ(change_future_1.Take(), blink::mojom::DevicePostureType::kFolded);

  // Hide the web contents.
  test_web_contents()->WasHidden();

  // Override while hidden.
  EXPECT_CALL(client, OnPostureChanged(_)).Times(0);
  provider()->OverrideDevicePostureForEmulation(
      blink::mojom::DevicePostureType::kContinuous);
  client.Flush();

  // Show the web contents again.
  base::test::TestFuture<blink::mojom::DevicePostureType> change_future_2;
  EXPECT_CALL(client, OnPostureChanged(_))
      .WillOnce(base::test::InvokeFuture(change_future_2));
  test_web_contents()->WasShown();
  EXPECT_EQ(change_future_2.Take(),
            blink::mojom::DevicePostureType::kContinuous);
}

TEST_F(DevicePostureProviderImplTest, ReturnFallbackWhileHiddenFirstBind) {
  MockDevicePostureClient client;

  // Start HIDDEN.
  test_web_contents()->WasHidden();

  // Emulate platform state as Folded (while hidden).
  provider()->OverrideDevicePostureForEmulation(
      blink::mojom::DevicePostureType::kFolded);

  // Bind new client while hidden.
  base::test::TestFuture<blink::mojom::DevicePostureType> posture_future;
  blink::mojom::DevicePostureProvider* posture_provider = provider();
  EXPECT_CALL(client, OnPostureChanged(_)).Times(0);
  posture_provider->AddListenerAndGetCurrentPosture(
      client.BindAndGetRemote(), posture_future.GetCallback());

  // It should return the FALLBACK (kContinuous) because
  // last_dispatched_posture_ was nullopt.
  EXPECT_EQ(posture_future.Get(), blink::mojom::DevicePostureType::kContinuous);
  client.Flush();

  // Show the web contents. It should dispatch the REAL state (kFolded).
  base::test::TestFuture<blink::mojom::DevicePostureType> change_future;
  EXPECT_CALL(client, OnPostureChanged(_))
      .WillOnce(base::test::InvokeFuture(change_future));
  test_web_contents()->WasShown();
  EXPECT_EQ(change_future.Take(), blink::mojom::DevicePostureType::kFolded);
}

TEST_F(DevicePostureProviderImplTest, ReturnFallbackWhileHiddenSubsequentBind) {
  MockDevicePostureClient client1;
  MockDevicePostureClient client2;

  // Initially visible.
  test_web_contents()->WasShown();

  // Add first listener.
  base::test::TestFuture<blink::mojom::DevicePostureType> posture_future_1;
  blink::mojom::DevicePostureProvider* posture_provider = provider();
  posture_provider->AddListenerAndGetCurrentPosture(
      client1.BindAndGetRemote(), posture_future_1.GetCallback());
  EXPECT_EQ(posture_future_1.Get(),
            blink::mojom::DevicePostureType::kContinuous);

  // Change to Folded while VISIBLE.
  base::test::TestFuture<blink::mojom::DevicePostureType> change_future_1;
  EXPECT_CALL(client1, OnPostureChanged(_))
      .WillOnce(base::test::InvokeFuture(change_future_1));
  provider()->OverrideDevicePostureForEmulation(
      blink::mojom::DevicePostureType::kFolded);
  EXPECT_EQ(change_future_1.Take(), blink::mojom::DevicePostureType::kFolded);

  // Go HIDDEN.
  test_web_contents()->WasHidden();

  // Change to Continuous while HIDDEN.
  EXPECT_CALL(client1, OnPostureChanged(_)).Times(0);
  provider()->OverrideDevicePostureForEmulation(
      blink::mojom::DevicePostureType::kContinuous);
  client1.Flush();

  // Bind SECOND listener while hidden.
  base::test::TestFuture<blink::mojom::DevicePostureType> posture_future_2;
  EXPECT_CALL(client2, OnPostureChanged(_)).Times(0);
  posture_provider->AddListenerAndGetCurrentPosture(
      client2.BindAndGetRemote(), posture_future_2.GetCallback());

  // It should return the LAST DISPATCHED state (kFolded), NOT the live state
  // (kContinuous).
  EXPECT_EQ(posture_future_2.Get(), blink::mojom::DevicePostureType::kFolded);
  client2.Flush();

  // Show the web contents. It should dispatch the REAL state (kContinuous) to
  // BOTH.
  base::test::TestFuture<blink::mojom::DevicePostureType> change_future_2_1;
  base::test::TestFuture<blink::mojom::DevicePostureType> change_future_2_2;
  EXPECT_CALL(client1, OnPostureChanged(_))
      .WillOnce(base::test::InvokeFuture(change_future_2_1));
  EXPECT_CALL(client2, OnPostureChanged(_))
      .WillOnce(base::test::InvokeFuture(change_future_2_2));

  test_web_contents()->WasShown();

  EXPECT_EQ(change_future_2_1.Take(),
            blink::mojom::DevicePostureType::kContinuous);
  EXPECT_EQ(change_future_2_2.Take(),
            blink::mojom::DevicePostureType::kContinuous);
}

TEST_F(DevicePostureProviderImplTest, SkipUpdateOnShowIfUnchanged) {
  MockDevicePostureClient client;

  // Start HIDDEN.
  test_web_contents()->WasHidden();

  // Emulate platform state as Continuous (while hidden).
  provider()->OverrideDevicePostureForEmulation(
      blink::mojom::DevicePostureType::kContinuous);

  // Bind new client while hidden.
  base::test::TestFuture<blink::mojom::DevicePostureType> posture_future;
  blink::mojom::DevicePostureProvider* posture_provider = provider();
  EXPECT_CALL(client, OnPostureChanged(_)).Times(0);
  posture_provider->AddListenerAndGetCurrentPosture(
      client.BindAndGetRemote(), posture_future.GetCallback());

  // It should return the FALLBACK (kContinuous).
  EXPECT_EQ(posture_future.Get(), blink::mojom::DevicePostureType::kContinuous);
  client.Flush();

  // Show the web contents. It should NOT dispatch an update because the
  // dispatched initial state (kContinuous) matches the REAL state
  // (kContinuous).
  EXPECT_CALL(client, OnPostureChanged(_)).Times(0);
  test_web_contents()->WasShown();
  client.Flush();
}

}  // namespace content
