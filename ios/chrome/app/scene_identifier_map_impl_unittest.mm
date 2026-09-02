// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/scene_identifier_map_impl.h"

#import <UIKit/UIKit.h>

#import <array>
#import <string_view>

#import "base/functional/bind.h"
#import "base/strings/strcat.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/mutable_profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_ios.h"
#import "ios/chrome/browser/window_activities/model/window_activity_helpers.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::IsSupersetOf;
using ::testing::Ne;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

namespace {

// Represents the context in which the test is run.
enum MultipleSceneSupport {
  kSupported,
  kUnsupported,
};

// Creates a fake UISceneSession with a given persistentIdentifier.
UISceneSession* CreateFakeSceneSession(std::string_view identifier) {
  id fake_scene_session = OCMClassMock([UISceneSession class]);
  OCMStub([fake_scene_session persistentIdentifier])
      .andReturn(base::SysUTF8ToNSString(identifier));
  return fake_scene_session;
}

// Creates a SceneState with a given sceneSessionID.
SceneState* CreateSceneState(std::string_view identifier) {
  SceneState* scene_state = [[SceneState alloc] init];
  scene_state.sceneSessionID = identifier;
  return scene_state;
}

// Adds boolean prefs to `attrs` for all of `sessions`.
void AddSessionScopedPrefs(std::initializer_list<std::string_view> sessions,
                           ProfileAttributesIOS& attrs) {
  for (std::string_view session : sessions) {
    attrs.SetSessionScopedBoolPref(session, "pref", true);
  }
}

// Constants used for tests.
inline constexpr std::string_view kProfile0 = "Profile0";
inline constexpr std::string_view kProfile1 = "Profile1";

inline constexpr std::string_view kSystemId0 = "SystemId0";
inline constexpr std::string_view kSystemId1 = "SystemId1";
inline constexpr std::string_view kSystemId2 = "SystemId2";

inline constexpr std::string_view kSavedId = "SavedIdentifier";

inline constexpr std::string_view kSceneState0 = "SceneState0";
inline constexpr std::string_view kSceneState1 = "SceneState1";
inline constexpr std::string_view kSceneState2 = "SceneState2";

inline constexpr std::string_view kSyntheticIdentifier =
    "{SyntheticIdentifier}";

// Sub-class of TestingPrefServiceSimple registering the LocalState
// preferences in its constructor (for SceneIdentifierMapImplTest).
class LocalStatePrefService : public TestingPrefServiceSimple {
 public:
  LocalStatePrefService() { RegisterLocalStatePrefs(registry()); }
};

}  // namespace

class SceneIdentifierMapImplTest
    : public PlatformTest,
      public ::testing::WithParamInterface<MultipleSceneSupport> {
 public:
  SceneIdentifierMapImplTest() : profile_attributes_storage_(&local_state_) {}

  // Returns the local state PrefService.
  PrefService* local_state() { return &local_state_; }

  // Returns the ProfileAttributesStorageIOS.
  ProfileAttributesStorageIOS* profile_attributes_storage() {
    return &profile_attributes_storage_;
  }

  // Returns whether the test should run as if the device supported multiple
  // scenes or not (as the SceneIdentifierMapImpl behaves differently).
  bool IsMultipleScenesSupported() {
    return GetParam() == MultipleSceneSupport::kSupported;
  }

  // Returns the value of the kLastConnectedSceneIdentifier preference.
  const std::string& LastConnectedSceneIdentifier() const {
    return local_state_.GetString(prefs::kLastConnectedSceneIdentifier);
  }

  // Returns the value of the kSceneSessionIdentifierMap preference.
  const base::DictValue& SceneSessionIdentifierMap() const {
    return local_state_.GetDict(prefs::kSceneSessionIdentifierMap);
  }

  // Sets the value of the kLastConnectedSceneIdentifier preference.
  void SetLastConnectedSceneIdentifier(std::string_view value) {
    local_state_.SetString(prefs::kLastConnectedSceneIdentifier, value);
  }

  // Sets the value of the kSceneSessionIdentifierMap preference.
  void SetSceneSessionIdentifierMap(base::DictValue value) {
    local_state_.SetDict(prefs::kSceneSessionIdentifierMap, std::move(value));
  }

  // Sets up the MutableProfileAttributesStorageIOS so that it knows about
  // a profile named `profile_name` with session scoped preferences for
  // `sessions`.
  void CreateProfile(std::string_view profile_name,
                     std::initializer_list<std::string_view> sessions) {
    profile_attributes_storage_.AddProfile(profile_name);
    profile_attributes_storage_.UpdateAttributesForProfileWithName(
        profile_name,
        base::BindOnce(&AddSessionScopedPrefs, std::move(sessions)));
  }

  // Mapping of discarded sessions per profile.
  using DiscardedSessionMap =
      std::map<std::string, std::set<std::string, std::less<>>>;

  // Returns the set of discarded session identifiers per profile and
  // clears the information from the ProfileAttributesStorageIOS.
  DiscardedSessionMap GetDiscardedSessionIdentifiersPerProfileName() {
    DiscardedSessionMap result;
    profile_attributes_storage_.IterateOverProfileAttributes(
        base::BindRepeating(
            [](DiscardedSessionMap& result, ProfileAttributesIOS& attrs) {
              const auto& sessions = attrs.GetDiscardedSessions();
              if (!sessions.empty()) {
                result.insert(std::make_pair(attrs.GetProfileName(), sessions));
              }
              attrs.SetDiscardedSessions({});
            },
            std::ref(result)));
    return result;
  }

 private:
  LocalStatePrefService local_state_;
  MutableProfileAttributesStorageIOS profile_attributes_storage_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    SceneIdentifierMapImplTest,
    ::testing::Values(MultipleSceneSupport::kSupported,
                      MultipleSceneSupport::kUnsupported),
    [](const ::testing::TestParamInfo<MultipleSceneSupport>& info) {
      return info.param == MultipleSceneSupport::kSupported ? "Tablet"
                                                            : "Phone";
    });

// Tests that SceneIdentifierMapImpl correctly assigns an identifier to the
// SceneState and sets the -currentOrigin property if the session is known
// (or restored from the last closed window).
TEST_P(SceneIdentifierMapImplTest, AssignIdentifierToSceneState) {
  struct TestCase {
    // Setup
    const bool is_first_scene;
    const uint32_t saved_mapping_count;
    const std::string_view saved_last_scene_id;
    const std::string_view session_id;

    // Expectations
    const std::string_view expected_id;
  };

  // Tablet
  static constexpr TestCase kTabletTestCases[] = {
      {
          // Setup
          .is_first_scene = false,
          .saved_mapping_count = 0,
          .saved_last_scene_id = "",
          .session_id = kSystemId0,

          // Expectations
          .expected_id = kSystemId0,
      },

      {
          // Setup
          .is_first_scene = true,
          .saved_mapping_count = 0,
          .saved_last_scene_id = "",
          .session_id = kSystemId0,

          // Expectations
          .expected_id = kSystemId0,
      },

      {
          // Setup
          .is_first_scene = false,
          .saved_mapping_count = 0,
          .saved_last_scene_id = kSavedId,
          .session_id = kSystemId0,

          // Expectations
          .expected_id = kSystemId0,
      },

      {
          // Setup
          .is_first_scene = true,
          .saved_mapping_count = 0,
          .saved_last_scene_id = kSavedId,
          .session_id = kSystemId0,

          // Expectations
          .expected_id = kSavedId,
      },

      {
          // Setup
          .is_first_scene = false,
          .saved_mapping_count = 1,
          .saved_last_scene_id = kSavedId,
          .session_id = kSystemId0,

          // Expectations
          .expected_id = kSceneState0,
      },

      {
          // Setup
          .is_first_scene = true,
          .saved_mapping_count = 1,
          .saved_last_scene_id = kSavedId,
          .session_id = kSystemId0,

          // Expectations
          .expected_id = kSceneState0,
      },

      {
          // Setup
          .is_first_scene = false,
          .saved_mapping_count = 2,
          .saved_last_scene_id = kSavedId,
          .session_id = kSystemId0,

          // Expectations
          .expected_id = kSceneState0,
      },

      {
          // Setup
          .is_first_scene = true,
          .saved_mapping_count = 2,
          .saved_last_scene_id = kSavedId,
          .session_id = kSystemId0,

          // Expectations
          .expected_id = kSceneState0,
      },

      {
          // Setup
          .is_first_scene = false,
          .saved_mapping_count = 2,
          .saved_last_scene_id = kSavedId,
          .session_id = kSystemId1,

          // Expectations
          .expected_id = kSceneState1,
      },

      {
          // Setup
          .is_first_scene = true,
          .saved_mapping_count = 2,
          .saved_last_scene_id = kSavedId,
          .session_id = kSystemId1,

          // Expectations
          .expected_id = kSceneState1,
      },

      {
          // Setup
          .is_first_scene = false,
          .saved_mapping_count = 2,
          .saved_last_scene_id = kSavedId,
          .session_id = kSceneState0,

          // Expectations
          .expected_id = kSceneState2,
      },

      {
          // Setup
          .is_first_scene = true,
          .saved_mapping_count = 2,
          .saved_last_scene_id = kSavedId,
          .session_id = kSceneState0,

          // Expectations
          .expected_id = kSavedId,
      },
  };

  // Phone
  static constexpr TestCase kPhoneTestCases[] = {
      {
          // Setup
          .is_first_scene = false,
          .saved_mapping_count = 0,
          .saved_last_scene_id = "",
          .session_id = kSystemId0,

          // Expectations
          .expected_id = kSyntheticIdentifier,
      },

      {
          // Setup
          .is_first_scene = true,
          .saved_mapping_count = 0,
          .saved_last_scene_id = "",
          .session_id = kSystemId0,

          // Expectations
          .expected_id = kSyntheticIdentifier,
      },

      {
          // Setup
          .is_first_scene = false,
          .saved_mapping_count = 0,
          .saved_last_scene_id = kSavedId,
          .session_id = kSystemId0,

          // Expectations
          .expected_id = kSyntheticIdentifier,
      },

      {
          // Setup
          .is_first_scene = true,
          .saved_mapping_count = 0,
          .saved_last_scene_id = kSavedId,
          .session_id = kSystemId0,

          // Expectations
          .expected_id = kSavedId,
      },

      {
          // Setup
          .is_first_scene = false,
          .saved_mapping_count = 1,
          .saved_last_scene_id = "",
          .session_id = kSystemId0,

          // Expectations
          .expected_id = kSceneState0,
      },

      {
          // Setup
          .is_first_scene = true,
          .saved_mapping_count = 1,
          .saved_last_scene_id = "",
          .session_id = kSystemId0,

          // Expectations
          .expected_id = kSceneState0,
      },
  };

  // Tablet and Phone have different expectations length, so use a lambda
  // to initialize the list of expectations.
  const base::span<const TestCase> kTestCases =
      ([&]() -> base::span<const TestCase> {
        if (IsMultipleScenesSupported()) {
          return base::span(kTabletTestCases);
        }
        return base::span(kPhoneTestCases);
      })();

  for (const TestCase& test_case : kTestCases) {
    // Configure the preferences according to the test case.
    base::DictValue identifier_mapping;
    for (uint32_t i = 0; i < test_case.saved_mapping_count; ++i) {
      identifier_mapping.Set(
          base::StrCat({"SystemId", base::NumberToString(i)}),
          base::Value(base::StrCat({"SceneState", base::NumberToString(i)})));
    }
    SetSceneSessionIdentifierMap(std::move(identifier_mapping));
    SetLastConnectedSceneIdentifier(test_case.saved_last_scene_id);

    // Create a UISceneSession and a SceneState used for the test.
    UISceneSession* session = CreateFakeSceneSession(test_case.session_id);
    SceneState* scene_state = [[SceneState alloc] init];
    EXPECT_THAT(scene_state.sceneSessionID, Eq(""));
    EXPECT_THAT(scene_state.currentOrigin, Ne(WindowActivityRestoredOrigin));

    // Create the SystemIdentifierMapImpl and request it to assign an
    // identifer to the SceneState, then check the expectations.
    SceneIdentifierMapImpl identifier_map =
        SceneIdentifierMapImpl(local_state(), profile_attributes_storage(),
                               IsMultipleScenesSupported());
    identifier_map.AssignIdentifierToSceneState(scene_state, session,
                                                test_case.is_first_scene);

    EXPECT_THAT(scene_state.sceneSessionID, test_case.expected_id);
    EXPECT_THAT(
        SceneSessionIdentifierMap(),
        IsSupersetOf({Pair(test_case.session_id, test_case.expected_id)}));
  }
}

// Tests that SceneIdentifierMapImpl updates its mapping and informs the
// ProfileAttributesStorageIOS that the sessions have been discarded.
TEST_P(SceneIdentifierMapImplTest, OnSessionsDiscarded) {
  // Configure pre-existing profiles.
  CreateProfile(kProfile0, {});
  CreateProfile(kProfile1, {});

  // Configure pre-existing preferences.
  SetLastConnectedSceneIdentifier(kSceneState0);
  SetSceneSessionIdentifierMap(base::DictValue()
                                   .Set(kSystemId0, kSceneState0)
                                   .Set(kSystemId1, kSceneState1)
                                   .Set(kSystemId2, kSceneState2));

  // Check that SceneIdentifierMapImpl correctly update the preferences
  // after calling OnSessionsDiscarded(...).
  SceneIdentifierMapImpl identifier_map = SceneIdentifierMapImpl(
      local_state(), profile_attributes_storage(), IsMultipleScenesSupported());
  identifier_map.OnSessionsDiscarded(
      [NSSet setWithObjects:CreateFakeSceneSession(kSystemId0),
                            CreateFakeSceneSession(kSystemId2), nil]);

  EXPECT_THAT(LastConnectedSceneIdentifier(), Eq(kSceneState0));
  EXPECT_THAT(SceneSessionIdentifierMap(),
              UnorderedElementsAre(Pair(kSystemId1, kSceneState1)));

  EXPECT_THAT(
      GetDiscardedSessionIdentifiersPerProfileName(),
      UnorderedElementsAre(
          Pair(kProfile0, UnorderedElementsAre(kSceneState0, kSceneState2)),
          Pair(kProfile1, UnorderedElementsAre(kSceneState0, kSceneState2))));
}

// Tests that SceneIdentifierMapImpl migrates the list of session identifiers
// and the identifier of the last connected SceneState from the preferences
// saved in the ProfileAttributesStorageIOS if its own preferences are missing.
TEST_P(SceneIdentifierMapImplTest, RestoreMappingFromHistoricalPrefs) {
  // Mappings managed by ProfileAttributesStorageIOS.
  CreateProfile(kProfile0, {kSystemId0, kSystemId2});
  CreateProfile(kProfile1, {kSystemId0});

  // Mappings managed by MainController.
  local_state()->SetDict(
      prefs::kProfileForScene,
      base::DictValue().Set(kSystemId0, kProfile1).Set(kSystemId1, kProfile1));

  // Create the SceneIdentifierMapImpl and check that the preferences are
  // migrated. The object is immediately destroyed since it is not needed
  // anymore after its constructor has been executed.
  std::ignore = SceneIdentifierMapImpl(
      local_state(), profile_attributes_storage(), IsMultipleScenesSupported());

  if (IsMultipleScenesSupported()) {
    // The mapping should be initialized with the three identifiers pair (where
    // key and value are equal since this is how the historical mapping worked).
    EXPECT_THAT(SceneSessionIdentifierMap(),
                UnorderedElementsAre(Pair(kSystemId0, kSystemId0),
                                     Pair(kSystemId1, kSystemId1),
                                     Pair(kSystemId2, kSystemId2)));
  } else {
    // The mapping should not be initialized as the legacy mapping used a
    // constant identifier on those devices and recovering the identifier
    // of UISceneSession is not possible.
    EXPECT_THAT(SceneSessionIdentifierMap(), IsEmpty());
  }

  // As there is more than one scene identifier, the identifier of the last
  // connected scene should be left empty (it is not possible to decide the
  // value that should be used).
  EXPECT_THAT(LastConnectedSceneIdentifier(), IsEmpty());
}

// Tests that SceneIdentifierMapImpl migrates the list of session identifiers
// and the identifier of the last connected SceneState from the preferences
// saved in the ProfileAttributesStorageIOS if its own preferences are missing.
//
// Case where there is a single session (in that case the identifier of the
// last connected scene can be determined and the corresponding preference
// initialized).
TEST_P(SceneIdentifierMapImplTest,
       RestoreMappingFromHistoricalPrefs_OneSession) {
  // Mappings managed by ProfileAttributesStorageIOS.
  CreateProfile(kProfile0, {kSystemId0});
  CreateProfile(kProfile1, {kSystemId0});

  // Mappings managed by MainController.
  local_state()->SetDict(prefs::kProfileForScene,
                         base::DictValue().Set(kSystemId0, kProfile1));

  // Create the SceneIdentifierMapImpl and check that the preferences are
  // migrated. The object is immediately destroyed since it is not needed
  // anymore after its constructor has been executed.
  std::ignore = SceneIdentifierMapImpl(
      local_state(), profile_attributes_storage(), IsMultipleScenesSupported());

  if (IsMultipleScenesSupported()) {
    // The mapping should be initialized with one identifier pair (where key
    // and value are equal since this is how the historical mapping worked).
    EXPECT_THAT(SceneSessionIdentifierMap(),
                UnorderedElementsAre(Pair(kSystemId0, kSystemId0)));
  } else {
    // The mapping should not be initialized as the legacy mapping used a
    // constant identifier on those devices and recovering the identifier
    // of UISceneSession is not possible.
    EXPECT_THAT(SceneSessionIdentifierMap(), IsEmpty());
  }

  // As there is exactly one scene identifier, the identifier of the last
  // connected scene should be initialized to that identifier.
  EXPECT_THAT(LastConnectedSceneIdentifier(), kSystemId0);
}

// Tests that SceneIdentifierMapImpl migrates the list of session identifiers
// and the identifier of the last connected SceneState from the preferences
// saved in the ProfileAttributesStorageIOS if its own preferences are missing.
//
// Case where there are no known profiles (e.g. brand new install).
TEST_P(SceneIdentifierMapImplTest,
       RestoreMappingFromHistoricalPrefs_NewInstall) {
  // Create the SceneIdentifierMapImpl and check that the preferences are
  // migrated. The object is immediately destroyed since it is not needed
  // anymore after its constructor has been executed.
  std::ignore = SceneIdentifierMapImpl(
      local_state(), profile_attributes_storage(), IsMultipleScenesSupported());

  // No data to load, so the mapping and identifier of the last connected
  // scene should both be left untouched.
  EXPECT_THAT(SceneSessionIdentifierMap(), UnorderedElementsAre());
  EXPECT_THAT(LastConnectedSceneIdentifier(), IsEmpty());
}

// Tests that SceneIdentifierMapImpl saves the identifier of the last SceneState
// when OnLastSceneStateDisconnected(...) is called.
TEST_P(SceneIdentifierMapImplTest, OnLastSceneStateDisconnected) {
  SceneIdentifierMapImpl identifier_map = SceneIdentifierMapImpl(
      local_state(), profile_attributes_storage(), IsMultipleScenesSupported());

  ASSERT_THAT(LastConnectedSceneIdentifier(), IsEmpty());

  // Verify that the preference is updated each time the method is invoked.
  identifier_map.OnLastSceneStateDisconnected(CreateSceneState(kSystemId0));
  EXPECT_THAT(LastConnectedSceneIdentifier(), kSystemId0);

  identifier_map.OnLastSceneStateDisconnected(CreateSceneState(kSystemId1));
  EXPECT_THAT(LastConnectedSceneIdentifier(), kSystemId1);

  identifier_map.OnLastSceneStateDisconnected(CreateSceneState(kSystemId1));
  EXPECT_THAT(LastConnectedSceneIdentifier(), kSystemId1);
}
