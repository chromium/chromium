// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/util/terms_util.h"

#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/application_context.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// English is the default locale for the Terms of Service.
const char kEnglishLocale[] = "en";
// The prefix of the Chrome Terms of Service file name.
const char kChromeTosFilePrefix[] = "terms";
// The file name extension for HTML files.
const char kHtmlFileExtension[] = "html";

// Checks that the requested file exists in the application's resource
// bundle and returns the filename. Returns an empty string if resource does not
// exist.
std::string FindFileInResource(const std::string& base_name,
                               const std::string& language,
                               const std::string& ext) {
  std::string resource_file(base_name + "_" + language);
  BOOL exists = [base::mac::FrameworkBundle()
                    URLForResource:base::SysUTF8ToNSString(resource_file)
                     withExtension:base::SysUTF8ToNSString(ext)] != nil;
  return exists ? resource_file + "." + ext : std::string();
}

}  // namespace

// iOS uses certain locale initials that are different from Chrome's.
// Most notably, "pt" means "pt-BR" (Brazillian Portuguese). This
// function normalizes the locale into language-region code used by
// Chrome.
std::string GetIOSLocaleMapping(const std::string& locale) {
  if (locale == "pt")  // Brazillian Portuguese
    return "pt-BR";
  else if (locale == "zh-Hans")  // Chinese Simplified script
    return "zh-CN";
  else if (locale == "zh-Hant")  // Chinese Traditional script
    return "zh-TW";
  else if (locale == "es-MX")
    return "es-419";
  return locale;
}

// Returns a filename based on the base file name and file extension and
// localized for the given locale. Checks the existence of the file based on
// |locale| as follows:
//   * if there is a file for file_<locale>.<ext>
//   * if not, check the language part as follows file_<locale[0..]>.<ext>
//   * when all else fails, use the English locale, e.g. file_en.<ext>, which
//     must exist.
// |locale| must be a valid Chrome locale as defined in file
// ui/base/l10n/l10n_util.cc. This corresponds to a language or a country code,
// e.g. "en", "en-US", "fr", etc.
std::string GetLocalizedFileName(const std::string& base_name,
                                 const std::string& locale,
                                 const std::string& ext) {
  std::string mappedLocale = GetIOSLocaleMapping(locale);
  std::string resource_file = FindFileInResource(base_name, mappedLocale, ext);
  if (resource_file.empty() && mappedLocale.length() > 2 &&
      mappedLocale[2] == '-') {
    std::string language = mappedLocale.substr(0, 2);
    resource_file = FindFileInResource(base_name, language, ext);
  }
  if (resource_file.empty() && mappedLocale.length() > 3 &&
      mappedLocale[3] == '-') {
    std::string language = mappedLocale.substr(0, 3);
    resource_file = FindFileInResource(base_name, language, ext);
  }
  if (resource_file.empty()) {
    // Default to English if resource is still not found.
    resource_file = FindFileInResource(base_name, kEnglishLocale, ext);
  }
  DCHECK(!resource_file.empty());
  return resource_file;
}

std::string GetTermsOfServicePath() {
  const std::string& locale = GetApplicationContext()->GetApplicationLocale();
  return GetLocalizedFileName(kChromeTosFilePrefix, locale, kHtmlFileExtension);
}
