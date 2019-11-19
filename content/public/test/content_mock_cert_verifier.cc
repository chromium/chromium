// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/content_mock_cert_verifier.h"

#include "base/command_line.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/network_service_util.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/network_service_test_helper.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "services/network/network_context.h"
#include "services/network/public/cpp/features.h"

namespace content {

ContentMockCertVerifier::CertVerifier::CertVerifier(
    net::MockCertVerifier* verifier)
    : verifier_(verifier) {}

ContentMockCertVerifier::CertVerifier::~CertVerifier() = default;

void ContentMockCertVerifier::CertVerifier::set_default_result(
    int default_result) {
  verifier_->set_default_result(default_result);

  // Set the default result as a flag in case the FeatureList has not been
  // initialized yet and we don't know if network service will run out of
  // process.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kMockCertVerifierDefaultResultForTesting,
      base::NumberToString(default_result));

  if (IsInProcessNetworkService())
    return;

  EnsureNetworkServiceTestInitialized();
  mojo::ScopedAllowSyncCallForTesting allow_sync_call;
  network_service_test_->MockCertVerifierSetDefaultResult(default_result);
}

void ContentMockCertVerifier::CertVerifier::AddResultForCert(
    scoped_refptr<net::X509Certificate> cert,
    const net::CertVerifyResult& verify_result,
    int rv) {
  AddResultForCertAndHost(cert, "*", verify_result, rv);
}

void ContentMockCertVerifier::CertVerifier::AddResultForCertAndHost(
    scoped_refptr<net::X509Certificate> cert,
    const std::string& host_pattern,
    const net::CertVerifyResult& verify_result,
    int rv) {
  verifier_->AddResultForCertAndHost(cert, host_pattern, verify_result, rv);

  if (IsInProcessNetworkService())
    return;

  EnsureNetworkServiceTestInitialized();
  mojo::ScopedAllowSyncCallForTesting allow_sync_call;
  network_service_test_->MockCertVerifierAddResultForCertAndHost(
      cert, host_pattern, verify_result, rv);
}

void ContentMockCertVerifier::CertVerifier::
    EnsureNetworkServiceTestInitialized() {
  if (!network_service_test_ || !network_service_test_.is_connected()) {
    network_service_test_.reset();
    GetNetworkService()->BindTestInterface(
        network_service_test_.BindNewPipeAndPassReceiver());
  }
  // TODO(crbug.com/901026): Make sure the network process is started to avoid a
  // deadlock on Android.
  network_service_test_.FlushForTesting();
}

ContentMockCertVerifier::ContentMockCertVerifier()
    : mock_cert_verifier_(new net::MockCertVerifier()),
      cert_verifier_(mock_cert_verifier_.get()) {}

ContentMockCertVerifier::~ContentMockCertVerifier() {}

void ContentMockCertVerifier::SetUpCommandLine(
    base::CommandLine* command_line) {
  // Enable the MockCertVerifier in the network process via a switch. This is
  // because it's too early to call the service manager at this point (it's not
  // created yet), and by the time we can call the service manager in
  // SetUpOnMainThread the main profile has already been created.
  command_line->AppendSwitch(switches::kUseMockCertVerifierForTesting);
}

void ContentMockCertVerifier::SetUpInProcessBrowserTestFixture() {
  network::NetworkContext::SetCertVerifierForTesting(mock_cert_verifier_.get());
}

void ContentMockCertVerifier::TearDownInProcessBrowserTestFixture() {
  network::NetworkContext::SetCertVerifierForTesting(nullptr);
}

ContentMockCertVerifier::CertVerifier*
ContentMockCertVerifier::mock_cert_verifier() {
  return &cert_verifier_;
}

}  // namespace content
