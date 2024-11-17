// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/with_feature_override.h"
#include "services/network/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/security_context/insecure_request_policy.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_init.h"
#include "third_party/blink/renderer/core/frame/csp/csp_directive_list.h"
#include "third_party/blink/renderer/core/frame/csp/test_util.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_script_element.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/crypto.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/integrity_metadata.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/network/content_security_policy_parsers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

namespace {

using network::mojom::ContentSecurityPolicySource;
using network::mojom::ContentSecurityPolicyType;
using testing::Contains;
using testing::SizeIs;

}  // namespace

class ContentSecurityPolicyTest : public testing::Test {
 public:
  ContentSecurityPolicyTest()
      : csp(MakeGarbageCollected<ContentSecurityPolicy>()),
        secure_url("https://example.test/index.html"),
        secure_origin(SecurityOrigin::Create(secure_url)) {}
  ~ContentSecurityPolicyTest() override {
    execution_context->NotifyContextDestroyed();
  }

 protected:
  void SetUp() override { CreateExecutionContext(); }

  void CreateExecutionContext() {
    if (execution_context)
      execution_context->NotifyContextDestroyed();
    execution_context = MakeGarbageCollected<NullExecutionContext>();
    execution_context->SetUpSecurityContextForTesting();
    execution_context->GetSecurityContext().SetSecurityOriginForTesting(
        secure_origin);
  }

  test::TaskEnvironment task_environment;
  Persistent<ContentSecurityPolicy> csp;
  KURL secure_url;
  scoped_refptr<SecurityOrigin> secure_origin;
  Persistent<NullExecutionContext> execution_context;
};

TEST_F(ContentSecurityPolicyTest, ParseInsecureRequestPolicy) {
  struct TestCase {
    const char* header;
    mojom::blink::InsecureRequestPolicy expected_policy;
  } cases[] = {
      {"default-src 'none'",
       mojom::blink::InsecureRequestPolicy::kLeaveInsecureRequestsAlone},
      {"upgrade-insecure-requests",
       mojom::blink::InsecureRequestPolicy::kUpgradeInsecureRequests},
      {"block-all-mixed-content",
       mojom::blink::InsecureRequestPolicy::kBlockAllMixedContent},
      {"upgrade-insecure-requests; block-all-mixed-content",
       mojom::blink::InsecureRequestPolicy::kUpgradeInsecureRequests |
           mojom::blink::InsecureRequestPolicy::kBlockAllMixedContent},
      {"upgrade-insecure-requests, block-all-mixed-content",
       mojom::blink::InsecureRequestPolicy::kUpgradeInsecureRequests |
           mojom::blink::InsecureRequestPolicy::kBlockAllMixedContent}};

  // Enforced
  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message()
                 << "[Enforce] Header: `" << test.header << "`");
    csp = MakeGarbageCollected<ContentSecurityPolicy>();
    csp->AddPolicies(ParseContentSecurityPolicies(
        test.header, ContentSecurityPolicyType::kEnforce,
        ContentSecurityPolicySource::kHTTP, *secure_origin));
    EXPECT_EQ(test.expected_policy, csp->GetInsecureRequestPolicy());

    auto dummy = std::make_unique<DummyPageHolder>();
    dummy->GetDocument().SetURL(secure_url);
    auto& security_context =
        dummy->GetFrame().DomWindow()->GetSecurityContext();
    security_context.SetSecurityOriginForTesting(secure_origin);

    csp->BindToDelegate(
        dummy->GetFrame().DomWindow()->GetContentSecurityPolicyDelegate());
    EXPECT_EQ(test.expected_policy,
              security_context.GetInsecureRequestPolicy());
    bool expect_upgrade =
        (test.expected_policy &
         mojom::blink::InsecureRequestPolicy::kUpgradeInsecureRequests) !=
        mojom::blink::InsecureRequestPolicy::kLeaveInsecureRequestsAlone;
    EXPECT_EQ(
        expect_upgrade,
        security_context.InsecureNavigationsToUpgrade().Contains(
            dummy->GetDocument().Url().Host().ToString().Impl()->GetHash()));
  }

  // Report-Only
  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message()
                 << "[Report-Only] Header: `" << test.header << "`");
    csp = MakeGarbageCollected<ContentSecurityPolicy>();
    csp->AddPolicies(ParseContentSecurityPolicies(
        test.header, ContentSecurityPolicyType::kReport,
        ContentSecurityPolicySource::kHTTP, *secure_origin));
    EXPECT_EQ(mojom::blink::InsecureRequestPolicy::kLeaveInsecureRequestsAlone,
              csp->GetInsecureRequestPolicy());

    CreateExecutionContext();
    execution_context->GetSecurityContext().SetSecurityOrigin(secure_origin);
    csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
    EXPECT_EQ(
        mojom::blink::InsecureRequestPolicy::kLeaveInsecureRequestsAlone,
        execution_context->GetSecurityContext().GetInsecureRequestPolicy());
    EXPECT_FALSE(execution_context->GetSecurityContext()
                     .InsecureNavigationsToUpgrade()
                     .Contains(secure_origin->Host().Impl()->GetHash()));
  }
}

MATCHER_P(HasSubstr, s, "") {
  return arg.Contains(s);
}

TEST_F(ContentSecurityPolicyTest, AddPolicies) {
  csp->AddPolicies(ParseContentSecurityPolicies(
      "script-src 'none'", ContentSecurityPolicyType::kReport,
      ContentSecurityPolicySource::kHTTP, *secure_origin));
  csp->AddPolicies(ParseContentSecurityPolicies(
      "img-src http://example.com", ContentSecurityPolicyType::kReport,
      ContentSecurityPolicySource::kHTTP, *secure_origin));

  const KURL example_url("http://example.com");
  const KURL not_example_url("http://not-example.com");

  auto* csp2 = MakeGarbageCollected<ContentSecurityPolicy>();
  TestCSPDelegate* test_delegate = MakeGarbageCollected<TestCSPDelegate>();
  csp2->BindToDelegate(*test_delegate);
  csp2->AddPolicies(mojo::Clone(csp->GetParsedPolicies()));

  EXPECT_TRUE(csp2->AllowScriptFromSource(
      example_url, String(), IntegrityMetadataSet(), kParserInserted,
      example_url, ResourceRequest::RedirectStatus::kNoRedirect,
      ReportingDisposition::kReport,
      ContentSecurityPolicy::CheckHeaderType::kCheckReportOnly));
  EXPECT_THAT(
      test_delegate->console_messages(),
      Contains(HasSubstr("Refused to load the script 'http://example.com/'")));

  test_delegate->console_messages().clear();
  EXPECT_TRUE(csp2->AllowImageFromSource(
      example_url, example_url, ResourceRequest::RedirectStatus::kNoRedirect,
      ReportingDisposition::kReport,
      ContentSecurityPolicy::CheckHeaderType::kCheckReportOnly));
  EXPECT_THAT(test_delegate->console_messages(), SizeIs(0));

  test_delegate->console_messages().clear();
  EXPECT_TRUE(csp2->AllowImageFromSource(
      not_example_url, not_example_url,
      ResourceRequest::RedirectStatus::kNoRedirect,
      ReportingDisposition::kReport,
      ContentSecurityPolicy::CheckHeaderType::kCheckReportOnly));
  EXPECT_THAT(test_delegate->console_messages(),
              Contains(HasSubstr(
                  "Refused to load the image 'http://not-example.com/'")));
}

TEST_F(ContentSecurityPolicyTest, IsActiveForConnectionsWithConnectSrc) {
  EXPECT_FALSE(csp->IsActiveForConnections());
  csp->AddPolicies(ParseContentSecurityPolicies(
      "connect-src 'none';", ContentSecurityPolicyType::kEnforce,
      ContentSecurityPolicySource::kHTTP, *secure_origin));
  EXPECT_TRUE(csp->IsActiveForConnections());
}

TEST_F(ContentSecurityPolicyTest, IsActiveForConnectionsWithDefaultSrc) {
  EXPECT_FALSE(csp->IsActiveForConnections());
  csp->AddPolicies(ParseContentSecurityPolicies(
      "default-src 'none';", ContentSecurityPolicyType::kEnforce,
      ContentSecurityPolicySource::kHTTP, *secure_origin));
  EXPECT_TRUE(csp->IsActiveForConnections());
}

// Tests that sandbox directives are discarded from policies
// delivered in <meta> elements.
TEST_F(ContentSecurityPolicyTest, SandboxInMeta) {
  csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
  EXPECT_EQ(network::mojom::blink::WebSandboxFlags::kNone,
            csp->GetSandboxMask());
  csp->AddPolicies(ParseContentSecurityPolicies(
      "sandbox;", ContentSecurityPolicyType::kEnforce,
      ContentSecurityPolicySource::kMeta, *secure_origin));
  EXPECT_EQ(network::mojom::blink::WebSandboxFlags::kNone,
            csp->GetSandboxMask());
  execution_context->GetSecurityContext().SetSandboxFlags(
      network::mojom::blink::WebSandboxFlags::kAll);
  csp->AddPolicies(ParseContentSecurityPolicies(
      "sandbox;", ContentSecurityPolicyType::kEnforce,
      ContentSecurityPolicySource::kHTTP, *secure_origin));
  EXPECT_EQ(network::mojom::blink::WebSandboxFlags::kAll,
            csp->GetSandboxMask());
}

// Tests that object-src directives are applied to a request to load a
// plugin, but not to subresource requests that the plugin itself
// makes. https://crbug.com/603952
TEST_F(ContentSecurityPolicyTest, ObjectSrc) {
  const KURL url("https://example.test");
  csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
  csp->AddPolicies(ParseContentSecurityPolicies(
      "object-src 'none';", ContentSecurityPolicyType::kEnforce,
      ContentSecurityPolicySource::kMeta, *secure_origin));
  EXPECT_FALSE(csp->AllowRequest(mojom::blink::RequestContextType::OBJECT,
                                 network::mojom::RequestDestination::kEmpty,
                                 url, String(), IntegrityMetadataSet(),
                                 kParserInserted, url,
                                 ResourceRequest::RedirectStatus::kNoRedirect,
                                 ReportingDisposition::kSuppressReporting));
  EXPECT_FALSE(csp->AllowRequest(mojom::blink::RequestContextType::EMBED,
                                 network::mojom::RequestDestination::kEmbed,
                                 url, String(), IntegrityMetadataSet(),
                                 kParserInserted, url,
                                 ResourceRequest::RedirectStatus::kNoRedirect,
                                 ReportingDisposition::kSuppressReporting));
  EXPECT_TRUE(csp->AllowRequest(mojom::blink::RequestContextType::PLUGIN,
                                network::mojom::RequestDestination::kEmpty, url,
                                String(), IntegrityMetadataSet(),
                                kParserInserted, url,
                                ResourceRequest::RedirectStatus::kNoRedirect,
                                ReportingDisposition::kSuppressReporting));
}

TEST_F(ContentSecurityPolicyTest, ConnectSrc) {
  const KURL url("https://example.test");
  csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
  csp->AddPolicies(ParseContentSecurityPolicies(
      "connect-src 'none';", ContentSecurityPolicyType::kEnforce,
      ContentSecurityPolicySource::kMeta, *secure_origin));
  EXPECT_FALSE(csp->AllowRequest(mojom::blink::RequestContextType::SUBRESOURCE,
                                 network::mojom::RequestDestination::kEmpty,
                                 url, String(), IntegrityMetadataSet(),
                                 kParserInserted, url,
                                 ResourceRequest::RedirectStatus::kNoRedirect,
                                 ReportingDisposition::kSuppressReporting));
  EXPECT_FALSE(
      csp->AllowRequest(mojom::blink::RequestContextType::XML_HTTP_REQUEST,
                        network::mojom::RequestDestination::kEmpty, url,
                        String(), IntegrityMetadataSet(), kParserInserted, url,
                        ResourceRequest::RedirectStatus::kNoRedirect,
                        ReportingDisposition::kSuppressReporting));
  EXPECT_FALSE(csp->AllowRequest(mojom::blink::RequestContextType::BEACON,
                                 network::mojom::RequestDestination::kEmpty,
                                 url, String(), IntegrityMetadataSet(),
                                 kParserInserted, url,
                                 ResourceRequest::RedirectStatus::kNoRedirect,
                                 ReportingDisposition::kSuppressReporting));
  EXPECT_FALSE(csp->AllowRequest(mojom::blink::RequestContextType::FETCH,
                                 network::mojom::RequestDestination::kEmpty,
                                 url, String(), IntegrityMetadataSet(),
                                 kParserInserted, url,
                                 ResourceRequest::RedirectStatus::kNoRedirect,
                                 ReportingDisposition::kSuppressReporting));
  EXPECT_TRUE(csp->AllowRequest(mojom::blink::RequestContextType::PLUGIN,
                                network::mojom::RequestDestination::kEmpty, url,
                                String(), IntegrityMetadataSet(),
                                kParserInserted, url,
                                ResourceRequest::RedirectStatus::kNoRedirect,
                                ReportingDisposition::kSuppressReporting));
}

TEST_F(ContentSecurityPolicyTest, NonceSinglePolicy) {
  struct TestCase {
    const char* policy;
    const char* url;
    const char* nonce;
    bool allowed;
  } cases[] = {
      {"script-src 'nonce-yay'", "https://example.com/js", "", false},
      {"script-src 'nonce-yay'", "https://example.com/js", "yay", true},
      {"script-src https://example.com", "https://example.com/js", "", true},
      {"script-src https://example.com", "https://example.com/js", "yay", true},
      {"script-src https://example.com 'nonce-yay'",
       "https://not.example.com/js", "", false},
      {"script-src https://example.com 'nonce-yay'",
       "https://not.example.com/js", "yay", true},
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message()
                 << "Policy: `" << test.policy << "`, URL: `" << test.url
                 << "`, Nonce: `" << test.nonce << "`");
    const KURL resource(test.url);

    unsigned expected_reports = test.allowed ? 0u : 1u;

    // Single enforce-mode policy should match `test.expected`:
    Persistent<ContentSecurityPolicy> policy =
        MakeGarbageCollected<ContentSecurityPolicy>();
    policy->BindToDelegate(
        execution_context->GetContentSecurityPolicyDelegate());
    policy->AddPolicies(ParseContentSecurityPolicies(
        test.policy, ContentSecurityPolicyType::kEnforce,
        ContentSecurityPolicySource::kHTTP, *secure_origin));
    EXPECT_EQ(test.allowed,
              policy->AllowScriptFromSource(
                  resource, String(test.nonce), IntegrityMetadataSet(),
                  kParserInserted, resource,
                  ResourceRequest::RedirectStatus::kNoRedirect));
    // If this is expected to generate a violation, we should have sent a
    // report.
    EXPECT_EQ(expected_reports, policy->violation_reports_sent_.size());

    // Single report-mode policy should always be `true`:
    policy = MakeGarbageCollected<ContentSecurityPolicy>();
    policy->BindToDelegate(
        execution_context->GetContentSecurityPolicyDelegate());
    policy->AddPolicies(ParseContentSecurityPolicies(
        test.policy, ContentSecurityPolicyType::kReport,
        ContentSecurityPolicySource::kHTTP, *secure_origin));
    EXPECT_TRUE(policy->AllowScriptFromSource(
        resource, String(test.nonce), IntegrityMetadataSet(), kParserInserted,
        resource, ResourceRequest::RedirectStatus::kNoRedirect,
        ReportingDisposition::kReport,
        ContentSecurityPolicy::CheckHeaderType::kCheckReportOnly));
    // If this is expected to generate a violation, we should have sent a
    // report, even though we don't deny access in `allowScriptFromSource`:
    EXPECT_EQ(expected_reports, policy->violation_reports_sent_.size());
  }
}

TEST_F(ContentSecurityPolicyTest, NonceInline) {
  struct TestCase {
    const char* policy;
    const char* nonce;
    bool allowed;
  } cases[] = {
      {"'unsafe-inline'", "", true},
      {"'unsafe-inline'", "yay", true},
      {"'nonce-yay'", "", false},
      {"'nonce-yay'", "yay", true},
      {"'unsafe-inline' 'nonce-yay'", "", false},
      {"'unsafe-inline' 'nonce-yay'", "yay", true},
  };

  String context_url;
  String content;
  OrdinalNumber context_line = OrdinalNumber::First();

  // We need document for HTMLScriptElement tests.
  auto dummy = std::make_unique<DummyPageHolder>();
  auto* window = dummy->GetFrame().DomWindow();
  window->GetSecurityContext().SetSecurityOriginForTesting(secure_origin);

  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message() << "Policy: `" << test.policy
                                    << "`, Nonce: `" << test.nonce << "`");

    unsigned expected_reports = test.allowed ? 0u : 1u;
    auto* element = MakeGarbageCollected<HTMLScriptElement>(
        *window->document(), CreateElementFlags());

    // Enforce 'script-src'
    Persistent<ContentSecurityPolicy> policy =
        MakeGarbageCollected<ContentSecurityPolicy>();
    policy->BindToDelegate(window->GetContentSecurityPolicyDelegate());
    policy->AddPolicies(ParseContentSecurityPolicies(
        String("script-src ") + test.policy,
        ContentSecurityPolicyType::kEnforce, ContentSecurityPolicySource::kHTTP,
        *secure_origin));
    EXPECT_EQ(test.allowed,
              policy->AllowInline(ContentSecurityPolicy::InlineType::kScript,
                                  element, content, String(test.nonce),
                                  context_url, context_line));
    EXPECT_EQ(expected_reports, policy->violation_reports_sent_.size());

    // Enforce 'style-src'
    policy = MakeGarbageCollected<ContentSecurityPolicy>();
    policy->BindToDelegate(window->GetContentSecurityPolicyDelegate());
    policy->AddPolicies(ParseContentSecurityPolicies(
        String("style-src ") + test.policy, ContentSecurityPolicyType::kEnforce,
        ContentSecurityPolicySource::kHTTP, *secure_origin));
    EXPECT_EQ(test.allowed,
              policy->AllowInline(ContentSecurityPolicy::InlineType::kStyle,
                                  element, content, String(test.nonce),
                                  context_url, context_line));
    EXPECT_EQ(expected_reports, policy->violation_reports_sent_.size());

    // Report 'script-src'
    policy = MakeGarbageCollected<ContentSecurityPolicy>();
    policy->BindToDelegate(window->GetContentSecurityPolicyDelegate());
    policy->AddPolicies(ParseContentSecurityPolicies(
        String("script-src ") + test.policy, ContentSecurityPolicyType::kReport,
        ContentSecurityPolicySource::kHTTP, *secure_origin));
    EXPECT_TRUE(policy->AllowInline(ContentSecurityPolicy::InlineType::kScript,
                                    element, content, String(test.nonce),
                                    context_url, context_line));
    EXPECT_EQ(expected_reports, policy->violation_reports_sent_.size());

    // Report 'style-src'
    policy = MakeGarbageCollected<ContentSecurityPolicy>();
    policy->BindToDelegate(window->GetContentSecurityPolicyDelegate());
    policy->AddPolicies(ParseContentSecurityPolicies(
        String("style-src ") + test.policy, ContentSecurityPolicyType::kReport,
        ContentSecurityPolicySource::kHTTP, *secure_origin));
    EXPECT_TRUE(policy->AllowInline(ContentSecurityPolicy::InlineType::kStyle,
                                    element, content, String(test.nonce),
                                    context_url, context_line));
    EXPECT_EQ(expected_reports, policy->violation_reports_sent_.size());
  }
}

TEST_F(ContentSecurityPolicyTest, NonceMultiplePolicy) {
  struct TestCase {
    const char* policy1;
    const char* policy2;
    const char* url;
    const char* nonce;
    bool allowed1;
    bool allowed2;
  } cases[] = {
      // Passes both:
      {"script-src 'nonce-yay'", "script-src 'nonce-yay'",
       "https://example.com/js", "yay", true, true},
      {"script-src https://example.com", "script-src 'nonce-yay'",
       "https://example.com/js", "yay", true, true},
      {"script-src 'nonce-yay'", "script-src https://example.com",
       "https://example.com/js", "yay", true, true},
      {"script-src https://example.com 'nonce-yay'",
       "script-src https://example.com 'nonce-yay'", "https://example.com/js",
       "yay", true, true},
      {"script-src https://example.com 'nonce-yay'",
       "script-src https://example.com 'nonce-yay'", "https://example.com/js",
       "", true, true},
      {"script-src https://example.com",
       "script-src https://example.com 'nonce-yay'", "https://example.com/js",
       "yay", true, true},
      {"script-src https://example.com 'nonce-yay'",
       "script-src https://example.com", "https://example.com/js", "yay", true,
       true},

      // Fails one:
      {"script-src 'nonce-yay'", "script-src https://example.com",
       "https://example.com/js", "", false, true},
      {"script-src 'nonce-yay'", "script-src 'none'", "https://example.com/js",
       "yay", true, false},
      {"script-src 'nonce-yay'", "script-src https://not.example.com",
       "https://example.com/js", "yay", true, false},

      // Fails both:
      {"script-src 'nonce-yay'", "script-src https://example.com",
       "https://not.example.com/js", "", false, false},
      {"script-src https://example.com", "script-src 'nonce-yay'",
       "https://not.example.com/js", "", false, false},
      {"script-src 'nonce-yay'", "script-src 'none'",
       "https://not.example.com/js", "boo", false, false},
      {"script-src 'nonce-yay'", "script-src https://not.example.com",
       "https://example.com/js", "", false, false},
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message() << "Policy: `" << test.policy1 << "`/`"
                                    << test.policy2 << "`, URL: `" << test.url
                                    << "`, Nonce: `" << test.nonce << "`");
    const KURL resource(test.url);

    unsigned expected_reports =
        test.allowed1 != test.allowed2 ? 1u : (test.allowed1 ? 0u : 2u);

    // Enforce / Report
    Persistent<ContentSecurityPolicy> policy =
        MakeGarbageCollected<ContentSecurityPolicy>();
    policy->BindToDelegate(
        execution_context->GetContentSecurityPolicyDelegate());
    policy->AddPolicies(ParseContentSecurityPolicies(
        test.policy1, ContentSecurityPolicyType::kEnforce,
        ContentSecurityPolicySource::kHTTP, *secure_origin));
    policy->AddPolicies(ParseContentSecurityPolicies(
        test.policy2, ContentSecurityPolicyType::kReport,
        ContentSecurityPolicySource::kHTTP, *secure_origin));
    EXPECT_EQ(test.allowed1,
              policy->AllowScriptFromSource(
                  resource, String(test.nonce), IntegrityMetadataSet(),
                  kParserInserted, resource,
                  ResourceRequest::RedirectStatus::kNoRedirect,
                  ReportingDisposition::kReport,
                  ContentSecurityPolicy::CheckHeaderType::kCheckEnforce));
    EXPECT_TRUE(policy->AllowScriptFromSource(
        resource, String(test.nonce), IntegrityMetadataSet(), kParserInserted,
        resource, ResourceRequest::RedirectStatus::kNoRedirect,
        ReportingDisposition::kReport,
        ContentSecurityPolicy::CheckHeaderType::kCheckReportOnly));
    EXPECT_EQ(expected_reports, policy->violation_reports_sent_.size());

    // Report / Enforce
    policy = MakeGarbageCollected<ContentSecurityPolicy>();
    policy->BindToDelegate(
        execution_context->GetContentSecurityPolicyDelegate());
    policy->AddPolicies(ParseContentSecurityPolicies(
        test.policy1, ContentSecurityPolicyType::kReport,
        ContentSecurityPolicySource::kHTTP, *secure_origin));
    policy->AddPolicies(ParseContentSecurityPolicies(
        test.policy2, ContentSecurityPolicyType::kEnforce,
        ContentSecurityPolicySource::kHTTP, *secure_origin));
    EXPECT_TRUE(policy->AllowScriptFromSource(
        resource, String(test.nonce), IntegrityMetadataSet(), kParserInserted,
        resource, ResourceRequest::RedirectStatus::kNoRedirect,
        ReportingDisposition::kReport,
        ContentSecurityPolicy::CheckHeaderType::kCheckReportOnly));
    EXPECT_EQ(test.allowed2,
              policy->AllowScriptFromSource(
                  resource, String(test.nonce), IntegrityMetadataSet(),
                  kParserInserted, resource,
                  ResourceRequest::RedirectStatus::kNoRedirect,
                  ReportingDisposition::kReport,
                  ContentSecurityPolicy::CheckHeaderType::kCheckEnforce));
    EXPECT_EQ(expected_reports, policy->violation_reports_sent_.size());

    // Enforce / Enforce
    policy = MakeGarbageCollected<ContentSecurityPolicy>();
    policy->BindToDelegate(
        execution_context->GetContentSecurityPolicyDelegate());
    policy->AddPolicies(ParseContentSecurityPolicies(
        test.policy1, ContentSecurityPolicyType::kEnforce,
        ContentSecurityPolicySource::kHTTP, *secure_origin));
    policy->AddPolicies(ParseContentSecurityPolicies(
        test.policy2, ContentSecurityPolicyType::kEnforce,
        ContentSecurityPolicySource::kHTTP, *secure_origin));
    EXPECT_EQ(test.allowed1 && test.allowed2,
              policy->AllowScriptFromSource(
                  resource, String(test.nonce), IntegrityMetadataSet(),
                  kParserInserted, resource,
                  ResourceRequest::RedirectStatus::kNoRedirect,
                  ReportingDisposition::kReport,
                  ContentSecurityPolicy::CheckHeaderType::kCheckEnforce));
    EXPECT_EQ(expected_reports, policy->violation_reports_sent_.size());

    // Report / Report
    policy = MakeGarbageCollected<ContentSecurityPolicy>();
    policy->BindToDelegate(
        execution_context->GetContentSecurityPolicyDelegate());
    policy->AddPolicies(ParseContentSecurityPolicies(
        test.policy1, ContentSecurityPolicyType::kReport,
        ContentSecurityPolicySource::kHTTP, *secure_origin));
    policy->AddPolicies(ParseContentSecurityPolicies(
        test.policy2, ContentSecurityPolicyType::kReport,
        ContentSecurityPolicySource::kHTTP, *secure_origin));
    EXPECT_TRUE(policy->AllowScriptFromSource(
        resource, String(test.nonce), IntegrityMetadataSet(), kParserInserted,
        resource, ResourceRequest::RedirectStatus::kNoRedirect,
        ReportingDisposition::kReport,
        ContentSecurityPolicy::CheckHeaderType::kCheckReportOnly));
    EXPECT_EQ(expected_reports, policy->violation_reports_sent_.size());
  }
}

TEST_F(ContentSecurityPolicyTest, DirectiveType) {
  struct TestCase {
    CSPDirectiveName type;
    const String& name;
  } cases[] = {
      {CSPDirectiveName::BaseURI, "base-uri"},
      {CSPDirectiveName::BlockAllMixedContent, "block-all-mixed-content"},
      {CSPDirectiveName::ChildSrc, "child-src"},
      {CSPDirectiveName::ConnectSrc, "connect-src"},
      {CSPDirectiveName::DefaultSrc, "default-src"},
      {CSPDirectiveName::FencedFrameSrc, "fenced-frame-src"},
      {CSPDirectiveName::FrameAncestors, "frame-ancestors"},
      {CSPDirectiveName::FrameSrc, "frame-src"},
      {CSPDirectiveName::FontSrc, "font-src"},
      {CSPDirectiveName::FormAction, "form-action"},
      {CSPDirectiveName::ImgSrc, "img-src"},
      {CSPDirectiveName::ManifestSrc, "manifest-src"},
      {CSPDirectiveName::MediaSrc, "media-src"},
      {CSPDirectiveName::ObjectSrc, "object-src"},
      {CSPDirectiveName::ReportURI, "report-uri"},
      {CSPDirectiveName::Sandbox, "sandbox"},
      {CSPDirectiveName::ScriptSrc, "script-src"},
      {CSPDirectiveName::ScriptSrcAttr, "script-src-attr"},
      {CSPDirectiveName::ScriptSrcElem, "script-src-elem"},
      {CSPDirectiveName::StyleSrc, "style-src"},
      {CSPDirectiveName::StyleSrcAttr, "style-src-attr"},
      {CSPDirectiveName::StyleSrcElem, "style-src-elem"},
      {CSPDirectiveName::UpgradeInsecureRequests, "upgrade-insecure-requests"},
      {CSPDirectiveName::WorkerSrc, "worker-src"},
  };

  EXPECT_EQ(CSPDirectiveName::Unknown,
            ContentSecurityPolicy::GetDirectiveType("random"));

  for (const auto& test : cases) {
    const String& name_from_type =
        ContentSecurityPolicy::GetDirectiveName(test.type);
    CSPDirectiveName type_from_name =
        ContentSecurityPolicy::GetDirectiveType(test.name);
    EXPECT_EQ(name_from_type, test.name);
    EXPECT_EQ(type_from_name, test.type);
    EXPECT_EQ(test.type,
              ContentSecurityPolicy::GetDirectiveType(name_from_type));
    EXPECT_EQ(test.name,
              ContentSecurityPolicy::GetDirectiveName(type_from_name));
  }
}

TEST_F(ContentSecurityPolicyTest, RequestsAllowedWhenBypassingCSP) {
  const KURL base;
  CreateExecutionContext();
  execution_context->GetSecurityContext().SetSecurityOrigin(
      secure_origin);                     // https://example.com
  execution_context->SetURL(secure_url);  // https://example.com
  csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
  csp->AddPolicies(ParseContentSecurityPolicies(
      "default-src https://example.com", ContentSecurityPolicyType::kEnforce,
      ContentSecurityPolicySource::kHTTP, *secure_origin));

  const KURL example_url("https://example.com/");
  EXPECT_TRUE(csp->AllowRequest(mojom::blink::RequestContextType::OBJECT,
                                network::mojom::RequestDestination::kEmpty,
                                example_url, String(), IntegrityMetadataSet(),
                                kParserInserted, example_url,
                                ResourceRequest::RedirectStatus::kNoRedirect,
                                ReportingDisposition::kSuppressReporting));

  const KURL not_example_url("https://not-example.com/");
  EXPECT_FALSE(csp->AllowRequest(
      mojom::blink::RequestContextType::OBJECT,
      network::mojom::RequestDestination::kEmpty, not_example_url, String(),
      IntegrityMetadataSet(), kParserInserted, not_example_url,
      ResourceRequest::RedirectStatus::kNoRedirect,
      ReportingDisposition::kSuppressReporting));

  // Register "https" as bypassing CSP, which should now bypass it entirely
  SchemeRegistry::RegisterURLSchemeAsBypassingContentSecurityPolicy("https");

  EXPECT_TRUE(csp->AllowRequest(mojom::blink::RequestContextType::OBJECT,
                                network::mojom::RequestDestination::kEmpty,
                                example_url, String(), IntegrityMetadataSet(),
                                kParserInserted, example_url,
                                ResourceRequest::RedirectStatus::kNoRedirect,
                                ReportingDisposition::kSuppressReporting));

  EXPECT_TRUE(csp->AllowRequest(
      mojom::blink::RequestContextType::OBJECT,
      network::mojom::RequestDestination::kEmpty, not_example_url, String(),
      IntegrityMetadataSet(), kParserInserted, not_example_url,
      ResourceRequest::RedirectStatus::kNoRedirect,
      ReportingDisposition::kSuppressReporting));

  SchemeRegistry::RemoveURLSchemeRegisteredAsBypassingContentSecurityPolicy(
      "https");
}
TEST_F(ContentSecurityPolicyTest, FilesystemAllowedWhenBypassingCSP) {
  const KURL base;
  CreateExecutionContext();
  execution_context->GetSecurityContext().SetSecurityOrigin(
      secure_origin);                     // https://example.com
  execution_context->SetURL(secure_url);  // https://example.com
  csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
  csp->AddPolicies(ParseContentSecurityPolicies(
      "default-src https://example.com", ContentSecurityPolicyType::kEnforce,
      ContentSecurityPolicySource::kHTTP, *secure_origin));

  const KURL example_url("filesystem:https://example.com/file.txt");
  EXPECT_FALSE(csp->AllowRequest(mojom::blink::RequestContextType::OBJECT,
                                 network::mojom::RequestDestination::kEmpty,
                                 example_url, String(), IntegrityMetadataSet(),
                                 kParserInserted, example_url,
                                 ResourceRequest::RedirectStatus::kNoRedirect,
                                 ReportingDisposition::kSuppressReporting));

  const KURL not_example_url("filesystem:https://not-example.com/file.txt");
  EXPECT_FALSE(csp->AllowRequest(
      mojom::blink::RequestContextType::OBJECT,
      network::mojom::RequestDestination::kEmpty, not_example_url, String(),
      IntegrityMetadataSet(), kParserInserted, not_example_url,
      ResourceRequest::RedirectStatus::kNoRedirect,
      ReportingDisposition::kSuppressReporting));

  // Register "https" as bypassing CSP, which should now bypass it entirely
  SchemeRegistry::RegisterURLSchemeAsBypassingContentSecurityPolicy("https");

  EXPECT_TRUE(csp->AllowRequest(mojom::blink::RequestContextType::OBJECT,
                                network::mojom::RequestDestination::kEmpty,
                                example_url, String(), IntegrityMetadataSet(),
                                kParserInserted, example_url,
                                ResourceRequest::RedirectStatus::kNoRedirect,
                                ReportingDisposition::kSuppressReporting));

  EXPECT_TRUE(csp->AllowRequest(
      mojom::blink::RequestContextType::OBJECT,
      network::mojom::RequestDestination::kEmpty, not_example_url, String(),
      IntegrityMetadataSet(), kParserInserted, not_example_url,
      ResourceRequest::RedirectStatus::kNoRedirect,
      ReportingDisposition::kSuppressReporting));

  SchemeRegistry::RemoveURLSchemeRegisteredAsBypassingContentSecurityPolicy(
      "https");
}

TEST_F(ContentSecurityPolicyTest, BlobAllowedWhenBypassingCSP) {
  const KURL base;
  CreateExecutionContext();
  execution_context->GetSecurityContext().SetSecurityOrigin(
      secure_origin);                     // https://example.com
  execution_context->SetURL(secure_url);  // https://example.com
  csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
  csp->AddPolicies(ParseContentSecurityPolicies(
      "default-src https://example.com", ContentSecurityPolicyType::kEnforce,
      ContentSecurityPolicySource::kHTTP, *secure_origin));

  const KURL example_url("blob:https://example.com/");
  EXPECT_FALSE(csp->AllowRequest(mojom::blink::RequestContextType::OBJECT,
                                 network::mojom::RequestDestination::kEmpty,
                                 example_url, String(), IntegrityMetadataSet(),
                                 kParserInserted, example_url,
                                 ResourceRequest::RedirectStatus::kNoRedirect,
                                 ReportingDisposition::kSuppressReporting));

  const KURL not_example_url("blob:https://not-example.com/");
  EXPECT_FALSE(csp->AllowRequest(
      mojom::blink::RequestContextType::OBJECT,
      network::mojom::RequestDestination::kEmpty, not_example_url, String(),
      IntegrityMetadataSet(), kParserInserted, not_example_url,
      ResourceRequest::RedirectStatus::kNoRedirect,
      ReportingDisposition::kSuppressReporting));

  // Register "https" as bypassing CSP, which should now bypass it entirely
  SchemeRegistry::RegisterURLSchemeAsBypassingContentSecurityPolicy("https");

  EXPECT_TRUE(csp->AllowRequest(mojom::blink::RequestContextType::OBJECT,
                                network::mojom::RequestDestination::kEmpty,
                                example_url, String(), IntegrityMetadataSet(),
                                kParserInserted, example_url,
                                ResourceRequest::RedirectStatus::kNoRedirect,
                                ReportingDisposition::kSuppressReporting));

  EXPECT_TRUE(csp->AllowRequest(
      mojom::blink::RequestContextType::OBJECT,
      network::mojom::RequestDestination::kEmpty, not_example_url, String(),
      IntegrityMetadataSet(), kParserInserted, not_example_url,
      ResourceRequest::RedirectStatus::kNoRedirect,
      ReportingDisposition::kSuppressReporting));

  SchemeRegistry::RemoveURLSchemeRegisteredAsBypassingContentSecurityPolicy(
      "https");
}

TEST_F(ContentSecurityPolicyTest, CSPBypassDisabledWhenSchemeIsPrivileged) {
  const KURL base;
  CreateExecutionContext();
  execution_context->GetSecurityContext().SetSecurityOrigin(secure_origin);
  execution_context->SetURL(BlankURL());
  csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
  csp->AddPolicies(ParseContentSecurityPolicies(
      "script-src http://example.com", ContentSecurityPolicyType::kEnforce,
      ContentSecurityPolicySource::kHTTP, *secure_origin));

  const KURL allowed_url("http://example.com/script.js");
  const KURL http_url("http://not-example.com/script.js");
  const KURL blob_url(base, "blob:http://not-example.com/uuid");
  const KURL filesystem_url(base, "filesystem:http://not-example.com/file.js");

  // The {Requests,Blob,Filesystem}AllowedWhenBypassingCSP tests have already
  // ensured that RegisterURLSchemeAsBypassingContentSecurityPolicy works as
  // expected.
  //
  // "http" is registered as bypassing CSP, but the context's scheme ("https")
  // is marked as a privileged scheme, so the bypass rule should be ignored.
  SchemeRegistry::RegisterURLSchemeAsBypassingContentSecurityPolicy("http");
  SchemeRegistry::RegisterURLSchemeAsNotAllowingJavascriptURLs("https");

  EXPECT_TRUE(csp->AllowScriptFromSource(
      allowed_url, String(), IntegrityMetadataSet(), kNotParserInserted,
      allowed_url, ResourceRequest::RedirectStatus::kNoRedirect,
      ReportingDisposition::kSuppressReporting));
  EXPECT_FALSE(csp->AllowScriptFromSource(
      http_url, String(), IntegrityMetadataSet(), kNotParserInserted, http_url,
      ResourceRequest::RedirectStatus::kNoRedirect,
      ReportingDisposition::kSuppressReporting));
  EXPECT_FALSE(csp->AllowScriptFromSource(
      blob_url, String(), IntegrityMetadataSet(), kNotParserInserted, blob_url,
      ResourceRequest::RedirectStatus::kNoRedirect,
      ReportingDisposition::kSuppressReporting));
  EXPECT_FALSE(csp->AllowScriptFromSource(
      filesystem_url, String(), IntegrityMetadataSet(), kNotParserInserted,
      filesystem_url, ResourceRequest::RedirectStatus::kNoRedirect,
      ReportingDisposition::kSuppressReporting));

  SchemeRegistry::RemoveURLSchemeRegisteredAsBypassingContentSecurityPolicy(
      "http");
  SchemeRegistry::RemoveURLSchemeAsNotAllowingJavascriptURLs("https");
}

TEST_F(ContentSecurityPolicyTest, TrustedTypesNoDirective) {
  csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
  csp->AddPolicies(ParseContentSecurityPolicies(
      "", ContentSecurityPolicyType::kEnforce,
      ContentSecurityPolicySource::kHTTP, *secure_origin));

  ContentSecurityPolicy::AllowTrustedTypePolicyDetails violation_details;
  EXPECT_TRUE(
      csp->AllowTrustedTypePolicy("somepolicy", false, violation_details));
  EXPECT_EQ(violation_details,
            ContentSecurityPolicy::AllowTrustedTypePolicyDetails::kAllowed);
  EXPECT_TRUE(
      csp->AllowTrustedTypePolicy("somepolicy", true, violation_details));
  EXPECT_EQ(violation_details,
            ContentSecurityPolicy::AllowTrustedTypePolicyDetails::kAllowed);
}

TEST_F(ContentSecurityPolicyTest, TrustedTypesSimpleDirective) {
  csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
  csp->AddPolicies(ParseContentSecurityPolicies(
      "trusted-types one two three", ContentSecurityPolicyType::kEnforce,
      ContentSecurityPolicySource::kHTTP, *secure_origin));
}

TEST_F(ContentSecurityPolicyTest, TrustedTypesWhitespace) {
  csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
  csp->AddPolicies(ParseContentSecurityPolicies(
      "trusted-types one\ntwo\rthree", ContentSecurityPolicyType::kEnforce,
      ContentSecurityPolicySource::kHTTP, *secure_origin));

  ContentSecurityPolicy::AllowTrustedTypePolicyDetails violation_details;
  EXPECT_TRUE(csp->AllowTrustedTypePolicy("one", false, violation_details));
  EXPECT_EQ(violation_details,
            ContentSecurityPolicy::AllowTrustedTypePolicyDetails::kAllowed);
  EXPECT_TRUE(csp->AllowTrustedTypePolicy("two", false, violation_details));
  EXPECT_EQ(violation_details,
            ContentSecurityPolicy::AllowTrustedTypePolicyDetails::kAllowed);
  EXPECT_TRUE(csp->AllowTrustedTypePolicy("three", false, violation_details));
  EXPECT_EQ(violation_details,
            ContentSecurityPolicy::AllowTrustedTypePolicyDetails::kAllowed);

  EXPECT_FALSE(csp->AllowTrustedTypePolicy("four", false, violation_details));
  EXPECT_EQ(
      violation_details,
      ContentSecurityPolicy::AllowTrustedTypePolicyDetails::kDisallowedName);
  EXPECT_FALSE(csp->AllowTrustedTypePolicy("one", true, violation_details));
  EXPECT_EQ(violation_details,
            ContentSecurityPolicy::AllowTrustedTypePolicyDetails::
                kDisallowedDuplicateName);
  EXPECT_FALSE(csp->AllowTrustedTypePolicy("four", true, violation_details));
  EXPECT_EQ(violation_details,
            ContentSecurityPolicy::AllowTrustedTypePolicyDetails::
                kDisallowedDuplicateName);
}

TEST_F(ContentSecurityPolicyTest, TrustedTypesEmpty) {
  csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
  csp->AddPolicies(ParseContentSecurityPolicies(
      "trusted-types", ContentSecurityPolicyType::kEnforce,
      ContentSecurityPolicySource::kHTTP, *secure_origin));

  ContentSecurityPolicy::AllowTrustedTypePolicyDetails violation_details;
  EXPECT_FALSE(
      csp->AllowTrustedTypePolicy("somepolicy", false, violation_details));
  EXPECT_EQ(
      violation_details,
      ContentSecurityPolicy::AllowTrustedTypePolicyDetails::kDisallowedName);
  EXPECT_FALSE(
      csp->AllowTrustedTypePolicy("somepolicy", true, violation_details));
  EXPECT_EQ(violation_details,
            ContentSecurityPolicy::AllowTrustedTypePolicyDetails::
                kDisallowedDuplicateName);
}

TEST_F(ContentSecurityPolicyTest, TrustedTypesStar) {
  csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
  csp->AddPolicies(ParseContentSecurityPolicies(
      "trusted-types *", ContentSecurityPolicyType::kEnforce,
      ContentSecurityPolicySource::kHTTP, *secure_origin));

  ContentSecurityPolicy::AllowTrustedTypePolicyDetails violation_details;
  EXPECT_TRUE(
      csp->AllowTrustedTypePolicy("somepolicy", false, violation_details));
  EXPECT_EQ(violation_details,
            ContentSecurityPolicy::AllowTrustedTypePolicyDetails::kAllowed);
  EXPECT_FALSE(
      csp->AllowTrustedTypePolicy("somepolicy", true, violation_details));
  EXPECT_EQ(violation_details,
            ContentSecurityPolicy::AllowTrustedTypePolicyDetails::
                kDisallowedDuplicateName);
}

TEST_F(ContentSecurityPolicyTest, TrustedTypesStarMix) {
  csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
  csp->AddPolicies(ParseContentSecurityPolicies(
      "trusted-types abc * def", ContentSecurityPolicyType::kEnforce,
      ContentSecurityPolicySource::kHTTP, *secure_origin));

  ContentSecurityPolicy::AllowTrustedTypePolicyDetails violation_details;
  EXPECT_TRUE(csp->AllowTrustedTypePolicy("abc", false, violation_details));
  EXPECT_EQ(violation_details,
            ContentSecurityPolicy::AllowTrustedTypePolicyDetails::kAllowed);
  EXPECT_TRUE(csp->AllowTrustedTypePolicy("def", false, violation_details));
  EXPECT_EQ(violation_details,
            ContentSecurityPolicy::AllowTrustedTypePolicyDetails::kAllowed);
  EXPECT_TRUE(csp->AllowTrustedTypePolicy("ghi", false, violation_details));
  EXPECT_EQ(violation_details,
            ContentSecurityPolicy::AllowTrustedTypePolicyDetails::kAllowed);

  EXPECT_FALSE(csp->AllowTrustedTypePolicy("abc", true, violation_details));
  EXPECT_EQ(violation_details,
            ContentSecurityPolicy::AllowTrustedTypePolicyDetails::
                kDisallowedDuplicateName);
  EXPECT_FALSE(csp->AllowTrustedTypePolicy("def", true, violation_details));
  EXPECT_EQ(violation_details,
            ContentSecurityPolicy::AllowTrustedTypePolicyDetails::
                kDisallowedDuplicateName);
  EXPECT_FALSE(csp->AllowTrustedTypePolicy("ghi", true, violation_details));
  EXPECT_EQ(violation_details,
            ContentSecurityPolicy::AllowTrustedTypePolicyDetails::
                kDisallowedDuplicateName);
}

TEST_F(ContentSecurityPolicyTest, TrustedTypeDupe) {
  csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
  csp->AddPolicies(ParseContentSecurityPolicies(
      "trusted-types somepolicy 'allow-duplicates'",
      ContentSecurityPolicyType::kEnforce, ContentSecurityPolicySource::kHTTP,
      *secure_origin));

  ContentSecurityPolicy::AllowTrustedTypePolicyDetails violation_details;
  EXPECT_TRUE(
      csp->AllowTrustedTypePolicy("somepolicy", false, violation_details));
  EXPECT_EQ(violation_details,
            ContentSecurityPolicy::AllowTrustedTypePolicyDetails::kAllowed);
  EXPECT_TRUE(
      csp->AllowTrustedTypePolicy("somepolicy", true, violation_details));
  EXPECT_EQ(violation_details,
            ContentSecurityPolicy::AllowTrustedTypePolicyDetails::kAllowed);
}

TEST_F(ContentSecurityPolicyTest, TrustedTypeDupeStar) {
  csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
  csp->AddPolicies(ParseContentSecurityPolicies(
      "trusted-types * 'allow-duplicates'", ContentSecurityPolicyType::kEnforce,
      ContentSecurityPolicySource::kHTTP, *secure_origin));

  ContentSecurityPolicy::AllowTrustedTypePolicyDetails violation_details;
  EXPECT_TRUE(
      csp->AllowTrustedTypePolicy("somepolicy", false, violation_details));
  EXPECT_EQ(violation_details,
            ContentSecurityPolicy::AllowTrustedTypePolicyDetails::kAllowed);
  EXPECT_TRUE(
      csp->AllowTrustedTypePolicy("somepolicy", true, violation_details));
  EXPECT_EQ(violation_details,
            ContentSecurityPolicy::AllowTrustedTypePolicyDetails::kAllowed);
}

TEST_F(ContentSecurityPolicyTest, TrustedTypesReserved) {
  csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
  csp->AddPolicies(ParseContentSecurityPolicies(
      "trusted-types one \"two\" 'three'", ContentSecurityPolicyType::kEnforce,
      ContentSecurityPolicySource::kHTTP, *secure_origin));

  ContentSecurityPolicy::AllowTrustedTypePolicyDetails violation_details;
  EXPECT_TRUE(csp->AllowTrustedTypePolicy("one", false, violation_details));
  EXPECT_EQ(violation_details,
            ContentSecurityPolicy::AllowTrustedTypePolicyDetails::kAllowed);
  EXPECT_FALSE(csp->AllowTrustedTypePolicy("one", true, violation_details));
  EXPECT_EQ(violation_details,
            ContentSecurityPolicy::AllowTrustedTypePolicyDetails::
                kDisallowedDuplicateName);

  // Quoted strings are considered 'reserved':
  EXPECT_FALSE(csp->AllowTrustedTypePolicy("two", false, violation_details));
  EXPECT_EQ(
      violation_details,
      ContentSecurityPolicy::AllowTrustedTypePolicyDetails::kDisallowedName);
  EXPECT_FALSE(
      csp->AllowTrustedTypePolicy("\"two\"", false, violation_details));
  EXPECT_EQ(
      violation_details,
      ContentSecurityPolicy::AllowTrustedTypePolicyDetails::kDisallowedName);
  EXPECT_FALSE(csp->AllowTrustedTypePolicy("three", false, violation_details));
  EXPECT_EQ(
      violation_details,
      ContentSecurityPolicy::AllowTrustedTypePolicyDetails::kDisallowedName);
  EXPECT_FALSE(
      csp->AllowTrustedTypePolicy("'three'", false, violation_details));
  EXPECT_EQ(
      violation_details,
      ContentSecurityPolicy::AllowTrustedTypePolicyDetails::kDisallowedName);
  EXPECT_FALSE(csp->AllowTrustedTypePolicy("two", true, violation_details));
  EXPECT_EQ(violation_details,
            ContentSecurityPolicy::AllowTrustedTypePolicyDetails::
                kDisallowedDuplicateName);
  EXPECT_FALSE(csp->AllowTrustedTypePolicy("\"two\"", true, violation_details));
  EXPECT_EQ(violation_details,
            ContentSecurityPolicy::AllowTrustedTypePolicyDetails::
                kDisallowedDuplicateName);
  EXPECT_FALSE(csp->AllowTrustedTypePolicy("three", true, violation_details));
  EXPECT_EQ(violation_details,
            ContentSecurityPolicy::AllowTrustedTypePolicyDetails::
                kDisallowedDuplicateName);
  EXPECT_FALSE(csp->AllowTrustedTypePolicy("'three'", true, violation_details));
  EXPECT_EQ(violation_details,
            ContentSecurityPolicy::AllowTrustedTypePolicyDetails::
                kDisallowedDuplicateName);
}

TEST_F(ContentSecurityPolicyTest, TrustedTypesReportingStar) {
  csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
  csp->AddPolicies(ParseContentSecurityPolicies(
      "trusted-types *", ContentSecurityPolicyType::kReport,
      ContentSecurityPolicySource::kHTTP, *secure_origin));

  ContentSecurityPolicy::AllowTrustedTypePolicyDetails violation_details;
  EXPECT_TRUE(
      csp->AllowTrustedTypePolicy("somepolicy", false, violation_details));
  EXPECT_EQ(violation_details,
            ContentSecurityPolicy::AllowTrustedTypePolicyDetails::kAllowed);
  EXPECT_TRUE(
      csp->AllowTrustedTypePolicy("somepolicy", true, violation_details));
  EXPECT_EQ(violation_details,
            ContentSecurityPolicy::AllowTrustedTypePolicyDetails::
                kDisallowedDuplicateName);
}

TEST_F(ContentSecurityPolicyTest, TrustedTypeReportingSimple) {
  csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
  csp->AddPolicies(ParseContentSecurityPolicies(
      "trusted-types a b c", ContentSecurityPolicyType::kReport,
      ContentSecurityPolicySource::kHTTP, *secure_origin));

  ContentSecurityPolicy::AllowTrustedTypePolicyDetails violation_details;
  EXPECT_TRUE(csp->AllowTrustedTypePolicy("a", false, violation_details));
  EXPECT_EQ(violation_details,
            ContentSecurityPolicy::AllowTrustedTypePolicyDetails::kAllowed);
  EXPECT_TRUE(csp->AllowTrustedTypePolicy("a", true, violation_details));
  EXPECT_EQ(violation_details,
            ContentSecurityPolicy::AllowTrustedTypePolicyDetails::
                kDisallowedDuplicateName);
}

TEST_F(ContentSecurityPolicyTest, TrustedTypeEnforce) {
  csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
  csp->AddPolicies(ParseContentSecurityPolicies(
      "trusted-types one\ntwo\rthree", ContentSecurityPolicyType::kEnforce,
      ContentSecurityPolicySource::kHTTP, *secure_origin));
  EXPECT_FALSE(csp->IsRequireTrustedTypes());
  EXPECT_TRUE(csp->AllowTrustedTypeAssignmentFailure("blabla"));
}

TEST_F(ContentSecurityPolicyTest, TrustedTypeReport) {
  csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
  csp->AddPolicies(ParseContentSecurityPolicies(
      "trusted-types one\ntwo\rthree", ContentSecurityPolicyType::kReport,
      ContentSecurityPolicySource::kHTTP, *secure_origin));
  EXPECT_FALSE(csp->IsRequireTrustedTypes());
  EXPECT_TRUE(csp->AllowTrustedTypeAssignmentFailure("blabla"));
}

TEST_F(ContentSecurityPolicyTest, TrustedTypeReportAndEnforce) {
  csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
  csp->AddPolicies(ParseContentSecurityPolicies(
      "trusted-types one", ContentSecurityPolicyType::kReport,
      ContentSecurityPolicySource::kHTTP, *secure_origin));
  csp->AddPolicies(ParseContentSecurityPolicies(
      "trusted-types two", ContentSecurityPolicyType::kEnforce,
      ContentSecurityPolicySource::kHTTP, *secure_origin));
  EXPECT_FALSE(csp->IsRequireTrustedTypes());
  EXPECT_TRUE(csp->AllowTrustedTypeAssignmentFailure("blabla"));
}

TEST_F(ContentSecurityPolicyTest, TrustedTypeReportAndNonTTEnforce) {
  csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
  csp->AddPolicies(ParseContentSecurityPolicies(
      "trusted-types one", ContentSecurityPolicyType::kReport,
      ContentSecurityPolicySource::kHTTP, *secure_origin));
  csp->AddPolicies(ParseContentSecurityPolicies(
      "script-src none", ContentSecurityPolicyType::kEnforce,
      ContentSecurityPolicySource::kHTTP, *secure_origin));
  EXPECT_FALSE(csp->IsRequireTrustedTypes());
  EXPECT_TRUE(csp->AllowTrustedTypeAssignmentFailure("blabla"));
}

TEST_F(ContentSecurityPolicyTest, RequireTrustedTypeForEnforce) {
  execution_context->SetRequireTrustedTypesForTesting();
  csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
  csp->AddPolicies(ParseContentSecurityPolicies(
      "require-trusted-types-for ''", ContentSecurityPolicyType::kEnforce,
      ContentSecurityPolicySource::kHTTP, *secure_origin));
  EXPECT_FALSE(csp->IsRequireTrustedTypes());

  csp->AddPolicies(ParseContentSecurityPolicies(
      "require-trusted-types-for 'script'", ContentSecurityPolicyType::kEnforce,
      ContentSecurityPolicySource::kHTTP, *secure_origin));
  EXPECT_TRUE(csp->IsRequireTrustedTypes());
}

TEST_F(ContentSecurityPolicyTest, RequireTrustedTypeForReport) {
  execution_context->SetRequireTrustedTypesForTesting();
  csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
  csp->AddPolicies(ParseContentSecurityPolicies(
      "require-trusted-types-for 'script'", ContentSecurityPolicyType::kReport,
      ContentSecurityPolicySource::kHTTP, *secure_origin));
  EXPECT_TRUE(csp->IsRequireTrustedTypes());
}

TEST_F(ContentSecurityPolicyTest, DefaultPolicy) {
  csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
  csp->AddPolicies(ParseContentSecurityPolicies(
      "trusted-types *", ContentSecurityPolicyType::kEnforce,
      ContentSecurityPolicySource::kHTTP, *secure_origin));

  ContentSecurityPolicy::AllowTrustedTypePolicyDetails violation_details;
  EXPECT_TRUE(csp->AllowTrustedTypePolicy("default", false, violation_details));
  EXPECT_EQ(violation_details,
            ContentSecurityPolicy::AllowTrustedTypePolicyDetails::kAllowed);
  EXPECT_FALSE(csp->AllowTrustedTypePolicy("default", true, violation_details));
  EXPECT_EQ(violation_details,
            ContentSecurityPolicy::AllowTrustedTypePolicyDetails::
                kDisallowedDuplicateName);
}

TEST_F(ContentSecurityPolicyTest, DirectiveNameCaseInsensitive) {
  KURL example_url("http://example.com");
  KURL not_example_url("http://not-example.com");

  // Directive name is case insensitive.
  csp = MakeGarbageCollected<ContentSecurityPolicy>();
  csp->AddPolicies(ParseContentSecurityPolicies(
      "sCrIpt-sRc http://example.com", ContentSecurityPolicyType::kEnforce,
      ContentSecurityPolicySource::kHTTP, *secure_origin));
  csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());

  EXPECT_TRUE(csp->AllowScriptFromSource(
      example_url, String(), IntegrityMetadataSet(), kParserInserted,
      example_url, ResourceRequest::RedirectStatus::kNoRedirect));
  EXPECT_FALSE(csp->AllowScriptFromSource(
      not_example_url, String(), IntegrityMetadataSet(), kParserInserted,
      not_example_url, ResourceRequest::RedirectStatus::kNoRedirect));

  // Duplicate directive that is in a different case pattern is
  // correctly treated as a duplicate directive and ignored.
  csp = MakeGarbageCollected<ContentSecurityPolicy>();
  csp->AddPolicies(ParseContentSecurityPolicies(
      "SCRipt-SRC http://example.com; script-src http://not-example.com;",
      ContentSecurityPolicyType::kEnforce, ContentSecurityPolicySource::kHTTP,
      *secure_origin));
  csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());

  EXPECT_TRUE(csp->AllowScriptFromSource(
      example_url, String(), IntegrityMetadataSet(), kParserInserted,
      example_url, ResourceRequest::RedirectStatus::kNoRedirect));
  EXPECT_FALSE(csp->AllowScriptFromSource(
      not_example_url, String(), IntegrityMetadataSet(), kParserInserted,
      not_example_url, ResourceRequest::RedirectStatus::kNoRedirect));
}

// Tests that using an empty CSP works and doesn't impose any policy
// restrictions.
TEST_F(ContentSecurityPolicyTest, EmptyCSPIsNoOp) {
  csp = MakeGarbageCollected<ContentSecurityPolicy>();
  csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());

  const KURL example_url("http://example.com");
  auto* document = Document::CreateForTest(*execution_context);
  String source;
  String context_url;
  String nonce;
  OrdinalNumber ordinal_number = OrdinalNumber::First();
  auto* element =
      MakeGarbageCollected<HTMLScriptElement>(*document, CreateElementFlags());

  EXPECT_TRUE(csp->AllowInline(ContentSecurityPolicy::InlineType::kNavigation,
                               element, source, String() /* nonce */,
                               context_url, ordinal_number));
  EXPECT_TRUE(csp->AllowInline(
      ContentSecurityPolicy::InlineType::kScriptAttribute, element, source,
      String() /* nonce */, context_url, ordinal_number));
  EXPECT_TRUE(csp->AllowEval(ReportingDisposition::kReport,
                             ContentSecurityPolicy::kWillNotThrowException,
                             g_empty_string));
  EXPECT_TRUE(csp->AllowWasmCodeGeneration(
      ReportingDisposition::kReport,
      ContentSecurityPolicy::kWillNotThrowException, g_empty_string));

  CSPDirectiveName types_to_test[] = {
      CSPDirectiveName::BaseURI,       CSPDirectiveName::ConnectSrc,
      CSPDirectiveName::FontSrc,       CSPDirectiveName::FormAction,
      CSPDirectiveName::FrameSrc,      CSPDirectiveName::ImgSrc,
      CSPDirectiveName::ManifestSrc,   CSPDirectiveName::MediaSrc,
      CSPDirectiveName::ObjectSrc,     CSPDirectiveName::ScriptSrcElem,
      CSPDirectiveName::StyleSrcElem,  CSPDirectiveName::WorkerSrc,
      CSPDirectiveName::FencedFrameSrc};
  for (auto type : types_to_test) {
    EXPECT_TRUE(
        csp->AllowFromSource(type, example_url, example_url,
                             ResourceRequest::RedirectStatus::kNoRedirect));
  }

  EXPECT_TRUE(csp->AllowObjectFromSource(example_url));
  EXPECT_TRUE(csp->AllowImageFromSource(
      example_url, example_url, ResourceRequest::RedirectStatus::kNoRedirect));
  EXPECT_TRUE(csp->AllowMediaFromSource(example_url));
  EXPECT_TRUE(csp->AllowConnectToSource(
      example_url, example_url, ResourceRequest::RedirectStatus::kNoRedirect));
  EXPECT_TRUE(csp->AllowFormAction(example_url));
  EXPECT_TRUE(csp->AllowBaseURI(example_url));
  EXPECT_TRUE(csp->AllowWorkerContextFromSource(example_url));
  EXPECT_TRUE(csp->AllowScriptFromSource(
      example_url, nonce, IntegrityMetadataSet(), kParserInserted, example_url,
      ResourceRequest::RedirectStatus::kNoRedirect));

  ContentSecurityPolicy::AllowTrustedTypePolicyDetails violation_details;

  EXPECT_TRUE(
      csp->AllowTrustedTypePolicy("somepolicy", true, violation_details));
  EXPECT_EQ(violation_details,
            ContentSecurityPolicy::AllowTrustedTypePolicyDetails::kAllowed);
  EXPECT_TRUE(
      csp->AllowTrustedTypePolicy("somepolicy", false, violation_details));
  EXPECT_EQ(violation_details,
            ContentSecurityPolicy::AllowTrustedTypePolicyDetails::kAllowed);
  EXPECT_TRUE(csp->AllowInline(ContentSecurityPolicy::InlineType::kScript,
                               element, source, nonce, context_url,
                               ordinal_number));
  EXPECT_TRUE(csp->AllowInline(ContentSecurityPolicy::InlineType::kStyle,
                               element, source, nonce, context_url,
                               ordinal_number));
  EXPECT_TRUE(csp->AllowRequest(mojom::blink::RequestContextType::SCRIPT,
                                network::mojom::RequestDestination::kScript,
                                example_url, nonce, IntegrityMetadataSet(),
                                kParserInserted, example_url,
                                ResourceRequest::RedirectStatus::kNoRedirect));
  EXPECT_FALSE(csp->IsActive());
  EXPECT_FALSE(csp->IsActiveForConnections());
  EXPECT_TRUE(csp->FallbackUrlForPlugin().IsEmpty());
  EXPECT_EQ(mojom::blink::InsecureRequestPolicy::kLeaveInsecureRequestsAlone,
            csp->GetInsecureRequestPolicy());
  EXPECT_FALSE(csp->HasHeaderDeliveredPolicy());
  EXPECT_FALSE(csp->SupportsWasmEval());
  EXPECT_EQ(network::mojom::blink::WebSandboxFlags::kNone,
            csp->GetSandboxMask());
  EXPECT_FALSE(csp->HasPolicyFromSource(ContentSecurityPolicySource::kHTTP));
}

TEST_F(ContentSecurityPolicyTest, WasmUnsafeEvalCSPEnable) {
  csp = MakeGarbageCollected<ContentSecurityPolicy>();
  csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());

  csp->AddPolicies(ParseContentSecurityPolicies(
      "script-src 'wasm-unsafe-eval'", ContentSecurityPolicyType::kEnforce,
      ContentSecurityPolicySource::kHTTP, *secure_origin));

  EXPECT_TRUE(csp->AllowWasmCodeGeneration(
      ReportingDisposition::kReport,
      ContentSecurityPolicy::kWillNotThrowException, g_empty_string));
}

TEST_F(ContentSecurityPolicyTest, OpaqueOriginBeforeBind) {
  const KURL url("https://example.test");

  // Security Origin of execution context might change when sandbox flags
  // are applied. This shouldn't change the application of the 'self'
  // determination.
  secure_origin = secure_origin->DeriveNewOpaqueOrigin();
  CreateExecutionContext();
  csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
  csp->AddPolicies(ParseContentSecurityPolicies(
      "default-src 'self';", ContentSecurityPolicyType::kEnforce,
      ContentSecurityPolicySource::kMeta, *secure_origin));
  EXPECT_TRUE(csp->AllowRequest(mojom::blink::RequestContextType::SUBRESOURCE,
                                network::mojom::RequestDestination::kEmpty, url,
                                String(), IntegrityMetadataSet(),
                                kParserInserted, url,
                                ResourceRequest::RedirectStatus::kNoRedirect,
                                ReportingDisposition::kSuppressReporting));
}

TEST_F(ContentSecurityPolicyTest, SelfForDataMatchesNothing) {
  const KURL url("https://example.test");
  auto reference_origin = SecurityOrigin::Create(url);
  const KURL data_url("data:text/html,hello");
  secure_origin = SecurityOrigin::CreateWithReferenceOrigin(
      data_url, reference_origin.get());

  CreateExecutionContext();
  csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
  csp->AddPolicies(ParseContentSecurityPolicies(
      "default-src 'self';", ContentSecurityPolicyType::kEnforce,
      ContentSecurityPolicySource::kMeta, *secure_origin));
  EXPECT_TRUE(csp->AllowRequest(mojom::blink::RequestContextType::SUBRESOURCE,
                                network::mojom::RequestDestination::kEmpty, url,
                                String(), IntegrityMetadataSet(),
                                kParserInserted, url,
                                ResourceRequest::RedirectStatus::kNoRedirect,
                                ReportingDisposition::kSuppressReporting));
  EXPECT_FALSE(csp->AllowRequest(mojom::blink::RequestContextType::SUBRESOURCE,
                                 network::mojom::RequestDestination::kEmpty,
                                 data_url, String(), IntegrityMetadataSet(),
                                 kParserInserted, url,
                                 ResourceRequest::RedirectStatus::kNoRedirect,
                                 ReportingDisposition::kSuppressReporting));
}

TEST_F(ContentSecurityPolicyTest, IsStrictPolicyEnforced) {
  // No policy, no strictness.
  csp = MakeGarbageCollected<ContentSecurityPolicy>();
  EXPECT_FALSE(csp->IsStrictPolicyEnforced());

  // Strict policy, strictness.
  const char* strict_policy =
      "object-src 'none'; "
      "script-src 'nonce-abc' 'unsafe-inline' 'unsafe-eval' 'strict-dynamic' "
      "           https: http:;"
      "base-uri 'none';";
  csp->AddPolicies(ParseContentSecurityPolicies(
      strict_policy, ContentSecurityPolicyType::kEnforce,
      ContentSecurityPolicySource::kHTTP, *secure_origin));
  EXPECT_TRUE(csp->IsStrictPolicyEnforced());

  // Report-only strict policy, no strictness.
  csp = MakeGarbageCollected<ContentSecurityPolicy>();
  csp->AddPolicies(ParseContentSecurityPolicies(
      strict_policy, ContentSecurityPolicyType::kReport,
      ContentSecurityPolicySource::kHTTP, *secure_origin));
  EXPECT_FALSE(csp->IsStrictPolicyEnforced());

  // Composed strict policy, strictness.
  const char* strict_object = "object-src 'none';";
  const char* strict_script = "script-src 'none';";
  const char* strict_base = "base-uri 'none';";
  csp = MakeGarbageCollected<ContentSecurityPolicy>();
  csp->AddPolicies(ParseContentSecurityPolicies(
      strict_object, ContentSecurityPolicyType::kEnforce,
      ContentSecurityPolicySource::kHTTP, *secure_origin));
  EXPECT_FALSE(csp->IsStrictPolicyEnforced());
  csp->AddPolicies(ParseContentSecurityPolicies(
      strict_script, ContentSecurityPolicyType::kEnforce,
      ContentSecurityPolicySource::kHTTP, *secure_origin));
  EXPECT_FALSE(csp->IsStrictPolicyEnforced());
  csp->AddPolicies(ParseContentSecurityPolicies(
      strict_base, ContentSecurityPolicyType::kEnforce,
      ContentSecurityPolicySource::kHTTP, *secure_origin));
  EXPECT_TRUE(csp->IsStrictPolicyEnforced());
}

TEST_F(ContentSecurityPolicyTest, ReasonableRestrictionMetrics) {
  struct TestCase {
    const char* header;
    bool expected_object;
    bool expected_base;
    bool expected_script;
  } cases[] = {{"object-src 'none'", true, false, false},
               {"object-src 'none'; base-uri 'none'", true, true, false},
               {"object-src 'none'; base-uri 'none'; script-src 'none'", true,
                true, true},
               {"object-src 'none'; base-uri 'none'; script-src 'nonce-abc'",
                true, true, true},
               {"object-src 'none'; base-uri 'none'; script-src 'sha256-abc'",
                true, true, true},
               {"object-src 'none'; base-uri 'none'; script-src 'nonce-abc' "
                "'strict-dynamic'",
                true, true, true},
               {"object-src 'none'; base-uri 'none'; script-src 'sha256-abc' "
                "'strict-dynamic'",
                true, true, true},
               {"object-src 'none'; base-uri 'none'; script-src 'sha256-abc' "
                "https://example.com/",
                true, true, false},
               {"object-src 'none'; base-uri 'none'; script-src 'sha256-abc' "
                "https://example.com/ 'strict-dynamic'",
                true, true, true}};

  // Enforced
  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message()
                 << "[Enforce] Header: `" << test.header << "`");
    csp = MakeGarbageCollected<ContentSecurityPolicy>();
    csp->AddPolicies(ParseContentSecurityPolicies(
        test.header, ContentSecurityPolicyType::kEnforce,
        ContentSecurityPolicySource::kHTTP, *secure_origin));
    auto dummy = std::make_unique<DummyPageHolder>();
    csp->BindToDelegate(
        dummy->GetFrame().DomWindow()->GetContentSecurityPolicyDelegate());

    EXPECT_EQ(test.expected_object,
              dummy->GetDocument().IsUseCounted(
                  WebFeature::kCSPWithReasonableObjectRestrictions));
    EXPECT_EQ(test.expected_base,
              dummy->GetDocument().IsUseCounted(
                  WebFeature::kCSPWithReasonableBaseRestrictions));
    EXPECT_EQ(test.expected_script,
              dummy->GetDocument().IsUseCounted(
                  WebFeature::kCSPWithReasonableScriptRestrictions));
    EXPECT_EQ(
        test.expected_object && test.expected_base && test.expected_script,
        dummy->GetDocument().IsUseCounted(
            WebFeature::kCSPWithReasonableRestrictions));
  }

  // Report-Only
  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message()
                 << "[ReportOnly] Header: `" << test.header << "`");
    csp = MakeGarbageCollected<ContentSecurityPolicy>();
    csp->AddPolicies(ParseContentSecurityPolicies(
        test.header, ContentSecurityPolicyType::kReport,
        ContentSecurityPolicySource::kHTTP, *secure_origin));
    auto dummy = std::make_unique<DummyPageHolder>();
    csp->BindToDelegate(
        dummy->GetFrame().DomWindow()->GetContentSecurityPolicyDelegate());

    EXPECT_EQ(test.expected_object,
              dummy->GetDocument().IsUseCounted(
                  WebFeature::kCSPROWithReasonableObjectRestrictions));
    EXPECT_EQ(test.expected_base,
              dummy->GetDocument().IsUseCounted(
                  WebFeature::kCSPROWithReasonableBaseRestrictions));
    EXPECT_EQ(test.expected_script,
              dummy->GetDocument().IsUseCounted(
                  WebFeature::kCSPROWithReasonableScriptRestrictions));
    EXPECT_EQ(
        test.expected_object && test.expected_base && test.expected_script,
        dummy->GetDocument().IsUseCounted(
            WebFeature::kCSPROWithReasonableRestrictions));
  }
}

TEST_F(ContentSecurityPolicyTest, BetterThanReasonableRestrictionMetrics) {
  struct TestCase {
    const char* header;
    bool expected;
  } cases[] = {
      {"object-src 'none'", false},
      {"object-src 'none'; base-uri 'none'", false},
      {"object-src 'none'; base-uri 'none'; script-src 'none'", true},
      {"object-src 'none'; base-uri 'none'; script-src 'nonce-abc'", true},
      {"object-src 'none'; base-uri 'none'; script-src 'sha256-abc'", true},
      {"object-src 'none'; base-uri 'none'; script-src 'nonce-abc' "
       "'strict-dynamic'",
       false},
      {"object-src 'none'; base-uri 'none'; script-src 'sha256-abc' "
       "'strict-dynamic'",
       false},
      {"object-src 'none'; base-uri 'none'; script-src 'sha256-abc' "
       "https://example.com/",
       false},
      {"object-src 'none'; base-uri 'none'; script-src 'sha256-abc' "
       "https://example.com/ 'strict-dynamic'",
       false}};

  // Enforced
  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message()
                 << "[Enforce] Header: `" << test.header << "`");
    csp = MakeGarbageCollected<ContentSecurityPolicy>();
    csp->AddPolicies(ParseContentSecurityPolicies(
        test.header, ContentSecurityPolicyType::kEnforce,
        ContentSecurityPolicySource::kHTTP, *secure_origin));
    auto dummy = std::make_unique<DummyPageHolder>();
    csp->BindToDelegate(
        dummy->GetFrame().DomWindow()->GetContentSecurityPolicyDelegate());

    EXPECT_EQ(test.expected,
              dummy->GetDocument().IsUseCounted(
                  WebFeature::kCSPWithBetterThanReasonableRestrictions));
  }

  // Report-Only
  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message()
                 << "[ReportOnly] Header: `" << test.header << "`");
    csp = MakeGarbageCollected<ContentSecurityPolicy>();
    csp->AddPolicies(ParseContentSecurityPolicies(
        test.header, ContentSecurityPolicyType::kReport,
        ContentSecurityPolicySource::kHTTP, *secure_origin));
    auto dummy = std::make_unique<DummyPageHolder>();
    csp->BindToDelegate(
        dummy->GetFrame().DomWindow()->GetContentSecurityPolicyDelegate());

    EXPECT_EQ(test.expected,
              dummy->GetDocument().IsUseCounted(
                  WebFeature::kCSPROWithBetterThanReasonableRestrictions));
  }
}

TEST_F(ContentSecurityPolicyTest, AllowFencedFrameOpaqueURL) {
  struct TestCase {
    const char* header;
    bool expected;
  } cases[] = {
      {"fenced-frame-src 'none'", false},
      {"fenced-frame-src http://", false},
      {"fenced-frame-src http://*:*", false},
      {"fenced-frame-src http://*.domain", false},
      {"fenced-frame-src https://*:80", false},
      {"fenced-frame-src https://localhost:*", false},
      {"fenced-frame-src https://localhost:80", false},
      // "https://*" is not allowed as it could leak data about ports.
      {"fenced-frame-src https://*", false},
      {"fenced-frame-src *", true},
      {"fenced-frame-src https:", true},
      {"fenced-frame-src https://*:*", true},
      {"fenced-frame-src https: wss:", true},
      {"fenced-frame-src https:; fenced-frame-src wss:", true},
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message() << "Header: `" << test.header << "`");
    csp = MakeGarbageCollected<ContentSecurityPolicy>();
    csp->AddPolicies(ParseContentSecurityPolicies(
        test.header, ContentSecurityPolicyType::kEnforce,
        ContentSecurityPolicySource::kHTTP, *secure_origin));
    EXPECT_EQ(test.expected, csp->AllowFencedFrameOpaqueURL());
  }
}

class SpeculationRulesHeaderContentSecurityPolicyTest
    : public base::test::WithFeatureOverride,
      public ContentSecurityPolicyTest {
 public:
  SpeculationRulesHeaderContentSecurityPolicyTest()
      : base::test::WithFeatureOverride(
            features::kExemptSpeculationRulesHeaderFromCSP) {}
};

TEST_P(SpeculationRulesHeaderContentSecurityPolicyTest,
       ExemptSpeculationRulesFromHeader) {
  KURL speculation_rules_url("http://example.com/rules.json");
  csp = MakeGarbageCollected<ContentSecurityPolicy>();
  csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
  csp->AddPolicies(ParseContentSecurityPolicies(
      "script-src 'strict-dynamic'", ContentSecurityPolicyType::kEnforce,
      ContentSecurityPolicySource::kHTTP, *secure_origin));

  EXPECT_EQ(
      base::FeatureList::IsEnabled(
          features::kExemptSpeculationRulesHeaderFromCSP),
      csp->AllowRequest(mojom::blink::RequestContextType::SPECULATION_RULES,
                        network::mojom::RequestDestination::kSpeculationRules,
                        speculation_rules_url, String(), IntegrityMetadataSet(),
                        kParserInserted, speculation_rules_url,
                        ResourceRequest::RedirectStatus::kNoRedirect,
                        ReportingDisposition::kSuppressReporting));
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(
    SpeculationRulesHeaderContentSecurityPolicyTest);

}  // namespace blink
