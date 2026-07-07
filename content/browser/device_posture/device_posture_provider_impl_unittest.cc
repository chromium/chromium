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

}  // namespace content
