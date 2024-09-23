// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/content_scripts_handler.h"

#include <stddef.h>

#include <memory>

#include "base/lazy_instance.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/optional_util.h"
#include "extensions/common/api/content_scripts.h"
#include "extensions/common/api/extension_types.h"
#include "extensions/common/api/scripts_internal.h"
#include "extensions/common/api/scripts_internal/script_serialization.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/mojom/host_id.mojom.h"
#include "extensions/common/mojom/run_location.mojom-shared.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/script_constants.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/url_pattern_set.h"
#include "extensions/common/utils/content_script_utils.h"
#include "extensions/common/utils/extension_types_utils.h"
#include "url/gurl.h"

namespace extensions {

namespace errors = manifest_errors;
namespace content_scripts_api = api::content_scripts;
using ContentScriptsKeys = content_scripts_api::ManifestKeys;

namespace {

// Helper method that converts a parsed ContentScript object into a UserScript
// object.
std::unique_ptr<UserScript> CreateUserScript(
    content_scripts_api::ContentScript content_script,
    int definition_index,
    bool can_execute_script_everywhere,
    bool all_urls_includes_chrome_urls,
    Extension* extension,
    std::u16string* error) {
  // We first convert to a `SerializedScript` to then convert that to a
  // `UserScript` through shared logic. We need a bit of custom handling for
  // match_origin_as_fallback, since manifest content scripts support
  // "match_about_blank".
  api::scripts_internal::SerializedUserScript serialized_script;
  serialized_script.source =
      api::scripts_internal::Source::kManifestContentScript;
  serialized_script.id = UserScript::GenerateUserScriptID();

  serialized_script.matches = std::move(content_script.matches);
  serialized_script.exclude_matches = std::move(content_script.exclude_matches);
  if (content_script.css) {
    serialized_script.css = script_serialization::GetSourcesFromFileNames(
        std::move(*content_script.css));
  }
  if (content_script.js) {
    serialized_script.js = script_serialization::GetSourcesFromFileNames(
        std::move(*content_script.js));
  }
  serialized_script.all_frames = content_script.all_frames;

  // match_origin_as_fallback and match_about_blank.
  // Note: `match_about_blank` is ignored if `match_origin_as_fallback` was
  // specified.
  if (content_script.match_origin_as_fallback) {
    serialized_script.match_origin_as_fallback =
        content_script.match_origin_as_fallback;
  }
  // Manifest content scripts support `match_about_blank` (unlike
  // `SerializedUserScript`). If `match_about_blank` is specified, we'll
  // override the `match_origin_as_fallback` behavior on the user script later.
  std::optional<MatchOriginAsFallbackBehavior>
      match_origin_as_fallback_override;
  if (!serialized_script.match_origin_as_fallback.has_value() &&
      content_script.match_about_blank && *content_script.match_about_blank) {
    match_origin_as_fallback_override =
        MatchOriginAsFallbackBehavior::kMatchForAboutSchemeAndClimbTree;
  }

  serialized_script.include_globs = std::move(content_script.include_globs);
  serialized_script.exclude_globs = std::move(content_script.exclude_globs);
  serialized_script.run_at = content_script.run_at;

  // Parse execution world. This should only be possible for MV3.
  if (content_script.world != api::extension_types::ExecutionWorld::kNone) {
    if (extension->manifest_version() >= 3) {
      serialized_script.world = content_script.world;
    } else {
      extension->AddInstallWarning(
          InstallWarning(errors::kExecutionWorldRestrictedToMV3,
                         ContentScriptsKeys::kContentScripts));
    }
  }

  // At this point, no script is allowed in incognito. If the extension is
  // allowed to run in incognito, this will be updated when loading the
  // script content.
  const bool allowed_in_incognito = false;
  bool wants_file_access = false;

  script_serialization::SerializedUserScriptParseOptions parse_options;
  parse_options.index_for_error = definition_index;
  parse_options.can_execute_script_everywhere = can_execute_script_everywhere;
  parse_options.all_urls_includes_chrome_urls = all_urls_includes_chrome_urls;

  std::unique_ptr<UserScript> user_script =
      script_serialization::ParseSerializedUserScript(
          serialized_script, *extension, allowed_in_incognito, error,
          &wants_file_access, parse_options);

  if (!user_script) {
    // Parsing failed. `error` should be properly populated.
    return nullptr;
  }

  if (match_origin_as_fallback_override) {
    // Note: No need to call `ValidateMatchOriginAsFallback()` since this
    // override is restricted to `kMatchForAboutSchemeAndClimbTree`, which
    // doesn't require validation.
    user_script->set_match_origin_as_fallback(
        *match_origin_as_fallback_override);
  }

  // Note: Not just `extension->set_wants_file_access(wants_file_access);` to
  // avoid overwriting a previous `true` value.
  if (wants_file_access) {
    extension->set_wants_file_access(true);
  }

  return user_script;
}

struct EmptyUserScriptList {
  UserScriptList user_script_list;
};

static base::LazyInstance<EmptyUserScriptList>::DestructorAtExit
    g_empty_script_list = LAZY_INSTANCE_INITIALIZER;

}  // namespace

ContentScriptsInfo::ContentScriptsInfo() = default;

ContentScriptsInfo::~ContentScriptsInfo() = default;

// static
const UserScriptList& ContentScriptsInfo::GetContentScripts(
    const Extension* extension) {
  ContentScriptsInfo* info = static_cast<ContentScriptsInfo*>(
      extension->GetManifestData(ContentScriptsKeys::kContentScripts));
  return info ? info->content_scripts
              : g_empty_script_list.Get().user_script_list;
}

// static
bool ContentScriptsInfo::ExtensionHasScriptAtURL(const Extension* extension,
                                                 const GURL& url) {
  for (const std::unique_ptr<UserScript>& script :
       GetContentScripts(extension)) {
    if (script->MatchesURL(url))
      return true;
  }
  return false;
}

// static
URLPatternSet ContentScriptsInfo::GetScriptableHosts(
    const Extension* extension) {
  URLPatternSet scriptable_hosts;
  for (const std::unique_ptr<UserScript>& script :
       GetContentScripts(extension)) {
    for (const URLPattern& pattern : script->url_patterns())
      scriptable_hosts.AddPattern(pattern);
  }
  return scriptable_hosts;
}

ContentScriptsHandler::ContentScriptsHandler() = default;

ContentScriptsHandler::~ContentScriptsHandler() = default;

base::span<const char* const> ContentScriptsHandler::Keys() const {
  static constexpr const char* kKeys[] = {ContentScriptsKeys::kContentScripts};
  return kKeys;
}

bool ContentScriptsHandler::Parse(Extension* extension, std::u16string* error) {
  ContentScriptsKeys manifest_keys;
  if (!ContentScriptsKeys::ParseFromDictionary(
          extension->manifest()->available_values(), manifest_keys, *error)) {
    return false;
  }

  auto content_scripts_info = std::make_unique<ContentScriptsInfo>();

  const bool can_execute_script_everywhere =
      PermissionsData::CanExecuteScriptEverywhere(extension->id(),
                                                  extension->location());
  const bool all_urls_includes_chrome_urls =
      PermissionsData::AllUrlsIncludesChromeUrls(extension->id());
  for (size_t i = 0; i < manifest_keys.content_scripts.size(); ++i) {
    std::unique_ptr<UserScript> user_script =
        CreateUserScript(std::move(manifest_keys.content_scripts[i]), i,
                         can_execute_script_everywhere,
                         all_urls_includes_chrome_urls, extension, error);
    if (!user_script)
      return false;  // Failed to parse script context definition.

    user_script->set_host_id(
        mojom::HostID(mojom::HostID::HostType::kExtensions, extension->id()));
    if (extension->converted_from_user_script()) {
      user_script->set_emulate_greasemonkey(true);
      // Greasemonkey matches all frames.
      user_script->set_match_all_frames(true);
    }
    content_scripts_info->content_scripts.push_back(std::move(user_script));
  }

  extension->SetManifestData(ContentScriptsKeys::kContentScripts,
                             std::move(content_scripts_info));
  PermissionsParser::SetScriptableHosts(
      extension, ContentScriptsInfo::GetScriptableHosts(extension));
  return true;
}

bool ContentScriptsHandler::Validate(
    const Extension* extension,
    std::string* error,
    std::vector<InstallWarning>* warnings) const {
  // Validate that claimed script resources actually exist,
  // and are UTF-8 encoded.
  return script_parsing::ValidateFileSources(
      ContentScriptsInfo::GetContentScripts(extension),
      script_parsing::GetSymlinkPolicy(extension), error, warnings);
}

}  // namespace extensions
