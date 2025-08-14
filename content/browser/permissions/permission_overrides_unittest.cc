// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/permissions/permission_overrides.h"

#include <optional>

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
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin) {
  base::flat_map<blink::PermissionType, PermissionStatus> out;
  for (const auto& type : blink::GetAllPermissionTypes()) {
    std::optional<PermissionStatus> status =
        overrides.Get(requesting_origin, embedding_origin, type);
    if (status) {
      out[type] = status.value();
    }
  }
  return out;
}

TEST(PermissionOverridesTest, GetOriginNoOverrides) {
  PermissionOverrides overrides;
  Origin url = Origin::Create(GURL("https://google.com/search?q=foo"));

  EXPECT_FALSE(
      overrides.Get(url, url, PermissionType::GEOLOCATION).has_value());
}

TEST(PermissionOverridesTests, SetMidi) {
  PermissionOverrides overrides;
  Origin url = Origin::Create(GURL("https://google.com/"));
  overrides.Set(url, url, PermissionType::MIDI_SYSEX,
                PermissionStatus::GRANTED);

  EXPECT_EQ(overrides.Get(url, url, PermissionType::MIDI_SYSEX),
            PermissionStatus::GRANTED);
  EXPECT_EQ(overrides.Get(url, url, PermissionType::MIDI),
            PermissionStatus::GRANTED);

  overrides.Set(url, url, PermissionType::MIDI_SYSEX, PermissionStatus::DENIED);
  EXPECT_EQ(overrides.Get(url, url, PermissionType::MIDI_SYSEX),
            PermissionStatus::DENIED);
  EXPECT_EQ(overrides.Get(url, url, PermissionType::MIDI),
            PermissionStatus::GRANTED);

  // Reset to all-granted MIDI.
  overrides.Set(url, url, PermissionType::MIDI_SYSEX,
                PermissionStatus::GRANTED);

  overrides.Set(url, url, PermissionType::MIDI, PermissionStatus::DENIED);
  EXPECT_EQ(overrides.Get(url, url, PermissionType::MIDI),
            PermissionStatus::DENIED);
  EXPECT_EQ(overrides.Get(url, url, PermissionType::MIDI_SYSEX),
            PermissionStatus::DENIED);
}

TEST(PermissionOverridesTest, GetBasic) {
  PermissionOverrides overrides;
  Origin url = Origin::Create(GURL("https://google.com/search?q=foo"));
  overrides.Set(url, url, PermissionType::GEOLOCATION,
                PermissionStatus::GRANTED);

  EXPECT_EQ(overrides.Get(url, url, PermissionType::GEOLOCATION),
            PermissionStatus::GRANTED);
}

TEST(PermissionOverridesTest, GetAllStates) {
  PermissionOverrides overrides;
  Origin url = Origin::Create(GURL("https://google.com/search?q=foo"));

  // Override some settings.
  overrides.Set(url, url, PermissionType::GEOLOCATION,
                PermissionStatus::GRANTED);
  overrides.Set(url, url, PermissionType::NOTIFICATIONS,
                PermissionStatus::DENIED);
  overrides.Set(url, url, PermissionType::AUDIO_CAPTURE, PermissionStatus::ASK);

  // Check that overrides are saved for the given url.
  EXPECT_EQ(overrides.Get(url, url, PermissionType::GEOLOCATION),
            PermissionStatus::GRANTED);

  EXPECT_EQ(overrides.Get(url, url, PermissionType::NOTIFICATIONS),
            PermissionStatus::DENIED);

  EXPECT_EQ(overrides.Get(url, url, PermissionType::AUDIO_CAPTURE),
            PermissionStatus::ASK);
}

TEST(PermissionOverridesTest, GetReturnsNullOptionalIfMissingOverride) {
  PermissionOverrides overrides;
  Origin url = Origin::Create(GURL("https://google.com/search?q=foo"));

  // Override some settings.
  overrides.Set(url, url, PermissionType::GEOLOCATION,
                PermissionStatus::GRANTED);
  overrides.Set(url, url, PermissionType::NOTIFICATIONS,
                PermissionStatus::DENIED);
  overrides.Set(url, url, PermissionType::AUDIO_CAPTURE, PermissionStatus::ASK);

  // If type was not overridden, report false.
  EXPECT_FALSE(
      overrides.Get(url, url, PermissionType::BACKGROUND_SYNC).has_value());

  // If URL not overridden, should report false.
  Origin no_overrides_origin = Origin::Create(GURL("https://facebook.com/"));
  EXPECT_FALSE(overrides
                   .Get(no_overrides_origin, no_overrides_origin,
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

  overrides.Set(url, url, PermissionType::GEOLOCATION,
                PermissionStatus::GRANTED);
  overrides.Set(url, url, PermissionType::NOTIFICATIONS,
                PermissionStatus::DENIED);
  overrides.Set(url, url, PermissionType::AUDIO_CAPTURE, PermissionStatus::ASK);

  EXPECT_THAT(GetAll(overrides, url, url),
              testing::Eq(expected_override_for_origin));

  Origin no_overrides_origin = Origin::Create(GURL("https://imgur.com/"));
  EXPECT_THAT(GetAll(overrides, no_overrides_origin, no_overrides_origin),
              testing::IsEmpty());
}

TEST(PermissionOverridesTest, SameOriginSameOverrides) {
  PermissionOverrides overrides;
  Origin url = Origin::Create(GURL("https://google.com/search?q=foo"));

  // Override some settings.
  overrides.Set(url, url, PermissionType::GEOLOCATION,
                PermissionStatus::GRANTED);
  overrides.Set(url, url, PermissionType::NOTIFICATIONS,
                PermissionStatus::DENIED);
  overrides.Set(url, url, PermissionType::AUDIO_CAPTURE, PermissionStatus::ASK);

  Origin overriden_origin = Origin::Create(GURL("https://google.com"));
  EXPECT_EQ(overrides.Get(overriden_origin, overriden_origin,
                          PermissionType::GEOLOCATION),
            PermissionStatus::GRANTED);
  EXPECT_EQ(overrides.Get(overriden_origin, overriden_origin,
                          PermissionType::AUDIO_CAPTURE),
            PermissionStatus::ASK);
}

TEST(PermissionOverridesTest, DifferentOriginExpectations) {
  PermissionOverrides overrides;
  Origin url = Origin::Create(GURL("https://google.com/search?q=foo"));

  // Override some settings.
  overrides.Set(url, url, PermissionType::GEOLOCATION,
                PermissionStatus::GRANTED);

  Origin origin = Origin::Create(GURL("https://www.google.com"));
  EXPECT_FALSE(overrides.Get(origin, origin, PermissionType::GEOLOCATION));

  origin = Origin::Create(GURL("http://google.com"));
  EXPECT_FALSE(overrides.Get(origin, origin, PermissionType::GEOLOCATION));

  origin = Origin();
  EXPECT_FALSE(overrides.Get(origin, origin, PermissionType::GEOLOCATION));
}

TEST(PermissionOverridesTest, DifferentOriginsDifferentOverrides) {
  PermissionOverrides overrides;
  Origin first_url = Origin::Create(GURL("https://google.com/search?q=foo"));
  Origin second_url = Origin::Create(GURL("https://tumblr.com/fizz_buzz"));

  // Override some settings.
  overrides.Set(first_url, first_url, PermissionType::GEOLOCATION,
                PermissionStatus::GRANTED);
  overrides.Set(second_url, second_url, PermissionType::NOTIFICATIONS,
                PermissionStatus::ASK);

  // Origins do not interfere.
  EXPECT_EQ(overrides.Get(first_url, first_url, PermissionType::GEOLOCATION),
            PermissionStatus::GRANTED);
  EXPECT_FALSE(
      overrides.Get(first_url, first_url, PermissionType::NOTIFICATIONS)
          .has_value());
  EXPECT_EQ(
      overrides.Get(second_url, second_url, PermissionType::NOTIFICATIONS),
      PermissionStatus::ASK);
  EXPECT_FALSE(
      overrides.Get(second_url, second_url, PermissionType::GEOLOCATION)
          .has_value());
}

TEST(PermissionOverridesTest, GrantPermissions_SetsSomeBlocksRest) {
  PermissionOverrides overrides;
  Origin url = Origin::Create(GURL("https://google.com/search?q=all"));

  overrides.GrantPermissions(
      url, url,
      {PermissionType::BACKGROUND_SYNC, PermissionType::BACKGROUND_FETCH,
       PermissionType::NOTIFICATIONS});

  // All other types should be blocked - will test a set of them.
  EXPECT_EQ(overrides.Get(url, url, PermissionType::GEOLOCATION),
            PermissionStatus::DENIED);
  EXPECT_EQ(overrides.Get(url, url, PermissionType::AUDIO_CAPTURE),
            PermissionStatus::DENIED);
  EXPECT_EQ(overrides.Get(url, url, PermissionType::MIDI_SYSEX),
            PermissionStatus::DENIED);
  EXPECT_EQ(overrides.Get(url, url, PermissionType::CLIPBOARD_READ_WRITE),
            PermissionStatus::DENIED);
  EXPECT_EQ(overrides.Get(url, url, PermissionType::WAKE_LOCK_SYSTEM),
            PermissionStatus::DENIED);

  // Specified types are granted.
  EXPECT_EQ(overrides.Get(url, url, PermissionType::NOTIFICATIONS),
            PermissionStatus::GRANTED);
  EXPECT_EQ(overrides.Get(url, url, PermissionType::BACKGROUND_SYNC),
            PermissionStatus::GRANTED);
  EXPECT_EQ(overrides.Get(url, url, PermissionType::BACKGROUND_FETCH),
            PermissionStatus::GRANTED);
}

TEST(PermissionOverridesTest, GrantPermissions_OverwritesPreviousState) {
  using enum blink::PermissionType;
  using enum PermissionStatus;
  PermissionOverrides overrides;
  Origin origin = Origin::Create(GURL("https://google.com/"));

  overrides.GrantPermissions(origin, origin, {GEOLOCATION});
  ASSERT_THAT(GetAll(overrides, origin, origin),
              AllOf(IsSupersetOf({
                        std::make_pair(NOTIFICATIONS, DENIED),
                        std::make_pair(GEOLOCATION, GRANTED),
                    }),
                    SizeIs(kPermissionsCount)));

  overrides.GrantPermissions(origin, origin, {NOTIFICATIONS});
  EXPECT_THAT(GetAll(overrides, origin, origin),
              AllOf(IsSupersetOf({
                        std::make_pair(NOTIFICATIONS, GRANTED),
                        std::make_pair(GEOLOCATION, DENIED),
                    }),
                    SizeIs(kPermissionsCount)));
}

TEST(PermissionOverridesTest, GrantPermissions_AllOriginsShadowing) {
  using enum blink::PermissionType;
  using enum PermissionStatus;
  PermissionOverrides overrides;

  // Override some types for all origins.
  overrides.GrantPermissions(std::nullopt, std::nullopt,
                             {GEOLOCATION, AUDIO_CAPTURE});

  {
    Origin origin = Origin::Create(GURL("https://google.com/search?q=all"));

    // Override other permissions types for one origin.
    overrides.GrantPermissions(
        origin, origin, {BACKGROUND_SYNC, BACKGROUND_FETCH, NOTIFICATIONS});

    // The per-origin overrides are respected.
    EXPECT_EQ(overrides.Get(origin, origin, NOTIFICATIONS), GRANTED);
    EXPECT_EQ(overrides.Get(origin, origin, BACKGROUND_SYNC), GRANTED);
    EXPECT_EQ(overrides.Get(origin, origin, BACKGROUND_FETCH), GRANTED);

    // Global overrides are shadowed by the single origin's `GrantPermissions`
    // call.
    EXPECT_EQ(overrides.Get(origin, origin, GEOLOCATION), DENIED);
    EXPECT_EQ(overrides.Get(origin, origin, AUDIO_CAPTURE), DENIED);

    EXPECT_THAT(GetAll(overrides, origin, origin),
                AllOf(IsSupersetOf({
                          std::make_pair(BACKGROUND_SYNC, GRANTED),
                          std::make_pair(BACKGROUND_FETCH, GRANTED),
                          std::make_pair(NOTIFICATIONS, GRANTED),
                          std::make_pair(GEOLOCATION, DENIED),
                          std::make_pair(AUDIO_CAPTURE, DENIED),
                      }),
                      SizeIs(kPermissionsCount)));

    Origin no_overrides_origin = Origin::Create(GURL("https://example.com"));
    EXPECT_THAT(GetAll(overrides, no_overrides_origin, no_overrides_origin),
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

    EXPECT_EQ(overrides.Get(origin, origin, NOTIFICATIONS), DENIED);
    EXPECT_EQ(overrides.Get(origin, origin, BACKGROUND_SYNC), DENIED);
    EXPECT_EQ(overrides.Get(origin, origin, BACKGROUND_FETCH), DENIED);

    EXPECT_EQ(overrides.Get(origin, origin, GEOLOCATION), GRANTED);
    EXPECT_EQ(overrides.Get(origin, origin, AUDIO_CAPTURE), GRANTED);

    EXPECT_THAT(GetAll(overrides, origin, origin),
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
  overrides.Set(std::nullopt, std::nullopt, GEOLOCATION, GRANTED);

  {
    Origin origin = Origin::Create(GURL("https://google.com/search?q=all"));

    // Override another permission type for one origin.
    overrides.Set(origin, origin, BACKGROUND_SYNC, GRANTED);

    // The per-origin override is respected.
    EXPECT_EQ(overrides.Get(origin, origin, BACKGROUND_SYNC), GRANTED);

    // Global overrides are not shadowed by the single origin's `Set` call.
    EXPECT_EQ(overrides.Get(origin, origin, GEOLOCATION), GRANTED);

    EXPECT_THAT(GetAll(overrides, origin, origin),
                UnorderedElementsAre(Pair(GEOLOCATION, GRANTED),
                                     Pair(BACKGROUND_SYNC, GRANTED)));

    Origin no_overrides_origin = Origin::Create(GURL("https://example.com"));
    EXPECT_THAT(GetAll(overrides, no_overrides_origin, no_overrides_origin),
                UnorderedElementsAre(Pair(GEOLOCATION, GRANTED)));
  }
  {
    // For a different origin, only the global overrides take effect.
    Origin origin = Origin::Create(GURL("https://www.google.com/search?q=all"));

    EXPECT_EQ(overrides.Get(origin, origin, BACKGROUND_SYNC), std::nullopt);

    EXPECT_EQ(overrides.Get(origin, origin, GEOLOCATION), GRANTED);
    EXPECT_THAT(GetAll(overrides, origin, origin),
                UnorderedElementsAre(Pair(GEOLOCATION, GRANTED)));
  }
}

TEST(PermissionOverridesTest,
     StorageAccess_SameRequestingOriginDifferentEmbeddingSite) {
  PermissionOverrides overrides;
  Origin requesting_origin = Origin::Create(GURL("https://requesting.com/"));
  Origin embedding_origin_1 = Origin::Create(GURL("https://embedding1.com/"));
  Origin embedding_origin_2 = Origin::Create(GURL("https://embedding2.com/"));

  overrides.Set(requesting_origin, embedding_origin_1,
                PermissionType::STORAGE_ACCESS_GRANT,
                PermissionStatus::GRANTED);
  EXPECT_EQ(overrides.Get(requesting_origin, embedding_origin_1,
                          PermissionType::STORAGE_ACCESS_GRANT),
            PermissionStatus::GRANTED);

  // Show that a different embedding origin for the same requester is not the
  // same key.
  EXPECT_EQ(overrides.Get(requesting_origin, embedding_origin_2,
                          PermissionType::STORAGE_ACCESS_GRANT),
            std::nullopt);

  overrides.Set(requesting_origin, embedding_origin_2,
                PermissionType::STORAGE_ACCESS_GRANT, PermissionStatus::ASK);
  EXPECT_EQ(overrides.Get(requesting_origin, embedding_origin_2,
                          PermissionType::STORAGE_ACCESS_GRANT),
            PermissionStatus::ASK);

  // Verify the first pair is still unaffected.
  EXPECT_EQ(overrides.Get(requesting_origin, embedding_origin_1,
                          PermissionType::STORAGE_ACCESS_GRANT),
            PermissionStatus::GRANTED);
}

TEST(PermissionOverridesTest, StorageAccess_SameRequestingAndEmbeddingSites) {
  PermissionOverrides overrides;
  Origin requesting_origin_1 =
      Origin::Create(GURL("https://foo.requesting.com/"));
  Origin requesting_origin_2 =
      Origin::Create(GURL("https://baz.requesting.com/"));
  Origin embedding_origin_1 =
      Origin::Create(GURL("https://bar.embedding.com/"));
  Origin embedding_origin_2 =
      Origin::Create(GURL("https://qux.embedding.com/"));

  overrides.Set(requesting_origin_1, embedding_origin_1,
                PermissionType::STORAGE_ACCESS_GRANT,
                PermissionStatus::GRANTED);
  EXPECT_EQ(overrides.Get(requesting_origin_1, embedding_origin_1,
                          PermissionType::STORAGE_ACCESS_GRANT),
            PermissionStatus::GRANTED);

  // Show that different origins with the same site return the correct status.
  // STORAGE_ACCESS_GRANT is keyed using schemeful sites, so
  // 'foo.requesting.com' and 'baz.requesting.com' resolve to the same
  // requesting site, and 'bar.embedding.com' and 'qux.embedding.com' resolve to
  // the same embedding site.
  EXPECT_EQ(overrides.Get(requesting_origin_2, embedding_origin_2,
                          PermissionType::STORAGE_ACCESS_GRANT),
            PermissionStatus::GRANTED);
}

TEST(PermissionOverridesTest,
     TopLevelStorageAccess_DifferentRequestingOriginSameEmbeddingSite) {
  PermissionOverrides overrides;
  Origin requesting_origin_1 =
      Origin::Create(GURL("https://foo.requesting.com/"));
  Origin requesting_origin_2 =
      Origin::Create(GURL("https://baz.requesting.com/"));
  Origin embedding_origin_1 =
      Origin::Create(GURL("https://bar.embedding.com/"));
  Origin embedding_origin_2 =
      Origin::Create(GURL("https://qux.embedding.com/"));

  overrides.Set(requesting_origin_1, embedding_origin_1,
                PermissionType::TOP_LEVEL_STORAGE_ACCESS,
                PermissionStatus::GRANTED);
  EXPECT_EQ(overrides.Get(requesting_origin_1, embedding_origin_1,
                          PermissionType::TOP_LEVEL_STORAGE_ACCESS),
            PermissionStatus::GRANTED);

  // Show that different embedding origins with the same site returns the
  // correct status.
  // TOP_LEVEL_STORAGE_ACCESS's embedding origin is keyed as a schemeful site,
  // so 'bar.embedding.com' and 'qux.embedding.com' resolve to the same
  // embedding site.
  EXPECT_EQ(overrides.Get(requesting_origin_1, embedding_origin_2,
                          PermissionType::TOP_LEVEL_STORAGE_ACCESS),
            PermissionStatus::GRANTED);

  // Show that a different requesting origin for the same embedding site should
  // not have the same key.
  EXPECT_FALSE(overrides.Get(requesting_origin_2, embedding_origin_1,
                             PermissionType::TOP_LEVEL_STORAGE_ACCESS));
}

}  // namespace
}  // namespace content
