// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/manifest/manifest_parser.h"

#include <stdint.h>

#include <memory>
#include <optional>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/blink/public/common/safe_url_pattern.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-blink.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_uchar.h"
#include "third_party/liburlpattern/pattern.h"

namespace blink {

namespace {
bool IsManifestEmpty(const mojom::blink::ManifestPtr& manifest) {
  return manifest == mojom::blink::Manifest::New();
}
}  // namespace

class ManifestParserTest : public testing::Test {
 public:
  ManifestParserTest(const ManifestParserTest&) = delete;
  ManifestParserTest& operator=(const ManifestParserTest&) = delete;

 protected:
  ManifestParserTest() {}
  ~ManifestParserTest() override {}

  mojom::blink::ManifestPtr& ParseManifestWithURLs(const String& data,
                                                   const KURL& manifest_url,
                                                   const KURL& document_url) {
    ManifestParser parser(data, manifest_url, document_url,
                          /*execution_context=*/nullptr);
    parser.Parse();
    Vector<mojom::blink::ManifestErrorPtr> errors;
    parser.TakeErrors(&errors);

    errors_.clear();
    for (auto& error : errors)
      errors_.push_back(std::move(error->message));
    manifest_ = parser.TakeManifest();
    EXPECT_TRUE(manifest_);
    return manifest_;
  }

  mojom::blink::ManifestPtr& ParseManifest(const String& data) {
    return ParseManifestWithURLs(data, DefaultManifestUrl(),
                                 DefaultDocumentUrl());
  }

  const Vector<String>& errors() const { return errors_; }

  unsigned int GetErrorCount() const { return errors_.size(); }

  static KURL DefaultDocumentUrl() { return KURL("http://foo.com/index.html"); }
  static KURL DefaultManifestUrl() {
    return KURL("http://foo.com/manifest.json");
  }

  bool HasDefaultValuesWithUrls(
      const mojom::blink::ManifestPtr& manifest,
      const KURL& document_url = DefaultDocumentUrl(),
      const KURL& manifest_url = DefaultManifestUrl()) {
    mojom::blink::ManifestPtr expected_manifest = mojom::blink::Manifest::New();
    // A true "default" manifest would have an empty manifest URL. However in
    // these tests we don't want to check for that, rather this method is used
    // to check that a manifest has all its fields set to "default" values, but
    // also have the expected manifest url.
    expected_manifest->manifest_url = manifest_url;
    expected_manifest->start_url = document_url;
    expected_manifest->id = document_url;
    expected_manifest->id.RemoveFragmentIdentifier();
    expected_manifest->scope = KURL(document_url.BaseAsString().ToString());
    return manifest == expected_manifest;
  }

  void VerifySafeUrlPatternSizes(const SafeUrlPattern& pattern,
                                 size_t protocol_size,
                                 size_t username_size,
                                 size_t password_size,
                                 size_t hostname_size,
                                 size_t port_size,
                                 size_t pathname_size,
                                 size_t search_size,
                                 size_t hash_size) {
    EXPECT_EQ(pattern.protocol.size(), protocol_size);
    EXPECT_EQ(pattern.username.size(), username_size);
    EXPECT_EQ(pattern.password.size(), password_size);
    EXPECT_EQ(pattern.hostname.size(), hostname_size);
    EXPECT_EQ(pattern.port.size(), port_size);
    EXPECT_EQ(pattern.pathname.size(), pathname_size);
    EXPECT_EQ(pattern.search.size(), search_size);
    EXPECT_EQ(pattern.hash.size(), hash_size);
  }

 private:
  test::TaskEnvironment task_environment_;
  mojom::blink::ManifestPtr manifest_;
  Vector<String> errors_;
};

TEST_F(ManifestParserTest, CrashTest) {
  // Passing temporary variables should not crash.
  const String json = R"({"start_url": "/"})";
  KURL url("http://example.com");
  ManifestParser parser(json, url, url, /*execution_context=*/nullptr);

  bool has_comments = parser.Parse();
  EXPECT_FALSE(has_comments);
  Vector<mojom::blink::ManifestErrorPtr> errors;
  parser.TakeErrors(&errors);
  auto manifest = parser.TakeManifest();

  // .Parse() should have been call without crashing and succeeded.
  EXPECT_EQ(0u, errors.size());
  EXPECT_FALSE(IsManifestEmpty(manifest));
}

TEST_F(ManifestParserTest, HasComments) {
  const String json = R"({
        // comment
        "start_url": "/"
      })";
  KURL url("http://example.com");
  ManifestParser parser(json, url, url, /*execution_context=*/nullptr);

  bool has_comments = parser.Parse();
  EXPECT_TRUE(has_comments);
}

TEST_F(ManifestParserTest, EmptyStringNull) {
  auto& manifest = ParseManifest("");

  // This Manifest is not a valid JSON object, it's a parsing error.
  EXPECT_EQ(1u, GetErrorCount());
  EXPECT_EQ("Line: 1, column: 1, Syntax error.", errors()[0]);

  // A parsing error is equivalent to an empty manifest.
  EXPECT_TRUE(IsManifestEmpty(manifest));
  EXPECT_FALSE(HasDefaultValuesWithUrls(manifest));
}

TEST_F(ManifestParserTest, ValidNoContentParses) {
  base::HistogramTester histogram_tester;
  auto& manifest = ParseManifestWithURLs("{}", KURL(), DefaultDocumentUrl());

  // Empty Manifest is not a parsing error.
  EXPECT_EQ(0u, GetErrorCount());

  // Check that the fields are null or set to their default values.
  EXPECT_FALSE(IsManifestEmpty(manifest));
  EXPECT_TRUE(HasDefaultValuesWithUrls(manifest, DefaultDocumentUrl(), KURL()));
  EXPECT_EQ(manifest->dir, mojom::blink::Manifest::TextDirection::kAuto);
  EXPECT_TRUE(manifest->name.IsNull());
  EXPECT_TRUE(manifest->short_name.IsNull());
  EXPECT_EQ(manifest->start_url, DefaultDocumentUrl());
  EXPECT_EQ(manifest->display, blink::mojom::DisplayMode::kUndefined);
  EXPECT_EQ(manifest->orientation,
            device::mojom::ScreenOrientationLockType::DEFAULT);
  EXPECT_FALSE(manifest->has_theme_color);
  EXPECT_FALSE(manifest->has_background_color);
  EXPECT_TRUE(manifest->gcm_sender_id.IsNull());
  EXPECT_EQ(DefaultDocumentUrl().BaseAsString(), manifest->scope.GetString());
  EXPECT_TRUE(manifest->shortcuts.empty());

  // Check that the metrics don't record anything
  EXPECT_THAT(histogram_tester.GetAllSamples("Manifest.HasProperty.name"),
              testing::IsEmpty());
  EXPECT_THAT(histogram_tester.GetAllSamples("Manifest.HasProperty.start_url"),
              testing::IsEmpty());
  EXPECT_THAT(histogram_tester.GetAllSamples("Manifest.HasProperty.short_name"),
              testing::IsEmpty());

  EXPECT_THAT(
      histogram_tester.GetAllSamples("Manifest.HasProperty.description"),
      testing::IsEmpty());
  EXPECT_THAT(histogram_tester.GetAllSamples("Manifest.HasProperty.start_url"),
              testing::IsEmpty());
  EXPECT_THAT(histogram_tester.GetAllSamples("Manifest.HasProperty.display"),
              testing::IsEmpty());
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Manifest.HasProperty.orientation"),
      testing::IsEmpty());
  EXPECT_THAT(histogram_tester.GetAllSamples("Manifest.HasProperty.icons"),
              testing::IsEmpty());
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Manifest.HasProperty.screenshots"),
      testing::IsEmpty());
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Manifest.HasProperty.share_target"),
      testing::IsEmpty());

  EXPECT_THAT(
      histogram_tester.GetAllSamples("Manifest.HasProperty.protocol_handlers"),
      testing::IsEmpty());
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Manifest.HasProperty.gcm_sender_id"),
      testing::IsEmpty());
}

TEST_F(ManifestParserTest, UnrecognizedFieldsIgnored) {
  auto& manifest = ParseManifest(
      R"({
        "unrecognizable_manifest_field": ["foo"],
        "name": "bar"
      })");

  // Unrecognized Manifest fields are not a parsing error.
  EXPECT_EQ(0u, GetErrorCount());

  // Check that subsequent fields parsed.
  EXPECT_FALSE(IsManifestEmpty(manifest));
  EXPECT_FALSE(HasDefaultValuesWithUrls(manifest));
  EXPECT_EQ(manifest->name, "bar");
  EXPECT_EQ(DefaultDocumentUrl().BaseAsString(), manifest->scope.GetString());
}

TEST_F(ManifestParserTest, MultipleErrorsReporting) {
  auto& manifest = ParseManifest(
      R"({ "dir": "foo", "name": 42, "short_name": 4,
      "id": 12, "orientation": {}, "display": "foo",
      "start_url": null, "icons": {}, "theme_color": 42,
      "background_color": 42, "shortcuts": {} })");
  EXPECT_FALSE(IsManifestEmpty(manifest));
  EXPECT_TRUE(HasDefaultValuesWithUrls(manifest));

  EXPECT_THAT(errors(),
              testing::UnorderedElementsAre(
                  "unknown 'dir' value ignored.",
                  "property 'name' ignored, type string expected.",
                  "property 'short_name' ignored, type string expected.",
                  "property 'start_url' ignored, type string expected.",
                  "property 'id' ignored, type string expected.",
                  "unknown 'display' value ignored.",
                  "property 'orientation' ignored, type string expected.",
                  "property 'icons' ignored, type array expected.",
                  "property 'theme_color' ignored, type string expected.",
                  "property 'background_color' ignored, type string expected.",
                  "property 'shortcuts' ignored, type array expected."));
}

TEST_F(ManifestParserTest, DirParseRules) {
  using TextDirection = mojom::blink::Manifest::TextDirection;

  // Smoke test.
  {
    auto& manifest = ParseManifest(R"({ "dir": "ltr" })");
    EXPECT_EQ(manifest->dir, TextDirection::kLTR);
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_FALSE(HasDefaultValuesWithUrls(manifest));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Trim whitespaces.
  {
    auto& manifest = ParseManifest(R"({ "dir": "  rtl  " })");
    EXPECT_EQ(manifest->dir, TextDirection::kRTL);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Don't parse if dir isn't a string.
  {
    auto& manifest = ParseManifest(R"({ "dir": {} })");
    EXPECT_EQ(manifest->dir, TextDirection::kAuto);
    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'dir' ignored, type string expected.", errors()[0]);
  }

  // Don't parse if dir isn't a string.
  {
    auto& manifest = ParseManifest(R"({ "dir": 42 })");
    EXPECT_EQ(manifest->dir, TextDirection::kAuto);
    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'dir' ignored, type string expected.", errors()[0]);
  }

  // Accept 'auto'.
  {
    auto& manifest = ParseManifest(R"({ "dir": "auto" })");
    EXPECT_EQ(manifest->dir, TextDirection::kAuto);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept 'ltr'.
  {
    auto& manifest = ParseManifest(R"({ "dir": "ltr" })");
    EXPECT_EQ(manifest->dir, TextDirection::kLTR);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept 'rtl'.
  {
    auto& manifest = ParseManifest(R"({ "dir": "rtl" })");
    EXPECT_EQ(manifest->dir, TextDirection::kRTL);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Parse fails if string isn't known.
  {
    auto& manifest = ParseManifest(R"({ "dir": "foo" })");
    EXPECT_EQ(manifest->dir, TextDirection::kAuto);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("unknown 'dir' value ignored.", errors()[0]);
  }
}

TEST_F(ManifestParserTest, NameParseRules) {
  // Smoke test.
  {
    auto& manifest = ParseManifest(R"({ "name": "foo" })");
    EXPECT_EQ(manifest->name, "foo");
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_FALSE(HasDefaultValuesWithUrls(manifest));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Trim whitespaces.
  {
    auto& manifest = ParseManifest(R"({ "name": "  foo  " })");
    EXPECT_EQ(manifest->name, "foo");
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Don't parse if name isn't a string.
  {
    auto& manifest = ParseManifest(R"({ "name": {} })");
    EXPECT_TRUE(manifest->name.IsNull());
    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'name' ignored, type string expected.", errors()[0]);
  }

  // Don't parse if name isn't a string.
  {
    auto& manifest = ParseManifest(R"({ "name": 42 })");
    EXPECT_TRUE(manifest->name.IsNull());
    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'name' ignored, type string expected.", errors()[0]);
  }

  // Test stripping out of \t \r and \n.
  {
    auto& manifest = ParseManifest("{ \"name\": \"abc\\t\\r\\ndef\" }");
    EXPECT_EQ(manifest->name, "abcdef");
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(0u, GetErrorCount());
  }
}

TEST_F(ManifestParserTest, DescriptionParseRules) {
  // Smoke test.
  {
    auto& manifest =
        ParseManifest(R"({ "description": "foo is the new black" })");
    EXPECT_EQ(manifest->description, "foo is the new black");
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Trim whitespaces.
  {
    auto& manifest = ParseManifest(R"({ "description": "  foo  " })");
    EXPECT_EQ(manifest->description, "foo");
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Don't parse if description isn't a string.
  {
    auto& manifest = ParseManifest(R"({ "description": {} })");
    EXPECT_TRUE(manifest->description.IsNull());
    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'description' ignored, type string expected.",
              errors()[0]);
  }

  // Don't parse if description isn't a string.
  {
    auto& manifest = ParseManifest(R"({ "description": 42 })");
    EXPECT_TRUE(manifest->description.IsNull());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'description' ignored, type string expected.",
              errors()[0]);
  }
}

TEST_F(ManifestParserTest, ShortNameParseRules) {
  // Smoke test.
  {
    auto& manifest = ParseManifest(R"({ "short_name": "foo" })");
    ASSERT_EQ(manifest->short_name, "foo");
    ASSERT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Trim whitespaces.
  {
    auto& manifest = ParseManifest(R"({ "short_name": "  foo  " })");
    ASSERT_EQ(manifest->short_name, "foo");
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Don't parse if name isn't a string.
  {
    auto& manifest = ParseManifest(R"({ "short_name": {} })");
    ASSERT_TRUE(manifest->short_name.IsNull());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'short_name' ignored, type string expected.",
              errors()[0]);
  }

  // Don't parse if name isn't a string.
  {
    auto& manifest = ParseManifest(R"({ "short_name": 42 })");
    ASSERT_TRUE(manifest->short_name.IsNull());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'short_name' ignored, type string expected.",
              errors()[0]);
  }

  // Test stripping out of \t \r and \n.
  {
    auto& manifest = ParseManifest("{ \"short_name\": \"abc\\t\\r\\ndef\" }");
    ASSERT_EQ(manifest->short_name, "abcdef");
    ASSERT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(0u, GetErrorCount());
  }
}

TEST_F(ManifestParserTest, IdParseRules) {
  // Empty manifest.
  {
    auto& manifest = ParseManifest("{ }");
    ASSERT_TRUE(manifest);
    EXPECT_THAT(errors(), testing::IsEmpty());
    EXPECT_EQ(manifest->id, DefaultDocumentUrl());
    EXPECT_FALSE(manifest->has_custom_id);
  }
  // Does not contain id field.
  {
    auto& manifest = ParseManifest(R"({"start_url": "/start?query=a" })");
    EXPECT_THAT(errors(), testing::IsEmpty());
    EXPECT_EQ("http://foo.com/start?query=a", manifest->id);
    EXPECT_FALSE(manifest->has_custom_id);
  }
  // Invalid type.
  {
    auto& manifest =
        ParseManifest("{\"start_url\": \"/start?query=a\", \"id\": 1}");
    EXPECT_THAT(errors(), testing::ElementsAre(
                              "property 'id' ignored, type string expected."));
    EXPECT_EQ("http://foo.com/start?query=a", manifest->id);
    EXPECT_FALSE(manifest->has_custom_id);
  }
  // Empty string.
  {
    auto& manifest =
        ParseManifest(R"({ "start_url": "/start?query=a", "id": "" })");
    EXPECT_THAT(errors(), testing::IsEmpty());
    EXPECT_EQ("http://foo.com/start?query=a", manifest->id);
    EXPECT_FALSE(manifest->has_custom_id);
  }
  // Full url.
  {
    auto& manifest = ParseManifest(
        "{ \"start_url\": \"/start?query=a\", \"id\": \"http://foo.com/foo\" "
        "}");
    EXPECT_THAT(errors(), testing::IsEmpty());
    EXPECT_EQ("http://foo.com/foo", manifest->id);
    EXPECT_TRUE(manifest->has_custom_id);
  }
  // Full url with different origin.
  {
    auto& manifest = ParseManifest(
        "{ \"start_url\": \"/start?query=a\", \"id\": "
        "\"http://another.com/foo\" }");
    EXPECT_THAT(
        errors(),
        testing::ElementsAre(
            "property 'id' ignored, should be same origin as document."));
    EXPECT_EQ("http://foo.com/start?query=a", manifest->id);
    EXPECT_FALSE(manifest->has_custom_id);
  }
  // Relative path
  {
    auto& manifest =
        ParseManifest("{ \"start_url\": \"/start?query=a\", \"id\": \".\" }");
    EXPECT_THAT(errors(), testing::IsEmpty());
    EXPECT_EQ("http://foo.com/", manifest->id);
    EXPECT_TRUE(manifest->has_custom_id);
  }
  // Absolute path
  {
    auto& manifest =
        ParseManifest("{ \"start_url\": \"/start?query=a\", \"id\": \"/\" }");
    EXPECT_THAT(errors(), testing::IsEmpty());
    EXPECT_EQ("http://foo.com/", manifest->id);
    EXPECT_TRUE(manifest->has_custom_id);
  }
  // url with fragment
  {
    auto& manifest = ParseManifest(
        "{ \"start_url\": \"/start?query=a\", \"id\": \"/#abc\" }");
    EXPECT_THAT(errors(), testing::IsEmpty());
    EXPECT_EQ("http://foo.com/", manifest->id);
    EXPECT_TRUE(manifest->has_custom_id);
  }
  // Smoke test.
  {
    auto& manifest =
        ParseManifest(R"({ "start_url": "/start?query=a", "id": "foo" })");
    EXPECT_THAT(errors(), testing::IsEmpty());
    EXPECT_EQ("http://foo.com/foo", manifest->id);
    EXPECT_TRUE(manifest->has_custom_id);
  }
  // Invalid UTF-8 character.
  {
    UChar invalid_utf8_chars[] = {0xD801, 0x0000};
    String manifest_str =
        String("{ \"start_url\": \"/start?query=a\", \"id\": \"") +
        String(invalid_utf8_chars) + String("\" }");

    auto& manifest = ParseManifest(manifest_str);
    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_THAT(
        errors()[0].Utf8(),
        testing::EndsWith("Unsupported encoding. JSON and all string literals "
                          "must contain valid Unicode characters."));
    ASSERT_TRUE(manifest);
    EXPECT_FALSE(manifest->has_custom_id);
  }
}

TEST_F(ManifestParserTest, StartURLParseRules) {
  // Smoke test.
  {
    base::HistogramTester histogram_tester;
    auto& manifest = ParseManifest(R"({ "start_url": "land.html" })");
    ASSERT_EQ(manifest->start_url, KURL(DefaultDocumentUrl(), "land.html"));
    ASSERT_FALSE(IsManifestEmpty(manifest));
    EXPECT_THAT(errors(), testing::IsEmpty());
    EXPECT_TRUE(manifest->has_valid_specified_start_url);
    EXPECT_FALSE(HasDefaultValuesWithUrls(manifest));
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Manifest.HasProperty.start_url"),
        base::BucketsAre(base::Bucket(1, 1)));
  }

  // Whitespaces.
  {
    auto& manifest = ParseManifest(R"({ "start_url": "  land.html  " })");
    ASSERT_EQ(manifest->start_url, KURL(DefaultDocumentUrl(), "land.html"));
    EXPECT_THAT(errors(), testing::IsEmpty());
    EXPECT_TRUE(manifest->has_valid_specified_start_url);
  }

  // Don't parse if property isn't a string.
  {
    auto& manifest = ParseManifest(R"({ "start_url": {} })");
    EXPECT_EQ(manifest->start_url, DefaultDocumentUrl());
    EXPECT_EQ(DefaultDocumentUrl(), manifest->id);
    EXPECT_THAT(errors(),
                testing::ElementsAre(
                    "property 'start_url' ignored, type string expected."));
    EXPECT_FALSE(manifest->has_valid_specified_start_url);
    EXPECT_TRUE(HasDefaultValuesWithUrls(manifest));
  }

  // Don't parse if property isn't a string.
  {
    auto& manifest = ParseManifest(R"({ "start_url": 42 })");
    EXPECT_EQ(manifest->start_url, DefaultDocumentUrl());
    EXPECT_THAT(errors(),
                testing::ElementsAre(
                    "property 'start_url' ignored, type string expected."));
    EXPECT_FALSE(manifest->has_valid_specified_start_url);
  }

  // Don't parse if property isn't a valid URL.
  {
    auto& manifest =
        ParseManifest(R"({ "start_url": "http://www.google.ca:a" })");
    EXPECT_EQ(manifest->start_url, DefaultDocumentUrl());
    EXPECT_THAT(errors(), testing::ElementsAre(
                              "property 'start_url' ignored, URL is invalid."));
    EXPECT_FALSE(manifest->has_valid_specified_start_url);
  }

  // Absolute start_url, same origin with document.
  {
    auto& manifest =
        ParseManifestWithURLs(R"({ "start_url": "http://foo.com/land.html" })",
                              KURL("http://foo.com/manifest.json"),
                              KURL("http://foo.com/index.html"));
    EXPECT_EQ(manifest->start_url.GetString(), "http://foo.com/land.html");
    EXPECT_THAT(errors(), testing::IsEmpty());
    EXPECT_TRUE(manifest->has_valid_specified_start_url);
  }

  // Absolute start_url, cross origin with document.
  {
    auto& manifest =
        ParseManifestWithURLs(R"({ "start_url": "http://bar.com/land.html" })",
                              KURL("http://foo.com/manifest.json"),
                              KURL("http://foo.com/index.html"));
    EXPECT_EQ(manifest->start_url, DefaultDocumentUrl());
    EXPECT_THAT(errors(),
                testing::ElementsAre("property 'start_url' ignored, should "
                                     "be same origin as document."));
    EXPECT_FALSE(manifest->has_valid_specified_start_url);
  }

  // Resolving has to happen based on the manifest_url.
  {
    auto& manifest =
        ParseManifestWithURLs(R"({ "start_url": "land.html" })",
                              KURL("http://foo.com/landing/manifest.json"),
                              KURL("http://foo.com/index.html"));
    EXPECT_EQ(manifest->start_url.GetString(),
              "http://foo.com/landing/land.html");
    EXPECT_THAT(errors(), testing::IsEmpty());
    EXPECT_TRUE(manifest->has_valid_specified_start_url);
  }
}

TEST_F(ManifestParserTest, ScopeParseRules) {
  // Smoke test.
  {
    auto& manifest = ParseManifest(
        R"({ "scope": "land", "start_url": "land/landing.html" })");
    ASSERT_EQ(manifest->scope, KURL(DefaultDocumentUrl(), "land"));
    ASSERT_FALSE(IsManifestEmpty(manifest));
    EXPECT_THAT(errors(), testing::IsEmpty());
  }

  // Whitespaces.
  {
    auto& manifest = ParseManifest(
        R"({ "scope": "  land  ", "start_url": "land/landing.html" })");
    ASSERT_EQ(manifest->scope, KURL(DefaultDocumentUrl(), "land"));
    EXPECT_THAT(errors(), testing::IsEmpty());
  }

  // Return the default value if the property isn't a string.
  {
    auto& manifest = ParseManifest(R"({ "scope": {} })");
    ASSERT_EQ(manifest->scope.GetString(), DefaultDocumentUrl().BaseAsString());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'scope' ignored, type string expected.", errors()[0]);
  }

  // Return the default value if property isn't a string.
  {
    auto& manifest = ParseManifest(
        R"({ "scope": 42,
        "start_url": "http://foo.com/land/landing.html" })");
    ASSERT_EQ(manifest->scope, KURL(DefaultDocumentUrl(), "land/"));
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'scope' ignored, type string expected.", errors()[0]);
  }

  // Absolute scope, start URL is in scope.
  {
    auto& manifest = ParseManifestWithURLs(
        R"({ "scope": "http://foo.com/land",
        "start_url": "http://foo.com/land/landing.html" })",
        KURL("http://foo.com/manifest.json"),
        KURL("http://foo.com/index.html"));
    ASSERT_EQ(manifest->scope.GetString(), "http://foo.com/land");
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Absolute scope, start URL is not in scope.
  {
    auto& manifest = ParseManifestWithURLs(
        R"({ "scope": "http://foo.com/land",
        "start_url": "http://foo.com/index.html" })",
        KURL("http://foo.com/manifest.json"),
        KURL("http://foo.com/index.html"));
    ASSERT_EQ(manifest->scope.GetString(), DefaultDocumentUrl().BaseAsString());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'scope' ignored. Start url should be within scope "
        "of scope URL.",
        errors()[0]);
  }

  // Absolute scope, start URL has different origin than scope URL.
  {
    auto& manifest = ParseManifestWithURLs(
        R"({ "scope": "http://foo.com/land",
        "start_url": "http://bar.com/land/landing.html" })",
        KURL("http://foo.com/manifest.json"),
        KURL("http://foo.com/index.html"));
    ASSERT_EQ(manifest->scope.GetString(), DefaultDocumentUrl().BaseAsString());
    ASSERT_EQ(2u, GetErrorCount());
    EXPECT_EQ(
        "property 'start_url' ignored, should be same origin as document.",
        errors()[0]);
    EXPECT_EQ(
        "property 'scope' ignored. Start url should be within scope "
        "of scope URL.",
        errors()[1]);
  }

  // scope and start URL have diferent origin than document URL.
  {
    KURL document_url("http://bar.com/index.html");
    auto& manifest = ParseManifestWithURLs(
        R"({ "scope": "http://foo.com/land",
        "start_url": "http://foo.com/land/landing.html" })",
        KURL("http://foo.com/manifest.json"), document_url);
    ASSERT_EQ(manifest->scope.GetString(), document_url.BaseAsString());
    ASSERT_EQ(2u, GetErrorCount());
    EXPECT_EQ(
        "property 'start_url' ignored, should be same origin as document.",
        errors()[0]);
    EXPECT_EQ(
        "property 'scope' ignored. Start url should be within scope "
        "of scope URL.",
        errors()[1]);
  }

  // No start URL. Document URL is in a subdirectory of scope.
  {
    auto& manifest =
        ParseManifestWithURLs(R"({ "scope": "http://foo.com/land" })",
                              KURL("http://foo.com/manifest.json"),
                              KURL("http://foo.com/land/site/index.html"));
    ASSERT_EQ(manifest->scope.GetString(), "http://foo.com/land");
    ASSERT_EQ(0u, GetErrorCount());
  }

  // No start URL. Document is out of scope.
  {
    KURL document_url("http://foo.com/index.html");
    auto& manifest =
        ParseManifestWithURLs(R"({ "scope": "http://foo.com/land" })",
                              KURL("http://foo.com/manifest.json"),
                              KURL("http://foo.com/index.html"));
    ASSERT_EQ(manifest->scope.GetString(), document_url.BaseAsString());
    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'scope' ignored. Start url should be within scope "
        "of scope URL.",
        errors()[0]);
  }

  // Resolving has to happen based on the manifest_url.
  {
    auto& manifest = ParseManifestWithURLs(
        R"({ "scope": "treasure" })", KURL("http://foo.com/map/manifest.json"),
        KURL("http://foo.com/map/treasure/island/index.html"));
    ASSERT_EQ(manifest->scope.GetString(), "http://foo.com/map/treasure");
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Scope is parent directory.
  {
    auto& manifest = ParseManifestWithURLs(
        R"({ "scope": ".." })", KURL("http://foo.com/map/manifest.json"),
        KURL("http://foo.com/index.html"));
    ASSERT_EQ(manifest->scope.GetString(), "http://foo.com/");
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Scope tries to go up past domain.
  {
    auto& manifest = ParseManifestWithURLs(
        R"({ "scope": "../.." })", KURL("http://foo.com/map/manifest.json"),
        KURL("http://foo.com/index.html"));
    ASSERT_EQ(manifest->scope.GetString(), "http://foo.com/");
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Scope removes query args.
  {
    auto& manifest = ParseManifest(
        R"({ "start_url": "app/index.html",
             "scope": "/?test=abc" })");
    ASSERT_EQ(manifest->scope.GetString(), "http://foo.com/");
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Scope removes fragments.
  {
    auto& manifest = ParseManifest(
        R"({ "start_url": "app/index.html",
             "scope": "/#abc" })");
    ASSERT_EQ(manifest->scope.GetString(), "http://foo.com/");
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Scope defaults to start_url with the filename, query, and fragment removed.
  {
    auto& manifest =
        ParseManifest(R"({ "start_url": "land/landing.html?query=test#abc" })");
    ASSERT_EQ(manifest->scope, KURL(DefaultDocumentUrl(), "land/"));
    EXPECT_EQ(0u, GetErrorCount());
  }

  {
    auto& manifest =
        ParseManifest(R"({ "start_url": "land/land/landing.html" })");
    ASSERT_EQ(manifest->scope, KURL(DefaultDocumentUrl(), "land/land/"));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Scope defaults to document_url if start_url is not present.
  {
    auto& manifest = ParseManifest("{}");
    ASSERT_EQ(manifest->scope, KURL(DefaultDocumentUrl(), "."));
    EXPECT_EQ(0u, GetErrorCount());
  }
}

TEST_F(ManifestParserTest, DisplayParseRules) {
  // Smoke test.
  {
    auto& manifest = ParseManifest(R"({ "display": "browser" })");
    EXPECT_EQ(manifest->display, blink::mojom::DisplayMode::kBrowser);
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Trim whitespaces.
  {
    auto& manifest = ParseManifest(R"({ "display": "  browser  " })");
    EXPECT_EQ(manifest->display, blink::mojom::DisplayMode::kBrowser);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Don't parse if name isn't a string.
  {
    auto& manifest = ParseManifest(R"({ "display": {} })");
    EXPECT_EQ(manifest->display, blink::mojom::DisplayMode::kUndefined);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'display' ignored,"
        " type string expected.",
        errors()[0]);
  }

  // Don't parse if name isn't a string.
  {
    auto& manifest = ParseManifest(R"({ "display": 42 })");
    EXPECT_EQ(manifest->display, blink::mojom::DisplayMode::kUndefined);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'display' ignored,"
        " type string expected.",
        errors()[0]);
  }

  // Parse fails if string isn't known.
  {
    auto& manifest = ParseManifest(R"({ "display": "browser_something" })");
    EXPECT_EQ(manifest->display, blink::mojom::DisplayMode::kUndefined);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("unknown 'display' value ignored.", errors()[0]);
  }

  // Accept 'fullscreen'.
  {
    auto& manifest = ParseManifest(R"({ "display": "fullscreen" })");
    EXPECT_EQ(manifest->display, blink::mojom::DisplayMode::kFullscreen);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept 'standalone'.
  {
    auto& manifest = ParseManifest(R"({ "display": "standalone" })");
    EXPECT_EQ(manifest->display, blink::mojom::DisplayMode::kStandalone);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept 'minimal-ui'.
  {
    auto& manifest = ParseManifest(R"({ "display": "minimal-ui" })");
    EXPECT_EQ(manifest->display, blink::mojom::DisplayMode::kMinimalUi);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept 'browser'.
  {
    auto& manifest = ParseManifest(R"({ "display": "browser" })");
    EXPECT_EQ(manifest->display, blink::mojom::DisplayMode::kBrowser);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Case insensitive.
  {
    auto& manifest = ParseManifest(R"({ "display": "BROWSER" })");
    EXPECT_EQ(manifest->display, blink::mojom::DisplayMode::kBrowser);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Do not accept 'window-controls-overlay' as a display mode.
  {
    auto& manifest =
        ParseManifest(R"({ "display": "window-controls-overlay" })");
    EXPECT_EQ(manifest->display, blink::mojom::DisplayMode::kUndefined);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("inapplicable 'display' value ignored.", errors()[0]);
  }

  // Parsing fails for 'borderless' when Borderless flag is disabled.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(blink::features::kWebAppBorderless);
    auto& manifest = ParseManifest(R"({ "display": "borderless" })");
    EXPECT_EQ(manifest->display, blink::mojom::DisplayMode::kUndefined);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("inapplicable 'display' value ignored.", errors()[0]);
  }

  // Parsing fails for 'borderless' when Borderless flag is enabled.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(blink::features::kWebAppBorderless);
    auto& manifest = ParseManifest(R"({ "display": "borderless" })");
    EXPECT_EQ(manifest->display, blink::mojom::DisplayMode::kUndefined);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("inapplicable 'display' value ignored.", errors()[0]);
  }

  // Parsing fails for 'tabbed' when flag is disabled.
  {
    ScopedWebAppTabStripForTest tabbed(false);
    auto& manifest = ParseManifest(R"({ "display": "tabbed" })");
    EXPECT_EQ(manifest->display, blink::mojom::DisplayMode::kUndefined);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("inapplicable 'display' value ignored.", errors()[0]);
  }

  // Parsing fails for 'tabbed' when flag is enabled.
  {
    ScopedWebAppTabStripForTest tabbed(true);
    auto& manifest = ParseManifest(R"({ "display": "tabbed" })");
    EXPECT_EQ(manifest->display, blink::mojom::DisplayMode::kUndefined);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("inapplicable 'display' value ignored.", errors()[0]);
  }
}

TEST_F(ManifestParserTest, DisplayOverrideParseRules) {

  // Smoke test: if no display_override, no value.
  {
    auto& manifest = ParseManifest(R"({ "display_override": [] })");
    EXPECT_TRUE(manifest->display_override.empty());
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Smoke test: if not array, value will be ignored
  {
    auto& manifest = ParseManifest(R"({ "display_override": 23 })");
    EXPECT_TRUE(manifest->display_override.empty());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'display_override' ignored, type array expected.",
              errors()[0]);
  }

  // Smoke test: if array value is not a string, it will be ignored
  {
    auto& manifest = ParseManifest(R"({ "display_override": [ 23 ] })");
    EXPECT_TRUE(manifest->display_override.empty());
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Smoke test: if array value is not not recognized, it will be ignored
  {
    auto& manifest = ParseManifest(R"({ "display_override": [ "test" ] })");
    EXPECT_TRUE(manifest->display_override.empty());
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Case insensitive
  {
    auto& manifest = ParseManifest(R"({ "display_override": [ "BROWSER" ] })");
    EXPECT_FALSE(manifest->display_override.empty());
    EXPECT_EQ(manifest->display_override[0],
              blink::mojom::DisplayMode::kBrowser);
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Trim whitespace
  {
    auto& manifest =
        ParseManifest(R"({ "display_override": [ " browser " ] })");
    EXPECT_FALSE(manifest->display_override.empty());
    EXPECT_EQ(manifest->display_override[0],
              blink::mojom::DisplayMode::kBrowser);
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept 'browser'
  {
    auto& manifest = ParseManifest(R"({ "display_override": [ "browser" ] })");
    EXPECT_FALSE(manifest->display_override.empty());
    EXPECT_EQ(manifest->display_override[0],
              blink::mojom::DisplayMode::kBrowser);
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept 'browser', 'minimal-ui'
  {
    auto& manifest =
        ParseManifest(R"({ "display_override": [ "browser", "minimal-ui" ] })");
    EXPECT_FALSE(manifest->display_override.empty());
    EXPECT_EQ(manifest->display_override[0],
              blink::mojom::DisplayMode::kBrowser);
    EXPECT_EQ(manifest->display_override[1],
              blink::mojom::DisplayMode::kMinimalUi);
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // if array value is not not recognized, it will be ignored
  // Accept 'browser', 'minimal-ui'
  {
    auto& manifest = ParseManifest(
        R"({ "display_override": [ 3, "browser", "invalid-display",
        "minimal-ui" ] })");
    EXPECT_FALSE(manifest->display_override.empty());
    EXPECT_EQ(manifest->display_override[0],
              blink::mojom::DisplayMode::kBrowser);
    EXPECT_EQ(manifest->display_override[1],
              blink::mojom::DisplayMode::kMinimalUi);
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // validate both display and display-override fields are parsed
  // if array value is not not recognized, it will be ignored
  // Accept 'browser', 'minimal-ui', 'standalone'
  {
    auto& manifest = ParseManifest(
        R"({ "display": "standalone", "display_override": [ "browser",
        "minimal-ui", "standalone" ] })");
    EXPECT_EQ(manifest->display, blink::mojom::DisplayMode::kStandalone);
    EXPECT_EQ(0u, GetErrorCount());
    EXPECT_FALSE(manifest->display_override.empty());
    EXPECT_EQ(manifest->display_override[0],
              blink::mojom::DisplayMode::kBrowser);
    EXPECT_EQ(manifest->display_override[1],
              blink::mojom::DisplayMode::kMinimalUi);
    EXPECT_EQ(manifest->display_override[2],
              blink::mojom::DisplayMode::kStandalone);
    EXPECT_FALSE(IsManifestEmpty(manifest));
  }

  // validate duplicate entries.
  // Accept 'browser', 'minimal-ui', 'browser'
  {
    auto& manifest =
        ParseManifest(R"({ "display_override": [ "browser", "minimal-ui",
        "browser" ] })");
    EXPECT_FALSE(manifest->display_override.empty());
    EXPECT_EQ(manifest->display_override[0],
              blink::mojom::DisplayMode::kBrowser);
    EXPECT_EQ(manifest->display_override[1],
              blink::mojom::DisplayMode::kMinimalUi);
    EXPECT_EQ(manifest->display_override[2],
              blink::mojom::DisplayMode::kBrowser);
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept 'window-controls-overlay'.
  {
    auto& manifest = ParseManifest(
        R"({ "display_override": [ "window-controls-overlay" ] })");
    EXPECT_FALSE(manifest->display_override.empty());
    EXPECT_EQ(manifest->display_override[0],
              blink::mojom::DisplayMode::kWindowControlsOverlay);
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Reject 'borderless' when Borderless flag is disabled.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(blink::features::kWebAppBorderless);
    auto& manifest =
        ParseManifest(R"({ "display_override": [ "borderless" ] })");
    EXPECT_TRUE(manifest->display_override.empty());
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept 'borderless' when Borderless flag is enabled.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(blink::features::kWebAppBorderless);
    auto& manifest =
        ParseManifest(R"({ "display_override": [ "borderless" ] })");
    EXPECT_FALSE(manifest->display_override.empty());
    EXPECT_EQ(manifest->display_override[0],
              blink::mojom::DisplayMode::kBorderless);
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Ignore 'tabbed' when flag is disabled.
  {
    ScopedWebAppTabStripForTest tabbed(false);
    auto& manifest = ParseManifest(R"({ "display_override": [ "tabbed" ] })");
    EXPECT_TRUE(manifest->display_override.empty());
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept 'tabbed' when flag is enabled.
  {
    ScopedWebAppTabStripForTest tabbed(true);
    auto& manifest = ParseManifest(R"({ "display_override": [ "tabbed" ] })");
    EXPECT_FALSE(manifest->display_override.empty());
    EXPECT_EQ(manifest->display_override[0],
              blink::mojom::DisplayMode::kTabbed);
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(0u, GetErrorCount());
  }
}

TEST_F(ManifestParserTest, OrientationParseRules) {
  // Smoke test.
  {
    auto& manifest = ParseManifest(R"({ "orientation": "natural" })");
    EXPECT_EQ(manifest->orientation,
              device::mojom::ScreenOrientationLockType::NATURAL);
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Trim whitespaces.
  {
    auto& manifest = ParseManifest(R"({ "orientation": "natural" })");
    EXPECT_EQ(manifest->orientation,
              device::mojom::ScreenOrientationLockType::NATURAL);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Don't parse if name isn't a string.
  {
    auto& manifest = ParseManifest(R"({ "orientation": {} })");
    EXPECT_EQ(manifest->orientation,
              device::mojom::ScreenOrientationLockType::DEFAULT);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'orientation' ignored, type string expected.",
              errors()[0]);
  }

  // Don't parse if name isn't a string.
  {
    auto& manifest = ParseManifest(R"({ "orientation": 42 })");
    EXPECT_EQ(manifest->orientation,
              device::mojom::ScreenOrientationLockType::DEFAULT);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'orientation' ignored, type string expected.",
              errors()[0]);
  }

  // Parse fails if string isn't known.
  {
    auto& manifest = ParseManifest(R"({ "orientation": "naturalish" })");
    EXPECT_EQ(manifest->orientation,
              device::mojom::ScreenOrientationLockType::DEFAULT);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("unknown 'orientation' value ignored.", errors()[0]);
  }

  // Accept 'any'.
  {
    auto& manifest = ParseManifest(R"({ "orientation": "any" })");
    EXPECT_EQ(manifest->orientation,
              device::mojom::ScreenOrientationLockType::ANY);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept 'natural'.
  {
    auto& manifest = ParseManifest(R"({ "orientation": "natural" })");
    EXPECT_EQ(manifest->orientation,
              device::mojom::ScreenOrientationLockType::NATURAL);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept 'landscape'.
  {
    auto& manifest = ParseManifest(R"({ "orientation": "landscape" })");
    EXPECT_EQ(manifest->orientation,
              device::mojom::ScreenOrientationLockType::LANDSCAPE);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept 'landscape-primary'.
  {
    auto& manifest = ParseManifest(R"({ "orientation": "landscape-primary" })");
    EXPECT_EQ(manifest->orientation,
              device::mojom::ScreenOrientationLockType::LANDSCAPE_PRIMARY);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept 'landscape-secondary'.
  {
    auto& manifest =
        ParseManifest(R"({ "orientation": "landscape-secondary" })");
    EXPECT_EQ(manifest->orientation,
              device::mojom::ScreenOrientationLockType::LANDSCAPE_SECONDARY);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept 'portrait'.
  {
    auto& manifest = ParseManifest(R"({ "orientation": "portrait" })");
    EXPECT_EQ(manifest->orientation,
              device::mojom::ScreenOrientationLockType::PORTRAIT);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept 'portrait-primary'.
  {
    auto& manifest = ParseManifest(R"({ "orientation": "portrait-primary" })");
    EXPECT_EQ(manifest->orientation,
              device::mojom::ScreenOrientationLockType::PORTRAIT_PRIMARY);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept 'portrait-secondary'.
  {
    auto& manifest =
        ParseManifest(R"({ "orientation": "portrait-secondary" })");
    EXPECT_EQ(manifest->orientation,
              device::mojom::ScreenOrientationLockType::PORTRAIT_SECONDARY);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Case insensitive.
  {
    auto& manifest = ParseManifest(R"({ "orientation": "LANDSCAPE" })");
    EXPECT_EQ(manifest->orientation,
              device::mojom::ScreenOrientationLockType::LANDSCAPE);
    EXPECT_EQ(0u, GetErrorCount());
  }
}

TEST_F(ManifestParserTest, IconsParseRules) {
  // Smoke test: if no icon, no value.
  {
    auto& manifest = ParseManifest(R"({ "icons": [] })");
    EXPECT_TRUE(manifest->icons.empty());
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Smoke test: if empty icon, no value.
  {
    auto& manifest = ParseManifest(R"({ "icons": [ {} ] })");
    EXPECT_TRUE(manifest->icons.empty());
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Smoke test: icon with invalid src, no value.
  {
    auto& manifest = ParseManifest(R"({ "icons": [ { "icons": [] } ] })");
    EXPECT_TRUE(manifest->icons.empty());
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Smoke test: if icon with empty src, it will be present in the list.
  {
    auto& manifest = ParseManifest(R"({ "icons": [ { "src": "" } ] })");
    EXPECT_FALSE(manifest->icons.empty());

    auto& icons = manifest->icons;
    EXPECT_EQ(icons.size(), 1u);
    EXPECT_EQ(icons[0]->src.GetString(), "http://foo.com/manifest.json");
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Smoke test: if one icons with valid src, it will be present in the list.
  {
    auto& manifest = ParseManifest(R"({ "icons": [{ "src": "foo.jpg" }] })");
    EXPECT_FALSE(manifest->icons.empty());

    auto& icons = manifest->icons;
    EXPECT_EQ(icons.size(), 1u);
    EXPECT_EQ(icons[0]->src.GetString(), "http://foo.com/foo.jpg");
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Smoke test.
  {
    auto& manifest = ParseManifest(R"(
          {
            "icons": [
              {
                "src": "foo.webp",
                "type": "image/webp",
                "sizes": "192x192"
              },
              {
                "src": "foo.svg",
                "type": "image/svg+xml",
                "sizes": "144x144"
              }
            ]
          }
        )");
    ASSERT_EQ(manifest->icons.size(), 2u);
    EXPECT_EQ(manifest->icons[0]->src, KURL(DefaultDocumentUrl(), "foo.webp"));
    EXPECT_EQ(manifest->icons[0]->type, "image/webp");
    EXPECT_EQ(manifest->icons[0]->sizes.size(), 1u);
    EXPECT_EQ(manifest->icons[0]->sizes[0].width(), 192);
    EXPECT_EQ(manifest->icons[0]->sizes[0].height(), 192);
    EXPECT_EQ(manifest->icons[1]->src, KURL(DefaultDocumentUrl(), "foo.svg"));
    EXPECT_EQ(manifest->icons[1]->type, "image/svg+xml");
    EXPECT_EQ(manifest->icons[1]->sizes.size(), 1u);
    EXPECT_EQ(manifest->icons[1]->sizes[0].width(), 144);
    EXPECT_EQ(manifest->icons[1]->sizes[0].height(), 144);
    EXPECT_EQ(0u, GetErrorCount());
  }
}

TEST_F(ManifestParserTest, ScreenshotsParseRules) {
  // Smoke test: if no screenshot, no value.
  {
    auto& manifest = ParseManifest(R"({ "screenshots": [] })");
    EXPECT_TRUE(manifest->screenshots.empty());
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Smoke test: if empty screenshot, no value.
  {
    auto& manifest = ParseManifest(R"({ "screenshots": [ {} ] })");
    EXPECT_TRUE(manifest->screenshots.empty());
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Smoke test: screenshot with invalid src, no value.
  {
    auto& manifest =
        ParseManifest(R"({ "screenshots": [ { "screenshots": [] } ] })");
    EXPECT_TRUE(manifest->screenshots.empty());
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Smoke test: if screenshot with empty src, it will be present in the list.
  {
    auto& manifest = ParseManifest(R"({ "screenshots": [ { "src": "" } ] })");
    EXPECT_FALSE(manifest->screenshots.empty());

    auto& screenshots = manifest->screenshots;
    EXPECT_EQ(screenshots.size(), 1u);
    EXPECT_EQ(screenshots[0]->image->src.GetString(),
              "http://foo.com/manifest.json");
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Smoke test: if one icons has valid src, it will be present in the list.
  {
    auto& manifest =
        ParseManifest(R"({ "screenshots": [{ "src": "foo.jpg" }] })");
    EXPECT_FALSE(manifest->screenshots.empty());

    auto& screenshots = manifest->screenshots;
    EXPECT_EQ(screenshots.size(), 1u);
    EXPECT_EQ(screenshots[0]->image->src.GetString(), "http://foo.com/foo.jpg");
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(0u, GetErrorCount());
  }
}

TEST_F(ManifestParserTest, ScreenshotFormFactorParseRules) {
  // Smoke test.
  {
    auto& manifest = ParseManifest(
        R"({ "screenshots": [{ "src": "foo.jpg", "form_factor": "narrow" }] })");
    EXPECT_FALSE(manifest->screenshots.empty());

    auto& screenshots = manifest->screenshots;
    EXPECT_EQ(screenshots.size(), 1u);
    EXPECT_EQ(screenshots[0]->form_factor,
              mojom::blink::ManifestScreenshot::FormFactor::kNarrow);
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Unspecified.
  {
    auto& manifest =
        ParseManifest(R"({ "screenshots": [{ "src": "foo.jpg"}] })");
    EXPECT_FALSE(manifest->screenshots.empty());

    auto& screenshots = manifest->screenshots;
    EXPECT_EQ(screenshots.size(), 1u);
    EXPECT_EQ(screenshots[0]->form_factor,
              mojom::blink::ManifestScreenshot::FormFactor::kUnknown);
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Invalid type.
  {
    auto& manifest = ParseManifest(
        R"({ "screenshots": [{ "src": "foo.jpg", "form_factor": 1}] })");
    EXPECT_FALSE(manifest->screenshots.empty());

    auto& screenshots = manifest->screenshots;
    EXPECT_EQ(screenshots.size(), 1u);
    EXPECT_EQ(screenshots[0]->form_factor,
              mojom::blink::ManifestScreenshot::FormFactor::kUnknown);
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(1u, GetErrorCount());
  }

  // Unrecognized string.
  {
    auto& manifest = ParseManifest(
        R"({ "screenshots": [{ "src": "foo.jpg", "form_factor": "windows"}] })");
    EXPECT_FALSE(manifest->screenshots.empty());

    auto& screenshots = manifest->screenshots;
    EXPECT_EQ(screenshots.size(), 1u);
    EXPECT_EQ(screenshots[0]->form_factor,
              mojom::blink::ManifestScreenshot::FormFactor::kUnknown);
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(1u, GetErrorCount());
  }
}

TEST_F(ManifestParserTest, ScreenshotLabelRules) {
  // Smoke test.
  {
    auto& manifest = ParseManifest(
        R"({ "screenshots": [{ "src": "foo.jpg", "label": "example screenshot." }] })");
    EXPECT_FALSE(manifest->screenshots.empty());

    auto& screenshots = manifest->screenshots;
    EXPECT_EQ(screenshots.size(), 1u);
    EXPECT_EQ(screenshots[0]->label, "example screenshot.");
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(0u, GetErrorCount());
  }
  // Unspecified.
  {
    auto& manifest =
        ParseManifest(R"({ "screenshots": [{ "src": "foo.jpg"}] })");
    EXPECT_FALSE(manifest->screenshots.empty());

    auto& screenshots = manifest->screenshots;
    EXPECT_EQ(screenshots.size(), 1u);
    EXPECT_TRUE(screenshots[0]->label.IsNull());
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(0u, GetErrorCount());
  }
  // Empty string.
  {
    auto& manifest = ParseManifest(
        R"({ "screenshots": [{ "src": "foo.jpg", "label": "" }] })");
    EXPECT_FALSE(manifest->screenshots.empty());

    auto& screenshots = manifest->screenshots;
    EXPECT_EQ(screenshots.size(), 1u);
    EXPECT_EQ(screenshots[0]->label, "");
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(0u, GetErrorCount());
  }
  // Invalid type.
  {
    auto& manifest = ParseManifest(
        R"({ "screenshots": [{ "src": "foo.jpg", "label": 2 }] })");
    EXPECT_FALSE(manifest->screenshots.empty());

    auto& screenshots = manifest->screenshots;
    EXPECT_EQ(screenshots.size(), 1u);
    EXPECT_TRUE(screenshots[0]->label.IsNull());
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(1u, GetErrorCount());
  }
}

TEST_F(ManifestParserTest, IconSrcParseRules) {
  // Smoke test.
  {
    auto& manifest = ParseManifest(R"({ "icons": [ {"src": "foo.png" } ] })");
    EXPECT_FALSE(manifest->icons.empty());
    EXPECT_EQ(manifest->icons[0]->src, KURL(DefaultDocumentUrl(), "foo.png"));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Whitespaces.
  {
    auto& manifest =
        ParseManifest(R"({ "icons": [ {"src": "   foo.png   " } ] })");
    EXPECT_FALSE(manifest->icons.empty());
    EXPECT_EQ(manifest->icons[0]->src, KURL(DefaultDocumentUrl(), "foo.png"));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Don't parse if property isn't a string.
  {
    auto& manifest = ParseManifest(R"({ "icons": [ {"src": {} } ] })");
    EXPECT_TRUE(manifest->icons.empty());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'src' ignored, type string expected.", errors()[0]);
  }

  // Don't parse if property isn't a string.
  {
    auto& manifest = ParseManifest(R"({ "icons": [ {"src": 42 } ] })");
    EXPECT_TRUE(manifest->icons.empty());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'src' ignored, type string expected.", errors()[0]);
  }

  // Resolving has to happen based on the document_url.
  {
    auto& manifest = ParseManifestWithURLs(
        R"({ "icons": [ {"src": "icons/foo.png" } ] })",
        KURL("http://foo.com/landing/index.html"), DefaultManifestUrl());
    EXPECT_FALSE(manifest->icons.empty());
    EXPECT_EQ(manifest->icons[0]->src.GetString(),
              "http://foo.com/landing/icons/foo.png");
    EXPECT_EQ(0u, GetErrorCount());
  }
}

TEST_F(ManifestParserTest, IconTypeParseRules) {
  // Smoke test.
  {
    auto& manifest =
        ParseManifest(R"({ "icons": [ {"src": "", "type": "foo" } ] })");
    EXPECT_FALSE(manifest->icons.empty());
    EXPECT_EQ(manifest->icons[0]->type, "foo");
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Trim whitespaces.
  {
    auto& manifest =
        ParseManifest(R"({ "icons": [ {"src": "", "type": "  foo  " } ] })");
    EXPECT_FALSE(manifest->icons.empty());
    EXPECT_EQ(manifest->icons[0]->type, "foo");
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Don't parse if property isn't a string.
  {
    auto& manifest =
        ParseManifest(R"({ "icons": [ {"src": "", "type": {} } ] })");
    EXPECT_FALSE(manifest->icons.empty());
    EXPECT_TRUE(manifest->icons[0]->type.empty());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'type' ignored, type string expected.", errors()[0]);
  }

  // Don't parse if property isn't a string.
  {
    auto& manifest =
        ParseManifest(R"({ "icons": [ {"src": "", "type": 42 } ] })");
    EXPECT_FALSE(manifest->icons.empty());
    EXPECT_TRUE(manifest->icons[0]->type.empty());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'type' ignored, type string expected.", errors()[0]);
  }
}

TEST_F(ManifestParserTest, IconSizesParseRules) {
  // Smoke test.
  {
    auto& manifest =
        ParseManifest(R"({ "icons": [ {"src": "", "sizes": "42x42" } ] })");
    EXPECT_FALSE(manifest->icons.empty());
    EXPECT_EQ(manifest->icons[0]->sizes.size(), 1u);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Trim whitespaces.
  {
    auto& manifest =
        ParseManifest(R"({ "icons": [ {"src": "", "sizes": "  42x42  " } ] })");
    EXPECT_FALSE(manifest->icons.empty());
    EXPECT_EQ(manifest->icons[0]->sizes.size(), 1u);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Ignore sizes if property isn't a string.
  {
    auto& manifest =
        ParseManifest(R"({ "icons": [ {"src": "", "sizes": {} } ] })");
    EXPECT_FALSE(manifest->icons.empty());
    EXPECT_EQ(manifest->icons[0]->sizes.size(), 0u);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'sizes' ignored, type string expected.", errors()[0]);
  }

  // Ignore sizes if property isn't a string.
  {
    auto& manifest =
        ParseManifest(R"({ "icons": [ {"src": "", "sizes": 42 } ] })");
    EXPECT_FALSE(manifest->icons.empty());
    EXPECT_EQ(manifest->icons[0]->sizes.size(), 0u);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'sizes' ignored, type string expected.", errors()[0]);
  }

  // Smoke test: value correctly parsed.
  {
    auto& manifest = ParseManifest(
        R"({ "icons": [ {"src": "", "sizes": "42x42  48x48" } ] })");
    EXPECT_FALSE(manifest->icons.empty());

    auto& icons = manifest->icons;
    EXPECT_EQ(icons[0]->sizes[0], gfx::Size(42, 42));
    EXPECT_EQ(icons[0]->sizes[1], gfx::Size(48, 48));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // <WIDTH>'x'<HEIGHT> and <WIDTH>'X'<HEIGHT> are equivalent.
  {
    auto& manifest = ParseManifest(
        R"({ "icons": [ {"src": "", "sizes": "42X42  48X48" } ] })");
    EXPECT_FALSE(manifest->icons.empty());

    auto& icons = manifest->icons;
    EXPECT_EQ(icons[0]->sizes[0], gfx::Size(42, 42));
    EXPECT_EQ(icons[0]->sizes[1], gfx::Size(48, 48));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Twice the same value is parsed twice.
  {
    auto& manifest = ParseManifest(
        R"({ "icons": [ {"src": "", "sizes": "42X42  42x42" } ] })");
    EXPECT_FALSE(manifest->icons.empty());

    auto& icons = manifest->icons;
    EXPECT_EQ(icons[0]->sizes[0], gfx::Size(42, 42));
    EXPECT_EQ(icons[0]->sizes[1], gfx::Size(42, 42));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Width or height can't start with 0.
  {
    auto& manifest = ParseManifest(
        R"({ "icons": [ {"src": "", "sizes": "004X007  042x00" } ] })");
    EXPECT_FALSE(manifest->icons.empty());
    EXPECT_EQ(manifest->icons[0]->sizes.size(), 0u);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("found icon with no valid size.", errors()[0]);
  }

  // Width and height MUST contain digits.
  {
    auto& manifest = ParseManifest(
        R"({ "icons": [ {"src": "", "sizes": "e4X1.0  55ax1e10" } ] })");
    EXPECT_FALSE(manifest->icons.empty());
    EXPECT_EQ(manifest->icons[0]->sizes.size(), 0u);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("found icon with no valid size.", errors()[0]);
  }

  // 'any' is correctly parsed and transformed to gfx::Size(0,0).
  {
    auto& manifest = ParseManifest(
        R"({ "icons": [ {"src": "", "sizes": "any AnY ANY aNy" } ] })");
    gfx::Size any = gfx::Size(0, 0);
    EXPECT_FALSE(manifest->icons.empty());

    auto& icons = manifest->icons;
    EXPECT_EQ(icons[0]->sizes.size(), 4u);
    EXPECT_EQ(icons[0]->sizes[0], any);
    EXPECT_EQ(icons[0]->sizes[1], any);
    EXPECT_EQ(icons[0]->sizes[2], any);
    EXPECT_EQ(icons[0]->sizes[3], any);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Some invalid width/height combinations.
  {
    auto& manifest = ParseManifest(
        R"({ "icons": [ {"src": "", "sizes": "x 40xx 1x2x3 x42 42xx42" } ] })");
    EXPECT_FALSE(manifest->icons.empty());
    EXPECT_EQ(manifest->icons[0]->sizes.size(), 0u);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("found icon with no valid size.", errors()[0]);
  }
}

TEST_F(ManifestParserTest, IconPurposeParseRules) {
  const String kPurposeParseStringError =
      "property 'purpose' ignored, type string expected.";
  const String kPurposeInvalidValueError =
      "found icon with no valid purpose; ignoring it.";
  const String kSomeInvalidPurposeError =
      "found icon with one or more invalid purposes; those purposes are "
      "ignored.";

  // Smoke test.
  {
    auto& manifest = ParseManifest(R"({ "icons": [ {"src": "",
        "purpose": "any" } ] })");
    EXPECT_FALSE(manifest->icons.empty());
    EXPECT_EQ(manifest->icons[0]->purpose.size(), 1u);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Trim leading and trailing whitespaces.
  {
    auto& manifest = ParseManifest(R"({ "icons": [ {"src": "",
        "purpose": "  any  " } ] })");
    EXPECT_FALSE(manifest->icons.empty());
    EXPECT_EQ(manifest->icons[0]->purpose.size(), 1u);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // 'any' is added when property isn't present.
  {
    auto& manifest = ParseManifest(R"({ "icons": [ {"src": "" } ] })");
    EXPECT_FALSE(manifest->icons.empty());

    auto& icons = manifest->icons;
    EXPECT_EQ(icons[0]->purpose.size(), 1u);
    EXPECT_EQ(icons[0]->purpose[0],
              mojom::blink::ManifestImageResource::Purpose::ANY);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // 'any' is added with error message when property isn't a string (is a
  // number).
  {
    auto& manifest = ParseManifest(R"({ "icons": [ {"src": "",
        "purpose": 42 } ] })");
    EXPECT_FALSE(manifest->icons.empty());

    auto& icons = manifest->icons;
    EXPECT_EQ(icons[0]->purpose.size(), 1u);
    EXPECT_EQ(icons[0]->purpose[0],
              mojom::blink::ManifestImageResource::Purpose::ANY);
    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ(kPurposeParseStringError, errors()[0]);
  }

  // 'any' is added with error message when property isn't a string (is a
  // dictionary).
  {
    auto& manifest = ParseManifest(R"({ "icons": [ {"src": "",
        "purpose": {} } ] })");
    EXPECT_FALSE(manifest->icons.empty());

    auto& icons = manifest->icons;
    EXPECT_EQ(icons[0]->purpose.size(), 1u);
    EXPECT_EQ(icons[0]->purpose[0],
              mojom::blink::ManifestImageResource::Purpose::ANY);
    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ(kPurposeParseStringError, errors()[0]);
  }

  // Smoke test: values correctly parsed.
  {
    auto& manifest = ParseManifest(R"({ "icons": [ {"src": "",
        "purpose": "Any Monochrome Maskable" } ] })");
    EXPECT_FALSE(manifest->icons.empty());

    auto& icons = manifest->icons;
    ASSERT_EQ(icons[0]->purpose.size(), 3u);
    EXPECT_EQ(icons[0]->purpose[0],
              mojom::blink::ManifestImageResource::Purpose::ANY);
    EXPECT_EQ(icons[0]->purpose[1],
              mojom::blink::ManifestImageResource::Purpose::MONOCHROME);
    EXPECT_EQ(icons[0]->purpose[2],
              mojom::blink::ManifestImageResource::Purpose::MASKABLE);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Trim whitespaces between values.
  {
    auto& manifest = ParseManifest(R"({ "icons": [ {"src": "",
        "purpose": "  Any   Monochrome  " } ] })");
    EXPECT_FALSE(manifest->icons.empty());

    auto& icons = manifest->icons;
    ASSERT_EQ(icons[0]->purpose.size(), 2u);
    EXPECT_EQ(icons[0]->purpose[0],
              mojom::blink::ManifestImageResource::Purpose::ANY);
    EXPECT_EQ(icons[0]->purpose[1],
              mojom::blink::ManifestImageResource::Purpose::MONOCHROME);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Twice the same value is parsed twice.
  {
    auto& manifest = ParseManifest(R"({ "icons": [ {"src": "",
        "purpose": "monochrome monochrome" } ] })");
    EXPECT_FALSE(manifest->icons.empty());

    auto& icons = manifest->icons;
    ASSERT_EQ(icons[0]->purpose.size(), 2u);
    EXPECT_EQ(icons[0]->purpose[0],
              mojom::blink::ManifestImageResource::Purpose::MONOCHROME);
    EXPECT_EQ(icons[0]->purpose[1],
              mojom::blink::ManifestImageResource::Purpose::MONOCHROME);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Invalid icon purpose is ignored.
  {
    auto& manifest = ParseManifest(R"({ "icons": [ {"src": "",
        "purpose": "monochrome fizzbuzz" } ] })");
    EXPECT_FALSE(manifest->icons.empty());

    auto& icons = manifest->icons;
    ASSERT_EQ(icons[0]->purpose.size(), 1u);
    EXPECT_EQ(icons[0]->purpose[0],
              mojom::blink::ManifestImageResource::Purpose::MONOCHROME);
    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ(kSomeInvalidPurposeError, errors()[0]);
  }

  // If developer-supplied purpose is invalid, entire icon is removed.
  {
    auto& manifest = ParseManifest(R"({ "icons": [ {"src": "",
        "purpose": "fizzbuzz" } ] })");
    ASSERT_TRUE(manifest->icons.empty());
    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ(kPurposeInvalidValueError, errors()[0]);
  }

  // Two icons, one with an invalid purpose and the other normal.
  {
    auto& manifest = ParseManifest(
        R"({ "icons": [ {"src": "", "purpose": "fizzbuzz" },
                       {"src": "" }] })");
    EXPECT_FALSE(manifest->icons.empty());

    auto& icons = manifest->icons;
    ASSERT_EQ(1u, icons.size());
    ASSERT_EQ(icons[0]->purpose.size(), 1u);
    EXPECT_EQ(icons[0]->purpose[0],
              mojom::blink::ManifestImageResource::Purpose::ANY);
    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ(kPurposeInvalidValueError, errors()[0]);
  }
}

TEST_F(ManifestParserTest, ShortcutsParseRules) {
  // Smoke test: if no shortcut, no value.
  {
    auto& manifest = ParseManifest(R"({ "shortcuts": [] })");
    EXPECT_TRUE(manifest->shortcuts.empty());
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Smoke test: if empty shortcut, no value.
  {
    auto& manifest = ParseManifest(R"({ "shortcuts": [ {} ] })");
    EXPECT_TRUE(manifest->icons.empty());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'url' of 'shortcut' not present.", errors()[0]);
  }

  // Smoke test: shortcut with invalid name and url, it will not be present in
  // the list.
  {
    auto& manifest =
        ParseManifest(R"({ "shortcuts": [ { "shortcuts": [] } ] })");
    EXPECT_TRUE(manifest->icons.empty());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'url' of 'shortcut' not present.", errors()[0]);
  }

  // Smoke test: shortcut with no name, it will not be present in the list.
  {
    auto& manifest = ParseManifest(R"({ "shortcuts": [ { "url": "" } ] })");
    EXPECT_TRUE(manifest->shortcuts.empty());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'name' of 'shortcut' not present.", errors()[0]);
  }

  // Smoke test: shortcut with no url, it will not be present in the list.
  {
    auto& manifest = ParseManifest(R"({ "shortcuts": [ { "name": "" } ] })");
    EXPECT_TRUE(manifest->shortcuts.empty());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'url' of 'shortcut' not present.", errors()[0]);
  }

  // Smoke test: shortcut with empty name, and empty src, will not be present in
  // the list.
  {
    auto& manifest =
        ParseManifest(R"({ "shortcuts": [ { "name": "", "url": "" } ] })");
    EXPECT_TRUE(manifest->shortcuts.empty());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'name' of 'shortcut' is an empty string.", errors()[0]);
  }

  // Smoke test: shortcut with valid (non-empty) name and src, will be present
  // in the list.
  {
    auto& manifest = ParseManifest(
        R"({ "shortcuts": [{ "name": "New Post", "url": "compose" }]
        })");
    EXPECT_FALSE(manifest->shortcuts.empty());

    auto& shortcuts = manifest->shortcuts;
    EXPECT_EQ(shortcuts.size(), 1u);
    EXPECT_EQ(shortcuts[0]->name, "New Post");
    EXPECT_EQ(shortcuts[0]->url.GetString(), "http://foo.com/compose");
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Validate only the first 10 shortcuts are parsed. The following manifest
  // specifies 11 shortcuts, so the last one should not be in the result.
  {
    auto& manifest = ParseManifest(
        R"({
          "shortcuts": [
            {
              "name": "1",
              "url": "1"
            },
            {
              "name": "2",
              "url": "2"
            },
            {
              "name": "3",
              "url": "3"
            },
            {
              "name": "4",
              "url": "4"
            },
            {
              "name": "5",
              "url": "5"
            },
            {
              "name": "6",
              "url": "6"
            },
            {
              "name": "7",
              "url": "7"
            },
            {
              "name": "8",
              "url": "8"
            },
            {
              "name": "9",
              "url": "9"
            },
            {
              "name": "10",
              "url": "10"
            },
            {
              "name": "11",
              "url": "11"
            }
          ]
        })");

    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'shortcuts' contains more than 10 valid elements, "
        "only the first 10 are parsed.",
        errors()[0]);

    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_FALSE(manifest->shortcuts.empty());
    auto& shortcuts = manifest->shortcuts;
    EXPECT_EQ(shortcuts.size(), 10u);
    EXPECT_EQ(shortcuts[9]->name, "10");
    EXPECT_EQ(shortcuts[9]->url.GetString(), "http://foo.com/10");
  }
}

TEST_F(ManifestParserTest, ShortcutNameParseRules) {
  // Smoke test.
  {
    auto& manifest = ParseManifest(
        R"({ "shortcuts": [ {"name": "foo", "url": "NameParseTest" } ]
        })");
    EXPECT_FALSE(manifest->shortcuts.empty());
    EXPECT_EQ(manifest->shortcuts[0]->name, "foo");
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Trim whitespaces.
  {
    auto& manifest = ParseManifest(
        R"({ "shortcuts": [ {"name": "  foo  ", "url": "NameParseTest"
        } ] })");
    ASSERT_EQ(manifest->shortcuts[0]->name, "foo");
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Don't parse if shortcut->name isn't present.
  {
    auto& manifest =
        ParseManifest(R"({ "shortcuts": [ {"url": "NameParseTest" } ] })");
    EXPECT_TRUE(manifest->shortcuts.empty());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'name' of 'shortcut' not present.", errors()[0]);
  }

  // Don't parse if shortcut->name isn't a string.
  {
    auto& manifest = ParseManifest(
        R"({ "shortcuts": [ {"name": {}, "url": "NameParseTest" } ] })");
    EXPECT_TRUE(manifest->shortcuts.empty());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'name' of 'shortcut' ignored, type string expected.",
              errors()[0]);
  }

  // Don't parse if shortcut->name isn't a string.
  {
    auto& manifest = ParseManifest(
        R"({ "shortcuts": [ {"name": 42, "url": "NameParseTest" } ] })");
    EXPECT_TRUE(manifest->shortcuts.empty());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'name' of 'shortcut' ignored, type string expected.",
              errors()[0]);
  }

  // Don't parse if shortcut->name is an empty string.
  {
    auto& manifest = ParseManifest(
        R"({ "shortcuts": [ {"name": "", "url": "NameParseTest" } ] })");
    EXPECT_TRUE(manifest->shortcuts.empty());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'name' of 'shortcut' is an empty string.", errors()[0]);
  }
}

TEST_F(ManifestParserTest, ShortcutShortNameParseRules) {
  // Smoke test.
  {
    auto& manifest = ParseManifest(
        R"({ "shortcuts": [ {"name": "ShortNameParseTest", "short_name":
        "foo", "url": "ShortNameParseTest" } ] })");
    ASSERT_EQ(manifest->shortcuts[0]->short_name, "foo");
    ASSERT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Shortcut member is parsed when no short_name is present
  {
    auto& manifest =
        ParseManifest(R"({ "shortcuts": [ {"name": "ShortNameParseTest", "url":
        "ShortNameParseTest" } ] })");
    ASSERT_TRUE(manifest->shortcuts[0]->short_name.IsNull());
    ASSERT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Trim whitespaces.
  {
    auto& manifest = ParseManifest(
        R"({ "shortcuts": [ {"name": "ShortNameParseTest", "short_name":
        "  foo  ", "url": "ShortNameParseTest" } ] })");
    ASSERT_EQ(manifest->shortcuts[0]->short_name, "foo");
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Don't parse short_name if it isn't a string.
  {
    auto& manifest = ParseManifest(
        R"({ "shortcuts": [ {"name": "ShortNameParseTest", "short_name":
        {}, "url": "ShortNameParseTest" } ] })");
    ASSERT_TRUE(manifest->shortcuts[0]->short_name.IsNull());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'short_name' of 'shortcut' ignored, type string expected.",
        errors()[0]);
  }

  // Don't parse short_name if it isn't a string.
  {
    auto& manifest = ParseManifest(
        R"({ "shortcuts": [ {"name": "ShortNameParseTest", "short_name":
        42, "url": "ShortNameParseTest" } ] })");
    ASSERT_TRUE(manifest->shortcuts[0]->short_name.IsNull());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'short_name' of 'shortcut' ignored, type string expected.",
        errors()[0]);
  }
}

TEST_F(ManifestParserTest, ShortcutDescriptionParseRules) {
  // Smoke test.
  {
    auto& manifest = ParseManifest(
        R"({ "shortcuts": [ {
          "name": "DescriptionParseTest",
          "description": "foo",
          "url": "DescriptionParseTest" } ]
        })");
    ASSERT_EQ(manifest->shortcuts[0]->description, "foo");
    ASSERT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Shortcut member is parsed when no description is present
  {
    auto& manifest = ParseManifest(
        R"({ "shortcuts": [ {"name": "DescriptionParseTest", "url":
        "DescriptionParseTest" } ] })");
    ASSERT_TRUE(manifest->shortcuts[0]->description.IsNull());
    ASSERT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Trim whitespaces.
  {
    auto& manifest = ParseManifest(
        R"({ "shortcuts": [ {
          "name": "DescriptionParseTest",
          "description": "  foo  ",
          "url": "DescriptionParseTest" } ]
        })");
    ASSERT_EQ(manifest->shortcuts[0]->description, "foo");
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Don't parse description if it isn't a string.
  {
    auto& manifest = ParseManifest(
        R"({ "shortcuts": [ {
          "name": "DescriptionParseTest",
          "description": {},
          "url": "DescriptionParseTest" } ]
        })");
    ASSERT_TRUE(manifest->shortcuts[0]->description.IsNull());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'description' of 'shortcut' ignored, type string expected.",
        errors()[0]);
  }

  // Don't parse description if it isn't a string.
  {
    auto& manifest = ParseManifest(
        R"({ "shortcuts": [ {
          "name": "DescriptionParseTest",
          "description": 42,
          "url": "DescriptionParseTest" } ]
        })");
    ASSERT_TRUE(manifest->shortcuts[0]->description.IsNull());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'description' of 'shortcut' ignored, type string expected.",
        errors()[0]);
  }
}

TEST_F(ManifestParserTest, ShortcutUrlParseRules) {
  // Smoke test.
  {
    auto& manifest = ParseManifest(
        R"({ "shortcuts": [ {"name": "UrlParseTest", "url": "foo" } ]
        })");
    EXPECT_FALSE(manifest->shortcuts.empty());
    EXPECT_EQ(manifest->shortcuts[0]->url, KURL(DefaultDocumentUrl(), "foo"));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Smoke test. Don't parse (with an error) when url is not present.
  {
    auto& manifest = ParseManifest(R"({ "shortcuts": [ { "name": "" } ] })");
    EXPECT_TRUE(manifest->shortcuts.empty());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'url' of 'shortcut' not present.", errors()[0]);
  }

  // Whitespaces.
  {
    auto& manifest = ParseManifest(
        R"({ "shortcuts": [ {"name": "UrlParseTest", "url": "   foo   " } ] })");
    EXPECT_FALSE(manifest->shortcuts.empty());
    EXPECT_EQ(manifest->shortcuts[0]->url, KURL(DefaultDocumentUrl(), "foo"));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Don't parse if url isn't a string.
  {
    auto& manifest = ParseManifest(
        R"({ "shortcuts": [ {"name": "UrlParseTest", "url": {} } ] })");
    EXPECT_TRUE(manifest->shortcuts.empty());
    EXPECT_EQ(2u, GetErrorCount());
    EXPECT_EQ("property 'url' ignored, type string expected.", errors()[0]);
    EXPECT_EQ("property 'url' of 'shortcut' not present.", errors()[1]);
  }

  // Don't parse if url isn't a string.
  {
    auto& manifest = ParseManifest(
        R"({ "shortcuts": [ {"name": "UrlParseTest", "url": 42 } ] })");
    EXPECT_TRUE(manifest->shortcuts.empty());
    EXPECT_EQ(2u, GetErrorCount());
    EXPECT_EQ("property 'url' ignored, type string expected.", errors()[0]);
    EXPECT_EQ("property 'url' of 'shortcut' not present.", errors()[1]);
  }

  // Resolving has to happen based on the manifest_url.
  {
    auto& manifest = ParseManifestWithURLs(
        R"({ "shortcuts": [ {"name": "UrlParseTest", "url": "foo" } ]
        })",
        KURL("http://foo.com/landing/manifest.json"), DefaultDocumentUrl());
    EXPECT_FALSE(manifest->shortcuts.empty());
    EXPECT_EQ(manifest->shortcuts[0]->url.GetString(),
              "http://foo.com/landing/foo");
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Shortcut url should have same origin as the document url.
  {
    auto& manifest = ParseManifestWithURLs(
        R"({ "shortcuts": [ {"name": "UrlParseTest", "url":
        "http://bar.com/landing" } ]
        })",
        KURL("http://foo.com/landing/manifest.json"), DefaultDocumentUrl());
    EXPECT_TRUE(manifest->shortcuts.empty());
    EXPECT_EQ(2u, GetErrorCount());
    EXPECT_EQ("property 'url' ignored, should be within scope of the manifest.",
              errors()[0]);
    EXPECT_EQ("property 'url' of 'shortcut' not present.", errors()[1]);
  }

  // Shortcut url should be within the manifest scope.
  // The scope will be http://foo.com/landing.
  // The shortcut_url will be http://foo.com/shortcut which is in not in scope.
  {
    auto& manifest = ParseManifestWithURLs(
        R"({ "scope": "http://foo.com/landing", "shortcuts": [ {"name":
        "UrlParseTest", "url": "shortcut" } ] })",
        KURL("http://foo.com/manifest.json"),
        KURL("http://foo.com/landing/index.html"));
    EXPECT_TRUE(manifest->shortcuts.empty());
    ASSERT_EQ(manifest->scope.GetString(), "http://foo.com/landing");
    EXPECT_EQ(2u, GetErrorCount());
    EXPECT_EQ("property 'url' ignored, should be within scope of the manifest.",
              errors()[0]);
    EXPECT_EQ("property 'url' of 'shortcut' not present.", errors()[1]);
  }

  // Shortcut url should be within the manifest scope.
  // The scope will be http://foo.com/land.
  // The shortcut_url will be http://foo.com/land/shortcut which is in scope.
  {
    auto& manifest = ParseManifestWithURLs(
        R"({ "scope": "http://foo.com/land", "start_url":
        "http://foo.com/land/landing.html", "shortcuts": [ {"name":
        "UrlParseTest", "url": "shortcut" } ] })",
        KURL("http://foo.com/land/manifest.json"),
        KURL("http://foo.com/index.html"));
    EXPECT_FALSE(manifest->shortcuts.empty());
    ASSERT_EQ(manifest->scope.GetString(), "http://foo.com/land");
    EXPECT_EQ(manifest->shortcuts[0]->url.GetString(),
              "http://foo.com/land/shortcut");
    EXPECT_EQ(0u, GetErrorCount());
  }
}

TEST_F(ManifestParserTest, ShortcutIconsParseRules) {
  // Smoke test: if no icons, shortcut->icons has no value.
  {
    auto& manifest = ParseManifest(
        R"({ "shortcuts": [ {"name": "IconParseTest", "url": "foo",
        "icons": [] } ] })");
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_FALSE(manifest->shortcuts.empty());
    EXPECT_TRUE(manifest->shortcuts[0]->icons.empty());
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Smoke test: if empty icon, shortcut->icons has no value.
  {
    auto& manifest = ParseManifest(
        R"({ "shortcuts": [ {"name": "IconParseTest", "url": "foo",
        "icons": [{}] } ] })");
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_FALSE(manifest->shortcuts.empty());
    EXPECT_TRUE(manifest->shortcuts[0]->icons.empty());
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Smoke test: icon with invalid src, shortcut->icons has no value.
  {
    auto& manifest = ParseManifest(
        R"({ "shortcuts": [ {"name": "IconParseTest", "url": "foo",
        "icons": [{ "icons": [] }] } ] })");
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_FALSE(manifest->shortcuts.empty());
    EXPECT_TRUE(manifest->shortcuts[0]->icons.empty());
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Smoke test: if icon with empty src, it will be present in shortcut->icons.
  {
    auto& manifest = ParseManifest(
        R"({ "shortcuts": [ {"name": "IconParseTest", "url": "foo",
        "icons": [ { "src": "" } ] } ] })");
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_FALSE(manifest->shortcuts.empty());
    EXPECT_FALSE(manifest->shortcuts[0]->icons.empty());

    auto& icons = manifest->shortcuts[0]->icons;
    EXPECT_EQ(icons.size(), 1u);
    EXPECT_EQ(icons[0]->src.GetString(), "http://foo.com/manifest.json");
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Smoke test: if one icon with valid src, it will be present in
  // shortcut->icons.
  {
    auto& manifest = ParseManifest(
        R"({ "shortcuts": [ {"name": "IconParseTest", "url": "foo",
        "icons": [ { "src": "foo.jpg" } ] } ] })");
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_FALSE(manifest->shortcuts.empty());
    EXPECT_FALSE(manifest->shortcuts[0]->icons.empty());
    auto& icons = manifest->shortcuts[0]->icons;
    EXPECT_EQ(icons.size(), 1u);
    EXPECT_EQ(icons[0]->src.GetString(), "http://foo.com/foo.jpg");
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Smoke test: if >1 icon with valid src, it will be present in
  // shortcut->icons.
  {
    auto& manifest = ParseManifest(
        R"({ "shortcuts": [ {"name": "IconParseTest", "url": "foo",
        "icons": [ {"src": "foo.jpg"}, {"src": "bar.jpg"} ] } ] })");
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_FALSE(manifest->shortcuts.empty());
    EXPECT_FALSE(manifest->shortcuts[0]->icons.empty());
    auto& icons = manifest->shortcuts[0]->icons;
    EXPECT_EQ(icons.size(), 2u);
    EXPECT_EQ(icons[0]->src.GetString(), "http://foo.com/foo.jpg");
    EXPECT_EQ(icons[1]->src.GetString(), "http://foo.com/bar.jpg");
    EXPECT_EQ(0u, GetErrorCount());
  }
}

TEST_F(ManifestParserTest, FileHandlerParseRules) {
  base::test::ScopedFeatureList feature_list(
      blink::features::kFileHandlingIcons);
  // Does not contain file_handlers field.
  {
    auto& manifest = ParseManifest("{ }");
    ASSERT_EQ(0u, GetErrorCount());
    EXPECT_EQ(0u, manifest->file_handlers.size());
  }

  // file_handlers is not an array.
  {
    auto& manifest = ParseManifest(R"({ "file_handlers": { } })");
    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'file_handlers' ignored, type array expected.",
              errors()[0]);
    EXPECT_EQ(0u, manifest->file_handlers.size());
  }

  // Contains file_handlers field but no file handlers.
  {
    auto& manifest = ParseManifest(R"({ "file_handlers": [ ] })");
    ASSERT_EQ(0u, GetErrorCount());
    EXPECT_EQ(0u, manifest->file_handlers.size());
  }

  // Entries must be objects.
  {
    auto& manifest = ParseManifest(R"({
          "file_handlers": [
            "hello world"
          ]
        })");
    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ("FileHandler ignored, type object expected.", errors()[0]);
    EXPECT_EQ(0u, manifest->file_handlers.size());
  }

  // Entry without an action is invalid.
  {
    auto& manifest = ParseManifest(
        R"({
          "file_handlers": [
            {
              "name": "name",
              "icons": [{ "src": "foo.jpg" }],
              "accept": {
                "image/png": [
                  ".png"
                ]
              }
            }
          ]
        })");
    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ("FileHandler ignored. Property 'action' is invalid.",
              errors()[0]);
    EXPECT_EQ(0u, manifest->file_handlers.size());
  }

  // Entry with an action on a different origin is invalid.
  {
    auto& manifest = ParseManifest(
        R"({
          "file_handlers": [
            {
              "name": "name",
              "icons": [{ "src": "foo.jpg" }],
              "action": "https://example.com/files",
              "accept": {
                "image/png": [
                  ".png"
                ]
              }
            }
          ]
        })");
    ASSERT_EQ(2u, GetErrorCount());
    EXPECT_EQ(
        "property 'action' ignored, should be within scope of the manifest.",
        errors()[0]);
    EXPECT_EQ("FileHandler ignored. Property 'action' is invalid.",
              errors()[1]);
    EXPECT_EQ(0u, manifest->file_handlers.size());
  }

  // Entry with an action outside of the manifest scope is invalid.
  {
    auto& manifest = ParseManifest(
        R"({
          "start_url": "/app/",
          "scope": "/app/",
          "file_handlers": [
            {
              "name": "name",
              "icons": [{ "src": "foo.jpg" }],
              "action": "/files",
              "accept": {
                "image/png": [
                  ".png"
                ]
              }
            }
          ]
        })");
    ASSERT_EQ(2u, GetErrorCount());
    EXPECT_EQ(
        "property 'action' ignored, should be within scope of the manifest.",
        errors()[0]);
    EXPECT_EQ("FileHandler ignored. Property 'action' is invalid.",
              errors()[1]);
    EXPECT_EQ(0u, manifest->file_handlers.size());
  }

  // Entry without a name is valid.
  {
    auto& manifest = ParseManifest(
        R"({
          "file_handlers": [
            {
              "icons": [{ "src": "foo.jpg" }],
              "action": "/files",
              "accept": {
                "image/png": [
                  ".png"
                ]
              }
            }
          ]
        })");
    ASSERT_EQ(0u, GetErrorCount());
    EXPECT_EQ(1u, manifest->file_handlers.size());
  }

  // Entry without an icon is valid.
  {
    auto& manifest = ParseManifest(
        R"({
          "file_handlers": [
            {
              "name": "name",
              "action": "/files",
              "accept": {
                "image/png": [
                  ".png"
                ]
              }
            }
          ]
        })");
    ASSERT_EQ(0u, GetErrorCount());
    EXPECT_EQ(1u, manifest->file_handlers.size());
  }

  // Entry without an accept is invalid.
  {
    auto& manifest = ParseManifest(
        R"({
          "file_handlers": [
            {
              "name": "name",
              "icons": [{ "src": "foo.jpg" }],
              "action": "/files"
            }
          ]
        })");
    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ("FileHandler ignored. Property 'accept' is invalid.",
              errors()[0]);
    EXPECT_EQ(0u, manifest->file_handlers.size());
  }

  // Entry where accept is not an object is invalid.
  {
    auto& manifest = ParseManifest(
        R"({
          "file_handlers": [
            {
              "name": "name",
              "icons": [{ "src": "foo.jpg" }],
              "action": "/files",
              "accept": "image/png"
            }
          ]
        })");
    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ("FileHandler ignored. Property 'accept' is invalid.",
              errors()[0]);
    EXPECT_EQ(0u, manifest->file_handlers.size());
  }

  // Entry where accept extensions are not an array or string is invalid.
  {
    auto& manifest = ParseManifest(
        R"({
          "file_handlers": [
            {
              "name": "name",
              "icons": [{ "src": "foo.jpg" }],
              "action": "/files",
              "accept": {
                "image/png": {}
              }
            }
          ]
        })");
    ASSERT_EQ(2u, GetErrorCount());
    EXPECT_EQ(
        "property 'accept' type ignored. File extensions must be type array or "
        "type string.",
        errors()[0]);
    EXPECT_EQ("FileHandler ignored. Property 'accept' is invalid.",
              errors()[1]);
    EXPECT_EQ(0u, manifest->file_handlers.size());
  }

  // Entry where accept extensions are not an array or string is invalid.
  {
    auto& manifest = ParseManifest(
        R"({
          "file_handlers": [
            {
              "name": "name",
              "icons": [{ "src": "foo.jpg" }],
              "action": "/files",
              "accept": {
                "image/png": 3
              }
            }
          ]
        })");
    ASSERT_EQ(2u, GetErrorCount());
    EXPECT_EQ(
        "property 'accept' type ignored. File extensions must be type array or "
        "type string.",
        errors()[0]);
    EXPECT_EQ("FileHandler ignored. Property 'accept' is invalid.",
              errors()[1]);
    EXPECT_EQ(0u, manifest->file_handlers.size());
  }

  // Entry with an empty list of extensions is not valid.
  {
    auto& manifest = ParseManifest(
        R"({
          "file_handlers": [
            {
              "name": "name",
              "icons": [{ "src": "foo.jpg" }],
              "action": "/files",
              "accept": {
                "image/png": []
              }
            }
          ]
        })");
    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ("FileHandler ignored. Property 'accept' is invalid.",
              errors()[0]);
    EXPECT_EQ(0u, manifest->file_handlers.size());
  }

  // Extensions that do not start with a '.' are invalid.
  {
    auto& manifest = ParseManifest(
        R"({
          "file_handlers": [
            {
              "name": "name",
              "icons": [{ "src": "foo.jpg" }],
              "action": "/files",
              "accept": {
                "image/png": [
                  "png"
                ]
              }
            }
          ]
        })");
    auto& file_handlers = manifest->file_handlers;

    ASSERT_EQ(2u, GetErrorCount());
    EXPECT_EQ(
        "property 'accept' file extension ignored, must start with a '.'.",
        errors()[0]);
    EXPECT_EQ("FileHandler ignored. Property 'accept' is invalid.",
              errors()[1]);
    ASSERT_EQ(0u, file_handlers.size());
  }

  // Invalid MIME types and those with parameters are stripped.
  {
    auto& manifest = ParseManifest(
        R"({
          "file_handlers": [
            {
              "name": "Foo",
              "icons": [{ "src": "foo.jpg" }],
              "action": "/files",
              "accept": {
                "image_png": ".png",
                "foo/bar": ".foo",
                "application/foobar;parameter=25": ".foobar",
                "application/its+xml": ".itsml"
              }
            }
          ]
        })");
    auto& file_handlers = manifest->file_handlers;

    ASSERT_EQ(3u, GetErrorCount());
    EXPECT_EQ("invalid MIME type: image_png", errors()[0]);
    EXPECT_EQ("invalid MIME type: foo/bar", errors()[1]);
    EXPECT_EQ("invalid MIME type: application/foobar;parameter=25",
              errors()[2]);
    ASSERT_EQ(1u, file_handlers.size());

    EXPECT_EQ("Foo", file_handlers[0]->name);
    EXPECT_EQ("http://foo.com/foo.jpg",
              file_handlers[0]->icons[0]->src.GetString());
    EXPECT_EQ(KURL("http://foo.com/files"), file_handlers[0]->action);
    ASSERT_EQ(1U, file_handlers[0]->accept.size());
    ASSERT_TRUE(file_handlers[0]->accept.Contains("application/its+xml"));
    EXPECT_EQ(0u, file_handlers[0]
                      ->accept.find("application/its+xml")
                      ->value.Contains(".foobar"));
  }

  // Extensions specified as a single string is valid.
  {
    auto& manifest = ParseManifest(
        R"({
          "file_handlers": [
            {
              "name": "name",
              "icons": [{ "src": "foo.jpg" }],
              "action": "/files",
              "accept": {
                "image/png": ".png"
              }
            }
          ]
        })");
    auto& file_handlers = manifest->file_handlers;

    ASSERT_EQ(0u, GetErrorCount());
    ASSERT_EQ(1u, file_handlers.size());

    EXPECT_EQ("name", file_handlers[0]->name);
    EXPECT_EQ("http://foo.com/foo.jpg",
              file_handlers[0]->icons[0]->src.GetString());
    EXPECT_EQ(KURL("http://foo.com/files"), file_handlers[0]->action);
    ASSERT_TRUE(file_handlers[0]->accept.Contains("image/png"));
    ASSERT_EQ(1u, file_handlers[0]->accept.find("image/png")->value.size());
    EXPECT_EQ(".png", file_handlers[0]->accept.find("image/png")->value[0]);
  }

  // An array of extensions is valid.
  {
    auto& manifest = ParseManifest(
        R"({
          "file_handlers": [
            {
              "name": "name",
              "icons": [{ "src": "foo.jpg" }],
              "action": "/files",
              "accept": {
                "image/jpg": [
                  ".jpg",
                  ".jpeg"
                ]
              }
            }
          ]
        })");
    auto& file_handlers = manifest->file_handlers;

    ASSERT_EQ(0u, GetErrorCount());
    ASSERT_EQ(1u, file_handlers.size());

    EXPECT_EQ("name", file_handlers[0]->name);
    EXPECT_EQ("http://foo.com/foo.jpg",
              file_handlers[0]->icons[0]->src.GetString());
    EXPECT_EQ(KURL("http://foo.com/files"), file_handlers[0]->action);
    ASSERT_TRUE(file_handlers[0]->accept.Contains("image/jpg"));
    ASSERT_EQ(2u, file_handlers[0]->accept.find("image/jpg")->value.size());
    EXPECT_EQ(".jpg", file_handlers[0]->accept.find("image/jpg")->value[0]);
    EXPECT_EQ(".jpeg", file_handlers[0]->accept.find("image/jpg")->value[1]);
  }

  // Multiple mime types are valid.
  {
    auto& manifest = ParseManifest(
        R"({
          "file_handlers": [
            {
              "name": "Image",
              "icons": [{ "src": "foo.jpg" }],
              "action": "/files",
              "accept": {
                "image/png": ".png",
                "image/jpg": [
                  ".jpg",
                  ".jpeg"
                ]
              }
            }
          ]
        })");
    auto& file_handlers = manifest->file_handlers;

    ASSERT_EQ(0u, GetErrorCount());
    ASSERT_EQ(1u, file_handlers.size());

    EXPECT_EQ("Image", file_handlers[0]->name);
    EXPECT_EQ("http://foo.com/foo.jpg",
              file_handlers[0]->icons[0]->src.GetString());
    EXPECT_EQ(KURL("http://foo.com/files"), file_handlers[0]->action);

    ASSERT_TRUE(file_handlers[0]->accept.Contains("image/jpg"));
    ASSERT_EQ(2u, file_handlers[0]->accept.find("image/jpg")->value.size());
    EXPECT_EQ(".jpg", file_handlers[0]->accept.find("image/jpg")->value[0]);
    EXPECT_EQ(".jpeg", file_handlers[0]->accept.find("image/jpg")->value[1]);

    ASSERT_TRUE(file_handlers[0]->accept.Contains("image/png"));
    ASSERT_EQ(1u, file_handlers[0]->accept.find("image/png")->value.size());
    EXPECT_EQ(".png", file_handlers[0]->accept.find("image/png")->value[0]);
  }

  // file_handlers with multiple entries is valid.
  {
    auto& manifest = ParseManifest(
        R"({
          "file_handlers": [
            {
              "name": "Graph",
              "icons": [{ "src": "graph.jpg" }],
              "action": "/graph",
              "accept": {
                "text/svg+xml": [
                  ".svg",
                  ".graph"
                ]
              }
            },
            {
              "name": "Raw",
              "icons": [{ "src": "raw.jpg" }],
              "action": "/raw",
              "accept": {
                "text/csv": ".csv"
              }
            }
          ]
        })");
    auto& file_handlers = manifest->file_handlers;

    ASSERT_EQ(0u, GetErrorCount());
    ASSERT_EQ(2u, file_handlers.size());

    EXPECT_EQ("Graph", file_handlers[0]->name);
    EXPECT_EQ("http://foo.com/graph.jpg",
              file_handlers[0]->icons[0]->src.GetString());
    EXPECT_EQ(KURL("http://foo.com/graph"), file_handlers[0]->action);
    ASSERT_TRUE(file_handlers[0]->accept.Contains("text/svg+xml"));
    ASSERT_EQ(2u, file_handlers[0]->accept.find("text/svg+xml")->value.size());
    EXPECT_EQ(".svg", file_handlers[0]->accept.find("text/svg+xml")->value[0]);
    EXPECT_EQ(".graph",
              file_handlers[0]->accept.find("text/svg+xml")->value[1]);

    EXPECT_EQ("Raw", file_handlers[1]->name);
    EXPECT_EQ("http://foo.com/raw.jpg",
              file_handlers[1]->icons[0]->src.GetString());
    EXPECT_EQ(KURL("http://foo.com/raw"), file_handlers[1]->action);
    ASSERT_TRUE(file_handlers[1]->accept.Contains("text/csv"));
    ASSERT_EQ(1u, file_handlers[1]->accept.find("text/csv")->value.size());
    EXPECT_EQ(".csv", file_handlers[1]->accept.find("text/csv")->value[0]);
  }

  // file_handlers limits the total number of file extensions. Everything after
  // and including the file handler that hits the extension limit
  {
    ManifestParser::SetFileHandlerExtensionLimitForTesting(5);
    auto& manifest = ParseManifest(
        R"({
          "file_handlers": [
            {
              "name": "Raw",
              "action": "/raw",
              "accept": {
                "text/csv": ".csv"
              }
            },
            {
              "name": "Graph",
              "action": "/graph",
              "accept": {
                "text/svg+xml": [
                  ".graph1",
                  ".graph2",
                  ".graph3",
                  ".graph4",
                  ".graph5",
                  ".graph6"
                ]
              }
            },
            {
              "name": "Data",
              "action": "/data",
              "accept": {
                "text/plain": [
                  ".data"
                ]
              }
            }
          ]
        })");
    auto& file_handlers = manifest->file_handlers;

    ASSERT_EQ(2u, GetErrorCount());
    EXPECT_EQ(
        "property 'accept': too many total file extensions, ignoring "
        "extensions starting from \".graph5\"",
        errors()[0]);
    EXPECT_EQ("FileHandler ignored. Property 'accept' is invalid.",
              errors()[1]);

    ASSERT_EQ(2u, file_handlers.size());

    EXPECT_EQ("Raw", file_handlers[0]->name);
    EXPECT_EQ(1u, file_handlers[0]->accept.find("text/csv")->value.size());

    EXPECT_EQ("Graph", file_handlers[1]->name);
    auto accept_map = file_handlers[1]->accept.find("text/svg+xml")->value;
    ASSERT_EQ(4u, accept_map.size());
    EXPECT_TRUE(accept_map.Contains(".graph1"));
    EXPECT_TRUE(accept_map.Contains(".graph2"));
    EXPECT_TRUE(accept_map.Contains(".graph3"));
    EXPECT_TRUE(accept_map.Contains(".graph4"));
  }

  // Test `launch_type` parsing and default.
  {
    auto& manifest = ParseManifest(
        R"({
          "file_handlers": [
            {
              "action": "/files",
              "accept": {
                "image/png": ".png"
              },
              "launch_type": "multiple-clients"
            },
            {
              "action": "/files2",
              "accept": {
                "image/jpeg": ".jpeg"
              },
              "launch_type": "single-client"
            },
            {
              "action": "/files3",
              "accept": {
                "text/plain": ".txt"
              }
            },
            {
              "action": "/files4",
              "accept": {
                "text/csv": ".csv"
              },
              "launch_type": "multiple-client"
            }
          ]
        })");
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_FALSE(manifest->file_handlers.empty());
    ASSERT_EQ(4U, manifest->file_handlers.size());
    EXPECT_EQ(mojom::blink::ManifestFileHandler::LaunchType::kMultipleClients,
              manifest->file_handlers[0]->launch_type);
    EXPECT_EQ(mojom::blink::ManifestFileHandler::LaunchType::kSingleClient,
              manifest->file_handlers[1]->launch_type);
    EXPECT_EQ(mojom::blink::ManifestFileHandler::LaunchType::kSingleClient,
              manifest->file_handlers[2]->launch_type);
    // This one has a typo.
    EXPECT_EQ(mojom::blink::ManifestFileHandler::LaunchType::kSingleClient,
              manifest->file_handlers[3]->launch_type);
    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ("launch_type value 'multiple-client' ignored, unknown value.",
              errors()[0]);
  }
}

TEST_F(ManifestParserTest, FileHandlerIconsParseRules) {
  // Smoke test: if no icons, file_handler->icon has no value.
  {
    auto& manifest = ParseManifest(
        R"({
          "file_handlers": [
            {
              "icons": [],
              "action": "/files",
              "accept": {
                "image/png": ".png"
              }
            }
          ]
        })");
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_FALSE(manifest->file_handlers.empty());
    EXPECT_TRUE(manifest->file_handlers[0]->icons.empty());
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Smoke test: if empty icon, file_handler->icons has no value.
  {
    auto& manifest = ParseManifest(
        R"({
          "file_handlers": [
            {
              "icons": [{}],
              "action": "/files",
              "accept": {
                "image/png": ".png"
              }
            }
          ]
        })");
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_FALSE(manifest->file_handlers.empty());
    EXPECT_TRUE(manifest->file_handlers[0]->icons.empty());
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Smoke test: icon with invalid src, file_handler->icons has no value.
  {
    auto& manifest = ParseManifest(
        R"({
          "file_handlers": [
            {
              "icons": [{ "icons": [] }],
              "action": "/files",
              "accept": {
                "image/png": ".png"
              }
            }
          ]
        })");
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_FALSE(manifest->file_handlers.empty());
    EXPECT_TRUE(manifest->file_handlers[0]->icons.empty());
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Smoke test: if icon with empty src, it will be present in
  // file_handler->icons.
  {
    auto& manifest = ParseManifest(
        R"({
          "file_handlers": [
            {
              "icons": [{ "src": "" }],
              "action": "/files",
              "accept": {
                "image/png": ".png"
              }
            }
          ]
        })");
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_FALSE(manifest->file_handlers.empty());
    EXPECT_FALSE(manifest->file_handlers[0]->icons.empty());

    auto& icons = manifest->file_handlers[0]->icons;
    EXPECT_EQ(icons.size(), 1u);
    EXPECT_EQ(icons[0]->src.GetString(), "http://foo.com/manifest.json");
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Smoke test: if one icon with valid src, it will be present in
  // file_handler->icons.
  {
    auto& manifest = ParseManifest(
        R"({
          "file_handlers": [
            {
              "icons": [{ "src": "foo.jpg" }],
              "action": "/files",
              "accept": {
                "image/png": ".png"
              }
            }
          ]
        })");
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_FALSE(manifest->file_handlers.empty());
    EXPECT_FALSE(manifest->file_handlers[0]->icons.empty());
    auto& icons = manifest->file_handlers[0]->icons;
    EXPECT_EQ(icons.size(), 1u);
    EXPECT_EQ(icons[0]->src.GetString(), "http://foo.com/foo.jpg");
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Smoke test: if >1 icon with valid src, it will be present in
  // file_handler->icons.
  {
    auto& manifest = ParseManifest(
        R"({
          "file_handlers": [
            {
              "icons": [{ "src": "foo.jpg" }, { "src": "bar.jpg" }],
              "action": "/files",
              "accept": {
                "image/png": ".png"
              }
            }
          ]
        })");
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_FALSE(manifest->file_handlers.empty());
    EXPECT_FALSE(manifest->file_handlers[0]->icons.empty());
    auto& icons = manifest->file_handlers[0]->icons;
    EXPECT_EQ(icons.size(), 2u);
    EXPECT_EQ(icons[0]->src.GetString(), "http://foo.com/foo.jpg");
    EXPECT_EQ(icons[1]->src.GetString(), "http://foo.com/bar.jpg");
    EXPECT_EQ(0u, GetErrorCount());
  }
}

TEST_F(ManifestParserTest, ProtocolHandlerParseRules) {
  // Does not contain protocol_handlers field.
  {
    auto& manifest = ParseManifest("{ }");
    ASSERT_EQ(0u, GetErrorCount());
    EXPECT_EQ(0u, manifest->protocol_handlers.size());
  }

  // protocol_handlers is not an array.
  {
    auto& manifest = ParseManifest(R"({ "protocol_handlers": { } })");
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'protocol_handlers' ignored, type array expected.",
              errors()[0]);
    EXPECT_EQ(0u, manifest->protocol_handlers.size());
  }

  // Contains protocol_handlers field but no protocol handlers.
  {
    auto& manifest = ParseManifest(R"({ "protocol_handlers": [ ] })");
    ASSERT_EQ(0u, GetErrorCount());
    EXPECT_EQ(0u, manifest->protocol_handlers.size());
  }

  // Entries must be objects
  {
    auto& manifest = ParseManifest(R"({
          "protocol_handlers": [
            "hello world"
          ]
        })");
    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ("protocol_handlers entry ignored, type object expected.",
              errors()[0]);
    EXPECT_EQ(0u, manifest->protocol_handlers.size());
  }

  // A valid protocol handler.
  {
    auto& manifest = ParseManifest(R"({
          "protocol_handlers": [
            {
              "protocol": "web+github",
              "url": "http://foo.com/?profile=%s"
            }
          ]
        })");
    auto& protocol_handlers = manifest->protocol_handlers;

    ASSERT_EQ(0u, GetErrorCount());
    ASSERT_EQ(1u, protocol_handlers.size());

    ASSERT_EQ("web+github", protocol_handlers[0]->protocol);
    ASSERT_EQ("http://foo.com/?profile=%s", protocol_handlers[0]->url);
  }

  // An invalid protocol handler with the URL not being from the same origin.
  {
    auto& manifest = ParseManifest(R"({
          "protocol_handlers": [
            {
              "protocol": "web+github",
              "url": "http://bar.com/?profile=%s"
            }
          ]
        })");
    auto& protocol_handlers = manifest->protocol_handlers;

    ASSERT_EQ(2u, GetErrorCount());
    EXPECT_EQ("property 'url' ignored, should be within scope of the manifest.",
              errors()[0]);
    EXPECT_EQ(
        "protocol_handlers entry ignored, required property 'url' is invalid.",
        errors()[1]);
    ASSERT_EQ(0u, protocol_handlers.size());
  }

  // An invalid protocol handler with the URL not being within manifest scope.
  {
    auto& manifest = ParseManifest(
        R"({
          "start_url": "/app/",
          "scope": "/app/",
          "protocol_handlers": [
            {
              "protocol": "web+github",
              "url": "/?profile=%s"
            }
          ]
        })");
    auto& protocol_handlers = manifest->protocol_handlers;

    ASSERT_EQ(2u, GetErrorCount());
    EXPECT_EQ("property 'url' ignored, should be within scope of the manifest.",
              errors()[0]);
    EXPECT_EQ(
        "protocol_handlers entry ignored, required property 'url' is invalid.",
        errors()[1]);
    ASSERT_EQ(0u, protocol_handlers.size());
  }

  // An invalid protocol handler with no value for protocol.
  {
    auto& manifest = ParseManifest(R"({
          "protocol_handlers": [
            {
              "url": "http://foo.com/?profile=%s"
            }
          ]
        })");
    auto& protocol_handlers = manifest->protocol_handlers;

    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "protocol_handlers entry ignored, required property 'protocol' is "
        "missing.",
        errors()[0]);
    ASSERT_EQ(0u, protocol_handlers.size());
  }

  // An invalid protocol handler with no url.
  {
    auto& manifest = ParseManifest(R"({
          "protocol_handlers": [
            {
              "protocol": "web+github"
            }
          ]
        })");
    auto& protocol_handlers = manifest->protocol_handlers;

    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "protocol_handlers entry ignored, required property 'url' is missing.",
        errors()[0]);
    ASSERT_EQ(0u, protocol_handlers.size());
  }

  // An invalid protocol handler with a url that doesn't contain the %s token.
  {
    auto& manifest = ParseManifest(R"({
          "protocol_handlers": [
            {
              "protocol": "web+github",
              "url": "http://foo.com/?profile="
            }
          ]
        })");
    auto& protocol_handlers = manifest->protocol_handlers;

    ASSERT_EQ(2u, GetErrorCount());
    EXPECT_EQ(
        "The url provided ('http://foo.com/?profile=') does not contain '%s'.",
        errors()[0]);
    EXPECT_EQ(
        "protocol_handlers entry ignored, required property 'url' is invalid.",
        errors()[1]);
    ASSERT_EQ(0u, protocol_handlers.size());
  }

  // An invalid protocol handler with a non-allowed protocol.
  {
    auto& manifest = ParseManifest(R"({
          "protocol_handlers": [
            {
              "protocol": "github",
              "url": "http://foo.com/?profile="
            }
          ]
        })");
    auto& protocol_handlers = manifest->protocol_handlers;

    ASSERT_EQ(2u, GetErrorCount());
    EXPECT_EQ(
        "The scheme 'github' doesn't belong to the scheme allowlist. Please "
        "prefix non-allowlisted schemes with the string 'web+'.",
        errors()[0]);
    EXPECT_EQ(
        "protocol_handlers entry ignored, required property 'protocol' is "
        "invalid.",
        errors()[1]);
    ASSERT_EQ(0u, protocol_handlers.size());
  }

  // Multiple valid protocol handlers
  {
    auto& manifest = ParseManifest(
        R"({
          "protocol_handlers": [
            {
              "protocol": "web+github",
              "url": "http://foo.com/?profile=%s"
            },
            {
              "protocol": "web+test",
              "url": "http://foo.com/?test=%s"
            },
            {
              "protocol": "web+relative",
              "url": "relativeURL=%s"
            }
          ]
        })");
    auto& protocol_handlers = manifest->protocol_handlers;

    ASSERT_EQ(0u, GetErrorCount());
    ASSERT_EQ(3u, protocol_handlers.size());

    ASSERT_EQ("web+github", protocol_handlers[0]->protocol);
    ASSERT_EQ("http://foo.com/?profile=%s", protocol_handlers[0]->url);
    ASSERT_EQ("web+test", protocol_handlers[1]->protocol);
    ASSERT_EQ("http://foo.com/?test=%s", protocol_handlers[1]->url);
    ASSERT_EQ("web+relative", protocol_handlers[2]->protocol);
    ASSERT_EQ("http://foo.com/relativeURL=%s", protocol_handlers[2]->url);
  }
}

TEST_F(ManifestParserTest, UrlHandlerParseRules) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(blink::features::kWebAppEnableUrlHandlers);

  // Manifest does not contain a 'url_handlers' field.
  {
    auto& manifest = ParseManifest("{ }");
    ASSERT_EQ(0u, GetErrorCount());
    EXPECT_EQ(0u, manifest->url_handlers.size());
  }

  // 'url_handlers' is not an array.
  {
    auto& manifest = ParseManifest(R"({ "url_handlers": { } })");
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'url_handlers' ignored, type array expected.",
              errors()[0]);
    EXPECT_EQ(0u, manifest->url_handlers.size());
  }

  // Contains 'url_handlers' field but no URL handler entries.
  {
    auto& manifest = ParseManifest(R"({ "url_handlers": [ ] })");
    ASSERT_EQ(0u, GetErrorCount());
    EXPECT_EQ(0u, manifest->url_handlers.size());
  }

  // 'url_handlers' array entries must be objects.
  {
    auto& manifest = ParseManifest(R"({
          "url_handlers": [
            "foo.com"
          ]
        })");
    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ("url_handlers entry ignored, type object expected.", errors()[0]);
    EXPECT_EQ(0u, manifest->url_handlers.size());
  }

  // A valid url handler.
  {
    auto& manifest = ParseManifest(R"({
          "url_handlers": [
            {
              "origin": "https://foo.com"
            }
          ]
        })");
    auto& url_handlers = manifest->url_handlers;

    ASSERT_EQ(0u, GetErrorCount());
    ASSERT_EQ(1u, url_handlers.size());
    ASSERT_TRUE(blink::SecurityOrigin::CreateFromString("https://foo.com")
                    ->IsSameOriginWith(url_handlers[0]->origin.get()));
  }

  // Scheme must be https.
  {
    auto& manifest = ParseManifest(R"({
          "url_handlers": [
            {
              "origin": "http://foo.com"
            }
          ]
        })");
    auto& url_handlers = manifest->url_handlers;

    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "url_handlers entry ignored, required property 'origin' must use the "
        "https scheme.",
        errors()[0]);
    ASSERT_EQ(0u, url_handlers.size());
  }

  // Origin must be valid.
  {
    auto& manifest = ParseManifest(R"({
          "url_handlers": [
            {
              "origin": "https:///////"
            }
          ]
        })");
    auto& url_handlers = manifest->url_handlers;

    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "url_handlers entry ignored, required property 'origin' is invalid.",
        errors()[0]);
    ASSERT_EQ(0u, url_handlers.size());
  }

  // Parse multiple valid handlers.
  {
    auto& manifest = ParseManifest(R"({
          "url_handlers": [
            {
              "origin": "https://foo.com"
            },
            {
              "origin": "https://bar.com"
            }
          ]
        })");
    auto& url_handlers = manifest->url_handlers;

    ASSERT_EQ(0u, GetErrorCount());
    ASSERT_EQ(2u, url_handlers.size());
    ASSERT_TRUE(blink::SecurityOrigin::CreateFromString("https://foo.com")
                    ->IsSameOriginWith(url_handlers[0]->origin.get()));
    ASSERT_TRUE(blink::SecurityOrigin::CreateFromString("https://bar.com")
                    ->IsSameOriginWith(url_handlers[1]->origin.get()));
  }

  // Parse both valid and invalid handlers.
  {
    auto& manifest = ParseManifest(R"({
          "url_handlers": [
            {
              "origin": "https://foo.com"
            },
            {
              "origin": "about:"
            }
          ]
        })");
    auto& url_handlers = manifest->url_handlers;

    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "url_handlers entry ignored, required property 'origin' is invalid.",
        errors()[0]);
    ASSERT_EQ(1u, url_handlers.size());
    ASSERT_TRUE(blink::SecurityOrigin::CreateFromString("https://foo.com")
                    ->IsSameOriginWith(url_handlers[0]->origin.get()));
  }

  // Parse invalid handler where the origin is a TLD.
  {
    auto& manifest = ParseManifest(R"({
          "url_handlers": [
            {
              "origin": "https://co.uk"
            }
          ]
        })");
    auto& url_handlers = manifest->url_handlers;

    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "url_handlers entry ignored, domain of required property 'origin' is "
        "invalid.",
        errors()[0]);
    ASSERT_EQ(0u, url_handlers.size());
  }

  // Parse origin with wildcard.
  {
    auto& manifest = ParseManifest(R"({
          "url_handlers": [
            {
              "origin": "https://*.foo.com"
            }
          ]
        })");
    auto& url_handlers = manifest->url_handlers;

    ASSERT_EQ(0u, GetErrorCount());
    ASSERT_EQ(1u, url_handlers.size());
    ASSERT_TRUE(blink::SecurityOrigin::CreateFromString("https://foo.com")
                    ->IsSameOriginWith(url_handlers[0]->origin.get()));
    ASSERT_TRUE(url_handlers[0]->has_origin_wildcard);
  }

  // Parse invalid origin wildcard format.
  {
    auto& manifest = ParseManifest(R"({
          "url_handlers": [
            {
              "origin": "https://*foo.com"
            }
          ]
        })");
    auto& url_handlers = manifest->url_handlers;

    ASSERT_EQ(0u, GetErrorCount());
    ASSERT_EQ(1u, url_handlers.size());
    ASSERT_TRUE(blink::SecurityOrigin::CreateFromString("https://*foo.com")
                    ->IsSameOriginWith(url_handlers[0]->origin.get()));
    ASSERT_FALSE(url_handlers[0]->has_origin_wildcard);
  }

  // Parse origin where the host is just the wildcard prefix.
  {
    auto& manifest = ParseManifest(R"({
          "url_handlers": [
            {
              "origin": "https://*."
            }
          ]
        })");
    auto& url_handlers = manifest->url_handlers;

    ASSERT_EQ(1u, GetErrorCount());
    ASSERT_EQ(
        "url_handlers entry ignored, domain of required property 'origin' is "
        "invalid.",
        errors()[0]);
    ASSERT_EQ(0u, url_handlers.size());
  }

  // Parse invalid origin where wildcard is used with a TLD.
  {
    auto& manifest = ParseManifest(R"({
          "url_handlers": [
            {
              "origin": "https://*.com"
            }
          ]
        })");
    auto& url_handlers = manifest->url_handlers;

    ASSERT_EQ(1u, GetErrorCount());
    ASSERT_EQ(
        "url_handlers entry ignored, domain of required property 'origin' is "
        "invalid.",
        errors()[0]);
    ASSERT_EQ(0u, url_handlers.size());
  }

  // Parse invalid origin where wildcard is used with an unknown TLD.
  {
    auto& manifest = ParseManifest(R"({
          "url_handlers": [
            {
              "origin": "https://*.foo"
            }
          ]
        })");
    auto& url_handlers = manifest->url_handlers;

    ASSERT_EQ(1u, GetErrorCount());
    ASSERT_EQ(
        "url_handlers entry ignored, domain of required property 'origin' is "
        "invalid.",
        errors()[0]);
    ASSERT_EQ(0u, url_handlers.size());
  }

  // Parse invalid origin where wildcard is used with a multipart TLD.
  {
    auto& manifest = ParseManifest(R"({
          "url_handlers": [
            {
              "origin": "https://*.co.uk"
            }
          ]
        })");
    auto& url_handlers = manifest->url_handlers;

    ASSERT_EQ(1u, GetErrorCount());
    ASSERT_EQ(
        "url_handlers entry ignored, domain of required property 'origin' is "
        "invalid.",
        errors()[0]);
    ASSERT_EQ(0u, url_handlers.size());
  }

  // Parse valid origin with private registry.
  {
    auto& manifest = ParseManifest(R"({
          "url_handlers": [
            {
              "origin": "https://*.glitch.me"
            }
          ]
        })");
    auto& url_handlers = manifest->url_handlers;

    ASSERT_EQ(0u, GetErrorCount());
    ASSERT_EQ(1u, url_handlers.size());
    ASSERT_TRUE(blink::SecurityOrigin::CreateFromString("https://glitch.me")
                    ->IsSameOriginWith(url_handlers[0]->origin.get()));
    ASSERT_TRUE(url_handlers[0]->has_origin_wildcard);
  }

  // Parse valid IP address as origin.
  {
    auto& manifest = ParseManifest(R"({
          "url_handlers": [
            {
              "origin": "https://192.168.0.1:8888"
            }
          ]
        })");
    auto& url_handlers = manifest->url_handlers;

    ASSERT_EQ(0u, GetErrorCount());
    ASSERT_EQ(1u, url_handlers.size());
    ASSERT_TRUE(
        blink::SecurityOrigin::CreateFromString("https://192.168.0.1:8888")
            ->IsSameOriginWith(url_handlers[0]->origin.get()));
    ASSERT_FALSE(url_handlers[0]->has_origin_wildcard);
  }

  // Validate only the first 10 handlers are parsed. The following manifest
  // specifies 11 handlers, so the last one should not be in the result.
  {
    auto& manifest = ParseManifest(
        R"({
          "url_handlers": [
            {
              "origin": "https://192.168.0.1:8001"
            },
            {
              "origin": "https://192.168.0.1:8002"
            },
            {
              "origin": "https://192.168.0.1:8003"
            },
            {
              "origin": "https://192.168.0.1:8004"
            },
            {
              "origin": "https://192.168.0.1:8005"
            },
            {
              "origin": "https://192.168.0.1:8006"
            },
            {
              "origin": "https://192.168.0.1:8007"
            },
            {
              "origin": "https://192.168.0.1:8008"
            },
            {
              "origin": "https://192.168.0.1:8009"
            },
            {
              "origin": "https://192.168.0.1:8010"
            },
            {
              "origin": "https://192.168.0.1:8011"
            }
          ]
        })");
    auto& url_handlers = manifest->url_handlers;

    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'url_handlers' contains more than 10 valid elements, "
        "only the first 10 are parsed.",
        errors()[0]);
    ASSERT_EQ(10u, url_handlers.size());
    ASSERT_TRUE(
        blink::SecurityOrigin::CreateFromString("https://192.168.0.1:8010")
            ->IsSameOriginWith(url_handlers[9]->origin.get()));
  }
}

TEST_F(ManifestParserTest, ScopeExtensionParseRules) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      blink::features::kWebAppEnableScopeExtensions);

  // Manifest does not contain a 'scope_extensions' field.
  {
    auto& manifest = ParseManifest("{ }");
    ASSERT_EQ(0u, GetErrorCount());
    EXPECT_EQ(0u, manifest->scope_extensions.size());
  }

  // 'scope_extensions' is not an array.
  {
    auto& manifest = ParseManifest(R"({ "scope_extensions": { } })");
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'scope_extensions' ignored, type array expected.",
              errors()[0]);
    EXPECT_EQ(0u, manifest->scope_extensions.size());
  }

  // Contains 'scope_extensions' field but no scope extension entries.
  {
    auto& manifest = ParseManifest(R"({ "scope_extensions": [ ] })");
    ASSERT_EQ(0u, GetErrorCount());
    EXPECT_EQ(0u, manifest->scope_extensions.size());
  }

  // Scope extension entry must be an object or a string.
  {
    auto& manifest = ParseManifest(R"({ "scope_extensions": [ 7 ] })");
    auto& scope_extensions = manifest->scope_extensions;

    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ("scope_extensions entry ignored, type string or object expected.",
              errors()[0]);
    EXPECT_EQ(0u, scope_extensions.size());
  }

  // A valid scope extension.
  {
    auto& manifest = ParseManifest(R"({
          "scope_extensions": [
            {
              "origin": "https://foo.com"
            }
          ]
        })");
    auto& scope_extensions = manifest->scope_extensions;

    ASSERT_EQ(0u, GetErrorCount());
    ASSERT_EQ(1u, scope_extensions.size());
    ASSERT_TRUE(blink::SecurityOrigin::CreateFromString("https://foo.com")
                    ->IsSameOriginWith(scope_extensions[0]->origin.get()));
  }

  // A valid scope extension in shorthand format.
  {
    auto& manifest = ParseManifest(R"({
          "scope_extensions": [
            "https://foo.com"
          ]
        })");
    auto& scope_extensions = manifest->scope_extensions;

    ASSERT_EQ(0u, GetErrorCount());
    ASSERT_EQ(1u, scope_extensions.size());
    ASSERT_TRUE(blink::SecurityOrigin::CreateFromString("https://foo.com")
                    ->IsSameOriginWith(scope_extensions[0]->origin.get()));
  }

  // Origin field is missing from the scope extension entry.
  {
    auto& manifest = ParseManifest(R"({
          "scope_extensions": [
            {
              "invalid_field": "https://foo.com"
            }
          ]
        })");
    auto& scope_extensions = manifest->scope_extensions;

    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "scope_extensions entry ignored, required property 'origin' is "
        "missing.",
        errors()[0]);
    EXPECT_EQ(0u, scope_extensions.size());
  }

  // Scope extension entry origin must be a string.
  {
    auto& manifest = ParseManifest(R"({
          "scope_extensions": [
            {
              "origin": 7
            }
          ]
        })");
    auto& scope_extensions = manifest->scope_extensions;

    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'origin' ignored, type string expected.", errors()[0]);
    EXPECT_EQ(0u, scope_extensions.size());
  }

  // Scheme must be https.
  {
    auto& manifest = ParseManifest(R"({
          "scope_extensions": [
            {
              "origin": "http://foo.com"
            }
          ]
        })");
    auto& scope_extensions = manifest->scope_extensions;

    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "scope_extensions entry ignored, required property 'origin' must use "
        "the https scheme.",
        errors()[0]);
    ASSERT_EQ(0u, scope_extensions.size());
  }

  // Scheme must be https in shorthand format.
  {
    auto& manifest = ParseManifest(R"({
          "scope_extensions": [
            "http://foo.com"
          ]
        })");
    auto& scope_extensions = manifest->scope_extensions;

    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "scope_extensions entry ignored, required property 'origin' must use "
        "the https scheme.",
        errors()[0]);
    ASSERT_EQ(0u, scope_extensions.size());
  }

  // Origin must be valid.
  {
    auto& manifest = ParseManifest(R"({
          "scope_extensions": [
            {
              "origin": "https:///////"
            }
          ]
        })");
    auto& scope_extensions = manifest->scope_extensions;

    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "scope_extensions entry ignored, required property 'origin' is "
        "invalid.",
        errors()[0]);
    ASSERT_EQ(0u, scope_extensions.size());
  }

  // Origin must be valid in shorthand format.
  {
    auto& manifest = ParseManifest(R"({
          "scope_extensions": [
            "https:///////"
          ]
        })");
    auto& scope_extensions = manifest->scope_extensions;

    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "scope_extensions entry ignored, required property 'origin' is "
        "invalid.",
        errors()[0]);
    ASSERT_EQ(0u, scope_extensions.size());
  }

  // Parse multiple valid scope extensions.
  {
    auto& manifest = ParseManifest(R"({
          "scope_extensions": [
            {
              "origin": "https://foo.com"
            },
            {
              "origin": "https://bar.com"
            }
          ]
        })");
    auto& scope_extensions = manifest->scope_extensions;

    ASSERT_EQ(0u, GetErrorCount());
    ASSERT_EQ(2u, scope_extensions.size());
    ASSERT_TRUE(blink::SecurityOrigin::CreateFromString("https://foo.com")
                    ->IsSameOriginWith(scope_extensions[0]->origin.get()));
    ASSERT_TRUE(blink::SecurityOrigin::CreateFromString("https://bar.com")
                    ->IsSameOriginWith(scope_extensions[1]->origin.get()));
  }

  // Parse multiple valid scope extensions in shorthand format.
  {
    auto& manifest = ParseManifest(R"({
          "scope_extensions": [
            "https://foo.com",
            "https://bar.com"
          ]
        })");
    auto& scope_extensions = manifest->scope_extensions;

    ASSERT_EQ(0u, GetErrorCount());
    ASSERT_EQ(2u, scope_extensions.size());
    ASSERT_TRUE(blink::SecurityOrigin::CreateFromString("https://foo.com")
                    ->IsSameOriginWith(scope_extensions[0]->origin.get()));
    ASSERT_TRUE(blink::SecurityOrigin::CreateFromString("https://bar.com")
                    ->IsSameOriginWith(scope_extensions[1]->origin.get()));
  }

  // Parse invalid scope extensions list with an array entry.
  {
    auto& manifest = ParseManifest(R"({
          "scope_extensions": [
            {
              "origin": "https://foo.com"
            },
            []
          ]
        })");
    auto& scope_extensions = manifest->scope_extensions;

    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ("scope_extensions entry ignored, type string or object expected.",
              errors()[0]);
    ASSERT_EQ(1u, scope_extensions.size());
    ASSERT_TRUE(blink::SecurityOrigin::CreateFromString("https://foo.com")
                    ->IsSameOriginWith(scope_extensions[0]->origin.get()));
  }

  // Parse invalid scope extensions list with an array entry in shorthand
  // format.
  {
    auto& manifest = ParseManifest(R"({
          "scope_extensions": [
            "https://foo.com",
            []
          ]
        })");
    auto& scope_extensions = manifest->scope_extensions;

    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ("scope_extensions entry ignored, type string or object expected.",
              errors()[0]);
    ASSERT_EQ(1u, scope_extensions.size());
    ASSERT_TRUE(blink::SecurityOrigin::CreateFromString("https://foo.com")
                    ->IsSameOriginWith(scope_extensions[0]->origin.get()));
  }

  // Parse invalid scope extensions list with entries in mixed formats.
  {
    auto& manifest = ParseManifest(R"({
          "scope_extensions": [
            {
              "origin": "https://foo.com"
            },
            "https://bar.com"
          ]
        })");
    auto& scope_extensions = manifest->scope_extensions;

    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ("scope_extensions entry ignored, type object expected.",
              errors()[0]);
    ASSERT_EQ(1u, scope_extensions.size());
    ASSERT_TRUE(blink::SecurityOrigin::CreateFromString("https://foo.com")
                    ->IsSameOriginWith(scope_extensions[0]->origin.get()));
  }

  // Parse both valid and invalid scope extensions.
  {
    auto& manifest = ParseManifest(R"({
          "scope_extensions": [
            {
              "origin": "https://foo.com"
            },
            {
              "origin": "about:"
            }
          ]
        })");
    auto& scope_extensions = manifest->scope_extensions;

    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "scope_extensions entry ignored, required property 'origin' is "
        "invalid.",
        errors()[0]);
    ASSERT_EQ(1u, scope_extensions.size());
    ASSERT_TRUE(blink::SecurityOrigin::CreateFromString("https://foo.com")
                    ->IsSameOriginWith(scope_extensions[0]->origin.get()));
  }

  // Parse both valid and invalid scope extensions in shorthand format.
  {
    auto& manifest = ParseManifest(R"({
          "scope_extensions": [
            "https://foo.com",
            "about:"
          ]
        })");
    auto& scope_extensions = manifest->scope_extensions;

    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "scope_extensions entry ignored, required property 'origin' is "
        "invalid.",
        errors()[0]);
    ASSERT_EQ(1u, scope_extensions.size());
    ASSERT_TRUE(blink::SecurityOrigin::CreateFromString("https://foo.com")
                    ->IsSameOriginWith(scope_extensions[0]->origin.get()));
  }

  // Parse invalid scope extension where the origin is a TLD.
  {
    auto& manifest = ParseManifest(R"({
          "scope_extensions": [
            {
              "origin": "https://co.uk"
            }
          ]
        })");
    auto& scope_extensions = manifest->scope_extensions;

    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "scope_extensions entry ignored, domain of required property 'origin' "
        "is invalid.",
        errors()[0]);
    ASSERT_EQ(0u, scope_extensions.size());
  }

  // Parse invalid scope extension where the origin is a TLD in shorthand
  // format.
  {
    auto& manifest = ParseManifest(R"({
          "scope_extensions": [
            "https://co.uk"
          ]
        })");
    auto& scope_extensions = manifest->scope_extensions;

    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "scope_extensions entry ignored, domain of required property 'origin' "
        "is invalid.",
        errors()[0]);
    ASSERT_EQ(0u, scope_extensions.size());
  }

  // Parse origin with wildcard.
  {
    auto& manifest = ParseManifest(R"({
          "scope_extensions": [
            {
              "origin": "https://*.foo.com"
            }
          ]
        })");
    auto& scope_extensions = manifest->scope_extensions;

    ASSERT_EQ(0u, GetErrorCount());
    ASSERT_EQ(1u, scope_extensions.size());
    ASSERT_TRUE(blink::SecurityOrigin::CreateFromString("https://foo.com")
                    ->IsSameOriginWith(scope_extensions[0]->origin.get()));
    ASSERT_TRUE(scope_extensions[0]->has_origin_wildcard);
  }

  // Parse origin with wildcard in shorthand format.
  {
    auto& manifest = ParseManifest(R"({
          "scope_extensions": [
            "https://*.foo.com"
          ]
        })");
    auto& scope_extensions = manifest->scope_extensions;

    ASSERT_EQ(0u, GetErrorCount());
    ASSERT_EQ(1u, scope_extensions.size());
    ASSERT_TRUE(blink::SecurityOrigin::CreateFromString("https://foo.com")
                    ->IsSameOriginWith(scope_extensions[0]->origin.get()));
    ASSERT_TRUE(scope_extensions[0]->has_origin_wildcard);
  }

  // Parse invalid origin wildcard format.
  {
    auto& manifest = ParseManifest(R"({
          "scope_extensions": [
            {
              "origin": "https://*foo.com"
            }
          ]
        })");
    auto& scope_extensions = manifest->scope_extensions;

    ASSERT_EQ(0u, GetErrorCount());
    ASSERT_EQ(1u, scope_extensions.size());
    ASSERT_TRUE(blink::SecurityOrigin::CreateFromString("https://*foo.com")
                    ->IsSameOriginWith(scope_extensions[0]->origin.get()));
    ASSERT_FALSE(scope_extensions[0]->has_origin_wildcard);
  }

  // Parse invalid origin wildcard format in shorthand format.
  {
    auto& manifest = ParseManifest(R"({
          "scope_extensions": [
            "https://*foo.com"
          ]
        })");
    auto& scope_extensions = manifest->scope_extensions;

    ASSERT_EQ(0u, GetErrorCount());
    ASSERT_EQ(1u, scope_extensions.size());
    ASSERT_TRUE(blink::SecurityOrigin::CreateFromString("https://*foo.com")
                    ->IsSameOriginWith(scope_extensions[0]->origin.get()));
    ASSERT_FALSE(scope_extensions[0]->has_origin_wildcard);
  }

  // Parse origin where the host is just the wildcard prefix.
  {
    auto& manifest = ParseManifest(R"({
          "scope_extensions": [
            {
              "origin": "https://*."
            }
          ]
        })");
    auto& scope_extensions = manifest->scope_extensions;

    ASSERT_EQ(1u, GetErrorCount());
    ASSERT_EQ(
        "scope_extensions entry ignored, domain of required property 'origin' "
        "is invalid.",
        errors()[0]);
    ASSERT_EQ(0u, scope_extensions.size());
  }

  // Parse origin where the host is just the wildcard prefix in shorthand
  // format.
  {
    auto& manifest = ParseManifest(R"({
          "scope_extensions": [
            "https://*."
          ]
        })");
    auto& scope_extensions = manifest->scope_extensions;

    ASSERT_EQ(1u, GetErrorCount());
    ASSERT_EQ(
        "scope_extensions entry ignored, domain of required property 'origin' "
        "is invalid.",
        errors()[0]);
    ASSERT_EQ(0u, scope_extensions.size());
  }

  // Parse invalid origin where wildcard is used with a TLD.
  {
    auto& manifest = ParseManifest(R"({
          "scope_extensions": [
            {
              "origin": "https://*.com"
            }
          ]
        })");
    auto& scope_extensions = manifest->scope_extensions;

    ASSERT_EQ(1u, GetErrorCount());
    ASSERT_EQ(
        "scope_extensions entry ignored, domain of required property 'origin' "
        "is invalid.",
        errors()[0]);
    ASSERT_EQ(0u, scope_extensions.size());
  }

  // Parse invalid origin where wildcard is used with a TLD in shorthand format.
  {
    auto& manifest = ParseManifest(R"({
          "scope_extensions": [
            "https://*.com"
          ]
        })");
    auto& scope_extensions = manifest->scope_extensions;

    ASSERT_EQ(1u, GetErrorCount());
    ASSERT_EQ(
        "scope_extensions entry ignored, domain of required property 'origin' "
        "is invalid.",
        errors()[0]);
    ASSERT_EQ(0u, scope_extensions.size());
  }

  // Parse invalid origin where wildcard is used with an unknown TLD.
  {
    auto& manifest = ParseManifest(R"({
          "scope_extensions": [
            {
              "origin": "https://*.foo"
            }
          ]
        })");
    auto& scope_extensions = manifest->scope_extensions;

    ASSERT_EQ(1u, GetErrorCount());
    ASSERT_EQ(
        "scope_extensions entry ignored, domain of required property 'origin' "
        "is invalid.",
        errors()[0]);
    ASSERT_EQ(0u, scope_extensions.size());
  }

  // Parse invalid origin where wildcard is used with an unknown TLD in
  // shorthand format.
  {
    auto& manifest = ParseManifest(R"({
          "scope_extensions": [
            "https://*.foo"
          ]
        })");
    auto& scope_extensions = manifest->scope_extensions;

    ASSERT_EQ(1u, GetErrorCount());
    ASSERT_EQ(
        "scope_extensions entry ignored, domain of required property 'origin' "
        "is invalid.",
        errors()[0]);
    ASSERT_EQ(0u, scope_extensions.size());
  }

  // Parse invalid origin where wildcard is used with a multipart TLD.
  {
    auto& manifest = ParseManifest(R"({
          "scope_extensions": [
            {
              "origin": "https://*.co.uk"
            }
          ]
        })");
    auto& scope_extensions = manifest->scope_extensions;

    ASSERT_EQ(1u, GetErrorCount());
    ASSERT_EQ(
        "scope_extensions entry ignored, domain of required property 'origin' "
        "is invalid.",
        errors()[0]);
    ASSERT_EQ(0u, scope_extensions.size());
  }

  // Parse invalid origin where wildcard is used with a multipart TLD in
  // shorthand format.
  {
    auto& manifest = ParseManifest(R"({
          "scope_extensions": [
            "https://*.co.uk"
          ]
        })");
    auto& scope_extensions = manifest->scope_extensions;

    ASSERT_EQ(1u, GetErrorCount());
    ASSERT_EQ(
        "scope_extensions entry ignored, domain of required property 'origin' "
        "is invalid.",
        errors()[0]);
    ASSERT_EQ(0u, scope_extensions.size());
  }

  // Parse valid origin with private registry.
  {
    auto& manifest = ParseManifest(R"({
          "scope_extensions": [
            {
              "origin": "https://*.glitch.me"
            }
          ]
        })");
    auto& scope_extensions = manifest->scope_extensions;

    ASSERT_EQ(0u, GetErrorCount());
    ASSERT_EQ(1u, scope_extensions.size());
    ASSERT_TRUE(blink::SecurityOrigin::CreateFromString("https://glitch.me")
                    ->IsSameOriginWith(scope_extensions[0]->origin.get()));
    ASSERT_TRUE(scope_extensions[0]->has_origin_wildcard);
  }

  // Parse valid origin with private registry in shorthand format.
  {
    auto& manifest = ParseManifest(R"({
          "scope_extensions": [
            "https://*.glitch.me"
          ]
        })");
    auto& scope_extensions = manifest->scope_extensions;

    ASSERT_EQ(0u, GetErrorCount());
    ASSERT_EQ(1u, scope_extensions.size());
    ASSERT_TRUE(blink::SecurityOrigin::CreateFromString("https://glitch.me")
                    ->IsSameOriginWith(scope_extensions[0]->origin.get()));
    ASSERT_TRUE(scope_extensions[0]->has_origin_wildcard);
  }

  // Parse valid IP address as origin.
  {
    auto& manifest = ParseManifest(R"({
          "scope_extensions": [
            {
              "origin": "https://192.168.0.1:8888"
            }
          ]
        })");
    auto& scope_extensions = manifest->scope_extensions;

    ASSERT_EQ(0u, GetErrorCount());
    ASSERT_EQ(1u, scope_extensions.size());
    ASSERT_TRUE(
        blink::SecurityOrigin::CreateFromString("https://192.168.0.1:8888")
            ->IsSameOriginWith(scope_extensions[0]->origin.get()));
    ASSERT_FALSE(scope_extensions[0]->has_origin_wildcard);
  }

  // Parse valid IP address as origin in shorthand format.
  {
    auto& manifest = ParseManifest(R"({
          "scope_extensions": [
            "https://192.168.0.1:8888"
          ]
        })");
    auto& scope_extensions = manifest->scope_extensions;

    ASSERT_EQ(0u, GetErrorCount());
    ASSERT_EQ(1u, scope_extensions.size());
    ASSERT_TRUE(
        blink::SecurityOrigin::CreateFromString("https://192.168.0.1:8888")
            ->IsSameOriginWith(scope_extensions[0]->origin.get()));
    ASSERT_FALSE(scope_extensions[0]->has_origin_wildcard);
  }

  // Validate only the first 10 scope extensions are parsed. The following
  // manifest specifies 11 scope extensions, so the last one should not be in
  // the result.
  {
    auto& manifest = ParseManifest(
        R"({
          "scope_extensions": [
            {
              "origin": "https://192.168.0.1:8001"
            },
            {
              "origin": "https://192.168.0.1:8002"
            },
            {
              "origin": "https://192.168.0.1:8003"
            },
            {
              "origin": "https://192.168.0.1:8004"
            },
            {
              "origin": "https://192.168.0.1:8005"
            },
            {
              "origin": "https://192.168.0.1:8006"
            },
            {
              "origin": "https://192.168.0.1:8007"
            },
            {
              "origin": "https://192.168.0.1:8008"
            },
            {
              "origin": "https://192.168.0.1:8009"
            },
            {
              "origin": "https://192.168.0.1:8010"
            },
            {
              "origin": "https://192.168.0.1:8011"
            }
          ]
        })");
    auto& scope_extensions = manifest->scope_extensions;

    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'scope_extensions' contains more than 10 valid elements, "
        "only the first 10 are parsed.",
        errors()[0]);
    ASSERT_EQ(10u, scope_extensions.size());
    ASSERT_TRUE(
        blink::SecurityOrigin::CreateFromString("https://192.168.0.1:8010")
            ->IsSameOriginWith(scope_extensions[9]->origin.get()));
  }

  // Validate only the first 10 scope extensions are parsed in shorthand format.
  // The following manifest specifies 11 scope extensions, so the last one
  // should not be in the result.
  {
    auto& manifest = ParseManifest(
        R"({
          "scope_extensions": [
            "https://192.168.0.1:8001",
            "https://192.168.0.1:8002",
            "https://192.168.0.1:8003",
            "https://192.168.0.1:8004",
            "https://192.168.0.1:8005",
            "https://192.168.0.1:8006",
            "https://192.168.0.1:8007",
            "https://192.168.0.1:8008",
            "https://192.168.0.1:8009",
            "https://192.168.0.1:8010",
            "https://192.168.0.1:8011"
          ]
        })");
    auto& scope_extensions = manifest->scope_extensions;

    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'scope_extensions' contains more than 10 valid elements, "
        "only the first 10 are parsed.",
        errors()[0]);
    ASSERT_EQ(10u, scope_extensions.size());
    ASSERT_TRUE(
        blink::SecurityOrigin::CreateFromString("https://192.168.0.1:8010")
            ->IsSameOriginWith(scope_extensions[9]->origin.get()));
  }
}

TEST_F(ManifestParserTest, LockScreenParseRules) {
  KURL manifest_url = KURL("https://foo.com/manifest.json");
  KURL document_url = KURL("https://foo.com/index.html");

  {
    // Manifest does not contain a 'lock_screen' field.
    auto& manifest = ParseManifest("{ }");
    ASSERT_EQ(0u, GetErrorCount());
    EXPECT_TRUE(manifest->lock_screen.is_null());
  }

  {
    // 'lock_screen' is not an object.
    auto& manifest = ParseManifest(R"( { "lock_screen": [ ] } )");
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'lock_screen' ignored, type object expected.",
              errors()[0]);
    EXPECT_TRUE(manifest->lock_screen.is_null());
  }

  {
    // Contains 'lock_screen' field but no start_url entry.
    auto& manifest = ParseManifest(R"( { "lock_screen": { } } )");
    ASSERT_EQ(0u, GetErrorCount());
    ASSERT_FALSE(manifest->lock_screen.is_null());
    EXPECT_TRUE(manifest->lock_screen->start_url.IsEmpty());
  }

  {
    // 'start_url' entries must be valid URLs.
    auto& manifest =
        ParseManifest(R"({ "lock_screen": { "start_url": {} } } )");
    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'start_url' ignored, type string expected.",
              errors()[0]);
    ASSERT_FALSE(manifest->lock_screen.is_null());
    EXPECT_TRUE(manifest->lock_screen->start_url.IsEmpty());
  }

  {
    // 'start_url' entries must be within scope.
    auto& manifest = ParseManifest(
        R"({ "lock_screen": { "start_url": "https://bar.com" } } )");
    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'start_url' ignored, should be within scope of the manifest.",
        errors()[0]);
    ASSERT_FALSE(manifest->lock_screen.is_null());
    EXPECT_TRUE(manifest->lock_screen->start_url.IsEmpty());
  }

  {
    // A valid lock_screen start_url entry.
    auto& manifest = ParseManifestWithURLs(
        R"({
          "lock_screen": {
            "start_url": "https://foo.com"
          }
        })",
        manifest_url, document_url);
    ASSERT_EQ(0u, GetErrorCount());
    ASSERT_FALSE(manifest->lock_screen.is_null());
    EXPECT_EQ("https://foo.com/", manifest->lock_screen->start_url.GetString());
  }

  {
    // A valid lock_screen start_url entry, parsed relative to manifest URL.
    auto& manifest = ParseManifestWithURLs(
        R"({
          "lock_screen": {
            "start_url": "new_note"
          }
        })",
        manifest_url, document_url);
    ASSERT_EQ(0u, GetErrorCount());
    ASSERT_FALSE(manifest->lock_screen.is_null());
    EXPECT_EQ("https://foo.com/new_note",
              manifest->lock_screen->start_url.GetString());
  }
}

TEST_F(ManifestParserTest, NoteTakingParseRules) {
  KURL manifest_url = KURL("https://foo.com/manifest.json");
  KURL document_url = KURL("https://foo.com/index.html");

  {
    // Manifest does not contain a 'note_taking' field.
    auto& manifest = ParseManifest("{ }");
    ASSERT_EQ(0u, GetErrorCount());
    EXPECT_TRUE(manifest->note_taking.is_null());
  }

  {
    // 'note_taking' is not an object.
    auto& manifest = ParseManifest(R"( { "note_taking": [ ] } )");
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'note_taking' ignored, type object expected.",
              errors()[0]);
    EXPECT_TRUE(manifest->note_taking.is_null());
  }

  {
    // Contains 'note_taking' field but no new_note_url entry.
    auto& manifest = ParseManifest(R"( { "note_taking": { } } )");
    ASSERT_EQ(0u, GetErrorCount());
    ASSERT_FALSE(manifest->note_taking.is_null());
    EXPECT_TRUE(manifest->note_taking->new_note_url.IsEmpty());
  }

  {
    // 'new_note_url' entries must be valid URLs.
    auto& manifest =
        ParseManifest(R"({ "note_taking": { "new_note_url": {} } } )");
    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'new_note_url' ignored, type string expected.",
              errors()[0]);
    ASSERT_FALSE(manifest->note_taking.is_null());
    EXPECT_TRUE(manifest->note_taking->new_note_url.IsEmpty());
  }

  {
    // 'new_note_url' entries must be within scope.
    auto& manifest = ParseManifest(
        R"({ "note_taking": { "new_note_url": "https://bar.com" } } )");
    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'new_note_url' ignored, should be within scope of the "
        "manifest.",
        errors()[0]);
    ASSERT_FALSE(manifest->note_taking.is_null());
    EXPECT_TRUE(manifest->note_taking->new_note_url.IsEmpty());
  }

  {
    // A valid note_taking new_note_url entry.
    auto& manifest = ParseManifestWithURLs(
        R"({
          "note_taking": {
            "new_note_url": "https://foo.com"
          }
        })",
        manifest_url, document_url);
    ASSERT_EQ(0u, GetErrorCount());
    ASSERT_FALSE(manifest->note_taking.is_null());
    EXPECT_EQ("https://foo.com/",
              manifest->note_taking->new_note_url.GetString());
  }

  {
    // A valid note_taking new_note_url entry, parsed relative to manifest URL.
    auto& manifest = ParseManifestWithURLs(
        R"({
          "note_taking": {
            "new_note_url": "new_note"
          }
        })",
        manifest_url, document_url);
    ASSERT_EQ(0u, GetErrorCount());
    ASSERT_FALSE(manifest->note_taking.is_null());
    EXPECT_EQ("https://foo.com/new_note",
              manifest->note_taking->new_note_url.GetString());
  }
}

TEST_F(ManifestParserTest, ShareTargetParseRules) {
  // Contains share_target field but no keys.
  {
    auto& manifest = ParseManifest(R"({ "share_target": {} })");
    EXPECT_FALSE(manifest->share_target.get());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'share_target' ignored. Property 'action' is invalid.",
              errors()[0]);
  }

  // Contains share_target field but no params key.
  {
    auto& manifest = ParseManifest(R"({ "share_target": { "action": "" } })");
    EXPECT_FALSE(manifest->share_target.get());
    EXPECT_EQ(3u, GetErrorCount());
    EXPECT_EQ(
        "Method should be set to either GET or POST. It currently defaults to "
        "GET.",
        errors()[0]);
    EXPECT_EQ(
        "Enctype should be set to either application/x-www-form-urlencoded or "
        "multipart/form-data. It currently defaults to "
        "application/x-www-form-urlencoded",
        errors()[1]);
    EXPECT_EQ(
        "property 'share_target' ignored. Property 'params' type "
        "dictionary expected.",
        errors()[2]);
  }

  // Contains share_target field but no action key.
  {
    auto& manifest = ParseManifest(R"({ "share_target": { "params": {} } })");
    EXPECT_FALSE(manifest->share_target.get());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'share_target' ignored. Property 'action' is invalid.",
              errors()[0]);
  }

  // Key in share_target that isn't valid.
  {
    auto& manifest = ParseManifest(
        R"({ "share_target": {"incorrect_key": "some_value" } })");
    ASSERT_FALSE(manifest->share_target.get());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'share_target' ignored. Property 'action' is invalid.",
              errors()[0]);
  }
}

TEST_F(ManifestParserTest, ShareTargetUrlTemplateParseRules) {
  KURL manifest_url = KURL("https://foo.com/manifest.json");
  KURL document_url = KURL("https://foo.com/index.html");

  // Contains share_target, but action is empty.
  {
    auto& manifest = ParseManifestWithURLs(
        R"({ "share_target": { "action": "", "params": {} } })", manifest_url,
        document_url);
    ASSERT_TRUE(manifest->share_target.get());
    EXPECT_EQ(manifest->share_target->action, manifest_url);
    EXPECT_TRUE(manifest->share_target->params->text.IsNull());
    EXPECT_TRUE(manifest->share_target->params->title.IsNull());
    EXPECT_TRUE(manifest->share_target->params->url.IsNull());
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(2u, GetErrorCount());
    EXPECT_EQ(
        "Method should be set to either GET or POST. It currently defaults to "
        "GET.",
        errors()[0]);
    EXPECT_EQ(
        "Enctype should be set to either application/x-www-form-urlencoded or "
        "multipart/form-data. It currently defaults to "
        "application/x-www-form-urlencoded",
        errors()[1]);
  }

  // Parse but throw an error if url_template property isn't a string.
  {
    auto& manifest = ParseManifestWithURLs(
        R"({ "share_target": { "action": "", "params": {} } })", manifest_url,
        document_url);
    EXPECT_TRUE(manifest->share_target.get());
    EXPECT_EQ(manifest->share_target->action, manifest_url);
    EXPECT_TRUE(manifest->share_target->params->text.IsNull());
    EXPECT_TRUE(manifest->share_target->params->title.IsNull());
    EXPECT_TRUE(manifest->share_target->params->url.IsNull());
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(2u, GetErrorCount());
    EXPECT_EQ(
        "Method should be set to either GET or POST. It currently defaults to "
        "GET.",
        errors()[0]);
    EXPECT_EQ(
        "Enctype should be set to either application/x-www-form-urlencoded or "
        "multipart/form-data. It currently defaults to "
        "application/x-www-form-urlencoded",
        errors()[1]);
  }

  // Don't parse if action property isn't a string.
  {
    auto& manifest = ParseManifestWithURLs(
        R"({ "share_target": { "action": {}, "params": {} } })", manifest_url,
        document_url);
    EXPECT_FALSE(manifest->share_target.get());
    EXPECT_EQ(2u, GetErrorCount());
    EXPECT_EQ("property 'action' ignored, type string expected.", errors()[0]);
    EXPECT_EQ("property 'share_target' ignored. Property 'action' is invalid.",
              errors()[1]);
  }

  // Don't parse if action property isn't a string.
  {
    auto& manifest = ParseManifestWithURLs(
        R"({ "share_target": { "action": 42, "params": {} } })", manifest_url,
        document_url);
    EXPECT_FALSE(manifest->share_target.get());
    EXPECT_EQ(2u, GetErrorCount());
    EXPECT_EQ("property 'action' ignored, type string expected.", errors()[0]);
    EXPECT_EQ("property 'share_target' ignored. Property 'action' is invalid.",
              errors()[1]);
  }

  // Don't parse if params property isn't a dict.
  {
    auto& manifest = ParseManifestWithURLs(
        R"({ "share_target": { "action": "", "params": "" } })", manifest_url,
        document_url);
    EXPECT_FALSE(manifest->share_target.get());
    EXPECT_EQ(3u, GetErrorCount());
    EXPECT_EQ(
        "Method should be set to either GET or POST. It currently defaults to "
        "GET.",
        errors()[0]);
    EXPECT_EQ(
        "Enctype should be set to either application/x-www-form-urlencoded or "
        "multipart/form-data. It currently defaults to "
        "application/x-www-form-urlencoded",
        errors()[1]);
    EXPECT_EQ(
        "property 'share_target' ignored. Property 'params' type "
        "dictionary expected.",
        errors()[2]);
  }

  // Don't parse if params property isn't a dict.
  {
    auto& manifest = ParseManifestWithURLs(
        R"({ "share_target": { "action": "", "params": 42 } })", manifest_url,
        document_url);
    EXPECT_FALSE(manifest->share_target.get());
    EXPECT_EQ(3u, GetErrorCount());
    EXPECT_EQ(
        "Method should be set to either GET or POST. It currently defaults to "
        "GET.",
        errors()[0]);
    EXPECT_EQ(
        "Enctype should be set to either application/x-www-form-urlencoded or "
        "multipart/form-data. It currently defaults to "
        "application/x-www-form-urlencoded",
        errors()[1]);
    EXPECT_EQ(
        "property 'share_target' ignored. Property 'params' type "
        "dictionary expected.",
        errors()[2]);
  }

  // Ignore params keys with invalid types.
  {
    auto& manifest = ParseManifestWithURLs(
        R"({ "share_target": { "action": "", "params": { "text": 42 }
         } })",
        manifest_url, document_url);
    ASSERT_TRUE(manifest->share_target.get());
    EXPECT_EQ(manifest->share_target->action, manifest_url);
    EXPECT_TRUE(manifest->share_target->params->text.IsNull());
    EXPECT_TRUE(manifest->share_target->params->title.IsNull());
    EXPECT_TRUE(manifest->share_target->params->url.IsNull());
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(3u, GetErrorCount());
    EXPECT_EQ(
        "Method should be set to either GET or POST. It currently defaults to "
        "GET.",
        errors()[0]);
    EXPECT_EQ(
        "Enctype should be set to either application/x-www-form-urlencoded or "
        "multipart/form-data. It currently defaults to "
        "application/x-www-form-urlencoded",
        errors()[1]);
    EXPECT_EQ("property 'text' ignored, type string expected.", errors()[2]);
  }

  // Ignore params keys with invalid types.
  {
    auto& manifest = ParseManifestWithURLs(
        R"({ "share_target": { "action": "",
        "params": { "title": 42 } } })",
        manifest_url, document_url);
    ASSERT_TRUE(manifest->share_target.get());
    EXPECT_EQ(manifest->share_target->action, manifest_url);
    EXPECT_TRUE(manifest->share_target->params->text.IsNull());
    EXPECT_TRUE(manifest->share_target->params->title.IsNull());
    EXPECT_TRUE(manifest->share_target->params->url.IsNull());
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(3u, GetErrorCount());
    EXPECT_EQ(
        "Method should be set to either GET or POST. It currently defaults to "
        "GET.",
        errors()[0]);
    EXPECT_EQ(
        "Enctype should be set to either application/x-www-form-urlencoded or "
        "multipart/form-data. It currently defaults to "
        "application/x-www-form-urlencoded",
        errors()[1]);
    EXPECT_EQ("property 'title' ignored, type string expected.", errors()[2]);
  }

  // Don't parse if params property has keys with invalid types.
  {
    auto& manifest = ParseManifestWithURLs(
        R"({ "share_target": { "action": "", "params": { "url": {},
        "text": "hi" } } })",
        manifest_url, document_url);
    ASSERT_TRUE(manifest->share_target.get());
    EXPECT_EQ(manifest->share_target->action, manifest_url);
    EXPECT_EQ(manifest->share_target->params->text, "hi");
    EXPECT_TRUE(manifest->share_target->params->title.IsNull());
    EXPECT_TRUE(manifest->share_target->params->url.IsNull());
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(3u, GetErrorCount());
    EXPECT_EQ(
        "Method should be set to either GET or POST. It currently defaults to "
        "GET.",
        errors()[0]);
    EXPECT_EQ(
        "Enctype should be set to either application/x-www-form-urlencoded or "
        "multipart/form-data. It currently defaults to "
        "application/x-www-form-urlencoded",
        errors()[1]);
    EXPECT_EQ("property 'url' ignored, type string expected.", errors()[2]);
  }

  // Don't parse if action property isn't a valid URL.
  {
    auto& manifest = ParseManifestWithURLs(
        R"({ "share_target": { "action": "https://foo.com:a", "params":
        {} } })",
        manifest_url, document_url);
    EXPECT_FALSE(manifest->share_target.get());
    EXPECT_EQ(2u, GetErrorCount());
    EXPECT_EQ("property 'action' ignored, URL is invalid.", errors()[0]);
    EXPECT_EQ("property 'share_target' ignored. Property 'action' is invalid.",
              errors()[1]);
  }

  // Fail parsing if action is at a different origin than the Web
  // manifest.
  {
    auto& manifest = ParseManifestWithURLs(
        R"({ "share_target": { "action": "https://foo2.com/",
        "params": {} } })",
        manifest_url, document_url);
    EXPECT_FALSE(manifest->share_target.get());
    EXPECT_EQ(2u, GetErrorCount());
    EXPECT_EQ(
        "property 'action' ignored, should be within scope of the manifest.",
        errors()[0]);
    EXPECT_EQ(
        "property 'share_target' ignored. Property 'action' is "
        "invalid.",
        errors()[1]);
  }

  // Fail parsing if action is not within scope of the manifest.
  {
    auto& manifest = ParseManifestWithURLs(
        R"({ "start_url": "/app/",
          "scope": "/app/",
          "share_target": { "action": "/",
        "params": {} } })",
        manifest_url, document_url);
    EXPECT_FALSE(manifest->share_target.get());
    EXPECT_EQ(2u, GetErrorCount());
    EXPECT_EQ(
        "property 'action' ignored, should be within scope of the manifest.",
        errors()[0]);
    EXPECT_EQ(
        "property 'share_target' ignored. Property 'action' is "
        "invalid.",
        errors()[1]);
  }

  // Smoke test: Contains share_target and action, and action is valid.
  {
    auto& manifest = ParseManifestWithURLs(
        R"({ "share_target": {"action": "share/", "params": {} } })",
        manifest_url, document_url);
    ASSERT_TRUE(manifest->share_target.get());
    EXPECT_EQ(manifest->share_target->action.GetString(),
              "https://foo.com/share/");
    EXPECT_TRUE(manifest->share_target->params->text.IsNull());
    EXPECT_TRUE(manifest->share_target->params->title.IsNull());
    EXPECT_TRUE(manifest->share_target->params->url.IsNull());
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(2u, GetErrorCount());
    EXPECT_EQ(
        "Method should be set to either GET or POST. It currently defaults to "
        "GET.",
        errors()[0]);
    EXPECT_EQ(
        "Enctype should be set to either application/x-www-form-urlencoded or "
        "multipart/form-data. It currently defaults to "
        "application/x-www-form-urlencoded",
        errors()[1]);
  }

  // Smoke test: Contains share_target and action, and action is valid, params
  // is populated.
  {
    auto& manifest = ParseManifestWithURLs(
        R"({ "share_target": {"action": "share/", "params": { "text":
        "foo", "title": "bar", "url": "baz" } } })",
        manifest_url, document_url);
    ASSERT_TRUE(manifest->share_target.get());
    EXPECT_EQ(manifest->share_target->action.GetString(),
              "https://foo.com/share/");
    EXPECT_EQ(manifest->share_target->params->text, "foo");
    EXPECT_EQ(manifest->share_target->params->title, "bar");
    EXPECT_EQ(manifest->share_target->params->url, "baz");
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(2u, GetErrorCount());
    EXPECT_EQ(
        "Method should be set to either GET or POST. It currently defaults to "
        "GET.",
        errors()[0]);
    EXPECT_EQ(
        "Enctype should be set to either application/x-www-form-urlencoded or "
        "multipart/form-data. It currently defaults to "
        "application/x-www-form-urlencoded",
        errors()[1]);
  }

  // Backwards compatibility test: Contains share_target, url_template and
  // action, and action is valid, params is populated.
  {
    auto& manifest = ParseManifestWithURLs(
        R"({ "share_target": { "url_template":
        "foo.com/share?title={title}",
        "action": "share/", "params": { "text":
        "foo", "title": "bar", "url": "baz" } } })",
        manifest_url, document_url);
    ASSERT_TRUE(manifest->share_target.get());
    EXPECT_EQ(manifest->share_target->action.GetString(),
              "https://foo.com/share/");
    EXPECT_EQ(manifest->share_target->params->text, "foo");
    EXPECT_EQ(manifest->share_target->params->title, "bar");
    EXPECT_EQ(manifest->share_target->params->url, "baz");
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(2u, GetErrorCount());
    EXPECT_EQ(
        "Method should be set to either GET or POST. It currently defaults to "
        "GET.",
        errors()[0]);
    EXPECT_EQ(
        "Enctype should be set to either application/x-www-form-urlencoded or "
        "multipart/form-data. It currently defaults to "
        "application/x-www-form-urlencoded",
        errors()[1]);
  }

  // Smoke test: Contains share_target, action and params. action is
  // valid and is absolute.
  {
    auto& manifest = ParseManifestWithURLs(
        R"({ "share_target": { "action": "https://foo.com/#", "params":
        { "title": "mytitle" } }
        })",
        manifest_url, document_url);
    ASSERT_TRUE(manifest->share_target.get());
    EXPECT_EQ(manifest->share_target->action.GetString(), "https://foo.com/#");
    EXPECT_TRUE(manifest->share_target->params->text.IsNull());
    EXPECT_EQ(manifest->share_target->params->title, "mytitle");
    EXPECT_TRUE(manifest->share_target->params->url.IsNull());
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(2u, GetErrorCount());
    EXPECT_EQ(
        "Method should be set to either GET or POST. It currently defaults to "
        "GET.",
        errors()[0]);
    EXPECT_EQ(
        "Enctype should be set to either application/x-www-form-urlencoded or "
        "multipart/form-data. It currently defaults to "
        "application/x-www-form-urlencoded",
        errors()[1]);
  }

  // Return undefined if method or enctype is not string.
  {
    auto& manifest = ParseManifestWithURLs(
        R"({ "share_target": { "action": "https://foo.com/#", "method":
        10, "enctype": 10, "params":
        { "title": "mytitle" } }
        })",
        manifest_url, document_url);
    EXPECT_FALSE(manifest->share_target.get());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "invalid method. Allowed methods are:"
        "GET and POST.",
        errors()[0]);
  }

  // Valid method and enctype.
  {
    auto& manifest = ParseManifestWithURLs(
        R"({ "share_target": { "action": "https://foo.com/#", "method":
        "GET", "enctype": "application/x-www-form-urlencoded",
        "params":
        { "title": "mytitle" } }
        })",
        manifest_url, document_url);
    EXPECT_TRUE(manifest->share_target.get());
    EXPECT_EQ(manifest->share_target->method,
              mojom::blink::ManifestShareTarget::Method::kGet);
    EXPECT_EQ(manifest->share_target->enctype,
              mojom::blink::ManifestShareTarget::Enctype::kFormUrlEncoded);
  }

  // Auto-fill in "GET" for method and "application/x-www-form-urlencoded" for
  // enctype.
  {
    auto& manifest = ParseManifestWithURLs(
        R"({ "share_target": { "action": "https://foo.com/#", "params":
        { "title": "mytitle" } }
        })",
        manifest_url, document_url);
    EXPECT_TRUE(manifest->share_target.get());
    EXPECT_EQ(manifest->share_target->method,
              mojom::blink::ManifestShareTarget::Method::kGet);
    EXPECT_EQ(manifest->share_target->enctype,
              mojom::blink::ManifestShareTarget::Enctype::kFormUrlEncoded);
  }

  // Invalid method values, return undefined.
  {
    auto& manifest = ParseManifestWithURLs(
        R"({ "share_target": { "action": "https://foo.com/#", "method":
        "", "enctype": "application/x-www-form-urlencoded", "params":
        { "title": "mytitle" } }
        })",
        manifest_url, document_url);
    EXPECT_FALSE(manifest->share_target.get());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "invalid method. Allowed methods are:"
        "GET and POST.",
        errors()[0]);
  }

  // When method is "GET", enctype cannot be anything other than
  // "application/x-www-form-urlencoded".
  {
    auto& manifest = ParseManifestWithURLs(
        R"({ "share_target": { "action": "https://foo.com/#", "method":
        "GET", "enctype": "RANDOM", "params":
        { "title": "mytitle" } }
        })",
        manifest_url, document_url);
    EXPECT_FALSE(manifest->share_target.get());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "invalid enctype. Allowed enctypes are:"
        "application/x-www-form-urlencoded and multipart/form-data.",
        errors()[0]);
  }

  // When method is "POST", enctype cannot be anything other than
  // "application/x-www-form-urlencoded" or "multipart/form-data".
  {
    auto& manifest = ParseManifestWithURLs(
        R"({ "share_target": { "action": "https://foo.com/#", "method":
        "POST", "enctype": "random", "params":
        { "title": "mytitle" } }
        })",
        manifest_url, document_url);
    EXPECT_FALSE(manifest->share_target.get());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "invalid enctype. Allowed enctypes are:"
        "application/x-www-form-urlencoded and multipart/form-data.",
        errors()[0]);
  }

  // Valid enctype for when method is "POST".
  {
    auto& manifest = ParseManifestWithURLs(
        R"( { "share_target": { "action": "https://foo.com/#", "method":
        "POST", "enctype": "application/x-www-form-urlencoded",
        "params":
        { "title": "mytitle" } }
        })",
        manifest_url, document_url);
    EXPECT_TRUE(manifest->share_target.get());
    EXPECT_EQ(manifest->share_target->method,
              mojom::blink::ManifestShareTarget::Method::kPost);
    EXPECT_EQ(manifest->share_target->enctype,
              mojom::blink::ManifestShareTarget::Enctype::kFormUrlEncoded);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Valid enctype for when method is "POST".
  {
    auto& manifest = ParseManifestWithURLs(
        R"({ "share_target": { "action": "https://foo.com/#", "method":
        "POST", "enctype": "multipart/form-data", "params":
        { "title": "mytitle" } }
        })",
        manifest_url, document_url);
    EXPECT_TRUE(manifest->share_target.get());
    EXPECT_EQ(manifest->share_target->method,
              mojom::blink::ManifestShareTarget::Method::kPost);
    EXPECT_EQ(manifest->share_target->enctype,
              mojom::blink::ManifestShareTarget::Enctype::kMultipartFormData);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Ascii in-sensitive.
  {
    auto& manifest = ParseManifestWithURLs(
        R"({ "share_target": { "action": "https://foo.com/#", "method":
        "PosT", "enctype": "mUltIparT/Form-dAta", "params":
        { "title": "mytitle" } }
        })",
        manifest_url, document_url);
    EXPECT_TRUE(manifest->share_target.get());
    EXPECT_EQ(manifest->share_target->method,
              mojom::blink::ManifestShareTarget::Method::kPost);
    EXPECT_EQ(manifest->share_target->enctype,
              mojom::blink::ManifestShareTarget::Enctype::kMultipartFormData);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // No files is okay.
  {
    auto& manifest = ParseManifestWithURLs(
        R"({ "share_target": { "action": "https://foo.com/#", "method":
        "POST", "enctype": "multipart/form-data", "params":
        { "title": "mytitle", "files": [] } }
        })",
        manifest_url, document_url);
    EXPECT_TRUE(manifest->share_target.get());
    EXPECT_EQ(manifest->share_target->method,
              mojom::blink::ManifestShareTarget::Method::kPost);
    EXPECT_EQ(manifest->share_target->enctype,
              mojom::blink::ManifestShareTarget::Enctype::kMultipartFormData);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Nonempty file must have POST method and multipart/form-data enctype.
  // GET method, for example, will cause an error in this case.
  {
    auto& manifest = ParseManifestWithURLs(
        R"({ "share_target": { "action": "https://foo.com/#", "method":
        "GET", "enctype": "multipart/form-data", "params":
        { "title": "mytitle", "files": [{ "name": "name",
        "accept": ["text/plain"]}] } }
        })",
        manifest_url, document_url);
    EXPECT_FALSE(manifest->share_target.get());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "invalid enctype for GET method. Only "
        "application/x-www-form-urlencoded is allowed.",
        errors()[0]);
  }

  // Nonempty file must have POST method and multipart/form-data enctype.
  // Enctype other than multipart/form-data will cause an error.
  {
    auto& manifest = ParseManifestWithURLs(
        R"({ "share_target": { "action": "https://foo.com/#", "method":
        "POST", "enctype": "application/x-www-form-urlencoded",
        "params":
        { "title": "mytitle", "files": [{ "name": "name",
        "accept": ["text/plain"]}] } }
        })",
        manifest_url, document_url);
    EXPECT_FALSE(manifest->share_target.get());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("files are only supported with multipart/form-data POST.",
              errors()[0]);
  }

  // Nonempty file must have POST method and multipart/form-data enctype.
  // This case is valid.
  {
    auto& manifest = ParseManifestWithURLs(
        R"({ "share_target": { "action": "https://foo.com/#", "method":
        "POST", "enctype": "multipart/form-data", "params":
        { "title": "mytitle", "files": [{ "name": "name",
        "accept": ["text/plain"]}] } }
        })",
        manifest_url, document_url);
    EXPECT_TRUE(manifest->share_target.get());
    EXPECT_TRUE(manifest->share_target->params->files.has_value());
    EXPECT_EQ(1u, manifest->share_target->params->files->size());
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Invalid mimetype.
  {
    auto& manifest = ParseManifestWithURLs(
        R"({ "share_target": { "action": "https://foo.com/#", "method":
        "POST", "enctype": "multipart/form-data", "params":
        { "title": "mytitle", "files": [{ "name": "name",
        "accept": [""]}] } }
        })",
        manifest_url, document_url);
    EXPECT_FALSE(manifest->share_target.get());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("invalid mime type inside files.", errors()[0]);
  }

  // Invalid mimetype.
  {
    auto& manifest = ParseManifestWithURLs(
        R"({ "share_target": { "action": "https://foo.com/#", "method":
        "POST", "enctype": "multipart/form-data", "params":
        { "title": "mytitle", "files": [{ "name": "name",
        "accept": ["helloworld"]}] } }
        })",
        manifest_url, document_url);
    EXPECT_FALSE(manifest->share_target.get());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("invalid mime type inside files.", errors()[0]);
  }

  // Invalid mimetype.
  {
    auto& manifest = ParseManifestWithURLs(
        R"({ "share_target": { "action": "https://foo.com/#", "method":
        "POST", "enctype": "multipart/form-data", "params":
        { "title": "mytitle", "files": [{ "name": "name",
        "accept": ["^$/@$"]}] } }
        })",
        manifest_url, document_url);
    EXPECT_FALSE(manifest->share_target.get());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("invalid mime type inside files.", errors()[0]);
  }

  // Invalid mimetype.
  {
    auto& manifest = ParseManifestWithURLs(
        R"({ "share_target": { "action": "https://foo.com/#", "method":
        "POST", "enctype": "multipart/form-data", "params":
        { "title": "mytitle", "files": [{ "name": "name",
        "accept": ["/"]}] } }
        })",
        manifest_url, document_url);
    EXPECT_FALSE(manifest->share_target.get());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("invalid mime type inside files.", errors()[0]);
  }

  // Invalid mimetype.
  {
    auto& manifest = ParseManifestWithURLs(
        R"({ "share_target": { "action": "https://foo.com/#", "method":
        "POST", "enctype": "multipart/form-data", "params":
        { "title": "mytitle", "files": [{ "name": "name",
        "accept": [" "]}] } }
        })",
        manifest_url, document_url);
    EXPECT_FALSE(manifest->share_target.get());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("invalid mime type inside files.", errors()[0]);
  }

  // Accept field is empty.
  {
    auto& manifest = ParseManifestWithURLs(
        R"({ "share_target": { "action": "https://foo.com/#", "method":
        "POST", "enctype": "multipart/form-data", "params":
        { "title": "mytitle", "files": [{ "name": "name",
        "accept": []}] } }
        })",
        manifest_url, document_url);
    EXPECT_TRUE(manifest->share_target.get());
    EXPECT_FALSE(manifest->share_target->params->files.has_value());
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept sequence contains non-string elements.
  {
    auto& manifest = ParseManifestWithURLs(
        R"({
          "share_target": {
            "action": "https://foo.com/#",
            "method": "POST",
            "enctype": "multipart/form-data",
            "params": {
              "title": "mytitle",
              "files": [{
                "name": "name",
                "accept": ["image/png", 42]
              }]
            }
          }
        })",
        manifest_url, document_url);
    auto* share_target = manifest->share_target.get();
    EXPECT_TRUE(share_target);

    EXPECT_TRUE(share_target->params->files.has_value());
    auto& files = share_target->params->files.value();
    EXPECT_EQ(1u, files.size());
    EXPECT_EQ(files[0]->name, "name");

    auto& accept = files[0]->accept;
    EXPECT_EQ(1u, accept.size());
    EXPECT_EQ(accept[0], "image/png");

    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("'accept' entry ignored, expected to be of type string.",
              errors()[0]);
  }

  // Accept is just a single string.
  {
    auto& manifest = ParseManifestWithURLs(
        R"({
          "share_target": {
            "action": "https://foo.com/#",
            "method": "POST",
            "enctype": "multipart/form-data",
            "params": {
              "title": "mytitle",
              "files": [{
                "name": "name",
                "accept": "image/png"
              }]
            }
          }
        })",
        manifest_url, document_url);
    auto* share_target = manifest->share_target.get();
    EXPECT_TRUE(share_target);

    EXPECT_TRUE(share_target->params->files.has_value());
    auto& files = share_target->params->files.value();
    EXPECT_EQ(1u, files.size());
    EXPECT_EQ(files[0]->name, "name");

    auto& accept = files[0]->accept;
    EXPECT_EQ(1u, accept.size());
    EXPECT_EQ(accept[0], "image/png");
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept is neither a string nor an array of strings.
  {
    auto& manifest = ParseManifestWithURLs(
        R"({
          "share_target": {
            "action": "https://foo.com/#",
            "method": "POST",
            "enctype": "multipart/form-data",
            "params": {
              "title": "mytitle",
              "files": [{
                "name": "name",
                "accept": true
              }]
            }
          }
        })",
        manifest_url, document_url);
    auto* share_target = manifest->share_target.get();
    EXPECT_TRUE(share_target);
    EXPECT_FALSE(share_target->params->files.has_value());

    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'accept' ignored, type array or string expected.",
              errors()[0]);
  }

  // Files is just a single FileFilter (not an array).
  {
    auto& manifest = ParseManifestWithURLs(
        R"({
          "share_target": {
            "action": "https://foo.com/#",
            "method": "POST",
            "enctype": "multipart/form-data",
            "params": {
              "title": "mytitle",
              "files": {
                "name": "name",
                "accept": "image/png"
              }
            }
          }
        })",
        manifest_url, document_url);
    EXPECT_TRUE(manifest->share_target.get());

    auto* params = manifest->share_target->params.get();
    EXPECT_TRUE(params->files.has_value());

    auto& file = params->files.value();
    EXPECT_EQ(1u, file.size());
    EXPECT_EQ(file[0]->name, "name");

    auto& accept = file[0]->accept;
    EXPECT_EQ(1u, accept.size());
    EXPECT_EQ(accept[0], "image/png");
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Files is neither array nor FileFilter.
  {
    auto& manifest = ParseManifestWithURLs(
        R"({
          "share_target": {
            "action": "https://foo.com/#",
            "method": "POST",
            "enctype": "multipart/form-data",
            "params": {
              "title": "mytitle",
              "files": 3
            }
          }
        })",
        manifest_url, document_url);
    auto* share_target = manifest->share_target.get();
    EXPECT_TRUE(share_target);

    EXPECT_FALSE(share_target->params->files.has_value());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'files' ignored, type array or FileFilter expected.",
              errors()[0]);
  }

  // Files contains a non-dictionary entry.
  {
    auto& manifest = ParseManifestWithURLs(
        R"({
          "share_target": {
            "action": "https://foo.com/#",
            "method": "POST",
            "enctype": "multipart/form-data",
            "params": {
              "title": "mytitle",
              "files": [
                {
                  "name": "name",
                  "accept": "image/png"
                },
                3
              ]
            }
          }
        })",
        manifest_url, document_url);
    auto* share_target = manifest->share_target.get();
    EXPECT_TRUE(share_target);

    EXPECT_TRUE(share_target->params->files.has_value());
    auto& files = share_target->params->files.value();
    EXPECT_EQ(1u, files.size());
    EXPECT_EQ(files[0]->name, "name");

    auto& accept = files[0]->accept;
    EXPECT_EQ(1u, accept.size());
    EXPECT_EQ(accept[0], "image/png");

    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("files must be a sequence of non-empty file entries.",
              errors()[0]);
  }

  // Files contains empty file.
  {
    auto& manifest = ParseManifestWithURLs(
        R"({
          "share_target": {
            "action": "https://foo.com/#",
            "method": "POST",
            "enctype": "multipart/form-data",
            "params": {
              "title": "mytitle",
              "files": [
                {
                  "name": "name",
                  "accept": "image/png"
                },
                {}
              ]
            }
          }
        })",
        manifest_url, document_url);
    auto* share_target = manifest->share_target.get();
    EXPECT_TRUE(share_target);

    EXPECT_TRUE(share_target->params->files.has_value());
    auto& files = share_target->params->files.value();
    EXPECT_EQ(1u, files.size());
    EXPECT_EQ(files[0]->name, "name");

    auto& accept = files[0]->accept;
    EXPECT_EQ(1u, accept.size());
    EXPECT_EQ(accept[0], "image/png");

    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'name' missing.", errors()[0]);
  }
}

TEST_F(ManifestParserTest, RelatedApplicationsParseRules) {
  // If no application, empty list.
  {
    auto& manifest = ParseManifest(R"({ "related_applications": []})");
    EXPECT_TRUE(manifest->related_applications.empty());
    EXPECT_EQ(0u, GetErrorCount());
  }

  // If empty application, empty list.
  {
    auto& manifest = ParseManifest(R"({ "related_applications": [{}]})");
    EXPECT_TRUE(manifest->related_applications.empty());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("'platform' is a required field, related application ignored.",
              errors()[0]);
  }

  // If invalid platform, application is ignored.
  {
    auto& manifest =
        ParseManifest(R"({ "related_applications": [{"platform": 123}]})");
    EXPECT_TRUE(manifest->related_applications.empty());
    EXPECT_EQ(2u, GetErrorCount());
    EXPECT_EQ("property 'platform' ignored, type string expected.",
              errors()[0]);
    EXPECT_EQ(
        "'platform' is a required field, "
        "related application ignored.",
        errors()[1]);
  }

  // If missing platform, application is ignored.
  {
    auto& manifest =
        ParseManifest(R"({ "related_applications": [{"id": "foo"}]})");
    EXPECT_TRUE(manifest->related_applications.empty());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("'platform' is a required field, related application ignored.",
              errors()[0]);
  }

  // If missing id and url, application is ignored.
  {
    auto& manifest =
        ParseManifest(R"({ "related_applications": [{"platform": "play"}]})");
    EXPECT_TRUE(manifest->related_applications.empty());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("one of 'url' or 'id' is required, related application ignored.",
              errors()[0]);
  }

  // Valid application, with url.
  {
    auto& manifest = ParseManifest(R"({ "related_applications": [
        {"platform": "play", "url": "http://www.foo.com"}]})");
    auto& related_applications = manifest->related_applications;
    EXPECT_EQ(related_applications.size(), 1u);
    EXPECT_EQ(related_applications[0]->platform, "play");
    EXPECT_TRUE(related_applications[0]->url.has_value());
    EXPECT_EQ(related_applications[0]->url->GetString(), "http://www.foo.com/");
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Application with an invalid url.
  {
    auto& manifest = ParseManifest(R"({ "related_applications": [
        {"platform": "play", "url": "http://www.foo.com:co&uk"}]})");
    EXPECT_TRUE(manifest->related_applications.empty());
    EXPECT_EQ(2u, GetErrorCount());
    EXPECT_EQ("property 'url' ignored, URL is invalid.", errors()[0]);
    EXPECT_EQ("one of 'url' or 'id' is required, related application ignored.",
              errors()[1]);
  }

  // Valid application, with id.
  {
    auto& manifest = ParseManifest(R"({ "related_applications": [
        {"platform": "itunes", "id": "foo"}]})");
    auto& related_applications = manifest->related_applications;
    EXPECT_EQ(related_applications.size(), 1u);
    EXPECT_EQ(related_applications[0]->platform, "itunes");
    EXPECT_EQ(related_applications[0]->id, "foo");
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // All valid applications are in list.
  {
    auto& manifest = ParseManifest(
        R"({ "related_applications": [
        {"platform": "play", "id": "foo"},
        {"platform": "itunes", "id": "bar"}]})");
    auto& related_applications = manifest->related_applications;
    EXPECT_EQ(related_applications.size(), 2u);
    EXPECT_EQ(related_applications[0]->platform, "play");
    EXPECT_EQ(related_applications[0]->id, "foo");
    EXPECT_EQ(related_applications[1]->platform, "itunes");
    EXPECT_EQ(related_applications[1]->id, "bar");
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Two invalid applications and one valid. Only the valid application should
  // be in the list.
  {
    auto& manifest = ParseManifest(
        R"({ "related_applications": [
        {"platform": "itunes"},
        {"platform": "play", "id": "foo"},
        {}]})");
    auto& related_applications = manifest->related_applications;
    EXPECT_EQ(related_applications.size(), 1u);
    EXPECT_EQ(related_applications[0]->platform, "play");
    EXPECT_EQ(related_applications[0]->id, "foo");
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(2u, GetErrorCount());
    EXPECT_EQ("one of 'url' or 'id' is required, related application ignored.",
              errors()[0]);
    EXPECT_EQ("'platform' is a required field, related application ignored.",
              errors()[1]);
  }
}

TEST_F(ManifestParserTest, ParsePreferRelatedApplicationsParseRules) {
  // Smoke test.
  {
    auto& manifest =
        ParseManifest(R"({ "prefer_related_applications": true })");
    EXPECT_TRUE(manifest->prefer_related_applications);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Don't parse if the property isn't a boolean.
  {
    auto& manifest = ParseManifest(R"({ "prefer_related_applications": {} })");
    EXPECT_FALSE(manifest->prefer_related_applications);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'prefer_related_applications' "
        "ignored, type boolean expected.",
        errors()[0]);
  }
  {
    auto& manifest =
        ParseManifest(R"({ "prefer_related_applications": "true" })");
    EXPECT_FALSE(manifest->prefer_related_applications);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'prefer_related_applications' "
        "ignored, type boolean expected.",
        errors()[0]);
  }
  {
    auto& manifest = ParseManifest(R"({ "prefer_related_applications": 1 })");
    EXPECT_FALSE(manifest->prefer_related_applications);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'prefer_related_applications' "
        "ignored, type boolean expected.",
        errors()[0]);
  }

  // "False" should set the boolean false without throwing errors.
  {
    auto& manifest =
        ParseManifest(R"({ "prefer_related_applications": false })");
    EXPECT_FALSE(manifest->prefer_related_applications);
    EXPECT_EQ(0u, GetErrorCount());
  }
}

TEST_F(ManifestParserTest, ThemeColorParserRules) {
  // Smoke test.
  {
    auto& manifest = ParseManifest(R"({ "theme_color": "#FF0000" })");
    EXPECT_TRUE(manifest->has_theme_color);
    EXPECT_EQ(manifest->theme_color, 0xFFFF0000u);
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Trim whitespaces.
  {
    auto& manifest = ParseManifest(R"({ "theme_color": "  blue   " })");
    EXPECT_TRUE(manifest->has_theme_color);
    EXPECT_EQ(manifest->theme_color, 0xFF0000FFu);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Don't parse if theme_color isn't a string.
  {
    auto& manifest = ParseManifest(R"({ "theme_color": {} })");
    EXPECT_FALSE(manifest->has_theme_color);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'theme_color' ignored, type string expected.",
              errors()[0]);
  }

  // Don't parse if theme_color isn't a string.
  {
    auto& manifest = ParseManifest(R"({ "theme_color": false })");
    EXPECT_FALSE(manifest->has_theme_color);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'theme_color' ignored, type string expected.",
              errors()[0]);
  }

  // Don't parse if theme_color isn't a string.
  {
    auto& manifest = ParseManifest(R"({ "theme_color": null })");
    EXPECT_FALSE(manifest->has_theme_color);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'theme_color' ignored, type string expected.",
              errors()[0]);
  }

  // Don't parse if theme_color isn't a string.
  {
    auto& manifest = ParseManifest(R"({ "theme_color": [] })");
    EXPECT_FALSE(manifest->has_theme_color);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'theme_color' ignored, type string expected.",
              errors()[0]);
  }

  // Don't parse if theme_color isn't a string.
  {
    auto& manifest = ParseManifest(R"({ "theme_color": 42 })");
    EXPECT_FALSE(manifest->has_theme_color);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'theme_color' ignored, type string expected.",
              errors()[0]);
  }

  // Parse fails if string is not in a known format.
  {
    auto& manifest = ParseManifest(R"~({ "theme_color": "foo(bar)" })~");
    EXPECT_FALSE(manifest->has_theme_color);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'theme_color' ignored,"
        " 'foo(bar)' is not a valid color.",
        errors()[0]);
  }

  // Parse fails if string is not in a known format.
  {
    auto& manifest = ParseManifest(R"({ "theme_color": "bleu" })");
    EXPECT_FALSE(manifest->has_theme_color);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'theme_color' ignored, 'bleu' is not a valid color.",
              errors()[0]);
  }

  // Parse fails if string is not in a known format.
  {
    auto& manifest = ParseManifest(R"({ "theme_color": "FF00FF" })");
    EXPECT_FALSE(manifest->has_theme_color);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'theme_color' ignored, 'FF00FF'"
        " is not a valid color.",
        errors()[0]);
  }

  // Parse fails if multiple values for theme_color are given.
  {
    auto& manifest = ParseManifest(R"({ "theme_color": "#ABC #DEF" })");
    EXPECT_FALSE(manifest->has_theme_color);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'theme_color' ignored, "
        "'#ABC #DEF' is not a valid color.",
        errors()[0]);
  }

  // Parse fails if multiple values for theme_color are given.
  {
    auto& manifest = ParseManifest(R"({ "theme_color": "#AABBCC #DDEEFF" })");
    EXPECT_FALSE(manifest->has_theme_color);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'theme_color' ignored, "
        "'#AABBCC #DDEEFF' is not a valid color.",
        errors()[0]);
  }

  // Accept CSS color keyword format.
  {
    auto& manifest = ParseManifest(R"({ "theme_color": "blue" })");
    EXPECT_EQ(manifest->theme_color, 0xFF0000FFu);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept CSS color keyword format.
  {
    auto& manifest = ParseManifest(R"({ "theme_color": "chartreuse" })");
    EXPECT_EQ(manifest->theme_color, 0xFF7FFF00u);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept CSS RGB format.
  {
    auto& manifest = ParseManifest(R"({ "theme_color": "#FFF" })");
    EXPECT_EQ(manifest->theme_color, 0xFFFFFFFFu);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept CSS RGB format.
  {
    auto& manifest = ParseManifest(R"({ "theme_color": "#ABC" })");
    EXPECT_EQ(manifest->theme_color, 0xFFAABBCCu);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept CSS RRGGBB format.
  {
    auto& manifest = ParseManifest(R"({ "theme_color": "#FF0000" })");
    EXPECT_EQ(manifest->theme_color, 0xFFFF0000u);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept translucent colors.
  {
    auto& manifest =
        ParseManifest(R"~({ "theme_color": "rgba(255,0,0,0.4)" })~");
    EXPECT_EQ(manifest->theme_color, 0x66FF0000u);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept transparent colors.
  {
    auto& manifest = ParseManifest(R"~({ "theme_color": "rgba(0,0,0,0)" })~");
    EXPECT_EQ(manifest->theme_color, 0x00000000u);
    EXPECT_EQ(0u, GetErrorCount());
  }
}

TEST_F(ManifestParserTest, BackgroundColorParserRules) {
  // Smoke test.
  {
    auto& manifest = ParseManifest(R"({ "background_color": "#FF0000" })");
    EXPECT_EQ(manifest->background_color, 0xFFFF0000u);
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Trim whitespaces.
  {
    auto& manifest = ParseManifest(R"({ "background_color": "  blue   " })");
    EXPECT_EQ(manifest->background_color, 0xFF0000FFu);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Don't parse if background_color isn't a string.
  {
    auto& manifest = ParseManifest(R"({ "background_color": {} })");
    EXPECT_FALSE(manifest->has_background_color);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'background_color' ignored, type string expected.",
              errors()[0]);
  }

  // Don't parse if background_color isn't a string.
  {
    auto& manifest = ParseManifest(R"({ "background_color": false })");
    EXPECT_FALSE(manifest->has_background_color);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'background_color' ignored, type string expected.",
              errors()[0]);
  }

  // Don't parse if background_color isn't a string.
  {
    auto& manifest = ParseManifest(R"({ "background_color": null })");
    EXPECT_FALSE(manifest->has_background_color);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'background_color' ignored, type string expected.",
              errors()[0]);
  }

  // Don't parse if background_color isn't a string.
  {
    auto& manifest = ParseManifest(R"({ "background_color": [] })");
    EXPECT_FALSE(manifest->has_background_color);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'background_color' ignored, type string expected.",
              errors()[0]);
  }

  // Don't parse if background_color isn't a string.
  {
    auto& manifest = ParseManifest(R"({ "background_color": 42 })");
    EXPECT_FALSE(manifest->has_background_color);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'background_color' ignored, type string expected.",
              errors()[0]);
  }

  // Parse fails if string is not in a known format.
  {
    auto& manifest = ParseManifest(R"~({ "background_color": "foo(bar)" })~");
    EXPECT_FALSE(manifest->has_background_color);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'background_color' ignored,"
        " 'foo(bar)' is not a valid color.",
        errors()[0]);
  }

  // Parse fails if string is not in a known format.
  {
    auto& manifest = ParseManifest(R"({ "background_color": "bleu" })");
    EXPECT_FALSE(manifest->has_background_color);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'background_color' ignored,"
        " 'bleu' is not a valid color.",
        errors()[0]);
  }

  // Parse fails if string is not in a known format.
  {
    auto& manifest = ParseManifest(R"({ "background_color": "FF00FF" })");
    EXPECT_FALSE(manifest->has_background_color);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'background_color' ignored,"
        " 'FF00FF' is not a valid color.",
        errors()[0]);
  }

  // Parse fails if multiple values for background_color are given.
  {
    auto& manifest = ParseManifest(R"({ "background_color": "#ABC #DEF" })");
    EXPECT_FALSE(manifest->has_background_color);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'background_color' ignored, "
        "'#ABC #DEF' is not a valid color.",
        errors()[0]);
  }

  // Parse fails if multiple values for background_color are given.
  {
    auto& manifest =
        ParseManifest(R"({ "background_color": "#AABBCC #DDEEFF" })");
    EXPECT_FALSE(manifest->has_background_color);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'background_color' ignored, "
        "'#AABBCC #DDEEFF' is not a valid color.",
        errors()[0]);
  }

  // Accept CSS color keyword format.
  {
    auto& manifest = ParseManifest(R"({ "background_color": "blue" })");
    EXPECT_EQ(manifest->background_color, 0xFF0000FFu);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept CSS color keyword format.
  {
    auto& manifest = ParseManifest(R"({ "background_color": "chartreuse" })");
    EXPECT_EQ(manifest->background_color, 0xFF7FFF00u);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept CSS RGB format.
  {
    auto& manifest = ParseManifest(R"({ "background_color": "#FFF" })");
    EXPECT_EQ(manifest->background_color, 0xFFFFFFFFu);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept CSS RGB format.
  {
    auto& manifest = ParseManifest(R"({ "background_color": "#ABC" })");
    EXPECT_EQ(manifest->background_color, 0xFFAABBCCu);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept CSS RRGGBB format.
  {
    auto& manifest = ParseManifest(R"({ "background_color": "#FF0000" })");
    EXPECT_EQ(manifest->background_color, 0xFFFF0000u);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept translucent colors.
  {
    auto& manifest =
        ParseManifest(R"~({ "background_color": "rgba(255,0,0,0.4)" })~");
    EXPECT_EQ(manifest->background_color, 0x66FF0000u);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept transparent colors.
  {
    auto& manifest =
        ParseManifest(R"~({ "background_color": "rgba(0,0,0,0)" })~");
    EXPECT_EQ(manifest->background_color, 0x00000000u);
    EXPECT_EQ(0u, GetErrorCount());
  }
}

TEST_F(ManifestParserTest, GCMSenderIDParseRules) {
  // Smoke test.
  {
    auto& manifest = ParseManifest(R"({ "gcm_sender_id": "foo" })");
    EXPECT_EQ(manifest->gcm_sender_id, "foo");
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Trim whitespaces.
  {
    auto& manifest = ParseManifest(R"({ "gcm_sender_id": "  foo  " })");
    EXPECT_EQ(manifest->gcm_sender_id, "foo");
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Don't parse if the property isn't a string.
  {
    auto& manifest = ParseManifest(R"({ "gcm_sender_id": {} })");
    EXPECT_TRUE(manifest->gcm_sender_id.IsNull());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'gcm_sender_id' ignored, type string expected.",
              errors()[0]);
  }
  {
    auto& manifest = ParseManifest(R"({ "gcm_sender_id": 42 })");
    EXPECT_TRUE(manifest->gcm_sender_id.IsNull());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'gcm_sender_id' ignored, type string expected.",
              errors()[0]);
  }
}

TEST_F(ManifestParserTest, PermissionsPolicyParsesOrigins) {
  auto& manifest = ParseManifest(
      R"({ "permissions_policy": {
                "geolocation": ["https://example.com"],
                "microphone": ["https://example.com"]
        }})");
  EXPECT_EQ(0u, GetErrorCount());
  EXPECT_EQ(2u, manifest->permissions_policy.size());
  for (const auto& policy : manifest->permissions_policy) {
    EXPECT_EQ(1u, policy.allowed_origins.size());
    EXPECT_EQ("https://example.com", policy.allowed_origins[0].Serialize());
    EXPECT_FALSE(manifest->permissions_policy[0].self_if_matches.has_value());
  }
}

TEST_F(ManifestParserTest, PermissionsPolicyParsesSelf) {
  auto& manifest = ParseManifest(
      R"({ "permissions_policy": {
        "geolocation": ["self"]
      }})");
  EXPECT_EQ(0u, GetErrorCount());
  EXPECT_EQ(1u, manifest->permissions_policy.size());
  EXPECT_EQ("http://foo.com",
            manifest->permissions_policy[0].self_if_matches->Serialize());
  EXPECT_EQ(0u, manifest->permissions_policy[0].allowed_origins.size());
}

TEST_F(ManifestParserTest, PermissionsPolicyIgnoresSrc) {
  auto& manifest = ParseManifest(
      R"({ "permissions_policy": {
        "geolocation": ["src"]
      }})");
  EXPECT_EQ(0u, GetErrorCount());
  EXPECT_EQ(1u, manifest->permissions_policy.size());
  EXPECT_EQ(0u, manifest->permissions_policy[0].allowed_origins.size());
  EXPECT_FALSE(manifest->permissions_policy[0].self_if_matches.has_value());
}

TEST_F(ManifestParserTest, PermissionsPolicyParsesNone) {
  auto& manifest = ParseManifest(
      R"({ "permissions_policy": {
        "geolocation": ["none"]
      }})");
  EXPECT_EQ(0u, GetErrorCount());
  EXPECT_EQ(1u, manifest->permissions_policy.size());
  EXPECT_EQ(0u, manifest->permissions_policy[0].allowed_origins.size());
}

TEST_F(ManifestParserTest, PermissionsPolicyParsesWildcard) {
  auto& manifest = ParseManifest(
      R"({ "permissions_policy": {
        "geolocation": ["*"]
      }})");
  EXPECT_EQ(0u, GetErrorCount());
  EXPECT_EQ(1u, manifest->permissions_policy.size());
  EXPECT_TRUE(manifest->permissions_policy[0].matches_all_origins);
}

TEST_F(ManifestParserTest, PermissionsPolicyEmptyOrigin) {
  auto& manifest = ParseManifest(
      R"({ "permissions_policy": {
                "geolocation": ["https://example.com"],
                "microphone": [""],
                "midi": []
        }})");
  EXPECT_EQ(1u, GetErrorCount());
  EXPECT_EQ(1u, manifest->permissions_policy.size());
}

TEST_F(ManifestParserTest, PermissionsPolicyAsArray) {
  auto& manifest = ParseManifest(
      R"({ "permissions_policy": [
          {"geolocation": ["https://example.com"]},
          {"microphone": [""]},
          {"midi": []}
        ]})");
  EXPECT_EQ(1u, GetErrorCount());
  EXPECT_EQ(0u, manifest->permissions_policy.size());
  EXPECT_EQ("property 'permissions_policy' ignored, type object expected.",
            errors()[0]);
}

TEST_F(ManifestParserTest, PermissionsPolicyInvalidType) {
  auto& manifest = ParseManifest(R"({ "permissions_policy": true})");
  EXPECT_EQ(1u, GetErrorCount());
  EXPECT_EQ(0u, manifest->permissions_policy.size());
  EXPECT_EQ("property 'permissions_policy' ignored, type object expected.",
            errors()[0]);
}

TEST_F(ManifestParserTest, PermissionsPolicyInvalidAllowlistType) {
  auto& manifest = ParseManifest(
      R"({ "permissions_policy": {
            "geolocation": ["https://example.com"],
            "microphone": 0,
            "midi": true
          }})");
  EXPECT_EQ(2u, GetErrorCount());
  EXPECT_EQ(1u, manifest->permissions_policy.size());
  EXPECT_EQ(
      "permission 'microphone' ignored, invalid allowlist: type array "
      "expected.",
      errors()[0]);
  EXPECT_EQ(
      "permission 'midi' ignored, invalid allowlist: type array expected.",
      errors()[1]);
}

TEST_F(ManifestParserTest, PermissionsPolicyInvalidAllowlistEntry) {
  auto& manifest = ParseManifest(
      R"({ "permissions_policy": {
            "geolocation": ["https://example.com", null],
            "microphone": ["https://example.com", {}]
          }})");
  EXPECT_EQ(2u, GetErrorCount());
  EXPECT_EQ(0u, manifest->permissions_policy.size());
  EXPECT_EQ(
      "permissions_policy entry ignored, required property 'origin' contains "
      "an invalid element: type string expected.",
      errors()[0]);
  EXPECT_EQ(
      "permissions_policy entry ignored, required property 'origin' contains "
      "an invalid element: type string expected.",
      errors()[1]);
}

TEST_F(ManifestParserTest, LaunchHandlerParseRules) {
  using ClientMode = mojom::blink::ManifestLaunchHandler::ClientMode;
  // Smoke test.
  {
    auto& manifest = ParseManifest(R"({
      "launch_handler": {
        "client_mode": "focus-existing"
      }
    })");
    EXPECT_EQ(manifest->launch_handler->client_mode,
              ClientMode::kFocusExisting);
    EXPECT_EQ(0u, GetErrorCount());
  }
  {
    auto& manifest = ParseManifest(R"({
      "launch_handler": {
        "client_mode": "navigate-new"
      }
    })");
    EXPECT_EQ(manifest->launch_handler->client_mode, ClientMode::kNavigateNew);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Empty object is fine.
  {
    auto& manifest = ParseManifest(R"({
      "launch_handler": {}
    })");
    EXPECT_EQ(manifest->launch_handler->client_mode, ClientMode::kAuto);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Empty array is fine.
  {
    auto& manifest = ParseManifest(R"({
      "launch_handler": {
        "client_mode": []
      }
    })");
    EXPECT_EQ(manifest->launch_handler->client_mode, ClientMode::kAuto);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Unknown single string.
  {
    auto& manifest = ParseManifest(R"({
      "launch_handler": {
        "client_mode": "space"
      }
    })");
    EXPECT_EQ(manifest->launch_handler->client_mode, ClientMode::kAuto);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("client_mode value 'space' ignored, unknown value.", errors()[0]);
  }

  // First known value in array is used.
  {
    auto& manifest = ParseManifest(R"({
      "launch_handler": {
        "client_mode": ["navigate-existing", "navigate-new"]
      }
    })");
    EXPECT_EQ(manifest->launch_handler->client_mode,
              ClientMode::kNavigateExisting);
    EXPECT_EQ(0u, GetErrorCount());
  }
  {
    auto& manifest = ParseManifest(R"({
      "launch_handler": {
        "client_mode": [null, "space", "focus-existing", "auto"]
      }
    })");
    EXPECT_EQ(manifest->launch_handler->client_mode,
              ClientMode::kFocusExisting);
    EXPECT_EQ(2u, GetErrorCount());
    EXPECT_EQ("client_mode value 'null' ignored, string expected.",
              errors()[0]);
    EXPECT_EQ("client_mode value 'space' ignored, unknown value.", errors()[1]);
  }

  // Don't parse if the property isn't an object.
  {
    auto& manifest = ParseManifest(R"({ "launch_handler": null })");
    EXPECT_FALSE(manifest->launch_handler);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("launch_handler value ignored, object expected.", errors()[0]);
  }
  {
    auto& manifest = ParseManifest(R"({
      "launch_handler": [{
        "client_mode": "navigate-new"
      }]
    })");
    EXPECT_FALSE(manifest->launch_handler);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("launch_handler value ignored, object expected.", errors()[0]);
  }
}

TEST_F(ManifestParserTest, TranslationsParseRules) {
  {
    ScopedWebAppTranslationsForTest feature(false);

    // Feature not enabled, should not be parsed.
    auto& manifest =
        ParseManifest(R"({ "translations": {"fr": {"name": "french name"}} })");
    EXPECT_TRUE(manifest->translations.empty());
    EXPECT_EQ(0u, GetErrorCount());
  }
  {
    ScopedWebAppTranslationsForTest feature(true);

    // Manifest does not contain a 'translations' field.
    {
      auto& manifest = ParseManifest(R"({ })");
      EXPECT_TRUE(manifest->translations.empty());
      EXPECT_EQ(0u, GetErrorCount());
    }

    // Don't parse if translations object is empty.
    {
      auto& manifest = ParseManifest(R"({ "translations": {} })");
      EXPECT_TRUE(manifest->translations.empty());
      EXPECT_EQ(0u, GetErrorCount());
    }

    // Empty translation is ignored.
    {
      auto& manifest = ParseManifest(R"({ "translations": {"fr": {}} })");
      EXPECT_TRUE(manifest->translations.empty());
      EXPECT_FALSE(manifest->translations.Contains("fr"));
      EXPECT_EQ(0u, GetErrorCount());
    }

    // Valid name, short_name and description should be parsed
    {
      auto& manifest = ParseManifest(
          R"({ "translations": {"fr": {"name": "french name", "short_name":
           "fr name", "description": "french description"}} })");
      EXPECT_FALSE(manifest->translations.empty());
      EXPECT_TRUE(manifest->translations.Contains("fr"));
      EXPECT_EQ(manifest->translations.find("fr")->value->name, "french name");
      EXPECT_EQ(manifest->translations.find("fr")->value->short_name,
                "fr name");
      EXPECT_EQ(manifest->translations.find("fr")->value->description,
                "french description");
      EXPECT_EQ(0u, GetErrorCount());
    }

    // Don't parse if the property isn't an object.
    {
      auto& manifest = ParseManifest(R"({ "translations": [] })");
      EXPECT_TRUE(manifest->translations.empty());
      EXPECT_EQ(1u, GetErrorCount());
      EXPECT_EQ("property 'translations' ignored, object expected.",
                errors()[0]);
    }

    // Ignore translation if it isn't an object.
    {
      auto& manifest = ParseManifest(R"({ "translations": {"fr": []} })");
      EXPECT_TRUE(manifest->translations.empty());
      EXPECT_EQ(1u, GetErrorCount());
      EXPECT_EQ("skipping translation, object expected.", errors()[0]);
    }

    // Multiple valid translations should all be parsed.
    {
      auto& manifest = ParseManifest(
          R"({ "translations": {"fr": {"name": "french name"},
          "es": {"name": "spanish name"}} })");
      EXPECT_FALSE(manifest->translations.empty());
      EXPECT_TRUE(manifest->translations.Contains("fr"));
      EXPECT_TRUE(manifest->translations.Contains("es"));
      EXPECT_EQ(manifest->translations.find("fr")->value->name, "french name");
      EXPECT_EQ(manifest->translations.find("es")->value->name, "spanish name");
      EXPECT_EQ(0u, GetErrorCount());
    }

    // Empty locale string should be ignored.
    {
      auto& manifest = ParseManifest(
          R"({ "translations": {"": {"name": "translated name"}} })");
      EXPECT_TRUE(manifest->translations.empty());
      EXPECT_EQ(1u, GetErrorCount());
      EXPECT_EQ("skipping translation, non-empty locale string expected.",
                errors()[0]);
    }
  }
}

TEST_F(ManifestParserTest, TranslationsStringsParseRules) {
  ScopedWebAppTranslationsForTest feature(true);

  // Ignore non-string translations name.
  {
    auto& manifest =
        ParseManifest(R"({ "translations": {"fr": {"name": {}}} })");
    EXPECT_TRUE(manifest->translations.empty());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'name' of 'translations' ignored, type string expected.",
        errors()[0]);
  }

  // Ignore non-string translations short_name.
  {
    auto& manifest =
        ParseManifest(R"({ "translations": {"fr": {"short_name": []}} })");
    EXPECT_TRUE(manifest->translations.empty());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'short_name' of 'translations' ignored, type string "
        "expected.",
        errors()[0]);
  }

  // Ignore non-string translations description.
  {
    auto& manifest =
        ParseManifest(R"({ "translations": {"fr": {"description": 42}} })");
    EXPECT_TRUE(manifest->translations.empty());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'description' of 'translations' ignored, type string "
        "expected.",
        errors()[0]);
  }

  // Translation with empty strings is ignored.
  {
    auto& manifest = ParseManifest(
        R"({ "translations": {"fr": {"name": "", "short_name": "",
        "description": ""}} })");
    EXPECT_TRUE(manifest->translations.empty());
    EXPECT_FALSE(manifest->translations.Contains("fr"));
    EXPECT_EQ(3u, GetErrorCount());
    EXPECT_EQ("property 'name' of 'translations' is an empty string.",
              errors()[0]);
    EXPECT_EQ("property 'short_name' of 'translations' is an empty string.",
              errors()[1]);
    EXPECT_EQ("property 'description' of 'translations' is an empty string.",
              errors()[2]);
  }
}

TEST_F(ManifestParserTest, TabStripParseRules) {
  using Visibility = mojom::blink::TabStripMemberVisibility;
  {
    ScopedWebAppTabStripForTest feature1(true);
    ScopedWebAppTabStripCustomizationsForTest feature2(false);
    // Tab strip customizations feature not enabled, should not be parsed.
    {
      auto& manifest =
          ParseManifest(R"({ "tab_strip": {"home_tab": "auto"} })");
      EXPECT_TRUE(manifest->tab_strip.is_null());
      EXPECT_EQ(0u, GetErrorCount());
    }
  }
  {
    ScopedWebAppTabStripForTest feature1(true);
    ScopedWebAppTabStripCustomizationsForTest feature2(true);

    // Display mode not 'tabbed', 'tab_strip' should still be parsed.
    {
      auto& manifest =
          ParseManifest(R"({ "tab_strip": {"home_tab": "auto"} })");
      EXPECT_FALSE(manifest->tab_strip.is_null());
      EXPECT_EQ(0u, GetErrorCount());
    }

    // Manifest does not contain 'tab_strip' field.
    {
      auto& manifest = ParseManifest(R"({ "display_override": [ "tabbed" ] })");
      EXPECT_TRUE(manifest->tab_strip.is_null());
      EXPECT_EQ(0u, GetErrorCount());
    }

    // 'tab_strip' object is empty.
    {
      auto& manifest = ParseManifest(R"({  "tab_strip": {} })");
      EXPECT_FALSE(manifest->tab_strip.is_null());
      EXPECT_EQ(manifest->tab_strip->home_tab->get_visibility(),
                Visibility::kAuto);
      EXPECT_FALSE(manifest->tab_strip->new_tab_button->url.has_value());
      EXPECT_EQ(0u, GetErrorCount());
    }

    // Home tab and new tab button are empty objects.
    {
      auto& manifest = ParseManifest(R"({
          "tab_strip": {"home_tab": {}, "new_tab_button": {}} })");
      EXPECT_FALSE(manifest->tab_strip.is_null());
      EXPECT_FALSE(manifest->tab_strip->home_tab->is_visibility());
      EXPECT_EQ(manifest->tab_strip->home_tab->get_params()->icons.size(), 0u);
      EXPECT_EQ(
          manifest->tab_strip->home_tab->get_params()->scope_patterns.size(),
          0u);
      EXPECT_FALSE(manifest->tab_strip->new_tab_button->url.has_value());
      EXPECT_EQ(0u, GetErrorCount());
    }

    // Home tab and new tab button are invalid.
    {
      auto& manifest = ParseManifest(R"({
          "tab_strip": {"home_tab": "something", "new_tab_button": 42} })");
      EXPECT_FALSE(manifest->tab_strip.is_null());
      EXPECT_EQ(manifest->tab_strip->home_tab->get_visibility(),
                Visibility::kAuto);
      EXPECT_FALSE(manifest->tab_strip->home_tab->is_params());
      EXPECT_FALSE(manifest->tab_strip->new_tab_button->url.has_value());
      EXPECT_EQ(0u, GetErrorCount());
    }

    // Unknown members of 'tab_strip' are ignored.
    {
      auto& manifest = ParseManifest(R"({
          "tab_strip": {"unknown": {}} })");
      EXPECT_FALSE(manifest->tab_strip.is_null());
      EXPECT_EQ(manifest->tab_strip->home_tab->get_visibility(),
                Visibility::kAuto);
      EXPECT_FALSE(manifest->tab_strip->home_tab->is_params());
      EXPECT_FALSE(manifest->tab_strip->new_tab_button->url.has_value());
      EXPECT_EQ(0u, GetErrorCount());
    }

    // Home tab with icons and new tab button with url are parsed.
    {
      auto& manifest = ParseManifest(R"({
          "tab_strip": {
            "home_tab": {"icons": [{"src": "foo.jpg"}]},
            "new_tab_button": {"url": "foo"}} })");
      EXPECT_FALSE(manifest->tab_strip.is_null());
      EXPECT_FALSE(manifest->tab_strip->home_tab->is_visibility());
      EXPECT_EQ(manifest->tab_strip->home_tab->get_params()->icons.size(), 1u);
      EXPECT_EQ(manifest->tab_strip->new_tab_button->url,
                KURL(DefaultDocumentUrl(), "foo"));
      EXPECT_EQ(0u, GetErrorCount());
    }

    // New tab button url out of scope.
    {
      auto& manifest = ParseManifest(R"({
          "tab_strip": {"new_tab_button": {"url": "https://bar.com"}} })");
      EXPECT_FALSE(manifest->tab_strip.is_null());
      EXPECT_FALSE(manifest->tab_strip->new_tab_button->url.has_value());
      EXPECT_EQ(1u, GetErrorCount());
      EXPECT_EQ(
          "property 'url' ignored, should be within scope of the manifest.",
          errors()[0]);
    }

    // Home tab and new tab button set to 'auto'.
    {
      auto& manifest = ParseManifest(R"({
          "tab_strip": {"home_tab": "auto", "new_tab_button": "auto"} })");
      EXPECT_FALSE(manifest->tab_strip.is_null());
      EXPECT_EQ(manifest->tab_strip->home_tab->get_visibility(),
                Visibility::kAuto);
      EXPECT_FALSE(manifest->tab_strip->home_tab->is_params());
      EXPECT_FALSE(manifest->tab_strip->new_tab_button->url.has_value());
      EXPECT_EQ(0u, GetErrorCount());
    }

    // Home tab set to 'absent'.
    {
      auto& manifest = ParseManifest(R"({
          "tab_strip": {"home_tab": "absent"} })");
      EXPECT_FALSE(manifest->tab_strip.is_null());
      EXPECT_EQ(manifest->tab_strip->home_tab->get_visibility(),
                Visibility::kAbsent);
      EXPECT_FALSE(manifest->tab_strip->home_tab->is_params());
      EXPECT_FALSE(manifest->tab_strip->new_tab_button->url.has_value());
      EXPECT_EQ(0u, GetErrorCount());
    }

    // Home tab with 'auto' icons and new tab button with 'auto' url.
    {
      auto& manifest = ParseManifest(R"({
          "tab_strip": {
            "home_tab": {"icons": "auto"},
            "new_tab_button": {"url": "auto"}} })");
      EXPECT_FALSE(manifest->tab_strip.is_null());
      EXPECT_FALSE(manifest->tab_strip->home_tab->is_visibility());
      EXPECT_EQ(manifest->tab_strip->home_tab->get_params()->icons.size(), 0u);
      EXPECT_FALSE(manifest->tab_strip->new_tab_button->url.has_value());
      EXPECT_EQ(0u, GetErrorCount());
    }
  }
}

TEST_F(ManifestParserTest, TabStripHomeTabScopeParseRules) {
  ScopedWebAppTabStripForTest feature(true);

  // Valid scope hostname and protocol patterns override the default manifest
  // URL.
  {
    auto& manifest = ParseManifest(R"({
        "tab_strip": {
          "home_tab": {"scope_patterns":
            [{"protocol": "ftp"}, {"hostname": "bar.com"},
            {"protocol": "ftp", "hostname": "bar.com"}]}} })");
    EXPECT_FALSE(manifest->tab_strip.is_null());
    EXPECT_FALSE(manifest->tab_strip->home_tab->is_visibility());
    EXPECT_EQ(
        manifest->tab_strip->home_tab->get_params()->scope_patterns.size(), 3u);
    VerifySafeUrlPatternSizes(
        manifest->tab_strip->home_tab->get_params()->scope_patterns[0], 1, 0, 0,
        0, 0, 0, 0, 0);
    VerifySafeUrlPatternSizes(
        manifest->tab_strip->home_tab->get_params()->scope_patterns[1], 1, 0, 0,
        1, 0, 0, 0, 0);
    VerifySafeUrlPatternSizes(
        manifest->tab_strip->home_tab->get_params()->scope_patterns[2], 1, 0, 0,
        1, 0, 0, 0, 0);
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[0]
                  .protocol[0]
                  .value,
              "ftp");
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[1]
                  .protocol[0]
                  .value,
              "http");
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[1]
                  .hostname[0]
                  .value,
              "bar.com");
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[2]
                  .protocol[0]
                  .value,
              "ftp");
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[2]
                  .hostname[0]
                  .value,
              "bar.com");

    EXPECT_EQ(0u, GetErrorCount());
  }

  // Valid scope pathname patterns are parsed. Relative pathnames are made
  // absolute, resolved relative to the manifest URL.
  {
    auto& manifest = ParseManifestWithURLs(
        R"({
        "tab_strip": {
          "home_tab": {"scope_patterns":
            [{"pathname": "foo"}, {"pathname": "foo/bar/"},
            {"pathname": "/foo/"}, {"pathname": "/foo/bar/"}]
          }} })",
        KURL("http://foo.com/static/manifest.json"), DefaultDocumentUrl());
    EXPECT_FALSE(manifest->tab_strip.is_null());
    EXPECT_FALSE(manifest->tab_strip->home_tab->is_visibility());
    EXPECT_EQ(
        manifest->tab_strip->home_tab->get_params()->scope_patterns.size(), 4u);
    VerifySafeUrlPatternSizes(
        manifest->tab_strip->home_tab->get_params()->scope_patterns[0], 1, 0, 0,
        1, 0, 1, 0, 0);
    VerifySafeUrlPatternSizes(
        manifest->tab_strip->home_tab->get_params()->scope_patterns[1], 1, 0, 0,
        1, 0, 1, 0, 0);
    VerifySafeUrlPatternSizes(
        manifest->tab_strip->home_tab->get_params()->scope_patterns[2], 1, 0, 0,
        1, 0, 1, 0, 0);
    VerifySafeUrlPatternSizes(
        manifest->tab_strip->home_tab->get_params()->scope_patterns[3], 1, 0, 0,
        1, 0, 1, 0, 0);
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[0]
                  .protocol[0]
                  .type,
              liburlpattern::PartType::kFixed);
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[0]
                  .hostname[0]
                  .type,
              liburlpattern::PartType::kFixed);
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[0]
                  .hostname[0]
                  .value,
              "foo.com");
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[0]
                  .pathname[0]
                  .type,
              liburlpattern::PartType::kFixed);
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[0]
                  .pathname[0]
                  .value,
              "/static/foo");
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[1]
                  .protocol[0]
                  .type,
              liburlpattern::PartType::kFixed);
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[1]
                  .hostname[0]
                  .type,
              liburlpattern::PartType::kFixed);
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[1]
                  .hostname[0]
                  .value,
              "foo.com");
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[1]
                  .pathname[0]
                  .type,
              liburlpattern::PartType::kFixed);
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[1]
                  .pathname[0]
                  .value,
              "/static/foo/bar/");
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[2]
                  .protocol[0]
                  .type,
              liburlpattern::PartType::kFixed);
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[2]
                  .hostname[0]
                  .type,
              liburlpattern::PartType::kFixed);
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[2]
                  .hostname[0]
                  .value,
              "foo.com");
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[2]
                  .pathname[0]
                  .type,
              liburlpattern::PartType::kFixed);
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[2]
                  .pathname[0]
                  .value,
              "/foo/");
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[3]
                  .protocol[0]
                  .type,
              liburlpattern::PartType::kFixed);
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[3]
                  .hostname[0]
                  .type,
              liburlpattern::PartType::kFixed);
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[3]
                  .hostname[0]
                  .value,
              "foo.com");
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[3]
                  .pathname[0]
                  .type,
              liburlpattern::PartType::kFixed);
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[3]
                  .pathname[0]
                  .value,
              "/foo/bar/");

    EXPECT_EQ(0u, GetErrorCount());
  }

  // Base URL provided in scope patterns is respected if it is valid.
  {
    auto& manifest = ParseManifest(R"({
        "tab_strip": {
          "home_tab": {"scope_patterns":
            [{"protocol": "ftp", "baseURL": "https://www.bar.com"},
            {"hostname": "bar.com", "baseURL": "https://foobar.com"},
            {"pathname": "/foo/bar/", "baseURL": "https://bar.com"},
            // Invalid (expect to be discarded).
            {"pathname": "/foobar/", "baseURL": "notaurl"},
            {"pathname": "bar", "baseURL": "https://bar.com/foo"},
            {"pathname": "bar", "baseURL": "https://bar.com/foo/"}
          ]}}
         })");
    EXPECT_FALSE(manifest->tab_strip.is_null());
    EXPECT_FALSE(manifest->tab_strip->home_tab->is_visibility());
    EXPECT_EQ(
        manifest->tab_strip->home_tab->get_params()->scope_patterns.size(), 5u);
    VerifySafeUrlPatternSizes(
        manifest->tab_strip->home_tab->get_params()->scope_patterns[0], 1, 0, 0,
        0, 0, 0, 0, 0);
    VerifySafeUrlPatternSizes(
        manifest->tab_strip->home_tab->get_params()->scope_patterns[1], 1, 0, 0,
        1, 0, 0, 0, 0);
    VerifySafeUrlPatternSizes(
        manifest->tab_strip->home_tab->get_params()->scope_patterns[2], 1, 0, 0,
        1, 0, 1, 0, 0);
    VerifySafeUrlPatternSizes(
        manifest->tab_strip->home_tab->get_params()->scope_patterns[3], 1, 0, 0,
        1, 0, 1, 0, 0);
    VerifySafeUrlPatternSizes(
        manifest->tab_strip->home_tab->get_params()->scope_patterns[4], 1, 0, 0,
        1, 0, 1, 0, 0);
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[0]
                  .protocol[0]
                  .value,
              "ftp");
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[1]
                  .protocol[0]
                  .value,
              "https");
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[1]
                  .hostname[0]
                  .value,
              "bar.com");
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[2]
                  .protocol[0]
                  .value,
              "https");
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[2]
                  .hostname[0]
                  .value,
              "bar.com");
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[2]
                  .pathname[0]
                  .value,
              "/foo/bar/");
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[3]
                  .protocol[0]
                  .value,
              "https");
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[3]
                  .hostname[0]
                  .value,
              "bar.com");
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[3]
                  .pathname[0]
                  .value,
              "/bar");
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[4]
                  .protocol[0]
                  .value,
              "https");
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[4]
                  .hostname[0]
                  .value,
              "bar.com");
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[4]
                  .pathname[0]
                  .value,
              "/foo/bar");

    EXPECT_EQ(1u, GetErrorCount());
  }

  // Allow patterns with wildcards and named groups in the pathname.
  {
    auto& manifest = ParseManifest(R"({
        "tab_strip": {
          "home_tab": {"scope_patterns":
            [{"pathname": "*"}, {"pathname": ":foo"}, {"pathname": "/foo/*"},
            {"pathname": "/foo/*/bar"}, {"pathname": "/foo/:bar"},
            {"pathname": "/foo/:bar/*"}]}}
        })");
    EXPECT_FALSE(manifest->tab_strip.is_null());
    EXPECT_FALSE(manifest->tab_strip->home_tab->is_visibility());
    EXPECT_EQ(
        manifest->tab_strip->home_tab->get_params()->scope_patterns.size(), 6u);
    VerifySafeUrlPatternSizes(
        manifest->tab_strip->home_tab->get_params()->scope_patterns[0], 1, 0, 0,
        1, 0, 1, 0, 0);
    VerifySafeUrlPatternSizes(
        manifest->tab_strip->home_tab->get_params()->scope_patterns[1], 1, 0, 0,
        1, 0, 1, 0, 0);
    VerifySafeUrlPatternSizes(
        manifest->tab_strip->home_tab->get_params()->scope_patterns[2], 1, 0, 0,
        1, 0, 2, 0, 0);
    VerifySafeUrlPatternSizes(
        manifest->tab_strip->home_tab->get_params()->scope_patterns[3], 1, 0, 0,
        1, 0, 3, 0, 0);
    VerifySafeUrlPatternSizes(
        manifest->tab_strip->home_tab->get_params()->scope_patterns[4], 1, 0, 0,
        1, 0, 2, 0, 0);
    VerifySafeUrlPatternSizes(
        manifest->tab_strip->home_tab->get_params()->scope_patterns[5], 1, 0, 0,
        1, 0, 3, 0, 0);

    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[0]
                  .pathname[0]
                  .type,
              liburlpattern::PartType::kFullWildcard);
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[1]
                  .pathname[0]
                  .type,
              liburlpattern::PartType::kSegmentWildcard);
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[2]
                  .pathname[0]
                  .type,
              liburlpattern::PartType::kFixed);
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[2]
                  .pathname[0]
                  .value,
              "/foo");
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[2]
                  .pathname[1]
                  .type,
              liburlpattern::PartType::kFullWildcard);
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[3]
                  .pathname[0]
                  .type,
              liburlpattern::PartType::kFixed);
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[3]
                  .pathname[0]
                  .value,
              "/foo");
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[3]
                  .pathname[1]
                  .type,
              liburlpattern::PartType::kFullWildcard);
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[3]
                  .pathname[2]
                  .type,
              liburlpattern::PartType::kFixed);
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[3]
                  .pathname[2]
                  .value,
              "/bar");
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[4]
                  .pathname[0]
                  .type,
              liburlpattern::PartType::kFixed);
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[4]
                  .pathname[0]
                  .value,
              "/foo");
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[4]
                  .pathname[1]
                  .type,
              liburlpattern::PartType::kSegmentWildcard);
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[5]
                  .pathname[0]
                  .type,
              liburlpattern::PartType::kFixed);
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[5]
                  .pathname[0]
                  .value,
              "/foo");
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[5]
                  .pathname[1]
                  .type,
              liburlpattern::PartType::kSegmentWildcard);
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[5]
                  .pathname[2]
                  .type,
              liburlpattern::PartType::kFullWildcard);

    EXPECT_EQ(0u, GetErrorCount());
  }

  // Allow patterns with wildcards and named groups in the hostname.
  {
    auto& manifest = ParseManifest(R"({
        "tab_strip": {
          "home_tab": {"scope_patterns":
            [{"hostname": "*"}, {"hostname": "bar.com"}, {"hostname": "bar*.com"},
            {"hostname": "bar.*"}, {"hostname": "bar.*.com"},
            {"hostname": "foo.:bar.*"}, {"hostname": "*.com"}]}}
        })");
    EXPECT_FALSE(manifest->tab_strip.is_null());
    EXPECT_FALSE(manifest->tab_strip->home_tab->is_visibility());
    EXPECT_EQ(
        manifest->tab_strip->home_tab->get_params()->scope_patterns.size(), 7u);
    VerifySafeUrlPatternSizes(
        manifest->tab_strip->home_tab->get_params()->scope_patterns[0], 1, 0, 0,
        1, 0, 0, 0, 0);
    VerifySafeUrlPatternSizes(
        manifest->tab_strip->home_tab->get_params()->scope_patterns[1], 1, 0, 0,
        1, 0, 0, 0, 0);
    VerifySafeUrlPatternSizes(
        manifest->tab_strip->home_tab->get_params()->scope_patterns[2], 1, 0, 0,
        3, 0, 0, 0, 0);
    VerifySafeUrlPatternSizes(
        manifest->tab_strip->home_tab->get_params()->scope_patterns[3], 1, 0, 0,
        2, 0, 0, 0, 0);
    VerifySafeUrlPatternSizes(
        manifest->tab_strip->home_tab->get_params()->scope_patterns[4], 1, 0, 0,
        3, 0, 0, 0, 0);
    VerifySafeUrlPatternSizes(
        manifest->tab_strip->home_tab->get_params()->scope_patterns[5], 1, 0, 0,
        3, 0, 0, 0, 0);
    VerifySafeUrlPatternSizes(
        manifest->tab_strip->home_tab->get_params()->scope_patterns[6], 1, 0, 0,
        2, 0, 0, 0, 0);

    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[0]
                  .hostname[0]
                  .type,
              liburlpattern::PartType::kFullWildcard);
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[1]
                  .hostname[0]
                  .type,
              liburlpattern::PartType::kFixed);
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[1]
                  .hostname[0]
                  .value,
              "bar.com");
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[2]
                  .hostname[0]
                  .type,
              liburlpattern::PartType::kFixed);
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[2]
                  .hostname[0]
                  .value,
              "bar");
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[2]
                  .hostname[1]
                  .type,
              liburlpattern::PartType::kFullWildcard);
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[2]
                  .hostname[2]
                  .type,
              liburlpattern::PartType::kFixed);
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[2]
                  .hostname[2]
                  .value,
              ".com");
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[3]
                  .hostname[0]
                  .type,
              liburlpattern::PartType::kFixed);
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[3]
                  .hostname[0]
                  .value,
              "bar");
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[3]
                  .hostname[1]
                  .type,
              liburlpattern::PartType::kFullWildcard);
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[4]
                  .hostname[0]
                  .type,
              liburlpattern::PartType::kFixed);
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[4]
                  .hostname[0]
                  .value,
              "bar");
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[4]
                  .hostname[1]
                  .type,
              liburlpattern::PartType::kFullWildcard);
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[4]
                  .hostname[2]
                  .type,
              liburlpattern::PartType::kFixed);
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[4]
                  .hostname[2]
                  .value,
              ".com");
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[5]
                  .hostname[0]
                  .type,
              liburlpattern::PartType::kFixed);
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[5]
                  .hostname[0]
                  .value,
              "foo");
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[5]
                  .hostname[1]
                  .type,
              liburlpattern::PartType::kSegmentWildcard);
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[5]
                  .hostname[2]
                  .type,
              liburlpattern::PartType::kFullWildcard);
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[6]
                  .hostname[0]
                  .type,
              liburlpattern::PartType::kFullWildcard);
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[6]
                  .hostname[1]
                  .type,
              liburlpattern::PartType::kFixed);
    EXPECT_EQ(manifest->tab_strip->home_tab->get_params()
                  ->scope_patterns[6]
                  .hostname[1]
                  .value,
              ".com");

    EXPECT_EQ(0u, GetErrorCount());
  }

  // Reject patterns containing custom regex in any field, with errors.
  {
    auto& manifest = ParseManifest(R"a({
        "tab_strip": {
          "home_tab": {"scope_patterns":
            [{"pathname": "([a-z]+)/"}, {"pathname": "/foo/([a-z]+)/"},
            {"protocol": "http([a-z])+)"}, {"hostname": "([a-z]+).com"},
            {"username": "([A-Za-z])+"}, {"password": "([A-Za-z0-9@%^!])+"},
            {"port": "(80|443)"}, {"hash": "([a-zA-Z0-9])+"},
            {"search": "([A-Za-z0-9])+"}
    ]}} })a");
    EXPECT_FALSE(manifest->tab_strip.is_null());
    EXPECT_FALSE(manifest->tab_strip->home_tab->is_visibility());
    EXPECT_EQ(
        manifest->tab_strip->home_tab->get_params()->scope_patterns.size(), 0u);

    EXPECT_EQ(9u, GetErrorCount());
  }

  // Patterns list doesn't contain objects.
  {
    auto& manifest = ParseManifest(R"({
        "tab_strip": {
          "home_tab": {"scope_patterns": ["blah", 3]}} })");
    EXPECT_FALSE(manifest->tab_strip.is_null());
    EXPECT_FALSE(manifest->tab_strip->home_tab->is_visibility());
    EXPECT_EQ(
        manifest->tab_strip->home_tab->get_params()->scope_patterns.size(), 0u);

    EXPECT_EQ(0u, GetErrorCount());
  }

  // Pattern list is empty.
  {
    auto& manifest = ParseManifest(R"({
        "tab_strip": {
          "home_tab": {"scope_patterns": []}} })");
    EXPECT_FALSE(manifest->tab_strip.is_null());
    EXPECT_FALSE(manifest->tab_strip->home_tab->is_visibility());
    EXPECT_EQ(
        manifest->tab_strip->home_tab->get_params()->scope_patterns.size(), 0u);

    EXPECT_EQ(0u, GetErrorCount());
  }
}

TEST_F(ManifestParserTest, VersionParseRules) {
  // Valid versions are parsed.
  {
    auto& manifest = ParseManifest(R"({ "version": "1.2.3" })");
    EXPECT_FALSE(manifest->version.IsNull());
    EXPECT_EQ(manifest->version, "1.2.3");

    EXPECT_EQ(0u, GetErrorCount());
  }

  // Do not tamper with the version string in any way.
  {
    auto& manifest = ParseManifest(R"({ "version": " abc !^?$ test " })");
    EXPECT_FALSE(manifest->version.IsNull());
    EXPECT_EQ(manifest->version, " abc !^?$ test ");

    EXPECT_EQ(0u, GetErrorCount());
  }

  // Reject versions that are not strings.
  {
    auto& manifest = ParseManifest(R"({ "version": 123 })");
    EXPECT_TRUE(manifest->version.IsNull());
    EXPECT_EQ(1u, GetErrorCount());
  }
}

}  // namespace blink
