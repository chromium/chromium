// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/runners/cast/cast_streaming.h"

#include <optional>
#include <string>

#include "components/fuchsia_component_support/config_reader.h"

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
  const std::optional<base::Value::Dict>& config =
      fuchsia_component_support::LoadPackageConfig();
  if (!config) {
    return kCastStreamingMessagePortOrigin;
  }

  constexpr char kEnableVideoOnlyReceiverSwitch[] =
      "enable-video-only-receiver-for-app-ids";
  const base::Value::List* app_id_list =
      config->FindList(kEnableVideoOnlyReceiverSwitch);
  if (!app_id_list) {
    return kCastStreamingMessagePortOrigin;
  }

  for (const base::Value& app_id_value : *app_id_list) {
    if (!app_id_value.is_string()) {
      continue;
    }
    if (app_id == app_id_value.GetString()) {
      return kCastStreamingVideoOnlyMessagePortOrigin;
    }
  }

  return kCastStreamingMessagePortOrigin;
}
