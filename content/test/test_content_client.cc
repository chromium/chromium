// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_content_client.h"

#include <utility>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_piece.h"
#include "build/build_config.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_ANDROID)
#include "base/android/apk_assets.h"
#include "base/android/locale_utils.h"
#include "ui/base/resource/resource_bundle_android.h"
#endif

namespace content {

TestContentClient::TestContentClient() {
  // content_shell.pak is not built on iOS as it is not required.
  base::FilePath content_shell_pack_path;

#if defined(OS_ANDROID)
  // on Android all pak files are inside the paks folder.
  CHECK(base::PathService::Get(base::DIR_ANDROID_APP_DATA,
                               &content_shell_pack_path));
  content_shell_pack_path = content_shell_pack_path.Append(
      FILE_PATH_LITERAL("paks"));
#else
  CHECK(base::PathService::Get(base::DIR_ASSETS, &content_shell_pack_path));
#endif  // defined(OS_ANDROID)

  // Add the content_shell main pak file.
  content_shell_pack_path =
      content_shell_pack_path.Append(FILE_PATH_LITERAL("content_shell.pak"));

  if (!ui::ResourceBundle::HasSharedInstance()) {
#if defined(OS_ANDROID)
    ui::ResourceBundle::InitSharedInstanceWithLocale(
        base::android::GetDefaultLocaleString(), NULL,
        ui::ResourceBundle::DO_NOT_LOAD_COMMON_RESOURCES);

    ui::LoadMainAndroidPackFile("assets/content_shell.pak",
                                content_shell_pack_path);
#else
    ui::ResourceBundle::InitSharedInstanceWithPakPath(content_shell_pack_path);
#endif
  }
}

TestContentClient::~TestContentClient() {
}

base::StringPiece TestContentClient::GetDataResource(
    int resource_id,
    ui::ScaleFactor scale_factor) {
  return ui::ResourceBundle::GetSharedInstance().GetRawDataResourceForScale(
      resource_id, scale_factor);
}

}  // namespace content
