// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/registration_fetcher.h"

#include <memory>
#include <string>
#include <utility>

#include "base/base64url.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "components/unexportable_keys/mock_unexportable_key_service.h"
#include "components/unexportable_keys/unexportable_key_service.h"
#include "components/unexportable_keys/unexportable_key_service_impl.h"
#include "components/unexportable_keys/unexportable_key_task_manager.h"
#include "crypto/scoped_fake_unexportable_key_provider.h"
#include "net/base/features.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/cookie_access_result.h"
#include "net/cookies/cookie_store.h"
#include "net/cookies/cookie_store_test_callbacks.h"
#include "net/cookies/parsed_cookie.h"
#include "net/device_bound_sessions/mock_session_service.h"
#include "net/device_bound_sessions/proto/storage.pb.h"
#include "net/device_bound_sessions/registration_request_param.h"
#include "net/device_bound_sessions/session_params.h"
#include "net/device_bound_sessions/session_service.h"
#include "net/device_bound_sessions/test_support.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/log/test_net_log.h"
#include "net/socket/socket_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/test_with_task_environment.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net::device_bound_sessions {

namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::Property;
using ::testing::Return;
using ::testing::WithArg;

constexpr char kBasicValidJson[] =
    R"({
  "session_identifier": "session_id",
  "refresh_url": "/refresh",
  "scope": {
    "origin": "https://a.test",
    "include_site": true,
    "scope_specification" : [
      {
        "type": "include",
        "domain": "trusted.a.test",
        "path": "/only_trusted_path"
      }
    ]
  },
  "credentials": [{
    "type": "cookie",
    "name": "auth_cookie",
    "attributes": "Domain=a.test; Path=/; Secure; SameSite=None"
  }]
})";

constexpr char kSessionIdentifier[] = "session_id";
constexpr char kRedirectPath[] = "/redirect";
constexpr char kChallenge[] = "test_challenge";
constexpr unexportable_keys::BackgroundTaskPriority kTaskPriority =
    unexportable_keys::BackgroundTaskPriority::kBestEffort;
constexpr char kSessionChallengeHeaderName[] = "Secure-Session-Challenge";
constexpr char kSessionResponseHeaderName[] = "Secure-Session-Response";
constexpr char kSessionIdHeaderName[] = "Sec-Secure-Session-Id";

std::vector<crypto::SignatureVerifier::SignatureAlgorithm> CreateAlgArray() {
  return {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256,
          crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256};
}

struct InvokeCallbackArgumentAction {};

class TestRegistrationCallback {
 public:
  TestRegistrationCallback() = default;

  RegistrationFetcher::RegistrationCompleteCallback callback() {
    return base::BindOnce(&TestRegistrationCallback::OnRegistrationComplete,
                          base::Unretained(this));
  }

  void WaitForCall() {
    if (outcome_.has_value()) {
      return;
    }

    base::RunLoop run_loop;

    waiting_ = true;
    closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  const RegistrationResult& outcome() {
    EXPECT_TRUE(outcome_.has_value());
    return *outcome_;
  }

 private:
  void OnRegistrationComplete(RegistrationFetcher* fetcher,
                              RegistrationResult params) {
    EXPECT_FALSE(outcome_.has_value());

    outcome_ = std::move(params);

    if (waiting_) {
      waiting_ = false;
      std::move(closure_).Run();
    }
  }

  std::optional<RegistrationResult> outcome_ = std::nullopt;

  bool waiting_ = false;
  base::OnceClosure closure_;
};

class RegistrationTest : public TestWithTaskEnvironment {
 protected:
  RegistrationTest()
      : server_(test_server::EmbeddedTestServer::TYPE_HTTPS),
        host_resolver_(
            base::MakeRefCounted<net::RuleBasedHostResolverProc>(nullptr)) {
    host_resolver_->AddRule("*", "127.0.0.1");
    server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);

    auto context_builder = CreateTestURLRequestContextBuilder();
    auto network_delegate = std::make_unique<TestNetworkDelegate>();
    network_delegate_ = network_delegate.get();
    context_builder->set_network_delegate(std::move(network_delegate));
    context_ = context_builder->Build();
  }

  void TearDown() override {
    // Reset the `network_delegate_` to avoid a dangling pointer.
    network_delegate_ = nullptr;
  }

  unexportable_keys::UnexportableKeyService& unexportable_key_service() {
    return unexportable_key_service_;
  }

  SessionServiceMock& session_service() { return session_service_; }

  TestNetworkDelegate* network_delegate() { return network_delegate_; }

  // In order to get HTTPS with a registered domain, use one of the sites
  // under [test_names] in net/data/ssl/scripts/ee.cnf. We arbitrarily
  // choose *.a.test.
  GURL GetBaseURL() { return server_.GetURL("a.test", "/"); }

  RegistrationRequestParam GetBasicParam(
      std::optional<GURL> url = std::nullopt) {
    if (!url) {
      url = GetBaseURL();
    }

    return RegistrationRequestParam::CreateForTesting(
        *url, /*session_identifier=*/std::nullopt, std::string(kChallenge));
  }

  unexportable_keys::UnexportableKeyId CreateKey() {
    base::test::TestFuture<
        unexportable_keys::ServiceErrorOr<unexportable_keys::UnexportableKeyId>>
        future;
    unexportable_key_service_.GenerateSigningKeySlowlyAsync(
        CreateAlgArray(), kTaskPriority, future.GetCallback());
    return *future.Take();
  }

  RegistrationResult FetchWithFederatedKey(
      RegistrationRequestParam param,
      unexportable_keys::UnexportableKeyId key,
      const GURL& provider_url) {
    base::test::TestFuture<RegistrationFetcher*, RegistrationResult> future;
    std::unique_ptr<RegistrationFetcher> fetcher =
        RegistrationFetcher::CreateFetcher(
            param, session_service(), unexportable_key_service(),
            context_.get(),
            IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
            /*net_log_source=*/std::nullopt,
            /*original_request_initiator=*/std::nullopt);
    fetcher->StartFetchWithFederatedKey(param, key, provider_url,
                                        future.GetCallback());
    return std::get<1>(future.Take());
  }

  std::unique_ptr<Session> CreateTestSession(std::string session_identifier) {
    SessionParams::Scope scope;
    scope.origin = url::Origin::Create(GetBaseURL()).Serialize();
    auto session_or_error = Session::CreateIfValid(
        SessionParams(std::move(session_identifier), GetBaseURL(),
                      GetBaseURL().spec(), std::move(scope),
                      /*creds=*/{}, unexportable_keys::UnexportableKeyId(),
                      /*allowed_refresh_initiators=*/{}));
    return std::move(*session_or_error);
  }

  test_server::EmbeddedTestServer server_;
  raw_ptr<TestNetworkDelegate> network_delegate_;
  std::unique_ptr<URLRequestContext> context_;

  const url::Origin kOrigin = url::Origin::Create(GURL("https://origin/"));
  unexportable_keys::UnexportableKeyTaskManager task_manager_;
  unexportable_keys::UnexportableKeyServiceImpl unexportable_key_service_{
      task_manager_, crypto::UnexportableKeyProvider::Config()};
  SessionServiceMock session_service_;
  scoped_refptr<net::RuleBasedHostResolverProc> host_resolver_;
};

std::unique_ptr<test_server::HttpResponse> ReturnResponse(
    HttpStatusCode code,
    std::string_view response_text,
    const test_server::HttpRequest& request) {
  auto response = std::make_unique<test_server::BasicHttpResponse>();
  response->set_code(code);
  response->set_content_type("application/json");
  response->set_content(response_text);
  return response;
}

std::unique_ptr<test_server::HttpResponse> ReturnUnauthorized(
    const test_server::HttpRequest& request) {
  auto response = std::make_unique<test_server::BasicHttpResponse>();
  response->set_code(HTTP_UNAUTHORIZED);
  response->AddCustomHeader(kSessionChallengeHeaderName, R"("challenge")");
  return response;
}

std::unique_ptr<test_server::HttpResponse> ReturnForbidden(
    const test_server::HttpRequest& request) {
  auto response = std::make_unique<test_server::BasicHttpResponse>();
  response->set_code(HTTP_FORBIDDEN);
  response->AddCustomHeader(kSessionChallengeHeaderName, R"("challenge")");
  return response;
}

std::unique_ptr<test_server::HttpResponse> ReturnTextResponse(
    const test_server::HttpRequest& request) {
  auto response = std::make_unique<test_server::BasicHttpResponse>();
  response->set_code(HTTP_OK);
  response->set_content_type("text/plain");
  response->set_content("some content");
  return response;
}

std::unique_ptr<test_server::HttpResponse> ReturnInvalidResponse(
    const test_server::HttpRequest& request) {
  return std::make_unique<test_server::RawHttpResponse>(
      "", "Not a valid HTTP response.");
}

std::unique_ptr<test_server::HttpResponse> ReturnForHostAndPath(
    std::string_view host,
    std::string_view path,
    test_server::EmbeddedTestServer::HandleRequestCallback callback,
    const test_server::HttpRequest& request) {
  // `base_url` resolved to 127.0.0.1, so get the host and port
  // from the Host header.
  auto it = request.headers.find("host");
  if (it == request.headers.end()) {
    return nullptr;
  }
  if (it->second.find(host) == std::string::npos) {
    return nullptr;
  }
  if (request.relative_url != path) {
    return nullptr;
  }

  return callback.Run(request);
}

// The .well-known will usually need to contain a port assigned
// dynamically by `EmbeddedTestServer`. We work around that by getting
// the port from `request.base_url` and replacing "$1" with the required
// port in the .well-known contents.
std::unique_ptr<test_server::HttpResponse> ReturnWellKnown(
    const std::string& contents,
    const test_server::HttpRequest& request) {
  auto response = std::make_unique<test_server::BasicHttpResponse>();
  response->set_content_type("application/json");
  response->set_code(HTTP_OK);
  response->set_content(base::ReplaceStringPlaceholders(
      contents, {request.base_url.GetPort()}, /*offsets=*/nullptr));
  return response;
}

class UnauthorizedThenSuccessResponseContainer {
 public:
  explicit UnauthorizedThenSuccessResponseContainer(
      int unauthorize_response_times)
      : error_response_times(unauthorize_response_times) {}

  std::unique_ptr<test_server::HttpResponse> Return(
      const test_server::HttpRequest& request) {
    if (run_times++ < error_response_times) {
      return ReturnUnauthorized(request);
    }
    return ReturnResponse(HTTP_OK, kBasicValidJson, request);
  }

 private:
  int run_times = 0;
  int error_response_times = 0;
};

class ForbiddenThenSuccessResponseContainer {
 public:
  explicit ForbiddenThenSuccessResponseContainer(int forbidden_response_times)
      : error_response_times(forbidden_response_times) {}

  std::unique_ptr<test_server::HttpResponse> Return(
      const test_server::HttpRequest& request) {
    if (run_times++ < error_response_times) {
      return ReturnForbidden(request);
    }
    return ReturnResponse(HTTP_OK, kBasicValidJson, request);
  }

 private:
  int run_times = 0;
  int error_response_times = 0;
};

MATCHER_P3(EqualsInclusionRule, rule_type, rule_host, rule_path, "") {
  return testing::ExplainMatchResult(
      AllOf(
          Property("rule_type", &proto::UrlRule::rule_type, Eq(rule_type)),
          Property("host_pattern", &proto::UrlRule::host_pattern,
                   Eq(rule_host)),
          Property("path_prefix", &proto::UrlRule::path_prefix, Eq(rule_path))),
      arg, result_listener);
}

MATCHER_P2(EqualsCredential, name, attributes, "") {
  ParsedCookie cookie(std::string(name) + "=value;" + attributes);
  EXPECT_TRUE(cookie.IsValid());

  proto::CookieSameSite expected_same_site;
  switch (cookie.SameSite().first) {
    case CookieSameSite::UNSPECIFIED:
      expected_same_site = proto::CookieSameSite::COOKIE_SAME_SITE_UNSPECIFIED;
      break;
    case CookieSameSite::NO_RESTRICTION:
      expected_same_site = proto::CookieSameSite::NO_RESTRICTION;
      break;
    case CookieSameSite::LAX_MODE:
      expected_same_site = proto::CookieSameSite::LAX_MODE;
      break;
    case CookieSameSite::STRICT_MODE:
      expected_same_site = proto::CookieSameSite::STRICT_MODE;
      break;
  }

  return testing::ExplainMatchResult(
      AllOf(Property("name", &proto::CookieCraving::name, Eq(name)),
            Property("domain", &proto::CookieCraving::domain,
                     Eq(cookie.Domain())),
            Property("path", &proto::CookieCraving::path, Eq(cookie.Path())),
            Property("secure", &proto::CookieCraving::secure,
                     Eq(cookie.IsSecure())),
            Property("httponly", &proto::CookieCraving::httponly,
                     Eq(cookie.IsHttpOnly())),
            Property("same_site", &proto::CookieCraving::same_site,
                     Eq(expected_same_site))),
      arg, result_listener);
}

std::optional<std::string> GetRequestChallenge(
    const test_server::HttpRequest& request) {
  auto resp_iter = request.headers.find(kSessionResponseHeaderName);
  if (resp_iter == request.headers.end()) {
    return std::nullopt;
  }
  const std::string& jwt = resp_iter->second;
  std::vector<std::string> jwt_sections =
      base::SplitString(jwt, ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (jwt_sections.size() != 3) {
    return std::nullopt;
  }
  std::string payload;
  if (!base::Base64UrlDecode(jwt_sections[1],
                             base::Base64UrlDecodePolicy::DISALLOW_PADDING,
                             &payload)) {
    return std::nullopt;
  }
  const std::optional<base::Value::Dict> payload_json =
      base::JSONReader::ReadDict(payload, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!payload_json.has_value()) {
    return std::nullopt;
  }
  const std::string* challenge = payload_json->FindString("jti");
  if (!challenge) {
    return std::nullopt;
  }

  return *challenge;
}

TEST_F(RegistrationTest, BasicSuccess) {
  base::HistogramTester histogram_tester;
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(
      base::BindRepeating([](const test_server::HttpRequest& request) {
        auto resp_iter = request.headers.find(kSessionResponseHeaderName);
        EXPECT_TRUE(resp_iter != request.headers.end());
        if (resp_iter != request.headers.end()) {
          EXPECT_TRUE(VerifyEs256Jwt(resp_iter->second));
        }
        return ReturnResponse(HTTP_OK, kBasicValidJson, request);
      }));
  ASSERT_TRUE(server_.Start());

  RecordingNetLogObserver net_log_observer;
  TestRegistrationCallback callback;

  auto param = GetBasicParam();
  std::unique_ptr<RegistrationFetcher> fetcher =
      RegistrationFetcher::CreateFetcher(
          param, session_service(), unexportable_key_service(), context_.get(),
          IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
          /*net_log_source=*/std::nullopt,
          /*original_request_initiator=*/std::nullopt);
  fetcher->StartCreateTokenAndFetch(param, CreateAlgArray(),
                                    callback.callback());
  callback.WaitForCall();
  const Session& session = callback.outcome().SessionForTesting();
  proto::Session session_proto = session.ToProto();
  EXPECT_TRUE(session_proto.session_inclusion_rules().do_include_site());
  EXPECT_THAT(
      session_proto.session_inclusion_rules().url_rules(),
      ElementsAre(
          EqualsInclusionRule(proto::RuleType::INCLUDE, "trusted.a.test",
                              "/only_trusted_path"),
          EqualsInclusionRule(proto::RuleType::EXCLUDE, "a.test", "/refresh")));
  EXPECT_THAT(
      session_proto.cookie_cravings(),
      ElementsAre(EqualsCredential(
          "auth_cookie", "Domain=.a.test; Path=/; Secure; SameSite=None")));
  histogram_tester.ExpectUniqueSample(
      "Net.DeviceBoundSessions.Registration.Network.Result", HTTP_OK, 1);
}

TEST_F(RegistrationTest, NoScopeJson) {
  constexpr char kTestingJson[] =
      R"({
  "session_identifier": "session_id",
  "credentials": [{
    "type": "cookie",
    "name": "auth_cookie",
    "attributes": "Domain=example.com; Path=/; Secure; SameSite=None"
  }]
})";
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kTestingJson));
  ASSERT_TRUE(server_.Start());

  RecordingNetLogObserver net_log_observer;
  TestRegistrationCallback callback;

  auto param = GetBasicParam();
  std::unique_ptr<RegistrationFetcher> fetcher =
      RegistrationFetcher::CreateFetcher(
          param, session_service(), unexportable_key_service(), context_.get(),
          IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
          /*net_log_source=*/std::nullopt,
          /*original_request_initiator=*/std::nullopt);
  fetcher->StartCreateTokenAndFetch(param, CreateAlgArray(),
                                    callback.callback());
  callback.WaitForCall();
  EXPECT_EQ(callback.outcome().SessionErrorForTesting().type,
            SessionError::kMissingScope);
}

TEST_F(RegistrationTest, NoSessionIdJson) {
  constexpr char kTestingJson[] =
      R"({
  "scope": {
    "include_site": true
  },
  "credentials": [{
    "type": "cookie",
    "name": "auth_cookie",
    "attributes": "Domain=example.com; Path=/; Secure; SameSite=None"
  }]
})";
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kTestingJson));
  ASSERT_TRUE(server_.Start());

  RecordingNetLogObserver net_log_observer;
  TestRegistrationCallback callback;

  auto param = GetBasicParam();
  std::unique_ptr<RegistrationFetcher> fetcher =
      RegistrationFetcher::CreateFetcher(
          param, session_service(), unexportable_key_service(), context_.get(),
          IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
          /*net_log_source=*/std::nullopt,
          /*original_request_initiator=*/std::nullopt);
  fetcher->StartCreateTokenAndFetch(param, CreateAlgArray(),
                                    callback.callback());
  callback.WaitForCall();
  EXPECT_EQ(callback.outcome().SessionErrorForTesting().type,
            SessionError::kInvalidSessionId);
}

TEST_F(RegistrationTest, EmptySessionIdJson) {
  constexpr char kTestingJson[] =
      R"({
  "session_identifier": "",
  "scope": {
    "include_site": true
  },
  "credentials": [{
    "type": "cookie",
    "name": "auth_cookie",
    "attributes": "Domain=example.com; Path=/; Secure; SameSite=None"
  }]
})";
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kTestingJson));
  ASSERT_TRUE(server_.Start());

  RecordingNetLogObserver net_log_observer;
  TestRegistrationCallback callback;

  auto param = GetBasicParam();
  std::unique_ptr<RegistrationFetcher> fetcher =
      RegistrationFetcher::CreateFetcher(
          param, session_service(), unexportable_key_service(), context_.get(),
          IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
          /*net_log_source=*/std::nullopt,
          /*original_request_initiator=*/std::nullopt);
  fetcher->StartCreateTokenAndFetch(param, CreateAlgArray(),
                                    callback.callback());
  callback.WaitForCall();
  EXPECT_EQ(callback.outcome().SessionErrorForTesting().type,
            SessionError::kInvalidSessionId);
}

TEST_F(RegistrationTest, SpecificationNotDictJson) {
  constexpr char kTestingJson[] =
      R"({
  "session_identifier": "session_id",
  "scope": {
    "include_site": true,
    "scope_specification" : [
      "type", "domain", "path"
    ]
  },
  "credentials": [{
    "type": "cookie",
    "name": "auth_cookie",
    "attributes": "Domain=example.com; Path=/; Secure; SameSite=None"
  }]
})";

  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kTestingJson));
  ASSERT_TRUE(server_.Start());

  RecordingNetLogObserver net_log_observer;
  TestRegistrationCallback callback;

  auto param = GetBasicParam();
  std::unique_ptr<RegistrationFetcher> fetcher =
      RegistrationFetcher::CreateFetcher(
          param, session_service(), unexportable_key_service(), context_.get(),
          IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
          /*net_log_source=*/std::nullopt,
          /*original_request_initiator=*/std::nullopt);
  fetcher->StartCreateTokenAndFetch(param, CreateAlgArray(),
                                    callback.callback());
  callback.WaitForCall();
  EXPECT_EQ(callback.outcome().SessionErrorForTesting().type,
            SessionError::kInvalidScopeSpecification);
}

TEST_F(RegistrationTest, MissingPathDefaults) {
  constexpr char kTestingJson[] =
      R"({
  "session_identifier": "session_id",
  "refresh_url": "/refresh",
  "scope": {
    "include_site": true,
    "scope_specification" : [
      {
        "type": "include",
        "domain": "trusted.a.test"
      },
      {
        "type": "exclude",
        "domain": "new.a.test",
        "path": "/only_trusted_path"
      }
    ]
  },
  "credentials": [{
    "type": "cookie",
    "name": "other_cookie",
    "attributes": "Domain=a.test; Path=/; Secure; SameSite=None"
  }]
})";

  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kTestingJson));
  ASSERT_TRUE(server_.Start());

  RecordingNetLogObserver net_log_observer;
  TestRegistrationCallback callback;

  auto param = GetBasicParam();
  std::unique_ptr<RegistrationFetcher> fetcher =
      RegistrationFetcher::CreateFetcher(
          param, session_service(), unexportable_key_service(), context_.get(),
          IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
          /*net_log_source=*/std::nullopt,
          /*original_request_initiator=*/std::nullopt);
  fetcher->StartCreateTokenAndFetch(param, CreateAlgArray(),
                                    callback.callback());
  callback.WaitForCall();
  const auto& session = callback.outcome().SessionForTesting();
  proto::Session session_proto = session.ToProto();
  EXPECT_THAT(
      session_proto.session_inclusion_rules().url_rules(),
      ElementsAre(
          EqualsInclusionRule(proto::RuleType::INCLUDE, "trusted.a.test", "/"),
          EqualsInclusionRule(proto::RuleType::EXCLUDE, "new.a.test",
                              "/only_trusted_path"),
          EqualsInclusionRule(proto::RuleType::EXCLUDE, "a.test", "/refresh")));
}

TEST_F(RegistrationTest, MissingDomainDefaults) {
  constexpr char kTestingJson[] =
      R"({
  "session_identifier": "session_id",
  "refresh_url": "/refresh",
  "scope": {
    "include_site": true,
    "scope_specification" : [
      {
        "type": "include",
        "path": "/included"
      },
      {
        "type": "exclude",
        "domain": "new.a.test",
        "path": "/only_trusted_path"
      }
    ]
  },
  "credentials": [{
    "type": "cookie",
    "name": "other_cookie",
    "attributes": "Domain=a.test; Path=/; Secure; SameSite=None"
  }]
})";

  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kTestingJson));
  ASSERT_TRUE(server_.Start());

  RecordingNetLogObserver net_log_observer;
  TestRegistrationCallback callback;

  auto param = GetBasicParam();
  std::unique_ptr<RegistrationFetcher> fetcher =
      RegistrationFetcher::CreateFetcher(
          param, session_service(), unexportable_key_service(), context_.get(),
          IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
          /*net_log_source=*/std::nullopt,
          /*original_request_initiator=*/std::nullopt);
  fetcher->StartCreateTokenAndFetch(param, CreateAlgArray(),
                                    callback.callback());
  callback.WaitForCall();
  const auto& session = callback.outcome().SessionForTesting();
  proto::Session session_proto = session.ToProto();
  EXPECT_THAT(
      session_proto.session_inclusion_rules().url_rules(),
      ElementsAre(
          EqualsInclusionRule(proto::RuleType::INCLUDE, "*", "/included"),
          EqualsInclusionRule(proto::RuleType::EXCLUDE, "new.a.test",
                              "/only_trusted_path"),
          EqualsInclusionRule(proto::RuleType::EXCLUDE, "a.test", "/refresh")));
}

TEST_F(RegistrationTest, MissingRefreshUrlDefault) {
  constexpr char kTestingJson[] =
      R"({
  "session_identifier": "session_id",
  "scope": {
    "include_site": true,
    "scope_specification" : [
      {
        "type": "include",
        "domain": "trusted.a.test"
      },
      {
        "type": "exclude",
        "domain": "new.a.test",
        "path": "/only_trusted_path"
      }
    ]
  },
  "credentials": [{
    "type": "cookie",
    "name": "other_cookie",
    "attributes": "Domain=a.test; Path=/; Secure; SameSite=None"
  }]
})";

  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kTestingJson));
  ASSERT_TRUE(server_.Start());

  RecordingNetLogObserver net_log_observer;
  TestRegistrationCallback callback;

  auto param = GetBasicParam();
  std::unique_ptr<RegistrationFetcher> fetcher =
      RegistrationFetcher::CreateFetcher(
          param, session_service(), unexportable_key_service(), context_.get(),
          IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
          /*net_log_source=*/std::nullopt,
          /*original_request_initiator=*/std::nullopt);
  fetcher->StartCreateTokenAndFetch(param, CreateAlgArray(),
                                    callback.callback());
  callback.WaitForCall();
  const auto& session = callback.outcome().SessionForTesting();
  EXPECT_EQ(session.refresh_url(), GetBaseURL());
}

TEST_F(RegistrationTest, OneSpecTypeInvalid) {
  constexpr char kTestingJson[] =
      R"({
  "session_identifier": "session_id",
  "scope": {
    "include_site": true,
    "scope_specification" : [
      {
        "type": "invalid",
        "domain": "trusted.example.com",
        "path": "/only_trusted_path"
      },
      {
        "type": "exclude",
        "domain": "new.example.com",
        "path": "/only_trusted_path"
      }
    ]
  },
  "credentials": [{
    "type": "cookie",
    "name": "auth_cookie",
    "attributes": "Domain=example.com; Path=/; Secure; SameSite=None"
  }]
})";

  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kTestingJson));
  ASSERT_TRUE(server_.Start());

  RecordingNetLogObserver net_log_observer;
  TestRegistrationCallback callback;

  auto param = GetBasicParam();
  std::unique_ptr<RegistrationFetcher> fetcher =
      RegistrationFetcher::CreateFetcher(
          param, session_service(), unexportable_key_service(), context_.get(),
          IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
          /*net_log_source=*/std::nullopt,
          /*original_request_initiator=*/std::nullopt);
  fetcher->StartCreateTokenAndFetch(param, CreateAlgArray(),
                                    callback.callback());
  callback.WaitForCall();
  EXPECT_EQ(callback.outcome().SessionErrorForTesting().type,
            SessionError::kInvalidScopeSpecificationType);
}

TEST_F(RegistrationTest, InvalidTypeSpecList) {
  constexpr char kTestingJson[] =
      R"({
  "session_identifier": "session_id",
  "refresh_url": "/refresh",
  "scope": {
    "include_site": true,
    "scope_specification" : "missing"
  },
  "credentials": [{
    "type": "cookie",
    "name": "auth_cookie",
    "attributes": "Domain=a.test; Path=/; Secure; SameSite=None"
  }]
})";

  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kTestingJson));
  ASSERT_TRUE(server_.Start());

  RecordingNetLogObserver net_log_observer;
  TestRegistrationCallback callback;

  auto param = GetBasicParam();
  std::unique_ptr<RegistrationFetcher> fetcher =
      RegistrationFetcher::CreateFetcher(
          param, session_service(), unexportable_key_service(), context_.get(),
          IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
          /*net_log_source=*/std::nullopt,
          /*original_request_initiator=*/std::nullopt);
  fetcher->StartCreateTokenAndFetch(param, CreateAlgArray(),
                                    callback.callback());
  callback.WaitForCall();
  const auto& session = callback.outcome().SessionForTesting();
  proto::Session session_proto = session.ToProto();
  EXPECT_TRUE(session_proto.session_inclusion_rules().do_include_site());
  EXPECT_THAT(session_proto.session_inclusion_rules().url_rules(),
              ElementsAre(EqualsInclusionRule(proto::RuleType::EXCLUDE,
                                              "a.test", "/refresh")));
}

TEST_F(RegistrationTest, TypeIsNotCookie) {
  constexpr char kTestingJson[] =
      R"({
  "session_identifier": "session_id",
  "scope": {
    "include_site": true
  },
  "credentials": [{
    "type": "sync auth",
    "name": "auth_cookie",
    "attributes": "Domain=example.com; Path=/; Secure; SameSite=None"
  }]
})";

  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kTestingJson));
  ASSERT_TRUE(server_.Start());

  RecordingNetLogObserver net_log_observer;
  TestRegistrationCallback callback;

  auto param = GetBasicParam();
  std::unique_ptr<RegistrationFetcher> fetcher =
      RegistrationFetcher::CreateFetcher(
          param, session_service(), unexportable_key_service(), context_.get(),
          IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
          /*net_log_source=*/std::nullopt,
          /*original_request_initiator=*/std::nullopt);
  fetcher->StartCreateTokenAndFetch(param, CreateAlgArray(),
                                    callback.callback());
  callback.WaitForCall();
  EXPECT_EQ(callback.outcome().SessionErrorForTesting().type,
            SessionError::kInvalidCredentialsType);
}

TEST_F(RegistrationTest, TwoTypesCookie_NotCookie) {
  constexpr char kTestingJson[] =
      R"({
  "session_identifier": "session_id",
  "scope": {
    "include_site": true
  },
  "credentials": [
    {
      "type": "cookie",
      "name": "auth_cookie",
      "attributes": "Domain=example.com; Path=/; Secure; SameSite=None"
    },
    {
      "type": "sync auth",
      "name": "auth_cookie",
      "attributes": "Domain=example.com; Path=/; Secure; SameSite=None"
    }
  ]
})";

  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kTestingJson));
  ASSERT_TRUE(server_.Start());

  RecordingNetLogObserver net_log_observer;
  TestRegistrationCallback callback;

  auto param = GetBasicParam();
  std::unique_ptr<RegistrationFetcher> fetcher =
      RegistrationFetcher::CreateFetcher(
          param, session_service(), unexportable_key_service(), context_.get(),
          IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
          /*net_log_source=*/std::nullopt,
          /*original_request_initiator=*/std::nullopt);
  fetcher->StartCreateTokenAndFetch(param, CreateAlgArray(),
                                    callback.callback());
  callback.WaitForCall();
  EXPECT_EQ(callback.outcome().SessionErrorForTesting().type,
            SessionError::kInvalidCredentialsType);
}

TEST_F(RegistrationTest, TwoTypesNotCookie_Cookie) {
  constexpr char kTestingJson[] =
      R"({
  "session_identifier": "session_id",
  "scope": {
    "include_site": true
  },
  "credentials": [
    {
      "type": "sync auth",
      "name": "auth_cookie",
      "attributes": "Domain=example.com; Path=/; Secure; SameSite=None"
    },
    {
      "type": "cookie",
      "name": "auth_cookie",
      "attributes": "Domain=example.com; Path=/; Secure; SameSite=None"
    }
  ]
})";

  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kTestingJson));
  ASSERT_TRUE(server_.Start());

  RecordingNetLogObserver net_log_observer;
  TestRegistrationCallback callback;

  auto param = GetBasicParam();
  std::unique_ptr<RegistrationFetcher> fetcher =
      RegistrationFetcher::CreateFetcher(
          param, session_service(), unexportable_key_service(), context_.get(),
          IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
          /*net_log_source=*/std::nullopt,
          /*original_request_initiator=*/std::nullopt);
  fetcher->StartCreateTokenAndFetch(param, CreateAlgArray(),
                                    callback.callback());
  callback.WaitForCall();
  EXPECT_EQ(callback.outcome().SessionErrorForTesting().type,
            SessionError::kInvalidCredentialsType);
}

TEST_F(RegistrationTest, CredEntryWithoutDict) {
  constexpr char kTestingJson[] =
      R"({
  "session_identifier": "session_id",
  "scope": {
    "include_site": true
  },
  "credentials": [{
    "type": "cookie",
    "name": "auth_cookie",
    "attributes": "Domain=example.com; Path=/; Secure; SameSite=None"
  },
  "test"]
})";

  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kTestingJson));
  ASSERT_TRUE(server_.Start());

  RecordingNetLogObserver net_log_observer;
  TestRegistrationCallback callback;

  auto param = GetBasicParam();
  std::unique_ptr<RegistrationFetcher> fetcher =
      RegistrationFetcher::CreateFetcher(
          param, session_service(), unexportable_key_service(), context_.get(),
          IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
          /*net_log_source=*/std::nullopt,
          /*original_request_initiator=*/std::nullopt);
  fetcher->StartCreateTokenAndFetch(param, CreateAlgArray(),
                                    callback.callback());
  callback.WaitForCall();
  EXPECT_EQ(callback.outcome().SessionErrorForTesting().type,
            SessionError::kInvalidCredentialsConfig);
}

TEST_F(RegistrationTest, CredEntryWithoutAttributes) {
  constexpr char kTestingJson[] =
      R"({
  "session_identifier": "session_id",
  "scope": {
    "include_site": true
  },
  "credentials": [{
    "type": "cookie",
    "name": "auth_cookie"
  }]
})";

  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kTestingJson));
  ASSERT_TRUE(server_.Start());

  // Since the cookie has no attributes, it's SameSite Lax. We set
  // a same-origin initiator to avoid registration being rejected,
  auto origin = url::Origin::Create(GetBaseURL());
  auto isolation_info = net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kOther, origin, origin,
      net::SiteForCookies::FromOrigin(origin));

  RecordingNetLogObserver net_log_observer;
  TestRegistrationCallback callback;

  auto param = GetBasicParam();
  std::unique_ptr<RegistrationFetcher> fetcher =
      RegistrationFetcher::CreateFetcher(param, session_service(),
                                         unexportable_key_service(),
                                         context_.get(), isolation_info,
                                         /*net_log_source=*/std::nullopt,
                                         /*original_request_initiator=*/origin);
  fetcher->StartCreateTokenAndFetch(param, CreateAlgArray(),
                                    callback.callback());
  callback.WaitForCall();
  callback.outcome().SessionForTesting();
}

TEST_F(RegistrationTest, CredEntryWithEmptyName) {
  constexpr char kTestingJson[] =
      R"({
  "session_identifier": "session_id",
  "scope": {
    "include_site": true
  },
  "credentials": [{
    "type": "cookie",
    "name": "",
    "attributes": "Domain=example.com; Path=/; Secure; SameSite=None"
  }]
})";

  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kTestingJson));
  ASSERT_TRUE(server_.Start());

  RecordingNetLogObserver net_log_observer;
  TestRegistrationCallback callback;

  auto param = GetBasicParam();
  std::unique_ptr<RegistrationFetcher> fetcher =
      RegistrationFetcher::CreateFetcher(
          param, session_service(), unexportable_key_service(), context_.get(),
          IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
          /*net_log_source=*/std::nullopt,
          /*original_request_initiator=*/std::nullopt);
  fetcher->StartCreateTokenAndFetch(param, CreateAlgArray(),
                                    callback.callback());
  callback.WaitForCall();
  EXPECT_EQ(callback.outcome().SessionErrorForTesting().type,
            SessionError::kInvalidCredentialsEmptyName);
}

TEST_F(RegistrationTest, ReturnTextFile) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(base::BindRepeating(&ReturnTextResponse));
  ASSERT_TRUE(server_.Start());

  RecordingNetLogObserver net_log_observer;
  TestRegistrationCallback callback;

  RegistrationRequestParam params = GetBasicParam();
  std::unique_ptr<RegistrationFetcher> fetcher =
      RegistrationFetcher::CreateFetcher(
          params, session_service(), unexportable_key_service(), context_.get(),
          IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
          /*net_log_source=*/std::nullopt,
          /*original_request_initiator=*/std::nullopt);
  fetcher->StartCreateTokenAndFetch(params, CreateAlgArray(),
                                    callback.callback());
  callback.WaitForCall();
  EXPECT_EQ(callback.outcome().SessionErrorForTesting().type,
            SessionError::kInvalidConfigJson);
}

TEST_F(RegistrationTest, ReturnInvalidJson) {
  std::string invalid_json = "*{}";
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, invalid_json));
  ASSERT_TRUE(server_.Start());

  RecordingNetLogObserver net_log_observer;
  TestRegistrationCallback callback;

  RegistrationRequestParam param = GetBasicParam();
  std::unique_ptr<RegistrationFetcher> fetcher =
      RegistrationFetcher::CreateFetcher(
          param, session_service(), unexportable_key_service(), context_.get(),
          IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
          /*net_log_source=*/std::nullopt,
          /*original_request_initiator=*/std::nullopt);
  fetcher->StartCreateTokenAndFetch(param, CreateAlgArray(),
                                    callback.callback());
  callback.WaitForCall();
  EXPECT_EQ(callback.outcome().SessionErrorForTesting().type,
            SessionError::kInvalidConfigJson);
}

TEST_F(RegistrationTest, ReturnEmptyJson) {
  std::string empty_json = "{}";
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, empty_json));
  ASSERT_TRUE(server_.Start());

  RecordingNetLogObserver net_log_observer;
  TestRegistrationCallback callback;

  RegistrationRequestParam param = GetBasicParam();
  std::unique_ptr<RegistrationFetcher> fetcher =
      RegistrationFetcher::CreateFetcher(
          param, session_service(), unexportable_key_service(), context_.get(),
          IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
          /*net_log_source=*/std::nullopt,
          /*original_request_initiator=*/std::nullopt);
  fetcher->StartCreateTokenAndFetch(param, CreateAlgArray(),
                                    callback.callback());
  callback.WaitForCall();
  EXPECT_EQ(callback.outcome().SessionErrorForTesting().type,
            SessionError::kInvalidSessionId);
}

TEST_F(RegistrationTest, NetworkErrorServerShutdown) {
  base::HistogramTester histogram_tester;
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  ASSERT_TRUE(server_.Start());
  GURL url = server_.GetURL("/");
  ASSERT_TRUE(server_.ShutdownAndWaitUntilComplete());

  RecordingNetLogObserver net_log_observer;
  TestRegistrationCallback callback;

  RegistrationRequestParam param = GetBasicParam(url);
  std::unique_ptr<RegistrationFetcher> fetcher =
      RegistrationFetcher::CreateFetcher(
          param, session_service(), unexportable_key_service(), context_.get(),
          IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
          /*net_log_source=*/std::nullopt,
          /*original_request_initiator=*/std::nullopt);
  fetcher->StartCreateTokenAndFetch(param, CreateAlgArray(),
                                    callback.callback());
  callback.WaitForCall();

  EXPECT_EQ(callback.outcome().SessionErrorForTesting().type,
            SessionError::kNetError);

  histogram_tester.ExpectUniqueSample(
      "Net.DeviceBoundSessions.Registration.Network.Result",
      net::ERR_CONNECTION_REFUSED, 1);
}

TEST_F(RegistrationTest, NetworkErrorInvalidResponse) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(base::BindRepeating(&ReturnInvalidResponse));
  ASSERT_TRUE(server_.Start());

  RecordingNetLogObserver net_log_observer;
  TestRegistrationCallback callback;

  RegistrationRequestParam param = GetBasicParam();
  std::unique_ptr<RegistrationFetcher> fetcher =
      RegistrationFetcher::CreateFetcher(
          param, session_service(), unexportable_key_service(), context_.get(),
          IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
          /*net_log_source=*/std::nullopt,
          /*original_request_initiator=*/std::nullopt);
  fetcher->StartCreateTokenAndFetch(param, CreateAlgArray(),
                                    callback.callback());
  callback.WaitForCall();

  EXPECT_EQ(callback.outcome().SessionErrorForTesting().type,
            SessionError::kNetError);
}

TEST_F(RegistrationTest, ServerError407) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(base::BindRepeating(
      &ReturnResponse, HTTP_PROXY_AUTHENTICATION_REQUIRED, kBasicValidJson));
  ASSERT_TRUE(server_.Start());

  RecordingNetLogObserver net_log_observer;
  TestRegistrationCallback callback;

  RegistrationRequestParam param = GetBasicParam();
  std::unique_ptr<RegistrationFetcher> fetcher =
      RegistrationFetcher::CreateFetcher(
          param, session_service(), unexportable_key_service(), context_.get(),
          IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
          /*net_log_source=*/std::nullopt,
          /*original_request_initiator=*/std::nullopt);
  fetcher->StartCreateTokenAndFetch(param, CreateAlgArray(),
                                    callback.callback());
  callback.WaitForCall();

  EXPECT_EQ(callback.outcome().SessionErrorForTesting().type,
            SessionError::kNetError);
}

TEST_F(RegistrationTest, ServerError400) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_BAD_REQUEST, kBasicValidJson));
  ASSERT_TRUE(server_.Start());

  RecordingNetLogObserver net_log_observer;
  TestRegistrationCallback callback;

  RegistrationRequestParam param = GetBasicParam();
  std::unique_ptr<RegistrationFetcher> fetcher =
      RegistrationFetcher::CreateFetcher(
          param, session_service(), unexportable_key_service(), context_.get(),
          IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
          /*net_log_source=*/std::nullopt,
          /*original_request_initiator=*/std::nullopt);
  fetcher->StartCreateTokenAndFetch(param, CreateAlgArray(),
                                    callback.callback());
  callback.WaitForCall();

  EXPECT_EQ(callback.outcome().SessionErrorForTesting().type,
            SessionError::kPersistentHttpError);
}

TEST_F(RegistrationTest, ServerError500) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(base::BindRepeating(
      &ReturnResponse, HTTP_INTERNAL_SERVER_ERROR, kBasicValidJson));
  ASSERT_TRUE(server_.Start());

  RecordingNetLogObserver net_log_observer;
  TestRegistrationCallback callback;

  RegistrationRequestParam param = GetBasicParam();
  std::unique_ptr<RegistrationFetcher> fetcher =
      RegistrationFetcher::CreateFetcher(
          param, session_service(), unexportable_key_service(), context_.get(),
          IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
          /*net_log_source=*/std::nullopt,
          /*original_request_initiator=*/std::nullopt);
  fetcher->StartCreateTokenAndFetch(param, CreateAlgArray(),
                                    callback.callback());
  callback.WaitForCall();

  EXPECT_EQ(callback.outcome().SessionErrorForTesting().type,
            SessionError::kTransientHttpError);
}

TEST_F(RegistrationTest, ServerErrorReturnOne403ThenSuccess) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;

  auto* container = new ForbiddenThenSuccessResponseContainer(1);
  server_.RegisterRequestHandler(base::BindRepeating(
      &ForbiddenThenSuccessResponseContainer::Return, base::Owned(container)));
  ASSERT_TRUE(server_.Start());

  std::unique_ptr<Session> session = CreateTestSession(kSessionIdentifier);
  session->set_cached_challenge("challenge");
  EXPECT_CALL(
      session_service(),
      GetSession(SessionKey{SchemefulSite(GetBaseURL()), session->id()}))
      .WillRepeatedly(Return(session.get()));

  RecordingNetLogObserver net_log_observer;
  TestRegistrationCallback callback;

  auto param = RegistrationRequestParam::CreateForTesting(
      GetBaseURL(), kSessionIdentifier, std::string(kChallenge));
  std::unique_ptr<RegistrationFetcher> fetcher =
      RegistrationFetcher::CreateFetcher(
          param, session_service(), unexportable_key_service(), context_.get(),
          IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
          /*net_log_source=*/std::nullopt,
          /*original_request_initiator=*/std::nullopt);
  fetcher->StartCreateTokenAndFetch(param, CreateAlgArray(),
                                    callback.callback());
  callback.WaitForCall();
  const auto& result_session = callback.outcome().SessionForTesting();
  proto::Session session_proto = result_session.ToProto();
  EXPECT_TRUE(session_proto.session_inclusion_rules().do_include_site());
  EXPECT_THAT(
      session_proto.session_inclusion_rules().url_rules(),
      ElementsAre(
          EqualsInclusionRule(proto::RuleType::INCLUDE, "trusted.a.test",
                              "/only_trusted_path"),
          EqualsInclusionRule(proto::RuleType::EXCLUDE, "a.test", "/refresh")));
  EXPECT_THAT(
      session_proto.cookie_cravings(),
      ElementsAre(EqualsCredential(
          "auth_cookie", "Domain=.a.test; Path=/; Secure; SameSite=None")));
}

std::unique_ptr<test_server::HttpResponse> ReturnRedirect(
    const std::string& location,
    const test_server::HttpRequest& request) {
  if (request.relative_url != "/") {
    return nullptr;
  }

  auto response = std::make_unique<test_server::BasicHttpResponse>();
  response->set_code(HTTP_FOUND);
  response->AddCustomHeader("Location", location);
  response->set_content("Redirected");
  response->set_content_type("text/plain");
  return std::move(response);
}

std::unique_ptr<test_server::HttpResponse> CheckRedirect(
    bool* redirect_followed_out,
    const test_server::HttpRequest& request) {
  if (request.relative_url != kRedirectPath) {
    return nullptr;
  }

  *redirect_followed_out = true;
  return ReturnResponse(HTTP_OK, kBasicValidJson, request);
}

// Should be allowed: https://a.test -> https://a.test/redirect.
TEST_F(RegistrationTest, FollowHttpsToHttpsRedirect) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  bool followed = false;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnRedirect, kRedirectPath));
  server_.RegisterRequestHandler(
      base::BindRepeating(&CheckRedirect, &followed));
  // Required to add a certificate for a.test, which is used below.
  server_.SetSSLConfig(EmbeddedTestServer::CERT_TEST_NAMES);
  ASSERT_TRUE(server_.Start());

  RecordingNetLogObserver net_log_observer;
  TestRegistrationCallback callback;

  RegistrationRequestParam param = GetBasicParam(server_.GetURL("a.test", "/"));
  std::unique_ptr<RegistrationFetcher> fetcher =
      RegistrationFetcher::CreateFetcher(
          param, session_service(), unexportable_key_service(), context_.get(),
          IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
          /*net_log_source=*/std::nullopt,
          /*original_request_initiator=*/std::nullopt);
  fetcher->StartCreateTokenAndFetch(param, CreateAlgArray(),
                                    callback.callback());
  callback.WaitForCall();

  EXPECT_TRUE(followed);
  callback.outcome().SessionForTesting();
}

TEST_F(RegistrationTest, FailOnSslErrorExpired) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kBasicValidJson));
  server_.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  ASSERT_TRUE(server_.Start());

  RecordingNetLogObserver net_log_observer;
  TestRegistrationCallback callback;

  RegistrationRequestParam param = GetBasicParam();
  std::unique_ptr<RegistrationFetcher> fetcher =
      RegistrationFetcher::CreateFetcher(
          param, session_service(), unexportable_key_service(), context_.get(),
          IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
          /*net_log_source=*/std::nullopt,
          /*original_request_initiator=*/std::nullopt);
  fetcher->StartCreateTokenAndFetch(param, CreateAlgArray(),
                                    callback.callback());

  callback.WaitForCall();
  EXPECT_EQ(callback.outcome().SessionErrorForTesting().type,
            SessionError::kNetError);
}

std::unique_ptr<test_server::HttpResponse> ReturnResponseForRefreshRequest(
    const test_server::HttpRequest& request) {
  auto response = std::make_unique<test_server::BasicHttpResponse>();

  auto resp_iter = request.headers.find(kSessionResponseHeaderName);
  std::string session_response =
      resp_iter != request.headers.end() ? resp_iter->second : "";
  if (session_response.empty()) {
    const auto session_iter = request.headers.find(kSessionIdHeaderName);
    EXPECT_TRUE(session_iter != request.headers.end() &&
                !session_iter->second.empty());

    response->set_code(HTTP_FORBIDDEN);
    response->AddCustomHeader(kSessionChallengeHeaderName,
                              R"("test_challenge";id="session_id")");
    return response;
  }

  response->set_code(HTTP_OK);
  response->set_content_type("application/json");
  response->set_content(kBasicValidJson);
  return response;
}

TEST_F(RegistrationTest, BasicSuccessForExistingKey) {
  base::HistogramTester histogram_tester;
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kBasicValidJson));
  ASSERT_TRUE(server_.Start());

  RecordingNetLogObserver net_log_observer;
  TestRegistrationCallback callback;

  auto isolation_info = IsolationInfo::CreateTransient(/*nonce=*/std::nullopt);
  auto request_param = RegistrationRequestParam::CreateForTesting(
      GetBaseURL(), kSessionIdentifier, kChallenge);
  unexportable_keys::UnexportableKeyId key = CreateKey();
  std::unique_ptr<RegistrationFetcher> fetcher =
      RegistrationFetcher::CreateFetcher(
          request_param, session_service(),
          std::ref(unexportable_key_service()), context_.get(),
          std::ref(isolation_info),
          /*net_log_source=*/std::nullopt,
          /*original_request_initiator=*/std::nullopt);
  fetcher->StartFetchWithExistingKey(request_param, std::move(key),
                                     callback.callback());
  callback.WaitForCall();
  const auto& session = callback.outcome().SessionForTesting();
  proto::Session session_proto = session.ToProto();
  EXPECT_TRUE(session_proto.session_inclusion_rules().do_include_site());
  EXPECT_THAT(
      session_proto.session_inclusion_rules().url_rules(),
      ElementsAre(
          EqualsInclusionRule(proto::RuleType::INCLUDE, "trusted.a.test",
                              "/only_trusted_path"),
          EqualsInclusionRule(proto::RuleType::EXCLUDE, "a.test", "/refresh")));
  EXPECT_THAT(
      session_proto.cookie_cravings(),
      ElementsAre(EqualsCredential(
          "auth_cookie", "Domain=.a.test; Path=/; Secure; SameSite=None")));

  histogram_tester.ExpectBucketCount(
      "Net.DeviceBoundSessions.Refresh.Network.Result", HTTP_OK, 1);
}

TEST_F(RegistrationTest, FetchRegistrationWithCachedChallenge) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponseForRefreshRequest));
  ASSERT_TRUE(server_.Start());

  RecordingNetLogObserver net_log_observer;
  TestRegistrationCallback callback;

  auto request_param = RegistrationRequestParam::CreateForTesting(
      GetBaseURL(), kSessionIdentifier, kChallenge);
  auto isolation_info = IsolationInfo::CreateTransient(/*nonce=*/std::nullopt);
  unexportable_keys::UnexportableKeyId key = CreateKey();
  std::unique_ptr<RegistrationFetcher> fetcher =
      RegistrationFetcher::CreateFetcher(
          request_param, session_service(),
          std::ref(unexportable_key_service()), context_.get(),
          std::ref(isolation_info),
          /*net_log_source=*/std::nullopt,
          /*original_request_initiator=*/std::nullopt);
  fetcher->StartFetchWithExistingKey(request_param, std::move(key),
                                     callback.callback());
  callback.WaitForCall();
  const auto& session = callback.outcome().SessionForTesting();
  proto::Session session_proto = session.ToProto();
  EXPECT_TRUE(session_proto.session_inclusion_rules().do_include_site());
  EXPECT_THAT(
      session_proto.session_inclusion_rules().url_rules(),
      ElementsAre(
          EqualsInclusionRule(proto::RuleType::INCLUDE, "trusted.a.test",
                              "/only_trusted_path"),
          EqualsInclusionRule(proto::RuleType::EXCLUDE, "a.test", "/refresh")));
  EXPECT_THAT(
      session_proto.cookie_cravings(),
      ElementsAre(EqualsCredential(
          "auth_cookie", "Domain=.a.test; Path=/; Secure; SameSite=None")));
}

TEST_F(RegistrationTest, FetchRegistrationAndChallengeRequired) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(base::BindRepeating(&ReturnForbidden));
  ASSERT_TRUE(server_.Start());

  RecordingNetLogObserver net_log_observer;
  TestRegistrationCallback callback;

  auto request_param = RegistrationRequestParam::CreateForTesting(
      GetBaseURL(), /*session_identifier=*/std::nullopt, kChallenge);
  auto isolation_info = IsolationInfo::CreateTransient(/*nonce=*/std::nullopt);
  unexportable_keys::UnexportableKeyId key = CreateKey();
  std::unique_ptr<RegistrationFetcher> fetcher =
      RegistrationFetcher::CreateFetcher(
          request_param, session_service(),
          std::ref(unexportable_key_service()), context_.get(),
          std::ref(isolation_info),
          /*net_log_source=*/std::nullopt,
          /*original_request_initiator=*/std::nullopt);
  fetcher->StartFetchWithExistingKey(request_param, std::move(key),
                                     callback.callback());
  callback.WaitForCall();
  EXPECT_EQ(callback.outcome().SessionErrorForTesting().type,
            SessionError::kRegistrationAttemptedChallenge);
}

TEST_F(RegistrationTest, FetchRefreshAndChallengeRequired_NoChallenge) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(base::BindRepeating(&ReturnForbidden));
  ASSERT_TRUE(server_.Start());

  std::unique_ptr<Session> session = CreateTestSession("session_identifier");
  EXPECT_CALL(
      session_service(),
      GetSession(SessionKey{SchemefulSite(GetBaseURL()), session->id()}))
      .WillRepeatedly(Return(session.get()));

  RecordingNetLogObserver net_log_observer;
  TestRegistrationCallback callback;

  auto request_param = RegistrationRequestParam::CreateForTesting(
      GetBaseURL(), "session_identifier", kChallenge);
  auto isolation_info = IsolationInfo::CreateTransient(/*nonce=*/std::nullopt);
  unexportable_keys::UnexportableKeyId key = CreateKey();
  std::unique_ptr<RegistrationFetcher> fetcher =
      RegistrationFetcher::CreateFetcher(
          request_param, session_service(),
          std::ref(unexportable_key_service()), context_.get(),
          std::ref(isolation_info),
          /*net_log_source=*/std::nullopt,
          /*original_request_initiator=*/std::nullopt);
  fetcher->StartFetchWithExistingKey(request_param, std::move(key),
                                     callback.callback());
  callback.WaitForCall();
  EXPECT_EQ(callback.outcome().SessionErrorForTesting().type,
            SessionError::kInvalidChallenge);
}

TEST_F(RegistrationTest,
       FetchRefreshAndChallengeRequired_NoChallengeToNewChallenge) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  ASSERT_TRUE(server_.InitializeAndListen());
  std::unique_ptr<Session> session = CreateTestSession(kSessionIdentifier);
  EXPECT_CALL(
      session_service(),
      GetSession(SessionKey{SchemefulSite(GetBaseURL()), session->id()}))
      .WillOnce(Return(session.get()));
  server_.RegisterRequestHandler(base::BindRepeating(
      [](Session* session, const test_server::HttpRequest& request)
          -> std::unique_ptr<test_server::HttpResponse> {
        auto response = std::make_unique<test_server::BasicHttpResponse>();
        const std::optional<std::string> challenge =
            GetRequestChallenge(request);
        if (!challenge.has_value()) {
          response->set_code(HTTP_FORBIDDEN);
          session->set_cached_challenge("updated_challenge");
          return response;
        }

        if (*challenge == "updated_challenge") {
          response->set_code(HTTP_OK);
          response->set_content_type("application/json");
          response->set_content(kBasicValidJson);
          return response;
        }

        response->set_code(HTTP_FORBIDDEN);
        return response;
      },
      session.get()));
  server_.StartAcceptingConnections();

  RecordingNetLogObserver net_log_observer;
  TestRegistrationCallback callback;

  auto request_param = RegistrationRequestParam::CreateForRefresh(*session);
  auto isolation_info = IsolationInfo::CreateTransient(/*nonce=*/std::nullopt);
  unexportable_keys::UnexportableKeyId key = CreateKey();
  std::unique_ptr<RegistrationFetcher> fetcher =
      RegistrationFetcher::CreateFetcher(
          request_param, session_service(),
          std::ref(unexportable_key_service()), context_.get(),
          std::ref(isolation_info),
          /*net_log_source=*/std::nullopt,
          /*original_request_initiator=*/std::nullopt);
  fetcher->StartFetchWithExistingKey(request_param, std::move(key),
                                     callback.callback());
  callback.WaitForCall();
  callback.outcome().SessionForTesting();
}

TEST_F(RegistrationTest,
       FetchRefreshAndChallengeRequired_ExistingChallengeToNewChallenge) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  ASSERT_TRUE(server_.InitializeAndListen());
  std::unique_ptr<Session> session = CreateTestSession(kSessionIdentifier);
  EXPECT_CALL(
      session_service(),
      GetSession(SessionKey{SchemefulSite(GetBaseURL()), session->id()}))
      .WillOnce(Return(session.get()));
  server_.RegisterRequestHandler(base::BindRepeating(
      [](Session* session, const test_server::HttpRequest& request)
          -> std::unique_ptr<test_server::HttpResponse> {
        auto response = std::make_unique<test_server::BasicHttpResponse>();
        const std::optional<std::string> challenge =
            GetRequestChallenge(request);
        if (*challenge == kChallenge) {
          response->set_code(HTTP_FORBIDDEN);
          session->set_cached_challenge("updated_challenge");
          return response;
        }

        if (*challenge == "updated_challenge") {
          response->set_code(HTTP_OK);
          response->set_content_type("application/json");
          response->set_content(kBasicValidJson);
          return response;
        }

        response->set_code(HTTP_FORBIDDEN);
        return response;
      },
      session.get()));
  server_.StartAcceptingConnections();

  session->set_cached_challenge(kChallenge);

  RecordingNetLogObserver net_log_observer;
  TestRegistrationCallback callback;

  auto request_param = RegistrationRequestParam::CreateForRefresh(*session);
  auto isolation_info = IsolationInfo::CreateTransient(/*nonce=*/std::nullopt);
  unexportable_keys::UnexportableKeyId key = CreateKey();
  std::unique_ptr<RegistrationFetcher> fetcher =
      RegistrationFetcher::CreateFetcher(
          request_param, session_service(),
          std::ref(unexportable_key_service()), context_.get(),
          std::ref(isolation_info),
          /*net_log_source=*/std::nullopt,
          /*original_request_initiator=*/std::nullopt);
  fetcher->StartFetchWithExistingKey(request_param, std::move(key),
                                     callback.callback());
  callback.WaitForCall();
  callback.outcome().SessionForTesting();
}

TEST_F(RegistrationTest, ContinueFalse) {
  constexpr char kTestingJson[] =
      R"({
  "session_identifier": "session_id",
  "continue": false
})";
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kTestingJson));
  ASSERT_TRUE(server_.Start());

  RecordingNetLogObserver net_log_observer;
  TestRegistrationCallback callback;

  auto param = GetBasicParam();
  std::unique_ptr<RegistrationFetcher> fetcher =
      RegistrationFetcher::CreateFetcher(
          param, session_service(), unexportable_key_service(), context_.get(),
          IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
          /*net_log_source=*/std::nullopt,
          /*original_request_initiator=*/std::nullopt);
  fetcher->StartCreateTokenAndFetch(param, CreateAlgArray(),
                                    callback.callback());
  callback.WaitForCall();
  EXPECT_EQ(callback.outcome().SessionErrorForTesting().type,
            SessionError::kServerRequestedTermination);
}

TEST_F(RegistrationTest, TerminateSessionOnRepeatedFailure_Refresh) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kBasicValidJson));
  ASSERT_TRUE(server_.Start());

  unexportable_keys::MockUnexportableKeyService mock_service;

  EXPECT_CALL(mock_service, GetAlgorithm(_))
      .WillRepeatedly(
          Invoke(&unexportable_key_service(),
                 &unexportable_keys::UnexportableKeyService::GetAlgorithm));
  EXPECT_CALL(mock_service, GetSubjectPublicKeyInfo(_))
      .WillRepeatedly(Invoke(
          &unexportable_key_service(),
          &unexportable_keys::UnexportableKeyService::GetSubjectPublicKeyInfo));
  EXPECT_CALL(mock_service, SignSlowlyAsync)
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<3>(
          base::unexpected(unexportable_keys::ServiceError::kCryptoApiFailed)));

  RecordingNetLogObserver net_log_observer;
  TestRegistrationCallback callback;

  auto isolation_info = IsolationInfo::CreateTransient(/*nonce=*/std::nullopt);
  auto request_param = RegistrationRequestParam::CreateForTesting(
      GetBaseURL(), kSessionIdentifier, kChallenge);
  unexportable_keys::UnexportableKeyId key = CreateKey();
  std::unique_ptr<RegistrationFetcher> fetcher =
      RegistrationFetcher::CreateFetcher(
          request_param, session_service(), std::ref(mock_service),
          context_.get(), std::ref(isolation_info),
          /*net_log_source=*/std::nullopt,
          /*original_request_initiator=*/std::nullopt);
  fetcher->StartFetchWithExistingKey(request_param, std::move(key),
                                     callback.callback());
  callback.WaitForCall();

  EXPECT_EQ(callback.outcome().SessionErrorForTesting().type,
            SessionError::kSigningError);
}

TEST_F(RegistrationTest, TerminateSessionOnRepeatedFailure_Registration) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kBasicValidJson));
  ASSERT_TRUE(server_.Start());

  unexportable_keys::MockUnexportableKeyService mock_service;

  EXPECT_CALL(mock_service, GetAlgorithm(_))
      .WillRepeatedly(
          Invoke(&unexportable_key_service(),
                 &unexportable_keys::UnexportableKeyService::GetAlgorithm));
  EXPECT_CALL(mock_service, GetSubjectPublicKeyInfo(_))
      .WillRepeatedly(Invoke(
          &unexportable_key_service(),
          &unexportable_keys::UnexportableKeyService::GetSubjectPublicKeyInfo));
  EXPECT_CALL(mock_service, SignSlowlyAsync)
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<3>(
          base::unexpected(unexportable_keys::ServiceError::kCryptoApiFailed)));

  RecordingNetLogObserver net_log_observer;
  TestRegistrationCallback callback;

  auto isolation_info = IsolationInfo::CreateTransient(/*nonce=*/std::nullopt);
  auto request_param = RegistrationRequestParam::CreateForTesting(
      GetBaseURL(), /*session_identifier=*/std::nullopt, kChallenge);
  unexportable_keys::UnexportableKeyId key = CreateKey();
  std::unique_ptr<RegistrationFetcher> fetcher =
      RegistrationFetcher::CreateFetcher(
          request_param, session_service(), std::ref(mock_service),
          context_.get(), std::ref(isolation_info),
          /*net_log_source=*/std::nullopt,
          /*original_request_initiator=*/std::nullopt);
  fetcher->StartFetchWithExistingKey(request_param, std::move(key),
                                     callback.callback());
  callback.WaitForCall();

  EXPECT_EQ(callback.outcome().SessionErrorForTesting().type,
            SessionError::kSigningError);
}

TEST_F(RegistrationTest, NetLogRegistrationResultLogged) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kBasicValidJson));
  ASSERT_TRUE(server_.Start());

  RecordingNetLogObserver net_log_observer;
  TestRegistrationCallback callback;
  auto param = GetBasicParam();
  std::unique_ptr<RegistrationFetcher> fetcher =
      RegistrationFetcher::CreateFetcher(
          param, session_service(), unexportable_key_service(), context_.get(),
          IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
          /*net_log_source=*/std::nullopt,
          /*original_request_initiator=*/std::nullopt);
  fetcher->StartCreateTokenAndFetch(param, CreateAlgArray(),
                                    callback.callback());
  callback.WaitForCall();

  EXPECT_EQ(net_log_observer
                .GetEntriesWithType(NetLogEventType::DBSC_REGISTRATION_RESULT)
                .size(),
            1u);
}

TEST_F(RegistrationTest, NetLogRefreshResultLogged) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kBasicValidJson));
  ASSERT_TRUE(server_.Start());

  RecordingNetLogObserver net_log_observer;
  TestRegistrationCallback callback;
  auto isolation_info = IsolationInfo::CreateTransient(/*nonce=*/std::nullopt);
  auto request_param = RegistrationRequestParam::CreateForTesting(
      GetBaseURL(), kSessionIdentifier, kChallenge);
  unexportable_keys::UnexportableKeyId key = CreateKey();
  std::unique_ptr<RegistrationFetcher> fetcher =
      RegistrationFetcher::CreateFetcher(
          request_param, session_service(),
          std::ref(unexportable_key_service()), context_.get(),
          std::ref(isolation_info),
          /*net_log_source=*/std::nullopt,
          /*original_request_initiator=*/std::nullopt);
  fetcher->StartFetchWithExistingKey(request_param, std::move(key),
                                     callback.callback());
  callback.WaitForCall();

  EXPECT_EQ(
      net_log_observer.GetEntriesWithType(NetLogEventType::DBSC_REFRESH_RESULT)
          .size(),
      1u);
}

TEST_F(RegistrationTest, TerminateSessionOnRepeatedChallenge) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;

  auto* container = new ForbiddenThenSuccessResponseContainer(100);
  server_.RegisterRequestHandler(base::BindRepeating(
      &ForbiddenThenSuccessResponseContainer::Return, base::Owned(container)));
  ASSERT_TRUE(server_.Start());

  std::unique_ptr<Session> session = CreateTestSession(kSessionIdentifier);
  session->set_cached_challenge("challenge");
  EXPECT_CALL(
      session_service(),
      GetSession(SessionKey{SchemefulSite(GetBaseURL()), session->id()}))
      .WillRepeatedly(Return(session.get()));

  RecordingNetLogObserver net_log_observer;
  TestRegistrationCallback callback;

  auto isolation_info = IsolationInfo::CreateTransient(/*nonce=*/std::nullopt);
  auto request_param = RegistrationRequestParam::CreateForTesting(
      GetBaseURL(), kSessionIdentifier, kChallenge);
  unexportable_keys::UnexportableKeyId key = CreateKey();
  std::unique_ptr<RegistrationFetcher> fetcher =
      RegistrationFetcher::CreateFetcher(
          request_param, session_service(),
          std::ref(unexportable_key_service()), context_.get(),
          std::ref(isolation_info),
          /*net_log_source=*/std::nullopt,
          /*original_request_initiator=*/std::nullopt);
  fetcher->StartFetchWithExistingKey(request_param, std::move(key),
                                     callback.callback());
  callback.WaitForCall();

  EXPECT_EQ(callback.outcome().SessionErrorForTesting().type,
            SessionError::kTooManyChallenges);
}

TEST_F(RegistrationTest, RefreshCachesSignedChallenge) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kDeviceBoundSessionSigningQuotaAndCaching);

  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kBasicValidJson));
  ASSERT_TRUE(server_.Start());

  unexportable_keys::MockUnexportableKeyService mock_key_service;
  auto [spki, jwk] = GetRS256SpkiAndJwkForTesting();
  std::vector<unsigned char> spki_vector(spki.begin(), spki.end());
  EXPECT_CALL(mock_key_service, GetAlgorithm(_))
      .WillOnce(Return(crypto::SignatureVerifier::RSA_PKCS1_SHA256));
  EXPECT_CALL(mock_key_service, GetSubjectPublicKeyInfo(_))
      .WillOnce(Return(spki_vector));
  EXPECT_CALL(mock_key_service, SignSlowlyAsync(_, _, _, _))
      .WillOnce(WithArg<3>([](auto callback) {
        std::move(callback).Run(std::vector<uint8_t>{'s', 'i', 'g'});
      }));

  // No cached challenge initially.
  EXPECT_CALL(session_service(), GetLatestSignedRefreshChallenge(_))
      .WillOnce(Return(nullptr));
  // Expect a signing occurrence and the new signed challenge to be cached.
  EXPECT_CALL(session_service(), AddSigningOccurrence(_)).Times(1);
  EXPECT_CALL(session_service(), SetLatestSignedRefreshChallenge(_, _))
      .Times(1);
  // Quota check should return false to allow signing.
  EXPECT_CALL(session_service(), SigningQuotaExceeded(_))
      .WillOnce(Return(false));

  RecordingNetLogObserver net_log_observer;
  TestRegistrationCallback callback;

  auto isolation_info = IsolationInfo::CreateTransient(/*nonce=*/std::nullopt);
  auto request_param = RegistrationRequestParam::CreateForTesting(
      GetBaseURL(), kSessionIdentifier, kChallenge);
  unexportable_keys::UnexportableKeyId key =
      unexportable_keys::UnexportableKeyId();
  std::unique_ptr<RegistrationFetcher> fetcher =
      RegistrationFetcher::CreateFetcher(
          request_param, session_service(), std::ref(mock_key_service),
          context_.get(), std::ref(isolation_info),
          /*net_log_source=*/std::nullopt,
          /*original_request_initiator=*/std::nullopt);
  fetcher->StartFetchWithExistingKey(request_param, std::move(key),
                                     callback.callback());
  callback.WaitForCall();

  callback.outcome().SessionForTesting();
}

TEST_F(RegistrationTest, RefreshCachedSignedChallengeUsed) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kDeviceBoundSessionSigningQuotaAndCaching);

  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kBasicValidJson));
  ASSERT_TRUE(server_.Start());

  // No calls to actual signing.
  unexportable_keys::MockUnexportableKeyService mock_key_service;
  EXPECT_CALL(mock_key_service, GetAlgorithm(_)).Times(0);
  EXPECT_CALL(mock_key_service, GetSubjectPublicKeyInfo(_)).Times(0);
  EXPECT_CALL(mock_key_service, SignSlowlyAsync(_, _, _, _)).Times(0);

  // Create a matching cached challenge.
  SessionService::SignedRefreshChallenge cached_challenge;
  cached_challenge.challenge = kChallenge;
  cached_challenge.key_id = unexportable_keys::UnexportableKeyId();
  cached_challenge.signed_challenge = "mock_signed_challenge";

  EXPECT_CALL(session_service(), GetLatestSignedRefreshChallenge(_))
      .WillOnce(Return(&cached_challenge));
  // There should be no signing or quota checking since that's skipped.
  EXPECT_CALL(session_service(), AddSigningOccurrence(_)).Times(0);
  EXPECT_CALL(session_service(), SigningQuotaExceeded(_)).Times(0);

  RecordingNetLogObserver net_log_observer;
  TestRegistrationCallback callback;

  auto isolation_info = IsolationInfo::CreateTransient(/*nonce=*/std::nullopt);
  auto request_param = RegistrationRequestParam::CreateForTesting(
      GetBaseURL(), kSessionIdentifier, kChallenge);
  unexportable_keys::UnexportableKeyId key = cached_challenge.key_id;
  std::unique_ptr<RegistrationFetcher> fetcher =
      RegistrationFetcher::CreateFetcher(
          request_param, session_service(), std::ref(mock_key_service),
          context_.get(), std::ref(isolation_info),
          /*net_log_source=*/std::nullopt,
          /*original_request_initiator=*/std::nullopt);
  fetcher->StartFetchWithExistingKey(request_param, std::move(key),
                                     callback.callback());
  callback.WaitForCall();

  callback.outcome().SessionForTesting();
}

TEST_F(RegistrationTest, RefreshCachedSignedChallengeDoesNotMatch) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kDeviceBoundSessionSigningQuotaAndCaching);

  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kBasicValidJson));
  ASSERT_TRUE(server_.Start());

  unexportable_keys::MockUnexportableKeyService mock_key_service;
  auto [spki, jwk] = GetRS256SpkiAndJwkForTesting();
  std::vector<unsigned char> spki_vector(spki.begin(), spki.end());
  EXPECT_CALL(mock_key_service, GetAlgorithm(_))
      .WillOnce(Return(crypto::SignatureVerifier::RSA_PKCS1_SHA256));
  EXPECT_CALL(mock_key_service, GetSubjectPublicKeyInfo(_))
      .WillOnce(Return(spki_vector));
  EXPECT_CALL(mock_key_service, SignSlowlyAsync(_, _, _, _))
      .WillOnce(WithArg<3>([](auto callback) {
        std::move(callback).Run(std::vector<uint8_t>{'s', 'i', 'g'});
      }));

  // Add cached signed challenge that doesn't match (the challenge used is
  // different).
  SessionService::SignedRefreshChallenge cached_challenge;
  cached_challenge.challenge = "different_challenge";
  cached_challenge.key_id = unexportable_keys::UnexportableKeyId();
  cached_challenge.signed_challenge = "mock_signed_challenge";
  EXPECT_CALL(session_service(), GetLatestSignedRefreshChallenge(_))
      .WillOnce(Return(&cached_challenge));
  EXPECT_CALL(session_service(), AddSigningOccurrence(_)).Times(1);
  EXPECT_CALL(session_service(), SetLatestSignedRefreshChallenge(_, _))
      .Times(1);
  EXPECT_CALL(session_service(), SigningQuotaExceeded(_))
      .WillOnce(Return(false));

  RecordingNetLogObserver net_log_observer;
  TestRegistrationCallback callback;

  auto isolation_info = IsolationInfo::CreateTransient(/*nonce=*/std::nullopt);
  auto request_param = RegistrationRequestParam::CreateForTesting(
      GetBaseURL(), kSessionIdentifier, kChallenge);
  unexportable_keys::UnexportableKeyId key =
      unexportable_keys::UnexportableKeyId();
  std::unique_ptr<RegistrationFetcher> fetcher =
      RegistrationFetcher::CreateFetcher(
          request_param, session_service(), std::ref(mock_key_service),
          context_.get(), std::ref(isolation_info),
          /*net_log_source=*/std::nullopt,
          /*original_request_initiator=*/std::nullopt);
  fetcher->StartFetchWithExistingKey(request_param, std::move(key),
                                     callback.callback());
  callback.WaitForCall();

  callback.outcome().SessionForTesting();
}

TEST_F(RegistrationTest, RegistrationTriggersSigningOccurrence) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kDeviceBoundSessionSigningQuotaAndCaching);

  // Expect the signing checks are done only when the feature is enabled.
  EXPECT_CALL(session_service(), AddSigningOccurrence(_)).Times(1);
  EXPECT_CALL(session_service(), SigningQuotaExceeded(_))
      .WillOnce(Return(false));

  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(
      base::BindRepeating([](const test_server::HttpRequest& request) {
        auto resp_iter = request.headers.find(kSessionResponseHeaderName);
        EXPECT_TRUE(resp_iter != request.headers.end());
        if (resp_iter != request.headers.end()) {
          EXPECT_TRUE(VerifyEs256Jwt(resp_iter->second));
        }
        return ReturnResponse(HTTP_OK, kBasicValidJson, request);
      }));
  ASSERT_TRUE(server_.Start());
  TestRegistrationCallback callback;
  auto param = GetBasicParam();
  std::unique_ptr<RegistrationFetcher> fetcher =
      RegistrationFetcher::CreateFetcher(
          param, session_service(), unexportable_key_service(), context_.get(),
          IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
          /*net_log_source=*/std::nullopt,
          /*original_request_initiator=*/std::nullopt);
  fetcher->StartCreateTokenAndFetch(param, CreateAlgArray(),
                                    callback.callback());
  callback.WaitForCall();
}

TEST_F(RegistrationTest, RefreshWithNewSessionIdFails) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;

  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kBasicValidJson));
  ASSERT_TRUE(server_.Start());

  RecordingNetLogObserver net_log_observer;
  TestRegistrationCallback callback;

  auto isolation_info = IsolationInfo::CreateTransient(/*nonce=*/std::nullopt);
  auto request_param = RegistrationRequestParam::CreateForTesting(
      GetBaseURL(), "old_session_id", kChallenge);
  unexportable_keys::UnexportableKeyId key = CreateKey();
  std::unique_ptr<RegistrationFetcher> fetcher =
      RegistrationFetcher::CreateFetcher(
          request_param, session_service(),
          std::ref(unexportable_key_service()), context_.get(),
          std::ref(isolation_info),
          /*net_log_source=*/std::nullopt,
          /*original_request_initiator=*/std::nullopt);
  fetcher->StartFetchWithExistingKey(request_param, std::move(key),
                                     callback.callback());
  callback.WaitForCall();

  const RegistrationResult& out_session = callback.outcome();
  const SessionError& session_error = out_session.SessionErrorForTesting();
  EXPECT_EQ(session_error.type, SessionError::kMismatchedSessionId);
}

TEST_F(RegistrationTest, RegistrationWithNonStringRefreshInitiatorsFails) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;

  constexpr char kNonStringInitiator[] =
      R"({
  "session_identifier": "session_id",
  "scope": {
    "include_site": true,
    "scope_specification" : [
      {
        "type": "include",
        "domain": "trusted.example.com",
        "path": "/only_trusted_path"
      }
    ]
  },
  "credentials": [{
    "type": "cookie",
    "name": "auth_cookie",
    "attributes": "Domain=example.com; Path=/; Secure; SameSite=None"
  }],
  "allowed_refresh_initiators": [ 12345 ]
})";
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kNonStringInitiator));
  ASSERT_TRUE(server_.Start());

  RecordingNetLogObserver net_log_observer;
  TestRegistrationCallback callback;

  auto isolation_info = IsolationInfo::CreateTransient(/*nonce=*/std::nullopt);
  auto request_param = RegistrationRequestParam::CreateForTesting(
      GetBaseURL(), kSessionIdentifier, kChallenge);
  unexportable_keys::UnexportableKeyId key = CreateKey();
  std::unique_ptr<RegistrationFetcher> fetcher =
      RegistrationFetcher::CreateFetcher(
          request_param, session_service(),
          std::ref(unexportable_key_service()), context_.get(),
          std::ref(isolation_info),
          /*net_log_source=*/std::nullopt,
          /*original_request_initiator=*/std::nullopt);
  fetcher->StartFetchWithExistingKey(request_param, std::move(key),
                                     callback.callback());
  callback.WaitForCall();

  const RegistrationResult& out_session = callback.outcome();
  const SessionError& session_error = out_session.SessionErrorForTesting();
  EXPECT_EQ(session_error.type, SessionError::kRefreshInitiatorNotString);
}

TEST_F(RegistrationTest, MissingIncludeSiteFails) {
  constexpr char kTestingJson[] =
      R"({
  "session_identifier": "session_id",
  "refresh_url": "/refresh",
  "scope": {
    "scope_specification" : [
      {
        "type": "include",
        "domain": "trusted.a.test"
      },
      {
        "type": "exclude",
        "domain": "new.a.test",
        "path": "/only_trusted_path"
      }
    ]
  },
  "credentials": [{
    "type": "cookie",
    "name": "other_cookie",
    "attributes": "Domain=a.test; Path=/; Secure; SameSite=None"
  }]
})";

  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kTestingJson));
  ASSERT_TRUE(server_.Start());

  RecordingNetLogObserver net_log_observer;
  TestRegistrationCallback callback;

  auto param = GetBasicParam();
  std::unique_ptr<RegistrationFetcher> fetcher =
      RegistrationFetcher::CreateFetcher(
          param, session_service(), unexportable_key_service(), context_.get(),
          IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
          /*net_log_source=*/std::nullopt,
          /*original_request_initiator=*/std::nullopt);
  fetcher->StartCreateTokenAndFetch(param, CreateAlgArray(),
                                    callback.callback());
  callback.WaitForCall();
  const RegistrationResult& out_session = callback.outcome();
  EXPECT_EQ(out_session.SessionErrorForTesting().type,
            SessionError::kMissingScopeIncludeSite);
}

TEST_F(RegistrationTest, ShutdownDuringRequest) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  base::RunLoop run_loop;
  server_.RegisterRequestHandler(base::BindRepeating(
      [](base::RunLoop* run_loop, const test_server::HttpRequest& request)
          -> std::unique_ptr<test_server::HttpResponse> {
        run_loop->Quit();
        return std::make_unique<test_server::HungResponse>();
      },
      &run_loop));
  ASSERT_TRUE(server_.Start());

  RecordingNetLogObserver net_log_observer;
  TestRegistrationCallback callback;

  auto param = GetBasicParam();
  std::unique_ptr<RegistrationFetcher> fetcher =
      RegistrationFetcher::CreateFetcher(
          param, session_service(), unexportable_key_service(), context_.get(),
          IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
          /*net_log_source=*/std::nullopt,
          /*original_request_initiator=*/std::nullopt);
  fetcher->StartCreateTokenAndFetch(param, CreateAlgArray(),
                                    callback.callback());

  run_loop.Run();

  EXPECT_EQ(context_->url_requests()->size(), 1u);

  fetcher.reset();

  EXPECT_EQ(context_->url_requests()->size(), 0u);
}

TEST_F(RegistrationTest, EmptyResponseOnRegistration) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, ""));
  ASSERT_TRUE(server_.Start());

  RecordingNetLogObserver net_log_observer;
  TestRegistrationCallback callback;

  auto param = GetBasicParam();
  std::unique_ptr<RegistrationFetcher> fetcher =
      RegistrationFetcher::CreateFetcher(
          param, session_service(), unexportable_key_service(), context_.get(),
          IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
          /*net_log_source=*/std::nullopt,
          /*original_request_initiator=*/std::nullopt);
  fetcher->StartCreateTokenAndFetch(param, CreateAlgArray(),
                                    callback.callback());
  callback.WaitForCall();
  const RegistrationResult& out_session = callback.outcome();
  EXPECT_EQ(out_session.SessionErrorForTesting().type,
            SessionError::kEmptySessionConfig);
}

TEST_F(RegistrationTest, EmptyResponseOnRefresh) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, ""));
  ASSERT_TRUE(server_.Start());

  RecordingNetLogObserver net_log_observer;
  TestRegistrationCallback callback;

  auto request_param = RegistrationRequestParam::CreateForTesting(
      GetBaseURL(), kSessionIdentifier, kChallenge);
  unexportable_keys::UnexportableKeyId key = CreateKey();
  std::unique_ptr<RegistrationFetcher> fetcher =
      RegistrationFetcher::CreateFetcher(
          request_param, session_service(), unexportable_key_service(),
          context_.get(),
          IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
          /*net_log_source=*/std::nullopt,
          /*original_request_initiator=*/std::nullopt);
  fetcher->StartFetchWithExistingKey(request_param, std::move(key),
                                     callback.callback());
  callback.WaitForCall();
  const RegistrationResult& out_session = callback.outcome();
  out_session.NoSessionConfigChangeForTesting();
}

TEST_F(RegistrationTest, SetChallengeOnRegistration) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(
      base::BindRepeating([](const test_server::HttpRequest& request)
                              -> std::unique_ptr<test_server::HttpResponse> {
        auto response = std::make_unique<test_server::BasicHttpResponse>();
        response->set_code(HTTP_OK);
        response->AddCustomHeader(kSessionChallengeHeaderName,
                                  R"("test_challenge";id="session_id")");
        response->set_content_type("application/json");
        response->set_content(kBasicValidJson);
        return response;
      }));
  ASSERT_TRUE(server_.Start());

  RecordingNetLogObserver net_log_observer;
  TestRegistrationCallback callback;

  auto param = GetBasicParam();
  std::unique_ptr<RegistrationFetcher> fetcher =
      RegistrationFetcher::CreateFetcher(
          param, session_service(), unexportable_key_service(), context_.get(),
          IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
          /*net_log_source=*/std::nullopt,
          /*original_request_initiator=*/std::nullopt);
  fetcher->StartCreateTokenAndFetch(param, CreateAlgArray(),
                                    callback.callback());
  callback.WaitForCall();
  const RegistrationResult& out_session = callback.outcome();
  EXPECT_EQ(out_session.SessionForTesting().cached_challenge(),
            "test_challenge");
}

TEST_F(RegistrationTest, RegistrationBySubdomain_Success) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;

  server_.RegisterRequestHandler(base::BindRepeating(
      &ReturnForHostAndPath, "a.test", "/.well-known/device-bound-sessions",
      base::BindRepeating(&ReturnWellKnown,
                          R"json({
                            "registering_origins": [ "https://subdomain.a.test:$1" ]
                          })json")));
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kBasicValidJson));
  ASSERT_TRUE(server_.Start());

  GURL registration_url = server_.GetURL("subdomain.a.test", "/");

  RecordingNetLogObserver net_log_observer;
  TestRegistrationCallback callback;

  auto param = GetBasicParam(registration_url);
  std::unique_ptr<RegistrationFetcher> fetcher =
      RegistrationFetcher::CreateFetcher(
          param, session_service(), unexportable_key_service(), context_.get(),
          IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
          /*net_log_source=*/std::nullopt,
          /*original_request_initiator=*/std::nullopt);
  fetcher->StartCreateTokenAndFetch(param, CreateAlgArray(),
                                    callback.callback());
  callback.WaitForCall();
  callback.outcome().SessionForTesting();
}

TEST_F(RegistrationTest, RegistrationBySubdomain_WellKnownUnavailable) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;

  server_.RegisterRequestHandler(base::BindRepeating(
      &ReturnForHostAndPath, "a.test", "/.well-known/device-bound-sessions",
      base::BindRepeating(&ReturnResponse, HTTP_BAD_REQUEST, "")));
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kBasicValidJson));
  ASSERT_TRUE(server_.Start());

  GURL registration_url = server_.GetURL("subdomain.a.test", "/");

  RecordingNetLogObserver net_log_observer;
  TestRegistrationCallback callback;

  auto param = GetBasicParam(registration_url);
  std::unique_ptr<RegistrationFetcher> fetcher =
      RegistrationFetcher::CreateFetcher(
          param, session_service(), unexportable_key_service(), context_.get(),
          IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
          /*net_log_source=*/std::nullopt,
          /*original_request_initiator=*/std::nullopt);
  fetcher->StartCreateTokenAndFetch(param, CreateAlgArray(),
                                    callback.callback());
  callback.WaitForCall();
  const RegistrationResult& out_session = callback.outcome();
  EXPECT_EQ(out_session.SessionErrorForTesting().type,
            SessionError::kSubdomainRegistrationWellKnownUnavailable);
}

TEST_F(RegistrationTest, RegistrationBySubdomain_WellKnownMalformed) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;

  server_.RegisterRequestHandler(base::BindRepeating(
      &ReturnForHostAndPath, "a.test", "/.well-known/device-bound-sessions",
      base::BindRepeating(&ReturnResponse, HTTP_OK, "invalid JSON")));
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kBasicValidJson));
  ASSERT_TRUE(server_.Start());

  GURL registration_url = server_.GetURL("subdomain.a.test", "/");

  RecordingNetLogObserver net_log_observer;
  TestRegistrationCallback callback;

  auto param = GetBasicParam(registration_url);
  std::unique_ptr<RegistrationFetcher> fetcher =
      RegistrationFetcher::CreateFetcher(
          param, session_service(), unexportable_key_service(), context_.get(),
          IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
          /*net_log_source=*/std::nullopt,
          /*original_request_initiator=*/std::nullopt);
  fetcher->StartCreateTokenAndFetch(param, CreateAlgArray(),
                                    callback.callback());
  callback.WaitForCall();
  const RegistrationResult& out_session = callback.outcome();
  EXPECT_EQ(out_session.SessionErrorForTesting().type,
            SessionError::kSubdomainRegistrationWellKnownMalformed);
}

TEST_F(RegistrationTest, RegistrationBySubdomain_WellKnownMalformedEntry) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;

  server_.RegisterRequestHandler(base::BindRepeating(
      &ReturnForHostAndPath, "a.test", "/.well-known/device-bound-sessions",
      base::BindRepeating(&ReturnResponse, HTTP_OK,
                          "{\"registering_origins\": [ 12345 ]}")));
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kBasicValidJson));
  ASSERT_TRUE(server_.Start());

  GURL registration_url = server_.GetURL("subdomain.a.test", "/");

  RecordingNetLogObserver net_log_observer;
  TestRegistrationCallback callback;

  auto param = GetBasicParam(registration_url);
  std::unique_ptr<RegistrationFetcher> fetcher =
      RegistrationFetcher::CreateFetcher(
          param, session_service(), unexportable_key_service(), context_.get(),
          IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
          /*net_log_source=*/std::nullopt,
          /*original_request_initiator=*/std::nullopt);
  fetcher->StartCreateTokenAndFetch(param, CreateAlgArray(),
                                    callback.callback());
  callback.WaitForCall();
  const RegistrationResult& out_session = callback.outcome();
  EXPECT_EQ(out_session.SessionErrorForTesting().type,
            SessionError::kSubdomainRegistrationWellKnownMalformed);
}

TEST_F(RegistrationTest, RegistrationBySubdomain_Unauthorized) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;

  server_.RegisterRequestHandler(base::BindRepeating(
      &ReturnForHostAndPath, "a.test", "/.well-known/device-bound-sessions",
      base::BindRepeating(&ReturnWellKnown,
                          R"json({
                            "registering_origins": [ "https://subdomain.a.test:$1" ]
                          })json")));
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kBasicValidJson));
  ASSERT_TRUE(server_.Start());

  GURL registration_url = server_.GetURL("not-allowed-subdomain.a.test", "/");

  RecordingNetLogObserver net_log_observer;
  TestRegistrationCallback callback;

  auto param = GetBasicParam(registration_url);
  std::unique_ptr<RegistrationFetcher> fetcher =
      RegistrationFetcher::CreateFetcher(
          param, session_service(), unexportable_key_service(), context_.get(),
          IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
          /*net_log_source=*/std::nullopt,
          /*original_request_initiator=*/std::nullopt);
  fetcher->StartCreateTokenAndFetch(param, CreateAlgArray(),
                                    callback.callback());
  callback.WaitForCall();
  const RegistrationResult& out_session = callback.outcome();
  EXPECT_EQ(out_session.SessionErrorForTesting().type,
            SessionError::kSubdomainRegistrationUnauthorized);
}

TEST_F(RegistrationTest, RegistrationBySubdomain_MultipleAllowed) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;

  server_.RegisterRequestHandler(base::BindRepeating(
      &ReturnForHostAndPath, "a.test", "/.well-known/device-bound-sessions",
      base::BindRepeating(&ReturnWellKnown,
                          R"json({
                            "registering_origins": [
                              "https://subdomain.a.test:$1",
                              "https://other-subdomain.a.test:$1"
                            ]
                          })json")));
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kBasicValidJson));
  ASSERT_TRUE(server_.Start());

  GURL registration_url = server_.GetURL("subdomain.a.test", "/");
  auto param = GetBasicParam(registration_url);

  {
    TestRegistrationCallback callback;
    std::unique_ptr<RegistrationFetcher> fetcher =
        RegistrationFetcher::CreateFetcher(
            param, session_service(), unexportable_key_service(),
            context_.get(),
            IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
            /*net_log_source=*/std::nullopt,
            /*original_request_initiator=*/std::nullopt);
    fetcher->StartCreateTokenAndFetch(param, CreateAlgArray(),
                                      callback.callback());

    callback.WaitForCall();
    const RegistrationResult& out_session = callback.outcome();
    out_session.SessionForTesting();
  }

  {
    TestRegistrationCallback callback;
    registration_url = server_.GetURL("other-subdomain.a.test", "/");

    param = GetBasicParam(registration_url);

    std::unique_ptr<RegistrationFetcher> fetcher =
        RegistrationFetcher::CreateFetcher(
            param, session_service(), unexportable_key_service(),
            context_.get(),
            IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
            /*net_log_source=*/std::nullopt,
            /*original_request_initiator=*/std::nullopt);
    fetcher->StartCreateTokenAndFetch(param, CreateAlgArray(),
                                      callback.callback());
    callback.WaitForCall();
    callback.outcome().SessionForTesting();
  }
}

TEST_F(RegistrationTest, FederatedSuccess) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;

  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnForHostAndPath, "provider.a.test",
                          "/.well-known/device-bound-sessions",
                          base::BindRepeating(&ReturnWellKnown,
                                              R"json({
                                                "relying_origins": [ "https://rp.a.test:$1" ]
                                              })json")));
  server_.RegisterRequestHandler(base::BindRepeating(
      &ReturnForHostAndPath, "rp.a.test", "/.well-known/device-bound-sessions",
      base::BindRepeating(&ReturnWellKnown,
                          R"json({
                            "provider_origin": "https://provider.a.test:$1"
                          })json")));
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kBasicValidJson));
  ASSERT_TRUE(server_.Start());

  unexportable_keys::UnexportableKeyId key = CreateKey();
  auto param = RegistrationRequestParam::CreateForTesting(
      server_.GetURL("rp.a.test", "/"), kSessionIdentifier, kChallenge);
  auto session_or_error =
      FetchWithFederatedKey(param, key, server_.GetURL("provider.a.test", "/"));
  EXPECT_EQ(session_or_error.SessionForTesting().unexportable_key_id(), key);
}

TEST_F(RegistrationTest, FederatedProviderHasProvider) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;

  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnForHostAndPath, "provider.a.test",
                          "/.well-known/device-bound-sessions",
                          base::BindRepeating(&ReturnWellKnown,
                                              R"json({
                                                "provider_origin": "https://provider-provider.a.test:$1",
                                                "relying_origins": [ "https://rp.a.test:$1" ]
                                              })json")));
  server_.RegisterRequestHandler(base::BindRepeating(
      &ReturnForHostAndPath, "rp.a.test", "/.well-known/device-bound-sessions",
      base::BindRepeating(&ReturnWellKnown,
                          R"json({
                            "provider_origin": "https://provider.a.test:$1",
                          })json")));
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kBasicValidJson));
  ASSERT_TRUE(server_.Start());

  unexportable_keys::UnexportableKeyId key = CreateKey();
  auto param = RegistrationRequestParam::CreateForTesting(
      server_.GetURL("rp.a.test", "/"), kSessionIdentifier, kChallenge);
  auto session_or_error =
      FetchWithFederatedKey(param, key, server_.GetURL("provider.a.test", "/"));

  EXPECT_EQ(session_or_error.SessionErrorForTesting().type,
            SessionError::kSessionProviderWellKnownHasProviderOrigin);
}

TEST_F(RegistrationTest, FederatedProviderUnvailable) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;

  server_.RegisterRequestHandler(base::BindRepeating(
      &ReturnForHostAndPath, "provider.a.test",
      "/.well-known/device-bound-sessions",
      base::BindRepeating(&ReturnResponse, HTTP_BAD_REQUEST, "")));
  server_.RegisterRequestHandler(base::BindRepeating(
      &ReturnForHostAndPath, "rp.a.test", "/.well-known/device-bound-sessions",
      base::BindRepeating(&ReturnWellKnown,
                          R"json({
                            "provider_origin": "https://provider.a.test:$1",
                          })json")));
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kBasicValidJson));
  ASSERT_TRUE(server_.Start());

  unexportable_keys::UnexportableKeyId key = CreateKey();
  auto param = RegistrationRequestParam::CreateForTesting(
      server_.GetURL("rp.a.test", "/"), kSessionIdentifier, kChallenge);
  auto session_or_error =
      FetchWithFederatedKey(param, key, server_.GetURL("provider.a.test", "/"));

  EXPECT_EQ(session_or_error.SessionErrorForTesting().type,
            SessionError::kSessionProviderWellKnownUnavailable);
}

TEST_F(RegistrationTest, FederatedProviderUnauthorized) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;

  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnForHostAndPath, "provider.a.test",
                          "/.well-known/device-bound-sessions",
                          base::BindRepeating(&ReturnWellKnown,
                                              R"json({
                                                "relying_origins": [ "https://other-rp.a.test:$1" ]
                                              })json")));
  server_.RegisterRequestHandler(base::BindRepeating(
      &ReturnForHostAndPath, "rp.a.test", "/.well-known/device-bound-sessions",
      base::BindRepeating(&ReturnWellKnown,
                          R"json({
                            "provider_origin": "https://provider.a.test:$1"
                          })json")));
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kBasicValidJson));
  ASSERT_TRUE(server_.Start());

  unexportable_keys::UnexportableKeyId key = CreateKey();
  auto param = RegistrationRequestParam::CreateForTesting(
      server_.GetURL("rp.a.test", "/"), kSessionIdentifier, kChallenge);
  auto session_or_error =
      FetchWithFederatedKey(param, key, server_.GetURL("provider.a.test", "/"));

  EXPECT_EQ(session_or_error.SessionErrorForTesting().type,
            SessionError::kFederatedNotAuthorizedByProvider);
}

TEST_F(RegistrationTest, FederatedRelyingUnavailable) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;

  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnForHostAndPath, "provider.a.test",
                          "/.well-known/device-bound-sessions",
                          base::BindRepeating(&ReturnWellKnown,
                                              R"json({
                                                "relying_origins": [ "https://rp.a.test:$1" ]
                                              })json")));
  server_.RegisterRequestHandler(base::BindRepeating(
      &ReturnForHostAndPath, "rp.a.test", "/.well-known/device-bound-sessions",
      base::BindRepeating(&ReturnResponse, HTTP_BAD_REQUEST, "")));
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kBasicValidJson));
  ASSERT_TRUE(server_.Start());

  unexportable_keys::UnexportableKeyId key = CreateKey();
  auto param = RegistrationRequestParam::CreateForTesting(
      server_.GetURL("rp.a.test", "/"), kSessionIdentifier, kChallenge);
  auto session_or_error =
      FetchWithFederatedKey(param, key, server_.GetURL("provider.a.test", "/"));

  EXPECT_EQ(session_or_error.SessionErrorForTesting().type,
            SessionError::kRelyingPartyWellKnownUnavailable);
}

TEST_F(RegistrationTest, FederatedRelyingHasRelying) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;

  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnForHostAndPath, "provider.a.test",
                          "/.well-known/device-bound-sessions",
                          base::BindRepeating(&ReturnWellKnown,
                                              R"json({
                                                "relying_origins": [ "https://rp.a.test:$1" ]
                                              })json")));
  server_.RegisterRequestHandler(base::BindRepeating(
      &ReturnForHostAndPath, "rp.a.test", "/.well-known/device-bound-sessions",
      base::BindRepeating(&ReturnWellKnown,
                          R"json({
                            "provider_origin": "https://provider.a.test:$1",
                            "relying_origins": [ "https://rp-rp.a.test:$1" ]
                          })json")));
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kBasicValidJson));
  ASSERT_TRUE(server_.Start());

  unexportable_keys::UnexportableKeyId key = CreateKey();
  auto param = RegistrationRequestParam::CreateForTesting(
      server_.GetURL("rp.a.test", "/"), kSessionIdentifier, kChallenge);
  auto session_or_error =
      FetchWithFederatedKey(param, key, server_.GetURL("provider.a.test", "/"));

  EXPECT_EQ(session_or_error.SessionErrorForTesting().type,
            SessionError::kRelyingPartyWellKnownHasRelyingOrigins);
}

TEST_F(RegistrationTest, FederatedRelyingNotAuthorized) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;

  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnForHostAndPath, "provider.a.test",
                          "/.well-known/device-bound-sessions",
                          base::BindRepeating(&ReturnWellKnown,
                                              R"json({
                                                "relying_origins": [ "https://rp.a.test:$1" ]
                                              })json")));
  server_.RegisterRequestHandler(base::BindRepeating(
      &ReturnForHostAndPath, "rp.a.test", "/.well-known/device-bound-sessions",
      base::BindRepeating(&ReturnWellKnown,
                          R"json({
                            "provider_origin": "https://other-provider.a.test:$1"
                          })json")));
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kBasicValidJson));
  ASSERT_TRUE(server_.Start());

  unexportable_keys::UnexportableKeyId key = CreateKey();
  auto param = RegistrationRequestParam::CreateForTesting(
      server_.GetURL("rp.a.test", "/"), kSessionIdentifier, kChallenge);
  auto session_or_error =
      FetchWithFederatedKey(param, key, server_.GetURL("provider.a.test", "/"));

  EXPECT_EQ(session_or_error.SessionErrorForTesting().type,
            SessionError::kFederatedNotAuthorizedByRelyingParty);
}

TEST_F(RegistrationTest, FederatedTooManyRelying) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;

  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnForHostAndPath, "provider.a.test",
                          "/.well-known/device-bound-sessions",
                          base::BindRepeating(&ReturnWellKnown,
                                              R"json({
                                                "relying_origins": [
                                                  "https://rp.b1.test:$1",
                                                  "https://rp.b2.test:$1",
                                                  "https://rp.b3.test:$1",
                                                  "https://rp.b4.test:$1",
                                                  "https://rp.b5.test:$1",
                                                  "https://rp.a.test:$1"
                                                ]
                                              })json")));
  server_.RegisterRequestHandler(base::BindRepeating(
      &ReturnForHostAndPath, "rp.a.test", "/.well-known/device-bound-sessions",
      base::BindRepeating(&ReturnWellKnown,
                          R"json({
                            "provider_origin": "https://provider.a.test:$1"
                          })json")));
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kBasicValidJson));
  ASSERT_TRUE(server_.Start());

  unexportable_keys::UnexportableKeyId key = CreateKey();
  auto param = RegistrationRequestParam::CreateForTesting(
      server_.GetURL("rp.a.test", "/"), kSessionIdentifier, kChallenge);
  auto session_or_error =
      FetchWithFederatedKey(param, key, server_.GetURL("provider.a.test", "/"));
  EXPECT_EQ(session_or_error.SessionErrorForTesting().type,
            SessionError::kTooManyRelyingOriginLabels);
}

TEST_F(RegistrationTest, FederatedTooManyRelyingFirstLabelAllowed) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;

  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnForHostAndPath, "provider.a.test",
                          "/.well-known/device-bound-sessions",
                          base::BindRepeating(&ReturnWellKnown,
                                              R"json({
                                                "relying_origins": [
                                                  "https://a-is-allowed-because-its-first.a.test:$1",
                                                  "https://rp.b1.test:$1",
                                                  "https://rp.b2.test:$1",
                                                  "https://rp.b3.test:$1",
                                                  "https://rp.b4.test:$1",
                                                  "https://rp.b5.test:$1",
                                                  "https://rp.a.test:$1"
                                                ]
                                              })json")));
  server_.RegisterRequestHandler(base::BindRepeating(
      &ReturnForHostAndPath, "rp.a.test", "/.well-known/device-bound-sessions",
      base::BindRepeating(&ReturnWellKnown,
                          R"json({
                            "provider_origin": "https://provider.a.test:$1"
                          })json")));
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kBasicValidJson));
  ASSERT_TRUE(server_.Start());

  unexportable_keys::UnexportableKeyId key = CreateKey();
  auto param = RegistrationRequestParam::CreateForTesting(
      server_.GetURL("rp.a.test", "/"), kSessionIdentifier, kChallenge);
  auto session_or_error =
      FetchWithFederatedKey(param, key, server_.GetURL("provider.a.test", "/"));
  EXPECT_EQ(session_or_error.SessionForTesting().unexportable_key_id(), key);
}

TEST_F(RegistrationTest, FederatedNotRegistrableDoesNotCount) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;

  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnForHostAndPath, "provider.a.test",
                          "/.well-known/device-bound-sessions",
                          base::BindRepeating(&ReturnWellKnown,
                                              R"json({
                                                "relying_origins": [
                                                  "https://tld",
                                                  "http://?not-a=url",
                                                  "http:///path",
                                                  "http:///path2",
                                                  "http:///path3",
                                                  "https://rp.a.test:$1"
                                                ]
                                              })json")));
  server_.RegisterRequestHandler(base::BindRepeating(
      &ReturnForHostAndPath, "rp.a.test", "/.well-known/device-bound-sessions",
      base::BindRepeating(&ReturnWellKnown,
                          R"json({
                            "provider_origin": "https://provider.a.test:$1"
                          })json")));
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kBasicValidJson));
  ASSERT_TRUE(server_.Start());

  unexportable_keys::UnexportableKeyId key = CreateKey();
  auto param = RegistrationRequestParam::CreateForTesting(
      server_.GetURL("rp.a.test", "/"), kSessionIdentifier, kChallenge);
  auto session_or_error =
      FetchWithFederatedKey(param, key, server_.GetURL("provider.a.test", "/"));
  EXPECT_EQ(session_or_error.SessionForTesting().unexportable_key_id(), key);
}

TEST_F(RegistrationTest, RegistrationFailsIfCantSetCookies) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;

  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kBasicValidJson));
  ASSERT_TRUE(server_.Start());

  GURL registration_url = server_.GetURL("a.test", "/");

  RecordingNetLogObserver net_log_observer;
  TestRegistrationCallback callback;

  auto param = GetBasicParam(registration_url);
  std::unique_ptr<RegistrationFetcher> fetcher =
      RegistrationFetcher::CreateFetcher(
          param, session_service(), unexportable_key_service(), context_.get(),
          IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
          /*net_log_source=*/std::nullopt,
          /*original_request_initiator=*/std::nullopt);

  network_delegate()->set_cookie_options(TestNetworkDelegate::NO_SET_COOKIE);

  fetcher->StartCreateTokenAndFetch(param, CreateAlgArray(),
                                    callback.callback());
  callback.WaitForCall();
  const RegistrationResult& out_session = callback.outcome();
  EXPECT_EQ(out_session.SessionErrorForTesting().type,
            SessionError::kBoundCookieSetForbidden);
}

class RegistrationTokenHelperTest : public testing::Test {
 public:
  unexportable_keys::UnexportableKeyService& unexportable_key_service() {
    return unexportable_key_service_;
  }

  void RunBackgroundTasks() { task_environment_.RunUntilIdle(); }

 private:
  base::test::TaskEnvironment task_environment_{
      // QUEUED - tasks don't run until `RunUntilIdle()` is called.
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED};
  unexportable_keys::UnexportableKeyTaskManager task_manager_;
  unexportable_keys::UnexportableKeyServiceImpl unexportable_key_service_{
      task_manager_, crypto::UnexportableKeyProvider::Config()};
};

TEST_F(RegistrationTokenHelperTest, CreateSuccess) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  base::test::TestFuture<std::optional<RegistrationFetcher::RegistrationToken>>
      future;
  RegistrationFetcher::CreateRegistrationTokenAsyncForTesting(
      unexportable_key_service(), "test_challenge",
      /*authorization=*/std::nullopt, future.GetCallback());
  RunBackgroundTasks();
  ASSERT_TRUE(future.Get().has_value());
}

TEST_F(RegistrationTokenHelperTest, CreateFail) {
  crypto::ScopedNullUnexportableKeyProvider scoped_null_key_provider;
  base::test::TestFuture<std::optional<RegistrationFetcher::RegistrationToken>>
      future;
  RegistrationFetcher::CreateRegistrationTokenAsyncForTesting(
      unexportable_key_service(), "test_challenge",
      /*authorization=*/std::nullopt, future.GetCallback());
  RunBackgroundTasks();
  EXPECT_FALSE(future.Get().has_value());
}

}  // namespace

}  // namespace net::device_bound_sessions
