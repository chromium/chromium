// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/runners/cast/cast_streaming.h"

#include <string>

#include "base/fuchsia/file_utils.h"
#include "base/path_service.h"
#include "fuchsia/base/config_reader.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

constexpr char kCastStreamingAppUrl[] = "cast-streaming:receiver";
constexpr char kCastDataDirectory[] = "fuchsia/runners/cast/data";
constexpr char kCastStreamingContentDirectoryName[] = "cast-streaming";
constexpr char kCastStreamingMessagePortOrigin[] = "cast-streaming:receiver";
constexpr char kCastStreamingVideoOnlyMessagePortOrigin[] =
    "cast-streaming:video-only-receiver";

// Returns the content directories for the Cast Streaming application.
std::vector<fuchsia::web::ContentDirectoryProvider>
GetCastStreamingContentDirectories() {
  base::FilePath pkg_path;
  bool success = base::PathService::Get(base::DIR_ASSETS, &pkg_path);
  DCHECK(success);

  fuchsia::web::ContentDirectoryProvider content_directory;
  content_directory.set_directory(
      base::OpenDirectoryHandle(pkg_path.AppendASCII(kCastDataDirectory)));
  content_directory.set_name(kCastStreamingContentDirectoryName);
  std::vector<fuchsia::web::ContentDirectoryProvider> content_directories;
  content_directories.emplace_back(std::move(content_directory));

  return content_directories;
}

}  // namespace

const char kCastStreamingWebUrl[] =
    "fuchsia-dir://cast-streaming/receiver.html";

const char kCastStreamingMessagePortName[] = "cast.__platform__.cast_transport";

bool IsAppConfigForCastStreaming(
    const chromium::cast::ApplicationConfig& application_config) {
  return application_config.web_url() == kCastStreamingAppUrl;
}

void ApplyCastStreamingContextParams(
    fuchsia::web::CreateContextParams* params) {
  *params->mutable_features() |= fuchsia::web::ContextFeatureFlags::NETWORK;

  // Set the content directory with the streaming app.
  params->set_content_directories(GetCastStreamingContentDirectories());
}

std::string GetMessagePortOriginForAppId(const std::string& app_id) {
  const absl::optional<base::Value>& config = cr_fuchsia::LoadPackageConfig();
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
