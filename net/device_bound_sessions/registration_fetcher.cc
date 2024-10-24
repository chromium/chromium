// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/registration_fetcher.h"

#include <utility>
#include <vector>

#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/unexportable_key_service.h"
#include "net/base/io_buffer.h"
#include "net/device_bound_sessions/session_binding_utils.h"
#include "net/device_bound_sessions/session_challenge_param.h"
#include "net/device_bound_sessions/session_json_utils.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_request_context.h"

namespace net::device_bound_sessions {

namespace {

constexpr char kJwtSessionHeaderName[] = "Sec-Session-Response";
constexpr net::NetworkTrafficAnnotationTag kRegistrationTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("dbsc_registration", R"(
        semantics {
          sender: "Device Bound Session Credentials API"
          description:
            "Device Bound Session Credentials (DBSC) let a server create a "
            "session with the local device. For more info see "
            "https://github.com/WICG/dbsc."
          trigger:
            "Server sending a response with a Sec-Session-Registration header."
          data: "A signed JWT with the new key created for this session."
          destination: WEBSITE
          last_reviewed: "2024-04-10"
          user_data {
            type: ACCESS_TOKEN
          }
          internal {
            contacts {
              email: "kristianm@chromium.org"
            }
            contacts {
              email: "chrome-counter-abuse-alerts@google.com"
            }
          }
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting: "There is no seperate setting for this feature, but it will "
            "follow the cookie settings."
          policy_exception_justification: "Not implemented."
        })");

constexpr int kBufferSize = 4096;

// New session registration doesn't block the user and can be done with a delay.
constexpr unexportable_keys::BackgroundTaskPriority kTaskPriority =
    unexportable_keys::BackgroundTaskPriority::kBestEffort;

void OnDataSigned(
    crypto::SignatureVerifier::SignatureAlgorithm algorithm,
    unexportable_keys::UnexportableKeyService& unexportable_key_service,
    std::string header_and_payload,
    unexportable_keys::UnexportableKeyId key_id,
    base::OnceCallback<void(
        std::optional<RegistrationFetcher::RegistrationTokenResult>)> callback,
    unexportable_keys::ServiceErrorOr<std::vector<uint8_t>> result) {
  if (!result.has_value()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  const std::vector<uint8_t>& signature = result.value();
  std::optional<std::string> registration_token =
      AppendSignatureToHeaderAndPayload(header_and_payload, algorithm,
                                        signature);
  if (!registration_token.has_value()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  std::move(callback).Run(RegistrationFetcher::RegistrationTokenResult(
      registration_token.value(), key_id));
}

void SignChallengeWithKey(
    unexportable_keys::UnexportableKeyService& unexportable_key_service,
    std::string_view challenge,
    const unexportable_keys::UnexportableKeyId key_id,
    const GURL& registration_url,
    std::optional<std::string> authorization,
    base::OnceCallback<
        void(std::optional<RegistrationFetcher::RegistrationTokenResult>)>
        callback) {
  auto expected_algorithm = unexportable_key_service.GetAlgorithm(key_id);
  auto expected_public_key =
      unexportable_key_service.GetSubjectPublicKeyInfo(key_id);
  if (!expected_algorithm.has_value() || !expected_public_key.has_value()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  std::optional<std::string> optional_header_and_payload =
      CreateKeyRegistrationHeaderAndPayload(
          challenge, registration_url, expected_algorithm.value(),
          expected_public_key.value(), base::Time::Now(),
          std::move(authorization));

  if (!optional_header_and_payload.has_value()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  std::string header_and_payload = optional_header_and_payload.value();
  unexportable_key_service.SignSlowlyAsync(
      key_id, base::as_bytes(base::make_span(header_and_payload)),
      kTaskPriority,
      base::BindOnce(&OnDataSigned, expected_algorithm.value(),
                     std::ref(unexportable_key_service), header_and_payload,
                     key_id, std::move(callback)));
}

void OnKeyGenerated(
    unexportable_keys::UnexportableKeyService& unexportable_key_service,
    std::string_view challenge,
    const GURL& registration_url,
    std::optional<std::string> authorization,
    base::OnceCallback<void(
        std::optional<RegistrationFetcher::RegistrationTokenResult>)> callback,
    unexportable_keys::ServiceErrorOr<unexportable_keys::UnexportableKeyId>
        result) {
  if (!result.has_value()) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  SignChallengeWithKey(unexportable_key_service, challenge, *result,
                       registration_url, std::move(authorization),
                       std::move(callback));
}

void CreateTokenAsync(
    unexportable_keys::UnexportableKeyService& unexportable_key_service,
    std::string challenge,
    const GURL& registration_url,
    base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
        supported_algos,
    std::optional<std::string> authorization,
    base::OnceCallback<
        void(std::optional<RegistrationFetcher::RegistrationTokenResult>)>
        callback) {
  unexportable_key_service.GenerateSigningKeySlowlyAsync(
      supported_algos, kTaskPriority,
      base::BindOnce(&OnKeyGenerated, std::ref(unexportable_key_service),
                     std::move(challenge), registration_url,
                     std::move(authorization), std::move(callback)));
}

class RegistrationFetcherImpl : public URLRequest::Delegate {
 public:
  // URLRequest::Delegate

  void OnReceivedRedirect(URLRequest* request,
                          const RedirectInfo& redirect_info,
                          bool* defer_redirect) override {
    if (!redirect_info.new_url.SchemeIsCryptographic()) {
      request->Cancel();
      OnResponseCompleted();
      // *this is deleted here
    }
  }

  // TODO(kristianm): Look into if OnAuthRequired might need to be customize
  // for DBSC

  // TODO(kristianm): Think about what to do for DBSC with
  // OnCertificateRequested, leaning towards not supporting it but not sure.

  // Always cancel requests on SSL errors, this is the default implementation
  // of OnSSLCertificateError.

  // This is always called unless the request is deleted before it is called.
  void OnResponseStarted(URLRequest* request, int net_error) override {
    if (net_error != OK) {
      OnResponseCompleted();
      // *this is deleted here
      return;
    }

    HttpResponseHeaders* headers = request->response_headers();
    int response_code = headers ? headers->response_code() : 0;

    if (response_code == 401) {
      response_status_ = RegistrationResponseStatus::kNeedNewRegistration;
      challenge_param_ =
          device_bound_sessions::SessionChallengeParam::CreateIfValid(
              fetcher_endpoint_, headers);
      OnResponseCompleted();
      // *this is deleted here
      return;
    } else if (response_code < 200 || response_code >= 300) {
      response_status_ = RegistrationResponseStatus::kFailed;
      OnResponseCompleted();
      // *this is deleted here
      return;
    }

    response_status_ = RegistrationResponseStatus::kSuccess;
    // Initiate the first read.
    int bytes_read = request->Read(buf_.get(), kBufferSize);
    if (bytes_read >= 0) {
      OnReadCompleted(request, bytes_read);
    } else if (bytes_read != ERR_IO_PENDING) {
      OnResponseCompleted();
      // *this is deleted here
    }
  }

  void OnReadCompleted(URLRequest* request, int bytes_read) override {
    data_received_.append(buf_->data(), bytes_read);
    while (bytes_read > 0) {
      bytes_read = request->Read(buf_.get(), kBufferSize);
      if (bytes_read > 0) {
        data_received_.append(buf_->data(), bytes_read);
      }
    }

    if (bytes_read != ERR_IO_PENDING) {
      OnResponseCompleted();
      // *this is deleted here
    }
  }

  RegistrationFetcherImpl(
      GURL fetcher_endpoint,
      std::optional<std::string> authorization,
      unexportable_keys::UnexportableKeyService& key_service,
      const URLRequestContext* context,
      const IsolationInfo& isolation_info,
      RegistrationFetcher::RegistrationCompleteCallback callback)
      : fetcher_endpoint_(fetcher_endpoint),
        authorization_(authorization),
        key_service_(key_service),
        context_(context),
        isolation_info_(isolation_info),
        callback_(std::move(callback)),
        buf_(base::MakeRefCounted<IOBufferWithSize>(kBufferSize)) {}

  ~RegistrationFetcherImpl() override { CHECK(!callback_); }

  void OnRegistrationTokenCreated(
      std::optional<RegistrationFetcher::RegistrationTokenResult> result) {
    if (!result) {
      RunCallbackAndDeleteSelf(std::nullopt);
      return;
    }

    key_id_ = result->key_id;
    StartFetchingRegistration(result->registration_token);
  }

 private:
  void StartFetchingRegistration(const std::string& registration_token) {
    request_ = context_->CreateRequest(fetcher_endpoint_, IDLE, this,
                                       kRegistrationTrafficAnnotation);
    request_->set_method("POST");
    request_->SetLoadFlags(LOAD_DISABLE_CACHE);
    request_->set_allow_credentials(true);

    request_->set_site_for_cookies(isolation_info_.site_for_cookies());
    // TODO(kristianm): Set initiator to the URL of the registration header
    request_->set_initiator(url::Origin());
    request_->set_isolation_info(isolation_info_);

    request_->SetExtraRequestHeaderByName(
        kJwtSessionHeaderName, registration_token, /*overwrite*/ true);
    request_->Start();
  }

  void OnResponseCompleted() {
    if (response_status_ == RegistrationResponseStatus::kNeedNewRegistration &&
        !challenge_param_.empty()) {
      if (!key_id_.has_value()) {
        RunCallbackAndDeleteSelf(std::nullopt);
      }

      // TODO(kristianm): Log if there is more than one challenge
      // Note this preserves the lifetime of *this
      std::string_view challenge(challenge_param_[0].challenge());
      SignChallengeWithKey(
          *key_service_, challenge, *key_id_, fetcher_endpoint_, authorization_,
          base::BindOnce(&RegistrationFetcherImpl::OnRegistrationTokenCreated,
                         base::Unretained(this)));
      return;
    }

    if (!data_received_.empty()) {
      std::optional<SessionParams> params =
          ParseSessionInstructionJson(data_received_);
      if (params) {
        RunCallbackAndDeleteSelf(
            RegistrationFetcher::RegistrationCompleteParams(
                std::move(*params), *key_id_, request_->url()));
      } else {
        RunCallbackAndDeleteSelf(std::nullopt);
      }
    } else {
      RunCallbackAndDeleteSelf(std::nullopt);
    }
    // *this is deleted here
  }

  // Running callback when fetching is complete or on error.
  // Deletes `this` afterwards.
  void RunCallbackAndDeleteSelf(
      std::optional<RegistrationFetcher::RegistrationCompleteParams> params) {
    std::move(callback_).Run(std::move(params));
    delete this;
  }

  enum class RegistrationResponseStatus {
    kUnknown = 0,
    kSuccess = 1,
    kFailed = 2,
    kNeedNewRegistration = 3,
    kMaxValue = kNeedNewRegistration
  };

  // State passed in to constructor
  GURL fetcher_endpoint_;
  std::optional<std::string> authorization_;
  const raw_ref<unexportable_keys::UnexportableKeyService> key_service_;
  raw_ptr<const URLRequestContext> context_;
  IsolationInfo isolation_info_;
  RegistrationFetcher::RegistrationCompleteCallback callback_;

  // Set during key creation, before sending request to fetch data.
  // Should always be nullopt before that, and always a valid key after key
  // creation.
  std::optional<unexportable_keys::UnexportableKeyId> key_id_ = std::nullopt;

  // Created to fetch data
  std::unique_ptr<URLRequest> request_;
  scoped_refptr<IOBuffer> buf_;
  std::string data_received_;
  RegistrationResponseStatus response_status_ =
      RegistrationResponseStatus::kUnknown;
  std::vector<SessionChallengeParam> challenge_param_;
};

std::optional<RegistrationFetcher::RegistrationCompleteParams> (
    *g_mock_fetcher)() = nullptr;

}  // namespace

void RegistrationFetcher::StartCreateTokenAndFetch(
    RegistrationFetcherParam registration_params,
    unexportable_keys::UnexportableKeyService& key_service,
    // TODO(kristianm): Check the lifetime of context and make sure this use
    // is safe.
    const URLRequestContext* context,
    const IsolationInfo& isolation_info,
    RegistrationCompleteCallback callback) {
  // Using mock fetcher for testing
  if (g_mock_fetcher) {
    std::move(callback).Run(g_mock_fetcher());
    return;
  }

  RegistrationFetcherImpl* fetcher = new RegistrationFetcherImpl(
      registration_params.registration_endpoint(),
      registration_params.authorization(), key_service, context, isolation_info,
      std::move(callback));

  GURL registration_endpoint = registration_params.registration_endpoint();
  std::string challenge = registration_params.challenge();
  std::string authorization =
      registration_params.authorization().value_or(std::string());

  // base::Unretained() is safe because the fetcher cannot be destroyed until
  // after this callback is run, as it controls its own lifetime.
  CreateTokenAsync(
      key_service, std::move(challenge), registration_endpoint,
      registration_params.supported_algos(), std::move(authorization),
      base::BindOnce(&RegistrationFetcherImpl::OnRegistrationTokenCreated,
                     base::Unretained(fetcher)));
}

void RegistrationFetcher::SetFetcherForTesting(FetcherType func) {
  if (g_mock_fetcher) {
    CHECK(!func);
    g_mock_fetcher = nullptr;
  } else {
    g_mock_fetcher = func;
  }
}

void RegistrationFetcher::CreateTokenAsyncForTesting(
    unexportable_keys::UnexportableKeyService& unexportable_key_service,
    std::string challenge,
    const GURL& registration_url,
    std::optional<std::string> authorization,
    base::OnceCallback<
        void(std::optional<RegistrationFetcher::RegistrationTokenResult>)>
        callback) {
  constexpr crypto::SignatureVerifier::SignatureAlgorithm supported_algos[] = {
      crypto::SignatureVerifier::ECDSA_SHA256,
      crypto::SignatureVerifier::RSA_PKCS1_SHA256};
  CreateTokenAsync(unexportable_key_service, std::move(challenge),
                   registration_url, supported_algos, std::move(authorization),
                   std::move(callback));
}

}  // namespace net::device_bound_sessions
