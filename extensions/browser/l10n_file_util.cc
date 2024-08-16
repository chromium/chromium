// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/l10n_file_util.h"

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "extensions/common/file_util.h"

namespace extensions::l10n_file_util {

std::unique_ptr<MessageBundle::SubstitutionMap>
LoadMessageBundleSubstitutionMap(
    const base::FilePath& extension_path,
    const ExtensionId& extension_id,
    const std::string& default_locale,
    extension_l10n_util::GzippedMessagesPermission gzip_permission) {
  return LoadMessageBundleSubstitutionMapFromPaths(
      {extension_path}, extension_id, default_locale, gzip_permission);
}

std::unique_ptr<MessageBundle::SubstitutionMap>
LoadNonLocalizedMessageBundleSubstitutionMap(const ExtensionId& extension_id) {
  auto return_value = std::make_unique<MessageBundle::SubstitutionMap>();

  // Add @@extension_id reserved message here.
  return_value->insert(
      std::make_pair(MessageBundle::kExtensionIdKey, extension_id));

  return return_value;
}

std::unique_ptr<MessageBundle::SubstitutionMap>
LoadMessageBundleSubstitutionMapFromPaths(
    const std::vector<base::FilePath>& paths,
    const ExtensionId& extension_id,
    const std::string& default_locale,
    extension_l10n_util::GzippedMessagesPermission gzip_permission) {
  std::unique_ptr<MessageBundle::SubstitutionMap> return_value =
      LoadNonLocalizedMessageBundleSubstitutionMap(extension_id);

  // Touch disk only if extension is localized.
  if (default_locale.empty()) {
    return return_value;
  }

  std::string error;
  for (const base::FilePath& path : paths) {
    std::unique_ptr<MessageBundle> bundle(file_util::LoadMessageBundle(
        path, default_locale, gzip_permission, &error));
    if (bundle) {
      for (const auto& iter : *bundle->dictionary()) {
        // |insert| only adds new entries, and does not replace entries in
        // the main extension or previously processed imports.
        return_value->insert(std::make_pair(iter.first, iter.second));
      }
    }
  }

  return return_value;
}

}  // namespace extensions::l10n_file_util
