// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_HANDLERS_AUTOMATION_H_
#define EXTENSIONS_COMMON_MANIFEST_HANDLERS_AUTOMATION_H_

#include <memory>
#include <string>

#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"
#include "extensions/common/url_pattern_set.h"
#include "extensions/common/user_script.h"

namespace extensions {

namespace api::extensions_manifest_types {
struct Automation;
}

class URLPatternSet;
class AutomationManifestPermission;

namespace automation_errors {
extern const char kErrorInvalidMatchPattern[];
extern const char kErrorDesktopTrueInteractFalse[];
extern const char kErrorDesktopTrueMatchesSpecified[];
extern const char kErrorURLMalformed[];
extern const char kErrorInvalidMatch[];
extern const char kErrorNoMatchesProvided[];
}  // namespace automation_errors

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

  // Returns the list of hosts that this extension can request an automation
  // tree from.
  const URLPatternSet matches;

  // Whether the extension is allowed interactive access (true) or read-only
  // access (false) to the automation tree.
  const bool interact;

 private:
  AutomationInfo();
  AutomationInfo(bool desktop, const URLPatternSet& matches, bool interact);

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
