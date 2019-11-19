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
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/host_id.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/url_pattern_set.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace extensions {

namespace keys = extensions::manifest_keys;
namespace values = manifest_values;
namespace errors = manifest_errors;

namespace {

// Helper method that loads either the include_globs or exclude_globs list
// from an entry in the content_script lists of the manifest.
bool LoadGlobsHelper(const base::Value& content_script,
                     int content_script_index,
                     const char* globs_property_name,
                     base::string16* error,
                     void (UserScript::*add_method)(const std::string& glob),
                     UserScript* instance) {
  const base::Value* list = content_script.FindKey(globs_property_name);
  if (!list)
    return true;  // they are optional

  if (!list->is_list()) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidGlobList, base::NumberToString(content_script_index),
        globs_property_name);
    return false;
  }

  base::span<const base::Value> list_storage = list->GetList();
  for (size_t i = 0; i < list_storage.size(); ++i) {
    if (!list_storage[i].is_string()) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidGlob, base::NumberToString(content_script_index),
          globs_property_name, base::NumberToString(i));
      return false;
    }

    (instance->*add_method)(list_storage[i].GetString());
  }

  return true;
}

// Helper method that loads a UserScript object from a dictionary in the
// content_script list of the manifest.
std::unique_ptr<UserScript> LoadUserScriptFromDictionary(
    const base::Value& content_script,
    int definition_index,
    Extension* extension,
    base::string16* error) {
  std::unique_ptr<UserScript> result(new UserScript());
  // run_at
  const base::Value* run_at = content_script.FindKey(keys::kRunAt);
  if (run_at != nullptr) {
    if (!run_at->is_string()) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidRunAt, base::NumberToString(definition_index));
      return nullptr;
    }

    const std::string& run_location = run_at->GetString();
    if (run_location == values::kRunAtDocumentStart) {
      result->set_run_location(UserScript::DOCUMENT_START);
    } else if (run_location == values::kRunAtDocumentEnd) {
      result->set_run_location(UserScript::DOCUMENT_END);
    } else if (run_location == values::kRunAtDocumentIdle) {
      result->set_run_location(UserScript::DOCUMENT_IDLE);
    } else {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidRunAt, base::NumberToString(definition_index));
      return nullptr;
    }
  }

  // all frames
  const base::Value* all_frames = content_script.FindKey(keys::kAllFrames);
  if (all_frames != nullptr) {
    if (!all_frames->is_bool()) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidAllFrames, base::NumberToString(definition_index));
      return nullptr;
    }
    result->set_match_all_frames(all_frames->GetBool());
  }

  // match about blank
  const base::Value* match_about_blank =
      content_script.FindKey(keys::kMatchAboutBlank);
  if (match_about_blank != nullptr) {
    if (!match_about_blank->is_bool()) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidMatchAboutBlank,
          base::NumberToString(definition_index));
      return nullptr;
    }
    result->set_match_about_blank(match_about_blank->GetBool());
  }

  // matches (required)
  const base::Value* matches =
      content_script.FindKeyOfType(keys::kMatches, base::Value::Type::LIST);
  if (matches == nullptr) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidMatches, base::NumberToString(definition_index));
    return nullptr;
  }

  base::span<const base::Value> list_storage = matches->GetList();
  if (list_storage.empty()) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidMatchCount, base::NumberToString(definition_index));
    return nullptr;
  }

  const bool can_execute_script_everywhere =
      PermissionsData::CanExecuteScriptEverywhere(extension->id(),
                                                  extension->location());
  const int valid_schemes =
      UserScript::ValidUserScriptSchemes(can_execute_script_everywhere);

  const bool all_urls_includes_chrome_urls =
      PermissionsData::AllUrlsIncludesChromeUrls(extension->id());

  for (size_t j = 0; j < list_storage.size(); ++j) {
    if (!list_storage[j].is_string()) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidMatch, base::NumberToString(definition_index),
          base::NumberToString(j), errors::kExpectString);
      return nullptr;
    }
    const std::string& match_str = list_storage[j].GetString();

    URLPattern pattern(valid_schemes);

    URLPattern::ParseResult parse_result = pattern.Parse(match_str);
    if (parse_result != URLPattern::ParseResult::kSuccess) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidMatch, base::NumberToString(definition_index),
          base::NumberToString(j),
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
  const base::Value* exclude_matches =
      content_script.FindKey(keys::kExcludeMatches);
  if (exclude_matches != nullptr) {  // optional
    if (!exclude_matches->is_list()) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidExcludeMatches,
          base::NumberToString(definition_index));
      return nullptr;
    }
    base::span<const base::Value> list_storage = exclude_matches->GetList();

    for (size_t j = 0; j < list_storage.size(); ++j) {
      if (!list_storage[j].is_string()) {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            errors::kInvalidExcludeMatch,
            base::NumberToString(definition_index), base::NumberToString(j),
            errors::kExpectString);
        return nullptr;
      }
      const std::string& match_str = list_storage[j].GetString();
      URLPattern pattern(valid_schemes);

      URLPattern::ParseResult parse_result = pattern.Parse(match_str);
      if (parse_result != URLPattern::ParseResult::kSuccess) {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            errors::kInvalidExcludeMatch,
            base::NumberToString(definition_index), base::NumberToString(j),
            URLPattern::GetParseResultString(parse_result));
        return nullptr;
      }

      result->add_exclude_url_pattern(pattern);
    }
  }

  // include/exclude globs (mostly for Greasemonkey compatibility)
  if (!LoadGlobsHelper(content_script, definition_index, keys::kIncludeGlobs,
                       error, &UserScript::add_glob, result.get())) {
    return nullptr;
  }

  if (!LoadGlobsHelper(content_script, definition_index, keys::kExcludeGlobs,
                       error, &UserScript::add_exclude_glob, result.get())) {
    return nullptr;
  }

  // js and css keys
  const base::Value* js = content_script.FindKey(keys::kJs);
  if (js != nullptr && !js->is_list()) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidJsList, base::NumberToString(definition_index));
    return nullptr;
  }

  const base::Value* css = content_script.FindKey(keys::kCss);
  if (css != nullptr && !css->is_list()) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidCssList, base::NumberToString(definition_index));
    return nullptr;
  }

  // The manifest needs to have at least one js or css user script definition.
  if (((js ? js->GetList().size() : 0) + (css ? css->GetList().size() : 0)) ==
      0) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kMissingFile, base::NumberToString(definition_index));
    return nullptr;
  }

  if (js) {
    base::span<const base::Value> js_list = js->GetList();
    result->js_scripts().reserve(js_list.size());
    for (size_t script_index = 0; script_index < js_list.size();
         ++script_index) {
      if (!js_list[script_index].is_string()) {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            errors::kInvalidJs, base::NumberToString(definition_index),
            base::NumberToString(script_index));
        return nullptr;
      }
      const std::string& relative = js_list[script_index].GetString();
      GURL url = extension->GetResourceURL(relative);
      ExtensionResource resource = extension->GetResource(relative);
      result->js_scripts().push_back(std::make_unique<UserScript::File>(
          resource.extension_root(), resource.relative_path(), url));
    }
  }

  if (css) {
    base::span<const base::Value> css_list = css->GetList();
    result->css_scripts().reserve(css_list.size());
    for (size_t script_index = 0; script_index < css_list.size();
         ++script_index) {
      if (!css_list[script_index].is_string()) {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            errors::kInvalidCss, base::NumberToString(definition_index),
            base::NumberToString(script_index));
        return nullptr;
      }
      const std::string& relative = css_list[script_index].GetString();
      GURL url = extension->GetResourceURL(relative);
      ExtensionResource resource = extension->GetResource(relative);
      result->css_scripts().push_back(std::make_unique<UserScript::File>(
          resource.extension_root(), resource.relative_path(), url));
    }
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
      extension->GetManifestData(keys::kContentScripts));
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
  static constexpr const char* kKeys[] = {keys::kContentScripts};
  return kKeys;
}

bool ContentScriptsHandler::Parse(Extension* extension, base::string16* error) {
  std::unique_ptr<ContentScriptsInfo> content_scripts_info(
      new ContentScriptsInfo);
  const base::Value* scripts_list = nullptr;
  if (!extension->manifest()->GetList(keys::kContentScripts, &scripts_list)) {
    *error = base::ASCIIToUTF16(errors::kInvalidContentScriptsList);
    return false;
  }

  base::span<const base::Value> list_storage = scripts_list->GetList();
  for (size_t i = 0; i < list_storage.size(); ++i) {
    if (!list_storage[i].is_dict()) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidContentScript, base::NumberToString(i));
      return false;
    }

    std::unique_ptr<UserScript> user_script =
        LoadUserScriptFromDictionary(list_storage[i], i, extension, error);
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
  extension->SetManifestData(keys::kContentScripts,
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
