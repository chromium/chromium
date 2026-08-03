// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/extension_l10n_util.h"

#include <stddef.h>

#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/auto_reset.h"
#include "base/containers/extend.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/i18n/icubridge/supported_locales.h"
#include "base/i18n/language_tag.h"
#include "base/i18n/tag_converters.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/message_bundle.h"
#include "extensions/common/utils/base_string.h"
#include "third_party/icu/source/common/unicode/uloc.h"
#include "third_party/zlib/google/compression_utils.h"
#include "ui/base/l10n/l10n_util.h"

namespace errors = extensions::manifest_errors;
namespace keys = extensions::manifest_keys;

namespace {

using ::base::i18n::GetKnownLanguageTag;
using ::base::i18n::LanguageTag;
using ::base::i18n::LanguageTagConverter;

bool g_allow_gzipped_messages_for_test = false;

// Loads contents of the messages file for given locale. If file is not found,
// or there was parsing error we return null and set |error|. If
// |gzip_permission| is kAllowForTrustedSource, this will also look for a .gz
// version of the file and if found will decompresses it into a string first.
std::optional<base::DictValue> LoadMessageFile(
    const base::FilePath& locale_path,
    const LanguageTag& locale,
    std::string* error,
    extension_l10n_util::GzippedMessagesPermission gzip_permission) {
  base::FilePath file_path = locale_path.AppendASCII(locale.ToLegacyICUFormat())
                                 .Append(extensions::kMessagesFilename);

  std::optional<base::DictValue> dictionary;
  if (base::PathExists(file_path)) {
    JSONFileValueDeserializer messages_deserializer(file_path);
    std::unique_ptr<base::Value> value =
        messages_deserializer.Deserialize(nullptr, error);
    if (value) {
      dictionary = std::move(*value).TakeDict();
    }
  } else if (gzip_permission == extension_l10n_util::GzippedMessagesPermission::
                                    kAllowForTrustedSource ||
             g_allow_gzipped_messages_for_test) {
    // If a compressed version of the file exists, load that.
    base::FilePath compressed_file_path =
        file_path.AddExtension(FILE_PATH_LITERAL(".gz"));
    if (base::PathExists(compressed_file_path)) {
      std::string compressed_data;
      if (!base::ReadFileToString(compressed_file_path, &compressed_data)) {
        *error = base::StringPrintf("Failed to read compressed locale %s.",
                                    locale.tag_string());
        return dictionary;
      }
      std::string data;
      if (!compression::GzipUncompress(compressed_data, &data)) {
        *error = base::StringPrintf("Failed to decompress locale %s.",
                                    locale.tag_string());
        return dictionary;
      }
      base::JSONReader::Result value =
          base::JSONReader::ReadAndReturnValueWithError(
              data, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
      if (value.has_value()) {
        dictionary = std::move(*value).TakeDict();
      } else {
        *error = value.error().message;
      }
    }
  } else {
    LOG(ERROR) << "Unable to load message file: " << locale_path.AsUTF8Unsafe();
  }

  if (!dictionary) {
    if (error->empty()) {
      // JSONFileValueSerializer just returns null if file cannot be read. It
      // doesn't set the error, so we have to do it.
      *error = base::StringPrintf("Catalog file is missing for locale %s.",
                                  locale.tag_string());
    } else {
      *error = extensions::ErrorUtils::FormatErrorMessage(
          errors::kLocalesInvalidLocale,
          base::UTF16ToUTF8(file_path.LossyDisplayName()), *error);
    }
  }

  return dictionary;
}

// Localizes manifest value of string type for a given key.
bool LocalizeManifestValue(const std::string& key,
                           const extensions::MessageBundle& messages,
                           base::DictValue* manifest,
                           std::string* error) {
  std::string* result = manifest->FindStringByDottedPath(key);
  if (!result)
    return true;

  if (!messages.ReplaceMessages(result, error))
    return false;

  manifest->SetByDottedPath(key, *result);
  return true;
}

// Localizes manifest value of list type for a given key.
bool LocalizeManifestListValue(const std::string& key,
                               const extensions::MessageBundle& messages,
                               base::DictValue* manifest,
                               std::string* error) {
  base::ListValue* list_value = manifest->FindListByDottedPath(key);
  if (!list_value)
    return true;

  for (base::Value& item : *list_value) {
    if (item.is_string()) {
      std::string result = item.GetString();
      if (!messages.ReplaceMessages(&result, error)) {
        return false;
      }
      item = base::Value(result);
    }
  }
  return true;
}

std::optional<LanguageTag>& GetProcessLocale() {
  static base::NoDestructor<std::optional<LanguageTag>> process_locale;
  return *process_locale;
}

std::optional<LanguageTag>& GetPreferredLocale() {
  static base::NoDestructor<std::optional<LanguageTag>> preferred_locale;
  return *preferred_locale;
}

// Returns the desired locale to use for localization.
std::string LocaleForLocalization() {
  const std::optional<LanguageTag>& preferred_tag = GetPreferredLocale();
  std::string preferred_locale =
      preferred_tag ? preferred_tag->ToLegacyICUFormat() : "";
  if (!preferred_locale.empty())
    return preferred_locale;
  return extension_l10n_util::CurrentLocaleOrDefault();
}

}  // namespace

namespace extension_l10n_util {

GzippedMessagesPermission GetGzippedMessagesPermissionForExtension(
    const extensions::Extension* extension) {
  return extension
             ? GetGzippedMessagesPermissionForLocation(extension->location())
             : GzippedMessagesPermission::kDisallow;
}

GzippedMessagesPermission GetGzippedMessagesPermissionForLocation(
    extensions::mojom::ManifestLocation location) {
  // Component extensions are part of the chromium or chromium OS source and
  // as such are considered a trusted source.
  return location == extensions::mojom::ManifestLocation::kComponent
             ? GzippedMessagesPermission::kAllowForTrustedSource
             : GzippedMessagesPermission::kDisallow;
}

base::AutoReset<bool> AllowGzippedMessagesAllowedForTest() {
  return base::AutoReset<bool>(&g_allow_gzipped_messages_for_test, true);
}

void SetProcessLocale(const std::string& locale) {
  GetProcessLocale() = LanguageTagConverter::GetInstance().FromString(locale);
}

void SetPreferredLocale(const std::string& locale) {
  GetPreferredLocale() = LanguageTagConverter::GetInstance().FromString(locale);
}

std::string GetDefaultLocaleFromManifest(const base::DictValue& manifest,
                                         std::string* error) {
  if (const std::string* default_locale =
          manifest.FindString(keys::kDefaultLocale)) {
    return *default_locale;
  }

  *error = errors::kInvalidDefaultLocale;
  return std::string();
}

bool ShouldRelocalizeManifest(const base::DictValue& manifest) {
  if (!manifest.Find(keys::kDefaultLocale))
    return false;

  std::string manifest_current_locale;
  const std::string* manifest_current_locale_in =
      manifest.FindString(keys::kCurrentLocale);
  if (manifest_current_locale_in)
    manifest_current_locale = *manifest_current_locale_in;
  return manifest_current_locale != LocaleForLocalization();
}

bool LocalizeManifest(const extensions::MessageBundle& messages,
                      base::DictValue* manifest,
                      std::string* error) {
  // Initialize name.
  const std::string* result = manifest->FindString(keys::kName);
  if (!result) {
    *error = errors::kInvalidName;
    return false;
  }
  if (!LocalizeManifestValue(keys::kName, messages, manifest, error)) {
    return false;
  }

  // Initialize short name.
  if (!LocalizeManifestValue(keys::kShortName, messages, manifest, error))
    return false;

  // Initialize description.
  if (!LocalizeManifestValue(keys::kDescription, messages, manifest, error))
    return false;

  // Returns the key for the "default_title" entry in the given
  // `action_key`'s manifest entry.
  auto get_title_key = [](const char* action_key) {
    return base::StrCat({action_key, ".", keys::kActionDefaultTitle});
  };

  // Initialize browser_action.default_title
  if (!LocalizeManifestValue(get_title_key(keys::kBrowserAction), messages,
                             manifest, error)) {
    return false;
  }

  // Initialize page_action.default_title
  if (!LocalizeManifestValue(get_title_key(keys::kPageAction), messages,
                             manifest, error)) {
    return false;
  }

  // Initialize action.default_title
  if (!LocalizeManifestValue(get_title_key(keys::kAction), messages, manifest,
                             error)) {
    return false;
  }

  // Initialize omnibox.keyword.
  if (!LocalizeManifestValue(keys::kOmniboxKeyword, messages, manifest, error))
    return false;

  base::ListValue* file_handlers =
      manifest->FindListByDottedPath(keys::kFileBrowserHandlers);
  if (file_handlers) {
    for (base::Value& handler : *file_handlers) {
      base::DictValue* dict = handler.GetIfDict();
      if (!dict) {
        *error = errors::kInvalidFileBrowserHandler;
        return false;
      }
      if (!LocalizeManifestValue(keys::kActionDefaultTitle, messages, dict,
                                 error))
        return false;
    }
  }

  // Initialize all input_components
  base::ListValue* input_components =
      manifest->FindListByDottedPath(keys::kInputComponents);
  if (input_components) {
    for (base::Value& module : *input_components) {
      base::DictValue* dict = module.GetIfDict();
      if (!dict) {
        *error = errors::kInvalidInputComponents;
        return false;
      }
      if (!LocalizeManifestValue(keys::kName, messages, dict, error))
        return false;
      if (!LocalizeManifestValue(keys::kDescription, messages, dict, error))
        return false;
    }
  }

  // Initialize app.launch.local_path.
  if (!LocalizeManifestValue(keys::kLaunchLocalPath, messages, manifest, error))
    return false;

  // Initialize app.launch.web_url.
  if (!LocalizeManifestValue(keys::kLaunchWebURL, messages, manifest, error))
    return false;

  // Initialize description of commmands.
  base::DictValue* commands_handler =
      manifest->FindDictByDottedPath(keys::kCommands);
  if (commands_handler) {
    for (auto iter : *commands_handler) {
      std::string key =
          base::StringPrintf("commands.%s.description", iter.first.c_str());
      if (!LocalizeManifestValue(key, messages, manifest, error))
        return false;
    }
  }

  // Initialize search_provider fields.
  base::DictValue* search_provider =
      manifest->FindDictByDottedPath(keys::kOverrideSearchProvider);
  if (search_provider) {
    for (auto iter : *search_provider) {
      std::string key = base::StrCat(
          {keys::kOverrideSearchProvider, ".", iter.first.c_str()});
      bool success =
          (key == keys::kSettingsOverrideAlternateUrls)
              ? LocalizeManifestListValue(key, messages, manifest, error)
              : LocalizeManifestValue(key, messages, manifest, error);
      if (!success)
        return false;
    }
  }

  // Initialize chrome_settings_overrides.homepage.
  if (!LocalizeManifestValue(
          keys::kOverrideHomepage, messages, manifest, error))
    return false;

  // Initialize chrome_settings_overrides.startup_pages.
  if (!LocalizeManifestListValue(
          keys::kOverrideStartupPage, messages, manifest, error))
    return false;

  // Add desired locale key to the manifest, so we can overwrite prefs
  // with new manifest when chrome locale changes.
  manifest->Set(keys::kCurrentLocale, LocaleForLocalization());
  return true;
}

bool LocalizeExtension(const base::FilePath& extension_path,
                       base::DictValue* manifest,
                       GzippedMessagesPermission gzip_permission,
                       std::string* error) {
  DCHECK(manifest);

  std::string default_locale = GetDefaultLocaleFromManifest(*manifest, error);

  std::unique_ptr<extensions::MessageBundle> message_bundle(
      extensions::file_util::LoadMessageBundle(extension_path, default_locale,
                                               gzip_permission, error));

  if (!message_bundle && !error->empty())
    return false;

  if (message_bundle && !LocalizeManifest(*message_bundle, manifest, error))
    return false;

  return true;
}

bool ShouldAddLocale(const base::FilePath& locale_folder,
                     const LanguageTag& locale_tag,
                     std::string* error) {
  if (!base::i18n::GetSupportedIcuLocales().contains(locale_tag)) {
    // Warn if there is an extension locale that's not in the Chrome list,
    // but don't fail.
    DLOG(WARNING) << base::StringPrintf("Supplied locale %s is not supported.",
                                        locale_tag.tag_string());
    return true;
  }
  // Check if messages file is actually present (but don't check content).
  if (!base::PathExists(locale_folder.Append(extensions::kMessagesFilename))) {
    *error = base::StringPrintf("Catalog file is missing for locale %s.",
                                locale_tag.tag_string());
    return false;
  }

  return true;
}

std::string CurrentLocaleOrDefault() {
  const std::optional<LanguageTag>& current_tag = GetProcessLocale();
  if (current_tag) {
    return current_tag->ToLegacyICUFormat();
  }
  return "en";
}

void GetAllLocales(std::set<std::string>* all_locales) {
  for (const LanguageTag& tag : base::i18n::GetSupportedIcuLocales()) {
    all_locales->emplace(tag.ToLegacyICUFormat());
  }
}

std::vector<LanguageTag> GetAllFallbackLocales(
    const LanguageTag& default_locale) {
  LanguageTag application_locale_tag =
      GetProcessLocale().value_or(GetKnownLanguageTag("en"));
  std::vector<LanguageTag> all_fallback_locales;

  // Use the preferred locale if available. Otherwise, fall back to the
  // application locale or the application locale's parent locales. Thus, a
  // preferred locale of "en_CA" with an application locale of "en_GB" will
  // first try to use an en_CA locale folder, followed by en_GB, followed by en.
  const std::optional<LanguageTag>& preferred_tag = GetPreferredLocale();
  if (preferred_tag && preferred_tag != default_locale &&
      *preferred_tag != application_locale_tag) {
    all_fallback_locales.push_back(*preferred_tag);
  }

  if (application_locale_tag != default_locale) {
    for (std::optional<LanguageTag> tag = application_locale_tag; tag;
         tag = tag->GetParentTag()) {
      all_fallback_locales.push_back(*tag);
    }
  }
  all_fallback_locales.push_back(default_locale);
  return all_fallback_locales;
}

std::set<LanguageTag> GetValidLocales(const base::FilePath& locale_path,
                                      std::string* error) {
  // Enumerate all supplied locales in the extension.
  base::FileEnumerator locales(
      locale_path, false, base::FileEnumerator::DIRECTORIES);
  base::FilePath locale_folder;
  std::set<LanguageTag> valid_locales;
  while (!(locale_folder = locales.Next()).empty()) {
    std::string locale_name = locale_folder.BaseName().MaybeAsASCII();
    if (locale_name.empty()) {
      NOTREACHED();  // Not ASCII.
    }
    // Locales that start with a "." are accepted but not included in
    // `valid_locales`.
    if (base::StartsWith(locale_name, ".")) {
      continue;
    }
    std::optional<LanguageTag> locale_tag =
        base::i18n::GetLanguageTagFromString(locale_name);
    if (!locale_tag.has_value()) {
      DLOG(WARNING) << "Supplied locale " << locale_name << " is invalid.";
      continue;
    }
    if (!base::i18n::GetSupportedIcuLocales().contains(*locale_tag)) {
      DLOG(WARNING) << "Supplied locale " << locale_tag->tag_string()
                    << " is not supported.";
      continue;
    }
    if (!ShouldAddLocale(locale_folder, *locale_tag, error)) {
      return {};
    }
    valid_locales.insert(*locale_tag);
  }

  if (valid_locales.empty()) {
    *error = errors::kLocalesNoValidLocaleNamesListed;
    return {};
  }

  return valid_locales;
}

extensions::MessageBundle* LoadMessageCatalogs(
    const base::FilePath& locale_path,
    const LanguageTag& default_locale,
    GzippedMessagesPermission gzip_permission,
    std::string* error) {
  std::vector<LanguageTag> all_fallback_locales =
      GetAllFallbackLocales(default_locale);

  extensions::MessageBundle::CatalogVector catalogs;
  for (const auto& fallback_locale : all_fallback_locales) {
    // Skip all parent locales that are not supplied.
    base::FilePath this_locale_path =
        locale_path.AppendASCII(fallback_locale.ToLegacyICUFormat());
    if (!base::PathExists(this_locale_path)) {
      continue;
    }
    std::optional<base::DictValue> catalog =
        LoadMessageFile(locale_path, fallback_locale, error, gzip_permission);
    if (!catalog.has_value()) {
      // If locale is valid, but messages.json is corrupted or missing, return
      // an error.
      return nullptr;
    }
    catalogs.push_back(std::move(*catalog));
  }

  return extensions::MessageBundle::Create(catalogs, error);
}

extensions::MessageBundle* LoadMessageCatalogs(
    const base::FilePath& locale_path,
    const std::string& default_locale,
    GzippedMessagesPermission gzip_permission,
    std::string* error) {
  std::optional<LanguageTag> default_locale_tag =
      base::i18n::GetLanguageTagFromString(default_locale);
  if (!default_locale_tag) {
    *error = "Invalid default locale";
    return nullptr;
  }
  return LoadMessageCatalogs(locale_path, *default_locale_tag, gzip_permission,
                             error);
}

bool ValidateExtensionLocales(const base::FilePath& extension_path,
                              const base::DictValue& manifest,
                              std::u16string* error) {
  // TODO(crbug.com/41317803): Continue removing std::string errors and
  // replacing with std::u16string.
  std::string utf8_error;
  std::string default_locale =
      GetDefaultLocaleFromManifest(manifest, &utf8_error);

  if (default_locale.empty()) {
    *error = base::UTF8ToUTF16(utf8_error);
    return true;
  }

  base::FilePath locale_path = extension_path.Append(extensions::kLocaleFolder);

  std::set<LanguageTag> valid_locales =
      GetValidLocales(locale_path, &utf8_error);
  // TODO(crbug.com/41317803): Continue removing std::string errors and
  // replacing with std::u16string.
  if (valid_locales.empty()) {
    *error = base::UTF8ToUTF16(utf8_error);
    return false;
  }

  // Load each available localization file and check for errors within. This
  // entire method only gets used when reloading unpacked or packing extensions.
  // Performance thus isn't of utmost importance here, but gathering all errors
  // in all languages at once provides a comprehensive view to extension devs.
  for (const auto& locale : valid_locales) {
    std::string locale_error;
    std::unique_ptr<extensions::MessageBundle> bundle(LoadMessageCatalogs(
        locale_path, locale, GzippedMessagesPermission::kDisallow,
        &locale_error));
    if (locale_error.empty()) {
      continue;
    }
    if (!utf8_error.empty()) {
      utf8_error += '\n';
    }
    base::FilePath file_path = locale_path.AppendASCII(locale.tag_string())
                                   .Append(extensions::kMessagesFilename);
    utf8_error.append(extensions::ErrorUtils::FormatErrorMessage(
        errors::kLocalesInvalidLocale,
        base::UTF16ToUTF8(file_path.LossyDisplayName()), locale_error));
  }

  if (!utf8_error.empty()) {
    *error = base::UTF8ToUTF16(utf8_error);
    return false;
  }

  return true;
}

bool ShouldSkipValidation(const base::FilePath& locales_path,
                          const base::FilePath& locale_path,
                          const std::set<std::string>& all_locales) {
  // Since we use this string as a key in a base::DictValue, be paranoid about
  // skipping any strings with '.'. This happens sometimes, for example with
  // '.svn' directories.
  base::FilePath relative_path;
  if (!locales_path.AppendRelativePath(locale_path, &relative_path)) {
    NOTREACHED();
  }
  std::string subdir = relative_path.MaybeAsASCII();
  if (subdir.empty())
    return true;  // Non-ASCII.

  if (subdir.contains('.'))
    return true;

  // On case-insensitive file systems we will load messages by matching them
  // with locale names (see LoadMessageCatalogs). Reversed comparison must still
  // work here, when we match locale name with file name.
  if (!extensions::ContainsStringIgnoreCaseASCII(all_locales, subdir))
    return true;

  return false;
}

ScopedLocaleForTest::ScopedLocaleForTest()
    : process_locale_(GetProcessLocale()),
      preferred_locale_(GetPreferredLocale()) {}

ScopedLocaleForTest::ScopedLocaleForTest(std::string_view locale)
    : ScopedLocaleForTest(locale, locale) {}

ScopedLocaleForTest::ScopedLocaleForTest(std::string_view process_locale,
                                         std::string_view preferred_locale)
    : ScopedLocaleForTest() {
  SetProcessLocale(std::string(process_locale));
  SetPreferredLocale(std::string(preferred_locale));
}

ScopedLocaleForTest::~ScopedLocaleForTest() {
  GetProcessLocale() = process_locale_;
  GetPreferredLocale() = preferred_locale_;
}

std::string GetPreferredLocaleForTest() {
  const std::optional<LanguageTag>& preferred_tag = GetPreferredLocale();
  return preferred_tag ? std::string(preferred_tag->tag_string()) : "";
}

}  // namespace extension_l10n_util
