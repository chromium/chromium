// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_sspi_win.h"

#include <string_view>
#include <vector>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_restrictions.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/request_priority.h"
#include "net/base/test_completion_callback.h"
#include "net/http/http_auth.h"
#include "net/http/http_auth_challenge_tokenizer.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_auth_handler_negotiate.h"
#include "net/http/http_auth_handler_ntlm.h"
#include "net/http/http_auth_preferences.h"
#include "net/http/http_auth_scheme.h"
#include "net/http/http_request_info.h"
#include "net/http/http_status_code.h"
#include "net/http/mock_sspi_library_win.h"
#include "net/log/net_log_entry.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_with_source.h"
#include "net/log/test_net_log.h"
#include "net/ssl/ssl_info.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

using net::test::IsError;
using net::test::IsOk;

namespace net {

namespace {

void MatchDomainUserAfterSplit(const std::u16string& combined,
                               const std::u16string& expected_domain,
                               const std::u16string& expected_user) {
  std::u16string actual_domain;
  std::u16string actual_user;
  SplitDomainAndUser(combined, &actual_domain, &actual_user);
  EXPECT_EQ(expected_domain, actual_domain);
  EXPECT_EQ(expected_user, actual_user);
}

const ULONG kMaxTokenLength = 100;

// Parks inside InitializeSecurityContext() until released.
class BlockingSSPILibrary : public MockSSPILibrary {
 public:
  explicit BlockingSSPILibrary(const wchar_t* package)
      : MockSSPILibrary(package) {}

  SECURITY_STATUS InitializeSecurityContext(PCredHandle phCredential,
                                            PCtxtHandle phContext,
                                            SEC_WCHAR* pszTargetName,
                                            unsigned long fContextReq,
                                            unsigned long Reserved1,
                                            unsigned long TargetDataRep,
                                            PSecBufferDesc pInput,
                                            unsigned long Reserved2,
                                            PCtxtHandle phNewContext,
                                            PSecBufferDesc pOutput,
                                            unsigned long* contextAttr,
                                            PTimeStamp ptsExpiry) override {
    entered_.Signal();
    if (on_entered_) {
      std::move(on_entered_).Run();
    }
    {
      // Bounded so that a synchronous implementation, which would park the
      // calling sequence here with nobody left to release it, fails the
      // ERR_IO_PENDING expectation instead of deadlocking.
      base::ScopedAllowBaseSyncPrimitivesForTesting allow_wait;
      released_ = release_.TimedWait(TestTimeouts::action_max_timeout());
    }
    return MockSSPILibrary::InitializeSecurityContext(
        phCredential, phContext, pszTargetName, fContextReq, Reserved1,
        TargetDataRep, pInput, Reserved2, phNewContext, pOutput, contextAttr,
        ptsExpiry);
  }

  void WaitUntilEntered() {
    base::ScopedAllowBaseSyncPrimitivesForTesting allow_wait;
    entered_.Wait();
  }
  void SetOnEntered(base::OnceClosure on_entered) {
    on_entered_ = std::move(on_entered);
  }
  void Release() { release_.Signal(); }
  bool WasReleased() const { return released_; }

 protected:
  ~BlockingSSPILibrary() override = default;

 private:
  base::WaitableEvent entered_;
  base::WaitableEvent release_;
  base::OnceClosure on_entered_;
  bool released_ = false;
};

// Answers /negotiate with a Negotiate challenge until the request carries a
// Negotiate token, and every other path with 200.
std::unique_ptr<test_server::HttpResponse> HandleNegotiateRequest(
    const test_server::HttpRequest& request) {
  auto response = std::make_unique<test_server::BasicHttpResponse>();
  if (request.relative_url == "/negotiate") {
    auto it = request.headers.find("Authorization");
    if (it == request.headers.end() || !it->second.starts_with("Negotiate ")) {
      response->set_code(HTTP_UNAUTHORIZED);
      response->AddCustomHeader("WWW-Authenticate", "Negotiate");
      return response;
    }
  }
  response->set_code(HTTP_OK);
  response->set_content("ok");
  return response;
}

int GenerateAuthToken(HttpAuthSSPI* auth_sspi,
                      std::string* auth_token,
                      const NetLogWithSource& net_log = NetLogWithSource()) {
  TestCompletionCallback callback;
  int rv = auth_sspi->GenerateAuthToken(nullptr, "HTTP/intranet.google.com",
                                        std::string(), auth_token, net_log,
                                        callback.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  return callback.GetResult(rv);
}

}  // namespace

class HttpAuthSSPITest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(HttpAuthSSPITest, SplitUserAndDomain) {
  MatchDomainUserAfterSplit(u"foobar", u"", u"foobar");
  MatchDomainUserAfterSplit(u"FOO\\bar", u"FOO", u"bar");
}

TEST_F(HttpAuthSSPITest, DetermineMaxTokenLength_Normal) {
  SecPkgInfoW package_info = {};
  package_info.cbMaxToken = 1337;

  auto mock_library = base::MakeRefCounted<MockSSPILibrary>(L"NTLM");
  mock_library->ExpectQuerySecurityPackageInfo(SEC_E_OK, &package_info);
  ULONG max_token_length = kMaxTokenLength;
  int rv = mock_library->DetermineMaxTokenLength(&max_token_length);
  EXPECT_THAT(rv, IsOk());
  EXPECT_EQ(1337u, max_token_length);
}

TEST_F(HttpAuthSSPITest, DetermineMaxTokenLength_InvalidPackage) {
  auto mock_library = base::MakeRefCounted<MockSSPILibrary>(L"Foo");
  mock_library->ExpectQuerySecurityPackageInfo(SEC_E_SECPKG_NOT_FOUND, nullptr);
  ULONG max_token_length = kMaxTokenLength;
  int rv = mock_library->DetermineMaxTokenLength(&max_token_length);
  EXPECT_THAT(rv, IsError(ERR_UNSUPPORTED_AUTH_SCHEME));
  // |DetermineMaxTokenLength()| interface states that |max_token_length| should
  // not change on failure.
  EXPECT_EQ(100u, max_token_length);
}

TEST_F(HttpAuthSSPITest, ParseChallenge_FirstRound) {
  // The first round should just consist of an unadorned "Negotiate" header.
  auto mock_library = base::MakeRefCounted<MockSSPILibrary>(NEGOSSP_NAME);
  HttpAuthSSPI auth_sspi(mock_library, HttpAuth::AUTH_SCHEME_NEGOTIATE);
  HttpAuthChallengeTokenizer challenge("Negotiate");
  EXPECT_EQ(HttpAuth::AUTHORIZATION_RESULT_ACCEPT,
            auth_sspi.ParseChallenge(&challenge));
}

TEST_F(HttpAuthSSPITest, ParseChallenge_TwoRounds) {
  // The first round should just have "Negotiate", and the second round should
  // have a valid base64 token associated with it.
  auto mock_library = base::MakeRefCounted<MockSSPILibrary>(NEGOSSP_NAME);
  HttpAuthSSPI auth_sspi(mock_library, HttpAuth::AUTH_SCHEME_NEGOTIATE);
  HttpAuthChallengeTokenizer first_challenge("Negotiate");
  EXPECT_EQ(HttpAuth::AUTHORIZATION_RESULT_ACCEPT,
            auth_sspi.ParseChallenge(&first_challenge));

  // Generate an auth token and create another thing.
  std::string auth_token;
  EXPECT_EQ(OK, GenerateAuthToken(&auth_sspi, &auth_token));

  HttpAuthChallengeTokenizer second_challenge("Negotiate Zm9vYmFy");
  EXPECT_EQ(HttpAuth::AUTHORIZATION_RESULT_ACCEPT,
            auth_sspi.ParseChallenge(&second_challenge));
}

TEST_F(HttpAuthSSPITest, ParseChallenge_UnexpectedTokenFirstRound) {
  // If the first round challenge has an additional authentication token, it
  // should be treated as an invalid challenge from the server.
  auto mock_library = base::MakeRefCounted<MockSSPILibrary>(NEGOSSP_NAME);
  HttpAuthSSPI auth_sspi(mock_library, HttpAuth::AUTH_SCHEME_NEGOTIATE);
  HttpAuthChallengeTokenizer challenge("Negotiate Zm9vYmFy");
  EXPECT_EQ(HttpAuth::AUTHORIZATION_RESULT_INVALID,
            auth_sspi.ParseChallenge(&challenge));
}

TEST_F(HttpAuthSSPITest, ParseChallenge_MissingTokenSecondRound) {
  // If a later-round challenge is simply "Negotiate", it should be treated as
  // an authentication challenge rejection from the server or proxy.
  auto mock_library = base::MakeRefCounted<MockSSPILibrary>(NEGOSSP_NAME);
  HttpAuthSSPI auth_sspi(mock_library, HttpAuth::AUTH_SCHEME_NEGOTIATE);
  HttpAuthChallengeTokenizer first_challenge("Negotiate");
  EXPECT_EQ(HttpAuth::AUTHORIZATION_RESULT_ACCEPT,
            auth_sspi.ParseChallenge(&first_challenge));

  std::string auth_token;
  EXPECT_EQ(OK, GenerateAuthToken(&auth_sspi, &auth_token));
  HttpAuthChallengeTokenizer second_challenge("Negotiate");
  EXPECT_EQ(HttpAuth::AUTHORIZATION_RESULT_REJECT,
            auth_sspi.ParseChallenge(&second_challenge));
}

TEST_F(HttpAuthSSPITest, ParseChallenge_NonBase64EncodedToken) {
  // If a later-round challenge has an invalid base64 encoded token, it should
  // be treated as an invalid challenge.
  auto mock_library = base::MakeRefCounted<MockSSPILibrary>(NEGOSSP_NAME);
  HttpAuthSSPI auth_sspi(mock_library, HttpAuth::AUTH_SCHEME_NEGOTIATE);
  std::string first_challenge_text = "Negotiate";
  HttpAuthChallengeTokenizer first_challenge("Negotiate");
  EXPECT_EQ(HttpAuth::AUTHORIZATION_RESULT_ACCEPT,
            auth_sspi.ParseChallenge(&first_challenge));

  std::string auth_token;
  EXPECT_EQ(OK, GenerateAuthToken(&auth_sspi, &auth_token));
  HttpAuthChallengeTokenizer second_challenge("Negotiate =happyjoy=");
  EXPECT_EQ(HttpAuth::AUTHORIZATION_RESULT_INVALID,
            auth_sspi.ParseChallenge(&second_challenge));
}

// Runs through a full handshake against the MockSSPILibrary.
TEST_F(HttpAuthSSPITest, GenerateAuthToken_FullHandshake_AmbientCreds) {
  auto mock_library = base::MakeRefCounted<MockSSPILibrary>(NEGOSSP_NAME);
  HttpAuthSSPI auth_sspi(mock_library, HttpAuth::AUTH_SCHEME_NEGOTIATE);
  std::string first_challenge_text = "Negotiate";
  HttpAuthChallengeTokenizer first_challenge("Negotiate");
  ASSERT_EQ(HttpAuth::AUTHORIZATION_RESULT_ACCEPT,
            auth_sspi.ParseChallenge(&first_challenge));

  std::string auth_token;
  ASSERT_EQ(OK, GenerateAuthToken(&auth_sspi, &auth_token));
  EXPECT_EQ("Negotiate ", auth_token.substr(0, 10));

  std::string decoded_token;
  ASSERT_TRUE(base::Base64Decode(auth_token.substr(10), &decoded_token));

  // This token string indicates that HttpAuthSSPI correctly established the
  // security context using the default credentials.
  EXPECT_EQ("<Default>'s token #1 for HTTP/intranet.google.com", decoded_token);

  // The server token is arbitrary.
  HttpAuthChallengeTokenizer second_challenge("Negotiate UmVzcG9uc2U=");
  ASSERT_EQ(HttpAuth::AUTHORIZATION_RESULT_ACCEPT,
            auth_sspi.ParseChallenge(&second_challenge));

  ASSERT_EQ(OK, GenerateAuthToken(&auth_sspi, &auth_token));
  ASSERT_EQ("Negotiate ", auth_token.substr(0, 10));
  ASSERT_TRUE(base::Base64Decode(auth_token.substr(10), &decoded_token));
  EXPECT_EQ("<Default>'s token #2 for HTTP/intranet.google.com", decoded_token);
}

// Test NetLogs produced while going through a full Negotiate handshake.
TEST_F(HttpAuthSSPITest, GenerateAuthToken_FullHandshake_AmbientCreds_Logging) {
  RecordingNetLogObserver net_log_observer;
  NetLogWithSource net_log_with_source =
      NetLogWithSource::Make(NetLogSourceType::NONE);
  auto mock_library = base::MakeRefCounted<MockSSPILibrary>(NEGOSSP_NAME);
  HttpAuthSSPI auth_sspi(mock_library, HttpAuth::AUTH_SCHEME_NEGOTIATE);
  HttpAuthChallengeTokenizer first_challenge("Negotiate");
  ASSERT_EQ(HttpAuth::AUTHORIZATION_RESULT_ACCEPT,
            auth_sspi.ParseChallenge(&first_challenge));

  std::string auth_token;
  ASSERT_EQ(OK,
            GenerateAuthToken(&auth_sspi, &auth_token, net_log_with_source));

  // The token is the ASCII string "Response" in base64.
  HttpAuthChallengeTokenizer second_challenge("Negotiate UmVzcG9uc2U=");
  ASSERT_EQ(HttpAuth::AUTHORIZATION_RESULT_ACCEPT,
            auth_sspi.ParseChallenge(&second_challenge));
  ASSERT_EQ(OK,
            GenerateAuthToken(&auth_sspi, &auth_token, net_log_with_source));

  auto entries = net_log_observer.GetEntriesWithType(
      NetLogEventType::AUTH_LIBRARY_ACQUIRE_CREDS);
  ASSERT_EQ(2u, entries.size());  // BEGIN and END.
  auto expected = base::JSONReader::Read(R"(
    {
      "status": {
        "net_error": 0,
        "security_status": 0
       }
    }
  )",
                                         base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  EXPECT_EQ(expected, entries[1].params);

  entries = net_log_observer.GetEntriesWithType(
      NetLogEventType::AUTH_LIBRARY_INIT_SEC_CTX);
  ASSERT_EQ(4u, entries.size());

  expected = base::JSONReader::Read(R"(
    {
       "flags": {
          "delegated": false,
          "mutual": false,
          "value": "0x00000000"
       },
       "spn": "HTTP/intranet.google.com"
    }
  )",
                                    base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  EXPECT_EQ(expected, entries[0].params);

  expected = base::JSONReader::Read(R"(
    {
      "context": {
         "authority": "Dodgy Server",
         "flags": {
            "delegated": false,
            "mutual": false,
            "value": "0x00000000"
         },
         "mechanism": "Itsa me Kerberos!!",
         "open": true,
         "source": "\u003CDefault>",
         "target": "HTTP/intranet.google.com"
      },
      "status": {
         "net_error": 0,
         "security_status": 0
      }
    }
  )",
                                    base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  EXPECT_EQ(expected, entries[1].params);

  expected = base::JSONReader::Read(R"(
    {
      "context": {
        "authority": "Dodgy Server",
        "flags": {
           "delegated": false,
           "mutual": false,
           "value": "0x00000000"
        },
        "mechanism": "Itsa me Kerberos!!",
        "open": false,
        "source": "\u003CDefault>",
        "target": "HTTP/intranet.google.com"
      },
      "status": {
         "net_error": 0,
         "security_status": 0
      }
    }
  )",
                                    base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  EXPECT_EQ(expected, entries[3].params);
}

TEST_F(HttpAuthSSPITest, GenerateAuthToken_Negotiate_WithDelegation) {
  RecordingNetLogObserver net_log_observer;
  NetLogWithSource net_log_with_source =
      NetLogWithSource::Make(NetLogSourceType::NONE);
  auto mock_library = base::MakeRefCounted<MockSSPILibrary>(NEGOSSP_NAME);
  HttpAuthSSPI auth_sspi(mock_library, HttpAuth::AUTH_SCHEME_NEGOTIATE);
  auth_sspi.SetDelegation(HttpAuth::DelegationType::kUnconstrained);
  HttpAuthChallengeTokenizer first_challenge("Negotiate");
  ASSERT_EQ(HttpAuth::AUTHORIZATION_RESULT_ACCEPT,
            auth_sspi.ParseChallenge(&first_challenge));

  std::string auth_token;
  ASSERT_EQ(OK,
            GenerateAuthToken(&auth_sspi, &auth_token, net_log_with_source));

  auto entries = net_log_observer.GetEntriesWithType(
      NetLogEventType::AUTH_LIBRARY_INIT_SEC_CTX);
  ASSERT_GE(entries.size(), 1u);

  auto expected = base::JSONReader::Read(R"(
    {
       "flags": {
          "delegated": true,
          "mutual": true,
          "value": "0x00000003"
       },
       "spn": "HTTP/intranet.google.com"
    }
  )",
                                         base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  EXPECT_EQ(expected, entries[0].params);
}

TEST_F(HttpAuthSSPITest, GenerateAuthToken_NTLM_WithDelegation) {
  RecordingNetLogObserver net_log_observer;
  NetLogWithSource net_log_with_source =
      NetLogWithSource::Make(NetLogSourceType::NONE);
  auto mock_library = base::MakeRefCounted<MockSSPILibrary>(L"NTLM");
  HttpAuthSSPI auth_sspi(mock_library, HttpAuth::AUTH_SCHEME_NTLM);
  auth_sspi.SetDelegation(HttpAuth::DelegationType::kUnconstrained);
  HttpAuthChallengeTokenizer first_challenge("NTLM");
  ASSERT_EQ(HttpAuth::AUTHORIZATION_RESULT_ACCEPT,
            auth_sspi.ParseChallenge(&first_challenge));

  std::string auth_token;
  ASSERT_EQ(OK,
            GenerateAuthToken(&auth_sspi, &auth_token, net_log_with_source));

  auto entries = net_log_observer.GetEntriesWithType(
      NetLogEventType::AUTH_LIBRARY_INIT_SEC_CTX);
  ASSERT_GE(entries.size(), 1u);

  auto expected = base::JSONReader::Read(R"(
    {
       "flags": {
          "delegated": false,
          "mutual": false,
          "value": "0x00000000"
       },
       "spn": "HTTP/intranet.google.com"
    }
  )",
                                         base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  EXPECT_EQ(expected, entries[0].params);
}

TEST_F(HttpAuthSSPITest, GenerateAuthToken_DeletedBeforeCompletion) {
  auto mock_library = base::MakeRefCounted<MockSSPILibrary>(NEGOSSP_NAME);
  std::string auth_token;
  bool callback_run = false;
  {
    HttpAuthSSPI auth_sspi(mock_library, HttpAuth::AUTH_SCHEME_NEGOTIATE);
    HttpAuthChallengeTokenizer challenge("Negotiate");
    ASSERT_EQ(HttpAuth::AUTHORIZATION_RESULT_ACCEPT,
              auth_sspi.ParseChallenge(&challenge));
    EXPECT_THAT(
        auth_sspi.GenerateAuthToken(
            nullptr, "HTTP/intranet.google.com", std::string(), &auth_token,
            NetLogWithSource(),
            base::BindLambdaForTesting([&](int) { callback_run = true; })),
        IsError(ERR_IO_PENDING));
  }
  base::ThreadPoolInstance::Get()->FlushForTesting();
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_FALSE(callback_run);
}

// The error now round-trips through the completion callback rather than
// being returned directly, and must leave `auth_token` untouched.
TEST_F(HttpAuthSSPITest, GenerateAuthToken_FailureLeavesTokenUntouched) {
  auto mock_library = base::MakeRefCounted<MockSSPILibrary>(NEGOSSP_NAME);
  mock_library->ExpectQuerySecurityPackageInfo(SEC_E_SECPKG_NOT_FOUND, nullptr);
  HttpAuthSSPI auth_sspi(mock_library, HttpAuth::AUTH_SCHEME_NEGOTIATE);
  HttpAuthChallengeTokenizer challenge("Negotiate");
  ASSERT_EQ(HttpAuth::AUTHORIZATION_RESULT_ACCEPT,
            auth_sspi.ParseChallenge(&challenge));

  std::string auth_token = "untouched";
  EXPECT_THAT(GenerateAuthToken(&auth_sspi, &auth_token),
              IsError(ERR_UNSUPPORTED_AUTH_SCHEME));
  EXPECT_EQ("untouched", auth_token);
}

// A failed round leaves no security context, so the next challenge is parsed
// as a first round.
TEST_F(HttpAuthSSPITest, GenerateAuthToken_FailureResetsSecurityContext) {
  auto mock_library = base::MakeRefCounted<MockSSPILibrary>(NEGOSSP_NAME);
  HttpAuthSSPI auth_sspi(mock_library, HttpAuth::AUTH_SCHEME_NEGOTIATE);
  HttpAuthChallengeTokenizer first_challenge("Negotiate");
  ASSERT_EQ(HttpAuth::AUTHORIZATION_RESULT_ACCEPT,
            auth_sspi.ParseChallenge(&first_challenge));

  std::string auth_token;
  ASSERT_EQ(OK, GenerateAuthToken(&auth_sspi, &auth_token));
  // A later round without a server token fails before
  // InitializeSecurityContext() is called.
  EXPECT_THAT(GenerateAuthToken(&auth_sspi, &auth_token),
              IsError(ERR_UNEXPECTED));

  HttpAuthChallengeTokenizer next_challenge("Negotiate");
  EXPECT_EQ(HttpAuth::AUTHORIZATION_RESULT_ACCEPT,
            auth_sspi.ParseChallenge(&next_challenge));
}

// Fails against a synchronous implementation, which returns OK before the
// calling sequence can be observed.
TEST_F(HttpAuthSSPITest, GenerateAuthToken_DoesNotBlockCallingSequence) {
  auto mock_library = base::MakeRefCounted<BlockingSSPILibrary>(NEGOSSP_NAME);
  HttpAuthSSPI auth_sspi(mock_library, HttpAuth::AUTH_SCHEME_NEGOTIATE);
  HttpAuthChallengeTokenizer challenge("Negotiate");
  ASSERT_EQ(HttpAuth::AUTHORIZATION_RESULT_ACCEPT,
            auth_sspi.ParseChallenge(&challenge));

  std::string auth_token;
  TestCompletionCallback callback;
  ASSERT_THAT(auth_sspi.GenerateAuthToken(
                  nullptr, "HTTP/intranet.google.com", std::string(),
                  &auth_token, NetLogWithSource(), callback.callback()),
              IsError(ERR_IO_PENDING));

  mock_library->WaitUntilEntered();

  bool ran_while_sspi_blocked = false;
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&] {
        ran_while_sspi_blocked = true;
        run_loop.Quit();
      }));
  run_loop.Run();
  EXPECT_TRUE(ran_while_sspi_blocked);
  EXPECT_FALSE(callback.have_result());

  mock_library->Release();
  EXPECT_THAT(callback.WaitForResult(), IsOk());
  EXPECT_EQ("Negotiate ", auth_token.substr(0, 10));
}

TEST_F(HttpAuthSSPITest, NtlmHandlerGenerateAuthTokenIsAsync) {
  HttpAuthPreferences prefs;
  prefs.SetServerAllowlist("www.example.com");
  HttpAuthHandlerNTLM handler(base::MakeRefCounted<MockSSPILibrary>(L"NTLM"),
                              &prefs);
  HttpAuthChallengeTokenizer challenge("NTLM");
  ASSERT_TRUE(handler.InitFromChallenge(
      &challenge, HttpAuth::AUTH_SERVER, SSLInfo(), NetworkAnonymizationKey(),
      url::SchemeHostPort(GURL("http://www.example.com")), NetLogWithSource()));

  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  int rv = handler.GenerateAuthToken(nullptr, &request_info,
                                     callback.callback(), &token);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_THAT(callback.GetResult(rv), IsOk());
  EXPECT_TRUE(token.starts_with("NTLM "));
}

class HttpAuthSSPIURLRequestTest : public TestWithTaskEnvironment {};

// Drives a URLRequest through the network stack against a local server that
// answers 401 Negotiate. While that request is parked inside
// InitializeSecurityContext(), an unrelated request on the same context must
// still complete.
TEST_F(HttpAuthSSPIURLRequestTest, OtherRequestsProceedWhileTokenIsPending) {
  test_server::EmbeddedTestServer server;
  server.RegisterRequestHandler(base::BindRepeating(&HandleNegotiateRequest));
  ASSERT_TRUE(server.Start());

  auto library = base::MakeRefCounted<BlockingSSPILibrary>(NEGOSSP_NAME);
  HttpAuthPreferences prefs;
  prefs.SetServerAllowlist(server.host_port_pair().host());
  prefs.set_negotiate_disable_cname_lookup(true);
  auto negotiate_factory = std::make_unique<HttpAuthHandlerNegotiate::Factory>(
      HttpAuthMechanismFactory());
  negotiate_factory->set_library(library);
  auto auth_factory = std::make_unique<HttpAuthHandlerRegistryFactory>(&prefs);
  auth_factory->RegisterSchemeFactory(kNegotiateAuthScheme,
                                      std::move(negotiate_factory));
  auto builder = CreateTestURLRequestContextBuilder();
  builder->SetHttpAuthHandlerFactory(std::move(auth_factory));
  auto context = builder->Build();

  base::RunLoop entered_loop;
  library->SetOnEntered(
      base::BindPostTaskToCurrentDefault(entered_loop.QuitClosure()));

  base::RunLoop negotiate_loop;
  TestDelegate negotiate_delegate;
  negotiate_delegate.set_on_complete(negotiate_loop.QuitClosure());
  std::unique_ptr<URLRequest> negotiate_request = context->CreateRequest(
      server.GetURL("/negotiate"), DEFAULT_PRIORITY, &negotiate_delegate,
      TRAFFIC_ANNOTATION_FOR_TESTS, handles::kInvalidNetworkHandle);
  negotiate_request->Start();
  entered_loop.Run();

  TestDelegate other_delegate;
  std::unique_ptr<URLRequest> other_request = context->CreateRequest(
      server.GetURL("/other"), DEFAULT_PRIORITY, &other_delegate,
      TRAFFIC_ANNOTATION_FOR_TESTS, handles::kInvalidNetworkHandle);
  other_request->Start();
  other_delegate.RunUntilComplete();
  EXPECT_THAT(other_delegate.request_status(), IsOk());
  EXPECT_EQ(HTTP_OK, other_request->GetResponseCode());
  EXPECT_FALSE(negotiate_delegate.response_completed());

  library->Release();
  negotiate_loop.Run();
  EXPECT_THAT(negotiate_delegate.request_status(), IsOk());
  EXPECT_EQ(HTTP_OK, negotiate_request->GetResponseCode());
  EXPECT_TRUE(library->WasReleased());
}

}  // namespace net
