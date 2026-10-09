// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/core/browser/cryptauth_cmtg_device_key_provider.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/device_event_log/device_event_log.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/oauth_consumer_id.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "crypto/random.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace webauthn {
namespace {

using KeysOrError = base::expected<std::vector<std::vector<uint8_t>>,
                                   CmtgDeviceKeyProvider::Error>;

constexpr size_t kMaxResponseSize = 64 * 1024;
constexpr size_t kWrapperKeySize = 32;
constexpr char kJsonContentType[] = "application/json";
constexpr char kQueryParameterAlternateOutputKey[] = "alt";
constexpr char kQueryParameterAlternateOutputJson[] = "json";
// Command-line switch to override `kCmtgServiceUrl`.
constexpr char kCmtgUrlSwitch[] = "webauthn-cmtg-url";

// The request messages (see cmtg_key_service.proto) are currently empty, so
// their JSON encoding is hardcoded.
constexpr char kEmptyRequestBody[] = "{}";

// JSON field names for the response messages in cmtg_key_service.proto.
constexpr char kCmtgWrapperKeyInfoField[] = "cmtgWrapperKeyInfo";
constexpr char kKeyMaterialField[] = "keyMaterial";
constexpr char kKeyAlgorithmField[] = "keyAlgorithm";
constexpr char kAes256GcmAlgorithm[] = "AES256_GCM";

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("cryptauth_cmtg_device_keys", R"(
        semantics {
          sender: "WebAuthentication"
          description:
            "Fetches or creates server-managed symmetric wrapper keys for "
            "Credential Manager Trust Group (CMTG) keys associated with a "
            "passkey in Google Password Manager."
          trigger:
            "When a website creates or asserts a passkey in Google Password "
            "Manager with the cmtgKey WebAuthn extension requested."
          data:
            "OAuth2 access token for the primary signed-in Google account and "
            "an empty JSON request."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
              email: "chrome-webauthn@google.com"
            }
          }
          user_data {
            type: ACCESS_TOKEN
          }
          last_reviewed: "2026-10-02"
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can disable saving and syncing passkeys in Google Password "
            "Manager settings, or sign out of their Google account."
          policy_exception_justification:
            "This request is only made when the user interacts with WebAuthn "
            "using Google Password Manager passkeys, which are already covered "
            "by sync and sign-in enterprise policies."
        })");

GURL GetBaseUrl() {
  const std::string switch_url =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          kCmtgUrlSwitch);
  if (!switch_url.empty()) {
    GURL url(switch_url);
    if (url.is_valid()) {
      return url;
    }
    FIDO_LOG(ERROR) << "Invalid CMTG URL from switch: " << switch_url;
  }
  return GURL(kCmtgServiceUrl);
}

GURL GetGetOrCreateUrl() {
  return net::AppendQueryParameter(GetBaseUrl().Resolve(kCmtgGetOrCreatePath),
                                   kQueryParameterAlternateOutputKey,
                                   kQueryParameterAlternateOutputJson);
}

// Parses the JSON encoding of a `CmtgWrapperKeyInfo` message and returns the
// raw key material, or `std::nullopt` if the key is malformed or uses an
// unsupported algorithm.
// TODO(crbug.com/485888879): Parse protos once the service returns them.
std::optional<std::vector<uint8_t>> ParseWrapperKeyInfo(
    const base::DictValue& key_info) {
  const std::string* algorithm = key_info.FindString(kKeyAlgorithmField);
  if (!algorithm || *algorithm != kAes256GcmAlgorithm) {
    FIDO_LOG(ERROR) << "Unsupported or missing CMTG wrapper key algorithm: "
                    << (algorithm ? *algorithm : "<none>");
    return std::nullopt;
  }
  const std::string* key_material = key_info.FindString(kKeyMaterialField);
  if (!key_material) {
    FIDO_LOG(ERROR) << "CMTG wrapper key is missing key material";
    return std::nullopt;
  }
  std::optional<std::vector<uint8_t>> key = base::Base64Decode(*key_material);
  if (!key || key->size() != kWrapperKeySize) {
    FIDO_LOG(ERROR) << "CMTG wrapper key material is invalid";
    return std::nullopt;
  }
  return key;
}

// Parses the JSON encoding of a `GetOrCreateCmtgWrapperKeyResponse` message.
KeysOrError ParseGetOrCreateResponse(const base::DictValue& response) {
  const base::DictValue* key_info = response.FindDict(kCmtgWrapperKeyInfoField);
  if (!key_info) {
    FIDO_LOG(ERROR) << "CMTG getOrCreate response is missing key info";
    return base::unexpected(CmtgDeviceKeyProvider::Error::kParseError);
  }
  std::optional<std::vector<uint8_t>> key = ParseWrapperKeyInfo(*key_info);
  if (!key) {
    return base::unexpected(CmtgDeviceKeyProvider::Error::kParseError);
  }
  std::vector<std::vector<uint8_t>> keys;
  keys.push_back(std::move(*key));
  return keys;
}

CmtgDeviceKeysResult ToMetricResult(const KeysOrError& result) {
  if (result.has_value()) {
    return CmtgDeviceKeysResult::kSuccess;
  }
  switch (result.error()) {
    case CmtgDeviceKeyProvider::Error::kNetworkError:
      return CmtgDeviceKeysResult::kNetworkError;
    case CmtgDeviceKeyProvider::Error::kParseError:
      return CmtgDeviceKeysResult::kParseError;
    case CmtgDeviceKeyProvider::Error::kAccessTokenError:
      return CmtgDeviceKeysResult::kAccessTokenError;
  }
}

class RequestImpl : public CmtgDeviceKeyProvider::Request {
 public:
  RequestImpl(signin::IdentityManager& identity_manager,
              scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
              CmtgDeviceKeyProvider::Operation operation,
              CmtgDeviceKeyProvider::Callback callback)
      : identity_manager_(identity_manager),
        url_loader_factory_(std::move(url_loader_factory)),
        operation_(operation),
        start_time_(base::TimeTicks::Now()),
        callback_(std::move(callback)) {}

  ~RequestImpl() override = default;

  void Start() {
    if (operation_ == CmtgDeviceKeyProvider::Operation::kGetAssertion) {
      // TODO(crbug.com/485888879): Fetch keys from the batchGet endpoint.
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&RequestImpl::FinishWithMockKeys,
                                    weak_ptr_factory_.GetWeakPtr()));
      return;
    }

    access_token_fetcher_ =
        std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
            signin::OAuthConsumerId::kCmtgDeviceKeyProvider,
            &identity_manager_.get(),
            base::BindOnce(&RequestImpl::OnAccessTokenFetched,
                           weak_ptr_factory_.GetWeakPtr()),
            signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate,
            signin::ConsentLevel::kSignin);
  }

 private:
  void OnAccessTokenFetched(GoogleServiceAuthError error,
                            signin::AccessTokenInfo access_token_info) {
    access_token_fetcher_.reset();
    if (error.state() != GoogleServiceAuthError::NONE) {
      FIDO_LOG(ERROR) << "Failed to fetch OAuth token for CMTG keys: "
                      << error.ToString();
      Finish(base::unexpected(CmtgDeviceKeyProvider::Error::kAccessTokenError));
      return;
    }

    auto resource_request = std::make_unique<network::ResourceRequest>();
    resource_request->url = GetGetOrCreateUrl();
    resource_request->method = net::HttpRequestHeaders::kPostMethod;
    resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
    resource_request->headers.SetHeader(
        net::HttpRequestHeaders::kAuthorization,
        base::StrCat({"Bearer ", access_token_info.token}));

    FIDO_LOG(EVENT) << "Sending CMTG wrapper key request to "
                    << resource_request->url.spec();

    url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                   kTrafficAnnotation);
    url_loader_->AttachStringForUpload(kEmptyRequestBody, kJsonContentType);
    url_loader_->DownloadToString(
        url_loader_factory_.get(),
        base::BindOnce(&RequestImpl::OnResponseReceived,
                       weak_ptr_factory_.GetWeakPtr()),
        kMaxResponseSize);
  }

  void OnResponseReceived(std::optional<std::string> response_body) {
    const int net_error = url_loader_->NetError();
    if (net_error != net::OK) {
      int response_code = -1;
      if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers) {
        response_code = url_loader_->ResponseInfo()->headers->response_code();
      }
      FIDO_LOG(ERROR) << "CMTG wrapper key request failed: net_error="
                      << net_error << " http_status=" << response_code;
      Finish(base::unexpected(CmtgDeviceKeyProvider::Error::kNetworkError));
      return;
    }

    std::optional<base::DictValue> response =
        base::JSONReader::ReadDict(*response_body, base::JSON_PARSE_RFC);
    if (!response) {
      FIDO_LOG(ERROR) << "Failed to parse CMTG wrapper key response as JSON";
      Finish(base::unexpected(CmtgDeviceKeyProvider::Error::kParseError));
      return;
    }

    Finish(ParseGetOrCreateResponse(*response));
  }

  void FinishWithMockKeys() {
    static const base::NoDestructor<std::vector<std::vector<uint8_t>>> kKeys(
        [] {
          std::vector<uint8_t> key(kWrapperKeySize);
          crypto::RandBytes(key);
          return std::vector<std::vector<uint8_t>>{std::move(key)};
        }());

    Finish(*kKeys);
  }

  void Finish(KeysOrError result) {
    url_loader_.reset();
    base::UmaHistogramMediumTimes(
        "WebAuthentication.CmtgDeviceKeys.RequestDuration",
        base::TimeTicks::Now() - start_time_);
    base::UmaHistogramEnumeration("WebAuthentication.CmtgDeviceKeys.Result",
                                  ToMetricResult(result));
    std::move(callback_).Run(std::move(result));
  }

  const raw_ref<signin::IdentityManager> identity_manager_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const CmtgDeviceKeyProvider::Operation operation_;
  const base::TimeTicks start_time_;
  CmtgDeviceKeyProvider::Callback callback_;
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  base::WeakPtrFactory<RequestImpl> weak_ptr_factory_{this};
};

}  // namespace

CryptauthCmtgDeviceKeyProvider::CryptauthCmtgDeviceKeyProvider(
    signin::IdentityManager& identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : identity_manager_(identity_manager),
      url_loader_factory_(std::move(url_loader_factory)) {
  CHECK(url_loader_factory_);
}

CryptauthCmtgDeviceKeyProvider::~CryptauthCmtgDeviceKeyProvider() = default;

std::unique_ptr<CmtgDeviceKeyProvider::Request>
CryptauthCmtgDeviceKeyProvider::GetDeviceKeys(Operation operation,
                                              Callback callback) {
  auto request = std::make_unique<RequestImpl>(
      *identity_manager_, url_loader_factory_, operation, std::move(callback));
  request->Start();
  return request;
}

}  // namespace webauthn
