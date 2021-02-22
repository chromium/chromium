// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/password_protection_java_script_feature.h"

#import <WebKit/WebKit.h>

#include "base/check.h"
#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/safe_browsing/input_event_observer.h"
#include "ios/web/public/js_messaging/web_view_web_state_map.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kScriptFilename[] = "password_protection_js";

const char kTextEnteredHandlerName[] = "PasswordProtectionTextEntered";

// Values for the "eventType" field in messages received by this feature's
// script message handler.
const char kPasteEventType[] = "TextPasted";
const char kKeyPressedEventType[] = "KeyPressed";
}  // namespace

PasswordProtectionJavaScriptFeature::PasswordProtectionJavaScriptFeature()
    : JavaScriptFeature(ContentWorld::kIsolatedWorldOnly,
                        {FeatureScript::CreateWithFilename(
                            kScriptFilename,
                            FeatureScript::InjectionTime::kDocumentStart,
                            FeatureScript::TargetFrames::kAllFrames,
                            FeatureScript::ReinjectionBehavior::
                                kReinjectOnDocumentRecreation)},
                        {}) {
  // This feature depends on JavaScript isolated worlds for security, so must
  // only be used on iOS 14+.
  DCHECK(base::ios::IsRunningOnIOS14OrLater());
}

PasswordProtectionJavaScriptFeature::~PasswordProtectionJavaScriptFeature() =
    default;

// static
PasswordProtectionJavaScriptFeature*
PasswordProtectionJavaScriptFeature::GetInstance() {
  static std::unique_ptr<PasswordProtectionJavaScriptFeature> feature = nullptr;
  if (!feature) {
    feature = std::make_unique<PasswordProtectionJavaScriptFeature>();
  }
  return feature.get();
}

base::Optional<std::string>
PasswordProtectionJavaScriptFeature::GetScriptMessageHandlerName() const {
  return kTextEnteredHandlerName;
}

void PasswordProtectionJavaScriptFeature::ScriptMessageReceived(
    web::BrowserState* browser_state,
    WKScriptMessage* message) {
  // Verify that the message is well-formed before using it.
  if (![message.body isKindOfClass:[NSDictionary class]])
    return;

  NSString* eventType = message.body[@"eventType"];
  if (!eventType || ![eventType isKindOfClass:[NSString class]] ||
      ![eventType length]) {
    return;
  }

  NSString* text = message.body[@"text"];
  if (!text || ![text isKindOfClass:[NSString class]] || ![text length]) {
    return;
  }

  std::string event_type_str = base::SysNSStringToUTF8(eventType);
  std::string text_str = base::SysNSStringToUTF8(text);

  web::WebViewWebStateMap* map =
      web::WebViewWebStateMap::FromBrowserState(browser_state);
  web::WebState* web_state = map->GetWebStateForWebView(message.webView);
  InputEventObserver* observer = lookup_by_web_state_[web_state];
  if (!observer)
    return;

  if (event_type_str == kKeyPressedEventType) {
    // A keypress event should consist of a single character. A longer string
    // means the message isn't well-formed, so might be coming from a
    // compromised WebProcess.
    if (text_str.size() > 1)
      return;
    observer->OnKeyPressed(text_str);
  } else if (event_type_str == kPasteEventType) {
    observer->OnPaste(text_str);
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
