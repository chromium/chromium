// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/api/scripts_internal/script_serialization.h"

#include <optional>

#include "base/types/optional_util.h"
#include "extensions/common/api/scripts_internal.h"
#include "extensions/common/user_script.h"
#include "extensions/common/utils/content_script_utils.h"
#include "extensions/common/utils/extension_types_utils.h"

namespace extensions::script_serialization {

std::vector<api::scripts_internal::ScriptSource> GetSourcesFromFileNames(
    std::vector<std::string> file_names) {
  std::vector<api::scripts_internal::ScriptSource> script_sources;
  script_sources.reserve(file_names.size());
  for (auto& file : file_names) {
    api::scripts_internal::ScriptSource script_source;
    script_source.file = std::move(file);
    script_sources.push_back(std::move(script_source));
  }
  return script_sources;
}

api::scripts_internal::SerializedUserScript SerializeUserScript(
    const UserScript& user_script) {
  api::scripts_internal::SerializedUserScript serialized_script;

  // `allFrames`.
  serialized_script.all_frames = user_script.match_all_frames();

  // `css`.
  if (!user_script.css_scripts().empty()) {
    serialized_script.css.emplace();
    serialized_script.css->reserve(user_script.css_scripts().size());
    for (const auto& css_script : user_script.css_scripts()) {
      // TODO(crbug.com/40061759): Handle `code`.
      api::scripts_internal::ScriptSource source;
      source.file = css_script->relative_path().AsUTF8Unsafe();
      serialized_script.css->push_back(std::move(source));
    }
  }

  // `excludeMatches`.
  if (!user_script.exclude_url_patterns().is_empty()) {
    serialized_script.exclude_matches.emplace();
    serialized_script.exclude_matches->reserve(
        user_script.exclude_url_patterns().size());
    for (const URLPattern& pattern : user_script.exclude_url_patterns()) {
      serialized_script.exclude_matches->push_back(pattern.GetAsString());
    }
  }

  // `excludeGlobs`.
  if (!user_script.exclude_globs().empty()) {
    serialized_script.exclude_globs.emplace();
    serialized_script.exclude_globs->reserve(
        user_script.exclude_globs().size());
    for (const std::string& exclude_glob : user_script.exclude_globs()) {
      serialized_script.exclude_globs->push_back(exclude_glob);
    }
  }

  // `id`.
  serialized_script.id = user_script.id();

  // `includeGlobs`.
  if (!user_script.globs().empty()) {
    serialized_script.include_globs.emplace();
    serialized_script.include_globs->reserve(user_script.globs().size());
    for (const std::string& glob : user_script.globs()) {
      serialized_script.include_globs->push_back(glob);
    }
  }

  // `js`.
  if (!user_script.js_scripts().empty()) {
    serialized_script.js.emplace();
    serialized_script.js->reserve(user_script.js_scripts().size());
    for (const auto& js_script : user_script.js_scripts()) {
      api::scripts_internal::ScriptSource source;
      switch (js_script->source()) {
        case UserScript::Content::Source::kFile:
          source.file = js_script->relative_path().AsUTF8Unsafe();
          break;
        case UserScript::Content::Source::kInlineCode:
          // Inline code is only serialized for user scripts.
          CHECK_EQ(user_script.GetSource(),
                   UserScript::Source::kDynamicUserScript);
          source.code = js_script->GetContent();
          break;
      }

      serialized_script.js->push_back(std::move(source));
    }
  }

  // `matches`.
  serialized_script.matches.reserve(user_script.url_patterns().size());
  for (const URLPattern& pattern : user_script.url_patterns()) {
    serialized_script.matches.push_back(pattern.GetAsString());
  }

  // `matchOriginAsFallback`.
  serialized_script.match_origin_as_fallback =
      user_script.match_origin_as_fallback() ==
      MatchOriginAsFallbackBehavior::kAlways;

  // `runAt`.
  serialized_script.run_at =
      ConvertRunLocationForAPI(user_script.run_location());

  // `source`.
  auto source_to_serialized_source = [](UserScript::Source source) {
    switch (source) {
      case UserScript::Source::kDynamicContentScript:
        return api::scripts_internal::Source::kDynamicContentScript;
      case UserScript::Source::kDynamicUserScript:
        return api::scripts_internal::Source::kDynamicUserScript;
      case UserScript::Source::kStaticContentScript:
      case UserScript::Source::kWebUIScript:
        // We shouldn't be serialized these script types, ever.
        NOTREACHED();
    }
  };
  serialized_script.source =
      source_to_serialized_source(user_script.GetSource());

  // `world`.
  serialized_script.world =
      ConvertExecutionWorldForAPI(user_script.execution_world());

  // `worldId`.
  serialized_script.world_id = user_script.world_id();

  return serialized_script;
}

std::unique_ptr<UserScript> ParseSerializedUserScript(
    const api::scripts_internal::SerializedUserScript& serialized_script,
    const Extension& extension,
    bool allowed_in_incognito,
    std::u16string* error_out,
    bool* wants_file_access_out,
    SerializedUserScriptParseOptions parse_options) {
  bool source_matches_id = true;
  switch (serialized_script.source) {
    case api::scripts_internal::Source::kDynamicContentScript:
      source_matches_id = base::StartsWith(
          serialized_script.id, UserScript::kDynamicContentScriptPrefix);
      break;
    case api::scripts_internal::Source::kDynamicUserScript:
      source_matches_id = base::StartsWith(
          serialized_script.id, UserScript::kDynamicUserScriptPrefix);
      break;
    case api::scripts_internal::Source::kManifestContentScript:
      source_matches_id = base::StartsWith(
          serialized_script.id, UserScript::kManifestContentScriptPrefix);
      break;
    case api::scripts_internal::Source::kNone:
      NOTREACHED_IN_MIGRATION();  // This should have been caught by our
                                  // parsing.
  }

  if (!source_matches_id) {
    return nullptr;
  }

  auto user_script = std::make_unique<UserScript>();

  user_script->set_host_id(
      mojom::HostID(mojom::HostID::HostType::kExtensions, extension.id()));
  // Note: `id` must be set here since error messages from the helper methods
  // use the script ID in their output.
  user_script->set_id(serialized_script.id);

  std::u16string error;
  if (!error_out) {
    error_out = &error;
  }

  // `allFrames`.
  if (serialized_script.all_frames) {
    user_script->set_match_all_frames(*serialized_script.all_frames);
  }
  // `css`/`js`.
  if (!script_parsing::ParseFileSources(
          &extension, base::OptionalToPtr(serialized_script.js),
          base::OptionalToPtr(serialized_script.css),
          parse_options.index_for_error, user_script.get(), error_out)) {
    return nullptr;
  }
  // `excludeMatches`/`matches`.
  if (!script_parsing::ParseMatchPatterns(
          serialized_script.matches,
          base::OptionalToPtr(serialized_script.exclude_matches),
          extension.creation_flags(),
          parse_options.can_execute_script_everywhere,
          parse_options.all_urls_includes_chrome_urls,
          parse_options.index_for_error, user_script.get(), error_out,
          wants_file_access_out)) {
    return nullptr;
  }
  // `excludeGlobs`/`includeGlobs`.
  script_parsing::ParseGlobs(
      base::OptionalToPtr(serialized_script.include_globs),
      base::OptionalToPtr(serialized_script.exclude_globs), user_script.get());
  // Note: `id` was handled above.
  // `matchOriginsAsFallback`.
  if (serialized_script.match_origin_as_fallback.has_value()) {
    user_script->set_match_origin_as_fallback(
        *serialized_script.match_origin_as_fallback
            ? MatchOriginAsFallbackBehavior::kAlways
            : MatchOriginAsFallbackBehavior::kNever);
  }
  // `runAt`.
  user_script->set_run_location(ConvertRunLocation(serialized_script.run_at));
  // Note: `source` is implicitly handled through setting the ID.

  // `world`.
  user_script->set_execution_world(
      ConvertExecutionWorld(serialized_script.world));

  // `worldId`.
  user_script->set_world_id(serialized_script.world_id);

  // Post-parse validation (these rely on multiple fields).
  if (!script_parsing::ValidateMatchOriginAsFallback(
          user_script->match_origin_as_fallback(), user_script->url_patterns(),
          error_out)) {
    return nullptr;
  }

  user_script->set_incognito_enabled(allowed_in_incognito);

  return user_script;
}

}  // namespace extensions::script_serialization
