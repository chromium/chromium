// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/utils/content_script_utils.h"

#include <stddef.h>

#include <algorithm>
#include <memory>

#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/api/content_scripts.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/mojom/host_id.mojom.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/script_constants.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/url_pattern_set.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace extensions {

namespace errors = manifest_errors;

namespace script_parsing {

namespace {

size_t g_max_script_length_in_bytes = 1024u * 1024u * 500u;  // 500 MB.
size_t g_max_scripts_length_per_extension_in_bytes =
    1024u * 1024u * 1024u;  // 1 GB.

// Returns false and sets the error if script file can't be loaded, or if it's
// not UTF-8 encoded. If a script file can be loaded but will exceed
// `max_script_length`, return true but add an install warning to `warnings`.
// Otherwise, decrement `remaining_length` by the script file's size.
bool IsScriptValid(const base::FilePath& path,
                   const base::FilePath& relative_path,
                   size_t max_script_length,
                   int message_id,
                   std::string* error,
                   std::vector<InstallWarning>* warnings,
                   size_t& remaining_length) {
  InstallWarning script_file_too_large_warning(
      l10n_util::GetStringFUTF8(IDS_EXTENSION_CONTENT_SCRIPT_FILE_TOO_LARGE,
                                relative_path.LossyDisplayName()),
      api::content_scripts::ManifestKeys::kContentScripts,
      base::UTF16ToUTF8(relative_path.LossyDisplayName()));
  if (remaining_length == 0u) {
    warnings->push_back(std::move(script_file_too_large_warning));
    return true;
  }

  std::string content;
  std::string file_not_read_error =
      l10n_util::GetStringFUTF8(message_id, relative_path.LossyDisplayName());

  if (!base::PathExists(path)) {
    *error = file_not_read_error;
    return false;
  }

  bool read_successful =
      base::ReadFileToStringWithMaxSize(path, &content, max_script_length);
  // If the size of the file in `path` exceeds `max_script_length`,
  // ReadFileToStringWithMaxSize will return false but `content` will contain
  // the file's content truncated to `max_script_length`.
  if (!read_successful && content.length() != max_script_length) {
    *error = file_not_read_error;
    return false;
  }

  if (!base::IsStringUTF8(content)) {
    *error = l10n_util::GetStringFUTF8(IDS_EXTENSION_BAD_FILE_ENCODING,
                                       relative_path.LossyDisplayName());
    return false;
  }

  if (read_successful) {
    remaining_length -= content.size();
  } else {
    // Even though the script file is over the max size, we don't throw a hard
    // error so as not to break any existing extensions for which this is the
    // case.
    warnings->push_back(std::move(script_file_too_large_warning));
  }
  return true;
}

}  // namespace

size_t GetMaxScriptLength() {
  return g_max_script_length_in_bytes;
}

size_t GetMaxScriptsLengthPerExtension() {
  return g_max_scripts_length_per_extension_in_bytes;
}

ScopedMaxScriptLengthOverride CreateScopedMaxScriptLengthForTesting(  // IN-TEST
    size_t max) {
  return ScopedMaxScriptLengthOverride(&g_max_script_length_in_bytes, max);
}

ScopedMaxScriptLengthOverride
CreateScopedMaxScriptsLengthPerExtensionForTesting(size_t max) {
  return ScopedMaxScriptLengthOverride(
      &g_max_scripts_length_per_extension_in_bytes, max);
}

mojom::RunLocation ConvertManifestRunLocation(
    api::content_scripts::RunAt run_at) {
  switch (run_at) {
    case api::content_scripts::RUN_AT_DOCUMENT_END:
      return mojom::RunLocation::kDocumentEnd;
    case api::content_scripts::RUN_AT_DOCUMENT_IDLE:
      return mojom::RunLocation::kDocumentIdle;
    case api::content_scripts::RUN_AT_DOCUMENT_START:
      return mojom::RunLocation::kDocumentStart;
    case api::content_scripts::RUN_AT_NONE:
      NOTREACHED();
      return mojom::RunLocation::kDocumentIdle;
  }
}

api::content_scripts::RunAt ConvertRunLocationToManifestType(
    mojom::RunLocation run_at) {
  // api::extension_types does not have analogues for kUndefined, kRunDeferred
  // or kBrowserDriven. We don't expect to encounter them here.
  switch (run_at) {
    case mojom::RunLocation::kDocumentEnd:
      return api::content_scripts::RUN_AT_DOCUMENT_END;
    case mojom::RunLocation::kDocumentStart:
      return api::content_scripts::RUN_AT_DOCUMENT_START;
    case mojom::RunLocation::kDocumentIdle:
      return api::content_scripts::RUN_AT_DOCUMENT_IDLE;
    case mojom::RunLocation::kUndefined:
    case mojom::RunLocation::kRunDeferred:
    case mojom::RunLocation::kBrowserDriven:
      break;
  }

  NOTREACHED();
  return api::content_scripts::RUN_AT_DOCUMENT_IDLE;
}

bool ParseMatchPatterns(const std::vector<std::string>& matches,
                        const std::vector<std::string>* exclude_matches,
                        int definition_index,
                        int creation_flags,
                        bool can_execute_script_everywhere,
                        int valid_schemes,
                        bool all_urls_includes_chrome_urls,
                        UserScript* result,
                        std::u16string* error,
                        bool* wants_file_access) {
  if (matches.empty()) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidMatchCount, base::NumberToString(definition_index));
    return false;
  }

  for (size_t i = 0; i < matches.size(); ++i) {
    URLPattern pattern(valid_schemes);

    const std::string& match_str = matches[i];
    URLPattern::ParseResult parse_result = pattern.Parse(match_str);
    if (parse_result != URLPattern::ParseResult::kSuccess) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidMatch, base::NumberToString(definition_index),
          base::NumberToString(i),
          URLPattern::GetParseResultString(parse_result));
      return false;
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
      if (wants_file_access)
        *wants_file_access = true;
      if (!(creation_flags & Extension::ALLOW_FILE_ACCESS)) {
        pattern.SetValidSchemes(pattern.valid_schemes() &
                                ~URLPattern::SCHEME_FILE);
      }
    }

    result->add_url_pattern(pattern);
  }

  if (exclude_matches) {
    for (size_t i = 0; i < exclude_matches->size(); ++i) {
      const std::string& match_str = exclude_matches->at(i);
      URLPattern pattern(valid_schemes);

      URLPattern::ParseResult parse_result = pattern.Parse(match_str);
      if (parse_result != URLPattern::ParseResult::kSuccess) {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            errors::kInvalidExcludeMatch,
            base::NumberToString(definition_index), base::NumberToString(i),
            URLPattern::GetParseResultString(parse_result));
        return false;
      }

      result->add_exclude_url_pattern(pattern);
    }
  }

  return true;
}

bool ParseFileSources(const Extension* extension,
                      const std::vector<std::string>* js,
                      const std::vector<std::string>* css,
                      int definition_index,
                      UserScript* result,
                      std::u16string* error) {
  if (js) {
    result->js_scripts().reserve(js->size());
    for (const std::string& relative : *js) {
      GURL url = extension->GetResourceURL(relative);
      ExtensionResource resource = extension->GetResource(relative);
      result->js_scripts().push_back(std::make_unique<UserScript::File>(
          resource.extension_root(), resource.relative_path(), url));
    }
  }

  if (css) {
    result->css_scripts().reserve(css->size());
    for (const std::string& relative : *css) {
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
    return false;
  }

  return true;
}

bool ValidateFileSources(const UserScriptList& scripts,
                         ExtensionResource::SymlinkPolicy symlink_policy,
                         std::string* error,
                         std::vector<InstallWarning>* warnings) {
  size_t remaining_scripts_length = GetMaxScriptsLengthPerExtension();

  for (const std::unique_ptr<UserScript>& script : scripts) {
    for (const std::unique_ptr<UserScript::File>& js_script :
         script->js_scripts()) {
      const base::FilePath& path = ExtensionResource::GetFilePath(
          js_script->extension_root(), js_script->relative_path(),
          symlink_policy);
      size_t max_script_length =
          std::min(remaining_scripts_length, GetMaxScriptLength());
      if (!IsScriptValid(path, js_script->relative_path(), max_script_length,
                         IDS_EXTENSION_LOAD_JAVASCRIPT_FAILED, error, warnings,
                         remaining_scripts_length)) {
        return false;
      }
    }

    for (const std::unique_ptr<UserScript::File>& css_script :
         script->css_scripts()) {
      const base::FilePath& path = ExtensionResource::GetFilePath(
          css_script->extension_root(), css_script->relative_path(),
          symlink_policy);
      size_t max_script_length =
          std::min(remaining_scripts_length, GetMaxScriptLength());
      if (!IsScriptValid(path, css_script->relative_path(), max_script_length,
                         IDS_EXTENSION_LOAD_CSS_FAILED, error, warnings,
                         remaining_scripts_length)) {
        return false;
      }
    }
  }

  return true;
}

ExtensionResource::SymlinkPolicy GetSymlinkPolicy(const Extension* extension) {
  if ((extension->creation_flags() & Extension::FOLLOW_SYMLINKS_ANYWHERE) !=
      0) {
    return ExtensionResource::FOLLOW_SYMLINKS_ANYWHERE;
  }

  return ExtensionResource::SYMLINKS_MUST_RESOLVE_WITHIN_ROOT;
}

}  // namespace script_parsing
}  // namespace extensions
