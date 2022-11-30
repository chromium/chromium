// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/win/mf_feature_checks.h"

#include "base/win/windows_version.h"
#include "media/base/media_switches.h"

namespace media {

bool SupportMediaFoundationPlayback() {
  return SupportMediaFoundationClearPlayback() ||
         SupportMediaFoundationEncryptedPlayback();
}

bool SupportMediaFoundationClearPlayback() {
  return base::win::GetVersion() >= base::win::Version::WIN10_RS3 &&
         base::FeatureList::IsEnabled(kMediaFoundationClearPlayback);
}

bool SupportMediaFoundationEncryptedPlayback() {
  // TODO(xhwang): Also check whether software secure decryption is enabled and
  // supported by MediaFoundationCdm in the future.
  return base::win::GetVersion() >= base::win::Version::WIN10_20H1 &&
         IsHardwareSecureDecryptionEnabled();
}

}  // namespace media
