// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/webauthn/ios/passkey_tab_helper.h"

#import "base/base64.h"
#import "base/base64url.h"
#import "base/check_deref.h"
#import "base/debug/dump_without_crashing.h"
#import "base/metrics/histogram_functions.h"
#import "base/notreached.h"
#import "components/password_manager/core/browser/passkey_credential.h"
#import "components/password_manager/ios/ios_password_manager_driver_factory.h"
#import "components/webauthn/core/browser/client_data_json.h"
#import "components/webauthn/core/browser/passkey_model_utils.h"
#import "components/webauthn/ios/ios_webauthn_credentials_delegate.h"
#import "components/webauthn/ios/passkey_java_script_feature.h"
#import "crypto/hash.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"

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

bool IsOriginValidForRelyingPartyId(const url::Origin& origin,
                                    const std::string& rp_id) {
  if (rp_id.empty()) {
    return false;
  }

  // TODO(crbug.com/460485600): Implement proper rp_id/origin validation.
  //                            See content related origin implementation:
  // https://chromium-review.googlesource.com/c/chromium/src/+/4973980
  return true;
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
    const passkey_model_utils::ExtensionInputData& extension_input_data,
    passkey_model_utils::ExtensionOutputData* extension_output_data) {
  auto [passkey, public_key_spki_der] =
      passkey_model_utils::GeneratePasskeyAndEncryptSecrets(
          rp_id, user_entity, trusted_vault_key,
          /*trusted_vault_key_version=*/0, extension_input_data,
          extension_output_data);

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
              std::move(public_key_spki_der), std::move(client_data_json))};
}

// Utility function to create an assertion object from the provided parameters.
// TODO(crbug.com/460485333): Merge this code with PerformPasskeyAssertion.
std::optional<PasskeyJavaScriptFeature::AssertionData> CreateAssertionObject(
    const SharedKey& trusted_vault_key,
    const sync_pb::WebauthnCredentialSpecifics& passkey,
    std::string client_data_json,
    std::string_view rp_id) {
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

  const std::string& user_id = passkey.user_id();
  return PasskeyJavaScriptFeature::AssertionData(
      std::move(*signature), std::move(authenticator_data),
      std::vector<uint8_t>(user_id.begin(), user_id.end()),
      std::move(client_data_json));
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
  if (params.RequestId().empty()) {
    return;
  }

  web::WebFrame* web_frame = GetWebFrame(params);
  if (!web_frame) {
    return;
  }

  if (!IsOriginValidForRelyingPartyId(web_frame->GetSecurityOrigin(),
                                      params.RpId())) {
    DeferToRenderer(web_frame, params);
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

  // Send available passkeys to the WebAuthnCredentialsDelegate.
  delegate->OnCredentialsReceived(std::move(filtered_passkeys));

  // Open the suggestion bottom sheet. The delegate's suggestions will be
  // presented in it and will be selectable by the user.
  std::string request_id = params.RequestId();
  assertion_requests_.emplace(request_id, std::move(params));
  client_->ShowSuggestionBottomSheet({std::move(request_id), params.FrameId()});
}

void PasskeyTabHelper::HandleCreateRequestedEvent(
    RegistrationRequestParams params) {
  // If the request ID is invalid, the request can't be processed.
  if (params.RequestId().empty()) {
    return;
  }

  web::WebFrame* web_frame = GetWebFrame(params);
  if (!web_frame) {
    return;
  }

  if (!IsOriginValidForRelyingPartyId(web_frame->GetSecurityOrigin(),
                                      params.RpId()) ||
      HasExcludedPasskey(params)) {
    DeferToRenderer(web_frame, params);
    return;
  }

  // Open the creation confirmation bottom sheet. A passkey will end up being
  // created by PasskeyTabHelper::StartPasskeyCreation() upon confirmation by
  // the user.
  std::string request_id = params.RequestId();
  registration_requests_.emplace(request_id, std::move(params));
  client_->ShowCreationBottomSheet({std::move(request_id), params.FrameId()});
}

bool PasskeyTabHelper::HasCredential(const std::string& rp_id,
                                     const std::string& credential_id) const {
  return passkey_model_->GetPasskeyByCredentialId(rp_id, credential_id)
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
}

web::WebFrame* PasskeyTabHelper::GetWebFrame(
    const PasskeyRequestParams& request_params) const {
  web::WebState* web_state = web_state_.get();
  if (!web_state) {
    return nullptr;
  }

  web::WebFramesManager* web_frames_manager =
      PasskeyJavaScriptFeature::GetInstance()->GetWebFramesManager(web_state);

  return web_frames_manager
             ? web_frames_manager->GetFrameWithId(request_params.FrameId())
             : nullptr;
}

bool PasskeyTabHelper::HasExcludedPasskey(
    const RegistrationRequestParams& params) const {
  std::set<std::string> exclude_credentials = params.GetExcludeCredentialIds();
  if (exclude_credentials.empty()) {
    return false;
  }

  std::vector<sync_pb::WebauthnCredentialSpecifics> passkeys =
      passkey_model_->GetPasskeysForRelyingPartyId(params.RpId());
  for (const auto& passkey : passkeys) {
    if (exclude_credentials.contains(passkey.credential_id())) {
      return true;
    }
  }
  return false;
}

std::vector<sync_pb::WebauthnCredentialSpecifics>
PasskeyTabHelper::GetFilteredPasskeys(
    const AssertionRequestParams& params) const {
  std::vector<sync_pb::WebauthnCredentialSpecifics> passkeys =
      passkey_model_->GetPasskeysForRelyingPartyId(params.RpId());
  if (passkeys.empty()) {
    return passkeys;
  }

  // If the allowed credentials array is empty, then the relying party accepts
  // any passkey credential.
  std::set<std::string> allow_credentials = params.GetAllowCredentialIds();
  if (allow_credentials.empty()) {
    return passkeys;
  }

  std::erase_if(passkeys, [&](sync_pb::WebauthnCredentialSpecifics cred) {
    return !allow_credentials.contains(cred.credential_id());
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
  web::WebFrame* web_frame = GetWebFrame(params);
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

void PasskeyTabHelper::DeferToRenderer(
    web::WebFrame* web_frame,
    const PasskeyRequestParams& request_params) const {
  PasskeyJavaScriptFeature::GetInstance()->DeferToRenderer(
      web_frame, request_params.RequestId());
}

void PasskeyTabHelper::CompletePasskeyCreation(
    RegistrationRequestParams params,
    std::string client_data_json,
    const SharedKeyList& shared_key_list) {
  web::WebFrame* web_frame = GetWebFrame(params);
  if (!web_frame) {
    return;
  }

  // `hw_protected` security domain currently supports a single secret.
  if (shared_key_list.size() != 1) {
    DeferToRenderer(web_frame, params);
    return;
  }

  // TODO(crbug.com/460485679) : Implement extension support.
  passkey_model_utils::ExtensionInputData extension_input_data;
  passkey_model_utils::ExtensionOutputData extension_output_data;

  // Create passkey and attestation object.
  auto [passkey, attestation_data] = CreatePasskeyAndAttestationObject(
      shared_key_list[0], std::move(client_data_json), params.RpId(),
      params.UserEntity(), extension_input_data, &extension_output_data);

  // Add passkey to the passkey model and present the confirmation infobar.
  // TODO(crbug.com/460485333): Wait until success message from TypeScript code?
  AddNewPasskey(passkey);

  // Resolve the PublicKeyCredential promise.
  const std::string& credential_id = passkey.credential_id();
  PasskeyJavaScriptFeature::GetInstance()->ResolveAttestationRequest(
      web_frame, params.RequestId(), credential_id,
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
  web::WebFrame* web_frame = GetWebFrame(params);
  if (!web_frame) {
    return;
  }

  std::optional<sync_pb::WebauthnCredentialSpecifics> passkey =
      FindPasskey(GetFilteredPasskeys(params), std::move(credential_id));
  if (!passkey.has_value()) {
    DeferToRenderer(web_frame, params);
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

void PasskeyTabHelper::CompletePasskeyAssertion(
    AssertionRequestParams params,
    sync_pb::WebauthnCredentialSpecifics passkey,
    std::string client_data_json,
    const SharedKeyList& shared_key_list) {
  web::WebFrame* web_frame = GetWebFrame(params);
  if (!web_frame) {
    return;
  }

  // `hw_protected` security domain currently supports a single secret.
  if (shared_key_list.size() != 1) {
    DeferToRenderer(web_frame, params);
    return;
  }

  // TODO(crbug.com/460485679) : Implement extension support.

  // Attempt to create an assertion object.
  std::optional<PasskeyJavaScriptFeature::AssertionData> assertion_data =
      CreateAssertionObject(shared_key_list[0], passkey,
                            std::move(client_data_json), params.RpId());

  // TODO(crbug.com/460485333): Update the passkey's last used time to
  // base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds().
  // TODO(crbug.com/460485333): Wait until success message from TypeScript code?

  if (assertion_data.has_value()) {
    // Resolve the PublicKeyCredential promise.
    const std::string& credential_id = passkey.credential_id();
    PasskeyJavaScriptFeature::GetInstance()->ResolveAssertionRequest(
        web_frame, params.RequestId(), credential_id,
        std::move(*assertion_data));
  } else {
    DeferToRenderer(web_frame, params);
  }
}

// WebStateObserver

void PasskeyTabHelper::WebStateDestroyed(web::WebState* web_state) {
  web_state->RemoveObserver(this);
}

base::WeakPtr<PasskeyTabHelper> PasskeyTabHelper::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace webauthn
