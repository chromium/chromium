// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/webauthn/ios/passkey_java_script_feature.h"

#import "base/base64.h"
#import "base/no_destructor.h"
#import "base/strings/strcat.h"
#import "base/strings/sys_string_conversions.h"
#import "base/values.h"
#import "components/webauthn/ios/features.h"
#import "components/webauthn/ios/passkey_request_parser.h"
#import "components/webauthn/ios/passkey_tab_helper.h"
#import "ios/web/public/js_messaging/java_script_feature.h"
#import "ios/web/public/js_messaging/java_script_feature_util.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"

namespace webauthn {

using PasskeyTabHelper::WebAuthenticationIOSContentAreaEvent::kCreateRequested;
using PasskeyTabHelper::WebAuthenticationIOSContentAreaEvent::
    kCreateResolvedGpm;
using PasskeyTabHelper::WebAuthenticationIOSContentAreaEvent::
    kCreateResolvedNonGpm;
using PasskeyTabHelper::WebAuthenticationIOSContentAreaEvent::kGetRequested;
using PasskeyTabHelper::WebAuthenticationIOSContentAreaEvent::kGetResolvedGpm;
using PasskeyTabHelper::WebAuthenticationIOSContentAreaEvent::
    kGetResolvedNonGpm;

namespace {

constexpr char kScriptName[] = "passkey_controller";
constexpr char kHandlerName[] = "PasskeyInteractionHandler";

// Placeholder logic.
constexpr char kHandleModalPasskeyRequestsPlaceholder[] =
    "/*! {{PLACEHOLDER_HANDLE_MODAL_PASSKEY_REQUESTS}} */";

// Message event.
constexpr char kEvent[] = "event";

// Message event types.
constexpr char kHandleGetRequest[] = "handleGetRequest";
constexpr char kHandleCreateRequest[] = "handleCreateRequest";
constexpr char kLogGetRequest[] = "logGetRequest";
constexpr char kLogCreateRequest[] = "logCreateRequest";
constexpr char kLogGetResolved[] = "logGetResolved";
constexpr char kLogCreateResolved[] = "logCreateResolved";

// Parameters of the "logGetResolved" event.
constexpr char kCredentialId[] = "credentialId";
constexpr char kRpId[] = "rpId";

// Parameter for the "logCreateResolved" event.
constexpr char kIsGpm[] = "isGpm";

// Returns the placeholder replacements for the JavaScript feature script.
web::JavaScriptFeature::FeatureScript::PlaceholderReplacements
GetPlaceholderReplacements() {
  // Overrides the placeholder for whether modal passkey requests can be handled
  // by the browser.
  bool handle_modal_passkey_requests =
      base::FeatureList::IsEnabled(kIOSPasskeyModalLoginWithShim);
  std::u16string full_script_block = base::StrCat(
      {u"const shouldHandleModalPasskeyRequests = () => { return ",
       handle_modal_passkey_requests ? u"true;" : u"false;", u" };"});
  return @{
    base::SysUTF8ToNSString(kHandleModalPasskeyRequestsPlaceholder) :
        base::SysUTF16ToNSString(full_script_block),
  };
}

// Reads the type of log event received.
// Returns std::nullopt on any non log event or invalid log event.
std::optional<PasskeyTabHelper::WebAuthenticationIOSContentAreaEvent>
ReadLogEventType(const std::string& event,
                 const base::Value::Dict& dict,
                 const PasskeyTabHelper& tab_helper) {
  if (event == kLogGetRequest) {
    return kGetRequested;
  } else if (event == kLogCreateRequest) {
    return kCreateRequested;
  } else if (event == kLogGetResolved) {
    const std::string* credential_id = dict.FindString(kCredentialId);
    const std::string* rp_id = dict.FindString(kRpId);
    if (!credential_id || credential_id->empty() || !rp_id || rp_id->empty()) {
      return std::nullopt;
    }

    bool isGpm = tab_helper.HasCredential(*rp_id, *credential_id);
    return isGpm ? kGetResolvedGpm : kGetResolvedNonGpm;
  } else if (event == kLogCreateResolved) {
    // Parameter for the "logCreateResolved" event.
    std::optional<bool> isGpm = dict.FindBool(kIsGpm);
    if (!isGpm.has_value()) {
      return std::nullopt;
    }

    return *isGpm ? kCreateResolvedGpm : kCreateResolvedNonGpm;
  }

  return std::nullopt;
}

}  // namespace

PasskeyJavaScriptFeature::AttestationData::AttestationData(
    std::vector<uint8_t> attestation_object,
    std::vector<uint8_t> authenticator_data,
    std::vector<uint8_t> public_key_spki_der,
    std::string client_data_json)
    : attestation_object(std::move(attestation_object)),
      authenticator_data(std::move(authenticator_data)),
      public_key_spki_der(std::move(public_key_spki_der)),
      client_data_json(std::move(client_data_json)) {}

PasskeyJavaScriptFeature::AttestationData::AttestationData(
    PasskeyJavaScriptFeature::AttestationData&& other) = default;
PasskeyJavaScriptFeature::AttestationData::~AttestationData() = default;

PasskeyJavaScriptFeature::AssertionData::AssertionData(
    std::vector<uint8_t> signature,
    std::vector<uint8_t> authenticator_data,
    std::vector<uint8_t> user_handle,
    std::string client_data_json)
    : signature(std::move(signature)),
      authenticator_data(std::move(authenticator_data)),
      user_handle(std::move(user_handle)),
      client_data_json(std::move(client_data_json)) {}

PasskeyJavaScriptFeature::AssertionData::AssertionData(
    PasskeyJavaScriptFeature::AssertionData&& other) = default;
PasskeyJavaScriptFeature::AssertionData::~AssertionData() = default;

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
              FeatureScript::ReinjectionBehavior::kInjectOncePerWindow,
              base::BindRepeating(&GetPlaceholderReplacements))},
          {web::java_script_features::GetCommonJavaScriptFeature()}) {}

PasskeyJavaScriptFeature::~PasskeyJavaScriptFeature() = default;

void PasskeyJavaScriptFeature::DeferToRenderer(web::WebFrame* web_frame,
                                               std::string_view request_id) {
  CallJavaScriptFunction(web_frame, "passkey.deferToRenderer",
                         base::Value::List().Append(request_id));
}

void PasskeyJavaScriptFeature::ResolveAttestationRequest(
    web::WebFrame* web_frame,
    std::string_view request_id,
    std::string_view credential_id,
    AttestationData attestation_data) {
  CallJavaScriptFunction(
      web_frame, "passkey.resolveAttestationRequest",
      base::Value::List()
          .Append(request_id)
          .Append(base::Base64Encode(credential_id))
          .Append(base::Base64Encode(attestation_data.attestation_object))
          .Append(base::Base64Encode(attestation_data.authenticator_data))
          .Append(base::Base64Encode(attestation_data.public_key_spki_der))
          .Append(attestation_data.client_data_json));
}

void PasskeyJavaScriptFeature::ResolveAssertionRequest(
    web::WebFrame* web_frame,
    std::string_view request_id,
    std::string_view credential_id,
    AssertionData assertion_data) {
  CallJavaScriptFunction(
      web_frame, "passkey.resolveAssertionRequest",
      base::Value::List()
          .Append(request_id)
          .Append(base::Base64Encode(credential_id))
          .Append(base::Base64Encode(assertion_data.authenticator_data))
          .Append(assertion_data.client_data_json)
          .Append(base::Base64Encode(assertion_data.signature))
          .Append(base::Base64Encode(assertion_data.user_handle)));
}

std::optional<std::string>
PasskeyJavaScriptFeature::GetScriptMessageHandlerName() const {
  return kHandlerName;
}

void PasskeyJavaScriptFeature::ScriptMessageReceived(
    web::WebState* web_state,
    const web::ScriptMessage& message) {
  PasskeyTabHelper* passkey_tab_helper =
      PasskeyTabHelper::FromWebState(web_state);
  if (!passkey_tab_helper) {
    // Passkey tab helper is not created in some WebState cases for which
    // passkey flows should not be applicable either (e.g. Lens overlay).
    // Return early in this case. If there is somehow a valid passkey flow that
    // should happen, it will still be handled by invoking Credential Provider
    // Extension logic in the controller.
    return;
  }

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
  const std::string* event = dict.FindString(kEvent);
  if (!event || event->empty()) {
    return;
  }

  if (*event == kHandleGetRequest) {
    if (!base::FeatureList::IsEnabled(kIOSPasskeyModalLoginWithShim)) {
      // TODO(crbug.com/369629469): Log metrics for unexpected events.
      return;
    }

    passkey_tab_helper->LogEvent(
        PasskeyTabHelper::WebAuthenticationIOSContentAreaEvent::kGetRequested);
    passkey_tab_helper->HandleGetRequestedEvent(
        BuildAssertionRequestParams(dict));
    return;
  } else if (*event == kHandleCreateRequest) {
    if (!base::FeatureList::IsEnabled(kIOSPasskeyModalLoginWithShim)) {
      // TODO(crbug.com/369629469): Log metrics for unexpected events.
      return;
    }

    passkey_tab_helper->LogEvent(
        PasskeyTabHelper::WebAuthenticationIOSContentAreaEvent::
            kCreateRequested);
    passkey_tab_helper->HandleCreateRequestedEvent(
        BuildRegistrationRequestParams(dict));
    return;
  }

  std::optional<PasskeyTabHelper::WebAuthenticationIOSContentAreaEvent>
      log_event_type = ReadLogEventType(*event, dict, *passkey_tab_helper);

  if (log_event_type.has_value()) {
    passkey_tab_helper->LogEvent(*log_event_type);
  }
}

}  // namespace webauthn
