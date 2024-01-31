// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_URL_HANDLERS_H_
#define EXTENSIONS_COMMON_MANIFEST_URL_HANDLERS_H_

#include <string>
#include <vector>

#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"

namespace extensions {

// A structure to hold various URLs like devtools_page, homepage_url, etc
// that may be specified in the manifest of an extension.
struct ManifestURL : public Extension::ManifestData {
  GURL url_;

  // Returns the value of a URL key for an extension, or an empty URL if unset.
  static const GURL& Get(const Extension* extension, const std::string& key);

  // Returns the Homepage URL for this extension.
  // If homepage_url was not specified in the manifest,
  // this returns the Google Gallery URL. For third-party extensions,
  // this returns a blank GURL.
  // See also: GetManifestHomePageURL(), SpecifiedHomepageURL()
  static GURL GetHomepageURL(const Extension* extension);

  // Returns true if the extension specified a valid home page url in the
  // manifest.
  static bool SpecifiedHomepageURL(const Extension* extension);

  // Returns the homepage specified by the extension in its manifest, if it
  // specifies a homepage. Otherwise, returns an empty url.
  // See also: GetHomepageURL()
  static const GURL& GetManifestHomePageURL(const Extension* extension);

  // Returns the Chrome Web Store URL for this extension if it is hosted in the
  // webstore; otherwise returns an empty url.
  // See also: GetHomepageURL()
  static GURL GetWebStoreURL(const Extension* extension);

  // Returns the Update URL for this extension.
  static const GURL& GetUpdateURL(const Extension* extension);

  // Returns true if this extension's update URL is the extension gallery.
  static bool UpdatesFromGallery(const Extension* extension);

  // Returns the About Page for this extension.
  static const GURL& GetAboutPage(const Extension* extension);

  // Returns the webstore page URL for this extension.
  static GURL GetDetailsURL(const Extension* extension);
};

// Parses the "homepage_url" manifest key.
class HomepageURLHandler : public ManifestHandler {
 public:
  HomepageURLHandler();

  HomepageURLHandler(const HomepageURLHandler&) = delete;
  HomepageURLHandler& operator=(const HomepageURLHandler&) = delete;

  ~HomepageURLHandler() override;

  bool Parse(Extension* extension, std::u16string* error) override;

 private:
  base::span<const char* const> Keys() const override;
};

// Parses the "update_url" manifest key.
class UpdateURLHandler : public ManifestHandler {
 public:
  UpdateURLHandler();

  UpdateURLHandler(const UpdateURLHandler&) = delete;
  UpdateURLHandler& operator=(const UpdateURLHandler&) = delete;

  ~UpdateURLHandler() override;

  bool Parse(Extension* extension, std::u16string* error) override;

 private:
  base::span<const char* const> Keys() const override;
};

// Parses the "about_page" manifest key.
// TODO(sashab): Make this and any other similar handlers extend from the same
// abstract class, URLManifestHandler, which has pure virtual methods for
// detecting the required URL type (relative or absolute) and abstracts the
// URL parsing logic away.
class AboutPageHandler : public ManifestHandler {
 public:
  AboutPageHandler();

  AboutPageHandler(const AboutPageHandler&) = delete;
  AboutPageHandler& operator=(const AboutPageHandler&) = delete;

  ~AboutPageHandler() override;

  bool Parse(Extension* extension, std::u16string* error) override;
  bool Validate(const Extension* extension,
                std::string* error,
                std::vector<InstallWarning>* warnings) const override;

 private:
  base::span<const char* const> Keys() const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_URL_HANDLERS_H_
