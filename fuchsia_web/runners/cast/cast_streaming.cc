// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/runners/cast/cast_streaming.h"

#include <string>

#include "components/fuchsia_component_support/config_reader.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

constexpr char kCastStreamingAppUrl[] = "cast-streaming:receiver";
constexpr char kCastStreamingMessagePortOrigin[] = "cast-streaming:receiver";
constexpr char kCastStreamingVideoOnlyMessagePortOrigin[] =
    "cast-streaming:video-only-receiver";

}  // namespace

const char kCastStreamingMessagePortName[] = "cast.__platform__.cast_transport";

bool IsAppConfigForCastStreaming(
    const chromium::cast::ApplicationConfig& application_config) {
  return application_config.web_url() == kCastStreamingAppUrl;
}

std::string GetMessagePortOriginForAppId(const std::string& app_id) {
  const absl::optional<base::Value>& config =
      fuchsia_component_support::LoadPackageConfig();
  if (!config) {
    return kCastStreamingMessagePortOrigin;
  }

  constexpr char kEnableVideoOnlyReceiverSwitch[] =
      "enable-video-only-receiver-for-app-ids";
  const base::Value* app_id_list =
      config->FindListKey(kEnableVideoOnlyReceiverSwitch);
  if (!app_id_list) {
    return kCastStreamingMessagePortOrigin;
  }

  for (const base::Value& app_id_value : app_id_list->GetListDeprecated()) {
    if (!app_id_value.is_string()) {
      continue;
    }
    if (app_id == app_id_value.GetString()) {
      return kCastStreamingVideoOnlyMessagePortOrigin;
    }
  }

  return kCastStreamingMessagePortOrigin;
}
