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
#include "build/build_config.h"
#include "crypto/crypto_buildflags.h"
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
#include "remoting/base/logging.h"
#include "remoting/protocol/authenticator.h"
#include "url/gurl.h"

#if BUILDFLAG(USE_NSS_CERTS)
#include "net/ssl/client_cert_store_nss.h"
#elif BUILDFLAG(IS_WIN)
#include "net/ssl/client_cert_store_win.h"
#elif BUILDFLAG(IS_APPLE)
#include "net/ssl/client_cert_store_mac.h"
#endif

namespace remoting {

namespace {

using RejectionReason = protocol::Authenticator::RejectionReason;

constexpr int kBufferSize = 4096;
constexpr char kCertIssuerWildCard[] = "*";
constexpr char kJsonSafetyPrefix[] = ")]}'\n";
constexpr char kForbiddenExceptionToken[] = "ForbiddenException: ";
constexpr char kLocationAuthzError[] = "Error Code 23:";

// Returns a value from the issuer field for certificate selection, in order of
// preference.  If the O or OU entries are populated with multiple values, we
// choose the first one.  This function should not be used for validation, only
// for logging or determining which certificate to select for validation.
std::string GetPreferredIssuerFieldValue(const net::X509Certificate* cert) {
  if (!cert->issuer().common_name.empty()) {
    return cert->issuer().common_name;
  }
  if (!cert->issuer().organization_names.empty() &&
      !cert->issuer().organization_names[0].empty()) {
    return cert->issuer().organization_names[0];
  }
  if (!cert->issuer().organization_unit_names.empty() &&
      !cert->issuer().organization_unit_names[0].empty()) {
    return cert->issuer().organization_unit_names[0];
  }

  return std::string();
}

// The certificate is valid if both are true:
// * The certificate issuer matches exactly |issuer| or the |issuer| is a
//   wildcard.
// * |now| is within [valid_start, valid_expiry].
bool IsCertificateValid(const std::string& issuer,
                        const base::Time& now,
                        const net::X509Certificate* cert) {
  return (issuer == kCertIssuerWildCard ||
          issuer == GetPreferredIssuerFieldValue(cert)) &&
         cert->valid_start() <= now && cert->valid_expiry() > now;
}

// Returns true if the certificate |c1| is worse than |c2|.
//
// Criteria:
// 1. An invalid certificate is always worse than a valid certificate.
// 2. Invalid certificates are equally bad, in which case false will be
//    returned.
// 3. A certificate with earlier |valid_start| time is worse.
// 4. When |valid_start| are the same, the certificate with earlier
//    |valid_expiry| is worse.
bool WorseThan(const std::string& issuer,
               const base::Time& now,
               const net::X509Certificate* c1,
               const net::X509Certificate* c2) {
  if (!IsCertificateValid(issuer, now, c2)) {
    return false;
  }

  if (!IsCertificateValid(issuer, now, c1)) {
    return true;
  }

  if (c1->valid_start() != c2->valid_start()) {
    return c1->valid_start() < c2->valid_start();
  }

  return c1->valid_expiry() < c2->valid_expiry();
}

#if BUILDFLAG(IS_WIN)
crypto::ScopedHCERTSTORE OpenLocalMachineCertStore() {
  return crypto::ScopedHCERTSTORE(::CertOpenStore(
      CERT_STORE_PROV_SYSTEM, 0, NULL,
      CERT_SYSTEM_STORE_LOCAL_MACHINE | CERT_STORE_READONLY_FLAG, L"MY"));
}
#endif

}  // namespace

TokenValidatorBase::TokenValidatorBase(
    const ThirdPartyAuthConfig& third_party_auth_config,
    const std::string& token_scope,
    scoped_refptr<net::URLRequestContextGetter> request_context_getter)
    : third_party_auth_config_(third_party_auth_config),
      token_scope_(token_scope),
      request_context_getter_(request_context_getter),
      buffer_(base::MakeRefCounted<net::IOBuffer>(kBufferSize)) {
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

  net::ClientCertStore* client_cert_store;
#if BUILDFLAG(USE_NSS_CERTS)
  client_cert_store = new net::ClientCertStoreNSS(
      net::ClientCertStoreNSS::PasswordDelegateFactory());
#elif BUILDFLAG(IS_WIN)
  // The network process is running as "Local Service" whose "Current User"
  // cert store doesn't contain any certificates. Use the "Local Machine"
  // store instead.
  // The ACL on the private key of the machine certificate in the "Local
  // Machine" cert store needs to allow access by "Local Service".
  client_cert_store = new net::ClientCertStoreWin(
      base::BindRepeating(&OpenLocalMachineCertStore));
#elif BUILDFLAG(IS_APPLE)
  client_cert_store = new net::ClientCertStoreMac();
#else
  // OpenSSL does not use the ClientCertStore infrastructure.
  client_cert_store = nullptr;
#endif
  // The callback is uncancellable, and GetClientCert requires
  // client_cert_store to stay alive until the callback is called. So we must
  // give it a WeakPtr for |this|, and ownership of the other parameters.
  client_cert_store->GetClientCerts(
      *cert_request_info,
      base::BindOnce(&TokenValidatorBase::OnCertificatesSelected,
                     weak_factory_.GetWeakPtr(),
                     base::Owned(client_cert_store)));
}

void TokenValidatorBase::OnCertificatesSelected(
    net::ClientCertStore* unused,
    net::ClientCertIdentityList selected_certs) {
  const std::string& issuer =
      third_party_auth_config_.token_validation_cert_issuer;

  base::Time now = base::Time::Now();

  auto best_match_position = std::max_element(
      selected_certs.begin(), selected_certs.end(),
      [&issuer, now](const std::unique_ptr<net::ClientCertIdentity>& i1,
                     const std::unique_ptr<net::ClientCertIdentity>& i2) {
        return WorseThan(issuer, now, i1->certificate(), i2->certificate());
      });

  if (best_match_position == selected_certs.end() ||
      !IsCertificateValid(issuer, now, (*best_match_position)->certificate())) {
    ContinueWithCertificate(nullptr, nullptr);
  } else {
    scoped_refptr<net::X509Certificate> cert =
        (*best_match_position)->certificate();
    net::ClientCertIdentity::SelfOwningAcquirePrivateKey(
        std::move(*best_match_position),
        base::BindOnce(&TokenValidatorBase::ContinueWithCertificate,
                       weak_factory_.GetWeakPtr(), std::move(cert)));
  }
}

void TokenValidatorBase::ContinueWithCertificate(
    scoped_refptr<net::X509Certificate> client_cert,
    scoped_refptr<net::SSLPrivateKey> client_private_key) {
  if (request_) {
    if (client_cert) {
      HOST_LOG << "Using client certificate issued by: '"
               << GetPreferredIssuerFieldValue(client_cert.get())
               << "' with start date: '" << client_cert->valid_start()
               << "' and expiry date: '" << client_cert->valid_expiry() << "'";
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

  absl::optional<base::Value> value = base::JSONReader::Read(responseData);
  if (!value || !value->is_dict()) {
    LOG(ERROR) << "Invalid token validation response: '" << data_ << "'";
    return RejectionReason::INVALID_CREDENTIALS;
  }

  std::string* token_scope = value->FindStringKey("scope");
  if (!token_scope || !IsValidScope(*token_scope)) {
    LOG(ERROR) << "Invalid scope: '" << *token_scope << "', expected: '"
               << token_scope_ << "'.";
    return RejectionReason::INVALID_CREDENTIALS;
  }

  // Everything is valid, so return the shared secret to the caller.
  std::string* shared_secret = value->FindStringKey("access_token");
  if (shared_secret && !shared_secret->empty()) {
    return *shared_secret;
  }

  return RejectionReason::INVALID_CREDENTIALS;
}

}  // namespace remoting
