// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_HANDLERS_INCOGNITO_INFO_H_
#define EXTENSIONS_COMMON_MANIFEST_HANDLERS_INCOGNITO_INFO_H_

#include <string>

#include "extensions/common/api/incognito.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"

namespace extensions {

struct IncognitoInfo : public Extension::ManifestData {
  explicit IncognitoInfo(api::incognito::IncognitoMode mode);
  ~IncognitoInfo() override;

  api::incognito::IncognitoMode mode;

  // Return whether the |extension| should run in spanning incognito mode.
  static bool IsSpanningMode(const Extension* extension);

  // Return whether the |extension| should run in split incognito mode.
  static bool IsSplitMode(const Extension* extension);

  // Return whether this extension can be run in incognito mode as specified
  // in its manifest.
  static bool IsIncognitoAllowed(const Extension* extension);
};

// Parses the "incognito" manifest key.
class IncognitoHandler : public ManifestHandler {
 public:
  IncognitoHandler();

  IncognitoHandler(const IncognitoHandler&) = delete;
  IncognitoHandler& operator=(const IncognitoHandler&) = delete;

  ~IncognitoHandler() override;

  bool Parse(Extension* extension, std::u16string* error) override;
  bool AlwaysParseForType(Manifest::Type type) const override;

 private:
  base::span<const char* const> Keys() const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_HANDLERS_INCOGNITO_INFO_H_
