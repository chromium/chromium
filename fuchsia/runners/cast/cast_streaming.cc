// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/runners/cast/cast_streaming.h"

#include "base/fuchsia/file_utils.h"
#include "base/path_service.h"

namespace {

constexpr char kCastStreamingAppUrl[] = "cast-streaming:receiver";
constexpr char kCastDataDirectory[] = "fuchsia/runners/cast/data";
constexpr char kCastStreamingContentDirectoryName[] = "cast-streaming";

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

const char kCastStreamingMessagePortOrigin[] = "cast-streaming:receiver";

const char kCastStreamingMessagePortName[] = "cast.__platform__.cast_transport";

bool IsAppConfigForCastStreaming(
    const chromium::cast::ApplicationConfig& application_config) {
  return application_config.web_url() == kCastStreamingAppUrl;
}

void ApplyCastStreamingContextParams(
    fuchsia::web::CreateContextParams* params) {
  // Disable the HARDWARE_VIDEO_DECODER_ONLY tag.
  // TODO(crbug.com/1078227): Remove HARDWARE_VIDEO_DECODER_ONLY once it is
  // no longer set in CastRunner::CommonContextParams(). For now, this is
  // required to enable software decoders.
  *params->mutable_features() &=
      ~fuchsia::web::ContextFeatureFlags::HARDWARE_VIDEO_DECODER_ONLY;

  // Disable the WIDEVINE_CDM tag.
  // TODO(crbug.com/1069746): Remove this once WIDEVINE_CDM is no longer set in
  // CastRunner::CommonContextParams().
  *params->mutable_features() &=
      ~fuchsia::web::ContextFeatureFlags::WIDEVINE_CDM;

  *params->mutable_features() |= fuchsia::web::ContextFeatureFlags::NETWORK;

  // Set the content directory with the streaming app.
  params->set_content_directories(GetCastStreamingContentDirectories());
}
