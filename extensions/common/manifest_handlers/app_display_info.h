// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_HANDLERS_APP_DISPLAY_INFO_H_
#define EXTENSIONS_COMMON_MANIFEST_HANDLERS_APP_DISPLAY_INFO_H_

#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"

namespace extensions {

// Stores data about app display info.
struct AppDisplayInfo : public Extension::ManifestData {
 public:
  AppDisplayInfo(bool display_in_launcher, bool display_in_new_tab_page);

  AppDisplayInfo(const AppDisplayInfo&) = delete;
  AppDisplayInfo& operator=(const AppDisplayInfo&) = delete;

  ~AppDisplayInfo() override;

  // Returns true if the extension requires a valid ordinal for sorting, e.g.,
  // for displaying in a launcher or new tab page.
  static bool RequiresSortOrdinal(const Extension& extension);

  // Returns true if the extension should be displayed in the app launcher.
  static bool ShouldDisplayInAppLauncher(const Extension& extension);

  // Returns true if the extension should be displayed in the browser NTP.
  static bool ShouldDisplayInNewTabPage(const Extension& extension);

 private:
  // Whether this app should be shown in the app launcher.
  const bool display_in_launcher_;

  // Whether this app be shown in the browser New Tab Page.
  const bool display_in_new_tab_page_;
};

// Parses the relevant keys in the manifest for app display preferences.
class AppDisplayManifestHandler : public ManifestHandler {
 public:
  AppDisplayManifestHandler();

  AppDisplayManifestHandler(const AppDisplayManifestHandler&) = delete;
  AppDisplayManifestHandler& operator=(const AppDisplayManifestHandler&) =
      delete;

  ~AppDisplayManifestHandler() override;

  // ManifestHandler:
  bool Parse(Extension* extension, std::u16string* error) override;

 private:
  // ManifestHandler:
  base::span<const char* const> Keys() const override;
  bool AlwaysParseForType(Manifest::Type type) const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_HANDLERS_APP_DISPLAY_INFO_H_
