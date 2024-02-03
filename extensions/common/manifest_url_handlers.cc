// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_url_handlers.h"

#include <memory>
#include <utility>

#include "base/files/file_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/shared_module_info.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

namespace keys = manifest_keys;
namespace errors = manifest_errors;

// static
const GURL& ManifestURL::Get(const Extension* extension,
                             const std::string& key) {
  ManifestURL* manifest_url =
      static_cast<ManifestURL*>(extension->GetManifestData(key));
  return manifest_url ? manifest_url->url_ : GURL::EmptyGURL();
}

// static
GURL ManifestURL::GetHomepageURL(const Extension* extension) {
  const GURL& homepage_url = Get(extension, keys::kHomepageURL);
  return homepage_url.is_valid() ? homepage_url : GetWebStoreURL(extension);
}

// static
bool ManifestURL::SpecifiedHomepageURL(const Extension* extension) {
  return Get(extension, keys::kHomepageURL).is_valid();
}

// static
const GURL& ManifestURL::GetManifestHomePageURL(const Extension* extension) {
  const GURL& homepage_url = Get(extension, keys::kHomepageURL);
  return homepage_url.is_valid() ? homepage_url : GURL::EmptyGURL();
}

// static
GURL ManifestURL::GetWebStoreURL(const Extension* extension) {
  bool use_webstore_url = UpdatesFromGallery(extension) &&
                          !SharedModuleInfo::IsSharedModule(extension);
  return use_webstore_url
             ? GURL(extension_urls::GetWebstoreItemDetailURLPrefix() +
                    extension->id())
             : GURL();
}

// static
const GURL& ManifestURL::GetUpdateURL(const Extension* extension) {
  return Get(extension, keys::kUpdateURL);
}

// static
bool ManifestURL::UpdatesFromGallery(const Extension* extension) {
  return extension_urls::IsWebstoreUpdateUrl(GetUpdateURL(extension));
}

// static
const GURL& ManifestURL::GetAboutPage(const Extension* extension) {
  return Get(extension, keys::kAboutPage);
}

// static
GURL ManifestURL::GetDetailsURL(const Extension* extension) {
  return extension->from_webstore()
             ? GURL(extension_urls::GetWebstoreItemDetailURLPrefix() +
                    extension->id())
             : GURL();
}

HomepageURLHandler::HomepageURLHandler() {
}

HomepageURLHandler::~HomepageURLHandler() {
}

bool HomepageURLHandler::Parse(Extension* extension, std::u16string* error) {
  std::unique_ptr<ManifestURL> manifest_url(new ManifestURL);
  const std::string* homepage_url_str =
      extension->manifest()->FindStringPath(keys::kHomepageURL);
  if (homepage_url_str == nullptr) {
    *error = ErrorUtils::FormatErrorMessageUTF16(errors::kInvalidHomepageURL,
                                                 std::string());
    return false;
  }
  manifest_url->url_ = GURL(*homepage_url_str);
  if (!manifest_url->url_.is_valid() ||
      !manifest_url->url_.SchemeIsHTTPOrHTTPS()) {
    *error = ErrorUtils::FormatErrorMessageUTF16(errors::kInvalidHomepageURL,
                                                 *homepage_url_str);
    return false;
  }
  extension->SetManifestData(keys::kHomepageURL, std::move(manifest_url));
  return true;
}

base::span<const char* const> HomepageURLHandler::Keys() const {
  static constexpr const char* kKeys[] = {keys::kHomepageURL};
  return kKeys;
}

UpdateURLHandler::UpdateURLHandler() {
}

UpdateURLHandler::~UpdateURLHandler() {
}

bool UpdateURLHandler::Parse(Extension* extension, std::u16string* error) {
  std::unique_ptr<ManifestURL> manifest_url(new ManifestURL);

  const std::string* tmp_update_url =
      extension->manifest()->FindStringPath(keys::kUpdateURL);
  if (tmp_update_url == nullptr) {
    *error = ErrorUtils::FormatErrorMessageUTF16(errors::kInvalidUpdateURL,
                                                 std::string());
    return false;
  }

  manifest_url->url_ = GURL(*tmp_update_url);
  if (!manifest_url->url_.is_valid() ||
      manifest_url->url_.has_ref()) {
    *error = ErrorUtils::FormatErrorMessageUTF16(errors::kInvalidUpdateURL,
                                                 *tmp_update_url);
    return false;
  }

  extension->SetManifestData(keys::kUpdateURL, std::move(manifest_url));
  return true;
}

base::span<const char* const> UpdateURLHandler::Keys() const {
  static constexpr const char* kKeys[] = {keys::kUpdateURL};
  return kKeys;
}

AboutPageHandler::AboutPageHandler() {
}

AboutPageHandler::~AboutPageHandler() {
}

bool AboutPageHandler::Parse(Extension* extension, std::u16string* error) {
  std::unique_ptr<ManifestURL> manifest_url(new ManifestURL);
  const std::string* about_str =
      extension->manifest()->FindStringPath(keys::kAboutPage);
  if (about_str == nullptr) {
    *error = errors::kInvalidAboutPage;
    return false;
  }

  GURL absolute(*about_str);
  if (absolute.is_valid()) {
    *error = errors::kInvalidAboutPageExpectRelativePath;
    return false;
  }
  manifest_url->url_ = extension->GetResourceURL(*about_str);
  if (!manifest_url->url_.is_valid()) {
    *error = errors::kInvalidAboutPage;
    return false;
  }
  extension->SetManifestData(keys::kAboutPage, std::move(manifest_url));
  return true;
}

bool AboutPageHandler::Validate(const Extension* extension,
                                std::string* error,
                                std::vector<InstallWarning>* warnings) const {
  // Validate path to the options page.
  if (!extensions::ManifestURL::GetAboutPage(extension).is_empty()) {
    const base::FilePath about_path =
        extensions::file_util::ExtensionURLToRelativeFilePath(
            extensions::ManifestURL::GetAboutPage(extension));
    const base::FilePath path =
        extension->GetResource(about_path).GetFilePath();
    if (path.empty() || !base::PathExists(path)) {
      *error = l10n_util::GetStringFUTF8(IDS_EXTENSION_LOAD_ABOUT_PAGE_FAILED,
                                         about_path.LossyDisplayName());
      return false;
    }
  }
  return true;
}

base::span<const char* const> AboutPageHandler::Keys() const {
  static constexpr const char* kKeys[] = {keys::kAboutPage};
  return kKeys;
}

}  // namespace extensions
