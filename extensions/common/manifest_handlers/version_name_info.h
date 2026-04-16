// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_HANDLERS_VERSION_NAME_INFO_H_
#define EXTENSIONS_COMMON_MANIFEST_HANDLERS_VERSION_NAME_INFO_H_

#include <string>

#include "base/containers/span.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"

namespace extensions {

struct VersionNameInfo : public Extension::ManifestData {
  explicit VersionNameInfo(const std::string& version_name);
  ~VersionNameInfo() override;

  // Return the `extension` attribute "version_name".
  static const std::string& GetVersionName(const Extension& extension);

 private:
  // The extension's user visible version name.
  const std::string version_name_;
};

// Parses the "version_name" manifest key.
class VersionNameHandler : public ManifestHandler {
 public:
  VersionNameHandler();

  VersionNameHandler(const VersionNameHandler&) = delete;
  VersionNameHandler& operator=(const VersionNameHandler&) = delete;

  ~VersionNameHandler() override;

  bool Parse(Extension* extension, std::u16string* error) override;

 private:
  base::span<const char* const> Keys() const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_HANDLERS_VERSION_NAME_INFO_H_
