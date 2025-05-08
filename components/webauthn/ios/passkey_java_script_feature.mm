// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/webauthn/ios/passkey_java_script_feature.h"

#import <optional>

#import "base/metrics/histogram_functions.h"
#import "base/no_destructor.h"
#import "base/values.h"
#import "ios/web/public/js_messaging/java_script_feature_util.h"
#import "ios/web/public/js_messaging/script_message.h"

namespace {

constexpr char kScriptName[] = "passkey_controller";
constexpr char kHandlerName[] = "PasskeyInteractionHandler";

// These values are logged to UMA. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange
enum class WebAuthenticationIOSContentAreaEvent {
  kGetRequested,
  kCreateRequested,
  kMaxValue = kCreateRequested,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/webauthn/enums.xml)

// Logs metrics indicating that an event occurred, with the event type
// determined by the given string.
void LogEvent(WebAuthenticationIOSContentAreaEvent event) {
  base::UmaHistogramEnumeration("WebAuthentication.IOS.ContentAreaEvent",
                                event);
}

}  // namespace

// static
PasskeyJavaScriptFeature* PasskeyJavaScriptFeature::GetInstance() {
  static base::NoDestructor<PasskeyJavaScriptFeature> instance;
  return instance.get();
}

PasskeyJavaScriptFeature::PasskeyJavaScriptFeature()
    : web::JavaScriptFeature(
          // This is a shim, so it needs to be in the page content world.
          web::ContentWorld::kPageContentWorld,
          {FeatureScript::CreateWithFilename(
              kScriptName,
              FeatureScript::InjectionTime::kDocumentStart,
              // It's valid for passkey flows to happen not in a main frame,
              // though it requires appropriate permissions policy to be set
              // (https://w3c.github.io/webauthn/#sctn-permissions-policy).
              FeatureScript::TargetFrames::kAllFrames,
              FeatureScript::ReinjectionBehavior::kInjectOncePerWindow)},
          {web::java_script_features::GetCommonJavaScriptFeature(),
           web::java_script_features::GetMessageJavaScriptFeature()}) {}

PasskeyJavaScriptFeature::~PasskeyJavaScriptFeature() = default;

std::optional<std::string>
PasskeyJavaScriptFeature::GetScriptMessageHandlerName() const {
  return kHandlerName;
}

void PasskeyJavaScriptFeature::ScriptMessageReceived(
    web::WebState* web_state,
    const web::ScriptMessage& message) {
  base::Value* body = message.body();
  if (!body || !body->is_dict()) {
    return;
  }

  const base::Value::Dict& dict = body->GetDict();
  const std::string* event = dict.FindString("event");
  if (!event || event->empty()) {
    return;
  }

  if (*event == "getRequested") {
    LogEvent(WebAuthenticationIOSContentAreaEvent::kGetRequested);
  } else if (*event == "createRequested") {
    LogEvent(WebAuthenticationIOSContentAreaEvent::kCreateRequested);
  }

  // TODO(crbug.com/369629469): Log other types of events.
}
