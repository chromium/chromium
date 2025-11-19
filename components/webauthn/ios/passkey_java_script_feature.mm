// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/webauthn/ios/passkey_java_script_feature.h"

#import <AuthenticationServices/AuthenticationServices.h>

#import "base/base64.h"
#import "base/no_destructor.h"
#import "base/strings/strcat.h"
#import "base/strings/sys_string_conversions.h"
#import "base/values.h"
#import "components/webauthn/ios/features.h"
#import "components/webauthn/ios/passkey_tab_helper.h"
#import "ios/web/public/js_messaging/java_script_feature.h"
#import "ios/web/public/js_messaging/java_script_feature_util.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"

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

// Frame ID for handle* events.
constexpr char kFrameId[] = "frameId";

// Common parameters of "handleGetRequest" and "handleCreateRequest" events.
constexpr char kRequest[] = "request";
constexpr char kRpEntity[] = "rpEntity";

// Parameters exclusive to the "handleCreateRequest" event.
constexpr char kUserEntity[] = "userEntity";
constexpr char kExcludeCredentials[] = "excludeCredentials";

// Parameter exclusive to the "handleGetRequest" event.
constexpr char kAllowCredentials[] = "allowCredentials";

// Members of the "request" dictionary.
constexpr char kChallenge[] = "challenge";
constexpr char kUserVerification[] = "userVerification";

// Common members of the "rpEntity" and "userEntity" dictionaries.
constexpr char kId[] = "id";
constexpr char kName[] = "name";

// Member exclusive to the "userEntity" dictionary.
constexpr char kDisplayName[] = "displayName";

// Member of the credential descriptors array.
constexpr char kType[] = "type";
constexpr char kTransports[] = "transports";

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

// Decodes a base 64 encoded string into a data vector.
// Returns an empty vector on failure.
std::vector<uint8_t> Base64Decode(const std::string* base_64_string) {
  std::vector<uint8_t> decoded_data;
  if (!base_64_string || base_64_string->empty()) {
    return decoded_data;
  }

  std::string decoded_string;
  if (base::Base64Decode(*base_64_string, &decoded_string,
                         base::Base64DecodePolicy::kStrict)) {
    decoded_data.assign(decoded_string.begin(), decoded_string.end());
  }

  return decoded_data;
}

// Extracts the type of log event received.
// Returns std::nullopt on any non log event or invalid log event.
std::optional<PasskeyTabHelper::WebAuthenticationIOSContentAreaEvent>
ExtractLogEventType(const std::string& event,
                    const base::Value::Dict& dict,
                    const PasskeyTabHelper& tab_helper) {
  if (event == kLogGetRequest) {
    return PasskeyTabHelper::WebAuthenticationIOSContentAreaEvent::
        kGetRequested;
  } else if (event == kLogCreateRequest) {
    return PasskeyTabHelper::WebAuthenticationIOSContentAreaEvent::
        kCreateRequested;
  } else if (event == kLogGetResolved) {
    const std::string* credential_id = dict.FindString(kCredentialId);
    const std::string* rp_id = dict.FindString(kRpId);
    if (!credential_id || credential_id->empty() || !rp_id || rp_id->empty()) {
      return std::nullopt;
    }

    return tab_helper.HasCredential(*rp_id, *credential_id)
               ? PasskeyTabHelper::WebAuthenticationIOSContentAreaEvent::
                     kGetResolvedGpm
               : PasskeyTabHelper::WebAuthenticationIOSContentAreaEvent::
                     kGetResolvedNonGpm;
  } else if (event == kLogCreateResolved) {
    // Parameter for the "logCreateResolved" event.
    std::optional<bool> isGpm = dict.FindBool(kIsGpm);
    if (!isGpm.has_value()) {
      return std::nullopt;
    }

    return *isGpm ? PasskeyTabHelper::WebAuthenticationIOSContentAreaEvent::
                        kCreateResolvedGpm
                  : PasskeyTabHelper::WebAuthenticationIOSContentAreaEvent::
                        kCreateResolvedNonGpm;
  }

  return std::nullopt;
}

// Extracts all parameters required to build a PublicKeyCredentialUserEntity
// object from the provided dictionary.
device::PublicKeyCredentialUserEntity ExtractUserEntity(
    const base::Value::Dict* dict) {
  device::PublicKeyCredentialUserEntity user_entity;
  if (!dict) {
    return user_entity;
  }

  const std::string* id_base_64 = dict->FindString(kId);
  std::vector<uint8_t> decoded_id = Base64Decode(id_base_64);
  if (!decoded_id.empty()) {
    user_entity.id = std::move(decoded_id);
  }

  const std::string* name = dict->FindString(kName);
  if (name && !name->empty()) {
    user_entity.name = *name;
  }

  const std::string* display_name = dict->FindString(kDisplayName);
  if (display_name && !display_name->empty()) {
    user_entity.display_name = *display_name;
  }

  return user_entity;
}

// Extracts all parameters required to build a PublicKeyCredentialRpEntity
// object from the provided dictionary.
device::PublicKeyCredentialRpEntity ExtractRpEntity(
    const base::Value::Dict* dict) {
  device::PublicKeyCredentialRpEntity rp_entity;
  if (!dict) {
    return rp_entity;
  }

  const std::string* id_str = dict->FindString(kId);
  if (id_str && !id_str->empty()) {
    rp_entity.id = *id_str;
  }

  const std::string* name = dict->FindString(kName);
  if (name && !name->empty()) {
    rp_entity.name = *name;
  }

  return rp_entity;
}

// Converts the provided string to a UserVerificationRequirement enum.
device::UserVerificationRequirement ExtractUserVerification(
    const std::string* user_verification) {
  // TODO(crbug.com/385174410): Verifiy that this is the correct default value.
  device::UserVerificationRequirement user_verification_requirement =
      device::UserVerificationRequirement::kPreferred;

  if (!user_verification || user_verification->empty()) {
    return user_verification_requirement;
  }

  // TODO(crbug.com/385174410): Merge this code with
  // UserVerificationPreferenceFromString().
  NSString* user_verification_preference_string =
      base::SysUTF8ToNSString(*user_verification);
  if ([user_verification_preference_string
          isEqualToString:
              ASAuthorizationPublicKeyCredentialUserVerificationPreferenceRequired]) {
    user_verification_requirement =
        device::UserVerificationRequirement::kRequired;
  } else if (
      [user_verification_preference_string
          isEqualToString:
              ASAuthorizationPublicKeyCredentialUserVerificationPreferencePreferred]) {
    user_verification_requirement =
        device::UserVerificationRequirement::kPreferred;
  } else if (
      [user_verification_preference_string
          isEqualToString:
              ASAuthorizationPublicKeyCredentialUserVerificationPreferenceDiscouraged]) {
    user_verification_requirement =
        device::UserVerificationRequirement::kDiscouraged;
  }

  return user_verification_requirement;
}

// Reads a list of PublicKeyCredentialDescriptor from the provided list.
std::vector<device::PublicKeyCredentialDescriptor> ExtractCredentials(
    const base::Value::List* serialized_descriptors) {
  std::vector<device::PublicKeyCredentialDescriptor> credential_descriptors;
  if (!serialized_descriptors) {
    return credential_descriptors;
  }

  for (const auto& serialized_descriptor : *serialized_descriptors) {
    const base::Value::Dict& dict = serialized_descriptor.GetDict();
    std::vector<uint8_t> decoded_id = Base64Decode(dict.FindString(kId));
    if (decoded_id.empty()) {
      continue;
    }

    // Only the public-key type is supported.
    const std::string* type = dict.FindString(kType);
    if (!type || *type != device::kPublicKey) {
      continue;
    }

    device::PublicKeyCredentialDescriptor credential_descriptor(
        device::CredentialType::kPublicKey, std::move(decoded_id));

    // Read transport protocols.
    const base::Value::List* transports = dict.FindList(kTransports);
    if (transports) {
      for (const auto& transport : *transports) {
        std::optional<device::FidoTransportProtocol> fidoTransportProtocol =
            device::ConvertToFidoTransportProtocol(transport.GetString());
        if (fidoTransportProtocol.has_value()) {
          credential_descriptor.transports.insert(
              *fidoTransportProtocol);
        }
      }
    }

    credential_descriptors.emplace_back(credential_descriptor);
  }

  return credential_descriptors;
}

// Extracts all parameters required to build a RequestParams object from the
// provided dictionary.
PasskeyTabHelper::RequestParams ExtractRequestParams(
    const base::Value::Dict* dict) {
  if (!dict) {
    return PasskeyTabHelper::RequestParams();
  }

  const std::string* frame_id = dict->FindString(kFrameId);

  return PasskeyTabHelper::RequestParams(
      frame_id ? *frame_id : "", ExtractRpEntity(dict->FindDict(kRpEntity)),
      Base64Decode(dict->FindString(kChallenge)),
      ExtractUserVerification(dict->FindString(kUserVerification)));
}

// Extracts all parameters required to build an ExtractAssertionRequestParams
// object from the provided dictionary.
PasskeyTabHelper::AssertionRequestParams ExtractAssertionRequestParams(
    const base::Value::Dict& dict) {
  return PasskeyTabHelper::AssertionRequestParams(
      ExtractRequestParams(dict.FindDict(kRequest)),
      ExtractCredentials(dict.FindList(kAllowCredentials)));
}

// Extracts all parameters required to build a RegistrationRequestParams object
// from the provided dictionary.
PasskeyTabHelper::RegistrationRequestParams ExtractRegistrationRequestParams(
    const base::Value::Dict& dict) {
  return PasskeyTabHelper::RegistrationRequestParams(
      ExtractRequestParams(dict.FindDict(kRequest)),
      ExtractUserEntity(dict.FindDict(kUserEntity)),
      ExtractCredentials(dict.FindList(kExcludeCredentials)));
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
              FeatureScript::ReinjectionBehavior::kInjectOncePerWindow,
              base::BindRepeating(&GetPlaceholderReplacements))},
          {web::java_script_features::GetCommonJavaScriptFeature()}) {}

PasskeyJavaScriptFeature::~PasskeyJavaScriptFeature() = default;

void PasskeyJavaScriptFeature::DeferToRenderer(web::WebFrame* web_frame) {
  CallJavaScriptFunction(web_frame, "passkey.deferToRenderer", {});
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
        ExtractAssertionRequestParams(dict));
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
        ExtractRegistrationRequestParams(dict));
    return;
  }

  std::optional<PasskeyTabHelper::WebAuthenticationIOSContentAreaEvent>
      log_event_type = ExtractLogEventType(*event, dict, *passkey_tab_helper);

  if (log_event_type.has_value()) {
    passkey_tab_helper->LogEvent(*log_event_type);
  }
}
