// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_HANDLERS_REPLACEMENT_APPS_H_
#define EXTENSIONS_COMMON_MANIFEST_HANDLERS_REPLACEMENT_APPS_H_

#include <string>

#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"

class GURL;

namespace extensions {

// A structure to hold replacement apps that may be specified in the
// manifest of an extension using the "replacement_web_app" key.
struct ReplacementAppsInfo : public Extension::ManifestData {
 public:
  ReplacementAppsInfo();

  ReplacementAppsInfo(const ReplacementAppsInfo&) = delete;
  ReplacementAppsInfo& operator=(const ReplacementAppsInfo&) = delete;

  ~ReplacementAppsInfo() override;

  // Returns true if the |extension| has a replacement web app.
  static bool HasReplacementWebApp(const Extension* extension);

  // Returns the replacement web app for |extension|.
  static GURL GetReplacementWebApp(const Extension* extension);

  bool Parse(const Extension* extension, std::u16string* error);

 private:
  bool LoadWebApp(const Extension* extension, std::u16string* error);

  // Optional URL of a Web app.
  GURL replacement_web_app;
};

// Parses the "replacement_web_app" manifest key.
class ReplacementAppsHandler : public ManifestHandler {
 public:
  ReplacementAppsHandler();

  ReplacementAppsHandler(const ReplacementAppsHandler&) = delete;
  ReplacementAppsHandler& operator=(const ReplacementAppsHandler&) = delete;

  ~ReplacementAppsHandler() override;

  bool Parse(Extension* extension, std::u16string* error) override;

 private:
  base::span<const char* const> Keys() const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_HANDLERS_REPLACEMENT_APPS_H_
