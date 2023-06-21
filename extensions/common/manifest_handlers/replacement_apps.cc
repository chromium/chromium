// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/replacement_apps.h"

#include <memory>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace extensions {

namespace keys = manifest_keys;
namespace errors = manifest_errors;

namespace {

const char kReplacementApps[] = "replacement_apps";

const ReplacementAppsInfo* GetReplacementAppsInfo(const Extension* extension) {
  return static_cast<ReplacementAppsInfo*>(
      extension->GetManifestData(kReplacementApps));
}

}  // namespace

ReplacementAppsInfo::ReplacementAppsInfo() = default;

ReplacementAppsInfo::~ReplacementAppsInfo() = default;

// static
bool ReplacementAppsInfo::HasReplacementWebApp(const Extension* extension) {
  const ReplacementAppsInfo* info = GetReplacementAppsInfo(extension);
  return info && !info->replacement_web_app.is_empty();
}

// static
GURL ReplacementAppsInfo::GetReplacementWebApp(const Extension* extension) {
  const ReplacementAppsInfo* info = GetReplacementAppsInfo(extension);

  if (info && !info->replacement_web_app.is_empty()) {
    return info->replacement_web_app;
  }

  return GURL();
}

bool ReplacementAppsInfo::LoadWebApp(const Extension* extension,
                                     std::u16string* error) {
  const base::Value* app_value =
      extension->manifest()->FindPath(keys::kReplacementWebApp);
  if (app_value == nullptr) {
    return true;
  }

  DCHECK(app_value);
  if (!app_value->is_string()) {
    *error = errors::kInvalidReplacementWebApp;
    return false;
  }

  const GURL web_app_url(app_value->GetString());
  if (!web_app_url.is_valid() || !web_app_url.SchemeIs(url::kHttpsScheme)) {
    *error = errors::kInvalidReplacementWebApp;
    return false;
  }

  replacement_web_app = std::move(web_app_url);
  return true;
}

bool ReplacementAppsInfo::Parse(const Extension* extension,
                                std::u16string* error) {
  if (!LoadWebApp(extension, error)) {
    return false;
  }
  return true;
}

ReplacementAppsHandler::ReplacementAppsHandler() = default;

ReplacementAppsHandler::~ReplacementAppsHandler() = default;

bool ReplacementAppsHandler::Parse(Extension* extension,
                                   std::u16string* error) {
  std::unique_ptr<ReplacementAppsInfo> info(new ReplacementAppsInfo);

  if (!info->Parse(extension, error)) {
    return false;
  }

  extension->SetManifestData(kReplacementApps, std::move(info));
  return true;
}

base::span<const char* const> ReplacementAppsHandler::Keys() const {
  static constexpr const char* kKeys[] = {
      keys::kReplacementWebApp,
  };
  return kKeys;
}

}  // namespace extensions
