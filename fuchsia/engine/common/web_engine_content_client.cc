// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/common/web_engine_content_client.h"

#include "base/command_line.h"
#include "fuchsia/engine/switches.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

const char WebEngineContentClient::kFuchsiaContentDirectoryScheme[] =
    "fuchsia-dir";

WebEngineContentClient::WebEngineContentClient() = default;
WebEngineContentClient::~WebEngineContentClient() = default;

base::string16 WebEngineContentClient::GetLocalizedString(int message_id) {
  return l10n_util::GetStringUTF16(message_id);
}

base::StringPiece WebEngineContentClient::GetDataResource(
    int resource_id,
    ui::ScaleFactor scale_factor) {
  return ui::ResourceBundle::GetSharedInstance().GetRawDataResourceForScale(
      resource_id, scale_factor);
}

base::RefCountedMemory* WebEngineContentClient::GetDataResourceBytes(
    int resource_id) {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
      resource_id);
}

gfx::Image& WebEngineContentClient::GetNativeImageNamed(int resource_id) {
  return ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      resource_id);
}

blink::OriginTrialPolicy* WebEngineContentClient::GetOriginTrialPolicy() {
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
}

void WebEngineContentClient::AddAdditionalSchemes(Schemes* schemes) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kContentDirectories))
    schemes->standard_schemes.push_back(kFuchsiaContentDirectoryScheme);
}
