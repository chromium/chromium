// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_L10N_FILE_UTIL_H_
#define EXTENSIONS_BROWSER_L10N_FILE_UTIL_H_

#include <memory>
#include <string>
#include <vector>

#include "extensions/common/extension_id.h"
#include "extensions/common/message_bundle.h"

namespace base {
class FilePath;
}

namespace extension_l10n_util {
enum class GzippedMessagesPermission;
}

namespace extensions::l10n_file_util {

// Loads the extension message bundle substitution map. Contains at least
// the extension_id item. Does not supported compressed locale files. Passes
// |gzip_permission| to extension_l10n_util::LoadMessageCatalogs (see
// extension_l10n_util.h).
std::unique_ptr<MessageBundle::SubstitutionMap>
LoadMessageBundleSubstitutionMap(
    const base::FilePath& extension_path,
    const ExtensionId& extension_id,
    const std::string& default_locale,
    extension_l10n_util::GzippedMessagesPermission gzip_permission);

// Loads the extension message bundle substitution map for a non-localized
// extension. Contains only the extension_id item.
// This doesn't require hitting disk, so it's safe to call on any thread.
std::unique_ptr<MessageBundle::SubstitutionMap>
LoadNonLocalizedMessageBundleSubstitutionMap(const ExtensionId& extension_id);

// Loads the extension message bundle substitution map from the specified paths.
// Contains at least the extension_id item. Passes |gzip_permission| to
// extension_l10n_util::LoadMessageCatalogs (see extension_l10n_util.h).
std::unique_ptr<MessageBundle::SubstitutionMap>
LoadMessageBundleSubstitutionMapFromPaths(
    const std::vector<base::FilePath>& paths,
    const ExtensionId& extension_id,
    const std::string& default_locale,
    extension_l10n_util::GzippedMessagesPermission gzip_permission);

}  // namespace extensions::l10n_file_util

#endif  // EXTENSIONS_BROWSER_L10N_FILE_UTIL_H_
