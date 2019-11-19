// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/strings/strcat.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/reporting/reporting_policy.h"
#include "net/reporting/reporting_service.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/url_request/url_request_context.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "services/network/origin_policy/origin_policy_fetcher.h"
#include "services/network/origin_policy/origin_policy_manager.h"
#include "services/network/origin_policy/origin_policy_parser.h"
#include "services/network/public/cpp/origin_policy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace network {

namespace {

void DummyRetrieveOriginPolicyCallback(const network::OriginPolicy& result) {}
}  // namespace

class OriginPolicyManagerTest : public testing::Test {
 public:
  OriginPolicyManagerTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {
    network_service_ = NetworkService::CreateForTesting();

    auto context_params = mojom::NetworkContextParams::New();
    // Use a fixed proxy config, to avoid dependencies on local network
    // configuration.
    context_params->initial_proxy_config =
        net::ProxyConfigWithAnnotation::CreateDirect();
    network_context_ = std::make_unique<NetworkContext>(
        network_service_.get(),
        network_context_remote_.BindNewPipeAndPassReceiver(),
        std::move(context_params));
    manager_ = std::make_unique<OriginPolicyManager>(network_context_.get());

    test_server_.RegisterRequestHandler(base::BindRepeating(
        &OriginPolicyManagerTest::HandleResponse, base::Unretained(this)));

    EXPECT_TRUE(test_server_.Start());

    test_server_origin_ = url::Origin::Create(test_server_.base_url());

    test_server_2_.RegisterRequestHandler(base::BindRepeating(
        &OriginPolicyManagerTest::HandleResponse, base::Unretained(this)));

    EXPECT_TRUE(test_server_2_.Start());

    test_server_origin_2_ = url::Origin::Create(test_server_2_.base_url());
  }

  const url::Origin& test_server_origin() const { return test_server_origin_; }
  const url::Origin& test_server_origin_2() const {
    return test_server_origin_2_;
  }

  OriginPolicyManager* manager() { return manager_.get(); }

  NetworkContext* network_context() { return network_context_.get(); }

  void WaitUntilResponseHandled() { response_run_loop.Run(); }

 protected:
  std::unique_ptr<net::test_server::HttpResponse> HandleResponse(
      const net::test_server::HttpRequest& request) {
    response_run_loop.Quit();
    std::unique_ptr<net::test_server::BasicHttpResponse> response =
        std::make_unique<net::test_server::BasicHttpResponse>();

    if (request.relative_url == "/.well-known/origin-policy") {
      response->set_code(net::HTTP_FOUND);
      response->AddCustomHeader("Location",
                                "/.well-known/origin-policy/policy-1");
    } else if (request.relative_url == "/.well-known/origin-policy/policy-1") {
      response->set_code(net::HTTP_OK);
      response->set_content(
          R"({ "feature-policy": ["geolocation http://example1.com"] })");
    } else if (request.relative_url == "/.well-known/origin-policy/policy-2") {
      response->set_code(net::HTTP_OK);
      response->set_content(
          R"({ "feature-policy": ["geolocation http://example2.com"] })");
    } else if (request.relative_url ==
               "/.well-known/origin-policy/redirect-policy") {
      response->set_code(net::HTTP_FOUND);
      response->AddCustomHeader(
          "Location",
          test_server_.GetURL("/.well-known/origin-policy/policy-1").spec());
    } else if (request.relative_url ==
               "/.well-known/origin-policy/policy/policy-3") {
      response->set_code(net::HTTP_OK);
      response->set_content(
          R"({ "feature-policy": ["geolocation http://example3.com"] })");
    } else if (request.relative_url == "/.well-known/delayed") {
      return std::make_unique<net::test_server::HungResponse>();
    } else {
      response->set_code(net::HTTP_NOT_FOUND);
    }

    return std::move(response);
  }

  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<NetworkService> network_service_;
  std::unique_ptr<NetworkContext> network_context_;
  mojo::Remote<mojom::NetworkContext> network_context_remote_;
  std::unique_ptr<OriginPolicyManager> manager_;
  base::RunLoop response_run_loop;
  net::test_server::EmbeddedTestServer test_server_;
  net::test_server::EmbeddedTestServer test_server_2_;
  url::Origin test_server_origin_;
  url::Origin test_server_origin_2_;

  DISALLOW_COPY_AND_ASSIGN(OriginPolicyManagerTest);
};

TEST_F(OriginPolicyManagerTest, AddBinding) {
  mojo::Remote<mojom::OriginPolicyManager> origin_policy_remote;
  EXPECT_EQ(0u, manager()->GetReceiversForTesting().size());

  manager()->AddReceiver(origin_policy_remote.BindNewPipeAndPassReceiver());

  EXPECT_EQ(1u, manager()->GetReceiversForTesting().size());
}

TEST_F(OriginPolicyManagerTest, ParseHeaders) {
  const std::string kExemptedOriginPolicyVersion =
      OriginPolicyManager::GetExemptedVersionForTesting();
  const struct {
    const std::string header;
    const char* expected_policy_version;
    const char* expected_report_to;
  } kTests[] = {
      // The common cases: We expect >99% of headers to look like these:
      {"policy=policy", "policy", ""},
      {"policy=policy, report-to=endpoint", "policy", "endpoint"},
      {"", "", ""},
      {"report-to=endpoint", "", ""},

      // Delete a policy. This better work.
      {"0", "0", ""},
      {"policy=0", "0", ""},
      {"policy=\"0\"", "0", ""},
      {"policy=0, report-to=endpoint", "0", "endpoint"},

      // Order, please!
      {"policy=policy, report-to=endpoint", "policy", "endpoint"},
      {"report-to=endpoint, policy=policy", "policy", "endpoint"},

      // Quoting:
      {"policy=\"policy\"", "policy", ""},
      {"policy=\"policy\", report-to=endpoint", "policy", "endpoint"},
      {"policy=\"policy\", report-to=\"endpoint\"", "policy", "endpoint"},
      {"policy=policy, report-to=\"endpoint\"", "policy", "endpoint"},

      // Whitespace, and funky but valid syntax:
      {"  policy  =   policy  ", "policy", ""},
      {" policy = \t policy ", "policy", ""},
      {" policy \t= \t \"policy\"  ", "policy", ""},
      {" policy = \" policy \" ", "policy", ""},
      {" , policy = policy , report-to=endpoint , ", "policy", "endpoint"},

      // Valid policy, invalid report-to:
      {"policy=policy, report-to endpoint", "", ""},
      {"policy=policy, report-to=here, report-to=there", "", ""},
      {"policy=policy, \"report-to\"=endpoint", "", ""},

      // Invalid policy, valid report-to:
      {"policy=policy1, policy=policy2", "", ""},
      {"policy, report-to=r", "", ""},

      // Invalid everything:
      {"one two three", "", ""},
      {"one, two, three", "", ""},
      {"policy report-to=endpoint", "", ""},
      {"policy=policy report-to=endpoint", "", ""},

      // Forward compatibility, ignore unknown keywords:
      {"policy=pol, report-to=endpoint, unknown=keyword", "pol", "endpoint"},
      {"unknown=keyword, policy=pol, report-to=endpoint", "pol", "endpoint"},
      {"policy=pol, unknown=keyword", "pol", ""},
      {"policy=policy, report_to=endpoint", "policy", ""},
      {"policy=policy, reportto=endpoint", "policy", ""},

      // Using relative paths
      {"policy=..", "", ""},
      {"policy=../some-policy", "", ""},
      {"policy=policy/../some-policy", "", ""},
      {"policy=.., report-to=endpoint", "", ""},
      {"report-to=endpoint, policy=..", "", ""},
      {"policy=. .", "", ""},
      {"policy= ..", "", ""},
      {"policy=.. ", "", ""},
      {"policy=.", "", ""},
      {"policy=policy/.", "", ""},
      {"policy=./some-policy", "", ""},

      // Repeated arguments
      {"policy=p1, policy=p2", "", ""},
      {"policy=, policy=p2", "", ""},
      {"report-to=r1, report-to=r2", "", ""},
      {"report-to=, report-to=r2", "", ""},
      {"policy=, policy=p2, report-to=r1", "", ""},
      {"policy=, policy=, report-to=r1", "", ""},
      {"policy=p1, report-to=r1, report-to=r2", "", ""},

      // kExemptedOriginPolicyVersion is not a valid version
      {base::StrCat({"policy=", kExemptedOriginPolicyVersion}), "", ""},
      {base::StrCat({"report-to=r, policy=", kExemptedOriginPolicyVersion}), "",
       ""},
      {base::StrCat({"policy=", kExemptedOriginPolicyVersion, ", report-to=r"}),
       "", ""},
  };
  for (const auto& test : kTests) {
    SCOPED_TRACE(test.header);
    OriginPolicyHeaderValues result = OriginPolicyManager::
        GetRequestedPolicyAndReportGroupFromHeaderStringForTesting(test.header);
    EXPECT_EQ(test.expected_policy_version, result.policy_version);
    EXPECT_EQ(test.expected_report_to, result.report_to);
  }

  EXPECT_FALSE(net::HttpUtil::IsToken(kExemptedOriginPolicyVersion));
}

// Helper class for starting saving a retrieved policy result
class TestOriginPolicyManagerResult {
 public:
  TestOriginPolicyManagerResult(OriginPolicyManagerTest* fixture,
                                OriginPolicyManager* manager = nullptr)
      : fixture_(fixture), manager_(manager ? manager : fixture->manager()) {}

  void RetrieveOriginPolicy(const std::string& header_value,
                            const url::Origin* origin = nullptr) {
    manager_->RetrieveOriginPolicy(
        origin ? *origin : fixture_->test_server_origin(), header_value,
        base::BindOnce(&TestOriginPolicyManagerResult::Callback,
                       base::Unretained(this)));
    run_loop_.Run();
  }

  const OriginPolicy* origin_policy_result() const {
    return origin_policy_result_.get();
  }

 private:
  void Callback(const OriginPolicy& result) {
    origin_policy_result_ = std::make_unique<OriginPolicy>(result);
    run_loop_.Quit();
  }

  base::RunLoop run_loop_;
  OriginPolicyManagerTest* fixture_;
  OriginPolicyManager* manager_;
  std::unique_ptr<OriginPolicy> origin_policy_result_;

  DISALLOW_COPY_AND_ASSIGN(TestOriginPolicyManagerResult);
};

TEST_F(OriginPolicyManagerTest, EndToEndPolicyRetrieve) {
  const struct {
    std::string header;
    OriginPolicyState expected_state;
    std::string expected_raw_policy;
  } kTests[] = {
      {"policy=policy-1", OriginPolicyState::kLoaded,
       R"({ "feature-policy": ["geolocation http://example1.com"] })"},
      {"policy=policy-2", OriginPolicyState::kLoaded,
       R"({ "feature-policy": ["geolocation http://example2.com"] })"},
      {"policy=redirect-policy", OriginPolicyState::kInvalidRedirect, ""},
      {"policy=policy-2, report-to=endpoint", OriginPolicyState::kLoaded,
       R"({ "feature-policy": ["geolocation http://example2.com"] })"},

      {"", OriginPolicyState::kNoPolicyApplies, ""},
      {"unknown=keyword", OriginPolicyState::kCannotLoadPolicy, ""},
      {"report_to=endpoint", OriginPolicyState::kCannotLoadPolicy, ""},
      {"policy=policy/policy-3", OriginPolicyState::kCannotLoadPolicy, ""},

      {"policy=../some-policy", OriginPolicyState::kCannotLoadPolicy, ""},
      {"policy=..", OriginPolicyState::kCannotLoadPolicy, ""},
      {"policy=.", OriginPolicyState::kCannotLoadPolicy, ""},
      {"policy=something-else/..", OriginPolicyState::kCannotLoadPolicy, ""},
  };

  for (const auto& test : kTests) {
    SCOPED_TRACE(test.header);

    OriginPolicyManager manager(network_context());

    TestOriginPolicyManagerResult tester(this, &manager);
    tester.RetrieveOriginPolicy(test.header);
    EXPECT_EQ(test.expected_state, tester.origin_policy_result()->state);
    if (test.expected_raw_policy.empty()) {
      EXPECT_FALSE(tester.origin_policy_result()->contents);
    } else {
      OriginPolicyContentsPtr expected_origin_policy_contents =
          OriginPolicyParser::Parse(test.expected_raw_policy);
      EXPECT_EQ(expected_origin_policy_contents,
                tester.origin_policy_result()->contents);
    }
  }
}

// Destroying un-invoked OnceCallback while the binding they are on is still
// live results in a DCHECK. This tests this scenario by destroying the manager
// while it's in the middle of fetching a policy that has a delay on the
// response. The manager will be destroyed before the return callback is called.
TEST_F(OriginPolicyManagerTest, DestroyWhileCallbackUninvoked) {
  {
    mojo::Remote<mojom::OriginPolicyManager> origin_policy_remote;

    OriginPolicyManager manager(network_context());

    manager.AddReceiver(origin_policy_remote.BindNewPipeAndPassReceiver());

    // This fetch will still be ongoing when the manager is destroyed.
    origin_policy_remote->RetrieveOriginPolicy(
        test_server_origin(), "policy=delayed",
        base::BindOnce(&DummyRetrieveOriginPolicyCallback));

    WaitUntilResponseHandled();
  }
  // At this point if we have not hit the DCHECK in OnIsConnectedComplete, then
  // the test has passed.
}

TEST_F(OriginPolicyManagerTest, CacheStatesAfterPolicyFetches) {
  const struct {
    std::string header;
    OriginPolicyState expected_state;
    std::string expected_raw_policy;
    const url::Origin& origin;
    bool add_exception_first = false;
  } kTests[] = {
      // The order of these tests is important as the cache is not cleared in
      // between tests and some tests rely on the state left over by previous
      // tests.

      // Nothing in the cache, no policy applies if header unspecified.
      {"", OriginPolicyState::kNoPolicyApplies, "", test_server_origin()},

      // An invalid header and nothing in the cache means an error.
      {"invalid", OriginPolicyState::kCannotLoadPolicy, "",
       test_server_origin()},

      // The invalid policy header has not been kept in the cache, an empty
      // policy still means no policy applies.
      {"", OriginPolicyState::kNoPolicyApplies, "", test_server_origin()},

      // A valid header results in loaded policy.
      {"policy=policy-1", OriginPolicyState::kLoaded,
       R"({ "feature-policy": ["geolocation http://example1.com"] })",
       test_server_origin()},

      // With a valid header, we use that version if header unspecified.
      {"", OriginPolicyState::kLoaded,
       R"({ "feature-policy": ["geolocation http://example1.com"] })",
       test_server_origin()},

      // A second valid header results in loaded policy. Changes cached last
      // version.
      {"policy=policy-2", OriginPolicyState::kLoaded,
       R"({ "feature-policy": ["geolocation http://example2.com"] })",
       test_server_origin()},

      // The latest version is correctly uses when header is unspecified.
      {"", OriginPolicyState::kLoaded,
       R"({ "feature-policy": ["geolocation http://example2.com"] })",
       test_server_origin()},

      // Same as above for invalid header.
      {"invalid", OriginPolicyState::kLoaded,
       R"({ "feature-policy": ["geolocation http://example2.com"] })",
       test_server_origin()},

      // Delete the policy.
      {base::StrCat({"policy=", kOriginPolicyDeletePolicy}),
       OriginPolicyState::kNoPolicyApplies, "", test_server_origin()},

      // We are the back to the initial status quo, no policy applies if header
      // unspecified.
      {"", OriginPolicyState::kNoPolicyApplies, "", test_server_origin()},

      // Load a new policy to have something in the cache.
      {"policy=policy-1", OriginPolicyState::kLoaded,
       R"({ "feature-policy": ["geolocation http://example1.com"] })",
       test_server_origin()},

      // Check that the version in the cache is used.
      {"", OriginPolicyState::kLoaded,
       R"({ "feature-policy": ["geolocation http://example1.com"] })",
       test_server_origin()},

      // In a different origin, it should not pick up the initial origin's
      // cached version.
      {"", OriginPolicyState::kNoPolicyApplies, "", test_server_origin_2()},

      // Load a new policy to have something in the cache for the second origin.
      {"policy=policy-2", OriginPolicyState::kLoaded,
       R"({ "feature-policy": ["geolocation http://example2.com"] })",
       test_server_origin_2()},

      // Check that the version in the cache is used for the second origin.
      {"", OriginPolicyState::kLoaded,
       R"({ "feature-policy": ["geolocation http://example2.com"] })",
       test_server_origin_2()},

      // The initial origins cached state is unaffected.
      {"", OriginPolicyState::kLoaded,
       R"({ "feature-policy": ["geolocation http://example1.com"] })",
       test_server_origin()},

      // Start testing exception logic.

      // Adding an exception means a kNoPolicyApplies state will be returned,
      // without attempting to retrieve a policy.
      {"policy=policy-1", OriginPolicyState::kNoPolicyApplies, "",
       test_server_origin(), true /* add_exception_first */},

      // And it will still be exempted in further calls, even if a valid policy
      // header is present.
      {"policy=policy-1", OriginPolicyState::kNoPolicyApplies, "",
       test_server_origin()},

      // And also if an invalid header is present.
      {"invalid", OriginPolicyState::kNoPolicyApplies, "",
       test_server_origin()},

      // This only affects the specified origin, a second origin should be
      // unaffected.
      {"policy=policy-2", OriginPolicyState::kLoaded,
       R"({ "feature-policy": ["geolocation http://example2.com"] })",
       test_server_origin_2()},

      // Adding an exception will work for the second origin as well.
      {"invalid", OriginPolicyState::kNoPolicyApplies, "",
       test_server_origin_2(), true /* add_exception_first */},

      // And future calls on the second origin will now return a
      // kNoPolicyApplies
      // state.
      {"invalid", OriginPolicyState::kNoPolicyApplies, "",
       test_server_origin_2()},

      // Deleting a policy will delete the exception (which will become apparent
      // in subsequent calls).
      {kOriginPolicyDeletePolicy, OriginPolicyState::kNoPolicyApplies, "",
       test_server_origin()},

      // Now attempting to load a policy will proceed as normal.
      {"policy=policy-1", OriginPolicyState::kLoaded,
       R"({ "feature-policy": ["geolocation http://example1.com"] })",
       test_server_origin()},

      // But the second origin is unaffected by the deletion and still exempted.
      {"invalid", OriginPolicyState::kNoPolicyApplies, "",
       test_server_origin_2()},
  };

  for (const auto& test : kTests) {
    if (test.add_exception_first)
      manager()->AddExceptionFor(test.origin);

    TestOriginPolicyManagerResult tester(this);
    tester.RetrieveOriginPolicy(test.header, &test.origin);
    EXPECT_EQ(test.expected_state, tester.origin_policy_result()->state);
    if (test.expected_raw_policy.empty()) {
      EXPECT_FALSE(tester.origin_policy_result()->contents);
    } else {
      OriginPolicyContentsPtr expected_origin_policy_contents =
          OriginPolicyParser::Parse(test.expected_raw_policy);
      EXPECT_EQ(expected_origin_policy_contents,
                tester.origin_policy_result()->contents);
    }
  }
}

#if BUILDFLAG(ENABLE_REPORTING)
class MockReportingService : public net::ReportingService {
 public:
  MOCK_METHOD6(QueueReport,
               void(const GURL&,
                    const std::string&,
                    const std::string&,
                    const std::string&,
                    std::unique_ptr<const base::Value>,
                    int));
  MOCK_METHOD2(ProcessHeader, void(const GURL&, const std::string&));
  MOCK_METHOD2(RemoveBrowsingData,
               void(int, const base::RepeatingCallback<bool(const GURL&)>&));
  MOCK_METHOD1(RemoveAllBrowsingData, void(int));
  MOCK_METHOD0(OnShutdown, void());
  MOCK_CONST_METHOD0(GetPolicy, const net::ReportingPolicy&());
  MOCK_CONST_METHOD0(StatusAsValue, base::Value());
  MOCK_CONST_METHOD0(GetContextForTesting, net::ReportingContext*());
};

MATCHER_P(ReportBodyEquals, expected, "") {
  if (!arg->is_dict() || !expected->is_dict() || arg->DictSize() != 3 ||
      expected->DictSize() != 3) {
    return false;
  }

  for (const std::string& key_name :
       {"origin_policy_url", "policy", "policy_error_reason"}) {
    if (!arg->FindStringKey(key_name) || !expected->FindStringKey(key_name) ||
        *arg->FindStringKey(key_name) != *expected->FindStringKey(key_name)) {
      return false;
    }
  }

  return true;
}

TEST_F(OriginPolicyManagerTest, TestMaybeReport) {
  struct ReportingTest {
    OriginPolicyState state;
    const OriginPolicyHeaderValues& header_info;
    const GURL& policy_url;
    const std::string expected_origin_policy_url = "";
    const std::string expected_policy = "";
    const std::string expected_policy_error_reason = "";
  };

  // These tests should cause the report to be queued.
  ReportingTest kReportingTests[] = {
      {OriginPolicyState::kCannotLoadPolicy,
       OriginPolicyHeaderValues({"version1", "report1", "raw1"}),
       GURL("http://example1.com/"), "http://example1.com/", "raw1",
       "CANNOT_LOAD"},
      {OriginPolicyState::kInvalidRedirect,
       OriginPolicyHeaderValues({"version2", "report2", "raw2"}),
       GURL("http://example2.com/"), "http://example2.com/", "raw2",
       "REDIRECT"},
      {OriginPolicyState::kOther,
       OriginPolicyHeaderValues({"version3", "report3", "raw3"}),
       GURL("http://example3.com/"), "http://example3.com/", "raw3", "OTHER"},
  };

  // These tests should trigger a dcheck.
  ReportingTest kDheckTests[] = {
      {OriginPolicyState::kLoaded,
       OriginPolicyHeaderValues({"version1", "report1", "raw1"}),
       GURL("http://example1.com/")},
      {OriginPolicyState::kNoPolicyApplies,
       OriginPolicyHeaderValues({"version2", "report2", "raw2"}),
       GURL("http://example2.com/")},
  };

  // These tests should return without queueing a report.
  ReportingTest kReturningTests[] = {
      {OriginPolicyState::kInvalidRedirect,
       OriginPolicyHeaderValues({"", "", ""}), GURL("http://example1.com/")},
      {OriginPolicyState::kLoaded,
       OriginPolicyHeaderValues({"version1", "", "raw1"}),
       GURL("http://example1.com/")},
      {OriginPolicyState::kCannotLoadPolicy,
       OriginPolicyHeaderValues({"version1", "", "raw1"}),
       GURL("http://example1.com/")},
      {OriginPolicyState::kOther,
       OriginPolicyHeaderValues({"version1", "", "raw1"}),
       GURL("http://example1.com/")},
      {OriginPolicyState::kInvalidRedirect,
       OriginPolicyHeaderValues({"version1", "", "raw1"}),
       GURL("http://example1.com/")},
      {OriginPolicyState::kNoPolicyApplies,
       OriginPolicyHeaderValues({"version1", "", "raw1"}),
       GURL("http://example1.com/")},
  };

  for (const auto& test : kReportingTests) {
    MockReportingService mock_service;
    network_context()->url_request_context()->set_reporting_service(
        &mock_service);

    base::DictionaryValue expected_report_body;
    expected_report_body.SetKey("origin_policy_url",
                                base::Value(test.expected_origin_policy_url));
    expected_report_body.SetKey("policy", base::Value(test.expected_policy));
    expected_report_body.SetKey("policy_error_reason",
                                base::Value(test.expected_policy_error_reason));

    EXPECT_CALL(
        mock_service,
        QueueReport(test.policy_url, _ /* user_agent */,
                    test.header_info.report_to, "origin-policy",
                    ReportBodyEquals(&expected_report_body), _ /* depth */))
        .Times(1);

    manager()->MaybeReport(test.state, test.header_info, test.policy_url);

    network_context()->url_request_context()->set_reporting_service(nullptr);
  }

  for (const auto& test : kDheckTests) {
    EXPECT_DCHECK_DEATH(
        manager()->MaybeReport(test.state, test.header_info, test.policy_url));
  }

  for (const auto& test : kReturningTests) {
    MockReportingService mock_service;
    network_context()->url_request_context()->set_reporting_service(
        &mock_service);

    EXPECT_CALL(mock_service, QueueReport(_, _, _, _, _, _)).Times(0);

    manager()->MaybeReport(test.state, test.header_info, test.policy_url);

    network_context()->url_request_context()->set_reporting_service(nullptr);
  }
}
#endif  // BUILDFLAG(ENABLE_REPORTING)

}  // namespace network
