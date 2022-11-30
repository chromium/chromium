// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_HANDLERS_APP_ISOLATION_INFO_H_
#define EXTENSIONS_COMMON_MANIFEST_HANDLERS_APP_ISOLATION_INFO_H_

#include <string>
#include <vector>

#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handler.h"

namespace extensions {

struct AppIsolationInfo : public Extension::ManifestData {
  explicit AppIsolationInfo(bool isolated_storage);
  ~AppIsolationInfo() override;

  static bool HasIsolatedStorage(const Extension* extension);

  // Whether this extension requests isolated storage.
  bool has_isolated_storage;
};

// Parses the "isolation" manifest key.
class AppIsolationHandler : public ManifestHandler {
 public:
  AppIsolationHandler();

  AppIsolationHandler(const AppIsolationHandler&) = delete;
  AppIsolationHandler& operator=(const AppIsolationHandler&) = delete;

  ~AppIsolationHandler() override;

  bool Parse(Extension* extension, std::u16string* error) override;
  bool AlwaysParseForType(Manifest::Type type) const override;

 private:
  base::span<const char* const> Keys() const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_HANDLERS_APP_ISOLATION_INFO_H_
