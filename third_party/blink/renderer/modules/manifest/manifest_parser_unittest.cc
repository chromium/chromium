// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/manifest/manifest_parser.h"

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/optional.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

bool IsManifestEmpty(const mojom::blink::ManifestPtr& manifest) {
  return manifest == mojom::blink::Manifest::New();
}

class ManifestParserTest : public testing::Test {
 protected:
  ManifestParserTest() {}
  ~ManifestParserTest() override {}

  mojom::blink::ManifestPtr& ParseManifestWithURLs(const String& data,
                                                   const KURL& manifest_url,
                                                   const KURL& document_url) {
    ManifestParser parser(data, manifest_url, document_url);
    parser.Parse();
    Vector<mojom::blink::ManifestErrorPtr> errors;
    parser.TakeErrors(&errors);

    errors_.clear();
    for (auto& error : errors)
      errors_.push_back(std::move(error->message));
    manifest_ = parser.manifest().Clone();
    return manifest_;
  }

  mojom::blink::ManifestPtr& ParseManifest(const String& data) {
    return ParseManifestWithURLs(data, default_manifest_url,
                                 default_document_url);
  }

  const Vector<String>& errors() const { return errors_; }

  unsigned int GetErrorCount() const { return errors_.size(); }

  const KURL& DefaultDocumentUrl() const { return default_document_url; }
  const KURL& DefaultManifestUrl() const { return default_manifest_url; }

 private:
  mojom::blink::ManifestPtr manifest_;
  Vector<String> errors_;

  const KURL default_document_url = KURL("http://foo.com/index.html");
  const KURL default_manifest_url = KURL("http://foo.com/manifest.json");

  DISALLOW_COPY_AND_ASSIGN(ManifestParserTest);
};

TEST_F(ManifestParserTest, CrashTest) {
  // Passing temporary variables should not crash.
  const String json = "{\"start_url\": \"/\"}";
  KURL url("http://example.com");
  ManifestParser parser(json, url, url);

  parser.Parse();
  Vector<mojom::blink::ManifestErrorPtr> errors;
  const auto& manifest = parser.manifest();
  parser.TakeErrors(&errors);

  // .Parse() should have been call without crashing and succeeded.
  EXPECT_EQ(0u, errors.size());
  EXPECT_FALSE(IsManifestEmpty(manifest));
}

TEST_F(ManifestParserTest, EmptyStringNull) {
  auto& manifest = ParseManifest("");

  // This Manifest is not a valid JSON object, it's a parsing error.
  EXPECT_EQ(1u, GetErrorCount());
  EXPECT_EQ("Line: 1, column: 1, Syntax error.", errors()[0]);

  // A parsing error is equivalent to an empty manifest.
  ASSERT_TRUE(IsManifestEmpty(manifest));
}

TEST_F(ManifestParserTest, ValidNoContentParses) {
  auto& manifest = ParseManifest("{}");

  // Empty Manifest is not a parsing error.
  EXPECT_EQ(0u, GetErrorCount());

  // Check that the fields are null or set to their default values.
  ASSERT_FALSE(IsManifestEmpty(manifest));
  ASSERT_TRUE(manifest->name.IsNull());
  ASSERT_TRUE(manifest->short_name.IsNull());
  ASSERT_TRUE(manifest->start_url.IsEmpty());
  ASSERT_EQ(manifest->display, blink::mojom::DisplayMode::kUndefined);
  ASSERT_EQ(manifest->orientation,
            device::mojom::ScreenOrientationLockType::DEFAULT);
  ASSERT_FALSE(manifest->has_theme_color);
  ASSERT_FALSE(manifest->has_background_color);
  ASSERT_TRUE(manifest->gcm_sender_id.IsNull());
  ASSERT_EQ(DefaultDocumentUrl().BaseAsString(), manifest->scope.GetString());
  ASSERT_TRUE(manifest->shortcuts.IsEmpty());
}

TEST_F(ManifestParserTest, MultipleErrorsReporting) {
  auto& manifest = ParseManifest(
      "{ \"name\": 42, \"short_name\": 4,"
      "\"orientation\": {}, \"display\": \"foo\","
      "\"start_url\": null, \"icons\": {}, \"theme_color\": 42,"
      "\"background_color\": 42, \"shortcuts\": {} }");
  ASSERT_FALSE(IsManifestEmpty(manifest));

  EXPECT_EQ(9u, GetErrorCount());

  EXPECT_EQ("property 'name' ignored, type string expected.", errors()[0]);
  EXPECT_EQ("property 'short_name' ignored, type string expected.",
            errors()[1]);
  EXPECT_EQ("property 'start_url' ignored, type string expected.", errors()[2]);
  EXPECT_EQ("unknown 'display' value ignored.", errors()[3]);
  EXPECT_EQ("property 'orientation' ignored, type string expected.",
            errors()[4]);
  EXPECT_EQ("property 'icons' ignored, type array expected.", errors()[5]);
  EXPECT_EQ("property 'theme_color' ignored, type string expected.",
            errors()[6]);
  EXPECT_EQ("property 'background_color' ignored, type string expected.",
            errors()[7]);
  EXPECT_EQ("property 'shortcuts' ignored, type array expected.", errors()[8]);
}

TEST_F(ManifestParserTest, NameParseRules) {
  // Smoke test.
  {
    auto& manifest = ParseManifest("{ \"name\": \"foo\" }");
    ASSERT_EQ(manifest->name, "foo");
    ASSERT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Trim whitespaces.
  {
    auto& manifest = ParseManifest("{ \"name\": \"  foo  \" }");
    ASSERT_EQ(manifest->name, "foo");
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Don't parse if name isn't a string.
  {
    auto& manifest = ParseManifest("{ \"name\": {} }");
    ASSERT_TRUE(manifest->name.IsNull());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'name' ignored, type string expected.", errors()[0]);
  }

  // Don't parse if name isn't a string.
  {
    auto& manifest = ParseManifest("{ \"name\": 42 }");
    ASSERT_TRUE(manifest->name.IsNull());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'name' ignored, type string expected.", errors()[0]);
  }
}

TEST_F(ManifestParserTest, ShortNameParseRules) {
  // Smoke test.
  {
    auto& manifest = ParseManifest("{ \"short_name\": \"foo\" }");
    ASSERT_EQ(manifest->short_name, "foo");
    ASSERT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Trim whitespaces.
  {
    auto& manifest = ParseManifest("{ \"short_name\": \"  foo  \" }");
    ASSERT_EQ(manifest->short_name, "foo");
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Don't parse if name isn't a string.
  {
    auto& manifest = ParseManifest("{ \"short_name\": {} }");
    ASSERT_TRUE(manifest->short_name.IsNull());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'short_name' ignored, type string expected.",
              errors()[0]);
  }

  // Don't parse if name isn't a string.
  {
    auto& manifest = ParseManifest("{ \"short_name\": 42 }");
    ASSERT_TRUE(manifest->short_name.IsNull());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'short_name' ignored, type string expected.",
              errors()[0]);
  }
}

TEST_F(ManifestParserTest, StartURLParseRules) {
  // Smoke test.
  {
    auto& manifest = ParseManifest("{ \"start_url\": \"land.html\" }");
    ASSERT_EQ(manifest->start_url, KURL(DefaultDocumentUrl(), "land.html"));
    ASSERT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Whitespaces.
  {
    auto& manifest = ParseManifest("{ \"start_url\": \"  land.html  \" }");
    ASSERT_EQ(manifest->start_url, KURL(DefaultDocumentUrl(), "land.html"));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Don't parse if property isn't a string.
  {
    auto& manifest = ParseManifest("{ \"start_url\": {} }");
    ASSERT_TRUE(manifest->start_url.IsEmpty());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'start_url' ignored, type string expected.",
              errors()[0]);
  }

  // Don't parse if property isn't a string.
  {
    auto& manifest = ParseManifest("{ \"start_url\": 42 }");
    ASSERT_TRUE(manifest->start_url.IsEmpty());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'start_url' ignored, type string expected.",
              errors()[0]);
  }

  // Don't parse if property isn't a valid URL.
  {
    auto& manifest =
        ParseManifest("{ \"start_url\": \"http://www.google.ca:a\" }");
    ASSERT_TRUE(manifest->start_url.IsEmpty());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'start_url' ignored, URL is invalid.", errors()[0]);
  }

  // Absolute start_url, same origin with document.
  {
    auto& manifest =
        ParseManifestWithURLs("{ \"start_url\": \"http://foo.com/land.html\" }",
                              KURL("http://foo.com/manifest.json"),
                              KURL("http://foo.com/index.html"));
    ASSERT_EQ(manifest->start_url.GetString(), "http://foo.com/land.html");
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Absolute start_url, cross origin with document.
  {
    auto& manifest =
        ParseManifestWithURLs("{ \"start_url\": \"http://bar.com/land.html\" }",
                              KURL("http://foo.com/manifest.json"),
                              KURL("http://foo.com/index.html"));
    ASSERT_TRUE(manifest->start_url.IsEmpty());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'start_url' ignored, should "
        "be same origin as document.",
        errors()[0]);
  }

  // Resolving has to happen based on the manifest_url.
  {
    auto& manifest =
        ParseManifestWithURLs("{ \"start_url\": \"land.html\" }",
                              KURL("http://foo.com/landing/manifest.json"),
                              KURL("http://foo.com/index.html"));
    ASSERT_EQ(manifest->start_url.GetString(),
              "http://foo.com/landing/land.html");
    EXPECT_EQ(0u, GetErrorCount());
  }
}

TEST_F(ManifestParserTest, ScopeParseRules) {
  // Smoke test.
  {
    auto& manifest = ParseManifest(
        "{ \"scope\": \"land\", \"start_url\": \"land/landing.html\" }");
    ASSERT_EQ(manifest->scope, KURL(DefaultDocumentUrl(), "land"));
    ASSERT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Whitespaces.
  {
    auto& manifest = ParseManifest(
        "{ \"scope\": \"  land  \", \"start_url\": \"land/landing.html\" }");
    ASSERT_EQ(manifest->scope, KURL(DefaultDocumentUrl(), "land"));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Return the default value if the property isn't a string.
  {
    auto& manifest = ParseManifest("{ \"scope\": {} }");
    ASSERT_EQ(manifest->scope.GetString(), DefaultDocumentUrl().BaseAsString());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'scope' ignored, type string expected.", errors()[0]);
  }

  // Return the default value if property isn't a string.
  {
    auto& manifest = ParseManifest(
        "{ \"scope\": 42, "
        "\"start_url\": \"http://foo.com/land/landing.html\" }");
    ASSERT_EQ(manifest->scope, KURL(DefaultDocumentUrl(), "land/"));
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'scope' ignored, type string expected.", errors()[0]);
  }

  // Absolute scope, start URL is in scope.
  {
    auto& manifest = ParseManifestWithURLs(
        "{ \"scope\": \"http://foo.com/land\", "
        "\"start_url\": \"http://foo.com/land/landing.html\" }",
        KURL("http://foo.com/manifest.json"),
        KURL("http://foo.com/index.html"));
    ASSERT_EQ(manifest->scope.GetString(), "http://foo.com/land");
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Absolute scope, start URL is not in scope.
  {
    auto& manifest = ParseManifestWithURLs(
        "{ \"scope\": \"http://foo.com/land\", "
        "\"start_url\": \"http://foo.com/index.html\" }",
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
        "{ \"scope\": \"http://foo.com/land\", "
        "\"start_url\": \"http://bar.com/land/landing.html\" }",
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
        "{ \"scope\": \"http://foo.com/land\", "
        "\"start_url\": \"http://foo.com/land/landing.html\" }",
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
        ParseManifestWithURLs("{ \"scope\": \"http://foo.com/land\" }",
                              KURL("http://foo.com/manifest.json"),
                              KURL("http://foo.com/land/site/index.html"));
    ASSERT_EQ(manifest->scope.GetString(), "http://foo.com/land");
    ASSERT_EQ(0u, GetErrorCount());
  }

  // No start URL. Document is out of scope.
  {
    KURL document_url("http://foo.com/index.html");
    auto& manifest =
        ParseManifestWithURLs("{ \"scope\": \"http://foo.com/land\" }",
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
        "{ \"scope\": \"treasure\" }", KURL("http://foo.com/map/manifest.json"),
        KURL("http://foo.com/map/treasure/island/index.html"));
    ASSERT_EQ(manifest->scope.GetString(), "http://foo.com/map/treasure");
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Scope is parent directory.
  {
    auto& manifest = ParseManifestWithURLs(
        "{ \"scope\": \"..\" }", KURL("http://foo.com/map/manifest.json"),
        KURL("http://foo.com/index.html"));
    ASSERT_EQ(manifest->scope.GetString(), "http://foo.com/");
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Scope tries to go up past domain.
  {
    auto& manifest = ParseManifestWithURLs(
        "{ \"scope\": \"../..\" }", KURL("http://foo.com/map/manifest.json"),
        KURL("http://foo.com/index.html"));
    ASSERT_EQ(manifest->scope.GetString(), "http://foo.com/");
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Scope defaults to start_url with the filename, query, and fragment removed.
  {
    auto& manifest = ParseManifest("{ \"start_url\": \"land/landing.html\" }");
    ASSERT_EQ(manifest->scope, KURL(DefaultDocumentUrl(), "land/"));
    EXPECT_EQ(0u, GetErrorCount());
  }

  {
    auto& manifest =
        ParseManifest("{ \"start_url\": \"land/land/landing.html\" }");
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
    auto& manifest = ParseManifest("{ \"display\": \"browser\" }");
    EXPECT_EQ(manifest->display, blink::mojom::DisplayMode::kBrowser);
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Trim whitespaces.
  {
    auto& manifest = ParseManifest("{ \"display\": \"  browser  \" }");
    EXPECT_EQ(manifest->display, blink::mojom::DisplayMode::kBrowser);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Don't parse if name isn't a string.
  {
    auto& manifest = ParseManifest("{ \"display\": {} }");
    EXPECT_EQ(manifest->display, blink::mojom::DisplayMode::kUndefined);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'display' ignored,"
        " type string expected.",
        errors()[0]);
  }

  // Don't parse if name isn't a string.
  {
    auto& manifest = ParseManifest("{ \"display\": 42 }");
    EXPECT_EQ(manifest->display, blink::mojom::DisplayMode::kUndefined);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'display' ignored,"
        " type string expected.",
        errors()[0]);
  }

  // Parse fails if string isn't known.
  {
    auto& manifest = ParseManifest("{ \"display\": \"browser_something\" }");
    EXPECT_EQ(manifest->display, blink::mojom::DisplayMode::kUndefined);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("unknown 'display' value ignored.", errors()[0]);
  }

  // Accept 'fullscreen'.
  {
    auto& manifest = ParseManifest("{ \"display\": \"fullscreen\" }");
    EXPECT_EQ(manifest->display, blink::mojom::DisplayMode::kFullscreen);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept 'standalone'.
  {
    auto& manifest = ParseManifest("{ \"display\": \"standalone\" }");
    EXPECT_EQ(manifest->display, blink::mojom::DisplayMode::kStandalone);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept 'minimal-ui'.
  {
    auto& manifest = ParseManifest("{ \"display\": \"minimal-ui\" }");
    EXPECT_EQ(manifest->display, blink::mojom::DisplayMode::kMinimalUi);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept 'browser'.
  {
    auto& manifest = ParseManifest("{ \"display\": \"browser\" }");
    EXPECT_EQ(manifest->display, blink::mojom::DisplayMode::kBrowser);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Case insensitive.
  {
    auto& manifest = ParseManifest("{ \"display\": \"BROWSER\" }");
    EXPECT_EQ(manifest->display, blink::mojom::DisplayMode::kBrowser);
    EXPECT_EQ(0u, GetErrorCount());
  }
}

TEST_F(ManifestParserTest, DisplayOverrideParseRules) {
  ScopedWebAppManifestDisplayOverrideForTest display_override(true);

  // Smoke test: if no display_override, no value.
  {
    auto& manifest = ParseManifest("{ \"display_override\": [] }");
    EXPECT_TRUE(manifest->display_override.IsEmpty());
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Smoke test: if not array, value will be ignored
  {
    auto& manifest = ParseManifest("{ \"display_override\": 23 }");
    EXPECT_TRUE(manifest->display_override.IsEmpty());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'display_override' ignored, type array expected.",
              errors()[0]);
  }

  // Smoke test: if array value is not a string, it will be ignored
  {
    auto& manifest = ParseManifest("{ \"display_override\": [ 23 ] }");
    EXPECT_TRUE(manifest->display_override.IsEmpty());
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Smoke test: if array value is not not recognized, it will be ignored
  {
    auto& manifest = ParseManifest("{ \"display_override\": [ \"test\" ] }");
    EXPECT_TRUE(manifest->display_override.IsEmpty());
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Case insensitive
  {
    auto& manifest = ParseManifest("{ \"display_override\": [ \"BROWSER\" ] }");
    EXPECT_FALSE(manifest->display_override.IsEmpty());
    EXPECT_EQ(manifest->display_override[0],
              blink::mojom::DisplayMode::kBrowser);
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Trim whitespace
  {
    auto& manifest =
        ParseManifest("{ \"display_override\": [ \" browser \" ] }");
    EXPECT_FALSE(manifest->display_override.IsEmpty());
    EXPECT_EQ(manifest->display_override[0],
              blink::mojom::DisplayMode::kBrowser);
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept 'browser'
  {
    auto& manifest = ParseManifest("{ \"display_override\": [ \"browser\" ] }");
    EXPECT_FALSE(manifest->display_override.IsEmpty());
    EXPECT_EQ(manifest->display_override[0],
              blink::mojom::DisplayMode::kBrowser);
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept 'browser', 'minimal-ui'
  {
    auto& manifest = ParseManifest(
        "{ \"display_override\": [ \"browser\", \"minimal-ui\" ] }");
    EXPECT_FALSE(manifest->display_override.IsEmpty());
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
        "{ \"display_override\": [ 3, \"browser\", \"invalid-display\", "
        "\"minimal-ui\" ] }");
    EXPECT_FALSE(manifest->display_override.IsEmpty());
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
        "{ \"display\": \"standalone\", \"display_override\": [ \"browser\", "
        "\"minimal-ui\", \"standalone\" ] }");
    EXPECT_EQ(manifest->display, blink::mojom::DisplayMode::kStandalone);
    EXPECT_EQ(0u, GetErrorCount());
    EXPECT_FALSE(manifest->display_override.IsEmpty());
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
    auto& manifest = ParseManifest(
        "{ \"display_override\": [ \"browser\", \"minimal-ui\", "
        "\"browser\" ] }");
    EXPECT_FALSE(manifest->display_override.IsEmpty());
    EXPECT_EQ(manifest->display_override[0],
              blink::mojom::DisplayMode::kBrowser);
    EXPECT_EQ(manifest->display_override[1],
              blink::mojom::DisplayMode::kMinimalUi);
    EXPECT_EQ(manifest->display_override[2],
              blink::mojom::DisplayMode::kBrowser);
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(0u, GetErrorCount());
  }
}

TEST_F(ManifestParserTest, OrientationParseRules) {
  // Smoke test.
  {
    auto& manifest = ParseManifest("{ \"orientation\": \"natural\" }");
    EXPECT_EQ(manifest->orientation,
              device::mojom::ScreenOrientationLockType::NATURAL);
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Trim whitespaces.
  {
    auto& manifest = ParseManifest("{ \"orientation\": \"natural\" }");
    EXPECT_EQ(manifest->orientation,
              device::mojom::ScreenOrientationLockType::NATURAL);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Don't parse if name isn't a string.
  {
    auto& manifest = ParseManifest("{ \"orientation\": {} }");
    EXPECT_EQ(manifest->orientation,
              device::mojom::ScreenOrientationLockType::DEFAULT);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'orientation' ignored, type string expected.",
              errors()[0]);
  }

  // Don't parse if name isn't a string.
  {
    auto& manifest = ParseManifest("{ \"orientation\": 42 }");
    EXPECT_EQ(manifest->orientation,
              device::mojom::ScreenOrientationLockType::DEFAULT);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'orientation' ignored, type string expected.",
              errors()[0]);
  }

  // Parse fails if string isn't known.
  {
    auto& manifest = ParseManifest("{ \"orientation\": \"naturalish\" }");
    EXPECT_EQ(manifest->orientation,
              device::mojom::ScreenOrientationLockType::DEFAULT);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("unknown 'orientation' value ignored.", errors()[0]);
  }

  // Accept 'any'.
  {
    auto& manifest = ParseManifest("{ \"orientation\": \"any\" }");
    EXPECT_EQ(manifest->orientation,
              device::mojom::ScreenOrientationLockType::ANY);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept 'natural'.
  {
    auto& manifest = ParseManifest("{ \"orientation\": \"natural\" }");
    EXPECT_EQ(manifest->orientation,
              device::mojom::ScreenOrientationLockType::NATURAL);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept 'landscape'.
  {
    auto& manifest = ParseManifest("{ \"orientation\": \"landscape\" }");
    EXPECT_EQ(manifest->orientation,
              device::mojom::ScreenOrientationLockType::LANDSCAPE);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept 'landscape-primary'.
  {
    auto& manifest =
        ParseManifest("{ \"orientation\": \"landscape-primary\" }");
    EXPECT_EQ(manifest->orientation,
              device::mojom::ScreenOrientationLockType::LANDSCAPE_PRIMARY);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept 'landscape-secondary'.
  {
    auto& manifest =
        ParseManifest("{ \"orientation\": \"landscape-secondary\" }");
    EXPECT_EQ(manifest->orientation,
              device::mojom::ScreenOrientationLockType::LANDSCAPE_SECONDARY);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept 'portrait'.
  {
    auto& manifest = ParseManifest("{ \"orientation\": \"portrait\" }");
    EXPECT_EQ(manifest->orientation,
              device::mojom::ScreenOrientationLockType::PORTRAIT);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept 'portrait-primary'.
  {
    auto& manifest = ParseManifest("{ \"orientation\": \"portrait-primary\" }");
    EXPECT_EQ(manifest->orientation,
              device::mojom::ScreenOrientationLockType::PORTRAIT_PRIMARY);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept 'portrait-secondary'.
  {
    auto& manifest =
        ParseManifest("{ \"orientation\": \"portrait-secondary\" }");
    EXPECT_EQ(manifest->orientation,
              device::mojom::ScreenOrientationLockType::PORTRAIT_SECONDARY);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Case insensitive.
  {
    auto& manifest = ParseManifest("{ \"orientation\": \"LANDSCAPE\" }");
    EXPECT_EQ(manifest->orientation,
              device::mojom::ScreenOrientationLockType::LANDSCAPE);
    EXPECT_EQ(0u, GetErrorCount());
  }
}

TEST_F(ManifestParserTest, IconsParseRules) {
  // Smoke test: if no icon, no value.
  {
    auto& manifest = ParseManifest("{ \"icons\": [] }");
    EXPECT_TRUE(manifest->icons.IsEmpty());
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Smoke test: if empty icon, no value.
  {
    auto& manifest = ParseManifest("{ \"icons\": [ {} ] }");
    EXPECT_TRUE(manifest->icons.IsEmpty());
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Smoke test: icon with invalid src, no value.
  {
    auto& manifest = ParseManifest("{ \"icons\": [ { \"icons\": [] } ] }");
    EXPECT_TRUE(manifest->icons.IsEmpty());
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Smoke test: if icon with empty src, it will be present in the list.
  {
    auto& manifest = ParseManifest("{ \"icons\": [ { \"src\": \"\" } ] }");
    EXPECT_FALSE(manifest->icons.IsEmpty());

    auto& icons = manifest->icons;
    EXPECT_EQ(icons.size(), 1u);
    EXPECT_EQ(icons[0]->src.GetString(), "http://foo.com/manifest.json");
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Smoke test: if one icons with valid src, it will be present in the list.
  {
    auto& manifest = ParseManifest("{ \"icons\": [{ \"src\": \"foo.jpg\" }] }");
    EXPECT_FALSE(manifest->icons.IsEmpty());

    auto& icons = manifest->icons;
    EXPECT_EQ(icons.size(), 1u);
    EXPECT_EQ(icons[0]->src.GetString(), "http://foo.com/foo.jpg");
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(0u, GetErrorCount());
  }
}

TEST_F(ManifestParserTest, IconSrcParseRules) {
  // Smoke test.
  {
    auto& manifest =
        ParseManifest("{ \"icons\": [ {\"src\": \"foo.png\" } ] }");
    EXPECT_FALSE(manifest->icons.IsEmpty());
    EXPECT_EQ(manifest->icons[0]->src, KURL(DefaultDocumentUrl(), "foo.png"));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Whitespaces.
  {
    auto& manifest =
        ParseManifest("{ \"icons\": [ {\"src\": \"   foo.png   \" } ] }");
    EXPECT_FALSE(manifest->icons.IsEmpty());
    EXPECT_EQ(manifest->icons[0]->src, KURL(DefaultDocumentUrl(), "foo.png"));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Don't parse if property isn't a string.
  {
    auto& manifest = ParseManifest("{ \"icons\": [ {\"src\": {} } ] }");
    EXPECT_TRUE(manifest->icons.IsEmpty());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'src' ignored, type string expected.", errors()[0]);
  }

  // Don't parse if property isn't a string.
  {
    auto& manifest = ParseManifest("{ \"icons\": [ {\"src\": 42 } ] }");
    EXPECT_TRUE(manifest->icons.IsEmpty());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'src' ignored, type string expected.", errors()[0]);
  }

  // Resolving has to happen based on the document_url.
  {
    auto& manifest = ParseManifestWithURLs(
        "{ \"icons\": [ {\"src\": \"icons/foo.png\" } ] }",
        KURL("http://foo.com/landing/index.html"), DefaultManifestUrl());
    EXPECT_FALSE(manifest->icons.IsEmpty());
    EXPECT_EQ(manifest->icons[0]->src.GetString(),
              "http://foo.com/landing/icons/foo.png");
    EXPECT_EQ(0u, GetErrorCount());
  }
}

TEST_F(ManifestParserTest, IconTypeParseRules) {
  // Smoke test.
  {
    auto& manifest =
        ParseManifest("{ \"icons\": [ {\"src\": \"\", \"type\": \"foo\" } ] }");
    EXPECT_FALSE(manifest->icons.IsEmpty());
    EXPECT_EQ(manifest->icons[0]->type, "foo");
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Trim whitespaces.
  {
    auto& manifest = ParseManifest(
        "{ \"icons\": [ {\"src\": \"\","
        " \"type\": \"  foo  \" } ] }");
    EXPECT_FALSE(manifest->icons.IsEmpty());
    EXPECT_EQ(manifest->icons[0]->type, "foo");
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Don't parse if property isn't a string.
  {
    auto& manifest =
        ParseManifest("{ \"icons\": [ {\"src\": \"\", \"type\": {} } ] }");
    EXPECT_FALSE(manifest->icons.IsEmpty());
    EXPECT_TRUE(manifest->icons[0]->type.IsEmpty());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'type' ignored, type string expected.", errors()[0]);
  }

  // Don't parse if property isn't a string.
  {
    auto& manifest =
        ParseManifest("{ \"icons\": [ {\"src\": \"\", \"type\": 42 } ] }");
    EXPECT_FALSE(manifest->icons.IsEmpty());
    EXPECT_TRUE(manifest->icons[0]->type.IsEmpty());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'type' ignored, type string expected.", errors()[0]);
  }
}

TEST_F(ManifestParserTest, IconSizesParseRules) {
  // Smoke test.
  {
    auto& manifest = ParseManifest(
        "{ \"icons\": [ {\"src\": \"\","
        "\"sizes\": \"42x42\" } ] }");
    EXPECT_FALSE(manifest->icons.IsEmpty());
    EXPECT_EQ(manifest->icons[0]->sizes.size(), 1u);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Trim whitespaces.
  {
    auto& manifest = ParseManifest(
        "{ \"icons\": [ {\"src\": \"\","
        "\"sizes\": \"  42x42  \" } ] }");
    EXPECT_FALSE(manifest->icons.IsEmpty());
    EXPECT_EQ(manifest->icons[0]->sizes.size(), 1u);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Ignore sizes if property isn't a string.
  {
    auto& manifest = ParseManifest(
        "{ \"icons\": [ {\"src\": \"\","
        "\"sizes\": {} } ] }");
    EXPECT_FALSE(manifest->icons.IsEmpty());
    EXPECT_EQ(manifest->icons[0]->sizes.size(), 0u);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'sizes' ignored, type string expected.", errors()[0]);
  }

  // Ignore sizes if property isn't a string.
  {
    auto& manifest = ParseManifest(
        "{ \"icons\": [ {\"src\": \"\","
        "\"sizes\": 42 } ] }");
    EXPECT_FALSE(manifest->icons.IsEmpty());
    EXPECT_EQ(manifest->icons[0]->sizes.size(), 0u);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'sizes' ignored, type string expected.", errors()[0]);
  }

  // Smoke test: value correctly parsed.
  {
    auto& manifest = ParseManifest(
        "{ \"icons\": [ {\"src\": \"\","
        "\"sizes\": \"42x42  48x48\" } ] }");
    EXPECT_FALSE(manifest->icons.IsEmpty());

    auto& icons = manifest->icons;
    EXPECT_EQ(icons[0]->sizes[0], gfx::Size(42, 42));
    EXPECT_EQ(icons[0]->sizes[1], gfx::Size(48, 48));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // <WIDTH>'x'<HEIGHT> and <WIDTH>'X'<HEIGHT> are equivalent.
  {
    auto& manifest = ParseManifest(
        "{ \"icons\": [ {\"src\": \"\","
        "\"sizes\": \"42X42  48X48\" } ] }");
    EXPECT_FALSE(manifest->icons.IsEmpty());

    auto& icons = manifest->icons;
    EXPECT_EQ(icons[0]->sizes[0], gfx::Size(42, 42));
    EXPECT_EQ(icons[0]->sizes[1], gfx::Size(48, 48));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Twice the same value is parsed twice.
  {
    auto& manifest = ParseManifest(
        "{ \"icons\": [ {\"src\": \"\","
        "\"sizes\": \"42X42  42x42\" } ] }");
    EXPECT_FALSE(manifest->icons.IsEmpty());

    auto& icons = manifest->icons;
    EXPECT_EQ(icons[0]->sizes[0], gfx::Size(42, 42));
    EXPECT_EQ(icons[0]->sizes[1], gfx::Size(42, 42));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Width or height can't start with 0.
  {
    auto& manifest = ParseManifest(
        "{ \"icons\": [ {\"src\": \"\","
        "\"sizes\": \"004X007  042x00\" } ] }");
    EXPECT_FALSE(manifest->icons.IsEmpty());
    EXPECT_EQ(manifest->icons[0]->sizes.size(), 0u);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("found icon with no valid size.", errors()[0]);
  }

  // Width and height MUST contain digits.
  {
    auto& manifest = ParseManifest(
        "{ \"icons\": [ {\"src\": \"\","
        "\"sizes\": \"e4X1.0  55ax1e10\" } ] }");
    EXPECT_FALSE(manifest->icons.IsEmpty());
    EXPECT_EQ(manifest->icons[0]->sizes.size(), 0u);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("found icon with no valid size.", errors()[0]);
  }

  // 'any' is correctly parsed and transformed to gfx::Size(0,0).
  {
    auto& manifest = ParseManifest(
        "{ \"icons\": [ {\"src\": \"\","
        "\"sizes\": \"any AnY ANY aNy\" } ] }");
    gfx::Size any = gfx::Size(0, 0);
    EXPECT_FALSE(manifest->icons.IsEmpty());

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
        "{ \"icons\": [ {\"src\": \"\","
        "\"sizes\": \"x 40xx 1x2x3 x42 42xx42\" } ] }");
    EXPECT_FALSE(manifest->icons.IsEmpty());
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
    auto& manifest = ParseManifest(
        "{ \"icons\": [ {\"src\": \"\","
        "\"purpose\": \"any\" } ] }");
    EXPECT_FALSE(manifest->icons.IsEmpty());
    EXPECT_EQ(manifest->icons[0]->purpose.size(), 1u);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Trim leading and trailing whitespaces.
  {
    auto& manifest = ParseManifest(
        "{ \"icons\": [ {\"src\": \"\","
        "\"purpose\": \"  any  \" } ] }");
    EXPECT_FALSE(manifest->icons.IsEmpty());
    EXPECT_EQ(manifest->icons[0]->purpose.size(), 1u);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // 'any' is added when property isn't present.
  {
    auto& manifest = ParseManifest("{ \"icons\": [ {\"src\": \"\" } ] }");
    EXPECT_FALSE(manifest->icons.IsEmpty());

    auto& icons = manifest->icons;
    EXPECT_EQ(icons[0]->purpose.size(), 1u);
    EXPECT_EQ(icons[0]->purpose[0],
              mojom::blink::ManifestImageResource::Purpose::ANY);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // 'any' is added with error message when property isn't a string (is a
  // number).
  {
    auto& manifest = ParseManifest(
        "{ \"icons\": [ {\"src\": \"\","
        "\"purpose\": 42 } ] }");
    EXPECT_FALSE(manifest->icons.IsEmpty());

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
    auto& manifest = ParseManifest(
        "{ \"icons\": [ {\"src\": \"\","
        "\"purpose\": {} } ] }");
    EXPECT_FALSE(manifest->icons.IsEmpty());

    auto& icons = manifest->icons;
    EXPECT_EQ(icons[0]->purpose.size(), 1u);
    EXPECT_EQ(icons[0]->purpose[0],
              mojom::blink::ManifestImageResource::Purpose::ANY);
    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ(kPurposeParseStringError, errors()[0]);
  }

  // Smoke test: values correctly parsed.
  {
    auto& manifest = ParseManifest(
        "{ \"icons\": [ {\"src\": \"\","
        "\"purpose\": \"Any Monochrome Maskable\" } ] }");
    EXPECT_FALSE(manifest->icons.IsEmpty());

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
    auto& manifest = ParseManifest(
        "{ \"icons\": [ {\"src\": \"\","
        "\"purpose\": \"  Any   Monochrome  \" } ] }");
    EXPECT_FALSE(manifest->icons.IsEmpty());

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
    auto& manifest = ParseManifest(
        "{ \"icons\": [ {\"src\": \"\","
        "\"purpose\": \"monochrome monochrome\" } ] }");
    EXPECT_FALSE(manifest->icons.IsEmpty());

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
    auto& manifest = ParseManifest(
        "{ \"icons\": [ {\"src\": \"\","
        "\"purpose\": \"monochrome fizzbuzz\" } ] }");
    EXPECT_FALSE(manifest->icons.IsEmpty());

    auto& icons = manifest->icons;
    ASSERT_EQ(icons[0]->purpose.size(), 1u);
    EXPECT_EQ(icons[0]->purpose[0],
              mojom::blink::ManifestImageResource::Purpose::MONOCHROME);
    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ(kSomeInvalidPurposeError, errors()[0]);
  }

  // If developer-supplied purpose is invalid, entire icon is removed.
  {
    auto& manifest = ParseManifest(
        "{ \"icons\": [ {\"src\": \"\","
        "\"purpose\": \"fizzbuzz\" } ] }");
    ASSERT_TRUE(manifest->icons.IsEmpty());
    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ(kPurposeInvalidValueError, errors()[0]);
  }

  // Two icons, one with an invalid purpose and the other normal.
  {
    auto& manifest = ParseManifest(
        "{ \"icons\": [ {\"src\": \"\", \"purpose\": \"fizzbuzz\" }, "
        "               {\"src\": \"\" }] }");
    EXPECT_FALSE(manifest->icons.IsEmpty());

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
    auto& manifest = ParseManifest("{ \"shortcuts\": [] }");
    EXPECT_TRUE(manifest->shortcuts.IsEmpty());
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Smoke test: if empty shortcut, no value.
  {
    auto& manifest = ParseManifest("{ \"shortcuts\": [ {} ] }");
    EXPECT_TRUE(manifest->icons.IsEmpty());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'url' of 'shortcut' not present.", errors()[0]);
  }

  // Smoke test: shortcut with invalid name and url, it will not be present in
  // the list.
  {
    auto& manifest =
        ParseManifest("{ \"shortcuts\": [ { \"shortcuts\": [] } ] }");
    EXPECT_TRUE(manifest->icons.IsEmpty());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'url' of 'shortcut' not present.", errors()[0]);
  }

  // Smoke test: shortcut with no name, it will not be present in the list.
  {
    auto& manifest = ParseManifest("{ \"shortcuts\": [ { \"url\": \"\" } ] }");
    EXPECT_TRUE(manifest->shortcuts.IsEmpty());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'name' of 'shortcut' not present.", errors()[0]);
  }

  // Smoke test: shortcut with no url, it will not be present in the list.
  {
    auto& manifest = ParseManifest("{ \"shortcuts\": [ { \"name\": \"\" } ] }");
    EXPECT_TRUE(manifest->shortcuts.IsEmpty());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'url' of 'shortcut' not present.", errors()[0]);
  }

  // Smoke test: shortcut with empty name, and empty src, will not be present in
  // the list.
  {
    auto& manifest = ParseManifest(
        "{ \"shortcuts\": [ { \"name\": \"\", \"url\": \"\" } ] }");
    EXPECT_TRUE(manifest->shortcuts.IsEmpty());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'name' of 'shortcut' is an empty string.", errors()[0]);
  }

  // Smoke test: shortcut with valid (non-empty) name and src, will be present
  // in the list.
  {
    auto& manifest = ParseManifest(
        "{ \"shortcuts\": [{ \"name\": \"New Post\", \"url\": \"compose\" }] "
        "}");
    EXPECT_FALSE(manifest->shortcuts.IsEmpty());

    auto& shortcuts = manifest->shortcuts;
    EXPECT_EQ(shortcuts.size(), 1u);
    EXPECT_EQ(shortcuts[0]->name, "New Post");
    EXPECT_EQ(shortcuts[0]->url.GetString(), "http://foo.com/compose");
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(0u, GetErrorCount());
  }
}

TEST_F(ManifestParserTest, ShortcutNameParseRules) {
  // Smoke test.
  {
    auto& manifest = ParseManifest(
        "{ \"shortcuts\": [ {\"name\": \"foo\", \"url\": \"NameParseTest\" } ] "
        "}");
    EXPECT_FALSE(manifest->shortcuts.IsEmpty());
    EXPECT_EQ(manifest->shortcuts[0]->name, "foo");
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Trim whitespaces.
  {
    auto& manifest = ParseManifest(
        "{ \"shortcuts\": [ {\"name\": \"  foo  \", \"url\": \"NameParseTest\" "
        "} ] }");
    ASSERT_EQ(manifest->shortcuts[0]->name, "foo");
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Don't parse if shortcut->name isn't present.
  {
    auto& manifest =
        ParseManifest("{ \"shortcuts\": [ {\"url\": \"NameParseTest\" } ] }");
    EXPECT_TRUE(manifest->shortcuts.IsEmpty());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'name' of 'shortcut' not present.", errors()[0]);
  }

  // Don't parse if shortcut->name isn't a string.
  {
    auto& manifest = ParseManifest(
        "{ \"shortcuts\": [ {\"name\": {}, \"url\": \"NameParseTest\" } ] }");
    EXPECT_TRUE(manifest->shortcuts.IsEmpty());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'name' of 'shortcut' ignored, type string expected.",
              errors()[0]);
  }

  // Don't parse if shortcut->name isn't a string.
  {
    auto& manifest = ParseManifest(
        "{ \"shortcuts\": [ {\"name\": 42, \"url\": \"NameParseTest\" } ] }");
    EXPECT_TRUE(manifest->shortcuts.IsEmpty());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'name' of 'shortcut' ignored, type string expected.",
              errors()[0]);
  }

  // Don't parse if shortcut->name is an empty string.
  {
    auto& manifest = ParseManifest(
        "{ \"shortcuts\": [ {\"name\": \"\", \"url\": \"NameParseTest\" } ] }");
    EXPECT_TRUE(manifest->shortcuts.IsEmpty());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'name' of 'shortcut' is an empty string.", errors()[0]);
  }
}

TEST_F(ManifestParserTest, ShortcutShortNameParseRules) {
  // Smoke test.
  {
    auto& manifest = ParseManifest(
        "{ \"shortcuts\": [ {\"name\": \"ShortNameParseTest\", \"short_name\": "
        "\"foo\", \"url\": \"ShortNameParseTest\" } ] }");
    ASSERT_EQ(manifest->shortcuts[0]->short_name, "foo");
    ASSERT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Shortcut member is parsed when no short_name is present
  {
    auto& manifest = ParseManifest(
        "{ \"shortcuts\": [ {\"name\": \"ShortNameParseTest\", \"url\": "
        "\"ShortNameParseTest\" } ] }");
    ASSERT_TRUE(manifest->shortcuts[0]->short_name.IsNull());
    ASSERT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Trim whitespaces.
  {
    auto& manifest = ParseManifest(
        "{ \"shortcuts\": [ {\"name\": \"ShortNameParseTest\", \"short_name\": "
        "\"  foo  \", \"url\": \"ShortNameParseTest\" } ] }");
    ASSERT_EQ(manifest->shortcuts[0]->short_name, "foo");
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Don't parse short_name if it isn't a string.
  {
    auto& manifest = ParseManifest(
        "{ \"shortcuts\": [ {\"name\": \"ShortNameParseTest\", \"short_name\": "
        "{}, \"url\": \"ShortNameParseTest\" } ] }");
    ASSERT_TRUE(manifest->shortcuts[0]->short_name.IsNull());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'short_name' of 'shortcut' ignored, type string expected.",
        errors()[0]);
  }

  // Don't parse short_name if it isn't a string.
  {
    auto& manifest = ParseManifest(
        "{ \"shortcuts\": [ {\"name\": \"ShortNameParseTest\", \"short_name\": "
        "42, \"url\": \"ShortNameParseTest\" } ] }");
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
        "{ \"shortcuts\": [ {\"name\": \"DescriptionParseTest\", "
        "\"description\": "
        "\"foo\", \"url\": \"DescriptionParseTest\" } ] }");
    ASSERT_EQ(manifest->shortcuts[0]->description, "foo");
    ASSERT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Shortcut member is parsed when no description is present
  {
    auto& manifest = ParseManifest(
        "{ \"shortcuts\": [ {\"name\": \"DescriptionParseTest\", \"url\": "
        "\"DescriptionParseTest\" } ] }");
    ASSERT_TRUE(manifest->shortcuts[0]->description.IsNull());
    ASSERT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Trim whitespaces.
  {
    auto& manifest = ParseManifest(
        "{ \"shortcuts\": [ {\"name\": \"DescriptionParseTest\", "
        "\"description\": "
        "\"  foo  \", \"url\": \"DescriptionParseTest\" } ] }");
    ASSERT_EQ(manifest->shortcuts[0]->description, "foo");
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Don't parse description if it isn't a string.
  {
    auto& manifest = ParseManifest(
        "{ \"shortcuts\": [ {\"name\": \"DescriptionParseTest\", "
        "\"description\": "
        "{}, \"url\": \"DescriptionParseTest\" } ] }");
    ASSERT_TRUE(manifest->shortcuts[0]->description.IsNull());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'description' of 'shortcut' ignored, type string expected.",
        errors()[0]);
  }

  // Don't parse description if it isn't a string.
  {
    auto& manifest = ParseManifest(
        "{ \"shortcuts\": [ {\"name\": \"DescriptionParseTest\", "
        "\"description\": "
        "42, \"url\": \"DescriptionParseTest\" } ] }");
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
        "{ \"shortcuts\": [ {\"name\": \"UrlParseTest\", \"url\": \"foo\" } ] "
        "}");
    EXPECT_FALSE(manifest->shortcuts.IsEmpty());
    EXPECT_EQ(manifest->shortcuts[0]->url, KURL(DefaultDocumentUrl(), "foo"));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Smoke test. Don't parse (with an error) when url is not present.
  {
    auto& manifest = ParseManifest("{ \"shortcuts\": [ { \"name\": \"\" } ] }");
    EXPECT_TRUE(manifest->shortcuts.IsEmpty());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'url' of 'shortcut' not present.", errors()[0]);
  }

  // Whitespaces.
  {
    auto& manifest = ParseManifest(
        "{ \"shortcuts\": [ {\"name\": \"UrlParseTest\", \"url\": \"   foo   "
        "\" } ] }");
    EXPECT_FALSE(manifest->shortcuts.IsEmpty());
    EXPECT_EQ(manifest->shortcuts[0]->url, KURL(DefaultDocumentUrl(), "foo"));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Don't parse if url isn't a string.
  {
    auto& manifest = ParseManifest(
        "{ \"shortcuts\": [ {\"name\": \"UrlParseTest\", \"url\": {} } ] }");
    EXPECT_TRUE(manifest->shortcuts.IsEmpty());
    EXPECT_EQ(2u, GetErrorCount());
    EXPECT_EQ("property 'url' ignored, type string expected.", errors()[0]);
    EXPECT_EQ("property 'url' of 'shortcut' not present.", errors()[1]);
  }

  // Don't parse if url isn't a string.
  {
    auto& manifest = ParseManifest(
        "{ \"shortcuts\": [ {\"name\": \"UrlParseTest\", \"url\": 42 } ] }");
    EXPECT_TRUE(manifest->shortcuts.IsEmpty());
    EXPECT_EQ(2u, GetErrorCount());
    EXPECT_EQ("property 'url' ignored, type string expected.", errors()[0]);
    EXPECT_EQ("property 'url' of 'shortcut' not present.", errors()[1]);
  }

  // Resolving has to happen based on the manifest_url.
  {
    auto& manifest = ParseManifestWithURLs(
        "{ \"shortcuts\": [ {\"name\": \"UrlParseTest\", \"url\": \"foo\" } ] "
        "}",
        KURL("http://foo.com/landing/manifest.json"), DefaultDocumentUrl());
    EXPECT_FALSE(manifest->shortcuts.IsEmpty());
    EXPECT_EQ(manifest->shortcuts[0]->url.GetString(),
              "http://foo.com/landing/foo");
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Shortcut url should have same origin as the document url.
  {
    auto& manifest = ParseManifestWithURLs(
        "{ \"shortcuts\": [ {\"name\": \"UrlParseTest\", \"url\": "
        "\"http://bar.com/landing\" } ] "
        "}",
        KURL("http://foo.com/landing/manifest.json"), DefaultDocumentUrl());
    EXPECT_TRUE(manifest->shortcuts.IsEmpty());
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
        "{ \"scope\": \"http://foo.com/landing\", \"shortcuts\": [ {\"name\": "
        "\"UrlParseTest\", \"url\": \"shortcut\" } ] }",
        KURL("http://foo.com/manifest.json"),
        KURL("http://foo.com/landing/index.html"));
    EXPECT_TRUE(manifest->shortcuts.IsEmpty());
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
        "{ \"scope\": \"http://foo.com/land\", \"start_url\": "
        "\"http://foo.com/land/landing.html\", \"shortcuts\": [ {\"name\": "
        "\"UrlParseTest\", \"url\": \"shortcut\" } ] }",
        KURL("http://foo.com/land/manifest.json"),
        KURL("http://foo.com/index.html"));
    EXPECT_FALSE(manifest->shortcuts.IsEmpty());
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
        "{ \"shortcuts\": [ {\"name\": \"IconParseTest\", \"url\": \"foo\", "
        "\"icons\": [] } ] }");
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_FALSE(manifest->shortcuts.IsEmpty());
    EXPECT_TRUE(manifest->shortcuts[0]->icons.IsEmpty());
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Smoke test: if empty icon, shortcut->icons has no value.
  {
    auto& manifest = ParseManifest(
        "{ \"shortcuts\": [ {\"name\": \"IconParseTest\", \"url\": \"foo\", "
        "\"icons\": [{}] } ] }");
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_FALSE(manifest->shortcuts.IsEmpty());
    EXPECT_TRUE(manifest->shortcuts[0]->icons.IsEmpty());
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Smoke test: icon with invalid src, shortcut->icons has no value.
  {
    auto& manifest = ParseManifest(
        "{ \"shortcuts\": [ {\"name\": \"IconParseTest\", \"url\": \"foo\", "
        "\"icons\": [{ \"icons\": [] }] } ] }");
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_FALSE(manifest->shortcuts.IsEmpty());
    EXPECT_TRUE(manifest->shortcuts[0]->icons.IsEmpty());
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Smoke test: if icon with empty src, it will be present in shortcut->icons.
  {
    auto& manifest = ParseManifest(
        "{ \"shortcuts\": [ {\"name\": \"IconParseTest\", \"url\": \"foo\", "
        "\"icons\": [ { \"src\": \"\" } ] } ] }");
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_FALSE(manifest->shortcuts.IsEmpty());
    EXPECT_FALSE(manifest->shortcuts[0]->icons.IsEmpty());

    auto& icons = manifest->shortcuts[0]->icons;
    EXPECT_EQ(icons.size(), 1u);
    EXPECT_EQ(icons[0]->src.GetString(), "http://foo.com/manifest.json");
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Smoke test: if one icon with valid src, it will be present in
  // shortcut->icons.
  {
    auto& manifest = ParseManifest(
        "{ \"shortcuts\": [ {\"name\": \"IconParseTest\", \"url\": \"foo\", "
        "\"icons\": [ { \"src\": \"foo.jpg\" } ] } ] }");
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_FALSE(manifest->shortcuts.IsEmpty());
    EXPECT_FALSE(manifest->shortcuts[0]->icons.IsEmpty());
    auto& icons = manifest->shortcuts[0]->icons;
    EXPECT_EQ(icons.size(), 1u);
    EXPECT_EQ(icons[0]->src.GetString(), "http://foo.com/foo.jpg");
    EXPECT_EQ(0u, GetErrorCount());
  }
}
TEST_F(ManifestParserTest, FileHandlerParseRules) {
  // Does not contain file_handlers field.
  {
    auto& manifest = ParseManifest("{ }");
    ASSERT_EQ(0u, GetErrorCount());
    EXPECT_EQ(0u, manifest->file_handlers.size());
  }

  // file_handlers is not an array.
  {
    auto& manifest = ParseManifest("{ \"file_handlers\": { } }");
    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'file_handlers' ignored, type array expected.",
              errors()[0]);
    EXPECT_EQ(0u, manifest->file_handlers.size());
  }

  // Contains file_handlers field but no file handlers.
  {
    auto& manifest = ParseManifest("{ \"file_handlers\": [ ] }");
    ASSERT_EQ(0u, GetErrorCount());
    EXPECT_EQ(0u, manifest->file_handlers.size());
  }

  // Entries must be objects
  {
    auto& manifest = ParseManifest(
        "{"
        "  \"file_handlers\": ["
        "    \"hello world\""
        "  ]"
        "}");
    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ("FileHandler ignored, type object expected.", errors()[0]);
    EXPECT_EQ(0u, manifest->file_handlers.size());
  }

  // Entry without an action is invalid.
  {
    auto& manifest = ParseManifest(
        "{"
        "  \"file_handlers\": ["
        "    {"
        "      \"name\": \"name\","
        "      \"accept\": {"
        "        \"image/png\": ["
        "          \".png\""
        "        ]"
        "      }"
        "    }"
        "  ]"
        "}");
    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ("FileHandler ignored. Property 'action' is invalid.",
              errors()[0]);
    EXPECT_EQ(0u, manifest->file_handlers.size());
  }

  // Entry with an action on a different origin is invalid.
  {
    auto& manifest = ParseManifest(
        "{"
        "  \"file_handlers\": ["
        "    {"
        "      \"name\": \"name\","
        "      \"action\": \"https://example.com/files\","
        "      \"accept\": {"
        "        \"image/png\": ["
        "          \".png\""
        "        ]"
        "      }"
        "    }"
        "  ]"
        "}");
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
        "{"
        "  \"start_url\": \"/app/\","
        "  \"scope\": \"/app/\","
        "  \"file_handlers\": ["
        "    {"
        "      \"name\": \"name\","
        "      \"action\": \"/files\","
        "      \"accept\": {"
        "        \"image/png\": ["
        "          \".png\""
        "        ]"
        "      }"
        "    }"
        "  ]"
        "}");
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
        "{"
        "  \"file_handlers\": ["
        "    {"
        "      \"action\": \"/files\","
        "      \"accept\": {"
        "        \"image/png\": ["
        "          \".png\""
        "        ]"
        "      }"
        "    }"
        "  ]"
        "}");
    ASSERT_EQ(0u, GetErrorCount());
    EXPECT_EQ(1u, manifest->file_handlers.size());
  }

  // Entry without an accept is invalid.
  {
    auto& manifest = ParseManifest(
        "{"
        "  \"file_handlers\": ["
        "    {"
        "      \"name\": \"name\","
        "      \"action\": \"/files\""
        "    }"
        "  ]"
        "}");
    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ("FileHandler ignored. Property 'accept' is invalid.",
              errors()[0]);
    EXPECT_EQ(0u, manifest->file_handlers.size());
  }

  // Entry where accept is not an object is invalid.
  {
    auto& manifest = ParseManifest(
        "{"
        "  \"file_handlers\": ["
        "    {"
        "      \"name\": \"name\","
        "      \"action\": \"/files\","
        "      \"accept\": \"image/png\""
        "    }"
        "  ]"
        "}");
    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ("FileHandler ignored. Property 'accept' is invalid.",
              errors()[0]);
    EXPECT_EQ(0u, manifest->file_handlers.size());
  }

  // Entry where accept extensions are not an array or string is invalid.
  {
    auto& manifest = ParseManifest(
        "{"
        "  \"file_handlers\": ["
        "    {"
        "      \"name\": \"name\","
        "      \"action\": \"/files\","
        "      \"accept\": {"
        "        \"image/png\": {}"
        "      }"
        "    }"
        "  ]"
        "}");
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
        "{"
        "  \"file_handlers\": ["
        "    {"
        "      \"name\": \"name\","
        "      \"action\": \"/files\","
        "      \"accept\": {"
        "        \"image/png\": ["
        "          {}"
        "        ]"
        "      }"
        "    }"
        "  ]"
        "}");
    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'accept' file extension ignored, type string expected.",
              errors()[0]);
    EXPECT_EQ(1u, manifest->file_handlers.size());
  }

  // Entry with an empty list of extensions is valid.
  {
    auto& manifest = ParseManifest(
        "{"
        "  \"file_handlers\": ["
        "    {"
        "      \"name\": \"name\","
        "      \"action\": \"/files\","
        "      \"accept\": {"
        "        \"image/png\": []"
        "      }"
        "    }"
        "  ]"
        "}");
    auto& file_handlers = manifest->file_handlers;

    ASSERT_EQ(0u, GetErrorCount());
    ASSERT_EQ(1u, file_handlers.size());

    EXPECT_EQ("name", file_handlers[0]->name);
    EXPECT_EQ(KURL("http://foo.com/files"), file_handlers[0]->action);
    ASSERT_TRUE(file_handlers[0]->accept.Contains("image/png"));
    EXPECT_EQ(0u, file_handlers[0]->accept.find("image/png")->value.size());
  }

  // Extensions that do not start with a '.' are invalid.
  {
    auto& manifest = ParseManifest(
        "{"
        "  \"file_handlers\": ["
        "    {"
        "      \"name\": \"name\","
        "      \"action\": \"/files\","
        "      \"accept\": {"
        "        \"image/png\": ["
        "          \"png\""
        "        ]"
        "      }"
        "    }"
        "  ]"
        "}");
    auto& file_handlers = manifest->file_handlers;

    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'accept' file extension ignored, must start with a '.'.",
        errors()[0]);
    ASSERT_EQ(1u, file_handlers.size());

    EXPECT_EQ("name", file_handlers[0]->name);
    EXPECT_EQ(KURL("http://foo.com/files"), file_handlers[0]->action);
    ASSERT_TRUE(file_handlers[0]->accept.Contains("image/png"));
    EXPECT_EQ(0u, file_handlers[0]->accept.find("image/png")->value.size());
  }

  // Extensions specified as a single string is valid.
  {
    auto& manifest = ParseManifest(
        "{"
        "  \"file_handlers\": ["
        "    {"
        "      \"name\": \"name\","
        "      \"action\": \"/files\","
        "      \"accept\": {"
        "        \"image/png\": \".png\""
        "      }"
        "    }"
        "  ]"
        "}");
    auto& file_handlers = manifest->file_handlers;

    ASSERT_EQ(0u, GetErrorCount());
    ASSERT_EQ(1u, file_handlers.size());

    EXPECT_EQ("name", file_handlers[0]->name);
    EXPECT_EQ(KURL("http://foo.com/files"), file_handlers[0]->action);
    ASSERT_TRUE(file_handlers[0]->accept.Contains("image/png"));
    ASSERT_EQ(1u, file_handlers[0]->accept.find("image/png")->value.size());
    EXPECT_EQ(".png", file_handlers[0]->accept.find("image/png")->value[0]);
  }

  // An array of extensions is valid.
  {
    auto& manifest = ParseManifest(
        "{"
        "  \"file_handlers\": ["
        "    {"
        "      \"name\": \"name\","
        "      \"action\": \"/files\","
        "      \"accept\": {"
        "        \"image/jpg\": ["
        "          \".jpg\","
        "          \".jpeg\""
        "        ]"
        "      }"
        "    }"
        "  ]"
        "}");
    auto& file_handlers = manifest->file_handlers;

    ASSERT_EQ(0u, GetErrorCount());
    ASSERT_EQ(1u, file_handlers.size());

    EXPECT_EQ("name", file_handlers[0]->name);
    EXPECT_EQ(KURL("http://foo.com/files"), file_handlers[0]->action);
    ASSERT_TRUE(file_handlers[0]->accept.Contains("image/jpg"));
    ASSERT_EQ(2u, file_handlers[0]->accept.find("image/jpg")->value.size());
    EXPECT_EQ(".jpg", file_handlers[0]->accept.find("image/jpg")->value[0]);
    EXPECT_EQ(".jpeg", file_handlers[0]->accept.find("image/jpg")->value[1]);
  }

  // Multiple mime types are valid.
  {
    auto& manifest = ParseManifest(
        "{"
        "  \"file_handlers\": ["
        "    {"
        "      \"name\": \"Image\","
        "      \"action\": \"/files\","
        "      \"accept\": {"
        "        \"image/png\": \".png\","
        "        \"image/jpg\": ["
        "          \".jpg\","
        "          \".jpeg\""
        "        ]"
        "      }"
        "    }"
        "  ]"
        "}");
    auto& file_handlers = manifest->file_handlers;

    ASSERT_EQ(0u, GetErrorCount());
    ASSERT_EQ(1u, file_handlers.size());

    EXPECT_EQ("Image", file_handlers[0]->name);
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
        "{"
        "  \"file_handlers\": ["
        "    {"
        "      \"name\": \"Graph\","
        "      \"action\": \"/graph\","
        "      \"accept\": {"
        "        \"text/svg+xml\": ["
        "          \".svg\","
        "          \".graph\""
        "        ]"
        "      }"
        "    },"
        "    {"
        "      \"name\": \"Raw\","
        "      \"action\": \"/raw\","
        "      \"accept\": {"
        "        \"text/csv\": \".csv\""
        "      }"
        "    }"
        "  ]"
        "}");
    auto& file_handlers = manifest->file_handlers;

    ASSERT_EQ(0u, GetErrorCount());
    ASSERT_EQ(2u, file_handlers.size());

    EXPECT_EQ("Graph", file_handlers[0]->name);
    EXPECT_EQ(KURL("http://foo.com/graph"), file_handlers[0]->action);
    ASSERT_TRUE(file_handlers[0]->accept.Contains("text/svg+xml"));
    ASSERT_EQ(2u, file_handlers[0]->accept.find("text/svg+xml")->value.size());
    EXPECT_EQ(".svg", file_handlers[0]->accept.find("text/svg+xml")->value[0]);
    EXPECT_EQ(".graph",
              file_handlers[0]->accept.find("text/svg+xml")->value[1]);

    EXPECT_EQ("Raw", file_handlers[1]->name);
    EXPECT_EQ(KURL("http://foo.com/raw"), file_handlers[1]->action);
    ASSERT_TRUE(file_handlers[1]->accept.Contains("text/csv"));
    ASSERT_EQ(1u, file_handlers[1]->accept.find("text/csv")->value.size());
    EXPECT_EQ(".csv", file_handlers[1]->accept.find("text/csv")->value[0]);
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
    auto& manifest = ParseManifest("{ \"protocol_handlers\": { } }");
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'protocol_handlers' ignored, type array expected.",
              errors()[0]);
    EXPECT_EQ(0u, manifest->protocol_handlers.size());
  }

  // Contains protocol_handlers field but no protocol handlers.
  {
    auto& manifest = ParseManifest("{ \"protocol_handlers\": [ ] }");
    ASSERT_EQ(0u, GetErrorCount());
    EXPECT_EQ(0u, manifest->protocol_handlers.size());
  }

  // Entries must be objects
  {
    auto& manifest = ParseManifest(
        "{"
        "  \"protocol_handlers\": ["
        "    \"hello world\""
        "  ]"
        "}");
    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ("protocol_handlers entry ignored, type object expected.",
              errors()[0]);
    EXPECT_EQ(0u, manifest->protocol_handlers.size());
  }

  // A valid protocol handler.
  {
    auto& manifest = ParseManifest(
        "{"
        "  \"protocol_handlers\": ["
        "    {"
        "      \"protocol\": \"web+github\","
        "      \"url\": \"http://foo.com/?profile=%s\""
        "    }"
        "  ]"
        "}");
    auto& protocol_handlers = manifest->protocol_handlers;

    ASSERT_EQ(0u, GetErrorCount());
    ASSERT_EQ(1u, protocol_handlers.size());

    ASSERT_EQ("web+github", protocol_handlers[0]->protocol);
    ASSERT_EQ("http://foo.com/?profile=%s", protocol_handlers[0]->url);
  }

  // An invalid protocol handler with the URL not being from the same origin.
  {
    auto& manifest = ParseManifest(
        "{"
        "  \"protocol_handlers\": ["
        "    {"
        "      \"protocol\": \"web+github\","
        "      \"url\": \"http://bar.com/?profile=%s\""
        "    }"
        "  ]"
        "}");
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
        "{"
        "  \"start_url\": \"/app/\","
        "  \"scope\": \"/app/\","
        "  \"protocol_handlers\": ["
        "    {"
        "      \"protocol\": \"web+github\","
        "      \"url\": \"/?profile=%s\""
        "    }"
        "  ]"
        "}");
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
    auto& manifest = ParseManifest(
        "{"
        "  \"protocol_handlers\": ["
        "    {"
        "      \"url\": \"http://foo.com/?profile=%s\""
        "    }"
        "  ]"
        "}");
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
    auto& manifest = ParseManifest(
        "{"
        "  \"protocol_handlers\": ["
        "    {"
        "      \"protocol\": \"web+github\""
        "    }"
        "  ]"
        "}");
    auto& protocol_handlers = manifest->protocol_handlers;

    ASSERT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "protocol_handlers entry ignored, required property 'url' is missing.",
        errors()[0]);
    ASSERT_EQ(0u, protocol_handlers.size());
  }

  // An invalid protocol handler with a url that doesn't contain the %s token.
  {
    auto& manifest = ParseManifest(
        "{"
        "  \"protocol_handlers\": ["
        "    {"
        "      \"protocol\": \"web+github\","
        "      \"url\": \"http://foo.com/?profile=\""
        "    }"
        "  ]"
        "}");
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
    auto& manifest = ParseManifest(
        "{"
        "  \"protocol_handlers\": ["
        "    {"
        "      \"protocol\": \"github\","
        "      \"url\": \"http://foo.com/?profile=\""
        "    }"
        "  ]"
        "}");
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
        "{"
        "  \"protocol_handlers\": ["
        "    {"
        "      \"protocol\": \"web+github\","
        "      \"url\": \"http://foo.com/?profile=%s\""
        "    },"
        "    {"
        "      \"protocol\": \"web+test\","
        "      \"url\": \"http://foo.com/?test=%s\""
        "    },"
        "    {"
        "      \"protocol\": \"web+relative\","
        "      \"url\": \"relativeURL=%s\""
        "    }"
        "  ]"
        "}");
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

TEST_F(ManifestParserTest, ShareTargetParseRules) {
  // Contains share_target field but no keys.
  {
    auto& manifest = ParseManifest("{ \"share_target\": {} }");
    EXPECT_FALSE(manifest->share_target.get());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'share_target' ignored. Property 'action' is invalid.",
              errors()[0]);
  }

  // Contains share_target field but no params key.
  {
    auto& manifest =
        ParseManifest("{ \"share_target\": { \"action\": \"\" } }");
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
    auto& manifest = ParseManifest("{ \"share_target\": { \"params\": {} } }");
    EXPECT_FALSE(manifest->share_target.get());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'share_target' ignored. Property 'action' is invalid.",
              errors()[0]);
  }

  // Key in share_target that isn't valid.
  {
    auto& manifest = ParseManifest(
        "{ \"share_target\": {\"incorrect_key\": \"some_value\" } }");
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
        "{ \"share_target\": { \"action\": \"\", \"params\": {} } }",
        manifest_url, document_url);
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
        "{ \"share_target\": { \"action\": \"\", \"params\": {} } }",
        manifest_url, document_url);
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
        "{ \"share_target\": { \"action\": {}, \"params\": {} } }",
        manifest_url, document_url);
    EXPECT_FALSE(manifest->share_target.get());
    EXPECT_EQ(2u, GetErrorCount());
    EXPECT_EQ("property 'action' ignored, type string expected.", errors()[0]);
    EXPECT_EQ("property 'share_target' ignored. Property 'action' is invalid.",
              errors()[1]);
  }

  // Don't parse if action property isn't a string.
  {
    auto& manifest = ParseManifestWithURLs(
        "{ \"share_target\": { \"action\": 42, \"params\": {} } }",
        manifest_url, document_url);
    EXPECT_FALSE(manifest->share_target.get());
    EXPECT_EQ(2u, GetErrorCount());
    EXPECT_EQ("property 'action' ignored, type string expected.", errors()[0]);
    EXPECT_EQ("property 'share_target' ignored. Property 'action' is invalid.",
              errors()[1]);
  }

  // Don't parse if params property isn't a dict.
  {
    auto& manifest = ParseManifestWithURLs(
        "{ \"share_target\": { \"action\": \"\", \"params\": \"\" } }",
        manifest_url, document_url);
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
        "{ \"share_target\": { \"action\": \"\", \"params\": 42 } }",
        manifest_url, document_url);
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
        "{ \"share_target\": { \"action\": \"\", \"params\": { \"text\": 42 }"
        " } }",
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
        "{ \"share_target\": { \"action\": \"\", "
        "\"params\": { \"title\": 42 } } }",
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
        "{ \"share_target\": { \"action\": \"\", \"params\": { \"url\": {}, "
        "\"text\": \"hi\" } } }",
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
        "{ \"share_target\": { \"action\": \"https://foo.com:a\", \"params\": "
        "{} } }",
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
        "{ \"share_target\": { \"action\": \"https://foo2.com/\", "
        "\"params\": {} } }",
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
        "{ \"start_url\": \"/app/\","
        "  \"scope\": \"/app/\","
        "  \"share_target\": { \"action\": \"/\", "
        "\"params\": {} } }",
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
        "{ \"share_target\": {\"action\": \"share/\", \"params\": {} } }",
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
        "{ \"share_target\": {\"action\": \"share/\", \"params\": { \"text\": "
        "\"foo\", \"title\": \"bar\", \"url\": \"baz\" } } }",
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
        "{ \"share_target\": { \"url_template\": "
        "\"foo.com/share?title={title}\", "
        "\"action\": \"share/\", \"params\": { \"text\": "
        "\"foo\", \"title\": \"bar\", \"url\": \"baz\" } } }",
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
        "{ \"share_target\": { \"action\": \"https://foo.com/#\", \"params\": "
        "{ \"title\": \"mytitle\" } } "
        "}",
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
        "{ \"share_target\": { \"action\": \"https://foo.com/#\", \"method\": "
        "10, \"enctype\": 10, \"params\": "
        "{ \"title\": \"mytitle\" } } "
        "}",
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
        "{ \"share_target\": { \"action\": \"https://foo.com/#\", \"method\": "
        "\"GET\", \"enctype\": \"application/x-www-form-urlencoded\", "
        "\"params\": "
        "{ \"title\": \"mytitle\" } } "
        "}",
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
        "{ \"share_target\": { \"action\": \"https://foo.com/#\", \"params\": "
        "{ \"title\": \"mytitle\" } } "
        "}",
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
        "{ \"share_target\": { \"action\": \"https://foo.com/#\", \"method\": "
        "\"\", \"enctype\": \"application/x-www-form-urlencoded\", \"params\": "
        "{ \"title\": \"mytitle\" } } "
        "}",
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
        "{ \"share_target\": { \"action\": \"https://foo.com/#\", \"method\": "
        "\"GET\", \"enctype\": \"RANDOM\", \"params\": "
        "{ \"title\": \"mytitle\" } } "
        "}",
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
        "{ \"share_target\": { \"action\": \"https://foo.com/#\", \"method\": "
        "\"POST\", \"enctype\": \"random\", \"params\": "
        "{ \"title\": \"mytitle\" } } "
        "}",
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
        "{ \"share_target\": { \"action\": \"https://foo.com/#\", \"method\": "
        "\"POST\", \"enctype\": \"application/x-www-form-urlencoded\", "
        "\"params\": "
        "{ \"title\": \"mytitle\" } } "
        "}",
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
        "{ \"share_target\": { \"action\": \"https://foo.com/#\", \"method\": "
        "\"POST\", \"enctype\": \"multipart/form-data\", \"params\": "
        "{ \"title\": \"mytitle\" } } "
        "}",
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
        "{ \"share_target\": { \"action\": \"https://foo.com/#\", \"method\": "
        "\"PosT\", \"enctype\": \"mUltIparT/Form-dAta\", \"params\": "
        "{ \"title\": \"mytitle\" } } "
        "}",
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
        "{ \"share_target\": { \"action\": \"https://foo.com/#\", \"method\": "
        "\"POST\", \"enctype\": \"multipart/form-data\", \"params\": "
        "{ \"title\": \"mytitle\", \"files\": [] } } "
        "}",
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
        "{ \"share_target\": { \"action\": \"https://foo.com/#\", \"method\": "
        "\"GET\", \"enctype\": \"multipart/form-data\", \"params\": "
        "{ \"title\": \"mytitle\", \"files\": [{ \"name\": \"name\", "
        "\"accept\": [\"text/plain\"]}] } } "
        "}",
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
        "{ \"share_target\": { \"action\": \"https://foo.com/#\", \"method\": "
        "\"POST\", \"enctype\": \"application/x-www-form-urlencoded\", "
        "\"params\": "
        "{ \"title\": \"mytitle\", \"files\": [{ \"name\": \"name\", "
        "\"accept\": [\"text/plain\"]}] } } "
        "}",
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
        "{ \"share_target\": { \"action\": \"https://foo.com/#\", \"method\": "
        "\"POST\", \"enctype\": \"multipart/form-data\", \"params\": "
        "{ \"title\": \"mytitle\", \"files\": [{ \"name\": \"name\", "
        "\"accept\": [\"text/plain\"]}] } } "
        "}",
        manifest_url, document_url);
    EXPECT_TRUE(manifest->share_target.get());
    EXPECT_TRUE(manifest->share_target->params->files.has_value());
    EXPECT_EQ(1u, manifest->share_target->params->files->size());
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Invalid mimetype.
  {
    auto& manifest = ParseManifestWithURLs(
        "{ \"share_target\": { \"action\": \"https://foo.com/#\", \"method\": "
        "\"POST\", \"enctype\": \"multipart/form-data\", \"params\": "
        "{ \"title\": \"mytitle\", \"files\": [{ \"name\": \"name\", "
        "\"accept\": [\"\"]}] } } "
        "}",
        manifest_url, document_url);
    EXPECT_FALSE(manifest->share_target.get());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("invalid mime type inside files.", errors()[0]);
  }

  // Invalid mimetype.
  {
    auto& manifest = ParseManifestWithURLs(
        "{ \"share_target\": { \"action\": \"https://foo.com/#\", \"method\": "
        "\"POST\", \"enctype\": \"multipart/form-data\", \"params\": "
        "{ \"title\": \"mytitle\", \"files\": [{ \"name\": \"name\", "
        "\"accept\": [\"helloworld\"]}] } } "
        "}",
        manifest_url, document_url);
    EXPECT_FALSE(manifest->share_target.get());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("invalid mime type inside files.", errors()[0]);
  }

  // Invalid mimetype.
  {
    auto& manifest = ParseManifestWithURLs(
        "{ \"share_target\": { \"action\": \"https://foo.com/#\", \"method\": "
        "\"POST\", \"enctype\": \"multipart/form-data\", \"params\": "
        "{ \"title\": \"mytitle\", \"files\": [{ \"name\": \"name\", "
        "\"accept\": [\"^$/@$\"]}] } } "
        "}",
        manifest_url, document_url);
    EXPECT_FALSE(manifest->share_target.get());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("invalid mime type inside files.", errors()[0]);
  }

  // Invalid mimetype.
  {
    auto& manifest = ParseManifestWithURLs(
        "{ \"share_target\": { \"action\": \"https://foo.com/#\", \"method\": "
        "\"POST\", \"enctype\": \"multipart/form-data\", \"params\": "
        "{ \"title\": \"mytitle\", \"files\": [{ \"name\": \"name\", "
        "\"accept\": [\"/\"]}] } } "
        "}",
        manifest_url, document_url);
    EXPECT_FALSE(manifest->share_target.get());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("invalid mime type inside files.", errors()[0]);
  }

  // Invalid mimetype.
  {
    auto& manifest = ParseManifestWithURLs(
        "{ \"share_target\": { \"action\": \"https://foo.com/#\", \"method\": "
        "\"POST\", \"enctype\": \"multipart/form-data\", \"params\": "
        "{ \"title\": \"mytitle\", \"files\": [{ \"name\": \"name\", "
        "\"accept\": [\" \"]}] } } "
        "}",
        manifest_url, document_url);
    EXPECT_FALSE(manifest->share_target.get());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("invalid mime type inside files.", errors()[0]);
  }

  // Accept field is empty.
  {
    auto& manifest = ParseManifestWithURLs(
        "{ \"share_target\": { \"action\": \"https://foo.com/#\", \"method\": "
        "\"POST\", \"enctype\": \"multipart/form-data\", \"params\": "
        "{ \"title\": \"mytitle\", \"files\": [{ \"name\": \"name\", "
        "\"accept\": []}] } } "
        "}",
        manifest_url, document_url);
    EXPECT_TRUE(manifest->share_target.get());
    EXPECT_FALSE(manifest->share_target->params->files.has_value());
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept sequence contains non-string elements.
  {
    auto& manifest = ParseManifestWithURLs(
        "{"
        "  \"share_target\": {"
        "    \"action\": \"https://foo.com/#\","
        "    \"method\": \"POST\","
        "    \"enctype\": \"multipart/form-data\","
        "    \"params\": {"
        "      \"title\": \"mytitle\","
        "      \"files\": [{"
        "        \"name\": \"name\","
        "        \"accept\": [\"image/png\", 42]"
        "      }]"
        "    }"
        "  }"
        "}",
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
        "{"
        "  \"share_target\": {"
        "    \"action\": \"https://foo.com/#\","
        "    \"method\": \"POST\","
        "    \"enctype\": \"multipart/form-data\","
        "    \"params\": {"
        "      \"title\": \"mytitle\","
        "      \"files\": [{"
        "        \"name\": \"name\","
        "        \"accept\": \"image/png\""
        "      }]"
        "    }"
        "  }"
        "}",
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
        "{"
        "  \"share_target\": {"
        "    \"action\": \"https://foo.com/#\","
        "    \"method\": \"POST\","
        "    \"enctype\": \"multipart/form-data\","
        "    \"params\": {"
        "      \"title\": \"mytitle\","
        "      \"files\": [{"
        "        \"name\": \"name\","
        "        \"accept\": true"
        "      }]"
        "    }"
        "  }"
        "}",
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
        "{"
        "  \"share_target\": {"
        "    \"action\": \"https://foo.com/#\","
        "    \"method\": \"POST\","
        "    \"enctype\": \"multipart/form-data\","
        "    \"params\": {"
        "      \"title\": \"mytitle\","
        "      \"files\": {"
        "        \"name\": \"name\","
        "        \"accept\": \"image/png\""
        "      }"
        "    }"
        "  }"
        "}",
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
        "{"
        "  \"share_target\": {"
        "    \"action\": \"https://foo.com/#\","
        "    \"method\": \"POST\","
        "    \"enctype\": \"multipart/form-data\","
        "    \"params\": {"
        "      \"title\": \"mytitle\","
        "      \"files\": 3"
        "    }"
        "  }"
        "}",
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
        "{"
        "  \"share_target\": {"
        "    \"action\": \"https://foo.com/#\","
        "    \"method\": \"POST\","
        "    \"enctype\": \"multipart/form-data\","
        "    \"params\": {"
        "      \"title\": \"mytitle\","
        "      \"files\": ["
        "        {"
        "          \"name\": \"name\","
        "          \"accept\": \"image/png\""
        "        },"
        "        3"
        "      ]"
        "    }"
        "  }"
        "}",
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
        "{"
        "  \"share_target\": {"
        "    \"action\": \"https://foo.com/#\","
        "    \"method\": \"POST\","
        "    \"enctype\": \"multipart/form-data\","
        "    \"params\": {"
        "      \"title\": \"mytitle\","
        "      \"files\": ["
        "        {"
        "          \"name\": \"name\","
        "          \"accept\": \"image/png\""
        "        },"
        "        {}"
        "      ]"
        "    }"
        "  }"
        "}",
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
    auto& manifest = ParseManifest("{ \"related_applications\": []}");
    EXPECT_TRUE(manifest->related_applications.IsEmpty());
    EXPECT_EQ(0u, GetErrorCount());
  }

  // If empty application, empty list.
  {
    auto& manifest = ParseManifest("{ \"related_applications\": [{}]}");
    EXPECT_TRUE(manifest->related_applications.IsEmpty());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("'platform' is a required field, related application ignored.",
              errors()[0]);
  }

  // If invalid platform, application is ignored.
  {
    auto& manifest =
        ParseManifest("{ \"related_applications\": [{\"platform\": 123}]}");
    EXPECT_TRUE(manifest->related_applications.IsEmpty());
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
        ParseManifest("{ \"related_applications\": [{\"id\": \"foo\"}]}");
    EXPECT_TRUE(manifest->related_applications.IsEmpty());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("'platform' is a required field, related application ignored.",
              errors()[0]);
  }

  // If missing id and url, application is ignored.
  {
    auto& manifest = ParseManifest(
        "{ \"related_applications\": [{\"platform\": \"play\"}]}");
    EXPECT_TRUE(manifest->related_applications.IsEmpty());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("one of 'url' or 'id' is required, related application ignored.",
              errors()[0]);
  }

  // Valid application, with url.
  {
    auto& manifest = ParseManifest(
        "{ \"related_applications\": ["
        "{\"platform\": \"play\", \"url\": \"http://www.foo.com\"}]}");
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
    auto& manifest = ParseManifest(
        "{ \"related_applications\": ["
        "{\"platform\": \"play\", \"url\": \"http://www.foo.com:co&uk\"}]}");
    EXPECT_TRUE(manifest->related_applications.IsEmpty());
    EXPECT_EQ(2u, GetErrorCount());
    EXPECT_EQ("property 'url' ignored, URL is invalid.", errors()[0]);
    EXPECT_EQ("one of 'url' or 'id' is required, related application ignored.",
              errors()[1]);
  }

  // Valid application, with id.
  {
    auto& manifest = ParseManifest(
        "{ \"related_applications\": ["
        "{\"platform\": \"itunes\", \"id\": \"foo\"}]}");
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
        "{ \"related_applications\": ["
        "{\"platform\": \"play\", \"id\": \"foo\"},"
        "{\"platform\": \"itunes\", \"id\": \"bar\"}]}");
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
        "{ \"related_applications\": ["
        "{\"platform\": \"itunes\"},"
        "{\"platform\": \"play\", \"id\": \"foo\"},"
        "{}]}");
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
    auto& manifest = ParseManifest("{ \"prefer_related_applications\": true }");
    EXPECT_TRUE(manifest->prefer_related_applications);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Don't parse if the property isn't a boolean.
  {
    auto& manifest = ParseManifest("{ \"prefer_related_applications\": {} }");
    EXPECT_FALSE(manifest->prefer_related_applications);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'prefer_related_applications' "
        "ignored, type boolean expected.",
        errors()[0]);
  }
  {
    auto& manifest =
        ParseManifest("{ \"prefer_related_applications\": \"true\" }");
    EXPECT_FALSE(manifest->prefer_related_applications);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'prefer_related_applications' "
        "ignored, type boolean expected.",
        errors()[0]);
  }
  {
    auto& manifest = ParseManifest("{ \"prefer_related_applications\": 1 }");
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
        ParseManifest("{ \"prefer_related_applications\": false }");
    EXPECT_FALSE(manifest->prefer_related_applications);
    EXPECT_EQ(0u, GetErrorCount());
  }
}

TEST_F(ManifestParserTest, ThemeColorParserRules) {
  // Smoke test.
  {
    auto& manifest = ParseManifest("{ \"theme_color\": \"#FF0000\" }");
    EXPECT_TRUE(manifest->has_theme_color);
    EXPECT_EQ(manifest->theme_color, 0xFFFF0000u);
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Trim whitespaces.
  {
    auto& manifest = ParseManifest("{ \"theme_color\": \"  blue   \" }");
    EXPECT_TRUE(manifest->has_theme_color);
    EXPECT_EQ(manifest->theme_color, 0xFF0000FFu);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Don't parse if theme_color isn't a string.
  {
    auto& manifest = ParseManifest("{ \"theme_color\": {} }");
    EXPECT_FALSE(manifest->has_theme_color);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'theme_color' ignored, type string expected.",
              errors()[0]);
  }

  // Don't parse if theme_color isn't a string.
  {
    auto& manifest = ParseManifest("{ \"theme_color\": false }");
    EXPECT_FALSE(manifest->has_theme_color);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'theme_color' ignored, type string expected.",
              errors()[0]);
  }

  // Don't parse if theme_color isn't a string.
  {
    auto& manifest = ParseManifest("{ \"theme_color\": null }");
    EXPECT_FALSE(manifest->has_theme_color);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'theme_color' ignored, type string expected.",
              errors()[0]);
  }

  // Don't parse if theme_color isn't a string.
  {
    auto& manifest = ParseManifest("{ \"theme_color\": [] }");
    EXPECT_FALSE(manifest->has_theme_color);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'theme_color' ignored, type string expected.",
              errors()[0]);
  }

  // Don't parse if theme_color isn't a string.
  {
    auto& manifest = ParseManifest("{ \"theme_color\": 42 }");
    EXPECT_FALSE(manifest->has_theme_color);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'theme_color' ignored, type string expected.",
              errors()[0]);
  }

  // Parse fails if string is not in a known format.
  {
    auto& manifest = ParseManifest("{ \"theme_color\": \"foo(bar)\" }");
    EXPECT_FALSE(manifest->has_theme_color);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'theme_color' ignored,"
        " 'foo(bar)' is not a valid color.",
        errors()[0]);
  }

  // Parse fails if string is not in a known format.
  {
    auto& manifest = ParseManifest("{ \"theme_color\": \"bleu\" }");
    EXPECT_FALSE(manifest->has_theme_color);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'theme_color' ignored, 'bleu' is not a valid color.",
              errors()[0]);
  }

  // Parse fails if string is not in a known format.
  {
    auto& manifest = ParseManifest("{ \"theme_color\": \"FF00FF\" }");
    EXPECT_FALSE(manifest->has_theme_color);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'theme_color' ignored, 'FF00FF'"
        " is not a valid color.",
        errors()[0]);
  }

  // Parse fails if multiple values for theme_color are given.
  {
    auto& manifest = ParseManifest("{ \"theme_color\": \"#ABC #DEF\" }");
    EXPECT_FALSE(manifest->has_theme_color);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'theme_color' ignored, "
        "'#ABC #DEF' is not a valid color.",
        errors()[0]);
  }

  // Parse fails if multiple values for theme_color are given.
  {
    auto& manifest = ParseManifest("{ \"theme_color\": \"#AABBCC #DDEEFF\" }");
    EXPECT_FALSE(manifest->has_theme_color);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'theme_color' ignored, "
        "'#AABBCC #DDEEFF' is not a valid color.",
        errors()[0]);
  }

  // Accept CSS color keyword format.
  {
    auto& manifest = ParseManifest("{ \"theme_color\": \"blue\" }");
    EXPECT_EQ(manifest->theme_color, 0xFF0000FFu);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept CSS color keyword format.
  {
    auto& manifest = ParseManifest("{ \"theme_color\": \"chartreuse\" }");
    EXPECT_EQ(manifest->theme_color, 0xFF7FFF00u);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept CSS RGB format.
  {
    auto& manifest = ParseManifest("{ \"theme_color\": \"#FFF\" }");
    EXPECT_EQ(manifest->theme_color, 0xFFFFFFFFu);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept CSS RGB format.
  {
    auto& manifest = ParseManifest("{ \"theme_color\": \"#ABC\" }");
    EXPECT_EQ(manifest->theme_color, 0xFFAABBCCu);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept CSS RRGGBB format.
  {
    auto& manifest = ParseManifest("{ \"theme_color\": \"#FF0000\" }");
    EXPECT_EQ(manifest->theme_color, 0xFFFF0000u);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept translucent colors.
  {
    auto& manifest = ParseManifest(
        "{ \"theme_color\": \"rgba(255,0,0,"
        "0.4)\" }");
    EXPECT_EQ(manifest->theme_color, 0x66FF0000u);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept transparent colors.
  {
    auto& manifest = ParseManifest("{ \"theme_color\": \"rgba(0,0,0,0)\" }");
    EXPECT_EQ(manifest->theme_color, 0x00000000u);
    EXPECT_EQ(0u, GetErrorCount());
  }
}

TEST_F(ManifestParserTest, BackgroundColorParserRules) {
  // Smoke test.
  {
    auto& manifest = ParseManifest("{ \"background_color\": \"#FF0000\" }");
    EXPECT_EQ(manifest->background_color, 0xFFFF0000u);
    EXPECT_FALSE(IsManifestEmpty(manifest));
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Trim whitespaces.
  {
    auto& manifest = ParseManifest("{ \"background_color\": \"  blue   \" }");
    EXPECT_EQ(manifest->background_color, 0xFF0000FFu);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Don't parse if background_color isn't a string.
  {
    auto& manifest = ParseManifest("{ \"background_color\": {} }");
    EXPECT_FALSE(manifest->has_background_color);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'background_color' ignored, type string expected.",
              errors()[0]);
  }

  // Don't parse if background_color isn't a string.
  {
    auto& manifest = ParseManifest("{ \"background_color\": false }");
    EXPECT_FALSE(manifest->has_background_color);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'background_color' ignored, type string expected.",
              errors()[0]);
  }

  // Don't parse if background_color isn't a string.
  {
    auto& manifest = ParseManifest("{ \"background_color\": null }");
    EXPECT_FALSE(manifest->has_background_color);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'background_color' ignored, type string expected.",
              errors()[0]);
  }

  // Don't parse if background_color isn't a string.
  {
    auto& manifest = ParseManifest("{ \"background_color\": [] }");
    EXPECT_FALSE(manifest->has_background_color);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'background_color' ignored, type string expected.",
              errors()[0]);
  }

  // Don't parse if background_color isn't a string.
  {
    auto& manifest = ParseManifest("{ \"background_color\": 42 }");
    EXPECT_FALSE(manifest->has_background_color);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'background_color' ignored, type string expected.",
              errors()[0]);
  }

  // Parse fails if string is not in a known format.
  {
    auto& manifest = ParseManifest("{ \"background_color\": \"foo(bar)\" }");
    EXPECT_FALSE(manifest->has_background_color);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'background_color' ignored,"
        " 'foo(bar)' is not a valid color.",
        errors()[0]);
  }

  // Parse fails if string is not in a known format.
  {
    auto& manifest = ParseManifest("{ \"background_color\": \"bleu\" }");
    EXPECT_FALSE(manifest->has_background_color);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'background_color' ignored,"
        " 'bleu' is not a valid color.",
        errors()[0]);
  }

  // Parse fails if string is not in a known format.
  {
    auto& manifest = ParseManifest("{ \"background_color\": \"FF00FF\" }");
    EXPECT_FALSE(manifest->has_background_color);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'background_color' ignored,"
        " 'FF00FF' is not a valid color.",
        errors()[0]);
  }

  // Parse fails if multiple values for background_color are given.
  {
    auto& manifest = ParseManifest("{ \"background_color\": \"#ABC #DEF\" }");
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
        ParseManifest("{ \"background_color\": \"#AABBCC #DDEEFF\" }");
    EXPECT_FALSE(manifest->has_background_color);
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ(
        "property 'background_color' ignored, "
        "'#AABBCC #DDEEFF' is not a valid color.",
        errors()[0]);
  }

  // Accept CSS color keyword format.
  {
    auto& manifest = ParseManifest("{ \"background_color\": \"blue\" }");
    EXPECT_EQ(manifest->background_color, 0xFF0000FFu);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept CSS color keyword format.
  {
    auto& manifest = ParseManifest("{ \"background_color\": \"chartreuse\" }");
    EXPECT_EQ(manifest->background_color, 0xFF7FFF00u);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept CSS RGB format.
  {
    auto& manifest = ParseManifest("{ \"background_color\": \"#FFF\" }");
    EXPECT_EQ(manifest->background_color, 0xFFFFFFFFu);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept CSS RGB format.
  {
    auto& manifest = ParseManifest("{ \"background_color\": \"#ABC\" }");
    EXPECT_EQ(manifest->background_color, 0xFFAABBCCu);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept CSS RRGGBB format.
  {
    auto& manifest = ParseManifest("{ \"background_color\": \"#FF0000\" }");
    EXPECT_EQ(manifest->background_color, 0xFFFF0000u);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept translucent colors.
  {
    auto& manifest = ParseManifest(
        "{ \"background_color\": \"rgba(255,0,0,"
        "0.4)\" }");
    EXPECT_EQ(manifest->background_color, 0x66FF0000u);
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Accept transparent colors.
  {
    auto& manifest = ParseManifest(
        "{ \"background_color\": \"rgba(0,0,0,"
        "0)\" }");
    EXPECT_EQ(manifest->background_color, 0x00000000u);
    EXPECT_EQ(0u, GetErrorCount());
  }
}

TEST_F(ManifestParserTest, GCMSenderIDParseRules) {
  // Smoke test.
  {
    auto& manifest = ParseManifest("{ \"gcm_sender_id\": \"foo\" }");
    EXPECT_EQ(manifest->gcm_sender_id, "foo");
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Trim whitespaces.
  {
    auto& manifest = ParseManifest("{ \"gcm_sender_id\": \"  foo  \" }");
    EXPECT_EQ(manifest->gcm_sender_id, "foo");
    EXPECT_EQ(0u, GetErrorCount());
  }

  // Don't parse if the property isn't a string.
  {
    auto& manifest = ParseManifest("{ \"gcm_sender_id\": {} }");
    EXPECT_TRUE(manifest->gcm_sender_id.IsNull());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'gcm_sender_id' ignored, type string expected.",
              errors()[0]);
  }
  {
    auto& manifest = ParseManifest("{ \"gcm_sender_id\": 42 }");
    EXPECT_TRUE(manifest->gcm_sender_id.IsNull());
    EXPECT_EQ(1u, GetErrorCount());
    EXPECT_EQ("property 'gcm_sender_id' ignored, type string expected.",
              errors()[0]);
  }
}

}  // namespace blink
