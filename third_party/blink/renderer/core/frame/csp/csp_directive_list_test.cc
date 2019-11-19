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

class CSPDirectiveListTest : public testing::Test {
 public:
  CSPDirectiveListTest() : csp(MakeGarbageCollected<ContentSecurityPolicy>()) {}
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures({network::features::kReporting}, {});
    csp->SetupSelf(
        *SecurityOrigin::CreateFromString("https://example.test/image.png"));
  }

  CSPDirectiveList* CreateList(const String& list,
                               ContentSecurityPolicyHeaderType type,
                               ContentSecurityPolicyHeaderSource source =
                                   kContentSecurityPolicyHeaderSourceHTTP) {
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
        CreateList(test.list, kContentSecurityPolicyHeaderTypeReport);
    EXPECT_EQ(test.expected, directive_list->Header());
    directive_list =
        CreateList(test.list, kContentSecurityPolicyHeaderTypeEnforce);
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
        CreateList(test.list, kContentSecurityPolicyHeaderTypeReport);
    Member<SourceListDirective> directive =
        directive_list->OperativeDirective(test.type);
    EXPECT_EQ(test.expected,
              directive_list->IsMatchingNoncePresent(directive, test.nonce));
    // Empty/null strings are always not present, regardless of the policy.
    EXPECT_FALSE(directive_list->IsMatchingNoncePresent(directive, ""));
    EXPECT_FALSE(directive_list->IsMatchingNoncePresent(directive, String()));

    // Enforce
    directive_list =
        CreateList(test.list, kContentSecurityPolicyHeaderTypeEnforce);
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
        CreateList(test.list, kContentSecurityPolicyHeaderTypeReport);
    EXPECT_EQ(test.expected,
              directive_list->AllowFromSource(
                  ContentSecurityPolicy::DirectiveType::kScriptSrcElem,
                  script_src, ResourceRequest::RedirectStatus::kNoRedirect,
                  SecurityViolationReportingPolicy::kSuppressReporting,
                  String(), IntegrityMetadataSet(), kParserInserted));

    // Enforce
    directive_list =
        CreateList(test.list, kContentSecurityPolicyHeaderTypeEnforce);
    EXPECT_EQ(test.expected,
              directive_list->AllowFromSource(
                  ContentSecurityPolicy::DirectiveType::kScriptSrcElem,
                  script_src, ResourceRequest::RedirectStatus::kNoRedirect,
                  SecurityViolationReportingPolicy::kSuppressReporting,
                  String(), IntegrityMetadataSet(), kParserInserted));
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

      // Doesn't affect URLs that match the whitelist.
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
    Member<CSPDirectiveList> directive_list =
        CreateList(String("script-src ") + test.list,
                   kContentSecurityPolicyHeaderTypeReport);
    EXPECT_EQ(test.expected,
              directive_list->AllowFromSource(
                  ContentSecurityPolicy::DirectiveType::kScriptSrcElem,
                  resource, ResourceRequest::RedirectStatus::kNoRedirect,
                  SecurityViolationReportingPolicy::kSuppressReporting,
                  String(test.nonce), IntegrityMetadataSet(), kParserInserted));

    // Enforce 'script-src'
    directive_list = CreateList(String("script-src ") + test.list,
                                kContentSecurityPolicyHeaderTypeEnforce);
    EXPECT_EQ(test.expected,
              directive_list->AllowFromSource(
                  ContentSecurityPolicy::DirectiveType::kScriptSrcElem,
                  resource, ResourceRequest::RedirectStatus::kNoRedirect,
                  SecurityViolationReportingPolicy::kSuppressReporting,
                  String(test.nonce), IntegrityMetadataSet(), kParserInserted));

    // Report-only 'style-src'
    directive_list = CreateList(String("style-src ") + test.list,
                                kContentSecurityPolicyHeaderTypeReport);
    EXPECT_EQ(test.expected,
              directive_list->AllowFromSource(
                  ContentSecurityPolicy::DirectiveType::kStyleSrcElem, resource,
                  ResourceRequest::RedirectStatus::kNoRedirect,
                  SecurityViolationReportingPolicy::kSuppressReporting,
                  String(test.nonce)));

    // Enforce 'style-src'
    directive_list = CreateList(String("style-src ") + test.list,
                                kContentSecurityPolicyHeaderTypeEnforce);
    EXPECT_EQ(test.expected,
              directive_list->AllowFromSource(
                  ContentSecurityPolicy::DirectiveType::kStyleSrcElem, resource,
                  ResourceRequest::RedirectStatus::kNoRedirect,
                  SecurityViolationReportingPolicy::kSuppressReporting,
                  String(test.nonce)));

    // Report-only 'style-src'
    directive_list = CreateList(String("default-src ") + test.list,
                                kContentSecurityPolicyHeaderTypeReport);
    EXPECT_EQ(test.expected,
              directive_list->AllowFromSource(
                  ContentSecurityPolicy::DirectiveType::kScriptSrcElem,
                  resource, ResourceRequest::RedirectStatus::kNoRedirect,
                  SecurityViolationReportingPolicy::kSuppressReporting,
                  String(test.nonce)));
    EXPECT_EQ(test.expected,
              directive_list->AllowFromSource(
                  ContentSecurityPolicy::DirectiveType::kStyleSrcElem, resource,
                  ResourceRequest::RedirectStatus::kNoRedirect,
                  SecurityViolationReportingPolicy::kSuppressReporting,
                  String(test.nonce)));

    // Enforce 'style-src'
    directive_list = CreateList(String("default-src ") + test.list,
                                kContentSecurityPolicyHeaderTypeEnforce);
    EXPECT_EQ(test.expected,
              directive_list->AllowFromSource(
                  ContentSecurityPolicy::DirectiveType::kScriptSrcElem,
                  resource, ResourceRequest::RedirectStatus::kNoRedirect,
                  SecurityViolationReportingPolicy::kSuppressReporting,
                  String(test.nonce), IntegrityMetadataSet(), kParserInserted));
    EXPECT_EQ(test.expected,
              directive_list->AllowFromSource(
                  ContentSecurityPolicy::DirectiveType::kStyleSrcElem, resource,
                  ResourceRequest::RedirectStatus::kNoRedirect,
                  SecurityViolationReportingPolicy::kSuppressReporting,
                  String(test.nonce)));
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

      // Doesn't affect URLs that match the whitelist.
      {"https://example.com 'sha256-yay'", "https://example.com/file",
       "sha256-yay", true},
      {"https://example.com 'sha256-yay'", "https://example.com/file",
       "sha256-boo", true},
      {"https://example.com 'sha256-yay'", "https://example.com/file", "",
       true},

      // Does affect URLs that don't match the whitelist.
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

      // Additional whitelisted hashes in the CSP don't interfere.
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
    Member<CSPDirectiveList> directive_list =
        CreateList(String("script-src ") + test.list,
                   kContentSecurityPolicyHeaderTypeReport);
    EXPECT_EQ(test.expected,
              directive_list->AllowFromSource(
                  ContentSecurityPolicy::DirectiveType::kScriptSrcElem,
                  resource, ResourceRequest::RedirectStatus::kNoRedirect,
                  SecurityViolationReportingPolicy::kSuppressReporting,
                  String(), integrity_metadata, kParserInserted));

    // Enforce 'script-src'
    directive_list = CreateList(String("script-src ") + test.list,
                                kContentSecurityPolicyHeaderTypeEnforce);
    EXPECT_EQ(test.expected,
              directive_list->AllowFromSource(
                  ContentSecurityPolicy::DirectiveType::kScriptSrcElem,
                  resource, ResourceRequest::RedirectStatus::kNoRedirect,
                  SecurityViolationReportingPolicy::kSuppressReporting,
                  String(), integrity_metadata, kParserInserted));
  }
}

TEST_F(CSPDirectiveListTest, allowRequestWithoutIntegrity) {
  struct TestCase {
    const char* list;
    const char* url;
    const mojom::RequestContextType context;
    bool expected;
  } cases[] = {
      {"require-sri-for script", "https://example.com/file",
       mojom::RequestContextType::SCRIPT, false},

      // Extra WSP
      {"require-sri-for  script     script  ", "https://example.com/file",
       mojom::RequestContextType::SCRIPT, false},
      {"require-sri-for      style    script", "https://example.com/file",
       mojom::RequestContextType::STYLE, false},

      {"require-sri-for style script", "https://example.com/file",
       mojom::RequestContextType::SCRIPT, false},
      {"require-sri-for style script", "https://example.com/file",
       mojom::RequestContextType::IMPORT, false},
      {"require-sri-for style script", "https://example.com/file",
       mojom::RequestContextType::IMAGE, true},

      {"require-sri-for script", "https://example.com/file",
       mojom::RequestContextType::AUDIO, true},
      {"require-sri-for script", "https://example.com/file",
       mojom::RequestContextType::SCRIPT, false},
      {"require-sri-for script", "https://example.com/file",
       mojom::RequestContextType::IMPORT, false},
      {"require-sri-for script", "https://example.com/file",
       mojom::RequestContextType::SERVICE_WORKER, false},
      {"require-sri-for script", "https://example.com/file",
       mojom::RequestContextType::SHARED_WORKER, false},
      {"require-sri-for script", "https://example.com/file",
       mojom::RequestContextType::WORKER, false},
      {"require-sri-for script", "https://example.com/file",
       mojom::RequestContextType::STYLE, true},

      {"require-sri-for style", "https://example.com/file",
       mojom::RequestContextType::AUDIO, true},
      {"require-sri-for style", "https://example.com/file",
       mojom::RequestContextType::SCRIPT, true},
      {"require-sri-for style", "https://example.com/file",
       mojom::RequestContextType::IMPORT, true},
      {"require-sri-for style", "https://example.com/file",
       mojom::RequestContextType::SERVICE_WORKER, true},
      {"require-sri-for style", "https://example.com/file",
       mojom::RequestContextType::SHARED_WORKER, true},
      {"require-sri-for style", "https://example.com/file",
       mojom::RequestContextType::WORKER, true},
      {"require-sri-for style", "https://example.com/file",
       mojom::RequestContextType::STYLE, false},

      // Multiple tokens
      {"require-sri-for script style", "https://example.com/file",
       mojom::RequestContextType::STYLE, false},
      {"require-sri-for script style", "https://example.com/file",
       mojom::RequestContextType::SCRIPT, false},
      {"require-sri-for script style", "https://example.com/file",
       mojom::RequestContextType::IMPORT, false},
      {"require-sri-for script style", "https://example.com/file",
       mojom::RequestContextType::IMAGE, true},

      // Matching is case-insensitive
      {"require-sri-for Script", "https://example.com/file",
       mojom::RequestContextType::SCRIPT, false},

      // Unknown tokens do not affect result
      {"require-sri-for blabla12 as", "https://example.com/file",
       mojom::RequestContextType::SCRIPT, true},
      {"require-sri-for blabla12 as  script", "https://example.com/file",
       mojom::RequestContextType::SCRIPT, false},
      {"require-sri-for script style img", "https://example.com/file",
       mojom::RequestContextType::SCRIPT, false},
      {"require-sri-for script style img", "https://example.com/file",
       mojom::RequestContextType::IMPORT, false},
      {"require-sri-for script style img", "https://example.com/file",
       mojom::RequestContextType::STYLE, false},
      {"require-sri-for script style img", "https://example.com/file",
       mojom::RequestContextType::IMAGE, true},

      // Empty token list has no effect
      {"require-sri-for      ", "https://example.com/file",
       mojom::RequestContextType::SCRIPT, true},
      {"require-sri-for      ", "https://example.com/file",
       mojom::RequestContextType::IMPORT, true},
      {"require-sri-for      ", "https://example.com/file",
       mojom::RequestContextType::STYLE, true},
      {"require-sri-for      ", "https://example.com/file",
       mojom::RequestContextType::SERVICE_WORKER, true},
      {"require-sri-for      ", "https://example.com/file",
       mojom::RequestContextType::SHARED_WORKER, true},
      {"require-sri-for      ", "https://example.com/file",
       mojom::RequestContextType::WORKER, true},

      // Order does not matter
      {"require-sri-for a b script", "https://example.com/file",
       mojom::RequestContextType::SCRIPT, false},
      {"require-sri-for a script b", "https://example.com/file",
       mojom::RequestContextType::SCRIPT, false},
  };

  for (const auto& test : cases) {
    const KURL resource(test.url);
    // Report-only
    Member<CSPDirectiveList> directive_list =
        CreateList(test.list, kContentSecurityPolicyHeaderTypeReport);
    EXPECT_EQ(true, directive_list->AllowRequestWithoutIntegrity(
                        test.context, resource,
                        ResourceRequest::RedirectStatus::kNoRedirect,
                        SecurityViolationReportingPolicy::kSuppressReporting));

    // Enforce
    directive_list =
        CreateList(test.list, kContentSecurityPolicyHeaderTypeEnforce);
    EXPECT_EQ(test.expected,
              directive_list->AllowRequestWithoutIntegrity(
                  test.context, resource,
                  ResourceRequest::RedirectStatus::kNoRedirect,
                  SecurityViolationReportingPolicy::kSuppressReporting));
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
        CreateList(test.list, kContentSecurityPolicyHeaderTypeEnforce);
    EXPECT_EQ(test.allowed,
              directive_list->AllowFromSource(
                  ContentSecurityPolicy::DirectiveType::kWorkerSrc, resource,
                  ResourceRequest::RedirectStatus::kNoRedirect,
                  SecurityViolationReportingPolicy::kSuppressReporting));
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
        CreateList(test.list, kContentSecurityPolicyHeaderTypeEnforce);
    EXPECT_EQ(test.allowed,
              directive_list->AllowFromSource(
                  ContentSecurityPolicy::DirectiveType::kWorkerSrc, resource,
                  ResourceRequest::RedirectStatus::kNoRedirect,
                  SecurityViolationReportingPolicy::kSuppressReporting));
  }
}

TEST_F(CSPDirectiveListTest, SubsumesBasedOnCSPSourcesOnly) {
  CSPDirectiveList* a = CreateList(
      "script-src http://*.one.com; img-src https://one.com "
      "http://two.com/imgs/",
      kContentSecurityPolicyHeaderTypeEnforce);

  struct TestCase {
    const Vector<const char*> policies;
    bool expected;
    bool expected_first_policy_opposite;
  } cases[] = {
      // `listB`, which is not as restrictive as `A`, is not subsumed.
      {{""}, false, true},
      {{"script-src http://example.com"}, false, false},
      {{"img-src http://example.com"}, false, false},
      {{"script-src http://*.one.com"}, false, true},
      {{"img-src https://one.com http://two.com/imgs/"}, false, true},
      {{"default-src http://example.com"}, false, false},
      {{"default-src https://one.com http://two.com/imgs/"}, false, false},
      {{"default-src http://one.com"}, false, false},
      {{"script-src http://*.one.com; img-src http://two.com/"}, false, false},
      {{"script-src http://*.one.com", "img-src http://one.com"}, false, true},
      {{"script-src http://*.one.com", "script-src https://two.com"},
       false,
       true},
      {{"script-src http://*.random.com", "script-src https://random.com"},
       false,
       false},
      {{"script-src http://one.com", "script-src https://random.com"},
       false,
       false},
      {{"script-src http://*.random.com; default-src http://one.com "
        "http://two.com/imgs/",
        "default-src https://random.com"},
       false,
       false},
      // `listB`, which is as restrictive as `A`, is subsumed.
      {{"default-src https://one.com"}, true, false},
      {{"default-src http://random.com",
        "default-src https://non-random.com:*"},
       true,
       false},
      {{"script-src http://*.one.com; img-src https://one.com"}, true, false},
      {{"script-src http://*.one.com; img-src https://one.com "
        "http://two.com/imgs/"},
       true,
       true},
      {{"script-src http://*.one.com",
        "img-src https://one.com http://two.com/imgs/"},
       true,
       true},
      {{"script-src http://*.random.com; default-src https://one.com "
        "http://two.com/imgs/",
        "default-src https://else.com"},
       true,
       false},
      {{"script-src http://*.random.com; default-src https://one.com "
        "http://two.com/imgs/",
        "default-src https://one.com"},
       true,
       false},
  };

  CSPDirectiveList* empty_a =
      CreateList("", kContentSecurityPolicyHeaderTypeEnforce);

  for (const auto& test : cases) {
    HeapVector<Member<CSPDirectiveList>> list_b;
    for (auto* const policy : test.policies) {
      list_b.push_back(
          CreateList(policy, kContentSecurityPolicyHeaderTypeEnforce));
    }

    EXPECT_EQ(test.expected, a->Subsumes(list_b));
    // Empty CSPDirective subsumes any list.
    EXPECT_TRUE(empty_a->Subsumes(list_b));
    // Check if first policy of `listB` subsumes `A`.
    EXPECT_EQ(test.expected_first_policy_opposite,
              list_b[0]->Subsumes(HeapVector<Member<CSPDirectiveList>>(1, a)));
  }
}

TEST_F(CSPDirectiveListTest, SubsumesIfNoneIsPresent) {
  struct TestCase {
    const char* policy_a;
    const Vector<const char*> policies_b;
    bool expected;
  } cases[] = {
      // `policyA` subsumes any vector of policies.
      {"", {""}, true},
      {"", {"script-src http://example.com"}, true},
      {"", {"script-src 'none'"}, true},
      {"", {"script-src http://*.one.com", "script-src https://two.com"}, true},
      // `policyA` is 'none', but no policy in `policiesB` is.
      {"script-src ", {""}, false},
      {"script-src 'none'", {""}, false},
      {"script-src ", {"script-src http://example.com"}, false},
      {"script-src 'none'", {"script-src http://example.com"}, false},
      {"script-src ", {"img-src 'none'"}, false},
      {"script-src 'none'", {"img-src 'none'"}, false},
      {"script-src ",
       {"script-src http://*.one.com", "img-src https://two.com"},
       false},
      {"script-src 'none'",
       {"script-src http://*.one.com", "img-src https://two.com"},
       false},
      {"script-src 'none'",
       {"script-src http://*.one.com", "script-src https://two.com"},
       true},
      {"script-src 'none'",
       {"script-src http://*.one.com", "script-src 'self'"},
       true},
      // `policyA` is not 'none', but at least effective result of `policiesB`
      // is.
      {"script-src http://example.com 'none'", {"script-src 'none'"}, true},
      {"script-src http://example.com", {"script-src 'none'"}, true},
      {"script-src http://example.com 'none'",
       {"script-src http://*.one.com", "script-src http://one.com",
        "script-src 'none'"},
       true},
      {"script-src http://example.com",
       {"script-src http://*.one.com", "script-src http://one.com",
        "script-src 'none'"},
       true},
      {"script-src http://one.com 'none'",
       {"script-src http://*.one.com", "script-src http://one.com",
        "script-src https://one.com"},
       true},
      // `policyA` is `none` and at least effective result of `policiesB` is
      // too.
      {"script-src ", {"script-src ", "script-src "}, true},
      {"script-src 'none'", {"script-src", "script-src 'none'"}, true},
      {"script-src ", {"script-src 'none'", "script-src 'none'"}, true},
      {"script-src ",
       {"script-src 'none' http://example.com",
        "script-src 'none' http://example.com"},
       false},
      {"script-src 'none'", {"script-src 'none'", "script-src 'none'"}, true},
      {"script-src 'none'",
       {"script-src 'none'", "script-src 'none'", "script-src 'none'"},
       true},
      {"script-src 'none'",
       {"script-src http://*.one.com", "script-src http://one.com",
        "script-src 'none'"},
       true},
      {"script-src 'none'",
       {"script-src http://*.one.com", "script-src http://two.com",
        "script-src http://three.com"},
       true},
      // Policies contain special keywords.
      {"script-src ", {"script-src ", "script-src 'unsafe-eval'"}, true},
      {"script-src 'none'",
       {"script-src 'unsafe-inline'", "script-src 'none'"},
       true},
      {"script-src ",
       {"script-src 'none' 'unsafe-inline'",
        "script-src 'none' 'unsafe-inline'"},
       false},
      {"script-src ",
       {"script-src 'none' 'unsafe-inline'",
        "script-src 'unsafe-inline' 'strict-dynamic'"},
       false},
      {"script-src 'unsafe-eval'",
       {"script-src 'unsafe-eval'", "script 'unsafe-inline'"},
       true},
      {"script-src 'unsafe-inline'",
       {"script-src  ", "script http://example.com"},
       true},
  };

  for (const auto& test : cases) {
    CSPDirectiveList* a =
        CreateList(test.policy_a, kContentSecurityPolicyHeaderTypeEnforce);

    HeapVector<Member<CSPDirectiveList>> list_b;
    for (auto* const policy_b : test.policies_b)
      list_b.push_back(
          CreateList(policy_b, kContentSecurityPolicyHeaderTypeEnforce));

    EXPECT_EQ(test.expected, a->Subsumes(list_b));
  }
}

TEST_F(CSPDirectiveListTest, SubsumesPluginTypes) {
  struct TestCase {
    const char* policy_a;
    const Vector<const char*> policies_b;
    bool expected;
  } cases[] = {
      // `policyA` subsumes `policiesB`.
      {"script-src 'unsafe-inline'",
       {"script-src  ", "script-src http://example.com",
        "plugin-types text/plain"},
       true},
      {"script-src http://example.com",
       {"script-src http://example.com; plugin-types "},
       true},
      {"script-src http://example.com",
       {"script-src http://example.com; plugin-types text/plain"},
       true},
      {"script-src http://example.com; plugin-types text/plain",
       {"script-src http://example.com; plugin-types text/plain"},
       true},
      {"script-src http://example.com; plugin-types text/plain",
       {"script-src http://example.com; plugin-types "},
       true},
      {"script-src http://example.com; plugin-types text/plain",
       {"script-src http://example.com; plugin-types ", "plugin-types "},
       true},
      {"plugin-types application/pdf text/plain",
       {"plugin-types application/pdf text/plain",
        "plugin-types application/x-blink-test-plugin"},
       true},
      {"plugin-types application/pdf text/plain",
       {"plugin-types application/pdf text/plain",
        "plugin-types application/pdf text/plain "
        "application/x-blink-test-plugin"},
       true},
      {"plugin-types application/x-shockwave-flash application/pdf text/plain",
       {"plugin-types application/x-shockwave-flash application/pdf text/plain",
        "plugin-types application/x-shockwave-flash"},
       true},
      {"plugin-types application/x-shockwave-flash",
       {"plugin-types application/x-shockwave-flash application/pdf text/plain",
        "plugin-types application/x-shockwave-flash"},
       true},
      // `policyA` does not subsume `policiesB`.
      {"script-src http://example.com; plugin-types text/plain",
       {"script-src http://example.com"},
       false},
      {"plugin-types random-value",
       {"script-src 'unsafe-inline'", "plugin-types text/plain"},
       false},
      {"plugin-types random-value",
       {"script-src http://example.com", "script-src http://example.com"},
       false},
      {"plugin-types random-value",
       {"plugin-types  text/plain", "plugin-types text/plain"},
       false},
      {"script-src http://example.com; plugin-types text/plain",
       {"plugin-types ", "plugin-types "},
       false},
      {"plugin-types application/pdf text/plain",
       {"plugin-types application/x-blink-test-plugin",
        "plugin-types application/x-blink-test-plugin"},
       false},
      {"plugin-types application/pdf text/plain",
       {"plugin-types application/pdf application/x-blink-test-plugin",
        "plugin-types application/x-blink-test-plugin"},
       false},
  };

  for (const auto& test : cases) {
    CSPDirectiveList* a =
        CreateList(test.policy_a, kContentSecurityPolicyHeaderTypeEnforce);

    HeapVector<Member<CSPDirectiveList>> list_b;
    for (auto* const policy_b : test.policies_b)
      list_b.push_back(
          CreateList(policy_b, kContentSecurityPolicyHeaderTypeEnforce));

    EXPECT_EQ(test.expected, a->Subsumes(list_b));
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

  CSPDirectiveList* empty =
      CreateList("", kContentSecurityPolicyHeaderTypeEnforce);

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
                                  kContentSecurityPolicyHeaderTypeEnforce);

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
                                kContentSecurityPolicyHeaderTypeEnforce);
    EXPECT_FALSE(directive_list->OperativeDirective(test.directive));
  }
}

TEST_F(CSPDirectiveListTest, GetSourceVector) {
  const Vector<const char*> policies = {
      // Policy 1
      "default-src https://default-src.com",
      // Policy 2
      "child-src http://child-src.com",
      // Policy 3
      "child-src http://child-src.com; default-src https://default-src.com",
      // Policy 4
      "base-uri http://base-uri.com",
      // Policy 5
      "frame-src http://frame-src.com"};

  // Check expectations on the initial set-up.
  HeapVector<Member<CSPDirectiveList>> policy_vector;
  for (auto* const policy : policies) {
    policy_vector.push_back(
        CreateList(policy, kContentSecurityPolicyHeaderTypeEnforce));
  }
  HeapVector<Member<SourceListDirective>> result =
      CSPDirectiveList::GetSourceVector(
          ContentSecurityPolicy::DirectiveType::kDefaultSrc, policy_vector);
  EXPECT_EQ(result.size(), 2u);
  result = CSPDirectiveList::GetSourceVector(
      ContentSecurityPolicy::DirectiveType::kChildSrc, policy_vector);
  EXPECT_EQ(result.size(), 3u);
  result = CSPDirectiveList::GetSourceVector(
      ContentSecurityPolicy::DirectiveType::kBaseURI, policy_vector);
  EXPECT_EQ(result.size(), 1u);
  result = CSPDirectiveList::GetSourceVector(
      ContentSecurityPolicy::DirectiveType::kFrameSrc, policy_vector);
  EXPECT_EQ(result.size(), 4u);

  enum DefaultBehaviour { kDefault, kNoDefault, kChildAndDefault };

  struct TestCase {
    ContentSecurityPolicy::DirectiveType directive;
    const DefaultBehaviour type;
    size_t expected_total;
    int expected_current;
    int expected_default_src;
    int expected_child_src;
  } cases[] = {
      // Directives with default directive.
      {ContentSecurityPolicy::DirectiveType::kChildSrc, kDefault, 4u, 3, 1, 3},
      {ContentSecurityPolicy::DirectiveType::kConnectSrc, kDefault, 3u, 1, 2,
       0},
      {ContentSecurityPolicy::DirectiveType::kFontSrc, kDefault, 3u, 1, 2, 0},
      {ContentSecurityPolicy::DirectiveType::kImgSrc, kDefault, 3u, 1, 2, 0},
      {ContentSecurityPolicy::DirectiveType::kManifestSrc, kDefault, 3u, 1, 2,
       0},
      {ContentSecurityPolicy::DirectiveType::kMediaSrc, kDefault, 3u, 1, 2, 0},
      {ContentSecurityPolicy::DirectiveType::kObjectSrc, kDefault, 3u, 1, 2, 0},
      {ContentSecurityPolicy::DirectiveType::kScriptSrc, kDefault, 3u, 1, 2, 0},
      {ContentSecurityPolicy::DirectiveType::kStyleSrc, kDefault, 3u, 1, 2, 0},
      // Directives with no default directive.
      {ContentSecurityPolicy::DirectiveType::kBaseURI, kNoDefault, 2u, 2, 0, 0},
      {ContentSecurityPolicy::DirectiveType::kFrameAncestors, kNoDefault, 1u, 1,
       0, 0},
      {ContentSecurityPolicy::DirectiveType::kFormAction, kNoDefault, 1u, 1, 0,
       0},
      // Directive with multiple default directives.
      {ContentSecurityPolicy::DirectiveType::kFrameSrc, kChildAndDefault, 5u, 2,
       1, 2},
  };

  for (const auto& test : cases) {
    // Initial set-up.
    HeapVector<Member<CSPDirectiveList>> policy_vector;
    for (auto* const policy : policies) {
      policy_vector.push_back(
          CreateList(policy, kContentSecurityPolicyHeaderTypeEnforce));
    }
    // Append current test's policy.
    std::stringstream current_directive;
    const char* name = ContentSecurityPolicy::GetDirectiveName(test.directive);
    current_directive << name << " http://" << name << ".com;";
    policy_vector.push_back(
        CreateList(current_directive.str().c_str(),
                   kContentSecurityPolicyHeaderTypeEnforce));

    HeapVector<Member<SourceListDirective>> result =
        CSPDirectiveList::GetSourceVector(test.directive, policy_vector);

    EXPECT_EQ(result.size(), test.expected_total);

    int actual_current = 0, actual_default = 0, actual_child = 0;
    for (const auto& src_list : result) {
      HeapVector<Member<CSPSource>> sources = src_list->list_;
      for (const auto& source : sources) {
        if (source->host_.StartsWith(name))
          actual_current += 1;
        else if (source->host_ == "default-src.com")
          actual_default += 1;

        if (source->host_ == "child-src.com")
          actual_child += 1;
      }
    }

    EXPECT_EQ(actual_default, test.expected_default_src);
    EXPECT_EQ(actual_current, test.expected_current);
    EXPECT_EQ(actual_child, test.expected_child_src);

    // If another default-src is added that should only impact Fetch Directives
    policy_vector.push_back(
        CreateList("default-src https://default-src.com;",
                   kContentSecurityPolicyHeaderTypeEnforce));
    size_t udpated_total =
        test.type != kNoDefault ? test.expected_total + 1 : test.expected_total;
    EXPECT_EQ(
        CSPDirectiveList::GetSourceVector(test.directive, policy_vector).size(),
        udpated_total);
    size_t expected_child_src =
        test.directive == ContentSecurityPolicy::DirectiveType::kChildSrc ? 5u
                                                                          : 4u;
    EXPECT_EQ(
        CSPDirectiveList::GetSourceVector(
            ContentSecurityPolicy::DirectiveType::kChildSrc, policy_vector)
            .size(),
        expected_child_src);

    // If another child-src is added that should only impact frame-src and
    // child-src
    policy_vector.push_back(
        CreateList("child-src http://child-src.com;",
                   kContentSecurityPolicyHeaderTypeEnforce));
    udpated_total = test.type == kChildAndDefault ||
                            test.directive ==
                                ContentSecurityPolicy::DirectiveType::kChildSrc
                        ? udpated_total + 1
                        : udpated_total;
    EXPECT_EQ(
        CSPDirectiveList::GetSourceVector(test.directive, policy_vector).size(),
        udpated_total);
    expected_child_src = expected_child_src + 1u;
    EXPECT_EQ(
        CSPDirectiveList::GetSourceVector(
            ContentSecurityPolicy::DirectiveType::kChildSrc, policy_vector)
            .size(),
        expected_child_src);

    // If we add sandbox, nothing should change since it is currenly not
    // considered.
    policy_vector.push_back(
        CreateList("sandbox http://sandbox.com;",
                   kContentSecurityPolicyHeaderTypeEnforce));
    EXPECT_EQ(
        CSPDirectiveList::GetSourceVector(test.directive, policy_vector).size(),
        udpated_total);
    EXPECT_EQ(
        CSPDirectiveList::GetSourceVector(
            ContentSecurityPolicy::DirectiveType::kChildSrc, policy_vector)
            .size(),
        expected_child_src);
  }
}

TEST_F(CSPDirectiveListTest, ReportEndpointsProperlyParsed) {
  struct TestCase {
    const char* policy;
    ContentSecurityPolicyHeaderSource header_source;
    Vector<String> expected_endpoints;
    bool expected_use_reporting_api;
  } cases[] = {
      {"script-src 'self';", kContentSecurityPolicyHeaderSourceHTTP, {}, false},
      {"script-src 'self'; report-uri https://example.com",
       kContentSecurityPolicyHeaderSourceHTTP,
       {"https://example.com"},
       false},
      {"script-src 'self'; report-uri https://example.com "
       "https://example2.com",
       kContentSecurityPolicyHeaderSourceHTTP,
       {"https://example.com", "https://example2.com"},
       false},
      {"script-src 'self'; report-uri https://example.com",
       kContentSecurityPolicyHeaderSourceMeta,
       {},
       false},
      {"script-src 'self'; report-to group",
       kContentSecurityPolicyHeaderSourceHTTP,
       {"group"},
       true},
      // report-to supersedes report-uri
      {"script-src 'self'; report-to group; report-uri https://example.com",
       kContentSecurityPolicyHeaderSourceHTTP,
       {"group"},
       true},
      {"script-src 'self'; report-to group",
       kContentSecurityPolicyHeaderSourceMeta,
       {"group"},
       true},
      {"script-src 'self'; report-to group; report-to group2;",
       kContentSecurityPolicyHeaderSourceHTTP,
       {"group"},
       true},
      {"script-src 'self'; report-to group; report-uri https://example.com; "
       "report-to group2",
       kContentSecurityPolicyHeaderSourceHTTP,
       {"group"},
       true},
      {"script-src 'self'; report-uri https://example.com; report-to group; "
       "report-to group2",
       kContentSecurityPolicyHeaderSourceHTTP,
       {"group"},
       true},
      {"script-src 'self'; report-uri https://example.com "
       "https://example2.com; report-to group",
       kContentSecurityPolicyHeaderSourceHTTP,
       {"group"},
       true},
      {"script-src 'self'; report-uri https://example.com; report-to group; "
       "report-uri https://example.com",
       kContentSecurityPolicyHeaderSourceHTTP,
       {"group"},
       true},
  };

  for (const auto& test : cases) {
    // Test both enforce and report, there should not be a difference
    for (const auto& header_type : {kContentSecurityPolicyHeaderTypeEnforce,
                                    kContentSecurityPolicyHeaderTypeReport}) {
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

}  // namespace blink
