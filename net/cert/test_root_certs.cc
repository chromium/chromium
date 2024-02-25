// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/test_root_certs.h"

#include <string>
#include <string_view>
#include <utility>

#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "third_party/boringssl/src/include/openssl/pool.h"
#include "third_party/boringssl/src/pki/cert_errors.h"
#include "third_party/boringssl/src/pki/trust_store.h"

namespace net {

namespace {

bool g_has_instance = false;

base::LazyInstance<TestRootCerts>::Leaky
    g_test_root_certs = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
TestRootCerts* TestRootCerts::GetInstance() {
  return g_test_root_certs.Pointer();
}

bool TestRootCerts::HasInstance() {
  return g_has_instance;
}

bool TestRootCerts::Add(X509Certificate* certificate,
                        bssl::CertificateTrust trust) {
  bssl::CertErrors errors;
  std::shared_ptr<const bssl::ParsedCertificate> parsed =
      bssl::ParsedCertificate::Create(
          bssl::UpRef(certificate->cert_buffer()),
          x509_util::DefaultParseCertificateOptions(), &errors);
  if (!parsed) {
    return false;
  }

  test_trust_store_.AddCertificate(std::move(parsed), trust);
  if (trust.HasUnspecifiedTrust() || trust.IsDistrusted()) {
    // TestRootCerts doesn't support passing the specific trust settings into
    // the OS implementations in any case, but in the case of unspecified trust
    // or explicit distrust, simply not passing the certs to the OS
    // implementation is better than nothing.
    return true;
  }
  return AddImpl(certificate);
}

void TestRootCerts::AddKnownRoot(base::span<const uint8_t> der_cert) {
  test_known_roots_.insert(std::string(
      reinterpret_cast<const char*>(der_cert.data()), der_cert.size()));
}

void TestRootCerts::Clear() {
  ClearImpl();
  test_trust_store_.Clear();
  test_known_roots_.clear();
}

bool TestRootCerts::IsEmpty() const {
  return test_trust_store_.IsEmpty();
}

bool TestRootCerts::IsKnownRoot(base::span<const uint8_t> der_cert) const {
  return test_known_roots_.find(
             std::string_view(reinterpret_cast<const char*>(der_cert.data()),
                              der_cert.size())) != test_known_roots_.end();
}

TestRootCerts::TestRootCerts() {
  Init();
  g_has_instance = true;
}

ScopedTestRoot::ScopedTestRoot() = default;

ScopedTestRoot::ScopedTestRoot(scoped_refptr<X509Certificate> cert,
                               bssl::CertificateTrust trust) {
  Reset({std::move(cert)}, trust);
}

ScopedTestRoot::ScopedTestRoot(CertificateList certs,
                               bssl::CertificateTrust trust) {
  Reset(std::move(certs), trust);
}

ScopedTestRoot::ScopedTestRoot(ScopedTestRoot&& other) {
  *this = std::move(other);
}

ScopedTestRoot& ScopedTestRoot::operator=(ScopedTestRoot&& other) {
  CertificateList tmp_certs;
  tmp_certs.swap(other.certs_);
  Reset(std::move(tmp_certs));
  return *this;
}

ScopedTestRoot::~ScopedTestRoot() {
  Reset({});
}

void ScopedTestRoot::Reset(CertificateList certs,
                           bssl::CertificateTrust trust) {
  if (!certs_.empty())
    TestRootCerts::GetInstance()->Clear();
  for (const auto& cert : certs)
    TestRootCerts::GetInstance()->Add(cert.get(), trust);
  certs_ = std::move(certs);
}

ScopedTestKnownRoot::ScopedTestKnownRoot() = default;

ScopedTestKnownRoot::ScopedTestKnownRoot(X509Certificate* cert) {
  TestRootCerts::GetInstance()->AddKnownRoot(
      x509_util::CryptoBufferAsSpan(cert->cert_buffer()));
}

ScopedTestKnownRoot::~ScopedTestKnownRoot() {
  TestRootCerts::GetInstance()->Clear();
}

}  // namespace net
