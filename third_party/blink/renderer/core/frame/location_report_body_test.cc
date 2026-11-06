// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/location_report_body.h"

#include <set>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/scheme_registry.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {
namespace {

class TestLocationReportBody : public LocationReportBody {
 public:
  explicit TestLocationReportBody(
      const String& source_file = g_empty_string,
      std::optional<uint32_t> line_number = std::nullopt,
      std::optional<uint32_t> column_number = std::nullopt)
      : LocationReportBody(source_file, line_number, column_number) {}
};

// Test whether LocationReportBody::MatchId() is a pure function, i.e. same
// input will give same return value.
TEST(LocationReportBodyMatchIdTest, SameInputGeneratesSameMatchId) {
  String url = "";
  std::optional<uint32_t> line = std::nullopt, column = std::nullopt;
  EXPECT_EQ(TestLocationReportBody(url, line, column).MatchId(),
            TestLocationReportBody(url, line, column).MatchId());

  url = "https://example.com";
  line = std::make_optional<uint32_t>(0);
  column = std::make_optional<uint32_t>(0);
  EXPECT_EQ(TestLocationReportBody(url, line, column).MatchId(),
            TestLocationReportBody(url, line, column).MatchId());
}

bool AllDistinct(const std::vector<unsigned>& match_ids) {
  return match_ids.size() ==
         std::set<unsigned>(match_ids.begin(), match_ids.end()).size();
}

const struct {
  const char* url;
  const std::optional<uint32_t> line_number;
  const std::optional<uint32_t> column_number;
} kLocationReportBodyInputs[] = {
    {"url", std::nullopt, std::nullopt},
    {"url", 0, std::nullopt},
    {"url", std::nullopt, 0},
    {"url", 0, 0},
    {"url", 1, std::nullopt},
    {"url", std::nullopt, 1},
    {"url", 1, 1},
};

TEST(LocationReportBodyMatchIdTest, DifferentInputsGenerateDifferentMatchId) {
  std::vector<unsigned> match_ids;
  for (const auto& input : kLocationReportBodyInputs) {
    match_ids.push_back(TestLocationReportBody(input.url, input.line_number,
                                               input.column_number)
                            .MatchId());
  }
  EXPECT_TRUE(AllDistinct(match_ids));
}

TEST(LocationReportBodyMatchIdTest, MatchIdGeneratedShouldNotBeZero) {
  for (const auto& input : kLocationReportBodyInputs) {
    EXPECT_NE(TestLocationReportBody(input.url, input.line_number,
                                     input.column_number)
                  .MatchId(),
              0u);
  }
}

// When URL is empty, LocationReportBody would call |CaptureSourceLocation()|
// to determine the location, and ignore |line_number| and |column_number|
// specified in constructor params.
TEST(LocationReportBodyMatchIdTest,
     EmptyURLGenerateSameMatchIdRegardlessOfOtherParams) {
  const unsigned empty_hash =
      TestLocationReportBody("", std::nullopt, std::nullopt).MatchId();
  for (const auto& input : kLocationReportBodyInputs) {
    EXPECT_EQ(TestLocationReportBody("", input.line_number, input.column_number)
                  .MatchId(),
              empty_hash);
  }
}

TEST(LocationReportBodyTest, ExtensionURLsAreIdentified) {
  const char* kExtensionUrl =
      "chrome-extension://abcdefghijklmnopabcdefghijklmnop/scripts/"
      "script.js";
  const char* kAboutBlankUrl = "about:blank";
  const char* kHttpsUrl = "https://example.com/";

  EXPECT_FALSE(TestLocationReportBody(kExtensionUrl, 1, 1).IsExtensionSource());
  EXPECT_FALSE(
      TestLocationReportBody(kAboutBlankUrl, 1, 1).IsExtensionSource());
  EXPECT_FALSE(TestLocationReportBody(kHttpsUrl, 1, 1).IsExtensionSource());

  CommonSchemeRegistry::RegisterURLSchemeAsExtension("chrome-extension");
  EXPECT_TRUE(TestLocationReportBody(kExtensionUrl, 1, 1).IsExtensionSource());
  EXPECT_FALSE(
      TestLocationReportBody(kAboutBlankUrl, 1, 1).IsExtensionSource());
  EXPECT_FALSE(TestLocationReportBody(kHttpsUrl, 1, 1).IsExtensionSource());
}

TEST(LocationReportBodyTest, OpaqueScriptLocationIsDropped) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope(KURL("https://example.com/"));
  v8::Isolate* isolate = scope.GetIsolate();
  v8::Local<v8::Context> context = scope.GetContext();

  Persistent<TestLocationReportBody> captured_body;
  v8::Local<v8::Function> capture_fn =
      v8::Function::New(
          context,
          [](const v8::FunctionCallbackInfo<v8::Value>& info) {
            auto* storage = static_cast<Persistent<TestLocationReportBody>*>(
                info.DataV2().As<v8::External>()->Value(
                    v8::kExternalPointerTypeTagDefault));
            *storage = MakeGarbageCollected<TestLocationReportBody>();
          },
          v8::External::New(isolate, &captured_body,
                            v8::kExternalPointerTypeTagDefault))
          .ToLocalChecked();
  context->Global()
      ->Set(context, V8AtomicString(isolate, "captureReport"), capture_fn)
      .Check();

  // 1. Non-opaque script (e.g. same-origin or CORS-enabled cross-origin):
  // location is preserved.
  ClassicScript::Create(
      "\ncaptureReport();", KURL("https://cdn.example.net/cors.js"),
      KURL("https://example.com/"), ScriptFetchOptions(),
      ScriptSourceLocationType::kUnknown, SanitizeScriptErrors::kDoNotSanitize)
      ->RunScriptOnScriptState(scope.GetScriptState());
  ASSERT_TRUE(captured_body);
  EXPECT_EQ("https://cdn.example.net/cors.js", captured_body->sourceFile());
  EXPECT_EQ(2u, captured_body->lineNumber());
  EXPECT_EQ(1u, captured_body->columnNumber());

  // 2. Opaque script (even with a same-origin pre-redirect URL and a
  // //# sourceURL= directive): location is dropped.
  captured_body = nullptr;
  ClassicScript::Create(
      "\ncaptureReport();\n//# sourceURL=webpack:///./src/index.js",
      KURL("https://example.com/redirect-to-cross-origin.js"),
      KURL("https://example.com/"), ScriptFetchOptions(),
      ScriptSourceLocationType::kUnknown, SanitizeScriptErrors::kSanitize)
      ->RunScriptOnScriptState(scope.GetScriptState());
  ASSERT_TRUE(captured_body);
  EXPECT_TRUE(captured_body->sourceFile().IsNull());
  EXPECT_EQ(std::nullopt, captured_body->lineNumber());
  EXPECT_EQ(std::nullopt, captured_body->columnNumber());
}

}  // namespace
}  // namespace blink
