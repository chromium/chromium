// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/registration_fetcher.h"

#include <algorithm>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ref.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/types/expected_macros.h"
#include "base/types/optional_util.h"
#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "components/unexportable_keys/unexportable_key_service.h"
#include "crypto/sign.h"
#include "crypto/unexportable_key.h"
#include "net/base/features.h"
#include "net/base/net_errors.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "net/device_bound_sessions/registration_request_param.h"
#include "net/device_bound_sessions/session_binding_utils.h"
#include "net/device_bound_sessions/session_challenge_param.h"
#include "net/device_bound_sessions/session_error.h"
#include "net/device_bound_sessions/session_json_utils.h"
#include "net/device_bound_sessions/session_key.h"
#include "net/device_bound_sessions/session_params.h"
#include "net/device_bound_sessions/url_fetcher.h"
#include "net/http/http_request_headers.h"
#include "net/log/net_log_event_type.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_request_context.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "url/origin.h"

namespace net::device_bound_sessions {

namespace {

constexpr char kSessionIdHeaderName[] = "Sec-Secure-Session-Id";
constexpr char kJwtSessionHeaderName[] = "Secure-Session-Response";

void RecordHttpResponseOrErrorCode(std::string_view metric_name,
                                   int net_error,
                                   int http_response_code) {
  // No need to special-case `net::ERR_HTTP_RESPONSE_CODE_FAILURE` to return
  // the HTTP response code, because `UrlRequest` does not use that net error.
  base::UmaHistogramSparse(
      metric_name, net_error == net::OK ? http_response_code : net_error);
}

void RecordNetworkResultMetrics(bool is_for_refresh,
                                size_t attempts_made,
                                int net_error,
                                int http_response_code) {
  std::string_view histogram_name =
      is_for_refresh ? "Net.DeviceBoundSessions.Refresh.Network.Result"
                     : "Net.DeviceBoundSessions.Registration.Network.Result";
  RecordHttpResponseOrErrorCode(histogram_name, net_error, http_response_code);
  if (is_for_refresh) {
    std::string_view attempt_histogram =
        (attempts_made == 1)
            ? "Net.DeviceBoundSessions.Refresh.Network.Result.FirstAttempt"
            : "Net.DeviceBoundSessions.Refresh.Network.Result.SecondAttempt";
    RecordHttpResponseOrErrorCode(attempt_histogram, net_error,
                                  http_response_code);
  }
}

// Invokes `callback` with `result`, mapping any transient errors to
// `kTransientSigningError` and persistent errors to `persistent_error`.
void RunSessionCallback(
    base::OnceCallback<
        void(SessionErrorOr<RegistrationFetcher::RegistrationToken>)> callback,
    SessionError::ErrorType persistent_error,
    unexportable_keys::ServiceErrorOr<RegistrationFetcher::RegistrationToken>
        result) {
  std::move(callback).Run(result.transform_error(
      [persistent_error](unexportable_keys::ServiceError error) {
        return unexportable_keys::IsPersistentError(error)
                   ? persistent_error
                   : SessionError::kTransientSigningError;
      }));
}

// Holds the signature algorithm and SubjectPublicKeyInfo (SPKI) bytes of an
// unexportable key.
struct KeyInfo {
  crypto::sign::SignatureKind algorithm;
  std::vector<uint8_t> pubkey;
};

// Retrieves the signature algorithm and SubjectPublicKeyInfo (SPKI) for
// `key_id` from `key_service`.
unexportable_keys::ServiceErrorOr<KeyInfo> GetKeyInfo(
    unexportable_keys::UnexportableKeyService& key_service,
    unexportable_keys::UnexportableSigningKeyId key_id) {
  KeyInfo info;
  ASSIGN_OR_RETURN(info.algorithm, key_service.GetAlgorithm(key_id));
  ASSIGN_OR_RETURN(info.pubkey, key_service.GetSubjectPublicKeyInfo(key_id));
  return info;
}

// Creates the unsigned JWT header and payload for either session registration
// or refresh.
unexportable_keys::ServiceErrorOr<std::string> CreateInnerHeaderAndPayload(
    bool is_for_refresh,
    unexportable_keys::UnexportableKeyService& unexportable_key_service,
    unexportable_keys::UnexportableSigningKeyId key_id,
    std::optional<std::string> challenge,
    std::optional<std::string> authorization) {
  ASSIGN_OR_RETURN(KeyInfo key_info,
                   GetKeyInfo(unexportable_key_service, key_id));
  return base::OptionalToExpected(
      is_for_refresh ? CreateKeyRefreshHeaderAndPayload(std::move(challenge),
                                                        key_info.algorithm)
                     : CreateKeyRegistrationHeaderAndPayload(
                           std::move(challenge), key_info.algorithm,
                           key_info.pubkey, std::move(authorization)),
      unexportable_keys::ServiceError::kCryptoApiFailed);
}

// Appends the signature resulting from key signing to `header_and_payload`
// using key info from `key_service`.
unexportable_keys::ServiceErrorOr<std::string> AppendSignature(
    unexportable_keys::UnexportableKeyService& key_service,
    unexportable_keys::UnexportableSigningKeyId key_id,
    std::string_view header_and_payload,
    unexportable_keys::ServiceErrorOr<std::vector<uint8_t>> sign_result) {
  ASSIGN_OR_RETURN(std::vector<uint8_t> signature, std::move(sign_result));
  ASSIGN_OR_RETURN(KeyInfo key_info, GetKeyInfo(key_service, key_id));
  return base::OptionalToExpected(
      AppendSignatureToHeaderAndPayload(header_and_payload, key_info.algorithm,
                                        key_info.pubkey, signature),
      unexportable_keys::ServiceError::kCryptoApiFailed);
}

// Asynchronously creates and signs a registration or refresh token using
// `key_id` without attestation.
void SignChallengeWithKey(
    bool is_for_refresh,
    unexportable_keys::UnexportableKeyService& unexportable_key_service,
    unexportable_keys::UnexportableSigningKeyId key_id,
    unexportable_keys::BackgroundTaskPriority priority,
    std::optional<std::string> challenge,
    std::optional<std::string> authorization,
    base::OnceCallback<void(
        SessionErrorOr<RegistrationFetcher::RegistrationToken>)> callback) {
  ASSIGN_OR_RETURN(std::string header_and_payload,
                   CreateInnerHeaderAndPayload(
                       is_for_refresh, unexportable_key_service, key_id,
                       std::move(challenge), std::move(authorization)),
                   [&](unexportable_keys::ServiceError error) {
                     RunSessionCallback(std::move(callback),
                                        SessionError::kSigningError,
                                        base::unexpected(error));
                   });

  // TODO(crbug.com/501306421): Encapsulate this flow into a dedicated helper
  // class owned by `RegistrationFetcherImpl` to establish clear object
  // ownership, consolidate signing logic, and support early cancellation via
  // `base::WeakPtr` if `RegistrationFetcherImpl` is destroyed during background
  // operations.
  unexportable_key_service.SignSlowlyAsync(
      key_id, base::as_byte_span(header_and_payload), priority,
      base::BindOnce(&AppendSignature, std::ref(unexportable_key_service),
                     key_id, header_and_payload)
          .Then(base::BindOnce(&RunSessionCallback, std::move(callback),
                               SessionError::kSigningError)));
}

// Tracks intermediate state across asynchronous operations when creating an
// attested registration token.
struct AttestedTokenSigningState {
  const raw_ref<unexportable_keys::UnexportableKeyService> key_service;
  const unexportable_keys::UnexportableAttestationKeyId attestation_key_id;
  const unexportable_keys::BackgroundTaskPriority priority;
  const std::string audience;
  base::OnceCallback<void(
      SessionErrorOr<RegistrationFetcher::RegistrationToken>)>
      callback;

  std::optional<unexportable_keys::ServiceErrorOr<std::string>> inner_jws;
  std::optional<unexportable_keys::ServiceErrorOr<crypto::AttestationStatement>>
      attestation_statement;
};

// Creates the unsigned outer JWT header and payload wrapping `inner_jws` with
// an attestation statement.
unexportable_keys::ServiceErrorOr<std::string> CreateOuterHeaderAndPayload(
    unexportable_keys::UnexportableKeyService& key_service,
    unexportable_keys::UnexportableAttestationKeyId attestation_key_id,
    std::string_view inner_jws,
    const crypto::AttestationStatement& attestation_statement,
    std::string_view audience) {
  ASSIGN_OR_RETURN(KeyInfo aik_info,
                   GetKeyInfo(key_service, attestation_key_id));
  return base::OptionalToExpected(
      CreateOuterRegistrationHeaderAndPayload(inner_jws, aik_info.algorithm,
                                              aik_info.pubkey, audience,
                                              attestation_statement),
      unexportable_keys::ServiceError::kCryptoApiFailed);
}

// Combines the inner token and attestation statement into an outer token, then
// initiates signing with the attestation identity key.
void SignOuterToken(std::unique_ptr<AttestedTokenSigningState> state) {
  CHECK(state->inner_jws);
  CHECK(state->attestation_statement);

  ASSIGN_OR_RETURN(const std::string& inner_jws, *state->inner_jws,
                   [&](unexportable_keys::ServiceError error) {
                     RunSessionCallback(std::move(state->callback),
                                        SessionError::kSigningError,
                                        base::unexpected(error));
                   });
  ASSIGN_OR_RETURN(const crypto::AttestationStatement& attestation_stmt,
                   *state->attestation_statement,
                   [&](unexportable_keys::ServiceError error) {
                     RunSessionCallback(
                         std::move(state->callback),
                         SessionError::kAttestationCertificationError,
                         base::unexpected(error));
                   });
  ASSIGN_OR_RETURN(std::string header_and_payload,
                   CreateOuterHeaderAndPayload(
                       *state->key_service, state->attestation_key_id,
                       inner_jws, attestation_stmt, state->audience),
                   [&](unexportable_keys::ServiceError error) {
                     RunSessionCallback(std::move(state->callback),
                                        SessionError::kAttestationSigningError,
                                        base::unexpected(error));
                   });

  state->key_service->SignSlowlyAsync(
      state->attestation_key_id, base::as_byte_span(header_and_payload),
      state->priority,
      base::BindOnce(&AppendSignature, std::ref(*state->key_service),
                     state->attestation_key_id, header_and_payload)
          .Then(base::BindOnce(&RunSessionCallback, std::move(state->callback),
                               SessionError::kAttestationSigningError)));
}

// Stores `value` into `target` and runs `done_closure`.
template <typename T>
auto StoreAndRun(std::optional<T>& target, base::OnceClosure done_closure) {
  return base::BindOnce(
      [](std::optional<T>& target, base::OnceClosure done_closure, T value) {
        target = std::move(value);
        std::move(done_closure).Run();
      },
      std::ref(target), std::move(done_closure));
}

// Asynchronously creates and signs an attested registration token using
// `key_id` and `attestation_key_id`.
void SignChallengeWithAttestationKey(
    unexportable_keys::UnexportableKeyService& unexportable_key_service,
    unexportable_keys::UnexportableSigningKeyId key_id,
    unexportable_keys::UnexportableAttestationKeyId attestation_key_id,
    unexportable_keys::BackgroundTaskPriority priority,
    std::string audience,
    std::optional<std::string> challenge,
    std::optional<std::string> authorization,
    base::OnceCallback<void(
        SessionErrorOr<RegistrationFetcher::RegistrationToken>)> callback) {
  ASSIGN_OR_RETURN(std::string header_and_payload,
                   CreateInnerHeaderAndPayload(
                       /*is_for_refresh=*/false, unexportable_key_service,
                       key_id, challenge, std::move(authorization)),
                   [&](unexportable_keys::ServiceError error) {
                     RunSessionCallback(std::move(callback),
                                        SessionError::kSigningError,
                                        base::unexpected(error));
                   });

  // SAFETY: `key_service` is referenced via `raw_ref` in
  // `AttestedTokenSigningState`. The caller ensures `unexportable_key_service`
  // outlives the async token creation request. When both asynchronous signing
  // and certification tasks complete, `base::BarrierClosure` synchronously
  // invokes `SignOuterToken`, which performs the final signing step before
  // releasing `state`.
  auto state = base::WrapUnique(new AttestedTokenSigningState{
      .key_service{unexportable_key_service},
      .attestation_key_id = attestation_key_id,
      .priority = priority,
      .audience = std::move(audience),
      .callback = std::move(callback),
  });

  // TODO(crbug.com/501306421): Encapsulate this flow into a dedicated helper
  // class owned by `RegistrationFetcherImpl` to establish clear object
  // ownership, consolidate signing logic, and support early cancellation via
  // `base::WeakPtr` if `RegistrationFetcherImpl` is destroyed during background
  // operations.
  AttestedTokenSigningState& state_ref = *state;
  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      2, base::BindOnce(&SignOuterToken, std::move(state)));

  unexportable_key_service.SignSlowlyAsync(
      key_id, base::as_byte_span(header_and_payload), priority,
      base::BindOnce(&AppendSignature, std::ref(unexportable_key_service),
                     key_id, header_and_payload)
          .Then(StoreAndRun(state_ref.inner_jws, barrier_closure)));

  unexportable_key_service.CertifySlowlyAsync(
      attestation_key_id, key_id, base::as_byte_span(challenge.value_or("")),
      priority,
      StoreAndRun(state_ref.attestation_statement, std::move(barrier_closure)));
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

    if (!labels_seen.contains(label)) {
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
      url::Origin referring_origin,
      std::optional<std::string> session_identifier,
      SessionService& session_service,
      unexportable_keys::UnexportableKeyService& key_service,
      const URLRequestContext* context,
      const IsolationInfo& isolation_info,
      const net::SiteForCookies& site_for_cookies,
      std::optional<NetLogSource> net_log_source,
      const std::optional<url::Origin>& original_request_initiator,
      unexportable_keys::BackgroundTaskPriority priority)
      : fetcher_endpoint_(fetcher_endpoint),
        referring_origin_(std::move(referring_origin)),
        session_identifier_(std::move(session_identifier)),
        session_service_(session_service),
        key_service_(key_service),
        context_(context),
        isolation_info_(isolation_info),
        site_for_cookies_(site_for_cookies),
        net_log_source_(std::move(net_log_source)),
        original_request_initiator_(original_request_initiator),
        priority_(priority) {}

  ~RegistrationFetcherImpl() override {}

  void OnSigningKeyGenerated(
      base::OnceClosure closure,
      unexportable_keys::ServiceErrorOr<
          unexportable_keys::UnexportableSigningKeyId> key_id_or_error) {
    ASSIGN_OR_RETURN(key_id_, key_id_or_error, [this](auto) {
      RunCallback(CreateErrorRegistrationResult(
          SessionError(SessionError::kSigningKeyGenerationError)));
      // `this` may be deleted.
    });

    std::move(closure).Run();
  }

  void OnAttestationKeyGenerated(
      base::OnceClosure closure,
      unexportable_keys::ServiceErrorOr<
          unexportable_keys::UnexportableAttestationKeyId> key_id_or_error) {
    ASSIGN_OR_RETURN(attestation_key_id_, key_id_or_error, [this](auto) {
      RunCallback(CreateErrorRegistrationResult(
          SessionError(SessionError::kAttestationKeyGenerationError)));
      // `this` may be deleted.
    });

    std::move(closure).Run();
  }

  SessionError FillSessionError(SessionError error) {
    if (!url_fetcher_) {
      return error;
    }
    FailedRequest failed_request;
    if (url_fetcher_->net_error() != OK) {
      failed_request.request_url = url_fetcher_->request().url();
      failed_request.net_error = url_fetcher_->net_error();
      error.failed_request = std::move(failed_request);
      return error;
    }
    HttpResponseHeaders* headers = url_fetcher_->request().response_headers();
    if (headers && headers->response_code() != 200) {
      failed_request.request_url = url_fetcher_->request().url();
      failed_request.response_error = headers->response_code();
      if (!url_fetcher_->data_received().empty()) {
        failed_request.response_error_body = url_fetcher_->TakeDataReceived();
      }
      error.failed_request = std::move(failed_request);
    }
    return error;
  }
  RegistrationResult CreateErrorRegistrationResult(SessionError error) {
    return RegistrationResult(FillSessionError(std::move(error)));
  }

  void StartFetch(std::optional<std::string> challenge,
                  std::optional<std::string> authorization) {
    CHECK(key_id_.has_value());

    current_challenge_ = std::move(challenge);
    current_authorization_ = std::move(authorization);

    if (current_challenge_.has_value() || current_authorization_.has_value()) {
      number_of_challenges_++;
      if (number_of_challenges_ < kMaxChallenges) {
        AttemptChallengeSigning();
        // `this` may be deleted.
        return;
      } else {
        RunCallback(CreateErrorRegistrationResult(
            SessionError(SessionError::kTooManyChallenges)));
        // `this` may be deleted.
        return;
      }
    }

    StartFetcherEndpointRequest();
  }

  base::WeakPtr<RegistrationFetcherImpl> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void StartCreateTokenAndFetch(
      RegistrationRequestParam& registration_params,
      base::span<const crypto::sign::SignatureKind> supported_algos,
      RegistrationCompleteCallback callback) override {
    // Using mock fetcher for testing.
    if (g_mock_fetcher) {
      g_mock_fetcher->Run(std::move(callback));
      // `this` may be deleted.
      return;
    }

    CHECK(callback_.is_null());
    callback_ = std::move(callback);
    CHECK(callback_);

    const bool aik_required =
        registration_params.attestation_mode() == AttestationMode::kRequired;
    if (aik_required) {
      CHECK(registration_params.challenge().has_value());
    }
    base::RepeatingClosure barrier_closure = base::BarrierClosure(
        aik_required ? 2 : 1,
        base::BindOnce(&RegistrationFetcherImpl::StartFetch, GetWeakPtr(),
                       registration_params.TakeChallenge(),
                       registration_params.TakeAuthorization()));

    if (aik_required) {
      key_service_->GenerateAttestationKeySlowlyAsync(
          supported_algos, priority_,
          base::BindOnce(&RegistrationFetcherImpl::OnAttestationKeyGenerated,
                         GetWeakPtr(), barrier_closure));
    }

    key_service_->GenerateSigningKeySlowlyAsync(
        supported_algos, priority_,
        base::BindOnce(&RegistrationFetcherImpl::OnSigningKeyGenerated,
                       GetWeakPtr(), std::move(barrier_closure)));
    // `this` may be deleted.
  }

  void StartFetchWithFederatedKey(
      RegistrationRequestParam& request_params,
      unexportable_keys::UnexportableSigningKeyId key_id,
      const GURL& provider_url,
      RegistrationCompleteCallback callback) override {
    // Using mock fetcher for testing.
    if (g_mock_fetcher) {
      g_mock_fetcher->Run(std::move(callback));
      // `this` may be deleted.
      return;
    }

    CHECK(callback_.is_null());
    callback_ = std::move(callback);

    key_id_ = key_id;
    provider_url_ = provider_url;

    if (!features::kDeviceBoundSessionsFederatedRegistrationCheckWellKnown
             .Get()) {
      StartFetch(request_params.TakeChallenge(),
                 request_params.TakeAuthorization());
      // `this` may be deleted.
      return;
    }

    GURL::Replacements replacements;
    replacements.SetPathStr("/.well-known/device-bound-sessions");
    GURL well_known_url = provider_url_.ReplaceComponents(replacements);
    // TODO(crbug.com/495096658): Assert that `IsForRefreshRequest()` is false
    // once the tests are fixed.
    url_fetcher_ = std::make_unique<URLFetcher>(
        context_, well_known_url, referring_origin_, net_log_source_,
        IsForRefreshRequest());
    ConfigureWellKnownRequest(url_fetcher_->request());
    url_fetcher_->Start(base::BindOnce(
        &RegistrationFetcherImpl::OnProviderWellKnownRequestComplete,
        GetWeakPtr(), request_params.TakeChallenge(),
        request_params.TakeAuthorization()));
  }

  void StartFetchWithExistingKey(
      RegistrationRequestParam& request_params,
      unexportable_keys::UnexportableSigningKeyId key_id,
      RegistrationCompleteCallback callback) override {
    // Using mock fetcher for testing.
    if (g_mock_fetcher) {
      g_mock_fetcher->Run(std::move(callback));
      // `this` may be deleted.
      return;
    }

    CHECK(callback_.is_null());
    callback_ = std::move(callback);

    key_id_ = key_id;

    StartFetch(request_params.TakeChallenge(),
               request_params.TakeAuthorization());
    // `this` may be deleted.
  }

 private:
  // Consolidates common URLRequest initialization, credential isolation, and
  // W3C Resource-Isolation 'no-cors'/'empty' Fetch-Metadata parameters utilized
  // uniformly across both Discovery (GET) and Registration (POST) DBSC
  // operations.
  // TODO(crbug.com/546625323): Stop duplicating request header and
  // Fetch-Metadata population logic, and explore offloading header construction
  // to `SessionService` or reusing `//services/network/sec_header_helpers.h`.
  void SetupCommonFetchMetadata(URLRequest& request) {
    request.set_site_for_cookies(site_for_cookies_);
    request.set_initiator(original_request_initiator_);
    request.set_isolation_info(isolation_info_);

    request.SetExtraRequestHeaderByName(kSecFetchModeHeaderName, "no-cors",
                                        /*overwrite=*/true);
    request.SetExtraRequestHeaderByName(kSecFetchDestHeaderName, "empty",
                                        /*overwrite=*/true);
    request.SetExtraRequestHeaderByName(
        kSecFetchSiteHeaderName,
        SecFetchSiteForReferringOrigin(referring_origin_, request.url()),
        /*overwrite=*/true);
  }

  // Configures and decorates an outbound `.well-known` DBSC discovery
  // URLRequest via a side-effect-free, credentialless HTTP GET operation.
  void ConfigureWellKnownRequest(URLRequest& request) {
    request.set_method("GET");
    request.set_disallow_credentials();
    SetupCommonFetchMetadata(request);
  }

  void StartFetcherEndpointRequest() {
    url_fetcher_ = std::make_unique<URLFetcher>(
        context_, fetcher_endpoint_, referring_origin_, net_log_source_,
        IsForRefreshRequest());
    ConfigureRequest(url_fetcher_->request());
    if (last_registration_token_.has_value()) {
      url_fetcher_->request().SetExtraRequestHeaderByName(
          kJwtSessionHeaderName, last_registration_token_.value(),
          /*overwrite=*/true);
    }
    // `this` owns `url_fetcher_`, so it's safe to use `base::Unretained`
    url_fetcher_->Start(base::BindOnce(
        &RegistrationFetcherImpl::OnRequestComplete, base::Unretained(this)));
  }

  void OnProviderWellKnownRequestComplete(
      std::optional<std::string> challenge,
      std::optional<std::string> authorization) {
    SessionError::ErrorType error =
        OnProviderWellKnownRequestCompleteInternal();
    if (error != SessionError::kSuccess) {
      RunCallback(CreateErrorRegistrationResult(SessionError(error)));
      // `this` may be deleted.
      return;
    }

    GURL::Replacements replacements;
    replacements.SetPathStr("/.well-known/device-bound-sessions");
    GURL well_known_url = fetcher_endpoint_.ReplaceComponents(replacements);
    url_fetcher_ = std::make_unique<URLFetcher>(
        context_, well_known_url, referring_origin_, net_log_source_,
        IsForRefreshRequest());
    ConfigureWellKnownRequest(url_fetcher_->request());
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
      return SessionError::kSessionProviderWellKnownUnavailable;
    }

    if (!headers || headers->response_code() != 200) {
      return SessionError::kSessionProviderWellKnownUnavailable;
    }

    std::optional<WellKnownParams> maybe_params =
        ParseWellKnownJson(url_fetcher_->data_received());
    if (!maybe_params.has_value()) {
      return SessionError::kSessionProviderWellKnownMalformed;
    }

    if (maybe_params->provider_origin.has_value()) {
      return SessionError::kSessionProviderWellKnownHasProviderOrigin;
    }

    // TODO(crbug.com/511776603): Evaluate whether to use the final redirect URL
    // instead of the original URL here in a follow-up.
    std::string target_origin =
        url::Origin::Create(fetcher_endpoint_).Serialize();
    if (!maybe_params->relying_origins.has_value() ||
        !std::ranges::contains(*maybe_params->relying_origins, target_origin)) {
      return SessionError::kFederatedNotAuthorizedByProvider;
    }

    if (!WithinOriginLabelLimit(*maybe_params->relying_origins,
                                target_origin)) {
      return SessionError::kTooManyRelyingOriginLabels;
    }

    return SessionError::kSuccess;
  }

  void OnRelyingPartyWellKnownRequestComplete(
      std::optional<std::string> challenge,
      std::optional<std::string> authorization) {
    SessionError::ErrorType error =
        OnRelyingPartyWellKnownRequestCompleteInternal();
    if (error != SessionError::kSuccess) {
      RunCallback(CreateErrorRegistrationResult(SessionError(error)));
      // `this` may be deleted.
      return;
    }

    StartFetch(std::move(challenge), std::move(authorization));
    // `this` may be deleted.
  }

  SessionError::ErrorType OnRelyingPartyWellKnownRequestCompleteInternal() {
    HttpResponseHeaders* headers = url_fetcher_->request().response_headers();
    const int response_code = headers ? headers->response_code() : 0;
    RecordHttpResponseOrErrorCode(
        "Net.DeviceBoundSessions.RelyingPartyWellKnown.Network.Result",
        url_fetcher_->net_error(), response_code);

    if (url_fetcher_->net_error() != OK) {
      return SessionError::kRelyingPartyWellKnownUnavailable;
    }

    if (!headers || headers->response_code() != 200) {
      return SessionError::kRelyingPartyWellKnownUnavailable;
    }

    std::optional<WellKnownParams> maybe_params =
        ParseWellKnownJson(url_fetcher_->data_received());
    if (!maybe_params.has_value()) {
      return SessionError::kRelyingPartyWellKnownMalformed;
    }

    if (maybe_params->relying_origins.has_value()) {
      return SessionError::kRelyingPartyWellKnownHasRelyingOrigins;
    }

    // TODO(crbug.com/511776603): Evaluate whether to use the final redirect URL
    // instead of the original URL here in a follow-up.
    if (!maybe_params->provider_origin.has_value() ||
        url::Origin::Create(provider_url_).Serialize() !=
            *maybe_params->provider_origin) {
      return SessionError::kFederatedNotAuthorizedByRelyingParty;
    }

    return SessionError::kSuccess;
  }

  static constexpr size_t kMaxChallenges = 5;

  void AttemptChallengeSigning() {
    base::OnceCallback<void(
        SessionErrorOr<RegistrationFetcher::RegistrationToken>)>
        callback =
            base::BindOnce(&RegistrationFetcherImpl::OnRegistrationTokenCreated,
                           GetWeakPtr(), current_challenge_, *key_id_);

    SchemefulSite site(fetcher_endpoint_);
    if (IsForRefreshRequest()) {
      SessionKey session_key{site, Session::Id(*session_identifier_)};
      const SessionService::SignedRefreshChallenge* signed_refresh_challenge =
          session_service_->GetLatestSignedRefreshChallenge(session_key);
      // If we already have a matching signed refresh challenge, we
      // can skip past the signing. We know we have a
      // `current_challenge_` here because this block is behind
      // `IsForRefreshRequest()`.
      if (signed_refresh_challenge &&
          signed_refresh_challenge->challenge == *current_challenge_ &&
          signed_refresh_challenge->key_id == *key_id_) {
        std::move(callback).Run(signed_refresh_challenge->signed_challenge);
        // `this` may be deleted.
        return;
      }
    }

    // Now, right before signing, we check whether the signing quota is
    // exceeded. Note this callback is intentionally different from the one
    // defined above.
    if (session_service_->SigningQuotaExceeded(site)) {
      RunCallback(CreateErrorRegistrationResult(
          SessionError(SessionError::kSigningQuotaExceeded)));
      // `this` may be deleted.
      return;
    }
    // Track a new signing attempt.
    session_service_->AddSigningOccurrence(site);

    if (attestation_key_id_.has_value()) {
      // AIK attestation is strictly supported for registration requests, not
      // refresh requests.
      CHECK(!IsForRefreshRequest());
      SignChallengeWithAttestationKey(
          *key_service_, *key_id_, *attestation_key_id_, priority_,
          fetcher_endpoint_.spec(), current_challenge_, current_authorization_,
          std::move(callback));
    } else {
      SignChallengeWithKey(IsForRefreshRequest(), *key_service_, *key_id_,
                           priority_, current_challenge_,
                           current_authorization_, std::move(callback));
    }
    // `this` may be deleted.
  }

  void OnRegistrationTokenCreated(
      std::optional<std::string> challenge,
      unexportable_keys::UnexportableSigningKeyId key_id,
      SessionErrorOr<RegistrationFetcher::RegistrationToken>
          registration_token) {
    if (!registration_token.has_value()) {
      RunCallback(CreateErrorRegistrationResult(
          SessionError(registration_token.error())));
      // `this` may be deleted.
      return;
    }
    last_registration_token_ = std::move(registration_token).value();

    // Cache the signed refresh challenge in case the same challenge is
    // attempted next time (e.g. if refresh transiently fails).
    if (IsForRefreshRequest() && challenge.has_value()) {
      SessionKey session_key{SchemefulSite(fetcher_endpoint_),
                             Session::Id(*session_identifier_)};
      SessionService::SignedRefreshChallenge signed_refresh_challenge = {
          .signed_challenge = last_registration_token_.value(),
          .challenge = std::move(*challenge),
          .key_id = key_id,
      };
      session_service_->SetLatestSignedRefreshChallenge(
          std::move(session_key), std::move(signed_refresh_challenge));
    }

    StartFetcherEndpointRequest();
  }

  void ConfigureRequest(URLRequest& request) {
    CHECK(IsSecure(fetcher_endpoint_));
    request.set_method("POST");
    request.SetLoadFlags(LOAD_DISABLE_CACHE);

    // Apply baseline W3C Fetch-Metadata ('no-cors', 'empty', 'Sec-Fetch-Site')
    // and isolation boundaries utilized across all DBSC request pipelines.
    SetupCommonFetchMetadata(request);

    // The endpoint may be a different (same-site) origin from the response or
    // session that configured it, so attach `Origin` and Fetch Metadata
    // headers reflecting that relationship. These requests don't go through
    // `network::URLLoader`, which would otherwise add them.
    request.SetExtraRequestHeaderByName(HttpRequestHeaders::kOrigin,
                                        referring_origin_.Serialize(),
                                        /*overwrite=*/true);

    if (IsForRefreshRequest()) {
      request.SetExtraRequestHeaderByName(
          kSessionIdHeaderName, *session_identifier_, /*overwrite*/ true);
    }
  }

  void OnChallengeNeeded() {
    if (!session_identifier_.has_value()) {
      RunCallback(CreateErrorRegistrationResult(
          SessionError(SessionError::kRegistrationAttemptedChallenge)));
      // `this` may be deleted.
      return;
    }
    const Session* session = session_service_->GetSession(SessionKey{
        SchemefulSite(fetcher_endpoint_), Session::Id(*session_identifier_)});
    if (!session || !session->cached_challenge().has_value()) {
      RunCallback(CreateErrorRegistrationResult(
          SessionError(SessionError::kInvalidChallenge)));
      // `this` may be deleted.
      return;
    }

    // Reset the number of attempts because we are transitioning to a new
    // challenge (a new roundtrip), and each roundtrip should have its own
    // retry budget.
    attempts_made_ = 0;
    StartFetch(*session->cached_challenge(), std::nullopt);
    // `this` may be deleted.
  }

  void OnRequestComplete() {
    attempts_made_++;
    HttpResponseHeaders* headers = url_fetcher_->request().response_headers();
    const int response_code = headers ? headers->response_code() : 0;
    RecordNetworkResultMetrics(IsForRefreshRequest(), attempts_made_,
                               url_fetcher_->net_error(), response_code);

    // Proxy errors are treated the same way as network errors.
    if (url_fetcher_->net_error() != OK || response_code == 407) {
      if (ShouldRetryOnTransientError()) {
        // 100ms with 40% jitter. Choosing a relatively small base value
        // because the fetcher might be blocking the user.
        constexpr base::TimeDelta kMinDelay = base::Milliseconds(60);
        constexpr base::TimeDelta kMaxDelay = base::Milliseconds(140);
        base::TimeDelta delay = base::RandTimeDelta(kMinDelay, kMaxDelay);
        base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(
                &RegistrationFetcherImpl::StartFetcherEndpointRequest,
                weak_ptr_factory_.GetWeakPtr()),
            delay);
        return;
      }

      SessionError::ErrorType error_type = (url_fetcher_->net_error() != OK)
                                               ? SessionError::kNetError
                                               : SessionError::kProxyError;
      RunCallback(CreateErrorRegistrationResult(SessionError(error_type)));
      // `this` may be deleted.
      return;
    }

    if (response_code == 403) {
      OnChallengeNeeded();
      // `this` may be deleted.
      return;
    }

    if (response_code < 200) {
      RunCallback(CreateErrorRegistrationResult(
          SessionError(SessionError::kPersistentHttpError)));
      // `this` may be deleted.
      return;
    } else if (300 <= response_code && response_code < 500) {
      RunCallback(CreateErrorRegistrationResult(
          SessionError(SessionError::kPersistentHttpError)));
      // `this` may be deleted.
      return;
    } else if (response_code >= 500) {
      RunCallback(CreateErrorRegistrationResult(
          SessionError(SessionError::kTransientHttpError)));
      // `this` may be deleted.
      return;
    }

    if (url_fetcher_->data_received().empty()) {
      if (IsForRefreshRequest()) {
        RunCallback(
            RegistrationResult(RegistrationResult::NoSessionConfigChange(),
                               url_fetcher_->maybe_stored_cookies()));
        // `this` may be deleted.
        return;
      } else {
        // No config changes is not allowed at registration.
        RunCallback(CreateErrorRegistrationResult(
            SessionError(SessionError::kEmptySessionConfig)));
        // `this` may be deleted.
        return;
      }
    }

    // Use the final URL after redirects for validation checks to ensure we
    // validate the origin that actually served the response.
    GURL final_registration_url = url_fetcher_->request().url();

    base::expected<SessionParams, SessionError> params_or_error =
        ParseSessionInstructionJson(final_registration_url, session_identifier_,
                                    url_fetcher_->data_received());
    if (!params_or_error.has_value()) {
      RunCallback(
          CreateErrorRegistrationResult(std::move(params_or_error).error()));
      // `this` may be deleted.
      return;
    }

    SessionParams& params = *params_or_error;
    params.key_id = CHECK_DEREF(key_id_);
    params.attestation_key_id = attestation_key_id_;
    base::expected<std::unique_ptr<Session>, SessionError> session_or_error =
        Session::CreateIfValid(params);
    if (!session_or_error.has_value()) {
      RunCallback(
          CreateErrorRegistrationResult(std::move(session_or_error).error()));
      // `this` may be deleted.
      return;
    }
    std::unique_ptr<Session> session = std::move(*session_or_error);

    // Re-process challenge headers now that a session exists so that cached
    // challenges work for the registration case as well.
    auto challenge_params =
        device_bound_sessions::SessionChallengeParam::CreateIfValid(
            final_registration_url, headers);
    for (const SessionChallengeParam& challenge_param : challenge_params) {
      if (challenge_param.session_id() == *session->id()) {
        session->set_cached_challenge(challenge_param.challenge());
      }
    }

    // The registration endpoint is required to be same-site with the
    // session. Therefore we don't need any FirstPartySetMetadata.  The
    // normalization provided by `DbscRequest` isn't technically needed
    // here. But `CanSetBoundCookie` needs that normalization for other
    // callers and its cheap enough that it's not worth working around.
    bool can_set_bound_cookie;
    {
      // If we can't set a bound cookie, we destroy `this`, which leads
      // to a dangling pointer in the `DbscRequest`. Instead, destroy
      // `dbsc_request` before handling the returned boolean.
      DbscRequest dbsc_request(&url_fetcher_->request());
      can_set_bound_cookie =
          session->CanSetBoundCookie(dbsc_request, FirstPartySetMetadata());
    }
    if (!can_set_bound_cookie) {
      RunCallback(CreateErrorRegistrationResult(
          SessionError(SessionError::kBoundCookieSetForbidden)));
      // `this` may be deleted.
      return;
    }

    // Session::CreateIfValid confirms that the registration endpoint is
    // same-site with the scope origin and allowed to register a session for the
    // scope origin. But for cross-origin same-site registrations, we still need
    // to validate that this subdomain is allowed to register a session for the
    // scope origin via a .well-known check.
    if (features::kDeviceBoundSessionsCheckSubdomainRegistration.Get() &&
        !IsForRefreshRequest() && params.scope.include_site &&
        // We compare hosts rather than origins here because the DBSC spec
        // (https://w3c.github.io/webappsec-dbsc/#algo-create-session) defines
        // the scope of .well-known files to be the host.
        // Skip all validations if the fetcher endpoint is not a subdomain but
        // rather the top-level site (which matches the origin when including
        // the site).
        final_registration_url.host() != session->origin().host()) {
      GURL::Replacements replacements;
      replacements.SetPathStr("/.well-known/device-bound-sessions");
      replacements.SetHostStr(session->origin().host());
      GURL well_known_url =
          fetcher_endpoint_.ReplaceComponents(std::move(replacements));
      url_fetcher_ = std::make_unique<URLFetcher>(
          context_, well_known_url, referring_origin_, net_log_source_,
          /*is_refresh=*/false);
      ConfigureWellKnownRequest(url_fetcher_->request());
      url_fetcher_->Start(base::BindOnce(
          &RegistrationFetcherImpl::
              OnSubdomainRegistrationWellKnownRequestComplete,
          GetWeakPtr(), std::move(final_registration_url), std::move(session)));
      return;
    }

    RunCallback(RegistrationResult(std::move(session)));
    // `this` may be deleted.
  }

  void OnSubdomainRegistrationWellKnownRequestComplete(
      GURL final_registration_url,
      std::unique_ptr<Session> session) {
    RunCallback(OnSubdomainRegistrationWellKnownRequestCompleteInternal(
        std::move(final_registration_url), std::move(session)));
    // `this` may be deleted.
  }

  RegistrationResult OnSubdomainRegistrationWellKnownRequestCompleteInternal(
      GURL final_registration_url,
      std::unique_ptr<Session> session) {
    HttpResponseHeaders* headers = url_fetcher_->request().response_headers();
    const int response_code = headers ? headers->response_code() : 0;
    RecordHttpResponseOrErrorCode(
        "Net.DeviceBoundSessions.SubdomainWellKnown.Network.Result",
        url_fetcher_->net_error(), response_code);

    if (url_fetcher_->net_error() != OK) {
      return CreateErrorRegistrationResult(SessionError(
          SessionError::kSubdomainRegistrationWellKnownUnavailable));
    }

    if (!headers || headers->response_code() != 200) {
      return CreateErrorRegistrationResult(SessionError(
          SessionError::kSubdomainRegistrationWellKnownUnavailable));
    }

    std::optional<WellKnownParams> maybe_params =
        ParseWellKnownJson(url_fetcher_->data_received());
    if (!maybe_params.has_value()) {
      return CreateErrorRegistrationResult(
          SessionError(SessionError::kSubdomainRegistrationWellKnownMalformed));
    }

    if (!maybe_params->registering_origins.has_value() ||
        !std::ranges::contains(
            *maybe_params->registering_origins,
            url::Origin::Create(final_registration_url).Serialize())) {
      return CreateErrorRegistrationResult(
          SessionError(SessionError::kSubdomainRegistrationUnauthorized));
    }

    return RegistrationResult(std::move(session));
  }

  void RunCallback(RegistrationResult registration_result) {
    // When generating signing and attestation keys in parallel, both steps can
    // fail independently. If the first failure invokes `RunCallback()`,
    // `callback_` is consumed. Ignore subsequent calls.
    if (!callback_) {
      return;
    }
    AddNetLogResult(registration_result);
    if (IsForRefreshRequest()) {
      base::UmaHistogramCounts100(
          "Net.DeviceBoundSessions.RefreshChallengeCount",
          number_of_challenges_);
    }
    std::move(callback_).Run(this, std::move(registration_result));
    // `this` may be deleted.
  }

  void AddNetLogResult(const RegistrationResult& registration_result) {
    if (!url_fetcher_) {
      return;
    }
    NetLogEventType result_event_type =
        IsForRefreshRequest() ? NetLogEventType::DBSC_REFRESH_RESULT
                              : NetLogEventType::DBSC_REGISTRATION_RESULT;
    url_fetcher_->request().net_log().AddEvent(result_event_type, [&]() {
      std::string result = registration_result.Visit(absl::Overload{
          [&](const SessionError& error) {
            if (IsForRefreshRequest()) {
              return error.GetDeletionReason().has_value() ? "session_ended"
                                                           : "failed_continue";
            } else {
              return "registration_failed";
            }
          },
          [&](const std::unique_ptr<Session>&) {
            return IsForRefreshRequest() ? "refreshed" : "registered";
          },
          [&](RegistrationResult::NoSessionConfigChange) {
            return IsForRefreshRequest() ? "refreshed" : "registered";
          }});

      base::DictValue dict;
      dict.Set("status", std::move(result));
      return dict;
    });
  }

  // Returns true if we're fetching for a refresh request. False means this is
  // for a registration request.
  bool IsForRefreshRequest() const { return session_identifier_.has_value(); }

  // Returns true if the fetcher should retry on transient network error.
  bool ShouldRetryOnTransientError() const {
    return IsForRefreshRequest() && attempts_made_ == 1 &&
           base::FeatureList::IsEnabled(
               features::kDeviceBoundSessionsRetryTransientRefreshErrors);
  }

  //// This section of fields is state passed into the constructor. ////
  // Refers to the endpoint this class will use when triggering a registration
  // or refresh request.
  GURL fetcher_endpoint_;
  // The origin that configured `fetcher_endpoint_`: the origin of the response
  // carrying the registration header, or the scope origin of the session being
  // refreshed.
  url::Origin referring_origin_;
  // Populated iff this is a refresh request (not a registration request).
  std::optional<std::string> session_identifier_;
  const raw_ref<SessionService> session_service_;
  const raw_ref<unexportable_keys::UnexportableKeyService> key_service_;
  std::optional<unexportable_keys::UnexportableSigningKeyId> key_id_;
  std::optional<unexportable_keys::UnexportableAttestationKeyId>
      attestation_key_id_;
  raw_ptr<const URLRequestContext> context_;
  IsolationInfo isolation_info_;
  net::SiteForCookies site_for_cookies_;
  std::optional<net::NetLogSource> net_log_source_;
  std::optional<url::Origin> original_request_initiator_;
  const unexportable_keys::BackgroundTaskPriority priority_ =
      unexportable_keys::BackgroundTaskPriority::kBestEffort;
  // This is called once the registration or refresh request completes, whether
  // or not it was successful.
  RegistrationFetcher::RegistrationCompleteCallback callback_;

  std::unique_ptr<URLFetcher> url_fetcher_;

  GURL provider_url_;
  std::optional<std::string> current_challenge_;
  std::optional<std::string> current_authorization_;
  size_t number_of_challenges_ = 0;

  size_t attempts_made_ = 0;
  std::optional<RegistrationToken> last_registration_token_;

  base::WeakPtrFactory<RegistrationFetcherImpl> weak_ptr_factory_{this};
};

}  // namespace

// static
std::unique_ptr<RegistrationFetcher> RegistrationFetcher::CreateFetcher(
    RegistrationRequestParam& request_params,
    SessionService& session_service,
    unexportable_keys::UnexportableKeyService& key_service,
    const URLRequestContext* context,
    const IsolationInfo& isolation_info,
    const net::SiteForCookies& site_for_cookies,
    std::optional<NetLogSource> net_log_source,
    const std::optional<url::Origin>& original_request_initiator,
    unexportable_keys::BackgroundTaskPriority priority) {
  return std::make_unique<RegistrationFetcherImpl>(
      request_params.TakeRegistrationEndpoint(),
      request_params.TakeReferringOrigin(),
      request_params.TakeSessionIdentifier(), session_service, key_service,
      context, isolation_info, site_for_cookies, net_log_source,
      original_request_initiator, priority);
}

void RegistrationFetcher::SetFetcherForTesting(FetcherType* func) {
  CHECK(!g_mock_fetcher || !func);
  g_mock_fetcher = func;
}

// static
void RegistrationFetcher::CreateRegistrationTokenAsyncForTesting(
    unexportable_keys::UnexportableKeyService& unexportable_key_service,
    std::string challenge,
    std::optional<std::string> authorization,
    base::OnceCallback<void(
        SessionErrorOr<RegistrationFetcher::RegistrationToken>)> callback) {
  static constexpr crypto::sign::SignatureKind kSupportedAlgos[] = {
      crypto::sign::ECDSA_SHA256, crypto::sign::RSA_PKCS1_SHA256};
  unexportable_key_service.GenerateSigningKeySlowlyAsync(
      kSupportedAlgos, unexportable_keys::BackgroundTaskPriority::kBestEffort,
      base::BindOnce(
          [](unexportable_keys::UnexportableKeyService&
                 unexportable_key_service,
             std::optional<std::string> challenge,
             std::optional<std::string> authorization,
             base::OnceCallback<void(
                 SessionErrorOr<RegistrationFetcher::RegistrationToken>)>
                 callback,
             unexportable_keys::ServiceErrorOr<
                 unexportable_keys::UnexportableSigningKeyId> key_result) {
            ASSIGN_OR_RETURN(unexportable_keys::UnexportableSigningKeyId key_id,
                             std::move(key_result),
                             [&](unexportable_keys::ServiceError error) {
                               RunSessionCallback(std::move(callback),
                                                  SessionError::kSigningError,
                                                  base::unexpected(error));
                             });
            SignChallengeWithKey(
                /*is_for_refresh=*/false, unexportable_key_service, key_id,
                unexportable_keys::BackgroundTaskPriority::kBestEffort,
                std::move(challenge), std::move(authorization),
                std::move(callback));
          },
          std::ref(unexportable_key_service), std::move(challenge),
          std::move(authorization), std::move(callback)));
}

}  // namespace net::device_bound_sessions
