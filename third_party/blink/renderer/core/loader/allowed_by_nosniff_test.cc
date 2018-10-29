// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/allowed_by_nosniff.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/inspector/console_message_storage.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"

namespace blink {

class AllowedByNosniffTest : public testing::Test {
 public:
  void SetUp() override {
    // Create a new dummy page holder for each test, so that we get a fresh
    // set of counters for each.
    dummy_page_holder_ = DummyPageHolder::Create();
    Page::InsertOrdinaryPageForTesting(&dummy_page_holder_->GetPage());
  }

  Document* doc() { return &dummy_page_holder_->GetDocument(); }

  size_t ConsoleMessageStoreSize() const {
    return dummy_page_holder_->GetPage().GetConsoleMessageStorage().size();
  }

 private:
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

TEST_F(AllowedByNosniffTest, SanityCheckSetUp) {
  // UseCounter counts will be silently swallowed under various conditions,
  // e.g. if the document doesn't actually hold a frame. This test is a sanity
  // test that UseCounter::Count + UseCounter::IsCounted work at all with the
  // current test setup. If this test fails, we know that the setup is wrong,
  // rather than the code under test.
  WebFeature f = WebFeature::kSameOriginTextScript;
  EXPECT_FALSE(UseCounter::IsCounted(*doc(), f));
  UseCounter::Count(doc(), f);
  EXPECT_TRUE(UseCounter::IsCounted(*doc(), f));

  EXPECT_EQ(ConsoleMessageStoreSize(), 0U);
}

TEST_F(AllowedByNosniffTest, AllowedOrNot) {
  struct {
    const char* mimetype;
    bool allowed;
    bool strict_allowed;
  } data[] = {
      // Supported mimetypes:
      {"text/javascript", true, true},
      {"application/javascript", true, true},
      {"text/ecmascript", true, true},

      // Blocked mimetpyes:
      {"image/png", false, false},
      {"text/csv", false, false},
      {"video/mpeg", false, false},

      // Legacy mimetypes:
      {"text/html", true, false},
      {"text/plain", true, false},
      {"application/xml", true, false},
      {"application/octet-stream", true, false},

      // Potato mimetypes:
      {"text/potato", true, false},
      {"potato/text", true, false},
      {"aaa/aaa", true, false},
      {"zzz/zzz", true, false},

      // Parameterized mime types:
      {"text/javascript; charset=utf-8", true, true},
      {"text/javascript;charset=utf-8", true, true},
      {"text/javascript;bla;bla", true, true},
      {"text/csv; charset=utf-8", false, false},
      {"text/csv;charset=utf-8", false, false},
      {"text/csv;bla;bla", false, false},

      // Funky capitalization:
      {"text/html", true, false},
      {"Text/html", true, false},
      {"text/Html", true, false},
      {"TeXt/HtMl", true, false},
      {"TEXT/HTML", true, false},
  };

  for (auto& testcase : data) {
    SCOPED_TRACE(testing::Message()
                 << "\n  mime type: " << testcase.mimetype
                 << "\n  allowed: " << (testcase.allowed ? "true" : "false")
                 << "\n  strict_allowed: "
                 << (testcase.strict_allowed ? "true" : "false"));

    const KURL url("https://bla.com/");
    doc()->SetSecurityOrigin(SecurityOrigin::Create(url));
    ResourceResponse response(url);
    response.SetHTTPHeaderField("Content-Type", testcase.mimetype);

    // Nosniff 'legacy' setting: Both worker + non-worker obey the 'allowed'
    // setting. Warnings for any blocked script.
    RuntimeEnabledFeatures::SetWorkerNosniffBlockEnabled(false);
    RuntimeEnabledFeatures::SetWorkerNosniffWarnEnabled(false);
    size_t message_count = ConsoleMessageStoreSize();
    EXPECT_EQ(testcase.allowed,
              AllowedByNosniff::MimeTypeAsScript(doc(), response));
    EXPECT_EQ(testcase.allowed, AllowedByNosniff::MimeTypeAsScriptForTesting(
                                    doc(), response, true));
    EXPECT_EQ(ConsoleMessageStoreSize(), message_count + 2 * !testcase.allowed);

    // Nosniff worker blocked: Workers follow the 'strict_allow' setting.
    // Warnings for any blocked scripts.
    RuntimeEnabledFeatures::SetWorkerNosniffBlockEnabled(true);
    RuntimeEnabledFeatures::SetWorkerNosniffWarnEnabled(false);
    message_count = ConsoleMessageStoreSize();
    EXPECT_EQ(testcase.allowed,
              AllowedByNosniff::MimeTypeAsScript(doc(), response));
    EXPECT_EQ(ConsoleMessageStoreSize(), message_count + !testcase.allowed);
    EXPECT_EQ(
        testcase.strict_allowed,
        AllowedByNosniff::MimeTypeAsScriptForTesting(doc(), response, true));
    EXPECT_EQ(ConsoleMessageStoreSize(),
              message_count + !testcase.allowed + !testcase.strict_allowed);

    // Nosniff 'legacy', but with warnings. The allowed setting follows the
    // 'allowed' setting, but the warnings follow the 'strict' setting.
    RuntimeEnabledFeatures::SetWorkerNosniffBlockEnabled(false);
    RuntimeEnabledFeatures::SetWorkerNosniffWarnEnabled(true);
    message_count = ConsoleMessageStoreSize();
    EXPECT_EQ(testcase.allowed,
              AllowedByNosniff::MimeTypeAsScript(doc(), response));
    EXPECT_EQ(ConsoleMessageStoreSize(), message_count + !testcase.allowed);
    EXPECT_EQ(testcase.allowed, AllowedByNosniff::MimeTypeAsScriptForTesting(
                                    doc(), response, true));
    EXPECT_EQ(ConsoleMessageStoreSize(),
              message_count + !testcase.allowed + !testcase.strict_allowed);
  }
}

TEST_F(AllowedByNosniffTest, Counters) {
  const char* bla = "https://bla.com";
  const char* blubb = "https://blubb.com";
  struct {
    const char* url;
    const char* origin;
    const char* mimetype;
    WebFeature expected;
  } data[] = {
      // Test same- vs cross-origin cases.
      {bla, "", "text/plain", WebFeature::kCrossOriginTextScript},
      {bla, "", "text/plain", WebFeature::kCrossOriginTextPlain},
      {bla, blubb, "text/plain", WebFeature::kCrossOriginTextScript},
      {bla, blubb, "text/plain", WebFeature::kCrossOriginTextPlain},
      {bla, bla, "text/plain", WebFeature::kSameOriginTextScript},
      {bla, bla, "text/plain", WebFeature::kSameOriginTextPlain},

      // Test mime type and subtype handling.
      {bla, bla, "text/xml", WebFeature::kSameOriginTextScript},
      {bla, bla, "text/xml", WebFeature::kSameOriginTextXml},

      // Test mime types from crbug.com/765544, with random cross/same site
      // origins.
      {bla, bla, "text/plain", WebFeature::kSameOriginTextPlain},
      {bla, blubb, "text/xml", WebFeature::kCrossOriginTextXml},
      {blubb, blubb, "application/octet-stream",
       WebFeature::kSameOriginApplicationOctetStream},
      {blubb, bla, "application/xml", WebFeature::kCrossOriginApplicationXml},
      {bla, bla, "text/html", WebFeature::kSameOriginTextHtml},
  };

  for (auto& testcase : data) {
    SetUp();
    SCOPED_TRACE(testing::Message() << "\n  url: " << testcase.url
                                    << "\n  origin: " << testcase.origin
                                    << "\n  mime type: " << testcase.mimetype
                                    << "\n  webfeature: " << testcase.expected);
    doc()->SetSecurityOrigin(SecurityOrigin::Create(KURL(testcase.origin)));
    ResourceResponse response(KURL(testcase.url));
    response.SetHTTPHeaderField("Content-Type", testcase.mimetype);

    AllowedByNosniff::MimeTypeAsScript(doc(), response);
    EXPECT_TRUE(UseCounter::IsCounted(*doc(), testcase.expected));
  }
}

TEST_F(AllowedByNosniffTest, AllTheSchemes) {
  // We test various URL schemes.
  // To force a decision based on the scheme, we give all responses an
  // invalid Content-Type plus a "nosniff" header. That way, all Content-Type
  // based checks are always denied and we can test for whether this is decided
  // based on the URL or not.
  struct {
    const char* url;
    bool allowed;
  } data[] = {
      {"http://example.com/bla.js", false},
      {"https://example.com/bla.js", false},
      {"file://etc/passwd.js", true},
      {"file://etc/passwd", false},
      {"chrome://dino/dino.js", true},
      {"chrome://dino/dino.css", false},
      {"ftp://example.com/bla.js", true},
      {"ftp://example.com/bla.txt", false},

      {"file://home/potato.txt", false},
      {"file://home/potato.js", true},
      {"file://home/potato.mjs", true},
      {"chrome://dino/dino.mjs", true},
  };

  for (auto& testcase : data) {
    SetUp();
    SCOPED_TRACE(testing::Message() << "\n  url: " << testcase.url
                                    << "\n  allowed: " << testcase.allowed);
    ResourceResponse response(KURL(testcase.url));
    response.SetHTTPHeaderField("Content-Type", "invalid");
    response.SetHTTPHeaderField("X-Content-Type-Options", "nosniff");
    EXPECT_EQ(testcase.allowed,
              AllowedByNosniff::MimeTypeAsScript(doc(), response));
  }
}

}  // namespace blink
