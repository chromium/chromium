// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/contextual_cueing/contextual_cue_log_event_builder.h"

#import <string>
#import <vector>

#import "base/uuid.h"
#import "components/application_locale_storage/application_locale_storage.h"
#import "components/optimization_guide/proto/features/contextual_cueing.pb.h"
#import "components/private_insights/events/coarse_system_profile.pb.h"
#import "components/private_insights/events/contextual_cue_log_event.pb.h"
#import "components/version_info/version_info.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/common/channel_info.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace contextual_cueing {

namespace {

using private_insights::events::CoarseSystemProfile;
using private_insights::events::ContextualCueLogEvent;

constexpr char kActiveUrl[] = "https://www.example.com/page?q=1";
constexpr char kActiveHost[] = "www.example.com";
constexpr char16_t kActiveTitle[] = u"Example page";
constexpr char kBackgroundUrl[] = "https://other.example/";
constexpr char kBackgroundTitle[] = "Other";
constexpr char kSuggestedCuj[] = "SHOPPING_COMPARE";
constexpr char kAnchoredMessageText[] = "Compare prices";
constexpr char kActionText[] = "Compare";
constexpr char kPrompt[] = "Compare these products";
constexpr char kExpectedPromotedFeature[] = "Glic";

// Builds a cue the way `CreateTestCueResponse()` does in the tab helper tests.
optimization_guide::proto::ContextualCue CreateTestCue(
    const std::string& suggested_cuj = kSuggestedCuj) {
  optimization_guide::proto::ContextualCue cue;
  cue.set_suggested_cuj(suggested_cuj);
  cue.mutable_gemini_in_chrome_surface()->set_prompt(kPrompt);
  cue.mutable_anchored_message_cue()->set_anchored_message_text(
      kAnchoredMessageText);
  cue.mutable_anchored_message_cue()->set_action_text(kActionText);
  return cue;
}

optimization_guide::proto::Tab CreateBackgroundTab() {
  optimization_guide::proto::Tab tab;
  tab.set_url(kBackgroundUrl);
  tab.set_title(kBackgroundTitle);
  return tab;
}

}  // namespace

class ContextualCueLogEventBuilderTest : public PlatformTest {
 protected:
  ContextualCueLogEventBuilderTest() {
    web_state_.SetCurrentURL(GURL(kActiveUrl));
    web_state_.SetTitle(kActiveTitle);
  }

  ContextualCueSnapshot CreateSnapshot(
      std::vector<optimization_guide::proto::Tab> background_tabs = {}) {
    return CreateContextualCueSnapshot(&web_state_, CreateTestCue(),
                                       std::move(background_tabs));
  }

  web::WebTaskEnvironment task_environment_;
  web::FakeWebState web_state_;
};

// Test that a snapshot captures the WebState's last committed page, the cue
// and the background tabs, under a fresh UUID `cue_id`.
TEST_F(ContextualCueLogEventBuilderTest, SnapshotCapturesPageAndCue) {
  ContextualCueSnapshot snapshot = CreateSnapshot({CreateBackgroundTab()});

  EXPECT_TRUE(base::Uuid::ParseLowercase(snapshot.cue_id).is_valid());
  EXPECT_EQ(GURL(kActiveUrl), snapshot.active_page_url);
  EXPECT_EQ("Example page", snapshot.active_page_title);
  EXPECT_EQ(kSuggestedCuj, snapshot.cue.suggested_cuj());
  ASSERT_EQ(1u, snapshot.background_tabs.size());
  EXPECT_EQ(kBackgroundUrl, snapshot.background_tabs[0].url());
}

// Test that each snapshot gets its own `cue_id`.
TEST_F(ContextualCueLogEventBuilderTest, SnapshotsHaveDistinctCueIds) {
  EXPECT_NE(CreateSnapshot().cue_id, CreateSnapshot().cue_id);
}

// Test that a snapshot outlives changes to the WebState: the event is built
// from the page captured at snapshot time, not from the live WebState.
TEST_F(ContextualCueLogEventBuilderTest, SnapshotIsDecoupledFromWebState) {
  ContextualCueSnapshot snapshot = CreateSnapshot();
  web_state_.SetCurrentURL(GURL("https://elsewhere.example/"));
  web_state_.SetTitle(u"Elsewhere");

  ContextualCueLogEvent event =
      CreateContextualCueLogEvent(ContextualCueLogEvent::CLICKED, snapshot);

  EXPECT_EQ(kActiveUrl, event.cue_context().active_page().url());
  EXPECT_EQ("Example page", event.cue_context().active_page().title());
}

// Test the event header and `cue_context` for a SHOWN event.
TEST_F(ContextualCueLogEventBuilderTest, ShownEventHeaderAndContext) {
  ContextualCueSnapshot snapshot = CreateSnapshot({CreateBackgroundTab()});

  ContextualCueLogEvent event =
      CreateContextualCueLogEvent(ContextualCueLogEvent::SHOWN, snapshot);

  EXPECT_EQ(snapshot.cue_id, event.cue_id());
  EXPECT_EQ(ContextualCueLogEvent::SHOWN, event.event_type());
  EXPECT_GT(event.event_timestamp_ms(), 0);

  EXPECT_EQ(kActiveUrl, event.cue_context().active_page().url());
  EXPECT_EQ("Example page", event.cue_context().active_page().title());
  EXPECT_EQ(kActiveHost, event.cue_context().active_page().hostname());
  EXPECT_EQ("[{\"title\":\"Other\",\"url\":\"https://other.example/\"}]",
            event.cue_context().recent_pages());
  EXPECT_EQ("[]", event.cue_context().tabs_shown());
}

// Test that a CLICKED event built from the same snapshot as the SHOWN event
// shares its `cue_id`.
TEST_F(ContextualCueLogEventBuilderTest, ClickedEventSharesCueIdWithShown) {
  ContextualCueSnapshot snapshot = CreateSnapshot();

  ContextualCueLogEvent shown =
      CreateContextualCueLogEvent(ContextualCueLogEvent::SHOWN, snapshot);
  ContextualCueLogEvent clicked =
      CreateContextualCueLogEvent(ContextualCueLogEvent::CLICKED, snapshot);

  EXPECT_EQ(ContextualCueLogEvent::CLICKED, clicked.event_type());
  EXPECT_GT(clicked.event_timestamp_ms(), 0);
  EXPECT_EQ(shown.cue_id(), clicked.cue_id());
}

// Test that no background tabs serialize to an empty JSON list.
TEST_F(ContextualCueLogEventBuilderTest, NoBackgroundTabs) {
  ContextualCueLogEvent event = CreateContextualCueLogEvent(
      ContextualCueLogEvent::SHOWN, CreateSnapshot());

  EXPECT_EQ("[]", event.cue_context().recent_pages());
}

// Test the mapping from the cue proto to `cue_details`, which mirrors desktop
// `internal::CreateContextualCueLogEvent`.
TEST_F(ContextualCueLogEventBuilderTest, CueDetails) {
  ContextualCueLogEvent event = CreateContextualCueLogEvent(
      ContextualCueLogEvent::SHOWN, CreateSnapshot());

  EXPECT_EQ(kSuggestedCuj, event.cue_details().cuj_type());
  EXPECT_EQ(kAnchoredMessageText, event.cue_details().suggestion_text());
  EXPECT_EQ(kActionText, event.cue_details().button_text());
  EXPECT_EQ(kPrompt, event.cue_details().prompt());
  EXPECT_EQ(kExpectedPromotedFeature, event.cue_details().promoted_feature());
}

// Test that `cuj_type` falls back to the promoted feature when the server did
// not name a CUJ.
TEST_F(ContextualCueLogEventBuilderTest, CujFallsBackToPromotedFeature) {
  ContextualCueSnapshot snapshot = CreateContextualCueSnapshot(
      &web_state_, CreateTestCue(/*suggested_cuj=*/""), {});

  ContextualCueLogEvent event =
      CreateContextualCueLogEvent(ContextualCueLogEvent::SHOWN, snapshot);

  EXPECT_EQ(kExpectedPromotedFeature, event.cue_details().cuj_type());
}

// Test that a cue without a Gemini surface leaves `prompt` empty.
TEST_F(ContextualCueLogEventBuilderTest, NoPromptWithoutGeminiSurface) {
  optimization_guide::proto::ContextualCue cue = CreateTestCue();
  cue.clear_gemini_in_chrome_surface();
  ContextualCueSnapshot snapshot =
      CreateContextualCueSnapshot(&web_state_, cue, {});

  ContextualCueLogEvent event =
      CreateContextualCueLogEvent(ContextualCueLogEvent::SHOWN, snapshot);

  EXPECT_TRUE(event.cue_details().prompt().empty());
}

// Test the iOS `system_profile`: version, milestone, channel, platform "iOS"
// and the application locale. The `TestingApplicationContext` has no
// `VariationsService`, so the country is left empty.
TEST_F(ContextualCueLogEventBuilderTest, SystemProfile) {
  ContextualCueLogEvent event = CreateContextualCueLogEvent(
      ContextualCueLogEvent::SHOWN, CreateSnapshot());

  const CoarseSystemProfile& system_profile = event.system_profile();
  // `UIDevice.systemName` reports "iPadOS" on iPad.
  EXPECT_THAT(system_profile.platform(),
              testing::AnyOf(testing::Eq("iOS"), testing::Eq("iPadOS")));
  EXPECT_EQ(version_info::GetVersionNumber(), system_profile.chrome_version());
  EXPECT_EQ(version_info::GetMajorVersionNumber(), system_profile.milestone());
  EXPECT_FALSE(system_profile.chrome_version().empty());
  EXPECT_FALSE(system_profile.milestone().empty());
  EXPECT_EQ(version_info::GetChannelString(::GetChannel()),
            system_profile.chrome_channel());
  EXPECT_EQ(GetApplicationContext()->GetApplicationLocaleStorage()->Get(),
            system_profile.language());
  EXPECT_FALSE(system_profile.language().empty());
  EXPECT_TRUE(system_profile.country().empty());
}

}  // namespace contextual_cueing
