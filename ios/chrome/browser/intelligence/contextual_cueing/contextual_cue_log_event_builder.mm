// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/contextual_cueing/contextual_cue_log_event_builder.h"

#import <string_view>
#import <utility>

#import "base/check.h"
#import "base/strings/utf_string_conversions.h"
#import "base/time/time.h"
#import "base/uuid.h"
#import "components/application_locale_storage/application_locale_storage.h"
#import "components/private_insights/contextual_cue_log_event_helpers.h"
#import "components/private_insights/events/coarse_system_profile.pb.h"
#import "components/variations/service/variations_service.h"
#import "components/version_info/version_info.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/common/channel_info.h"
#import "ios/web/public/web_state.h"

namespace contextual_cueing {

namespace {

// `cue_details.promoted_feature` for iOS cues, which always promote Gemini.
// Must match what desktop emits for Gemini cues
// (`GetName(CueTargetType::kGlic)` in chrome/browser/contextual_cueing) and the
// Private Insights pipeline's closed domain for this field.
constexpr std::string_view kPromotedFeature = "Glic";

// What `private_insights::SerializeCollectionToPageInfoJson` produces for an
// empty collection.
constexpr std::string_view kEmptyPageInfoJson = "[]";

void PopulateSystemProfile(
    private_insights::events::CoarseSystemProfile* system_profile) {
  private_insights::PopulateCommonSystemProfile(system_profile);
  system_profile->set_chrome_channel(
      version_info::GetChannelString(::GetChannel()));

  ApplicationContext* application_context = GetApplicationContext();
  if (!application_context) {
    return;
  }
  system_profile->set_language(
      application_context->GetApplicationLocaleStorage()->Get());
  if (variations::VariationsService* variations_service =
          application_context->GetVariationsService()) {
    system_profile->set_country(
        variations_service->GetStoredPermanentCountry());
  }
}

}  // namespace

ContextualCueSnapshot::ContextualCueSnapshot() = default;
ContextualCueSnapshot::ContextualCueSnapshot(const ContextualCueSnapshot&) =
    default;
ContextualCueSnapshot::ContextualCueSnapshot(ContextualCueSnapshot&&) = default;
ContextualCueSnapshot& ContextualCueSnapshot::operator=(
    const ContextualCueSnapshot&) = default;
ContextualCueSnapshot& ContextualCueSnapshot::operator=(
    ContextualCueSnapshot&&) = default;
ContextualCueSnapshot::~ContextualCueSnapshot() = default;

ContextualCueSnapshot CreateContextualCueSnapshot(
    web::WebState* web_state,
    const optimization_guide::proto::ContextualCue& cue,
    std::vector<optimization_guide::proto::Tab> background_tabs) {
  CHECK(web_state);
  ContextualCueSnapshot snapshot;
  snapshot.cue_id = base::Uuid::GenerateRandomV4().AsLowercaseString();
  snapshot.cue = cue;
  snapshot.active_page_url = web_state->GetLastCommittedURL();
  snapshot.active_page_title = base::UTF16ToUTF8(web_state->GetTitle());
  snapshot.background_tabs = std::move(background_tabs);
  return snapshot;
}

private_insights::events::ContextualCueLogEvent CreateContextualCueLogEvent(
    private_insights::events::ContextualCueLogEvent::EventType event_type,
    const ContextualCueSnapshot& snapshot) {
  private_insights::events::ContextualCueLogEvent event;
  event.set_cue_id(snapshot.cue_id);
  event.set_event_type(event_type);
  event.set_event_timestamp_ms(
      base::Time::Now().InMillisecondsSinceUnixEpoch());

  PopulateSystemProfile(event.mutable_system_profile());

  private_insights::events::ContextualCueLogEvent::CueContext* cue_context =
      event.mutable_cue_context();
  private_insights::PopulateActivePage(snapshot.active_page_url,
                                       snapshot.active_page_title,
                                       cue_context->mutable_active_page());
  cue_context->set_recent_pages(
      private_insights::SerializeCollectionToPageInfoJson(
          snapshot.background_tabs, private_insights::ExtractFromProtoTab));
  // iOS cues never point at other tabs, so `tabs_shown` is always an empty
  // JSON list. It is still set so the pipeline sees the same shape as desktop
  // events.
  cue_context->set_tabs_shown(kEmptyPageInfoJson);

  // iOS cues always promote Gemini. Like desktop, fall back to the promoted
  // feature when the server did not name a CUJ, so `cuj_type` is never empty.
  std::string_view cuj = snapshot.cue.suggested_cuj();
  if (cuj.empty()) {
    cuj = kPromotedFeature;
  }
  private_insights::PopulateCueDetails(snapshot.cue, cuj, kPromotedFeature,
                                       event.mutable_cue_details());

  return event;
}

}  // namespace contextual_cueing
