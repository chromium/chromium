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
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "services/network/origin_policy/origin_policy_constants.h"
#include "services/network/origin_policy/origin_policy_fetcher.h"
#include "services/network/origin_policy/origin_policy_header_values.h"
#include "services/network/origin_policy/origin_policy_manager.h"
#include "services/network/origin_policy/origin_policy_parser.h"
#include "services/network/public/cpp/origin_policy.h"
#include "services/network/test/test_network_service_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {

void DummyRetrieveOriginPolicyCallback(const network::OriginPolicy& result) {}

}  // namespace

class OriginPolicyFetcherTest : public testing::Test {
 public:
  OriginPolicyFetcherTest()
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

    mojom::URLLoaderFactoryParamsPtr params =
        mojom::URLLoaderFactoryParams::New();
    params->process_id = mojom::kBrowserProcessId;
    params->is_corb_enabled = false;
    network_context_->CreateURLLoaderFactory(
        url_loader_factory_.BindNewPipeAndPassReceiver(), std::move(params));

    manager_ = std::make_unique<OriginPolicyManager>(network_context_.get());

    test_server_.RegisterRequestHandler(base::BindRepeating(
        &OriginPolicyFetcherTest::HandleResponse, base::Unretained(this)));

    EXPECT_TRUE(test_server_.Start());

    test_server_origin_ = url::Origin::Create(test_server_.base_url());
  }

  const url::Origin& test_server_origin() const { return test_server_origin_; }

  OriginPolicyManager* manager() { return manager_.get(); }

  mojom::URLLoaderFactory* url_loader_factory() {
    return url_loader_factory_.get();
  }

 protected:
  const net::test_server::EmbeddedTestServer& test_server() const {
    return test_server_;
  }

  void SetUpDefaultPolicyResponse(const std::string& manifest_body) {
    default_policy_response_is_redirect_ = false;
    default_policy_extra_info_ = manifest_body;
  }

  void SetUpDefaultPolicyRedirect(const std::string& location_path) {
    default_policy_response_is_redirect_ = true;
    default_policy_extra_info_ = test_server_.GetURL(location_path).spec();
  }

 private:
  // The test server will know how to respond to the following requests:
  // `/.well-known/origin-policy` => use the response that was setup via
  //            SetUpDefaultPolicyRedirect() or SetUpDefaultPolicyResponse()
  // `/.well-known/origin-policy/policy-1` => 200, body: R"({ "feature-policy":
  // ["geolocation http://example1.com"] })"
  // `/.well-known/origin-policy/policy-2` => 200, body: R"({ "feature-policy":
  // ["geolocation http://example2.com"] })"
  // `/.well-known/origin-policy/redirect-policy` => 302 redirect to policy-1
  std::unique_ptr<net::test_server::HttpResponse> HandleResponse(
      const net::test_server::HttpRequest& request) {
    std::unique_ptr<net::test_server::BasicHttpResponse> response =
        std::make_unique<net::test_server::BasicHttpResponse>();

    if (request.relative_url == "/.well-known/origin-policy") {
      if (default_policy_response_is_redirect_) {
        response->set_code(net::HTTP_FOUND);
        response->AddCustomHeader("Location", default_policy_extra_info_);
      } else {
        response->set_code(net::HTTP_OK);
        response->set_content(default_policy_extra_info_);
      }
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
    } else {
      response->set_code(net::HTTP_NOT_FOUND);
    }

    return std::move(response);
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<NetworkService> network_service_;
  std::unique_ptr<NetworkContext> network_context_;
  mojo::Remote<mojom::NetworkContext> network_context_remote_;
  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory_;

  net::test_server::EmbeddedTestServer test_server_;
  url::Origin test_server_origin_;

  std::unique_ptr<OriginPolicyManager> manager_;

  bool default_policy_response_is_redirect_ = false;
  std::string default_policy_extra_info_;
};

TEST_F(OriginPolicyFetcherTest, GetPolicyURL) {
  url::Origin example_origin = url::Origin::Create(GURL("http://example.com/"));

  EXPECT_EQ(GURL("http://example.com/.well-known/origin-policy"),
            OriginPolicyFetcher::GetDefaultPolicyURL(example_origin));

  EXPECT_EQ(GURL("http://example.com/.well-known/origin-policy/some-version"),
            OriginPolicyFetcher::GetPolicyURL("some-version", example_origin));
}

TEST_F(OriginPolicyFetcherTest, IsValidRedirect) {
  const struct {
    const std::string initial_version;
    const std::string redirect_path;
    const bool expected;
  } kTests[] = {
      // Default policy is only allowed to redirect to valid policy version.
      {"", "/.well-known/origin-policy/some-policy", true},
      {"", "/not-in-well-known/origin-policy/some-policy", false},
      {"", "/.well-known/origin-policy", false},
      {"", "/.well-known/a/b", false},
      {"", "/.well-known/origin-policy-foo", false},
      {"", "/.well-known/return-policy", false},
      {"", "/.well-known/origin-policy", false},

      // Specific version policy is not allowed to redirect to other policy.
      {"some-policy", "/.well-known/origin-policy/other-policy", false},
      {"some-policy", "/.well-known/origin-policy", false},
      {"some-policy", "/.well-known/origin-policy/some-policy", false},
      {"other-policy", "/.well-known/origin-policy/some-policy/other-policy",
       false},

      // Query strings/hashes not allowed.
      {"", "/.well-known/origin-policy/some-policy?param=value", false},
      {"", "/.well-known/origin-policy/some-policy?%20", false},
      {"", "/.well-known/origin-policy?some-policy", false},
      {"", "/.well-known/origin-policy?version=some-policy", false},
      {"", "/.well-known/origin-policy/some-policy?", false},
      {"", "/.well-known/origin-policy/some-policy#h1", false},
      {"", "/.well-known/origin-policy#h1", false},
      {"", "/.well-known/origin-policy#some-policy", false},
      {"", "/.well-known/origin-policy/some-policy#", false},
      {"", "/.well-known/origin-policy/some-policy?param=value#h1", false},
      {"", "/.well-known/origin-policy/some-policy?param=value#", false},
      {"", "/.well-known/origin-policy/some-policy?#h1", false},
      {"", "/.well-known/origin-policy/some-policy?#", false},
      {"", "/.well-known/origin-policy?some-policy#", false},
      {"", "/.well-known/origin-policy?#some-policy", false},
  };

  for (const auto& test : kTests) {
    SCOPED_TRACE(test.redirect_path);
    auto fetcher =
        test.initial_version.empty()
            ? std::make_unique<OriginPolicyFetcher>(
                  manager(), test_server_origin(), url_loader_factory(),
                  base::BindOnce(&DummyRetrieveOriginPolicyCallback))
            : std::make_unique<OriginPolicyFetcher>(
                  manager(),
                  OriginPolicyHeaderValues(
                      {.policy_version = test.initial_version}),
                  test_server_origin(), url_loader_factory(),
                  base::BindOnce(&DummyRetrieveOriginPolicyCallback));

    net::RedirectInfo redirect_info;
    redirect_info.new_url = test_server().GetURL(test.redirect_path);
    EXPECT_EQ(test.expected, fetcher->IsValidRedirectForTesting(redirect_info));
  }
}

// Helper class for starting a fetcher and saving the result
class TestOriginPolicyFetcherResult {
 public:
  TestOriginPolicyFetcherResult() {}

  void RetrieveOriginPolicy(const std::string& version,
                            OriginPolicyFetcherTest* fixture) {
    if (version.empty()) {
      fixture->manager()->RetrieveDefaultOriginPolicy(
          fixture->test_server_origin(),
          base::BindOnce(&TestOriginPolicyFetcherResult::Callback,
                         base::Unretained(this)));
    } else {
      fixture->manager()->RetrieveOriginPolicy(
          fixture->test_server_origin(), base::StrCat({"policy=", version}),
          base::BindOnce(&TestOriginPolicyFetcherResult::Callback,
                         base::Unretained(this)));
    }
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
  std::unique_ptr<OriginPolicyFetcher> fetcher_;
  std::unique_ptr<OriginPolicy> origin_policy_result_;

  DISALLOW_COPY_AND_ASSIGN(TestOriginPolicyFetcherResult);
};

TEST_F(OriginPolicyFetcherTest, EndToEnd) {
  const struct {
    const std::string version;
    const OriginPolicyState expected_state;
    const std::string expected_raw_policy;
  } kTests[] = {
      {"policy-1", OriginPolicyState::kLoaded,
       R"({ "feature-policy": ["geolocation http://example1.com"] })"},
      {"policy-2", OriginPolicyState::kLoaded,
       R"({ "feature-policy": ["geolocation http://example2.com"] })"},
      {"", OriginPolicyState::kLoaded,
       R"({ "feature-policy": ["geolocation http://example1.com"] })"},
      {"redirect-policy", OriginPolicyState::kInvalidRedirect, ""},
      {"404-version", OriginPolicyState::kCannotLoadPolicy, ""},
  };

  SetUpDefaultPolicyRedirect("/.well-known/origin-policy/policy-1");

  for (const auto& test : kTests) {
    SCOPED_TRACE(test.version);
    TestOriginPolicyFetcherResult tester;
    tester.RetrieveOriginPolicy(test.version, this);
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

TEST_F(OriginPolicyFetcherTest, EndToEndInvalidRedirects) {
  const struct {
    std::string default_redirect_location;
  } kTests[] = {
      {"/.well-known/origin-policy/redirect-policy"},
      {"/not-well-known/origin-policy/policy-1"},
      {"/.well-known/origin-policy-something-else"},
      {"/.well-known/origin-policy/policy-1/policy-2"},
      {"/.well-known/origin-policy/policy-1/policy-2/policy-3"},
      {"/.well-known/origin-policy/policy-1?foo=bar"},
  };

  for (const auto& test : kTests) {
    SCOPED_TRACE(test.default_redirect_location);
    SetUpDefaultPolicyRedirect(test.default_redirect_location);

    TestOriginPolicyFetcherResult tester;
    tester.RetrieveOriginPolicy("", this);
    EXPECT_EQ(OriginPolicyState::kInvalidRedirect,
              tester.origin_policy_result()->state);
    EXPECT_FALSE(tester.origin_policy_result()->contents);
  }
}

TEST_F(OriginPolicyFetcherTest, EndToEndDefaultNotRedirecting) {
  SetUpDefaultPolicyResponse(
      R"({ "feature-policy": ["geolocation http://example1.com"] })");

  TestOriginPolicyFetcherResult tester;
  tester.RetrieveOriginPolicy("", this);
  EXPECT_EQ(OriginPolicyState::kCannotLoadPolicy,
            tester.origin_policy_result()->state);
  EXPECT_FALSE(tester.origin_policy_result()->contents);
}

TEST_F(OriginPolicyFetcherTest, EmptyVersionConstructor) {
  EXPECT_DCHECK_DEATH(
      OriginPolicyFetcher(manager(), OriginPolicyHeaderValues(),
                          test_server_origin(), url_loader_factory(),
                          base::BindOnce(&DummyRetrieveOriginPolicyCallback)));
}

}  // namespace network
