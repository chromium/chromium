// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/features/sync_os_state_api_bindings.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "gin/arguments.h"
#include "gin/converter.h"
#include "ui/base/l10n/l10n_util.h"
#include "v8/include/v8-function-callback.h"
#include "v8/include/v8-value.h"

namespace {
constexpr char kUndeterminedLocale[] = "und";
}  // namespace

namespace ax {

std::string GetDisplayNameForLocale(const std::string& locale,
                                    const std::string& display_locale) {
  bool found_valid_result = false;
  std::string locale_result;
  if (l10n_util::IsValidLocaleSyntax(locale) &&
      l10n_util::IsValidLocaleSyntax(display_locale)) {
    locale_result = base::UTF16ToUTF8(l10n_util::GetDisplayNameForLocale(
        locale, display_locale, /*is_ui=*/true));
    // Check for valid locales before getting the display name.
    // The ICU Locale class returns "und" for undetermined locales, and
    // returns the locale string directly if it has no translation.
    // Treat these cases as invalid results.
    found_valid_result =
        locale_result != kUndeterminedLocale && locale_result != locale;
  }

  // We return an empty string to communicate that we could not determine the
  // display name.
  if (!found_valid_result) {
    locale_result = std::string();
  }

  return locale_result;
}

}  // namespace ax
