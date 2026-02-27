// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/webauthn/ios/passkey_tab_helper.h"

#import "base/check_deref.h"
#import "base/debug/dump_without_crashing.h"
#import "base/metrics/histogram_functions.h"
#import "base/notreached.h"
#import "components/password_manager/core/browser/passkey_credential.h"
#import "components/password_manager/ios/ios_password_manager_driver_factory.h"
#import "components/webauthn/core/browser/client_data_json.h"
#import "components/webauthn/core/browser/common_utils.h"
#import "components/webauthn/core/browser/passkey_model.h"
#import "components/webauthn/core/browser/passkey_model_utils.h"
#import "components/webauthn/core/browser/remote_validation.h"
#import "components/webauthn/core/browser/webauthn_security_utils.h"
#import "components/webauthn/ios/ios_webauthn_credentials_delegate.h"
#import "components/webauthn/ios/passkey_java_script_feature.h"
#import "crypto/hash.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

namespace webauthn {

namespace {

constexpr char kWebAuthenticationIOSContentAreaEventHistogram[] =
    "WebAuthentication.IOS.ContentAreaEvent";

class [[maybe_unused, nodiscard]] ScopedAllowPasskeyCreationInfobar {
 public:
  ScopedAllowPasskeyCreationInfobar(IOSPasskeyClient* client)
      : client_(client) {
    client_->AllowPasskeyCreationInfobar(true);
  }
  ~ScopedAllowPasskeyCreationInfobar() {
    client_->AllowPasskeyCreationInfobar(false);
  }

 private:
  raw_ptr<IOSPasskeyClient> client_;
};

// Converts an std::string to a byte vector.
std::vector<uint8_t> ToByteVector(const std::string& str) {
  return std::vector<uint8_t>(str.begin(), str.end());
}

// Utility function to create a passkey and an attestation object from the
// provided parameters.
// TODO(crbug.com/460485333): Merge this code with PerformPasskeyCreation.
std::pair<sync_pb::WebauthnCredentialSpecifics,
          PasskeyJavaScriptFeature::AttestationData>
CreatePasskeyAndAttestationObject(
    const SharedKey& trusted_vault_key,
    std::string client_data_json,
    std::string_view rp_id,
    const PasskeyModel::UserEntity& user_entity,
    const passkey_model_utils::ExtensionInputData& extension_input_data) {
  passkey_model_utils::ExtensionOutputData extension_output_data;
  auto [passkey, public_key_spki_der] =
      passkey_model_utils::GeneratePasskeyAndEncryptSecrets(
          rp_id, user_entity, trusted_vault_key,
          /*trusted_vault_key_version=*/0, extension_input_data,
          &extension_output_data);

  // TODO(crbug.com/460485333): use the real value for `did_complete_uv`.
  passkey_model_utils::SerializedAttestationObject
      serialized_attestation_object =
          passkey_model_utils::MakeAttestationObjectForCreation(
              rp_id, /*did_complete_uv=*/false,
              base::as_byte_span(passkey.credential_id()), public_key_spki_der);

  return {std::move(passkey),
          PasskeyJavaScriptFeature::AttestationData(
              std::move(serialized_attestation_object.attestation_object),
              std::move(serialized_attestation_object.authenticator_data),
              std::move(public_key_spki_der), std::move(client_data_json),
              std::move(extension_output_data))};
}

// Utility function to create an assertion object from the provided parameters.
// TODO(crbug.com/460485333): Merge this code with PerformPasskeyAssertion.
std::optional<PasskeyJavaScriptFeature::AssertionData> CreateAssertionObject(
    const SharedKey& trusted_vault_key,
    const sync_pb::WebauthnCredentialSpecifics& passkey,
    std::string client_data_json,
    std::string_view rp_id,
    const passkey_model_utils::ExtensionInputData& extension_input_data) {
  // Fetch secrets from passkey if possible.
  sync_pb::WebauthnCredentialSpecifics_Encrypted credential_secrets;
  if (!passkey_model_utils::DecryptWebauthnCredentialSpecificsData(
          trusted_vault_key, passkey, &credential_secrets)) {
    return std::nullopt;
  }

  // Generate authenticator data.
  // TODO(crbug.com/460485333): use the real value for `did_complete_uv`.
  std::vector<uint8_t> authenticator_data =
      passkey_model_utils::MakeAuthenticatorDataForAssertion(
          rp_id, /*did_complete_uv=*/false);

  // Generate client data hash.
  std::array<uint8_t, crypto::hash::kSha256Size> client_data_hash =
      crypto::hash::Sha256(client_data_json);

  // Prepare the signed data.
  std::vector<uint8_t> signed_over_data = authenticator_data;
  signed_over_data.insert(signed_over_data.end(), client_data_hash.begin(),
                          client_data_hash.end());

  // Compute signature.
  std::optional<std::vector<uint8_t>> signature =
      passkey_model_utils::GenerateEcSignature(
          base::as_byte_span(credential_secrets.private_key()),
          signed_over_data);

  if (!signature.has_value()) {
    return std::nullopt;
  }

  return PasskeyJavaScriptFeature::AssertionData(
      std::move(*signature), std::move(authenticator_data),
      ToByteVector(passkey.user_id()), std::move(client_data_json),
      extension_input_data.ToOutputData(credential_secrets));
}

// Attempts to find a passkey matching the provided credential ID in a list of
// passkeys. Returns the passkey on success and std::nullopt on failure.
std::optional<sync_pb::WebauthnCredentialSpecifics> FindPasskey(
    std::vector<sync_pb::WebauthnCredentialSpecifics> passkeys,
    std::string credential_id) {
  if (passkeys.empty()) {
    return std::nullopt;
  }

  auto it = std::find_if(
      passkeys.begin(), passkeys.end(),
      [&credential_id](const sync_pb::WebauthnCredentialSpecifics& passkey) {
        return passkey.credential_id() == credential_id;
      });
  if (it == passkeys.end()) {
    return std::nullopt;
  }
  return *it;
}

}  // namespace

PasskeyTabHelper::~PasskeyTabHelper() = default;

void PasskeyTabHelper::LogEvent(
    WebAuthenticationIOSContentAreaEvent event_type) {
  base::UmaHistogramEnumeration(kWebAuthenticationIOSContentAreaEventHistogram,
                                event_type);
}

void PasskeyTabHelper::HandleGetRequestedEvent(AssertionRequestParams params) {
  // If the request is invalid, the request can't be processed.
  const IOSPasskeyClient::RequestInfo& request_info = params.RequestInfo();
  if (request_info.request_id.empty()) {
    return;
  }

  web::WebFrame* web_frame = GetWebFrame(request_info.frame_id);
  if (!web_frame) {
    // Buffer this request until the frame becomes available.
    pending_requests_by_frame_[request_info.frame_id].emplace_back(
        std::move(params));
    return;
  }

  HandleGetRequestedEvent(web_frame, std::move(params));
}

void PasskeyTabHelper::HandleGetRequestedEvent(web::WebFrame* web_frame,
                                               AssertionRequestParams params) {
  const std::string& passkey_request_id = params.RequestId();
  const PasskeyRequestParams::RequestType request_type = params.Type();
  CHECK(!passkey_request_id.empty());
  CHECK(web_frame);
  CHECK(request_type == PasskeyRequestParams::RequestType::kConditionalGet ||
        request_type == PasskeyRequestParams::RequestType::kModal);

  const url::Origin& origin = web_frame->GetSecurityOrigin();
  const std::string& rp_id = params.RpId();
  if (!OriginIsAllowedToClaimRelyingPartyId(rp_id, origin)) {
    if (!PerformRemoteRpIdValidation(
            origin, rp_id, passkey_request_id,
            base::BindOnce(&PasskeyTabHelper::OnRemoteRpIdValidationCompleted,
                           AsWeakPtr(), std::move(params)))) {
      DeferToRenderer(web_frame, passkey_request_id, request_type);
    }
    return;
  }

  HandleAssertion(std::move(params));
}

void PasskeyTabHelper::HandleAssertion(AssertionRequestParams params) {
  web::WebFrame* web_frame = GetWebFrame(params.FrameId());
  if (!web_frame) {
    return;
  }

  // Get available passkeys for the request.
  std::vector<password_manager::PasskeyCredential> filtered_passkeys =
      password_manager::PasskeyCredential::FromCredentialSpecifics(
          GetFilteredPasskeys(params));

  IOSPasswordManagerDriver* driver =
      IOSPasswordManagerDriverFactory::FromWebStateAndWebFrame(web_state_.get(),
                                                               web_frame);
  CHECK(driver);

  IOSWebAuthnCredentialsDelegate* delegate =
      static_cast<IOSWebAuthnCredentialsDelegate*>(
          client_->GetWebAuthnCredentialsDelegateForDriver(driver));
  CHECK(delegate);

  const std::string& passkey_request_id = params.RequestId();
  // Send available passkeys to the WebAuthnCredentialsDelegate.
  delegate->OnCredentialsReceived(std::move(filtered_passkeys),
                                  passkey_request_id);

  // Open the suggestion bottom sheet. The delegate's suggestions will be
  // presented in it and will be selectable by the user.
  IOSPasskeyClient::RequestInfo request_info = params.RequestInfo();
  PasskeyRequestParams::RequestType request_type = params.Type();
  CHECK(request_type == PasskeyRequestParams::RequestType::kConditionalGet ||
        request_type == PasskeyRequestParams::RequestType::kModal);

  assertion_requests_.emplace(passkey_request_id, std::move(params));
  if (request_type == PasskeyRequestParams::RequestType::kModal) {
    // On modal requests, show the passkey suggestion bottom sheet. On
    // conditional requests, the credential bottom sheet will be opened by the
    // listener on the 'webauthn' text field in AutofillBottomSheetTabHelper.
    // TODO(crbug.com/460486390): Verify if conditional assertion requests can
    // come from other sources than a 'webauthn' text field.
    client_->ShowSuggestionBottomSheet(std::move(request_info));
  }
}

bool PasskeyTabHelper::PerformRemoteRpIdValidation(
    const url::Origin& origin,
    const std::string& rp_id,
    const std::string& passkey_request_id,
    base::OnceCallback<void(ValidationStatus)> callback) {
  std::unique_ptr<RemoteValidation> loader = RemoteValidation::Create(
      origin, rp_id, web_state_->GetBrowserState()->GetSharedURLLoaderFactory(),
      std::move(callback));
  if (loader) {
    loaders_[passkey_request_id] = std::move(loader);
    return true;
  }
  return false;
}

void PasskeyTabHelper::OnRemoteRpIdValidationCompleted(
    PendingRequest request,
    ValidationStatus result) {
  const std::string& passkey_request_id = std::visit(
      [](const auto& params) { return params.RequestId(); }, request);
  loaders_.erase(passkey_request_id);

  if (std::holds_alternative<AssertionRequestParams>(request)) {
    AssertionRequestParams params =
        std::move(std::get<AssertionRequestParams>(request));
    if (result != ValidationStatus::kSuccess) {
      DeferToRenderer(params.RequestInfo(), params.Type());
      return;
    }

    HandleAssertion(std::move(params));
  } else {
    RegistrationRequestParams params =
        std::move(std::get<RegistrationRequestParams>(request));
    if (result != ValidationStatus::kSuccess) {
      DeferToRenderer(params.RequestInfo(), params.Type());
      return;
    }
    HandleRegistration(std::move(params));
  }
}

void PasskeyTabHelper::HandleCreateRequestedEvent(
    RegistrationRequestParams params) {
  // If the request ID is invalid, the request can't be processed.
  const IOSPasskeyClient::RequestInfo& request_info = params.RequestInfo();
  if (request_info.request_id.empty()) {
    return;
  }

  web::WebFrame* web_frame = GetWebFrame(request_info.frame_id);
  if (!web_frame) {
    // Buffer this request until the frame becomes available.
    pending_requests_by_frame_[request_info.frame_id].emplace_back(
        std::move(params));
    return;
  }

  HandleCreateRequestedEvent(web_frame, std::move(params));
}

void PasskeyTabHelper::HandleCreateRequestedEvent(
    web::WebFrame* web_frame,
    RegistrationRequestParams params) {
  const std::string& passkey_request_id = params.RequestId();
  const PasskeyRequestParams::RequestType request_type = params.Type();
  CHECK(!passkey_request_id.empty());
  CHECK(web_frame);
  CHECK(request_type == PasskeyRequestParams::RequestType::kConditionalCreate ||
        request_type == PasskeyRequestParams::RequestType::kModal);

  if (HasExcludedPasskey(params)) {
    DeferToRenderer(web_frame, passkey_request_id, request_type);
    return;
  }

  const url::Origin& origin = web_frame->GetSecurityOrigin();
  const std::string& rp_id = params.RpId();
  if (!OriginIsAllowedToClaimRelyingPartyId(rp_id, origin)) {
    if (!PerformRemoteRpIdValidation(
            origin, rp_id, passkey_request_id,
            base::BindOnce(&PasskeyTabHelper::OnRemoteRpIdValidationCompleted,
                           AsWeakPtr(), std::move(params)))) {
      DeferToRenderer(web_frame, passkey_request_id, request_type);
    }
    return;
  }

  HandleRegistration(std::move(params));
}

bool PasskeyTabHelper::CanPerformAutomaticPasskeyUpgrade(
    const RegistrationRequestParams& params) const {
  // TODO(crbug.com/460486709): Add a proper check similar to the
  // PasskeyUpgradeRequestController on Desktop.
  return true;
}

void PasskeyTabHelper::HandleRegistration(RegistrationRequestParams params) {
  IOSPasskeyClient::RequestInfo request_info = params.RequestInfo();

  PasskeyRequestParams::RequestType request_type = params.Type();
  CHECK(request_type == PasskeyRequestParams::RequestType::kConditionalCreate ||
        request_type == PasskeyRequestParams::RequestType::kModal);
  bool is_conditional =
      request_type == PasskeyRequestParams::RequestType::kConditionalCreate;

  bool can_upgrade = CanPerformAutomaticPasskeyUpgrade(params);
  if (is_conditional && !can_upgrade) {
    // Automatic passkey upgrade is not allowed, defer to renderer.
    DeferToRenderer(std::move(request_info), request_type);
    return;
  }

  const std::string& passkey_request_id = params.RequestId();
  registration_requests_.emplace(passkey_request_id, std::move(params));

  if (is_conditional) {
    // Automatic passkey upgrade is allowed, create a passkey.
    CHECK(can_upgrade);
    StartPasskeyCreation(passkey_request_id);
  } else {
    // Open the creation confirmation bottom sheet. A passkey will end up being
    // created by StartPasskeyCreation() below upon confirmation by the user.
    client_->ShowCreationBottomSheet(std::move(request_info));
  }
}

bool PasskeyTabHelper::HasPendingValidationForTesting() const {
  return !loaders_.empty();
}

bool PasskeyTabHelper::HasCredential(const std::string& rp_id,
                                     const std::string& credential_id) const {
  return passkey_model_
      ->GetPasskey(rp_id, credential_id,
                   webauthn::PasskeyModel::ShadowedCredentials::kExclude)
      .has_value();
}

PasskeyTabHelper::PasskeyTabHelper(web::WebState* web_state,
                                   PasskeyModel* passkey_model,
                                   std::unique_ptr<IOSPasskeyClient> client)
    : passkey_model_(CHECK_DEREF(passkey_model)),
      web_state_(web_state->GetWeakPtr()),
      client_(std::move(client)) {
  CHECK(client_);
  web_state->AddObserver(this);

  // Observe WebFramesManager to be notified when frames become available.
  if (web::WebFramesManager* web_frames_manager =
          PasskeyJavaScriptFeature::GetInstance()->GetWebFramesManager(
              web_state)) {
    web_frames_manager->AddObserver(this);
  }
}

void PasskeyTabHelper::SetIOSPasskeyClientCommandsHandler(
    id<IOSPasskeyClientCommands> handler) {
  client_->SetIOSPasskeyClientCommandsHandler(handler);
}

web::WebFrame* PasskeyTabHelper::GetWebFrame(
    const std::string& frame_id) const {
  web::WebState* web_state = web_state_.get();
  if (!web_state) {
    return nullptr;
  }

  web::WebFramesManager* web_frames_manager =
      PasskeyJavaScriptFeature::GetInstance()->GetWebFramesManager(web_state);

  return web_frames_manager ? web_frames_manager->GetFrameWithId(frame_id)
                            : nullptr;
}

bool PasskeyTabHelper::HasExcludedPasskey(
    const RegistrationRequestParams& params) const {
  std::set<std::vector<uint8_t>> exclude_credentials =
      params.GetExcludeCredentialIds();
  if (exclude_credentials.empty()) {
    return false;
  }

  std::vector<sync_pb::WebauthnCredentialSpecifics> passkeys =
      passkey_model_->GetPasskeys(params.RpId(),
                                  PasskeyModel::ShadowedCredentials::kExclude);
  for (const auto& passkey : passkeys) {
    if (exclude_credentials.contains(ToByteVector(passkey.credential_id()))) {
      return true;
    }
  }
  return false;
}

std::vector<sync_pb::WebauthnCredentialSpecifics>
PasskeyTabHelper::GetFilteredPasskeys(
    const AssertionRequestParams& params) const {
  std::vector<sync_pb::WebauthnCredentialSpecifics> passkeys =
      passkey_model_->GetPasskeys(params.RpId(),
                                  PasskeyModel::ShadowedCredentials::kExclude);
  if (passkeys.empty()) {
    return passkeys;
  }

  // If the allowed credentials array is empty, then the relying party accepts
  // any passkey credential.
  std::set<std::vector<uint8_t>> allow_credentials =
      params.GetAllowCredentialIds();
  if (allow_credentials.empty()) {
    return passkeys;
  }

  std::erase_if(passkeys, [&](sync_pb::WebauthnCredentialSpecifics cred) {
    return !allow_credentials.contains(ToByteVector(cred.credential_id()));
  });

  return passkeys;
}

void PasskeyTabHelper::AddNewPasskey(
    sync_pb::WebauthnCredentialSpecifics& passkey) {
  ScopedAllowPasskeyCreationInfobar scopedAllowPasskeyCreationInfobar(
      client_.get());
  CHECK(passkey_model_utils::IsGpmPasskeyValid(passkey));
  passkey_model_->CreatePasskey(passkey);
}

std::optional<AssertionRequestParams>
PasskeyTabHelper::ExtractParamsFromAssertionRequestsMap(
    std::string request_id) {
  // Get parameters and remove the entry from the map.
  auto params_handle = assertion_requests_.extract(request_id);
  if (params_handle) {
    return std::move(params_handle.mapped());
  }

  // Passkey request not found. The UI should never be requesting passkey
  // assertion for the same passkey request ID twice.
  base::debug::DumpWithoutCrashing();
  return std::nullopt;
}

std::optional<RegistrationRequestParams>
PasskeyTabHelper::ExtractParamsFromRegistrationRequestsMap(
    std::string request_id) {
  // Get parameters and remove the entry from the map.
  auto params_handle = registration_requests_.extract(request_id);
  if (params_handle) {
    return std::move(params_handle.mapped());
  }

  // Passkey request not found. The UI should never be requesting passkey
  // creation for the same passkey request ID twice.
  base::debug::DumpWithoutCrashing();
  return std::nullopt;
}

void PasskeyTabHelper::StartPasskeyCreation(std::string request_id) {
  std::optional<RegistrationRequestParams> optional_params =
      ExtractParamsFromRegistrationRequestsMap(request_id);
  if (!optional_params.has_value()) {
    // Passkey request not found.
    return;
  }

  RegistrationRequestParams params = std::move(*optional_params);
  web::WebFrame* web_frame = GetWebFrame(params.FrameId());
  if (!web_frame) {
    return;
  }

  // TODO(crbug.com/460485333): Use proper top origin.
  std::string client_data_json = BuildClientDataJson(
      {ClientDataRequestType::kWebAuthnCreate, web_frame->GetSecurityOrigin(),
       /*top_origin=*/url::Origin(), params.Challenge(),
       /*is_cross_origin_iframe=*/false},
      /*payment_json=*/std::nullopt);

  client_->FetchKeys(ReauthenticatePurpose::kEncrypt,
                     base::BindOnce(&PasskeyTabHelper::CompletePasskeyCreation,
                                    this->AsWeakPtr(), std::move(params),
                                    std::move(client_data_json)));
}

std::optional<std::pair<std::string, PasskeyRequestParams::RequestType>>
PasskeyTabHelper::ExtractRequestInfo(const std::string& request_id) {
  if (registration_requests_.contains(request_id)) {
    std::optional<RegistrationRequestParams> optional_params =
        ExtractParamsFromRegistrationRequestsMap(request_id);
    if (optional_params.has_value()) {
      return std::make_pair(optional_params->FrameId(),
                            optional_params->Type());
    }
  } else if (assertion_requests_.contains(request_id)) {
    std::optional<AssertionRequestParams> optional_params =
        ExtractParamsFromAssertionRequestsMap(request_id);
    if (optional_params.has_value()) {
      return std::make_pair(optional_params->FrameId(),
                            optional_params->Type());
    }
  }

  return std::nullopt;
}

void PasskeyTabHelper::RejectPendingRequest(const std::string& request_id) {
  auto request_info = ExtractRequestInfo(request_id);

  if (!request_info.has_value()) {
    // Passkey request not found.
    return;
  }

  const std::string& frame_id = request_info->first;

  if (frame_id.empty()) {
    return;
  }

  web::WebFrame* web_frame = GetWebFrame(frame_id);
  if (!web_frame) {
    return;
  }

  RejectPasskeyRequest(web_frame, request_id);
}

void PasskeyTabHelper::RejectPasskeyRequest(web::WebFrame* web_frame,
                                            const std::string& request_id) {
  PasskeyJavaScriptFeature::GetInstance()->RejectPasskeyRequest(web_frame,
                                                                request_id);
}

void PasskeyTabHelper::DeferToRenderer(
    IOSPasskeyClient::RequestInfo request_info,
    PasskeyRequestParams::RequestType request_type) const {
  web::WebFrame* web_frame = GetWebFrame(request_info.frame_id);
  if (!web_frame) {
    return;
  }

  DeferToRenderer(web_frame, request_info.request_id, request_type);
}

void PasskeyTabHelper::DeferToRenderer(
    web::WebFrame* web_frame,
    const std::string& request_id,
    PasskeyRequestParams::RequestType request_type) const {
  PasskeyJavaScriptFeature::GetInstance()->DeferToRenderer(
      web_frame, request_id, request_type);
}

void PasskeyTabHelper::DeferPendingRequestToRenderer(
    const std::string& request_id) {
  auto request_info = ExtractRequestInfo(request_id);

  if (!request_info.has_value()) {
    // Passkey request not found.
    return;
  }

  const auto& [frame_id, request_type] = *request_info;

  if (frame_id.empty()) {
    return;
  }

  web::WebFrame* web_frame = GetWebFrame(frame_id);
  if (!web_frame) {
    return;
  }

  DeferToRenderer(web_frame, request_id, request_type);
}

std::string PasskeyTabHelper::UsernameForRequest(
    const std::string& request_id) {
  // Check registration requests
  auto registration_it = registration_requests_.find(request_id);
  if (registration_it != registration_requests_.end()) {
    return registration_it->second.UserEntity().name;
  }

  return "";
}

std::optional<bool> PasskeyTabHelper::ShouldPerformUserVerification(
    const std::string& request_id,
    bool is_biometric_authentication_enabled) const {
  auto assertion_it = assertion_requests_.find(request_id);
  if (assertion_it != assertion_requests_.end()) {
    return assertion_it->second.ShouldPerformUserVerification(
        is_biometric_authentication_enabled);
  }

  auto registration_it = registration_requests_.find(request_id);
  if (registration_it != registration_requests_.end()) {
    return registration_it->second.ShouldPerformUserVerification(
        is_biometric_authentication_enabled);
  }

  return std::nullopt;
}

// TODO(crbug.com/460485614): Handle error here or in the passkey client.
void PasskeyTabHelper::CompletePasskeyCreation(RegistrationRequestParams params,
                                               std::string client_data_json,
                                               SharedKeyList shared_key_list,
                                               NSError* error) {
  web::WebFrame* web_frame = GetWebFrame(params.FrameId());
  if (!web_frame) {
    return;
  }

  // `hw_protected` security domain currently supports a single secret.
  const std::string& passkey_request_id = params.RequestId();
  if (shared_key_list.size() != 1) {
    DeferToRenderer(web_frame, passkey_request_id, params.Type());
    return;
  }

  // Create passkey and attestation object.
  passkey_model_utils::ExtensionInputData extension_input_data =
      params.ExtensionInputForCreation();
  auto [passkey, attestation_data] = CreatePasskeyAndAttestationObject(
      shared_key_list[0], std::move(client_data_json), params.RpId(),
      params.UserEntity(), extension_input_data);

  if (!webauthn::passkey_model_utils::IsPasskeyValid(passkey)) {
    DeferToRenderer(web_frame, passkey_request_id, params.Type());
    return;
  }

  // Add passkey to the passkey model and present the confirmation infobar.
  // TODO(crbug.com/460485333): Wait until success message from TypeScript code?
  AddNewPasskey(passkey);

  // Resolve the PublicKeyCredential promise.
  const std::string& credential_id = passkey.credential_id();
  PasskeyJavaScriptFeature::GetInstance()->ResolveAttestationRequest(
      web_frame, passkey_request_id, credential_id,
      std::move(attestation_data));
}

void PasskeyTabHelper::StartPasskeyAssertion(std::string request_id,
                                             std::string credential_id) {
  std::optional<AssertionRequestParams> optional_params =
      ExtractParamsFromAssertionRequestsMap(request_id);
  if (!optional_params.has_value()) {
    // Passkey request not found.
    return;
  }

  AssertionRequestParams params = std::move(*optional_params);
  web::WebFrame* web_frame = GetWebFrame(params.FrameId());
  if (!web_frame) {
    return;
  }

  std::optional<sync_pb::WebauthnCredentialSpecifics> passkey =
      FindPasskey(GetFilteredPasskeys(params), std::move(credential_id));
  if (!passkey.has_value()) {
    DeferToRenderer(web_frame, params.RequestId(), params.Type());
    return;
  }

  // TODO(crbug.com/460485333): Use proper top origin.
  std::string client_data_json = BuildClientDataJson(
      {ClientDataRequestType::kWebAuthnGet, web_frame->GetSecurityOrigin(),
       /*top_origin=*/url::Origin(), params.Challenge(),
       /*is_cross_origin_iframe=*/false},
      /*payment_json=*/std::nullopt);

  client_->FetchKeys(
      ReauthenticatePurpose::kDecrypt,
      base::BindOnce(&PasskeyTabHelper::CompletePasskeyAssertion,
                     this->AsWeakPtr(), std::move(params), std::move(*passkey),
                     std::move(client_data_json)));
}

// TODO(crbug.com/460485614): Handle error here or in the passkey client.
void PasskeyTabHelper::CompletePasskeyAssertion(
    AssertionRequestParams params,
    sync_pb::WebauthnCredentialSpecifics passkey,
    std::string client_data_json,
    SharedKeyList shared_key_list,
    NSError* error) {
  web::WebFrame* web_frame = GetWebFrame(params.FrameId());
  if (!web_frame) {
    return;
  }

  // `hw_protected` security domain currently supports a single secret.
  const std::string& passkey_request_id = params.RequestId();
  if (shared_key_list.size() != 1) {
    DeferToRenderer(web_frame, passkey_request_id, params.Type());
    return;
  }

  // Attempt to create an assertion object.
  const std::string& credential_id = passkey.credential_id();
  passkey_model_utils::ExtensionInputData extension_input_data =
      params.ExtensionInputForCredential(ToByteVector(credential_id));
  std::optional<PasskeyJavaScriptFeature::AssertionData> assertion_data =
      CreateAssertionObject(shared_key_list[0], passkey,
                            std::move(client_data_json), params.RpId(),
                            extension_input_data);

  // TODO(crbug.com/460485333): Update the passkey's last used time to
  // base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds().
  // TODO(crbug.com/460485333): Wait until success message from TypeScript code?

  if (assertion_data.has_value()) {
    // Resolve the PublicKeyCredential promise.
    PasskeyJavaScriptFeature::GetInstance()->ResolveAssertionRequest(
        web_frame, passkey_request_id, credential_id,
        std::move(*assertion_data));
  } else {
    DeferToRenderer(web_frame, passkey_request_id, params.Type());
  }
}

// WebStateObserver

void PasskeyTabHelper::WebStateDestroyed(web::WebState* web_state) {
  web_state->RemoveObserver(this);
  if (web::WebFramesManager* web_frames_manager =
          PasskeyJavaScriptFeature::GetInstance()->GetWebFramesManager(
              web_state)) {
    web_frames_manager->RemoveObserver(this);
  }
}

// WebFramesManager::Observer

void PasskeyTabHelper::WebFrameBecameAvailable(
    web::WebFramesManager* web_frames_manager,
    web::WebFrame* web_frame) {
  if (!web_frame) {
    return;
  }
  const std::string frame_id = web_frame->GetFrameId();

  auto it = pending_requests_by_frame_.find(frame_id);
  if (it == pending_requests_by_frame_.end()) {
    return;
  }

  // Move out the pending vector to process without reentrancy issues.
  std::vector<PendingRequest> pending = std::move(it->second);
  pending_requests_by_frame_.erase(it);

  for (auto& request : pending) {
    if (std::holds_alternative<AssertionRequestParams>(request)) {
      HandleGetRequestedEvent(
          web_frame, std::move(std::get<AssertionRequestParams>(request)));
    } else {
      HandleCreateRequestedEvent(
          web_frame, std::move(std::get<RegistrationRequestParams>(request)));
    }
  }
}

base::WeakPtr<PasskeyTabHelper> PasskeyTabHelper::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

bool PasskeyTabHelper::ShowCreationInterstitialIfNecessary(
    base::OnceCallback<void(bool)> callback) {
  if (web_state_->GetBrowserState()->IsOffTheRecord()) {
    LogEvent(WebAuthenticationIOSContentAreaEvent::kIncognitoInterstitialShown);
    client_->ShowInterstitial(std::move(callback));
    return true;
  }
  return false;
}

}  // namespace webauthn
