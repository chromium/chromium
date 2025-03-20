// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/model/reader_mode_tab_helper.h"

#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/reader_mode/model/features.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_java_script_feature.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/navigation/navigation_context.h"

ReaderModeTabHelper::ReaderModeTabHelper(web::WebState* web_state)
    : web_state_(web_state) {
  web_state_->AddObserver(this);
}

ReaderModeTabHelper::~ReaderModeTabHelper() = default;

void ReaderModeTabHelper::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  CHECK_EQ(web_state, web_state_);
  if (load_completion_status == web::PageLoadCompletionStatus::SUCCESS) {
    // Guarantee that there is only one trigger heuristic running at a time.
    if (trigger_reader_mode_timer_.IsRunning()) {
      trigger_reader_mode_timer_.Stop();
    }
    trigger_reader_mode_timer_.Start(
        FROM_HERE, ReaderModeDistillerPageLoadDelay(),
        base::BindOnce(&ReaderModeTabHelper::TriggerReaderModeHeuristic,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void ReaderModeTabHelper::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (navigation_context->IsSameDocument()) {
    return;
  }
  // A new navigation is started while the Reader Mode heuristic trigger is
  // running on the previous navigation. Stop the trigger to attach the new
  // navigation.
  if (trigger_reader_mode_timer_.IsRunning()) {
    trigger_reader_mode_timer_.Stop();
  }
}

void ReaderModeTabHelper::WebStateDestroyed(web::WebState* web_state) {
  CHECK_EQ(web_state_, web_state);
  web_state_->RemoveObserver(this);
  web_state_ = nullptr;
}

bool ReaderModeTabHelper::CanTriggerReaderModeHeuristic() {
  if (!base::FeatureList::IsEnabled(kEnableReaderModeDistillerHeuristic)) {
    return false;
  }
  const double page_load_probability =
      kReaderModeDistillerPageLoadProbability.Get();
  if (page_load_probability <= 0.0 || page_load_probability > 1.0) {
    // Invalid probability range. Disable the Reader Mode feature.
    return false;
  }

  const double rand_double = base::RandDouble();
  return rand_double < page_load_probability;
}

void ReaderModeTabHelper::TriggerReaderModeHeuristic() {
  if (!CanTriggerReaderModeHeuristic()) {
    return;
  }
  web::WebFramesManager* web_frames_manager =
      ReaderModeJavaScriptFeature::GetInstance()->GetWebFramesManager(
          web_state_);
  web::WebFrame* web_frame = web_frames_manager->GetMainWebFrame();
  if (!web_frame) {
    return;
  }
  ReaderModeJavaScriptFeature::GetInstance()->TriggerReaderModeHeuristic(
      web_frame);
}

WEB_STATE_USER_DATA_KEY_IMPL(ReaderModeTabHelper)
