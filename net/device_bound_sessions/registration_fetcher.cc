// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/registration_fetcher.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/unexportable_key_service.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/device_bound_sessions/registration_request_param.h"
#include "net/device_bound_sessions/session_binding_utils.h"
#include "net/device_bound_sessions/session_challenge_param.h"
#include "net/device_bound_sessions/session_json_utils.h"
#include "net/log/net_log_event_type.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "url/origin.h"

namespace net::device_bound_sessions {

namespace {

constexpr char kSessionIdHeaderName[] = "Sec-Session-Id";
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
          setting: "There is no separate setting for this feature, but it will "
            "follow the cookie settings."
          policy_exception_justification: "Not implemented."
        })");

constexpr int kBufferSize = 4096;

// New session registration doesn't block the user and can be done with a delay.
constexpr unexportable_keys::BackgroundTaskPriority kTaskPriority =
    unexportable_keys::BackgroundTaskPriority::kBestEffort;

void RecordHttpResponseOrErrorCode(const char* metric_name,
                                   int net_error,
                                   int http_response_code) {
  // No need to special-case `net::ERR_HTTP_RESPONSE_CODE_FAILURE` to return
  // the HTTP response code, because `UrlRequest` does not use that net error.
  base::UmaHistogramSparse(
      metric_name, net_error == net::OK ? http_response_code : net_error);
}

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
    unexportable_keys::UnexportableKeyId& key_id,
    const GURL& registration_url,
    std::string_view challenge,
    std::optional<std::string> authorization,
    std::optional<std::string> session_identifier,
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
          std::move(authorization), std::move(session_identifier));

  if (!optional_header_and_payload.has_value()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  std::string header_and_payload =
      std::move(optional_header_and_payload.value());
  unexportable_key_service.SignSlowlyAsync(
      key_id, base::as_byte_span(header_and_payload), kTaskPriority,
      /*max_retries=*/0,
      base::BindOnce(&OnDataSigned, expected_algorithm.value(),
                     std::ref(unexportable_key_service), header_and_payload,
                     key_id, std::move(callback)));
}

RegistrationFetcher::FetcherType* g_mock_fetcher = nullptr;

class RegistrationFetcherImpl : public RegistrationFetcher,
                                URLRequest::Delegate {
 public:
  // URLRequest::Delegate

  // TODO(kristianm): Look into if OnAuthRequired might need to be customize
  // for DBSC

  // TODO(kristianm): Think about what to do for DBSC with
  // OnCertificateRequested, leaning towards not supporting it but not sure.

  // Always cancel requests on SSL errors, this is the default implementation
  // of OnSSLCertificateError.

  // This is always called unless the request is deleted before it is called.
  void OnResponseStarted(URLRequest* request, int net_error) override {
    HttpResponseHeaders* headers = request->response_headers();
    const int response_code = headers ? headers->response_code() : 0;
    const char* histogram_name =
        IsForRefreshRequest()
            ? "Net.DeviceBoundSessions.Refresh.Network.Result"
            : "Net.DeviceBoundSessions.Registration.Network.Result";
    RecordHttpResponseOrErrorCode(histogram_name, net_error, response_code);

    if (net_error != OK) {
      OnResponseCompleted(
          /*error_on_no_data=*/SessionError::ErrorType::kNetError);
      // *this is deleted here
      return;
    }

    if (response_code == 401) {
      auto challenge_params =
          device_bound_sessions::SessionChallengeParam::CreateIfValid(
              fetcher_endpoint_, headers);
      OnChallengeNeeded(std::move(challenge_params));
      // *this is preserved here.
      return;
    }

    if (response_code < 200) {
      OnResponseCompleted(
          /*error_on_no_data=*/SessionError::ErrorType::kPersistentHttpError);
      // *this is deleted here
      return;
    } else if (response_code == 407) {
      // Proxy errors are treated as network errors
      OnResponseCompleted(
          /*error_on_no_data=*/SessionError::ErrorType::kNetError);
      // *this is deleted here
      return;
    } else if (300 <= response_code && response_code < 500) {
      OnResponseCompleted(
          /*error_on_no_data=*/SessionError::ErrorType::kPersistentHttpError);
      // *this is deleted here
      return;
    } else if (response_code >= 500) {
      OnResponseCompleted(
          /*error_on_no_data=*/SessionError::ErrorType::kTransientHttpError);
      // *this is deleted here
      return;
    }

    // Initiate the first read.
    int bytes_read = request->Read(buf_.get(), kBufferSize);
    if (bytes_read >= 0) {
      OnReadCompleted(request, bytes_read);
    } else if (bytes_read != ERR_IO_PENDING) {
      OnResponseCompleted(
          /*error_on_no_data=*/SessionError::ErrorType::kNetError);
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
      OnResponseCompleted(
          /*error_on_no_data=*/SessionError::ErrorType::kNetError);
      // *this is deleted here
    }
  }

  RegistrationFetcherImpl(
      const GURL& fetcher_endpoint,
      std::optional<std::string> session_identifier,
      unexportable_keys::UnexportableKeyService& key_service,
      const URLRequestContext* context,
      const IsolationInfo& isolation_info,
      std::optional<NetLogSource> net_log_source,
      const std::optional<url::Origin>& original_request_initiator)
      : fetcher_endpoint_(fetcher_endpoint),
        session_identifier_(std::move(session_identifier)),
        key_service_(key_service),
        context_(context),
        isolation_info_(isolation_info),
        net_log_source_(std::move(net_log_source)),
        original_request_initiator_(original_request_initiator),
        buf_(base::MakeRefCounted<IOBufferWithSize>(kBufferSize)) {}

  ~RegistrationFetcherImpl() override {}

  void OnKeyGenerated(
      unexportable_keys::ServiceErrorOr<unexportable_keys::UnexportableKeyId>
          key_id) {
    if (!key_id.has_value()) {
      RunCallback(
          base::unexpected(SessionError{SessionError::ErrorType::kKeyError}));
      // `this` may be deleted.
      return;
    }

    key_id_ = std::move(*key_id);
  }

  void StartFetch(std::optional<std::string> challenge,
                  std::optional<std::string> authorization) {
    CHECK(key_id_.has_value());

    current_challenge_ = std::move(challenge);
    current_authorization_ = std::move(authorization);

    if (current_challenge_.has_value()) {
      number_of_challenges_++;
      if (number_of_challenges_ < kMaxChallenges) {
        AttemptChallengeSigning();
        return;
      } else {
        RunCallback(base::unexpected(
            SessionError{SessionError::ErrorType::kTooManyChallenges}));
        // `this` may be deleted.
        return;
      }
    }

    // Start a request to get a challenge with the session identifier. The
    // `RegistrationRequestParam` constructors guarantee `session_identifier_`
    // is set when `challenge_` is missing.
    CHECK(IsForRefreshRequest());
    request_ = CreateBaseRequest();
    request_->Start();
  }

  base::WeakPtr<RegistrationFetcherImpl> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void StartCreateTokenAndFetch(
      RegistrationRequestParam& registration_params,
      base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
          supported_algos,
      RegistrationCompleteCallback callback) override {
    // Using mock fetcher for testing.
    if (g_mock_fetcher) {
      std::move(callback).Run(nullptr, g_mock_fetcher->Run());
      return;
    }

    CHECK(callback_.is_null());
    callback_ = std::move(callback);

    key_service_->GenerateSigningKeySlowlyAsync(
        supported_algos, kTaskPriority,
        base::BindOnce(&RegistrationFetcherImpl::OnKeyGenerated, GetWeakPtr())
            .Then(base::BindOnce(&RegistrationFetcherImpl::StartFetch,
                                 GetWeakPtr(),
                                 registration_params.TakeChallenge(),
                                 registration_params.TakeAuthorization())));
  }

  void StartFetchWithExistingKey(
      RegistrationRequestParam& request_params,
      unexportable_keys::ServiceErrorOr<unexportable_keys::UnexportableKeyId>
          key_id,
      RegistrationCompleteCallback callback) override {
    // Using mock fetcher for testing.
    if (g_mock_fetcher) {
      std::move(callback).Run(nullptr, g_mock_fetcher->Run());
      return;
    }

    if (!key_id.has_value()) {
      std::move(callback).Run(
          nullptr,
          base::unexpected(SessionError{SessionError::ErrorType::kKeyError}));
      return;
    }

    CHECK(callback_.is_null());
    callback_ = std::move(callback);

    key_id_ = *key_id;

    StartFetch(request_params.TakeChallenge(),
               request_params.TakeAuthorization());
  }

 private:
  static constexpr size_t kMaxSigningFailures = 2;
  static constexpr size_t kMaxChallenges = 5;

  void AttemptChallengeSigning() {
    SignChallengeWithKey(
        *key_service_, *key_id_, fetcher_endpoint_, *current_challenge_,
        current_authorization_, session_identifier_,
        base::BindOnce(&RegistrationFetcherImpl::OnRegistrationTokenCreated,
                       GetWeakPtr()));
  }

  void OnRegistrationTokenCreated(
      std::optional<RegistrationFetcher::RegistrationTokenResult> result) {
    if (!result) {
      number_of_signing_failures_++;
      if (number_of_signing_failures_ < kMaxSigningFailures) {
        AttemptChallengeSigning();
        return;
      } else {
        RunCallback(base::unexpected(
            SessionError{SessionError::ErrorType::kSigningError}));
        // `this` may be deleted.
        return;
      }
    }

    request_ = CreateBaseRequest();
    request_->SetExtraRequestHeaderByName(
        kJwtSessionHeaderName, result->registration_token, /*overwrite*/ true);
    request_->Start();
  }

  std::unique_ptr<net::URLRequest> CreateBaseRequest() {
    CHECK(IsSecure(fetcher_endpoint_));

    std::unique_ptr<net::URLRequest> request = context_->CreateRequest(
        fetcher_endpoint_, IDLE, this, kRegistrationTrafficAnnotation,
        /*is_for_websockets=*/false, net_log_source_);
    request->set_method("POST");
    request->SetLoadFlags(LOAD_DISABLE_CACHE);
    request->set_allow_credentials(true);

    request->set_site_for_cookies(isolation_info_.site_for_cookies());
    request->set_initiator(original_request_initiator_);
    request->set_isolation_info(isolation_info_);

    if (IsForRefreshRequest()) {
      request->SetExtraRequestHeaderByName(
          kSessionIdHeaderName, *session_identifier_, /*overwrite*/ true);
    }

    return request;
  }

  void OnChallengeNeeded(
      std::optional<std::vector<SessionChallengeParam>> challenge_params) {
    if (!challenge_params || challenge_params->empty()) {
      RunCallback(base::unexpected(
          SessionError{SessionError::ErrorType::kInvalidChallenge}));
      // `this` may be deleted.
      return;
    }

    // TODO(kristianm): Log if there is more than one challenge
    // TODO(kristianm): Handle if session identifiers don't match
    const std::string& challenge = (*challenge_params)[0].challenge();
    StartFetch(challenge, std::nullopt);
  }

  void OnResponseCompleted(SessionError::ErrorType error_on_no_data) {
    if (data_received_.empty()) {
      RunCallback(base::unexpected(SessionError{error_on_no_data}));
      // `this` may be deleted.
      return;
    }

    RunCallback(ParseSessionInstructionJson(
        request_->url(), *key_id_, session_identifier_, data_received_));
    // `this` may be deleted.
  }

  void RunCallback(
      base::expected<SessionParams, SessionError> params_or_error) {
    AddNetLogResult(params_or_error);
    std::move(callback_).Run(this, std::move(params_or_error));
  }

  void AddNetLogResult(
      const base::expected<SessionParams, SessionError>& params_or_error) {
    if (!request_) {
      return;
    }
    NetLogEventType result_event_type =
        IsForRefreshRequest() ? NetLogEventType::DBSC_REFRESH_RESULT
                              : NetLogEventType::DBSC_REGISTRATION_RESULT;
    request_->net_log().AddEvent(result_event_type, [&]() {
      std::string result;
      if (params_or_error.has_value()) {
        result = IsForRefreshRequest() ? "refreshed" : "registered";
      } else {
        const SessionError& error = params_or_error.error();
        if (error.IsFatal()) {
          result = "session_ended";
        } else {
          result = "failed_continue";
        }
      }

      base::Value::Dict dict;
      dict.Set("status", std::move(result));
      return dict;
    });
  }

  // Returns true if we're fetching for a refresh request. False means this is
  // for a registration request.
  bool IsForRefreshRequest() { return session_identifier_.has_value(); }

  //// This section of fields is state passed into the constructor. ////
  // Refers to the endpoint this class will use when triggering a registration
  // or refresh request.
  GURL fetcher_endpoint_;
  // Populated iff this is a refresh request (not a registration request).
  std::optional<std::string> session_identifier_;
  const raw_ref<unexportable_keys::UnexportableKeyService> key_service_;
  std::optional<unexportable_keys::UnexportableKeyId> key_id_;
  raw_ptr<const URLRequestContext> context_;
  IsolationInfo isolation_info_;
  std::optional<net::NetLogSource> net_log_source_;
  std::optional<url::Origin> original_request_initiator_;
  // This is called once the registration or refresh request completes, whether
  // or not it was successful.
  RegistrationFetcher::RegistrationCompleteCallback callback_;

  // Created to fetch data
  std::unique_ptr<URLRequest> request_;
  scoped_refptr<IOBuffer> buf_;
  std::string data_received_;

  std::optional<std::string> current_challenge_;
  std::optional<std::string> current_authorization_;
  size_t number_of_signing_failures_ = 0;
  size_t number_of_challenges_ = 0;

  base::WeakPtrFactory<RegistrationFetcherImpl> weak_ptr_factory_{this};
};

}  // namespace

// static
std::unique_ptr<RegistrationFetcher> RegistrationFetcher::CreateFetcher(
    RegistrationRequestParam& request_params,
    unexportable_keys::UnexportableKeyService& key_service,
    const URLRequestContext* context,
    const IsolationInfo& isolation_info,
    std::optional<NetLogSource> net_log_source,
    const std::optional<url::Origin>& original_request_initiator) {
  return std::make_unique<RegistrationFetcherImpl>(
      request_params.TakeRegistrationEndpoint(),
      request_params.TakeSessionIdentifier(), key_service, context,
      isolation_info, net_log_source, original_request_initiator);
}

void RegistrationFetcher::SetFetcherForTesting(FetcherType* func) {
  CHECK(!g_mock_fetcher || !func);
  g_mock_fetcher = func;
}

void RegistrationFetcher::CreateTokenAsyncForTesting(
    unexportable_keys::UnexportableKeyService& unexportable_key_service,
    std::string challenge,
    const GURL& registration_url,
    std::optional<std::string> authorization,
    std::optional<std::string> session_identifier,
    base::OnceCallback<
        void(std::optional<RegistrationFetcher::RegistrationTokenResult>)>
        callback) {
  static constexpr crypto::SignatureVerifier::SignatureAlgorithm
      kSupportedAlgos[] = {crypto::SignatureVerifier::ECDSA_SHA256,
                           crypto::SignatureVerifier::RSA_PKCS1_SHA256};
  unexportable_key_service.GenerateSigningKeySlowlyAsync(
      kSupportedAlgos, kTaskPriority,
      base::BindOnce(
          [](unexportable_keys::UnexportableKeyService& key_service,
             const GURL& registration_url, const std::string& challenge,
             std::optional<std::string>&& authorization,
             std::optional<std::string>&& session_identifier,
             base::OnceCallback<void(
                 std::optional<RegistrationFetcher::RegistrationTokenResult>)>
                 callback,
             unexportable_keys::ServiceErrorOr<
                 unexportable_keys::UnexportableKeyId> key_result) {
            if (!key_result.has_value()) {
              std::move(callback).Run(std::nullopt);
              return;
            }

            SignChallengeWithKey(
                key_service, key_result.value(), registration_url, challenge,
                std::move(authorization), std::move(session_identifier),
                std::move(callback));
          },
          std::ref(unexportable_key_service), registration_url,
          std::move(challenge), std::move(authorization),
          std::move(session_identifier), std::move(callback)));
}

}  // namespace net::device_bound_sessions
