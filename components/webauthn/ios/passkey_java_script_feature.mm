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
#import "components/webauthn/ios/passkey_tab_helper.h"
#import "device/fido/fido_user_verification_requirement.h"
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

// Request ID associated with deferred promises.
constexpr char kRequestId[] = "requestId";

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
  if (!user_verification) {
    // Fall back to the `kPreferred` UV requirement as per the WebAuthn spec.
    return device::UserVerificationRequirement::kPreferred;
  }

  // TODO(crbug.com/460484682): Merge this code with
  // ShouldPerformUserVerificationForPreference().
  return device::ConvertToUserVerificationRequirement(*user_verification)
      .value_or(device::UserVerificationRequirement::kPreferred);
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
          credential_descriptor.transports.insert(*fidoTransportProtocol);
        }
      }
    }

    credential_descriptors.emplace_back(credential_descriptor);
  }

  return credential_descriptors;
}

// Extracts all parameters required to build a PasskeyRequestParams object from
// the provided dictionary.
PasskeyRequestParams ExtractRequestParams(const base::Value::Dict& dict) {
  const std::string* frame_id = dict.FindString(kFrameId);
  const std::string* request_id = dict.FindString(kRequestId);
  const base::Value::Dict* request_dict = dict.FindDict(kRequest);
  if (!frame_id || !request_id || !request_dict) {
    return PasskeyRequestParams();
  }

  return PasskeyRequestParams(
      *frame_id, *request_id, ExtractRpEntity(dict.FindDict(kRpEntity)),
      Base64Decode(request_dict->FindString(kChallenge)),
      ExtractUserVerification(request_dict->FindString(kUserVerification)));
}

// Extracts all parameters required to build an ExtractAssertionRequestParams
// object from the provided dictionary.
AssertionRequestParams ExtractAssertionRequestParams(
    const base::Value::Dict& dict) {
  return AssertionRequestParams(
      ExtractRequestParams(dict),
      ExtractCredentials(dict.FindList(kAllowCredentials)));
}

// Extracts all parameters required to build a RegistrationRequestParams object
// from the provided dictionary.
RegistrationRequestParams ExtractRegistrationRequestParams(
    const base::Value::Dict& dict) {
  return RegistrationRequestParams(
      ExtractRequestParams(dict), ExtractUserEntity(dict.FindDict(kUserEntity)),
      ExtractCredentials(dict.FindList(kExcludeCredentials)));
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

}  // namespace webauthn
