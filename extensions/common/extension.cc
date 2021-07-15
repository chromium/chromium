// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/extension.h"

#include <stddef.h>

#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/i18n/rtl.h"
#include "base/json/json_writer.h"
#include "base/memory/singleton.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/elapsed_timer.h"
#include "base/values.h"
#include "base/version.h"
#include "components/crx_file/id_util.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handler.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/permissions/permissions_info.h"
#include "extensions/common/switches.h"
#include "extensions/common/url_pattern.h"
#include "net/base/filename_util.h"
#include "url/url_util.h"

using extensions::mojom::ManifestLocation;

namespace extensions {

namespace keys = manifest_keys;
namespace values = manifest_values;
namespace errors = manifest_errors;

namespace {

constexpr int kModernManifestVersion = 2;
constexpr int kMaximumSupportedManifestVersion = 3;
constexpr int kPEMOutputColumns = 64;
static_assert(kMaximumSupportedManifestVersion >= kModernManifestVersion,
              "The modern manifest version must be supported.");

// KEY MARKERS
constexpr char kKeyBeginHeaderMarker[] = "-----BEGIN";
constexpr char kKeyBeginFooterMarker[] = "-----END";
constexpr char kKeyInfoEndMarker[] = "KEY-----";
constexpr char kPublic[] = "PUBLIC";
constexpr char kPrivate[] = "PRIVATE";

bool ContainsReservedCharacters(const base::FilePath& path) {
  // We should disallow backslash '\\' as file path separator even on Windows,
  // because the backslash is not regarded as file path separator on Linux/Mac.
  // Extensions are cross-platform.
  // Since FilePath uses backslash '\\' as file path separator on Windows, so we
  // need to check manually.
  if (path.value().find('\\') != path.value().npos)
    return true;
  return !net::IsSafePortableRelativePath(path);
}

// Returns true if the given |manifest_version| is supported for the specified
// |type| of extension. Optionally populates |warning| if an InstallWarning
// should be added.
bool IsManifestSupported(int manifest_version,
                         Manifest::Type type,
                         int creation_flags,
                         std::string* warning) {
  // The ultimate short-circuit: If the feature for MV3 is disabled, it's not
  // supported.
  if (manifest_version == 3 &&
      !base::FeatureList::IsEnabled(
          extensions_features::kMv3ExtensionsSupported)) {
    return false;
  }

  // Modern is always safe.
  if (manifest_version >= kModernManifestVersion &&
      manifest_version <= kMaximumSupportedManifestVersion) {
    return true;
  }

  if (manifest_version > kMaximumSupportedManifestVersion) {
    // Silence future manifest error with flag.
    bool allow_future_manifest_version =
        base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kAllowFutureManifestVersion);
    if (!allow_future_manifest_version) {
      *warning = ErrorUtils::FormatErrorMessage(
          manifest_errors::kManifestVersionTooHighWarning,
          base::NumberToString(kMaximumSupportedManifestVersion),
          base::NumberToString(manifest_version));
    }
    return true;
  }

  // Allow an exception for extensions if a special commandline flag is present.
  // Note: This allows the extension to load, but it may effectively be treated
  // as a higher manifest version. For instance, all extension v1-specific
  // handling has been removed, which means they will effectively be treated as
  // v2s.
  bool allow_legacy_extensions =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAllowLegacyExtensionManifests);
  if (type == Manifest::TYPE_EXTENSION && allow_legacy_extensions)
    return true;

  if ((creation_flags & Extension::REQUIRE_MODERN_MANIFEST_VERSION) != 0)
    return false;

  static constexpr int kMinimumExtensionManifestVersion = 2;
  if (type == Manifest::TYPE_EXTENSION)
    return manifest_version >= kMinimumExtensionManifestVersion;

  static constexpr int kMinimumPlatformAppManifestVersion = 2;
  if (type == Manifest::TYPE_PLATFORM_APP)
    return manifest_version >= kMinimumPlatformAppManifestVersion;

  return true;
}

// Computes the |extension_id| from the given parameters. On success, returns
// true. On failure, populates |error| and returns false.
bool ComputeExtensionID(const base::DictionaryValue& manifest,
                        const base::FilePath& path,
                        int creation_flags,
                        std::u16string* error,
                        ExtensionId* extension_id) {
  if (manifest.HasKey(keys::kPublicKey)) {
    std::string public_key;
    std::string public_key_bytes;
    if (!manifest.GetString(keys::kPublicKey, &public_key) ||
        !Extension::ParsePEMKeyBytes(public_key, &public_key_bytes)) {
      *error = base::ASCIIToUTF16(errors::kInvalidKey);
      return false;
    }
    *extension_id = crx_file::id_util::GenerateId(public_key_bytes);
    return true;
  }

  if (creation_flags & Extension::REQUIRE_KEY) {
    *error = base::ASCIIToUTF16(errors::kInvalidKey);
    return false;
  }

  // If there is a path, we generate the ID from it. This is useful for
  // development mode, because it keeps the ID stable across restarts and
  // reloading the extension.
  *extension_id = crx_file::id_util::GenerateIdForPath(path);
  if (extension_id->empty()) {
    NOTREACHED() << "Could not create ID from path.";
    return false;
  }
  return true;
}

std::u16string InvalidManifestVersionError(const char* manifest_version_error,
                                           bool is_platform_app) {
  std::string valid_version;
  if (kModernManifestVersion == kMaximumSupportedManifestVersion) {
    valid_version = base::NumberToString(kModernManifestVersion);
  } else if (kMaximumSupportedManifestVersion - kModernManifestVersion == 1) {
    valid_version = base::StrCat(
        {"either ", base::NumberToString(kModernManifestVersion), " or ",
         base::NumberToString(kMaximumSupportedManifestVersion)});
  } else {
    valid_version = base::StrCat(
        {"between ", base::NumberToString(kModernManifestVersion), " and ",
         base::NumberToString(kMaximumSupportedManifestVersion)});
  }

  return ErrorUtils::FormatErrorMessageUTF16(
      manifest_version_error, valid_version,
      is_platform_app ? "apps" : "extensions");
}

}  // namespace

const int Extension::kInitFromValueFlagBits = 15;

const char Extension::kMimeType[] = "application/x-chrome-extension";

const int Extension::kValidWebExtentSchemes =
    URLPattern::SCHEME_HTTP | URLPattern::SCHEME_HTTPS;

const int Extension::kValidBookmarkAppSchemes = URLPattern::SCHEME_HTTP |
                                                URLPattern::SCHEME_HTTPS |
                                                URLPattern::SCHEME_EXTENSION;

const int Extension::kValidHostPermissionSchemes =
    URLPattern::SCHEME_CHROMEUI | URLPattern::SCHEME_HTTP |
    URLPattern::SCHEME_HTTPS | URLPattern::SCHEME_FILE |
    URLPattern::SCHEME_FTP | URLPattern::SCHEME_WS | URLPattern::SCHEME_WSS |
    URLPattern::SCHEME_URN;

//
// Extension
//

// static
scoped_refptr<Extension> Extension::Create(const base::FilePath& path,
                                           ManifestLocation location,
                                           const base::DictionaryValue& value,
                                           int flags,
                                           std::string* utf8_error) {
  return Extension::Create(path,
                           location,
                           value,
                           flags,
                           std::string(),  // ID is ignored if empty.
                           utf8_error);
}

// TODO(sungguk): Continue removing std::string errors and replacing
// with std::u16string. See http://crbug.com/71980.
scoped_refptr<Extension> Extension::Create(const base::FilePath& path,
                                           ManifestLocation location,
                                           const base::DictionaryValue& value,
                                           int flags,
                                           const std::string& explicit_id,
                                           std::string* utf8_error) {
  base::ElapsedTimer timer;
  DCHECK(utf8_error);
  std::u16string error;

  ExtensionId extension_id;
  if (!explicit_id.empty()) {
    extension_id = explicit_id;
  } else if (!ComputeExtensionID(value, path, flags, &error, &extension_id)) {
    *utf8_error = base::UTF16ToUTF8(error);
    return nullptr;
  }

  std::unique_ptr<extensions::Manifest> manifest;
  if (flags & FOR_LOGIN_SCREEN) {
    manifest = Manifest::CreateManifestForLoginScreen(
        location, value.CreateDeepCopy(), std::move(extension_id));
  } else {
    manifest = std::make_unique<Manifest>(location, value.CreateDeepCopy(),
                                          std::move(extension_id));
  }

  std::vector<InstallWarning> install_warnings;
  if (!manifest->ValidateManifest(utf8_error, &install_warnings)) {
    return nullptr;
  }

  scoped_refptr<Extension> extension = new Extension(path, std::move(manifest));
  extension->install_warnings_.swap(install_warnings);

  if (!extension->InitFromValue(flags, &error)) {
    *utf8_error = base::UTF16ToUTF8(error);
    return nullptr;
  }

  extension->guid_ = base::GUID::GenerateRandomV4();

  return extension;
}

Manifest::Type Extension::GetType() const {
  return converted_from_user_script() ?
      Manifest::TYPE_USER_SCRIPT : manifest_->type();
}

// static
GURL Extension::GetResourceURL(const GURL& extension_url,
                               const std::string& relative_path) {
  DCHECK(extension_url.SchemeIs(kExtensionScheme));
  return extension_url.Resolve(relative_path);
}

bool Extension::ResourceMatches(const URLPatternSet& pattern_set,
                                const std::string& resource) const {
  return pattern_set.MatchesURL(extension_url_.Resolve(resource));
}

ExtensionResource Extension::GetResource(
    base::StringPiece relative_path) const {
  // We have some legacy data where resources have leading slashes.
  // See: http://crbug.com/121164
  if (!relative_path.empty() && relative_path[0] == '/')
    relative_path.remove_prefix(1);
  base::FilePath relative_file_path =
      base::FilePath::FromUTF8Unsafe(relative_path);
  if (ContainsReservedCharacters(relative_file_path))
    return ExtensionResource();
  ExtensionResource r(id(), path(), relative_file_path);
  if ((creation_flags() & Extension::FOLLOW_SYMLINKS_ANYWHERE)) {
    r.set_follow_symlinks_anywhere();
  }
  return r;
}

ExtensionResource Extension::GetResource(
    const base::FilePath& relative_file_path) const {
  if (ContainsReservedCharacters(relative_file_path))
    return ExtensionResource();
  ExtensionResource r(id(), path(), relative_file_path);
  if ((creation_flags() & Extension::FOLLOW_SYMLINKS_ANYWHERE)) {
    r.set_follow_symlinks_anywhere();
  }
  return r;
}

// TODO(rafaelw): Move ParsePEMKeyBytes, ProducePEM & FormatPEMForOutput to a
// util class in base:
// http://code.google.com/p/chromium/issues/detail?id=13572
// static
bool Extension::ParsePEMKeyBytes(const std::string& input,
                                 std::string* output) {
  DCHECK(output);
  if (!output)
    return false;
  if (input.length() == 0)
    return false;

  std::string working = input;
  if (base::StartsWith(working, kKeyBeginHeaderMarker,
                       base::CompareCase::SENSITIVE)) {
    working = base::CollapseWhitespaceASCII(working, true);
    size_t header_pos = working.find(kKeyInfoEndMarker,
      sizeof(kKeyBeginHeaderMarker) - 1);
    if (header_pos == std::string::npos)
      return false;
    size_t start_pos = header_pos + sizeof(kKeyInfoEndMarker) - 1;
    size_t end_pos = working.rfind(kKeyBeginFooterMarker);
    if (end_pos == std::string::npos)
      return false;
    if (start_pos >= end_pos)
      return false;

    working = working.substr(start_pos, end_pos - start_pos);
    if (working.length() == 0)
      return false;
  }

  return base::Base64Decode(working, output);
}

// static
bool Extension::ProducePEM(const std::string& input, std::string* output) {
  DCHECK(output);
  if (input.empty())
    return false;
  base::Base64Encode(input, output);
  return true;
}

// static
bool Extension::FormatPEMForFileOutput(const std::string& input,
                                       std::string* output,
                                       bool is_public) {
  DCHECK(output);
  if (input.length() == 0)
    return false;
  *output = "";
  output->append(kKeyBeginHeaderMarker);
  output->append(" ");
  output->append(is_public ? kPublic : kPrivate);
  output->append(" ");
  output->append(kKeyInfoEndMarker);
  output->append("\n");
  for (size_t i = 0; i < input.length(); ) {
    int slice = std::min<int>(input.length() - i, kPEMOutputColumns);
    output->append(input.substr(i, slice));
    output->append("\n");
    i += slice;
  }
  output->append(kKeyBeginFooterMarker);
  output->append(" ");
  output->append(is_public ? kPublic : kPrivate);
  output->append(" ");
  output->append(kKeyInfoEndMarker);
  output->append("\n");

  return true;
}

// static
GURL Extension::GetBaseURLFromExtensionId(const std::string& extension_id) {
  return GURL(base::StrCat({extensions::kExtensionScheme,
                            url::kStandardSchemeSeparator, extension_id}));
}

bool Extension::OverlapsWithOrigin(const GURL& origin) const {
  if (url() == origin)
    return true;

  if (web_extent().is_empty())
    return false;

  // Note: patterns and extents ignore port numbers.
  URLPattern origin_only_pattern(kValidWebExtentSchemes);
  if (!origin_only_pattern.SetScheme(origin.scheme()))
    return false;
  origin_only_pattern.SetHost(origin.host());
  origin_only_pattern.SetPath("/*");

  URLPatternSet origin_only_pattern_list;
  origin_only_pattern_list.AddPattern(origin_only_pattern);

  return web_extent().OverlapsWith(origin_only_pattern_list);
}

bool Extension::RequiresSortOrdinal() const {
  return is_app() && (display_in_launcher_ || display_in_new_tab_page_);
}

bool Extension::ShouldDisplayInAppLauncher() const {
  // Only apps should be displayed in the launcher.
  return is_app() && display_in_launcher_;
}

bool Extension::ShouldDisplayInNewTabPage() const {
  // Only apps should be displayed on the NTP.
  return is_app() && display_in_new_tab_page_;
}

bool Extension::ShouldExposeViaManagementAPI() const {
  // Hide component extensions because they are only extensions as an
  // implementation detail of Chrome.
  return !extensions::Manifest::IsComponentLocation(location());
}

Extension::ManifestData* Extension::GetManifestData(const std::string& key)
    const {
  DCHECK(finished_parsing_manifest_ || thread_checker_.CalledOnValidThread());
  auto iter = manifest_data_.find(key);
  if (iter != manifest_data_.end())
    return iter->second.get();
  return nullptr;
}

void Extension::SetManifestData(const std::string& key,
                                std::unique_ptr<Extension::ManifestData> data) {
  DCHECK(!finished_parsing_manifest_ && thread_checker_.CalledOnValidThread());
  manifest_data_[key] = std::move(data);
}

void Extension::SetGUID(const ExtensionGuid& guid) {
  guid_ = base::GUID::ParseLowercase(guid);
  DCHECK(guid_.is_valid());
}

const ExtensionGuid& Extension::guid() const {
  DCHECK(guid_.is_valid());
  return guid_.AsLowercaseString();
}

ManifestLocation Extension::location() const {
  return manifest_->location();
}

const std::string& Extension::id() const {
  return manifest_->extension_id();
}

const HashedExtensionId& Extension::hashed_id() const {
  return manifest_->hashed_id();
}

std::string Extension::VersionString() const {
  return version_.GetString();
}

std::string Extension::DifferentialFingerprint() const {
  std::string fingerprint;
  // We currently support two sources of differential fingerprints:
  // server-provided and synthesized. Fingerprints are of the format V.FP, where
  // V indicates the fingerprint type (1 for SHA256 hash, 2 for app version) and
  // FP indicates the value. The hash-based FP from the server is more precise
  // (a hash of the extension CRX), so use that when available, otherwise
  // synthesize a 2.VERSION fingerprint for use. For more information, see
  // https://github.com/google/omaha/blob/master/doc/ServerProtocolV3.md#packages--fingerprints
  return manifest_->GetString(keys::kDifferentialFingerprint, &fingerprint)
             ? fingerprint
             : "2." + VersionString();
}

std::string Extension::GetVersionForDisplay() const {
  if (version_name_.size() > 0)
    return version_name_;
  return VersionString();
}

void Extension::AddInstallWarning(InstallWarning new_warning) {
  install_warnings_.push_back(std::move(new_warning));
}

void Extension::AddInstallWarnings(std::vector<InstallWarning> new_warnings) {
  install_warnings_.insert(install_warnings_.end(),
                           std::make_move_iterator(new_warnings.begin()),
                           std::make_move_iterator(new_warnings.end()));
}

bool Extension::is_app() const {
  return manifest()->is_app();
}

bool Extension::is_platform_app() const {
  return manifest()->is_platform_app();
}

bool Extension::is_hosted_app() const {
  return manifest()->is_hosted_app();
}

bool Extension::is_legacy_packaged_app() const {
  return manifest()->is_legacy_packaged_app();
}

bool Extension::is_extension() const {
  return manifest()->is_extension();
}

bool Extension::is_shared_module() const {
  return manifest()->is_shared_module();
}

bool Extension::is_theme() const {
  return manifest()->is_theme();
}

bool Extension::is_login_screen_extension() const {
  return manifest()->is_login_screen_extension();
}

bool Extension::is_chromeos_system_extension() const {
  return manifest()->is_chromeos_system_extension();
}

void Extension::AddWebExtentPattern(const URLPattern& pattern) {
  // Bookmark apps are permissionless.
  if (from_bookmark())
    return;

  extent_.AddPattern(pattern);
}

Extension::Extension(const base::FilePath& path,
                     std::unique_ptr<extensions::Manifest> manifest)
    : manifest_version_(0),
      converted_from_user_script_(false),
      manifest_(manifest.release()),
      finished_parsing_manifest_(false),
      display_in_launcher_(true),
      display_in_new_tab_page_(true),
      wants_file_access_(false),
      creation_flags_(0) {
  DCHECK(path.empty() || path.IsAbsolute());
  path_ = crx_file::id_util::MaybeNormalizePath(path);
}

Extension::~Extension() {
}

bool Extension::InitFromValue(int flags, std::u16string* error) {
  DCHECK(error);

  creation_flags_ = flags;

  // Check for |converted_from_user_script| first, since it affects the type
  // returned by GetType(). This is needed to determine if the manifest version
  // is valid.
  if (manifest_->HasKey(keys::kConvertedFromUserScript)) {
    manifest_->GetBoolean(keys::kConvertedFromUserScript,
                          &converted_from_user_script_);
  }

  // Important to load manifest version first because many other features
  // depend on its value.
  if (!LoadManifestVersion(error))
    return false;

  if (!LoadRequiredFeatures(error))
    return false;

  // We don't need to validate because InitExtensionID already did that.
  manifest_->GetString(keys::kPublicKey, &public_key_);

  extension_url_ = Extension::GetBaseURLFromExtensionId(id());

  // Load App settings. LoadExtent at least has to be done before
  // ParsePermissions(), because the valid permissions depend on what type of
  // package this is.
  if (is_app() && !LoadAppFeatures(error))
    return false;

  permissions_parser_ = std::make_unique<PermissionsParser>();
  if (!permissions_parser_->Parse(this, error))
    return false;

  if (!LoadSharedFeatures(error))
    return false;

  permissions_parser_->Finalize(this);
  permissions_parser_.reset();

  finished_parsing_manifest_ = true;

  permissions_data_ = std::make_unique<PermissionsData>(
      id(), GetType(), location(),
      PermissionsParser::GetRequiredPermissions(this).Clone());

  return true;
}

bool Extension::LoadRequiredFeatures(std::u16string* error) {
  if (!LoadName(error) ||
      !LoadVersion(error))
    return false;
  return true;
}

bool Extension::LoadName(std::u16string* error) {
  std::u16string localized_name;
  if (!manifest_->GetString(keys::kName, &localized_name)) {
    *error = base::ASCIIToUTF16(errors::kInvalidName);
    return false;
  }

  non_localized_name_ = base::UTF16ToUTF8(localized_name);
  std::u16string sanitized_name =
      base::CollapseWhitespace(localized_name, true);
  base::i18n::SanitizeUserSuppliedString(&sanitized_name);
  display_name_ = base::UTF16ToUTF8(sanitized_name);
  return true;
}

bool Extension::LoadVersion(std::u16string* error) {
  std::string version_str;
  if (!manifest_->GetString(keys::kVersion, &version_str)) {
    *error = base::ASCIIToUTF16(errors::kInvalidVersion);
    return false;
  }
  version_ = base::Version(version_str);
  if (!version_.IsValid() || version_.components().size() > 4) {
    *error = base::ASCIIToUTF16(errors::kInvalidVersion);
    return false;
  }
  if (manifest_->HasKey(keys::kVersionName)) {
    if (!manifest_->GetString(keys::kVersionName, &version_name_)) {
      *error = base::ASCIIToUTF16(errors::kInvalidVersionName);
      return false;
    }
  }
  return true;
}

bool Extension::LoadAppFeatures(std::u16string* error) {
  if (!LoadExtent(keys::kWebURLs, &extent_,
                  errors::kInvalidWebURLs, errors::kInvalidWebURL, error)) {
    return false;
  }
  if (manifest_->HasKey(keys::kDisplayInLauncher) &&
      !manifest_->GetBoolean(keys::kDisplayInLauncher, &display_in_launcher_)) {
    *error = base::ASCIIToUTF16(errors::kInvalidDisplayInLauncher);
    return false;
  }
  if (manifest_->HasKey(keys::kDisplayInNewTabPage)) {
    if (!manifest_->GetBoolean(keys::kDisplayInNewTabPage,
                               &display_in_new_tab_page_)) {
      *error = base::ASCIIToUTF16(errors::kInvalidDisplayInNewTabPage);
      return false;
    }
  } else {
    // Inherit default from display_in_launcher property.
    display_in_new_tab_page_ = display_in_launcher_;
  }
  return true;
}

bool Extension::LoadExtent(const char* key,
                           URLPatternSet* extent,
                           const char* list_error,
                           const char* value_error,
                           std::u16string* error) {
  const base::Value* temp_pattern_value = nullptr;
  if (!manifest_->Get(key, &temp_pattern_value))
    return true;

  const base::ListValue* pattern_list = nullptr;
  if (!temp_pattern_value->GetAsList(&pattern_list)) {
    *error = base::ASCIIToUTF16(list_error);
    return false;
  }

  for (size_t i = 0; i < pattern_list->GetSize(); ++i) {
    std::string pattern_string;
    if (!pattern_list->GetString(i, &pattern_string)) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          value_error, base::NumberToString(i), errors::kExpectString);
      return false;
    }

    URLPattern pattern(kValidWebExtentSchemes);
    URLPattern::ParseResult parse_result = pattern.Parse(pattern_string);
    if (parse_result == URLPattern::ParseResult::kEmptyPath) {
      pattern_string += "/";
      parse_result = pattern.Parse(pattern_string);
    }

    if (parse_result != URLPattern::ParseResult::kSuccess) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          value_error, base::NumberToString(i),
          URLPattern::GetParseResultString(parse_result));
      return false;
    }

    // Do not allow authors to claim "<all_urls>".
    if (pattern.match_all_urls()) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          value_error, base::NumberToString(i),
          errors::kCannotClaimAllURLsInExtent);
      return false;
    }

    // Do not allow authors to claim "*" for host.
    if (pattern.host().empty()) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          value_error, base::NumberToString(i),
          errors::kCannotClaimAllHostsInExtent);
      return false;
    }

    // We do not allow authors to put wildcards in their paths. Instead, we
    // imply one at the end.
    if (pattern.path().find('*') != std::string::npos) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          value_error, base::NumberToString(i), errors::kNoWildCardsInPaths);
      return false;
    }
    pattern.SetPath(pattern.path() + '*');

    extent->AddPattern(pattern);
  }

  return true;
}

bool Extension::LoadSharedFeatures(std::u16string* error) {
  if (!LoadDescription(error) ||
      !ManifestHandler::ParseExtension(this, error) ||
      !LoadShortName(error))
    return false;

  return true;
}

bool Extension::LoadDescription(std::u16string* error) {
  if (manifest_->HasKey(keys::kDescription) &&
      !manifest_->GetString(keys::kDescription, &description_)) {
    *error = base::ASCIIToUTF16(errors::kInvalidDescription);
    return false;
  }
  return true;
}

bool Extension::LoadManifestVersion(std::u16string* error) {
  // Get the original value out of the dictionary so that we can validate it
  // more strictly.
  bool key_exists =
      manifest_->available_values().HasKey(keys::kManifestVersion);
  if (key_exists) {
    int manifest_version = 1;
    if (!manifest_->GetInteger(keys::kManifestVersion, &manifest_version)) {
      *error = InvalidManifestVersionError(
          errors::kInvalidManifestVersionUnsupported, is_platform_app());
      return false;
    }
  }

  manifest_version_ = manifest_->manifest_version();
  std::string warning;
  if (!IsManifestSupported(manifest_version_, GetType(), creation_flags_,
                           &warning)) {
    std::string json;
    base::JSONWriter::Write(*manifest_->value(), &json);
    LOG(WARNING) << "Failed to load extension.  Manifest JSON: " << json;
    *error = InvalidManifestVersionError(
        key_exists ? errors::kInvalidManifestVersionUnsupported
                   : errors::kInvalidManifestVersionMissingKey,
        is_platform_app());
    return false;
  }

  if (!warning.empty())
    AddInstallWarning(InstallWarning(warning, keys::kManifestVersion));

  return true;
}

bool Extension::LoadShortName(std::u16string* error) {
  if (manifest_->HasKey(keys::kShortName)) {
    std::u16string localized_short_name;
    if (!manifest_->GetString(keys::kShortName, &localized_short_name) ||
        localized_short_name.empty()) {
      *error = base::ASCIIToUTF16(errors::kInvalidShortName);
      return false;
    }

    base::i18n::AdjustStringForLocaleDirection(&localized_short_name);
    short_name_ = base::UTF16ToUTF8(localized_short_name);
  } else {
    short_name_ = display_name_;
  }
  return true;
}

ExtensionInfo::ExtensionInfo(const base::DictionaryValue* manifest,
                             const std::string& id,
                             const base::FilePath& path,
                             ManifestLocation location)
    : extension_id(id), extension_path(path), extension_location(location) {
  if (manifest)
    extension_manifest = manifest->CreateDeepCopy();
}

ExtensionInfo::~ExtensionInfo() {}

UpdatedExtensionPermissionsInfo::UpdatedExtensionPermissionsInfo(
    const Extension* extension,
    const PermissionSet& permissions,
    Reason reason)
    : reason(reason), extension(extension), permissions(permissions) {}

}   // namespace extensions
