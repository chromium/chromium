// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_HANDLERS_AUTOMATION_H_
#define EXTENSIONS_COMMON_MANIFEST_HANDLERS_AUTOMATION_H_

#include <memory>
#include <string>

#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"
#include "extensions/common/user_script.h"

namespace extensions {

namespace api::extensions_manifest_types {
struct Automation;
}

class AutomationManifestPermission;

// The parsed form of the automation manifest entry.
struct AutomationInfo : public Extension::ManifestData {
 public:
  static const AutomationInfo* Get(const Extension* extension);
  static std::unique_ptr<AutomationInfo> FromValue(
      const base::Value& value,
      std::vector<InstallWarning>* install_warnings,
      std::u16string* error);

  static std::unique_ptr<base::Value> ToValue(const AutomationInfo& info);

  AutomationInfo(const AutomationInfo&) = delete;
  AutomationInfo& operator=(const AutomationInfo&) = delete;

  ~AutomationInfo() override;

  // true if the extension has requested 'desktop' permission.
  const bool desktop;

 private:
  AutomationInfo();
  explicit AutomationInfo(bool desktop);

  static std::unique_ptr<api::extensions_manifest_types::Automation>
  AsManifestType(const AutomationInfo& info);
  friend class AutomationManifestPermission;
  friend class AutomationHandler;
};

// Parses the automation manifest entry.
class AutomationHandler : public ManifestHandler {
 public:
  AutomationHandler();

  AutomationHandler(const AutomationHandler&) = delete;
  AutomationHandler& operator=(const AutomationHandler&) = delete;

  ~AutomationHandler() override;

 private:
  // ManifestHandler implementation.
  bool Parse(Extension* extensions, std::u16string* error) override;

  ManifestPermission* CreatePermission() override;
  ManifestPermission* CreateInitialRequiredPermission(
      const Extension* extension) override;
  base::span<const char* const> Keys() const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_HANDLERS_AUTOMATION_H_
