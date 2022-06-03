// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/platform_mime_util.h"

#include <string>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

#if defined(OS_ANDROID)
#include "net/android/network_library.h"
#else
#include "base/nix/mime_util_xdg.h"
#endif

namespace net {

#if defined(OS_ANDROID)
bool PlatformMimeUtil::GetPlatformMimeTypeFromExtension(
    const base::FilePath::StringType& ext,
    std::string* result) const {
  return android::GetMimeTypeFromExtension(ext, result);
}
#elif BUILDFLAG(IS_CHROMEOS_ASH)
bool PlatformMimeUtil::GetPlatformMimeTypeFromExtension(
    const base::FilePath::StringType& ext,
    std::string* result) const {
  return false;
}
#else
bool PlatformMimeUtil::GetPlatformMimeTypeFromExtension(
    const base::FilePath::StringType& ext,
    std::string* result) const {
  base::FilePath dummy_path("foo." + ext);
  std::string out = base::nix::GetFileMimeType(dummy_path);

  // GetFileMimeType likes to return application/octet-stream
  // for everything it doesn't know - ignore that.
  if (out == "application/octet-stream" || out.empty())
    return false;

  // GetFileMimeType returns image/x-ico because that's what's in the XDG
  // mime database. That database is the merger of the Gnome and KDE mime
  // databases. Apparently someone working on KDE in 2001 decided .ico
  // resolves to image/x-ico, whereas the rest of the world uses image/x-icon.
  // FWIW, image/vnd.microsoft.icon is the official IANA assignment.
  if (out == "image/x-ico")
    out = "image/x-icon";

  *result = out;
  return true;
}

#endif  // defined(OS_ANDROID)

bool PlatformMimeUtil::GetPlatformPreferredExtensionForMimeType(
    const std::string& mime_type,
    base::FilePath::StringType* ext) const {
  // xdg_mime doesn't provide an API to get extension from a MIME type, so we
  // rely on the mappings hardcoded in mime_util.cc .
  return false;
}

void PlatformMimeUtil::GetPlatformExtensionsForMimeType(
    const std::string& mime_type,
    std::unordered_set<base::FilePath::StringType>* extensions) const {
  // xdg_mime doesn't provide an API to get extension from a MIME type, so we
  // rely on the mappings hardcoded in mime_util.cc .
}

}  // namespace net
