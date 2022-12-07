// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/test_root_certs.h"

#include <string>
#include <utility>

#include "net/cert/pki/cert_errors.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "third_party/boringssl/src/include/openssl/pool.h"

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

bool TestRootCerts::Add(X509Certificate* certificate) {
  CertErrors errors;
  std::shared_ptr<const ParsedCertificate> parsed = ParsedCertificate::Create(
      bssl::UpRef(certificate->cert_buffer()),
      x509_util::DefaultParseCertificateOptions(), &errors);
  if (!parsed) {
    return false;
  }

  test_trust_store_.AddTrustAnchor(std::move(parsed));
  return AddImpl(certificate);
}

void TestRootCerts::Clear() {
  ClearImpl();
  test_trust_store_.Clear();
}

bool TestRootCerts::IsEmpty() const {
  return test_trust_store_.IsEmpty();
}

TestRootCerts::TestRootCerts() {
  Init();
  g_has_instance = true;
}

ScopedTestRoot::ScopedTestRoot() = default;

ScopedTestRoot::ScopedTestRoot(X509Certificate* cert) {
  Reset({cert});
}

ScopedTestRoot::ScopedTestRoot(CertificateList certs) {
  Reset(std::move(certs));
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

void ScopedTestRoot::Reset(CertificateList certs) {
  if (!certs_.empty())
    TestRootCerts::GetInstance()->Clear();
  for (const auto& cert : certs)
    TestRootCerts::GetInstance()->Add(cert.get());
  certs_ = certs;
}

}  // namespace net
