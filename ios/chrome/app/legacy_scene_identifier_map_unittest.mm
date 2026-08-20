// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/legacy_scene_identifier_map.h"

#import <UIKit/UIKit.h>

#import <functional>
#import <map>
#import <set>
#import <string>
#import <string_view>

#import "base/strings/strcat.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/model/profile/mutable_profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_ios.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

using ::base::Bucket;
using ::base::BucketsAre;

using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

namespace {

// Represents the context in which the test is run.
enum MultipleSceneSupport {
  kSupported,
  kUnsupported,
};

// Constants used by LegacySceneIdentifierMapTest.
inline constexpr std::string_view kProfile1 = "Profile1";
inline constexpr std::string_view kProfile2 = "Profile2";
inline constexpr std::string_view kSessionIdentifier = "SessionIdentifier";
inline constexpr std::string_view kSyntheticIdentifier =
    "{SyntheticIdentifier}";

// Creates a fake UISceneSession with a given persistentIdentifier.
UISceneSession* CreateFakeSceneSession(std::string_view identifier) {
  id fake_scene_session = OCMClassMock([UISceneSession class]);
  OCMStub([fake_scene_session persistentIdentifier])
      .andReturn(base::SysUTF8ToNSString(identifier));
  return fake_scene_session;
}

// Sub-class of TestingPrefServiceSimple registering the LocalState
// preferences in its constructor (for LegacySceneIdentifierMapTest).
class LocalStatePrefService : public TestingPrefServiceSimple {
 public:
  LocalStatePrefService() { RegisterLocalStatePrefs(registry()); }
};

}  // namespace

class LegacySceneIdentifierMapTest
    : public PlatformTest,
      public ::testing::WithParamInterface<MultipleSceneSupport> {
 public:
  LegacySceneIdentifierMapTest()
      : profile_attributes_storage_(&local_state_),
        app_state_([[AppState alloc] initWithStartupInformation:nil]) {
    profile_attributes_storage_.AddProfile(kProfile1);
    profile_attributes_storage_.AddProfile(kProfile2);
  }

  // Returns the AppState.
  AppState* app_state() { return app_state_; }

  // Returns the ProfileAttributesStorageIOS.
  ProfileAttributesStorageIOS* profile_attributes_storage() {
    return &profile_attributes_storage_;
  }

  // Returns whether the test should run as if the device supported multiple
  // scenes or not (as the LegacySceneIdentifierMap behaves differently).
  bool IsMultipleScenesSupported() {
    return GetParam() == MultipleSceneSupport::kSupported;
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
  AppState* app_state_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    LegacySceneIdentifierMapTest,
    ::testing::Values(MultipleSceneSupport::kSupported,
                      MultipleSceneSupport::kUnsupported),
    [](const ::testing::TestParamInfo<MultipleSceneSupport>& info) {
      return info.param == MultipleSceneSupport::kSupported ? "Tablet"
                                                            : "Phone";
    });

// Tests that LegacySceneIdentifierMap assigns an expected identifier to the
// SceneState in AssignIdentifierToSceneState() according to the capabilities
// of the device and record an histogram if the device supports multi windows.
TEST_P(LegacySceneIdentifierMapTest, AssignIdentifierToSceneState) {
  struct TestCase {
    const bool is_first_scene;
    const bool is_discarded;
  };

  static constexpr TestCase kTestCase[] = {
      {
          .is_first_scene = false,
          .is_discarded = false,
      },
      {
          .is_first_scene = false,
          .is_discarded = true,
      },
      {
          .is_first_scene = true,
          .is_discarded = false,
      },
      {
          .is_first_scene = true,
          .is_discarded = true,
      },
  };

  for (const TestCase& test_case : kTestCase) {
    UISceneSession* session = CreateFakeSceneSession(kSessionIdentifier);
    SceneState* scene_state = [[SceneState alloc] init];
    ASSERT_THAT(scene_state.sceneSessionID, Eq(""));

    LegacySceneIdentifierMap identifier_map = LegacySceneIdentifierMap(
        app_state(), profile_attributes_storage(), IsMultipleScenesSupported());

    // Mark the session as discarded before trying to assign its identifer.
    if (test_case.is_discarded) {
      identifier_map.OnSessionsDiscarded([NSSet setWithObject:session]);
    }

    base::HistogramTester histogram_tester;
    identifier_map.AssignIdentifierToSceneState(scene_state, session,
                                                test_case.is_first_scene);

    if (IsMultipleScenesSupported()) {
      // Check that if the device supports multiple scenes, then the session
      // identifier was used as the scene state identifier, and an histogram
      // was recorded to count whether the session was discarded before the
      // call.
      EXPECT_THAT(scene_state.sceneSessionID, Eq(kSessionIdentifier));
      EXPECT_THAT(histogram_tester.GetAllSamplesForPrefix("IOS.Sessions"),
                  UnorderedElementsAre(Pair(
                      "IOS.Sessions.DiscardedSceneConnectedAfterBeingPurged",
                      BucketsAre(Bucket(test_case.is_discarded, 1)))));
    } else {
      // Check that if the device does not support multiple scenes, then a
      // fixed identifier has been assigned as the scene state identifier,
      // and no histogram were recorded.
      EXPECT_THAT(scene_state.sceneSessionID, Eq(kSyntheticIdentifier));
      EXPECT_THAT(histogram_tester.GetAllSamplesForPrefix("IOS.Sessions"),
                  IsEmpty());
    }
  }
}

// Tests that LegacySceneIdentifierMap marks the session as discarded for
// all the profiles known to the ProfileAttributeStorageIOS and records an
// histogram if the device supports multi windows.
TEST_P(LegacySceneIdentifierMapTest, OnSessionsDiscarded) {
  struct TestCase {
    uint32_t sessions;
    bool (*connected)(uint32_t session);
  };

  static constexpr TestCase kTestCases[] = {
      {
          .sessions = 1,
          .connected = [](uint32_t session) { return false; },
      },
      {
          .sessions = 1,
          .connected = [](uint32_t session) { return true; },
      },
      {
          .sessions = 5,
          .connected = [](uint32_t session) { return session % 2 != 0; },
      },
  };

  for (const TestCase& test_case : kTestCases) {
    // If the device does not supports having multiple scenes, and the
    // test case has more than one session, skip it.
    if (!IsMultipleScenesSupported() && test_case.sessions > 1) {
      continue;
    }

    // The set of discarded session identifiers.
    std::set<std::string, std::less<>> discarded_session_identifiers;

    // Create the UISceneSession and SceneState objects.
    NSMutableSet<UISceneSession*>* sessions = [[NSMutableSet alloc] init];
    NSMutableSet<SceneState*>* scene_states = [[NSMutableSet alloc] init];
    for (uint32_t i = 0; i < test_case.sessions; ++i) {
      const std::string identifier =
          base::StrCat({kSessionIdentifier, base::NumberToString(i)});
      discarded_session_identifiers.insert(identifier);

      // Create the session and add it to the set of discarded sessions.
      UISceneSession* session = CreateFakeSceneSession(identifier);
      [sessions addObject:session];

      // If the sessions is supposed to be connected, create a SceneState
      // object with the same identifier and connect it with the AppState.
      if (test_case.connected(i)) {
        SceneState* scene_state = [[SceneState alloc] init];
        scene_state.sceneSessionID =
            IsMultipleScenesSupported() ? identifier : kSyntheticIdentifier;

        [app_state() sceneStateConnected:scene_state];
        [scene_states addObject:scene_state];
      }
    }

    LegacySceneIdentifierMap identifier_map = LegacySceneIdentifierMap(
        app_state(), profile_attributes_storage(), IsMultipleScenesSupported());

    base::HistogramTester histogram_tester;
    identifier_map.OnSessionsDiscarded(sessions);

    if (IsMultipleScenesSupported()) {
      // Check that if the device supports multiple scenes, then the
      // identifiers of the discarded sessions were registered with
      // all profiles and the histogram was updated with a count of
      // still connected scenes.
      EXPECT_THAT(GetDiscardedSessionIdentifiersPerProfileName(),
                  UnorderedElementsAre(
                      Pair(kProfile1, Eq(discarded_session_identifiers)),
                      Pair(kProfile2, Eq(discarded_session_identifiers))));
      EXPECT_THAT(histogram_tester.GetAllSamplesForPrefix("IOS.Sessions"),
                  UnorderedElementsAre(
                      Pair("IOS.Sessions.DiscardedScenesStillConnectedCount",
                           BucketsAre(Bucket(scene_states.count, 1)))));
    } else {
      // Check that nothing is recorded if the device does not support
      // multiple scenes.
      EXPECT_THAT(GetDiscardedSessionIdentifiersPerProfileName(), IsEmpty());
      EXPECT_THAT(histogram_tester.GetAllSamplesForPrefix("IOS.Sessions"),
                  IsEmpty());
    }

    // Clear the list of connected SceneState with the AppState.
    for (SceneState* scene_state in scene_states) {
      [app_state() sceneStateDisconnected:scene_state];
    }
  }
}

// Tests that OnLastSceneStateDisconnected() can be called and do nothing.
TEST_P(LegacySceneIdentifierMapTest, OnLastSceneStateDisconnected) {
  SceneState* scene_state = [[SceneState alloc] init];
  scene_state.sceneSessionID = kSessionIdentifier;

  LegacySceneIdentifierMap identifier_map = LegacySceneIdentifierMap(
      app_state(), profile_attributes_storage(), IsMultipleScenesSupported());

  base::HistogramTester histogram_tester;
  identifier_map.OnLastSceneStateDisconnected(scene_state);

  // Check that the SceneState identifier has not changed, and that no
  // histograms were recorded.
  EXPECT_THAT(scene_state.sceneSessionID, Eq(kSessionIdentifier));
  EXPECT_THAT(histogram_tester.GetAllSamplesForPrefix("IOS.Sessions"),
              IsEmpty());
}
