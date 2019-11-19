// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/test_root_certs.h"

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/threading/thread_restrictions.h"
#include "net/cert/x509_certificate.h"

namespace net {

namespace {

bool g_has_instance = false;

base::LazyInstance<TestRootCerts>::Leaky
    g_test_root_certs = LAZY_INSTANCE_INITIALIZER;

CertificateList LoadCertificates(const base::FilePath& filename) {
  std::string raw_cert;
  if (!base::ReadFileToString(filename, &raw_cert)) {
    LOG(ERROR) << "Can't load certificate " << filename.value();
    return CertificateList();
  }

  return X509Certificate::CreateCertificateListFromBytes(
      raw_cert.data(), raw_cert.length(), X509Certificate::FORMAT_AUTO);
}

}  // namespace

// static
TestRootCerts* TestRootCerts::GetInstance() {
  return g_test_root_certs.Pointer();
}

bool TestRootCerts::HasInstance() {
  return g_has_instance;
}

bool TestRootCerts::AddFromFile(const base::FilePath& file) {
  base::ThreadRestrictions::ScopedAllowIO allow_io_for_loading_test_certs;
  CertificateList root_certs = LoadCertificates(file);
  if (root_certs.empty() || root_certs.size() > 1)
    return false;

  return Add(root_certs.front().get());
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
