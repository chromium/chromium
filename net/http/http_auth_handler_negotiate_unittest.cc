// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_handler_negotiate.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "net/base/auth.h"
#include "net/base/features.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_auth.h"
#include "net/http/http_auth_challenge_tokenizer.h"
#include "net/http/http_auth_mechanism.h"
#include "net/http/http_auth_preferences.h"
#include "net/http/http_request_info.h"
#include "net/http/mock_allow_http_auth_preferences.h"
#include "net/log/net_log_entry.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source_type.h"
#include "net/log/net_log_with_source.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_util.h"
#include "net/net_buildflags.h"
#include "net/ssl/ssl_info.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

#if !BUILDFLAG(USE_KERBEROS)
#error "use_kerberos should be true to use Negotiate authentication scheme."
#endif

#if BUILDFLAG(IS_ANDROID)
#include "net/android/dummy_spnego_authenticator.h"
#elif BUILDFLAG(IS_WIN)
#include "net/http/mock_sspi_library_win.h"
#elif BUILDFLAG(USE_EXTERNAL_GSSAPI)
#include "net/http/http_auth_gssapi_posix.h"
#include "net/http/mock_gssapi_library_posix.h"
#else
#error "use_kerberos is true, but no Kerberos implementation available."
#endif

using net::test::IsError;
using net::test::IsOk;

namespace net {

constexpr char kFakeToken[] = "FakeToken";

namespace {

// Records the delegation type that is configured when generating a token.
class DelegationRecordingAuthSystem : public HttpAuthMechanism {
 public:
  // `observed_delegation` is retained and must be non-null and outlive `this`.
  explicit DelegationRecordingAuthSystem(
      HttpAuth::DelegationType* observed_delegation)
      : observed_delegation_(
            raw_ref<HttpAuth::DelegationType>::from_ptr(observed_delegation)) {}

  // HttpAuthMechanism:
  bool Init(const NetLogWithSource& /*net_log*/) override { return true; }
  bool NeedsIdentity() const override { return true; }
  bool AllowsExplicitCredentials() const override { return true; }
  HttpAuth::AuthorizationResult ParseChallenge(
      HttpAuthChallengeTokenizer* /*tok*/) override {
    return HttpAuth::AUTHORIZATION_RESULT_ACCEPT;
  }
  int GenerateAuthToken(const AuthCredentials* /*credentials*/,
                        const std::string& /*spn*/,
                        const std::string& /*channel_bindings*/,
                        std::string* auth_token,
                        const NetLogWithSource& /*net_log*/,
                        CompletionOnceCallback /*callback*/) override {
    *observed_delegation_ = delegation_type_;
    *auth_token = kFakeToken;
    return OK;
  }
  void SetDelegation(HttpAuth::DelegationType delegation_type) override {
    delegation_type_ = delegation_type;
  }

 private:
  HttpAuth::DelegationType delegation_type_ = HttpAuth::DelegationType::kNone;
  const raw_ref<HttpAuth::DelegationType> observed_delegation_;
};

}  // namespace

class HttpAuthHandlerNegotiateTest : public PlatformTest,
                                     public WithTaskEnvironment {
 public:
  void SetUp() override {
    AddScopedFeatureList().InitAndEnableFeature(
        features::kPartitionConnectionsByNetworkIsolationKey);
    network_anoymization_key_ = NetworkAnonymizationKey::CreateTransient();
#if BUILDFLAG(IS_WIN)
    auto auth_library = base::MakeRefCounted<MockAuthLibrary>(
        const_cast<wchar_t*>(NEGOSSP_NAME));
#elif BUILDFLAG(IS_ANDROID)
    // MockAuthLibrary is DummySpnegoAuthenticator here, which is not an
    // AuthLibrary and is not refcounted.
    auto auth_library = std::make_unique<MockAuthLibrary>();
#else
    auto auth_library = base::MakeRefCounted<MockAuthLibrary>();
#endif
    auth_library_ = auth_library.get();
    resolver_ = std::make_unique<MockCachingHostResolver>(
        /*cache_invalidation_num=*/0,
        /*default_result=*/MockHostResolverBase::RuleResolver::
            GetLocalhostResult());
    resolver_->rules()->AddIPLiteralRule("alias", "10.0.0.2",
                                         "canonical.example.com");

    http_auth_preferences_ = std::make_unique<MockAllowHttpAuthPreferences>();
    factory_ = std::make_unique<HttpAuthHandlerNegotiate::Factory>(
        HttpAuthMechanismFactory());
    factory_->set_http_auth_preferences(http_auth_preferences_.get());
#if BUILDFLAG(IS_ANDROID)
    auth_library_for_android_ = std::move(auth_library);
    http_auth_preferences_->set_auth_android_negotiate_account_type(
        "org.chromium.test.DummySpnegoAuthenticator");
    MockAuthLibrary::EnsureTestAccountExists();
#else
    factory_->set_library(std::move(auth_library));
#endif  // BUILDFLAG(IS_ANDROID)
  }

#if BUILDFLAG(IS_ANDROID)
  void TearDown() override { MockAuthLibrary::RemoveTestAccounts(); }
#endif

  std::unique_ptr<HttpAuthPreferences> CreateHttpAuthPreferences() {
    auto prefs = std::make_unique<HttpAuthPreferences>();
#if BUILDFLAG(IS_ANDROID)
    prefs->set_auth_android_negotiate_account_type(
        "org.chromium.test.DummySpnegoAuthenticator");
#endif
    return prefs;
  }

  void SetupMocks(
      MockAuthLibrary* mock_library,
      const std::string& target_name = "HTTP/canonical.example.com@REALM") {
#if BUILDFLAG(IS_WIN)
    security_package_ = std::make_unique<SecPkgInfoW>();
    UNSAFE_TODO(memset(security_package_.get(), 0x0, sizeof(SecPkgInfoW)));
    security_package_->cbMaxToken = 1337;
    mock_library->ExpectQuerySecurityPackageInfo(SEC_E_OK,
                                                 security_package_.get());
#else
    // Copied from an actual transaction!
    static const char kAuthResponse[] =
        "\x60\x82\x02\xCA\x06\x09\x2A\x86\x48\x86\xF7\x12\x01\x02\x02\x01"
        "\x00\x6E\x82\x02\xB9\x30\x82\x02\xB5\xA0\x03\x02\x01\x05\xA1\x03"
        "\x02\x01\x0E\xA2\x07\x03\x05\x00\x00\x00\x00\x00\xA3\x82\x01\xC1"
        "\x61\x82\x01\xBD\x30\x82\x01\xB9\xA0\x03\x02\x01\x05\xA1\x16\x1B"
        "\x14\x55\x4E\x49\x58\x2E\x43\x4F\x52\x50\x2E\x47\x4F\x4F\x47\x4C"
        "\x45\x2E\x43\x4F\x4D\xA2\x2C\x30\x2A\xA0\x03\x02\x01\x01\xA1\x23"
        "\x30\x21\x1B\x04\x68\x6F\x73\x74\x1B\x19\x6E\x69\x6E\x6A\x61\x2E"
        "\x63\x61\x6D\x2E\x63\x6F\x72\x70\x2E\x67\x6F\x6F\x67\x6C\x65\x2E"
        "\x63\x6F\x6D\xA3\x82\x01\x6A\x30\x82\x01\x66\xA0\x03\x02\x01\x10"
        "\xA1\x03\x02\x01\x01\xA2\x82\x01\x58\x04\x82\x01\x54\x2C\xB1\x2B"
        "\x0A\xA5\xFF\x6F\xEC\xDE\xB0\x19\x6E\x15\x20\x18\x0C\x42\xB3\x2C"
        "\x4B\xB0\x37\x02\xDE\xD3\x2F\xB4\xBF\xCA\xEC\x0E\xF9\xF3\x45\x6A"
        "\x43\xF3\x8D\x79\xBD\xCB\xCD\xB2\x2B\xB8\xFC\xD6\xB4\x7F\x09\x48"
        "\x14\xA7\x4F\xD2\xEE\xBC\x1B\x2F\x18\x3B\x81\x97\x7B\x28\xA4\xAF"
        "\xA8\xA3\x7A\x31\x1B\xFC\x97\xB6\xBA\x8A\x50\x50\xD7\x44\xB8\x30"
        "\xA4\x51\x4C\x3A\x95\x6C\xA1\xED\xE2\xEF\x17\xFE\xAB\xD2\xE4\x70"
        "\xDE\xEB\x7E\x86\x48\xC5\x3E\x19\x5B\x83\x17\xBB\x52\x26\xC0\xF3"
        "\x38\x0F\xB0\x8C\x72\xC9\xB0\x8B\x99\x96\x18\xE1\x9E\x67\x9D\xDC"
        "\xF5\x39\x80\x70\x35\x3F\x98\x72\x16\x44\xA2\xC0\x10\xAA\x70\xBD"
        "\x06\x6F\x83\xB1\xF4\x67\xA4\xBD\xDA\xF7\x79\x1D\x96\xB5\x7E\xF8"
        "\xC6\xCF\xB4\xD9\x51\xC9\xBB\xB4\x20\x3C\xDD\xB9\x2C\x38\xEA\x40"
        "\xFB\x02\x6C\xCB\x48\x71\xE8\xF4\x34\x5B\x63\x5D\x13\x57\xBD\xD1"
        "\x3D\xDE\xE8\x4A\x51\x6E\xBE\x4C\xF5\xA3\x84\xF7\x4C\x4E\x58\x04"
        "\xBE\xD1\xCC\x22\xA0\x43\xB0\x65\x99\x6A\xE0\x78\x0D\xFC\xE1\x42"
        "\xA9\x18\xCF\x55\x4D\x23\xBD\x5C\x0D\xB5\x48\x25\x47\xCC\x01\x54"
        "\x36\x4D\x0C\x6F\xAC\xCD\x33\x21\xC5\x63\x18\x91\x68\x96\xE9\xD1"
        "\xD8\x23\x1F\x21\xAE\x96\xA3\xBD\x27\xF7\x4B\xEF\x4C\x43\xFF\xF8"
        "\x22\x57\xCF\x68\x6C\x35\xD5\x21\x48\x5B\x5F\x8F\xA5\xB9\x6F\x99"
        "\xA6\xE0\x6E\xF0\xC5\x7C\x91\xC8\x0B\x8A\x4B\x4E\x80\x59\x02\xE9"
        "\xE8\x3F\x87\x04\xA6\xD1\xCA\x26\x3C\xF0\xDA\x57\xFA\xE6\xAF\x25"
        "\x43\x34\xE1\xA4\x06\x1A\x1C\xF4\xF5\x21\x9C\x00\x98\xDD\xF0\xB4"
        "\x8E\xA4\x81\xDA\x30\x81\xD7\xA0\x03\x02\x01\x10\xA2\x81\xCF\x04"
        "\x81\xCC\x20\x39\x34\x60\x19\xF9\x4C\x26\x36\x46\x99\x7A\xFD\x2B"
        "\x50\x8B\x2D\x47\x72\x38\x20\x43\x0E\x6E\x28\xB3\xA7\x4F\x26\xF1"
        "\xF1\x7B\x02\x63\x58\x5A\x7F\xC8\xD0\x6E\xF5\xD1\xDA\x28\x43\x1B"
        "\x6D\x9F\x59\x64\xDE\x90\xEA\x6C\x8C\xA9\x1B\x1E\x92\x29\x24\x23"
        "\x2C\xE3\xEA\x64\xEF\x91\xA5\x4E\x94\xE1\xDC\x56\x3A\xAF\xD5\xBC"
        "\xC9\xD3\x9B\x6B\x1F\xBE\x40\xE5\x40\xFF\x5E\x21\xEA\xCE\xFC\xD5"
        "\xB0\xE5\xBA\x10\x94\xAE\x16\x54\xFC\xEB\xAB\xF1\xD4\x20\x31\xCC"
        "\x26\xFE\xBE\xFE\x22\xB6\x9B\x1A\xE5\x55\x2C\x93\xB7\x3B\xD6\x4C"
        "\x35\x35\xC1\x59\x61\xD4\x1F\x2E\x4C\xE1\x72\x8F\x71\x4B\x0C\x39"
        "\x80\x79\xFA\xCD\xEA\x71\x1B\xAE\x35\x41\xED\xF9\x65\x0C\x59\xF8"
        "\xE1\x27\xDA\xD6\xD1\x20\x32\xCD\xBF\xD1\xEF\xE2\xED\xAD\x5D\xA7"
        "\x69\xE3\x55\xF9\x30\xD3\xD4\x08\xC8\xCA\x62\xF8\x64\xEC\x9B\x92"
        "\x1A\xF1\x03\x2E\xCC\xDC\xEB\x17\xDE\x09\xAC\xA9\x58\x86";
    test::GssContextMockImpl context1(
        "localhost",                       // Source name
        target_name.c_str(),               // Target name
        23,                                // Lifetime
        *CHROME_GSS_SPNEGO_MECH_OID_DESC,  // Mechanism
        0,                                 // Context flags
        1,                                 // Locally initiated
        0);                                // Open
    test::GssContextMockImpl context2(
        "localhost",                       // Source name
        target_name.c_str(),               // Target name
        23,                                // Lifetime
        *CHROME_GSS_SPNEGO_MECH_OID_DESC,  // Mechanism
        0,                                 // Context flags
        1,                                 // Locally initiated
        1);                                // Open
    MockAuthLibrary::SecurityContextQuery queries[] = {
        MockAuthLibrary::SecurityContextQuery(
            "Negotiate",            // Package name
            GSS_S_CONTINUE_NEEDED,  // Major response code
            0,                      // Minor response code
            context1,               // Context
            nullptr,                // Expected input token
            kAuthResponse),         // Output token
        MockAuthLibrary::SecurityContextQuery(
            "Negotiate",     // Package name
            GSS_S_COMPLETE,  // Major response code
            0,               // Minor response code
            context2,        // Context
            kAuthResponse,   // Expected input token
            kAuthResponse)   // Output token
    };

    for (const auto& query : queries) {
      mock_library->ExpectSecurityContext(
          query.expected_package, query.response_code,
          query.minor_response_code, query.context_info,
          query.expected_input_token, query.output_token);
    }
#endif  // BUILDFLAG(IS_WIN)
  }

#if BUILDFLAG(IS_POSIX)
  void SetupErrorMocks(MockAuthLibrary* mock_library,
                       int major_status,
                       int minor_status) {
    const gss_OID_desc kDefaultMech = {0, nullptr};
    test::GssContextMockImpl context(
        "localhost",                    // Source name
        "example.com",                  // Target name
        0,                              // Lifetime
        kDefaultMech,                   // Mechanism
        0,                              // Context flags
        1,                              // Locally initiated
        0);                             // Open
    MockAuthLibrary::SecurityContextQuery query(
        "Negotiate",   // Package name
        major_status,  // Major response code
        minor_status,  // Minor response code
        context,       // Context
        nullptr,       // Expected input token
        nullptr);      // Output token

    mock_library->ExpectSecurityContext(query.expected_package,
                                        query.response_code,
                                        query.minor_response_code,
                                        query.context_info,
                                        query.expected_input_token,
                                        query.output_token);
  }
#endif  // BUILDFLAG(IS_POSIX)

  int CreateHandler(bool disable_cname_lookup,
                    bool use_port,
                    bool synchronous_resolve_mode,
                    const std::string& url_string,
                    std::unique_ptr<HttpAuthHandlerNegotiate>* handler,
                    HttpAuth::Target target = HttpAuth::AUTH_SERVER) {
    http_auth_preferences_->set_negotiate_disable_cname_lookup(
        disable_cname_lookup);
    http_auth_preferences_->set_negotiate_enable_port(use_port);
    resolver_->set_synchronous_mode(synchronous_resolve_mode);
    url::SchemeHostPort scheme_host_port{GURL(url_string)};

    // Note: This is a little tricky because CreateAuthHandlerFromString
    // expects a std::unique_ptr<HttpAuthHandler>* rather than a
    // std::unique_ptr<HttpAuthHandlerNegotiate>*. This needs to do the cast
    // after creating the handler, and make sure that generic_handler
    // no longer holds on to the HttpAuthHandlerNegotiate object.
    std::unique_ptr<HttpAuthHandler> generic_handler;
    SSLInfo null_ssl_info;
    int rv = factory_->CreateAuthHandlerFromString(
        "Negotiate", target, null_ssl_info, network_anonymization_key(),
        scheme_host_port, NetLogWithSource::Make(NetLogSourceType::NONE),
        resolver_.get(), &generic_handler);
    if (rv != OK)
      return rv;
    HttpAuthHandlerNegotiate* negotiate_handler =
        static_cast<HttpAuthHandlerNegotiate*>(generic_handler.release());
    handler->reset(negotiate_handler);
    return rv;
  }

  int CreateProxyHandler(bool disable_cname_lookup,
                         bool use_port,
                         bool synchronous_resolve_mode,
                         const std::string& url_string,
                         std::unique_ptr<HttpAuthHandlerNegotiate>* handler) {
    return CreateHandler(disable_cname_lookup, use_port,
                         synchronous_resolve_mode, url_string, handler,
                         HttpAuth::AUTH_PROXY);
  }

  MockAuthLibrary* AuthLibrary() { return auth_library_; }
  MockCachingHostResolver* resolver() { return resolver_.get(); }
  HttpAuthPreferences* http_auth_preferences() {
    return http_auth_preferences_.get();
  }
  void SetHttpAuthPreferences(
      std::unique_ptr<HttpAuthPreferences> http_auth_preferences) {
    factory_->set_http_auth_preferences(nullptr);
    http_auth_preferences_ = std::move(http_auth_preferences);
    factory_->set_http_auth_preferences(http_auth_preferences_.get());
  }

  const NetworkAnonymizationKey& network_anonymization_key() const {
    return network_anoymization_key_;
  }

  // Creates a server handler for `url_string` whose auth system stores the
  // delegation type configured when generating a token in
  // `observed_delegation`, which must be non-null and outlive `this` and the
  // returned handler.
  std::unique_ptr<HttpAuthHandlerNegotiate> CreateDelegationRecordingHandler(
      const std::string& url_string,
      HttpAuth::DelegationType* observed_delegation) {
    CHECK(observed_delegation);
    delegation_recording_factory_ =
        std::make_unique<HttpAuthHandlerNegotiate::Factory>(base::BindRepeating(
            [](HttpAuth::DelegationType* out,
               const HttpAuthPreferences* /*prefs*/)
                -> std::unique_ptr<HttpAuthMechanism> {
              return std::make_unique<DelegationRecordingAuthSystem>(out);
            },
            base::Unretained(observed_delegation)));
    delegation_recording_factory_->set_http_auth_preferences(
        http_auth_preferences_.get());
#if BUILDFLAG(IS_WIN)
    delegation_recording_factory_->set_library(
        base::MakeRefCounted<MockAuthLibrary>(NEGOSSP_NAME));
#elif !BUILDFLAG(IS_ANDROID)
    delegation_recording_factory_->set_library(
        base::MakeRefCounted<MockAuthLibrary>());
#endif
    std::unique_ptr<HttpAuthHandler> generic_handler;
    if (delegation_recording_factory_->CreateAuthHandlerFromString(
            "Negotiate", HttpAuth::AUTH_SERVER, SSLInfo(),
            network_anonymization_key(), url::SchemeHostPort(GURL(url_string)),
            NetLogWithSource::Make(NetLogSourceType::NONE), resolver_.get(),
            &generic_handler) != OK) {
      return nullptr;
    }
    return base::WrapUnique(
        static_cast<HttpAuthHandlerNegotiate*>(generic_handler.release()));
  }

 private:
  NetworkAnonymizationKey network_anoymization_key_;

#if BUILDFLAG(IS_WIN)
  std::unique_ptr<SecPkgInfoW> security_package_;
#elif BUILDFLAG(IS_ANDROID)
  std::unique_ptr<MockAuthLibrary> auth_library_for_android_;
#endif
  std::unique_ptr<MockCachingHostResolver> resolver_;
  std::unique_ptr<HttpAuthPreferences> http_auth_preferences_;
  std::unique_ptr<HttpAuthHandlerNegotiate::Factory> factory_;
  std::unique_ptr<HttpAuthHandlerNegotiate::Factory>
      delegation_recording_factory_;

  // |auth_library_| is passed to |factory_|, which assumes ownership of it, but
  // can't be a scoped pointer to it since the tests need access when they set
  // up the mocks after passing ownership.
  raw_ptr<MockAuthLibrary> auth_library_;
};

#if BUILDFLAG(IS_WIN)
// On Windows the SSPI calls run on a blocking sequence, so the handler must
// report ERR_IO_PENDING to its caller rather than completing inline.
TEST_F(HttpAuthHandlerNegotiateTest, GenerateAuthTokenIsAsync) {
  SetupMocks(AuthLibrary());
  std::unique_ptr<HttpAuthHandlerNegotiate> auth_handler;
  ASSERT_EQ(
      OK, CreateHandler(true, false, true, "http://alias:500", &auth_handler));
  ASSERT_TRUE(auth_handler.get() != nullptr);

  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  int rv = auth_handler->GenerateAuthToken(nullptr, &request_info,
                                           callback.callback(), &token);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_THAT(callback.GetResult(rv), IsOk());
  EXPECT_FALSE(token.empty());
}
#endif  // BUILDFLAG(IS_WIN)

TEST_F(HttpAuthHandlerNegotiateTest, DisableCname) {
  SetupMocks(AuthLibrary(), "HTTP/alias@REALM");
  std::unique_ptr<HttpAuthHandlerNegotiate> auth_handler;
  EXPECT_EQ(OK, CreateHandler(
      true, false, true, "http://alias:500", &auth_handler));

  ASSERT_TRUE(auth_handler.get() != nullptr);
  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  EXPECT_EQ(OK, callback.GetResult(auth_handler->GenerateAuthToken(
                    nullptr, &request_info, callback.callback(), &token)));
#if BUILDFLAG(IS_WIN)
  EXPECT_EQ("HTTP/alias", auth_handler->spn_for_testing());
#else
  EXPECT_EQ("HTTP@alias", auth_handler->spn_for_testing());
#endif
}

TEST_F(HttpAuthHandlerNegotiateTest, DisableCnameStandardPort) {
  SetupMocks(AuthLibrary(), "HTTP/alias@REALM");
  std::unique_ptr<HttpAuthHandlerNegotiate> auth_handler;
  EXPECT_EQ(OK, CreateHandler(
      true, true, true, "http://alias:80", &auth_handler));
  ASSERT_TRUE(auth_handler.get() != nullptr);
  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  EXPECT_EQ(OK, callback.GetResult(auth_handler->GenerateAuthToken(
                    nullptr, &request_info, callback.callback(), &token)));
#if BUILDFLAG(IS_WIN)
  EXPECT_EQ("HTTP/alias", auth_handler->spn_for_testing());
#else
  EXPECT_EQ("HTTP@alias", auth_handler->spn_for_testing());
#endif
}

TEST_F(HttpAuthHandlerNegotiateTest, DisableCnameNonstandardPort) {
  SetupMocks(AuthLibrary(), "HTTP/alias@REALM");
  std::unique_ptr<HttpAuthHandlerNegotiate> auth_handler;
  EXPECT_EQ(OK, CreateHandler(
      true, true, true, "http://alias:500", &auth_handler));
  ASSERT_TRUE(auth_handler.get() != nullptr);
  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  EXPECT_EQ(OK, callback.GetResult(auth_handler->GenerateAuthToken(
                    nullptr, &request_info, callback.callback(), &token)));
#if BUILDFLAG(IS_WIN)
  EXPECT_EQ("HTTP/alias:500", auth_handler->spn_for_testing());
#else
  EXPECT_EQ("HTTP@alias:500", auth_handler->spn_for_testing());
#endif
}

TEST_F(HttpAuthHandlerNegotiateTest, CnameSync) {
  SetupMocks(AuthLibrary());
  std::unique_ptr<HttpAuthHandlerNegotiate> auth_handler;
  const std::string url_string = "http://alias:500";
  EXPECT_EQ(OK, CreateHandler(false, false, true, url_string, &auth_handler));
  ASSERT_TRUE(auth_handler.get() != nullptr);
  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  EXPECT_EQ(OK, callback.GetResult(auth_handler->GenerateAuthToken(
                    nullptr, &request_info, callback.callback(), &token)));
#if BUILDFLAG(IS_WIN)
  EXPECT_EQ("HTTP/canonical.example.com", auth_handler->spn_for_testing());
#else
  EXPECT_EQ("HTTP@canonical.example.com", auth_handler->spn_for_testing());
#endif

  // Make sure a cache-only lookup with the wrong NetworkAnonymizationKey (an
  // empty one) fails, to make sure the right NetworkAnonymizationKey was used.
  url::SchemeHostPort scheme_host_port{GURL(url_string)};
  HostResolver::ResolveHostParameters resolve_params;
  resolve_params.include_canonical_name = true;
  resolve_params.source = HostResolverSource::LOCAL_ONLY;
  std::unique_ptr<HostResolver::ResolveHostRequest> host_request1 =
      resolver()->CreateRequest(scheme_host_port, NetworkAnonymizationKey(),
                                handles::kInvalidNetworkHandle,
                                NetLogWithSource(), resolve_params);
  TestCompletionCallback callback2;
  int result = host_request1->Start(callback2.callback());
  EXPECT_EQ(ERR_NAME_NOT_RESOLVED, callback2.GetResult(result));

  // Make sure a cache-only lookup with the same NetworkAnonymizationKey
  // succeeds, to make sure the right NetworkAnonymizationKey was used.
  std::unique_ptr<HostResolver::ResolveHostRequest> host_request2 =
      resolver()->CreateRequest(scheme_host_port, network_anonymization_key(),
                                handles::kInvalidNetworkHandle,
                                NetLogWithSource(), resolve_params);
  TestCompletionCallback callback3;
  result = host_request2->Start(callback3.callback());
  EXPECT_EQ(OK, callback3.GetResult(result));
}

TEST_F(HttpAuthHandlerNegotiateTest, CnameAsync) {
  SetupMocks(AuthLibrary());
  std::unique_ptr<HttpAuthHandlerNegotiate> auth_handler;
  const std::string url_string = "http://alias:500";
  EXPECT_EQ(OK, CreateHandler(false, false, false, url_string, &auth_handler));
  ASSERT_TRUE(auth_handler.get() != nullptr);
  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  EXPECT_EQ(ERR_IO_PENDING,
            auth_handler->GenerateAuthToken(nullptr, &request_info,
                                            callback.callback(), &token));
  EXPECT_THAT(callback.WaitForResult(), IsOk());
#if BUILDFLAG(IS_WIN)
  EXPECT_EQ("HTTP/canonical.example.com", auth_handler->spn_for_testing());
#else
  EXPECT_EQ("HTTP@canonical.example.com", auth_handler->spn_for_testing());
#endif

  // Make sure a cache-only lookup with the wrong NetworkAnonymizationKey (an
  // empty one) fails, to make sure the right NetworkAnonymizationKey was used.
  url::SchemeHostPort scheme_host_port{GURL(url_string)};
  HostResolver::ResolveHostParameters resolve_params;
  resolve_params.include_canonical_name = true;
  resolve_params.source = HostResolverSource::LOCAL_ONLY;
  std::unique_ptr<HostResolver::ResolveHostRequest> host_request1 =
      resolver()->CreateRequest(scheme_host_port, NetworkAnonymizationKey(),
                                handles::kInvalidNetworkHandle,
                                NetLogWithSource(), resolve_params);
  TestCompletionCallback callback2;
  int result = host_request1->Start(callback2.callback());
  EXPECT_EQ(ERR_NAME_NOT_RESOLVED, callback2.GetResult(result));

  // Make sure a cache-only lookup with the same NetworkAnonymizationKey
  // succeeds, to make sure the right NetworkAnonymizationKey was used.
  std::unique_ptr<HostResolver::ResolveHostRequest> host_request2 =
      resolver()->CreateRequest(scheme_host_port, network_anonymization_key(),
                                handles::kInvalidNetworkHandle,
                                NetLogWithSource(), resolve_params);
  TestCompletionCallback callback3;
  result = host_request2->Start(callback3.callback());
  EXPECT_EQ(OK, callback3.GetResult(result));
}

TEST_F(HttpAuthHandlerNegotiateTest, CnameOutsideAllowlistNotUsedForSpn) {
  base::HistogramTester histogram_tester;
  RecordingNetLogObserver net_log_observer;

  SetupMocks(AuthLibrary(), "HTTP/app.dev.corp.com@REALM");
  std::unique_ptr<HttpAuthPreferences> prefs = CreateHttpAuthPreferences();
  prefs->SetServerAllowlist("*.dev.corp.com");
  SetHttpAuthPreferences(std::move(prefs));

  resolver()->rules()->AddIPLiteralRule("app.dev.corp.com", "10.0.0.3",
                                        "vault.prod.corp.com");

  std::unique_ptr<HttpAuthHandlerNegotiate> auth_handler;
  const std::string url_string = "http://app.dev.corp.com";
  EXPECT_EQ(OK, CreateHandler(false, false, true, url_string, &auth_handler));
  ASSERT_TRUE(auth_handler.get() != nullptr);
  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  EXPECT_EQ(OK, callback.GetResult(auth_handler->GenerateAuthToken(
                    nullptr, &request_info, callback.callback(), &token)));
  EXPECT_EQ(
      BUILDFLAG(IS_WIN) ? "HTTP/app.dev.corp.com" : "HTTP@app.dev.corp.com",
      auth_handler->spn_for_testing());
  histogram_tester.ExpectUniqueSample(
      "Net.HttpAuth.NegotiateCnameValidationResult",
      HttpAuthHandlerNegotiate::CnameValidationResult::
          kDisallowedByServerAllowlist,
      1);
  std::vector<NetLogEntry> entries =
      net_log_observer.GetEntriesWithType(NetLogEventType::AUTH_CNAME_REJECTED);
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ("app.dev.corp.com",
            GetStringValueFromParams(entries[0], "requested_host"));
  EXPECT_EQ("vault.prod.corp.com",
            GetStringValueFromParams(entries[0], "canonical_host"));
  EXPECT_EQ("disallowed_by_server_allowlist",
            GetStringValueFromParams(entries[0], "reason"));
}

TEST_F(HttpAuthHandlerNegotiateTest, CnameInsideAllowlistUsedForSpn) {
  base::HistogramTester histogram_tester;
  RecordingNetLogObserver net_log_observer;

  SetupMocks(AuthLibrary(), "HTTP/service.dev.corp.com@REALM");
  std::unique_ptr<HttpAuthPreferences> prefs = CreateHttpAuthPreferences();
  prefs->SetServerAllowlist("*.dev.corp.com");
  SetHttpAuthPreferences(std::move(prefs));

  resolver()->rules()->AddIPLiteralRule("app.dev.corp.com", "10.0.0.3",
                                        "service.dev.corp.com");

  std::unique_ptr<HttpAuthHandlerNegotiate> auth_handler;
  const std::string url_string = "http://app.dev.corp.com";
  EXPECT_EQ(OK, CreateHandler(false, false, true, url_string, &auth_handler));
  ASSERT_TRUE(auth_handler.get() != nullptr);
  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  EXPECT_EQ(OK, callback.GetResult(auth_handler->GenerateAuthToken(
                    nullptr, &request_info, callback.callback(), &token)));
  EXPECT_EQ(BUILDFLAG(IS_WIN) ? "HTTP/service.dev.corp.com"
                              : "HTTP@service.dev.corp.com",
            auth_handler->spn_for_testing());
  histogram_tester.ExpectUniqueSample(
      "Net.HttpAuth.NegotiateCnameValidationResult",
      HttpAuthHandlerNegotiate::CnameValidationResult::kAllowedDifferentHost,
      1);
  EXPECT_TRUE(
      net_log_observer.GetEntriesWithType(NetLogEventType::AUTH_CNAME_REJECTED)
          .empty());
}

TEST_F(HttpAuthHandlerNegotiateTest,
       CnameMatchesBothServerAndDelegateAllowlistsPreservesDelegation) {
  base::HistogramTester histogram_tester;
  RecordingNetLogObserver net_log_observer;

  SetupMocks(AuthLibrary(), "HTTP/service.prod.corp.com@REALM");
  std::unique_ptr<HttpAuthPreferences> prefs = CreateHttpAuthPreferences();
  prefs->SetServerAllowlist("*.corp.com");
  prefs->SetDelegateAllowlist("*.corp.com");
  SetHttpAuthPreferences(std::move(prefs));

  resolver()->rules()->AddIPLiteralRule("app.dev.corp.com", "10.0.0.3",
                                        "service.prod.corp.com");

  std::unique_ptr<HttpAuthHandlerNegotiate> auth_handler;
  const std::string url_string = "http://app.dev.corp.com";
  EXPECT_EQ(OK, CreateHandler(false, false, true, url_string, &auth_handler));
  ASSERT_TRUE(auth_handler.get() != nullptr);
  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  EXPECT_EQ(OK, callback.GetResult(auth_handler->GenerateAuthToken(
                    nullptr, &request_info, callback.callback(), &token)));
  EXPECT_EQ(BUILDFLAG(IS_WIN) ? "HTTP/service.prod.corp.com"
                              : "HTTP@service.prod.corp.com",
            auth_handler->spn_for_testing());
  EXPECT_EQ(HttpAuth::DelegationType::kUnconstrained,
            auth_handler->delegation_type_for_testing());
  histogram_tester.ExpectUniqueSample(
      "Net.HttpAuth.NegotiateCnameValidationResult",
      HttpAuthHandlerNegotiate::CnameValidationResult::kAllowedDifferentHost,
      1);
  EXPECT_TRUE(
      net_log_observer
          .GetEntriesWithType(NetLogEventType::AUTH_CNAME_DELEGATION_CLAMPED)
          .empty());
}

TEST_F(HttpAuthHandlerNegotiateTest,
       CnameInServerAllowlistOutsideDelegateAllowlistClampsDelegation) {
  base::HistogramTester histogram_tester;
  RecordingNetLogObserver net_log_observer;

  std::unique_ptr<HttpAuthPreferences> prefs = CreateHttpAuthPreferences();
  prefs->SetServerAllowlist("*.corp.com");
  prefs->SetDelegateAllowlist("*.dev.corp.com");
  SetHttpAuthPreferences(std::move(prefs));

  resolver()->rules()->AddIPLiteralRule("app.dev.corp.com", "10.0.0.3",
                                        "vault.prod.corp.com");

  HttpAuth::DelegationType observed_delegation =
      HttpAuth::DelegationType::kUnconstrained;
  std::unique_ptr<HttpAuthHandlerNegotiate> auth_handler =
      CreateDelegationRecordingHandler("http://app.dev.corp.com",
                                       &observed_delegation);
  ASSERT_TRUE(auth_handler);

  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  EXPECT_EQ(OK, callback.GetResult(auth_handler->GenerateAuthToken(
                    nullptr, &request_info, callback.callback(), &token)));
  EXPECT_EQ(BUILDFLAG(IS_WIN) ? "HTTP/vault.prod.corp.com"
                              : "HTTP@vault.prod.corp.com",
            auth_handler->spn_for_testing());
  EXPECT_EQ(HttpAuth::DelegationType::kNone,
            auth_handler->delegation_type_for_testing());
  EXPECT_EQ(HttpAuth::DelegationType::kNone, observed_delegation);
  histogram_tester.ExpectUniqueSample(
      "Net.HttpAuth.NegotiateCnameValidationResult",
      HttpAuthHandlerNegotiate::CnameValidationResult::
          kAllowedWithClampedDelegation,
      1);
  std::vector<NetLogEntry> entries = net_log_observer.GetEntriesWithType(
      NetLogEventType::AUTH_CNAME_DELEGATION_CLAMPED);
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ("app.dev.corp.com",
            GetStringValueFromParams(entries[0], "requested_host"));
  EXPECT_EQ("vault.prod.corp.com",
            GetStringValueFromParams(entries[0], "canonical_host"));
}

TEST_F(HttpAuthHandlerNegotiateTest,
       ExplicitCredentials_CnameOutsideDelegateAllowlistClampsDelegation) {
  base::HistogramTester histogram_tester;

  SetupMocks(AuthLibrary(), "HTTP/vault.prod.corp.com@REALM");
  std::unique_ptr<HttpAuthPreferences> prefs = CreateHttpAuthPreferences();
  prefs->SetServerAllowlist("*.corp.com");
  prefs->SetDelegateAllowlist("*.dev.corp.com");
  SetHttpAuthPreferences(std::move(prefs));

  resolver()->rules()->AddIPLiteralRule("app.dev.corp.com", "10.0.0.3",
                                        "vault.prod.corp.com");

  std::unique_ptr<HttpAuthHandlerNegotiate> auth_handler;
  const std::string url_string = "http://app.dev.corp.com";
  EXPECT_EQ(OK, CreateHandler(false, false, true, url_string, &auth_handler));
  ASSERT_TRUE(auth_handler.get() != nullptr);
  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  AuthCredentials credentials(u"user", u"password");
  EXPECT_EQ(OK, callback.GetResult(auth_handler->GenerateAuthToken(
                    &credentials, &request_info, callback.callback(), &token)));
  EXPECT_EQ(BUILDFLAG(IS_WIN) ? "HTTP/vault.prod.corp.com"
                              : "HTTP@vault.prod.corp.com",
            auth_handler->spn_for_testing());
  EXPECT_EQ(HttpAuth::DelegationType::kNone,
            auth_handler->delegation_type_for_testing());
  histogram_tester.ExpectUniqueSample(
      "Net.HttpAuth.NegotiateCnameValidationResult",
      HttpAuthHandlerNegotiate::CnameValidationResult::
          kAllowedWithClampedDelegation,
      1);
}

#if BUILDFLAG(IS_POSIX)
TEST_F(HttpAuthHandlerNegotiateTest,
       PosixExplicitCredentials_CnameOutsideServerAllowlistNotUsedForSpn) {
  base::HistogramTester histogram_tester;

  SetupMocks(AuthLibrary(), "HTTP/app.dev.corp.com@REALM");
  std::unique_ptr<HttpAuthPreferences> prefs = CreateHttpAuthPreferences();
  prefs->SetServerAllowlist("*.dev.corp.com");
  SetHttpAuthPreferences(std::move(prefs));

  resolver()->rules()->AddIPLiteralRule("app.dev.corp.com", "10.0.0.3",
                                        "vault.prod.corp.com");

  std::unique_ptr<HttpAuthHandlerNegotiate> auth_handler;
  const std::string url_string = "http://app.dev.corp.com";
  EXPECT_EQ(OK, CreateHandler(false, false, true, url_string, &auth_handler));
  ASSERT_TRUE(auth_handler.get() != nullptr);
  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  AuthCredentials credentials(u"user", u"password");
  EXPECT_EQ(OK, callback.GetResult(auth_handler->GenerateAuthToken(
                    &credentials, &request_info, callback.callback(), &token)));
  EXPECT_EQ("HTTP@app.dev.corp.com", auth_handler->spn_for_testing());
  histogram_tester.ExpectUniqueSample(
      "Net.HttpAuth.NegotiateCnameValidationResult",
      HttpAuthHandlerNegotiate::CnameValidationResult::
          kDisallowedByServerAllowlist,
      1);
}
#endif  // BUILDFLAG(IS_POSIX)

TEST_F(HttpAuthHandlerNegotiateTest, MixedCaseAndTrailingDotCnameNormalized) {
  base::HistogramTester histogram_tester;

  SetupMocks(AuthLibrary(), "HTTP/Service.Dev.Corp.Com.@REALM");
  std::unique_ptr<HttpAuthPreferences> prefs = CreateHttpAuthPreferences();
  prefs->SetServerAllowlist("*.dev.corp.com");
  SetHttpAuthPreferences(std::move(prefs));

  resolver()->rules()->AddIPLiteralRule("app.dev.corp.com", "10.0.0.3",
                                        "Service.Dev.Corp.Com.");

  std::unique_ptr<HttpAuthHandlerNegotiate> auth_handler;
  const std::string url_string = "http://app.dev.corp.com";
  EXPECT_EQ(OK, CreateHandler(false, false, true, url_string, &auth_handler));
  ASSERT_TRUE(auth_handler.get() != nullptr);
  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  EXPECT_EQ(OK, callback.GetResult(auth_handler->GenerateAuthToken(
                    nullptr, &request_info, callback.callback(), &token)));
  EXPECT_EQ(BUILDFLAG(IS_WIN) ? "HTTP/Service.Dev.Corp.Com."
                              : "HTTP@Service.Dev.Corp.Com.",
            auth_handler->spn_for_testing());
  histogram_tester.ExpectUniqueSample(
      "Net.HttpAuth.NegotiateCnameValidationResult",
      HttpAuthHandlerNegotiate::CnameValidationResult::kAllowedDifferentHost,
      1);
}

TEST_F(HttpAuthHandlerNegotiateTest, BareIpv6CnameMatchesSameHost) {
  base::HistogramTester histogram_tester;

  SetupMocks(AuthLibrary(), "HTTP/2001:db8::1@REALM");
  std::unique_ptr<HttpAuthPreferences> prefs = CreateHttpAuthPreferences();
  prefs->SetServerAllowlist("[2001:db8::1]");
  SetHttpAuthPreferences(std::move(prefs));

  std::unique_ptr<HttpAuthHandlerNegotiate> auth_handler;
  const std::string url_string = "http://[2001:db8::1]";
  EXPECT_EQ(OK, CreateHandler(false, false, true, url_string, &auth_handler));
  ASSERT_TRUE(auth_handler.get() != nullptr);
  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  EXPECT_EQ(OK, callback.GetResult(auth_handler->GenerateAuthToken(
                    nullptr, &request_info, callback.callback(), &token)));
  EXPECT_EQ(BUILDFLAG(IS_WIN) ? "HTTP/2001:db8::1" : "HTTP@2001:db8::1",
            auth_handler->spn_for_testing());
  histogram_tester.ExpectUniqueSample(
      "Net.HttpAuth.NegotiateCnameValidationResult",
      HttpAuthHandlerNegotiate::CnameValidationResult::kSameHost, 1);
}

TEST_F(HttpAuthHandlerNegotiateTest, BareIpv6CnameAllowedByAllowlist) {
  base::HistogramTester histogram_tester;

  SetupMocks(AuthLibrary(), "HTTP/2001:DB8::1@REALM");
  std::unique_ptr<HttpAuthPreferences> prefs = CreateHttpAuthPreferences();
  prefs->SetServerAllowlist("app.dev.corp.com,[2001:db8::1]");
  SetHttpAuthPreferences(std::move(prefs));

  resolver()->rules()->AddIPLiteralRule("app.dev.corp.com", "2001:db8::1",
                                        "2001:DB8::1");

  std::unique_ptr<HttpAuthHandlerNegotiate> auth_handler;
  const std::string url_string = "http://app.dev.corp.com";
  EXPECT_EQ(OK, CreateHandler(false, false, true, url_string, &auth_handler));
  ASSERT_TRUE(auth_handler.get() != nullptr);
  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  EXPECT_EQ(OK, callback.GetResult(auth_handler->GenerateAuthToken(
                    nullptr, &request_info, callback.callback(), &token)));
  EXPECT_EQ(BUILDFLAG(IS_WIN) ? "HTTP/2001:DB8::1" : "HTTP@2001:DB8::1",
            auth_handler->spn_for_testing());
  histogram_tester.ExpectUniqueSample(
      "Net.HttpAuth.NegotiateCnameValidationResult",
      HttpAuthHandlerNegotiate::CnameValidationResult::kAllowedDifferentHost,
      1);
}

TEST_F(HttpAuthHandlerNegotiateTest, NonRfc5952Ipv6CnameRejected) {
  base::HistogramTester histogram_tester;
  RecordingNetLogObserver net_log_observer;

  SetupMocks(AuthLibrary(), "HTTP/app.dev.corp.com@REALM");
  std::unique_ptr<HttpAuthPreferences> prefs = CreateHttpAuthPreferences();
  prefs->SetServerAllowlist("*");
  SetHttpAuthPreferences(std::move(prefs));

  resolver()->rules()->AddIPLiteralRule("app.dev.corp.com", "2001:db8::1",
                                        "2001:0db8:0:0:0:0:0:1");

  std::unique_ptr<HttpAuthHandlerNegotiate> auth_handler;
  const std::string url_string = "http://app.dev.corp.com";
  EXPECT_EQ(OK, CreateHandler(false, false, true, url_string, &auth_handler));
  ASSERT_TRUE(auth_handler);
  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  EXPECT_EQ(OK, callback.GetResult(auth_handler->GenerateAuthToken(
                    nullptr, &request_info, callback.callback(), &token)));
  EXPECT_EQ(
      BUILDFLAG(IS_WIN) ? "HTTP/app.dev.corp.com" : "HTTP@app.dev.corp.com",
      auth_handler->spn_for_testing());
  histogram_tester.ExpectUniqueSample(
      "Net.HttpAuth.NegotiateCnameValidationResult",
      HttpAuthHandlerNegotiate::CnameValidationResult::kInvalidCanonicalHost,
      1);
  std::vector<NetLogEntry> entries =
      net_log_observer.GetEntriesWithType(NetLogEventType::AUTH_CNAME_REJECTED);
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ("unparsable_host", GetStringValueFromParams(entries[0], "reason"));
}

TEST_F(HttpAuthHandlerNegotiateTest, NonRfc5952LeadingZeroIpv6CnameRejected) {
  base::HistogramTester histogram_tester;
  RecordingNetLogObserver net_log_observer;

  SetupMocks(AuthLibrary(), "HTTP/app.dev.corp.com@REALM");
  std::unique_ptr<HttpAuthPreferences> prefs = CreateHttpAuthPreferences();
  prefs->SetServerAllowlist("*");
  SetHttpAuthPreferences(std::move(prefs));

  resolver()->rules()->AddIPLiteralRule("app.dev.corp.com", "2001:db8::1",
                                        "2001:0DB8::1");

  std::unique_ptr<HttpAuthHandlerNegotiate> auth_handler;
  const std::string url_string = "http://app.dev.corp.com";
  EXPECT_EQ(OK, CreateHandler(false, false, true, url_string, &auth_handler));
  ASSERT_TRUE(auth_handler);
  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  EXPECT_EQ(OK, callback.GetResult(auth_handler->GenerateAuthToken(
                    nullptr, &request_info, callback.callback(), &token)));
  EXPECT_EQ(
      BUILDFLAG(IS_WIN) ? "HTTP/app.dev.corp.com" : "HTTP@app.dev.corp.com",
      auth_handler->spn_for_testing());
  histogram_tester.ExpectUniqueSample(
      "Net.HttpAuth.NegotiateCnameValidationResult",
      HttpAuthHandlerNegotiate::CnameValidationResult::kInvalidCanonicalHost,
      1);
  std::vector<NetLogEntry> entries =
      net_log_observer.GetEntriesWithType(NetLogEventType::AUTH_CNAME_REJECTED);
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ("unparsable_host", GetStringValueFromParams(entries[0], "reason"));
}

TEST_F(HttpAuthHandlerNegotiateTest, BracketedIpv6CnameRejected) {
  base::HistogramTester histogram_tester;
  RecordingNetLogObserver net_log_observer;

  SetupMocks(AuthLibrary(), "HTTP/app.dev.corp.com@REALM");
  std::unique_ptr<HttpAuthPreferences> prefs = CreateHttpAuthPreferences();
  prefs->SetServerAllowlist("*");
  SetHttpAuthPreferences(std::move(prefs));

  resolver()->rules()->AddIPLiteralRule("app.dev.corp.com", "2001:db8::1",
                                        "[2001:db8::1]");

  std::unique_ptr<HttpAuthHandlerNegotiate> auth_handler;
  const std::string url_string = "http://app.dev.corp.com";
  EXPECT_EQ(OK, CreateHandler(false, false, true, url_string, &auth_handler));
  ASSERT_TRUE(auth_handler);
  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  EXPECT_EQ(OK, callback.GetResult(auth_handler->GenerateAuthToken(
                    nullptr, &request_info, callback.callback(), &token)));
  EXPECT_EQ(
      BUILDFLAG(IS_WIN) ? "HTTP/app.dev.corp.com" : "HTTP@app.dev.corp.com",
      auth_handler->spn_for_testing());
  histogram_tester.ExpectUniqueSample(
      "Net.HttpAuth.NegotiateCnameValidationResult",
      HttpAuthHandlerNegotiate::CnameValidationResult::kInvalidCanonicalHost,
      1);
  std::vector<NetLogEntry> entries =
      net_log_observer.GetEntriesWithType(NetLogEventType::AUTH_CNAME_REJECTED);
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ("unparsable_host", GetStringValueFromParams(entries[0], "reason"));
}

TEST_F(HttpAuthHandlerNegotiateTest, PercentEncodedCnameRejected) {
  base::HistogramTester histogram_tester;
  RecordingNetLogObserver net_log_observer;

  SetupMocks(AuthLibrary(), "HTTP/app.dev.corp.com@REALM");
  std::unique_ptr<HttpAuthPreferences> prefs = CreateHttpAuthPreferences();
  prefs->SetServerAllowlist("*");
  SetHttpAuthPreferences(std::move(prefs));

  resolver()->rules()->AddIPLiteralRule("app.dev.corp.com", "10.0.0.3",
                                        "app%2edev.corp.com");

  std::unique_ptr<HttpAuthHandlerNegotiate> auth_handler;
  const std::string url_string = "http://app.dev.corp.com";
  EXPECT_EQ(OK, CreateHandler(false, false, true, url_string, &auth_handler));
  ASSERT_TRUE(auth_handler.get() != nullptr);
  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  EXPECT_EQ(OK, callback.GetResult(auth_handler->GenerateAuthToken(
                    nullptr, &request_info, callback.callback(), &token)));
  EXPECT_EQ(
      BUILDFLAG(IS_WIN) ? "HTTP/app.dev.corp.com" : "HTTP@app.dev.corp.com",
      auth_handler->spn_for_testing());
  histogram_tester.ExpectUniqueSample(
      "Net.HttpAuth.NegotiateCnameValidationResult",
      HttpAuthHandlerNegotiate::CnameValidationResult::kInvalidCanonicalHost,
      1);
  std::vector<NetLogEntry> entries =
      net_log_observer.GetEntriesWithType(NetLogEventType::AUTH_CNAME_REJECTED);
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ("unparsable_host", GetStringValueFromParams(entries[0], "reason"));
}

TEST_F(HttpAuthHandlerNegotiateTest, CnameAllowlistValidationKillswitch) {
  AddScopedFeatureList().InitAndDisableFeature(
      features::kValidateNegotiateCnameAllowlist);
  base::HistogramTester histogram_tester;
  RecordingNetLogObserver net_log_observer;

  std::unique_ptr<HttpAuthPreferences> prefs = CreateHttpAuthPreferences();
  prefs->SetServerAllowlist("*.dev.corp.com");
  prefs->SetDelegateAllowlist("*.dev.corp.com");
  SetHttpAuthPreferences(std::move(prefs));

  resolver()->rules()->AddIPLiteralRule("app.dev.corp.com", "10.0.0.3",
                                        "vault.prod.corp.com");

  HttpAuth::DelegationType observed_delegation =
      HttpAuth::DelegationType::kNone;
  std::unique_ptr<HttpAuthHandlerNegotiate> auth_handler =
      CreateDelegationRecordingHandler("http://app.dev.corp.com",
                                       &observed_delegation);
  ASSERT_TRUE(auth_handler);

  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  EXPECT_EQ(OK, callback.GetResult(auth_handler->GenerateAuthToken(
                    nullptr, &request_info, callback.callback(), &token)));
  EXPECT_EQ(BUILDFLAG(IS_WIN) ? "HTTP/vault.prod.corp.com"
                              : "HTTP@vault.prod.corp.com",
            auth_handler->spn_for_testing());
  EXPECT_EQ(HttpAuth::DelegationType::kUnconstrained,
            auth_handler->delegation_type_for_testing());
  EXPECT_EQ(HttpAuth::DelegationType::kUnconstrained, observed_delegation);
  histogram_tester.ExpectTotalCount(
      "Net.HttpAuth.NegotiateCnameValidationResult", 0);
  EXPECT_TRUE(
      net_log_observer.GetEntriesWithType(NetLogEventType::AUTH_CNAME_REJECTED)
          .empty());
  EXPECT_TRUE(
      net_log_observer
          .GetEntriesWithType(NetLogEventType::AUTH_CNAME_DELEGATION_CLAMPED)
          .empty());
}

TEST_F(HttpAuthHandlerNegotiateTest,
       CnameOutsideServerAllowlistClampsDelegation) {
  base::HistogramTester histogram_tester;
  RecordingNetLogObserver net_log_observer;

  std::unique_ptr<HttpAuthPreferences> prefs = CreateHttpAuthPreferences();
  prefs->SetServerAllowlist("*.dev.corp.com");
  prefs->SetDelegateAllowlist("*.dev.corp.com");
  SetHttpAuthPreferences(std::move(prefs));

  resolver()->rules()->AddIPLiteralRule("app.dev.corp.com", "10.0.0.3",
                                        "vault.prod.corp.com");

  HttpAuth::DelegationType observed_delegation =
      HttpAuth::DelegationType::kUnconstrained;
  std::unique_ptr<HttpAuthHandlerNegotiate> auth_handler =
      CreateDelegationRecordingHandler("http://app.dev.corp.com",
                                       &observed_delegation);
  ASSERT_TRUE(auth_handler);

  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  EXPECT_EQ(OK, callback.GetResult(auth_handler->GenerateAuthToken(
                    nullptr, &request_info, callback.callback(), &token)));
  EXPECT_EQ(
      BUILDFLAG(IS_WIN) ? "HTTP/app.dev.corp.com" : "HTTP@app.dev.corp.com",
      auth_handler->spn_for_testing());
  EXPECT_EQ(HttpAuth::DelegationType::kNone,
            auth_handler->delegation_type_for_testing());
  EXPECT_EQ(HttpAuth::DelegationType::kNone, observed_delegation);
  histogram_tester.ExpectUniqueSample(
      "Net.HttpAuth.NegotiateCnameValidationResult",
      HttpAuthHandlerNegotiate::CnameValidationResult::
          kDisallowedByServerAllowlist,
      1);
  EXPECT_EQ(1u, net_log_observer
                    .GetEntriesWithType(NetLogEventType::AUTH_CNAME_REJECTED)
                    .size());
  std::vector<NetLogEntry> entries = net_log_observer.GetEntriesWithType(
      NetLogEventType::AUTH_CNAME_DELEGATION_CLAMPED);
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ("app.dev.corp.com",
            GetStringValueFromParams(entries[0], "requested_host"));
  EXPECT_EQ("vault.prod.corp.com",
            GetStringValueFromParams(entries[0], "canonical_host"));
}

TEST_F(HttpAuthHandlerNegotiateTest, NonCanonicalIpv4CnameRejected) {
  base::HistogramTester histogram_tester;

  SetupMocks(AuthLibrary(), "HTTP/app.dev.corp.com@REALM");
  std::unique_ptr<HttpAuthPreferences> prefs = CreateHttpAuthPreferences();
  prefs->SetServerAllowlist("app.dev.corp.com,10.0.0.2");
  SetHttpAuthPreferences(std::move(prefs));

  // `"167772162"` canonicalizes to `"10.0.0.2"`, but would name a different
  // principal in the SPN.
  resolver()->rules()->AddIPLiteralRule("app.dev.corp.com", "10.0.0.2",
                                        "167772162");

  std::unique_ptr<HttpAuthHandlerNegotiate> auth_handler;
  EXPECT_EQ(OK, CreateHandler(false, false, true, "http://app.dev.corp.com",
                              &auth_handler));
  ASSERT_TRUE(auth_handler);
  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  EXPECT_EQ(OK, callback.GetResult(auth_handler->GenerateAuthToken(
                    nullptr, &request_info, callback.callback(), &token)));
  EXPECT_EQ(
      BUILDFLAG(IS_WIN) ? "HTTP/app.dev.corp.com" : "HTTP@app.dev.corp.com",
      auth_handler->spn_for_testing());
  histogram_tester.ExpectUniqueSample(
      "Net.HttpAuth.NegotiateCnameValidationResult",
      HttpAuthHandlerNegotiate::CnameValidationResult::kInvalidCanonicalHost,
      1);
}

TEST_F(HttpAuthHandlerNegotiateTest, FullwidthCnameRejected) {
  base::HistogramTester histogram_tester;
  RecordingNetLogObserver net_log_observer;

  SetupMocks(AuthLibrary(), "HTTP/app.dev.corp.com@REALM");
  std::unique_ptr<HttpAuthPreferences> prefs = CreateHttpAuthPreferences();
  prefs->SetServerAllowlist("*.dev.corp.com");
  SetHttpAuthPreferences(std::move(prefs));

  // Fullwidth `'s'` (U+FF53) and ideographic full stop (U+3002) IDNA-map to
  // `"service.dev.corp.com"`, which matches `"*.dev.corp.com"`, but the raw
  // string would name a different principal in the SPN.
  constexpr char kFullwidthCname[] =
      "\xef\xbd\x93"
      "ervice\xe3\x80\x82"
      "dev.corp.com";
  resolver()->rules()->AddIPLiteralRule("app.dev.corp.com", "10.0.0.3",
                                        kFullwidthCname);

  std::unique_ptr<HttpAuthHandlerNegotiate> auth_handler;
  EXPECT_EQ(OK, CreateHandler(false, false, true, "http://app.dev.corp.com",
                              &auth_handler));
  ASSERT_TRUE(auth_handler);
  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  EXPECT_EQ(OK, callback.GetResult(auth_handler->GenerateAuthToken(
                    nullptr, &request_info, callback.callback(), &token)));
  EXPECT_EQ(
      BUILDFLAG(IS_WIN) ? "HTTP/app.dev.corp.com" : "HTTP@app.dev.corp.com",
      auth_handler->spn_for_testing());
  histogram_tester.ExpectUniqueSample(
      "Net.HttpAuth.NegotiateCnameValidationResult",
      HttpAuthHandlerNegotiate::CnameValidationResult::kInvalidCanonicalHost,
      1);
  std::vector<NetLogEntry> entries =
      net_log_observer.GetEntriesWithType(NetLogEventType::AUTH_CNAME_REJECTED);
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ("unparsable_host", GetStringValueFromParams(entries[0], "reason"));
  EXPECT_EQ("%ESCAPED:\xe2\x80\x8b %EF%BD%93ervice%E3%80%82dev.corp.com",
            GetStringValueFromParams(entries[0], "canonical_host"));
}

TEST_F(HttpAuthHandlerNegotiateTest, ProxyAuthUsesCnameWithoutValidation) {
  AddScopedFeatureList().InitAndEnableFeature(
      features::kValidateNegotiateCnameAllowlist);
  base::HistogramTester histogram_tester;

  SetupMocks(AuthLibrary(), "HTTP/vault.prod.corp.com@REALM");
  std::unique_ptr<HttpAuthPreferences> prefs = CreateHttpAuthPreferences();
  prefs->SetServerAllowlist("*.dev.corp.com");
  SetHttpAuthPreferences(std::move(prefs));

  resolver()->rules()->AddIPLiteralRule("app.dev.corp.com", "10.0.0.3",
                                        "vault.prod.corp.com");

  std::unique_ptr<HttpAuthHandlerNegotiate> auth_handler;
  EXPECT_EQ(OK, CreateProxyHandler(/*disable_cname_lookup=*/false,
                                   /*use_port=*/false,
                                   /*synchronous_resolve_mode=*/true,
                                   "http://app.dev.corp.com", &auth_handler));
  ASSERT_TRUE(auth_handler);
  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  EXPECT_EQ(OK, callback.GetResult(auth_handler->GenerateAuthToken(
                    nullptr, &request_info, callback.callback(), &token)));
  EXPECT_EQ(BUILDFLAG(IS_WIN) ? "HTTP/vault.prod.corp.com"
                              : "HTTP@vault.prod.corp.com",
            auth_handler->spn_for_testing());
  histogram_tester.ExpectTotalCount(
      "Net.HttpAuth.NegotiateCnameValidationResult", 0);
}

#if BUILDFLAG(IS_WIN)
TEST_F(HttpAuthHandlerNegotiateTest,
       WinExplicitCredentials_CnameOutsideServerAllowlistUsedForSpn) {
  base::HistogramTester histogram_tester;

  SetupMocks(AuthLibrary());
  std::unique_ptr<HttpAuthPreferences> prefs = CreateHttpAuthPreferences();
  prefs->SetServerAllowlist("*.dev.corp.com");
  SetHttpAuthPreferences(std::move(prefs));

  resolver()->rules()->AddIPLiteralRule("app.dev.corp.com", "10.0.0.3",
                                        "vault.prod.corp.com");

  std::unique_ptr<HttpAuthHandlerNegotiate> auth_handler;
  EXPECT_EQ(OK, CreateHandler(false, false, true, "http://app.dev.corp.com",
                              &auth_handler));
  ASSERT_TRUE(auth_handler);
  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  AuthCredentials credentials(u"user", u"password");
  EXPECT_EQ(OK, callback.GetResult(auth_handler->GenerateAuthToken(
                    &credentials, &request_info, callback.callback(), &token)));
  // The server allowlist only governs ambient credentials.
  EXPECT_EQ("HTTP/vault.prod.corp.com", auth_handler->spn_for_testing());
  histogram_tester.ExpectUniqueSample(
      "Net.HttpAuth.NegotiateCnameValidationResult",
      HttpAuthHandlerNegotiate::CnameValidationResult::kAllowedDifferentHost,
      1);
}

namespace {

// Emulates `URLSecurityManagerWin` zone mapping: allows ambient credentials and
// unconstrained delegation only for `allowed_hosts`.
class FakeZoneHttpAuthPreferences : public HttpAuthPreferences {
 public:
  FakeZoneHttpAuthPreferences(std::set<std::string> allowed_hosts,
                              bool has_default_allowlist)
      : allowed_hosts_(std::move(allowed_hosts)),
        has_default_allowlist_(has_default_allowlist) {}

  // HttpAuthPreferences:
  bool CanUseDefaultCredentials(
      const url::SchemeHostPort& auth_scheme_host_port) const override {
    return allowed_hosts_.contains(auth_scheme_host_port.host());
  }
  bool HasDefaultAllowlist() const override { return has_default_allowlist_; }
  HttpAuth::DelegationType GetDelegationType(
      const url::SchemeHostPort& auth_scheme_host_port) const override {
    return allowed_hosts_.contains(auth_scheme_host_port.host())
               ? HttpAuth::DelegationType::kUnconstrained
               : HttpAuth::DelegationType::kNone;
  }

 private:
  const std::set<std::string> allowed_hosts_;
  const bool has_default_allowlist_;
};

}  // namespace

TEST_F(HttpAuthHandlerNegotiateTest,
       WinDotlessIntranetCnameUsedWithoutAllowlist) {
  base::HistogramTester histogram_tester;
  RecordingNetLogObserver net_log_observer;

  SetupMocks(AuthLibrary());
  SetHttpAuthPreferences(std::make_unique<FakeZoneHttpAuthPreferences>(
      std::set<std::string>{"intranet"}, /*has_default_allowlist=*/false));

  resolver()->rules()->AddIPLiteralRule("intranet", "10.0.0.3",
                                        "intranet.corp.com");

  std::unique_ptr<HttpAuthHandlerNegotiate> auth_handler;
  EXPECT_EQ(
      OK, CreateHandler(false, false, true, "http://intranet", &auth_handler));
  ASSERT_TRUE(auth_handler);
  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  EXPECT_EQ(OK, callback.GetResult(auth_handler->GenerateAuthToken(
                    nullptr, &request_info, callback.callback(), &token)));
  EXPECT_EQ("HTTP/intranet.corp.com", auth_handler->spn_for_testing());
  EXPECT_EQ(HttpAuth::DelegationType::kNone,
            auth_handler->delegation_type_for_testing());
  histogram_tester.ExpectUniqueSample(
      "Net.HttpAuth.NegotiateCnameValidationResult",
      HttpAuthHandlerNegotiate::CnameValidationResult::
          kAllowedWindowsDotlessIntranet,
      1);
  EXPECT_EQ(1u, net_log_observer
                    .GetEntriesWithType(
                        NetLogEventType::AUTH_CNAME_DELEGATION_CLAMPED)
                    .size());
}

TEST_F(HttpAuthHandlerNegotiateTest,
       WinDotlessIntranetCnameRejectedWithAllowlist) {
  base::HistogramTester histogram_tester;

  SetupMocks(AuthLibrary());
  SetHttpAuthPreferences(std::make_unique<FakeZoneHttpAuthPreferences>(
      std::set<std::string>{"intranet"}, /*has_default_allowlist=*/true));

  resolver()->rules()->AddIPLiteralRule("intranet", "10.0.0.3",
                                        "intranet.corp.com");

  std::unique_ptr<HttpAuthHandlerNegotiate> auth_handler;
  EXPECT_EQ(
      OK, CreateHandler(false, false, true, "http://intranet", &auth_handler));
  ASSERT_TRUE(auth_handler);
  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  EXPECT_EQ(OK, callback.GetResult(auth_handler->GenerateAuthToken(
                    nullptr, &request_info, callback.callback(), &token)));
  EXPECT_EQ("HTTP/intranet", auth_handler->spn_for_testing());
  histogram_tester.ExpectUniqueSample(
      "Net.HttpAuth.NegotiateCnameValidationResult",
      HttpAuthHandlerNegotiate::CnameValidationResult::
          kDisallowedByServerAllowlist,
      1);
}

TEST_F(HttpAuthHandlerNegotiateTest,
       WinDottedHostCnameRejectedWithoutAllowlist) {
  base::HistogramTester histogram_tester;

  SetupMocks(AuthLibrary());
  SetHttpAuthPreferences(std::make_unique<FakeZoneHttpAuthPreferences>(
      std::set<std::string>{"app.corp.com"}, /*has_default_allowlist=*/false));

  resolver()->rules()->AddIPLiteralRule("app.corp.com", "10.0.0.3",
                                        "vault.corp.com");

  std::unique_ptr<HttpAuthHandlerNegotiate> auth_handler;
  EXPECT_EQ(OK, CreateHandler(false, false, true, "http://app.corp.com",
                              &auth_handler));
  ASSERT_TRUE(auth_handler);
  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  EXPECT_EQ(OK, callback.GetResult(auth_handler->GenerateAuthToken(
                    nullptr, &request_info, callback.callback(), &token)));
  EXPECT_EQ("HTTP/app.corp.com", auth_handler->spn_for_testing());
  histogram_tester.ExpectUniqueSample(
      "Net.HttpAuth.NegotiateCnameValidationResult",
      HttpAuthHandlerNegotiate::CnameValidationResult::
          kDisallowedByServerAllowlist,
      1);
}
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(USE_EXTERNAL_GSSAPI)
TEST_F(HttpAuthHandlerNegotiateTest,
       CnameClampedDelegationPassesTargetPrincipalValidation) {
  AddScopedFeatureList().InitWithFeatures(
      {features::kValidateNegotiateTargetPrincipalHost,
       features::kValidateNegotiateCnameAllowlist},
      {});
  base::HistogramTester histogram_tester;

  SetupMocks(AuthLibrary(), "HTTP/vault.prod.corp.com@REALM");
  std::unique_ptr<HttpAuthPreferences> prefs = CreateHttpAuthPreferences();
  prefs->SetServerAllowlist("*.corp.com");
  prefs->SetDelegateAllowlist("*.dev.corp.com");
  SetHttpAuthPreferences(std::move(prefs));

  resolver()->rules()->AddIPLiteralRule("app.dev.corp.com", "10.0.0.3",
                                        "vault.prod.corp.com");

  std::unique_ptr<HttpAuthHandlerNegotiate> auth_handler;
  ASSERT_EQ(OK, CreateHandler(false, false, true, "http://app.dev.corp.com",
                              &auth_handler));
  ASSERT_TRUE(auth_handler);

  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  int rv = auth_handler->GenerateAuthToken(nullptr, &request_info,
                                           callback.callback(), &token);
  EXPECT_THAT(callback.GetResult(rv), IsOk());
  EXPECT_FALSE(token.empty());
  EXPECT_EQ(HttpAuth::DelegationType::kNone,
            auth_handler->delegation_type_for_testing());
  histogram_tester.ExpectUniqueSample(
      "Net.HttpAuth.NegotiateCnameValidationResult",
      HttpAuthHandlerNegotiate::CnameValidationResult::
          kAllowedWithClampedDelegation,
      1);
  histogram_tester.ExpectUniqueSample(
      "Net.HttpAuth.NegotiateTargetPrincipalValidationResult",
      HttpAuthHandlerNegotiate::TargetHostValidationResult::
          kAllowedDifferentHost,
      1);
}
#endif  // BUILDFLAG(USE_EXTERNAL_GSSAPI)

#if BUILDFLAG(IS_POSIX)

// This test is only for GSSAPI, as we can't use explicit credentials with
// that library.
TEST_F(HttpAuthHandlerNegotiateTest, ServerNotInKerberosDatabase) {
  SetupErrorMocks(AuthLibrary(), GSS_S_FAILURE, 0x96C73A07);  // No server
  std::unique_ptr<HttpAuthHandlerNegotiate> auth_handler;
  EXPECT_EQ(OK, CreateHandler(false, false, false, "http://alias:500",
                              &auth_handler));
  ASSERT_TRUE(auth_handler.get() != nullptr);
  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  EXPECT_EQ(ERR_IO_PENDING,
            auth_handler->GenerateAuthToken(nullptr, &request_info,
                                            callback.callback(), &token));
  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_MISSING_AUTH_CREDENTIALS));
}

// This test is only for GSSAPI, as we can't use explicit credentials with
// that library.
TEST_F(HttpAuthHandlerNegotiateTest, NoKerberosCredentials) {
  SetupErrorMocks(AuthLibrary(), GSS_S_FAILURE, 0x96C73AC3);  // No credentials
  std::unique_ptr<HttpAuthHandlerNegotiate> auth_handler;
  EXPECT_EQ(OK, CreateHandler(false, false, false, "http://alias:500",
                              &auth_handler));
  ASSERT_TRUE(auth_handler.get() != nullptr);
  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  EXPECT_EQ(ERR_IO_PENDING,
            auth_handler->GenerateAuthToken(nullptr, &request_info,
                                            callback.callback(), &token));
  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_MISSING_AUTH_CREDENTIALS));
}

#if BUILDFLAG(USE_EXTERNAL_GSSAPI)
TEST_F(HttpAuthHandlerNegotiateTest, MissingGSSAPI) {
  MockAllowHttpAuthPreferences http_auth_preferences;
  auto negotiate_factory = std::make_unique<HttpAuthHandlerNegotiate::Factory>(
      HttpAuthMechanismFactory());
  negotiate_factory->set_http_auth_preferences(&http_auth_preferences);
  negotiate_factory->set_library(base::MakeRefCounted<GSSAPISharedLibrary>(
      "/this/library/does/not/exist"));

  url::SchemeHostPort scheme_host_port(GURL("http://www.example.com"));
  std::unique_ptr<HttpAuthHandler> generic_handler;
  int rv = negotiate_factory->CreateAuthHandlerFromString(
      "Negotiate", HttpAuth::AUTH_SERVER, SSLInfo(), NetworkAnonymizationKey(),
      scheme_host_port, NetLogWithSource(), resolver(), &generic_handler);
  EXPECT_THAT(rv, IsError(ERR_UNSUPPORTED_AUTH_SCHEME));
  EXPECT_TRUE(generic_handler.get() == nullptr);
}
#endif  // BUILDFLAG(USE_EXTERNAL_GSSAPI)

// AllowGssapiLibraryLoad() is only supported on ChromeOS and Linux.
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
TEST_F(HttpAuthHandlerNegotiateTest, AllowGssapiLibraryLoad) {
  // Disabling allow_gssapi_library_load should prevent handler creation.
  SetupMocks(AuthLibrary());
  http_auth_preferences()->set_allow_gssapi_library_load(false);
  std::unique_ptr<HttpAuthHandlerNegotiate> auth_handler;
  int rv = CreateHandler(true, false, true, "http://alias:500", &auth_handler);
  EXPECT_THAT(rv, IsError(ERR_UNSUPPORTED_AUTH_SCHEME));
  EXPECT_FALSE(auth_handler);

  // Handler creation can be dynamically re-enabled.
  http_auth_preferences()->set_allow_gssapi_library_load(true);
  rv = CreateHandler(true, false, true, "http://alias:500", &auth_handler);
  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(auth_handler);
}
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(USE_EXTERNAL_GSSAPI)

namespace {

constexpr char kTargetPrincipalValidationHistogram[] =
    "Net.HttpAuth.NegotiateTargetPrincipalValidationResult";

// Expects an `init_sec_context()` call establishing `context`. The tokens must
// outlive the mock's expectation.
void ExpectInitSecContext(MockAuthLibrary* library,
                          const test::GssContextMockImpl& context,
                          OM_uint32 major_status,
                          const char* expected_input_token,
                          const char* output_token) {
  MockAuthLibrary::SecurityContextQuery query("Negotiate", major_status, 0,
                                              context, expected_input_token,
                                              output_token);
  library->ExpectSecurityContext(
      query.expected_package, query.response_code, query.minor_response_code,
      query.context_info, query.expected_input_token, query.output_token);
}

}  // namespace

class HttpAuthHandlerNegotiateTargetValidationTest
    : public HttpAuthHandlerNegotiateTest {
 public:
  void SetUp() override {
    HttpAuthHandlerNegotiateTest::SetUp();
    AddScopedFeatureList().InitAndEnableFeature(
        features::kValidateNegotiateTargetPrincipalHost);
  }
};

TEST_F(HttpAuthHandlerNegotiateTest,
       TargetValidationFeatureDisabledSkipsPrincipalQuery) {
  // Disabling `kValidateNegotiateTargetPrincipalHost` is the kill switch for
  // the `gss_inquire_context()` and `gss_display_name()` calls themselves.
  AddScopedFeatureList().InitAndDisableFeature(
      features::kValidateNegotiateTargetPrincipalHost);
  base::HistogramTester histogram_tester;

  SetupMocks(AuthLibrary(), "HTTP/canonical.test@REALM");
  std::unique_ptr<HttpAuthPreferences> prefs = CreateHttpAuthPreferences();
  prefs->SetServerAllowlist("alias.test");
  SetHttpAuthPreferences(std::move(prefs));

  std::unique_ptr<HttpAuthHandlerNegotiate> auth_handler;
  ASSERT_EQ(OK, CreateHandler(/*disable_cname_lookup=*/true, /*use_port=*/false,
                              /*synchronous_resolve_mode=*/true,
                              "http://alias.test:80", &auth_handler));
  ASSERT_TRUE(auth_handler);

  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  int rv = auth_handler->GenerateAuthToken(nullptr, &request_info,
                                           callback.callback(), &token);
  EXPECT_THAT(callback.GetResult(rv), IsOk());
  EXPECT_FALSE(token.empty());
  EXPECT_EQ(0, AuthLibrary()->inquire_context_call_count());
  histogram_tester.ExpectTotalCount(kTargetPrincipalValidationHistogram, 0);
}

TEST_F(HttpAuthHandlerNegotiateTargetValidationTest,
       DisableCnamePolicyAllowsAllowlistedLibraryCanonicalization) {
  base::HistogramTester histogram_tester;
  RecordingNetLogObserver net_log_observer;

  SetupMocks(AuthLibrary(), "HTTP/canonical.test@REALM");
  std::unique_ptr<HttpAuthPreferences> prefs = CreateHttpAuthPreferences();
  prefs->SetServerAllowlist("alias.test,canonical.test");
  prefs->SetDelegateAllowlist("alias.test,canonical.test");
  SetHttpAuthPreferences(std::move(prefs));

  std::unique_ptr<HttpAuthHandlerNegotiate> auth_handler;
  ASSERT_EQ(OK, CreateHandler(/*disable_cname_lookup=*/true, /*use_port=*/false,
                              /*synchronous_resolve_mode=*/true,
                              "http://alias.test:80", &auth_handler));
  ASSERT_TRUE(auth_handler);

  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  int rv = auth_handler->GenerateAuthToken(nullptr, &request_info,
                                           callback.callback(), &token);
  EXPECT_THAT(callback.GetResult(rv), IsOk());
  EXPECT_FALSE(token.empty());
  EXPECT_EQ("HTTP@alias.test", auth_handler->spn_for_testing());
  histogram_tester.ExpectUniqueSample(
      kTargetPrincipalValidationHistogram,
      HttpAuthHandlerNegotiate::TargetHostValidationResult::
          kAllowedDifferentHost,
      1);
  EXPECT_TRUE(
      net_log_observer
          .GetEntriesWithType(NetLogEventType::AUTH_TARGET_PRINCIPAL_REJECTED)
          .empty());
}

TEST_F(HttpAuthHandlerNegotiateTargetValidationTest,
       CanonicalTargetNotInServerAllowlist) {
  base::HistogramTester histogram_tester;
  RecordingNetLogObserver net_log_observer;

  SetupMocks(AuthLibrary(), "HTTP/canonical.test@REALM");
  std::unique_ptr<HttpAuthPreferences> prefs = CreateHttpAuthPreferences();
  prefs->SetServerAllowlist("alias.test");
  SetHttpAuthPreferences(std::move(prefs));

  std::unique_ptr<HttpAuthHandlerNegotiate> auth_handler;
  EXPECT_EQ(OK,
            CreateHandler(/*disable_cname_lookup=*/false, /*use_port=*/false,
                          /*synchronous_resolve_mode=*/true,
                          "http://alias.test:80", &auth_handler));
  ASSERT_TRUE(auth_handler);

  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  int rv = auth_handler->GenerateAuthToken(nullptr, &request_info,
                                           callback.callback(), &token);
  EXPECT_THAT(callback.GetResult(rv),
              IsError(ERR_MISCONFIGURED_AUTH_ENVIRONMENT));
  EXPECT_TRUE(token.empty());

  histogram_tester.ExpectUniqueSample(
      kTargetPrincipalValidationHistogram,
      HttpAuthHandlerNegotiate::TargetHostValidationResult::
          kDisallowedByServerAllowlist,
      1);
  std::vector<NetLogEntry> entries = net_log_observer.GetEntriesWithType(
      NetLogEventType::AUTH_TARGET_PRINCIPAL_REJECTED);
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ("alias.test",
            GetStringValueFromParams(entries[0], "requested_host"));
  EXPECT_EQ("canonical.test",
            GetStringValueFromParams(entries[0], "principal_host"));
  EXPECT_EQ("disallowed_by_server_allowlist",
            GetStringValueFromParams(entries[0], "reason"));
}

TEST_F(HttpAuthHandlerNegotiateTargetValidationTest,
       CanonicalTargetNotInServerAllowlistWithCredentials) {
  base::HistogramTester histogram_tester;
  SetupMocks(AuthLibrary(), "HTTP/canonical.test@REALM");
  std::unique_ptr<HttpAuthPreferences> prefs = CreateHttpAuthPreferences();
  prefs->SetServerAllowlist("alias.test");
  SetHttpAuthPreferences(std::move(prefs));

  std::unique_ptr<HttpAuthHandlerNegotiate> auth_handler;
  ASSERT_EQ(OK,
            CreateHandler(/*disable_cname_lookup=*/false, /*use_port=*/false,
                          /*synchronous_resolve_mode=*/true,
                          "http://alias.test:80", &auth_handler));
  ASSERT_TRUE(auth_handler);

  AuthCredentials credentials(u"user", u"pass");
  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  int rv = auth_handler->GenerateAuthToken(&credentials, &request_info,
                                           callback.callback(), &token);
  EXPECT_THAT(callback.GetResult(rv),
              IsError(ERR_MISCONFIGURED_AUTH_ENVIRONMENT));
  EXPECT_TRUE(token.empty());
  histogram_tester.ExpectUniqueSample(
      kTargetPrincipalValidationHistogram,
      HttpAuthHandlerNegotiate::TargetHostValidationResult::
          kDisallowedByServerAllowlist,
      1);
}

TEST_F(HttpAuthHandlerNegotiateTargetValidationTest,
       UserPrincipalNameTargetRejected) {
  base::HistogramTester histogram_tester;
  RecordingNetLogObserver net_log_observer;

  SetupMocks(AuthLibrary(), "user@REALM");
  std::unique_ptr<HttpAuthPreferences> prefs = CreateHttpAuthPreferences();
  prefs->SetServerAllowlist("*");
  SetHttpAuthPreferences(std::move(prefs));

  std::unique_ptr<HttpAuthHandlerNegotiate> auth_handler;
  ASSERT_EQ(OK,
            CreateHandler(/*disable_cname_lookup=*/false, /*use_port=*/false,
                          /*synchronous_resolve_mode=*/true,
                          "http://alias.test:80", &auth_handler));
  ASSERT_TRUE(auth_handler);

  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  int rv = auth_handler->GenerateAuthToken(nullptr, &request_info,
                                           callback.callback(), &token);
  EXPECT_THAT(callback.GetResult(rv),
              IsError(ERR_MISCONFIGURED_AUTH_ENVIRONMENT));
  EXPECT_TRUE(token.empty());

  histogram_tester.ExpectUniqueSample(
      kTargetPrincipalValidationHistogram,
      HttpAuthHandlerNegotiate::TargetHostValidationResult::
          kNonHttpServicePrincipal,
      1);
  std::vector<NetLogEntry> entries = net_log_observer.GetEntriesWithType(
      NetLogEventType::AUTH_TARGET_PRINCIPAL_REJECTED);
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ("alias.test",
            GetStringValueFromParams(entries[0], "requested_host"));
  EXPECT_FALSE(GetOptionalStringValueFromParams(entries[0], "principal_host"));
  EXPECT_EQ("non_http_service_principal",
            GetStringValueFromParams(entries[0], "reason"));
}

TEST_F(HttpAuthHandlerNegotiateTargetValidationTest,
       PercentEncodedTargetPrincipalRejected) {
  base::HistogramTester histogram_tester;
  SetupMocks(AuthLibrary(), "HTTP/app%2edev.test@REALM");
  std::unique_ptr<HttpAuthPreferences> prefs = CreateHttpAuthPreferences();
  prefs->SetServerAllowlist("alias.test,*.dev.test");
  SetHttpAuthPreferences(std::move(prefs));

  std::unique_ptr<HttpAuthHandlerNegotiate> auth_handler;
  ASSERT_EQ(OK,
            CreateHandler(/*disable_cname_lookup=*/false, /*use_port=*/false,
                          /*synchronous_resolve_mode=*/true,
                          "http://alias.test:80", &auth_handler));
  ASSERT_TRUE(auth_handler);

  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  int rv = auth_handler->GenerateAuthToken(nullptr, &request_info,
                                           callback.callback(), &token);
  EXPECT_THAT(callback.GetResult(rv),
              IsError(ERR_MISCONFIGURED_AUTH_ENVIRONMENT));
  EXPECT_TRUE(token.empty());
  histogram_tester.ExpectUniqueSample(
      kTargetPrincipalValidationHistogram,
      HttpAuthHandlerNegotiate::TargetHostValidationResult::kUnparsableHost, 1);
}

// `AuthServerAllowlist` permits the canonical host but
// `AuthNegotiateDelegateAllowlist` doesn't, so the unsent first-round token is
// regenerated without delegation instead of failing authentication.
TEST_F(HttpAuthHandlerNegotiateTargetValidationTest,
       CanonicalTargetNotInDelegateAllowlistClampsDelegation) {
  base::HistogramTester histogram_tester;
  RecordingNetLogObserver net_log_observer;

  test::GssContextMockImpl context("localhost", "HTTP/canonical.test@REALM", 23,
                                   *CHROME_GSS_SPNEGO_MECH_OID_DESC, 0, 1, 0);
  ExpectInitSecContext(AuthLibrary(), context, GSS_S_CONTINUE_NEEDED,
                       /*expected_input_token=*/nullptr, "DelegatingToken");
  ExpectInitSecContext(AuthLibrary(), context, GSS_S_CONTINUE_NEEDED,
                       /*expected_input_token=*/nullptr, "NonDelegatingToken");
  std::unique_ptr<HttpAuthPreferences> prefs = CreateHttpAuthPreferences();
  prefs->SetServerAllowlist("alias.test,canonical.test");
  prefs->SetDelegateAllowlist("alias.test");
  SetHttpAuthPreferences(std::move(prefs));

  std::unique_ptr<HttpAuthHandlerNegotiate> auth_handler;
  ASSERT_EQ(OK,
            CreateHandler(/*disable_cname_lookup=*/false, /*use_port=*/false,
                          /*synchronous_resolve_mode=*/true,
                          "http://alias.test:80", &auth_handler));
  ASSERT_TRUE(auth_handler);

  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  int rv = auth_handler->GenerateAuthToken(nullptr, &request_info,
                                           callback.callback(), &token);
  EXPECT_THAT(callback.GetResult(rv), IsOk());
  EXPECT_EQ("Negotiate " + base::Base64Encode("NonDelegatingToken"), token);
  EXPECT_EQ(AuthLibrary()->init_sec_context_req_flags(),
            (std::vector<OM_uint32>{GSS_C_DELEG_FLAG, 0}));

  histogram_tester.ExpectBucketCount(
      kTargetPrincipalValidationHistogram,
      HttpAuthHandlerNegotiate::TargetHostValidationResult::
          kAllowedWithClampedDelegation,
      1);
  histogram_tester.ExpectBucketCount(
      kTargetPrincipalValidationHistogram,
      HttpAuthHandlerNegotiate::TargetHostValidationResult::
          kAllowedDifferentHost,
      1);
  histogram_tester.ExpectTotalCount(kTargetPrincipalValidationHistogram, 2);
  std::vector<NetLogEntry> entries = net_log_observer.GetEntriesWithType(
      NetLogEventType::AUTH_TARGET_PRINCIPAL_DELEGATION_CLAMPED);
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ("alias.test",
            GetStringValueFromParams(entries[0], "requested_host"));
  EXPECT_EQ("canonical.test",
            GetStringValueFromParams(entries[0], "principal_host"));
  EXPECT_TRUE(
      net_log_observer
          .GetEntriesWithType(NetLogEventType::AUTH_TARGET_PRINCIPAL_REJECTED)
          .empty());
}

TEST_F(HttpAuthHandlerNegotiateTargetValidationTest,
       CanonicalTargetDowngradesDelegationFromUnconstrainedToKdcPolicy) {
  test::GssContextMockImpl context("localhost", "HTTP/canonical.test@REALM", 23,
                                   *CHROME_GSS_SPNEGO_MECH_OID_DESC, 0, 1, 0);
  ExpectInitSecContext(AuthLibrary(), context, GSS_S_CONTINUE_NEEDED,
                       /*expected_input_token=*/nullptr, "UnconstrainedToken");
  ExpectInitSecContext(AuthLibrary(), context, GSS_S_CONTINUE_NEEDED,
                       /*expected_input_token=*/nullptr, "KdcPolicyToken");
  std::unique_ptr<HttpAuthPreferences> prefs = CreateHttpAuthPreferences();
  prefs->SetServerAllowlist("alias.test,canonical.test");
  prefs->SetDelegateAllowlist("alias.test,canonical.test");
  SetHttpAuthPreferences(std::move(prefs));

  // `Init()` snapshots `DelegationType::kUnconstrained` on `auth_system_`.
  std::unique_ptr<HttpAuthHandlerNegotiate> auth_handler;
  ASSERT_EQ(OK,
            CreateHandler(/*disable_cname_lookup=*/false, /*use_port=*/false,
                          /*synchronous_resolve_mode=*/true,
                          "http://alias.test:80", &auth_handler));
  ASSERT_TRUE(auth_handler);

  // Tightening policy before token completion leaves `canonical.test` with
  // `kByKdcPolicy`, which is weaker than the `kUnconstrained` flag already set
  // on `auth_system_`, so the first-round token is regenerated with it.
  http_auth_preferences()->set_delegate_by_kdc_policy(true);

  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  int rv = auth_handler->GenerateAuthToken(nullptr, &request_info,
                                           callback.callback(), &token);
  EXPECT_THAT(callback.GetResult(rv), IsOk());
  EXPECT_EQ("Negotiate " + base::Base64Encode("KdcPolicyToken"), token);
  EXPECT_EQ(
      AuthLibrary()->init_sec_context_req_flags(),
      (std::vector<OM_uint32>{GSS_C_DELEG_FLAG, GSS_C_DELEG_POLICY_FLAG}));
}

TEST_F(HttpAuthHandlerNegotiateTargetValidationTest,
       DisableCnameDisabledAllowsAllowlistedCanonicalization) {
  SetupMocks(AuthLibrary(), "HTTP/canonical.test@REALM");
  std::unique_ptr<HttpAuthPreferences> prefs = CreateHttpAuthPreferences();
  prefs->SetServerAllowlist("alias.test,canonical.test");
  SetHttpAuthPreferences(std::move(prefs));

  std::unique_ptr<HttpAuthHandlerNegotiate> auth_handler;
  EXPECT_EQ(OK,
            CreateHandler(/*disable_cname_lookup=*/false, /*use_port=*/false,
                          /*synchronous_resolve_mode=*/true,
                          "http://alias.test:80", &auth_handler));
  ASSERT_TRUE(auth_handler);

  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  int rv = auth_handler->GenerateAuthToken(nullptr, &request_info,
                                           callback.callback(), &token);
  EXPECT_THAT(callback.GetResult(rv), IsOk());
  EXPECT_FALSE(token.empty());
}

TEST_F(HttpAuthHandlerNegotiateTargetValidationTest,
       AllowlistedCanonicalizationWithKdcPolicyDelegation) {
  SetupMocks(AuthLibrary(), "HTTP/canonical.test@REALM");
  std::unique_ptr<HttpAuthPreferences> prefs = CreateHttpAuthPreferences();
  prefs->SetServerAllowlist("alias.test,canonical.test");
  prefs->SetDelegateAllowlist("alias.test,canonical.test");
  prefs->set_delegate_by_kdc_policy(true);
  SetHttpAuthPreferences(std::move(prefs));

  std::unique_ptr<HttpAuthHandlerNegotiate> auth_handler;
  ASSERT_EQ(OK,
            CreateHandler(/*disable_cname_lookup=*/false, /*use_port=*/false,
                          /*synchronous_resolve_mode=*/true,
                          "http://alias.test:80", &auth_handler));
  ASSERT_TRUE(auth_handler);

  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  int rv = auth_handler->GenerateAuthToken(nullptr, &request_info,
                                           callback.callback(), &token);
  EXPECT_THAT(callback.GetResult(rv), IsOk());
  EXPECT_FALSE(token.empty());
}

TEST_F(HttpAuthHandlerNegotiateTargetValidationTest,
       AllowlistedCanonicalizationWithUnconstrainedDelegation) {
  SetupMocks(AuthLibrary(), "HTTP/canonical.test@REALM");
  std::unique_ptr<HttpAuthPreferences> prefs = CreateHttpAuthPreferences();
  prefs->SetServerAllowlist("alias.test,canonical.test");
  prefs->SetDelegateAllowlist("alias.test,canonical.test");
  SetHttpAuthPreferences(std::move(prefs));

  std::unique_ptr<HttpAuthHandlerNegotiate> auth_handler;
  ASSERT_EQ(OK,
            CreateHandler(/*disable_cname_lookup=*/false, /*use_port=*/false,
                          /*synchronous_resolve_mode=*/true,
                          "http://alias.test:80", &auth_handler));
  ASSERT_TRUE(auth_handler);

  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  int rv = auth_handler->GenerateAuthToken(nullptr, &request_info,
                                           callback.callback(), &token);
  EXPECT_THAT(callback.GetResult(rv), IsOk());
  EXPECT_FALSE(token.empty());
}

TEST_F(HttpAuthHandlerNegotiateTargetValidationTest,
       HostServicePrefixAndTrailingDotUrlMatchSameHost) {
  base::HistogramTester histogram_tester;
  SetupMocks(AuthLibrary(), "host/service.test@REALM");
  std::unique_ptr<HttpAuthPreferences> prefs = CreateHttpAuthPreferences();
  prefs->SetServerAllowlist("*");
  SetHttpAuthPreferences(std::move(prefs));

  std::unique_ptr<HttpAuthHandlerNegotiate> auth_handler;
  ASSERT_EQ(OK, CreateHandler(/*disable_cname_lookup=*/true, /*use_port=*/false,
                              /*synchronous_resolve_mode=*/true,
                              "http://service.test.:80", &auth_handler));
  ASSERT_TRUE(auth_handler);

  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  int rv = auth_handler->GenerateAuthToken(nullptr, &request_info,
                                           callback.callback(), &token);
  EXPECT_THAT(callback.GetResult(rv), IsOk());
  EXPECT_FALSE(token.empty());
  histogram_tester.ExpectUniqueSample(
      kTargetPrincipalValidationHistogram,
      HttpAuthHandlerNegotiate::TargetHostValidationResult::kSameHost, 1);
}

TEST_F(HttpAuthHandlerNegotiateTargetValidationTest,
       NtlmsspFallbackUnderSpnegoSkipsTargetValidation) {
  base::HistogramTester histogram_tester;
  RecordingNetLogObserver net_log_observer;

  PaddedOidDesc ntlmssp_mech_oid = {
      .desc = {10,
               const_cast<char*>("\x2b\x06\x01\x04\x01\x82\x37\x02\x02\x0a")}};
  PaddedOidDesc user_name_oid = {
      .desc = {10,
               const_cast<char*>("\x2a\x86\x48\x86\xf7\x12\x01\x02\x01\x01")}};
  test::GssContextMockImpl context("localhost", "HTTP@alias.test", 23,
                                   ntlmssp_mech_oid.desc, 0, 1, 1);
  context.targ_name_type = user_name_oid.desc;
  static constexpr char kNtlmToken[] = "NTLMSSP";
  MockAuthLibrary::SecurityContextQuery query("Negotiate", GSS_S_COMPLETE, 0,
                                              context, nullptr, kNtlmToken);
  AuthLibrary()->ExpectSecurityContext(
      query.expected_package, query.response_code, query.minor_response_code,
      query.context_info, query.expected_input_token, query.output_token);

  std::unique_ptr<HttpAuthPreferences> prefs = CreateHttpAuthPreferences();
  prefs->SetServerAllowlist("alias.test");
  SetHttpAuthPreferences(std::move(prefs));

  std::unique_ptr<HttpAuthHandlerNegotiate> auth_handler;
  ASSERT_EQ(OK, CreateHandler(/*disable_cname_lookup=*/true, /*use_port=*/false,
                              /*synchronous_resolve_mode=*/true,
                              "http://alias.test:80", &auth_handler));
  ASSERT_TRUE(auth_handler);

  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  int rv = auth_handler->GenerateAuthToken(nullptr, &request_info,
                                           callback.callback(), &token);
  EXPECT_THAT(callback.GetResult(rv), IsOk());
  EXPECT_FALSE(token.empty());
  histogram_tester.ExpectTotalCount(kTargetPrincipalValidationHistogram, 0);
  EXPECT_TRUE(
      net_log_observer
          .GetEntriesWithType(NetLogEventType::AUTH_TARGET_PRINCIPAL_REJECTED)
          .empty());
}

TEST_F(HttpAuthHandlerNegotiateTargetValidationTest,
       IncompleteSpnegoContextWithNoTargetNameSkipsValidation) {
  base::HistogramTester histogram_tester;

  test::GssContextMockImpl context("localhost", "", 23,
                                   *CHROME_GSS_SPNEGO_MECH_OID_DESC, 0, 1, 0);
  context.has_targ_name = false;
  static constexpr char kSpnegoToken[] = "SPNEGO";
  MockAuthLibrary::SecurityContextQuery query(
      "Negotiate", GSS_S_CONTINUE_NEEDED, 0, context, nullptr, kSpnegoToken);
  AuthLibrary()->ExpectSecurityContext(
      query.expected_package, query.response_code, query.minor_response_code,
      query.context_info, query.expected_input_token, query.output_token);

  std::unique_ptr<HttpAuthPreferences> prefs = CreateHttpAuthPreferences();
  prefs->SetServerAllowlist("alias.test");
  SetHttpAuthPreferences(std::move(prefs));

  std::unique_ptr<HttpAuthHandlerNegotiate> auth_handler;
  ASSERT_EQ(OK, CreateHandler(/*disable_cname_lookup=*/true, /*use_port=*/false,
                              /*synchronous_resolve_mode=*/true,
                              "http://alias.test:80", &auth_handler));
  ASSERT_TRUE(auth_handler);

  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  int rv = auth_handler->GenerateAuthToken(nullptr, &request_info,
                                           callback.callback(), &token);
  EXPECT_THAT(callback.GetResult(rv), IsOk());
  EXPECT_FALSE(token.empty());
  histogram_tester.ExpectTotalCount(kTargetPrincipalValidationHistogram, 0);
}

TEST_F(HttpAuthHandlerNegotiateTargetValidationTest,
       BareIpv6TargetPrincipalMatchesUrl) {
  base::HistogramTester histogram_tester;
  SetupMocks(AuthLibrary(), "HTTP/2001:DB8::1@REALM");
  std::unique_ptr<HttpAuthPreferences> prefs = CreateHttpAuthPreferences();
  prefs->SetServerAllowlist("*");
  SetHttpAuthPreferences(std::move(prefs));

  std::unique_ptr<HttpAuthHandlerNegotiate> auth_handler;
  ASSERT_EQ(OK, CreateHandler(/*disable_cname_lookup=*/true, /*use_port=*/false,
                              /*synchronous_resolve_mode=*/true,
                              "http://[2001:db8::1]:80", &auth_handler));
  ASSERT_TRUE(auth_handler);

  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  int rv = auth_handler->GenerateAuthToken(nullptr, &request_info,
                                           callback.callback(), &token);
  EXPECT_THAT(callback.GetResult(rv), IsOk());
  EXPECT_FALSE(token.empty());
  histogram_tester.ExpectUniqueSample(
      kTargetPrincipalValidationHistogram,
      HttpAuthHandlerNegotiate::TargetHostValidationResult::kSameHost, 1);
}

// Principal hosts that only match the requested host after many-to-one host
// canonicalization are distinct principals and must not take the `kSameHost`
// path, which skips the allowlist checks.
TEST_F(HttpAuthHandlerNegotiateTargetValidationTest,
       NonCanonicalTargetPrincipalHostsRejected) {
  static constexpr struct {
    const char* target_name;
    const char* url;
  } kTestCases[] = {
      {"HTTP/0x0a.0.0.2@REALM", "http://10.0.0.2:80"},
      {"HTTP/167772162@REALM", "http://10.0.0.2:80"},
      {"HTTP/2001:0DB8:0:0:0:0:0:1@REALM", "http://[2001:db8::1]:80"},
      {"HTTP/b\xc3\xbc"
       "cher.test@REALM",
       "http://xn--bcher-kva.test:80"},
      {"HTTP/\xef\xbd\x81"
       "lias.test@REALM",
       "http://alias.test:80"},
      {"HTTP/ali\xc2\xad"
       "as.test@REALM",
       "http://alias.test:80"},
  };
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.target_name);
    base::HistogramTester histogram_tester;
    test::GssContextMockImpl context("localhost", test_case.target_name, 23,
                                     *CHROME_GSS_SPNEGO_MECH_OID_DESC, 0, 1, 0);
    static constexpr char kSpnegoToken[] = "SPNEGO";
    MockAuthLibrary::SecurityContextQuery query(
        "Negotiate", GSS_S_CONTINUE_NEEDED, 0, context, nullptr, kSpnegoToken);
    AuthLibrary()->ExpectSecurityContext(
        query.expected_package, query.response_code, query.minor_response_code,
        query.context_info, query.expected_input_token, query.output_token);
    std::unique_ptr<HttpAuthPreferences> prefs = CreateHttpAuthPreferences();
    prefs->SetServerAllowlist("*");
    SetHttpAuthPreferences(std::move(prefs));

    std::unique_ptr<HttpAuthHandlerNegotiate> auth_handler;
    ASSERT_EQ(OK,
              CreateHandler(/*disable_cname_lookup=*/true, /*use_port=*/false,
                            /*synchronous_resolve_mode=*/true, test_case.url,
                            &auth_handler));
    ASSERT_TRUE(auth_handler);

    TestCompletionCallback callback;
    HttpRequestInfo request_info;
    std::string token;
    int rv = auth_handler->GenerateAuthToken(nullptr, &request_info,
                                             callback.callback(), &token);
    EXPECT_THAT(callback.GetResult(rv),
                IsError(ERR_MISCONFIGURED_AUTH_ENVIRONMENT));
    EXPECT_TRUE(token.empty());
    histogram_tester.ExpectUniqueSample(
        kTargetPrincipalValidationHistogram,
        HttpAuthHandlerNegotiate::TargetHostValidationResult::kUnparsableHost,
        1);
  }
}

// MIT krb5 reports the SPNEGO `mech_type` on partially established contexts,
// where an underlying `gss-ntlmssp` mechanism displays its target as
// `HTTP/<host>` under `GSS_C_NT_HOSTBASED_SERVICE`. That host is validated, so
// the NTLM fallback still succeeds when it matches the URL host.
TEST_F(HttpAuthHandlerNegotiateTargetValidationTest,
       PartiallyEstablishedSpnegoNtlmTargetMatchesUrl) {
  base::HistogramTester histogram_tester;

  PaddedOidDesc hostbased_service_oid = {
      .desc = {10,
               const_cast<char*>("\x2a\x86\x48\x86\xf7\x12\x01\x02\x01\x04")}};
  test::GssContextMockImpl context("localhost", "HTTP/alias.test", 23,
                                   *CHROME_GSS_SPNEGO_MECH_OID_DESC, 0, 1, 0);
  context.targ_name_type = hostbased_service_oid.desc;
  static constexpr char kNtlmToken[] = "NTLMSSP";
  MockAuthLibrary::SecurityContextQuery query(
      "Negotiate", GSS_S_CONTINUE_NEEDED, 0, context, nullptr, kNtlmToken);
  AuthLibrary()->ExpectSecurityContext(
      query.expected_package, query.response_code, query.minor_response_code,
      query.context_info, query.expected_input_token, query.output_token);

  std::unique_ptr<HttpAuthPreferences> prefs = CreateHttpAuthPreferences();
  prefs->SetServerAllowlist("alias.test");
  SetHttpAuthPreferences(std::move(prefs));

  std::unique_ptr<HttpAuthHandlerNegotiate> auth_handler;
  ASSERT_EQ(OK, CreateHandler(/*disable_cname_lookup=*/true, /*use_port=*/false,
                              /*synchronous_resolve_mode=*/true,
                              "http://alias.test:80", &auth_handler));
  ASSERT_TRUE(auth_handler);

  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  int rv = auth_handler->GenerateAuthToken(nullptr, &request_info,
                                           callback.callback(), &token);
  EXPECT_THAT(callback.GetResult(rv), IsOk());
  EXPECT_FALSE(token.empty());
  histogram_tester.ExpectUniqueSample(
      kTargetPrincipalValidationHistogram,
      HttpAuthHandlerNegotiate::TargetHostValidationResult::kSameHost, 1);
}

// On partially established SPNEGO contexts, 1-component `GSS_C_NT_USER_NAME`
// and `GSS_KRB5_NT_PRINCIPAL_NAME` principals (`HTTP@<name>`) are still
// rejected as `kNonHttpServicePrincipal`.
TEST_F(HttpAuthHandlerNegotiateTargetValidationTest,
       PartiallyEstablishedSpnegoKerberosUserPrincipalRejected) {
  const struct {
    const char* label;
    PaddedOidDesc oid;
  } kNameTypes[] = {
      {"GSS_C_NT_USER_NAME",
       {.desc = {10, const_cast<char*>(
                         "\x2a\x86\x48\x86\xf7\x12\x01\x02\x01\x01")}}},
      {"GSS_KRB5_NT_PRINCIPAL_NAME",
       {.desc = {10, const_cast<char*>(
                         "\x2a\x86\x48\x86\xf7\x12\x01\x02\x02\x01")}}},
  };
  for (const auto& name_type : kNameTypes) {
    SCOPED_TRACE(name_type.label);
    base::HistogramTester histogram_tester;
    test::GssContextMockImpl context("localhost", "HTTP@alias.test", 23,
                                     *CHROME_GSS_SPNEGO_MECH_OID_DESC, 0, 1,
                                     /*open=*/0);
    context.targ_name_type = name_type.oid.desc;
    static constexpr char kSpnegoToken[] = "SPNEGO";
    MockAuthLibrary::SecurityContextQuery query(
        "Negotiate", GSS_S_CONTINUE_NEEDED, 0, context, nullptr, kSpnegoToken);
    AuthLibrary()->ExpectSecurityContext(
        query.expected_package, query.response_code, query.minor_response_code,
        query.context_info, query.expected_input_token, query.output_token);

    std::unique_ptr<HttpAuthPreferences> prefs = CreateHttpAuthPreferences();
    prefs->SetServerAllowlist("alias.test");
    SetHttpAuthPreferences(std::move(prefs));

    std::unique_ptr<HttpAuthHandlerNegotiate> auth_handler;
    ASSERT_EQ(OK, CreateHandler(/*disable_cname_lookup=*/true,
                                /*use_port=*/false,
                                /*synchronous_resolve_mode=*/true,
                                "http://alias.test:80", &auth_handler));
    ASSERT_TRUE(auth_handler);

    TestCompletionCallback callback;
    HttpRequestInfo request_info;
    std::string token;
    int rv = auth_handler->GenerateAuthToken(nullptr, &request_info,
                                             callback.callback(), &token);
    EXPECT_THAT(callback.GetResult(rv),
                IsError(ERR_MISCONFIGURED_AUTH_ENVIRONMENT));
    EXPECT_TRUE(token.empty());
    histogram_tester.ExpectUniqueSample(
        kTargetPrincipalValidationHistogram,
        HttpAuthHandlerNegotiate::TargetHostValidationResult::
            kNonHttpServicePrincipal,
        1);
  }
}

// Once a token has been returned (for example, when policy tightens
// mid-handshake), a disallowed delegation can't be withdrawn by regenerating
// the token, so authentication fails closed.
TEST_F(HttpAuthHandlerNegotiateTargetValidationTest,
       DelegationDisallowedAfterFirstRoundFailsClosed) {
  base::HistogramTester histogram_tester;

  test::GssContextMockImpl round1_context(
      "localhost", "HTTP/canonical.test@REALM", 23,
      *CHROME_GSS_SPNEGO_MECH_OID_DESC, 0, 1, /*open=*/0);
  test::GssContextMockImpl round2_context(
      "localhost", "HTTP/canonical.test@REALM", 23,
      *CHROME_GSS_SPNEGO_MECH_OID_DESC, 0, 1, /*open=*/1);
  ExpectInitSecContext(AuthLibrary(), round1_context, GSS_S_CONTINUE_NEEDED,
                       /*expected_input_token=*/nullptr, "Round1Token");
  ExpectInitSecContext(AuthLibrary(), round2_context, GSS_S_COMPLETE,
                       "ServerToken", "Round2Token");
  std::unique_ptr<HttpAuthPreferences> prefs = CreateHttpAuthPreferences();
  prefs->SetServerAllowlist("alias.test,canonical.test");
  prefs->SetDelegateAllowlist("alias.test,canonical.test");
  SetHttpAuthPreferences(std::move(prefs));

  std::unique_ptr<HttpAuthHandlerNegotiate> auth_handler;
  ASSERT_EQ(OK, CreateHandler(/*disable_cname_lookup=*/true, /*use_port=*/false,
                              /*synchronous_resolve_mode=*/true,
                              "http://alias.test:80", &auth_handler));
  ASSERT_TRUE(auth_handler);

  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  int rv = auth_handler->GenerateAuthToken(nullptr, &request_info,
                                           callback.callback(), &token);
  EXPECT_THAT(callback.GetResult(rv), IsOk());
  EXPECT_EQ("Negotiate " + base::Base64Encode("Round1Token"), token);

  http_auth_preferences()->SetDelegateAllowlist("alias.test");
  const std::string challenge_text =
      "Negotiate " + base::Base64Encode("ServerToken");
  HttpAuthChallengeTokenizer challenge(challenge_text);
  EXPECT_EQ(HttpAuth::AUTHORIZATION_RESULT_ACCEPT,
            auth_handler->HandleAnotherChallenge(&challenge));

  std::string round2_token;
  rv = auth_handler->GenerateAuthToken(nullptr, &request_info,
                                       callback.callback(), &round2_token);
  EXPECT_THAT(callback.GetResult(rv),
              IsError(ERR_MISCONFIGURED_AUTH_ENVIRONMENT));
  EXPECT_TRUE(round2_token.empty());
  EXPECT_EQ(AuthLibrary()->init_sec_context_req_flags(),
            (std::vector<OM_uint32>{GSS_C_DELEG_FLAG, GSS_C_DELEG_FLAG}));
  histogram_tester.ExpectBucketCount(
      kTargetPrincipalValidationHistogram,
      HttpAuthHandlerNegotiate::TargetHostValidationResult::
          kAllowedDifferentHost,
      1);
  histogram_tester.ExpectBucketCount(
      kTargetPrincipalValidationHistogram,
      HttpAuthHandlerNegotiate::TargetHostValidationResult::
          kDisallowedByDelegateAllowlist,
      1);
  histogram_tester.ExpectTotalCount(kTargetPrincipalValidationHistogram, 2);
}

// Heimdal SPNEGO returns `GSS_S_NO_CONTEXT` from `gss_inquire_context()` when
// the first-round `NegTokenInit` only proposes a mechanism list (for example,
// without a Kerberos TGT when falling back to NTLMSSP). No ticket exists yet,
// so validation is skipped instead of failing closed.
TEST_F(HttpAuthHandlerNegotiateTargetValidationTest,
       MechListOnlySpnegoContextSkipsValidation) {
  base::HistogramTester histogram_tester;

  test::GssContextMockImpl context("localhost", "HTTP/canonical.test@REALM", 23,
                                   *CHROME_GSS_SPNEGO_MECH_OID_DESC, 0, 1,
                                   /*open=*/0);
  context.inquire_context_status = GSS_S_NO_CONTEXT;
  ExpectInitSecContext(AuthLibrary(), context, GSS_S_CONTINUE_NEEDED,
                       /*expected_input_token=*/nullptr, "MechListToken");
  std::unique_ptr<HttpAuthPreferences> prefs = CreateHttpAuthPreferences();
  prefs->SetServerAllowlist("alias.test");
  SetHttpAuthPreferences(std::move(prefs));

  std::unique_ptr<HttpAuthHandlerNegotiate> auth_handler;
  ASSERT_EQ(OK, CreateHandler(/*disable_cname_lookup=*/true, /*use_port=*/false,
                              /*synchronous_resolve_mode=*/true,
                              "http://alias.test:80", &auth_handler));
  ASSERT_TRUE(auth_handler);

  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  int rv = auth_handler->GenerateAuthToken(nullptr, &request_info,
                                           callback.callback(), &token);
  EXPECT_THAT(callback.GetResult(rv), IsOk());
  EXPECT_EQ("Negotiate " + base::Base64Encode("MechListToken"), token);
  histogram_tester.ExpectTotalCount(kTargetPrincipalValidationHistogram, 0);
}

#endif  // BUILDFLAG(USE_EXTERNAL_GSSAPI)
#endif  // BUILDFLAG(IS_POSIX)

TEST_F(HttpAuthHandlerNegotiateTest, ProxyAuthBypassesTargetValidation) {
  AddScopedFeatureList().InitAndEnableFeature(
      features::kValidateNegotiateTargetPrincipalHost);
  SetupMocks(AuthLibrary(), "HTTP/canonical.test@REALM");
  std::unique_ptr<HttpAuthPreferences> prefs = CreateHttpAuthPreferences();
  prefs->SetServerAllowlist("alias.test");
  SetHttpAuthPreferences(std::move(prefs));

  std::unique_ptr<HttpAuthHandlerNegotiate> auth_handler;
  EXPECT_EQ(
      OK, CreateProxyHandler(/*disable_cname_lookup=*/true, /*use_port=*/false,
                             /*synchronous_resolve_mode=*/true,
                             "http://alias.test:80", &auth_handler));
  ASSERT_TRUE(auth_handler);

  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  int rv = auth_handler->GenerateAuthToken(nullptr, &request_info,
                                           callback.callback(), &token);
  EXPECT_THAT(callback.GetResult(rv), IsOk());
  EXPECT_FALSE(token.empty());
}

class TestAuthSystem : public HttpAuthMechanism {
 public:
  TestAuthSystem() = default;
  ~TestAuthSystem() override = default;

  // HttpAuthMechanism implementation:
  bool Init(const NetLogWithSource&) override { return true; }
  bool NeedsIdentity() const override { return true; }
  bool AllowsExplicitCredentials() const override { return true; }

  HttpAuth::AuthorizationResult ParseChallenge(
      HttpAuthChallengeTokenizer* tok) override {
    return HttpAuth::AUTHORIZATION_RESULT_ACCEPT;
  }

  int GenerateAuthToken(const AuthCredentials* credentials,
                        const std::string& spn,
                        const std::string& channel_bindings,
                        std::string* auth_token,
                        const NetLogWithSource& net_log,
                        CompletionOnceCallback callback) override {
    *auth_token = kFakeToken;
    return OK;
  }

  void SetDelegation(HttpAuth::DelegationType delegation_type) override {}
};

TEST_F(HttpAuthHandlerNegotiateTest, OverrideAuthSystem) {
  auto negotiate_factory =
      std::make_unique<HttpAuthHandlerNegotiate::Factory>(base::BindRepeating(
          [](const HttpAuthPreferences*) -> std::unique_ptr<HttpAuthMechanism> {
            return std::make_unique<TestAuthSystem>();
          }));
  negotiate_factory->set_http_auth_preferences(http_auth_preferences());
#if BUILDFLAG(IS_WIN)
  negotiate_factory->set_library(
      base::MakeRefCounted<MockAuthLibrary>(NEGOSSP_NAME));
#elif !BUILDFLAG(IS_ANDROID)
  negotiate_factory->set_library(base::MakeRefCounted<MockAuthLibrary>());
#endif

  url::SchemeHostPort scheme_host_port{GURL("http://www.example.com")};
  std::unique_ptr<HttpAuthHandler> handler;
  EXPECT_EQ(OK, negotiate_factory->CreateAuthHandlerFromString(
                    "Negotiate", HttpAuth::AUTH_SERVER, SSLInfo(),
                    NetworkAnonymizationKey(), scheme_host_port,
                    NetLogWithSource(), resolver(), &handler));
  EXPECT_TRUE(handler);

  TestCompletionCallback callback;
  std::string auth_token;
  HttpRequestInfo request_info;
  EXPECT_EQ(OK, callback.GetResult(handler->GenerateAuthToken(
                    nullptr, &request_info, callback.callback(), &auth_token)));
  EXPECT_EQ(kFakeToken, auth_token);
}

}  // namespace net
