// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/csp/csp_directive_list.h"

#include <optional>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/content_security_policy.mojom-blink.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/csp/test_util.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/subresource_integrity.h"
#include "third_party/blink/renderer/platform/network/content_security_policy_parsers.h"
#include "third_party/blink/renderer/platform/weborigin/reporting_disposition.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/string_operators.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

using network::mojom::ContentSecurityPolicySource;
using network::mojom::ContentSecurityPolicyType;

class CSPDirectiveListTest : public testing::Test {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures({network::features::kReporting}, {});
  }

  network::mojom::blink::ContentSecurityPolicyPtr CreateList(
      const String& list,
      ContentSecurityPolicyType type,
      ContentSecurityPolicySource source = ContentSecurityPolicySource::kHTTP) {
    Vector<network::mojom::blink::ContentSecurityPolicyPtr> parsed =
        ParseContentSecurityPolicies(list, type, source,
                                     KURL("https://example.test/index.html"));
    return std::move(parsed[0]);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(CSPDirectiveListTest, Header) {
  struct TestCase {
    const char* list;
    const char* expected;
  } cases[] = {{"script-src 'self'", "script-src 'self'"},
               {"  script-src 'self'  ", "script-src 'self'"},
               {"\t\tscript-src 'self'", "script-src 'self'"},
               {"script-src 'self' \t", "script-src 'self'"}};

  for (const auto& test : cases) {
    network::mojom::blink::ContentSecurityPolicyPtr directive_list =
        CreateList(test.list, ContentSecurityPolicyType::kReport);
    EXPECT_EQ(test.expected, directive_list->header->header_value);
    directive_list = CreateList(test.list, ContentSecurityPolicyType::kEnforce);
    EXPECT_EQ(test.expected, directive_list->header->header_value);
  }
}

TEST_F(CSPDirectiveListTest, IsMatchingNoncePresent) {
  struct TestCase {
    const char* list;
    const char* nonce;
    bool expected;
  } cases[] = {
      {"script-src 'self'", "yay", false},
      {"script-src 'self'", "boo", false},
      {"script-src 'nonce-yay'", "yay", true},
      {"script-src 'nonce-yay'", "boo", false},
      {"script-src 'nonce-yay' 'nonce-boo'", "yay", true},
      {"script-src 'nonce-yay' 'nonce-boo'", "boo", true},

      // Falls back to 'default-src'
      {"default-src 'nonce-yay'", "yay", true},
      {"default-src 'nonce-yay'", "boo", false},
      {"default-src 'nonce-boo'; script-src 'nonce-yay'", "yay", true},
      {"default-src 'nonce-boo'; script-src 'nonce-yay'", "boo", false},

      // Unrelated directives do not affect result
      {"style-src 'nonce-yay'; default-src 'none'", "yay", false},
      {"style-src 'nonce-yay'; default-src 'none'", "boo", false},
      {"script-src-attr 'nonce-yay'; default-src 'none'", "yay", false},

      // Script-src-elem falls back on script-src and then default-src.
      {"script-src 'nonce-yay'", "yay", true},
      {"script-src 'nonce-yay'; default-src 'nonce-boo'", "yay", true},
      {"script-src 'nonce-boo'; default-src 'nonce-yay'", "yay", false},
      {"script-src-elem 'nonce-yay'; script-src 'nonce-boo'; default-src "
       "'nonce-boo'",
       "yay", true},
      {"default-src 'nonce-yay'", "yay", true},

      {"script-src-attr 'nonce-yay'; script-src 'nonce-boo'; default-src "
       "'nonce-foo'",
       "yay", false},
      {"script-src-attr 'nonce-yay'; script-src 'nonce-boo'; default-src "
       "'nonce-foo'",
       "boo", true},
      {"script-src-attr 'nonce-yay'; script-src 'nonce-boo'; default-src "
       "'nonce-foo'",
       "foo", false},
  };

  ContentSecurityPolicy* context =
      MakeGarbageCollected<ContentSecurityPolicy>();
  TestCSPDelegate* test_delegate = MakeGarbageCollected<TestCSPDelegate>();
  context->BindToDelegate(*test_delegate);

  KURL blocked_url = KURL("https://blocked.com");
  for (const auto& test : cases) {
    for (auto reporting_disposition : {ReportingDisposition::kSuppressReporting,
                                       ReportingDisposition::kReport}) {
      // Report-only
      network::mojom::blink::ContentSecurityPolicyPtr directive_list =
          CreateList(test.list, ContentSecurityPolicyType::kReport);

      EXPECT_TRUE(CSPDirectiveListAllowFromSource(
          *directive_list, context, CSPDirectiveName::ScriptSrcElem,
          blocked_url, blocked_url,
          ResourceRequest::RedirectStatus::kNoRedirect, reporting_disposition,
          test.nonce));

      // Enforce
      directive_list =
          CreateList(test.list, ContentSecurityPolicyType::kEnforce);
      EXPECT_EQ(CSPCheckResult(test.expected),
                CSPDirectiveListAllowFromSource(
                    *directive_list, context, CSPDirectiveName::ScriptSrcElem,
                    blocked_url, blocked_url,
                    ResourceRequest::RedirectStatus::kNoRedirect,
                    reporting_disposition, test.nonce));
    }
  }
}

TEST_F(CSPDirectiveListTest, AllowScriptFromSourceNoNonce) {
  struct TestCase {
    const char* list;
    const char* url;
    bool expected;
  } cases[] = {
      {"script-src https://example.com", "https://example.com/script.js", true},
      {"script-src https://example.com/", "https://example.com/script.js",
       true},
      {"script-src https://example.com/",
       "https://example.com/script/script.js", true},
      {"script-src https://example.com/script", "https://example.com/script.js",
       false},
      {"script-src https://example.com/script",
       "https://example.com/script/script.js", false},
      {"script-src https://example.com/script/",
       "https://example.com/script.js", false},
      {"script-src https://example.com/script/",
       "https://example.com/script/script.js", true},
      {"script-src https://example.com", "https://not.example.com/script.js",
       false},
      {"script-src https://*.example.com", "https://not.example.com/script.js",
       true},
      {"script-src https://*.example.com", "https://example.com/script.js",
       false},

      // Falls back to default-src:
      {"default-src https://example.com", "https://example.com/script.js",
       true},
      {"default-src https://example.com/", "https://example.com/script.js",
       true},
      {"default-src https://example.com/",
       "https://example.com/script/script.js", true},
      {"default-src https://example.com/script",
       "https://example.com/script.js", false},
      {"default-src https://example.com/script",
       "https://example.com/script/script.js", false},
      {"default-src https://example.com/script/",
       "https://example.com/script.js", false},
      {"default-src https://example.com/script/",
       "https://example.com/script/script.js", true},
      {"default-src https://example.com", "https://not.example.com/script.js",
       false},
      {"default-src https://*.example.com", "https://not.example.com/script.js",
       true},
      {"default-src https://*.example.com", "https://example.com/script.js",
       false},
  };

  ContentSecurityPolicy* context =
      MakeGarbageCollected<ContentSecurityPolicy>();
  TestCSPDelegate* test_delegate = MakeGarbageCollected<TestCSPDelegate>();
  context->BindToDelegate(*test_delegate);

  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message()
                 << "List: `" << test.list << "`, URL: `" << test.url << "`");
    const KURL script_src(test.url);

    // Report-only
    network::mojom::blink::ContentSecurityPolicyPtr directive_list =
        CreateList(test.list, ContentSecurityPolicyType::kReport);
    EXPECT_TRUE(CSPDirectiveListAllowFromSource(
        *directive_list, context, CSPDirectiveName::ScriptSrcElem, script_src,
        script_src, ResourceRequest::RedirectStatus::kNoRedirect,
        ReportingDisposition::kSuppressReporting, String(),
        IntegrityMetadataSet(), kParserInserted));

    // Enforce
    directive_list = CreateList(test.list, ContentSecurityPolicyType::kEnforce);
    EXPECT_EQ(CSPCheckResult(test.expected),
              CSPDirectiveListAllowFromSource(
                  *directive_list, context, CSPDirectiveName::ScriptSrcElem,
                  script_src, script_src,
                  ResourceRequest::RedirectStatus::kNoRedirect,
                  ReportingDisposition::kSuppressReporting, String(),
                  IntegrityMetadataSet(), kParserInserted));
  }
}

TEST_F(CSPDirectiveListTest, AllowFromSourceWithNonce) {
  struct TestCase {
    const char* list;
    const char* url;
    const char* nonce;
    bool expected;
  } cases[] = {
      // Doesn't affect lists without nonces:
      {"https://example.com", "https://example.com/file", "yay", true},
      {"https://example.com", "https://example.com/file", "boo", true},
      {"https://example.com", "https://example.com/file", "", true},
      {"https://example.com", "https://not.example.com/file", "yay", false},
      {"https://example.com", "https://not.example.com/file", "boo", false},
      {"https://example.com", "https://not.example.com/file", "", false},

      // Doesn't affect URLs that match the allowlist.
      {"https://example.com 'nonce-yay'", "https://example.com/file", "yay",
       true},
      {"https://example.com 'nonce-yay'", "https://example.com/file", "boo",
       true},
      {"https://example.com 'nonce-yay'", "https://example.com/file", "", true},

      // Does affect URLs that don't.
      {"https://example.com 'nonce-yay'", "https://not.example.com/file", "yay",
       true},
      {"https://example.com 'nonce-yay'", "https://not.example.com/file", "boo",
       false},
      {"https://example.com 'nonce-yay'", "https://not.example.com/file", "",
       false},
  };

  ContentSecurityPolicy* context =
      MakeGarbageCollected<ContentSecurityPolicy>();
  TestCSPDelegate* test_delegate = MakeGarbageCollected<TestCSPDelegate>();
  context->BindToDelegate(*test_delegate);

  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message()
                 << "List: `" << test.list << "`, URL: `" << test.url << "`");
    const KURL resource(test.url);

    // Report-only 'script-src'
    network::mojom::blink::ContentSecurityPolicyPtr directive_list = CreateList(
        String("script-src ") + test.list, ContentSecurityPolicyType::kReport);
    EXPECT_TRUE(CSPDirectiveListAllowFromSource(
        *directive_list, context, CSPDirectiveName::ScriptSrcElem, resource,
        resource, ResourceRequest::RedirectStatus::kNoRedirect,
        ReportingDisposition::kSuppressReporting, String(test.nonce),
        IntegrityMetadataSet(), kParserInserted));

    // Enforce 'script-src'
    directive_list = CreateList(String("script-src ") + test.list,
                                ContentSecurityPolicyType::kEnforce);
    EXPECT_EQ(
        CSPCheckResult(test.expected),
        CSPDirectiveListAllowFromSource(
            *directive_list, context, CSPDirectiveName::ScriptSrcElem, resource,
            resource, ResourceRequest::RedirectStatus::kNoRedirect,
            ReportingDisposition::kSuppressReporting, String(test.nonce),
            IntegrityMetadataSet(), kParserInserted));

    // Report-only 'style-src'
    directive_list = CreateList(String("style-src ") + test.list,
                                ContentSecurityPolicyType::kReport);
    EXPECT_TRUE(CSPDirectiveListAllowFromSource(
        *directive_list, context, CSPDirectiveName::StyleSrcElem, resource,
        resource, ResourceRequest::RedirectStatus::kNoRedirect,
        ReportingDisposition::kSuppressReporting, String(test.nonce)));

    // Enforce 'style-src'
    directive_list = CreateList(String("style-src ") + test.list,
                                ContentSecurityPolicyType::kEnforce);
    EXPECT_EQ(
        CSPCheckResult(test.expected),
        CSPDirectiveListAllowFromSource(
            *directive_list, context, CSPDirectiveName::StyleSrcElem, resource,
            resource, ResourceRequest::RedirectStatus::kNoRedirect,
            ReportingDisposition::kSuppressReporting, String(test.nonce)));

    // Report-only 'style-src'
    directive_list = CreateList(String("default-src ") + test.list,
                                ContentSecurityPolicyType::kReport);
    EXPECT_TRUE(CSPDirectiveListAllowFromSource(
        *directive_list, context, CSPDirectiveName::ScriptSrcElem, resource,
        resource, ResourceRequest::RedirectStatus::kNoRedirect,
        ReportingDisposition::kSuppressReporting, String(test.nonce)));
    EXPECT_TRUE(CSPDirectiveListAllowFromSource(
        *directive_list, context, CSPDirectiveName::StyleSrcElem, resource,
        resource, ResourceRequest::RedirectStatus::kNoRedirect,
        ReportingDisposition::kSuppressReporting, String(test.nonce)));

    // Enforce 'style-src'
    directive_list = CreateList(String("default-src ") + test.list,
                                ContentSecurityPolicyType::kEnforce);
    EXPECT_EQ(
        CSPCheckResult(test.expected),
        CSPDirectiveListAllowFromSource(
            *directive_list, context, CSPDirectiveName::ScriptSrcElem, resource,
            resource, ResourceRequest::RedirectStatus::kNoRedirect,
            ReportingDisposition::kSuppressReporting, String(test.nonce),
            IntegrityMetadataSet(), kParserInserted));
    EXPECT_EQ(
        CSPCheckResult(test.expected),
        CSPDirectiveListAllowFromSource(
            *directive_list, context, CSPDirectiveName::StyleSrcElem, resource,
            resource, ResourceRequest::RedirectStatus::kNoRedirect,
            ReportingDisposition::kSuppressReporting, String(test.nonce)));
  }
}

TEST_F(CSPDirectiveListTest, AllowScriptFromSourceWithHash) {
  struct TestCase {
    const char* list;
    const char* url;
    const char* integrity;
    bool expected;
  } cases[] = {
      // Doesn't affect lists without hashes.
      {"https://example.com", "https://example.com/file", "sha256-yay", true},
      {"https://example.com", "https://example.com/file", "sha256-boo", true},
      {"https://example.com", "https://example.com/file", "", true},
      {"https://example.com", "https://not.example.com/file", "sha256-yay",
       false},
      {"https://example.com", "https://not.example.com/file", "sha256-boo",
       false},
      {"https://example.com", "https://not.example.com/file", "", false},

      // Doesn't affect URLs that match the allowlist.
      {"https://example.com 'sha256-yay'", "https://example.com/file",
       "sha256-yay", true},
      {"https://example.com 'sha256-yay'", "https://example.com/file",
       "sha256-boo", true},
      {"https://example.com 'sha256-yay'", "https://example.com/file", "",
       true},

      // Does affect URLs that don't match the allowlist.
      {"https://example.com 'sha256-yay'", "https://not.example.com/file",
       "sha256-yay", true},
      {"https://example.com 'sha256-yay'", "https://not.example.com/file",
       "sha256-boo", false},
      {"https://example.com 'sha256-yay'", "https://not.example.com/file", "",
       false},

      // Both algorithm and digest must match.
      {"'sha256-yay'", "https://a.com/file", "sha384-yay", false},

      // Sha-1 is not supported, but -384 and -512 are.
      {"'sha1-yay'", "https://a.com/file", "sha1-yay", false},
      {"'sha384-yay'", "https://a.com/file", "sha384-yay", true},
      {"'sha512-yay'", "https://a.com/file", "sha512-yay", true},

      // Unknown (or future) hash algorithms don't work.
      {"'asdf256-yay'", "https://a.com/file", "asdf256-yay", false},

      // But they also don't interfere.
      {"'sha256-yay'", "https://a.com/file", "sha256-yay asdf256-boo", true},

      // Additional allowlisted hashes in the CSP don't interfere.
      {"'sha256-yay' 'sha384-boo'", "https://a.com/file", "sha256-yay", true},
      {"'sha256-yay' 'sha384-boo'", "https://a.com/file", "sha384-boo", true},

      // All integrity hashes must appear in the CSP (and match).
      {"'sha256-yay'", "https://a.com/file", "sha256-yay sha384-boo", false},
      {"'sha384-boo'", "https://a.com/file", "sha256-yay sha384-boo", false},
      {"'sha256-yay' 'sha384-boo'", "https://a.com/file",
       "sha256-yay sha384-yay", false},
      {"'sha256-yay' 'sha384-boo'", "https://a.com/file",
       "sha256-boo sha384-boo", false},
      {"'sha256-yay' 'sha384-boo'", "https://a.com/file",
       "sha256-yay sha384-boo", true},

      // At least one integrity hash must be present.
      {"'sha256-yay'", "https://a.com/file", "", false},
  };

  ContentSecurityPolicy* context =
      MakeGarbageCollected<ContentSecurityPolicy>();
  TestCSPDelegate* test_delegate = MakeGarbageCollected<TestCSPDelegate>();
  context->BindToDelegate(*test_delegate);

  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message()
                 << "List: `" << test.list << "`, URL: `" << test.url
                 << "`, Integrity: `" << test.integrity << "`");
    const KURL resource(test.url);

    IntegrityMetadataSet integrity_metadata;
    SubresourceIntegrity::ParseIntegrityAttribute(
        test.integrity, SubresourceIntegrity::IntegrityFeatures::kDefault,
        integrity_metadata);

    // Report-only 'script-src'
    network::mojom::blink::ContentSecurityPolicyPtr directive_list = CreateList(
        String("script-src ") + test.list, ContentSecurityPolicyType::kReport);
    EXPECT_TRUE(CSPDirectiveListAllowFromSource(
        *directive_list, context, CSPDirectiveName::ScriptSrcElem, resource,
        resource, ResourceRequest::RedirectStatus::kNoRedirect,
        ReportingDisposition::kSuppressReporting, String(), integrity_metadata,
        kParserInserted));

    // Enforce 'script-src'
    directive_list = CreateList(String("script-src ") + test.list,
                                ContentSecurityPolicyType::kEnforce);
    EXPECT_EQ(
        CSPCheckResult(test.expected),
        CSPDirectiveListAllowFromSource(
            *directive_list, context, CSPDirectiveName::ScriptSrcElem, resource,
            resource, ResourceRequest::RedirectStatus::kNoRedirect,
            ReportingDisposition::kSuppressReporting, String(),
            integrity_metadata, kParserInserted));
  }
}

TEST_F(CSPDirectiveListTest, WorkerSrc) {
  struct TestCase {
    const char* list;
    bool allowed;
  } cases[] = {
      {"worker-src 'none'", false},
      {"worker-src http://not.example.test", false},
      {"worker-src https://example.test", true},
      {"default-src *; worker-src 'none'", false},
      {"default-src *; worker-src http://not.example.test", false},
      {"default-src *; worker-src https://example.test", true},
      {"script-src *; worker-src 'none'", false},
      {"script-src *; worker-src http://not.example.test", false},
      {"script-src *; worker-src https://example.test", true},
      {"default-src *; script-src *; worker-src 'none'", false},
      {"default-src *; script-src *; worker-src http://not.example.test",
       false},
      {"default-src *; script-src *; worker-src https://example.test", true},

      // Fallback to script-src.
      {"script-src 'none'", false},
      {"script-src http://not.example.test", false},
      {"script-src https://example.test", true},
      {"default-src *; script-src 'none'", false},
      {"default-src *; script-src http://not.example.test", false},
      {"default-src *; script-src https://example.test", true},

      // Fallback to default-src.
      {"default-src 'none'", false},
      {"default-src http://not.example.test", false},
      {"default-src https://example.test", true},
  };

  ContentSecurityPolicy* context =
      MakeGarbageCollected<ContentSecurityPolicy>();
  TestCSPDelegate* test_delegate = MakeGarbageCollected<TestCSPDelegate>();
  context->BindToDelegate(*test_delegate);

  for (const auto& test : cases) {
    SCOPED_TRACE(test.list);
    const KURL resource("https://example.test/worker.js");
    network::mojom::blink::ContentSecurityPolicyPtr directive_list =
        CreateList(test.list, ContentSecurityPolicyType::kEnforce);
    EXPECT_EQ(
        CSPCheckResult(test.allowed),
        CSPDirectiveListAllowFromSource(
            *directive_list, context, CSPDirectiveName::WorkerSrc, resource,
            resource, ResourceRequest::RedirectStatus::kNoRedirect,
            ReportingDisposition::kSuppressReporting));
  }
}

TEST_F(CSPDirectiveListTest, WorkerSrcChildSrcFallback) {
  // TODO(mkwst): Remove this test once we remove the temporary fallback
  // behavior. https://crbug.com/662930
  struct TestCase {
    const char* list;
    bool allowed;
  } cases[] = {
      // When 'worker-src' is not present, 'child-src' can allow a worker when
      // present.
      {"child-src https://example.test", true},
      {"child-src https://not-example.test", false},
      {"script-src https://example.test", true},
      {"script-src https://not-example.test", false},
      {"child-src https://example.test; script-src https://example.test", true},
      {"child-src https://example.test; script-src https://not-example.test",
       true},
      {"child-src https://not-example.test; script-src https://example.test",
       false},
      {"child-src https://not-example.test; script-src "
       "https://not-example.test",
       false},

      // If 'worker-src' is present, 'child-src' will not allow a worker.
      {"worker-src https://example.test; child-src https://example.test", true},
      {"worker-src https://example.test; child-src https://not-example.test",
       true},
      {"worker-src https://not-example.test; child-src https://example.test",
       false},
      {"worker-src https://not-example.test; child-src "
       "https://not-example.test",
       false},
  };

  ContentSecurityPolicy* context =
      MakeGarbageCollected<ContentSecurityPolicy>();
  TestCSPDelegate* test_delegate = MakeGarbageCollected<TestCSPDelegate>();
  context->BindToDelegate(*test_delegate);

  for (const auto& test : cases) {
    SCOPED_TRACE(test.list);
    const KURL resource("https://example.test/worker.js");
    network::mojom::blink::ContentSecurityPolicyPtr directive_list =
        CreateList(test.list, ContentSecurityPolicyType::kEnforce);
    EXPECT_EQ(
        CSPCheckResult(test.allowed),
        CSPDirectiveListAllowFromSource(
            *directive_list, context, CSPDirectiveName::WorkerSrc, resource,
            resource, ResourceRequest::RedirectStatus::kNoRedirect,
            ReportingDisposition::kSuppressReporting));
  }
}

TEST_F(CSPDirectiveListTest, OperativeDirectiveGivenType) {
  struct TestCase {
    CSPDirectiveName directive;
    Vector<CSPDirectiveName> fallback_list;
  } cases[] = {
      // Directives with default directive.
      {CSPDirectiveName::ChildSrc, {CSPDirectiveName::DefaultSrc}},
      {CSPDirectiveName::ConnectSrc, {CSPDirectiveName::DefaultSrc}},
      {CSPDirectiveName::FontSrc, {CSPDirectiveName::DefaultSrc}},
      {CSPDirectiveName::ImgSrc, {CSPDirectiveName::DefaultSrc}},
      {CSPDirectiveName::ManifestSrc, {CSPDirectiveName::DefaultSrc}},
      {CSPDirectiveName::MediaSrc, {CSPDirectiveName::DefaultSrc}},
      {CSPDirectiveName::ObjectSrc, {CSPDirectiveName::DefaultSrc}},
      {CSPDirectiveName::ScriptSrc, {CSPDirectiveName::DefaultSrc}},
      {CSPDirectiveName::StyleSrc, {CSPDirectiveName::DefaultSrc}},
      // Directives with no default directive.
      {CSPDirectiveName::BaseURI, {}},
      {CSPDirectiveName::DefaultSrc, {}},
      {CSPDirectiveName::FrameAncestors, {}},
      {CSPDirectiveName::FormAction, {}},
      // Directive with multiple default directives.
      {CSPDirectiveName::ScriptSrcAttr,
       {CSPDirectiveName::ScriptSrc, CSPDirectiveName::DefaultSrc}},
      {CSPDirectiveName::ScriptSrcElem,
       {CSPDirectiveName::ScriptSrc, CSPDirectiveName::DefaultSrc}},
      {CSPDirectiveName::FrameSrc,
       {CSPDirectiveName::ChildSrc, CSPDirectiveName::DefaultSrc}},
      {CSPDirectiveName::WorkerSrc,
       {CSPDirectiveName::ChildSrc, CSPDirectiveName::ScriptSrc,
        CSPDirectiveName::DefaultSrc}},
  };

  std::stringstream all_directives;
  for (const auto& test : cases) {
    const char* name = ContentSecurityPolicy::GetDirectiveName(test.directive);
    all_directives << name << " http://" << name << ".com; ";
  }

  network::mojom::blink::ContentSecurityPolicyPtr empty =
      CreateList("nonexistent-directive", ContentSecurityPolicyType::kEnforce);

  std::string directive_string;
  network::mojom::blink::ContentSecurityPolicyPtr directive_list;
  // Initial set-up.
  for (auto& test : cases) {
    // With an empty directive list the returned directive should always be
    // null.
    EXPECT_FALSE(
        CSPDirectiveListOperativeDirective(*empty, test.directive).source_list);

    // Add the directive itself as it should be the first one to be returned.
    test.fallback_list.push_front(test.directive);

    // Start the tests with all directives present.
    directive_string = all_directives.str();

    while (!test.fallback_list.empty()) {
      directive_list = CreateList(directive_string.c_str(),
                                  ContentSecurityPolicyType::kEnforce);

      CSPOperativeDirective operative_directive =
          CSPDirectiveListOperativeDirective(*directive_list, test.directive);

      // We should have an actual directive returned here.
      EXPECT_TRUE(operative_directive.source_list);

      // The OperativeDirective should be first one in the fallback chain.
      EXPECT_EQ(test.fallback_list.front(), operative_directive.type);

      // Remove the first directive in the fallback chain from the directive
      // list and continue by testing that the next one is returned until we
      // have no more directives in the fallback list.
      const char* current_directive_name =
          ContentSecurityPolicy::GetDirectiveName(test.fallback_list.front());

      std::stringstream current_directive;
      current_directive << current_directive_name << " http://"
                        << current_directive_name << ".com; ";

      size_t index = directive_string.find(current_directive.str());
      directive_string.replace(index, current_directive.str().size(), "");

      test.fallback_list.erase(test.fallback_list.begin());
    }

    // After we have checked and removed all the directives in the fallback
    // chain we should ensure that there is no unexpected directive outside of
    // the fallback chain that is returned.
    directive_list = CreateList(directive_string.c_str(),
                                ContentSecurityPolicyType::kEnforce);
    EXPECT_FALSE(
        CSPDirectiveListOperativeDirective(*directive_list, test.directive)
            .source_list);
  }
}

TEST_F(CSPDirectiveListTest, ReportEndpointsProperlyParsed) {
  struct TestCase {
    const char* policy;
    ContentSecurityPolicySource header_source;
    Vector<String> expected_endpoints;
    bool expected_use_reporting_api;
  } cases[] = {
      {"script-src 'self';", ContentSecurityPolicySource::kHTTP, {}, false},
      {"script-src 'self'; report-uri https://example.com",
       ContentSecurityPolicySource::kHTTP,
       {"https://example.com/"},
       false},
      {"script-src 'self'; report-uri https://example.com "
       "https://example2.com",
       ContentSecurityPolicySource::kHTTP,
       {"https://example.com/", "https://example2.com/"},
       false},
      {"script-src 'self'; report-uri https://example.com "
       "http://example2.com /relative/path",
       // Mixed Content report-uri endpoint is ignored.
       ContentSecurityPolicySource::kHTTP,
       {"https://example.com/", "https://example.test/relative/path"},
       false},
      {"script-src 'self'; report-uri https://example.com",
       ContentSecurityPolicySource::kMeta,
       {},
       false},
      {"script-src 'self'; report-to group",
       ContentSecurityPolicySource::kHTTP,
       {"group"},
       true},
      // report-to supersedes report-uri
      {"script-src 'self'; report-to group; report-uri https://example.com",
       ContentSecurityPolicySource::kHTTP,
       {"group"},
       true},
      {"script-src 'self'; report-to group",
       ContentSecurityPolicySource::kMeta,
       {"group"},
       true},
      {"script-src 'self'; report-to group group2",
       ContentSecurityPolicySource::kHTTP,
       // Only the first report-to endpoint is used. The other ones are ignored.
       {"group"},
       true},
      {"script-src 'self'; report-to group; report-to group2;",
       ContentSecurityPolicySource::kHTTP,
       {"group"},
       true},
      {"script-src 'self'; report-to group; report-uri https://example.com; "
       "report-to group2",
       ContentSecurityPolicySource::kHTTP,
       {"group"},
       true},
      {"script-src 'self'; report-uri https://example.com; report-to group; "
       "report-to group2",
       ContentSecurityPolicySource::kHTTP,
       {"group"},
       true},
      {"script-src 'self'; report-uri https://example.com "
       "https://example2.com; report-to group",
       ContentSecurityPolicySource::kHTTP,
       {"group"},
       true},
      {"script-src 'self'; report-uri https://example.com; report-to group; "
       "report-uri https://example.com",
       ContentSecurityPolicySource::kHTTP,
       {"group"},
       true},
  };

  for (const auto& test : cases) {
    // Test both enforce and report, there should not be a difference
    for (const auto& header_type : {ContentSecurityPolicyType::kEnforce,
                                    ContentSecurityPolicyType::kReport}) {
      network::mojom::blink::ContentSecurityPolicyPtr directive_list =
          CreateList(test.policy, header_type, test.header_source);

      EXPECT_EQ(directive_list->use_reporting_api,
                test.expected_use_reporting_api);
      EXPECT_EQ(directive_list->report_endpoints.size(),
                test.expected_endpoints.size());

      for (const String& endpoint : test.expected_endpoints) {
        EXPECT_TRUE(directive_list->report_endpoints.Contains(endpoint));
      }
      for (const String& endpoint : directive_list->report_endpoints) {
        EXPECT_TRUE(test.expected_endpoints.Contains(endpoint));
      }
    }
  }
}

TEST_F(CSPDirectiveListTest, ReasonableObjectRestriction) {
  struct TestCase {
    const char* list;
    bool expected;
  } cases[] = {// Insufficient restriction!
               {"img-src *", false},
               {"object-src *", false},
               {"object-src https://very.safe.test/", false},
               {"object-src https:", false},
               {"script-src *", false},
               {"script-src https://very.safe.test/", false},
               {"script-src https:", false},
               {"script-src 'none'; object-src *", false},
               {"script-src 'none'; object-src https://very.safe.test/", false},
               {"script-src 'none'; object-src https:", false},

               // Sufficient restrictions!
               {"default-src 'none'", true},
               {"object-src 'none'", true},
               {"object-src 'none'; script-src 'unsafe-inline'", true},
               {"object-src 'none'; script-src *", true}};

  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message() << "List: `" << test.list << "`");
    network::mojom::blink::ContentSecurityPolicyPtr directive_list =
        CreateList(test.list, ContentSecurityPolicyType::kReport);
    EXPECT_EQ(test.expected,
              CSPDirectiveListIsObjectRestrictionReasonable(*directive_list));
    directive_list = CreateList(test.list, ContentSecurityPolicyType::kEnforce);
    EXPECT_EQ(test.expected,
              CSPDirectiveListIsObjectRestrictionReasonable(*directive_list));
  }
}

TEST_F(CSPDirectiveListTest, ReasonableBaseRestriction) {
  struct TestCase {
    const char* list;
    bool expected;
  } cases[] = {// Insufficient restriction!
               {"default-src 'none'", false},
               {"base-uri https://very.safe.test/", false},
               {"base-uri *", false},
               {"base-uri https:", false},

               // Sufficient restrictions!
               {"base-uri 'none'", true},
               {"base-uri 'self'", true}};

  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message() << "List: `" << test.list << "`");
    network::mojom::blink::ContentSecurityPolicyPtr directive_list =
        CreateList(test.list, ContentSecurityPolicyType::kReport);
    EXPECT_EQ(test.expected,
              CSPDirectiveListIsBaseRestrictionReasonable(*directive_list));
    directive_list = CreateList(test.list, ContentSecurityPolicyType::kEnforce);
    EXPECT_EQ(test.expected,
              CSPDirectiveListIsBaseRestrictionReasonable(*directive_list));
  }
}

TEST_F(CSPDirectiveListTest, ReasonableScriptRestriction) {
  struct TestCase {
    const char* list;
    bool expected;
  } cases[] = {
      // Insufficient restriction!
      {"img-src *", false},
      {"script-src *", false},
      {"script-src https://very.safe.test/", false},
      {"script-src https:", false},
      {"default-src 'none'; script-src *", false},
      {"default-src 'none'; script-src https://very.safe.test/", false},
      {"default-src 'none'; script-src https:", false},

      // Sufficient restrictions!
      {"default-src 'none'", true},
      {"script-src 'none'", true},
      {"script-src 'nonce-abc'", true},
      {"script-src 'sha256-abc'", true},
      {"script-src 'nonce-abc' 'unsafe-inline'", true},
      {"script-src 'sha256-abc' 'unsafe-inline'", true},
      {"script-src 'nonce-abc' 'strict-dynamic'", true},
      {"script-src 'sha256-abc' 'strict-dynamic'", true},
      {"script-src 'nonce-abc' 'unsafe-inline' 'strict-dynamic'", true},
      {"script-src 'sha256-abc' 'unsafe-inline' 'strict-dynamic'", true},
      {"script-src 'nonce-abc' 'unsafe-inline' 'unsafe-eval' 'unsafe-hashes'",
       true},
      {"script-src 'sha256-abc' 'unsafe-inline' 'unsafe-eval' 'unsafe-hashes'",
       true},
      {"script-src 'nonce-abc' 'strict-dynamic' 'unsafe-eval' 'unsafe-hashes'",
       true},
      {"script-src 'sha256-abc' 'strict-dynamic' 'unsafe-eval' 'unsafe-hashes'",
       true},
      {"script-src 'nonce-abc' 'unsafe-inline' 'strict-dynamic' 'unsafe-eval' "
       "'unsafe-hashes'",
       true},
      {"script-src 'sha256-abc' 'unsafe-inline' 'strict-dynamic' 'unsafe-eval' "
       "'unsafe-hashes'",
       true}};

  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message() << "List: `" << test.list << "`");
    network::mojom::blink::ContentSecurityPolicyPtr directive_list =
        CreateList(test.list, ContentSecurityPolicyType::kReport);
    EXPECT_EQ(test.expected,
              CSPDirectiveListIsScriptRestrictionReasonable(*directive_list));
    directive_list = CreateList(test.list, ContentSecurityPolicyType::kEnforce);
    EXPECT_EQ(test.expected,
              CSPDirectiveListIsScriptRestrictionReasonable(*directive_list));
  }
}

// Tests that report-uri directives are discarded from policies
// delivered in <meta> elements.
TEST_F(CSPDirectiveListTest, ReportURIInMeta) {
  String policy = "img-src 'none'; report-uri https://foo.test";
  network::mojom::blink::ContentSecurityPolicyPtr directive_list =
      CreateList(policy, ContentSecurityPolicyType::kEnforce,
                 ContentSecurityPolicySource::kMeta);
  EXPECT_TRUE(directive_list->report_endpoints.empty());
  directive_list = CreateList(policy, ContentSecurityPolicyType::kEnforce,
                              ContentSecurityPolicySource::kHTTP);
  EXPECT_FALSE(directive_list->report_endpoints.empty());
}

MATCHER_P(HasSubstr, s, "") {
  return arg.Contains(s);
}

TEST_F(CSPDirectiveListTest, StrictDynamicIgnoresAllowlistWarning) {
  KURL blocked_url = KURL("https://blocked.com");
  KURL other_blocked_url = KURL("https://other-blocked.com");
  network::mojom::blink::ContentSecurityPolicyPtr directive_list_with_blocked =
      CreateList("script-src 'nonce-abc' https://blocked.com 'strict-dynamic'",
                 ContentSecurityPolicyType::kEnforce);
  network::mojom::blink::ContentSecurityPolicyPtr
      directive_list_without_blocked =
          CreateList("script-src 'nonce-abc' 'strict-dynamic'",
                     ContentSecurityPolicyType::kEnforce);

  struct {
    const char* name;
    const network::mojom::blink::ContentSecurityPolicyPtr& directive_list;
    const KURL& script_url;
    const char* script_nonce;
    bool allowed;
    bool console_message;
  } testCases[]{
      {
          "Url in the allowlist ignored because of 'strict-dynamic'",
          directive_list_with_blocked,
          blocked_url,
          "",
          false,
          true,
      },
      {
          "Url in the allowlist ignored because of 'strict-dynamic', but "
          "script allowed by nonce",
          directive_list_with_blocked,
          blocked_url,
          "abc",
          true,
          false,
      },
      {
          "No allowlistUrl",
          directive_list_without_blocked,
          blocked_url,
          "",
          false,
          false,
      },
      {
          "Url in the allowlist ignored because of 'strict-dynamic', but "
          "script has another url",
          directive_list_with_blocked,
          other_blocked_url,
          "",
          false,
          false,
      },
  };
  for (const auto& testCase : testCases) {
    SCOPED_TRACE(testCase.name);
    ContentSecurityPolicy* context =
        MakeGarbageCollected<ContentSecurityPolicy>();
    TestCSPDelegate* test_delegate = MakeGarbageCollected<TestCSPDelegate>();
    context->BindToDelegate(*test_delegate);
    for (auto reporting_disposition : {ReportingDisposition::kSuppressReporting,
                                       ReportingDisposition::kReport}) {
      EXPECT_EQ(
          CSPCheckResult(testCase.allowed),
          CSPDirectiveListAllowFromSource(
              *testCase.directive_list, context,
              CSPDirectiveName::ScriptSrcElem, testCase.script_url,
              testCase.script_url, ResourceRequest::RedirectStatus::kNoRedirect,
              reporting_disposition, testCase.script_nonce));
    }
    static const char* message =
        "Note that 'strict-dynamic' is present, so "
        "host-based allowlisting is disabled.";
    if (testCase.console_message) {
      EXPECT_THAT(test_delegate->console_messages(),
                  testing::Contains(HasSubstr(message)));
    } else {
      EXPECT_THAT(test_delegate->console_messages(),
                  testing::Not(testing::Contains(HasSubstr(message))));
    }
  }
}

}  // namespace blink
