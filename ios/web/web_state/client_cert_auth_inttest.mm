// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Security/Security.h>

#import <optional>
#import <vector>

#import "base/apple/scoped_cftyperef.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/run_loop.h"
#import "base/strings/strcat.h"
#import "base/test/ios/wait_util.h"
#import "crypto/apple/test_helpers.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/security/certificate_policy_cache.h"
#import "ios/web/public/test/fakes/fake_web_state_delegate.h"
#import "ios/web/public/test/navigation_test_util.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "net/cert/x509_certificate.h"
#import "net/cert/x509_util_apple.h"
#import "net/ssl/ssl_server_config.h"
#import "net/test/cert_test_util.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/test_data_directory.h"
#import "testing/gtest_mac.h"

using base::test::ios::kWaitForPageLoadTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace web {

namespace {

// Loads a test client certificate from the network stack's test data.
scoped_refptr<net::X509Certificate> LoadTestCert() {
  static constexpr char kTestCertFileName[] = "client_1.pem";
  return net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                 kTestCertFileName);
}

// Loads a test private key from the network stack's test data and converts it
// to a SecKeyRef.
base::apple::ScopedCFTypeRef<SecKeyRef> LoadTestKey() {
  static constexpr char kTestKeyFileName[] = "client_1.pk8";
  base::FilePath pkcs8_path =
      net::GetTestCertsDirectory().AppendASCII(kTestKeyFileName);
  std::optional<std::vector<uint8_t>> pkcs8 = base::ReadFileToBytes(pkcs8_path);
  return crypto::apple::SecKeyFromPKCS8(*pkcs8);
}

}  // namespace

// Test fixture for WebStateDelegate::OnAuthRequired (client certificate)
// integration tests.
class ClientCertAuthTest : public WebTestWithWebState {
 protected:
  void SetUp() override {
    WebTestWithWebState::SetUp();
    web_state()->SetDelegate(&delegate_);

    // Configure server to optionally require client certificates. Using
    // OPTIONAL_CLIENT_CERT allows the handshake to complete even if the
    // embedder cancels the challenge, which is useful for verifying both paths.
    net::SSLServerConfig ssl_config;
    ssl_config.client_cert_type = net::SSLServerConfig::OPTIONAL_CLIENT_CERT;
    server_.SetSSLConfig(net::EmbeddedTestServer::CERT_OK, ssl_config);

    server_.RegisterRequestHandler(base::BindRepeating(
        &ClientCertAuthTest::HandleRequest, base::Unretained(this)));
    ASSERT_TRUE(server_.Start());

    // Trust the server certificate to avoid ServerTrust challenges.
    scoped_refptr<net::X509Certificate> cert = server_.GetCertificate();
    BrowserState::GetCertificatePolicyCache(web_state()->GetBrowserState())
        ->AllowCertForHost(cert.get(), server_.host_port_pair().host(),
                           net::CERT_STATUS_AUTHORITY_INVALID);
  }

  // Waits until WebStateDelegate::OnAuthRequired callback is called for a
  // client certificate challenge.
  [[nodiscard]] bool WaitForOnAuthRequiredCallback() {
    delegate_.ClearLastAuthenticationRequest();
    return WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^bool {
      base::RunLoop().RunUntilIdle();
      return delegate_.last_authentication_request() != nullptr;
    });
  }

  // Simulates a server endpoint that requires mutual TLS. Returns the
  // certificate's common name if provided, otherwise returns a 403 Forbidden.
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    const auto& request_url = request.GetURL();
    if (request_url.path() != "/mtls") {
      return nullptr;
    }

    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_content_type("text/html");
    if (!request.ssl_info || !request.ssl_info->cert) {
      response->set_code(net::HTTP_FORBIDDEN);
      response->set_content("<!doctype html><title>No certificate</title>");
      return response;
    }

    response->set_content(base::StrCat(
        {"<!doctype html><title>",
         request.ssl_info->cert->subject().common_name, "</title>"}));
    return response;
  }

  net::EmbeddedTestServer server_{net::EmbeddedTestServer::TYPE_HTTPS};
  FakeWebStateDelegate delegate_;
};

// Tests that providing a valid SecIdentityRef correctly authenticates the
// connection and allows the page to load with the certificate subject as the
// title.
TEST_F(ClientCertAuthTest, ProvideIdentity) {
  test::LoadUrl(web_state(), server_.GetURL("/mtls"));

  ASSERT_TRUE(WaitForOnAuthRequiredCallback());

  auto* auth_request = delegate_.last_authentication_request();
  EXPECT_NSEQ(NSURLAuthenticationMethodClientCertificate,
              auth_request->protection_space.authenticationMethod);

  // Load the test certificate and key to create a SecIdentityRef.
  scoped_refptr<net::X509Certificate> cert = LoadTestCert();
  base::apple::ScopedCFTypeRef<SecCertificateRef> certificate(
      net::x509_util::CreateSecCertificateFromX509Certificate(cert.get()));
  base::apple::ScopedCFTypeRef<SecKeyRef> key = LoadTestKey();
  base::apple::ScopedCFTypeRef<SecIdentityRef> identity(
      SecIdentityCreate(kCFAllocatorDefault, certificate.get(), key.get()));

  std::move(auth_request->client_cert_auth_callback).Run(identity.get());

  // Verify that the server received the certificate by checking the page title.
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return web_state()->GetTitle() == u"Client Cert A";
  }));
}

// Tests that providing nil to the callback correctly cancels the
// authentication challenge, resulting in no certificate being sent to the
// server.
TEST_F(ClientCertAuthTest, CancelChallenge) {
  test::LoadUrl(web_state(), server_.GetURL("/mtls"));

  ASSERT_TRUE(WaitForOnAuthRequiredCallback());

  auto* auth_request = delegate_.last_authentication_request();
  EXPECT_NSEQ(NSURLAuthenticationMethodClientCertificate,
              auth_request->protection_space.authenticationMethod);

  // Provide nil to signal that no certificate should be used.
  std::move(auth_request->client_cert_auth_callback).Run(nil);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return web_state()->GetTitle() == u"No certificate";
  }));
}

}  // namespace web
