// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/csp/csp_directive_list.h"

#include <list>
#include <string>

#include "base/test/scoped_feature_list.h"
#include "services/network/public/cpp/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/csp/source_list_directive.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/subresource_integrity.h"
#include "third_party/blink/renderer/platform/network/content_security_policy_parsers.h"
#include "third_party/blink/renderer/platform/wtf/text/string_operators.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

using network::mojom::ContentSecurityPolicySource;
using network::mojom::ContentSecurityPolicyType;

class CSPDirectiveListTest : public testing::Test {
 public:
  CSPDirectiveListTest() : csp(MakeGarbageCollected<ContentSecurityPolicy>()) {}
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures({network::features::kReporting}, {});
    csp->SetupSelf(
        *SecurityOrigin::CreateFromString("https://example.test/image.png"));
  }

  CSPDirectiveList* CreateList(
      const String& list,
      ContentSecurityPolicyType type,
      ContentSecurityPolicySource source = ContentSecurityPolicySource::kHTTP) {
    Vector<UChar> characters;
    list.AppendTo(characters);
    const UChar* begin = characters.data();
    const UChar* end = begin + characters.size();

    return CSPDirectiveList::Create(csp, begin, end, type, source);
  }

 protected:
  Persistent<ContentSecurityPolicy> csp;
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
    Member<CSPDirectiveList> directive_list =
        CreateList(test.list, ContentSecurityPolicyType::kReport);
    EXPECT_EQ(test.expected, directive_list->Header());
    directive_list = CreateList(test.list, ContentSecurityPolicyType::kEnforce);
    EXPECT_EQ(test.expected, directive_list->Header());
  }
}

TEST_F(CSPDirectiveListTest, IsMatchingNoncePresent) {
  struct TestCase {
    ContentSecurityPolicy::DirectiveType type;
    const char* list;
    const char* nonce;
    bool expected;
  } cases[] = {
      {ContentSecurityPolicy::DirectiveType::kScriptSrc, "script-src 'self'",
       "yay", false},
      {ContentSecurityPolicy::DirectiveType::kScriptSrc, "script-src 'self'",
       "boo", false},
      {ContentSecurityPolicy::DirectiveType::kScriptSrc,
       "script-src 'nonce-yay'", "yay", true},
      {ContentSecurityPolicy::DirectiveType::kScriptSrc,
       "script-src 'nonce-yay'", "boo", false},
      {ContentSecurityPolicy::DirectiveType::kScriptSrc,
       "script-src 'nonce-yay' 'nonce-boo'", "yay", true},
      {ContentSecurityPolicy::DirectiveType::kScriptSrc,
       "script-src 'nonce-yay' 'nonce-boo'", "boo", true},

      // Falls back to 'default-src'
      {ContentSecurityPolicy::DirectiveType::kScriptSrc,
       "default-src 'nonce-yay'", "yay", true},
      {ContentSecurityPolicy::DirectiveType::kScriptSrc,
       "default-src 'nonce-yay'", "boo", false},
      {ContentSecurityPolicy::DirectiveType::kScriptSrc,
       "default-src 'nonce-boo'; script-src 'nonce-yay'", "yay", true},
      {ContentSecurityPolicy::DirectiveType::kScriptSrc,
       "default-src 'nonce-boo'; script-src 'nonce-yay'", "boo", false},

      // Unrelated directives do not affect result
      {ContentSecurityPolicy::DirectiveType::kScriptSrc,
       "style-src 'nonce-yay'", "yay", false},
      {ContentSecurityPolicy::DirectiveType::kScriptSrc,
       "style-src 'nonce-yay'", "boo", false},

      // Script-src-elem/attr falls back on script-src and then default-src.
      {ContentSecurityPolicy::DirectiveType::kScriptSrcElem,
       "script-src 'nonce-yay'", "yay", true},
      {ContentSecurityPolicy::DirectiveType::kScriptSrcElem,
       "script-src 'nonce-yay'; default-src 'nonce-boo'", "yay", true},
      {ContentSecurityPolicy::DirectiveType::kScriptSrcElem,
       "script-src 'nonce-boo'; default-src 'nonce-yay'", "yay", false},
      {ContentSecurityPolicy::DirectiveType::kScriptSrcElem,
       "script-src-elem 'nonce-yay'; script-src 'nonce-boo'; default-src "
       "'nonce-boo'",
       "yay", true},
      {ContentSecurityPolicy::DirectiveType::kScriptSrcElem,
       "default-src 'nonce-yay'", "yay", true},

      {ContentSecurityPolicy::DirectiveType::kScriptSrcAttr,
       "script-src 'nonce-yay'", "yay", true},
      {ContentSecurityPolicy::DirectiveType::kScriptSrcAttr,
       "script-src 'nonce-yay'; default-src 'nonce-boo'", "yay", true},
      {ContentSecurityPolicy::DirectiveType::kScriptSrcAttr,
       "script-src 'nonce-boo'; default-src 'nonce-yay'", "yay", false},
      {ContentSecurityPolicy::DirectiveType::kScriptSrcAttr,
       "script-src-attr 'nonce-yay'; script-src 'nonce-boo'; default-src "
       "'nonce-boo'",
       "yay", true},
      {ContentSecurityPolicy::DirectiveType::kScriptSrcAttr,
       "default-src 'nonce-yay'", "yay", true},
  };

  for (const auto& test : cases) {
    // Report-only
    Member<CSPDirectiveList> directive_list =
        CreateList(test.list, ContentSecurityPolicyType::kReport);
    Member<SourceListDirective> directive =
        directive_list->OperativeDirective(test.type);
    EXPECT_EQ(test.expected,
              directive_list->IsMatchingNoncePresent(directive, test.nonce));
    // Empty/null strings are always not present, regardless of the policy.
    EXPECT_FALSE(directive_list->IsMatchingNoncePresent(directive, ""));
    EXPECT_FALSE(directive_list->IsMatchingNoncePresent(directive, String()));

    // Enforce
    directive_list = CreateList(test.list, ContentSecurityPolicyType::kEnforce);
    directive = directive_list->OperativeDirective(test.type);
    EXPECT_EQ(test.expected,
              directive_list->IsMatchingNoncePresent(directive, test.nonce));
    // Empty/null strings are always not present, regardless of the policy.
    EXPECT_FALSE(directive_list->IsMatchingNoncePresent(directive, ""));
    EXPECT_FALSE(directive_list->IsMatchingNoncePresent(directive, String()));
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

  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message()
                 << "List: `" << test.list << "`, URL: `" << test.url << "`");
    const KURL script_src(test.url);

    // Report-only
    Member<CSPDirectiveList> directive_list =
        CreateList(test.list, ContentSecurityPolicyType::kReport);
    EXPECT_EQ(
        test.expected,
        directive_list->AllowFromSource(
            ContentSecurityPolicy::DirectiveType::kScriptSrcElem, script_src,
            script_src, ResourceRequest::RedirectStatus::kNoRedirect,
            ReportingDisposition::kSuppressReporting, String(),
            IntegrityMetadataSet(), kParserInserted));

    // Enforce
    directive_list = CreateList(test.list, ContentSecurityPolicyType::kEnforce);
    EXPECT_EQ(
        test.expected,
        directive_list->AllowFromSource(
            ContentSecurityPolicy::DirectiveType::kScriptSrcElem, script_src,
            script_src, ResourceRequest::RedirectStatus::kNoRedirect,
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

  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message()
                 << "List: `" << test.list << "`, URL: `" << test.url << "`");
    const KURL resource(test.url);

    // Report-only 'script-src'
    Member<CSPDirectiveList> directive_list = CreateList(
        String("script-src ") + test.list, ContentSecurityPolicyType::kReport);
    EXPECT_EQ(
        test.expected,
        directive_list->AllowFromSource(
            ContentSecurityPolicy::DirectiveType::kScriptSrcElem, resource,
            resource, ResourceRequest::RedirectStatus::kNoRedirect,
            ReportingDisposition::kSuppressReporting, String(test.nonce),
            IntegrityMetadataSet(), kParserInserted));

    // Enforce 'script-src'
    directive_list = CreateList(String("script-src ") + test.list,
                                ContentSecurityPolicyType::kEnforce);
    EXPECT_EQ(
        test.expected,
        directive_list->AllowFromSource(
            ContentSecurityPolicy::DirectiveType::kScriptSrcElem, resource,
            resource, ResourceRequest::RedirectStatus::kNoRedirect,
            ReportingDisposition::kSuppressReporting, String(test.nonce),
            IntegrityMetadataSet(), kParserInserted));

    // Report-only 'style-src'
    directive_list = CreateList(String("style-src ") + test.list,
                                ContentSecurityPolicyType::kReport);
    EXPECT_EQ(
        test.expected,
        directive_list->AllowFromSource(
            ContentSecurityPolicy::DirectiveType::kStyleSrcElem, resource,
            resource, ResourceRequest::RedirectStatus::kNoRedirect,
            ReportingDisposition::kSuppressReporting, String(test.nonce)));

    // Enforce 'style-src'
    directive_list = CreateList(String("style-src ") + test.list,
                                ContentSecurityPolicyType::kEnforce);
    EXPECT_EQ(
        test.expected,
        directive_list->AllowFromSource(
            ContentSecurityPolicy::DirectiveType::kStyleSrcElem, resource,
            resource, ResourceRequest::RedirectStatus::kNoRedirect,
            ReportingDisposition::kSuppressReporting, String(test.nonce)));

    // Report-only 'style-src'
    directive_list = CreateList(String("default-src ") + test.list,
                                ContentSecurityPolicyType::kReport);
    EXPECT_EQ(
        test.expected,
        directive_list->AllowFromSource(
            ContentSecurityPolicy::DirectiveType::kScriptSrcElem, resource,
            resource, ResourceRequest::RedirectStatus::kNoRedirect,
            ReportingDisposition::kSuppressReporting, String(test.nonce)));
    EXPECT_EQ(
        test.expected,
        directive_list->AllowFromSource(
            ContentSecurityPolicy::DirectiveType::kStyleSrcElem, resource,
            resource, ResourceRequest::RedirectStatus::kNoRedirect,
            ReportingDisposition::kSuppressReporting, String(test.nonce)));

    // Enforce 'style-src'
    directive_list = CreateList(String("default-src ") + test.list,
                                ContentSecurityPolicyType::kEnforce);
    EXPECT_EQ(
        test.expected,
        directive_list->AllowFromSource(
            ContentSecurityPolicy::DirectiveType::kScriptSrcElem, resource,
            resource, ResourceRequest::RedirectStatus::kNoRedirect,
            ReportingDisposition::kSuppressReporting, String(test.nonce),
            IntegrityMetadataSet(), kParserInserted));
    EXPECT_EQ(
        test.expected,
        directive_list->AllowFromSource(
            ContentSecurityPolicy::DirectiveType::kStyleSrcElem, resource,
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

  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message()
                 << "List: `" << test.list << "`, URL: `" << test.url
                 << "`, Integrity: `" << test.integrity << "`");
    const KURL resource(test.url);

    IntegrityMetadataSet integrity_metadata;
    EXPECT_EQ(
        SubresourceIntegrity::kIntegrityParseValidResult,
        SubresourceIntegrity::ParseIntegrityAttribute(
            test.integrity, SubresourceIntegrity::IntegrityFeatures::kDefault,
            integrity_metadata));

    // Report-only 'script-src'
    Member<CSPDirectiveList> directive_list = CreateList(
        String("script-src ") + test.list, ContentSecurityPolicyType::kReport);
    EXPECT_EQ(
        test.expected,
        directive_list->AllowFromSource(
            ContentSecurityPolicy::DirectiveType::kScriptSrcElem, resource,
            resource, ResourceRequest::RedirectStatus::kNoRedirect,
            ReportingDisposition::kSuppressReporting, String(),
            integrity_metadata, kParserInserted));

    // Enforce 'script-src'
    directive_list = CreateList(String("script-src ") + test.list,
                                ContentSecurityPolicyType::kEnforce);
    EXPECT_EQ(
        test.expected,
        directive_list->AllowFromSource(
            ContentSecurityPolicy::DirectiveType::kScriptSrcElem, resource,
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

  for (const auto& test : cases) {
    SCOPED_TRACE(test.list);
    const KURL resource("https://example.test/worker.js");
    Member<CSPDirectiveList> directive_list =
        CreateList(test.list, ContentSecurityPolicyType::kEnforce);
    EXPECT_EQ(test.allowed,
              directive_list->AllowFromSource(
                  ContentSecurityPolicy::DirectiveType::kWorkerSrc, resource,
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

  for (const auto& test : cases) {
    SCOPED_TRACE(test.list);
    const KURL resource("https://example.test/worker.js");
    Member<CSPDirectiveList> directive_list =
        CreateList(test.list, ContentSecurityPolicyType::kEnforce);
    EXPECT_EQ(test.allowed,
              directive_list->AllowFromSource(
                  ContentSecurityPolicy::DirectiveType::kWorkerSrc, resource,
                  resource, ResourceRequest::RedirectStatus::kNoRedirect,
                  ReportingDisposition::kSuppressReporting));
  }
}

TEST_F(CSPDirectiveListTest, OperativeDirectiveGivenType) {
  struct TestCase {
    ContentSecurityPolicy::DirectiveType directive;
    Vector<ContentSecurityPolicy::DirectiveType> fallback_list;
  } cases[] = {
      // Directives with default directive.
      {ContentSecurityPolicy::DirectiveType::kChildSrc,
       {ContentSecurityPolicy::DirectiveType::kDefaultSrc}},
      {ContentSecurityPolicy::DirectiveType::kConnectSrc,
       {ContentSecurityPolicy::DirectiveType::kDefaultSrc}},
      {ContentSecurityPolicy::DirectiveType::kFontSrc,
       {ContentSecurityPolicy::DirectiveType::kDefaultSrc}},
      {ContentSecurityPolicy::DirectiveType::kImgSrc,
       {ContentSecurityPolicy::DirectiveType::kDefaultSrc}},
      {ContentSecurityPolicy::DirectiveType::kManifestSrc,
       {ContentSecurityPolicy::DirectiveType::kDefaultSrc}},
      {ContentSecurityPolicy::DirectiveType::kMediaSrc,
       {ContentSecurityPolicy::DirectiveType::kDefaultSrc}},
      {ContentSecurityPolicy::DirectiveType::kObjectSrc,
       {ContentSecurityPolicy::DirectiveType::kDefaultSrc}},
      {ContentSecurityPolicy::DirectiveType::kScriptSrc,
       {ContentSecurityPolicy::DirectiveType::kDefaultSrc}},
      {ContentSecurityPolicy::DirectiveType::kStyleSrc,
       {ContentSecurityPolicy::DirectiveType::kDefaultSrc}},
      // Directives with no default directive.
      {ContentSecurityPolicy::DirectiveType::kBaseURI, {}},
      {ContentSecurityPolicy::DirectiveType::kDefaultSrc, {}},
      {ContentSecurityPolicy::DirectiveType::kFrameAncestors, {}},
      {ContentSecurityPolicy::DirectiveType::kFormAction, {}},
      // Directive with multiple default directives.
      {ContentSecurityPolicy::DirectiveType::kScriptSrcAttr,
       {ContentSecurityPolicy::DirectiveType::kScriptSrc,
        ContentSecurityPolicy::DirectiveType::kDefaultSrc}},
      {ContentSecurityPolicy::DirectiveType::kScriptSrcElem,
       {ContentSecurityPolicy::DirectiveType::kScriptSrc,
        ContentSecurityPolicy::DirectiveType::kDefaultSrc}},
      {ContentSecurityPolicy::DirectiveType::kFrameSrc,
       {ContentSecurityPolicy::DirectiveType::kChildSrc,
        ContentSecurityPolicy::DirectiveType::kDefaultSrc}},
      {ContentSecurityPolicy::DirectiveType::kWorkerSrc,
       {ContentSecurityPolicy::DirectiveType::kChildSrc,
        ContentSecurityPolicy::DirectiveType::kScriptSrc,
        ContentSecurityPolicy::DirectiveType::kDefaultSrc}},
  };

  std::stringstream all_directives;
  for (const auto& test : cases) {
    const char* name = ContentSecurityPolicy::GetDirectiveName(test.directive);
    all_directives << name << " http://" << name << ".com; ";
  }

  CSPDirectiveList* empty = CreateList("", ContentSecurityPolicyType::kEnforce);

  std::string directive_string;
  CSPDirectiveList* directive_list;
  // Initial set-up.
  for (auto& test : cases) {
    // With an empty directive list the returned directive should always be
    // null.
    EXPECT_FALSE(empty->OperativeDirective(test.directive));

    // Add the directive itself as it should be the first one to be returned.
    test.fallback_list.push_front(test.directive);

    // Start the tests with all directives present.
    directive_string = all_directives.str();

    while (!test.fallback_list.IsEmpty()) {
      directive_list = CreateList(directive_string.c_str(),
                                  ContentSecurityPolicyType::kEnforce);

      CSPDirective* operative_directive =
          directive_list->OperativeDirective(test.directive);

      // We should have an actual directive returned here.
      EXPECT_TRUE(operative_directive);

      // The OperativeDirective should be first one in the fallback chain.
      EXPECT_EQ(test.fallback_list.front(),
                ContentSecurityPolicy::GetDirectiveType(
                    operative_directive->GetName()));

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
    EXPECT_FALSE(directive_list->OperativeDirective(test.directive));
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
       {"https://example.com"},
       false},
      {"script-src 'self'; report-uri https://example.com "
       "https://example2.com",
       ContentSecurityPolicySource::kHTTP,
       {"https://example.com", "https://example2.com"},
       false},
      {"script-src 'self'; report-uri https://example.com "
       "http://example2.com /relative/path",
       // Mixed Content report-uri endpoint is ignored.
       ContentSecurityPolicySource::kHTTP,
       {"https://example.com", "/relative/path"},
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
      Member<CSPDirectiveList> directive_list =
          CreateList(test.policy, header_type, test.header_source);

      EXPECT_EQ(directive_list->UseReportingApi(),
                test.expected_use_reporting_api);
      EXPECT_EQ(directive_list->ReportEndpoints().size(),
                test.expected_endpoints.size());

      for (const String& endpoint : test.expected_endpoints) {
        EXPECT_TRUE(directive_list->ReportEndpoints().Contains(endpoint));
      }
      for (const String& endpoint : directive_list->ReportEndpoints()) {
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
    Member<CSPDirectiveList> directive_list =
        CreateList(test.list, ContentSecurityPolicyType::kReport);
    EXPECT_EQ(test.expected, directive_list->IsObjectRestrictionReasonable());
    directive_list = CreateList(test.list, ContentSecurityPolicyType::kEnforce);
    EXPECT_EQ(test.expected, directive_list->IsObjectRestrictionReasonable());
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
    Member<CSPDirectiveList> directive_list =
        CreateList(test.list, ContentSecurityPolicyType::kReport);
    EXPECT_EQ(test.expected, directive_list->IsBaseRestrictionReasonable());
    directive_list = CreateList(test.list, ContentSecurityPolicyType::kEnforce);
    EXPECT_EQ(test.expected, directive_list->IsBaseRestrictionReasonable());
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
    Member<CSPDirectiveList> directive_list =
        CreateList(test.list, ContentSecurityPolicyType::kReport);
    EXPECT_EQ(test.expected, directive_list->IsScriptRestrictionReasonable());
    directive_list = CreateList(test.list, ContentSecurityPolicyType::kEnforce);
    EXPECT_EQ(test.expected, directive_list->IsScriptRestrictionReasonable());
  }
}

}  // namespace blink
