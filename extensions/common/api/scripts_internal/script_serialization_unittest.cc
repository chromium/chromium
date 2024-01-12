// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/api/scripts_internal/script_serialization.h"

#include <string_view>

#include "base/test/values_test_util.h"
#include "extensions/common/api/scripts_internal.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions::script_serialization {

using api::scripts_internal::SerializedUserScript;

namespace {

// Returns the SerializedUserScript object from the given `json` blob.
// Note: this will crash if `json` doesn't parse to a valid script.
SerializedUserScript SerializedScriptFromJson(std::string_view json) {
  base::Value value = base::test::ParseJson(json);
  std::optional<SerializedUserScript> serialized_script =
      SerializedUserScript::FromValue(value);
  return std::move(*serialized_script);
}

}  // namespace

// Tests parsing a minimally-specified serialized user script. This ensures all
// defaults are properly set.
TEST(ScriptSerializationUnitTest, ParseMinimalScript) {
  // A set of the minimal required properties, according to
  // scripts_internal.idl.
  constexpr char kMinimalScriptJson[] =
      R"({
           "id": "_dc_minimal_script",
           "js": [{"file": "script.js"}],
           "matches": ["http://matches.example/*"],
           "source": "DYNAMIC_CONTENT_SCRIPT",
           "world": "ISOLATED"
         })";

  SerializedUserScript serialized_script =
      SerializedScriptFromJson(kMinimalScriptJson);

  auto stub_extension = ExtensionBuilder("foo").Build();
  std::unique_ptr<UserScript> script = ParseSerializedUserScript(
      serialized_script, *stub_extension, /*allowed_in_incognito=*/false);

  ASSERT_TRUE(script);

  EXPECT_FALSE(script->match_all_frames());
  EXPECT_EQ(0u, script->css_scripts().size());
  EXPECT_EQ(0u, script->exclude_url_patterns().size());
  EXPECT_EQ(0u, script->exclude_globs().size());
  EXPECT_EQ("_dc_minimal_script", script->id());
  EXPECT_EQ(0u, script->globs().size());
  ASSERT_EQ(1u, script->js_scripts().size());
  EXPECT_EQ("script.js",
            script->js_scripts()[0]->relative_path().AsUTF8Unsafe());
  EXPECT_EQ(UserScript::Content::Source::kFile,
            script->js_scripts()[0]->source());
  EXPECT_THAT(script->url_patterns().ToStringVector(),
              testing::ElementsAre("http://matches.example/*"));
  EXPECT_EQ(MatchOriginAsFallbackBehavior::kNever,
            script->match_origin_as_fallback());
  EXPECT_EQ(mojom::RunLocation::kDocumentIdle, script->run_location());
  EXPECT_EQ(UserScript::Source::kDynamicContentScript, script->GetSource());
  EXPECT_EQ(mojom::ExecutionWorld::kIsolated, script->execution_world());
}

// Tests parsing a maximally-specified serialized user script. This ensures each
// field is properly deserialized and curried to the outcoming UserScript.
TEST(ScriptSerializationUnitTest, ParseMaximalScript) {
  // A set of all possible properties.
  // Note we explicitly choose non-default options to ensure they are properly
  // read.
  constexpr char kMaximalScriptJson[] =
      R"({
           "allFrames": true,
           "css": [{"file": "style.css"}],
           "excludeMatches": ["http://exclude.example/*"],
           "excludeGlobs": ["*exclude_glob*"],
           "id": "_dc_maximal_script",
           "includeGlobs": ["*include_glob*"],
           "js": [{"file": "script.js"}],
           "matches": ["http://matches.example/*"],
           "matchOriginAsFallback": true,
           "runAt": "document_start",
           "source": "DYNAMIC_CONTENT_SCRIPT",
           "world": "MAIN"
         })";

  SerializedUserScript serialized_script =
      SerializedScriptFromJson(kMaximalScriptJson);

  auto stub_extension = ExtensionBuilder("foo").Build();
  std::unique_ptr<UserScript> script = ParseSerializedUserScript(
      serialized_script, *stub_extension, /*allowed_in_incognito=*/false);

  ASSERT_TRUE(script);

  EXPECT_TRUE(script->match_all_frames());
  ASSERT_EQ(1u, script->css_scripts().size());
  EXPECT_EQ("style.css",
            script->css_scripts()[0]->relative_path().AsUTF8Unsafe());
  EXPECT_EQ(UserScript::Content::Source::kFile,
            script->css_scripts()[0]->source());
  EXPECT_THAT(script->exclude_url_patterns().ToStringVector(),
              testing::ElementsAre("http://exclude.example/*"));
  EXPECT_THAT(script->exclude_globs(), testing::ElementsAre("*exclude_glob*"));
  EXPECT_EQ("_dc_maximal_script", script->id());
  EXPECT_THAT(script->globs(), testing::ElementsAre("*include_glob*"));
  ASSERT_EQ(1u, script->js_scripts().size());
  EXPECT_EQ("script.js",
            script->js_scripts()[0]->relative_path().AsUTF8Unsafe());
  EXPECT_EQ(UserScript::Content::Source::kFile,
            script->js_scripts()[0]->source());
  EXPECT_THAT(script->url_patterns().ToStringVector(),
              testing::ElementsAre("http://matches.example/*"));
  EXPECT_EQ(MatchOriginAsFallbackBehavior::kAlways,
            script->match_origin_as_fallback());
  EXPECT_EQ(mojom::RunLocation::kDocumentStart, script->run_location());
  EXPECT_EQ(UserScript::Source::kDynamicContentScript, script->GetSource());
  EXPECT_EQ(mojom::ExecutionWorld::kMain, script->execution_world());
}

// Tests serializing a UserScript object to a SerializedUserScript.
TEST(ScriptSerializationUnitTest, SerializeUserScript) {
  auto stub_extension = ExtensionBuilder("foo").Build();
  const int valid_schemes = UserScript::ValidUserScriptSchemes();

  UserScript script;
  script.set_host_id(mojom::HostID(mojom::HostID::HostType::kExtensions,
                                   stub_extension->id()));

  script.set_match_all_frames(true);
  script.css_scripts().push_back(UserScript::Content::CreateFile(
      base::FilePath(), base::FilePath(FILE_PATH_LITERAL("style.css")),
      stub_extension->GetResourceURL("style.css")));
  script.add_exclude_url_pattern(
      URLPattern(valid_schemes, "http://exclude.example/*"));
  script.add_exclude_glob("*exclude_glob*");
  script.set_id("_dc_user_script");
  script.add_glob("*include_glob*");
  script.js_scripts().push_back(UserScript::Content::CreateFile(
      base::FilePath(), base::FilePath(FILE_PATH_LITERAL("script.js")),
      stub_extension->GetResourceURL("script.js")));
  script.add_url_pattern(URLPattern(valid_schemes, "http://matches.example/*"));
  script.set_match_origin_as_fallback(MatchOriginAsFallbackBehavior::kAlways);
  script.set_run_location(mojom::RunLocation::kDocumentStart);
  script.set_execution_world(mojom::ExecutionWorld::kMain);

  SerializedUserScript serialized = SerializeUserScript(script);
  constexpr char kExpectedJson[] =
      R"({
           "allFrames": true,
           "css": [{"file": "style.css"}],
           "excludeMatches": ["http://exclude.example/*"],
           "excludeGlobs": ["*exclude_glob*"],
           "id": "_dc_user_script",
           "includeGlobs": ["*include_glob*"],
           "js": [{"file": "script.js"}],
           "matches": ["http://matches.example/*"],
           "matchOriginAsFallback": true,
           "runAt": "document_start",
           "source": "DYNAMIC_CONTENT_SCRIPT",
           "world": "MAIN"
         })";
  EXPECT_THAT(serialized.ToValue(), base::test::IsJson(kExpectedJson));
}

// Tests that scripts cannot specify `"match_origin_as_fallback": true` if
// they include match patterns with paths.
TEST(ScriptSerializationUnitTest, DisallowMatchOriginAsFallbackWithPaths) {
  static constexpr char kScriptJson[] =
      R"({
           "id": "_dc_script",
           "js": [{"file": "script.js"}],
           "matches": ["http://matches.example/path"],
           "matchOriginAsFallback": true,
           "source": "DYNAMIC_CONTENT_SCRIPT",
           "world": "ISOLATED"
         })";

  SerializedUserScript serialized_script =
      SerializedScriptFromJson(kScriptJson);

  std::u16string error;
  auto stub_extension = ExtensionBuilder("foo").Build();
  std::unique_ptr<UserScript> script =
      ParseSerializedUserScript(serialized_script, *stub_extension,
                                /*allowed_in_incognito=*/false, &error);

  EXPECT_FALSE(script);
  EXPECT_EQ(error, manifest_errors::kMatchOriginAsFallbackCantHavePaths);
}

}  // namespace extensions::script_serialization
