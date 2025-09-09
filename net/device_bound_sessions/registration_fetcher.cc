// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/registration_fetcher.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/unexportable_key_service.h"
#include "net/base/features.h"
#include "net/base/net_errors.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/device_bound_sessions/registration_request_param.h"
#include "net/device_bound_sessions/session_binding_utils.h"
#include "net/device_bound_sessions/session_challenge_param.h"
#include "net/device_bound_sessions/session_json_utils.h"
#include "net/device_bound_sessions/url_fetcher.h"
#include "net/log/net_log_event_type.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_request_context.h"
#include "url/origin.h"

namespace net::device_bound_sessions {

namespace {

constexpr char kSessionIdHeaderName[] = "Sec-Session-Id";
constexpr char kJwtSessionHeaderName[] = "Sec-Session-Response";

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
    base::OnceCallback<
        void(std::optional<RegistrationFetcher::RegistrationToken>)> callback,
    unexportable_keys::ServiceErrorOr<std::vector<uint8_t>> result) {
  if (!result.has_value()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  const std::vector<uint8_t>& signature = result.value();
  std::optional<std::string> registration_token =
      AppendSignatureToHeaderAndPayload(header_and_payload, algorithm,
                                        signature);
  std::move(callback).Run(std::move(registration_token));
}

void SignChallengeWithKey(
    unexportable_keys::UnexportableKeyService& unexportable_key_service,
    unexportable_keys::UnexportableKeyId& key_id,
    const GURL& registration_url,
    std::string_view challenge,
    std::optional<std::string> authorization,
    std::optional<std::string> session_identifier,
    base::OnceCallback<
        void(std::optional<RegistrationFetcher::RegistrationToken>)> callback) {
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
                     std::move(callback)));
}

// Returns the registrable origin label for `origin_str`, or empty if the origin
// is invalid or not registrable.
std::string GetOriginLabel(const std::string& origin_str) {
  GURL url(origin_str);
  if (!url.is_valid()) {
    return "";
  }

  std::string domain = net::registry_controlled_domains::GetDomainAndRegistry(
      url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  const std::string::size_type dot_index = domain.find('.');
  if (dot_index == std::string::npos) {
    return "";
  }

  return domain.substr(0, dot_index);
}

bool WithinOriginLabelLimit(const std::vector<std::string>& relying_origins,
                            const std::string& target_origin) {
  constexpr size_t kMaxLabels = 5;
  base::flat_set<std::string> labels_seen;
  for (const std::string& origin_str : relying_origins) {
    std::string label = GetOriginLabel(origin_str);
    if (label.empty()) {
      continue;
    }

    if (!base::Contains(labels_seen, label)) {
      if (labels_seen.size() >= kMaxLabels) {
        continue;
      }

      labels_seen.insert(std::move(label));
    }

    if (origin_str == target_origin) {
      return true;
    }
  }

  return false;
}

RegistrationFetcher::FetcherType* g_mock_fetcher = nullptr;

class RegistrationFetcherImpl : public RegistrationFetcher {
 public:
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
        original_request_initiator_(original_request_initiator) {}

  ~RegistrationFetcherImpl() override {}

  void OnKeyGenerated(
      unexportable_keys::ServiceErrorOr<unexportable_keys::UnexportableKeyId>
          key_id) {
    if (!key_id.has_value()) {
      RunCallback(
          RegistrationResult(SessionError{SessionError::ErrorType::kKeyError}));
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
        RunCallback(RegistrationResult(
            SessionError{SessionError::ErrorType::kTooManyChallenges}));
        // `this` may be deleted.
        return;
      }
    }

    // Start a request to get a challenge with the session identifier. The
    // `RegistrationRequestParam` constructors guarantee `session_identifier_`
    // is set when `challenge_` is missing.
    CHECK(IsForRefreshRequest());

    url_fetcher_ = std::make_unique<URLFetcher>(context_, fetcher_endpoint_,
                                                net_log_source_);
    ConfigureRequest(url_fetcher_->request());
    // `this` owns `url_fetcher_`, so it's safe to use
    // `base::Unretained`
    url_fetcher_->Start(base::BindOnce(
        &RegistrationFetcherImpl::OnRequestComplete, base::Unretained(this)));
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

  void StartFetchWithFederatedKey(
      RegistrationRequestParam& request_params,
      unexportable_keys::UnexportableKeyId key_id,
      const GURL& provider_url,
      RegistrationCompleteCallback callback) override {
    // Using mock fetcher for testing.
    if (g_mock_fetcher) {
      std::move(callback).Run(nullptr, g_mock_fetcher->Run());
      return;
    }

    CHECK(callback_.is_null());
    callback_ = std::move(callback);

    key_id_ = key_id;
    provider_url_ = provider_url;

    if (!features::kDeviceBoundSessionsCheckFederatedRegistration.Get()) {
      StartFetch(request_params.TakeChallenge(),
                 request_params.TakeAuthorization());
      return;
    }

    GURL::Replacements replacements;
    replacements.SetPathStr("/.well-known/device-bound-sessions");
    GURL well_known_url = provider_url_.ReplaceComponents(replacements);
    url_fetcher_ =
        std::make_unique<URLFetcher>(context_, well_known_url, net_log_source_);
    url_fetcher_->request().set_method("GET");
    url_fetcher_->request().set_allow_credentials(false);
    url_fetcher_->request().set_site_for_cookies(
        isolation_info_.site_for_cookies());
    url_fetcher_->request().set_initiator(original_request_initiator_);
    url_fetcher_->request().set_isolation_info(isolation_info_);
    url_fetcher_->Start(base::BindOnce(
        &RegistrationFetcherImpl::OnProviderWellKnownRequestComplete,
        GetWeakPtr(), request_params.TakeChallenge(),
        request_params.TakeAuthorization()));
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
          RegistrationResult(SessionError{SessionError::ErrorType::kKeyError}));
      return;
    }

    CHECK(callback_.is_null());
    callback_ = std::move(callback);

    key_id_ = *key_id;

    StartFetch(request_params.TakeChallenge(),
               request_params.TakeAuthorization());
  }

 private:
  void OnProviderWellKnownRequestComplete(
      std::optional<std::string> challenge,
      std::optional<std::string> authorization) {
    SessionError::ErrorType error =
        OnProviderWellKnownRequestCompleteInternal();
    if (error != SessionError::ErrorType::kSuccess) {
      RunCallback(RegistrationResult(SessionError{error}));
      // `this` may be deleted.
      return;
    }

    GURL::Replacements replacements;
    replacements.SetPathStr("/.well-known/device-bound-sessions");
    GURL well_known_url = fetcher_endpoint_.ReplaceComponents(replacements);
    url_fetcher_ =
        std::make_unique<URLFetcher>(context_, well_known_url, net_log_source_);
    url_fetcher_->request().set_method("GET");
    url_fetcher_->request().set_allow_credentials(false);
    url_fetcher_->request().set_site_for_cookies(
        isolation_info_.site_for_cookies());
    url_fetcher_->request().set_initiator(original_request_initiator_);
    url_fetcher_->request().set_isolation_info(isolation_info_);
    url_fetcher_->Start(base::BindOnce(
        &RegistrationFetcherImpl::OnRelyingPartyWellKnownRequestComplete,
        GetWeakPtr(), std::move(challenge), std::move(authorization)));
  }

  SessionError::ErrorType OnProviderWellKnownRequestCompleteInternal() {
    HttpResponseHeaders* headers = url_fetcher_->request().response_headers();
    const int response_code = headers ? headers->response_code() : 0;
    RecordHttpResponseOrErrorCode(
        "Net.DeviceBoundSessions.ProviderWellKnown.Network.Result",
        url_fetcher_->net_error(), response_code);

    if (url_fetcher_->net_error() != OK) {
      return SessionError::ErrorType::kSessionProviderWellKnownUnavailable;
    }

    if (!headers || headers->response_code() != 200) {
      return SessionError::ErrorType::kSessionProviderWellKnownUnavailable;
    }

    std::optional<WellKnownParams> maybe_params =
        ParseWellKnownJson(url_fetcher_->data_received());
    if (!maybe_params.has_value()) {
      return SessionError::ErrorType::kSessionProviderWellKnownMalformed;
    }

    if (maybe_params->provider_origin.has_value()) {
      return SessionError::ErrorType::kSessionProviderWellKnownMalformed;
    }

    std::string target_origin =
        url::Origin::Create(fetcher_endpoint_).Serialize();
    if (!maybe_params->relying_origins.has_value() ||
        !base::Contains(*maybe_params->relying_origins, target_origin)) {
      return SessionError::ErrorType::kFederatedNotAuthorized;
    }

    if (!WithinOriginLabelLimit(*maybe_params->relying_origins,
                                target_origin)) {
      return SessionError::ErrorType::kTooManyRelyingOriginLabels;
    }

    return SessionError::ErrorType::kSuccess;
  }

  void OnRelyingPartyWellKnownRequestComplete(
      std::optional<std::string> challenge,
      std::optional<std::string> authorization) {
    SessionError::ErrorType error =
        OnRelyingPartyWellKnownRequestCompleteInternal();
    if (error != SessionError::ErrorType::kSuccess) {
      RunCallback(RegistrationResult(SessionError{error}));
      // `this` may be deleted.
      return;
    }

    StartFetch(std::move(challenge), std::move(authorization));
  }

  SessionError::ErrorType OnRelyingPartyWellKnownRequestCompleteInternal() {
    HttpResponseHeaders* headers = url_fetcher_->request().response_headers();
    const int response_code = headers ? headers->response_code() : 0;
    RecordHttpResponseOrErrorCode(
        "Net.DeviceBoundSessions.RelyingPartyWellKnown.Network.Result",
        url_fetcher_->net_error(), response_code);

    if (url_fetcher_->net_error() != OK) {
      return SessionError::ErrorType::kRelyingPartyWellKnownUnavailable;
    }

    if (!headers || headers->response_code() != 200) {
      return SessionError::ErrorType::kRelyingPartyWellKnownUnavailable;
    }

    std::optional<WellKnownParams> maybe_params =
        ParseWellKnownJson(url_fetcher_->data_received());
    if (!maybe_params.has_value()) {
      return SessionError::ErrorType::kRelyingPartyWellKnownMalformed;
    }

    if (maybe_params->relying_origins.has_value()) {
      return SessionError::ErrorType::kRelyingPartyWellKnownMalformed;
    }

    if (!maybe_params->provider_origin.has_value() ||
        url::Origin::Create(provider_url_).Serialize() !=
            *maybe_params->provider_origin) {
      return SessionError::ErrorType::kFederatedNotAuthorized;
    }

    return SessionError::ErrorType::kSuccess;
  }

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
      std::optional<RegistrationFetcher::RegistrationToken>
          registration_token) {
    if (!registration_token) {
      number_of_signing_failures_++;
      if (number_of_signing_failures_ < kMaxSigningFailures) {
        AttemptChallengeSigning();
        return;
      } else {
        RunCallback(RegistrationResult(
            SessionError{SessionError::ErrorType::kSigningError}));
        // `this` may be deleted.
        return;
      }
    }

    url_fetcher_ = std::make_unique<URLFetcher>(context_, fetcher_endpoint_,
                                                net_log_source_);
    ConfigureRequest(url_fetcher_->request());
    url_fetcher_->request().SetExtraRequestHeaderByName(
        kJwtSessionHeaderName, registration_token.value(), /*overwrite*/ true);

    // `this` owns `url_fetcher_`, so it's safe to use
    // `base::Unretained`
    url_fetcher_->Start(base::BindOnce(
        &RegistrationFetcherImpl::OnRequestComplete, base::Unretained(this)));
  }

  void ConfigureRequest(URLRequest& request) {
    CHECK(IsSecure(fetcher_endpoint_));
    request.set_method("POST");
    request.SetLoadFlags(LOAD_DISABLE_CACHE);
    request.set_allow_credentials(true);

    request.set_site_for_cookies(isolation_info_.site_for_cookies());
    request.set_initiator(original_request_initiator_);
    request.set_isolation_info(isolation_info_);

    if (IsForRefreshRequest()) {
      request.SetExtraRequestHeaderByName(
          kSessionIdHeaderName, *session_identifier_, /*overwrite*/ true);
    }
  }

  void OnChallengeNeeded(
      std::optional<std::vector<SessionChallengeParam>> challenge_params) {
    if (!challenge_params || challenge_params->empty()) {
      RunCallback(RegistrationResult(
          SessionError{SessionError::ErrorType::kInvalidChallenge}));
      // `this` may be deleted.
      return;
    }

    // TODO(crbug.com/438783634): Log if there is more than one challenge
    // TODO(crbug.com/438783634): Handle if session identifiers don't match
    const std::string& challenge = (*challenge_params)[0].challenge();
    StartFetch(challenge, std::nullopt);
  }

  void OnRequestComplete() {
    HttpResponseHeaders* headers = url_fetcher_->request().response_headers();
    const int response_code = headers ? headers->response_code() : 0;
    const char* histogram_name =
        IsForRefreshRequest()
            ? "Net.DeviceBoundSessions.Refresh.Network.Result"
            : "Net.DeviceBoundSessions.Registration.Network.Result";
    RecordHttpResponseOrErrorCode(histogram_name, url_fetcher_->net_error(),
                                  response_code);

    if (url_fetcher_->net_error() != OK) {
      RunCallback(
          RegistrationResult(SessionError{SessionError::ErrorType::kNetError}));
      // *this is deleted here.
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
      RunCallback(RegistrationResult(
          SessionError{SessionError::ErrorType::kPersistentHttpError}));
      // *this is deleted here
      return;
    } else if (response_code == 407) {
      // Proxy errors are treated as network errors
      RunCallback(
          RegistrationResult(SessionError{SessionError::ErrorType::kNetError}));
      // *this is deleted here
      return;
    } else if (300 <= response_code && response_code < 500) {
      RunCallback(RegistrationResult(
          SessionError{SessionError::ErrorType::kPersistentHttpError}));
      // *this is deleted here
      return;
    } else if (response_code >= 500) {
      RunCallback(RegistrationResult(
          SessionError{SessionError::ErrorType::kTransientHttpError}));
      // *this is deleted here
      return;
    }

    base::expected<SessionParams, SessionError> params_or_error =
        ParseSessionInstructionJson(url_fetcher_->request().url(), *key_id_,
                                    session_identifier_,
                                    url_fetcher_->data_received());
    if (!params_or_error.has_value()) {
      RunCallback(RegistrationResult(std::move(params_or_error).error()));
      // *this is deleted here.
      return;
    }

    base::expected<std::unique_ptr<Session>, SessionError> session_or_error =
        Session::CreateIfValid(params_or_error.value());
    if (!session_or_error.has_value()) {
      RunCallback(RegistrationResult(std::move(session_or_error).error()));
      // *this is deleted here
      return;
    }

    // Session::CreateIfValid confirms that the registration endpoint is
    // same-site with the scope origin. But we still need to validate
    // that this subdomain is allowed to register a session for the
    // whole site.
    if (features::kDeviceBoundSessionsCheckSubdomainRegistration.Get() &&
        base::FeatureList::IsEnabled(
            features::kDeviceBoundSessionsOriginTrialFeedback) &&
        !IsForRefreshRequest() && params_or_error->scope.include_site &&
        // Skip all validations if the fetcher endpoint is not a subdomain but
        // rather the top-level site (which matches the origin when including
        // the site).
        fetcher_endpoint_.host() != (*session_or_error)->origin().host()) {
      GURL::Replacements replacements;
      replacements.SetPathStr("/.well-known/device-bound-sessions");
      replacements.SetHostStr((*session_or_error)->origin().host());
      GURL well_known_url =
          fetcher_endpoint_.ReplaceComponents(std::move(replacements));
      url_fetcher_ = std::make_unique<URLFetcher>(context_, well_known_url,
                                                  net_log_source_);
      url_fetcher_->request().set_method("GET");
      url_fetcher_->request().set_allow_credentials(false);
      url_fetcher_->request().set_site_for_cookies(
          isolation_info_.site_for_cookies());
      url_fetcher_->request().set_initiator(original_request_initiator_);
      url_fetcher_->request().set_isolation_info(isolation_info_);
      url_fetcher_->Start(
          base::BindOnce(&RegistrationFetcherImpl::
                             OnSubdomainRegistrationWellKnownRequestComplete,
                         GetWeakPtr(), std::move(*session_or_error)));
      return;
    }

    RunCallback(RegistrationResult(std::move(session_or_error)));
    // *this is deleted here
  }

  void OnSubdomainRegistrationWellKnownRequestComplete(
      std::unique_ptr<Session> session) {
    RunCallback(OnSubdomainRegistrationWellKnownRequestCompleteInternal(
        std::move(session)));
    // *this is deleted here.
  }

  RegistrationResult OnSubdomainRegistrationWellKnownRequestCompleteInternal(
      std::unique_ptr<Session> session) {
    HttpResponseHeaders* headers = url_fetcher_->request().response_headers();
    const int response_code = headers ? headers->response_code() : 0;
    RecordHttpResponseOrErrorCode(
        "Net.DeviceBoundSessions.SubdomainWellKnown.Network.Result",
        url_fetcher_->net_error(), response_code);

    if (url_fetcher_->net_error() != OK) {
      return RegistrationResult(SessionError{
          SessionError::ErrorType::kSubdomainRegistrationWellKnownUnavailable});
    }

    if (!headers || headers->response_code() != 200) {
      return RegistrationResult(SessionError{
          SessionError::ErrorType::kSubdomainRegistrationWellKnownUnavailable});
    }

    std::optional<WellKnownParams> maybe_params =
        ParseWellKnownJson(url_fetcher_->data_received());
    if (!maybe_params.has_value()) {
      return RegistrationResult(SessionError{
          SessionError::ErrorType::kSubdomainRegistrationWellKnownMalformed});
    }

    if (!maybe_params->registering_origins.has_value() ||
        !base::Contains(*maybe_params->registering_origins,
                        url::Origin::Create(fetcher_endpoint_).Serialize())) {
      return RegistrationResult(SessionError{
          SessionError::ErrorType::kSubdomainRegistrationUnauthorized});
    }

    return RegistrationResult(std::move(session));
  }

  void RunCallback(RegistrationResult registration_result) {
    AddNetLogResult(registration_result);
    std::move(callback_).Run(this, std::move(registration_result));
  }

  void AddNetLogResult(const RegistrationResult& registration_result) {
    if (!url_fetcher_) {
      return;
    }
    NetLogEventType result_event_type =
        IsForRefreshRequest() ? NetLogEventType::DBSC_REFRESH_RESULT
                              : NetLogEventType::DBSC_REGISTRATION_RESULT;
    url_fetcher_->request().net_log().AddEvent(result_event_type, [&]() {
      std::string result;
      if (registration_result.is_session()) {
        result = IsForRefreshRequest() ? "refreshed" : "registered";
      } else {
        const SessionError& error = registration_result.error();
        if (error.GetDeletionReason().has_value()) {
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

  std::unique_ptr<URLFetcher> url_fetcher_;

  GURL provider_url_;
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
        void(std::optional<RegistrationFetcher::RegistrationToken>)> callback) {
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
                 std::optional<RegistrationFetcher::RegistrationToken>)>
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
