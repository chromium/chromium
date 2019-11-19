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
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/platform/websocket_handshake_throttle.h"
#include "third_party/blink/renderer/core/script/fetch_client_settings_object_impl.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/testing/test_loader_factory.h"
#include "third_party/blink/renderer/platform/loader/testing/test_resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/scheduler/test/fake_task_runner.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class MockBaseFetchContext final : public BaseFetchContext {
 public:
  MockBaseFetchContext(const DetachableResourceFetcherProperties& properties,
                       ExecutionContext* execution_context)
      : BaseFetchContext(properties), execution_context_(execution_context) {}
  ~MockBaseFetchContext() override = default;

  // BaseFetchContext overrides:
  KURL GetSiteForCookies() const override { return KURL(); }
  scoped_refptr<const blink::SecurityOrigin> GetTopFrameOrigin()
      const override {
    return SecurityOrigin::CreateUniqueOpaque();
  }
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

  const SecurityOrigin* GetParentSecurityOrigin() const override {
    return nullptr;
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

 private:
  Member<ExecutionContext> execution_context_;
  Member<const FetchClientSettingsObjectImpl> fetch_client_settings_object_;
};

class BaseFetchContextTest : public testing::Test {
 protected:
  void SetUp() override {
    execution_context_ = MakeGarbageCollected<NullExecutionContext>();
    static_cast<NullExecutionContext*>(execution_context_.Get())
        ->SetUpSecurityContext();
    resource_fetcher_properties_ =
        MakeGarbageCollected<TestResourceFetcherProperties>(
            *MakeGarbageCollected<FetchClientSettingsObjectImpl>(
                *execution_context_));
    auto& properties = resource_fetcher_properties_->MakeDetachable();
    fetch_context_ = MakeGarbageCollected<MockBaseFetchContext>(
        properties, execution_context_);
    resource_fetcher_ = MakeGarbageCollected<ResourceFetcher>(
        ResourceFetcherInit(properties, fetch_context_,
                            base::MakeRefCounted<scheduler::FakeTaskRunner>(),
                            MakeGarbageCollected<TestLoaderFactory>()));
  }

  const FetchClientSettingsObject& GetFetchClientSettingsObject() const {
    return resource_fetcher_->GetProperties().GetFetchClientSettingsObject();
  }
  const SecurityOrigin* GetSecurityOrigin() const {
    return GetFetchClientSettingsObject().GetSecurityOrigin();
  }

  Persistent<ExecutionContext> execution_context_;
  Persistent<MockBaseFetchContext> fetch_context_;
  Persistent<ResourceFetcher> resource_fetcher_;
  Persistent<TestResourceFetcherProperties> resource_fetcher_properties_;
};

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
  resource_request.SetRequestorOrigin(GetSecurityOrigin());

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
  request.SetRequestorOrigin(GetSecurityOrigin());
  ResourceRequest keepalive_request(url);
  keepalive_request.SetRequestorOrigin(GetSecurityOrigin());
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

  resource_fetcher_->ClearContext();

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
  resource_request.SetRequestorOrigin(GetSecurityOrigin());
  ResourceLoaderOptions options;
  options.initiator_info.name = fetch_initiator_type_names::kUacss;

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
  resource_request.SetRequestorOrigin(GetSecurityOrigin());
  ResourceLoaderOptions options;
  options.initiator_info.name = fetch_initiator_type_names::kUacss;

  EXPECT_EQ(base::nullopt,
            fetch_context_->CanRequest(
                ResourceType::kImage, resource_request, data_url, options,
                SecurityViolationReportingPolicy::kReport,
                ResourceRequest::RedirectStatus::kFollowedRedirect));
}

}  // namespace blink
