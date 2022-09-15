// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_X509_CERTIFICATE_PRIVATE_H_
#define PPAPI_TESTS_TEST_X509_CERTIFICATE_PRIVATE_H_

#include <string>

#include "ppapi/tests/test_case.h"

class TestX509CertificatePrivate : public TestCase {
 public:
  explicit TestX509CertificatePrivate(TestingInstance* instance);

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

 private:
  std::string TestValidCertificate();
  std::string TestInvalidCertificate();
};

#endif  // PPAPI_TESTS_TEST_X509_CERTIFICATE_PRIVATE_H_
