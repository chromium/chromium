// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_tcp_socket_private_trusted.h"

#include "ppapi/cpp/private/tcp_socket_private.h"
#include "ppapi/cpp/private/x509_certificate_private.h"
#include "ppapi/tests/testing_instance.h"
#include "ppapi/tests/test_utils.h"

REGISTER_TEST_CASE(TCPSocketPrivateTrusted);

TestTCPSocketPrivateTrusted::TestTCPSocketPrivateTrusted(
    TestingInstance* instance)
    : TestCase(instance) {
}

bool TestTCPSocketPrivateTrusted::Init() {
  if (!pp::TCPSocketPrivate::IsAvailable())
    return false;

  // We need something to connect to, so we connect to the HTTP server whence we
  // came. Grab the host and port.
  if (!EnsureRunningOverHTTP())
    return false;

  if (!GetLocalHostPort(instance_->pp_instance(), &host_, &port_))
    return false;

  // Get the port for the SSL server.
  ssl_port_ = instance_->ssl_server_port();

  return true;
}

void TestTCPSocketPrivateTrusted::RunTests(const std::string& filter) {
  RUN_CALLBACK_TEST(TestTCPSocketPrivateTrusted, GetServerCertificate, filter);
}

std::string TestTCPSocketPrivateTrusted::TestGetServerCertificate() {
  pp::TCPSocketPrivate socket(instance_);
  TestCompletionCallback cb(instance_->pp_instance(), callback_type());

  cb.WaitForResult(
      socket.Connect(host_.c_str(), ssl_port_, cb.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(cb);
  ASSERT_EQ(PP_OK, cb.result());

  cb.WaitForResult(
      socket.SSLHandshake(host_.c_str(), ssl_port_, cb.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(cb);
  ASSERT_EQ(PP_OK, cb.result());

  const pp::X509CertificatePrivate& cert = socket.GetServerCertificate();
  ASSERT_EQ(
      cert.GetField(PP_X509CERTIFICATE_PRIVATE_ISSUER_COMMON_NAME).AsString(),
      "Test Root CA");
  ASSERT_EQ(
      cert.GetField(PP_X509CERTIFICATE_PRIVATE_SUBJECT_COMMON_NAME).AsString(),
      "127.0.0.1");
  PASS();
}
