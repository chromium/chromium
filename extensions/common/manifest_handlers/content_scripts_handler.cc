// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/content_scripts_handler.h"

#include <stddef.h>

#include <memory>

#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/optional_util.h"
#include "base/values.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/api/content_scripts.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/mojom/host_id.mojom.h"
#include "extensions/common/mojom/run_location.mojom-shared.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/script_constants.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/url_pattern_set.h"
#include "extensions/common/utils/content_script_utils.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace extensions {

namespace errors = manifest_errors;
namespace content_scripts_api = api::content_scripts;
using ContentScriptsKeys = content_scripts_api::ManifestKeys;

namespace {

void ParseGlobs(const std::vector<std::string>* include_globs,
                const std::vector<std::string>* exclude_globs,
                UserScript* result) {
  // include/exclude globs (mostly for Greasemonkey compatibility).
  if (include_globs) {
    for (const std::string& glob : *include_globs)
      result->add_glob(glob);
  }
  if (exclude_globs) {
    for (const std::string& glob : *exclude_globs)
      result->add_exclude_glob(glob);
  }
}

// Helper method that converts a parsed ContentScript object into a UserScript
// object.
std::unique_ptr<UserScript> CreateUserScript(
    const content_scripts_api::ContentScript& content_script,
    int definition_index,
    bool can_execute_script_everywhere,
    int valid_schemes,
    bool all_urls_includes_chrome_urls,
    Extension* extension,
    std::u16string* error) {
  auto result = std::make_unique<UserScript>();

  // run_at
  if (content_script.run_at != content_scripts_api::RUN_AT_NONE) {
    result->set_run_location(
        script_parsing::ConvertManifestRunLocation(content_script.run_at));
  }

  // all_frames
  if (content_script.all_frames)
    result->set_match_all_frames(*content_script.all_frames);

  // match_origin_as_fallback and match_about_blank.
  // Note: `match_about_blank` is ignored if `match_origin_as_fallback` was
  // specified. `match_origin_as_fallback` can only be specified for extensions
  // running manifest version 3 or higher. `match_about_blank` can be specified
  // by any extensions (and is used by MV3+ extensions for compatibility).
  absl::optional<MatchOriginAsFallbackBehavior> match_origin_as_fallback;

  if (content_script.match_origin_as_fallback &&
      base::FeatureList::IsEnabled(
          extensions_features::kContentScriptsMatchOriginAsFallback)) {
    if (extension->manifest_version() >= 3) {
      match_origin_as_fallback = *content_script.match_origin_as_fallback
                                     ? MatchOriginAsFallbackBehavior::kAlways
                                     : MatchOriginAsFallbackBehavior::kNever;
    } else {
      extension->AddInstallWarning(
          InstallWarning(errors::kMatchOriginAsFallbackRestrictedToMV3,
                         ContentScriptsKeys::kContentScripts));
    }
  }

  if (!match_origin_as_fallback && content_script.match_about_blank) {
    match_origin_as_fallback =
        *content_script.match_about_blank
            ? MatchOriginAsFallbackBehavior::kMatchForAboutSchemeAndClimbTree
            : MatchOriginAsFallbackBehavior::kNever;
  }

  bool wants_file_access = false;
  if (!script_parsing::ParseMatchPatterns(
          content_script.matches,
          base::OptionalToPtr(content_script.exclude_matches), definition_index,
          extension->creation_flags(), can_execute_script_everywhere,
          valid_schemes, all_urls_includes_chrome_urls, result.get(), error,
          &wants_file_access)) {
    return nullptr;
  }

  if (match_origin_as_fallback) {
    // If the extension is using `match_origin_as_fallback`, we require the
    // pattern to match all paths. This is because origins don't have a path;
    // thus, if an extension specified `"match_origin_as_fallback": true` for
    // a pattern of `"https://google.com/maps/*"`, this script would also run
    // on about:blank, data:, etc frames from https://google.com (because in
    // both cases, the precursor origin is https://google.com).
    if (match_origin_as_fallback == MatchOriginAsFallbackBehavior::kAlways) {
      for (const auto& pattern : result->url_patterns()) {
        if (pattern.path() != "/*") {
          *error =
              base::ASCIIToUTF16(errors::kMatchOriginAsFallbackCantHavePaths);
          return nullptr;
        }
      }
    }

    result->set_match_origin_as_fallback(*match_origin_as_fallback);
  }

  if (wants_file_access)
    extension->set_wants_file_access(true);

  ParseGlobs(base::OptionalToPtr(content_script.include_globs),
             base::OptionalToPtr(content_script.exclude_globs), result.get());

  if (!script_parsing::ParseFileSources(
          extension, base::OptionalToPtr(content_script.js),
          base::OptionalToPtr(content_script.css), definition_index,
          result.get(), error)) {
    return nullptr;
  }

  return result;
}

struct EmptyUserScriptList {
  UserScriptList user_script_list;
};

static base::LazyInstance<EmptyUserScriptList>::DestructorAtExit
    g_empty_script_list = LAZY_INSTANCE_INITIALIZER;

}  // namespace

ContentScriptsInfo::ContentScriptsInfo() {}

ContentScriptsInfo::~ContentScriptsInfo() {}

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

ContentScriptsHandler::ContentScriptsHandler() {}

ContentScriptsHandler::~ContentScriptsHandler() {}

base::span<const char* const> ContentScriptsHandler::Keys() const {
  static constexpr const char* kKeys[] = {ContentScriptsKeys::kContentScripts};
  return kKeys;
}

bool ContentScriptsHandler::Parse(Extension* extension, std::u16string* error) {
  ContentScriptsKeys manifest_keys;
  if (!ContentScriptsKeys::ParseFromDictionary(
          extension->manifest()->available_values().GetDict(), &manifest_keys,
          error)) {
    return false;
  }

  auto content_scripts_info = std::make_unique<ContentScriptsInfo>();

  const bool can_execute_script_everywhere =
      PermissionsData::CanExecuteScriptEverywhere(extension->id(),
                                                  extension->location());
  const int valid_schemes =
      UserScript::ValidUserScriptSchemes(can_execute_script_everywhere);
  const bool all_urls_includes_chrome_urls =
      PermissionsData::AllUrlsIncludesChromeUrls(extension->id());
  for (size_t i = 0; i < manifest_keys.content_scripts.size(); ++i) {
    std::unique_ptr<UserScript> user_script = CreateUserScript(
        manifest_keys.content_scripts[i], i, can_execute_script_everywhere,
        valid_schemes, all_urls_includes_chrome_urls, extension, error);
    if (!user_script)
      return false;  // Failed to parse script context definition.

    user_script->set_host_id(
        mojom::HostID(mojom::HostID::HostType::kExtensions, extension->id()));
    if (extension->converted_from_user_script()) {
      user_script->set_emulate_greasemonkey(true);
      // Greasemonkey matches all frames.
      user_script->set_match_all_frames(true);
    }
    user_script->set_id(UserScript::GenerateUserScriptID());
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
      script_parsing::GetSymlinkPolicy(extension), error);
}

}  // namespace extensions
