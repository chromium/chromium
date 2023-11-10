// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/token_validator_base.h"

#include <stddef.h>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "net/base/io_buffer.h"
#include "net/base/request_priority.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_data_stream.h"
#include "net/ssl/client_cert_store.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_private_key.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "remoting/base/certificate_helpers.h"
#include "remoting/base/logging.h"
#include "remoting/protocol/authenticator.h"
#include "url/gurl.h"

namespace remoting {

namespace {

using RejectionReason = protocol::Authenticator::RejectionReason;

constexpr int kBufferSize = 4096;
constexpr char kJsonSafetyPrefix[] = ")]}'\n";
constexpr char kForbiddenExceptionToken[] = "ForbiddenException: ";
constexpr char kLocationAuthzError[] = "Error Code 23:";

}  // namespace

TokenValidatorBase::TokenValidatorBase(
    const ThirdPartyAuthConfig& third_party_auth_config,
    const std::string& token_scope,
    scoped_refptr<net::URLRequestContextGetter> request_context_getter)
    : third_party_auth_config_(third_party_auth_config),
      token_scope_(token_scope),
      request_context_getter_(request_context_getter),
      buffer_(base::MakeRefCounted<net::IOBufferWithSize>(kBufferSize)) {
  DCHECK(third_party_auth_config_.token_url.is_valid());
  DCHECK(third_party_auth_config_.token_validation_url.is_valid());
}

TokenValidatorBase::~TokenValidatorBase() = default;

// TokenValidator interface.
void TokenValidatorBase::ValidateThirdPartyToken(
    const std::string& token,
    TokenValidatedCallback on_token_validated) {
  DCHECK(!request_);
  DCHECK(!on_token_validated.is_null());

  on_token_validated_ = std::move(on_token_validated);
  token_ = token;
  StartValidateRequest(token);
}

const GURL& TokenValidatorBase::token_url() const {
  return third_party_auth_config_.token_url;
}

const std::string& TokenValidatorBase::token_scope() const {
  return token_scope_;
}

// URLRequest::Delegate interface.
void TokenValidatorBase::OnResponseStarted(net::URLRequest* source,
                                           int net_result) {
  DCHECK_NE(net_result, net::ERR_IO_PENDING);
  DCHECK_EQ(request_.get(), source);

  if (net_result != net::OK) {
    // Process all network errors in the same manner as read errors.
    OnReadCompleted(request_.get(), net_result);
    return;
  }

  int bytes_read = request_->Read(buffer_.get(), kBufferSize);
  if (bytes_read != net::ERR_IO_PENDING) {
    OnReadCompleted(request_.get(), bytes_read);
  }
}

void TokenValidatorBase::OnReadCompleted(net::URLRequest* source,
                                         int net_result) {
  DCHECK_NE(net_result, net::ERR_IO_PENDING);
  DCHECK_EQ(request_.get(), source);

  while (net_result > 0) {
    data_.append(buffer_->data(), net_result);
    net_result = request_->Read(buffer_.get(), kBufferSize);
  }

  if (net_result == net::ERR_IO_PENDING) {
    return;
  }

  retrying_request_ = false;
  auto validation_result = ProcessResponse(net_result);
  request_.reset();
  std::move(on_token_validated_).Run(validation_result);
}

void TokenValidatorBase::OnReceivedRedirect(
    net::URLRequest* request,
    const net::RedirectInfo& redirect_info,
    bool* defer_redirect) {
  if (!retrying_request_ && redirect_info.new_method == "GET" &&
      redirect_info.new_url == third_party_auth_config_.token_validation_url) {
    // A sequence of redirects caused the original POST request to become a GET
    // request for this URL. Cancel the request, and re-submit the POST request.
    // The chain of redirects are expected to set some cookies that will
    // ensure the new POST request succeeds.
    retrying_request_ = true;
    DCHECK(data_.empty());
    StartValidateRequest(token_);
  }
}

void TokenValidatorBase::OnCertificateRequested(
    net::URLRequest* source,
    net::SSLCertRequestInfo* cert_request_info) {
  DCHECK_EQ(request_.get(), source);

  std::unique_ptr<net::ClientCertStore> client_cert_store =
      CreateClientCertStoreInstance();
  net::ClientCertStore* const temp_client_cert_store = client_cert_store.get();

  // The callback is uncancellable, and GetClientCert requires client_cert_store
  // to stay alive until the callback is called. So we must give it a WeakPtr
  // for |this|, and ownership of the other parameters.
  temp_client_cert_store->GetClientCerts(
      *cert_request_info,
      base::BindOnce(&TokenValidatorBase::OnCertificatesSelected,
                     weak_factory_.GetWeakPtr(), std::move(client_cert_store)));
}

void TokenValidatorBase::OnCertificatesSelected(
    std::unique_ptr<net::ClientCertStore> unused,
    net::ClientCertIdentityList selected_certs) {
  const std::string& issuer =
      third_party_auth_config_.token_validation_cert_issuer;

  base::Time now = base::Time::Now();

  auto best_match =
      GetBestMatchFromCertificateList(issuer, now, selected_certs);
  if (!best_match) {
    ContinueWithCertificate(nullptr, nullptr);
    return;
  }

  scoped_refptr<net::X509Certificate> cert = best_match->certificate();
  if (!IsCertificateValid(issuer, now, *cert.get())) {
    LOG(ERROR) << "Best client certificate match was not valid: " << std::endl
               << "    issued by: " << GetPreferredIssuerFieldValue(*cert.get())
               << std::endl
               << "    with start date: " << cert->valid_start() << std::endl
               << "    and expiry date: " << cert->valid_expiry();
    ContinueWithCertificate(nullptr, nullptr);
    return;
  }

  net::ClientCertIdentity::SelfOwningAcquirePrivateKey(
      std::move(best_match),
      base::BindOnce(&TokenValidatorBase::ContinueWithCertificate,
                     weak_factory_.GetWeakPtr(), std::move(cert)));
}

void TokenValidatorBase::ContinueWithCertificate(
    scoped_refptr<net::X509Certificate> client_cert,
    scoped_refptr<net::SSLPrivateKey> client_private_key) {
  if (request_) {
    if (client_cert) {
      auto* cert = client_cert.get();
      HOST_LOG << "Using client certificate: " << std::endl
               << "    issued by: " << GetPreferredIssuerFieldValue(*cert)
               << std::endl
               << "    with start date: " << cert->valid_start() << std::endl
               << "    and expiry date: " << cert->valid_expiry();
    }

    request_->ContinueWithCertificate(std::move(client_cert),
                                      std::move(client_private_key));
  }
}

bool TokenValidatorBase::IsValidScope(const std::string& token_scope) {
  // TODO(rmsousa): Deal with reordering/subsets/supersets/aliases/etc.
  return token_scope == token_scope_;
}

protocol::TokenValidator::ValidationResult TokenValidatorBase::ProcessResponse(
    int net_result) {
  // Verify that we got a successful response.
  if (net_result != net::OK) {
    LOG(ERROR) << "Error validating token, err=" << net_result;
    return RejectionReason::INVALID_CREDENTIALS;
  }

  int response = request_->GetResponseCode();
  if (response != 200) {
    LOG(ERROR) << "Error " << response << " validating token: '" << data_
               << "'";
    // If we receive a 403, check to see if we can extract an error reason from
    // the response. This isn't ideal but for error cases we don't receive a
    // structured response so we need to inspect the response data. The error
    // retrieved is used to provide some guidance on how to rectify the issue to
    // the client user, it won't affect the outcome of this connection attempt.
    if (response == 403) {
      // The received response can have quite a bit of cruft before the error
      // so seek forward to the exception info and then scan it for the code.
      size_t start_pos = data_.find(kForbiddenExceptionToken);
      if (start_pos != std::string::npos) {
        if (data_.find(kLocationAuthzError, start_pos) != std::string::npos) {
          return RejectionReason::LOCATION_AUTHZ_POLICY_CHECK_FAILED;
        }

        return RejectionReason::AUTHZ_POLICY_CHECK_FAILED;
      }
    }

    return RejectionReason::INVALID_CREDENTIALS;
  }

  // Decode the JSON data from the response.
  // Server can potentially pad the JSON response with a magic prefix. We need
  // to strip that off if that exists.
  std::string responseData =
      base::StartsWith(data_, kJsonSafetyPrefix, base::CompareCase::SENSITIVE)
          ? data_.substr(sizeof(kJsonSafetyPrefix) - 1)
          : data_;

  std::optional<base::Value> value = base::JSONReader::Read(responseData);
  if (!value || !value->is_dict()) {
    LOG(ERROR) << "Invalid token validation response: '" << data_ << "'";
    return RejectionReason::INVALID_CREDENTIALS;
  }

  std::string* token_scope = value->GetDict().FindString("scope");
  if (!token_scope || !IsValidScope(*token_scope)) {
    LOG(ERROR) << "Invalid scope: '" << *token_scope << "', expected: '"
               << token_scope_ << "'.";
    return RejectionReason::INVALID_CREDENTIALS;
  }

  // Everything is valid, so return the shared secret to the caller.
  std::string* shared_secret = value->GetDict().FindString("access_token");
  if (shared_secret && !shared_secret->empty()) {
    return *shared_secret;
  }

  return RejectionReason::INVALID_CREDENTIALS;
}

}  // namespace remoting
