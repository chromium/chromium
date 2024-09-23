// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/content/init/ios_content_client.h"

#include <string_view>

#import "ui/base/l10n/l10n_util.h"
#import "ui/base/resource/resource_bundle.h"

namespace web {

IOSContentClient::IOSContentClient() {}
IOSContentClient::~IOSContentClient() {}

blink::OriginTrialPolicy* IOSContentClient::GetOriginTrialPolicy() {
  return &origin_trial_policy_;
}
std::u16string IOSContentClient::GetLocalizedString(int message_id) {
  return l10n_util::GetStringUTF16(message_id);
}

std::u16string IOSContentClient::GetLocalizedString(
    int message_id,
    const std::u16string& replacement) {
  return l10n_util::GetStringFUTF16(message_id, replacement);
}

std::string_view IOSContentClient::GetDataResource(
    int resource_id,
    ui::ResourceScaleFactor scale_factor) {
  return ui::ResourceBundle::GetSharedInstance().GetRawDataResourceForScale(
      resource_id, scale_factor);
}

base::RefCountedMemory* IOSContentClient::GetDataResourceBytes(
    int resource_id) {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
      resource_id);
}

std::string IOSContentClient::GetDataResourceString(int resource_id) {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
      resource_id);
}

gfx::Image& IOSContentClient::GetNativeImageNamed(int resource_id) {
  return ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      resource_id);
}

}  // namespace web
