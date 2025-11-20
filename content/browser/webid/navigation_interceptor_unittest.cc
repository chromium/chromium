// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/navigation_interceptor.h"

#include <variant>

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "content/browser/webid/accounts_fetcher.h"
#include "content/browser/webid/request_service.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/mock_navigation_throttle_registry.h"
#include "content/public/test/test_renderer_host.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/structured_headers.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "url/gurl.h"

using ::testing::_;
using ::testing::Return;
using ::testing::WithArgs;

namespace content::webid {

class InterceptorMockNavigationHandle : public MockNavigationHandle {
 public:
  explicit InterceptorMockNavigationHandle(WebContents* web_contents)
      : MockNavigationHandle(web_contents) {}

  blink::mojom::NavigationInitiatorActivationAndAdStatus
  GetNavigationInitiatorActivationAndAdStatus() override {
    return blink::mojom::NavigationInitiatorActivationAndAdStatus::
        kStartedWithTransientActivationFromNonAd;
  }
};

class MockFederatedAuthRequest : public RequestService {
 public:
  explicit MockFederatedAuthRequest(RenderFrameHost* rfh)
      : RequestService(rfh) {}

  MOCK_METHOD(
      void,
      RequestToken,
      (std::vector<blink::mojom::IdentityProviderGetParametersPtr>
           idp_get_params,
       password_manager::CredentialMediationRequirement mediation_requirement,
       NavigationHandle* navigation_handle,
       RequestTokenCallback callback),
      (override));
  MOCK_METHOD(void, CancelTokenRequest, (), (override));
  MOCK_METHOD(void,
              RequestUserInfo,
              (blink::mojom::IdentityProviderConfigPtr provider,
               RequestUserInfoCallback callback),
              (override));
  MOCK_METHOD(void,
              ResolveTokenRequest,
              (const std::optional<std::string>& account_id,
               base::Value token,
               ResolveTokenRequestCallback callback),
              (override));
  MOCK_METHOD(
      void,
      SetIdpSigninStatus,
      (const ::url::Origin& origin,
       blink::mojom::IdpSigninStatus status,
       const std::optional<::blink::common::webid::LoginStatusOptions>& options,
       blink::mojom::FederatedAuthRequest::SetIdpSigninStatusCallback callback),
      (override));
  MOCK_METHOD(
      void,
      RegisterIdP,
      (const ::GURL& url,
       blink::mojom::FederatedAuthRequest::RegisterIdPCallback callback),
      (override));
  MOCK_METHOD(
      void,
      UnregisterIdP,
      (const ::GURL& url,
       blink::mojom::FederatedAuthRequest::UnregisterIdPCallback callback),
      (override));
  MOCK_METHOD(void, CloseModalDialogView, (), (override));
  MOCK_METHOD(void,
              PreventSilentAccess,
              (blink::mojom::FederatedAuthRequest::PreventSilentAccessCallback
                   callback),
              (override));
  MOCK_METHOD(void,
              Disconnect,
              (blink::mojom::IdentityCredentialDisconnectOptionsPtr options,
               blink::mojom::FederatedAuthRequest::DisconnectCallback callback),
              (override));
};

net::structured_headers::Dictionary EncodeParams(
    const std::map<std::string,
                   std::variant<std::string, std::vector<std::string>>>&
        params) {
  net::structured_headers::Dictionary dictionary;
  for (const auto& pair : params) {
    const std::string& key = pair.first;
    const auto& value_variant = pair.second;

    std::vector<net::structured_headers::ParameterizedItem>
        member_items_for_param_member;

    if (std::holds_alternative<std::string>(value_variant)) {
      const std::string& value = std::get<std::string>(value_variant);
      member_items_for_param_member.emplace_back(
          net::structured_headers::Item(
              value, net::structured_headers::Item::kStringType),
          net::structured_headers::Parameters());
    } else if (std::holds_alternative<std::vector<std::string>>(
                   value_variant)) {
      const std::vector<std::string>& values =
          std::get<std::vector<std::string>>(value_variant);
      for (const auto& value : values) {
        member_items_for_param_member.emplace_back(
            net::structured_headers::Item(
                value, net::structured_headers::Item::kStringType),
            net::structured_headers::Parameters());
      }
    }

    auto member = net::structured_headers::ParameterizedMember(
        std::move(member_items_for_param_member),
        net::structured_headers::Parameters());
    if (std::holds_alternative<std::string>(value_variant)) {
      member.member_is_inner_list = false;
    }
    dictionary[key] = std::move(member);
  }
  return dictionary;
}

class NavigationStartObserver : public WebContentsObserver {
 public:
  explicit NavigationStartObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}
  ~NavigationStartObserver() override = default;

  void DidStartNavigation(NavigationHandle* navigation_handle) override {
    started_url_ = navigation_handle->GetURL();
    if (wait_loop_) {
      wait_loop_->Quit();
    }
  }

  void Wait() {
    if (started_url_.is_valid()) {
      return;
    }
    wait_loop_ = std::make_unique<base::RunLoop>();
    wait_loop_->Run();
  }

  const GURL& started_url() const { return started_url_; }

 private:
  std::unique_ptr<base::RunLoop> wait_loop_;
  GURL started_url_;
};

class NavigationFinishObserver : public WebContentsObserver {
 public:
  explicit NavigationFinishObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}
  ~NavigationFinishObserver() override = default;

  void DidFinishNavigation(NavigationHandle* navigation_handle) override {
    wait_loop_.Quit();
  }

  void Wait() { wait_loop_.Run(); }

 private:
  base::RunLoop wait_loop_;
};

class NavigationInterceptorTest : public RenderViewHostTestHarness {
 public:
  NavigationInterceptorTest() = default;
  ~NavigationInterceptorTest() override = default;
};

TEST_F(NavigationInterceptorTest, SerializedHeaderFormat) {
  auto header = webid::EncodeParams({
      {"config_url", "https://idp.example/fedcm.json"},
      {"client_id", "1234"},
      {"context", "continue"},
      {"login_hint", "user@email.com"},
      {"domain_hint", "domain.com"},
      {"params", R"({"custom_key":"custom_value"})"},
      {"fields", std::vector<std::string>{"name", "email"}},
  });
  EXPECT_EQ(
      net::structured_headers::SerializeDictionary(header).value(),
      R"(client_id="1234", config_url="https://idp.example/fedcm.json", context="continue", domain_hint="domain.com", fields=("name" "email"), login_hint="user@email.com", params="{\"custom_key\":\"custom_value\"}")");
}

TEST_F(NavigationInterceptorTest, WillProcessResponse) {
  // Uses an in-process data decoder service for testing.
  data_decoder::test::InProcessDataDecoder in_process_data_decoder;

  std::unique_ptr<MockFederatedAuthRequest> federated_auth_request =
      std::make_unique<MockFederatedAuthRequest>(
          web_contents()->GetPrimaryMainFrame());

  NavigateAndCommit(GURL("https://rp.example/"));
  InterceptorMockNavigationHandle mock_navigation_handle(web_contents());
  EXPECT_CALL(mock_navigation_handle, GetPreviousRenderFrameHostId)
      .WillRepeatedly(
          Return(web_contents()->GetPrimaryMainFrame()->GetGlobalId()));
  mock_navigation_handle.set_render_frame_host(
      web_contents()->GetPrimaryMainFrame());
  mock_navigation_handle.set_is_in_primary_main_frame(true);

  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->AddHeader("FedCM-Intercept-Navigation",
                     net::structured_headers::SerializeDictionary(
                         webid::EncodeParams({
                             {"config_url", "https://idp.example/fedcm.json"},
                             {"client_id", "1234"},
                         }))
                         .value());
  mock_navigation_handle.set_response_headers(headers);

  content::MockNavigationThrottleRegistry registry(&mock_navigation_handle);

  webid::NavigationInterceptor interceptor(
      registry,
      base::BindLambdaForTesting(
          [&federated_auth_request](RenderFrameHost* rfh) -> RequestService* {
            return federated_auth_request.get();
          }));

  GURL redirect_to("https://rp.example");

  EXPECT_CALL(*federated_auth_request.get(), RequestToken)
      .WillOnce(WithArgs<3>(
          [&redirect_to](
              blink::mojom::FederatedAuthRequest::RequestTokenCallback
                  callback) {
            base::Value::Dict token_dict;
            token_dict.Set("redirect_to", redirect_to.spec());
            std::move(callback).Run(
                blink::mojom::RequestTokenStatus::kSuccess,
                /*selected_identity_provider_config_url=*/GURL(),
                base::Value(token_dict.Clone()),
                /*error=*/nullptr,
                /*is_auto_selected=*/false);
          }));

  NavigationStartObserver observer(web_contents());
  interceptor.WillStartRequest();
  auto result = interceptor.WillProcessResponse();
  EXPECT_EQ(result, content::NavigationThrottle::DEFER);

  observer.Wait();

  EXPECT_EQ(observer.started_url(), redirect_to);
}

TEST_F(NavigationInterceptorTest, WillProcessResponseNoActivation) {
  std::unique_ptr<MockFederatedAuthRequest> federated_auth_request =
      std::make_unique<MockFederatedAuthRequest>(
          web_contents()->GetPrimaryMainFrame());

  NavigateAndCommit(GURL("https://rp.example/"));
  // MockNavigationHandle (as opposed to InterceptorNavigationHandle) does not
  // have activation.
  MockNavigationHandle mock_navigation_handle(web_contents());
  EXPECT_CALL(mock_navigation_handle, GetPreviousRenderFrameHostId)
      .WillRepeatedly(
          Return(web_contents()->GetPrimaryMainFrame()->GetGlobalId()));
  mock_navigation_handle.set_render_frame_host(
      web_contents()->GetPrimaryMainFrame());
  mock_navigation_handle.set_is_in_primary_main_frame(true);

  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->AddHeader("FedCM-Intercept-Navigation",
                     net::structured_headers::SerializeDictionary(
                         webid::EncodeParams({
                             {"config_url", "https://idp.example/fedcm.json"},
                             {"client_id", "1234"},
                         }))
                         .value());
  mock_navigation_handle.set_response_headers(headers);

  content::MockNavigationThrottleRegistry registry(&mock_navigation_handle);

  webid::NavigationInterceptor interceptor(
      registry,
      base::BindLambdaForTesting(
          [&federated_auth_request](RenderFrameHost* rfh) -> RequestService* {
            return federated_auth_request.get();
          }));

  // Because there was no activation, we should proceed.
  auto result = interceptor.WillProcessResponse();
  EXPECT_EQ(result, content::NavigationThrottle::PROCEED);
}

TEST_F(NavigationInterceptorTest, NavigationAfterStartRequest) {
  // Uses an in-process data decoder service for testing.
  data_decoder::test::InProcessDataDecoder in_process_data_decoder;

  // TODO(crbug.com/462217238): Make interception work with bfcache.
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  NavigateAndCommit(GURL("https://rp.example/"));
  InterceptorMockNavigationHandle mock_navigation_handle(web_contents());
  EXPECT_CALL(mock_navigation_handle, GetPreviousRenderFrameHostId)
      .WillRepeatedly(
          Return(web_contents()->GetPrimaryMainFrame()->GetGlobalId()));
  mock_navigation_handle.set_render_frame_host(
      web_contents()->GetPrimaryMainFrame());
  mock_navigation_handle.set_is_in_primary_main_frame(true);

  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->AddHeader("FedCM-Intercept-Navigation",
                     net::structured_headers::SerializeDictionary(
                         webid::EncodeParams({
                             {"config_url", "https://idp.example/fedcm.json"},
                             {"client_id", "1234"},
                         }))
                         .value());
  mock_navigation_handle.set_response_headers(headers);

  content::MockNavigationThrottleRegistry registry(&mock_navigation_handle);

  webid::NavigationInterceptor interceptor(
      registry,
      base::BindLambdaForTesting(
          [](RenderFrameHost* rfh) -> RequestService* { return nullptr; }));

  NavigationFinishObserver observer(web_contents());
  interceptor.WillStartRequest();
  NavigateAndCommit(GURL("https://foo.example/"), ui::PAGE_TRANSITION_TYPED);
  observer.Wait();
  auto result = interceptor.WillProcessResponse();
  EXPECT_EQ(result, content::NavigationThrottle::PROCEED);
}

TEST_F(NavigationInterceptorTest, WillProcessResponseTokenRequestFails) {
  // Uses an in-process data decoder service for testing.
  data_decoder::test::InProcessDataDecoder in_process_data_decoder;

  auto federated_auth_request = std::make_unique<MockFederatedAuthRequest>(
      web_contents()->GetPrimaryMainFrame());

  NavigateAndCommit(GURL("https://rp.example/"));
  InterceptorMockNavigationHandle mock_navigation_handle(web_contents());
  EXPECT_CALL(mock_navigation_handle, GetPreviousRenderFrameHostId)
      .WillRepeatedly(
          Return(web_contents()->GetPrimaryMainFrame()->GetGlobalId()));
  mock_navigation_handle.set_render_frame_host(
      web_contents()->GetPrimaryMainFrame());
  mock_navigation_handle.set_is_in_primary_main_frame(true);

  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->AddHeader("FedCM-Intercept-Navigation",
                     net::structured_headers::SerializeDictionary(
                         webid::EncodeParams({
                             {"config_url", "https://idp.example/fedcm.json"},
                             {"client_id", "1234"},
                         }))
                         .value());
  mock_navigation_handle.set_response_headers(headers);

  content::MockNavigationThrottleRegistry registry(&mock_navigation_handle);

  webid::NavigationInterceptor interceptor(
      registry,
      base::BindLambdaForTesting(
          [&federated_auth_request](RenderFrameHost* rfh) -> RequestService* {
            return federated_auth_request.get();
          }));

  EXPECT_CALL(*federated_auth_request.get(), RequestToken)
      .WillOnce(WithArgs<3>(
          [](blink::mojom::FederatedAuthRequest::RequestTokenCallback
                 callback) {
            std::move(callback).Run(
                blink::mojom::RequestTokenStatus::kError,
                /*selected_identity_provider_config_url=*/std::nullopt,
                /*token=*/std::nullopt,
                /*error=*/nullptr,
                /*is_auto_selected=*/false);
          }));

  base::RunLoop run_loop;
  bool was_cancelled = false;
  std::optional<NavigationThrottle::ThrottleCheckResult> cancel_result;

  interceptor.set_cancel_deferred_navigation_callback_for_testing(
      base::BindLambdaForTesting(
          [&](NavigationThrottle::ThrottleCheckResult result) {
            was_cancelled = true;
            cancel_result = result;
            run_loop.Quit();
          }));

  interceptor.WillStartRequest();
  auto result = interceptor.WillProcessResponse();
  EXPECT_EQ(result, content::NavigationThrottle::DEFER);

  run_loop.Run();

  EXPECT_TRUE(was_cancelled);
  ASSERT_TRUE(cancel_result.has_value());
  EXPECT_EQ(cancel_result->action(), content::NavigationThrottle::CANCEL);
}

TEST_F(NavigationInterceptorTest, RequestBuilderBuildsRequest) {
  const char* kconfig_url = "https://idp.example/fedcm.json";
  const char* kclient_id = "1234";
  const char* kLoginHint = "user@example.com";
  const char* kDomainHint = "example.com";
  const char* kParamsJson = "{\"custom_key\":\"custom_value\"}";

  webid::NavigationInterceptor::RequestBuilder builder;
  auto parsed_dictionary = webid::EncodeParams({
      {"config_url", kconfig_url},
      {"client_id", kclient_id},
      {"context", "continue"},
      {"login_hint", kLoginHint},
      {"domain_hint", kDomainHint},
      {"params", kParamsJson},
      {"fields", std::vector<std::string>{"name", "email"}},
  });
  auto result = builder.Build(parsed_dictionary);

  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->size(), 1u);

  const auto& idp_get_params = (*result)[0];
  ASSERT_TRUE(idp_get_params);

  EXPECT_EQ(idp_get_params->context, blink::mojom::RpContext::kContinue);
  EXPECT_EQ(idp_get_params->mode, blink::mojom::RpMode::kActive);

  ASSERT_EQ(idp_get_params->providers.size(), 1u);
  const auto& idp_options = idp_get_params->providers[0];
  ASSERT_TRUE(idp_options);
  EXPECT_EQ(idp_options->login_hint, kLoginHint);
  EXPECT_EQ(idp_options->domain_hint, kDomainHint);
  EXPECT_EQ(idp_options->params_json, kParamsJson);
  EXPECT_EQ(idp_options->fields, std::vector<std::string>({"name", "email"}));

  const auto& idp_config = idp_options->config;
  ASSERT_TRUE(idp_config);
  EXPECT_EQ(idp_config->config_url, GURL(kconfig_url));
  EXPECT_EQ(idp_config->client_id, kclient_id);
}

TEST_F(NavigationInterceptorTest, RequestBuilderParsesAllFields) {
  webid::NavigationInterceptor::RequestBuilder builder;
  auto parsed_dictionary = webid::EncodeParams({
      {"config_url", "https://idp.example/fedcm.json"},
      {"client_id", "123"},
      {"fields",
       std::vector<std::string>{"name", "email", "picture", "tel", "username"}},
  });
  auto result = builder.Build(parsed_dictionary);

  ASSERT_TRUE(result.has_value());
  const auto& idp_options = (*result)[0]->providers[0];
  ASSERT_TRUE(idp_options);
  EXPECT_EQ(idp_options->fields,
            std::vector<std::string>(
                {"name", "email", "picture", "tel", "username"}));
}

TEST_F(NavigationInterceptorTest, RequestBuilderHandlesMissingFields) {
  webid::NavigationInterceptor::RequestBuilder builder;
  auto parsed_dictionary = webid::EncodeParams({
      {"config_url", "https://idp.example/fedcm.json"},
      {"client_id", "123"},
  });
  auto result = builder.Build(parsed_dictionary);

  ASSERT_TRUE(result.has_value());
  const auto& idp_options = (*result)[0]->providers[0];
  ASSERT_TRUE(idp_options);
  EXPECT_TRUE(!idp_options->fields.has_value() || idp_options->fields->empty());
}

TEST_F(NavigationInterceptorTest, RequestBuilderMissingParams) {
  webid::NavigationInterceptor::RequestBuilder builder;
  auto parsed_dictionary = webid::EncodeParams({
      {"config_url", "https://idp.example/fedcm.json"},
      {"client_id", "123"},
  });
  auto result = builder.Build(parsed_dictionary);

  ASSERT_TRUE(result.has_value());
  const auto& idp_options = (*result)[0]->providers[0];
  ASSERT_TRUE(idp_options);
  EXPECT_FALSE(idp_options->params_json.has_value());
}

TEST_F(NavigationInterceptorTest,
       RequestBuilderMissingContextDefaultsToSignIn) {
  webid::NavigationInterceptor::RequestBuilder builder;
  auto parsed_dictionary = webid::EncodeParams({
      {"config_url", "https://idp.example/fedcm.json"},
      {"client_id", "123"},
  });
  auto result = builder.Build(parsed_dictionary);

  ASSERT_TRUE(result.has_value());
  ASSERT_EQ((*result)[0]->context, blink::mojom::RpContext::kSignIn);
}

TEST_F(NavigationInterceptorTest, RequestBuilderParsesContext) {
  const struct {
    std::string context_str;
    blink::mojom::RpContext context_enum;
  } kTestCases[] = {
      {"signin", blink::mojom::RpContext::kSignIn},
      {"signup", blink::mojom::RpContext::kSignUp},
      {"use", blink::mojom::RpContext::kUse},
      {"continue", blink::mojom::RpContext::kContinue},
  };

  for (const auto& test_case : kTestCases) {
    webid::NavigationInterceptor::RequestBuilder builder;
    auto parsed_dictionary = webid::EncodeParams({
        {"config_url", "https://idp.example/fedcm.json"},
        {"client_id", "123"},
        {"context", test_case.context_str},
    });
    auto result = builder.Build(parsed_dictionary);

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ((*result)[0]->context, test_case.context_enum);
  }
}

TEST_F(NavigationInterceptorTest,
       RequestBuilderReturnsNulloptOnInvalidContext) {
  webid::NavigationInterceptor::RequestBuilder builder;
  auto parsed_dictionary = webid::EncodeParams({
      {"config_url", "https://idp.example/fedcm.json"},
      {"client_id", "123"},
      {"context", "invalid"},
  });
  auto result = builder.Build(parsed_dictionary);

  ASSERT_FALSE(result.has_value());
}

TEST_F(NavigationInterceptorTest,
       RequestBuilderReturnsNulloptOnMissingconfig_url) {
  webid::NavigationInterceptor::RequestBuilder builder;
  auto parsed_dictionary = webid::EncodeParams({
      {"client_id", "123"},
  });
  auto result = builder.Build(parsed_dictionary);

  ASSERT_FALSE(result.has_value());
}

TEST_F(NavigationInterceptorTest,
       RequestBuilderReturnsNulloptOnMissingclient_id) {
  webid::NavigationInterceptor::RequestBuilder builder;
  auto parsed_dictionary = webid::EncodeParams({
      {"config_url", "https://idp.example/fedcm.json"},
  });
  auto result = builder.Build(parsed_dictionary);

  ASSERT_FALSE(result.has_value());
}

TEST_F(NavigationInterceptorTest, ResponseBuilderBuildsResponse) {
  NavigationInterceptor::ResponseBuilder builder;
  base::Value::Dict response_dict;
  response_dict.Set("redirect_to", "https://example.com/redirect");
  base::Value response(std::move(response_dict));

  auto params = builder.Build(response);

  ASSERT_TRUE(params.has_value());
  EXPECT_EQ(params->url, GURL("https://example.com/redirect"));
  EXPECT_EQ(static_cast<int>(params->transition_type),
            static_cast<int>(ui::PAGE_TRANSITION_LINK));
}

TEST_F(NavigationInterceptorTest, ResponseBuilderFailsWithNoRedirectUrl) {
  NavigationInterceptor::ResponseBuilder builder;
  base::Value response(base::Value::Type::DICT);

  auto params = builder.Build(response);

  ASSERT_FALSE(params.has_value());
}

TEST_F(NavigationInterceptorTest, ResponseBuilderFailsWithInvalidRedirectUrl) {
  NavigationInterceptor::ResponseBuilder builder;
  base::Value::Dict response_dict;
  response_dict.Set("redirect_to", "not a valid url");
  base::Value response(std::move(response_dict));

  auto params = builder.Build(response);

  ASSERT_FALSE(params.has_value());
}

TEST_F(NavigationInterceptorTest, ResponseBuilderFailsWithInternalUrls) {
  NavigationInterceptor::ResponseBuilder builder;
  base::Value::Dict response_dict;
  response_dict.Set("redirect_to", "chrome://settings");
  base::Value response(std::move(response_dict));

  auto params = builder.Build(response);

  ASSERT_FALSE(params.has_value());
}

TEST_F(NavigationInterceptorTest, ResponseBuilderBuildsPostResponse) {
  NavigationInterceptor::ResponseBuilder builder;
  base::Value::Dict redirect_dict;
  redirect_dict.Set("url", "https://example.com/redirect");
  redirect_dict.Set("method", "POST");
  redirect_dict.Set("body", "key=value");
  base::Value::Dict response_dict;
  response_dict.Set("redirect_to", std::move(redirect_dict));
  base::Value response(std::move(response_dict));

  auto params = builder.Build(response);

  ASSERT_TRUE(params.has_value());
  EXPECT_EQ(params->url, GURL("https://example.com/redirect"));
  EXPECT_EQ(static_cast<int>(params->transition_type),
            static_cast<int>(ui::PAGE_TRANSITION_FORM_SUBMIT));
  ASSERT_TRUE(params->post_data);
  const auto& elements = *params->post_data->elements();
  ASSERT_EQ(elements.size(), 1u);
  ASSERT_EQ(elements[0].type(), network::DataElement::Tag::kBytes);
  EXPECT_EQ(elements[0].As<network::DataElementBytes>().AsStringPiece(),
            "key=value");
}

TEST_F(NavigationInterceptorTest,
       ResponseBuilderFailsWithPostResponseMissingUrl) {
  NavigationInterceptor::ResponseBuilder builder;
  base::Value::Dict redirect_dict;
  redirect_dict.Set("method", "POST");
  redirect_dict.Set("body", "key=value");
  base::Value::Dict response_dict;
  response_dict.Set("redirect_to", std::move(redirect_dict));
  base::Value response(std::move(response_dict));

  auto params = builder.Build(response);

  ASSERT_FALSE(params.has_value());
}

TEST_F(NavigationInterceptorTest,
       ResponseBuilderSucceedsWithPostResponseMissingMethod) {
  NavigationInterceptor::ResponseBuilder builder;
  base::Value::Dict redirect_dict;
  redirect_dict.Set("url", "https://example.com/redirect");
  redirect_dict.Set("body", "key=value");
  base::Value::Dict response_dict;
  response_dict.Set("redirect_to", std::move(redirect_dict));
  base::Value response(std::move(response_dict));

  auto params = builder.Build(response);

  ASSERT_TRUE(params.has_value());
  EXPECT_EQ(params->url, GURL("https://example.com/redirect"));
  EXPECT_FALSE(params->post_data);
}

TEST_F(NavigationInterceptorTest,
       ResponseBuilderSucceedsWithPostResponseMissingBody) {
  NavigationInterceptor::ResponseBuilder builder;
  base::Value::Dict redirect_dict;
  redirect_dict.Set("url", "https://example.com/redirect");
  redirect_dict.Set("method", "POST");
  base::Value::Dict response_dict;
  response_dict.Set("redirect_to", std::move(redirect_dict));
  base::Value response(std::move(response_dict));

  auto params = builder.Build(response);

  ASSERT_TRUE(params.has_value());
  EXPECT_EQ(params->url, GURL("https://example.com/redirect"));
  ASSERT_TRUE(params->post_data);
  const auto& elements = *params->post_data->elements();
  ASSERT_EQ(elements.size(), 0u);
}

}  // namespace content::webid
