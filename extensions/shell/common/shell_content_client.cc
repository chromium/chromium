// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/common/shell_content_client.h"

#include <string_view>

#include "base/strings/utf_string_conversions.h"
#include "extensions/common/constants.h"
#include "extensions/shell/common/version.h"  // Generated file.
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace extensions {

ShellContentClient::ShellContentClient() {
}

ShellContentClient::~ShellContentClient() {
}

void ShellContentClient::AddAdditionalSchemes(Schemes* schemes) {
  schemes->standard_schemes.push_back(extensions::kExtensionScheme);
  schemes->savable_schemes.push_back(kExtensionScheme);
  schemes->secure_schemes.push_back(kExtensionScheme);
  schemes->cors_enabled_schemes.push_back(kExtensionScheme);
  schemes->csp_bypassing_schemes.push_back(kExtensionScheme);
}

std::u16string ShellContentClient::GetLocalizedString(int message_id) {
  return l10n_util::GetStringUTF16(message_id);
}

std::string_view ShellContentClient::GetDataResource(
    int resource_id,
    ui::ResourceScaleFactor scale_factor) {
  return ui::ResourceBundle::GetSharedInstance().GetRawDataResourceForScale(
      resource_id, scale_factor);
}

base::RefCountedMemory* ShellContentClient::GetDataResourceBytes(
    int resource_id) {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
      resource_id);
}

gfx::Image& ShellContentClient::GetNativeImageNamed(int resource_id) {
  return ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      resource_id);
}

}  // namespace extensions
