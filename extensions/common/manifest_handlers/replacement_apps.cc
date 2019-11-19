// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/replacement_apps.h"

#include <memory>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
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

ReplacementAppsInfo::ReplacementAppsInfo() {}

ReplacementAppsInfo::~ReplacementAppsInfo() {}

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

// static
bool ReplacementAppsInfo::HasReplacementAndroidApp(const Extension* extension) {
  const ReplacementAppsInfo* info = GetReplacementAppsInfo(extension);
  return info && !info->replacement_android_app.empty();
}

// static
const std::string& ReplacementAppsInfo::GetReplacementAndroidApp(
    const Extension* extension) {
  const ReplacementAppsInfo* info = GetReplacementAppsInfo(extension);

  if (info && !info->replacement_android_app.empty()) {
    return info->replacement_android_app;
  }

  return base::EmptyString();
}

bool ReplacementAppsInfo::LoadWebApp(const Extension* extension,
                                     base::string16* error) {
  const base::Value* app_value = nullptr;
  if (!extension->manifest()->Get(keys::kReplacementWebApp, &app_value)) {
    return true;
  }

  DCHECK(app_value);
  if (!app_value->is_string()) {
    *error = base::ASCIIToUTF16(errors::kInvalidReplacementWebApp);
    return false;
  }

  const GURL web_app_url(app_value->GetString());
  if (!web_app_url.is_valid() || !web_app_url.SchemeIs(url::kHttpsScheme)) {
    *error = base::ASCIIToUTF16(errors::kInvalidReplacementWebApp);
    return false;
  }

  replacement_web_app = std::move(web_app_url);
  return true;
}

bool ReplacementAppsInfo::LoadAndroidApp(const Extension* extension,
                                         base::string16* error) {
  const base::Value* app_value = nullptr;
  if (!extension->manifest()->Get(keys::kReplacementAndroidApp, &app_value)) {
    return true;
  }

  DCHECK(app_value);
  if (!app_value->is_string()) {
    *error = base::ASCIIToUTF16(errors::kInvalidReplacementAndroidApp);
    return false;
  }

  replacement_android_app = std::move(app_value->GetString());
  return true;
}

bool ReplacementAppsInfo::Parse(const Extension* extension,
                                base::string16* error) {
  if (!LoadWebApp(extension, error) || !LoadAndroidApp(extension, error)) {
    return false;
  }
  return true;
}

ReplacementAppsHandler::ReplacementAppsHandler() {}

ReplacementAppsHandler::~ReplacementAppsHandler() {}

bool ReplacementAppsHandler::Parse(Extension* extension,
                                   base::string16* error) {
  std::unique_ptr<ReplacementAppsInfo> info(new ReplacementAppsInfo);

  if (!info->Parse(extension, error)) {
    return false;
  }

  extension->SetManifestData(kReplacementApps, std::move(info));
  return true;
}

base::span<const char* const> ReplacementAppsHandler::Keys() const {
  static constexpr const char* kKeys[] = {keys::kReplacementWebApp,
                                          keys::kReplacementAndroidApp};
  return kKeys;
}

}  // namespace extensions