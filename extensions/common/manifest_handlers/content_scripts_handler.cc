// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/content_scripts_handler.h"

#include <stddef.h>

#include <memory>

#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/api/content_scripts.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/host_id.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/script_constants.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/url_pattern_set.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace extensions {

namespace errors = manifest_errors;
namespace content_scripts_api = api::content_scripts;
using ContentScriptsKeys = content_scripts_api::ManifestKeys;

namespace {

UserScript::RunLocation ConvertRunLocation(content_scripts_api::RunAt run_at) {
  switch (run_at) {
    case content_scripts_api::RUN_AT_DOCUMENT_END:
      return UserScript::DOCUMENT_END;
    case content_scripts_api::RUN_AT_DOCUMENT_IDLE:
      return UserScript::DOCUMENT_IDLE;
    case content_scripts_api::RUN_AT_DOCUMENT_START:
      return UserScript::DOCUMENT_START;
    case content_scripts_api::RUN_AT_NONE:
      NOTREACHED();
      return UserScript::DOCUMENT_IDLE;
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
    base::string16* error) {
  auto result = std::make_unique<UserScript>();

  // run_at
  if (content_script.run_at != content_scripts_api::RUN_AT_NONE)
    result->set_run_location(ConvertRunLocation(content_script.run_at));

  // all_frames
  if (content_script.all_frames)
    result->set_match_all_frames(*content_script.all_frames);

  // match_origin_as_fallback
  bool has_match_origin_as_fallback = false;
  if (content_script.match_origin_as_fallback &&
      base::FeatureList::IsEnabled(
          extensions_features::kContentScriptsMatchOriginAsFallback)) {
    has_match_origin_as_fallback = true;
    result->set_match_origin_as_fallback(
        *content_script.match_origin_as_fallback
            ? MatchOriginAsFallbackBehavior::kAlways
            : MatchOriginAsFallbackBehavior::kNever);
  }

  // match_about_blank
  // Note: match_about_blank is ignored if |match_origin_as_fallback| was
  // specified.
  if (!has_match_origin_as_fallback && content_script.match_about_blank) {
    result->set_match_origin_as_fallback(
        *content_script.match_about_blank
            ? MatchOriginAsFallbackBehavior::kMatchForAboutSchemeAndClimbTree
            : MatchOriginAsFallbackBehavior::kNever);
  }

  // matches
  if (content_script.matches.empty()) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidMatchCount, base::NumberToString(definition_index));
    return nullptr;
  }

  for (size_t i = 0; i < content_script.matches.size(); ++i) {
    URLPattern pattern(valid_schemes);

    const std::string& match_str = content_script.matches[i];
    URLPattern::ParseResult parse_result = pattern.Parse(match_str);
    if (parse_result != URLPattern::ParseResult::kSuccess) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidMatch, base::NumberToString(definition_index),
          base::NumberToString(i),
          URLPattern::GetParseResultString(parse_result));
      return nullptr;
    }

    // TODO(aboxhall): check for webstore
    if (!all_urls_includes_chrome_urls &&
        pattern.scheme() != content::kChromeUIScheme) {
      // Exclude SCHEME_CHROMEUI unless it's been explicitly requested or
      // been granted by extension ID.
      // If the --extensions-on-chrome-urls flag has not been passed, requesting
      // a chrome:// url will cause a parse failure above, so there's no need to
      // check the flag here.
      pattern.SetValidSchemes(pattern.valid_schemes() &
                              ~URLPattern::SCHEME_CHROMEUI);
    }

    if (pattern.MatchesScheme(url::kFileScheme) &&
        !can_execute_script_everywhere) {
      extension->set_wants_file_access(true);
      if (!(extension->creation_flags() & Extension::ALLOW_FILE_ACCESS)) {
        pattern.SetValidSchemes(pattern.valid_schemes() &
                                ~URLPattern::SCHEME_FILE);
      }
    }

    result->add_url_pattern(pattern);
  }

  // exclude_matches
  if (content_script.exclude_matches) {
    for (size_t i = 0; i < content_script.exclude_matches->size(); ++i) {
      const std::string& match_str = content_script.exclude_matches->at(i);
      URLPattern pattern(valid_schemes);

      URLPattern::ParseResult parse_result = pattern.Parse(match_str);
      if (parse_result != URLPattern::ParseResult::kSuccess) {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            errors::kInvalidExcludeMatch,
            base::NumberToString(definition_index), base::NumberToString(i),
            URLPattern::GetParseResultString(parse_result));
        return nullptr;
      }

      result->add_exclude_url_pattern(pattern);
    }
  }

  // include/exclude globs (mostly for Greasemonkey compatibility).
  if (content_script.include_globs) {
    for (const std::string& glob : *content_script.include_globs)
      result->add_glob(glob);
  }
  if (content_script.exclude_globs) {
    for (const std::string& glob : *content_script.exclude_globs)
      result->add_exclude_glob(glob);
  }

  // js
  if (content_script.js) {
    result->js_scripts().reserve(content_script.js->size());
    for (const std::string& relative : *content_script.js) {
      GURL url = extension->GetResourceURL(relative);
      ExtensionResource resource = extension->GetResource(relative);
      result->js_scripts().push_back(std::make_unique<UserScript::File>(
          resource.extension_root(), resource.relative_path(), url));
    }
  }

  // css
  if (content_script.css) {
    result->css_scripts().reserve(content_script.css->size());
    for (const std::string& relative : *content_script.css) {
      GURL url = extension->GetResourceURL(relative);
      ExtensionResource resource = extension->GetResource(relative);
      result->css_scripts().push_back(std::make_unique<UserScript::File>(
          resource.extension_root(), resource.relative_path(), url));
    }
  }

  // The manifest needs to have at least one js or css user script definition.
  if (result->js_scripts().empty() && result->css_scripts().empty()) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kMissingFile, base::NumberToString(definition_index));
    return nullptr;
  }

  return result;
}

// Returns false and sets the error if script file can't be loaded,
// or if it's not UTF-8 encoded.
static bool IsScriptValid(const base::FilePath& path,
                          const base::FilePath& relative_path,
                          int message_id,
                          std::string* error) {
  std::string content;
  if (!base::PathExists(path) || !base::ReadFileToString(path, &content)) {
    *error =
        l10n_util::GetStringFUTF8(message_id, relative_path.LossyDisplayName());
    return false;
  }

  if (!base::IsStringUTF8(content)) {
    *error = l10n_util::GetStringFUTF8(IDS_EXTENSION_BAD_FILE_ENCODING,
                                       relative_path.LossyDisplayName());
    return false;
  }

  return true;
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

bool ContentScriptsHandler::Parse(Extension* extension, base::string16* error) {
  ContentScriptsKeys manifest_keys;
  if (!ContentScriptsKeys::ParseFromDictionary(
          extension->manifest()->available_values(), &manifest_keys, error)) {
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

    user_script->set_host_id(HostID(HostID::EXTENSIONS, extension->id()));
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
  ExtensionResource::SymlinkPolicy symlink_policy;
  if ((extension->creation_flags() & Extension::FOLLOW_SYMLINKS_ANYWHERE) !=
      0) {
    symlink_policy = ExtensionResource::FOLLOW_SYMLINKS_ANYWHERE;
  } else {
    symlink_policy = ExtensionResource::SYMLINKS_MUST_RESOLVE_WITHIN_ROOT;
  }

  const UserScriptList& content_scripts =
      ContentScriptsInfo::GetContentScripts(extension);
  for (const std::unique_ptr<UserScript>& script : content_scripts) {
    for (const std::unique_ptr<UserScript::File>& js_script :
         script->js_scripts()) {
      const base::FilePath& path = ExtensionResource::GetFilePath(
          js_script->extension_root(), js_script->relative_path(),
          symlink_policy);
      if (!IsScriptValid(path, js_script->relative_path(),
                         IDS_EXTENSION_LOAD_JAVASCRIPT_FAILED, error))
        return false;
    }

    for (const std::unique_ptr<UserScript::File>& css_script :
         script->css_scripts()) {
      const base::FilePath& path = ExtensionResource::GetFilePath(
          css_script->extension_root(), css_script->relative_path(),
          symlink_policy);
      if (!IsScriptValid(path, css_script->relative_path(),
                         IDS_EXTENSION_LOAD_CSS_FAILED, error))
        return false;
    }
  }

  return true;
}

}  // namespace extensions
