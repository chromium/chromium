// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/webauthn/ios/passkey_tab_helper.h"

#import "base/base64.h"
#import "base/base64url.h"
#import "base/check_deref.h"
#import "base/metrics/histogram_functions.h"
#import "base/notreached.h"
#import "components/password_manager/ios/ios_password_manager_driver_factory.h"
#import "components/webauthn/core/browser/client_data_json.h"
#import "components/webauthn/core/browser/passkey_model_utils.h"
#import "components/webauthn/ios/passkey_java_script_feature.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"

namespace webauthn {

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

// Returns a set of credential ids from a vector of credential descriptors.
std::set<std::string> GetIdsFromDescriptors(
    const std::vector<device::PublicKeyCredentialDescriptor>& descriptors) {
  std::set<std::string> descriptor_ids;
  std::transform(descriptors.begin(), descriptors.end(),
                 std::inserter(descriptor_ids, descriptor_ids.begin()),
                 [](const device::PublicKeyCredentialDescriptor& desc) {
                   return std::string(desc.id.begin(), desc.id.end());
                 });
  return descriptor_ids;
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

PasskeyTabHelper::RequestParams::RequestParams()
    : user_verification_(device::UserVerificationRequirement::kPreferred) {}

PasskeyTabHelper::RequestParams::RequestParams(
    const std::string& frame_id,
    device::PublicKeyCredentialRpEntity rp_entity,
    std::vector<uint8_t> challenge,
    device::UserVerificationRequirement user_verification)
    : frame_id_(frame_id),
      rp_entity_(std::move(rp_entity)),
      challenge_(std::move(challenge)),
      user_verification_(user_verification) {}

PasskeyTabHelper::RequestParams::RequestParams(
    PasskeyTabHelper::RequestParams&& other) = default;

PasskeyTabHelper::RequestParams::~RequestParams() {}

PasskeyTabHelper::AssertionRequestParams::AssertionRequestParams(
    RequestParams request_params,
    std::vector<device::PublicKeyCredentialDescriptor> allow_credentials)
    : request_params_(std::move(request_params)),
      allow_credentials_(std::move(allow_credentials)) {}

PasskeyTabHelper::AssertionRequestParams::AssertionRequestParams(
    PasskeyTabHelper::AssertionRequestParams&& other) = default;

const std::set<std::string>
PasskeyTabHelper::AssertionRequestParams::GetAllowCredentialIds() const {
  return GetIdsFromDescriptors(allow_credentials_);
}

const std::string& PasskeyTabHelper::AssertionRequestParams::RpId() const {
  return request_params_.rp_entity_.id;
}

PasskeyTabHelper::AssertionRequestParams::~AssertionRequestParams() {}

PasskeyTabHelper::RegistrationRequestParams::RegistrationRequestParams(
    RequestParams request_params,
    device::PublicKeyCredentialUserEntity user_entity,
    std::vector<device::PublicKeyCredentialDescriptor> exclude_credentials)
    : request_params_(std::move(request_params)),
      user_entity_(std::move(user_entity)),
      exclude_credentials_(std::move(exclude_credentials)) {}

PasskeyTabHelper::RegistrationRequestParams::RegistrationRequestParams(
    PasskeyTabHelper::RegistrationRequestParams&& other) = default;

const std::set<std::string>
PasskeyTabHelper::RegistrationRequestParams::GetExcludeCredentialIds() const {
  return GetIdsFromDescriptors(exclude_credentials_);
}

const std::string& PasskeyTabHelper::RegistrationRequestParams::RpId() const {
  return request_params_.rp_entity_.id;
}

PasskeyModel::UserEntity
PasskeyTabHelper::RegistrationRequestParams::UserEntity() const {
  return PasskeyModel::UserEntity(user_entity_.id,
                                  user_entity_.name.value_or(""),
                                  user_entity_.display_name.value_or(""));
}

PasskeyTabHelper::RegistrationRequestParams::~RegistrationRequestParams() {}

PasskeyTabHelper::~PasskeyTabHelper() = default;

void PasskeyTabHelper::LogEvent(
    WebAuthenticationIOSContentAreaEvent event_type) {
  base::UmaHistogramEnumeration(kWebAuthenticationIOSContentAreaEventHistogram,
                                event_type);
}

void PasskeyTabHelper::HandleGetRequestedEvent(AssertionRequestParams params) {
  web::WebFrame* web_frame = GetWebFrame(params.request_params_);
  if (!web_frame) {
    return;
  }

  if (!IsOriginValidForRelyingPartyId(web_frame->GetSecurityOrigin(),
                                      params.RpId())) {
    PasskeyJavaScriptFeature::GetInstance()->DeferToRenderer(web_frame);
    return;
  }

  IOSPasswordManagerDriver* driver =
      IOSPasswordManagerDriverFactory::FromWebStateAndWebFrame(web_state_.get(),
                                                               web_frame);
  CHECK(driver);

  password_manager::WebAuthnCredentialsDelegate* delegate =
      client_->GetWebAuthnCredentialsDelegateForDriver(driver);
  if (delegate) {
    // TODO(crbug.com/462121114): Pass filtered passkeys to the delegate.
  }

  // TODO(crbug.com/460485333): Handle this event.
  PasskeyJavaScriptFeature::GetInstance()->DeferToRenderer(web_frame);
}

void PasskeyTabHelper::HandleCreateRequestedEvent(
    RegistrationRequestParams params) {
  web::WebFrame* web_frame = GetWebFrame(params.request_params_);
  if (!web_frame) {
    return;
  }

  if (!IsOriginValidForRelyingPartyId(web_frame->GetSecurityOrigin(),
                                      params.RpId()) ||
      HasExcludedPasskey(params)) {
    PasskeyJavaScriptFeature::GetInstance()->DeferToRenderer(web_frame);
    return;
  }

  // TODO(crbug.com/460485333): Handle this event.
  PasskeyJavaScriptFeature::GetInstance()->DeferToRenderer(web_frame);
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
    const RequestParams& request_params) const {
  web::WebState* web_state = web_state_.get();
  if (!web_state) {
    return nullptr;
  }

  web::WebFramesManager* web_frames_manager =
      PasskeyJavaScriptFeature::GetInstance()->GetWebFramesManager(web_state);

  return web_frames_manager
             ? web_frames_manager->GetFrameWithId(request_params.frame_id_)
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
  passkey_model_->CreatePasskey(passkey);
}

void PasskeyTabHelper::StartPasskeyCreation(RegistrationRequestParams params) {
  web::WebFrame* web_frame = GetWebFrame(params.request_params_);
  if (!web_frame) {
    return;
  }

  // TODO(crbug.com/460485333): Use proper top origin.
  std::string client_data_json = BuildClientDataJson(
      {ClientDataRequestType::kWebAuthnCreate, web_frame->GetSecurityOrigin(),
       /*top_origin=*/url::Origin(), params.request_params_.challenge_,
       /*is_cross_origin_iframe=*/false},
      /*payment_json=*/std::nullopt);

  if (client_data_json.empty()) {
    PasskeyJavaScriptFeature::GetInstance()->DeferToRenderer(web_frame);
    return;
  }

  client_->FetchKeys(ReauthenticatePurpose::kEncrypt,
                     base::BindOnce(&PasskeyTabHelper::CompletePasskeyCreation,
                                    this->AsWeakPtr(), std::move(params),
                                    std::move(client_data_json)));
}

void PasskeyTabHelper::CompletePasskeyCreation(
    RegistrationRequestParams params,
    std::string client_data_json,
    const SharedKeyList& shared_key_list) {
  web::WebFrame* web_frame = GetWebFrame(params.request_params_);
  if (!web_frame) {
    return;
  }

  // `hw_protected` security domain currently supports a single secret.
  if (shared_key_list.size() != 1) {
    PasskeyJavaScriptFeature::GetInstance()->DeferToRenderer(web_frame);
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
  AddNewPasskey(passkey);

  // Resolve the PublicKeyCredential promise.
  PasskeyJavaScriptFeature::GetInstance()->ResolveAttestationRequest(
      web_frame, passkey.credential_id(), std::move(attestation_data));
}

// WebStateObserver

void PasskeyTabHelper::WebStateDestroyed(web::WebState* web_state) {
  web_state->RemoveObserver(this);
}

base::WeakPtr<PasskeyTabHelper> PasskeyTabHelper::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace webauthn
