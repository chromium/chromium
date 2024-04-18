// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/session/session_certificate.h"

#include <string_view>

#include "ios/web/public/session/proto/session.pb.h"
#include "ios/web/session/hash_util.h"
#include "net/cert/x509_util.h"

// Break if CertStatus values changed, as they are persisted on disk and thus
// must be consistent.
static_assert(net::CERT_STATUS_ALL_ERRORS == 0xFF00FFFF,
              "The value of CERT_STATUS_ALL_ERRORS changed!");
static_assert(net::CERT_STATUS_COMMON_NAME_INVALID == 1 << 0,
              "The value of CERT_STATUS_COMMON_NAME_INVALID changed!");
static_assert(net::CERT_STATUS_DATE_INVALID == 1 << 1,
              "The value of CERT_STATUS_DATE_INVALID changed!");
static_assert(net::CERT_STATUS_AUTHORITY_INVALID == 1 << 2,
              "The value of CERT_STATUS_AUTHORITY_INVALID changed!");
static_assert(net::CERT_STATUS_NO_REVOCATION_MECHANISM == 1 << 4,
              "The value of CERT_STATUS_NO_REVOCATION_MECHANISM changed!");
static_assert(net::CERT_STATUS_UNABLE_TO_CHECK_REVOCATION == 1 << 5,
              "The value of CERT_STATUS_UNABLE_TO_CHECK_REVOCATION changed!");
static_assert(net::CERT_STATUS_REVOKED == 1 << 6,
              "The value of CERT_STATUS_REVOKED changed!");
static_assert(net::CERT_STATUS_INVALID == 1 << 7,
              "The value of CERT_STATUS_INVALID changed!");
static_assert(net::CERT_STATUS_WEAK_SIGNATURE_ALGORITHM == 1 << 8,
              "The value of CERT_STATUS_WEAK_SIGNATURE_ALGORITHM changed!");
static_assert(net::CERT_STATUS_NON_UNIQUE_NAME == 1 << 10,
              "The value of CERT_STATUS_NON_UNIQUE_NAME changed!");
static_assert(net::CERT_STATUS_WEAK_KEY == 1 << 11,
              "The value of CERT_STATUS_WEAK_KEY changed!");
static_assert(net::CERT_STATUS_IS_EV == 1 << 16,
              "The value of CERT_STATUS_IS_EV changed!");
static_assert(net::CERT_STATUS_REV_CHECKING_ENABLED == 1 << 17,
              "The value of CERT_STATUS_REV_CHECKING_ENABLED changed!");

namespace web {
namespace {

// Extracts the leaf certificate in the chain from `certificate`.
scoped_refptr<net::X509Certificate> ExtractLeafCertificate(
    const scoped_refptr<net::X509Certificate>& certificate) {
  // Nothing to do if `certificate` is already a leaf certificate.
  if (certificate->intermediate_buffers().empty()) {
    return certificate;
  }

  scoped_refptr<net::X509Certificate> leaf_certificate =
      net::X509Certificate::CreateFromBuffer(
          bssl::UpRef(certificate->cert_buffer()), {});
  CHECK(leaf_certificate);
  CHECK(leaf_certificate->intermediate_buffers().empty());
  return leaf_certificate;
}

}  // namespace

// Store user decisions with the leaf cert, ignoring any intermediates.
// This is because WKWebView returns the verified certificate chain in
// `-webView:didReceiveAuthenticationChallenge:completionHandler:` but
// `-webView:didFailProvisionalNavigation:withError:` only receive the
// server-supplied chain.
SessionCertificate::SessionCertificate(
    const scoped_refptr<net::X509Certificate>& certificate,
    const std::string& host,
    net::CertStatus status)
    : certificate_(ExtractLeafCertificate(certificate)),
      host_(host),
      status_(status) {}

SessionCertificate::SessionCertificate(const proto::CertificateStorage& storage)
    : host_(storage.host()), status_(storage.status()) {
  certificate_ = net::X509Certificate::CreateFromBytes(
      base::as_byte_span(storage.certificate()));
}

SessionCertificate::SessionCertificate(SessionCertificate&&) = default;
SessionCertificate::SessionCertificate(const SessionCertificate&) = default;

SessionCertificate& SessionCertificate::operator=(SessionCertificate&&) =
    default;
SessionCertificate& SessionCertificate::operator=(const SessionCertificate&) =
    default;

SessionCertificate::~SessionCertificate() = default;

void SessionCertificate::SerializeToProto(
    proto::CertificateStorage& storage) const {
  const std::string_view cert_string =
      net::x509_util::CryptoBufferAsStringPiece(certificate_->cert_buffer());

  storage.set_certificate(cert_string.data(), cert_string.size());
  storage.set_host(host_);
  storage.set_status(status_);
}

bool operator==(const SessionCertificate& lhs, const SessionCertificate& rhs) {
  if (lhs.status() != rhs.status()) {
    return false;
  }

  if (lhs.host() != rhs.host()) {
    return false;
  }

  return net::x509_util::CryptoBufferEqual(lhs.certificate()->cert_buffer(),
                                           rhs.certificate()->cert_buffer());
}

bool operator!=(const SessionCertificate& lhs, const SessionCertificate& rhs) {
  return !(lhs == rhs);
}

size_t SessionCertificateHasher::operator()(
    const SessionCertificate& value) const {
  return session::ComputeHash(value.certificate(), value.host(),
                              value.status());
}

}  // namespace web
