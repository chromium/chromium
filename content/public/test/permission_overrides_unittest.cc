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
using url::Origin;

TEST(PermissionOverridesTest, GetOriginNoOverrides) {
  PermissionOverrides overrides;
  Origin url = Origin::Create(GURL("https://google.com/search?q=foo"));

  EXPECT_FALSE(overrides.Get(url, PermissionType::GEOLOCATION).has_value());
}

TEST(PermissionOverridesTests, SetMidi) {
  PermissionOverrides overrides;
  Origin url = Origin::Create(GURL("https://google.com/"));
  overrides.Set(url, PermissionType::MIDI_SYSEX, PermissionStatus::GRANTED);

  EXPECT_EQ(*overrides.Get(url, PermissionType::MIDI_SYSEX),
            PermissionStatus::GRANTED);
  EXPECT_EQ(*overrides.Get(url, PermissionType::MIDI),
            PermissionStatus::GRANTED);

  overrides.Set(url, PermissionType::MIDI_SYSEX, PermissionStatus::DENIED);
  EXPECT_EQ(*overrides.Get(url, PermissionType::MIDI_SYSEX),
            PermissionStatus::DENIED);
  EXPECT_EQ(*overrides.Get(url, PermissionType::MIDI),
            PermissionStatus::GRANTED);

  // Reset to all-granted MIDI.
  overrides.Set(url, PermissionType::MIDI_SYSEX, PermissionStatus::GRANTED);

  overrides.Set(url, PermissionType::MIDI, PermissionStatus::DENIED);
  EXPECT_EQ(*overrides.Get(url, PermissionType::MIDI),
            PermissionStatus::DENIED);
  EXPECT_EQ(*overrides.Get(url, PermissionType::MIDI_SYSEX),
            PermissionStatus::DENIED);
}

TEST(PermissionOverridesTest, GetBasic) {
  PermissionOverrides overrides;
  Origin url = Origin::Create(GURL("https://google.com/search?q=foo"));
  overrides.Set(url, PermissionType::GEOLOCATION, PermissionStatus::GRANTED);

  EXPECT_EQ(*overrides.Get(url, PermissionType::GEOLOCATION),
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
  EXPECT_EQ(*overrides.Get(url, PermissionType::GEOLOCATION),
            PermissionStatus::GRANTED);

  EXPECT_EQ(*overrides.Get(url, PermissionType::NOTIFICATIONS),
            PermissionStatus::DENIED);

  EXPECT_EQ(*overrides.Get(url, PermissionType::AUDIO_CAPTURE),
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

  EXPECT_THAT(overrides.GetAllForTest(url),
              testing::Eq(expected_override_for_origin));
  EXPECT_THAT(
      overrides.GetAllForTest(Origin::Create(GURL("https://imgur.com/"))),
      testing::IsEmpty());
}

TEST(PermissionOverridesTest, SameOriginSameOverrides) {
  PermissionOverrides overrides;
  Origin url = Origin::Create(GURL("https://google.com/search?q=foo"));

  // Override some settings.
  overrides.Set(url, PermissionType::GEOLOCATION, PermissionStatus::GRANTED);
  overrides.Set(url, PermissionType::NOTIFICATIONS, PermissionStatus::DENIED);
  overrides.Set(url, PermissionType::AUDIO_CAPTURE, PermissionStatus::ASK);

  EXPECT_EQ(*overrides.Get(Origin::Create(GURL("https://google.com")),
                           PermissionType::GEOLOCATION),
            PermissionStatus::GRANTED);
  EXPECT_EQ(*overrides.Get(Origin::Create(GURL("https://google.com/")),
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
  EXPECT_EQ(*overrides.Get(first_url, PermissionType::GEOLOCATION),
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
  EXPECT_EQ(*overrides.Get(url, PermissionType::GEOLOCATION),
            PermissionStatus::GRANTED);
  EXPECT_EQ(*overrides.Get(other_url, PermissionType::GEOLOCATION),
            PermissionStatus::GRANTED);

  overrides.Reset(url);
  EXPECT_FALSE(overrides.Get(url, PermissionType::GEOLOCATION).has_value());
  EXPECT_EQ(*overrides.Get(other_url, PermissionType::GEOLOCATION),
            PermissionStatus::GRANTED);
}

TEST(PermissionOverridesTest, GrantPermissionsSetsSomeBlocksRest) {
  PermissionOverrides overrides;
  Origin url = Origin::Create(GURL("https://google.com/search?q=all"));

  overrides.GrantPermissions(
      url, {PermissionType::BACKGROUND_SYNC, PermissionType::BACKGROUND_FETCH,
            PermissionType::NOTIFICATIONS});

  // All other types should be blocked - will test a set of them.
  EXPECT_EQ(*overrides.Get(url, PermissionType::GEOLOCATION),
            PermissionStatus::DENIED);
  EXPECT_EQ(*overrides.Get(url, PermissionType::AUDIO_CAPTURE),
            PermissionStatus::DENIED);
  EXPECT_EQ(*overrides.Get(url, PermissionType::MIDI_SYSEX),
            PermissionStatus::DENIED);
  EXPECT_EQ(*overrides.Get(url, PermissionType::CLIPBOARD_READ_WRITE),
            PermissionStatus::DENIED);
  EXPECT_EQ(*overrides.Get(url, PermissionType::WAKE_LOCK_SYSTEM),
            PermissionStatus::DENIED);

  // Specified types are granted.
  EXPECT_EQ(*overrides.Get(url, PermissionType::NOTIFICATIONS),
            PermissionStatus::GRANTED);
  EXPECT_EQ(*overrides.Get(url, PermissionType::BACKGROUND_SYNC),
            PermissionStatus::GRANTED);
  EXPECT_EQ(*overrides.Get(url, PermissionType::BACKGROUND_FETCH),
            PermissionStatus::GRANTED);
}

}  // namespace
}  // namespace content
