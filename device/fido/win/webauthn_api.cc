// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/win/webauthn_api.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util_win.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/scoped_thread_priority.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/features.h"
#include "device/fido/fido_types.h"
#include "device/fido/win/logging.h"
#include "device/fido/win/type_conversions.h"
#include "third_party/microsoft_webauthn/webauthn.h"

namespace device {

namespace {

WinWebAuthnApi* g_api_override = nullptr;

// Time out all Windows API requests after 5 minutes. We maintain our own
// timeout and cancel the operation when it expires, so this value simply needs
// to be larger than the largest internal request timeout.
constexpr uint32_t kWinWebAuthnTimeoutMilliseconds = 1000 * 60 * 5;

std::string HresultToHex(HRESULT hr) {
  return base::StringPrintf("0x%0lX", hr);
}

// FillHMACSalts converts `input` to the Windows representation of a pair of
// HMAC salt values, using `salts_storage` to own the returned pointer.
WEBAUTHN_HMAC_SECRET_SALT* FillHMACSalts(
    std::vector<WEBAUTHN_HMAC_SECRET_SALT>* salts_storage,
    const PRFInput& input) {
  const WEBAUTHN_HMAC_SECRET_SALT salts{
      base::checked_cast<DWORD>(input.salt1.size()),
      const_cast<PBYTE>(input.salt1.data()),
      input.salt2.has_value() ? base::checked_cast<DWORD>(input.salt2->size())
                              : 0,
      input.salt2.has_value() ? const_cast<PBYTE>(input.salt2->data())
                              : nullptr,
  };
  salts_storage->push_back(salts);
  return &salts_storage->back();
}

// FillHMACSaltValues converts `inputs` to the Windows representation of the
// PRF inputs and uses the `*_storage` arguments to own the returned structures.
WEBAUTHN_HMAC_SECRET_SALT_VALUES* FillHMACSaltValues(
    WEBAUTHN_HMAC_SECRET_SALT_VALUES* values_storage,
    std::vector<WEBAUTHN_HMAC_SECRET_SALT>* salts_storage,
    std::vector<WEBAUTHN_CRED_WITH_HMAC_SECRET_SALT>* cred_salts_storage,
    const std::vector<PRFInput>& inputs) {
  if (inputs.empty()) {
    return nullptr;
  }

  memset(values_storage, 0, sizeof(*values_storage));
  // These vectors must not reallocate because the Windows structures will have
  // pointers into their elements.
  salts_storage->reserve(inputs.size());
  cred_salts_storage->reserve(inputs.size());

  for (const auto& input : inputs) {
    if (!input.credential_id.has_value()) {
      // Only the first input may omit the credential ID.
      DCHECK(cred_salts_storage->empty());
      values_storage->pGlobalHmacSalt = FillHMACSalts(salts_storage, input);
    } else {
      const WEBAUTHN_CRED_WITH_HMAC_SECRET_SALT cred_salt{
          base::checked_cast<DWORD>(input.credential_id->size()),
          const_cast<PBYTE>(input.credential_id->data()),
          FillHMACSalts(salts_storage, input),
      };
      cred_salts_storage->push_back(cred_salt);
    }
  }

  if (!cred_salts_storage->empty()) {
    values_storage->cCredWithHmacSecretSaltList =
        base::checked_cast<DWORD>(cred_salts_storage->size());
    values_storage->pCredWithHmacSecretSaltList = cred_salts_storage->data();
  }

  return values_storage;
}

}  // namespace

class WinWebAuthnApiImpl : public WinWebAuthnApi {
 public:
  WinWebAuthnApiImpl() {
    if (!base::FeatureList::IsEnabled(device::kWebAuthUseNativeWinApi)) {
      FIDO_LOG(DEBUG) << "Windows WebAuthn API deactivated via feature flag";
      return;
    }
    {
      // Mitigate the issues caused by loading DLLs on a background thread
      // (http://crbug/973868).
      SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();
      webauthn_dll_ =
          LoadLibraryExA("webauthn.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    }
    if (!webauthn_dll_) {
      FIDO_LOG(ERROR) << "Windows WebAuthn API failed to load";
      return;
    }

#define BIND_FN(fn_pointer, lib_handle, fn_name)       \
  DCHECK(!fn_pointer);                                 \
  fn_pointer = reinterpret_cast<decltype(fn_pointer)>( \
      GetProcAddress(lib_handle, fn_name));

#define BIND_FN_OR_RETURN(fn_pointer, lib_handle, fn_name) \
  BIND_FN(fn_pointer, lib_handle, fn_name);                \
  if (!fn_pointer) {                                       \
    DLOG(ERROR) << "failed to bind " << fn_name;           \
    return;                                                \
  }

    BIND_FN_OR_RETURN(is_user_verifying_platform_authenticator_available_,
                      webauthn_dll_,
                      "WebAuthNIsUserVerifyingPlatformAuthenticatorAvailable");
    BIND_FN_OR_RETURN(authenticator_make_credential_, webauthn_dll_,
                      "WebAuthNAuthenticatorMakeCredential");
    BIND_FN_OR_RETURN(authenticator_get_assertion_, webauthn_dll_,
                      "WebAuthNAuthenticatorGetAssertion");
    BIND_FN_OR_RETURN(cancel_current_operation_, webauthn_dll_,
                      "WebAuthNCancelCurrentOperation");
    BIND_FN_OR_RETURN(get_error_name_, webauthn_dll_, "WebAuthNGetErrorName");
    BIND_FN_OR_RETURN(free_credential_attestation_, webauthn_dll_,
                      "WebAuthNFreeCredentialAttestation");
    BIND_FN_OR_RETURN(free_assertion_, webauthn_dll_, "WebAuthNFreeAssertion");

    // The platform credential list set of functions was added in version 4.
    BIND_FN(get_platform_credential_list_, webauthn_dll_,
            "WebAuthNGetPlatformCredentialList");
    if (get_platform_credential_list_) {
      BIND_FN_OR_RETURN(free_platform_credential_list_, webauthn_dll_,
                        "WebAuthNFreePlatformCredentialList");
      BIND_FN_OR_RETURN(delete_platform_credential_, webauthn_dll_,
                        "WebAuthNDeletePlatformCredential");
    }

    is_bound_ = true;

    // Determine the API version of webauthn.dll. There is a version currently
    // shipping with Windows RS5 from before WebAuthNGetApiVersionNumber was
    // added (i.e., before WEBAUTHN_API_VERSION_1). Therefore we allow this
    // function to be missing.
    BIND_FN(get_api_version_number_, webauthn_dll_,
            "WebAuthNGetApiVersionNumber");
    api_version_ = get_api_version_number_ ? get_api_version_number_() : 0;

    FIDO_LOG(DEBUG) << "webauthn.dll version " << api_version_;
  }

  ~WinWebAuthnApiImpl() override = default;

  // WinWebAuthnApi:
  bool IsAvailable() const override {
    return base::FeatureList::IsEnabled(device::kWebAuthUseNativeWinApi) &&
           is_bound_ && (api_version_ >= WEBAUTHN_API_VERSION_1);
  }

  bool SupportsSilentDiscovery() const override {
    return get_platform_credential_list_;
  }

  HRESULT IsUserVerifyingPlatformAuthenticatorAvailable(
      BOOL* available) override {
    DCHECK(is_bound_);
    // Mitigate the issues caused by loading DLLs on a background thread
    // (http://crbug/973868).
    SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);
    return is_user_verifying_platform_authenticator_available_(available);
  }

  HRESULT AuthenticatorMakeCredential(
      HWND h_wnd,
      PCWEBAUTHN_RP_ENTITY_INFORMATION rp,
      PCWEBAUTHN_USER_ENTITY_INFORMATION user,
      PCWEBAUTHN_COSE_CREDENTIAL_PARAMETERS cose_credential_parameters,
      PCWEBAUTHN_CLIENT_DATA client_data,
      PCWEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS options,
      PWEBAUTHN_CREDENTIAL_ATTESTATION* credential_attestation_ptr) override {
    DCHECK(is_bound_);
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);
    return authenticator_make_credential_(
        h_wnd, rp, user, cose_credential_parameters, client_data, options,
        credential_attestation_ptr);
  }

  HRESULT AuthenticatorGetAssertion(
      HWND h_wnd,
      LPCWSTR rp_id,
      PCWEBAUTHN_CLIENT_DATA client_data,
      PCWEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS options,
      PWEBAUTHN_ASSERTION* assertion_ptr) override {
    DCHECK(is_bound_);
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);
    return authenticator_get_assertion_(h_wnd, rp_id, client_data, options,
                                        assertion_ptr);
  }

  HRESULT CancelCurrentOperation(GUID* cancellation_id) override {
    DCHECK(is_bound_);
    return cancel_current_operation_(cancellation_id);
  }

  HRESULT GetPlatformCredentialList(
      PCWEBAUTHN_GET_CREDENTIALS_OPTIONS options,
      PWEBAUTHN_CREDENTIAL_DETAILS_LIST* credentials) override {
    DCHECK(is_bound_ && get_platform_credential_list_);
    return get_platform_credential_list_(options, credentials);
  }

  HRESULT DeletePlatformCredential(
      base::span<const uint8_t> credential_id) override {
    return delete_platform_credential_(credential_id.size(),
                                       credential_id.data());
  }

  PCWSTR GetErrorName(HRESULT hr) override {
    DCHECK(is_bound_);
    return get_error_name_(hr);
  }

  void FreeCredentialAttestation(
      PWEBAUTHN_CREDENTIAL_ATTESTATION attestation_ptr) override {
    DCHECK(is_bound_);
    return free_credential_attestation_(attestation_ptr);
  }

  void FreeAssertion(PWEBAUTHN_ASSERTION assertion_ptr) override {
    DCHECK(is_bound_);
    return free_assertion_(assertion_ptr);
  }

  void FreePlatformCredentialList(
      PWEBAUTHN_CREDENTIAL_DETAILS_LIST credentials) override {
    DCHECK(is_bound_ && free_platform_credential_list_);
    free_platform_credential_list_(credentials);
  }

  int Version() override { return api_version_; }

 private:
  bool is_bound_ = false;
  uint32_t api_version_ = 0;
  HMODULE webauthn_dll_;

  decltype(&WebAuthNIsUserVerifyingPlatformAuthenticatorAvailable)
      is_user_verifying_platform_authenticator_available_ = nullptr;
  decltype(&WebAuthNAuthenticatorMakeCredential)
      authenticator_make_credential_ = nullptr;
  decltype(&WebAuthNAuthenticatorGetAssertion) authenticator_get_assertion_ =
      nullptr;
  decltype(&WebAuthNCancelCurrentOperation) cancel_current_operation_ = nullptr;
  decltype(&WebAuthNGetPlatformCredentialList) get_platform_credential_list_ =
      nullptr;
  decltype(&WebAuthNDeletePlatformCredential) delete_platform_credential_ =
      nullptr;
  decltype(&WebAuthNGetErrorName) get_error_name_ = nullptr;
  decltype(&WebAuthNFreeCredentialAttestation) free_credential_attestation_ =
      nullptr;
  decltype(&WebAuthNFreeAssertion) free_assertion_ = nullptr;
  decltype(&WebAuthNFreePlatformCredentialList) free_platform_credential_list_ =
      nullptr;

  // This method is not available in all versions of webauthn.dll.
  decltype(&WebAuthNGetApiVersionNumber) get_api_version_number_ = nullptr;
};

WinWebAuthnApi::ScopedOverride::ScopedOverride(WinWebAuthnApi* api) {
  CHECK(api);
  CHECK(!g_api_override);
  g_api_override = api;
}

WinWebAuthnApi::ScopedOverride::~ScopedOverride() {
  CHECK(g_api_override);
  g_api_override = nullptr;
}

// static
WinWebAuthnApi* WinWebAuthnApi::GetDefault() {
  if (g_api_override) {
    return g_api_override;
  }
  static base::NoDestructor<WinWebAuthnApiImpl> api;
  return api.get();
}

WinWebAuthnApi::WinWebAuthnApi() = default;

WinWebAuthnApi::~WinWebAuthnApi() = default;

bool WinWebAuthnApi::SupportsHybrid() {
  return IsAvailable() && Version() >= WEBAUTHN_API_VERSION_6;
}

std::pair<MakeCredentialStatus,
          std::optional<AuthenticatorMakeCredentialResponse>>
AuthenticatorMakeCredentialBlocking(WinWebAuthnApi* webauthn_api,
                                    HWND h_wnd,
                                    GUID cancellation_id,
                                    CtapMakeCredentialRequest request,
                                    MakeCredentialOptions request_options) {
  DCHECK(webauthn_api->IsAvailable());
  const int api_version = webauthn_api->Version();
  DCHECK(
      request_options.large_blob_support != LargeBlobSupport::kRequired ||
      (api_version >= WEBAUTHN_API_VERSION_3 &&
       request.authenticator_attachment != AuthenticatorAttachment::kPlatform));

  std::u16string rp_id = base::UTF8ToUTF16(request.rp.id);
  std::u16string rp_name = base::UTF8ToUTF16(request.rp.name.value_or(""));
  WEBAUTHN_RP_ENTITY_INFORMATION rp_info{
      WEBAUTHN_RP_ENTITY_INFORMATION_CURRENT_VERSION, base::as_wcstr(rp_id),
      base::as_wcstr(rp_name),
      /*pwszIcon=*/nullptr};

  std::u16string user_name = base::UTF8ToUTF16(request.user.name.value_or(""));
  std::u16string user_display_name =
      base::UTF8ToUTF16(request.user.display_name.value_or(""));
  std::vector<uint8_t> user_id = request.user.id;
  WEBAUTHN_USER_ENTITY_INFORMATION user_info{
      WEBAUTHN_USER_ENTITY_INFORMATION_CURRENT_VERSION,
      base::checked_cast<DWORD>(user_id.size()),
      const_cast<unsigned char*>(user_id.data()),
      base::as_wcstr(user_name),
      /*pwszIcon=*/nullptr,
      base::as_wcstr(user_display_name),
  };

  std::vector<WEBAUTHN_COSE_CREDENTIAL_PARAMETER>
      cose_credential_parameter_values;
  for (const PublicKeyCredentialParams::CredentialInfo& credential_info :
       request.public_key_credential_params.public_key_credential_params()) {
    if (credential_info.type != CredentialType::kPublicKey) {
      continue;
    }
    cose_credential_parameter_values.push_back(
        {WEBAUTHN_COSE_CREDENTIAL_PARAMETER_CURRENT_VERSION,
         WEBAUTHN_CREDENTIAL_TYPE_PUBLIC_KEY, credential_info.algorithm});
  }
  WEBAUTHN_COSE_CREDENTIAL_PARAMETERS cose_credential_parameters{
      base::checked_cast<DWORD>(cose_credential_parameter_values.size()),
      cose_credential_parameter_values.data()};

  std::string client_data_json = request.client_data_json;
  WEBAUTHN_CLIENT_DATA client_data{
      WEBAUTHN_CLIENT_DATA_CURRENT_VERSION,
      base::checked_cast<DWORD>(client_data_json.size()),
      const_cast<unsigned char*>(
          reinterpret_cast<const unsigned char*>(client_data_json.data())),
      WEBAUTHN_HASH_ALGORITHM_SHA_256};

  std::vector<WEBAUTHN_EXTENSION> extensions;
  if (request.hmac_secret) {
    // In version six of webauthn.dll, there's an explicit boolean for this. But
    // older versions of the library require that the extension be listed.
    static BOOL kHMACSecretTrue = TRUE;
    extensions.emplace_back(
        WEBAUTHN_EXTENSION{WEBAUTHN_EXTENSIONS_IDENTIFIER_HMAC_SECRET,
                           sizeof(BOOL), static_cast<void*>(&kHMACSecretTrue)});
  }

  WEBAUTHN_CRED_PROTECT_EXTENSION_IN maybe_cred_protect_extension;
  if (request.cred_protect) {
    // MakeCredentialRequestHandler rejects a request with credProtect
    // enforced=true if webauthn.dll does not support credProtect.
    if (request.cred_protect_enforce && api_version < WEBAUTHN_API_VERSION_2) {
      NOTREACHED();
    }
    // Windows doesn't support the concept of
    // CredProtectRequest::kUVOrCredIDRequiredOrBetter. So an authenticators
    // that defaults to credProtect level three will only use level two when
    // Chrome is setting the credProtect level for discoverable credentials.
    maybe_cred_protect_extension = WEBAUTHN_CRED_PROTECT_EXTENSION_IN{
        /*dwCredProtect=*/static_cast<uint8_t>(*request.cred_protect),
        /*bRequireCredProtect=*/request.cred_protect_enforce,
    };
    extensions.emplace_back(WEBAUTHN_EXTENSION{
        /*pwszExtensionIdentifier=*/WEBAUTHN_EXTENSIONS_IDENTIFIER_CRED_PROTECT,
        /*cbExtension=*/sizeof(WEBAUTHN_CRED_PROTECT_EXTENSION_IN),
        /*pvExtension=*/&maybe_cred_protect_extension,
    });
  }

  uint32_t authenticator_attachment;
  if (request_options.is_off_the_record_context &&
      api_version < WEBAUTHN_API_VERSION_4) {
    // API versions before `WEBAUTHN_API_VERSION_4` don't have support for
    // showing a warning message that platform credentials will out last the
    // Incognito session. Thus, in this case, only external authenticators are
    // enabled.
    authenticator_attachment = WEBAUTHN_AUTHENTICATOR_ATTACHMENT_CROSS_PLATFORM;
  } else if (request_options.large_blob_support ==
             LargeBlobSupport::kRequired) {
    // The Windows platform authenticator does not have support for large blob,
    // and will ignore the requirement if the user selects it. Force the request
    // to be only external authenticators.
    authenticator_attachment = WEBAUTHN_AUTHENTICATOR_ATTACHMENT_CROSS_PLATFORM;
  } else {
    authenticator_attachment =
        ToWinAuthenticatorAttachment(request.authenticator_attachment);
  }

  WEBAUTHN_CRED_BLOB_EXTENSION cred_blob_ext;
  if (request.cred_blob && api_version >= WEBAUTHN_API_VERSION_3 &&
      request.cred_blob->size() <=
          std::numeric_limits<decltype(cred_blob_ext.cbCredBlob)>::max()) {
    cred_blob_ext = {
        /*cbCredBlob=*/base::checked_cast<decltype(cred_blob_ext.cbCredBlob)>(
            request.cred_blob->size()),
        /*pbCredBlob=*/request.cred_blob->data(),
    };
    extensions.emplace_back(WEBAUTHN_EXTENSION{
        /*pwszExtensionIdentifier=*/WEBAUTHN_EXTENSIONS_IDENTIFIER_CRED_BLOB,
        /*cbExtension=*/sizeof(cred_blob_ext),
        /*pvExtension=*/&cred_blob_ext,
    });
  }

  if (request.min_pin_length_requested &&
      api_version >= WEBAUTHN_API_VERSION_3) {
    static const BOOL kRequestMinPINLength = TRUE;
    extensions.emplace_back(WEBAUTHN_EXTENSION{
        /*pwszExtensionIdentifier=*/
        WEBAUTHN_EXTENSIONS_IDENTIFIER_MIN_PIN_LENGTH,
        /*cbExtension=*/sizeof(kRequestMinPINLength),
        /*pvExtension=*/const_cast<BOOL*>(&kRequestMinPINLength),
    });
  }

  DWORD enterprise_attestation = WEBAUTHN_ENTERPRISE_ATTESTATION_NONE;
  switch (request.attestation_preference) {
    case AttestationConveyancePreference::kEnterpriseIfRPListedOnAuthenticator:
      enterprise_attestation =
          WEBAUTHN_ENTERPRISE_ATTESTATION_VENDOR_FACILITATED;
      break;
    case AttestationConveyancePreference::kEnterpriseApprovedByBrowser:
      enterprise_attestation = WEBAUTHN_ENTERPRISE_ATTESTATION_PLATFORM_MANAGED;
      break;
    default:
      break;
  }

  // Note that entries in |exclude_list_credentials| hold pointers
  // into request.exclude_list.
  std::vector<WEBAUTHN_CREDENTIAL_EX> exclude_list_credentials =
      ToWinCredentialExVector(&request.exclude_list);
  std::vector<WEBAUTHN_CREDENTIAL_EX*> exclude_list_ptrs;
  base::ranges::transform(exclude_list_credentials,
                          std::back_inserter(exclude_list_ptrs),
                          [](auto& cred) { return &cred; });
  WEBAUTHN_CREDENTIAL_LIST exclude_credential_list{
      base::checked_cast<DWORD>(exclude_list_ptrs.size()),
      exclude_list_ptrs.data()};

  WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS options{
      WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS_VERSION_7,
      kWinWebAuthnTimeoutMilliseconds,
      WEBAUTHN_CREDENTIALS{
          0, nullptr},  // Ignored because pExcludeCredentialList is set.
      WEBAUTHN_EXTENSIONS{base::checked_cast<DWORD>(extensions.size()),
                          extensions.data()},
      authenticator_attachment,
      request.resident_key_required,
      ToWinUserVerificationRequirement(request.user_verification),
      ToWinAttestationConveyancePreference(request.attestation_preference,
                                           api_version),
      /*dwFlags=*/0,
      &cancellation_id,
      &exclude_credential_list,
      enterprise_attestation,
      ToWinLargeBlobSupport(request_options.large_blob_support),
      /*bPreferResidentKey=*/request_options.resident_key ==
          ResidentKeyRequirement::kPreferred,
      request_options.is_off_the_record_context,
      request.hmac_secret,
      /*pLinkedDevice=*/nullptr,
      /*cbJsonExt=*/0,
      /*pbJsonExt=*/nullptr,
  };

  FIDO_LOG(DEBUG) << "WebAuthNAuthenticatorMakeCredential("
                  << "rp=" << rp_info << ", user=" << user_info
                  << ", cose_credential_parameters="
                  << cose_credential_parameters
                  << ", client_data=" << client_data << ", options=" << options
                  << ")";

  WEBAUTHN_CREDENTIAL_ATTESTATION* credential_attestation = nullptr;
  HRESULT hresult = webauthn_api->AuthenticatorMakeCredential(
      h_wnd, &rp_info, &user_info, &cose_credential_parameters, &client_data,
      &options, &credential_attestation);
  std::unique_ptr<WEBAUTHN_CREDENTIAL_ATTESTATION,
                  std::function<void(PWEBAUTHN_CREDENTIAL_ATTESTATION)>>
      credential_attestation_deleter(
          credential_attestation,
          [webauthn_api](PWEBAUTHN_CREDENTIAL_ATTESTATION ptr) {
            webauthn_api->FreeCredentialAttestation(ptr);
          });
  if (hresult != S_OK) {
    FIDO_LOG(DEBUG) << "WebAuthNAuthenticatorMakeCredential()="
                    << HresultToHex(hresult) << " ("
                    << webauthn_api->GetErrorName(hresult) << ")";
    return {WinErrorNameToMakeCredentialStatus(
                base::as_u16cstr(webauthn_api->GetErrorName(hresult))),
            std::nullopt};
  }
  FIDO_LOG(DEBUG) << "WebAuthNAuthenticatorMakeCredential()="
                  << *credential_attestation;
  return {MakeCredentialStatus::kSuccess,
          ToAuthenticatorMakeCredentialResponse(*credential_attestation)};
}

std::pair<GetAssertionStatus, std::optional<AuthenticatorGetAssertionResponse>>
AuthenticatorGetAssertionBlocking(WinWebAuthnApi* webauthn_api,
                                  HWND h_wnd,
                                  GUID cancellation_id,
                                  CtapGetAssertionRequest request,
                                  CtapGetAssertionOptions request_options) {
  DCHECK(webauthn_api->IsAvailable());
  const int api_version = webauthn_api->Version();

  std::u16string rp_id16 = base::UTF8ToUTF16(request.rp_id);
  std::string client_data_json = request.client_data_json;
  WEBAUTHN_CLIENT_DATA client_data{
      WEBAUTHN_CLIENT_DATA_CURRENT_VERSION,
      base::checked_cast<DWORD>(client_data_json.size()),
      const_cast<unsigned char*>(
          reinterpret_cast<const unsigned char*>(client_data_json.data())),
      WEBAUTHN_HASH_ALGORITHM_SHA_256};

  std::optional<std::u16string> opt_app_id16 = std::nullopt;
  if (request.app_id) {
    opt_app_id16 = base::UTF8ToUTF16(
        std::string_view(reinterpret_cast<const char*>(request.app_id->data()),
                         request.app_id->size()));
  }

  // Note that entries in |allow_list_credentials| hold pointers into
  // request.allow_list.
  std::vector<WEBAUTHN_CREDENTIAL_EX> allow_list_credentials =
      ToWinCredentialExVector(&request.allow_list);
  std::vector<WEBAUTHN_CREDENTIAL_EX*> allow_list_ptrs;
  base::ranges::transform(allow_list_credentials,
                          std::back_inserter(allow_list_ptrs),
                          [](auto& cred) { return &cred; });
  WEBAUTHN_CREDENTIAL_LIST allow_credential_list{
      base::checked_cast<DWORD>(allow_list_ptrs.size()),
      allow_list_ptrs.data()};

  // Note that entries in |legacy_credentials| hold pointers into
  // request.allow_list.
  auto legacy_credentials = ToWinCredentialVector(&request.allow_list);

  std::vector<WEBAUTHN_EXTENSION> extensions;
  if (api_version >= WEBAUTHN_API_VERSION_3 && request.get_cred_blob) {
    static const BOOL kCredBlobTrue = TRUE;
    extensions.emplace_back(WEBAUTHN_EXTENSION{
        /*pwszExtensionIdentifier=*/WEBAUTHN_EXTENSIONS_IDENTIFIER_CRED_BLOB,
        /*cbExtension=*/sizeof(kCredBlobTrue),
        /*pvExtension=*/const_cast<BOOL*>(&kCredBlobTrue),
    });
  }

  WEBAUTHN_HMAC_SECRET_SALT_VALUES hmac_salt_values_storage;
  std::vector<WEBAUTHN_HMAC_SECRET_SALT> salts_storage;
  std::vector<WEBAUTHN_CRED_WITH_HMAC_SECRET_SALT> cred_salts_storage;
  WEBAUTHN_HMAC_SECRET_SALT_VALUES* const hmac_salt_values =
      FillHMACSaltValues(&hmac_salt_values_storage, &salts_storage,
                         &cred_salts_storage, request_options.prf_inputs);

  DWORD flags = 0;
  if (hmac_salt_values) {
    // The HMAC salts are hashed in the renderer. This flag indicates that they
    // should not be hashed again.
    flags |= WEBAUTHN_AUTHENTICATOR_HMAC_SECRET_VALUES_FLAG;
  }

  DWORD large_blob_operation = WEBAUTHN_CRED_LARGE_BLOB_OPERATION_NONE;
  base::span<uint8_t> large_blob;
  if (api_version >= WEBAUTHN_API_VERSION_3) {
    if (request_options.large_blob_read) {
      large_blob_operation = WEBAUTHN_CRED_LARGE_BLOB_OPERATION_GET;
    } else if (request_options.large_blob_write) {
      large_blob_operation = WEBAUTHN_CRED_LARGE_BLOB_OPERATION_SET;
      large_blob = *request_options.large_blob_write;
    }
  }

  static BOOL kUseAppIdTrue = TRUE;    // const
  static BOOL kUseAppIdFalse = FALSE;  // const
  WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS options{
      WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS_VERSION_7,
      kWinWebAuthnTimeoutMilliseconds,
      // As of Nov 2018, the WebAuthNAuthenticatorGetAssertion method will
      // fail to challenge credentials via CTAP1 if the allowList is passed
      // in the extended form in WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS
      // (i.e.  pAllowCredentialList instead of CredentialList). The legacy
      // CredentialList field works fine, but does not support setting
      // transport restrictions on the credential descriptor.
      //
      // As a workaround, MS tells us to also set the CredentialList
      // parameter with an accurate cCredentials count and some arbitrary
      // pCredentials data.
      WEBAUTHN_CREDENTIALS{base::checked_cast<DWORD>(legacy_credentials.size()),
                           legacy_credentials.data()},
      WEBAUTHN_EXTENSIONS{base::checked_cast<DWORD>(extensions.size()),
                          extensions.data()},
      WEBAUTHN_AUTHENTICATOR_ATTACHMENT_ANY,
      ToWinUserVerificationRequirement(request.user_verification),
      flags,
      opt_app_id16 ? base::as_wcstr(*opt_app_id16) : nullptr,
      opt_app_id16 ? &kUseAppIdTrue : &kUseAppIdFalse,
      &cancellation_id,
      &allow_credential_list,
      large_blob_operation,
      base::checked_cast<DWORD>(large_blob.size()),
      large_blob.data(),
      hmac_salt_values,
      request_options.is_off_the_record_context,
      /*pLinkedDevice=*/nullptr,
      /*bAutoFill=*/FALSE,
      /*cbJsonExt=*/0,
      /*pbJsonExt=*/nullptr,
  };

  FIDO_LOG(DEBUG) << "WebAuthNAuthenticatorGetAssertion("
                  << "rp_id=\"" << rp_id16 << "\", client_data=" << client_data
                  << ", options=" << options << ")";

  WEBAUTHN_ASSERTION* assertion = nullptr;
  HRESULT hresult = webauthn_api->AuthenticatorGetAssertion(
      h_wnd, base::as_wcstr(rp_id16), &client_data, &options, &assertion);
  std::unique_ptr<WEBAUTHN_ASSERTION, std::function<void(PWEBAUTHN_ASSERTION)>>
      assertion_deleter(assertion, [webauthn_api](PWEBAUTHN_ASSERTION ptr) {
        webauthn_api->FreeAssertion(ptr);
      });
  if (hresult != S_OK) {
    FIDO_LOG(DEBUG) << "WebAuthNAuthenticatorGetAssertion()="
                    << HresultToHex(hresult) << " ("
                    << webauthn_api->GetErrorName(hresult) << ")";
    return {WinErrorNameToGetAssertionStatus(
                base::as_u16cstr(webauthn_api->GetErrorName(hresult))),
            std::nullopt};
  }

  FIDO_LOG(DEBUG) << "WebAuthNAuthenticatorGetAssertion()=" << *assertion;
  std::optional<AuthenticatorGetAssertionResponse> response =
      ToAuthenticatorGetAssertionResponse(*assertion, request_options);
  if (response && !request_options.prf_inputs.empty() &&
      webauthn_api->Version() < WEBAUTHN_API_VERSION_4) {
    // This version of Windows does not yet support passing in inputs for
    // hmac_secret.
    response->hmac_secret_not_evaluated = true;
  }
  return {response ? GetAssertionStatus::kSuccess
                   : GetAssertionStatus::kAuthenticatorResponseInvalid,
          std::move(response)};
}

std::pair<bool, std::vector<DiscoverableCredentialMetadata>>
AuthenticatorEnumerateCredentialsBlocking(WinWebAuthnApi* webauthn_api,
                                          std::u16string_view rp_id,
                                          bool is_incognito) {
  if (!webauthn_api || !webauthn_api->IsAvailable() ||
      !webauthn_api->SupportsSilentDiscovery()) {
    FIDO_LOG(DEBUG) << "Silent discovery unavailable";
    return {false, {}};
  }

  WEBAUTHN_GET_CREDENTIALS_OPTIONS options{
      .dwVersion = WEBAUTHN_GET_CREDENTIALS_OPTIONS_VERSION_1,
      // For a default-initialized string_view `pwszRpId` will be nullptr,
      // which makes the API not filter on RP ID.
      .pwszRpId = base::as_wcstr(rp_id),
      .bBrowserInPrivateMode = is_incognito};

  FIDO_LOG(DEBUG) << "WebAuthNGetCredentialList("
                  << ", options=" << options << ")";

  PWEBAUTHN_CREDENTIAL_DETAILS_LIST credentials = nullptr;
  HRESULT hresult =
      webauthn_api->GetPlatformCredentialList(&options, &credentials);
  std::unique_ptr<WEBAUTHN_CREDENTIAL_DETAILS_LIST,
                  std::function<void(PWEBAUTHN_CREDENTIAL_DETAILS_LIST)>>
      credentials_deleter(
          credentials, [webauthn_api](PWEBAUTHN_CREDENTIAL_DETAILS_LIST ptr) {
            webauthn_api->FreePlatformCredentialList(ptr);
          });
  if (hresult != S_OK) {
    FIDO_LOG(DEBUG) << "WebAuthNGetPlatformCredentialList()="
                    << HresultToHex(hresult) << " ("
                    << webauthn_api->GetErrorName(hresult) << ")";
    // Indicate failure only if the hresult is unexpected.
    if (hresult != NTE_NOT_FOUND) {
      FIDO_LOG(ERROR) << "Windows API returned unknown result: " << hresult;
      return {false, {}};
    }
    return {true, {}};
  }
  FIDO_LOG(DEBUG) << "WebAuthNGetCredentialList returned "
                  << credentials->cCredentialDetails << " credential(s)";
  return {true, WinCredentialDetailsListToCredentialMetadata(*credentials)};
}

}  // namespace device
