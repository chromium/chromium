// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/haptics/haptics_manager_impl.h"

#include <limits>

#include "base/test/test_future.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/device/device_service_test_base.h"
#include "services/device/public/mojom/haptics.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

class MockHapticsManagerImpl : public HapticsManagerImpl {
 public:
  MockHapticsManagerImpl() = default;
  MockHapticsManagerImpl(const MockHapticsManagerImpl&) = delete;
  MockHapticsManagerImpl& operator=(const MockHapticsManagerImpl&) = delete;
  ~MockHapticsManagerImpl() override = default;

  MOCK_METHOD(void,
              PlatformPlayHaptics,
              (mojom::HapticEffect, double),
              (override));
};

class HapticsManagerImplTest : public DeviceServiceTestBase {
 public:
  HapticsManagerImplTest() = default;
  HapticsManagerImplTest(const HapticsManagerImplTest&) = delete;
  HapticsManagerImplTest& operator=(const HapticsManagerImplTest&) = delete;
  ~HapticsManagerImplTest() override = default;

 protected:
  void SetUp() override {
    DeviceServiceTestBase::SetUp();
    receiver_.Bind(haptics_service_.BindNewPipeAndPassReceiver());
  }

  MockHapticsManagerImpl impl_;
  mojo::Receiver<mojom::HapticsService> receiver_{&impl_};
  mojo::Remote<mojom::HapticsService> haptics_service_;
};

TEST_F(HapticsManagerImplTest, PlayHaptics) {
  constexpr struct {
    mojom::HapticEffect effect;
    double intensity;
  } kTestCases[] = {
      {mojom::HapticEffect::kTick, 0.75},   {mojom::HapticEffect::kEdge, 1.0},
      {mojom::HapticEffect::kHint, 0.3},    {mojom::HapticEffect::kAlign, 0.5},
      {mojom::HapticEffect::kSuccess, 0.8}, {mojom::HapticEffect::kError, 0.9},
  };

  for (const auto& test_case : kTestCases) {
    base::test::TestFuture<mojom::HapticEffect, double> future;
    EXPECT_CALL(impl_, PlatformPlayHaptics)
        .WillOnce(base::test::InvokeFuture(future));

    haptics_service_->PlayHaptics(test_case.effect, test_case.intensity);

    EXPECT_EQ(future.Get<0>(), test_case.effect);
    EXPECT_DOUBLE_EQ(future.Get<1>(), test_case.intensity);
  }
}

TEST_F(HapticsManagerImplTest, BindHapticsService) {
  mojo::Remote<mojom::HapticsService> remote;
  device_service()->BindHapticsService(remote.BindNewPipeAndPassReceiver());

  remote->PlayHaptics(mojom::HapticEffect::kTick, 0.5);
  remote.FlushForTesting();
  EXPECT_TRUE(remote.is_connected());
}

TEST_F(HapticsManagerImplTest, InvalidIntensityReportsBadMessage) {
  constexpr double kInvalidIntensities[] = {
      std::numeric_limits<double>::quiet_NaN(),
      std::numeric_limits<double>::infinity(),
      -std::numeric_limits<double>::infinity(),
      -0.1,
      1.1,
  };

  for (double invalid_intensity : kInvalidIntensities) {
    mojo::test::BadMessageObserver bad_message_observer;
    mojo::Remote<mojom::HapticsService> remote;
    device_service()->BindHapticsService(remote.BindNewPipeAndPassReceiver());

    remote->PlayHaptics(mojom::HapticEffect::kTick, invalid_intensity);
    EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
              "Invalid haptic intensity.");
  }
}

}  // namespace

}  // namespace device
