// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/registration_fetcher.h"

#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/unexportable_key_service.h"
#include "net/base/io_buffer.h"
#include "net/device_bound_sessions/session_binding_utils.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_request_context.h"

namespace net {

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

// A server will provide a list of acceptable algorithms in the future.
constexpr crypto::SignatureVerifier::SignatureAlgorithm
    kAcceptableAlgorithms[] = {crypto::SignatureVerifier::ECDSA_SHA256,
                               crypto::SignatureVerifier::RSA_PKCS1_SHA256};

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

  std::move(callback).Run(
      RegistrationFetcher::RegistrationTokenResult(registration_token.value()));
}

void OnKeyGenerated(
    unexportable_keys::UnexportableKeyService& unexportable_key_service,
    std::string_view challenge,
    const GURL& registration_url,
    base::OnceCallback<void(
        std::optional<RegistrationFetcher::RegistrationTokenResult>)> callback,
    unexportable_keys::ServiceErrorOr<unexportable_keys::UnexportableKeyId>
        result) {
  if (!result.has_value()) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  unexportable_keys::UnexportableKeyId key_id = result.value();

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
          expected_public_key.value(), base::Time::Now());

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

void CreateTokenAsync(
    unexportable_keys::UnexportableKeyService& unexportable_key_service,
    std::string challenge,
    const GURL& registration_url,
    base::OnceCallback<
        void(std::optional<RegistrationFetcher::RegistrationTokenResult>)>
        callback) {
  unexportable_key_service.GenerateSigningKeySlowlyAsync(
      kAcceptableAlgorithms, kTaskPriority,
      base::BindOnce(&OnKeyGenerated, std::ref(unexportable_key_service),
                     challenge, registration_url, std::move(callback)));
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
      return;
    }

    HttpResponseHeaders* headers = request->response_headers();
    int response_code = headers ? headers->response_code() : 0;
    if (response_code < 200 || response_code >= 300) {
      OnResponseCompleted();
      return;
    }

    // Initiate the first read.
    int bytes_read = request->Read(buf_.get(), kBufferSize);
    if (bytes_read >= 0) {
      OnReadCompleted(request, bytes_read);
    } else if (bytes_read != ERR_IO_PENDING) {
      OnResponseCompleted();
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
    }
  }

  RegistrationFetcherImpl(
      DeviceBoundSessionRegistrationFetcherParam registration_params,
      unexportable_keys::UnexportableKeyService& key_service,
      const URLRequestContext* context,
      const IsolationInfo& isolation_info,
      RegistrationFetcher::RegistrationCompleteCallback callback)
      : registration_params_(std::move(registration_params)),
        key_service_(key_service),
        context_(context),
        isolation_info_(isolation_info),
        callback_(std::move(callback)),
        buf_(base::MakeRefCounted<IOBufferWithSize>(kBufferSize)) {}

  ~RegistrationFetcherImpl() override { CHECK(!callback_); }

  void OnRegistrationTokenCreated(
      std::optional<RegistrationFetcher::RegistrationTokenResult> result) {
    if (!result.has_value()) {
      RunCallbackAndDeleteSelf(std::nullopt);
      return;
    }

    StartFetchingRegistration(result.value().registration_token);
  }

 private:
  void StartFetchingRegistration(const std::string& registration_token) {
    request_ =
        context_->CreateRequest(registration_params_.registration_endpoint(),
                                IDLE, this, kRegistrationTrafficAnnotation);
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
    // TODO(kristianm): Parse response in data_received_
    // For now just mark it as correct if *any* content was received.
    if (!data_received_.empty()) {
      DeviceBoundSessionParams params;
      RunCallbackAndDeleteSelf(params);
    } else {
      RunCallbackAndDeleteSelf(std::nullopt);
    }
  }

  // Running callback when fetching is complete or on error.
  // Deletes `this` afterwards.
  void RunCallbackAndDeleteSelf(
      std::optional<DeviceBoundSessionParams> params) {
    std::move(callback_).Run(std::move(params));
    delete this;
  }

  // State passed in to constructor
  DeviceBoundSessionRegistrationFetcherParam registration_params_;
  const raw_ref<unexportable_keys::UnexportableKeyService> key_service_;
  raw_ptr<const URLRequestContext> context_;
  IsolationInfo isolation_info_;
  RegistrationFetcher::RegistrationCompleteCallback callback_;

  // Created to fetch data
  std::unique_ptr<URLRequest> request_;
  scoped_refptr<IOBuffer> buf_;
  std::string data_received_;
};

}  // namespace

void RegistrationFetcher::StartCreateTokenAndFetch(
    DeviceBoundSessionRegistrationFetcherParam registration_params,
    unexportable_keys::UnexportableKeyService& key_service,
    // TODO(kristianm): Check the lifetime of context and make sure this use
    // is safe.
    const URLRequestContext* context,
    const IsolationInfo& isolation_info,
    RegistrationCompleteCallback callback) {
  GURL registration_endpoint = registration_params.registration_endpoint();
  std::string challenge = registration_params.challenge();

  RegistrationFetcherImpl* fetcher =
      new RegistrationFetcherImpl(std::move(registration_params), key_service,
                                  context, isolation_info, std::move(callback));

  // base::Unretained() is safe because the fetcher cannot be destroyed until
  // after this callback is run, as it controls its own lifetime.
  CreateTokenAsync(
      key_service, std::move(challenge), registration_endpoint,
      base::BindOnce(&RegistrationFetcherImpl::OnRegistrationTokenCreated,
                     base::Unretained(fetcher)));
}

void RegistrationFetcher::CreateTokenAsyncForTesting(
    unexportable_keys::UnexportableKeyService& unexportable_key_service,
    std::string challenge,
    const GURL& registration_url,
    base::OnceCallback<
        void(std::optional<RegistrationFetcher::RegistrationTokenResult>)>
        callback) {
  CreateTokenAsync(unexportable_key_service, challenge, registration_url,
                   std::move(callback));
}

}  // namespace net
