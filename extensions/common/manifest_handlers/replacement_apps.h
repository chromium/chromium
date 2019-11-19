// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_HANDLERS_REPLACEMENT_APPS_H_
#define EXTENSIONS_COMMON_MANIFEST_HANDLERS_REPLACEMENT_APPS_H_

#include "base/strings/string16.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"

class GURL;

namespace extensions {

// A structure to hold replacement apps that may be specified in the
// manifest of an extension using the "replacement_web_app"  or
// "replacement_android_app" keys.
struct ReplacementAppsInfo : public Extension::ManifestData {
 public:
  ReplacementAppsInfo();
  ~ReplacementAppsInfo() override;

  // Returns true if the |extension| has a replacement web app.
  static bool HasReplacementWebApp(const Extension* extension);

  // Returns the replacement web app for |extension|.
  static GURL GetReplacementWebApp(const Extension* extension);

  // Returns true if the |extension| has a replacement android app.
  static bool HasReplacementAndroidApp(const Extension* extension);

  // Returns the replacement android app package name for |extension|.
  static const std::string& GetReplacementAndroidApp(
      const Extension* extension);

  bool Parse(const Extension* extension, base::string16* error);

 private:
  bool LoadWebApp(const Extension* extension, base::string16* error);
  bool LoadAndroidApp(const Extension* extension, base::string16* error);

  // Optional URL of a Web app.
  GURL replacement_web_app;

  // Optional package name of an Android app.
  std::string replacement_android_app;

  DISALLOW_COPY_AND_ASSIGN(ReplacementAppsInfo);
};

// Parses the "replacement_web_app" and "replacement_android_app" manifest keys.
class ReplacementAppsHandler : public ManifestHandler {
 public:
  ReplacementAppsHandler();
  ~ReplacementAppsHandler() override;

  bool Parse(Extension* extension, base::string16* error) override;

 private:
  base::span<const char* const> Keys() const override;

  DISALLOW_COPY_AND_ASSIGN(ReplacementAppsHandler);
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_HANDLERS_REPLACEMENT_APPS_H_
