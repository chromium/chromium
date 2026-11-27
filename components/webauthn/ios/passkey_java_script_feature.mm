// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/webauthn/ios/passkey_java_script_feature.h"

#import "base/base64url.h"
#import "base/metrics/histogram_functions.h"
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
#import "ios/web/public/web_state.h"
#import "url/origin.h"

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
using WebAuthenticationIOSContentAreaEvent =
    PasskeyTabHelper::WebAuthenticationIOSContentAreaEvent;

namespace {

constexpr char kScriptName[] = "passkey_controller";
constexpr char kHandlerName[] = "PasskeyInteractionHandler";
constexpr char kPasskeyParsingErrorHistogram[] =
    "WebAuthentication.IOS.PasskeyParsingError";

// Placeholder logic.
constexpr char kHandlePasskeyRequestsPlaceholder[] =
    "/*! {{PLACEHOLDER_HANDLE_PASSKEY_REQUESTS}} */";

// Returns the placeholder replacements for the JavaScript feature script.
web::JavaScriptFeature::FeatureScript::PlaceholderReplacements
GetPlaceholderReplacements() {
  // Overrides the placeholder for whether modal and conditional passkey
  // requests can be handled by the browser.
  bool handle_modal_passkey_requests =
      base::FeatureList::IsEnabled(kIOSPasskeyModalLoginWithShim);
  bool handle_conditional_passkey_requests =
      base::FeatureList::IsEnabled(kIOSPasskeyConditionalLoginWithShim);
  // Overrides the placeholder for whether to shim
  // PublicKeyCredential.isUserVerifyingPlatformAuthenticatorAvailable.
  bool shim_is_uvpaa = base::FeatureList::IsEnabled(kIOSPasskeyUVPAAWorkaround);

  std::u16string handle_passkey_requests_script_block = base::StrCat(
      {u"const shouldHandleModalPasskeyRequests = () => { return ",
       handle_modal_passkey_requests ? u"true;" : u"false;", u" };\n\n",
       u"const shouldHandleConditionalPasskeyRequests = () => { return ",
       handle_conditional_passkey_requests ? u"true;" : u"false;", u" };\n\n",
       u"const shouldShimIsUVPAA = () => { return ",
       shim_is_uvpaa ? u"true;" : u"false;", u" };"});
  return @{
    base::SysUTF8ToNSString(kHandlePasskeyRequestsPlaceholder) :
        base::SysUTF16ToNSString(handle_passkey_requests_script_block),
  };
}

// Encodes a byte vector to base 64 URL encoded string.
std::string Base64UrlEncode(base::span<const uint8_t> input) {
  // Omit padding, according to the spec. See:
  // https://w3c.github.io/webauthn/#base64url-encoding
  return base::Base64UrlEncode(input,
                               base::Base64UrlEncodePolicy::OMIT_PADDING);
}

// Encodes a string to a base 64 URL encoded string.
std::string Base64UrlEncode(std::string_view input) {
  // Omit padding, according to the spec. See:
  // https://w3c.github.io/webauthn/#base64url-encoding
  return base::Base64UrlEncode(input,
                               base::Base64UrlEncodePolicy::OMIT_PADDING);
}

// Validates whether the given passkey registration (create) request is
// permitted to proceed.
bool ValidateFeatureUsageForCreate(
    const RegistrationRequestParams& request_params,
    bool has_user_interaction,
    PasskeyTabHelper* passkey_tab_helper) {
  switch (request_params.Type()) {
    case PasskeyRequestParams::RequestType::kModal:
      // Modal passkey registration is only allowed if it originates from a user
      // gesture.
      if (!has_user_interaction) {
        return false;
      }
      return base::FeatureList::IsEnabled(kIOSPasskeyModalLoginWithShim);
    case PasskeyRequestParams::RequestType::kConditionalCreate: {
      // Conditional create is only allowed if there is user interaction or if
      // the tab has eligible automatic passkey upgrade credentials from a
      // recent password login.
      bool is_user_authenticated =
          has_user_interaction ||
          (passkey_tab_helper &&
           passkey_tab_helper->HasAutomaticPasskeyUpgradeEligibility(
               request_params));
      if (!is_user_authenticated) {
        return false;
      }
      return base::FeatureList::IsEnabled(kIOSPasskeyConditionalLoginWithShim);
    }
    case PasskeyRequestParams::RequestType::kConditionalGet:
    case PasskeyRequestParams::RequestType::kUnknown:
      return false;
  }
}

// Validates whether the given passkey assertion (get) request is permitted to
// proceed.
bool ValidateFeatureUsageForGet(const AssertionRequestParams& request_params,
                                bool has_user_interaction) {
  switch (request_params.Type()) {
    case PasskeyRequestParams::RequestType::kModal:
      // Modal passkey assertion is only allowed if it originates from a user
      // gesture.
      if (!has_user_interaction) {
        return false;
      }
      return base::FeatureList::IsEnabled(kIOSPasskeyModalLoginWithShim);
    case PasskeyRequestParams::RequestType::kConditionalGet:
      return base::FeatureList::IsEnabled(kIOSPasskeyConditionalLoginWithShim);
    case PasskeyRequestParams::RequestType::kConditionalCreate:
    case PasskeyRequestParams::RequestType::kUnknown:
      return false;
  }
}

// Helper to determine the logging event type based on the parsed script event.
// Returns std::nullopt for internal events that should not be logged to UMA.
std::optional<WebAuthenticationIOSContentAreaEvent>
ToWebAuthenticationIOSContentAreaEvent(PasskeyScriptEvent event) {
  switch (event) {
    case PasskeyScriptEvent::kCleanupRequest:
      return std::nullopt;
    case PasskeyScriptEvent::kHandleGetRequest:
    case PasskeyScriptEvent::kLogGetRequest:
      return WebAuthenticationIOSContentAreaEvent::kGetRequested;
    case PasskeyScriptEvent::kHandleCreateRequest:
    case PasskeyScriptEvent::kLogCreateRequest:
      return WebAuthenticationIOSContentAreaEvent::kCreateRequested;
    case PasskeyScriptEvent::kLogGetResolvedGpm:
      return WebAuthenticationIOSContentAreaEvent::kGetResolvedGpm;
    case PasskeyScriptEvent::kLogGetResolvedNonGpm:
      return WebAuthenticationIOSContentAreaEvent::kGetResolvedNonGpm;
    case PasskeyScriptEvent::kLogCreateResolvedGpm:
      return WebAuthenticationIOSContentAreaEvent::kCreateResolvedGpm;
    case PasskeyScriptEvent::kLogCreateResolvedNonGpm:
      return WebAuthenticationIOSContentAreaEvent::kCreateResolvedNonGpm;
    case PasskeyScriptEvent::kCancelRequest:
      return WebAuthenticationIOSContentAreaEvent::kCancelRequested;
    case PasskeyScriptEvent::kSignalUnknownCredential:
      return WebAuthenticationIOSContentAreaEvent::
          kSignalUnknownCredentialRequested;
    case PasskeyScriptEvent::kSignalCurrentUserDetails:
      return WebAuthenticationIOSContentAreaEvent::
          kSignalCurrentUserDetailsRequested;
    case PasskeyScriptEvent::kSignalAllAcceptedCredentials:
      return WebAuthenticationIOSContentAreaEvent::
          kSignalAllAcceptedCredentialsRequested;
  }
}

// Returns whether `event` is only used for metrics logging and requires no
// further browser-side handling.
bool IsLogOnlyEvent(PasskeyScriptEvent event) {
  switch (event) {
    case PasskeyScriptEvent::kLogGetRequest:
    case PasskeyScriptEvent::kLogCreateRequest:
    case PasskeyScriptEvent::kLogGetResolvedGpm:
    case PasskeyScriptEvent::kLogGetResolvedNonGpm:
    case PasskeyScriptEvent::kLogCreateResolvedGpm:
    case PasskeyScriptEvent::kLogCreateResolvedNonGpm:
      return true;
    case PasskeyScriptEvent::kCancelRequest:
    case PasskeyScriptEvent::kCleanupRequest:
    case PasskeyScriptEvent::kHandleGetRequest:
    case PasskeyScriptEvent::kHandleCreateRequest:
    case PasskeyScriptEvent::kSignalUnknownCredential:
    case PasskeyScriptEvent::kSignalCurrentUserDetails:
    case PasskeyScriptEvent::kSignalAllAcceptedCredentials:
      return false;
  }
}

// Builds `RequestInfo` from `dict` and logs a parsing error metric on failure.
std::optional<IOSPasskeyClient::RequestInfo> ParseRequestInfo(
    const base::DictValue& dict) {
  auto request_info = BuildRequestInfo(dict);
  if (!request_info.has_value()) {
    base::UmaHistogramEnumeration(kPasskeyParsingErrorHistogram,
                                  request_info.error());
    return std::nullopt;
  }
  return std::move(*request_info);
}

// Returns the string representation of a WebAuthnError's name.
std::string_view WebAuthnErrorToName(WebAuthnError error) {
  switch (error) {
    case WebAuthnError::kNotAllowedError:
      return kNotAllowedErrorName;
    case WebAuthnError::kInvalidStateError:
      return kInvalidStateErrorName;
  }
}

// Returns the string representation of a WebAuthnError's description.
std::string_view WebAuthnErrorToMessage(WebAuthnError error) {
  switch (error) {
    case WebAuthnError::kNotAllowedError:
      return kNotAllowedErrorMessage;
    case WebAuthnError::kInvalidStateError:
      return kCredentialExcludedErrorMessage;
  }
}

}  // namespace

PasskeyJavaScriptFeature::AttestationData::AttestationData(
    std::vector<uint8_t> attestation_object,
    std::vector<uint8_t> authenticator_data,
    std::vector<uint8_t> public_key_spki_der,
    std::string client_data_json,
    passkey_model_utils::ExtensionOutputData extension_output_data)
    : attestation_object(std::move(attestation_object)),
      authenticator_data(std::move(authenticator_data)),
      public_key_spki_der(std::move(public_key_spki_der)),
      client_data_json(std::move(client_data_json)),
      extension_output_data(std::move(extension_output_data)) {}

PasskeyJavaScriptFeature::AttestationData::AttestationData(
    PasskeyJavaScriptFeature::AttestationData&& other) = default;
PasskeyJavaScriptFeature::AttestationData::~AttestationData() = default;

PasskeyJavaScriptFeature::AssertionData::AssertionData(
    std::vector<uint8_t> signature,
    std::vector<uint8_t> authenticator_data,
    std::vector<uint8_t> user_handle,
    std::string client_data_json,
    passkey_model_utils::ExtensionOutputData extension_output_data)
    : signature(std::move(signature)),
      authenticator_data(std::move(authenticator_data)),
      user_handle(std::move(user_handle)),
      client_data_json(std::move(client_data_json)),
      extension_output_data(std::move(extension_output_data)) {}

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
              base::BindRepeating(&GetPlaceholderReplacements))}) {}

PasskeyJavaScriptFeature::~PasskeyJavaScriptFeature() = default;

void PasskeyJavaScriptFeature::RejectPasskeyRequest(web::WebFrame* web_frame,
                                                    std::string_view request_id,
                                                    WebAuthnError error) {
  CallJavaScriptFunction(web_frame, "passkey.rejectPasskeyRequest",
                         base::ListValue()
                             .Append(request_id)
                             .Append(WebAuthnErrorToName(error))
                             .Append(WebAuthnErrorToMessage(error)));
}

void PasskeyJavaScriptFeature::DeferToRenderer(
    web::WebFrame* web_frame,
    std::string_view request_id,
    PasskeyRequestParams::RequestType request_type) {
  CallJavaScriptFunction(web_frame, "passkey.deferToRenderer",
                         base::ListValue()
                             .Append(request_id)
                             .Append(std::to_underlying(request_type)));
}

void PasskeyJavaScriptFeature::ResolveAttestationRequest(
    web::WebFrame* web_frame,
    std::string_view request_id,
    std::string_view credential_id,
    AttestationData attestation_data) {
  CallJavaScriptFunction(
      web_frame, "passkey.resolveAttestationRequest",
      base::ListValue()
          .Append(request_id)
          .Append(Base64UrlEncode(credential_id))
          .Append(Base64UrlEncode(attestation_data.attestation_object))
          .Append(Base64UrlEncode(attestation_data.authenticator_data))
          .Append(Base64UrlEncode(attestation_data.public_key_spki_der))
          .Append(attestation_data.client_data_json)
          .Append(ToAuthenticationExtensionsClientOutputsJSON(
              std::move(attestation_data.extension_output_data))));
}

void PasskeyJavaScriptFeature::ResolveAssertionRequest(
    web::WebFrame* web_frame,
    std::string_view request_id,
    std::string_view credential_id,
    AssertionData assertion_data) {
  CallJavaScriptFunction(
      web_frame, "passkey.resolveAssertionRequest",
      base::ListValue()
          .Append(request_id)
          .Append(Base64UrlEncode(credential_id))
          .Append(Base64UrlEncode(assertion_data.signature))
          .Append(Base64UrlEncode(assertion_data.authenticator_data))
          .Append(Base64UrlEncode(assertion_data.user_handle))
          .Append(assertion_data.client_data_json)
          .Append(ToAuthenticationExtensionsClientOutputsJSON(
              std::move(assertion_data.extension_output_data))));
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
  base::Value* body = message.legacy_body();
  if (!body || !body->is_dict()) {
    return;
  }

  const base::DictValue& dict = body->GetDict();

  std::optional<PasskeyScriptEvent> event = ParsePasskeyScriptEvent(
      dict, message.security_origin(),
      [passkey_tab_helper](const std::string& rp_id,
                           const std::string& credential_id) {
        return passkey_tab_helper->HasCredential(rp_id, credential_id);
      });

  if (!event.has_value()) {
    // TODO(crbug.com/460485333): Log parsing failure metrics.
    return;
  }

  std::optional<WebAuthenticationIOSContentAreaEvent> content_area_event =
      ToWebAuthenticationIOSContentAreaEvent(*event);
  if (content_area_event.has_value()) {
    passkey_tab_helper->LogEvent(*content_area_event,
                                 ParsePasskeyRequestMediation(dict));
  }

  if (IsLogOnlyEvent(*event)) {
    return;
  }

  if (!base::FeatureList::IsEnabled(kIOSPasskeyModalLoginWithShim) &&
      !base::FeatureList::IsEnabled(kIOSPasskeyConditionalLoginWithShim)) {
    // TODO(crbug.com/369629469): Log metrics for unexpected events.
    return;
  }

  switch (*event) {
    case PasskeyScriptEvent::kLogGetRequest:
    case PasskeyScriptEvent::kLogCreateRequest:
    case PasskeyScriptEvent::kLogGetResolvedGpm:
    case PasskeyScriptEvent::kLogGetResolvedNonGpm:
    case PasskeyScriptEvent::kLogCreateResolvedGpm:
    case PasskeyScriptEvent::kLogCreateResolvedNonGpm:
      return;
    case PasskeyScriptEvent::kSignalUnknownCredential:
      if (auto params = BuildSignalUnknownCredentialParams(dict)) {
        passkey_tab_helper->HandleSignalUnknownCredentialEvent(
            message.security_origin(), *std::move(params));
      }
      return;
    case PasskeyScriptEvent::kSignalCurrentUserDetails:
      if (auto params = BuildSignalCurrentUserDetailsParams(dict)) {
        passkey_tab_helper->HandleSignalCurrentUserDetailsEvent(
            message.security_origin(), *std::move(params));
      }
      return;
    case PasskeyScriptEvent::kSignalAllAcceptedCredentials:
      if (auto params = BuildSignalAllAcceptedCredentialsParams(dict)) {
        passkey_tab_helper->HandleSignalAllAcceptedCredentialsEvent(
            message.security_origin(), *std::move(params));
      }
      return;
    case PasskeyScriptEvent::kCancelRequest:
    case PasskeyScriptEvent::kCleanupRequest:
      if (auto request_info = ParseRequestInfo(dict)) {
        passkey_tab_helper->HandleCancelRequestEvent(std::move(*request_info));
      }
      return;
    case PasskeyScriptEvent::kHandleCreateRequest: {
      auto request_info = ParseRequestInfo(dict);
      if (!request_info.has_value()) {
        return;
      }

      auto registration_request_params =
          BuildRegistrationRequestParams(*request_info, dict);
      if (!registration_request_params.has_value()) {
        base::UmaHistogramEnumeration(kPasskeyParsingErrorHistogram,
                                      registration_request_params.error());
        passkey_tab_helper->DeferToRenderer(
            std::move(*request_info),
            PasskeyRequestParams::RequestType::kUnknown);
        return;
      }

      if (!ValidateFeatureUsageForCreate(*registration_request_params,
                                         message.is_user_interacting(),
                                         passkey_tab_helper)) {
        // TODO(crbug.com/460485333): Log the error.
        passkey_tab_helper->DeferToRenderer(
            std::move(*request_info), registration_request_params->Type());
        return;
      }

      passkey_tab_helper->HandleCreateRequestedEvent(
          std::move(*registration_request_params));
      return;
    }
    case PasskeyScriptEvent::kHandleGetRequest: {
      auto request_info = ParseRequestInfo(dict);
      if (!request_info.has_value()) {
        return;
      }

      auto assertion_request_params =
          BuildAssertionRequestParams(*request_info, dict);
      if (!assertion_request_params.has_value()) {
        base::UmaHistogramEnumeration(kPasskeyParsingErrorHistogram,
                                      assertion_request_params.error());
        passkey_tab_helper->DeferToRenderer(
            std::move(*request_info),
            PasskeyRequestParams::RequestType::kUnknown);
        return;
      }

      if (!ValidateFeatureUsageForGet(*assertion_request_params,
                                      message.is_user_interacting())) {
        // TODO(crbug.com/460485333): Log the error.
        passkey_tab_helper->DeferToRenderer(std::move(*request_info),
                                            assertion_request_params->Type());
        return;
      }

      passkey_tab_helper->HandleGetRequestedEvent(
          std::move(*assertion_request_params));
      return;
    }
  }
}

}  // namespace webauthn
