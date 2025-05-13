// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/webauthn/ios/passkey_java_script_feature.h"

#import "base/no_destructor.h"
#import "base/values.h"
#import "components/webauthn/ios/passkey_tab_helper.h"
#import "ios/web/public/js_messaging/java_script_feature_util.h"
#import "ios/web/public/js_messaging/script_message.h"

namespace {

constexpr char kScriptName[] = "passkey_controller";
constexpr char kHandlerName[] = "PasskeyInteractionHandler";

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
  // This message is sent whenever a navigator.credentials get() or create() is
  // called for a WebAuthn credential.
  // Expected argument:
  // event: (string) Describes a type of event.
  //
  // For some events there are more expected arguments described below.

  base::Value* body = message.body();
  if (!body || !body->is_dict()) {
    return;
  }

  const base::Value::Dict& dict = body->GetDict();
  const std::string* event = dict.FindString("event");
  if (!event || event->empty()) {
    return;
  }

  PasskeyTabHelper* passkey_tab_helper =
      PasskeyTabHelper::FromWebState(web_state);
  CHECK(passkey_tab_helper);

  // For those events there are no more expected arguments.
  if (*event == "getRequested" || *event == "createRequested" ||
      *event == "createResolvedGpm" || *event == "createResolvedNonGpm") {
    passkey_tab_helper->LogEventFromString(*event);
    return;
  }

  // Expected arguments for "getResolved" event:
  // credential_id: (string) base64url encoded identifer of the credential.
  // rp_id: (string) The relying party's identifier.
  if (*event == "getResolved") {
    const std::string* credential_id = dict.FindString("credential_id");
    const std::string* rp_id = dict.FindString("rp_id");
    if (credential_id && !credential_id->empty() && rp_id && !rp_id->empty()) {
      passkey_tab_helper->HandleGetResolvedEvent(*credential_id, *rp_id);
    }
    return;
  }

  // TODO(crbug.com/369629469): Handle other types of events.
}
