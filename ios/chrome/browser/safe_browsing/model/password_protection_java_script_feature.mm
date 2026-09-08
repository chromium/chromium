// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/password_protection_java_script_feature.h"

#import "base/check.h"
#import "base/ios/ios_util.h"
#import "base/no_destructor.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversion_utils.h"
#import "ios/chrome/browser/safe_browsing/model/input_event_observer.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/js_messaging/script_message.h"

namespace {
const char kScriptFilename[] = "password_protection";

const char kTextEnteredHandlerName[] = "PasswordProtectionTextEntered";

// Values for the "eventType" field in messages received by this feature's
// script message handler.
const char kKeyDownEventType[] = "KeyDown";
const char kPasteEventType[] = "TextPasted";
const char kPasteKeyDetectedEventType[] = "PasteKeyDetected";

constexpr base::TimeDelta kPasteRateLimit = base::Milliseconds(200);
inline constexpr base::TimeDelta kPasteKeyTimerDuration =
    base::Milliseconds(100);
}  // namespace

PasswordProtectionJavaScriptFeature::PasswordProtectionJavaScriptFeature()
    : JavaScriptFeature(web::ContentWorld::kIsolatedWorld,
                        {FeatureScript::CreateWithFilename(
                            kScriptFilename,
                            FeatureScript::InjectionTime::kDocumentStart,
                            FeatureScript::TargetFrames::kAllFrames,
                            FeatureScript::ReinjectionBehavior::
                                kReinjectOnDocumentRecreation)},
                        {}) {}

PasswordProtectionJavaScriptFeature::~PasswordProtectionJavaScriptFeature() =
    default;

// static
PasswordProtectionJavaScriptFeature*
PasswordProtectionJavaScriptFeature::GetInstance() {
  static base::NoDestructor<PasswordProtectionJavaScriptFeature> feature;
  return feature.get();
}

std::optional<std::string>
PasswordProtectionJavaScriptFeature::GetScriptMessageHandlerName() const {
  return kTextEnteredHandlerName;
}

void PasswordProtectionJavaScriptFeature::ScriptMessageReceived(
    web::WebState* web_state,
    const web::ScriptMessage& message) {
  // Verify that the message is well-formed before using it.
  if (!message.legacy_body()->is_dict()) {
    return;
  }
  const base::DictValue& dict = message.legacy_body()->GetDict();

  const std::string* event_type = dict.FindString("eventType");
  if (!event_type || event_type->empty()) {
    return;
  }

  auto observer_it = lookup_by_web_state_.find(web_state);
  if (observer_it == lookup_by_web_state_.end()) {
    return;
  }
  InputEventObserver* observer = observer_it->second;

  if (*event_type == kPasteKeyDetectedEventType) {
    if (!IsIOSPhishGuardPasteShortcutDetectionEnabled()) {
      return;
    }
    if (IsPasteRateLimited(web_state)) {
      return;
    }
    StartPasteKeyTimer(web_state);
    return;
  }

  const std::string* text = dict.FindString("text");
  if (!text || text->empty()) {
    return;
  }

  if (*event_type == kKeyDownEventType) {
    // A key event should consist of a single character. A longer string
    // means the message isn't well-formed, so might be coming from a
    // compromised WebProcess.
    if (base::CountUnicodeCharacters(*text) != 1) {
      return;
    }
    observer->OnKeyPressed(*text);
  } else if (*event_type == kPasteEventType) {
    auto timer_it = paste_key_timers_.find(web_state);
    if (timer_it != paste_key_timers_.end()) {
      // We received the text pasted event very soon after the key detection.
      // Stop the timer to prevent duplicate native pasteboard reading.
      timer_it->second->Stop();
      paste_key_timers_.erase(timer_it);
      observer->OnPaste(*text);
    } else {
      // Standalone paste (context menu) - must check rate limiting.
      if (IsPasteRateLimited(web_state)) {
        return;
      }
      observer->OnPaste(*text);
    }
  }
}

bool PasswordProtectionJavaScriptFeature::IsPasteRateLimited(
    web::WebState* web_state) {
  const base::TimeTicks now = base::TimeTicks::Now();
  auto [it, inserted] = last_paste_timestamps_.insert({web_state, now});
  if (inserted) {
    // First paste for this tab - not rate limited.
    return false;
  }

  const base::TimeDelta elapsed = now - it->second;
  if (elapsed < kPasteRateLimit) {
    return true;
  }

  it->second = now;
  return false;
}

void PasswordProtectionJavaScriptFeature::StartPasteKeyTimer(
    web::WebState* web_state) {
  auto& timer = paste_key_timers_[web_state];
  if (!timer) {
    timer = std::make_unique<base::OneShotTimer>();
  }
  timer->Start(FROM_HERE, kPasteKeyTimerDuration,
               base::BindOnce(
                   &PasswordProtectionJavaScriptFeature::OnPasteKeyTimerExpired,
                   base::Unretained(this), web_state));
}

void PasswordProtectionJavaScriptFeature::OnPasteKeyTimerExpired(
    web::WebState* web_state) {
  paste_key_timers_.erase(web_state);
  auto observer_it = lookup_by_web_state_.find(web_state);
  if (observer_it != lookup_by_web_state_.end()) {
    observer_it->second->OnPasteKeyDetected();
  }
}

void PasswordProtectionJavaScriptFeature::AddObserver(
    InputEventObserver* observer) {
  DCHECK(!lookup_by_observer_[observer]);
  web::WebState* web_state = observer->web_state();
  DCHECK(web_state);
  // A web state can only have one observer.
  DCHECK(!lookup_by_web_state_[web_state]);
  lookup_by_web_state_[web_state] = observer;
  lookup_by_observer_[observer] = web_state;
}

void PasswordProtectionJavaScriptFeature::RemoveObserver(
    InputEventObserver* observer) {
  // Note: observer->web_state() can already be null if the WebState has been
  // destroyed.
  web::WebState* web_state = lookup_by_observer_[observer];
  DCHECK(web_state);
  DCHECK_EQ(observer, lookup_by_web_state_[web_state]);
  lookup_by_web_state_.erase(web_state);
  lookup_by_observer_.erase(observer);
  last_paste_timestamps_.erase(web_state);
  paste_key_timers_.erase(web_state);
}
