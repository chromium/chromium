// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/allowed_by_nosniff.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/public/mojom/use_counter/metrics/webdx_feature.mojom-blink.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/console_logger.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/loader/testing/test_loader_factory.h"
#include "third_party/blink/renderer/platform/loader/testing/test_resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

namespace {

using MimeTypeCheck = AllowedByNosniff::MimeTypeCheck;
using WebFeature = mojom::WebFeature;
using WebDXFeature = mojom::blink::WebDXFeature;
using ::testing::_;

class MockUseCounter : public GarbageCollected<MockUseCounter>,
                       public UseCounter {
 public:
  static MockUseCounter* Create() {
    return MakeGarbageCollected<testing::StrictMock<MockUseCounter>>();
  }

  MOCK_METHOD1(CountUse, void(WebFeature));
  MOCK_METHOD1(CountWebDXFeature, void(WebDXFeature));
  MOCK_METHOD1(CountDeprecation, void(WebFeature));
};

class MockConsoleLogger : public GarbageCollected<MockConsoleLogger>,
                          public ConsoleLogger {
 public:
  MOCK_METHOD5(AddConsoleMessageImpl,
               void(mojom::ConsoleMessageSource,
                    mojom::ConsoleMessageLevel,
                    const String&,
                    bool,
                    std::optional<mojom::ConsoleMessageCategory>));
  MOCK_METHOD2(AddConsoleMessageImpl, void(ConsoleMessage*, bool));
};

}  // namespace

class AllowedByNosniffTest : public testing::TestWithParam<bool> {
 public:
};

INSTANTIATE_TEST_SUITE_P(All, AllowedByNosniffTest, ::testing::Bool());

TEST_P(AllowedByNosniffTest, AllowedOrNot) {
  RuntimeEnabledFeaturesTestHelpers::ScopedStrictMimeTypesForWorkers feature(
      GetParam());

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
    MockUseCounter* use_counter = MockUseCounter::Create();
    MockConsoleLogger* logger = MakeGarbageCollected<MockConsoleLogger>();
    ResourceResponse response(url);
    response.SetHttpHeaderField(http_names::kContentType,
                                AtomicString(testcase.mimetype));

    EXPECT_CALL(*use_counter, CountUse(_)).Times(::testing::AnyNumber());
    if (!testcase.allowed)
      EXPECT_CALL(*logger, AddConsoleMessageImpl(_, _, _, _, _));
    EXPECT_EQ(testcase.allowed, AllowedByNosniff::MimeTypeAsScript(
                                    *use_counter, logger, response,
                                    MimeTypeCheck::kLaxForElement));
    ::testing::Mock::VerifyAndClear(use_counter);

    EXPECT_CALL(*use_counter, CountUse(_)).Times(::testing::AnyNumber());
    bool expect_allowed =
        RuntimeEnabledFeatures::StrictMimeTypesForWorkersEnabled()
            ? testcase.strict_allowed
            : testcase.allowed;
    if (!expect_allowed)
      EXPECT_CALL(*logger, AddConsoleMessageImpl(_, _, _, _, _));
    EXPECT_EQ(expect_allowed,
              AllowedByNosniff::MimeTypeAsScript(*use_counter, logger, response,
                                                 MimeTypeCheck::kLaxForWorker));
    ::testing::Mock::VerifyAndClear(use_counter);

    EXPECT_CALL(*use_counter, CountUse(_)).Times(::testing::AnyNumber());
    if (!testcase.strict_allowed)
      EXPECT_CALL(*logger, AddConsoleMessageImpl(_, _, _, _, _));
    EXPECT_EQ(testcase.strict_allowed,
              AllowedByNosniff::MimeTypeAsScript(*use_counter, logger, response,
                                                 MimeTypeCheck::kStrict));
    ::testing::Mock::VerifyAndClear(use_counter);
  }
}

TEST_P(AllowedByNosniffTest, Counters) {
  RuntimeEnabledFeaturesTestHelpers::ScopedStrictMimeTypesForWorkers feature(
      GetParam());

  constexpr auto kBasic = network::mojom::FetchResponseType::kBasic;
  constexpr auto kOpaque = network::mojom::FetchResponseType::kOpaque;
  constexpr auto kCors = network::mojom::FetchResponseType::kCors;
  const char* bla = "https://bla.com";
  const char* blubb = "https://blubb.com";
  struct {
    const char* url;
    const char* origin;
    const char* mimetype;
    network::mojom::FetchResponseType response_type;
    WebFeature expected;
  } data[] = {
      // Test same- vs cross-origin cases.
      {bla, "", "text/plain", kOpaque, WebFeature::kCrossOriginTextScript},
      {bla, "", "text/plain", kCors, WebFeature::kCrossOriginTextPlain},
      {bla, blubb, "text/plain", kCors, WebFeature::kCrossOriginTextScript},
      {bla, blubb, "text/plain", kOpaque, WebFeature::kCrossOriginTextPlain},
      {bla, bla, "text/plain", kBasic, WebFeature::kSameOriginTextScript},
      {bla, bla, "text/plain", kBasic, WebFeature::kSameOriginTextPlain},
      {bla, bla, "text/json", kBasic, WebFeature::kSameOriginTextScript},

      // JSON
      {bla, bla, "text/json", kBasic, WebFeature::kSameOriginJsonTypeForScript},
      {bla, bla, "application/json", kBasic,
       WebFeature::kSameOriginJsonTypeForScript},
      {bla, blubb, "text/json", kOpaque,
       WebFeature::kCrossOriginJsonTypeForScript},
      {bla, blubb, "application/json", kOpaque,
       WebFeature::kCrossOriginJsonTypeForScript},

      // Test mime type and subtype handling.
      {bla, bla, "text/xml", kBasic, WebFeature::kSameOriginTextScript},
      {bla, bla, "text/xml", kBasic, WebFeature::kSameOriginTextXml},

      // Test mime types from crbug.com/765544, with random cross/same site
      // origins.
      {bla, bla, "text/plain", kBasic, WebFeature::kSameOriginTextPlain},
      {bla, bla, "text/xml", kOpaque, WebFeature::kCrossOriginTextXml},
      {blubb, blubb, "application/octet-stream", kBasic,
       WebFeature::kSameOriginApplicationOctetStream},
      {blubb, blubb, "application/xml", kCors,
       WebFeature::kCrossOriginApplicationXml},
      {bla, bla, "text/html", kBasic, WebFeature::kSameOriginTextHtml},

      // Unknown
      {bla, bla, "not/script", kBasic,
       WebFeature::kSameOriginStrictNosniffWouldBlock},
      {bla, blubb, "not/script", kOpaque,
       WebFeature::kCrossOriginStrictNosniffWouldBlock},
  };

  for (auto& testcase : data) {
    SCOPED_TRACE(testing::Message()
                 << "\n  url: " << testcase.url << "\n  origin: "
                 << testcase.origin << "\n  mime type: " << testcase.mimetype
                 << "\n response type: " << testcase.response_type
                 << "\n  webfeature: " << testcase.expected);
    MockUseCounter* use_counter = MockUseCounter::Create();
    MockConsoleLogger* logger = MakeGarbageCollected<MockConsoleLogger>();
    ResourceResponse response(KURL(testcase.url));
    response.SetType(testcase.response_type);
    response.SetHttpHeaderField(http_names::kContentType,
                                AtomicString(testcase.mimetype));

    EXPECT_CALL(*use_counter, CountUse(testcase.expected));
    EXPECT_CALL(*use_counter, CountUse(::testing::Ne(testcase.expected)))
        .Times(::testing::AnyNumber());
    AllowedByNosniff::MimeTypeAsScript(*use_counter, logger, response,
                                       MimeTypeCheck::kLaxForElement);
    ::testing::Mock::VerifyAndClear(use_counter);

    // kLaxForWorker should (by default) behave the same as kLaxForElement,
    // but should behave like kStrict if StrictMimeTypesForWorkersEnabled().
    // So in the strict case we'll expect the counter calls only if it's a
    // legitimate script.
    bool expect_worker_lax =
        (testcase.expected == WebFeature::kCrossOriginTextScript ||
         testcase.expected == WebFeature::kSameOriginTextScript) ||
        !RuntimeEnabledFeatures::StrictMimeTypesForWorkersEnabled();
    EXPECT_CALL(*use_counter, CountUse(testcase.expected))
        .Times(expect_worker_lax);
    EXPECT_CALL(*use_counter, CountUse(::testing::Ne(testcase.expected)))
        .Times(::testing::AnyNumber());
    AllowedByNosniff::MimeTypeAsScript(*use_counter, logger, response,
                                       MimeTypeCheck::kLaxForWorker);
    ::testing::Mock::VerifyAndClear(use_counter);

    // The kStrictMimeTypeChecksWouldBlockWorker counter should only be active
    // is "lax" checking for workers is enabled.
    EXPECT_CALL(*use_counter,
                CountUse(WebFeature::kStrictMimeTypeChecksWouldBlockWorker))
        .Times(!RuntimeEnabledFeatures::StrictMimeTypesForWorkersEnabled());
    EXPECT_CALL(*use_counter,
                CountUse(::testing::Ne(
                    WebFeature::kStrictMimeTypeChecksWouldBlockWorker)))
        .Times(::testing::AnyNumber());
    AllowedByNosniff::MimeTypeAsScript(*use_counter, logger, response,
                                       MimeTypeCheck::kLaxForWorker);
    ::testing::Mock::VerifyAndClear(use_counter);
  }
}

TEST_P(AllowedByNosniffTest, AllTheSchemes) {
  RuntimeEnabledFeaturesTestHelpers::ScopedStrictMimeTypesForWorkers feature(
      GetParam());

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

      // `blob:` and `filesystem:` are excluded:
      {"blob:https://example.com/bla.js", true},
      {"blob:https://example.com/bla.txt", true},
      {"filesystem:https://example.com/temporary/bla.js", true},
      {"filesystem:https://example.com/temporary/bla.txt", true},
  };

  for (auto& testcase : data) {
    auto* use_counter = MockUseCounter::Create();
    MockConsoleLogger* logger = MakeGarbageCollected<MockConsoleLogger>();
    EXPECT_CALL(*logger, AddConsoleMessageImpl(_, _, _, _, _))
        .Times(::testing::AnyNumber());
    SCOPED_TRACE(testing::Message() << "\n  url: " << testcase.url
                                    << "\n  allowed: " << testcase.allowed);
    ResourceResponse response(KURL(testcase.url));
    response.SetHttpHeaderField(http_names::kContentType,
                                AtomicString("invalid"));
    response.SetHttpHeaderField(http_names::kXContentTypeOptions,
                                AtomicString("nosniff"));
    EXPECT_EQ(testcase.allowed,
              AllowedByNosniff::MimeTypeAsScript(*use_counter, logger, response,
                                                 MimeTypeCheck::kStrict));
    EXPECT_EQ(testcase.allowed, AllowedByNosniff::MimeTypeAsScript(
                                    *use_counter, logger, response,
                                    MimeTypeCheck::kLaxForElement));
    EXPECT_EQ(testcase.allowed,
              AllowedByNosniff::MimeTypeAsScript(*use_counter, logger, response,
                                                 MimeTypeCheck::kLaxForWorker));
  }
}

TEST(AllowedByNosniffTest, XMLExternalEntity) {
  MockConsoleLogger* logger = MakeGarbageCollected<MockConsoleLogger>();

  {
    ResourceResponse response(KURL("https://example.com/"));
    EXPECT_TRUE(
        AllowedByNosniff::MimeTypeAsXMLExternalEntity(logger, response));
  }

  {
    ResourceResponse response(KURL("https://example.com/"));
    response.SetHttpHeaderField(http_names::kContentType,
                                AtomicString("text/plain"));
    EXPECT_TRUE(
        AllowedByNosniff::MimeTypeAsXMLExternalEntity(logger, response));
  }

  {
    ResourceResponse response(KURL("https://example.com/"));
    response.SetHttpHeaderField(http_names::kXContentTypeOptions,
                                AtomicString("nosniff"));
    EXPECT_CALL(*logger, AddConsoleMessageImpl(_, _, _, _, _));
    EXPECT_FALSE(
        AllowedByNosniff::MimeTypeAsXMLExternalEntity(logger, response));
  }

  {
    ResourceResponse response(KURL("https://example.com/"));
    response.SetHttpHeaderField(http_names::kContentType,
                                AtomicString("text/plain"));
    response.SetHttpHeaderField(http_names::kXContentTypeOptions,
                                AtomicString("nosniff"));
    EXPECT_CALL(*logger, AddConsoleMessageImpl(_, _, _, _, _));
    EXPECT_FALSE(
        AllowedByNosniff::MimeTypeAsXMLExternalEntity(logger, response));
  }

  {
    ResourceResponse response(KURL("https://example.com/"));
    response.SetHttpHeaderField(
        http_names::kContentType,
        AtomicString("application/xml-external-parsed-entity"));
    response.SetHttpHeaderField(http_names::kXContentTypeOptions,
                                AtomicString("nosniff"));
    EXPECT_TRUE(
        AllowedByNosniff::MimeTypeAsXMLExternalEntity(logger, response));
  }

  {
    ResourceResponse response(KURL("https://example.com/"));
    response.SetHttpHeaderField(
        http_names::kContentType,
        AtomicString("text/xml-external-parsed-entity"));
    response.SetHttpHeaderField(http_names::kXContentTypeOptions,
                                AtomicString("nosniff"));
    EXPECT_TRUE(
        AllowedByNosniff::MimeTypeAsXMLExternalEntity(logger, response));
  }
}

}  // namespace blink
