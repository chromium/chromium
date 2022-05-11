// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "remoting/host/chromeos/features.h"
#include "remoting/host/chromeos/scoped_fake_ash_display_util.h"
#include "remoting/host/desktop_display_info_loader.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {
using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;

constexpr DisplayId kAnyId = 1234567890123456789;

auto Id(DisplayId id) {
  return testing::Field("id", &DisplayGeometry::id, testing::Eq(id));
}

auto Origin(int x, int y) {
  return testing::AllOf(
      testing::Field("x", &DisplayGeometry::x, testing::Eq(x)),
      testing::Field("y", &DisplayGeometry::y, testing::Eq(y)));
}

auto Dimensions(uint32_t width, uint32_t height) {
  return testing::AllOf(
      testing::Field("width", &DisplayGeometry::width, testing::Eq(width)),
      testing::Field("height", &DisplayGeometry::height, testing::Eq(height)));
}

auto Dpi(uint32_t dpi) {
  return testing::Field("dpi", &DisplayGeometry::dpi, testing::Eq(dpi));
}

auto IsDefault(bool is_default) {
  return testing::Field("is_default", &DisplayGeometry::is_default,
                        testing::Eq(is_default));
}

template <typename... Matchers>
auto DisplayWith(Matchers... matchers) {
  return testing::AllOf(std::forward<Matchers>(matchers)...);
}

template <typename... Matchers>
auto IsSingleDisplayWith(Matchers... matchers) {
  return ElementsAre(DisplayWith(std::forward<Matchers>(matchers)...));
}

}  // namespace

class DesktopDisplayInfoLoaderChromeOsTest : public ::testing::Test {
 public:
  DesktopDisplayInfoLoaderChromeOsTest() = default;
  DesktopDisplayInfoLoaderChromeOsTest(
      const DesktopDisplayInfoLoaderChromeOsTest&) = delete;
  DesktopDisplayInfoLoaderChromeOsTest& operator=(
      const DesktopDisplayInfoLoaderChromeOsTest&) = delete;
  ~DesktopDisplayInfoLoaderChromeOsTest() override = default;

  test::ScopedFakeAshDisplayUtil& display_util() { return display_util_; }

  std::vector<DisplayGeometry> CalculateDisplayInfo() {
    return display_info_loader_->GetCurrentDisplayInfo().displays();
  }

 private:
  base::test::SingleThreadTaskEnvironment environment_;
  base::test::ScopedFeatureList features_{features::kEnableMultiMonitorsInCrd};
  test::ScopedFakeAshDisplayUtil display_util_;

  std::unique_ptr<DesktopDisplayInfoLoader> display_info_loader_ =
      DesktopDisplayInfoLoader::Create();
};

TEST_F(DesktopDisplayInfoLoaderChromeOsTest,
       ShouldReturnNothingIfFeatureIsDisabled) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(features::kEnableMultiMonitorsInCrd);

  display_util().AddPrimaryDisplay();
  display_util().AddDisplayWithId(111);

  EXPECT_THAT(CalculateDisplayInfo(), ElementsAre());
}

TEST_F(DesktopDisplayInfoLoaderChromeOsTest, ShouldReturnDisplayId) {
  constexpr int64_t kFirstDisplayId = (0xFFl << 40);
  constexpr int64_t kSecondDisplayId = (0xFFl << 42);

  display_util().AddDisplayWithId(kFirstDisplayId);
  display_util().AddDisplayWithId(kSecondDisplayId);

  EXPECT_THAT(CalculateDisplayInfo(),
              ElementsAre(DisplayWith(Id(kFirstDisplayId)),
                          DisplayWith(Id(kSecondDisplayId))));
}

TEST_F(DesktopDisplayInfoLoaderChromeOsTest, ShouldReturnDisplayBounds) {
  display_util().AddDisplayFromSpecWithId("10+20-1000x500", kAnyId);

  EXPECT_THAT(CalculateDisplayInfo(),
              IsSingleDisplayWith(Origin(10, 20), Dimensions(1000, 500)));
}

TEST_F(DesktopDisplayInfoLoaderChromeOsTest, ShouldReturnMultipleDisplays) {
  display_util().AddDisplayFromSpecWithId("10+20-1000x500", 111);
  display_util().AddDisplayFromSpecWithId("110+220-2000x1000", 222);

  EXPECT_THAT(
      CalculateDisplayInfo(),
      ElementsAre(
          DisplayWith(Id(111), Origin(10, 20), Dimensions(1000, 500)),
          DisplayWith(Id(222), Origin(110, 220), Dimensions(2000, 1000))));
}

TEST_F(DesktopDisplayInfoLoaderChromeOsTest,
       ShouldReturnNativeResolutionEvenWhenDeviceScaleFactorIsSet) {
  display_util().AddDisplayFromSpecWithId("1000x500*2.25", kAnyId);

  EXPECT_THAT(CalculateDisplayInfo(),
              IsSingleDisplayWith(Dimensions(1000, 500)));
}

TEST_F(DesktopDisplayInfoLoaderChromeOsTest,
       ShouldSetIsDefaultForPrimaryDisplay) {
  display_util().AddDisplayWithId(111);
  display_util().AddPrimaryDisplay(222);

  EXPECT_THAT(CalculateDisplayInfo(),
              UnorderedElementsAre(DisplayWith(Id(222), IsDefault(true)),
                                   DisplayWith(Id(111), IsDefault(false))));
}

TEST_F(DesktopDisplayInfoLoaderChromeOsTest,
       OriginShouldRespectDeviceScaleFactor) {
  display_util().AddDisplayFromSpecWithId("10+20-100x100*2", kAnyId);

  EXPECT_THAT(CalculateDisplayInfo(), IsSingleDisplayWith(Origin(5, 10)));
}

TEST_F(DesktopDisplayInfoLoaderChromeOsTest, ShouldSetDefaultDpi) {
  constexpr int kDefaultDpi = 96;
  display_util().AddDisplayFromSpecWithId("1000x500", kAnyId);

  EXPECT_THAT(CalculateDisplayInfo(), IsSingleDisplayWith(Dpi(kDefaultDpi)));
}

TEST_F(DesktopDisplayInfoLoaderChromeOsTest, ShouldSetDpiBasedOnScaleFactor) {
  // scale factor = dpi / default_dpi (which is 96)
  const int scale_factor = 3;
  const int expected_dpi = scale_factor * 96;
  display_util().AddDisplayFromSpecWithId("1000x500*3", kAnyId);

  EXPECT_THAT(CalculateDisplayInfo(), IsSingleDisplayWith(Dpi(expected_dpi)));
}

TEST_F(DesktopDisplayInfoLoaderChromeOsTest,
       ShouldRotateBoundsWhenRotated90Degrees) {
  display_util().AddDisplayFromSpecWithId("1000x500/r", kAnyId);

  EXPECT_THAT(CalculateDisplayInfo(),
              IsSingleDisplayWith(Dimensions(500, 1000)));
}

TEST_F(DesktopDisplayInfoLoaderChromeOsTest,
       ShouldKeepBoundsWhenRotated180Degrees) {
  display_util().AddDisplayFromSpecWithId("1000x500/u", kAnyId);

  EXPECT_THAT(CalculateDisplayInfo(),
              IsSingleDisplayWith(Dimensions(1000, 500)));
}

TEST_F(DesktopDisplayInfoLoaderChromeOsTest,
       ShouldRotateBoundsWhenRotated270Degrees) {
  display_util().AddDisplayFromSpecWithId("1000x500/l", kAnyId);

  EXPECT_THAT(CalculateDisplayInfo(),
              IsSingleDisplayWith(Dimensions(500, 1000)));
}

}  // namespace remoting
