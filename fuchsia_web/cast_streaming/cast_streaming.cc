// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/cast_streaming/cast_streaming.h"

#include <string>

#include "base/check.h"
#include "base/fuchsia/file_utils.h"
#include "base/path_service.h"

namespace {

constexpr char kContentDirectoryRelativePath[] =
    "fuchsia_web/cast_streaming/data";
constexpr char kCastStreamingContentDirectoryName[] = "cast-streaming";

// Returns the content directories for the Cast Streaming application.
std::vector<fuchsia::web::ContentDirectoryProvider>
GetCastStreamingContentDirectories() {
  base::FilePath pkg_path;
  bool success = base::PathService::Get(base::DIR_ASSETS, &pkg_path);
  DCHECK(success);

  fuchsia::web::ContentDirectoryProvider content_directory;
  content_directory.set_directory(base::OpenDirectoryHandle(
      pkg_path.AppendASCII(kContentDirectoryRelativePath)));
  content_directory.set_name(kCastStreamingContentDirectoryName);
  std::vector<fuchsia::web::ContentDirectoryProvider> content_directories;
  content_directories.emplace_back(std::move(content_directory));

  return content_directories;
}

}  // namespace

const char kCastStreamingWebUrl[] =
    "fuchsia-dir://cast-streaming/receiver.html";

void ApplyCastStreamingContextParams(
    fuchsia::web::CreateContextParams* params) {
  *params->mutable_features() |= fuchsia::web::ContextFeatureFlags::NETWORK;

  // Set the content directory with the streaming app.
  params->set_content_directories(GetCastStreamingContentDirectories());
}
