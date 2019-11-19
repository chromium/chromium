// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_insecure_request_policy.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_init.h"
#include "third_party/blink/renderer/core/frame/csp/csp_directive_list.h"
#include "third_party/blink/renderer/core/html/html_script_element.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/crypto.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/loader/fetch/integrity_metadata.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/network/content_security_policy_parsers.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

class ContentSecurityPolicyTest : public testing::Test {
 public:
  ContentSecurityPolicyTest()
      : csp(MakeGarbageCollected<ContentSecurityPolicy>()),
        secure_url("https://example.test/image.png"),
        secure_origin(SecurityOrigin::Create(secure_url)) {}

 protected:
  void SetUp() override { execution_context = CreateExecutionContext(); }

  NullExecutionContext* CreateExecutionContext() {
    NullExecutionContext* context =
        MakeGarbageCollected<NullExecutionContext>();
    context->SetUpSecurityContext();
    context->SetSecurityOrigin(secure_origin);
    return context;
  }

  Persistent<ContentSecurityPolicy> csp;
  KURL secure_url;
  scoped_refptr<SecurityOrigin> secure_origin;
  Persistent<NullExecutionContext> execution_context;
};

TEST_F(ContentSecurityPolicyTest, ParseInsecureRequestPolicy) {
  struct TestCase {
    const char* header;
    WebInsecureRequestPolicy expected_policy;
  } cases[] = {{"default-src 'none'", kLeaveInsecureRequestsAlone},
               {"upgrade-insecure-requests", kUpgradeInsecureRequests},
               {"block-all-mixed-content", kBlockAllMixedContent},
               {"upgrade-insecure-requests; block-all-mixed-content",
                kUpgradeInsecureRequests | kBlockAllMixedContent},
               {"upgrade-insecure-requests, block-all-mixed-content",
                kUpgradeInsecureRequests | kBlockAllMixedContent}};

  // Enforced
  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message()
                 << "[Enforce] Header: `" << test.header << "`");
    csp = MakeGarbageCollected<ContentSecurityPolicy>();
    csp->DidReceiveHeader(test.header, kContentSecurityPolicyHeaderTypeEnforce,
                          kContentSecurityPolicyHeaderSourceHTTP);
    EXPECT_EQ(test.expected_policy, csp->GetInsecureRequestPolicy());

    DocumentInit init = DocumentInit::Create()
                            .WithOriginToCommit(secure_origin)
                            .WithURL(secure_url);
    auto* document = MakeGarbageCollected<Document>(init);
    csp->BindToDelegate(document->GetContentSecurityPolicyDelegate());
    EXPECT_EQ(test.expected_policy, document->GetInsecureRequestPolicy());
    bool expect_upgrade = test.expected_policy & kUpgradeInsecureRequests;
    EXPECT_EQ(expect_upgrade, document->InsecureNavigationsToUpgrade().Contains(
                                  document->Url().Host().Impl()->GetHash()));
  }

  // Report-Only
  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message()
                 << "[Report-Only] Header: `" << test.header << "`");
    csp = MakeGarbageCollected<ContentSecurityPolicy>();
    csp->DidReceiveHeader(test.header, kContentSecurityPolicyHeaderTypeReport,
                          kContentSecurityPolicyHeaderSourceHTTP);
    EXPECT_EQ(kLeaveInsecureRequestsAlone, csp->GetInsecureRequestPolicy());

    execution_context = CreateExecutionContext();
    execution_context->SetSecurityOrigin(secure_origin);
    csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
    EXPECT_EQ(kLeaveInsecureRequestsAlone,
              execution_context->GetInsecureRequestPolicy());
    EXPECT_FALSE(execution_context->InsecureNavigationsToUpgrade().Contains(
        secure_origin->Host().Impl()->GetHash()));
  }
}

TEST_F(ContentSecurityPolicyTest, CopyStateFrom) {
  csp->DidReceiveHeader("script-src 'none'; plugin-types application/x-type-1",
                        kContentSecurityPolicyHeaderTypeReport,
                        kContentSecurityPolicyHeaderSourceHTTP);
  csp->DidReceiveHeader("img-src http://example.com",
                        kContentSecurityPolicyHeaderTypeReport,
                        kContentSecurityPolicyHeaderSourceHTTP);

  const KURL example_url("http://example.com");
  const KURL not_example_url("http://not-example.com");

  auto* csp2 = MakeGarbageCollected<ContentSecurityPolicy>();
  csp2->CopyStateFrom(csp.Get());
  EXPECT_FALSE(csp2->AllowScriptFromSource(
      example_url, String(), IntegrityMetadataSet(), kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting,
      ContentSecurityPolicy::CheckHeaderType::kCheckReportOnly));
  EXPECT_TRUE(csp2->AllowPluginType(
      "application/x-type-1", "application/x-type-1", example_url,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(csp2->AllowImageFromSource(
      example_url, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting,
      ContentSecurityPolicy::CheckHeaderType::kCheckReportOnly));
  EXPECT_FALSE(csp2->AllowImageFromSource(
      not_example_url, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting,
      ContentSecurityPolicy::CheckHeaderType::kCheckReportOnly));
  EXPECT_FALSE(csp2->AllowPluginType(
      "application/x-type-2", "application/x-type-2", example_url,
      SecurityViolationReportingPolicy::kSuppressReporting));
}

TEST_F(ContentSecurityPolicyTest, CopyPluginTypesFrom) {
  csp->DidReceiveHeader("script-src 'none'; plugin-types application/x-type-1",
                        kContentSecurityPolicyHeaderTypeEnforce,
                        kContentSecurityPolicyHeaderSourceHTTP);
  csp->DidReceiveHeader("img-src http://example.com",
                        kContentSecurityPolicyHeaderTypeEnforce,
                        kContentSecurityPolicyHeaderSourceHTTP);

  const KURL example_url("http://example.com");
  const KURL not_example_url("http://not-example.com");

  auto* csp2 = MakeGarbageCollected<ContentSecurityPolicy>();
  csp2->CopyPluginTypesFrom(csp.Get());
  EXPECT_TRUE(csp2->AllowScriptFromSource(
      example_url, String(), IntegrityMetadataSet(), kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(csp2->AllowPluginType(
      "application/x-type-1", "application/x-type-1", example_url,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(csp2->AllowImageFromSource(
      example_url, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(csp2->AllowImageFromSource(
      not_example_url, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_FALSE(csp2->AllowPluginType(
      "application/x-type-2", "application/x-type-2", example_url,
      SecurityViolationReportingPolicy::kSuppressReporting));
}

TEST_F(ContentSecurityPolicyTest, IsFrameAncestorsEnforced) {
  csp->DidReceiveHeader("script-src 'none';",
                        kContentSecurityPolicyHeaderTypeEnforce,
                        kContentSecurityPolicyHeaderSourceHTTP);
  EXPECT_FALSE(csp->IsFrameAncestorsEnforced());

  csp->DidReceiveHeader("frame-ancestors 'self'",
                        kContentSecurityPolicyHeaderTypeReport,
                        kContentSecurityPolicyHeaderSourceHTTP);
  EXPECT_FALSE(csp->IsFrameAncestorsEnforced());

  csp->DidReceiveHeader("frame-ancestors 'self'",
                        kContentSecurityPolicyHeaderTypeEnforce,
                        kContentSecurityPolicyHeaderSourceHTTP);
  EXPECT_TRUE(csp->IsFrameAncestorsEnforced());
}

TEST_F(ContentSecurityPolicyTest, IsActiveForConnectionsWithConnectSrc) {
  EXPECT_FALSE(csp->IsActiveForConnections());
  csp->DidReceiveHeader("connect-src 'none';",
                        kContentSecurityPolicyHeaderTypeEnforce,
                        kContentSecurityPolicyHeaderSourceHTTP);
  EXPECT_TRUE(csp->IsActiveForConnections());
}

TEST_F(ContentSecurityPolicyTest, IsActiveForConnectionsWithDefaultSrc) {
  EXPECT_FALSE(csp->IsActiveForConnections());
  csp->DidReceiveHeader("default-src 'none';",
                        kContentSecurityPolicyHeaderTypeEnforce,
                        kContentSecurityPolicyHeaderSourceHTTP);
  EXPECT_TRUE(csp->IsActiveForConnections());
}

// Tests that frame-ancestors directives are discarded from policies
// delivered in <meta> elements.
TEST_F(ContentSecurityPolicyTest, FrameAncestorsInMeta) {
  csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
  csp->DidReceiveHeader("frame-ancestors 'none';",
                        kContentSecurityPolicyHeaderTypeEnforce,
                        kContentSecurityPolicyHeaderSourceMeta);
  EXPECT_FALSE(csp->IsFrameAncestorsEnforced());
  csp->DidReceiveHeader("frame-ancestors 'none';",
                        kContentSecurityPolicyHeaderTypeEnforce,
                        kContentSecurityPolicyHeaderSourceHTTP);
  EXPECT_TRUE(csp->IsFrameAncestorsEnforced());
}

// Tests that sandbox directives are discarded from policies
// delivered in <meta> elements.
TEST_F(ContentSecurityPolicyTest, SandboxInMeta) {
  csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
  EXPECT_EQ(WebSandboxFlags::kNone, csp->GetSandboxMask());
  csp->DidReceiveHeader("sandbox;", kContentSecurityPolicyHeaderTypeEnforce,
                        kContentSecurityPolicyHeaderSourceMeta);
  EXPECT_EQ(WebSandboxFlags::kNone, csp->GetSandboxMask());
  execution_context->SetSandboxFlags(WebSandboxFlags::kAll);
  csp->DidReceiveHeader("sandbox;", kContentSecurityPolicyHeaderTypeEnforce,
                        kContentSecurityPolicyHeaderSourceHTTP);
  EXPECT_EQ(WebSandboxFlags::kAll, csp->GetSandboxMask());
}

// Tests that report-uri directives are discarded from policies
// delivered in <meta> elements.
TEST_F(ContentSecurityPolicyTest, ReportURIInMeta) {
  String policy = "img-src 'none'; report-uri http://foo.test";
  Vector<UChar> characters;
  policy.AppendTo(characters);
  const UChar* begin = characters.data();
  const UChar* end = begin + characters.size();
  CSPDirectiveList* directive_list(CSPDirectiveList::Create(
      csp, begin, end, kContentSecurityPolicyHeaderTypeEnforce,
      kContentSecurityPolicyHeaderSourceMeta));
  EXPECT_TRUE(directive_list->ReportEndpoints().IsEmpty());
  directive_list = CSPDirectiveList::Create(
      csp, begin, end, kContentSecurityPolicyHeaderTypeEnforce,
      kContentSecurityPolicyHeaderSourceHTTP);
  EXPECT_FALSE(directive_list->ReportEndpoints().IsEmpty());
}

// Tests that object-src directives are applied to a request to load a
// plugin, but not to subresource requests that the plugin itself
// makes. https://crbug.com/603952
TEST_F(ContentSecurityPolicyTest, ObjectSrc) {
  const KURL url("https://example.test");
  csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
  csp->DidReceiveHeader("object-src 'none';",
                        kContentSecurityPolicyHeaderTypeEnforce,
                        kContentSecurityPolicyHeaderSourceMeta);
  EXPECT_FALSE(csp->AllowRequest(
      mojom::RequestContextType::OBJECT, url, String(), IntegrityMetadataSet(),
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_FALSE(csp->AllowRequest(
      mojom::RequestContextType::EMBED, url, String(), IntegrityMetadataSet(),
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(csp->AllowRequest(
      mojom::RequestContextType::PLUGIN, url, String(), IntegrityMetadataSet(),
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
}

TEST_F(ContentSecurityPolicyTest, ConnectSrc) {
  const KURL url("https://example.test");
  csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
  csp->DidReceiveHeader("connect-src 'none';",
                        kContentSecurityPolicyHeaderTypeEnforce,
                        kContentSecurityPolicyHeaderSourceMeta);
  EXPECT_FALSE(
      csp->AllowRequest(mojom::RequestContextType::SUBRESOURCE, url, String(),
                        IntegrityMetadataSet(), kParserInserted,
                        ResourceRequest::RedirectStatus::kNoRedirect,
                        SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_FALSE(
      csp->AllowRequest(mojom::RequestContextType::XML_HTTP_REQUEST, url,
                        String(), IntegrityMetadataSet(), kParserInserted,
                        ResourceRequest::RedirectStatus::kNoRedirect,
                        SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_FALSE(csp->AllowRequest(
      mojom::RequestContextType::BEACON, url, String(), IntegrityMetadataSet(),
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_FALSE(csp->AllowRequest(
      mojom::RequestContextType::FETCH, url, String(), IntegrityMetadataSet(),
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(csp->AllowRequest(
      mojom::RequestContextType::PLUGIN, url, String(), IntegrityMetadataSet(),
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
}
// Tests that requests for scripts and styles are blocked
// if `require-sri-for` delivered in HTTP header requires integrity be present
TEST_F(ContentSecurityPolicyTest, RequireSRIForInHeaderMissingIntegrity) {
  const KURL url("https://example.test");
  // Enforce
  Persistent<ContentSecurityPolicy> policy =
      MakeGarbageCollected<ContentSecurityPolicy>();
  policy->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
  policy->DidReceiveHeader("require-sri-for script style",
                           kContentSecurityPolicyHeaderTypeEnforce,
                           kContentSecurityPolicyHeaderSourceHTTP);
  EXPECT_FALSE(policy->AllowRequest(
      mojom::RequestContextType::SCRIPT, url, String(), IntegrityMetadataSet(),
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_FALSE(policy->AllowRequest(
      mojom::RequestContextType::IMPORT, url, String(), IntegrityMetadataSet(),
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_FALSE(policy->AllowRequest(
      mojom::RequestContextType::STYLE, url, String(), IntegrityMetadataSet(),
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_FALSE(policy->AllowRequest(
      mojom::RequestContextType::SERVICE_WORKER, url, String(),
      IntegrityMetadataSet(), kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_FALSE(policy->AllowRequest(
      mojom::RequestContextType::SHARED_WORKER, url, String(),
      IntegrityMetadataSet(), kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_FALSE(policy->AllowRequest(
      mojom::RequestContextType::WORKER, url, String(), IntegrityMetadataSet(),
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      mojom::RequestContextType::IMAGE, url, String(), IntegrityMetadataSet(),
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  // Report
  policy = MakeGarbageCollected<ContentSecurityPolicy>();
  policy->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
  policy->DidReceiveHeader("require-sri-for script style",
                           kContentSecurityPolicyHeaderTypeReport,
                           kContentSecurityPolicyHeaderSourceHTTP);
  EXPECT_TRUE(policy->AllowRequest(
      mojom::RequestContextType::SCRIPT, url, String(), IntegrityMetadataSet(),
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      mojom::RequestContextType::IMPORT, url, String(), IntegrityMetadataSet(),
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      mojom::RequestContextType::STYLE, url, String(), IntegrityMetadataSet(),
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      mojom::RequestContextType::SERVICE_WORKER, url, String(),
      IntegrityMetadataSet(), kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      mojom::RequestContextType::SHARED_WORKER, url, String(),
      IntegrityMetadataSet(), kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      mojom::RequestContextType::WORKER, url, String(), IntegrityMetadataSet(),
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      mojom::RequestContextType::IMAGE, url, String(), IntegrityMetadataSet(),
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
}

// Tests that requests for scripts and styles are allowed
// if `require-sri-for` delivered in HTTP header requires integrity be present
TEST_F(ContentSecurityPolicyTest, RequireSRIForInHeaderPresentIntegrity) {
  const KURL url("https://example.test");
  IntegrityMetadataSet integrity_metadata;
  integrity_metadata.insert(
      IntegrityMetadata("1234", IntegrityAlgorithm::kSha384).ToPair());
  csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
  // Enforce
  Persistent<ContentSecurityPolicy> policy =
      MakeGarbageCollected<ContentSecurityPolicy>();
  policy->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
  policy->DidReceiveHeader("require-sri-for script style",
                           kContentSecurityPolicyHeaderTypeEnforce,
                           kContentSecurityPolicyHeaderSourceHTTP);
  EXPECT_TRUE(policy->AllowRequest(
      mojom::RequestContextType::SCRIPT, url, String(), integrity_metadata,
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      mojom::RequestContextType::IMPORT, url, String(), integrity_metadata,
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      mojom::RequestContextType::STYLE, url, String(), integrity_metadata,
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      mojom::RequestContextType::SERVICE_WORKER, url, String(),
      integrity_metadata, kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      mojom::RequestContextType::SHARED_WORKER, url, String(),
      integrity_metadata, kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      mojom::RequestContextType::WORKER, url, String(), integrity_metadata,
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      mojom::RequestContextType::IMAGE, url, String(), integrity_metadata,
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  // Content-Security-Policy-Report-Only is not supported in meta element,
  // so nothing should be blocked
  policy = MakeGarbageCollected<ContentSecurityPolicy>();
  policy->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
  policy->DidReceiveHeader("require-sri-for script style",
                           kContentSecurityPolicyHeaderTypeReport,
                           kContentSecurityPolicyHeaderSourceHTTP);
  EXPECT_TRUE(policy->AllowRequest(
      mojom::RequestContextType::SCRIPT, url, String(), integrity_metadata,
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      mojom::RequestContextType::IMPORT, url, String(), integrity_metadata,
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      mojom::RequestContextType::STYLE, url, String(), integrity_metadata,
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      mojom::RequestContextType::SERVICE_WORKER, url, String(),
      integrity_metadata, kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      mojom::RequestContextType::SHARED_WORKER, url, String(),
      integrity_metadata, kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      mojom::RequestContextType::WORKER, url, String(), integrity_metadata,
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      mojom::RequestContextType::IMAGE, url, String(), integrity_metadata,
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
}

// Tests that requests for scripts and styles are blocked
// if `require-sri-for` delivered in meta tag requires integrity be present
TEST_F(ContentSecurityPolicyTest, RequireSRIForInMetaMissingIntegrity) {
  const KURL url("https://example.test");
  // Enforce
  Persistent<ContentSecurityPolicy> policy =
      MakeGarbageCollected<ContentSecurityPolicy>();
  policy->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
  policy->DidReceiveHeader("require-sri-for script style",
                           kContentSecurityPolicyHeaderTypeEnforce,
                           kContentSecurityPolicyHeaderSourceMeta);
  EXPECT_FALSE(policy->AllowRequest(
      mojom::RequestContextType::SCRIPT, url, String(), IntegrityMetadataSet(),
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_FALSE(policy->AllowRequest(
      mojom::RequestContextType::IMPORT, url, String(), IntegrityMetadataSet(),
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_FALSE(policy->AllowRequest(
      mojom::RequestContextType::STYLE, url, String(), IntegrityMetadataSet(),
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_FALSE(policy->AllowRequest(
      mojom::RequestContextType::SERVICE_WORKER, url, String(),
      IntegrityMetadataSet(), kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_FALSE(policy->AllowRequest(
      mojom::RequestContextType::SHARED_WORKER, url, String(),
      IntegrityMetadataSet(), kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_FALSE(policy->AllowRequest(
      mojom::RequestContextType::WORKER, url, String(), IntegrityMetadataSet(),
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      mojom::RequestContextType::IMAGE, url, String(), IntegrityMetadataSet(),
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  // Content-Security-Policy-Report-Only is not supported in meta element,
  // so nothing should be blocked
  policy = MakeGarbageCollected<ContentSecurityPolicy>();
  policy->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
  policy->DidReceiveHeader("require-sri-for script style",
                           kContentSecurityPolicyHeaderTypeReport,
                           kContentSecurityPolicyHeaderSourceMeta);
  EXPECT_TRUE(policy->AllowRequest(
      mojom::RequestContextType::SCRIPT, url, String(), IntegrityMetadataSet(),
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      mojom::RequestContextType::IMPORT, url, String(), IntegrityMetadataSet(),
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      mojom::RequestContextType::STYLE, url, String(), IntegrityMetadataSet(),
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      mojom::RequestContextType::SERVICE_WORKER, url, String(),
      IntegrityMetadataSet(), kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      mojom::RequestContextType::SHARED_WORKER, url, String(),
      IntegrityMetadataSet(), kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      mojom::RequestContextType::WORKER, url, String(), IntegrityMetadataSet(),
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      mojom::RequestContextType::IMAGE, url, String(), IntegrityMetadataSet(),
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
}

// Tests that requests for scripts and styles are allowed
// if `require-sri-for` delivered meta tag requires integrity be present
TEST_F(ContentSecurityPolicyTest, RequireSRIForInMetaPresentIntegrity) {
  const KURL url("https://example.test");
  IntegrityMetadataSet integrity_metadata;
  integrity_metadata.insert(
      IntegrityMetadata("1234", IntegrityAlgorithm::kSha384).ToPair());
  csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
  // Enforce
  Persistent<ContentSecurityPolicy> policy =
      MakeGarbageCollected<ContentSecurityPolicy>();
  policy->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
  policy->DidReceiveHeader("require-sri-for script style",
                           kContentSecurityPolicyHeaderTypeEnforce,
                           kContentSecurityPolicyHeaderSourceMeta);
  EXPECT_TRUE(policy->AllowRequest(
      mojom::RequestContextType::SCRIPT, url, String(), integrity_metadata,
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      mojom::RequestContextType::IMPORT, url, String(), integrity_metadata,
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      mojom::RequestContextType::STYLE, url, String(), integrity_metadata,
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      mojom::RequestContextType::SERVICE_WORKER, url, String(),
      integrity_metadata, kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      mojom::RequestContextType::SHARED_WORKER, url, String(),
      integrity_metadata, kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      mojom::RequestContextType::WORKER, url, String(), integrity_metadata,
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      mojom::RequestContextType::IMAGE, url, String(), integrity_metadata,
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  // Content-Security-Policy-Report-Only is not supported in meta element,
  // so nothing should be blocked
  policy = MakeGarbageCollected<ContentSecurityPolicy>();
  policy->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
  policy->DidReceiveHeader("require-sri-for script style",
                           kContentSecurityPolicyHeaderTypeReport,
                           kContentSecurityPolicyHeaderSourceMeta);
  EXPECT_TRUE(policy->AllowRequest(
      mojom::RequestContextType::SCRIPT, url, String(), integrity_metadata,
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      mojom::RequestContextType::IMPORT, url, String(), integrity_metadata,
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      mojom::RequestContextType::STYLE, url, String(), integrity_metadata,
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      mojom::RequestContextType::SERVICE_WORKER, url, String(),
      integrity_metadata, kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      mojom::RequestContextType::SHARED_WORKER, url, String(),
      integrity_metadata, kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      mojom::RequestContextType::WORKER, url, String(), integrity_metadata,
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      mojom::RequestContextType::IMAGE, url, String(), integrity_metadata,
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
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
    policy->DidReceiveHeader(test.policy,
                             kContentSecurityPolicyHeaderTypeEnforce,
                             kContentSecurityPolicyHeaderSourceHTTP);
    EXPECT_EQ(test.allowed, policy->AllowScriptFromSource(
                                resource, String(test.nonce),
                                IntegrityMetadataSet(), kParserInserted));
    // If this is expected to generate a violation, we should have sent a
    // report.
    EXPECT_EQ(expected_reports, policy->violation_reports_sent_.size());

    // Single report-mode policy should always be `true`:
    policy = MakeGarbageCollected<ContentSecurityPolicy>();
    policy->BindToDelegate(
        execution_context->GetContentSecurityPolicyDelegate());
    policy->DidReceiveHeader(test.policy,
                             kContentSecurityPolicyHeaderTypeReport,
                             kContentSecurityPolicyHeaderSourceHTTP);
    EXPECT_TRUE(policy->AllowScriptFromSource(
        resource, String(test.nonce), IntegrityMetadataSet(), kParserInserted,
        ResourceRequest::RedirectStatus::kNoRedirect,
        SecurityViolationReportingPolicy::kReport,
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
  WTF::OrdinalNumber context_line;

  // We need document for HTMLScriptElement tests.
  DocumentInit init = DocumentInit::Create().WithOriginToCommit(secure_origin);
  auto* document = MakeGarbageCollected<Document>(init);

  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message() << "Policy: `" << test.policy
                                    << "`, Nonce: `" << test.nonce << "`");

    unsigned expected_reports = test.allowed ? 0u : 1u;
    auto* element = MakeGarbageCollected<HTMLScriptElement>(
        *document, CreateElementFlags::ByParser());

    // Enforce 'script-src'
    Persistent<ContentSecurityPolicy> policy =
        MakeGarbageCollected<ContentSecurityPolicy>();
    policy->BindToDelegate(document->GetContentSecurityPolicyDelegate());
    policy->DidReceiveHeader(String("script-src ") + test.policy,
                             kContentSecurityPolicyHeaderTypeEnforce,
                             kContentSecurityPolicyHeaderSourceHTTP);
    EXPECT_EQ(test.allowed,
              policy->AllowInline(ContentSecurityPolicy::InlineType::kScript,
                                  element, content, String(test.nonce),
                                  context_url, context_line));
    EXPECT_EQ(expected_reports, policy->violation_reports_sent_.size());

    // Enforce 'style-src'
    policy = MakeGarbageCollected<ContentSecurityPolicy>();
    policy->BindToDelegate(document->GetContentSecurityPolicyDelegate());
    policy->DidReceiveHeader(String("style-src ") + test.policy,
                             kContentSecurityPolicyHeaderTypeEnforce,
                             kContentSecurityPolicyHeaderSourceHTTP);
    EXPECT_EQ(test.allowed,
              policy->AllowInline(ContentSecurityPolicy::InlineType::kStyle,
                                  element, content, String(test.nonce),
                                  context_url, context_line));
    EXPECT_EQ(expected_reports, policy->violation_reports_sent_.size());

    // Report 'script-src'
    policy = MakeGarbageCollected<ContentSecurityPolicy>();
    policy->BindToDelegate(document->GetContentSecurityPolicyDelegate());
    policy->DidReceiveHeader(String("script-src ") + test.policy,
                             kContentSecurityPolicyHeaderTypeReport,
                             kContentSecurityPolicyHeaderSourceHTTP);
    EXPECT_TRUE(policy->AllowInline(ContentSecurityPolicy::InlineType::kScript,
                                    element, content, String(test.nonce),
                                    context_url, context_line));
    EXPECT_EQ(expected_reports, policy->violation_reports_sent_.size());

    // Report 'style-src'
    policy = MakeGarbageCollected<ContentSecurityPolicy>();
    policy->BindToDelegate(document->GetContentSecurityPolicyDelegate());
    policy->DidReceiveHeader(String("style-src ") + test.policy,
                             kContentSecurityPolicyHeaderTypeReport,
                             kContentSecurityPolicyHeaderSourceHTTP);
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
    policy->DidReceiveHeader(test.policy1,
                             kContentSecurityPolicyHeaderTypeEnforce,
                             kContentSecurityPolicyHeaderSourceHTTP);
    policy->DidReceiveHeader(test.policy2,
                             kContentSecurityPolicyHeaderTypeReport,
                             kContentSecurityPolicyHeaderSourceHTTP);
    EXPECT_EQ(test.allowed1,
              policy->AllowScriptFromSource(
                  resource, String(test.nonce), IntegrityMetadataSet(),
                  kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
                  SecurityViolationReportingPolicy::kReport,
                  ContentSecurityPolicy::CheckHeaderType::kCheckEnforce));
    EXPECT_TRUE(policy->AllowScriptFromSource(
        resource, String(test.nonce), IntegrityMetadataSet(), kParserInserted,
        ResourceRequest::RedirectStatus::kNoRedirect,
        SecurityViolationReportingPolicy::kReport,
        ContentSecurityPolicy::CheckHeaderType::kCheckReportOnly));
    EXPECT_EQ(expected_reports, policy->violation_reports_sent_.size());

    // Report / Enforce
    policy = MakeGarbageCollected<ContentSecurityPolicy>();
    policy->BindToDelegate(
        execution_context->GetContentSecurityPolicyDelegate());
    policy->DidReceiveHeader(test.policy1,
                             kContentSecurityPolicyHeaderTypeReport,
                             kContentSecurityPolicyHeaderSourceHTTP);
    policy->DidReceiveHeader(test.policy2,
                             kContentSecurityPolicyHeaderTypeEnforce,
                             kContentSecurityPolicyHeaderSourceHTTP);
    EXPECT_TRUE(policy->AllowScriptFromSource(
        resource, String(test.nonce), IntegrityMetadataSet(), kParserInserted,
        ResourceRequest::RedirectStatus::kNoRedirect,
        SecurityViolationReportingPolicy::kReport,
        ContentSecurityPolicy::CheckHeaderType::kCheckReportOnly));
    EXPECT_EQ(test.allowed2,
              policy->AllowScriptFromSource(
                  resource, String(test.nonce), IntegrityMetadataSet(),
                  kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
                  SecurityViolationReportingPolicy::kReport,
                  ContentSecurityPolicy::CheckHeaderType::kCheckEnforce));
    EXPECT_EQ(expected_reports, policy->violation_reports_sent_.size());

    // Enforce / Enforce
    policy = MakeGarbageCollected<ContentSecurityPolicy>();
    policy->BindToDelegate(
        execution_context->GetContentSecurityPolicyDelegate());
    policy->DidReceiveHeader(test.policy1,
                             kContentSecurityPolicyHeaderTypeEnforce,
                             kContentSecurityPolicyHeaderSourceHTTP);
    policy->DidReceiveHeader(test.policy2,
                             kContentSecurityPolicyHeaderTypeEnforce,
                             kContentSecurityPolicyHeaderSourceHTTP);
    EXPECT_EQ(test.allowed1 && test.allowed2,
              policy->AllowScriptFromSource(
                  resource, String(test.nonce), IntegrityMetadataSet(),
                  kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
                  SecurityViolationReportingPolicy::kReport,
                  ContentSecurityPolicy::CheckHeaderType::kCheckEnforce));
    EXPECT_EQ(expected_reports, policy->violation_reports_sent_.size());

    // Report / Report
    policy = MakeGarbageCollected<ContentSecurityPolicy>();
    policy->BindToDelegate(
        execution_context->GetContentSecurityPolicyDelegate());
    policy->DidReceiveHeader(test.policy1,
                             kContentSecurityPolicyHeaderTypeReport,
                             kContentSecurityPolicyHeaderSourceHTTP);
    policy->DidReceiveHeader(test.policy2,
                             kContentSecurityPolicyHeaderTypeReport,
                             kContentSecurityPolicyHeaderSourceHTTP);
    EXPECT_TRUE(policy->AllowScriptFromSource(
        resource, String(test.nonce), IntegrityMetadataSet(), kParserInserted,
        ResourceRequest::RedirectStatus::kNoRedirect,
        SecurityViolationReportingPolicy::kReport,
        ContentSecurityPolicy::CheckHeaderType::kCheckReportOnly));
    EXPECT_EQ(expected_reports, policy->violation_reports_sent_.size());
  }
}

TEST_F(ContentSecurityPolicyTest, ShouldEnforceEmbeddersPolicy) {
  struct TestCase {
    const char* resource_url;
    const bool inherits;
  } cases[] = {
      // Same-origin
      {"https://example.test/index.html", true},
      // Cross-origin
      {"http://example.test/index.html", false},
      {"http://example.test:8443/index.html", false},
      {"https://example.test:8443/index.html", false},
      {"http://not.example.test/index.html", false},
      {"https://not.example.test/index.html", false},
      {"https://not.example.test:8443/index.html", false},

      // Inherit
      {"about:blank", true},
      {"data:text/html,yay", true},
      {"blob:https://example.test/bbe708f3-defd-4852-93b6-cf94e032f08d", true},
      {"filesystem:http://example.test/temporary/index.html", true},
  };

  for (const auto& test : cases) {
    ResourceResponse response(KURL(test.resource_url));
    EXPECT_EQ(ContentSecurityPolicy::ShouldEnforceEmbeddersPolicy(
                  response, secure_origin.get()),
              test.inherits);

    response.SetHttpHeaderField(http_names::kAllowCSPFrom, AtomicString("*"));
    EXPECT_TRUE(ContentSecurityPolicy::ShouldEnforceEmbeddersPolicy(
        response, secure_origin.get()));

    response.SetHttpHeaderField(http_names::kAllowCSPFrom,
                                AtomicString("* not a valid header"));
    EXPECT_EQ(ContentSecurityPolicy::ShouldEnforceEmbeddersPolicy(
                  response, secure_origin.get()),
              test.inherits);

    response.SetHttpHeaderField(http_names::kAllowCSPFrom,
                                AtomicString("http://example.test"));
    EXPECT_EQ(ContentSecurityPolicy::ShouldEnforceEmbeddersPolicy(
                  response, secure_origin.get()),
              test.inherits);

    response.SetHttpHeaderField(http_names::kAllowCSPFrom,
                                AtomicString("https://example.test"));
    EXPECT_TRUE(ContentSecurityPolicy::ShouldEnforceEmbeddersPolicy(
        response, secure_origin.get()));
  }
}

TEST_F(ContentSecurityPolicyTest, DirectiveType) {
  struct TestCase {
    ContentSecurityPolicy::DirectiveType type;
    const String& name;
  } cases[] = {
      {ContentSecurityPolicy::DirectiveType::kBaseURI, "base-uri"},
      {ContentSecurityPolicy::DirectiveType::kBlockAllMixedContent,
       "block-all-mixed-content"},
      {ContentSecurityPolicy::DirectiveType::kChildSrc, "child-src"},
      {ContentSecurityPolicy::DirectiveType::kConnectSrc, "connect-src"},
      {ContentSecurityPolicy::DirectiveType::kDefaultSrc, "default-src"},
      {ContentSecurityPolicy::DirectiveType::kFrameAncestors,
       "frame-ancestors"},
      {ContentSecurityPolicy::DirectiveType::kFrameSrc, "frame-src"},
      {ContentSecurityPolicy::DirectiveType::kFontSrc, "font-src"},
      {ContentSecurityPolicy::DirectiveType::kFormAction, "form-action"},
      {ContentSecurityPolicy::DirectiveType::kImgSrc, "img-src"},
      {ContentSecurityPolicy::DirectiveType::kManifestSrc, "manifest-src"},
      {ContentSecurityPolicy::DirectiveType::kMediaSrc, "media-src"},
      {ContentSecurityPolicy::DirectiveType::kNavigateTo, "navigate-to"},
      {ContentSecurityPolicy::DirectiveType::kObjectSrc, "object-src"},
      {ContentSecurityPolicy::DirectiveType::kPluginTypes, "plugin-types"},
      {ContentSecurityPolicy::DirectiveType::kReportURI, "report-uri"},
      {ContentSecurityPolicy::DirectiveType::kRequireSRIFor, "require-sri-for"},
      {ContentSecurityPolicy::DirectiveType::kSandbox, "sandbox"},
      {ContentSecurityPolicy::DirectiveType::kScriptSrc, "script-src"},
      {ContentSecurityPolicy::DirectiveType::kScriptSrcAttr, "script-src-attr"},
      {ContentSecurityPolicy::DirectiveType::kScriptSrcElem, "script-src-elem"},
      {ContentSecurityPolicy::DirectiveType::kStyleSrc, "style-src"},
      {ContentSecurityPolicy::DirectiveType::kStyleSrcAttr, "style-src-attr"},
      {ContentSecurityPolicy::DirectiveType::kStyleSrcElem, "style-src-elem"},
      {ContentSecurityPolicy::DirectiveType::kUpgradeInsecureRequests,
       "upgrade-insecure-requests"},
      {ContentSecurityPolicy::DirectiveType::kWorkerSrc, "worker-src"},
  };

  EXPECT_EQ(ContentSecurityPolicy::DirectiveType::kUndefined,
            ContentSecurityPolicy::GetDirectiveType("random"));

  for (const auto& test : cases) {
    const String& name_from_type =
        ContentSecurityPolicy::GetDirectiveName(test.type);
    ContentSecurityPolicy::DirectiveType type_from_name =
        ContentSecurityPolicy::GetDirectiveType(test.name);
    EXPECT_EQ(name_from_type, test.name);
    EXPECT_EQ(type_from_name, test.type);
    EXPECT_EQ(test.type,
              ContentSecurityPolicy::GetDirectiveType(name_from_type));
    EXPECT_EQ(test.name,
              ContentSecurityPolicy::GetDirectiveName(type_from_name));
  }
}

TEST_F(ContentSecurityPolicyTest, Subsumes) {
  auto* other = MakeGarbageCollected<ContentSecurityPolicy>();
  EXPECT_TRUE(csp->Subsumes(*other));
  EXPECT_TRUE(other->Subsumes(*csp));

  csp->DidReceiveHeader("default-src http://example.com;",
                        kContentSecurityPolicyHeaderTypeEnforce,
                        kContentSecurityPolicyHeaderSourceHTTP);
  // If this CSP is not empty, the other must not be empty either.
  EXPECT_FALSE(csp->Subsumes(*other));
  EXPECT_TRUE(other->Subsumes(*csp));

  // Report-only policies do not impact subsumption.
  other->DidReceiveHeader("default-src http://example.com;",
                          kContentSecurityPolicyHeaderTypeReport,
                          kContentSecurityPolicyHeaderSourceHTTP);
  EXPECT_FALSE(csp->Subsumes(*other));

  // CSPDirectiveLists have to subsume.
  other->DidReceiveHeader("default-src http://example.com https://another.com;",
                          kContentSecurityPolicyHeaderTypeEnforce,
                          kContentSecurityPolicyHeaderSourceHTTP);
  EXPECT_FALSE(csp->Subsumes(*other));

  // `other` is stricter than `this`.
  other->DidReceiveHeader("default-src https://example.com;",
                          kContentSecurityPolicyHeaderTypeEnforce,
                          kContentSecurityPolicyHeaderSourceHTTP);
  EXPECT_TRUE(csp->Subsumes(*other));
}

TEST_F(ContentSecurityPolicyTest, RequestsAllowedWhenBypassingCSP) {
  const KURL base;
  execution_context = CreateExecutionContext();
  execution_context->SetSecurityOrigin(secure_origin);  // https://example.com
  execution_context->SetURL(secure_url);                // https://example.com
  csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
  csp->DidReceiveHeader("default-src https://example.com",
                        kContentSecurityPolicyHeaderTypeEnforce,
                        kContentSecurityPolicyHeaderSourceHTTP);

  EXPECT_TRUE(csp->AllowRequest(
      mojom::RequestContextType::OBJECT, KURL(base, "https://example.com/"),
      String(), IntegrityMetadataSet(), kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));

  EXPECT_FALSE(csp->AllowRequest(
      mojom::RequestContextType::OBJECT, KURL(base, "https://not-example.com/"),
      String(), IntegrityMetadataSet(), kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));

  // Register "https" as bypassing CSP, which should now bypass it entirely
  SchemeRegistry::RegisterURLSchemeAsBypassingContentSecurityPolicy("https");

  EXPECT_TRUE(csp->AllowRequest(
      mojom::RequestContextType::OBJECT, KURL(base, "https://example.com/"),
      String(), IntegrityMetadataSet(), kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));

  EXPECT_TRUE(csp->AllowRequest(
      mojom::RequestContextType::OBJECT, KURL(base, "https://not-example.com/"),
      String(), IntegrityMetadataSet(), kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));

  SchemeRegistry::RemoveURLSchemeRegisteredAsBypassingContentSecurityPolicy(
      "https");
}
TEST_F(ContentSecurityPolicyTest, FilesystemAllowedWhenBypassingCSP) {
  const KURL base;
  execution_context = CreateExecutionContext();
  execution_context->SetSecurityOrigin(secure_origin);  // https://example.com
  execution_context->SetURL(secure_url);                // https://example.com
  csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
  csp->DidReceiveHeader("default-src https://example.com",
                        kContentSecurityPolicyHeaderTypeEnforce,
                        kContentSecurityPolicyHeaderSourceHTTP);

  EXPECT_FALSE(
      csp->AllowRequest(mojom::RequestContextType::OBJECT,
                        KURL(base, "filesystem:https://example.com/file.txt"),
                        String(), IntegrityMetadataSet(), kParserInserted,
                        ResourceRequest::RedirectStatus::kNoRedirect,
                        SecurityViolationReportingPolicy::kSuppressReporting));

  EXPECT_FALSE(csp->AllowRequest(
      mojom::RequestContextType::OBJECT,
      KURL(base, "filesystem:https://not-example.com/file.txt"), String(),
      IntegrityMetadataSet(), kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));

  // Register "https" as bypassing CSP, which should now bypass it entirely
  SchemeRegistry::RegisterURLSchemeAsBypassingContentSecurityPolicy("https");

  EXPECT_TRUE(
      csp->AllowRequest(mojom::RequestContextType::OBJECT,
                        KURL(base, "filesystem:https://example.com/file.txt"),
                        String(), IntegrityMetadataSet(), kParserInserted,
                        ResourceRequest::RedirectStatus::kNoRedirect,
                        SecurityViolationReportingPolicy::kSuppressReporting));

  EXPECT_TRUE(csp->AllowRequest(
      mojom::RequestContextType::OBJECT,
      KURL(base, "filesystem:https://not-example.com/file.txt"), String(),
      IntegrityMetadataSet(), kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));

  SchemeRegistry::RemoveURLSchemeRegisteredAsBypassingContentSecurityPolicy(
      "https");
}

TEST_F(ContentSecurityPolicyTest, BlobAllowedWhenBypassingCSP) {
  const KURL base;
  execution_context = CreateExecutionContext();
  execution_context->SetSecurityOrigin(secure_origin);  // https://example.com
  execution_context->SetURL(secure_url);                // https://example.com
  csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
  csp->DidReceiveHeader("default-src https://example.com",
                        kContentSecurityPolicyHeaderTypeEnforce,
                        kContentSecurityPolicyHeaderSourceHTTP);

  EXPECT_FALSE(csp->AllowRequest(
      mojom::RequestContextType::OBJECT,
      KURL(base, "blob:https://example.com/"), String(), IntegrityMetadataSet(),
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));

  EXPECT_FALSE(
      csp->AllowRequest(mojom::RequestContextType::OBJECT,
                        KURL(base, "blob:https://not-example.com/"), String(),
                        IntegrityMetadataSet(), kParserInserted,
                        ResourceRequest::RedirectStatus::kNoRedirect,
                        SecurityViolationReportingPolicy::kSuppressReporting));

  // Register "https" as bypassing CSP, which should now bypass it entirely
  SchemeRegistry::RegisterURLSchemeAsBypassingContentSecurityPolicy("https");

  EXPECT_TRUE(csp->AllowRequest(
      mojom::RequestContextType::OBJECT,
      KURL(base, "blob:https://example.com/"), String(), IntegrityMetadataSet(),
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));

  EXPECT_TRUE(
      csp->AllowRequest(mojom::RequestContextType::OBJECT,
                        KURL(base, "blob:https://not-example.com/"), String(),
                        IntegrityMetadataSet(), kParserInserted,
                        ResourceRequest::RedirectStatus::kNoRedirect,
                        SecurityViolationReportingPolicy::kSuppressReporting));

  SchemeRegistry::RemoveURLSchemeRegisteredAsBypassingContentSecurityPolicy(
      "https");
}

TEST_F(ContentSecurityPolicyTest, CSPBypassDisabledWhenSchemeIsPrivileged) {
  const KURL base;
  execution_context = CreateExecutionContext();
  execution_context->SetSecurityOrigin(secure_origin);
  execution_context->SetURL(BlankURL());
  csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
  csp->DidReceiveHeader("script-src http://example.com",
                        kContentSecurityPolicyHeaderTypeEnforce,
                        kContentSecurityPolicyHeaderSourceHTTP);

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
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_FALSE(csp->AllowScriptFromSource(
      http_url, String(), IntegrityMetadataSet(), kNotParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_FALSE(csp->AllowScriptFromSource(
      blob_url, String(), IntegrityMetadataSet(), kNotParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_FALSE(csp->AllowScriptFromSource(
      filesystem_url, String(), IntegrityMetadataSet(), kNotParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));

  SchemeRegistry::RemoveURLSchemeRegisteredAsBypassingContentSecurityPolicy(
      "http");
  SchemeRegistry::RemoveURLSchemeAsNotAllowingJavascriptURLs("https");
}

TEST_F(ContentSecurityPolicyTest, IsValidCSPAttrTest) {
  // Empty string is invalid
  EXPECT_FALSE(ContentSecurityPolicy::IsValidCSPAttr("", ""));

  // Policy with single directive
  EXPECT_TRUE(
      ContentSecurityPolicy::IsValidCSPAttr("base-uri http://example.com", ""));
  EXPECT_FALSE(ContentSecurityPolicy::IsValidCSPAttr(
      "invalid-policy-name http://example.com", ""));

  // Policy with multiple directives
  EXPECT_TRUE(ContentSecurityPolicy::IsValidCSPAttr(
      "base-uri http://example.com 'self'; child-src http://example.com; "
      "default-src http://example.com",
      ""));
  EXPECT_FALSE(ContentSecurityPolicy::IsValidCSPAttr(
      "default-src http://example.com; "
      "invalid-policy-name http://example.com",
      ""));

  // 'self', 'none'
  EXPECT_TRUE(ContentSecurityPolicy::IsValidCSPAttr("script-src 'self'", ""));
  EXPECT_TRUE(ContentSecurityPolicy::IsValidCSPAttr("default-src 'none'", ""));
  EXPECT_FALSE(ContentSecurityPolicy::IsValidCSPAttr("script-src 'slef'", ""));
  EXPECT_FALSE(ContentSecurityPolicy::IsValidCSPAttr("default-src 'non'", ""));

  // invalid ascii character
  EXPECT_FALSE(
      ContentSecurityPolicy::IsValidCSPAttr("script-src https:  \x08", ""));
  EXPECT_FALSE(ContentSecurityPolicy::IsValidCSPAttr(
      "script-src 127.0.0.1%2F%DFisnotSorB%2F", ""));

  // paths on script-src
  EXPECT_TRUE(
      ContentSecurityPolicy::IsValidCSPAttr("script-src 127.0.0.1:*/", ""));
  EXPECT_TRUE(
      ContentSecurityPolicy::IsValidCSPAttr("script-src 127.0.0.1:*/path", ""));
  EXPECT_FALSE(ContentSecurityPolicy::IsValidCSPAttr(
      "script-src 127.0.0.1:*/path?query=string", ""));
  EXPECT_FALSE(ContentSecurityPolicy::IsValidCSPAttr(
      "script-src 127.0.0.1:*/path#anchor", ""));
  EXPECT_TRUE(
      ContentSecurityPolicy::IsValidCSPAttr("script-src 127.0.0.1:8000/", ""));
  EXPECT_TRUE(ContentSecurityPolicy::IsValidCSPAttr(
      "script-src 127.0.0.1:8000/path", ""));
  EXPECT_FALSE(ContentSecurityPolicy::IsValidCSPAttr(
      "script-src 127.0.0.1:8000/path?query=string", ""));
  EXPECT_FALSE(ContentSecurityPolicy::IsValidCSPAttr(
      "script-src 127.0.0.1:8000/path#anchor", ""));
  EXPECT_FALSE(ContentSecurityPolicy::IsValidCSPAttr(
      "script-src 127.0.0.1:8000/thisisa;pathwithasemicolon", ""));

  // script-src invalid hosts
  EXPECT_FALSE(ContentSecurityPolicy::IsValidCSPAttr("script-src http:/", ""));
  EXPECT_FALSE(ContentSecurityPolicy::IsValidCSPAttr("script-src http://", ""));
  EXPECT_FALSE(
      ContentSecurityPolicy::IsValidCSPAttr("script-src http:/127.0.0.1", ""));
  EXPECT_FALSE(ContentSecurityPolicy::IsValidCSPAttr(
      "script-src http:///127.0.0.1", ""));
  EXPECT_FALSE(ContentSecurityPolicy::IsValidCSPAttr(
      "script-src http://127.0.0.1:/", ""));
  EXPECT_FALSE(ContentSecurityPolicy::IsValidCSPAttr(
      "script-src https://127.?.0.1:*", ""));
  EXPECT_FALSE(ContentSecurityPolicy::IsValidCSPAttr(
      "script-src https://127.0.0.1:", ""));
  EXPECT_FALSE(ContentSecurityPolicy::IsValidCSPAttr(
      "script-src https://127.0.0.1:\t*   ", ""));

  // script-src host wildcards
  EXPECT_TRUE(ContentSecurityPolicy::IsValidCSPAttr(
      "script-src http://*.0.1:8000", ""));
  EXPECT_TRUE(ContentSecurityPolicy::IsValidCSPAttr(
      "script-src http://*.0.1:8000/", ""));
  EXPECT_TRUE(
      ContentSecurityPolicy::IsValidCSPAttr("script-src http://*.0.1:*", ""));
  EXPECT_TRUE(
      ContentSecurityPolicy::IsValidCSPAttr("script-src http://*.0.1:*/", ""));

  // missing semicolon
  EXPECT_FALSE(ContentSecurityPolicy::IsValidCSPAttr(
      "default-src 'self' script-src example.com", ""));
  EXPECT_FALSE(ContentSecurityPolicy::IsValidCSPAttr(
      "script-src 'self' object-src 'self' style-src *", ""));

  // 'none' with other sources
  EXPECT_FALSE(ContentSecurityPolicy::IsValidCSPAttr(
      "script-src http://127.0.0.1:8000 'none'", ""));
  EXPECT_FALSE(ContentSecurityPolicy::IsValidCSPAttr(
      "script-src 'none' 'none' 'none'", ""));

  // comma separated
  EXPECT_FALSE(ContentSecurityPolicy::IsValidCSPAttr(
      "script-src 'none', object-src 'none'", ""));

  // reporting not allowed
  EXPECT_FALSE(ContentSecurityPolicy::IsValidCSPAttr(
      "script-src 'none'; report-uri http://example.com/reporting", ""));
  EXPECT_FALSE(ContentSecurityPolicy::IsValidCSPAttr(
      "report-uri relative-path/reporting;"
      "base-uri http://example.com 'self'",
      ""));

  EXPECT_FALSE(ContentSecurityPolicy::IsValidCSPAttr(
      "script-src 'none'; report-to http://example.com/reporting", ""));
  EXPECT_FALSE(ContentSecurityPolicy::IsValidCSPAttr(
      "report-to relative-path/reporting;"
      "base-uri http://example.com 'self'",
      ""));

  // CRLF should not be allowed
  EXPECT_FALSE(ContentSecurityPolicy::IsValidCSPAttr(
      "base-uri\nhttp://example.com", ""));
  EXPECT_FALSE(ContentSecurityPolicy::IsValidCSPAttr(
      "base-uri http://example.com\nhttp://example2.com", ""));
  EXPECT_FALSE(ContentSecurityPolicy::IsValidCSPAttr(
      "base\n-uri http://example.com", ""));
  EXPECT_FALSE(ContentSecurityPolicy::IsValidCSPAttr(
      "\nbase-uri http://example.com", ""));

  EXPECT_FALSE(ContentSecurityPolicy::IsValidCSPAttr(
      "base-uri\r\nhttp://example.com", ""));
  EXPECT_FALSE(ContentSecurityPolicy::IsValidCSPAttr(
      "base-uri http://example.com\r\nhttp://example2.com", ""));
  EXPECT_FALSE(ContentSecurityPolicy::IsValidCSPAttr(
      "base\r\n-uri http://example.com", ""));
  EXPECT_FALSE(ContentSecurityPolicy::IsValidCSPAttr(
      "\r\nbase-uri http://example.com", ""));

  EXPECT_FALSE(ContentSecurityPolicy::IsValidCSPAttr(
      "base-uri\rhttp://example.com", ""));
  EXPECT_FALSE(ContentSecurityPolicy::IsValidCSPAttr(
      "base-uri http://example.com\rhttp://example2.com", ""));
  EXPECT_FALSE(ContentSecurityPolicy::IsValidCSPAttr(
      "base\r-uri http://example.com", ""));
  EXPECT_FALSE(ContentSecurityPolicy::IsValidCSPAttr(
      "\rbase-uri http://example.com", ""));
}

TEST_F(ContentSecurityPolicyTest, TrustedTypesNoDirective) {
  execution_context->SetRequireTrustedTypesForTesting();
  csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
  csp->DidReceiveHeader("", kContentSecurityPolicyHeaderTypeEnforce,
                        kContentSecurityPolicyHeaderSourceHTTP);
  EXPECT_TRUE(csp->AllowTrustedTypePolicy("somepolicy", false));
  EXPECT_TRUE(csp->AllowTrustedTypePolicy("somepolicy", true));
}

TEST_F(ContentSecurityPolicyTest, TrustedTypesSimpleDirective) {
  execution_context->SetRequireTrustedTypesForTesting();
  csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
  csp->DidReceiveHeader("trusted-types one two three",
                        kContentSecurityPolicyHeaderTypeEnforce,
                        kContentSecurityPolicyHeaderSourceHTTP);
}

TEST_F(ContentSecurityPolicyTest, TrustedTypesWhitespace) {
  execution_context->SetRequireTrustedTypesForTesting();
  csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
  csp->DidReceiveHeader("trusted-types one\ntwo\rthree",
                        kContentSecurityPolicyHeaderTypeEnforce,
                        kContentSecurityPolicyHeaderSourceHTTP);
  EXPECT_TRUE(csp->AllowTrustedTypePolicy("one", false));
  EXPECT_TRUE(csp->AllowTrustedTypePolicy("two", false));
  EXPECT_TRUE(csp->AllowTrustedTypePolicy("three", false));
  EXPECT_FALSE(csp->AllowTrustedTypePolicy("four", false));
  EXPECT_FALSE(csp->AllowTrustedTypePolicy("one", true));
  EXPECT_FALSE(csp->AllowTrustedTypePolicy("four", true));
}

TEST_F(ContentSecurityPolicyTest, TrustedTypesEmpty) {
  execution_context->SetRequireTrustedTypesForTesting();
  csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
  csp->DidReceiveHeader("trusted-types",
                        kContentSecurityPolicyHeaderTypeEnforce,
                        kContentSecurityPolicyHeaderSourceHTTP);
  EXPECT_FALSE(csp->AllowTrustedTypePolicy("somepolicy", false));
  EXPECT_FALSE(csp->AllowTrustedTypePolicy("somepolicy", true));
}

TEST_F(ContentSecurityPolicyTest, TrustedTypesStar) {
  execution_context->SetRequireTrustedTypesForTesting();
  csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
  csp->DidReceiveHeader("trusted-types *",
                        kContentSecurityPolicyHeaderTypeEnforce,
                        kContentSecurityPolicyHeaderSourceHTTP);
  EXPECT_TRUE(csp->AllowTrustedTypePolicy("somepolicy", false));
  EXPECT_TRUE(csp->AllowTrustedTypePolicy("somepolicy", true));
}

TEST_F(ContentSecurityPolicyTest, TrustedTypesReserved) {
  execution_context->SetRequireTrustedTypesForTesting();
  csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
  csp->DidReceiveHeader("trusted-types one \"two\" 'three'",
                        kContentSecurityPolicyHeaderTypeEnforce,
                        kContentSecurityPolicyHeaderSourceHTTP);
  EXPECT_TRUE(csp->AllowTrustedTypePolicy("one", false));
  EXPECT_TRUE(csp->AllowTrustedTypePolicy("one", false));

  // Quoted strings are considered 'reserved':
  EXPECT_FALSE(csp->AllowTrustedTypePolicy("two", false));
  EXPECT_FALSE(csp->AllowTrustedTypePolicy("\"two\"", false));
  EXPECT_FALSE(csp->AllowTrustedTypePolicy("three", false));
  EXPECT_FALSE(csp->AllowTrustedTypePolicy("'three'", false));
  EXPECT_FALSE(csp->AllowTrustedTypePolicy("two", true));
  EXPECT_FALSE(csp->AllowTrustedTypePolicy("\"two\"", true));
  EXPECT_FALSE(csp->AllowTrustedTypePolicy("three", true));
  EXPECT_FALSE(csp->AllowTrustedTypePolicy("'three'", true));
}

TEST_F(ContentSecurityPolicyTest, TrustedTypesReportingStar) {
  execution_context->SetRequireTrustedTypesForTesting();
  csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
  csp->DidReceiveHeader("trusted-types *",
                        kContentSecurityPolicyHeaderTypeReport,
                        kContentSecurityPolicyHeaderSourceHTTP);
  EXPECT_TRUE(csp->AllowTrustedTypePolicy("somepolicy", false));
  EXPECT_TRUE(csp->AllowTrustedTypePolicy("somepolicy", true));
}

TEST_F(ContentSecurityPolicyTest, TrustedTypeReportingSimple) {
  execution_context->SetRequireTrustedTypesForTesting();
  csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
  csp->DidReceiveHeader("trusted-types a b c",
                        kContentSecurityPolicyHeaderTypeReport,
                        kContentSecurityPolicyHeaderSourceHTTP);
  EXPECT_TRUE(csp->AllowTrustedTypePolicy("a", false));
  EXPECT_TRUE(csp->AllowTrustedTypePolicy("a", true));
}

TEST_F(ContentSecurityPolicyTest, TrustedTypeEnforce) {
  execution_context->SetRequireTrustedTypesForTesting();
  csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
  csp->DidReceiveHeader("trusted-types one\ntwo\rthree",
                        kContentSecurityPolicyHeaderTypeEnforce,
                        kContentSecurityPolicyHeaderSourceHTTP);
  EXPECT_TRUE(csp->IsRequireTrustedTypes());
  EXPECT_FALSE(csp->AllowTrustedTypeAssignmentFailure("blabla"));
}

TEST_F(ContentSecurityPolicyTest, TrustedTypeReport) {
  execution_context->SetRequireTrustedTypesForTesting();
  csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
  csp->DidReceiveHeader("trusted-types one\ntwo\rthree",
                        kContentSecurityPolicyHeaderTypeReport,
                        kContentSecurityPolicyHeaderSourceHTTP);
  EXPECT_TRUE(csp->IsRequireTrustedTypes());
  EXPECT_TRUE(csp->AllowTrustedTypeAssignmentFailure("blabla"));
}

TEST_F(ContentSecurityPolicyTest, TrustedTypeReportAndEnforce) {
  execution_context->SetRequireTrustedTypesForTesting();
  csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
  csp->DidReceiveHeader("trusted-types one",
                        kContentSecurityPolicyHeaderTypeReport,
                        kContentSecurityPolicyHeaderSourceHTTP);
  csp->DidReceiveHeader("trusted-types two",
                        kContentSecurityPolicyHeaderTypeEnforce,
                        kContentSecurityPolicyHeaderSourceHTTP);
  EXPECT_TRUE(csp->IsRequireTrustedTypes());
  EXPECT_FALSE(csp->AllowTrustedTypeAssignmentFailure("blabla"));
}

TEST_F(ContentSecurityPolicyTest, TrustedTypeReportAndNonTTEnforce) {
  execution_context->SetRequireTrustedTypesForTesting();
  csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
  csp->DidReceiveHeader("trusted-types one",
                        kContentSecurityPolicyHeaderTypeReport,
                        kContentSecurityPolicyHeaderSourceHTTP);
  csp->DidReceiveHeader("script-src none",
                        kContentSecurityPolicyHeaderTypeEnforce,
                        kContentSecurityPolicyHeaderSourceHTTP);
  EXPECT_TRUE(csp->IsRequireTrustedTypes());
  EXPECT_TRUE(csp->AllowTrustedTypeAssignmentFailure("blabla"));
}

TEST_F(ContentSecurityPolicyTest, DefaultPolicy) {
  execution_context->SetRequireTrustedTypesForTesting();
  csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
  csp->DidReceiveHeader("trusted-types *",
                        kContentSecurityPolicyHeaderTypeEnforce,
                        kContentSecurityPolicyHeaderSourceHTTP);
  EXPECT_TRUE(csp->AllowTrustedTypePolicy("default", false));
  EXPECT_FALSE(csp->AllowTrustedTypePolicy("default", true));
}

TEST_F(ContentSecurityPolicyTest, DirectiveNameCaseInsensitive) {
  KURL example_url("http://example.com");
  KURL not_example_url("http://not-example.com");

  // Directive name is case insensitive.
  csp = MakeGarbageCollected<ContentSecurityPolicy>();
  csp->DidReceiveHeader("sCrIpt-sRc http://example.com",
                        kContentSecurityPolicyHeaderTypeEnforce,
                        kContentSecurityPolicyHeaderSourceHTTP);
  csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());

  EXPECT_TRUE(csp->AllowScriptFromSource(
      example_url, String(), IntegrityMetadataSet(), kParserInserted));
  EXPECT_FALSE(csp->AllowScriptFromSource(
      not_example_url, String(), IntegrityMetadataSet(), kParserInserted));

  // Duplicate directive that is in a different case pattern is
  // correctly treated as a duplicate directive and ignored.
  csp = MakeGarbageCollected<ContentSecurityPolicy>();
  csp->DidReceiveHeader(
      "SCRipt-SRC http://example.com; script-src http://not-example.com;",
      kContentSecurityPolicyHeaderTypeEnforce,
      kContentSecurityPolicyHeaderSourceHTTP);
  csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());

  EXPECT_TRUE(csp->AllowScriptFromSource(
      example_url, String(), IntegrityMetadataSet(), kParserInserted));
  EXPECT_FALSE(csp->AllowScriptFromSource(
      not_example_url, String(), IntegrityMetadataSet(), kParserInserted));
}

// Tests that using an empty CSP works and doesn't impose any policy
// restrictions.
TEST_F(ContentSecurityPolicyTest, EmptyCSPIsNoOp) {
  csp = MakeGarbageCollected<ContentSecurityPolicy>();
  csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());

  const KURL example_url("http://example.com");
  auto* document = MakeGarbageCollected<Document>();
  String source;
  String context_url;
  String nonce;
  OrdinalNumber ordinal_number;
  auto* element = MakeGarbageCollected<HTMLScriptElement>(
      *document, CreateElementFlags::ByParser());

  EXPECT_TRUE(csp->Headers().IsEmpty());
  EXPECT_TRUE(csp->AllowInline(ContentSecurityPolicy::InlineType::kNavigation,
                               element, source, String() /* nonce */,
                               context_url, ordinal_number));
  EXPECT_TRUE(csp->AllowInline(
      ContentSecurityPolicy::InlineType::kScriptAttribute, element, source,
      String() /* nonce */, context_url, ordinal_number));
  EXPECT_TRUE(csp->AllowEval(SecurityViolationReportingPolicy::kReport,
                             ContentSecurityPolicy::kWillNotThrowException,
                             g_empty_string));
  EXPECT_TRUE(csp->AllowWasmEval(SecurityViolationReportingPolicy::kReport,
                                 ContentSecurityPolicy::kWillNotThrowException,
                                 g_empty_string));
  EXPECT_TRUE(csp->AllowPluginType("application/x-type-1",
                                   "application/x-type-1", example_url));
  EXPECT_TRUE(csp->AllowPluginTypeForDocument(
      *document, "application/x-type-1", "application/x-type-1", example_url,
      SecurityViolationReportingPolicy::kSuppressReporting));

  ContentSecurityPolicy::DirectiveType types_to_test[] = {
      ContentSecurityPolicy::DirectiveType::kBaseURI,
      ContentSecurityPolicy::DirectiveType::kConnectSrc,
      ContentSecurityPolicy::DirectiveType::kFontSrc,
      ContentSecurityPolicy::DirectiveType::kFormAction,
      ContentSecurityPolicy::DirectiveType::kFrameSrc,
      ContentSecurityPolicy::DirectiveType::kImgSrc,
      ContentSecurityPolicy::DirectiveType::kManifestSrc,
      ContentSecurityPolicy::DirectiveType::kMediaSrc,
      ContentSecurityPolicy::DirectiveType::kObjectSrc,
      ContentSecurityPolicy::DirectiveType::kPrefetchSrc,
      ContentSecurityPolicy::DirectiveType::kScriptSrcElem,
      ContentSecurityPolicy::DirectiveType::kStyleSrcElem,
      ContentSecurityPolicy::DirectiveType::kWorkerSrc};
  for (auto type : types_to_test) {
    EXPECT_TRUE(csp->AllowFromSource(type, example_url));
  }

  EXPECT_TRUE(csp->AllowObjectFromSource(example_url));
  EXPECT_TRUE(csp->AllowImageFromSource(example_url));
  EXPECT_TRUE(csp->AllowMediaFromSource(example_url));
  EXPECT_TRUE(csp->AllowConnectToSource(example_url));
  EXPECT_TRUE(csp->AllowFormAction(example_url));
  EXPECT_TRUE(csp->AllowBaseURI(example_url));
  EXPECT_TRUE(csp->AllowWorkerContextFromSource(example_url));
  EXPECT_TRUE(csp->AllowScriptFromSource(
      example_url, nonce, IntegrityMetadataSet(), kParserInserted));

  EXPECT_TRUE(csp->AllowTrustedTypePolicy("somepolicy", true));
  EXPECT_TRUE(csp->AllowTrustedTypePolicy("somepolicy", false));
  EXPECT_TRUE(csp->AllowInline(ContentSecurityPolicy::InlineType::kScript,
                               element, source, nonce, context_url,
                               ordinal_number));
  EXPECT_TRUE(csp->AllowInline(ContentSecurityPolicy::InlineType::kStyle,
                               element, source, nonce, context_url,
                               ordinal_number));
  EXPECT_TRUE(csp->AllowAncestors(document->GetFrame(), example_url));
  EXPECT_FALSE(csp->IsFrameAncestorsEnforced());
  EXPECT_TRUE(csp->AllowRequestWithoutIntegrity(
      mojom::RequestContextType::SCRIPT, example_url));
  EXPECT_TRUE(csp->AllowRequest(mojom::RequestContextType::SCRIPT, example_url,
                                nonce, IntegrityMetadataSet(),
                                kParserInserted));
  EXPECT_FALSE(csp->IsActive());
  EXPECT_FALSE(csp->IsActiveForConnections());
  EXPECT_TRUE(csp->FallbackUrlForPlugin().IsEmpty());
  EXPECT_EQ(kLeaveInsecureRequestsAlone, csp->GetInsecureRequestPolicy());
  EXPECT_FALSE(csp->HasHeaderDeliveredPolicy());
  EXPECT_FALSE(csp->SupportsWasmEval());
  EXPECT_EQ(WebSandboxFlags::kNone, csp->GetSandboxMask());
  EXPECT_FALSE(
      csp->HasPolicyFromSource(kContentSecurityPolicyHeaderSourceHTTP));
}

TEST_F(ContentSecurityPolicyTest, OpaqueOriginBeforeBind) {
  const KURL url("https://example.test");

  // Security Origin of execution context might change when sandbox flags
  // are applied. This shouldn't change the application of the 'self'
  // determination.
  secure_origin = secure_origin->DeriveNewOpaqueOrigin();
  execution_context = CreateExecutionContext();
  csp->BindToDelegate(execution_context->GetContentSecurityPolicyDelegate());
  csp->DidReceiveHeader("default-src 'self';",
                        kContentSecurityPolicyHeaderTypeEnforce,
                        kContentSecurityPolicyHeaderSourceMeta);
  EXPECT_TRUE(
      csp->AllowRequest(mojom::RequestContextType::SUBRESOURCE, url, String(),
                        IntegrityMetadataSet(), kParserInserted,
                        ResourceRequest::RedirectStatus::kNoRedirect,
                        SecurityViolationReportingPolicy::kSuppressReporting));
}

}  // namespace blink
