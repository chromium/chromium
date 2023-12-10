// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/password_protection_java_script_feature.h"

#import "base/check.h"
#import "base/ios/ios_util.h"
#import "base/no_destructor.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/safe_browsing/model/input_event_observer.h"
#import "ios/web/public/js_messaging/script_message.h"

namespace {
const char kScriptFilename[] = "password_protection";

const char kTextEnteredHandlerName[] = "PasswordProtectionTextEntered";

// Values for the "eventType" field in messages received by this feature's
// script message handler.
const char kPasteEventType[] = "TextPasted";
const char kKeyPressedEventType[] = "KeyPressed";
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
  if (!message.body()->is_dict()) {
    return;
  }
  const base::Value::Dict& dict = message.body()->GetDict();

  const std::string* event_type = dict.FindString("eventType");
  if (!event_type || event_type->empty()) {
    return;
  }

  const std::string* text = dict.FindString("text");
  if (!text || text->empty()) {
    return;
  }

  InputEventObserver* observer = lookup_by_web_state_[web_state];
  if (!observer) {
    return;
  }

  if (*event_type == kKeyPressedEventType) {
    // A keypress event should consist of a single character. A longer string
    // means the message isn't well-formed, so might be coming from a
    // compromised WebProcess.
    if ((*text).size() > 1) {
      return;
    }
    observer->OnKeyPressed(*text);
  } else if (*event_type == kPasteEventType) {
    observer->OnPaste(*text);
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
}
