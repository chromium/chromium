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
  std::string output;
  // Omit padding, according to the spec. See:
  // https://w3c.github.io/webauthn/#base64url-encoding
  base::Base64UrlEncode(input, base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &output);
  return output;
}

// Encodes a string to a base 64 URL encoded string.
std::string Base64UrlEncode(std::string_view input) {
  std::string output;
  // Omit padding, according to the spec. See:
  // https://w3c.github.io/webauthn/#base64url-encoding
  base::Base64UrlEncode(input, base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &output);
  return output;
}

bool ValidateFeatureUsage(const PasskeyRequestParams& request_params) {
  if (request_params.Type() == PasskeyRequestParams::RequestType::kModal) {
    return base::FeatureList::IsEnabled(kIOSPasskeyModalLoginWithShim);
  } else {
    return base::FeatureList::IsEnabled(kIOSPasskeyConditionalLoginWithShim);
  }
}

// Helper to determine the logging event type based on the parsed script event.
WebAuthenticationIOSContentAreaEvent ToWebAuthenticationIOSContentAreaEvent(
    PasskeyScriptEvent event) {
  switch (event) {
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

void PasskeyJavaScriptFeature::RejectPasskeyRequest(
    web::WebFrame* web_frame,
    std::string_view request_id) {
  CallJavaScriptFunction(web_frame, "passkey.rejectPasskeyRequest",
                         base::ListValue().Append(request_id));
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
  base::Value* body = message.body();
  if (!body || !body->is_dict()) {
    return;
  }

  const base::DictValue& dict = body->GetDict();

  std::optional<PasskeyScriptEvent> event = ParsePasskeyScriptEvent(
      dict, [passkey_tab_helper](const std::string& rp_id,
                                 const std::string& credential_id) {
        return passkey_tab_helper->HasCredential(rp_id, credential_id);
      });

  if (!event.has_value()) {
    // TODO(crbug.com/460485333): Log parsing failure metrics.
    return;
  }

  passkey_tab_helper->LogEvent(ToWebAuthenticationIOSContentAreaEvent(*event));

  bool is_handle_get_request_event =
      (*event == PasskeyScriptEvent::kHandleGetRequest);
  bool is_handle_create_request_event =
      (*event == PasskeyScriptEvent::kHandleCreateRequest);

  if (!is_handle_get_request_event && !is_handle_create_request_event) {
    return;
  }

  if (!base::FeatureList::IsEnabled(kIOSPasskeyModalLoginWithShim) &&
      !base::FeatureList::IsEnabled(kIOSPasskeyConditionalLoginWithShim)) {
    // TODO(crbug.com/369629469): Log metrics for unexpected events.
    return;
  }

  auto request_info = BuildRequestInfo(dict);
  if (!request_info.has_value()) {
    base::UmaHistogramEnumeration("WebAuthentication.IOS.PasskeyParsingError",
                                  request_info.error());
    return;
  }

  if (is_handle_create_request_event) {
    // base::Unretained is safe because this is a singleton.
    if (passkey_tab_helper->ShowCreationInterstitialIfNecessary(
            base::BindOnce(&PasskeyJavaScriptFeature::OnInterstitialDecision,
                           base::Unretained(this), web_state->GetWeakPtr(),
                           *request_info, dict.Clone()))) {
      return;
    }

    ProcessCreateRequest(web_state, std::move(*request_info), dict.Clone());
    return;
  }

  if (is_handle_get_request_event) {
    auto assertion_request_params =
        BuildAssertionRequestParams(*request_info, dict);
    if (!assertion_request_params.has_value()) {
      base::UmaHistogramEnumeration("WebAuthentication.IOS.PasskeyParsingError",
                                    assertion_request_params.error());
      passkey_tab_helper->DeferToRenderer(
          std::move(*request_info),
          PasskeyRequestParams::RequestType::kUnknown);
      return;
    }

    if (!ValidateFeatureUsage(*assertion_request_params)) {
      // TODO(460485333): Log the error.
      passkey_tab_helper->DeferToRenderer(std::move(*request_info),
                                          assertion_request_params->Type());
      return;
    }

    passkey_tab_helper->HandleGetRequestedEvent(
        std::move(*assertion_request_params));
  }
}

void PasskeyJavaScriptFeature::OnInterstitialDecision(
    base::WeakPtr<web::WebState> web_state,
    IOSPasskeyClient::RequestInfo request_info,
    base::DictValue dict,
    bool proceed) {
  if (!web_state) {
    return;
  }

  if (!proceed) {
    web::WebFramesManager* frames_manager =
        GetWebFramesManager(web_state.get());
    web::WebFrame* frame =
        frames_manager->GetFrameWithId(request_info.frame_id);
    if (frame) {
      RejectPasskeyRequest(frame, request_info.request_id);
    }
    return;
  }

  ProcessCreateRequest(web_state.get(), std::move(request_info),
                       std::move(dict));
}

void PasskeyJavaScriptFeature::ProcessCreateRequest(
    web::WebState* web_state,
    IOSPasskeyClient::RequestInfo request_info,
    base::DictValue dict) {
  PasskeyTabHelper* passkey_tab_helper =
      PasskeyTabHelper::FromWebState(web_state);
  if (!passkey_tab_helper) {
    return;
  }

  auto registration_request_params =
      BuildRegistrationRequestParams(request_info, dict);

  if (!registration_request_params.has_value()) {
    base::UmaHistogramEnumeration("WebAuthentication.IOS.PasskeyParsingError",
                                  registration_request_params.error());
    passkey_tab_helper->DeferToRenderer(
        std::move(request_info), PasskeyRequestParams::RequestType::kUnknown);
    return;
  }

  if (!ValidateFeatureUsage(*registration_request_params)) {
    // TODO(460485333): Log the error.
    passkey_tab_helper->DeferToRenderer(std::move(request_info),
                                        registration_request_params->Type());
    return;
  }

  passkey_tab_helper->HandleCreateRequestedEvent(
      std::move(*registration_request_params));
}

}  // namespace webauthn
