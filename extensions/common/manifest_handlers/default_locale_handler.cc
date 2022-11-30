// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/default_locale_handler.h"

#include <memory>

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_l10n_util.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

namespace keys = manifest_keys;
namespace errors = manifest_errors;

// static
const std::string& LocaleInfo::GetDefaultLocale(const Extension* extension) {
  LocaleInfo* info = static_cast<LocaleInfo*>(
      extension->GetManifestData(keys::kDefaultLocale));
  return info ? info->default_locale : base::EmptyString();
}

DefaultLocaleHandler::DefaultLocaleHandler() {
}

DefaultLocaleHandler::~DefaultLocaleHandler() {
}

bool DefaultLocaleHandler::Parse(Extension* extension, std::u16string* error) {
  std::unique_ptr<LocaleInfo> info(new LocaleInfo);

  const std::string* default_locale =
      extension->manifest()->FindStringPath(keys::kDefaultLocale);
  if (default_locale == nullptr ||
      !l10n_util::IsValidLocaleSyntax(*default_locale)) {
    *error = manifest_errors::kInvalidDefaultLocale16;
    return false;
  }
  info->default_locale = *default_locale;

  extension->SetManifestData(keys::kDefaultLocale, std::move(info));
  return true;
}

bool DefaultLocaleHandler::Validate(
    const Extension* extension,
    std::string* error,
    std::vector<InstallWarning>* warnings) const {
  // default_locale and _locales have to be both present or both missing.
  const base::FilePath path = extension->path().Append(kLocaleFolder);
  bool path_exists = base::PathExists(path);
  std::string default_locale =
      extensions::LocaleInfo::GetDefaultLocale(extension);

  // If both default locale and _locales folder are empty, skip verification.
  if (default_locale.empty() && !path_exists)
    return true;

  if (default_locale.empty() && path_exists) {
    *error = l10n_util::GetStringUTF8(
        IDS_EXTENSION_LOCALES_NO_DEFAULT_LOCALE_SPECIFIED);
    return false;
  } else if (!default_locale.empty() && !path_exists) {
    *error = errors::kLocalesTreeMissing;
    return false;
  }

  // Treat all folders under _locales as valid locales.
  base::FileEnumerator locales(path, false, base::FileEnumerator::DIRECTORIES);

  std::set<std::string> all_locales;
  extension_l10n_util::GetAllLocales(&all_locales);
  const base::FilePath default_locale_path = path.AppendASCII(default_locale);
  bool has_default_locale_message_file = false;

  bool gzipped_messages_allowed =
      extension_l10n_util::GetGzippedMessagesPermissionForLocation(
          extension->location()) ==
      extension_l10n_util::GzippedMessagesPermission::kAllowForTrustedSource;

  base::FilePath locale_path;
  while (!(locale_path = locales.Next()).empty()) {
    if (extension_l10n_util::ShouldSkipValidation(path, locale_path,
                                                  all_locales))
      continue;

    base::FilePath messages_path = locale_path.Append(kMessagesFilename);
    base::FilePath gzipped_messages_path =
        locale_path.Append(kGzippedMessagesFilename);

    // Fail unless plain exists or (gzip allowed and gzip exists)
    if (!base::PathExists(messages_path) &&
        !(gzipped_messages_allowed &&
          base::PathExists(gzipped_messages_path))) {
      *error = base::StringPrintf(
          "%s %s",
          base::UTF16ToUTF8(errors::kLocalesMessagesFileMissing).c_str(),
          base::UTF16ToUTF8(messages_path.LossyDisplayName()).c_str());
      return false;
    }

    if (locale_path == default_locale_path)
      has_default_locale_message_file = true;
  }

  // Only message file for default locale has to exist.
  if (!has_default_locale_message_file) {
    *error = errors::kLocalesNoDefaultMessages;
    return false;
  }

  return true;
}

bool DefaultLocaleHandler::AlwaysValidateForType(Manifest::Type type) const {
  // Required to validate _locales directory; see Validate.
  return true;
}

base::span<const char* const> DefaultLocaleHandler::Keys() const {
  static constexpr const char* kKeys[] = {keys::kDefaultLocale};
  return kKeys;
}

}  // namespace extensions
