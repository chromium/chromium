// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/permission_overrides.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {
using blink::PermissionType;
using blink::mojom::PermissionStatus;
using testing::AllOf;
using testing::IsSupersetOf;
using testing::Pair;
using testing::SizeIs;
using testing::UnorderedElementsAre;
using url::Origin;

constexpr size_t kPermissionsCount = 37;

// Collects all permission statuses for the given input.
base::flat_map<blink::PermissionType, PermissionStatus> GetAll(
    const PermissionOverrides& overrides,
    const url::Origin& origin) {
  base::flat_map<blink::PermissionType, PermissionStatus> out;
  for (const auto& type : blink::GetAllPermissionTypes()) {
    std::optional<PermissionStatus> status = overrides.Get(origin, type);
    if (status) {
      out[type] = status.value();
    }
  }
  return out;
}

TEST(PermissionOverridesTest, GetOriginNoOverrides) {
  PermissionOverrides overrides;
  Origin url = Origin::Create(GURL("https://google.com/search?q=foo"));

  EXPECT_FALSE(overrides.Get(url, PermissionType::GEOLOCATION).has_value());
}

TEST(PermissionOverridesTests, SetMidi) {
  PermissionOverrides overrides;
  Origin url = Origin::Create(GURL("https://google.com/"));
  overrides.Set(url, PermissionType::MIDI_SYSEX, PermissionStatus::GRANTED);

  EXPECT_EQ(overrides.Get(url, PermissionType::MIDI_SYSEX),
            PermissionStatus::GRANTED);
  EXPECT_EQ(overrides.Get(url, PermissionType::MIDI),
            PermissionStatus::GRANTED);

  overrides.Set(url, PermissionType::MIDI_SYSEX, PermissionStatus::DENIED);
  EXPECT_EQ(overrides.Get(url, PermissionType::MIDI_SYSEX),
            PermissionStatus::DENIED);
  EXPECT_EQ(overrides.Get(url, PermissionType::MIDI),
            PermissionStatus::GRANTED);

  // Reset to all-granted MIDI.
  overrides.Set(url, PermissionType::MIDI_SYSEX, PermissionStatus::GRANTED);

  overrides.Set(url, PermissionType::MIDI, PermissionStatus::DENIED);
  EXPECT_EQ(overrides.Get(url, PermissionType::MIDI), PermissionStatus::DENIED);
  EXPECT_EQ(overrides.Get(url, PermissionType::MIDI_SYSEX),
            PermissionStatus::DENIED);
}

TEST(PermissionOverridesTest, GetBasic) {
  PermissionOverrides overrides;
  Origin url = Origin::Create(GURL("https://google.com/search?q=foo"));
  overrides.Set(url, PermissionType::GEOLOCATION, PermissionStatus::GRANTED);

  EXPECT_EQ(overrides.Get(url, PermissionType::GEOLOCATION),
            PermissionStatus::GRANTED);
}

TEST(PermissionOverridesTest, GetAllStates) {
  PermissionOverrides overrides;
  Origin url = Origin::Create(GURL("https://google.com/search?q=foo"));

  // Override some settings.
  overrides.Set(url, PermissionType::GEOLOCATION, PermissionStatus::GRANTED);
  overrides.Set(url, PermissionType::NOTIFICATIONS, PermissionStatus::DENIED);
  overrides.Set(url, PermissionType::AUDIO_CAPTURE, PermissionStatus::ASK);

  // Check that overrides are saved for the given url.
  EXPECT_EQ(overrides.Get(url, PermissionType::GEOLOCATION),
            PermissionStatus::GRANTED);

  EXPECT_EQ(overrides.Get(url, PermissionType::NOTIFICATIONS),
            PermissionStatus::DENIED);

  EXPECT_EQ(overrides.Get(url, PermissionType::AUDIO_CAPTURE),
            PermissionStatus::ASK);
}

TEST(PermissionOverridesTest, GetReturnsNullOptionalIfMissingOverride) {
  PermissionOverrides overrides;
  Origin url = Origin::Create(GURL("https://google.com/search?q=foo"));

  // Override some settings.
  overrides.Set(url, PermissionType::GEOLOCATION, PermissionStatus::GRANTED);
  overrides.Set(url, PermissionType::NOTIFICATIONS, PermissionStatus::DENIED);
  overrides.Set(url, PermissionType::AUDIO_CAPTURE, PermissionStatus::ASK);

  // If type was not overridden, report false.
  EXPECT_FALSE(overrides.Get(url, PermissionType::BACKGROUND_SYNC).has_value());

  // If URL not overridden, should report false.
  EXPECT_FALSE(overrides
                   .Get(Origin::Create(GURL("https://facebook.com/")),
                        PermissionType::GEOLOCATION)
                   .has_value());
}

TEST(PermissionOverridesTest, GetAllOverrides) {
  PermissionOverrides overrides;
  Origin url = Origin::Create(GURL("https://google.com/search?q=foo"));

  // Override some settings.
  base::flat_map<blink::PermissionType, blink::mojom::PermissionStatus>
      expected_override_for_origin;
  expected_override_for_origin[PermissionType::GEOLOCATION] =
      PermissionStatus::GRANTED;
  expected_override_for_origin[PermissionType::NOTIFICATIONS] =
      PermissionStatus::DENIED;
  expected_override_for_origin[PermissionType::AUDIO_CAPTURE] =
      PermissionStatus::ASK;

  overrides.Set(url, PermissionType::GEOLOCATION, PermissionStatus::GRANTED);
  overrides.Set(url, PermissionType::NOTIFICATIONS, PermissionStatus::DENIED);
  overrides.Set(url, PermissionType::AUDIO_CAPTURE, PermissionStatus::ASK);

  EXPECT_THAT(GetAll(overrides, url),
              testing::Eq(expected_override_for_origin));
  EXPECT_THAT(GetAll(overrides, Origin::Create(GURL("https://imgur.com/"))),
              testing::IsEmpty());
}

TEST(PermissionOverridesTest, SameOriginSameOverrides) {
  PermissionOverrides overrides;
  Origin url = Origin::Create(GURL("https://google.com/search?q=foo"));

  // Override some settings.
  overrides.Set(url, PermissionType::GEOLOCATION, PermissionStatus::GRANTED);
  overrides.Set(url, PermissionType::NOTIFICATIONS, PermissionStatus::DENIED);
  overrides.Set(url, PermissionType::AUDIO_CAPTURE, PermissionStatus::ASK);

  EXPECT_EQ(overrides.Get(Origin::Create(GURL("https://google.com")),
                          PermissionType::GEOLOCATION),
            PermissionStatus::GRANTED);
  EXPECT_EQ(overrides.Get(Origin::Create(GURL("https://google.com/")),
                          PermissionType::AUDIO_CAPTURE),
            PermissionStatus::ASK);
}

TEST(PermissionOverridesTest, DifferentOriginExpectations) {
  PermissionOverrides overrides;
  Origin url = Origin::Create(GURL("https://google.com/search?q=foo"));

  // Override some settings.
  overrides.Set(url, PermissionType::GEOLOCATION, PermissionStatus::GRANTED);

  // Different origin examples.
  EXPECT_FALSE(overrides
                   .Get(Origin::Create(GURL("https://www.google.com")),
                        PermissionType::GEOLOCATION)
                   .has_value());
  EXPECT_FALSE(
      overrides
          .Get(Origin::Create(GURL("google.com")), PermissionType::GEOLOCATION)
          .has_value());
  EXPECT_FALSE(overrides
                   .Get(Origin::Create(GURL("http://google.com")),
                        PermissionType::GEOLOCATION)
                   .has_value());
}

TEST(PermissionOverridesTest, DifferentOriginsDifferentOverrides) {
  PermissionOverrides overrides;
  Origin first_url = Origin::Create(GURL("https://google.com/search?q=foo"));
  Origin second_url = Origin::Create(GURL("https://tumblr.com/fizz_buzz"));

  // Override some settings.
  overrides.Set(first_url, PermissionType::GEOLOCATION,
                PermissionStatus::GRANTED);
  overrides.Set(second_url, PermissionType::NOTIFICATIONS,
                PermissionStatus::ASK);

  // Origins do not interfere.
  EXPECT_EQ(overrides.Get(first_url, PermissionType::GEOLOCATION),
            PermissionStatus::GRANTED);
  EXPECT_FALSE(
      overrides.Get(first_url, PermissionType::NOTIFICATIONS).has_value());
  EXPECT_EQ(overrides.Get(second_url, PermissionType::NOTIFICATIONS),
            PermissionStatus::ASK);
  EXPECT_FALSE(
      overrides.Get(second_url, PermissionType::GEOLOCATION).has_value());
}

TEST(PermissionOverridesTest, ResetOneOrigin) {
  PermissionOverrides overrides;
  Origin url = Origin::Create(GURL("https://google.com/search?q=foo"));
  Origin other_url = Origin::Create(GURL("https://pinterest.com/index.php"));

  overrides.Set(url, PermissionType::GEOLOCATION, PermissionStatus::GRANTED);
  overrides.Set(other_url, PermissionType::GEOLOCATION,
                PermissionStatus::GRANTED);
  EXPECT_EQ(overrides.Get(url, PermissionType::GEOLOCATION),
            PermissionStatus::GRANTED);
  EXPECT_EQ(overrides.Get(other_url, PermissionType::GEOLOCATION),
            PermissionStatus::GRANTED);

  overrides.Reset(url);
  EXPECT_FALSE(overrides.Get(url, PermissionType::GEOLOCATION).has_value());
  EXPECT_EQ(overrides.Get(other_url, PermissionType::GEOLOCATION),
            PermissionStatus::GRANTED);
}

TEST(PermissionOverridesTest, GrantPermissionsSetsSomeBlocksRest) {
  PermissionOverrides overrides;
  Origin url = Origin::Create(GURL("https://google.com/search?q=all"));

  overrides.GrantPermissions(
      url, {PermissionType::BACKGROUND_SYNC, PermissionType::BACKGROUND_FETCH,
            PermissionType::NOTIFICATIONS});

  // All other types should be blocked - will test a set of them.
  EXPECT_EQ(overrides.Get(url, PermissionType::GEOLOCATION),
            PermissionStatus::DENIED);
  EXPECT_EQ(overrides.Get(url, PermissionType::AUDIO_CAPTURE),
            PermissionStatus::DENIED);
  EXPECT_EQ(overrides.Get(url, PermissionType::MIDI_SYSEX),
            PermissionStatus::DENIED);
  EXPECT_EQ(overrides.Get(url, PermissionType::CLIPBOARD_READ_WRITE),
            PermissionStatus::DENIED);
  EXPECT_EQ(overrides.Get(url, PermissionType::WAKE_LOCK_SYSTEM),
            PermissionStatus::DENIED);

  // Specified types are granted.
  EXPECT_EQ(overrides.Get(url, PermissionType::NOTIFICATIONS),
            PermissionStatus::GRANTED);
  EXPECT_EQ(overrides.Get(url, PermissionType::BACKGROUND_SYNC),
            PermissionStatus::GRANTED);
  EXPECT_EQ(overrides.Get(url, PermissionType::BACKGROUND_FETCH),
            PermissionStatus::GRANTED);
}

TEST(PermissionOverridesTest, GrantPermissions_AllOriginsShadowing) {
  using enum blink::PermissionType;
  using enum PermissionStatus;
  PermissionOverrides overrides;

  // Override some types for all origins.
  overrides.GrantPermissions(std::nullopt, {GEOLOCATION, AUDIO_CAPTURE});

  {
    Origin origin = Origin::Create(GURL("https://google.com/search?q=all"));

    // Override other permissions types for one origin.
    overrides.GrantPermissions(
        origin, {BACKGROUND_SYNC, BACKGROUND_FETCH, NOTIFICATIONS});

    // The per-origin overrides are respected.
    EXPECT_EQ(overrides.Get(origin, NOTIFICATIONS), GRANTED);
    EXPECT_EQ(overrides.Get(origin, BACKGROUND_SYNC), GRANTED);
    EXPECT_EQ(overrides.Get(origin, BACKGROUND_FETCH), GRANTED);

    // Global overrides are shadowed by the single origin's `GrantPermissions`
    // call.
    EXPECT_EQ(overrides.Get(origin, GEOLOCATION), DENIED);
    EXPECT_EQ(overrides.Get(origin, AUDIO_CAPTURE), DENIED);

    EXPECT_THAT(GetAll(overrides, origin),
                AllOf(IsSupersetOf({
                          std::make_pair(BACKGROUND_SYNC, GRANTED),
                          std::make_pair(BACKGROUND_FETCH, GRANTED),
                          std::make_pair(NOTIFICATIONS, GRANTED),
                          std::make_pair(GEOLOCATION, DENIED),
                          std::make_pair(AUDIO_CAPTURE, DENIED),
                      }),
                      SizeIs(kPermissionsCount)));
    EXPECT_THAT(
        GetAll(overrides, url::Origin::Create(GURL("https://example.com"))),
        AllOf(IsSupersetOf({
                  std::make_pair(BACKGROUND_SYNC, DENIED),
                  std::make_pair(BACKGROUND_FETCH, DENIED),
                  std::make_pair(NOTIFICATIONS, DENIED),
                  std::make_pair(GEOLOCATION, GRANTED),
                  std::make_pair(AUDIO_CAPTURE, GRANTED),
              }),
              SizeIs(kPermissionsCount)));
  }
  {
    // For a different origin, only the global overrides take effect.
    Origin origin = Origin::Create(GURL("https://www.google.com/search?q=all"));

    EXPECT_EQ(overrides.Get(origin, NOTIFICATIONS), DENIED);
    EXPECT_EQ(overrides.Get(origin, BACKGROUND_SYNC), DENIED);
    EXPECT_EQ(overrides.Get(origin, BACKGROUND_FETCH), DENIED);

    EXPECT_EQ(overrides.Get(origin, GEOLOCATION), GRANTED);
    EXPECT_EQ(overrides.Get(origin, AUDIO_CAPTURE), GRANTED);

    EXPECT_THAT(GetAll(overrides, origin),
                AllOf(IsSupersetOf({
                          std::make_pair(BACKGROUND_SYNC, DENIED),
                          std::make_pair(BACKGROUND_FETCH, DENIED),
                          std::make_pair(NOTIFICATIONS, DENIED),
                          std::make_pair(GEOLOCATION, GRANTED),
                          std::make_pair(AUDIO_CAPTURE, GRANTED),
                      }),
                      SizeIs(kPermissionsCount)));
  }
}

TEST(PermissionOverridesTest, SetPermission_AllOriginsNoShadowing) {
  using enum blink::PermissionType;
  using enum PermissionStatus;
  PermissionOverrides overrides;

  // Override a permission type for all origins.
  overrides.Set(std::nullopt, GEOLOCATION, GRANTED);

  {
    Origin origin = Origin::Create(GURL("https://google.com/search?q=all"));

    // Override another permission type for one origin.
    overrides.Set(origin, BACKGROUND_SYNC, GRANTED);

    // The per-origin override is respected.
    EXPECT_EQ(overrides.Get(origin, BACKGROUND_SYNC), GRANTED);

    // Global overrides are not shadowed by the single origin's `Set` call.
    EXPECT_EQ(overrides.Get(origin, GEOLOCATION), GRANTED);

    EXPECT_THAT(GetAll(overrides, origin),
                UnorderedElementsAre(Pair(GEOLOCATION, GRANTED),
                                     Pair(BACKGROUND_SYNC, GRANTED)));
    EXPECT_THAT(
        GetAll(overrides, url::Origin::Create(GURL("https://example.com"))),
        UnorderedElementsAre(Pair(GEOLOCATION, GRANTED)));
  }
  {
    // For a different origin, only the global overrides take effect.
    Origin origin = Origin::Create(GURL("https://www.google.com/search?q=all"));

    EXPECT_EQ(overrides.Get(origin, BACKGROUND_SYNC), std::nullopt);

    EXPECT_EQ(overrides.Get(origin, GEOLOCATION), GRANTED);
    EXPECT_THAT(GetAll(overrides, origin),
                UnorderedElementsAre(Pair(GEOLOCATION, GRANTED)));
  }
}

}  // namespace
}  // namespace content
