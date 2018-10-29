/*
 * Copyright (c) 2015, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/loader/base_fetch_context.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/websocket_handshake_throttle.h"
#include "third_party/blink/renderer/core/script/fetch_client_settings_object_impl.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class MockBaseFetchContext final : public BaseFetchContext {
 public:
  explicit MockBaseFetchContext(ExecutionContext* execution_context)
      : BaseFetchContext(
            execution_context->GetTaskRunner(blink::TaskType::kInternalTest)),
        execution_context_(execution_context),
        fetch_client_settings_object_(
            new FetchClientSettingsObjectImpl(*execution_context)) {}
  ~MockBaseFetchContext() override = default;

  // BaseFetchContext overrides:
  const FetchClientSettingsObject* GetFetchClientSettingsObject()
      const override {
    return fetch_client_settings_object_.Get();
  }
  KURL GetSiteForCookies() const override { return KURL(); }
  bool AllowScriptFromSource(const KURL&) const override { return false; }
  SubresourceFilter* GetSubresourceFilter() const override { return nullptr; }
  PreviewsResourceLoadingHints* GetPreviewsResourceLoadingHints()
      const override {
    return nullptr;
  }
  bool ShouldBlockRequestByInspector(const KURL&) const override {
    return false;
  }
  void DispatchDidBlockRequest(const ResourceRequest&,
                               const FetchInitiatorInfo&,
                               ResourceRequestBlockedReason,
                               ResourceType) const override {}
  bool ShouldBypassMainWorldCSP() const override { return false; }
  bool IsSVGImageChromeClient() const override { return false; }
  void CountUsage(WebFeature) const override {}
  void CountDeprecation(WebFeature) const override {}
  bool ShouldBlockWebSocketByMixedContentCheck(const KURL&) const override {
    return false;
  }
  std::unique_ptr<WebSocketHandshakeThrottle> CreateWebSocketHandshakeThrottle()
      override {
    return nullptr;
  }
  bool ShouldBlockFetchByMixedContentCheck(
      mojom::RequestContextType,
      network::mojom::RequestContextFrameType,
      ResourceRequest::RedirectStatus,
      const KURL&,
      SecurityViolationReportingPolicy) const override {
    return false;
  }
  bool ShouldBlockFetchAsCredentialedSubresource(const ResourceRequest&,
                                                 const KURL&) const override {
    return false;
  }
  const KURL& Url() const override { return execution_context_->Url(); }

  const SecurityOrigin* GetSecurityOrigin() const override {
    return fetch_client_settings_object_->GetSecurityOrigin();
  }
  const SecurityOrigin* GetParentSecurityOrigin() const override {
    return nullptr;
  }
  base::Optional<mojom::IPAddressSpace> GetAddressSpace() const override {
    return base::make_optional(
        execution_context_->GetSecurityContext().AddressSpace());
  }
  const ContentSecurityPolicy* GetContentSecurityPolicy() const override {
    return execution_context_->GetContentSecurityPolicy();
  }
  void AddConsoleMessage(ConsoleMessage*) const override {}

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(execution_context_);
    visitor->Trace(fetch_client_settings_object_);
    BaseFetchContext::Trace(visitor);
  }

  bool IsDetached() const override { return is_detached_; }
  void SetIsDetached(bool is_detached) { is_detached_ = is_detached; }

 private:
  Member<ExecutionContext> execution_context_;
  Member<FetchClientSettingsObjectImpl> fetch_client_settings_object_;
  bool is_detached_ = false;
};

class BaseFetchContextTest : public testing::Test {
 protected:
  void SetUp() override {
    execution_context_ = new NullExecutionContext();
    static_cast<NullExecutionContext*>(execution_context_.Get())
        ->SetUpSecurityContext();
    fetch_context_ = new MockBaseFetchContext(execution_context_);
  }

  Persistent<ExecutionContext> execution_context_;
  Persistent<MockBaseFetchContext> fetch_context_;
};

TEST_F(BaseFetchContextTest, SetIsExternalRequestForPublicContext) {
  EXPECT_EQ(mojom::IPAddressSpace::kPublic,
            execution_context_->GetSecurityContext().AddressSpace());

  struct TestCase {
    const char* url;
    bool is_external_expectation;
  } cases[] = {
      {"data:text/html,whatever", false},  {"file:///etc/passwd", false},
      {"blob:http://example.com/", false},

      {"http://example.com/", false},      {"https://example.com/", false},

      {"http://192.168.1.1:8000/", true},  {"http://10.1.1.1:8000/", true},

      {"http://localhost/", true},         {"http://127.0.0.1/", true},
      {"http://127.0.0.1:8000/", true}};
  {
    ScopedCorsRFC1918ForTest cors_rfc1918(false);
    for (const auto& test : cases) {
      SCOPED_TRACE(test.url);
      ResourceRequest main_request(test.url);
      fetch_context_->AddAdditionalRequestHeaders(main_request,
                                                  kFetchMainResource);
      EXPECT_FALSE(main_request.IsExternalRequest());

      ResourceRequest sub_request(test.url);
      fetch_context_->AddAdditionalRequestHeaders(sub_request,
                                                  kFetchSubresource);
      EXPECT_FALSE(sub_request.IsExternalRequest());
    }
  }

  {
    ScopedCorsRFC1918ForTest cors_rfc1918(true);
    for (const auto& test : cases) {
      SCOPED_TRACE(test.url);
      ResourceRequest main_request(test.url);
      fetch_context_->AddAdditionalRequestHeaders(main_request,
                                                  kFetchMainResource);
      EXPECT_EQ(test.is_external_expectation, main_request.IsExternalRequest());

      ResourceRequest sub_request(test.url);
      fetch_context_->AddAdditionalRequestHeaders(sub_request,
                                                  kFetchSubresource);
      EXPECT_EQ(test.is_external_expectation, sub_request.IsExternalRequest());
    }
  }
}

TEST_F(BaseFetchContextTest, SetIsExternalRequestForPrivateContext) {
  execution_context_->GetSecurityContext().SetAddressSpace(
      mojom::IPAddressSpace::kPrivate);
  EXPECT_EQ(mojom::IPAddressSpace::kPrivate,
            execution_context_->GetSecurityContext().AddressSpace());

  struct TestCase {
    const char* url;
    bool is_external_expectation;
  } cases[] = {
      {"data:text/html,whatever", false},  {"file:///etc/passwd", false},
      {"blob:http://example.com/", false},

      {"http://example.com/", false},      {"https://example.com/", false},

      {"http://192.168.1.1:8000/", false}, {"http://10.1.1.1:8000/", false},

      {"http://localhost/", true},         {"http://127.0.0.1/", true},
      {"http://127.0.0.1:8000/", true}};
  {
    ScopedCorsRFC1918ForTest cors_rfc1918(false);
    for (const auto& test : cases) {
      SCOPED_TRACE(test.url);
      ResourceRequest main_request(test.url);
      fetch_context_->AddAdditionalRequestHeaders(main_request,
                                                  kFetchMainResource);
      EXPECT_FALSE(main_request.IsExternalRequest());

      ResourceRequest sub_request(test.url);
      fetch_context_->AddAdditionalRequestHeaders(sub_request,
                                                  kFetchSubresource);
      EXPECT_FALSE(sub_request.IsExternalRequest());
    }
  }

  {
    ScopedCorsRFC1918ForTest cors_rfc1918(true);
    for (const auto& test : cases) {
      SCOPED_TRACE(test.url);
      ResourceRequest main_request(test.url);
      fetch_context_->AddAdditionalRequestHeaders(main_request,
                                                  kFetchMainResource);
      EXPECT_EQ(test.is_external_expectation, main_request.IsExternalRequest());

      ResourceRequest sub_request(test.url);
      fetch_context_->AddAdditionalRequestHeaders(sub_request,
                                                  kFetchSubresource);
      EXPECT_EQ(test.is_external_expectation, sub_request.IsExternalRequest());
    }
  }
}

TEST_F(BaseFetchContextTest, SetIsExternalRequestForLocalContext) {
  execution_context_->GetSecurityContext().SetAddressSpace(
      mojom::IPAddressSpace::kLocal);
  EXPECT_EQ(mojom::IPAddressSpace::kLocal,
            execution_context_->GetSecurityContext().AddressSpace());

  struct TestCase {
    const char* url;
    bool is_external_expectation;
  } cases[] = {
      {"data:text/html,whatever", false},  {"file:///etc/passwd", false},
      {"blob:http://example.com/", false},

      {"http://example.com/", false},      {"https://example.com/", false},

      {"http://192.168.1.1:8000/", false}, {"http://10.1.1.1:8000/", false},

      {"http://localhost/", false},        {"http://127.0.0.1/", false},
      {"http://127.0.0.1:8000/", false}};
  {
    ScopedCorsRFC1918ForTest cors_rfc1918(false);
    for (const auto& test : cases) {
      ResourceRequest main_request(test.url);
      fetch_context_->AddAdditionalRequestHeaders(main_request,
                                                  kFetchMainResource);
      EXPECT_FALSE(main_request.IsExternalRequest());

      ResourceRequest sub_request(test.url);
      fetch_context_->AddAdditionalRequestHeaders(sub_request,
                                                  kFetchSubresource);
      EXPECT_FALSE(sub_request.IsExternalRequest());
    }
  }

  {
    ScopedCorsRFC1918ForTest cors_rfc1918(true);
    for (const auto& test : cases) {
      ResourceRequest main_request(test.url);
      fetch_context_->AddAdditionalRequestHeaders(main_request,
                                                  kFetchMainResource);
      EXPECT_EQ(test.is_external_expectation, main_request.IsExternalRequest());

      ResourceRequest sub_request(test.url);
      fetch_context_->AddAdditionalRequestHeaders(sub_request,
                                                  kFetchSubresource);
      EXPECT_EQ(test.is_external_expectation, sub_request.IsExternalRequest());
    }
  }
}

// Tests that CanRequest() checks the enforced CSP headers.
TEST_F(BaseFetchContextTest, CanRequest) {
  ContentSecurityPolicy* policy =
      execution_context_->GetContentSecurityPolicy();
  policy->DidReceiveHeader("script-src https://foo.test",
                           kContentSecurityPolicyHeaderTypeEnforce,
                           kContentSecurityPolicyHeaderSourceHTTP);
  policy->DidReceiveHeader("script-src https://bar.test",
                           kContentSecurityPolicyHeaderTypeReport,
                           kContentSecurityPolicyHeaderSourceHTTP);

  KURL url(NullURL(), "http://baz.test");
  ResourceRequest resource_request(url);
  resource_request.SetRequestContext(mojom::RequestContextType::SCRIPT);
  resource_request.SetRequestorOrigin(fetch_context_->GetSecurityOrigin());
  resource_request.SetFetchCredentialsMode(
      network::mojom::FetchCredentialsMode::kOmit);

  ResourceLoaderOptions options;

  EXPECT_EQ(ResourceRequestBlockedReason::kCSP,
            fetch_context_->CanRequest(
                ResourceType::kScript, resource_request, url, options,
                SecurityViolationReportingPolicy::kReport,
                ResourceRequest::RedirectStatus::kFollowedRedirect));
  EXPECT_EQ(1u, policy->violation_reports_sent_.size());
}

// Tests that CheckCSPForRequest() checks the report-only CSP headers.
TEST_F(BaseFetchContextTest, CheckCSPForRequest) {
  ContentSecurityPolicy* policy =
      execution_context_->GetContentSecurityPolicy();
  policy->DidReceiveHeader("script-src https://foo.test",
                           kContentSecurityPolicyHeaderTypeEnforce,
                           kContentSecurityPolicyHeaderSourceHTTP);
  policy->DidReceiveHeader("script-src https://bar.test",
                           kContentSecurityPolicyHeaderTypeReport,
                           kContentSecurityPolicyHeaderSourceHTTP);

  KURL url(NullURL(), "http://baz.test");

  ResourceLoaderOptions options;

  EXPECT_EQ(base::nullopt,
            fetch_context_->CheckCSPForRequest(
                mojom::RequestContextType::SCRIPT, url, options,
                SecurityViolationReportingPolicy::kReport,
                ResourceRequest::RedirectStatus::kFollowedRedirect));
  EXPECT_EQ(1u, policy->violation_reports_sent_.size());
}

TEST_F(BaseFetchContextTest, CanRequestWhenDetached) {
  KURL url(NullURL(), "http://www.example.com/");
  ResourceRequest request(url);
  request.SetRequestorOrigin(fetch_context_->GetSecurityOrigin());
  ResourceRequest keepalive_request(url);
  keepalive_request.SetRequestorOrigin(fetch_context_->GetSecurityOrigin());
  keepalive_request.SetKeepalive(true);

  EXPECT_EQ(base::nullopt,
            fetch_context_->CanRequest(
                ResourceType::kRaw, request, url, ResourceLoaderOptions(),
                SecurityViolationReportingPolicy::kSuppressReporting,
                ResourceRequest::RedirectStatus::kNoRedirect));

  EXPECT_EQ(
      base::nullopt,
      fetch_context_->CanRequest(
          ResourceType::kRaw, keepalive_request, url, ResourceLoaderOptions(),
          SecurityViolationReportingPolicy::kSuppressReporting,
          ResourceRequest::RedirectStatus::kNoRedirect));

  EXPECT_EQ(base::nullopt,
            fetch_context_->CanRequest(
                ResourceType::kRaw, request, url, ResourceLoaderOptions(),
                SecurityViolationReportingPolicy::kSuppressReporting,
                ResourceRequest::RedirectStatus::kFollowedRedirect));

  EXPECT_EQ(
      base::nullopt,
      fetch_context_->CanRequest(
          ResourceType::kRaw, keepalive_request, url, ResourceLoaderOptions(),
          SecurityViolationReportingPolicy::kSuppressReporting,
          ResourceRequest::RedirectStatus::kFollowedRedirect));

  fetch_context_->SetIsDetached(true);

  EXPECT_EQ(ResourceRequestBlockedReason::kOther,
            fetch_context_->CanRequest(
                ResourceType::kRaw, request, url, ResourceLoaderOptions(),
                SecurityViolationReportingPolicy::kSuppressReporting,
                ResourceRequest::RedirectStatus::kNoRedirect));

  EXPECT_EQ(
      ResourceRequestBlockedReason::kOther,
      fetch_context_->CanRequest(
          ResourceType::kRaw, keepalive_request, url, ResourceLoaderOptions(),
          SecurityViolationReportingPolicy::kSuppressReporting,
          ResourceRequest::RedirectStatus::kNoRedirect));

  EXPECT_EQ(ResourceRequestBlockedReason::kOther,
            fetch_context_->CanRequest(
                ResourceType::kRaw, request, url, ResourceLoaderOptions(),
                SecurityViolationReportingPolicy::kSuppressReporting,
                ResourceRequest::RedirectStatus::kFollowedRedirect));

  EXPECT_EQ(
      base::nullopt,
      fetch_context_->CanRequest(
          ResourceType::kRaw, keepalive_request, url, ResourceLoaderOptions(),
          SecurityViolationReportingPolicy::kSuppressReporting,
          ResourceRequest::RedirectStatus::kFollowedRedirect));
}

// Test that User Agent CSS can only load images with data urls.
TEST_F(BaseFetchContextTest, UACSSTest) {
  KURL test_url("https://example.com");
  KURL data_url("data:image/png;base64,test");

  ResourceRequest resource_request(test_url);
  resource_request.SetRequestorOrigin(fetch_context_->GetSecurityOrigin());
  ResourceLoaderOptions options;
  options.initiator_info.name = FetchInitiatorTypeNames::uacss;

  EXPECT_EQ(ResourceRequestBlockedReason::kOther,
            fetch_context_->CanRequest(
                ResourceType::kScript, resource_request, test_url, options,
                SecurityViolationReportingPolicy::kReport,
                ResourceRequest::RedirectStatus::kFollowedRedirect));

  EXPECT_EQ(ResourceRequestBlockedReason::kOther,
            fetch_context_->CanRequest(
                ResourceType::kImage, resource_request, test_url, options,
                SecurityViolationReportingPolicy::kReport,
                ResourceRequest::RedirectStatus::kFollowedRedirect));

  EXPECT_EQ(base::nullopt,
            fetch_context_->CanRequest(
                ResourceType::kImage, resource_request, data_url, options,
                SecurityViolationReportingPolicy::kReport,
                ResourceRequest::RedirectStatus::kFollowedRedirect));
}

// Test that User Agent CSS can bypass CSP to load embedded images.
TEST_F(BaseFetchContextTest, UACSSTest_BypassCSP) {
  ContentSecurityPolicy* policy =
      execution_context_->GetContentSecurityPolicy();
  policy->DidReceiveHeader("default-src 'self'",
                           kContentSecurityPolicyHeaderTypeEnforce,
                           kContentSecurityPolicyHeaderSourceHTTP);

  KURL data_url("data:image/png;base64,test");

  ResourceRequest resource_request(data_url);
  resource_request.SetRequestorOrigin(fetch_context_->GetSecurityOrigin());
  ResourceLoaderOptions options;
  options.initiator_info.name = FetchInitiatorTypeNames::uacss;

  EXPECT_EQ(base::nullopt,
            fetch_context_->CanRequest(
                ResourceType::kImage, resource_request, data_url, options,
                SecurityViolationReportingPolicy::kReport,
                ResourceRequest::RedirectStatus::kFollowedRedirect));
}

}  // namespace blink
